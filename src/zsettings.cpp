#include "zsettings.h"
#include "Core/UnitSettings.h"
#include "common.h"

#include <stdio.h>

ZSettings::ZSettings()
{
	SetDefaults();
}

void ZSettings::SetDefaults()
{
	int oid;
	const double run_past_radius = 1.3;

	//robots
	oid = GRUNT;
	robot_settings[oid].groupAmount = 3;
	robot_settings[oid].moveSpeed = 14;
	robot_settings[oid].attackRadius = 120;
	robot_settings[oid].attackDamage = 0.0011046;
	robot_settings[oid].attackDamageChance = 0.7;
	robot_settings[oid].attackDamageRadius = 0;
	robot_settings[oid].attackMissileSpeed = 0;
	robot_settings[oid].attackSpeed = 0.5;
	robot_settings[oid].attackSnipeChance = 0.3;
	robot_settings[oid].health = 8.0 / 74;
	robot_settings[oid].buildTime = 1 * 60 + 12;
	robot_settings[oid].maxRunTime = run_past_radius * robot_settings[oid].attackRadius / robot_settings[oid].moveSpeed;

	oid = PSYCHO;
	robot_settings[oid].groupAmount = 3;
	robot_settings[oid].moveSpeed = 12;
	robot_settings[oid].attackRadius = 120;
	robot_settings[oid].attackDamage = 0.002617;
	robot_settings[oid].attackDamageChance = 0.65;
	robot_settings[oid].attackDamageRadius = 0;
	robot_settings[oid].attackMissileSpeed = 0;
	robot_settings[oid].attackSpeed = 0.1;
	robot_settings[oid].attackSnipeChance = 0.3;
	robot_settings[oid].health = 13.0 / 74;
	robot_settings[oid].buildTime = 1 * 60 + 38;
	robot_settings[oid].maxRunTime = run_past_radius * robot_settings[oid].attackRadius / robot_settings[oid].moveSpeed;

	oid = SNIPER;
	robot_settings[oid].groupAmount = 3;
	robot_settings[oid].moveSpeed = 14;
	robot_settings[oid].attackRadius = 144;
	robot_settings[oid].attackDamage = 0.007008;
	robot_settings[oid].attackDamageChance = 0.8;
	robot_settings[oid].attackDamageRadius = 0;
	robot_settings[oid].attackMissileSpeed = 0;
	robot_settings[oid].attackSpeed = 0.4;
	robot_settings[oid].attackSnipeChance = 0.8;
	robot_settings[oid].health = 13.0 / 74;
	robot_settings[oid].buildTime = 2 * 60 + 28;
	robot_settings[oid].maxRunTime = run_past_radius * robot_settings[oid].attackRadius / robot_settings[oid].moveSpeed;
	
	oid = TOUGH;
	robot_settings[oid].groupAmount = 2;
	robot_settings[oid].moveSpeed = 12;
	robot_settings[oid].attackRadius = 120;
	robot_settings[oid].attackDamage = 40.0 / 240;
	robot_settings[oid].attackDamageChance = 0;
	robot_settings[oid].attackDamageRadius = 40;
	robot_settings[oid].attackMissileSpeed = 150;
	robot_settings[oid].attackSpeed = 1.442;
	robot_settings[oid].attackSnipeChance = 0.0;
	robot_settings[oid].health = 25.0 / 74;
	robot_settings[oid].buildTime = 1 * 60 + 56;
	robot_settings[oid].maxRunTime = 0.5 * run_past_radius * robot_settings[oid].attackRadius / robot_settings[oid].moveSpeed;
	
	oid = PYRO;
	robot_settings[oid].groupAmount = 4;
	robot_settings[oid].moveSpeed = 12;
	robot_settings[oid].attackRadius = 120;
	robot_settings[oid].attackDamage = 0.010486;
	robot_settings[oid].attackDamageChance = 0.7;
	robot_settings[oid].attackDamageRadius = 0;
	robot_settings[oid].attackMissileSpeed = 0;
	robot_settings[oid].attackSpeed = 0.1;
	robot_settings[oid].attackSnipeChance = 0.0;
	robot_settings[oid].health = 20.0 / 74;
	robot_settings[oid].buildTime = 2 * 60 + 41;
	robot_settings[oid].maxRunTime = run_past_radius * robot_settings[oid].attackRadius / robot_settings[oid].moveSpeed;
	
	oid = LASER;
	robot_settings[oid].groupAmount = 4;
	robot_settings[oid].moveSpeed = 14;
	robot_settings[oid].attackRadius = 136;
	robot_settings[oid].attackDamage = 0.017799;
	robot_settings[oid].attackDamageChance = 0.7;
	robot_settings[oid].attackDamageRadius = 0;
	robot_settings[oid].attackMissileSpeed = 0;
	robot_settings[oid].attackSpeed = 0.4;
	robot_settings[oid].attackSnipeChance = 0.6;
	robot_settings[oid].health = 15.0 / 74;
	robot_settings[oid].buildTime = 2 * 60 + 59;
	robot_settings[oid].maxRunTime = run_past_radius * robot_settings[oid].attackRadius / robot_settings[oid].moveSpeed;

	//vehicles
	oid = JEEP;
	vehicle_settings[oid].groupAmount = 0;
	vehicle_settings[oid].moveSpeed = 17;
	vehicle_settings[oid].attackRadius = 120;
	vehicle_settings[oid].attackDamage = 0.0027067;
	vehicle_settings[oid].attackDamageChance = 0.65;
	vehicle_settings[oid].attackDamageRadius = 0;
	vehicle_settings[oid].attackMissileSpeed = 0;
	vehicle_settings[oid].attackSpeed = 0.1;
	vehicle_settings[oid].attackSnipeChance = 0.4;
	vehicle_settings[oid].health = 13.0 / 74;
	vehicle_settings[oid].buildTime = 1 * 60 + 21;
	vehicle_settings[oid].maxRunTime = run_past_radius * vehicle_settings[oid].attackRadius / vehicle_settings[oid].moveSpeed;

	oid = LIGHT;
	vehicle_settings[oid].groupAmount = 0;
	vehicle_settings[oid].moveSpeed = 14;
	vehicle_settings[oid].attackRadius = 120;
	vehicle_settings[oid].attackDamage = 50.0 / 240;
	vehicle_settings[oid].attackDamageChance = 0;
	vehicle_settings[oid].attackDamageRadius = 40;
	vehicle_settings[oid].attackMissileSpeed = 225;
	vehicle_settings[oid].attackSpeed = 1.128;
	vehicle_settings[oid].attackSnipeChance = 0.0;
	vehicle_settings[oid].health = 25.0 / 74;
	vehicle_settings[oid].buildTime = 2 * 60 + 17;
	vehicle_settings[oid].maxRunTime = run_past_radius * vehicle_settings[oid].attackRadius / vehicle_settings[oid].moveSpeed;

	oid = MEDIUM;
	vehicle_settings[oid].groupAmount = 0;
	vehicle_settings[oid].moveSpeed = 12;
	vehicle_settings[oid].attackRadius = 128;
	vehicle_settings[oid].attackDamage = 80.0 / 240;
	vehicle_settings[oid].attackDamageChance = 0;
	vehicle_settings[oid].attackDamageRadius = 45;
	vehicle_settings[oid].attackMissileSpeed = 160;
	vehicle_settings[oid].attackSpeed = 2.336;
	vehicle_settings[oid].attackSnipeChance = 0.0;
	vehicle_settings[oid].health = 50.0 / 74;
	vehicle_settings[oid].buildTime = 3 * 60 + 45;
	vehicle_settings[oid].maxRunTime = run_past_radius * vehicle_settings[oid].attackRadius / vehicle_settings[oid].moveSpeed;

	oid = HEAVY;
	vehicle_settings[oid].groupAmount = 0;
	vehicle_settings[oid].moveSpeed = 9;
	vehicle_settings[oid].attackRadius = 144;
	vehicle_settings[oid].attackDamage = 120.0 / 240;
	vehicle_settings[oid].attackDamageChance = 0;
	vehicle_settings[oid].attackDamageRadius = 50;
	vehicle_settings[oid].attackMissileSpeed = 135;
	vehicle_settings[oid].attackSpeed = 4.088;
	vehicle_settings[oid].attackSnipeChance = 0.0;
	vehicle_settings[oid].health = 62.0 / 74;
	vehicle_settings[oid].buildTime = 5 * 60 + 9;
	vehicle_settings[oid].maxRunTime = 0.7 * run_past_radius * vehicle_settings[oid].attackRadius / vehicle_settings[oid].moveSpeed;

	oid = APC;
	vehicle_settings[oid].groupAmount = 0;
	vehicle_settings[oid].moveSpeed = 14;
	vehicle_settings[oid].attackRadius = 0;
	vehicle_settings[oid].attackDamage = 0;
	vehicle_settings[oid].attackDamageChance = 0;
	vehicle_settings[oid].attackDamageRadius = 0;
	vehicle_settings[oid].attackMissileSpeed = 0;
	vehicle_settings[oid].attackSpeed = 0;
	vehicle_settings[oid].attackSnipeChance = 0.0;
	vehicle_settings[oid].health = 50.0 / 74;
	vehicle_settings[oid].buildTime = 1 * 60 + 58;
	vehicle_settings[oid].maxRunTime = run_past_radius * 120 / vehicle_settings[oid].moveSpeed;

	oid = MISSILE_LAUNCHER;
	vehicle_settings[oid].groupAmount = 0;
	vehicle_settings[oid].moveSpeed = 6;
	vehicle_settings[oid].attackRadius = 160;
	vehicle_settings[oid].attackDamage = 62.0 / 74;//360.0 / 240;
	vehicle_settings[oid].attackDamageChance = 0;
	vehicle_settings[oid].attackDamageRadius = 80;
	vehicle_settings[oid].attackMissileSpeed = 70;
	vehicle_settings[oid].attackSpeed = 4.454;
	vehicle_settings[oid].attackSnipeChance = 0.0;
	vehicle_settings[oid].health = 50.0 / 74;
	vehicle_settings[oid].buildTime = 6 * 60 + 13;
	vehicle_settings[oid].maxRunTime = 0.5 * run_past_radius * vehicle_settings[oid].attackRadius / vehicle_settings[oid].moveSpeed;

	oid = CRANE;
	vehicle_settings[oid].groupAmount = 0;
	vehicle_settings[oid].moveSpeed = 14;
	vehicle_settings[oid].attackRadius = 0;
	vehicle_settings[oid].attackDamage = 0;
	vehicle_settings[oid].attackDamageChance = 0;
	vehicle_settings[oid].attackDamageRadius = 0;
	vehicle_settings[oid].attackMissileSpeed = 0;
	vehicle_settings[oid].attackSpeed = 0;
	vehicle_settings[oid].attackSnipeChance = 0.0;
	vehicle_settings[oid].health = 1.0;
	vehicle_settings[oid].buildTime = 1 * 60 + 37;
	vehicle_settings[oid].maxRunTime = 0.7 * run_past_radius * vehicle_settings[oid].attackRadius / vehicle_settings[oid].moveSpeed;

	//cannons
	oid = GATLING;
	cannon_settings[oid].groupAmount = 0;
	cannon_settings[oid].moveSpeed = 0;
	cannon_settings[oid].attackRadius = 120;
	cannon_settings[oid].attackDamage = 0.00397566;
	cannon_settings[oid].attackDamageChance = 0.65;
	cannon_settings[oid].attackDamageRadius = 0;
	cannon_settings[oid].attackMissileSpeed = 0;
	cannon_settings[oid].attackSpeed = 0.1;
	cannon_settings[oid].attackSnipeChance = 0.4;
	cannon_settings[oid].health = 13.0 / 74;
	cannon_settings[oid].buildTime = 1 * 60 + 36;

	oid = GUN;
	cannon_settings[oid].groupAmount = 0;
	cannon_settings[oid].moveSpeed = 0;
	cannon_settings[oid].attackRadius = 128;
	cannon_settings[oid].attackDamage = 75.0 / 240;
	cannon_settings[oid].attackDamageChance = 0;
	cannon_settings[oid].attackDamageRadius = 40;
	cannon_settings[oid].attackMissileSpeed = 225;
	cannon_settings[oid].attackSpeed = 2.254;
	cannon_settings[oid].attackSnipeChance = 0.0;
	cannon_settings[oid].health = 25.0 / 74;
	cannon_settings[oid].buildTime = 2 * 60 + 5;

	oid = HOWITZER;
	cannon_settings[oid].groupAmount = 0;
	cannon_settings[oid].moveSpeed = 0;
	cannon_settings[oid].attackRadius = 200;
	cannon_settings[oid].attackDamage = 100.0 / 240;
	cannon_settings[oid].attackDamageChance = 0;
	cannon_settings[oid].attackDamageRadius = 40;
	cannon_settings[oid].attackMissileSpeed = 95;
	cannon_settings[oid].attackSpeed = 4.86;
	cannon_settings[oid].attackSnipeChance = 0.0;
	cannon_settings[oid].health = 25.0 / 74;
	cannon_settings[oid].buildTime = 2 * 60 + 59;

	oid = MISSILE_CANNON;
	cannon_settings[oid].groupAmount = 0;
	cannon_settings[oid].moveSpeed = 0;
	cannon_settings[oid].attackRadius = 144;
	cannon_settings[oid].attackDamage = 200.0 / 240;
	cannon_settings[oid].attackDamageChance = 0;
	cannon_settings[oid].attackDamageRadius = 50;
	cannon_settings[oid].attackMissileSpeed = 128;
	cannon_settings[oid].attackSpeed = 1.124;
	cannon_settings[oid].attackSnipeChance = 0.0;
	cannon_settings[oid].health = 25.0 / 74;
	cannon_settings[oid].buildTime = 3 * 60 + 2;

	//rogue variables
	fort_building_health = 10000.0 / 240;
	robot_building_health = 2000.0 / 240;
	vehicle_building_health = 2000.0 / 240;
	repair_building_health = 2000.0 / 240;
	radar_building_health = 2000.0 / 240;
	bridge_building_health = 2000.0 / 240;

	rock_item_health = 30.0 / 240;
	grenades_item_health = 40.0 / 240;
	rockets_item_health = 40.0 / 240;
	hut_item_health = 40.0 / 240;
	map_item_health = 40.0 / 240;

	grenade_damage = 40.0 / 240;
	grenade_damage_radius = 30;
	grenade_missile_speed = 40;
	grenade_attack_speed = 2.254;
	map_item_turrent_damage = 50.0 / 240;

	agro_distance = 40;
	auto_grab_vehicle_distance = 220;
	auto_grab_flag_distance = 220;
	building_auto_repair_time = 10 * 60;
	building_auto_repair_random_additional_time = 60;
	max_turrent_horizontal_distance = 300;
	max_turrent_vertical_distance = 300;
	grenades_per_box = 20;
	partially_damaged_unit_speed = 0.9;
	damaged_unit_speed = 0.8;
	run_unit_speed = 1.8;
	run_recharge_rate = 0.3;
	hut_animal_max = 5;
	hut_animal_min = 3;
	hut_animal_roam_distance = 7 * 16;
}

void ZSettings::CensorSettings()
{
	//some settings are forced to certain values
	//as some parts of the code were set to expect this

	//non missiles
	robot_settings[GRUNT].CensorNonMissileUnit();
	robot_settings[PSYCHO].CensorNonMissileUnit();
	robot_settings[SNIPER].CensorNonMissileUnit();
	robot_settings[PYRO].CensorNonMissileUnit();
	robot_settings[LASER].CensorNonMissileUnit();
	vehicle_settings[JEEP].CensorNonMissileUnit();
	cannon_settings[GATLING].CensorNonMissileUnit();

	//missiles
	robot_settings[TOUGH].CensorMissileUnit();
	vehicle_settings[LIGHT].CensorMissileUnit();
	vehicle_settings[MEDIUM].CensorMissileUnit();
	vehicle_settings[HEAVY].CensorMissileUnit();
	vehicle_settings[MISSILE_LAUNCHER].CensorMissileUnit();
	cannon_settings[GUN].CensorMissileUnit();
	cannon_settings[HOWITZER].CensorMissileUnit();
	cannon_settings[MISSILE_CANNON].CensorMissileUnit();

	//non weapon
	vehicle_settings[APC].CensorMissileUnit();
	vehicle_settings[CRANE].CensorMissileUnit();

	//non movement
	for(int i=0;i<MAX_CANNON_TYPES;i++)
		cannon_settings[i].moveSpeed = 0;

	//non negatives
	for(int i=0;i<MAX_ROBOT_TYPES;i++) robot_settings[i].CensorNegatives();
	for(int i=0;i<MAX_VEHICLE_TYPES;i++) vehicle_settings[i].CensorNegatives();
	for(int i=0;i<MAX_CANNON_TYPES;i++) cannon_settings[i].CensorNegatives();
	if(agro_distance < 0) agro_distance = 0;
	if(auto_grab_vehicle_distance < 0) auto_grab_vehicle_distance = 0;
	if(auto_grab_flag_distance < 0) auto_grab_flag_distance = 0;
	if(building_auto_repair_time < 0) building_auto_repair_time = 0;
	if(building_auto_repair_random_additional_time < 0) building_auto_repair_random_additional_time = 0;
	if(max_turrent_horizontal_distance < 0) max_turrent_horizontal_distance = 0;
	if(max_turrent_vertical_distance < 0) max_turrent_vertical_distance = 0;
	if(grenades_per_box < 0) grenades_per_box = 0;
	if(grenades_per_box > 99) grenades_per_box = 99;

	if(grenade_damage < 0) grenade_damage = 0;
	if(grenade_damage_radius < 0) grenade_damage_radius = 0;
	if(map_item_turrent_damage < 0) map_item_turrent_damage = 0;
	if(partially_damaged_unit_speed < 0) partially_damaged_unit_speed = 0;
	if(damaged_unit_speed < 0) damaged_unit_speed = 0;
	if(run_unit_speed < 0) run_unit_speed = 0;
	if(run_recharge_rate < 0) run_recharge_rate = 0;
	if(run_recharge_rate > 1) run_recharge_rate = 1;
	if(hut_animal_max < 0) hut_animal_max = 0;
	if(hut_animal_min < 0) hut_animal_min = 0;
	if(hut_animal_roam_distance < 0) hut_animal_roam_distance = 0;
}

bool ZSettings::LoadSettings(std::string filename)
{
	FILE *fp = fopen(filename.c_str(), "r");
	if(!fp)
	{
		printf("ZSettings::could not open '%s' to read settings\n", filename.c_str());
		return false;
	}

	const int buf_size = 500;
	char cur_line[buf_size];
	char element_type[buf_size];
	char element[buf_size];
	char variable[buf_size];
	char value[buf_size];
	bool loaded = false;

	while(fgets(cur_line , buf_size , fp))
	{
		COMMON::clean_newline(cur_line, buf_size);

		if(strlen(cur_line))
		{
			int pt = 0;

			//don't read comment lines
			if(cur_line[0] == '#') continue;

			//parse this line
			COMMON::split(element_type, cur_line, '.', &pt, buf_size, buf_size);
			COMMON::split(element, cur_line, '.', &pt, buf_size, buf_size);
			COMMON::split(variable, cur_line, '=', &pt, buf_size, buf_size);
			COMMON::split(value, cur_line, '=', &pt, buf_size, buf_size);

			COMMON::lcase(element_type, buf_size);
			COMMON::lcase(element, buf_size);
			COMMON::lcase(variable, buf_size);

			if(!strcmp(element_type, "unit"))
			{
				int found = false;

				for(int i=0;i<MAX_ROBOT_TYPES && !found;i++)
					if(robot_type_string[i] == element)
					{
						robot_settings[i].ReadEntry(variable, value);
						found = true;
						break;
					}
				for(int i=0;i<MAX_VEHICLE_TYPES && !found;i++)
					if(vehicle_type_string[i] == element)
					{
						vehicle_settings[i].ReadEntry(variable, value);
						found = true;
						break;
					}
				for(int i=0;i<MAX_CANNON_TYPES && !found;i++)
					if(cannon_type_string[i] == element)
					{
						cannon_settings[i].ReadEntry(variable, value);
						found = true;
						break;
					}
			}
			else if(!strcmp(element_type, "building"))
			{
				if(!strcmp(variable, "health"))
				{
					if(!strcmp(element, "fort"))
						fort_building_health = atof(value);
					else if(!strcmp(element, "robot"))
						robot_building_health = atof(value);
					else if(!strcmp(element, "vehicle"))
						vehicle_building_health = atof(value);
					else if(!strcmp(element, "repair"))
						repair_building_health = atof(value);
					else if(!strcmp(element, "radar"))
						radar_building_health = atof(value);
					else if(!strcmp(element, "bridge"))
						bridge_building_health = atof(value);
				}
			}
			else if(!strcmp(element_type, "map_item"))
			{
				if(!strcmp(variable, "health"))
				{
					if(!strcmp(element, "rock"))
						rock_item_health = atof(value);
					else if(!strcmp(element, "grenades"))
						grenades_item_health = atof(value);
					else if(!strcmp(element, "rockets"))
						rockets_item_health = atof(value);
					else if(!strcmp(element, "hut"))
						hut_item_health = atof(value);
					else if(!strcmp(element, "map_item"))
						map_item_health = atof(value);
				}
				else if(!strcmp(element, "grenades"))
				{
					if(!strcmp(variable, "grenade_damage"))
						grenade_damage = atof(value);
					else if(!strcmp(variable, "grenade_damage_radius"))
						grenade_damage_radius = atoi(value);
					else if(!strcmp(variable, "grenade_missile_speed"))
						grenade_missile_speed = atoi(value);
					else if(!strcmp(variable, "grenade_attack_speed"))
						grenade_attack_speed = atof(value);
				}
				else if(!strcmp(element, "map_item") && !strcmp(variable, "map_item_turrent_damage"))
					map_item_turrent_damage = atof(value);
			}
			else if(!strcmp(element_type, "global") && !strcmp(element, "global"))
			{
				if(!strcmp(variable, "agro_distance"))
					agro_distance = atoi(value);
				else if(!strcmp(variable, "auto_grab_vehicle_distance"))
					auto_grab_vehicle_distance = atoi(value);
				else if(!strcmp(variable, "auto_grab_flag_distance"))
					auto_grab_flag_distance = atoi(value);
				else if(!strcmp(variable, "building_auto_repair_time"))
					building_auto_repair_time = atoi(value);
				else if(!strcmp(variable, "building_auto_repair_random_additional_time"))
					building_auto_repair_random_additional_time = atoi(value);
				else if(!strcmp(variable, "max_turrent_horizontal_distance"))
					max_turrent_horizontal_distance = atoi(value);
				else if(!strcmp(variable, "max_turrent_vertical_distance"))
					max_turrent_vertical_distance = atoi(value);
				else if(!strcmp(variable, "grenades_per_box"))
					grenades_per_box = atoi(value);
				else if(!strcmp(variable, "partially_damaged_unit_speed"))
					partially_damaged_unit_speed = atof(value);
				else if(!strcmp(variable, "damaged_unit_speed"))
					damaged_unit_speed = atof(value);
				else if(!strcmp(variable, "run_unit_speed"))
					run_unit_speed = atof(value);
				else if(!strcmp(variable, "run_recharge_rate"))
					run_recharge_rate = atof(value);
				else if(!strcmp(variable, "hut_animal_max"))
					hut_animal_max = atoi(value);
				else if(!strcmp(variable, "hut_animal_min"))
					hut_animal_min = atoi(value);
				else if(!strcmp(variable, "hut_animal_roam_distance"))
					hut_animal_roam_distance = atoi(value);
			}

			//printf("ZSettings::LoadSettings:['%s'.'%s'.'%s'='%s']\n", element_type, element, variable, value);

			loaded = true;
		}
	}

	fclose(fp);

	//now censor some oddities
	CensorSettings();

	return loaded;
}

bool ZSettings::SaveSettings(std::string filename)
{
	FILE *fp = fopen(filename.c_str(), "w");
	if(!fp)
	{
		printf("ZSettings::could not open '%s' to save settings\n", filename.c_str());
		return false;
	}

	std::string type_name;

	type_name = "unit";

	for(int i=0;i<MAX_ROBOT_TYPES;i++)
		robot_settings[i].SaveLine(fp, type_name + "." + robot_type_string[i]);

	for(int i=0;i<MAX_VEHICLE_TYPES;i++)
		vehicle_settings[i].SaveLine(fp, type_name + "." + vehicle_type_string[i]);

	for(int i=0;i<MAX_CANNON_TYPES;i++)
		cannon_settings[i].SaveLine(fp, type_name + "." + cannon_type_string[i]);

	//rogue variables
	fprintf(fp, "\n");
	fprintf(fp, "building.fort.health=%lf\n", fort_building_health);
	fprintf(fp, "building.robot.health=%lf\n", robot_building_health);
	fprintf(fp, "building.vehicle.health=%lf\n", vehicle_building_health);
	fprintf(fp, "building.repair.health=%lf\n", repair_building_health);
	fprintf(fp, "building.radar.health=%lf\n", radar_building_health);
	fprintf(fp, "building.bridge.health=%lf\n", bridge_building_health);

	fprintf(fp, "\n");
	fprintf(fp, "map_item.rock.health=%lf\n", rock_item_health);
	fprintf(fp, "map_item.grenades.health=%lf\n", grenades_item_health);
	fprintf(fp, "map_item.rockets.health=%lf\n", rockets_item_health);
	fprintf(fp, "map_item.hut.health=%lf\n", hut_item_health);
	fprintf(fp, "map_item.map_item.health=%lf\n", map_item_health);

	fprintf(fp, "\n");
	fprintf(fp, "map_item.grenades.grenade_damage=%lf\n", grenade_damage);
	fprintf(fp, "map_item.grenades.grenade_damage_radius=%d\n", grenade_damage_radius);
	fprintf(fp, "map_item.grenades.grenade_missile_speed=%d\n", grenade_missile_speed);
	fprintf(fp, "map_item.grenades.grenade_attack_speed=%lf\n", grenade_attack_speed);
	fprintf(fp, "map_item.map_item.map_item_turrent_damage=%lf\n", map_item_turrent_damage);

	fprintf(fp, "\n");
	fprintf(fp, "global.global.agro_distance=%d\n", agro_distance);
	fprintf(fp, "global.global.auto_grab_vehicle_distance=%d\n", auto_grab_vehicle_distance);
	fprintf(fp, "global.global.auto_grab_flag_distance=%d\n", auto_grab_flag_distance);
	fprintf(fp, "global.global.building_auto_repair_time=%d\n", building_auto_repair_time);
	fprintf(fp, "global.global.building_auto_repair_random_additional_time=%d\n", building_auto_repair_random_additional_time);
	fprintf(fp, "global.global.max_turrent_horizontal_distance=%d\n", max_turrent_horizontal_distance);
	fprintf(fp, "global.global.max_turrent_vertical_distance=%d\n", max_turrent_vertical_distance);
	fprintf(fp, "global.global.grenades_per_box=%d\n", grenades_per_box);
	fprintf(fp, "global.global.partially_damaged_unit_speed=%lf\n", partially_damaged_unit_speed);
	fprintf(fp, "global.global.damaged_unit_speed=%lf\n", damaged_unit_speed);
	fprintf(fp, "global.global.run_unit_speed=%lf\n", run_unit_speed);
	fprintf(fp, "global.global.run_recharge_rate=%lf\n", run_recharge_rate);
	fprintf(fp, "global.global.hut_animal_max=%d\n", hut_animal_max);
	fprintf(fp, "global.global.hut_animal_min=%d\n", hut_animal_min);
	fprintf(fp, "global.global.hut_animal_roam_distance=%d\n", hut_animal_roam_distance);

	fclose(fp);

	return true;
}

OpenZod::Core::UnitSettings &ZSettings::GetUnitSettings(unsigned char ot, unsigned char oid)
{
	static OpenZod::Core::UnitSettings empty_settings;

	switch(ot)
	{
	case CANNON_OBJECT:
		if(oid >= MAX_CANNON_TYPES) break;
		return cannon_settings[oid];
		break;
	case VEHICLE_OBJECT:
		if(oid >= MAX_VEHICLE_TYPES) break;
		return vehicle_settings[oid];
		break;
	case ROBOT_OBJECT:
		if(oid >= MAX_ROBOT_TYPES) break;
		return robot_settings[oid];
		break;
	}

	return empty_settings;
}
