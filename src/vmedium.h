#ifndef _VMEDIUM_H_
#define _VMEDIUM_H_

#include "zvehicle.h"

class VMedium : public ZVehicle
{
	public:
		VMedium(ZTime *ztime_, ZSettings *zsettings_ = NULL);
		
		static void Init();
		
// 		SDL_Surface *GetRender();
		int Process();
		void DoRender(ZMap &the_map, SDL_Surface *dest, int shift_x = 0, int shift_y = 0);
		void DoAfterEffects(ZMap &the_map, SDL_Surface *dest, int shift_x, int shift_y);
		void FireMissile(int x_, int y_);
		void DoDeathEffect(bool do_fire_death, bool do_missile_death);
		void FireTurrentMissile(int x_, int y_, double offset_time);
		//bool ServerFireTurrentMissile(int &x_, int &y_, int &damage, int &radius, double &offset_time);
		std::vector<fire_missile_info> ServerFireTurrentMissile(int &damage, int &radius);
	private:
		static ZSDL_Surface base[MAX_TEAM_TYPES][MAX_ANGLE_TYPES][3];
		static ZSDL_Surface base_damaged[MAX_TEAM_TYPES][MAX_ANGLE_TYPES][3];
		static ZSDL_Surface top[MAX_ANGLE_TYPES];
		static ZSDL_Surface top_pop[8];
		
		double next_turrent_time;
};

#endif
