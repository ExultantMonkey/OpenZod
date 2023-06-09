#include "zbuilding.h"
#include "zfont_engine.h"

#include "Util/Random.h"

#include <spdlog/spdlog.h>

ZSDL_Surface ZBuilding::level_img[MAX_BUILDING_LEVELS];
ZSDL_Surface ZBuilding::exhaust[13];
ZSDL_Surface ZBuilding::little_exhaust[4];

ZBuilding::ZBuilding(ZTime *ztime_, ZSettings *zsettings_, planet_type palette_) : ZObject(ztime_, zsettings_)
{
	object_name = "building";
	destroyed = false;
	palette = palette_;
	level = 0;
	object_type = BUILDING_OBJECT;
	bot = -1;
	boid = -1;
	binit_time = 0;
	bfinal_time = 0;
	btotal_time = 0;
	can_be_destroyed = false;
	zone_ownage = 0;
	attacked_by_explosives = true;

	do_base_rerender = true;

	//default
	build_state = BUILDING_SELECT;

	//some defaults
	unit_create_x = 32;
	unit_create_y = 32;

	unit_move_x = 32;
	unit_move_y = 112;

	//more defaults
	effects_box.x = 16;
	effects_box.y = 16;
	effects_box.w = 32;
	effects_box.h = 32;
	max_effects = 8;

	show_time = -1;
}

ZBuilding::~ZBuilding()
{
	//no memory leaks
	for(std::vector<EStandard*>::iterator i=extra_effects.begin(); i!=extra_effects.begin(); i++)
	{
		delete *i;
	}

	extra_effects.clear();
}

void ZBuilding::Init()
{
	char filename[500];
	
	for(int i=0;i<MAX_BUILDING_LEVELS;i++)
	{
		sprintf(filename, "assets/buildings/level_%d.bmp", i+1);
		level_img[i].LoadBaseImage(filename);
	}
	
	for(int i=0;i<13;i++)
	{
		sprintf(filename, "assets/buildings/exhaust_%d.png", i);
		exhaust[i].LoadBaseImage(filename);
	}
	
	for(int i=0;i<4;i++)
	{
		sprintf(filename, "assets/buildings/little_exhaust_%d.png", i);
		little_exhaust[i].LoadBaseImage(filename);
	}
}

int ZBuilding::GetBuildState()
{
	if(unit_limit_reached[owner])
	{
		return BUILDING_PAUSED;
	}
	else
	{
		return build_state;
	}
}

int ZBuilding::GetLevel()
{
	return level;
}

void ZBuilding::ChangePalette(planet_type palette_)
{
	palette = palette_;
}

void ZBuilding::ReRenderBase()
{
	do_base_rerender = true;
}

bool ZBuilding::SetBuildingDefaultProduction()
{
	unsigned char ot, oid;

	if(bot == (unsigned char)-1 && boid == (unsigned char)-1 && build_state == BUILDING_SELECT)
	{
		if(!buildlist->GetFirstUnitInBuildList(object_id, level, ot, oid))
		{
			return false;
		}

		return SetBuildingProduction(ot, oid);
	}
	else
	{
		return false;
	}
}

bool ZBuilding::SetBuildingProduction(unsigned char ot, unsigned char oid)
{
	if(owner == NULL_TEAM)
	{
		return false;
	}

	//produces units?
	if(!ProducesUnits())
	{
		return false;
	}

	//we already building it?
	if(ot == bot && oid == boid)
	{
		return false;
	}

	//is this unit available to be produced here?
	if(!buildlist->UnitInBuildList(object_id, level, ot, oid))
	{
		return false;
	}

	//set this building to be producing this unit
	//with the correct building time
	bot = ot;
	boid = oid;

	build_state = BUILDING_BUILDING;
	double &the_time = ztime->ztime;
	binit_time = the_time;
	RecalcBuildTime();

	//add to queue list?
	if(!queue_list.size())
	{
		AddBuildingQueue(ot, oid);
	}

	return true;
}

bool ZBuilding::AddBuildingQueue(unsigned char ot, unsigned char oid, bool push_to_front)
{
	if(owner == NULL_TEAM)
	{
		return false;
	}

	//produces units?
	if(!ProducesUnits())
	{
		return false;
	}

	//already maxed out?
	if(queue_list.size() >= MAX_QUEUE_ITEMS)
	{
		return false;
	}

	//is this unit available to be produced here?
	if(!buildlist->UnitInBuildList(object_id, level, ot, oid))
	{
		return false;
	}

	//add it..
	if(push_to_front)
	{
		queue_list.insert(queue_list.begin(), ZBProductionUnit(ot, oid));
	}
	else
	{
		queue_list.push_back(ZBProductionUnit(ot, oid));
	}

	return true;
}

bool ZBuilding::CancelBuildingQueue(int list_i, unsigned char ot, unsigned char oid)
{
	if(owner == NULL_TEAM)
	{
		return false;
	}

	//produces units?
	if(!ProducesUnits())
	{
		return false;
	}

	//list_i ok?
	if(list_i<0)
	{
		return false;
	}
	if(list_i>=queue_list.size())
	{
		return false;
	}

	//ot and oid match?
	if((ot != queue_list[list_i].ot) || (oid != queue_list[list_i].oid))
	{
		return false;
	}

	//erase it
	queue_list.erase(queue_list.begin() + list_i);

	return true;
}

void ZBuilding::CreateBuildingQueueData(char *&data, int &size)
{
	data = NULL;
	size = 0;

	//produces units?
	if(!ProducesUnits())
	{
		return;
	}

	int queue_amount = queue_list.size();

	//make mem
	char* message;
	size = 8 + (queue_amount * sizeof(ZBProductionUnit));
	data = message = (char*)malloc(size);

	//push header, the ref id of this object and the number of waypoints
	((int*)message)[0] = ref_id;
	((int*)message)[1] = queue_amount;

	//populate
	message += 8;
	for(ZBProductionUnit j : queue_list)
	{
		memcpy(message, &j, sizeof(ZBProductionUnit));
		message += sizeof(ZBProductionUnit);
	}
}

void ZBuilding::ProcessBuildingQueueData(char *data, int size)
{
	int expected_packet_size;
	int queue_amount;
	int data_ref_id;

	//produces units?
	if(!ProducesUnits())
	{
		return;
	}

	//does it hold the header info?
	if(size < 8)
	{
		return;
	}

	//get header
	data_ref_id = ((int*)data)[0];
	queue_amount = ((int*)data)[1];

	expected_packet_size = 8 + (queue_amount * sizeof(ZBProductionUnit));

	//should we toss this bad data?
	if(size != expected_packet_size)
	{
		return;
	}

	//not our ref_id... ?
	if(ref_id != data_ref_id)
	{
		spdlog::error("ZBuilding::ProcessBuildingQueueData - ref_id's do not match  - {} vs {}", ref_id, data_ref_id);
		return;
	}

	//clear list
	queue_list.clear();

	//begin the push
	data += 8;
	for(int i=0;i<queue_amount;i++)
	{
		ZBProductionUnit new_unit;

		memcpy(&new_unit, data, sizeof(ZBProductionUnit));

		//add
		if(!AddBuildingQueue(new_unit.ot, new_unit.oid, false))
		{
			spdlog::error("ZBuilding::ProcessBuildingQueueData - Could not add unit {}:{} {}", i, new_unit.ot, new_unit.oid);
		}

		data += sizeof(ZBProductionUnit);
	}
}

bool ZBuilding::StopBuildingProduction(bool clear_queue_list)
{
	if(bot == (unsigned char)-1 && boid == (unsigned char)-1 && build_state == BUILDING_SELECT)
	{
		return false;
	}

	build_state = BUILDING_SELECT;
	bot = -1;
	boid = -1;

	//also clear the queue_list
	if(clear_queue_list)
	{
		queue_list.clear();
	}

	return true;
}

void ZBuilding::CreateBuiltCannonData(char *&data, int &size)
{
	size = 8 + built_cannon_list.size();
	data = (char*)malloc(size);

	*(int*)(data) = ref_id;
	*(int*)(data + 4) = built_cannon_list.size();

	for(int i=0;i<built_cannon_list.size();i++)
	{
		*(char*)(data + 8 + i) = built_cannon_list[i];
	}
}

void ZBuilding::ProcessSetBuiltCannonData(char *data, int size)
{
	//bad data?
	if(size < 8)
	{
		return;
	}

	int cannon_amt = *(int*)(data + 4);

	//bad data?
	if(size - 8 != cannon_amt)
	{
		return;
	}

	//set cannon placement list
	built_cannon_list.clear();
	for(int i=0;i<cannon_amt;i++)
	{
		built_cannon_list.push_back(*(unsigned char*)(data + 8 + i));
	}
}

void ZBuilding::CreateBuildingStateData(char *&data, int &size)
{
	double &the_time = ztime->ztime;

	size = sizeof(set_building_state_packet);
	data = (char*)malloc(size);

	set_building_state_packet *pi = (set_building_state_packet*)data;

	pi->ref_id = ref_id;
	pi->state = build_state;
	pi->ot = bot;
	pi->oid = boid;
	pi->prod_time = bfinal_time - binit_time;
	pi->init_offset = binit_time - the_time;
}

void ZBuilding::ProcessSetBuildingStateData(char *data, int size)
{
	double &the_time = ztime->ztime;

	//good packet?
	if(size != sizeof(set_building_state_packet))
	{
		return;
	}
	set_building_state_packet *pi = (set_building_state_packet*)data;

	//haha.
	if(ref_id != pi->ref_id)
	{
		return;
	}

	build_state = pi->state;
	bot = pi->ot;
	boid = pi->oid;
	binit_time = the_time + pi->init_offset;
	bfinal_time = binit_time + pi->prod_time;
	btotal_time = pi->prod_time;
}

double ZBuilding::PercentageProduced(double &the_time)
{
	double the_percentage = (the_time - binit_time) / (bfinal_time - binit_time);

	if(the_percentage < 0)
	{
		the_percentage = 0;
	}
	if(the_percentage > 1)
	{
		the_percentage = 1;
	}

	return the_percentage;
}

double ZBuilding::ProductionTimeLeft(double &the_time)
{
	double time_left = bfinal_time - the_time;

	if(time_left < 0)
	{
		time_left = 0;
	}

	return time_left;
}

bool ZBuilding::GetBuildingCreationPoint(int &x, int &y)
{
	x = loc.x + unit_create_x;
	y = loc.y + unit_create_y;

	return true;
}

bool ZBuilding::GetBuildingCreationMovePoint(int &x, int &y)
{
	x = loc.x + unit_move_x;
	y = loc.y + unit_move_y;

	return true;
}

bool ZBuilding::BuildUnit(double &the_time, unsigned char &ot, unsigned char &oid)
{
	if(bot == (unsigned char)-1)
	{
		return false;
	}
	if(boid == (unsigned char)-1)
	{
		return false;
	}
	if(build_state == BUILDING_SELECT)
	{
		return false;
	}
	if(owner == NULL_TEAM)
	{
		return false;
	}

	if(the_time >= bfinal_time && !unit_limit_reached[owner])
	{
		ot = bot;
		oid = boid;
		return true;
	}

	return false;
}

bool ZBuilding::StoreBuiltCannon(unsigned char oid)
{
	//already full?
	if(built_cannon_list.size() >= MAX_STORED_CANNONS)
	{
		return false;
	}

	built_cannon_list.push_back(oid);
	return true;
}

int ZBuilding::CannonsInZone(ZOLists &ols)
{
	//the zbuilding version starts with built_cannon_list.size()
	int cannons_found = built_cannon_list.size();

	for(std::vector<ZObject*>::iterator i=ols.object_list->begin();i!=ols.object_list->end();i++)
	{
		if(this != *i && connected_zone == (*i)->GetConnectedZone())
		{
			unsigned char ot, oid;

			(*i)->GetObjectID(ot, oid);

			//collect in cannons that other buildings have not yet placed
			if(ot == BUILDING_OBJECT)
				cannons_found += (*i)->GetBuiltCannonList().size();

			if(ot != CANNON_OBJECT) continue;

			cannons_found++;
		}
	}

	return cannons_found;
}

bool ZBuilding::RemoveStoredCannon(unsigned char oid)
{
	//find it
	for(std::vector<unsigned char>::iterator i=built_cannon_list.begin();i!=built_cannon_list.end();i++)
	{
		if(oid == *i)
		{
			built_cannon_list.erase(i);
			return true;
		}
	}

	return false;
}

bool ZBuilding::HaveStoredCannon(unsigned char oid)
{
	//find it
	for(std::vector<unsigned char>::iterator i=built_cannon_list.begin();i!=built_cannon_list.end();i++)
	{
		if(oid == *i)
		{
			return true;
		}
	}

	return false;
}

void ZBuilding::ResetProduction()
{
	if(!queue_list.size())
	{
		StopBuildingProduction();
		return;
	}

	unsigned char ot, oid;

	ot = queue_list.begin()->ot;
	oid = queue_list.begin()->oid;

	//take this off the list
	queue_list.erase(queue_list.begin());

	//clear current production
	StopBuildingProduction(false);

	//start new
	SetBuildingProduction(ot, oid);
}

bool ZBuilding::GetBuildUnit(unsigned char &ot, unsigned char &oid)
{
	ot = bot;
	oid = boid;

	return true;
}

void ZBuilding::ResetShowTime(int new_time)
{
	char message[50];

	if(new_time == show_time)
	{
		return;
	}

	show_time_img.Unload();

	if(new_time <= -1)
	{
		return;
	}

	show_time = new_time;

	//setup these numbers
	int seconds = new_time % 60;
	new_time /= 60;
	int minutes = new_time % 60;

	sprintf(message, "%i:%02i", minutes, seconds);
	show_time_img.LoadBaseImage(ZFontEngine::GetFont(GREEN_BUILDING_FONT).Render(message));
}

void ZBuilding::SetOwner(team_type owner_)
{
	owner = owner_;
	do_base_rerender = true;
}

std::vector<unsigned char> &ZBuilding::GetBuiltCannonList()
{
	return built_cannon_list;
}

void ZBuilding::SetLevel(int level_)
{
	level = level_;
}

void ZBuilding::DoDeathEffect(bool do_fire_death, bool do_missile_death)
{
	do_base_rerender = true;
}

void ZBuilding::DoReviveEffect()
{
	do_base_rerender = true;

	//no memory leaks
	for(std::vector<EStandard*>::iterator i=extra_effects.begin(); i!=extra_effects.begin(); i++)
	{
		delete *i;
	}

	extra_effects.clear();
}

void ZBuilding::ProcessBuildingsEffects(double &the_time)
{
	for(std::vector<EStandard*>::iterator i=extra_effects.begin(); i!=extra_effects.end(); i++)
	{
		(*i)->Process();
	}

	double damage_percent = 1.0 * health / max_health;

	if(damage_percent > 1)
	{
		damage_percent = 1;
	}
	if(damage_percent < 0)
	{
		damage_percent = 0;
	}

	int should_effects = max_effects * (1 - damage_percent);
	bool effects_added = false;
	for(int i=extra_effects.size();i<should_effects;i++)
	{
		int ex, ey;
		int choice;

		ex = loc.x + effects_box.x + OpenZod::Util::Random::Int(0, effects_box.w - 1);
		ey = loc.y + effects_box.y + OpenZod::Util::Random::Int(0, effects_box.h - 1);

		
		choice = OpenZod::Util::Random::Int(0, 99);

		if(choice < 10)
		{
			extra_effects.push_back(new EStandard(ztime, ex, ey, EDEATH_BIG_SMOKE));
		}
		else if(choice < 20)
		{
			extra_effects.push_back(new EStandard(ztime, ex, ey, EDEATH_SMALL_FIRE_SMOKE));
		}
		else if(choice < 50)
		{
			extra_effects.push_back(new EStandard(ztime, ex, ey, EDEATH_FIRE));
		}
		else
		{
			extra_effects.push_back(new EStandard(ztime, ex, ey, EDEATH_LITTLE_FIRE));
		}

		effects_added = true;
	}

	//sort effects
	if(effects_added)
	{
		sort(extra_effects.begin(), extra_effects.end(), sort_estandards_func);
	}
}

bool ZBuilding::ResetBuildTime(float zone_ownage_)
{
	if(zone_ownage_ == zone_ownage)
	{
		return false;
	}

	zone_ownage = zone_ownage_;

	if(zone_ownage > 1)
	{
		zone_ownage = 1;
	}
	if(zone_ownage < 0)
	{
		zone_ownage = 0;
	}

	return RecalcBuildTime();
}

bool ZBuilding::RecalcBuildTime()
{
	double bfinal_time_old = bfinal_time;

	//calc
	if(!buildlist)
	{
		return false;
	}

	if((bot == (unsigned char)-1) || (boid == (unsigned char)-1))
	{
		return false;
	}
	if(build_state == BUILDING_SELECT)
	{
		return false;
	}
		
	//base time
	double build_time = BuildTimeModified(buildlist->UnitBuildTime(bot, boid));
	bfinal_time = binit_time + build_time;

	return bfinal_time_old != bfinal_time;
}

double ZBuilding::BuildTimeModified(double base_build_time)
{
	base_build_time -= base_build_time * 0.5 * zone_ownage;
	base_build_time += base_build_time * (1.25 * (1.0 - (1.0 * health / max_health)));

	return base_build_time;
}
