#include "UnitSettings.h"

#include <spdlog/spdlog.h>

OpenZod::Core::UnitSettings::UnitSettings()
{
    groupAmount = 0;
    moveSpeed = 0;
    attackRadius = 0;
    attackDamage = 0;
    attackDamageChance = 0;
    attackDamageRadius = 0;
    attackMissileSpeed = 0;
    attackSpeed = 0;
    attackSnipeChance = 0;
    health = 0;
    buildTime = 0;
    maxRunTime = 0;
}

void OpenZod::Core::UnitSettings::CensorNegatives()
{
	if(groupAmount < 0)
    {
        groupAmount = 0;
    }
	if(moveSpeed < 0)
    {
        moveSpeed = 0;
    }
	if(attackRadius < 0)
    {
        attackRadius = 0;
    }
	if(attackDamage < 0)
    {
        attackDamage = 0;
    }
	if(attackDamageChance < 0)
    {
        attackDamageChance = 0;
    }
	if(attackDamageRadius < 0)
    {
        attackDamageRadius = 0;
    }
	if(attackMissileSpeed < 0)
    {
        attackMissileSpeed = 0;
    }
	if(attackSpeed < 0)
    {
        attackSpeed = 0;
    }
	if(attackSnipeChance < 0)
    {
        attackSnipeChance = 0;
    }
	if(health < 0)
    {
        health = 0;
    }
	if(buildTime < 0)
    {
        buildTime = 0;
    }

	if(attackDamageChance > 1.0)
    {
        attackDamageChance = 1.0;
    }
	if(attackSnipeChance > 1.0)
    {
        attackSnipeChance = 1.0;
    }
}

void OpenZod::Core::UnitSettings::CensorNonMissileUnit()
{
	attackDamageRadius = 0;
	attackMissileSpeed = 0;
}

void OpenZod::Core::UnitSettings::CensorMissileUnit()
{
	attackDamageChance = 0;
}

void OpenZod::Core::UnitSettings::CensorNonWeaponUnit()
{
	attackRadius = 0;
	attackDamage = 0;
	attackDamageChance = 0;
	attackDamageRadius = 0;
	attackMissileSpeed = 0;
	attackSpeed = 0;
	attackSnipeChance = 0;
}

void OpenZod::Core::UnitSettings::SaveLine(FILE *fp, std::string obj_name)
{
	fprintf(fp, "\n");
	fprintf(fp, "%s.group_amount=%d\n", obj_name.c_str(), groupAmount);
	fprintf(fp, "%s.move_speed=%d\n", obj_name.c_str(), moveSpeed);
	fprintf(fp, "%s.attack_radius=%d\n", obj_name.c_str(), attackRadius);
	fprintf(fp, "%s.attack_damage=%lf\n", obj_name.c_str(), attackDamage);
	fprintf(fp, "%s.attack_damage_chance=%lf\n", obj_name.c_str(), attackDamageChance);
	fprintf(fp, "%s.attack_damage_radius=%d\n", obj_name.c_str(), attackDamageRadius);
	fprintf(fp, "%s.attack_missile_speed=%d\n", obj_name.c_str(), attackMissileSpeed);
	fprintf(fp, "%s.attack_speed=%lf\n", obj_name.c_str(), attackSpeed);
	fprintf(fp, "%s.attack_snipe_chance=%lf\n", obj_name.c_str(), attackSnipeChance);
	fprintf(fp, "%s.health=%lf\n", obj_name.c_str(), health);
	fprintf(fp, "%s.build_time=%d\n", obj_name.c_str(), buildTime);
	fprintf(fp, "%s.max_run_time=%lf\n", obj_name.c_str(), maxRunTime);
}

void OpenZod::Core::UnitSettings::ReadEntry(std::string variable, std::string value)
{
    if(variable == "group_amount")
    {
        groupAmount = atoi(value.c_str());
    }
    else if (variable == "move_speed")
    {
        moveSpeed = atoi(value.c_str());
    }
    else if (variable == "attack_radius")
    {
        attackRadius = atoi(value.c_str());
    }
    else if (variable == "attack_damage")
    {
        attackDamage = atof(value.c_str());
    }
    else if (variable == "attack_damage_chance")
    {
        attackDamageChance = atof(value.c_str());
    }
    else if (variable == "attack_damage_radius")
    {
        attackDamageRadius = atoi(value.c_str());
    }
    else if (variable == "attack_missile_speed")
    {
        attackMissileSpeed = atoi(value.c_str());
    }
    else if (variable == "attack_speed")
    {
        attackSpeed = atof(value.c_str());
    }
    else if (variable == "attack_snipe_chance")
    {
        attackSnipeChance = atof(value.c_str());
    }
    else if (variable == "health")
    {
        health = atof(value.c_str());
    }
    else if (variable == "build_time")
    {
        buildTime = atoi(value.c_str());
    }
    else if (variable == "max_run_time")
    {
        maxRunTime = atof(value.c_str());
    }
    else
    {
        spdlog::warn("UnitSettings::ReadEntry - Unknown variable '{}'", variable);
    }
}