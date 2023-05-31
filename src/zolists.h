#ifndef _ZOLISTS_H_
#define _ZOLISTS_H_

#include <vector>

class ZObject;

class ZOLists
{
public:
	ZOLists();

	void Init(std::vector<ZObject*> *object_list_) { object_list = object_list_; }

	void AddObject(ZObject *obj);
	void DeleteObject(ZObject *obj);
	void RemoveObject(ZObject *obj);
	static void DeleteObjectFromList(ZObject *obj, std::vector<ZObject*> *olist);
	static void RemoveObjectFromList(ZObject *obj, std::vector<ZObject*> *olist);
	void DeleteAllObjects();

	void SetupFlagList();

	std::vector<ZObject*> *object_list;
	std::vector<ZObject*> flag_olist;
	std::vector<ZObject*> cannon_olist;
	std::vector<ZObject*> building_olist;
	std::vector<ZObject*> rock_olist;
	std::vector<ZObject*> passive_engagable_olist;
	std::vector<ZObject*> mobile_olist;
	std::vector<ZObject*> prender_olist;
	std::vector<ZObject*> non_mapitem_olist;
	std::vector<ZObject*> grenades_olist;
};

#endif
