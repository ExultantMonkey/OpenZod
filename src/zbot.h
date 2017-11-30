#ifndef _ZBOT_H_
#define _ZBOT_H_

#include "zclient.h"

class PreferredUnit;
class BuildingUnit;
class BuildCombo;

class ZBot : public ZClient
{
	public:
		ZBot();
		
		void Setup();
		void Run();
	private:
		void SetupEHandler();

		void ProcessSocketEvents();

		void ProcessAI();

		//AI 1
		bool Stage1AI();
		bool Stage2AI();

		//AI 2
		bool Stage1AI_2();
		void GiveOutOrders_2(vector<ZObject*> unit_list, vector<ZObject*> target_list, vector<ZObject*> &repair_building_list, vector<ZObject*> &grenade_box_list);

		//AI 3
		bool Stage1AI_3();
		void CollectOurUnits_3(vector<ZObject*> &units_list, vector<ZObject*> &targeted_list);
		void CollectOurTargets_3(vector<ZObject*> &targets_list, bool all_out);
		void ReduceUnitsToPercent(vector<ZObject*> &units_list, double max_percent);
		void RemoveTargetedFromTargets(vector<ZObject*> &targets_list, vector<ZObject*> &targeted_list);
		void MatchTargets_3(vector<ZObject*> &units_list, vector<ZObject*> &targets_list);
		void GiveOutOrders_3(vector<ZObject*> &units_list, vector<ZObject*> &targets_list);
		bool GoAllOut_3(double &percent_to_order, double &order_delay);

		void ChooseBuildOrders();
		void ChooseBuildOrders_2();
		BuildCombo GetBestBuildCombo(vector<ZObject*> &b_list, vector<PreferredUnit> &pu_list);
		bool GetBestBuildComboIncCI(vector<int> &ci, int max_pu);
		bool GetBestBuildComboBuildretn(vector<int> &ci, vector<ZObject*> &b_list, vector<PreferredUnit> &pu_list, BuildCombo &retn);
		bool CanBuildAt(ZObject *b);
		void AddBuildingProductionSums(ZObject *b, vector<PreferredUnit> &preferred_build_list);

		void SendBotDevWaypointList(ZObject *obj);
		
		EventHandler<ZBot> ehandler;
		
		static void nothing_event(ZBot *p, char *data, int size, int dummy);
		static void test_event(ZBot *p, char *data, int size, int dummy);
		static void connect_event(ZBot *p, char *data, int size, int dummy);
		static void disconnect_event(ZBot *p, char *data, int size, int dummy);
		static void store_map_event(ZBot *p, char *data, int size, int dummy);
		static void add_new_object_event(ZBot *p, char *data, int size, int dummy);
		static void set_zone_info_event(ZBot *p, char *data, int size, int dummy);
		static void display_news_event(ZBot *p, char *data, int size, int dummy);
		static void set_object_waypoints_event(ZBot *p, char *data, int size, int dummy);
		static void set_object_rallypoints_event(ZBot *p, char *data, int size, int dummy);
		static void set_object_loc_event(ZBot *p, char *data, int size, int dummy);
		static void set_object_team_event(ZBot *p, char *data, int size, int dummy);
		static void set_object_attack_object_event(ZBot *p, char *data, int size, int dummy);
		static void delete_object_event(ZBot *p, char *data, int size, int dummy);
		static void set_object_health_event(ZBot *p, char *data, int size, int dummy);
		static void end_game_event(ZBot *p, char *data, int size, int dummy);
		static void reset_game_event(ZBot *p, char *data, int size, int dummy);
		static void fire_object_missile_event(ZBot *p, char *data, int size, int dummy);
		static void destroy_object_event(ZBot *p, char *data, int size, int dummy);
		static void set_building_state_event(ZBot *p, char *data, int size, int dummy);
		static void set_building_cannon_list_event(ZBot *p, char *data, int size, int dummy);
		static void set_computer_message_event(ZBot *p, char *data, int size, int dummy);
		static void set_object_group_info_event(ZBot *p, char *data, int size, int dummy);
		static void do_crane_anim_event(ZBot *p, char *data, int size, int dummy);
		static void set_repair_building_anim_event(ZBot *p, char *data, int size, int dummy);
		static void set_settings_event(ZBot *p, char *data, int size, int dummy);
		static void set_lid_open_event(ZBot *p, char *data, int size, int dummy);
		static void snipe_object_event(ZBot *p, char *data, int size, int dummy);
		static void driver_hit_effect_event(ZBot *p, char *data, int size, int dummy);
		static void clear_player_list_event(ZBot *p, char *data, int size, int dummy);
		static void add_player_event(ZBot *p, char *data, int size, int dummy);
		static void delete_player_event(ZBot *p, char *data, int size, int dummy);
		static void set_player_name_event(ZBot *p, char *data, int size, int dummy);
		static void set_player_team_event(ZBot *p, char *data, int size, int dummy);
		static void set_player_mode_event(ZBot *p, char *data, int size, int dummy);
		static void set_player_ignored_event(ZBot *p, char *data, int size, int dummy);
		static void set_player_loginfo_event(ZBot *p, char *data, int size, int dummy);
		static void set_player_voteinfo_event(ZBot *p, char *data, int size, int dummy);
		static void update_game_paused_event(ZBot *p, char *data, int size, int dummy);
		static void update_game_speed_event(ZBot *p, char *data, int size, int dummy);
		static void set_vote_info_event(ZBot *p, char *data, int size, int dummy);
		static void set_player_id_event(ZBot *p, char *data, int size, int dummy);
		static void set_selectable_map_list_event(ZBot *p, char *data, int size, int dummy);
		static void display_login_event(ZBot *p, char *data, int size, int dummy);
		static void set_grenade_amount_event(ZBot *p, char *data, int size, int dummy);
		static void set_team_event(ZBot *p, char *data, int size, int dummy);
		static void set_build_queue_list_event(ZBot *p, char *data, int size, int dummy);

		vector<ZObject*> flag_object_list;

		vector<PreferredUnit> preferred_build_list;
		double next_ai_time;
		double last_order_time;
};

class PreferredUnit
{
public:
	PreferredUnit()
	{
		ot = (unsigned char)-1;
		oid = (unsigned char)-1;
		p_value = 1;
		in_production = 0;
	}
	
	PreferredUnit(unsigned char ot_, unsigned char oid_, double p_value_ = 1.0) 
	{
		ot = ot_; 
		oid = oid_;
		p_value = p_value_;
		in_production = 0;
	}
	
	unsigned char ot, oid;
	double p_value;
	int in_production;
};

class BuildingUnit
{
public:
	ZObject* b;
	unsigned char pot, poid;
};

class BuildCombo
{
public:
	void Debug();
	
	vector<BuildingUnit> b_list;
	double target_distance;
};

#endif
