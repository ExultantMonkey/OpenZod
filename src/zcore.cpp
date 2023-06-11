#include "zcore.h"

#include "Util/Random.h"

#include <spdlog/spdlog.h>

int p_info::next_p_id = 0;

ZCore::ZCore()
{
	InitEncryption();

	zunitrating.Init();

	ols.Init(&object_list);

	max_units_per_team = DEFAULT_MAX_UNITS_PER_TEAM;
	buildlist.SetZSettings(&zsettings);
	buildlist.LoadDefaults();
	ResetUnitLimitReached();

	bot_bypass_size = 0;

	is_registered = false;

	allow_run = true;
}

void ZCore::Setup()
{
	SetupRandomizer();
}

void ZCore::Run()
{
	
}

void ZCore::SetBotBypassData(char *data, int size)
{
	if((size < 1) || (size > MAX_BOT_BYPASS_SIZE))
	{
		return;
	}

	memcpy(bot_bypass_data, data, size);
	bot_bypass_size = size;
}

void ZCore::CreateRandomBotBypassData(char *data, int &size)
{
	size = MAX_BOT_BYPASS_SIZE - OpenZod::Util::Random::Int(0, MAX_BOT_BYPASS_RANDOM_SIZE_OFFSET - 1);

	for(int i=0;i<size;i++)
	{
		data[i] = OpenZod::Util::Random::Int(0, 255);
	}
}

void ZCore::SetupRandomizer()
{
	//setup randomizer
	srand(time(0));
}

void ZCore::InitEncryption()
{
	unsigned char key[16] = {0xFE, 0xEA, 0x42, 0x35, 0x78, 0x02, 0x57, 0xEC, 0xEE, 0x92, 0x11, 0x58, 0xC2, 0x5d, 0xC3, 0x23};

	zencrypt.Init_Key(key, 128);
}

bool ZCore::CheckRegistration()
{
	static SDL_mutex *check_mutex = SDL_CreateMutex();
	char buf_enc[16];
	char buf_key[16];

#ifdef DISABLE_REGCHECK
	printf("<<<< ---------------------------------------------------------------- >>>>\n");
	printf("<<<<                  REGISTRATION CHECKING DISABLED                  >>>>\n");
	printf("<<<< ---------------------------------------------------------------- >>>>\n");
	is_registered = true;
	return is_registered;
#endif

	//clients and servers on different threads may use this function
	SDL_LockMutex(check_mutex);

	FILE* fp = fopen("registration.zkey", "r");

	if(!fp)
	{
		printf(">>>> ---------------------------------------------------------------- <<<<\n");
		printf(">>>>                  no registration key file found                  <<<<\n");
		printf(">>>> please visit www.nighsoft.com on how to register your zod engine <<<<\n");
		printf(">>>> ---------------------------------------------------------------- <<<<\n");

		SDL_UnlockMutex(check_mutex);

		return false;
	}

	int ret = fread(buf_enc, 1, 16, fp);

	if(ret == 16)
	{
		char buf_mac[16];

		//clear
		memset(buf_mac, 0, 16);

		//get our mac
		SocketHandler::GetMAC(buf_mac);

		//key stored reg key
		zencrypt.AES_Decrypt(buf_enc,16,buf_key);

		//check key
		is_registered = true;
		for(int i=0;i<16;i++)
			if(buf_mac[i] != buf_key[i])
			{
				printf(">>>> ---------------------------------------------------------------- <<<<\n");
				printf(">>>>                     registration key invalid                     <<<<\n");
				printf(">>>> please visit www.nighsoft.com on how to register your zod engine <<<<\n");
				printf(">>>> ---------------------------------------------------------------- <<<<\n");

				is_registered = false;
				break;
			}
	}
	else
	{
		printf(">>>> ---------------------------------------------------------------- <<<<\n");
		printf(">>>>            registration key file unreadable or corrupt           <<<<\n");
		printf(">>>> please visit www.nighsoft.com on how to register your zod engine <<<<\n");
		printf(">>>> ---------------------------------------------------------------- <<<<\n");
	}

	fclose(fp);

	SDL_UnlockMutex(check_mutex);

	return is_registered;
}

int ZCore::GetObjectIndex(ZObject* &the_object, std::vector<ZObject*> &the_list)
{
	int i;
	std::vector<ZObject*>::iterator obj;

	for(i=0, obj=the_list.begin(); obj!=the_list.end();obj++, i++)
	{
		if(*obj == the_object)
		{
			return i;
		}
	}

	return -1;
}

ZObject* ZCore::GetObjectFromID(int ref_id, std::vector<ZObject*> &the_list)
{
	return ZObject::GetObjectFromID_BS(ref_id, the_list);
}

bool ZCore::CheckRallypoint(ZObject *obj, waypoint *wp)
{
	if(wp->mode != MOVE_WP)
	{
		return false;
	}

	return true;
}

bool ZCore::CheckWaypoint(ZObject *obj, waypoint *wp)
{
	unsigned char ot, oid;
	unsigned char aot, aoid;
	ZObject *aobj;

	obj->GetObjectID(ot, oid);

	//just set this to false if
	//the unit can not attack
	if(!obj->CanAttack())
	{
		wp->attack_to = false;
	}

	switch(wp->mode)
	{
	case MOVE_WP:
		//can move?
		if(!obj->CanMove())
		{
			return false;
		}
		break;
	case FORCE_MOVE_WP:
		//can move?
		if(!obj->CanMove())
		{
			return false;
		}

		//clients are not allowed to set forcemove waypoints
		wp->mode = MOVE_WP;
		break;
	case DODGE_WP:
		//can move?
		if(!obj->CanMove())
		{
			return false;
		}

		//clients are not allowed to set dodge waypoints
		wp->mode = MOVE_WP;
	case AGRO_WP:
		//clients are not allowed to set agro waypoints
		wp->mode = ATTACK_WP;
		break;
	case ATTACK_WP:
		//attacking an object that can only be attacked by explosions?
		aobj = GetObjectFromID(wp->ref_id, object_list);

		if(!aobj)
		{
			return false;
		}

		if(!obj->CanAttackObject(aobj))
		{
			return false;
		}
		break;
	case ENTER_WP:
		//can move?
		if(!obj->CanMove())
		{
			return false;
		}

		//entering ok?
		if(ot != ROBOT_OBJECT)
		{
			return false;
		}

		//target exist?
		aobj = GetObjectFromID(wp->ref_id, object_list);
		if(!aobj)
		{
			return false;
		}

		//can the target be entered?
		if(!aobj->CanBeEntered())
		{
			return false;
		}
		break;
	case CRANE_REPAIR_WP:
		//can move?
		if(!obj->CanMove())
		{
			return false;
		}

		//it a crane?
		if(!(ot == VEHICLE_OBJECT && oid == CRANE))
		{
			return false;
		}

		//target exist?
		aobj = GetObjectFromID(wp->ref_id, object_list);
		if(!aobj)
		{
			return false;
		}

		//can it repair that target?
		if(!aobj->CanBeRepairedByCrane(obj->GetOwner()))
		{
			return false;
		}
		break;
	case UNIT_REPAIR_WP:
		//can move?
		if(!obj->CanMove())
		{
			return false;
		}

		//can it be repaired?
		if(!obj->CanBeRepaired())
		{
			return false;
		}

		//target exist?
		aobj = GetObjectFromID(wp->ref_id, object_list);
		if(!aobj)
		{
			return false;
		}

		//can the target repair it?
		if(!aobj->CanRepairUnit(obj->GetOwner()))
		{
			return false;
		}
		break;
	case ENTER_FORT_WP:
		//can move?
		if(!obj->CanMove())
		{
			return false;
		}

		//target exist?
		aobj = GetObjectFromID(wp->ref_id, object_list);
		if(!aobj)
		{
			return false;
		}

		//can we enter it?
		if(!aobj->CanEnterFort(obj->GetOwner()))
		{
			return false;
		}

		break;
	case PICKUP_GRENADES_WP:
		//can move?
		if(!obj->CanMove())
		{
			return false;
		}

		//can pickup grenades?
		if(!obj->CanPickupGrenades())
		{
			return false;
		}

		//target exist?
		aobj = GetObjectFromID(wp->ref_id, object_list);
		if(!aobj)
		{
			return false;
		}

		//is it grenades?
		aobj->GetObjectID(aot, aoid);
		if(!(aot == MAP_ITEM_OBJECT && aoid == GRENADES_ITEM))
		{
			return false;
		}

		break;
	}

	//bad mode?
	if(wp->mode < 0 || wp->mode >= MAX_WAYPOINT_MODES)
	{
		return false;
	}

	//should be good
	return true;
}

bool ZCore::UnitRequiresActivation(unsigned char ot, unsigned char oid)
{
	switch(ot)
	{
	case ROBOT_OBJECT:
		switch(oid)
		{
		case PYRO:
			return true;
			break;
		case LASER:
			return true;
			break;
		}
		break;
	case VEHICLE_OBJECT:
		switch(oid)
		{
		case JEEP:
			return false;
			break;
		case LIGHT:
			return false;
			break;
		default:
			return true;
			break;
		}

		break;
	}

	return false;
}

bool ZCore::CreateObjectOk(unsigned char ot, unsigned char oid, int x, int y, int owner, int blevel, unsigned short extra_links)
{
	if(owner < 0 || owner >= MAX_TEAM_TYPES)
	{
		spdlog::error("ZCore::CreateObjectOK - Invalid team {}", owner);
		return false;
	}

	return true;
}

void ZCore::ResetZoneOwnagePercentages(bool notify_players)
{
	//clear
	int zones = 0;
	int zone_ownage[MAX_TEAM_TYPES];
	for(int i=0;i<MAX_TEAM_TYPES;i++)
	{
		team_zone_percentage[i] = 0;
		zone_ownage[i] = 0;
	}

	//tally
	for(ZObject* of : object_list)
	{
		unsigned char ot, oid;
		of->GetObjectID(ot, oid);

		if(ot != MAP_ITEM_OBJECT || oid != FLAG_ITEM)
		{
			continue;
		}

		zone_ownage[of->GetOwner()]++;
		zones++;
	}

	//no flags?
	if(!zones)
	{
		return;
	}

	//percentage
	for(int i=0;i<MAX_TEAM_TYPES;i++)
	{
		team_zone_percentage[i] = 1.0 * zone_ownage[i] / zones;
	}

	//tell all the buildings
	for(ZObject* obj : object_list)
	{
		obj->SetZoneOwnage(team_zone_percentage[obj->GetOwner()]);
	}
}

int ZCore::VotesNeeded()
{
	int needed_power = 0;

	for(p_info i : player_info)
	{
		if(i.vote_choice != P_PASS_VOTE)
		{
			needed_power += i.real_voting_power();
		}
	}

	//make an even number
	if(needed_power % 2)
	{
		needed_power++;
	}

	return needed_power / 2;
}

int ZCore::VotesFor()
{
	int votes = 0;

	for(p_info i : player_info)
	{
		if(i.vote_choice == P_YES_VOTE)
		{
			votes += i.real_voting_power();
		}
	}

	return votes;
}

int ZCore::VotesAgainst()
{
	int votes = 0;

	for(p_info i : player_info)
	{
		if(i.vote_choice == P_NO_VOTE)
		{
			votes += i.real_voting_power();
		}
	}

	return votes;
}

std::string ZCore::VoteAppendDescription()
{
	int vote_type = zvote.GetVoteType();
	int value = zvote.GetVoteValue();

	switch(vote_type)
	{
	case CHANGE_MAP_VOTE:
		if((value < 0) || (value >= selectable_map_list.size()))
		{
			return "";
		}

		char num_c[50];
		sprintf(num_c, "%d. ", value);

		return num_c + selectable_map_list[value];
		break;
	case START_BOT:
	case STOP_BOT:
		if((value < 0) || (value >= MAX_TEAM_TYPES))
		{
			return "";
		}

		return team_type_string[value];
		break;
	}

	return "";
}

void ZCore::CreateWaypointSendData(int ref_id, std::vector<waypoint> &waypoint_list, char* &data, int &size)
{
	char *message;
	int waypoint_amount = waypoint_list.size();

	//make mem
	size = 8 + (waypoint_amount * sizeof(waypoint));
	data = message = (char*)malloc(size);

	//push header, the ref id of this object and the number of waypoints
	((int*)message)[0] = ref_id;
	((int*)message)[1] = waypoint_amount;

	//populate
	message += 8;
	for(std::vector<waypoint>::iterator j=waypoint_list.begin();j!=waypoint_list.end();j++)
	{
		memcpy(message, &(*j), sizeof(waypoint));
		message += sizeof(waypoint);
	}
}

ZObject* ZCore::ProcessWaypointData(char *data, int size, bool is_server, int ok_team)
{
	//does it hold the header info?
	if(size < 8)
	{
		return NULL;
	}

	//get header
	int ref_id = ((int*)data)[0];
	int waypoint_amount = ((int*)data)[1];
	int expected_packet_size = 8 + (waypoint_amount * sizeof(waypoint));

	//should we toss this packet for bad data?
	if(size != expected_packet_size)
	{
		return NULL;
	}

	//find our object
	ZObject* our_object = GetObjectFromID(ref_id, object_list);
	if(!our_object)
	{
		return NULL;
	}

	//ok team?
	if(is_server && our_object->GetOwner() == NULL_TEAM)
	{
		return NULL;
	}
	if(is_server && our_object->GetOwner() != ok_team)
	{
		return NULL;
	}

	//can set waypoints?
	if(is_server && !our_object->CanSetWaypoints())
	{
		return NULL;
	}

	//is this object a minion?
	if(is_server && our_object->IsMinion())
	{
		return NULL;
	}

	//clear this objects waypoint list
	if(is_server && !our_object->CanOverwriteWP())
	{
		//keep the first one because we are not allowed to 
		//write over it
		if(our_object->GetWayPointList().size())
		{
			waypoint first_waypoint = *our_object->GetWayPointList().begin();
			our_object->GetWayPointList().clear();
			our_object->GetWayPointList().push_back(first_waypoint);
		}
	}
	else
	{
		our_object->GetWayPointList().clear();
	}

	//begin the waypoint push
	data += 8;
	for(int i=0;i<waypoint_amount;i++)
	{
		waypoint new_waypoint;

		memcpy(&new_waypoint, data, sizeof(waypoint));

		if(!is_server || CheckWaypoint(our_object, &new_waypoint))
		{
			our_object->GetWayPointList().push_back(new_waypoint);
		}

		data += sizeof(waypoint);
	}

	return our_object;
}

ZObject* ZCore::ProcessRallypointData(char *data, int size, bool is_server, int ok_team)
{
	//does it hold the header info?
	if(size < 8)
	{
		return NULL;
	}

	//get header
	int ref_id = ((int*)data)[0];
	int waypoint_amount = ((int*)data)[1];

	//should we toss this packet for bad data?
	int expected_packet_size = 8 + (waypoint_amount * sizeof(waypoint));
	if(size != expected_packet_size)
	{
		return NULL;
	}

	//find our object
	ZObject* our_object = GetObjectFromID(ref_id, object_list);
	if(!our_object)
	{
		return NULL;
	}

	//ok team?
	if(is_server && our_object->GetOwner() == NULL_TEAM)
	{
		return NULL;
	}
	if(is_server && our_object->GetOwner() != ok_team)
	{
		return NULL;
	}

	//can set rallypoints?
	if(!our_object->CanSetRallypoints())
	{
		return NULL;
	}

	//clear
	our_object->GetRallyPointList().clear();

	//begin the waypoint push
	data += 8;
	for(int i=0;i<waypoint_amount;i++)
	{
		waypoint new_waypoint;

		memcpy(&new_waypoint, data, sizeof(waypoint));

		if(!is_server || CheckRallypoint(our_object, &new_waypoint))
		{
			our_object->GetRallyPointList().push_back(new_waypoint);
		}

		data += sizeof(waypoint);
	}

	return our_object;
}

void ZCore::DeleteObjectCleanUp(ZObject *obj)
{

}

bool ZCore::CannonPlacable(ZObject *building_obj, int tx, int ty)
{	
	if(!building_obj)
	{
		return false;
	}

	int x = tx * 16;
	int y = ty * 16;
	int right = x + 32;
	int bottom = y + 32;

	//within the map at all?
	if(tx < 0)
	{
		return false;
	}
	if(ty < 0)
	{
		return false;
	}
	if(tx+1 >= zmap.GetMapBasics().width)
	{
		return false;
	}
	if(ty+1 >= zmap.GetMapBasics().height)
	{
		return false;
	}

	//it within the zone? (is there a zone?)
	if(!building_obj->GetConnectedZone())
	{
		return false;
	}
	if(x < building_obj->GetConnectedZone()->x + 16)
	{
		return false;
	}
	if(y < building_obj->GetConnectedZone()->y + 16)
	{
		return false;
	}
	if(x + 32 > building_obj->GetConnectedZone()->x + building_obj->GetConnectedZone()->w - 16)
	{
		return false;
	}
	if(y + 32 > building_obj->GetConnectedZone()->y + building_obj->GetConnectedZone()->h - 16)
	{
		return false;
	}

	//it over lap any particular objects like buildings or other cannons?
	for(ZObject* i : object_list)
	{
		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		if((ot == VEHICLE_OBJECT) || (ot == ROBOT_OBJECT))
		{
			continue;
		}

		if(i->CannonNotPlacable(x, right, y, bottom))
		{
			return false;
		}
	}

	//is it all over good terrain?
	planet_type tt = (planet_type)zmap.GetMapBasics().terrain_type;
	if(zmap.GetMapPaletteTileInfo(tt, zmap.GetTile(x,y).tile).is_water)
	{
		return false;
	}
	if(zmap.GetMapPaletteTileInfo(tt, zmap.GetTile(x,y+16).tile).is_water)
	{
		return false;
	}
	if(zmap.GetMapPaletteTileInfo(tt, zmap.GetTile(x+16,y).tile).is_water)
	{
		return false;
	}
	if(zmap.GetMapPaletteTileInfo(tt, zmap.GetTile(x+16,y+16).tile).is_water)
	{
		return false;
	}

	if(!zmap.GetMapPaletteTileInfo(tt, zmap.GetTile(x,y).tile).is_passable)
	{
		return false;
	}
	if(!zmap.GetMapPaletteTileInfo(tt, zmap.GetTile(x,y+16).tile).is_passable)
	{
		return false;
	}
	if(!zmap.GetMapPaletteTileInfo(tt, zmap.GetTile(x+16,y).tile).is_passable)
	{
		return false;
	}
	if(!zmap.GetMapPaletteTileInfo(tt, zmap.GetTile(x+16,y+16).tile).is_passable)
	{
		return false;
	}

	return true;
}

bool ZCore::AreaIsFortTurret(int tx, int ty)
{
	int x = tx * 16;
	int y = ty * 16;
	int right = x + 32;
	int bottom = y + 32;

	for(ZObject* i : object_list)
	{
		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		if(!(ot == BUILDING_OBJECT && (oid == FORT_FRONT || oid == FORT_BACK)))
		{
			continue;
		}

		if(i->WithinSelection(x, right, y, bottom) && !i->CannonNotPlacable(x, right, y, bottom))
		{
			return true;
		}
	}

	return false;
}

void ZCore::ResetUnitLimitReached()
{
	for(int i=0;i<MAX_TEAM_TYPES;i++)
	{
		unit_limit_reached[i] = false;
		team_units_available[i] = 0;
	}
}

bool ZCore::CheckUnitLimitReached()
{
	bool change_made = false;

	//clear
	for(int i=0;i<MAX_TEAM_TYPES;i++)
	{
		team_units_available[i] = 0;
	}

	for(ZObject* obj : object_list)
	{
		unsigned char ot, oid;
		obj->GetObjectID(ot, oid);

		if(!(ot == ROBOT_OBJECT || ot == VEHICLE_OBJECT || ot == CANNON_OBJECT))
		{
			continue;
		}

		team_units_available[obj->GetOwner()]++;
	}

	for(int i=1;i<MAX_TEAM_TYPES;i++)
	{
		if(team_units_available[i] >= max_units_per_team)
		{
			if(!unit_limit_reached[i]) change_made = true;
			unit_limit_reached[i] = true;
		}
		else
		{
			if(unit_limit_reached[i]) change_made = true;
			unit_limit_reached[i] = false;
		}
	}

	return change_made;
}