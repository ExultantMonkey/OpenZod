#include "zobject.h"
#include "common.h"
#include "zfont_engine.h"

#include "Util/Math.h"
#include "Util/Random.h"

#include <spdlog/spdlog.h>

ZSettings ZObject::default_zsettings;

ZSDL_Surface ZObject::group_tag[10];

std::vector<damage_missile> *ZObject::damage_missile_list = NULL;

ZObject::ZObject(ZTime *ztime_, ZSettings *zsettings_)
{
	if(!zsettings_)
	{
		zsettings = &default_zsettings;
	}
	else
	{
		zsettings = zsettings_;
	}

	ztime = ztime_;

	zmap = NULL;
	effect_list = NULL;

	double &the_time = ztime->ztime;

	object_name = "object";
	owner = NULL_TEAM;
	last_process_time = the_time;
	width = 0;
	height = 0;
	width_pix = 0;
	height_pix = 0;
	mode = NULL_MODE;
	destroyed = false;
	direction = 4;
	selectable = false;
	ref_id = -1;
	waypoint_i = 0;
	attack_i = 0;
	next_waypoint_time = 0;
	render_death_time = 0;
	show_waypoints = false;
	loc.x = loc.y = 0;
	loc.dx = loc.dy = 0;
	center_x = center_y = 0;
	move_speed = 0;
	real_move_speed = 0;
	xover = yover = 0;
	last_process_server_time = last_process_time;
	last_radius_time = last_process_time;
	last_loc_set_time = last_process_time;
	waypoint_cursor.SetTeam(NULL_TEAM);
	group_num = -1;
	attack_radius = 0;
	radius_i = 0;
	max_health = 1;
	health = max_health;
	last_wp.mode = -1;
	next_check_passive_attack_time = 0;
	buildlist = NULL;
	connected_zone = NULL;
	unit_limit_reached = NULL;
	dont_stamp = false;
	do_auto_repair = false;
	can_be_destroyed = true;
	has_explosives = false;
	attacked_by_explosives = false;
	do_hit_effect = false;
	do_driver_hit_effect = false;
	can_snipe = false;
	has_lid = false;
	processed_death = false;
	can_be_sniped = false;
	last_damaged_by_fire_time = 0;
	last_damaged_by_missile_time = 0;
	just_left_cannon = false;
	initial_health_percent = 100;
	next_drop_track_time = 0;
	max_stamina = 0;
	stamina = max_stamina;

	driver_type = GRUNT;

	//to make sure loc updates from the server are not over frequent
	loc_update_int = 0.8 + (0.001 * (rand() % 25));
	next_loc_update_time = 0;

	//attack stuff
	attack_object = NULL;
	next_damage_time = 0;
	damage_int_time = 0;
	damage_chance = 0;
	damage = 0;
	damage_is_missile = false;
	damage_radius = 1;
	snipe_chance = 0;

	//needed for when an object needs to be removed after some time
	killme = false;
	killme_time = 0;
	
	//ai stuff
	ai_last_set_build_time = -50000;

	ClearGroupInfo();
}

ZObject::~ZObject()
{
	//kill any pathfinding threads
	if(zmap)
	{
		cur_wp_info.clear_and_kill(*zmap);
	}
}

void ZObject::InitTypeId(unsigned char ot, unsigned char oid)
{
	object_type = ot;
	object_id = oid;

	if(!zsettings)
	{
		spdlog::error("ZObject::InitTypeId - ZSettings not set");
		return;
	}

	if(ot == BUILDING_OBJECT)
	{
		switch(oid)
		{
		case FORT_FRONT:
		case FORT_BACK:
			max_health = zsettings->fort_building_health * MAX_UNIT_HEALTH;
			break;
		case RADAR:
			max_health = zsettings->radar_building_health * MAX_UNIT_HEALTH;
			break;
		case REPAIR:
			max_health = zsettings->repair_building_health * MAX_UNIT_HEALTH;
			break;
		case ROBOT_FACTORY:
			max_health = zsettings->robot_building_health * MAX_UNIT_HEALTH;
			break;
		case VEHICLE_FACTORY:
			max_health = zsettings->vehicle_building_health * MAX_UNIT_HEALTH;
			break;
		case BRIDGE_VERT:
		case BRIDGE_HORZ:
			max_health = zsettings->bridge_building_health * MAX_UNIT_HEALTH;
			break;
		}
	}
	else if (ot == MAP_ITEM_OBJECT)
	{
		switch(oid)
		{
		case ROCK_ITEM:
			max_health = zsettings->rock_item_health * MAX_UNIT_HEALTH;
			break;
		case GRENADES_ITEM:
			max_health = zsettings->grenades_item_health * MAX_UNIT_HEALTH;
			break;
		case ROCKETS_ITEM:
			max_health = zsettings->rockets_item_health * MAX_UNIT_HEALTH;
			break;
		case HUT_ITEM:
			max_health = zsettings->hut_item_health * MAX_UNIT_HEALTH;
			break;
		}

		if(oid >= MAP0_ITEM && oid < MAP0_ITEM + MAP_ITEMS_AMOUNT)
		{
			max_health = zsettings->map_item_health * MAX_UNIT_HEALTH;
		}
	}
	else
	{
		OpenZod::Core::UnitSettings &unit_settings = zsettings->GetUnitSettings(object_type, object_id);

		move_speed = unit_settings.moveSpeed;
		attack_radius = unit_settings.attackRadius;
		damage = unit_settings.attackDamage * MAX_UNIT_HEALTH;
		damage_chance = unit_settings.attackDamageChance;
		damage_radius = unit_settings.attackDamageRadius;
		missile_speed = unit_settings.attackMissileSpeed;
		snipe_chance = unit_settings.attackSnipeChance;
		damage_int_time = unit_settings.attackSpeed;
		max_health = unit_settings.health * MAX_UNIT_HEALTH;
		max_stamina = unit_settings.maxRunTime;
	}

	//set health and stamina to full
	health = max_health;
	stamina = max_stamina;
}

void ZObject::Init(TTF_Font *ttf_font)
{
	char filename_c[500];
	SDL_Color textcolor;

	for(int i=0;i<10;i++)
	{
		textcolor.r = 200;
		textcolor.g = 200;
		textcolor.b = 200;

		sprintf(filename_c, "%d", i);
		group_tag[i].LoadBaseImage(TTF_RenderText_Solid(ttf_font, filename_c, textcolor));
	}
}

void ZObject::InitRealMoveSpeed(ZMap &tmap)
{
	real_move_speed = move_speed * tmap.GetTileWalkSpeed(center_x, center_y);
}

void ZObject::SetMap(ZMap *zmap_)
{
	zmap = zmap_;
}

void ZObject::DontStamp(bool dont_stamp_)
{
	dont_stamp = dont_stamp_;
}

void ZObject::DoDeathEffect(bool do_fire_death, bool do_missile_death)
{

}

void ZObject::DoReviveEffect()
{

}

void ZObject::FireMissile(int x_, int y_)
{

}

bool ZObject::KillMe(double the_time)
{
	return (killme && the_time >= killme_time);
}

void ZObject::DoKillMe(double killtime)
{
	//they were already set to die
	//make sure they die at their original time
	if(killme)
	{
		return;
	}

	killme = true;
	killme_time = killtime;
}

bool ZObject::IsDestroyed()
{
	return (health <= 0 && max_health > 0);
}

int ZObject::GetHealth()
{
	return health;
}

int ZObject::GetMaxHealth()
{
	return max_health;
}

void ZObject::SetHealthPercent(int health_percent, ZMap &tmap)
{
	if(health_percent > 100)
	{
		health_percent = 100;
	}
	if(health_percent < 0)
	{
		health_percent = 0;
	}

	//for the map editor
	initial_health_percent = health_percent;

	SetHealth(health_percent * max_health / 100, tmap);
}

void ZObject::SetHealth(int new_health, ZMap &tmap)
{
	health = new_health;
	if(health < 0)
	{
		health = 0;
	}
	if(health > max_health)
	{
		health = max_health;
	}

	bool was_destroyed = IsDestroyed();
	if(was_destroyed && !IsDestroyed())
	{
		DoReviveEffect();
		UnSetDestroyMapImpassables(tmap);
	}
	else if(!was_destroyed && IsDestroyed())
	{
		SetDestroyMapImpassables(tmap);
		ProcessKillObject();
	}
}

int ZObject::DamageDriverHealth(int damage_amount)
{
	if(!driver_info.size())
	{
		return 0;
	}

	driver_info_s &the_driver = *driver_info.begin();

	if(the_driver.driver_health <= 0)
	{
		return 0;
	}

	the_driver.driver_health -= damage_amount;

	if(the_driver.driver_health <= 0)
	{
		//we killed the driver

		//clear drivers
		ClearDrivers();

		//go to NULL_TEAM
		SetOwner(NULL_TEAM);

		//disengage, clear waypoints, and stop movement
		//done at the server level
	}
	
	return 1;
}

int ZObject::DamageHealth(int damage_amount, ZMap &tmap)
{
	//already dead foo
	if(health <= 0)
	{
		return health;
	}

	SetHealth(health - damage_amount, tmap);

	//got to recalc the build time if this is a building
	RecalcBuildTime();

	return health;
}

void ZObject::SetGroup(int group_num_)
{
	group_num = group_num_;
}

void ZObject::PlaySelectedWav()
{

}

void ZObject::PlaySelectedAnim(ZPortrait &portrait)
{
	switch(OpenZod::Util::Random::Int(0, 3))
	{
	case 0: 
		portrait.StartAnim(YES_SIR_ANIM); 
		break;
	case 1: 
		portrait.StartAnim(YES_SIR3_ANIM); 
		break;
	case 2: 
		portrait.StartAnim(UNIT_REPORTING1_ANIM); 
		break;
	case 3: 
		portrait.StartAnim(UNIT_REPORTING2_ANIM); 
		break;
	}
}

void ZObject::PlayAcknowledgeWav()
{

}

void ZObject::PlayAcknowledgeAnim(ZPortrait &portrait, bool no_way)
{
	if(no_way)
	{
		switch(OpenZod::Util::Random::Int(0, 2))
		{
		case 0:
			portrait.StartAnim(FORGET_IT_ANIM);
			break;
		case 1:
			portrait.StartAnim(GET_OUTTA_HERE_ANIM);
			break;
		case 2:
			portrait.StartAnim(NO_WAY_ANIM);
			break;
		}
	}
	else
	{
		switch(OpenZod::Util::Random::Int(0, 11))
		{
		case 0: 
			portrait.StartAnim(WERE_ON_OUR_WAY_ANIM);
			break;
		case 1:
			portrait.StartAnim(HERE_WE_GO_ANIM);
			break;
		case 2:
			portrait.StartAnim(YOUVE_GOT_IT_ANIM);
			break;
		case 3:
			portrait.StartAnim(MOVING_IN_ANIM);
			break;
		case 4:
			portrait.StartAnim(OKAY_ANIM);
			break;
		case 5:
			portrait.StartAnim(ALRIGHT_ANIM);
			break;
		case 6:
			portrait.StartAnim(NO_PROBLEM_ANIM);
			break;
		case 7:
			portrait.StartAnim(OVER_N_OUT_ANIM);
			break;
		case 8:
			portrait.StartAnim(AFFIRMATIVE_ANIM);
			break;
		case 9:
			portrait.StartAnim(GOING_IN_ANIM);
			break;
		case 10:
			portrait.StartAnim(LETS_DO_IT_ANIM);
			break;
		case 11:
			portrait.StartAnim(LETS_GET_EM_ANIM);
			break;
		}
	}
}

std::vector<waypoint> &ZObject::GetWayPointList()
{
	return waypoint_list;
}

std::vector<waypoint> &ZObject::GetWayPointDevList()
{
	return waypoint_dev_list;
}

void ZObject::SetRefID(int id)
{
	ref_id = id;
}

int ZObject::GetRefID()
{
	return ref_id;
}

ZSDL_Surface &ZObject::GetHoverNameImg()
{
	if(hover_name_img.GetBaseSurface())
	{
		return hover_name_img;
	}

	//make it?
	if(hover_name.size())
	{
		hover_name_img.Unload();
		hover_name_star_img.Unload();

		hover_name_img.LoadBaseImage(ZFontEngine::GetFont(SMALL_WHITE_FONT).Render(hover_name.c_str()));
		hover_name_star_img.LoadBaseImage(ZFontEngine::GetFont(SMALL_WHITE_FONT).Render((hover_name + " *").c_str()));

		if(hover_name_img.GetBaseSurface())
		{
			hover_name_x_shift = (width_pix - hover_name_img.GetBaseSurface()->w) >> 1;
		}

		if(hover_name_star_img.GetBaseSurface())
		{
			hover_name_star_x_shift = (width_pix - hover_name_star_img.GetBaseSurface()->w) >> 1;
		}
	}

	return hover_name_img;
}

ZSDL_Surface &ZObject::GetHoverNameImgStatic(unsigned char ot, unsigned char oid)
{
	const int max_units_in_type = 7;
	static ZSDL_Surface static_hover_name_img[MAX_MAP_OBJECT_TYPES][max_units_in_type];
	static ZSDL_Surface null_return;

	if((ot < 0) || ot >= MAX_MAP_OBJECT_TYPES)
	{
		return null_return;
	}
	if((oid < 0) || (oid >= max_units_in_type))
	{
		return null_return;
	}

	if(!static_hover_name_img[ot][oid].GetBaseSurface())
	{
		std::string render_str = GetHoverName(ot, oid);
		if(render_str.length())
		{
			static_hover_name_img[ot][oid].LoadBaseImage(ZFontEngine::GetFont(SMALL_WHITE_FONT).Render(render_str.c_str()));
		}
	}

	return static_hover_name_img[ot][oid];
}

std::string ZObject::GetHoverName(unsigned char ot, unsigned char oid)
{
	switch(ot)
	{
	case CANNON_OBJECT:
		switch(oid)
		{
		case GATLING:
			return "Gatling";
			break;
		case GUN:
			return "Gun";
			break;
		case HOWITZER:
			return "Howitzer";
			break;
		case MISSILE_CANNON:
			return "Missile";
			break;
		}
		break;
	case VEHICLE_OBJECT:
		switch(oid)
		{
		case JEEP:
			return "Jeep";
			break;
		case LIGHT:
			return "Light";
			break;
		case MEDIUM:
			return "Medium";
			break;
		case HEAVY:
			return "Heavy";
			break;
		case APC:
			return "APC";
			break;
		case MISSILE_LAUNCHER:
			return "M Missile";
			break;
		case CRANE:
			return "Crane";
			break;
		}
		break;
	case ROBOT_OBJECT:
		switch(oid)
		{
		case GRUNT:
			return "Grunt";
			break;
		case PSYCHO:
			return "Psychos";
			break;
		case SNIPER:
			return "Sniper";
			break;
		case TOUGH:
			return "Tough";
			break;
		case PYRO:
			return "Pyros";
			break;
		case LASER:
			return "Laser";
			break;
		}
		break;
	}

	return "";
}

void ZObject::RenderHover(ZMap &zmap, SDL_Surface *dest, team_type viewers_team)
{
	int &x = loc.x;
	int &y = loc.y;
	const int hover_name_y_shift = -19;

	if(hover_name_img.GetBaseSurface())
	{
		if(viewers_team == owner && viewers_team != NULL_TEAM)
		{
			zmap.RenderZSurface(&hover_name_star_img, x + hover_name_star_x_shift, y + hover_name_y_shift);
		}
		else
		{
			zmap.RenderZSurface(&hover_name_star_img, x + hover_name_x_shift, y + hover_name_y_shift);
		}
	}
	else if(hover_name.size())
	{
		hover_name_img.Unload();
		hover_name_star_img.Unload();

		hover_name_img.LoadBaseImage( ZFontEngine::GetFont(SMALL_WHITE_FONT).Render(hover_name.c_str()));
		hover_name_star_img.LoadBaseImage( ZFontEngine::GetFont(SMALL_WHITE_FONT).Render((hover_name + " *").c_str()));

		if(hover_name_img.GetBaseSurface())
		{
			hover_name_x_shift = (width_pix - hover_name_img.GetBaseSurface()->w) >> 1;
		}

		if(hover_name_star_img.GetBaseSurface())
		{
			hover_name_star_x_shift = (width_pix - hover_name_star_img.GetBaseSurface()->w) >> 1;
		}

		if(viewers_team == owner && viewers_team != NULL_TEAM)
		{
			if(hover_name_star_img.GetBaseSurface())
			{
				zmap.RenderZSurface(&hover_name_star_img, x + hover_name_star_x_shift, y + hover_name_y_shift);
			}
		}
		else
		{
			if(hover_name_img.GetBaseSurface())
			{
				zmap.RenderZSurface(&hover_name_img, x + hover_name_x_shift, y + hover_name_y_shift);
			}
		}
	}

	//if you are rendering the hover name, then you are also rendering the health
	RenderHealth(zmap, dest);
}

void ZObject::RenderHealth(ZMap &zmap, SDL_Surface *dest)
{
	int &x = loc.x;
	int &y = loc.y;
	SDL_Rect the_box;
	SDL_Rect from_rect, to_rect;
	const char g_r = 82, g_g = 190, g_b = 33;
	const char y_r = 247, y_g = 203, y_b = 107;
	const char b_r = 0, b_g = 0, b_b = 0;
	const int bar_x_shift = -3;
	const int bar_y_shift = -8;
	const int max_dist = 30 + 6;

	if(!hover_name.size())
	{
		return;
	}

	//calculate
	int green_dist = max_dist * (1.0 * health / MAX_UNIT_HEALTH);
	int yellow_dist = max_dist * (1.0 * max_health / MAX_UNIT_HEALTH);

	if(green_dist <= 0)
	{
		green_dist = 1;
	}
	if(yellow_dist <= 0)
	{
		yellow_dist = 1;
	}
	int total_dist = yellow_dist + 2;

	if(zmap.GetBlitInfo(x + bar_x_shift, y + bar_y_shift, total_dist, 4, from_rect, to_rect))
	{
		the_box.x = to_rect.x;
		the_box.y = to_rect.y;
		the_box.w = from_rect.w;
		the_box.h = from_rect.h;

		ZSDL_FillRect(&the_box, b_r, b_g, b_b);
	}

	if(zmap.GetBlitInfo(x + bar_x_shift + 1, y + bar_y_shift + 1, green_dist, 2, from_rect, to_rect))
	{
		the_box.x = to_rect.x;
		the_box.y = to_rect.y;
		the_box.w = from_rect.w;
		the_box.h = from_rect.h;

		ZSDL_FillRect(&the_box, g_r, g_g, g_b);
	}

	if(yellow_dist - green_dist)
	{
		if(zmap.GetBlitInfo(x + bar_x_shift + 1 + green_dist, y + bar_y_shift + 1, yellow_dist - green_dist, 2, from_rect, to_rect))
		{
			the_box.x = to_rect.x;
			the_box.y = to_rect.y;
			the_box.w = from_rect.w;
			the_box.h = from_rect.h;

			ZSDL_FillRect(&the_box, y_r, y_g, y_b);
		}
	}
}

void ZObject::RenderAttackRadius(ZMap &zmap, SDL_Surface *dest, std::vector<ZObject*> &avoid_list)
{
	int &x = loc.x;
	int &y = loc.y;
	int mx, my;
	int shift_x, shift_y, view_w, view_h;
	const int dots = 10;
	const double PI_shift = (PI / 2) / dots;

	if(!attack_radius)
	{
		return;
	}

	zmap.GetViewShiftFull(shift_x, shift_y, view_w, view_h);

	//find the center
	int cx = (x - shift_x) + (width_pix >> 1);
	int cy = (y - shift_y) + (height_pix >> 1);

	//is any of this even on the map?
	if(cx + attack_radius < 0)
	{
		return;
	}
	if(cy + attack_radius < 0)
	{
		return;
	}
	if(cx - attack_radius > view_w)
	{
		return;
	}
	if(cy - attack_radius > view_h)
	{
		return;
	}

	//init stuff
	SDL_Rect to_rect;
	to_rect.w = 3;
	to_rect.h = 3;

	double deg = radius_i;
	for(int i=0;i<dots && deg <= PI / 2;i++,deg+=PI_shift)
	{
		mx = (attack_radius + 3) * sin(deg);
		my = (attack_radius + 3) * cos(deg);

		to_rect.x = mx + cx;
		to_rect.y = my + cy;
		to_rect.w = 2;
		to_rect.h = 2;
		if((to_rect.x >= 0 && to_rect.x + 2 < view_w) && (to_rect.y >= 0 && to_rect.y + 2 < view_h))
		{
			if(!WithinAttackRadiusOf(avoid_list, to_rect.x + shift_x, to_rect.y + shift_y))
			{
				ZSDL_FillRect(&to_rect, team_color[owner].r, team_color[owner].g, team_color[owner].b);
			}
		}

		to_rect.x = -mx + cx;
		to_rect.y = -my + cy;
		to_rect.w = 2;
		to_rect.h = 2;
		if((to_rect.x >= 0 && to_rect.x + 2 < view_w) && (to_rect.y >= 0 && to_rect.y + 2 < view_h))
		{
			if(!WithinAttackRadiusOf(avoid_list, to_rect.x + shift_x, to_rect.y + shift_y))
			{
				ZSDL_FillRect(&to_rect, team_color[owner].r, team_color[owner].g, team_color[owner].b);
			}
		}

		to_rect.x = -mx + cx;
		to_rect.y = my + cy;
		to_rect.w = 2;
		to_rect.h = 2;
		if((to_rect.x >= 0 && to_rect.x + 2 < view_w) && (to_rect.y >= 0 && to_rect.y + 2 < view_h))
		{
			if(!WithinAttackRadiusOf(avoid_list, to_rect.x + shift_x, to_rect.y + shift_y))
			{
				ZSDL_FillRect(&to_rect, team_color[owner].r, team_color[owner].g, team_color[owner].b);
			}
		}

		to_rect.x = mx + cx;
		to_rect.y = -my + cy;
		to_rect.w = 2;
		to_rect.h = 2;
		if((to_rect.x >= 0 && to_rect.x + 2 < view_w) && (to_rect.y >= 0 && to_rect.y + 2 < view_h))
		{
			if(!WithinAttackRadiusOf(avoid_list, to_rect.x + shift_x, to_rect.y + shift_y))
			{
				ZSDL_FillRect(&to_rect, team_color[owner].r, team_color[owner].g, team_color[owner].b);
			}
		}
	}
}

void ZObject::RenderSelection(ZMap &zmap, SDL_Surface *dest)
{
	int &x = loc.x;
	int &y = loc.y;

	int shift_x, shift_y, view_w, view_h;
	zmap.GetViewShiftFull(shift_x, shift_y, view_w, view_h);

	SDL_Rect dim;
	dim.x = x - shift_x;
	dim.y = y - shift_y;
	dim.w = width_pix;
	dim.h = height_pix;

	draw_selection_box(dest, dim, team_color[owner], view_w, view_h);

	//draw group number
	if(group_num != -1)
	{
		zmap.RenderZSurface(&group_tag[group_num], x-2, y-3);
	}

	//render health too
	RenderHealth(zmap, dest);
}

bool ZObject::WithinAutoEnterRadius(int ox, int oy)
{
	int ae_radius = zsettings->auto_grab_vehicle_distance;
	int &x = center_x;
	int &y = center_y;

	return OpenZod::Util::Math::PointsWithinDistance(x, y, ox, oy, ae_radius);
}

bool ZObject::WithinAutoGrabFlagRadius(int ox, int oy)
{
	int agf_radius = zsettings->auto_grab_flag_distance;
	int &x = center_x;
	int &y = center_y;

	return OpenZod::Util::Math::PointsWithinDistance(x, y, ox, oy, agf_radius);
}

bool ZObject::WithinAgroRadius(ZObject *obj)
{
	if(!obj)
	{
		return false;
	}

	int &x = center_x;
	int &y = center_y;
	int &ox = obj->center_x;
	int &oy = obj->center_y;

	if(!WithinAgroRadius(ox, oy))
	{
		return false;
	}

	if(!obj->IsDestroyableImpass() && zmap && zmap->EngageBarrierBetweenCoords(x,y,ox,oy))
	{
		return false;
	}

	return true;
}

bool ZObject::WithinAgroRadius(int ox, int oy)
{
	int agro_radius = attack_radius + zsettings->agro_distance;
	int &x = center_x;
	int &y = center_y;

	return OpenZod::Util::Math::PointsWithinDistance(x, y, ox, oy, agro_radius);
}

bool ZObject::WithinAttackRadius(ZObject *obj)
{
	if(!obj)
	{
		return false;
	}

	int &x = center_x;
	int &y = center_y;
	int &ox = obj->center_x;
	int &oy = obj->center_y;

	if(!WithinAttackRadius(ox, oy))
	{
		return false;
	}

	if(!obj->IsDestroyableImpass() && zmap && zmap->EngageBarrierBetweenCoords(x,y,ox,oy))
	{
		return false;
	}

	return true;
}

bool ZObject::WithinAttackRadius(int ox, int oy)
{
	int &x = center_x;
	int &y = center_y;

	return OpenZod::Util::Math::PointsWithinDistance(x, y, ox, oy, attack_radius);
}

bool ZObject::WithinAttackRadiusOf(std::vector<ZObject*> &avoid_list, int ox, int oy)
{
	for(ZObject* i : avoid_list)
	{
		if(i != this && i->WithinAttackRadius(ox, oy))
		{
			return true;
		}
	}

	return false;
}

bool ZObject::Selectable()
{
	//minions are not selectable
	if(leader_obj)
	{
		return false;
	}

	return selectable;
}

bool ZObject::UnderCursor(int &map_x, int &map_y)
{
	int &x = loc.x;
	int &y = loc.y;
	if(map_x < x)
	{
		return false;
	}
	if(map_y < y)
	{
		return false;
	}
	if(map_x > x + width_pix)
	{
		return false;
	}
	if(map_y > y + height_pix)
	{
		return false;
	}

	return true;
}

double ZObject::DistanceFromCoords(int x, int y)
{
	int dx = loc.x - x;
	int dy = loc.y - y;

	return sqrt((double)((dx * dx) + (dy * dy)));
}

double ZObject::DistanceFromObject(ZObject &obj)
{
	int dx = center_x - obj.center_x;
	int dy = center_y - obj.center_y;
	
	return sqrt((double)((dx * dx) + (dy * dy)));
}

bool ZObject::IntersectsObject(ZObject &obj)
{
	int &x = loc.x;
	int &y = loc.y;
	if(obj.loc.x >= x + width_pix)
	{
		return false;
	}
	if(obj.loc.x + obj.width_pix <= x)
	{
		return false;
	}
	if(obj.loc.y >= y + height_pix)
	{
		return false;
	}
	if(obj.loc.y + obj.height_pix <= y)
	{
		return false;
	}

	return true;
}

bool ZObject::WithinSelection(int &map_left, int &map_right, int &map_top, int &map_bottom)
{
	int &x = loc.x;
	int &y = loc.y;

	if(map_left >= x + width_pix)
	{
		return false;
	}
	if(map_right <= x)
	{
		return false;
	}
	if(map_top >= y + height_pix)
	{
		return false;
	}
	if(map_bottom <= y)
	{
		return false;
	}

	return true;
}

bool ZObject::CannonNotPlacable(int &map_left, int &map_right, int &map_top, int &map_bottom)
{
	return WithinSelection(map_left, map_right, map_top, map_bottom);
}

std::string ZObject::GetObjectName()
{
	return object_name;
}

void ZObject::SetDirection(int direction_)
{
	direction = direction_;
}

void ZObject::SetCords(int x_, int y_)
{
	int &x = loc.x;
	int &y = loc.y;
	x = x_;
	y = y_;

	//set centers
	center_x = x + (width_pix >> 1);
	center_y = y + (height_pix >> 1);
}

void ZObject::SetOwner(team_type owner_)
{
	owner = owner_;
}

int ZObject::Process()
{
	return 0;
}

int ZObject::ProcessObject()
{
	double &the_time = ztime->ztime;

	//smooth the walk because of lag
	SmoothMove(the_time);

	//attack radius
	const int dots = 10;
	const double PI_shift = (PI / 2) / dots;

	double time_dif = the_time - last_radius_time;
	last_radius_time = the_time;

	radius_i += PI_shift * time_dif;

	//pull it back down
	while(radius_i > PI_shift)
	{
		radius_i -= PI_shift;
	}

	if(the_time >= next_waypoint_time)
	{
		waypoint_i++;
		if(waypoint_i >= 4)
		{
			waypoint_i = 0;
		}

		next_waypoint_time = the_time + 0.1;
	}

	if(show_waypoints)
	{
		waypoint_cursor.Process(the_time);

		if(the_time >= render_death_time)
		{
			show_waypoints = false;
		}
	}

	TryDropTracks();

	return 1;
}

void ZObject::ShowWaypoints()
{
	double &the_time = ztime->ztime;

	render_death_time = the_time + 3.0;
	show_waypoints = true;

	//set cursor
	if(!waypoint_list.size())
	{
		waypoint_cursor.SetCursor(PLACED_C);
		return;
	}

	std::vector<waypoint>::iterator i=waypoint_list.end();
	i--;

	switch(i->mode)
	{
	case MOVE_WP:
	case FORCE_MOVE_WP:
	case ENTER_FORT_WP:
	case DODGE_WP:
		waypoint_cursor.SetCursor(PLACED_C);
		break;
	case PICKUP_GRENADES_WP:
		waypoint_cursor.SetCursor(GRABBED_C);
		break;
	case ENTER_WP:
		waypoint_cursor.SetCursor(ENTERED_C);
		break;
	case ATTACK_WP:
	case AGRO_WP:
		waypoint_cursor.SetCursor(ATTACKED_C);
		break;
	case CRANE_REPAIR_WP:
	case UNIT_REPAIR_WP:
		waypoint_cursor.SetCursor(REPAIRED_C);
		break;
	default:
		waypoint_cursor.SetCursor(PLACED_C);
		break;
	}
		
}

void ZObject::DoRenderWaypoints(ZMap &the_map, SDL_Surface *dest, std::vector<ZObject*> &object_list, bool is_rally_points, int shift_x, int shift_y)
{
	int x, y;
	int nx, ny;
	const int glvl = 170;
	SDL_Rect the_box;
	std::vector<waypoint>::iterator i;
	double tx,ty;
	double txp, typ;
	double dx, dy;
	double dist;
	int ox, oy;
	int maxj;
	int j;
	int mshift_x, mshift_y;
	int view_h, view_w;
	ZObject *target_object;
	std::vector<waypoint> *render_waypoint_list;

	if(!is_rally_points && !show_waypoints)
	{
		return;
	}

	if(is_rally_points)
	{
		if(!GetBuildingCreationPoint(x, y))
		{
			x = center_x;
			y = center_y;
		}
		render_waypoint_list = &rallypoint_list;
	}
	else
	{
		x = center_x;
		y = center_y;
		render_waypoint_list = &waypoint_list;
	}

	//do we even have waypoints to render?
	if(!render_waypoint_list->size())
	{
		return;
	}

	the_map.GetViewShiftFull(mshift_x, mshift_y, view_w, view_h);

	view_w -= 2;
	view_h -= 2;

	tx = x - mshift_x;
	ty = y - mshift_y;

	//draw from inside
	if(is_rally_points && GetBuildingCreationMovePoint(nx, ny))
	{
		dx = nx - x;
		dy = ny - y;

		dist = sqrt((dx * dx) + (dy * dy));

		txp = 4 * dx / dist;
		typ = 4 * dy / dist;

		tx += txp * (waypoint_i / 4.0);
		ty += typ * (waypoint_i / 4.0);

		maxj = dist / 4;
		maxj++;

		if(waypoint_i)
		{
			maxj--;
		}

		//draw this segments points
		for(j=0;j<maxj;j++)
		{
			if((int)tx < view_w && (int)ty < view_h)
			{
				the_box.x = (int)tx;
				the_box.y = (int)ty;
				the_box.w = 2;
				the_box.h = 2;

				ZSDL_FillRect(&the_box, glvl, glvl, glvl);
			}

			tx += txp;
			ty += typ;
		}

		//setup for next round
		tx = (nx - mshift_x);
		ty = (ny - mshift_y);
	}
	
	for(i=render_waypoint_list->begin();i!=render_waypoint_list->end();i++)
	{
		switch(i->mode)
		{
		case MOVE_WP:
		case FORCE_MOVE_WP:
		case ENTER_WP:
		case CRANE_REPAIR_WP:
		case UNIT_REPAIR_WP:
		case ENTER_FORT_WP:
		case DODGE_WP:
		case PICKUP_GRENADES_WP:
			nx = (i->x - mshift_x);
			ny = (i->y - mshift_y);
			break;
		case ATTACK_WP:
		case AGRO_WP:
			target_object = GetObjectFromID(i->ref_id, object_list);

			if(target_object)
			{
				target_object->GetCenterCords(ox, oy);
				nx = (ox - mshift_x);
				ny = (oy - mshift_y);
			}
			else
			{
				nx = (i->x - mshift_x);
				ny = (i->y - mshift_y);
			}
			
			break;
		}

		//render
		RenderWaypointLine(tx, ty, nx, ny, view_h, view_w);

		//set old to new
		tx = nx;
		ty = ny;
	}

	//render cursor
	if(!render_waypoint_list->size())
	{
		return;
	}

	int kx, ky;
	i=render_waypoint_list->end();
	i--;

	switch(i->mode)
	{
	case MOVE_WP:
	case FORCE_MOVE_WP:
	case ENTER_FORT_WP:
	case DODGE_WP:
		kx = i->x;
		ky = i->y;
		waypoint_cursor.SetCursor(PLACED_C);
		break;
	case ENTER_WP:
		kx = i->x;
		ky = i->y;
		waypoint_cursor.SetCursor(ENTERED_C);
		break;
	case PICKUP_GRENADES_WP:
		kx = i->x;
		ky = i->y;
		waypoint_cursor.SetCursor(GRABBED_C);
		break;
	case CRANE_REPAIR_WP:
	case UNIT_REPAIR_WP:
		kx = i->x;
		ky = i->y;
		waypoint_cursor.SetCursor(REPAIRED_C);
		break;
	case ATTACK_WP:
	case AGRO_WP:
		target_object = GetObjectFromID(i->ref_id, object_list);

		if(target_object)
		{
			target_object->GetCenterCords(ox, oy);

			kx = ox;
			ky = oy;

			waypoint_cursor.SetCursor(ATTACKED_C);
		}
		else
		{
			kx = i->x;
			ky = i->y;
			waypoint_cursor.SetCursor(PLACED_C);
		}
		
		break;
	}

	waypoint_cursor.Render(the_map, dest, kx, ky, true);
}

void ZObject::RenderWaypointLine(int sx, int sy, int ex, int ey, int view_h, int view_w)
{
	const int glvl = 170;
	SDL_Rect the_box;

	double tx = sx;
	double ty = sy;

	double dx = ex - sx;
	double dy = ey - sy;

	double dist = sqrt((dx * dx) + (dy * dy));

	double txp = 4 * dx / dist;
	double typ = 4 * dy / dist;

	tx += txp * (waypoint_i / 4.0);
	ty += typ * (waypoint_i / 4.0);

	int maxj = dist / 4;
	maxj++;

	if(waypoint_i)
	{
		maxj--;
	}

	//draw this segments points
	for(int j=0;j<maxj;j++)
	{
		if((int)tx < view_w && (int)ty < view_h)
		{
			the_box.x = (int)tx;
			the_box.y = (int)ty;
			the_box.w = 2;
			the_box.h = 2;

			ZSDL_FillRect(&the_box, glvl, glvl, glvl);
		}

		tx += txp;
		ty += typ;
	}
}

void ZObject::DoRender(ZMap &the_map, SDL_Surface *dest, int shift_x, int shift_y)
{
	
}

void ZObject::DoAfterEffects(ZMap &the_map, SDL_Surface *dest, int shift_x, int shift_y)
{

}

void ZObject::SetDestroyed(bool is_destroyed)
{
	destroyed = is_destroyed;
}

void ZObject::GetObjectID(unsigned char &object_type_, unsigned char &object_id_)
{
   object_type_ = object_type;
   object_id_ = object_id;
}

int ZObject::ProcessServer(ZMap &tmap, ZOLists &ols)
{
	double &the_time = ztime->ztime;
	std::vector<waypoint>::iterator wp;
	bool is_new_waypoint;
	bool attack_player_given = false;

	double time_dif = the_time - last_process_server_time;
	last_process_server_time = the_time;

	sflags.clear();

	//it being repaired?
	sflags.auto_repair = DoAutoRepair(tmap, ols);

	//increase / decrease stamina
	ProcessRunStamina(time_dif);

	if(IsDestroyed())
	{
		return 0;
	}

	//was a unit created?
	sflags.build_unit = BuildUnit(the_time, sflags.bot, sflags.boid);

	//repair unit
	sflags.repair_unit = RepairUnit(the_time, sflags.rot, sflags.roid, sflags.rdriver_type, sflags.rdriver_info, sflags.rwaypoint_list);

	//lid business
	ProcessServerLid();

	//go through waypoints
	if(waypoint_list.size())
	{
		wp = waypoint_list.begin();

		if(*wp != last_wp)
		{
			is_new_waypoint = true;
			last_wp = *wp;
			xover = yover = 0;

			cur_wp_info.clear_and_kill(tmap);

			//because all waypoints use cur_wp_info.x+y for movement
			//we set it to our current location to set ourselves to
			//an initial "stop"
			SetTarget(center_x, center_y);

			//stop running
			is_running = false;
		}
		else
		{
			is_new_waypoint = false;
		}

		switch(wp->mode)
		{
		case MOVE_WP:
			ProcessMoveWP(wp, time_dif, is_new_waypoint, ols, tmap, true);
			break;
		case FORCE_MOVE_WP:
			ProcessMoveWP(wp, time_dif, is_new_waypoint, ols, tmap, false);
			break;
		case DODGE_WP:
			ProcessDodgeWP(wp, time_dif, is_new_waypoint, ols, tmap);
			break;
		case ENTER_WP:
			ProcessEnterWP(wp, time_dif, is_new_waypoint, ols, tmap);
			break;
		case ATTACK_WP:
			attack_player_given = wp->player_given;
			ProcessAttackWP(wp, time_dif, is_new_waypoint, ols, tmap);
			break;
		case AGRO_WP:
			ProcessAgroWP(wp, time_dif, is_new_waypoint, ols, tmap);
			break;
		case CRANE_REPAIR_WP:
			ProcessCraneRepairWP(wp, time_dif, is_new_waypoint, ols, tmap);
			break;
		case UNIT_REPAIR_WP:
			ProcessUnitRepairWP(wp, time_dif, is_new_waypoint, ols, tmap);
			break;
		case ENTER_FORT_WP:
			ProcessEnterFortWP(wp, time_dif, is_new_waypoint, ols, tmap);
			break;
		case PICKUP_GRENADES_WP:
			ProcessPickupWP(wp, time_dif, is_new_waypoint, ols, tmap);
			break;
		default:
			spdlog::warn("ZObject::ProcessServer - Killing unknown waypoint {}", wp->mode);
			KillWP(wp);
			break;
		}
	}

	//see if they want to attack someone nearby
	CheckPassiveEngage(the_time, ols);

	//let them do their damage
	ProcessAttackDamage(tmap, attack_player_given);

	//update location only happens in intervals
	//update velocity always happens
	if(sflags.updated_location)
	{
		if(the_time >= next_loc_update_time)
		{
			next_loc_update_time = the_time + loc_update_int;
		}
		else
		{
			sflags.updated_location = false;
		}
	}

	return 1;
}

void ZObject::Engage(ZObject *attack_object_)
{
	//are we really disengaging by chance?
	if(!attack_object_)
	{
		Disengage();
		return;
	}
	if(attack_object == attack_object_)
	{
		return;
	}

	//attack then
	attack_object = attack_object_;
	sflags.updated_attack_object = true;

	//if we can be sniped by the target, we should open our lid!
	if(attack_object->CanSnipe())
	{
		SignalLidShouldOpen();
	}
}

bool ZObject::Disengage()
{
	if(!attack_object)
	{
		return false;
	}

	attack_object = NULL;
	sflags.updated_attack_object = true;

	//we only open our lid while we're engaged.
	SignalLidShouldClose();

	return true;
}

bool ZObject::IsMoving()
{
	return !(OpenZod::Util::Math::IsZ(loc.dx) && OpenZod::Util::Math::IsZ(loc.dy));
}

void ZObject::CheckPassiveEngage(double &the_time, ZOLists &ols)
{
	//is it time, since this is a "tiny bit intensive"
	if(the_time < next_check_passive_attack_time)
	{
		return;
	}

	//set for next time
	next_check_passive_attack_time = the_time + 0.5;

	//can we even attack
	if(!CanAttack())
	{
		return;
	}
	if(owner == NULL_TEAM)
	{
		return;
	}
	if(!(object_type == CANNON_OBJECT || object_type == VEHICLE_OBJECT || object_type == ROBOT_OBJECT))
	{
		return;
	}
	if(object_type == ROBOT_OBJECT && IsMoving())
	{
		return;
	}

	//are we already attacking someone?
	if(attack_object) 
	{
		//is it still within range?
		int ox, oy;
		attack_object->GetCenterCords(ox, oy);
		if(!WithinAttackRadius(attack_object))
		{
			Disengage();
		}
		
		return;
	}

	std::vector<ZObject*>::iterator obj;

	std::vector<ZObject*> agro_choices;
	std::vector<ZObject*> enter_vehicle_choices;
	std::vector<ZObject*> grab_flag_choices;
	std::vector<ZObject*> grab_grenades_choices;

	//lets see if there are any enemies to attack..
	if(CanAttack())
	{
		for(obj=ols.passive_engagable_olist.begin();obj!=ols.passive_engagable_olist.end();obj++)
		{
			int ox, oy;
			unsigned char ot, oid;

			(*obj)->GetObjectID(ot, oid);
			(*obj)->GetCenterCords(ox, oy);

			if((*obj)->GetOwner() != NULL_TEAM && (*obj)->GetOwner() != owner && CanAttackObject((*obj)))
			{
				//do not auto attack buildings
				if(!(ot == CANNON_OBJECT || ot == VEHICLE_OBJECT || ot == ROBOT_OBJECT))
				{
					continue;
				}

				//if(WithinAttackRadius(ox, oy))
				if(WithinAttackRadius(*obj))
				{
					Engage(*obj);
					return;
				}

				//start an agro waypoint?
				//if we have no waypoints and are "not a cannon" / "able to move"
				if(!waypoint_list.size() && object_type != CANNON_OBJECT)
				{
					if(WithinAgroRadius(*obj))
					{
						agro_choices.push_back(*obj);
					}
				}
			}

			//add to the auto enter or grab lists?
			if(!waypoint_list.size() && object_type != CANNON_OBJECT)
			{
				if(!IsMinion() && !agro_choices.size())
				{
					//auto enter vehicle (not apc though)?
					if(object_type == ROBOT_OBJECT && (*obj)->CanBeEntered() && WithinAutoEnterRadius(ox, oy) && !(ot == VEHICLE_OBJECT && oid == APC) && !(this->just_left_cannon && ot == CANNON_OBJECT))
					{
						enter_vehicle_choices.push_back(*obj);
					}
				}
			}
		}
	}

	if(!IsMinion() && !agro_choices.size() && !waypoint_list.size())
	{
		//collect grabbable flags
		if(CanMove())
		{
			for(obj=ols.flag_olist.begin();obj!=ols.flag_olist.end();obj++)
			{
				int ox, oy;
				unsigned char ot, oid;

				(*obj)->GetObjectID(ot, oid);
				(*obj)->GetCenterCords(ox, oy);

				if(ot != MAP_ITEM_OBJECT)
				{
					continue;
				}
				if(oid != FLAG_ITEM)
				{
					continue;
				}
				if((*obj)->GetOwner() == owner)
				{
					continue;
				}
				if(!WithinAutoGrabFlagRadius(ox, oy))
				{
					continue;
				}

				grab_flag_choices.push_back(*obj);
			}
		}

		//collect grenades list
		if(CanPickupGrenades())
		{
			for(obj=ols.grenades_olist.begin();obj!=ols.grenades_olist.end();obj++)
			{
				int ox, oy;
				unsigned char ot, oid;

				(*obj)->GetObjectID(ot, oid);
				(*obj)->GetCenterCords(ox, oy);

				if(ot != MAP_ITEM_OBJECT)
				{
					continue;
				}
				if(oid != GRENADES_ITEM)
				{
					continue;
				}
				if(!WithinAutoEnterRadius(ox, oy))
				{
					continue;
				}

				grab_grenades_choices.push_back(*obj);
			}
		}
	}

	if(agro_choices.size())
	{
		ZObject *agro_choice;
		int ox, oy;
		double least_distance;

		//find choice (closest)
		agro_choice = agro_choices[0];
		least_distance = DistanceFromObject(*agro_choice);
		for(obj=agro_choices.begin()++;obj!=agro_choices.end();obj++)
		{
			double this_distance = DistanceFromObject(**obj);

			if(this_distance < least_distance)
			{
				least_distance = this_distance;
				agro_choice = *obj;
			}
		}
		

		agro_choice->GetCenterCords(ox, oy);

		waypoint new_movepoint;

		new_movepoint.mode = AGRO_WP;
		new_movepoint.ref_id = agro_choice->GetRefID();
		new_movepoint.x = ox;
		new_movepoint.y = oy;

		waypoint_list.push_back(new_movepoint);
	}
	else
	{
		std::vector<ZObject*> *obj_choices = NULL;
		int waypoint_mode = MOVE_WP;
		bool do_auto_enter = false;
		bool do_auto_grab_flag = false;
		bool do_auto_grab_grenades = false;

		if(enter_vehicle_choices.size() && grab_flag_choices.size() && grab_grenades_choices.size())
		{
			switch(OpenZod::Util::Random::Int(0, 2))
			{
			case 0:
				do_auto_enter = true;
				break;
			case 1:
				do_auto_grab_flag = true;
				break;
			case 2:
				do_auto_grab_grenades = true;
				break;
			}
		}
		else if(enter_vehicle_choices.size() && grab_flag_choices.size())
		{
			if(OpenZod::Util::Random::Bool())
			{
				do_auto_enter = true;
			}
			else
			{
				do_auto_grab_flag = true;
			}
		}
		else if(enter_vehicle_choices.size() && grab_grenades_choices.size())
		{
			if(OpenZod::Util::Random::Bool())
			{
				do_auto_enter = true;
			}
			else
			{
				do_auto_grab_grenades = true;
			}
		}
		else if(grab_flag_choices.size() && grab_grenades_choices.size())
		{
			if(OpenZod::Util::Random::Bool())
			{
				do_auto_grab_flag = true;
			}
			else
			{
				do_auto_grab_grenades = true;
			}
		}
		else if(enter_vehicle_choices.size())
		{
			do_auto_enter = true;
		}
		else if(grab_flag_choices.size())
		{
			do_auto_grab_flag = true;
		}
		else if(grab_grenades_choices.size())
		{
			do_auto_grab_grenades = true;
		}

		if(do_auto_enter)
		{
			obj_choices = &enter_vehicle_choices;
			waypoint_mode = ENTER_WP;
		}
		else if(do_auto_grab_flag)
		{
			obj_choices = &grab_flag_choices;
			waypoint_mode = MOVE_WP;
		}
		else if(do_auto_grab_grenades)
		{
			obj_choices = &grab_grenades_choices;
			waypoint_mode = PICKUP_GRENADES_WP;
		}

		if(obj_choices && obj_choices->size())
		{
			ZObject *obj_choice;
			int ox, oy;
			double least_distance;

			//find choice (closest)
			obj_choice = *obj_choices->begin();
			least_distance = DistanceFromObject(*obj_choice);
			for(obj=obj_choices->begin()++;obj!=obj_choices->end();obj++)
			{
				double this_distance = DistanceFromObject(**obj);

				if(this_distance < least_distance)
				{
					least_distance = this_distance;
					obj_choice = *obj;
				}
			}

			obj_choice->GetCenterCords(ox, oy);

			waypoint new_movepoint;

			new_movepoint.mode = waypoint_mode;
			new_movepoint.ref_id = obj_choice->GetRefID();
			new_movepoint.x = ox;
			new_movepoint.y = oy;
			new_movepoint.attack_to = true;

			waypoint_list.push_back(new_movepoint);

			//doesn't hurt to call this if we don't have minions
			//but needed if we do
			CloneMinionWayPoints();
		}
	}
}

bool ZObject::EstimateMissileTarget(ZObject *target, int &tx, int &ty)
{
	//some checks
	if(missile_speed <= 0)
	{
		return false;
	}

	//don't need to estimate if
	//the target isn't moving
	if (!target->IsMoving())
	{
		return false;
	}

	double dx2, dy2;
	int ixo, iyo;

	double dx = target->loc.dx;
	double dy = target->loc.dy;
	target->GetCenterCords(ixo, iyo);
	double xo = ixo;
	double yo = iyo;

	double x2o = center_x;
	double y2o = center_y;

	double Cu = yo - y2o;
	double Cd = xo - x2o;

	double a = (Cu * Cu) + (Cd * Cd);
	double b = (2 * Cu * Cd * dy) - (2 * Cu * Cu * dx);
	double c = (Cd * Cd * dy * dy) - (2 * Cu * Cd * dx) + (Cu * Cu * dx * dx) - (Cd * Cd * missile_speed * missile_speed);
	double d = (b * b) - (4 * a * c);

	if(d <= 0.00001) 
	{
		spdlog::error("ZObject::EstimateMissileTarget - Not solvable");
		return false;
	}
	if(OpenZod::Util::Math::IsZ(a))
	{
		spdlog::error("ZObject::EstimateMissileTarget - a is zero");
		return false;
	}

	dx2 = (-1 * b - sqrt(d)) / (2 * a);

	double dy2_guts = (double)(missile_speed * missile_speed) - (dx2 * dx2);

	if(dy2_guts <= 0.00001) 
	{
		dx2 = (-1 * b + sqrt(d)) / (2 * a);

		dy2_guts = (missile_speed * missile_speed) - (dx2 * dx2);

		if(dy2_guts <= 0.00001) 
		{
			spdlog::error("ZObject:EstimateMissileTarget - Bad dy2_guts value {}", dy2_guts);
			return false;
		}
	}

	dy2 = sqrt(dy2_guts);

	double t;
	double dd;

	//dd = dx - dx2;
	if(!OpenZod::Util::Math::IsZ(dd = dx - dx2))
	{
		t = -1 * Cd / dd;
	}
	else if(!OpenZod::Util::Math::IsZ(dd = dy - dy2))
	{
		t = -1 * Cu / dd;
	}
	else
	{
		spdlog::error("ZObject::EstimateMissileTarget - Bad dd value {}", dd);
		return false;
	}

	if(t < 0)
	{
		dx2 = (-1 * b + sqrt(d)) / (2 * a);

		double dy2_guts = (double)(missile_speed * missile_speed) - (dx2 * dx2);

		if(dy2_guts <= 0.00001) 
		{
			spdlog::error("ZObject::EstimateMissileTarget - Bad dy2_guts value {}", dy2_guts);
			return false;
		}

		dy2 = sqrt(dy2_guts);

		//dd = dx - dx2;
		if(!OpenZod::Util::Math::IsZ(dd = dx - dx2))
		{
			t = -1 * Cd / dd;
		}
		else if(!OpenZod::Util::Math::IsZ(dd = dy - dy2))
		{
			t = -1 * Cu / dd;
		}
		else
		{
			spdlog::error("ZObject::EstimateMissileTarget - Bad dd value {}", dd);
			return false;
		}

		if(t < 0)
		{
			spdlog::error("ZObject::EstimateMissileTarget - Bad t value {}", t);
			return false;
		}
	}

	tx = dx * t + xo;
	ty = dy * t + yo;

	return true;
}

bool ZObject::NearestAttackLoc(int sx, int sy, int &ex, int &ey, int aa_radius, bool is_robot, ZMap &tmap)
{
	int &x = loc.x;
	int &y = loc.y;
	int &cx = center_x;
	int &cy = center_y;
	

	//normal checks
	{
		//center ok?
		ex = cx; ey = cy;
		if(tmap.GetPathFinder().ShouldBeAbleToMoveTo(sx, sy, ex, ey, is_robot))
		{
			return true;
		}

		//+8 ok?
		ex = x+8; ey = y+8;
		if((ex != cx && ey != cy) && tmap.GetPathFinder().ShouldBeAbleToMoveTo(sx, sy, ex, ey, is_robot))
		{
			return true;
		}
	}

	//quick cannon fort checks
	{
		//up 2 tiles ok?
		ex = x+8; ey = (y+8)-32;
		if((ex != cx && ey != cy) && tmap.GetPathFinder().ShouldBeAbleToMoveTo(sx, sy, ex, ey, is_robot))
		{
			return true;
		}

		//right 3 tiles ok?
		ex = x+8+48; ey = y+8;
		if((ex != cx && ey != cy) && tmap.GetPathFinder().ShouldBeAbleToMoveTo(sx, sy, ex, ey, is_robot))
		{
			return true;
		}

		//left 3 tiles ok?
		ex = (x+8)-48; ey = y+8;
		if((ex != cx && ey != cy) && tmap.GetPathFinder().ShouldBeAbleToMoveTo(sx, sy, ex, ey, is_robot))
		{
			return true;
		}
	}

	//full direct line check
	{
		ZPath_Finding_Bresenham ls;
		int lx, ly;

		ls.Init(cx>>4, cy>>4, sx>>4, sy>>4, tmap.GetMapBasics().width, tmap.GetMapBasics().height);

		while(ls.GetNext(lx, ly))
		{
			ex = (lx<<4)+8;
			ey = (ly<<4)+8;

			if(!OpenZod::Util::Math::PointsWithinDistance(cx, cy, ex, ey, aa_radius))
			{
				break;
			}

			if(tmap.GetPathFinder().ShouldBeAbleToMoveTo(sx, sy, ex, ey, is_robot))
			{
				return true;
			}
		}
	}

	return false;
}

double ZObject::DamagedSpeed()
{
	if(ShowPartiallyDamaged())
	{
		return zsettings->partially_damaged_unit_speed;
	}
	if(ShowDamaged())
	{
		return zsettings->damaged_unit_speed;
	}

	return 1.0;
}

double ZObject::RunSpeed() 
{
	if(leader_obj)
	{
		return leader_obj->is_running && !ShowDamaged() ? zsettings->run_unit_speed : 1.0; 
	}
	else
	{
		return is_running && !ShowDamaged() ? zsettings->run_unit_speed : 1.0; 
	}
}

bool ZObject::CanReachTargetRunning(int x, int y)
{
	//an estimate because the real speed can change with terrain
	double distance_runnable = move_speed * stamina;

	return OpenZod::Util::Math::PointsWithinDistance(center_x, center_y, x, y, distance_runnable);
}

void ZObject::ProcessRunStamina(double time_dif)
{
	if(is_running)
	{
		stamina -= time_dif;

		if(stamina < 0)
		{
			stamina = 0;
			is_running = false;
		}
	}
	else
	{
		stamina += time_dif * zsettings->run_recharge_rate;

		if(stamina > max_stamina)
		{
			stamina = max_stamina;
		}
	}
}

void ZObject::AttemptStartRun()
{
	const double min_stamina = 0.3;

	if(is_running)
	{
		return;
	}

	//one in five we don't run
	if(OpenZod::Util::Random::OneInX(5))
	{
		return;
	}

	if(stamina < min_stamina)
	{
		return;
	}

	is_running = true;
}

void ZObject::ProcessAttackDamage(ZMap &tmap, bool attack_player_given)
{
	double &the_time = ztime->ztime;
	if(!attack_object || !damage)
	{
		return;
	}
	if(the_time < next_damage_time)
	{
		return;
	}

	next_damage_time = the_time + damage_int_time;

	if(!this->CanAttackObject(attack_object))
	{
		Disengage();
		return;
	}

	bool can_attack_with_grenades = (GetGrenadeAmount() || (GetGroupLeader() && GetGroupLeader()->GetGrenadeAmount()));

	if(damage_is_missile || can_attack_with_grenades)
	{
		int tx, ty;
		damage_missile new_missile;

		if(!EstimateMissileTarget(attack_object, tx, ty))
		{
			attack_object->GetCenterCords(tx, ty);
		}

		if(can_attack_with_grenades)
		{
			tx += (rand() % 48) - 24;
			ty += (rand() % 48) - 24;

			new_missile.x = tx;
			new_missile.y = ty;
			new_missile.damage = zsettings->grenade_damage * MAX_UNIT_HEALTH;
			new_missile.radius = zsettings->grenade_damage_radius;
			new_missile.team = owner;
			new_missile.attacker_ref_id = ref_id;
			new_missile.attack_player_given = attack_player_given;
			new_missile.target_ref_id = attack_object->GetRefID();
			new_missile.CalcExplodeTimeTo(center_x, center_y, zsettings->grenade_missile_speed, the_time);

			//remove a grenade
			if(GetGrenadeAmount())
			{
				SetGrenadeAmount(GetGrenadeAmount()-1);
				sflags.updated_grenade_amount = true;
			}
			else if(GetGroupLeader() && GetGroupLeader()->GetGrenadeAmount())
			{
				GetGroupLeader()->SetGrenadeAmount(GetGroupLeader()->GetGrenadeAmount()-1);
				sflags.updated_leader_grenade_amount = true;
			}

			//set a different next-time
			next_damage_time = the_time + zsettings->grenade_attack_speed;
		}
		else
		{
			tx += (rand() % 32) - 16;
			ty += (rand() % 32) - 16;

			new_missile.x = tx;
			new_missile.y = ty;
			new_missile.damage = damage;
			new_missile.radius = damage_radius;
			new_missile.team = owner;
			new_missile.attacker_ref_id = ref_id;
			new_missile.attack_player_given = attack_player_given;
			new_missile.target_ref_id = attack_object->GetRefID();
			new_missile.CalcExplodeTimeTo(center_x, center_y, missile_speed, the_time);
		}

		damage_missile_list->push_back(new_missile);

		sflags.fired_missile = true;
		sflags.missile_x = tx;
		sflags.missile_y = ty;

		//tell the unit to dodge
		attack_object->DodgeMissile(tx, ty, new_missile.explode_time - the_time);
	}
	else
	{
		if(OpenZod::Util::Random::Float(0, 1) > damage_chance)
		{
			return;
		}

		//attack vehicle or driver?
		if(can_snipe && attack_object->CanBeSniped() && (OpenZod::Util::Random::Float(0, 1) <= snipe_chance))
		{
			attack_object->DamageDriverHealth(damage);
			sflags.updated_attack_object_driver_health = true;
		}
		else
		{
			attack_object->DamageHealth(damage, tmap);
			sflags.updated_attack_object_health = true;
		}

		//are we a pyro? have to set victom's last hurt by fire time 
		//(so that we can do the melt death effect)
		if(object_type == ROBOT_OBJECT && object_id == PYRO)
		{
			attack_object->SetDamagedByFireTime(the_time);
		}

		//if destroyed give a target destroyed
		if(attack_object->IsDestroyed() && attack_player_given)
		{
			sflags.portrait_anim_ref_id = ref_id;
			sflags.portrait_anim_value = TARGET_DESTROYED_ANIM;
		}
	}
}

void ZObject::ProcessKillObject()
{
	double &the_time = ztime->ztime;

	if(object_type == BUILDING_OBJECT && (object_id != FORT_FRONT && object_id != FORT_BACK))
	{
		do_auto_repair = true;
		next_auto_repair_time = the_time + zsettings->building_auto_repair_time;

		if(zsettings->building_auto_repair_random_additional_time > 0)
		{
			next_auto_repair_time += OpenZod::Util::Random::Int(0, zsettings->building_auto_repair_random_additional_time);
		}
	}

	//stop production?
	if(ProducesUnits())
	{
		StopBuildingProduction();
	}
}

void ZObject::ProcessAgroWP(std::vector<waypoint>::iterator &wp, double time_dif, bool is_new, ZOLists &ols, ZMap &tmap)
{
	if(is_new)
	{
		//set the agro center
		cur_wp_info.agro_center_x = center_x;
		cur_wp_info.agro_center_y = center_y;
		cur_wp_info.stage = ATTACK_AWS;
	}

	ZObject* target_object = GetObjectFromID(wp->ref_id, *ols.object_list);

	//we still attacking this target?
	if(!CanAttackObject(target_object))
	{
		KillWP(wp);
		return;
	}

	int ox, oy;
	target_object->GetCenterCords(ox, oy);

	if(!OpenZod::Util::Math::PointsWithinDistance(ox, oy, cur_wp_info.agro_center_x, cur_wp_info.agro_center_y, attack_radius + zsettings->agro_distance))
	{
		//go back to center
		//if we aren't already
		if(cur_wp_info.stage != RETURN_URWS)
		{
			cur_wp_info.stage = RETURN_URWS;
			SetTarget(cur_wp_info.agro_center_x, cur_wp_info.agro_center_y);
			SetVelocity();
		}
	}
	else
	{
		//we are within distance
		//so continue the attack if we are not already
		if(cur_wp_info.stage != ATTACK_AWS)
		{
			cur_wp_info.stage = ATTACK_AWS;
			SetTarget(ox, oy);
			SetVelocity();
		}
	}

	if(cur_wp_info.stage == ATTACK_AWS)
	{
		//we're there
		if(WithinAttackRadius(target_object))
		{
			StopMove();
			//attack!
			Engage(target_object);
		}
		else
		{
			//hunt down the bastard

			//move there
			SetTarget(ox, oy);
			SetVelocity();

			//did we halt movement?
			if(!ProcessMoveOrKillWP(time_dif, tmap, wp, ols))
			{
				return;
			}
		}
	}
	else
	{
		if(!ProcessMoveOrKillWP(time_dif, tmap, wp, ols))
		{
			return;
		}

		if(!ReachedTarget())
		{
			return;
		}

		KillWP(wp);
		return;
	}
}

void ZObject::ProcessAttackWP(std::vector<waypoint>::iterator &wp, double time_dif, bool is_new, ZOLists &ols, ZMap &tmap)
{
	int &x = loc.x;
	int &y = loc.y;

	ZObject* target_object = GetObjectFromID(wp->ref_id, *ols.object_list);

	//we still attacking this target?
	if(!CanAttackObject(target_object))
	{
		KillWP(wp);
		return;
	}

	int ox, oy;
	target_object->GetCenterCords(ox, oy);

	//we're there
	if(WithinAttackRadius(target_object))
	{
		StopMove();

		//attack!
		Engage(target_object);
	}
	else
	{
		//hunt down the bastard

		//are we waiting for a path finding response?
		if(cur_wp_info.path_finding_id)
		{
			return;
		}
		else if(!cur_wp_info.got_pf_response)
		{
			//we are not waiting for waypoints
			//so start doing that
			if(!target_object->NearestAttackLoc(x, y, cur_wp_info.x, cur_wp_info.y, attack_radius, (object_type == ROBOT_OBJECT), tmap))
			{
				target_object->GetCords(ox, oy);
				SetTarget(ox+8, oy+8);
			}
			else
			{
				SetTarget();
			}

			//needed to check if we should recalc a path
			cur_wp_info.init_attack_x = cur_wp_info.x;
			cur_wp_info.init_attack_y = cur_wp_info.y;

			cur_wp_info.path_finding_id = tmap.GetPathFinder().Find_Path(x, y, cur_wp_info.x, cur_wp_info.y, (object_type == ROBOT_OBJECT), HasExplosives(), ref_id);

			//don't wait for thread if it wasn't created
			if(cur_wp_info.path_finding_id)
			{
				StopMove();
			}
			else
			{
				cur_wp_info.got_pf_response = true;
				SetVelocity();
			}

			return;
		}

		//redo path finding?
		{
			int adx, ady;

			adx = ox - cur_wp_info.init_attack_x;
			ady = oy - cur_wp_info.init_attack_y;

			//has it moved too far from our current destination?
			if(abs(adx) > attack_radius || abs(ady) > attack_radius)
			{
				//reset the waypoint
				StopMove();
				cur_wp_info.clear();

				return;
			}
		}

		//move there
		SetVelocity();
		//did we halt movement?
		if(!ProcessMoveOrKillWP(time_dif, tmap, wp, ols))
		{
			return;
		}

		{
			if(!ReachedTarget()) return;

			//go to the next pf_point, or kill this waypoint?
			if(cur_wp_info.pf_point_list.size())
			{
				SetTarget(cur_wp_info.pf_point_list.begin()->x, cur_wp_info.pf_point_list.begin()->y);
				SetVelocity();

				cur_wp_info.pf_point_list.erase(cur_wp_info.pf_point_list.begin());
			}
			else
			{
				//we are out of waypoints

				//reset the waypoint
				StopMove();
				cur_wp_info.clear();
			}
		}
	}
}

void ZObject::ProcessPickupWP(std::vector<waypoint>::iterator &wp, double time_dif, bool is_new, ZOLists &ols, ZMap &tmap)
{
	int &x = loc.x;
	int &y = loc.y;

	ZObject* target_object = GetObjectFromID(wp->ref_id, *ols.object_list);

	//can pickup grenades?
	if(!CanPickupGrenades())
	{
		KillWP(wp);
		return;
	}

	//grenades still exist?
	if(!target_object)
	{
		KillWP(wp);
		return;
	}

	//are they grenades?
	unsigned char ot, oid;
	target_object->GetObjectID(ot, oid);
	if(ot != MAP_ITEM_OBJECT || oid != GRENADES_ITEM)
	{
		KillWP(wp);
		return;
	}

	//are we there?
	if(target_object->UnderCursor(center_x, center_y))
	{
		KillWP(wp);

		if(!IsMinion()) 
		{
			sflags.do_pickup_grenade_anim = true;
			sflags.updated_grenade_amount = true;
			SetGrenadeAmount(GetGrenadeAmount() + target_object->GetGrenadeAmount());
			target_object->SetGrenadeAmount(0);
			target_object->SetHealth(0, tmap);

			sflags.delete_grenade_box_ref_id = target_object->GetRefID();
		}
		return;
	}

	//disengage if this is a robot, because they can not fire and walk
	//also setvelocity
	if(is_new)
	{
		//force move waypoints are expected to always go
		//straight to their targets
		SetTarget(wp->x, wp->y);

		cur_wp_info.path_finding_id = tmap.GetPathFinder().Find_Path(x, y, cur_wp_info.x, cur_wp_info.y, (object_type == ROBOT_OBJECT), HasExplosives(), ref_id);

		//don't wait for thread if it wasn't created
		if(cur_wp_info.path_finding_id)
			StopMove();
		else
		{
			cur_wp_info.got_pf_response = true;
			SetVelocity();
		}

		//run to pick things up
		AttemptStartRun(wp->x, wp->y);
	}

	//attack to
	if(CheckAttackTo(wp, ols))
	{
		return;
	}

	if(!cur_wp_info.got_pf_response)
	{
		return;
	}

	//did we halt movement?
	if(!ProcessMoveOrKillWP(time_dif, tmap, wp, ols))
	{
		return;
	}

	if(!ReachedTarget())
	{
		return;
	}

	//go to the next pf_point, or kill this waypoint?
	if(cur_wp_info.pf_point_list.size())
	{
		SetTarget(cur_wp_info.pf_point_list.begin()->x, cur_wp_info.pf_point_list.begin()->y);
		SetVelocity();

		cur_wp_info.pf_point_list.erase(cur_wp_info.pf_point_list.begin());
	}
	else
	{
		KillWP(wp);
	}
}

void ZObject::ProcessEnterWP(std::vector<waypoint>::iterator &wp, double time_dif, bool is_new, ZOLists &ols, ZMap &tmap)
{
	int &x = loc.x;
	int &y = loc.y;

	if(is_new)
	{
		SetTarget(wp->x, wp->y);

		cur_wp_info.path_finding_id = tmap.GetPathFinder().Find_Path(x, y, cur_wp_info.x, cur_wp_info.y, (object_type == ROBOT_OBJECT), HasExplosives(), ref_id);

		//don't wait for thread if it wasn't created
		if(cur_wp_info.path_finding_id)
		{
			StopMove();
		}
		else
		{
			cur_wp_info.got_pf_response = true;
			SetVelocity();
		}

		//run to enter vehicles
		AttemptStartRun(wp->x, wp->y);
	}

	ZObject* target_object = GetObjectFromID(wp->ref_id, *ols.object_list);
	if(!target_object || !target_object->CanBeEntered())
	{
		KillWP(wp);
		return;
	}

	int &cx = center_x;
	int &cy = center_y;

	//are we at the target?
	if(target_object->UnderCursor(cx, cy))
	{
		KillWP(wp);

		if(!IsMinion())
		{
			sflags.entered_target_ref_id = target_object->GetRefID();
		}
		return;
	}

	//attack to
	if(CheckAttackTo(wp, ols))
	{
		return;
	}

	//don't actually move without the pf_waypoints
	if(!cur_wp_info.got_pf_response)
	{
		return;
	}

	//did we halt movement?
	if(!ProcessMoveOrKillWP(time_dif, tmap, wp, ols))
	{
		return;
	}

	if(!ReachedTarget())
	{
		return;
	}

	//go to the next pf_point, or kill this waypoint?
	if(cur_wp_info.pf_point_list.size())
	{
		SetTarget(cur_wp_info.pf_point_list.begin()->x, cur_wp_info.pf_point_list.begin()->y);
		SetVelocity();

		cur_wp_info.pf_point_list.erase(cur_wp_info.pf_point_list.begin());
	}
	else
	{
		KillWP(wp);
	}
}

void ZObject::ProcessDodgeWP(std::vector<waypoint>::iterator &wp, double time_dif, bool is_new, ZOLists &ols, ZMap &tmap)
{
	if(is_new)
	{
		SetTarget(wp->x, wp->y);
		SetVelocity();
		AttemptStartRun();
	}

	//did we halt movement?
	if(!ProcessMoveOrKillWP(time_dif, tmap, wp, ols))
	{
		return;
	}

	if(!ReachedTarget())
	{
		return;
	}

	KillWP(wp);
}

void ZObject::ProcessMoveWP(std::vector<waypoint>::iterator &wp, double time_dif, bool is_new, ZOLists &ols, ZMap &tmap, bool stoppable)
{
	int &x = loc.x;
	int &y = loc.y;

	//disengage if this is a robot, because they can not fire and walk
	//also setvelocity
	if(is_new)
	{
		//force move waypoints are expected to always go
		//straight to their targets
		if(stoppable)
		{
			SetTarget(wp->x, wp->y);

			cur_wp_info.path_finding_id = tmap.GetPathFinder().Find_Path(x, y, cur_wp_info.x, cur_wp_info.y, (object_type == ROBOT_OBJECT), HasExplosives(), ref_id);

			//don't wait for thread if it wasn't created
			if(cur_wp_info.path_finding_id)
			{
				StopMove();
			}
			else
			{
				cur_wp_info.got_pf_response = true;
				SetVelocity();
			}
		}
		else
		{
			SetTarget(wp->x, wp->y);
			SetVelocity();
		}

		//run if we are going for a flag
		for(ZObject* obj : ols.flag_olist)
		{
			if(obj->DistanceFromCoords(wp->x, wp->y) <= 32)
			{
				AttemptStartRun(wp->x, wp->y);
				break;
			}
		}
	}

	//attack to
	if(CheckAttackTo(wp, ols))
	{
		return;
	}

	if(!cur_wp_info.got_pf_response && stoppable)
	{
		return;
	}

	//did we halt movement?
	if(!ProcessMoveOrKillWP(time_dif, tmap, wp, ols, stoppable))
	{
		return;
	}

	if(!ReachedTarget())
	{
		return;
	}

	//go to the next pf_point, or kill this waypoint?
	if(cur_wp_info.pf_point_list.size())
	{
		SetTarget(cur_wp_info.pf_point_list.begin()->x, cur_wp_info.pf_point_list.begin()->y);
		SetVelocity();

		cur_wp_info.pf_point_list.erase(cur_wp_info.pf_point_list.begin());
	}
	else
	{
		KillWP(wp);
	}
}

void ZObject::ProcessEnterFortWP(std::vector<waypoint>::iterator &wp, double time_dif, bool is_new, ZOLists &ols, ZMap &tmap)
{
	int &x = loc.x;
	int &y = loc.y;
	bool stoppable;

	ZObject* target_object = GetObjectFromID(wp->ref_id, ols.building_olist);

	//target still exist?
	//target still need repaired?
	if(!target_object)
	{
		KillWP(wp);
		return;
	}

	if(is_new)
	{
		cur_wp_info.stage = GOTO_ENTRANCE_EFWS;

		//goto entrace
		if(target_object->GetBuildingCreationMovePoint(cur_wp_info.x, cur_wp_info.y))
		{
			//these will be our exit cords for the last stage
			cur_wp_info.fort_exit_x = cur_wp_info.x;
			cur_wp_info.fort_exit_y = cur_wp_info.y;

			SetTarget();

			cur_wp_info.path_finding_id = tmap.GetPathFinder().Find_Path(x, y, cur_wp_info.x, cur_wp_info.y, (object_type == ROBOT_OBJECT), HasExplosives(), ref_id);

			//don't wait for thread if it wasn't created
			if(cur_wp_info.path_finding_id)
			{
				StopMove();
			}
			else
			{
				cur_wp_info.got_pf_response = true;
				SetVelocity();
			}
		}
		else
		{
			//didn't get the entrance points?
			KillWP(wp);
			return;
		}
	}

	if(!target_object->CanEnterFort(owner))
	{
		if(cur_wp_info.stage == GOTO_ENTRANCE_EFWS)
		{
			KillWP(wp);
			return;
		}
		else if(cur_wp_info.stage == ENTER_BUILDING_EFWS)
		{
			//if the fort gets destroyed while we are entering it
			//then execute the exit stage of this WP
			SetTarget(cur_wp_info.fort_exit_x, cur_wp_info.fort_exit_y);

			cur_wp_info.stage = EXIT_BUILDING_EFWS;
			SetVelocity();
		}
	}

	//attack to
	if(CheckAttackTo(wp, ols))
	{
		return;
	}

	//don't move if we do not have a response
	if(!cur_wp_info.got_pf_response)
	{
		return;
	}

	switch(cur_wp_info.stage)
	{
	case GOTO_ENTRANCE_EFWS:
		stoppable = true;
		break;
	case ENTER_BUILDING_EFWS:
	case EXIT_BUILDING_EFWS:
		stoppable = false;
		break;
	}

	if(!ProcessMoveOrKillWP(time_dif, tmap, wp, ols, stoppable))
	{
		return;
	}

	if(!ReachedTarget())
	{
		return;
	}

	//so we reached our current target... now enter the next stage
	switch(cur_wp_info.stage)
	{
	case GOTO_ENTRANCE_EFWS:
		//go to the next pf_point, or next stage?
		if(cur_wp_info.pf_point_list.size())
		{
			SetTarget(cur_wp_info.pf_point_list.begin()->x, cur_wp_info.pf_point_list.begin()->y);
			SetVelocity();

			cur_wp_info.pf_point_list.erase(cur_wp_info.pf_point_list.begin());
		}
		else
		{
			if(!target_object->GetBuildingCreationPoint(cur_wp_info.x, cur_wp_info.y))
			{
				KillWP(wp);
			}
			else
			{
				SetTarget();
				SetVelocity();

				cur_wp_info.stage = ENTER_BUILDING_EFWS;
			}
		}
		break;
	case ENTER_BUILDING_EFWS:
		SetTarget(cur_wp_info.fort_exit_x, cur_wp_info.fort_exit_y);

		cur_wp_info.stage = EXIT_BUILDING_EFWS;

		//destroy the fort
		sflags.destroy_fort_building_ref_id = target_object->GetRefID();

		SetVelocity();
		break;
	case EXIT_BUILDING_EFWS:
		KillWP(wp);
		break;
	default:
		KillWP(wp);
		break;
	}
}

void ZObject::ProcessCraneRepairWP(std::vector<waypoint>::iterator &wp, double time_dif, bool is_new, ZOLists &ols, ZMap &tmap)
{
	int &x = loc.x;
	int &y = loc.y;
	bool stoppable;

	ZObject* target_object = GetObjectFromID(wp->ref_id, ols.building_olist);

	//target still exist?
	//target still need repaired?
	if(!target_object)
	{
		KillWP(wp);
		return;
	}

	//begin movement towards the entrance
	if(is_new)
	{
		int ent_x, ent_y, ent_x2, ent_y2;

		cur_wp_info.stage = GOTO_ENTRANCE_CRWS;

		if(target_object->GetCraneEntrance(ent_x, ent_y, ent_x2, ent_y2))
		{
			if(ent_x == ent_x2 && ent_y == ent_y2)
			{
				//building has one entrance
				SetTarget(ent_x, ent_y);
			}
			else
			{
				//bridge has two entrances, which is closer?
				double d1, d2;

				d1 = sqrt(pow((double)(x - ent_x), 2) + pow((double)(y - ent_y), 2));
				d2 = sqrt(pow((double)(x - ent_x2), 2) + pow((double)(y - ent_y2), 2));

				if(d1 < d2)
				{
					SetTarget(ent_x, ent_y);
				}
				else
				{
					SetTarget(ent_x2, ent_y2);
				}
			}
		}
		else //could not get building entrance info?
		{
			KillWP(wp);
			return;
		}

		//these will be our exit cords for the last stage
		cur_wp_info.crane_exit_x = cur_wp_info.x;
		cur_wp_info.crane_exit_y = cur_wp_info.y;

		//find our way to the entrance
		cur_wp_info.path_finding_id = tmap.GetPathFinder().Find_Path(x, y, cur_wp_info.x, cur_wp_info.y, (object_type == ROBOT_OBJECT), HasExplosives(), ref_id);

		//don't wait for thread if it wasn't created
		if(cur_wp_info.path_finding_id)
		{
			StopMove();
		}
		else
		{
			cur_wp_info.got_pf_response = true;
			SetVelocity();
		}
	}

	if(!target_object->CanBeRepairedByCrane(owner))
	{
		if(cur_wp_info.stage == GOTO_ENTRANCE_CRWS)
		{
			KillWP(wp);
			return;
		}
		else if(cur_wp_info.stage == ENTER_BUILDING_CRWS)
		{
			//if the building gets repaired while we are entering it
			//then execute the exit stage of this WP
			SetTarget(cur_wp_info.crane_exit_x, cur_wp_info.crane_exit_y);

			cur_wp_info.stage = EXIT_BUILDING_CRWS;
			SetVelocity();
		}
	}

	//don't move if we do not have a response
	if(!cur_wp_info.got_pf_response)
	{
		return;
	}

	switch(cur_wp_info.stage)
	{
	case GOTO_ENTRANCE_CRWS:
		stoppable = true;
		break;
	case ENTER_BUILDING_CRWS:
	case EXIT_BUILDING_CRWS:
		stoppable = false;
		break;
	}

	if(!ProcessMoveOrKillWP(time_dif, tmap, wp, ols, stoppable))
	{
		return;
	}

	if(!ReachedTarget())
	{
		return;
	}

	//so we reached our current target... now enter the next stage
	switch(cur_wp_info.stage)
	{
	case GOTO_ENTRANCE_CRWS:
		//go to the next pf_point, or next stage?
		if(cur_wp_info.pf_point_list.size())
		{
			SetTarget(cur_wp_info.pf_point_list.begin()->x, cur_wp_info.pf_point_list.begin()->y);
			SetVelocity();

			cur_wp_info.pf_point_list.erase(cur_wp_info.pf_point_list.begin());
		}
		else
		{
			if(!target_object->GetCraneCenter(cur_wp_info.x, cur_wp_info.y))
			{
				KillWP(wp);
			}
			else
			{
				cur_wp_info.stage = ENTER_BUILDING_CRWS;

				SetTarget();
				SetVelocity();

				sflags.set_crane_anim = true;
				sflags.crane_anim_on = true;
				sflags.crane_rep_ref_id = wp->ref_id;
			}
		}
		break;
	case ENTER_BUILDING_CRWS:
		SetTarget(cur_wp_info.crane_exit_x, cur_wp_info.crane_exit_y);
		SetVelocity();

		cur_wp_info.stage = EXIT_BUILDING_CRWS;
		break;
	case EXIT_BUILDING_CRWS:
		//set the building to auto repair immediately...
		target_object->do_auto_repair = true;
		target_object->next_auto_repair_time = 0;

		sflags.set_crane_anim = true;
		sflags.crane_anim_on = false;
		sflags.crane_rep_ref_id = wp->ref_id;
		KillWP(wp);
		break;
	default:
		KillWP(wp);
		break;
	}
}

void ZObject::ProcessUnitRepairWP(std::vector<waypoint>::iterator &wp, double time_dif, bool is_new, ZOLists &ols, ZMap &tmap)
{
	int &x = loc.x;
	int &y = loc.y;
	bool stoppable;
	int ent_x, ent_y;

	ZObject* target_object = GetObjectFromID(wp->ref_id, ols.building_olist);

	//target still exist?
	if(!target_object)
	{
		KillWP(wp);
		return;
	}

	//begin movement towards the entrance
	if(is_new)
	{
		cur_wp_info.stage = GOTO_ENTRANCE_URWS;

		if(target_object->GetRepairEntrance(ent_x, ent_y))
		{
			cur_wp_info.x = ent_x;
			cur_wp_info.y = ent_y;
			SetTarget(ent_x, ent_y);
		}
		else
		{
			KillWP(wp);
			return;
		}

		//find our way to the entrance
		cur_wp_info.path_finding_id = tmap.GetPathFinder().Find_Path(x, y, cur_wp_info.x, cur_wp_info.y, (object_type == ROBOT_OBJECT), HasExplosives(), ref_id);

		//don't wait for thread if it wasn't created
		if(cur_wp_info.path_finding_id)
		{
			StopMove();
		}
		else
		{
			cur_wp_info.got_pf_response = true;
			SetVelocity();
		}
	}

	//still ok to do this wp?
	if(!target_object->CanRepairUnit(owner) || !CanBeRepaired())
	{
		//if the building can not repair us anymore (got destroyed or changed teams?)
		//then if we are entering this building we must now leave it
		if(cur_wp_info.stage == ENTER_BUILDING_URWS)
		{
			if(target_object->GetRepairEntrance(ent_x, ent_y))
			{
				cur_wp_info.x = ent_x;
				cur_wp_info.y = ent_y;
				SetTarget(ent_x, ent_y);
			}
			else
			{
				KillWP(wp);
				return;
			}
			cur_wp_info.stage = EXIT_BUILDING_URWS;
			SetVelocity();
		}
		else
		{
			KillWP(wp);
			return;
		}
	}

	//attack to
	if(CheckAttackTo(wp, ols))
	{
		return;
	}

	//don't move if we do not have a response
	if(!cur_wp_info.got_pf_response)
	{
		return;
	}

	//do we need to wait?
	if(target_object->RepairingAUnit())
	{
		//if the building can repair us but it is repairing someone else,
		//then we will wait if we are in the wait state
		//and we will leave the building if we are currently entering it
		switch(cur_wp_info.stage)
		{
		case ENTER_BUILDING_URWS:
			if(target_object->GetRepairEntrance(ent_x, ent_y))
			{
				SetTarget(ent_x, ent_y);
			}
			else
			{
				KillWP(wp);
				return;
			}
			cur_wp_info.stage = EXIT_BUILDING_URWS;
			SetVelocity();
			break;
		case WAIT_URWS:
			return;
			break;
		}
	}

	switch(cur_wp_info.stage)
	{
	case GOTO_ENTRANCE_URWS:
		stoppable = true;
		break;
	case ENTER_BUILDING_URWS:
	case EXIT_BUILDING_URWS:
		stoppable = false;
		break;
	default:
		stoppable = true;
		break;
	}

	if(!ProcessMoveOrKillWP(time_dif, tmap, wp, ols, stoppable))
	{
		return;
	}

	if(!ReachedTarget())
	{
		return;
	}

	switch(cur_wp_info.stage)
	{
	case GOTO_ENTRANCE_URWS:
		//go to the next pf_point, or next stage?
		if(cur_wp_info.pf_point_list.size())
		{
			SetTarget(cur_wp_info.pf_point_list.begin()->x, cur_wp_info.pf_point_list.begin()->y);
			SetVelocity();

			cur_wp_info.pf_point_list.erase(cur_wp_info.pf_point_list.begin());
		}
		else
		{
			cur_wp_info.stage = WAIT_URWS;
			SetVelocity();
		}
		break;
	case EXIT_BUILDING_URWS:
		cur_wp_info.stage = WAIT_URWS;
		SetVelocity();
		break;
	case ENTER_BUILDING_URWS:
		sflags.entered_repair_building_ref_id = target_object->GetRefID();
		
		//set this to leave the building incase it isn't accepted
		//because of another unit entering at the exact same interval / "time"
		if(target_object->GetRepairEntrance(ent_x, ent_y))
		{
			SetTarget(ent_x, ent_y);
		}
		else
		{
			KillWP(wp);
			return;
		}
		cur_wp_info.stage = EXIT_BUILDING_URWS;
		SetVelocity();
		break;
	case WAIT_URWS:
		if(!target_object->GetRepairCenter(cur_wp_info.x, cur_wp_info.y))
		{
			KillWP(wp);
		}

		cur_wp_info.stage = ENTER_BUILDING_URWS;

		SetTarget();
		SetVelocity();
		break;
	default:
		KillWP(wp);
		break;
	}
}

bool ZObject::CheckAttackTo(std::vector<waypoint>::iterator &wp, ZOLists &ols)
{
	if(wp->attack_to)
	{
		if(!CanOverwriteWP())
		{
			return false;
		}

		//look for attack choices
		std::vector<ZObject*> agro_choices;
		for(ZObject* obj : ols.passive_engagable_olist)
		{
			int ox, oy;
			unsigned char ot, oid;

			obj->GetObjectID(ot, oid);
			obj->GetCenterCords(ox, oy);

			if(obj->GetOwner() != NULL_TEAM && obj->GetOwner() != owner)
			{
				//do not auto attack buildings
				if(!(ot == CANNON_OBJECT || ot == VEHICLE_OBJECT || ot == ROBOT_OBJECT))
				{
					continue;
				}

				//push it?
				if(WithinAgroRadius(obj))
				{
					agro_choices.push_back(obj);
				}
			}
		}

		//choose a random agro choice if we have one
		if(agro_choices.size())
		{
			ZObject *agro_choice;
			int ox, oy;

			//find choice (closest)
			agro_choice = agro_choices[OpenZod::Util::Random::Int(0, agro_choices.size() - 1)];

			agro_choice->GetCenterCords(ox, oy);

			waypoint new_waypoint;

			new_waypoint.mode = ATTACK_WP;
			new_waypoint.ref_id = agro_choice->GetRefID();
			new_waypoint.x = ox;
			new_waypoint.y = oy;

			//push this attack waypoint to the front,
			//and exit this current waypoint
			waypoint_list.insert(waypoint_list.begin(), new_waypoint);

			return true;
		}
	}

	return false;
}

void ZObject::KillWP(std::vector<waypoint>::iterator &wp)
{
	sflags.updated_waypoints = true;
	waypoint_list.erase(wp);

	StopMove();

	//clean this
	last_wp.clear();
}

bool ZObject::CanOverwriteWP()
{
	std::vector<waypoint>::iterator wp;

	//check current wp
	if(waypoint_list.size())
	{
		wp = waypoint_list.begin();

		switch(wp->mode)
		{
		case FORCE_MOVE_WP:
			return false;
			break;
		case CRANE_REPAIR_WP:
			if(cur_wp_info.stage != GOTO_ENTRANCE_CRWS)
				return false;
			break;
		case UNIT_REPAIR_WP:
			if(cur_wp_info.stage != GOTO_ENTRANCE_URWS && cur_wp_info.stage != WAIT_URWS)
				return false;
			break;
		case ENTER_FORT_WP:
			if(cur_wp_info.stage != GOTO_ENTRANCE_EFWS)
				return false;
			break;
		}
	}

	return true;
}

bool ZObject::StopMove()
{
	float &dx = loc.dx;
	float &dy = loc.dy;

	if(!IsMoving())
	{
		return false;
	}

	dx = 0;
	dy = 0;

	sflags.updated_velocity = true;

	return true;
}

void ZObject::SetTarget(int x, int y)
{
	//to
	cur_wp_info.x = x;
	cur_wp_info.y = y;

	//from
	cur_wp_info.sx = center_x;
	cur_wp_info.sy = center_y;

	//distance to go
	cur_wp_info.adx = abs(cur_wp_info.x - cur_wp_info.sx);
	cur_wp_info.ady = abs(cur_wp_info.y - cur_wp_info.sy);
}

bool ZObject::ReachedTarget()
{
	int &x = loc.x;
	int &y = loc.y;
	int &cx = center_x;
	int &cy = center_y;
	
	//we at the target?
	if(cx == cur_wp_info.x && cy == cur_wp_info.y) 
	{
		xover = yover = 0;
		return true;
	}

	//we over pass the target?
	if(abs(cx - cur_wp_info.sx) >= cur_wp_info.adx && abs(cy - cur_wp_info.sy) >= cur_wp_info.ady)
	{
		x = cur_wp_info.x - (width_pix >> 1);
		y = cur_wp_info.y - (height_pix >> 1);
		xover = yover = 0;
		return true;
	}

	return false;
}

bool ZObject::ProcessMoveOrKillWP(double time_dif, ZMap &tmap, std::vector<waypoint>::iterator &wp, ZOLists &ols, bool stoppable)
{
	int stop_x, stop_y;
	if(!ProcessMove(time_dif, tmap, stop_x, stop_y, stoppable))
	{
		//attack our way through?
		if(DoAttackImpassableAtCoords(ols, stop_x, stop_y))
		{
			return false;
		}

		//otherwise kill the move and leave
		KillWP(wp);
		return false;
	}

	return true;
}

bool ZObject::ProcessMove(double time_dif, ZMap &tmap, int &stop_x, int &stop_y, bool stoppable)
{
	int &x = loc.x;
	int &y = loc.y;
	float &dx = loc.dx;
	float &dy = loc.dy;

	//are we moving?
	if(OpenZod::Util::Math::IsZ(dx) && OpenZod::Util::Math::IsZ(dy))
	{
		return true;
	}

	double nx = x + (dx * time_dif) + xover;
	double ny = y + (dy * time_dif) + yover;

	int inx = (int)floor(nx);
	int iny = (int)floor(ny);

	//minions never stoppable (for now)
	if(IsMinion())
	{
		stoppable = false;
	}

	//is the new x and y kosher?
	if(stoppable && tmap.WithinImpassable(inx+1, iny+1, width_pix-2, height_pix-2, stop_x, stop_y, object_type == ROBOT_OBJECT))
	{
		return ReachedTarget();
	}

	//robots don't attack while moving
	if(object_type == ROBOT_OBJECT)
	{
		Disengage();
	}

	//capture the overflow loss from double to int conversion
	xover = nx - inx;
	yover = ny - iny;

	x = inx;
	y = iny;

	//set centers
	center_x = x + (width_pix >> 1);
	center_y = y + (height_pix >> 1);

	//set new real move speed
	if(!OpenZod::Util::Math::IsZ(dx) || !OpenZod::Util::Math::IsZ(dy))
	{
		double previous_real_speed = real_move_speed;

		real_move_speed = move_speed * tmap.GetTileWalkSpeed(center_x, center_y) * DamagedSpeed() * RunSpeed();

		//update in speed?
		if(real_move_speed != previous_real_speed)
		{
			if(previous_real_speed > 0)
			{
				dx /= previous_real_speed;
				dy /= previous_real_speed;
			}
			else
			{
				spdlog::error("ZObject::ProcessMove - previous_real_speed not above zero! {}", previous_real_speed);
			}
			dx *= real_move_speed;
			dy *= real_move_speed;

			sflags.updated_velocity = true;
		}
	}

	return true;
}

void ZObject::SetVelocity(ZObject *target_object)
{
	float &dx = loc.dx;
	float &dy = loc.dy;
	std::vector<waypoint>::iterator wp;
	float mag;
	float old_dx = loc.dx;
	float old_dy = loc.dy;
	
	//do we have a waypoint?
	if(waypoint_list.size())
	{
		int &cx = center_x;
		int &cy = center_y;

		//cx = x + (width_pix >> 1);
		//cy = y + (height_pix >> 1);

		dx = cur_wp_info.x - cx;
		dy = cur_wp_info.y - cy;
		if(!OpenZod::Util::Math::IsZ(dx) || !OpenZod::Util::Math::IsZ(dy))
		{
			mag = sqrt((dx * dx) + (dy * dy));
			dx /= mag;
			dy /= mag;
			dx *= real_move_speed;
			dy *= real_move_speed;
		}
	}
	else
	{
		//we aint moving if we have no waypoints
		StopMove();
	}

	if(fabs(dx - old_dx) < 0.1)
	{
		dx = old_dx;
	}
	if(fabs(dy - old_dy) < 0.1)
	{
		dy = old_dy;
	}

	//update velocity?
	if(dx != old_dx || dy != old_dy)
	{
		sflags.updated_velocity = true;
	}
}

bool ZObject::DodgeMissile(int tx, int ty, double time_till_explode)
{
	int nx, ny;

	if(!CanOverwriteWP())
	{
		return false;
	}
	if(move_speed <= 0)
	{
		return false;
	}
	if(real_move_speed <= 0)
	{
		return false;
	}
	if(object_type == ROBOT_OBJECT && attack_object)
	{
		return false;
	}
	if(owner == NULL_TEAM)
	{
		return false;
	}

	double dist; 
	if(time_till_explode > stamina)
	{
		dist = time_till_explode * real_move_speed;
	}
	else
	{
		dist = time_till_explode * real_move_speed * zsettings->run_unit_speed;
	}

	int m_move_dist = dist * 2.0 / 4.0;
	int f_move_dist = dist * 4.0 / 4.0;
	if(m_move_dist <= 0)
	{
		m_move_dist = 1;
	}
	int move_dist = f_move_dist + OpenZod::Util::Random::Int(0, m_move_dist - 1);
	double theta = (2 * PI) * OpenZod::Util::Random::Float(0, 1);

	nx = center_x + (move_dist * cos(theta));
	ny = center_y + (move_dist * sin(theta));

	//modify current dodge or make new?
	if(waypoint_list.size() && waypoint_list.begin()->mode == DODGE_WP)
	{
		//altering this should make it
		//recognize as a new waypoint later
		waypoint_list.begin()->x = nx;
		waypoint_list.begin()->y = ny;
	}
	else
	{
		waypoint dodge_waypoint;

		dodge_waypoint.mode = DODGE_WP;
		dodge_waypoint.ref_id = -1;
		dodge_waypoint.x = nx;
		dodge_waypoint.y = ny;

		waypoint_list.insert(waypoint_list.begin(), dodge_waypoint);
	}

	return true;
}

bool ZObject::DoAttackImpassableAtCoords(ZOLists &ols, int x, int y)
{
	//all impasses require explosives to destroy
	if(!HasExplosives())
	{
		return false;
	}

	for(std::vector<ZObject*>::iterator o=ols.object_list->begin(); o!=ols.object_list->end();++o)
	{
		if(!(*o)->IsDestroyableImpass())
		{
			continue;
		}
		if(!(*o)->CausesImpassAtCoord(x, y))
		{
			continue;
		}
		if(!CanAttackObject(*o))
		{
			continue;
		}

		//ok attack it
		waypoint new_waypoint;
		new_waypoint.mode = ATTACK_WP;
		new_waypoint.ref_id = (*o)->GetRefID();
		(*o)->GetCenterCords(new_waypoint.x, new_waypoint.y);

		//push this attack waypoint to the front,
		//and exit this current waypoint
		waypoint_list.insert(waypoint_list.begin(), new_waypoint);

		//a kind of solution for the minions
		CloneMinionWayPoints();

		return true;
	}

	return false;
}

server_flag &ZObject::GetSFlags()
{
	return sflags;
}

void ZObject::CreateLocationData(char *&data, int &size)
{
	size = 4 + sizeof(object_location);
	data = (char*)malloc(size);

	((int*)data)[0] = ref_id;
	memcpy(data+4, &loc, sizeof(object_location));
}

void ZObject::CreateAttackObjectData(char *&data, int &size)
{
	attack_object_packet send_data;

	size = sizeof(attack_object_packet);
	data = (char*)malloc(size);

	send_data.ref_id = ref_id;
	if(attack_object)
	{
		send_data.attack_object_ref_id = attack_object->ref_id;
	}
	else
	{
		send_data.attack_object_ref_id = -1;
	}

	memcpy(data, &send_data, sizeof(attack_object_packet));
}

void ZObject::SetLoc(object_location new_loc)
{
	double &the_time = ztime->ztime;
	int &x = loc.x;
	int &y = loc.y;

	if(new_loc.dx != loc.dx || new_loc.dy != loc.dy)
	{
		loc = new_loc;
		RecalcDirection();
	}
	else
	{
		loc = new_loc;
	}

	//for estimating where it really is
	last_loc = loc;
	last_loc_set_time = the_time;

	//set centers
	center_x = x + (width_pix >> 1);
	center_y = y + (height_pix >> 1);
}

void ZObject::SmoothMove(double &the_time)
{
	int &x = loc.x;
	int &y = loc.y;
	float &dx = loc.dx;
	float &dy = loc.dy;

	//move if it is moving
	if(!OpenZod::Util::Math::IsZ(dx))
	{
		loc.x = last_loc.x + floor(dx * (the_time - last_loc_set_time));
	}
	if(!OpenZod::Util::Math::IsZ(dy))
	{
		loc.y = last_loc.y + floor(dy * (the_time - last_loc_set_time));
	}

	//set centers
	center_x = x + (width_pix >> 1);
	center_y = y + (height_pix >> 1);
}

void ZObject::RecalcDirection()
{
	int new_dir = DirectionFromLoc(loc.dx, loc.dy);

	if(new_dir != -1)
	{
		direction = new_dir;
	}
}

int ZObject::DirectionFromLoc(float dx, float dy)
{
	//are we going anywhere?
	if(OpenZod::Util::Math::IsZ(dx) && OpenZod::Util::Math::IsZ(dy))
	{
		return -1;
	}

	float a = atan2(dy,dx);

	//atan2 is kind of funky
	if(a < 0)
	{
		a += PI + PI;
	}
	a += PI * 1 / 8;

	if(a < PI / 4)
	{
		return 0;
	}
	if(a < PI / 2)
	{
		return 7;
	}
	if(a < PI * 3 / 4)
	{
		return 6;
	}
	if(a < PI)
	{
		return 5;
	}
	if(a < 5 * PI / 4)
	{
		return 4;
	}
	if(a < 6 * PI / 4)
	{
		return 3;
	}
	if(a < 7 * PI / 4)
	{
		return 2;
	}
	if(a < 2 * PI)
	{
		return 1;
	}
	
	return 0;
}

ZObject* ZObject::GetObjectFromID_BS(int ref_id_, std::vector<ZObject*> &the_list)
{
	int low = 0;
	int high = the_list.size() - 1;
	int midpoint = 0;

	while (low <= high)
	{
		int tref_id;

		midpoint = low + ((high - low) >> 1);

		tref_id = the_list[midpoint]->GetRefID();

		if (ref_id_ == tref_id)
		{
			return the_list[midpoint];
		}
		else if (ref_id_ < tref_id)
		{
			high = midpoint - 1;
		}
		else
		{
			low = midpoint + 1;
		}
	}

	return NULL;
}

ZObject* ZObject::GetObjectFromID(int ref_id_, std::vector<ZObject*> &the_list)
{
	return GetObjectFromID_BS(ref_id_, the_list);
}

void ZObject::SetAttackObject(ZObject *obj)
{
	attack_object = obj;
}

ZObject* ZObject::GetAttackObject()
{
	return attack_object;
}

ZObject* ZObject::NearestObjectFromList(std::vector<ZObject*> &the_list)
{
	if(!the_list.size())
	{
		return NULL;
	}

	ZObject* obj_choice = the_list[0];
	double least_distance = DistanceFromObject(*obj_choice);
	for(std::vector<ZObject*>::iterator obj=the_list.begin()++;obj!=the_list.end();obj++)
	{
		double this_distance = DistanceFromObject(**obj);

		if(this_distance < least_distance)
		{
			least_distance = this_distance;
			obj_choice = *obj;
		}
	}

	return obj_choice;
}

void ZObject::RemoveObjectFromList(ZObject* the_object, std::vector<ZObject*> &the_list)
{
	for(std::vector<ZObject*>::iterator i=the_list.begin();i!=the_list.end();)
	{
		if(*i == the_object)
		{
			i = the_list.erase(i);
		}
		else
		{
			i++;
		}
	}
}

ZObject* ZObject::NearestObjectToCoords(std::vector<ZObject*> &the_list, int x, int y)
{

	if(!the_list.size())
	{
		return NULL;
	}

	ZObject* obj_choice = the_list[0];
	double least_distance = obj_choice->DistanceFromCoords(x,y);
	for(std::vector<ZObject*>::iterator i=the_list.begin();i!=the_list.end();i++)
	{
		double this_distance;

		this_distance = (*i)->DistanceFromCoords(x,y);
		if(this_distance < least_distance)
		{
			least_distance = this_distance;
			obj_choice = *i;
		}
	}

	return obj_choice;
}

void ZObject::ClearAndDeleteList(std::vector<ZObject*> &the_list)
{
	for(std::vector<ZObject*>::iterator obj=the_list.begin(); obj!=the_list.end(); ++obj)
	{
		if(*obj)
		{
			delete *obj;
		}
	}

	the_list.clear();
}

void ZObject::ProcessList(std::vector<ZObject*> &the_list)
{
	for(std::vector<ZObject*>::iterator obj=the_list.begin(); obj!=the_list.end(); ++obj)
	{
		if(*obj)
		{
			(*obj)->Process();
		}
	}
}

ZObject* ZObject::NearestSelectableObject(std::vector<ZObject*> &the_list, int unit_type, int only_team, int x, int y)
{
	double least_distance;

	if(!the_list.size())
	{
		return NULL;
	}

	ZObject* obj_choice = NULL;
	
	for(std::vector<ZObject*>::iterator i=the_list.begin();i!=the_list.end();i++)
	{
		unsigned char ot, oid;
		double this_distance;

		if((*i)->GetOwner() != only_team)
		{
			continue;
		}
		if((*i)->IsMinion())
		{
			continue;
		}

		(*i)->GetObjectID(ot, oid);
		if(ot != unit_type)
		{
			continue;
		}

		//this the first found?
		if(!obj_choice)
		{
			obj_choice = *i;
			least_distance = obj_choice->DistanceFromCoords(x,y);
			continue;
		}

		//is this one closer then the previous?
		this_distance = (*i)->DistanceFromCoords(x,y);
		if(this_distance < least_distance)
		{
			obj_choice = *i;
			least_distance = this_distance;
		}
	}

	return obj_choice;
}

ZObject* ZObject::NextSelectableObjectAboveID(std::vector<ZObject*> &the_list, int unit_type, int only_team, int min_ref_id)
{
	for(std::vector<ZObject*>::iterator i=the_list.begin();i!=the_list.end();i++)
	{
		unsigned char ot, oid;

		if((*i)->GetRefID() <= min_ref_id)
		{
			continue;
		}
		if((*i)->GetOwner() != only_team)
		{
			continue;
		}
		if((*i)->IsMinion())
		{
			continue;
		}

		(*i)->GetObjectID(ot, oid);
		if(ot != unit_type)
		{
			continue;
		}
			
		return *i;
	}

	return NULL;
}

void ZObject::RemoveObject(ZObject *obj)
{
	if(attack_object == obj)
	{
		SetAttackObject(NULL);

		//only the server needs this
		//but it doesn't hurt to do it in the client
		SignalLidShouldClose();
	}

	//this is kind of crude but it needs to be done
	for(std::vector<ZObject*>::iterator i=minion_list.begin();i!=minion_list.end();i++)
	{
		if(*i == obj)
		{
			*i = NULL;
		}
	}

	//crude
	if(leader_obj == obj)
	{
		leader_obj = NULL;
	}
}

std::vector<fire_missile_info> ZObject::ServerFireTurrentMissile(int &damage, int &radius)
{
	std::vector<fire_missile_info> ret;

	damage = 0;
	radius = 0;

	return ret;
}

void ZObject::FireTurrentMissile(int x_, int y_, double offset_time)
{

}

ZGuiWindow *ZObject::MakeGuiWindow()
{
	return NULL;
}

void ZObject::ProcessSetBuiltCannonData(char *data, int size)
{

}

void ZObject::ProcessSetBuildingStateData(char *data, int size)
{

}

bool ZObject::GetBuildingCreationPoint(int &x, int &y)
{
	return false;
}

bool ZObject::GetBuildingCreationMovePoint(int &x, int &y)
{
	return false;
}

double ZObject::PercentageProduced(double &the_time)
{
	return 0;
}

double ZObject::ProductionTimeTotal()
{
	return 0;
}

bool ZObject::GetBuildUnit(unsigned char &ot, unsigned char &oid)
{
	return false;
}

bool ZObject::BuildUnit(double &the_time, unsigned char &ot, unsigned char &oid)
{
	return false;
}

void ZObject::ResetProduction()
{

}

void ZObject::SetBuildList(ZBuildList *buildlist_)
{
	buildlist = buildlist_;
}

bool ZObject::StoreBuiltCannon(unsigned char oid)
{
	return false;
}

map_zone_info *ZObject::GetConnectedZone()
{
	return connected_zone;
}

void ZObject::SetConnectedZone(map_zone_info *connected_zone_)
{
	connected_zone = connected_zone_;
}

void ZObject::SetConnectedZone(ZMap &the_map)
{
	connected_zone = the_map.GetZone(loc.x, loc.y);
}

bool ZObject::RemoveStoredCannon(unsigned char oid)
{
	return false;
}

bool ZObject::HaveStoredCannon(unsigned char oid)
{
	return false;
}

int ZObject::CannonsInZone(ZOLists &ols)
{
	int cannons_found = 0;

	for(std::vector<ZObject*>::iterator i=ols.object_list->begin();i!=ols.object_list->end();i++)
	{
		if(this != *i && connected_zone == (*i)->GetConnectedZone())
		{
			unsigned char ot, oid;

			(*i)->GetObjectID(ot, oid);

			if(ot != CANNON_OBJECT)
			{
				continue;
			}

			cannons_found++;
		}
	}

	return cannons_found;
}

std::vector<unsigned char> &ZObject::GetBuiltCannonList()
{
	static std::vector<unsigned char> arg;

	return arg;
}

void ZObject::SetUnitLimitReachedList(bool *unit_limit_reached_)
{
	unit_limit_reached = unit_limit_reached_;
}

void ZObject::SetLevel(int level_)
{

}

int ZObject::GetLevel()
{
	return 0;
}

void ZObject::DoPreRender(ZMap &the_map, SDL_Surface *dest, int shift_x, int shift_y)
{

}

bool ZObject::HasDestroyedFortInZone(ZOLists &ols)
{
	if(!connected_zone)
	{
		return false;
	}

	for(ZObject* i : ols.building_olist)
	{
		if(connected_zone != i->GetConnectedZone())
		{
			continue;
		}
		if(!i->IsDestroyed())
		{
			continue;
		}

		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		if(ot == BUILDING_OBJECT && (oid == FORT_FRONT || oid == FORT_FRONT))
		{
			return true;
		}
	}

	return false;
}

bool ZObject::DoAutoRepair(ZMap &tmap, ZOLists &ols)
{
	double &the_time = ztime->ztime;

	if(!do_auto_repair)
	{
		return false;
	}

	if(the_time < next_auto_repair_time)
	{
		return false;
	}
	
	do_auto_repair = false;

	//nevermind?
	if(HasDestroyedFortInZone(ols))
	{
		return false;
	}
	
	//do the repair
	//health = max_health;
	SetHealth(max_health, tmap);

	//let them know
	return true;
}

void ZObject::StopAutoRepair()
{
	do_auto_repair = false;
}

int ZObject::GetBuildState()
{
	return -1;
}

void ZObject::SetMapImpassables(ZMap &tmap)
{

}

void ZObject::UnSetMapImpassables(ZMap &tmap)
{

}

void ZObject::SetDestroyMapImpassables(ZMap &tmap)
{

}

void ZObject::UnSetDestroyMapImpassables(ZMap &tmap)
{

}

void ZObject::AddGroupMinion(ZObject *obj)
{
	if(!obj)
	{
		return;
	}
	if(obj == this)
	{
		return;
	}

	minion_list.push_back(obj);
}

void ZObject::RemoveGroupMinion(ZObject *obj)
{
	for(std::vector<ZObject*>::iterator i=minion_list.begin();i!=minion_list.end();)
	{
		if(!*i || *i == obj)
		{
			i = minion_list.erase(i);
		}
		else
		{
			i++;
		}
	}
}

std::vector<ZObject*> &ZObject::GetMinionList()
{
	return minion_list;
}

ZObject* ZObject::GetGroupLeader()
{
	return leader_obj;
}

void ZObject::SetGroupLeader(ZObject *obj)
{
	//cant be your own leader
	if(obj == this)
	{
		return;
	}

	leader_obj = obj;
}

void ZObject::ClearGroupInfo()
{
	minion_list.clear();
	leader_obj = NULL;
}

bool ZObject::IsMinion()
{
	return leader_obj;
}

bool ZObject::IsApartOfAGroup()
{
	return leader_obj || minion_list.size();
}

void ZObject::CloneMinionWayPoints()
{
	for(ZObject* i : minion_list)
	{
		if(!i)
		{
			continue;
		}

		i->GetWayPointList() = waypoint_list;
		i->SetVelocity();

		//just left cannon...
		i->SetJustLeftCannon(just_left_cannon);
	}
}

void ZObject::CreateGroupInfoData(char *&data, int &size)
{
	//only robots can create groups so
	if(object_type != ROBOT_OBJECT)
	{
		data = NULL;
		size = 0;
		return;
	}

	//int ref_id;
	int leader_ref_id;
	if(leader_obj)
	{
		leader_ref_id = leader_obj->GetRefID();
	}
	else
	{
		leader_ref_id = -1;
	}

	int minions = minion_list.size();

	size = 12 + (4 * minions);

	data = (char*)malloc(size);
	((int*)data)[0] = ref_id;
	((int*)data)[1] = leader_ref_id;
	((int*)data)[2] = minions;

	for(int i=0;i<minions;i++)
	{
		if(minion_list[i])
		{
			((int*)data)[2+i] = minion_list[i]->GetRefID();
		}
		else
		{
			((int*)data)[2+i] = -1;
		}
	}
}

void ZObject::CreateTeamData(char *&data, int &size)
{
	size = sizeof(object_team_packet) + (driver_info.size() * sizeof(driver_info_s));
	data = (char*)malloc(size);

	object_team_packet packet_header;
	packet_header.ref_id = ref_id;
	packet_header.owner = owner;
	packet_header.driver_type = driver_type;
	packet_header.driver_amount = driver_info.size();

	memcpy(data, &packet_header, sizeof(object_team_packet));

	int shift_amt = sizeof(object_team_packet);
	for(std::vector<driver_info_s>::iterator i=driver_info.begin();i!=driver_info.end();i++)
	{
		memcpy(data + shift_amt, &(*i), sizeof(driver_info_s));
		shift_amt += sizeof(driver_info_s);
	}
}

void ZObject::ProcessGroupInfoData(char *data, int size, std::vector<ZObject*> &object_list)
{
	//meet min requirements?
	if(size < 12)
	{
		return;
	}

	int ref_id_ = ((int*)data)[0];
	int leader_ref_id = ((int*)data)[1];
	int minions = ((int*)data)[2];

	//other requirements?
	if(ref_id_ != ref_id)
	{
		return;
	}
	if(size != 12 + (4 * minions))
	{
		return;
	}

	ClearGroupInfo();

	leader_obj = GetObjectFromID(leader_ref_id, object_list);

	//fill the minion list
	for(int i=0;i<minions;i++)
	{
		ZObject* new_minion = GetObjectFromID(((int*)data)[2+i], object_list);

		if(!new_minion)
		{
			spdlog::warn("ZObject::ProcessGroupInfoData - Could not find a minion");
			continue;
		}

		minion_list.push_back(new_minion);
	}
}

void ZObject::SetupRockRender(bool **rock_list, int map_w, int map_h)
{

}

bool ZObject::CanBeDestroyed()
{
	return can_be_destroyed;
}

void ZObject::CreationMapEffects(ZMap &tmap)
{

}

void ZObject::DeathMapEffects(ZMap &tmap)
{

}

bool ZObject::ResetBuildTime(float zone_ownage)
{
	return false;
}

bool ZObject::RecalcBuildTime()
{
	return false;
}

double ZObject::SpeedOffsetPercent()
{
	if(!move_speed)
	{
		return 1.0;
	}
	if(!IsMoving())
	{
		return 1.0;
	}

	return sqrt((loc.dx*loc.dx)+(loc.dy*loc.dy)) / move_speed;
}

bool ZObject::CanBeRepairedByCrane(int repairers_team)
{
	if(object_type != BUILDING_OBJECT)
	{
		return false;
	}
	if(object_id == FORT_FRONT)
	{
		return false;
	}
	if(object_id == FORT_BACK)
	{
		return false;
	}

	if(owner != NULL_TEAM && repairers_team != owner)
	{
		return false;
	}
	if(!IsDestroyed())
	{
		return false;
	}

	return true;
}

bool ZObject::CanAttack()
{
	return damage && !IsDestroyed();
}

bool ZObject::HasExplosives()
{
	if(has_explosives)
	{
		return true;
	}
	if(GetGrenadeAmount())
	{
		return true;
	}
	if(GetGroupLeader() && GetGroupLeader()->GetGrenadeAmount())
	{
		return true;
	}

	return false;
}

bool ZObject::AttackedOnlyByExplosives()
{
	return attacked_by_explosives;
}

void ZObject::SetDriverType(int driver_type_)
{
	driver_type = driver_type_;

	if(driver_type < 0)
	{
		driver_type = 0;
	}
	if(driver_type >= MAX_ROBOT_TYPES)
	{
		driver_type = MAX_ROBOT_TYPES-1;
	}

	//drivers can mess with damage info
	ResetDamageInfo();
}

void ZObject::AddDriver(int driver_health_)
{
	driver_info_s new_driver;

	new_driver.driver_health = driver_health_;

	AddDriver(new_driver);
}

void ZObject::AddDriver(driver_info_s new_driver)
{
	new_driver.next_attack_time = 0;

	driver_info.push_back(new_driver);

	//drivers can mess with damage info
	ResetDamageInfo();
}

std::vector<driver_info_s> &ZObject::GetDrivers()
{
	return driver_info;
}

void ZObject::ClearDrivers()
{
	driver_info.clear();

	//drivers can mess with damage info
	ResetDamageInfo();
}

int ZObject::GetDriverType()
{
	return driver_type;
}

int ZObject::GetDriverHealth()
{
	if(driver_info.size())
	{
		return driver_info.begin()->driver_health;
	}

	return 0;
}

bool ZObject::CanEjectDrivers()
{
	return false;
}

void ZObject::SetInitialDrivers()
{
	driver_type = GRUNT;
	ClearDrivers();
}

void ZObject::SetEjectableCannon(bool ejectable_)
{

}

void ZObject::ResetDamageInfo()
{

}

unsigned short ZObject::GetExtraLinks()
{
	return 0;
}

void ZObject::DoHitEffect()
{
	do_hit_effect = true;
}

bool ZObject::GetCraneEntrance(int &x, int &y, int &x2, int &y2)
{
	return false;
}

bool ZObject::GetCraneCenter(int &x, int &y)
{
	return false;
}

bool ZObject::GetRepairEntrance(int &x, int &y)
{
	return false;
}

bool ZObject::GetRepairCenter(int &x, int &y)
{
	return false;
}

void ZObject::DoCraneAnim(bool on_, ZObject *rep_obj)
{

}

bool ZObject::CanBeRepaired()
{
	return false;
}

bool ZObject::CanRepairUnit(int units_team)
{
	return false;
}

bool ZObject::RepairingAUnit()
{
	return false;
}

bool ZObject::SetRepairUnit(ZObject *unit_obj)
{
	return false;
}

bool ZObject::GetLidState()
{
	return false;
}

void ZObject::SetLidState(bool lid_open_)
{

}

void ZObject::DoRepairBuildingAnim(bool on_, double remaining_time_)
{

}

bool ZObject::RepairUnit(double &the_time, unsigned char &ot, unsigned char &oid, int &driver_type_, std::vector<driver_info_s> &driver_info_, std::vector<waypoint> &rwaypoint_list)
{
	return false;
}

void ZObject::ProcessServerLid()
{

}

void ZObject::SignalLidShouldOpen()
{

}

void ZObject::SignalLidShouldClose()
{

}

bool ZObject::CanBeSniped()
{
	return can_be_sniped && driver_info.size();
}

bool ZObject::CanSnipe()
{
	return can_snipe;
}

bool ZObject::HasLid()
{
	return has_lid;
}

bool ZObject::HasProcessedDeath()
{
	return processed_death;
}

void ZObject::SetHasProcessedDeath(bool processed_death_)
{
	processed_death = processed_death_;
}

void ZObject::DoDriverHitEffect()
{
	do_driver_hit_effect = true;
}

bool ZObject::CanBeEntered()
{
	if(owner != NULL_TEAM)
	{
		return false;
	}
	if(IsDestroyed())
	{
		return false;
	}
	if(!(object_type == VEHICLE_OBJECT || object_type == CANNON_OBJECT))
	{
		return false;
	}

	return true;
}

void ZObject::SetDamagedByFireTime(double the_time)
{
	last_damaged_by_fire_time = the_time;
}

void ZObject::SetDamagedByMissileTime(double the_time)
{
	last_damaged_by_missile_time = the_time;
}

double ZObject::GetDamagedByFireTime()
{
	return last_damaged_by_fire_time;
}

double ZObject::GetDamagedByMissileTime()
{
	return last_damaged_by_missile_time;
}

bool ZObject::CanEnterFort(int team)
{
	return false;
}

bool ZObject::CanSetWaypoints()
{
	return false;
}

bool ZObject::CanMove()
{
	return move_speed;
}

bool ZObject::CanAttackObject(ZObject *obj)
{
	if(!obj)
	{
		return false;
	}

	if(!CanAttack())
	{
		return false;
	}
	if(obj->IsDestroyed())
	{
		return false;
	}
	if(owner == obj->GetOwner())
	{
		return false;
	}
	if(!HasExplosives() && obj->AttackedOnlyByExplosives())
	{
		return false;
	}

	return true;
}

void ZObject::PostPathFindingResult(ZPath_Finding_Response* response)
{
	if(!response)
	{
		return;
	}

	if(response->thread_id != cur_wp_info.path_finding_id)
	{
		return;
	}

	cur_wp_info.got_pf_response = true;
	cur_wp_info.pf_point_list = response->pf_point_list;
	cur_wp_info.path_finding_id = 0;

	if(cur_wp_info.pf_point_list.size())
	{
		SetTarget(cur_wp_info.pf_point_list.begin()->x, cur_wp_info.pf_point_list.begin()->y);
		SetVelocity();
		cur_wp_info.pf_point_list.erase(cur_wp_info.pf_point_list.begin());
	}
}

bool sort_objects_func (ZObject *a, ZObject *b)
{
	return ((a->loc.y + a->height_pix) < (b->loc.y + b->height_pix));
}

