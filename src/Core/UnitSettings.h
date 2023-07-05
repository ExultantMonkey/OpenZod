#pragma once

#include <cstdio>
#include <string>

namespace OpenZod { namespace Core {
#pragma pack(1)
class UnitSettings {
public:
    UnitSettings();
    void SaveLine(FILE *fp, std::string obj_name);
	void ReadEntry(std::string variable, std::string value);
	void CensorNonMissileUnit();
	void CensorMissileUnit();
	void CensorNonWeaponUnit();
	void CensorNegatives();

    unsigned char groupAmount;
	unsigned char moveSpeed;
	unsigned char attackRadius;
	double attackDamage;
	double attackDamageChance;
	unsigned char attackDamageRadius;
	unsigned char attackMissileSpeed;
	double attackSpeed;
	double attackSnipeChance;
	double health;
	unsigned short int buildTime;
	double maxRunTime;
};
#pragma pack()
}}