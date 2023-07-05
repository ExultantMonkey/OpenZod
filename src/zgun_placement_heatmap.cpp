#include "zgun_placement_heatmap.h"
#include "zobject.h"
#include "zmap.h"
#include "zcore.h"
#include "zsettings.h"
#include "zolists.h"

#include "Util/Math.h"

ZGunPlacementHeatMap::ZGunPlacementHeatMap()
{
	hm_list.push_back(new ZFlagHeatMap());
	hm_list.push_back(new ZUnitHistoryHeatMap());
	hm_list.push_back(new ZBuildingHeatMap());
	hm_list.push_back(new ZOurCannonHeatMap());
	
	time_till_next_process = 0;
}

ZGunPlacementHeatMap::~ZGunPlacementHeatMap()
{
	for(std::vector<ZHeatMapBase*>::iterator hmi=hm_list.begin();hmi!=hm_list.end();++hmi)
	{		
		if(*hmi) free(*hmi);
	}
	
	hm_list.clear();
}

void ZGunPlacementHeatMap::Process(double the_time, ZOLists &ols, ZMap &tmap, int our_team)
{
	const double process_time_inc = 0.25;

	//heatmap processing is expensive
	if(fabs(the_time - time_till_next_process) < process_time_inc) return;
	
	time_till_next_process = the_time + process_time_inc;
	
	//process heatmaps
	for(std::vector<ZHeatMapBase*>::iterator hmi=hm_list.begin();hmi!=hm_list.end();++hmi)
	{
		ZHeatMapBase* hm = *hmi;
		
		if(!hm) continue;
		
		if(hm->ShouldReset(tmap))
			hm->DoReset(tmap);
		
		if(hm->ShouldClear(our_team))
			hm->DoClear(our_team);
		
		hm->Process(the_time, ols, tmap, our_team);
	}
	
	ProcessFinalHeatMap(the_time, ols, tmap, our_team);
}

void ZGunPlacementHeatMap::ProcessFinalHeatMap(double the_time, ZOLists &ols, ZMap &tmap, int our_team)
{
	//reset?
	if(hm_final.ShouldReset(tmap))
		hm_final.DoReset(tmap);
	
	//clear
	hm_final.DoClear();
	
	//add
	int hm_size = hm_final.GetHeatMapSize();
	float *hm_data = hm_final.GetHeatMapData();
	//float max_val = 0;
	
	//checks
	if(!hm_size) return;
	if(!hm_data) return;
	
	for(std::vector<ZHeatMapBase*>::iterator hmi=hm_list.begin();hmi!=hm_list.end();++hmi)
	{
		if((*hmi)->GetHeatMapSize() != hm_size) return;
		if(!(*hmi)->GetHeatMapData()) return;
	}
	
	//do add
	for(int i=0;i<hm_size;i++)
	{
		for(std::vector<ZHeatMapBase*>::iterator hmi=hm_list.begin();hmi!=hm_list.end();++hmi)
			hm_data[i] += (*hmi)->GetHeatMapData()[i];
		
		//if(hm_data[i] > max_val)
		//	max_val = hm_data[i];
	}
	
	//make sure nothing under 0
	#pragma omp simd
	for(int i=0;i<hm_size;i++)
	{
		if(hm_data[i] < 0)
			hm_data[i] = 0;
	}
	
	//normalize
	//if(max_val > 1)
	//	for(int i=0;i<hm_size;i++)
	//		hm_data[i] /= max_val;
}

bool ZGunPlacementHeatMap::FindCannonPlace(ZCore *zcore, ZMap &tmap, ZSettings &zsettings, ZObject *bo, int coid, int &tx, int &ty)
{
	int hm_size = hm_final.GetHeatMapSize();
	float *hm_data = hm_final.GetHeatMapData();
	
	//final heatmap data not loaded?
	if(hm_size != tmap.GetMapBasics().width * tmap.GetMapBasics().height) return false;
	if(hm_size <= 0) return false;
	if(!hm_data) return false;
	
	//no building object?
	if(!bo) return false;
	
	//no zcore?
	if(!zcore) return false;
	
	//bad cannon object id?
	if(coid < 0) return false;
	if(coid >= MAX_CANNON_TYPES) return false;
	
	//no zone?
	map_zone_info *z = bo->GetConnectedZone();
	if(!z) return false;
	
	int zxts = z->x / 16;
	int zyts = z->y / 16;
	int zxte = zxts + (z->w / 16);
	int zyte = zyts + (z->h / 16);
	
	zxts += 1;
	zyts += 1;
	zxte -= 3;
	zyte -= 3;
	
	//going past map dimensions?
	if(zxts < 0) zxts = 0;
	if(zyts < 0) zyts = 0;
	if(zxte >= tmap.GetMapBasics().width) zxte = tmap.GetMapBasics().width;
	if(zyte >= tmap.GetMapBasics().height) zyte = tmap.GetMapBasics().height;
	
	//bad dimensions?
	if(zxte - zxts < 2) return false;
	if(zyte - zyts < 2) return false;
	
	{
		std::map<float, std::vector< std::pair<int,int> > > results;
		int h = tmap.GetMapBasics().height;
		double c_attack_radius = zsettings.GetUnitSettings(CANNON_OBJECT, coid).attackRadius;
		int c_tile_dist = ceil((float)(zsettings.GetUnitSettings(CANNON_OBJECT, coid).attackRadius / 16));
		int c_tile_width = c_tile_dist * 2;
		int c_center = c_tile_dist * 16;
		int c_size = c_tile_width * c_tile_width;
		
		//cannon has range?
		if(c_tile_dist > 0)
		{
			//malloc cannon heat
			float *cheat = (float*)malloc(sizeof(float) * c_size);
			
			//setup cannon heat
			for(int i=0;i<c_tile_width;i++)
				for(int j=0;j<c_tile_width;j++)
				{
					float dist = OpenZod::Util::Math::DistanceBetweenPoints((i*16)+8, (j*16)+8, c_center, c_center);
					int cxyi = COMMON::xy_to_i(i,j,c_tile_width);
					
					//insanity check
					if(cxyi < 0) continue;
					if(cxyi >= c_size) continue;
					
					if(dist < c_attack_radius)
						cheat[cxyi] = (c_attack_radius - dist) / c_attack_radius;
					else
						cheat[cxyi] = 0;
				}
				
			//find the tile which removes the most heat
			for(int i=zxts;i<=zxte;i++)
				for(int j=zyts;j<=zyte;j++)
				{
					if(!zcore->CannonPlacable(bo, i ,j)) continue;
					
					float val = 0;
					
					for(int ci=0;ci<c_tile_width;ci++)
						for(int cj=0;cj<c_tile_width;cj++)
						{
							int i_off = i + (ci - (c_tile_dist-1));
							int j_off = j + (cj - (c_tile_dist-1));
							
							//basic check
							if(i_off < 0) continue;
							if(j_off < 0) continue;
							if(i_off >= tmap.GetMapBasics().width) continue;
							if(j_off >= tmap.GetMapBasics().height) continue;
							
							int cxyi = COMMON::xy_to_i(ci,cj,c_tile_width);
							int hm_xyi = COMMON::xy_to_i(i_off,j_off,h);
							
							//insanity check
							if(cxyi < 0) continue;
							if(cxyi >= c_size) continue;
							if(hm_xyi < 0) continue;
							if(hm_xyi >= hm_size) continue;
							
							float cur_c_heat = cheat[cxyi];
							float cur_fm_heat = hm_data[hm_xyi];
							
							if(cur_c_heat > cur_fm_heat)
								val += cur_fm_heat;
							else
								val += cur_c_heat;
						}
					
					
					if(val > 0) results[val].push_back(std::pair<int,int>(i,j));
				}
			
			//free cannon heat
			free(cheat);
		}
		else
		{
			//cannon doesn't have range?
			//just choose the highest heated area
			for(int i=zxts;i<=zxte;i++)
				for(int j=zyts;j<=zyte;j++)
				{
					if(!zcore->CannonPlacable(bo, i ,j)) continue;
					
					float val = hm_data[COMMON::xy_to_i(i,j,h)] + hm_data[COMMON::xy_to_i(i+1,j,h)] +
									hm_data[COMMON::xy_to_i(i,j+1,h)] + hm_data[COMMON::xy_to_i(i+1,j+1,h)];
									
					if(val > 0) results[val].push_back(std::pair<int,int>(i,j));
				}
		}
		
		if(results.size())
		{
			std::map<float, std::vector< std::pair<int,int> > >::reverse_iterator r = results.rbegin();
			
			float best_val = r->first;
			
			//build list of choices within % of top choices
			if(best_val > 0)
			{
				std::vector< std::pair<int,int> > choose_list = r->second;

				for(r++;r!=results.rend();++r)
					if(fabs((best_val - r->first) / best_val) < 0.005)
						choose_list.insert(choose_list.end(), r->second.begin(), r->second.end());
					
				if(choose_list.size())
				{
					std::pair<int,int> choosen = choose_list[rand() % choose_list.size()];
					
					tx = choosen.first;
					ty = choosen.second;
				}
			}
		}
	}
	
	return true;
}

bool ZGunPlacementHeatMap::LazyCreateRedTile()
{
	if(!red_tile.GetBaseSurface())
	{
		red_tile.LoadNewSurface(16, 16);
		
		//couldn't create a new surface?
		if(!red_tile.GetBaseSurface())
			return false;
		
		red_tile.MakeAlphable();
		
		SDL_Rect trect;
	
		trect.x = 0;
		trect.y = 0;
		trect.w = 16;
		trect.h = 16;
		
		red_tile.FillRectOnToMe(&trect, 255, 0, 0);
	}
	
	return true;
}

void ZGunPlacementHeatMap::DoRender(ZMap &tmap)
{
	int shift_x, shift_y, view_w, view_h;
	
	//lazy create red tile
	if(!LazyCreateRedTile())
		return;
	
	if(hm_final.ShouldReset(tmap))
		hm_final.DoReset(tmap);

	tmap.GetViewShiftFull(shift_x, shift_y, view_w, view_h);
	
	//if(hm_list.size()==2 && hm_list[1]) return;
	//ZHeatMapBase *render_hm = hm_list[1];
	
	ZHeatMapBase *render_hm = &hm_final;
		
	int hm_size = render_hm->GetHeatMapSize();
	float *hm_data = render_hm->GetHeatMapData();
	int x,y;
	
	x=y=0;
	
	if(hm_size && hm_data)
		for(int i=0;i<hm_size;++i)
		{
			SDL_Rect trect;
			SDL_Rect srect;

			trect.x = (x*16) - shift_x;
			trect.y = (y*16) - shift_y;
			trect.w = 0;
			trect.h = 0;
			
			srect.x = 0;
			srect.y = 0;
			srect.w = 16;
			srect.h = 16;
			
			int sw = view_w - trect.x;
			int sh = view_h - trect.y;
			if(sw < 16) srect.w = sw;
			if(sh < 16) srect.h = sh;
			
			int alpha = 150 * hm_data[i];
			
			if(alpha>200) alpha = 200;
			
			if(sw > 0 && sh > 0)
			{
				red_tile.SetAlpha(alpha);
				red_tile.BlitSurface(&srect, &trect);
			}
			
			y++;
			if(y>=tmap.GetMapBasics().height)
			{
				y=0;
				x++;
			}
		}
}

ZHeatMapBase::ZHeatMapBase()
{
	heatmap = NULL;
	heatmap_size = 0;
	last_team = -1;
}

bool ZHeatMapBase::ShouldReset(ZMap &tmap)
{
	if(tmap.GetMapBasics().width * tmap.GetMapBasics().height != heatmap_size) return true;
	
	return false;
}

void ZHeatMapBase::DoReset(ZMap &tmap)
{
	heatmap_size = tmap.GetMapBasics().width * tmap.GetMapBasics().height;
	
	if(heatmap_size < 0) heatmap_size = 0;
	
	if(heatmap)
	{
		free(heatmap);
		heatmap = NULL;
	}
	
	if(heatmap_size)
	{
		heatmap = (float*)malloc(sizeof(float) * heatmap_size);
		
		DoClear();
	}
}

bool ZHeatMapBase::ShouldClear(int our_team)
{
	if(last_team != our_team) return true;
	
	return false;
}

void ZHeatMapBase::DoClear(int our_team)
{
	//clear data
	#pragma omp simd
	for(int i=0;i<heatmap_size;i++) heatmap[i] = 0;
	
	if(our_team != -1)
		last_team = our_team;
}

void ZHeatMapBase::AddHeat(ZMap &tmap, int cx, int cy, int heat_dist, float weight, bool heat_stacks)
{
	int heat_tile_dist = ceil((float)(heat_dist / 16));
	
	int tx = cx / 16;
	int ty = cy / 16;
	
	int sx, ex, sy, ey;
	
	sx = tx - heat_tile_dist;
	ex = tx + heat_tile_dist;
	sy = ty - heat_tile_dist;
	ey = ty + heat_tile_dist;
	
	if(sx < 0) sx = 0;
	if(sy < 0) sy = 0;
	if(ex >= tmap.GetMapBasics().width) ex = tmap.GetMapBasics().width-1;
	if(ey >= tmap.GetMapBasics().height) ey = tmap.GetMapBasics().height-1;
	
	for(int i=sx;i<=ex;i++)
		for(int j=sy;j<=ey;j++)
		{
			int xyi = COMMON::xy_to_i(i,j,tmap.GetMapBasics().height);
			
			if(xyi < 0) continue;
			if(xyi >= heatmap_size) continue;
			
			double dist = OpenZod::Util::Math::DistanceBetweenPoints(cx, cy, (i*16)+8, (j*16)+8);
			
			//tile outside of heat range?
			if(dist > heat_dist) continue;
			
			float heat = weight * ((heat_dist - dist) / heat_dist);

			//add heat
			if(heat_stacks)
				heatmap[xyi] += heat;
			else
			{
				if(weight > 0)
				{
					if(heat > heatmap[xyi])
						heatmap[xyi] = heat;
				}
				else if(heat < heatmap[xyi])
					heatmap[xyi] = heat;
			}
		}
}

void ZFlagHeatMap::Process(double the_time, ZOLists &ols, ZMap &tmap, int our_team)
{
	const int heat_tile_dist = 6;
	
	//use optimized lists
	//if(!ols.object_list) return;
	//vector<ZObject*> &object_list = *ols.object_list;
	
	//clear data
	DoClear();
	
	//for all our flags add heat
	for(std::vector<ZObject*>::iterator o=ols.flag_olist.begin(); o!=ols.flag_olist.end(); o++)
	{
		unsigned char ot, oid;
		
		(*o)->GetObjectID(ot, oid);
		
		//a flag?
		if(ot != MAP_ITEM_OBJECT) continue;
		if(oid != FLAG_ITEM) continue;
		
		//add the heat
		int ox, oy;
		
		(*o)->GetCords(ox, oy);
		
		AddHeat(tmap, ox+8, oy+8, heat_tile_dist * 16, 1.25);
	}
}

void ZUnitHistoryHeatMap::Process(double the_time, ZOLists &ols, ZMap &tmap, int our_team)
{
	const int heat_tile_dist = 6;
	const double time_inc = 0.01;
	const double time_dec = pow(0.2, (1 / (2 * 60 * (1 / time_inc))));
	const double clear_threshold = pow(time_dec, (5 * 60 * (1 / time_inc)));
	
	//have time_dec set to reduce the value to 20% and 2 minutes
	//after 5 minutes it should clear whatever is there
	
	//use optimized lists
	//if(!ols.object_list) return;
	//vector<ZObject*> &object_list = *ols.object_list;
	
	//reduce heat
	if(set_last_process_time)
	{
		if(the_time - last_process_time > time_inc)
		{
			double dec_amount = 1;
			
			//accumulate all the decrements
			while(the_time - last_process_time > time_inc)
			{
				dec_amount *= time_dec;
				last_process_time += time_inc;
			}
			
			//decrement
			#pragma omp simd
			for(int i=0;i<heatmap_size;i++) 
			{
				heatmap[i] *= dec_amount;
				
				if(heatmap[i] < clear_threshold)
					heatmap[i] = 0;
			}
		}
	}
	else
	{
		last_process_time = the_time;
		set_last_process_time = true;
	}
	
	//add in current enemy units
	for(std::vector<ZObject*>::iterator o=ols.mobile_olist.begin(); o!=ols.mobile_olist.end(); o++)
	{
		unsigned char ot, oid;
		
		//only enemy units
		if((*o)->GetOwner() == our_team) continue;
		if((*o)->GetOwner() == NULL_TEAM) continue;
		
		(*o)->GetObjectID(ot, oid);
		
		//a robot / vehicle?
		if(ot != ROBOT_OBJECT && ot != VEHICLE_OBJECT) continue;
		
		//add the heat
		int ox, oy;
		
		(*o)->GetCenterCords(ox, oy);
		
		//unit heat doesn't stack
		AddHeat(tmap, ox, oy, heat_tile_dist * 16, 0.75, false);
	}
}

void ZBuildingHeatMap::Process(double the_time, ZOLists &ols, ZMap &tmap, int our_team)
{
	const int enemy_heat_tile_dist = 3;
	const int fort_heat_tile_dist = 3;
	
	//use optimized lists
	//if(!ols.object_list) return;
	//vector<ZObject*> &object_list = *ols.object_list;
	
	//clear data
	DoClear();
	
	//for all enemy building entrances
	//and our fort entrances
	for(std::vector<ZObject*>::iterator o=ols.building_olist.begin(); o!=ols.building_olist.end(); o++)
	{
		unsigned char ot, oid;
		ZObject* oo = *o;
		
		oo->GetObjectID(ot, oid);
		
		//a building that can build?
		if(ot != BUILDING_OBJECT) continue;
		if(oid == RADAR) continue;
		if(oid == REPAIR) continue;
		if(oid == BRIDGE_VERT) continue;
		if(oid == BRIDGE_HORZ) continue;
		
		//our fort?
		if(oo->GetOwner() == our_team)
		{
			int cx, cy;
			
			//only interested in forts
			if(oid != FORT_FRONT && oid != FORT_BACK) continue;

			if(oo->GetBuildingCreationPoint(cx, cy))
				AddHeat(tmap, cx, cy, fort_heat_tile_dist * 16, 2);
			
			if(oo->GetBuildingCreationMovePoint(cx, cy))
				AddHeat(tmap, cx, cy, fort_heat_tile_dist * 16, 2);
		}
		else if(oo->GetOwner() != NULL_TEAM)
		{
			int cx, cy;
			
			if(oo->GetBuildingCreationPoint(cx, cy))
				AddHeat(tmap, cx, cy, enemy_heat_tile_dist * 16, 0.25);
			
			if(oo->GetBuildingCreationMovePoint(cx, cy))
				AddHeat(tmap, cx, cy, enemy_heat_tile_dist * 16, 0.25);
			
			//virtual bool GetBuildingCreationPoint(int &x, int &y);
			//virtual bool GetBuildingCreationMovePoint(int &x, int &y);
			//GetCraneCenter
			//GetCraneEntrance
		}
	}
}

void ZOurCannonHeatMap::Process(double the_time, ZOLists &ols, ZMap &tmap, int our_team)
{
	//use optimized lists
	//if(!ols.object_list) return;
	//vector<ZObject*> &object_list = *ols.object_list;
	
	//clear data
	DoClear();
	
	//add in negative heat from cannons
	for(std::vector<ZObject*>::iterator o=ols.cannon_olist.begin(); o!=ols.cannon_olist.end(); o++)
	{
		unsigned char ot, oid;
		
		//only our cannons
		if((*o)->GetOwner() != our_team) continue;
		
		(*o)->GetObjectID(ot, oid);
		
		//not a cannon?
		if(ot != CANNON_OBJECT) continue;
		
		//add the heat
		int ox, oy;
		
		(*o)->GetCenterCords(ox, oy);
		
		AddHeat(tmap, ox, oy, (*o)->GetAttackRadius(), -1);
	}
}

