#ifndef _ZSETTINGS_H_
#define _ZSETTINGS_H_

#include <string>

#include "zmap.h"
#include "constants.h"

#include "Core/UnitSettings.h"

#pragma pack(1)
class ZSettings
{
public:
	ZSettings();
	void SetDefaults();
	bool LoadSettings(std::string filename);
	bool SaveSettings(std::string filename);
	OpenZod::Core::UnitSettings &GetUnitSettings(unsigned char ot, unsigned char oid);

	double fort_building_health;
	double robot_building_health;
	double vehicle_building_health;
	double repair_building_health;
	double radar_building_health;
	double bridge_building_health;

	double rock_item_health;
	double grenades_item_health;
	double rockets_item_health;
	double hut_item_health;
	double map_item_health;

	double grenade_damage;
	int grenade_damage_radius;
	int grenade_missile_speed;
	double grenade_attack_speed;
	double map_item_turrent_damage;

	int agro_distance;
	int auto_grab_vehicle_distance;
	int auto_grab_flag_distance;
	int building_auto_repair_time;
	int building_auto_repair_random_additional_time;
	int max_turrent_horizontal_distance;
	int max_turrent_vertical_distance;
	int grenades_per_box;
	double partially_damaged_unit_speed;
	double damaged_unit_speed;
	double run_unit_speed;
	double run_recharge_rate;
	int hut_animal_max;
	int hut_animal_min;
	int hut_animal_roam_distance;

private:
	void CensorSettings();
	OpenZod::Core::UnitSettings robot_settings[MAX_ROBOT_TYPES];
	OpenZod::Core::UnitSettings vehicle_settings[MAX_VEHICLE_TYPES];
	OpenZod::Core::UnitSettings cannon_settings[MAX_CANNON_TYPES];
};

#pragma pack()

#endif
