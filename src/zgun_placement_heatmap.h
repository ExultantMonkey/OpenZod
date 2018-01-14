#ifndef _ZGUN_PLACEMENT_HEATMAP_H_
#define _ZGUN_PLACEMENT_HEATMAP_H_

#include "zsdl_opengl.h"
#include <vector>
#include <utility>

using namespace std;

class ZCore;
class ZObject;
class ZMap;
class ZSettings;
class ZOLists;
class map_zone_info;

class ZHeatMapBase
{
public:
	ZHeatMapBase();
	
	virtual void Process(double the_time, ZOLists &ols, ZMap &tmap, int our_team) {}
	virtual bool ShouldReset(ZMap &tmap);
	virtual bool DoReset(ZMap &tmap);
	virtual bool ShouldClear(int our_team);
	virtual bool DoClear(int our_team = -1);
	
	int GetHeatMapSize() { return heatmap_size; }
	float *GetHeatMapData() { return heatmap; }
	
protected:
	void AddHeat(ZMap &tmap, int cx, int cy, int heat_tile_dist, float weight, bool heat_stacks = true);
	
	float *heatmap;
	int heatmap_size;
	int last_team;
};

class ZGunPlacementHeatMap
{
public:
	ZGunPlacementHeatMap();
	~ZGunPlacementHeatMap();
	
	void Process(double the_time, ZOLists &ols, ZMap &tmap, int our_team);
	void ProcessFinalHeatMap(double the_time, ZOLists &ols, ZMap &tmap, int our_team);
	void DoRender(ZMap &tmap);
	bool FindCannonPlace(ZCore *zcore, ZMap &tmap, ZSettings &zsettings, ZObject *bo, int coid, int &tx, int &ty);
	
	//void FindGunPlaceForZone();
private:
	bool LazyCreateRedTile();
	
	vector<ZHeatMapBase*> hm_list;
	ZHeatMapBase hm_final;
	double time_till_next_process;
	
	ZSDL_Surface red_tile;
};

class ZFlagHeatMap : public ZHeatMapBase
{
public:
	ZFlagHeatMap() : ZHeatMapBase() {}
	
	void Process(double the_time, ZOLists &ols, ZMap &tmap, int our_team);
};

class ZUnitHistoryHeatMap : public ZHeatMapBase
{
public:
	ZUnitHistoryHeatMap() : ZHeatMapBase() 
	{ 
		last_process_time = 0; 
		set_last_process_time = false;
	}
	
	void Process(double the_time, ZOLists &ols, ZMap &tmap, int our_team);
	
private:
	double last_process_time;
	bool set_last_process_time;
};

class ZBuildingHeatMap : public ZHeatMapBase
{
public:
	ZBuildingHeatMap() : ZHeatMapBase() {}
	
	void Process(double the_time, ZOLists &ols, ZMap &tmap, int our_team);
};

class ZOurCannonHeatMap : public ZHeatMapBase
{
public:
	ZOurCannonHeatMap() : ZHeatMapBase() {}
	
	void Process(double the_time, ZOLists &ols, ZMap &tmap, int our_team);
};

#endif
