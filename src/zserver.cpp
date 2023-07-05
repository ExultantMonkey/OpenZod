#include "zserver.h"

#include "Util/Random.h"

#include <spdlog/spdlog.h>

ZServer::ZServer() : ZCore()
{
	//setup randomizer
	srand(time(0));
	
	current_map_i = -1;
	next_ref_id = 0;
	game_on = false;
	next_end_game_check_time = 0;
	next_reset_game_time = 0;
	do_reset_game = false;
	next_scuffle_time = 0;
	next_make_suggestions_time = 0;

	//bots
	for(int i=0;i<MAX_TEAM_TYPES;i++)
	{
		init_bot[i] = false;
		bot_thread[i] = NULL;
	}

	SetupEHandler();
	
	server_socket.SetEventHandler(&ehandler);
	server_socket.SetEventList(&ehandler.GetEventList());

	ZObject::SetDamageMissileList(&damage_missile_list);
}

ZServer::~ZServer()
{
	KillAllBots();
}

void ZServer::Setup()
{
	//randomizer
	SetupRandomizer();

	//registered?
	CheckRegistration();

	//load base settings
	if(settings_filename.length())
	{
		zsettings.LoadSettings(settings_filename);
	}
	else
	{
		//if we can't load the default filename, then make it
		if(!zsettings.LoadSettings("default_settings.txt"))
		{
			zsettings.SaveSettings("default_settings.txt");
		}
	}

	//load the map list
	if(map_list_name.size() && !ReadMapList())
	{
		spdlog::warn("ZServer::Setup - Could not load map list {}", map_list_name);
	}

	//init main server settings (if there are any)
	InitPerpetualServerSettings();

	//use selectable map list as map list?
	if(!map_name.size() && !map_list.size())
	{
		load_maps_randomly = false;
		map_list = selectable_map_list;

		spdlog::warn("ZServer::Setup - Using selectable map list as the map list");
	}


	//needed for pathfinding
	ZMap::ServerInit();

	LoadNextMap();

	//start listen socket
	if(!server_socket.Start())
	{
		spdlog::error("ZServer::Setup - Socket not setup");
	}

	//init bots
	InitBots();
}

void ZServer::InitBot(int bot_team, bool do_init)
{
	if((bot_team<0) || (bot_team>=MAX_TEAM_TYPES) || (bot_team==NULL_TEAM))
	{
		spdlog::error("ZServer::InitBot - Invalid Team {}", bot_team);
		return;
	}

	init_bot[bot_team] = do_init;
}

void ZServer::InitBots()
{
	for(int i=0;i<MAX_TEAM_TYPES;i++)
	{
		bot[i].SetDesiredTeam((team_type)i);
		bot[i].SetRemoteAddress("localhost");
		bot[i].SetBotBypassData(bot_bypass_data, bot_bypass_size);

		if(init_bot[i])
		{
			StartBot(i);
		}
	}
}

int StartBotThreadFunc(void *vbot_ptr)
{
	ZBot *zbot_ptr = (ZBot *)vbot_ptr;

	//incase we set this previously to close it
	zbot_ptr->AllowRun();

	zbot_ptr->Setup();
	zbot_ptr->Run();

	return 0;
}

void ZServer::StartBot(int bot_team)
{
	if((bot_team<0) || (bot_team>=MAX_TEAM_TYPES) || (bot_team==NULL_TEAM))
	{
		spdlog::error("ZServer::StartBot - Invalid Team {}", bot_team);
		return;
	}

	if(bot_thread[bot_team])
	{
		spdlog::error("ZServer::StartBot - {} Team Bot already started", team_type_string[bot_team]);
		return;
	}

	bot_thread[bot_team] = SDL_CreateThread(StartBotThreadFunc, (void*)&bot[bot_team]);

	spdlog::info("{} Team Bot started", team_type_string[bot_team]);
}

void ZServer::KillBot(int bot_team)
{
	if((bot_team<0) || (bot_team>=MAX_TEAM_TYPES) ||  (bot_team==NULL_TEAM))
	{
		spdlog::error("ZServer::KillBot - Invalid Team {}", bot_team);
	}

	if(!bot_thread[bot_team])
	{
		return;
	}

	bot[bot_team].AllowRun(false);

	int wait_status;
	SDL_WaitThread(bot_thread[bot_team], &wait_status);
	bot_thread[bot_team] = NULL;

	spdlog::info("{} Team Bot stopped", team_type_string[bot_team]);
}

void ZServer::KillAllBots()
{
	for(int i=0;i<MAX_TEAM_TYPES;i++)
	{
		if(bot_thread[i])
		{
			KillBot(i);
		}
	}
}

void ZServer::InitPerpetualServerSettings()
{
	if(p_settings_filename.length())
	{
		if(!psettings.LoadSettings(p_settings_filename))
		{
			psettings.SaveSettings(p_settings_filename);
		}
	}

	if(psettings.use_database && psettings.use_mysql)
	{
		std::string err_msg;

		if(!zmysql.LoadDetails(err_msg, psettings))
		{
			spdlog::error("ZServer::InitPerpetualServerSettings - MySQL Error - {}", err_msg);
		}
		else
		{
			if(!zmysql.Connect(err_msg))
			{
				spdlog::error("ZServer::InitPerpetualServerSettings - MySQL Error - {}", err_msg);
			}
			else
			{
				//make the initial users table if needed
				if(!zmysql.CreateUserTable(err_msg))
				{
					spdlog::error("ZServer::InitPerpetualServerSettings - MySQL Error - {}", err_msg);
				}

				if(!zmysql.CreateOnlineHistoryTable(err_msg))
				{
					spdlog::error("ZServer::InitPerpetualServerSettings - MySQL Error - {}", err_msg);
				}

				if(!zmysql.CreateAffiliateTable(err_msg))
				{
					spdlog::error("ZServer::InitPerpetualServerSettings - MySQL Error - {}", err_msg);
				}
			}
		}
	}

	InitOnlineHistory();
	InitSelectableMapList();
}

void ZServer::InitSelectableMapList()
{
	//if we can't read in the official list
	//and we can't make one
	//then just use the regular map list
	if(!ReadSelectableMapList() && !ReadSelectableMapListFromFolder())
	{
		selectable_map_list = map_list;
	}
}

void ZServer::ProcessEndGame()
{
	//needed for end animations
	RelayTeamsWonEnded();

	//update the dbs
	UpdateUsersWinLoses();

	game_on = false;

	//set reset time?
	if(map_list.size())
	{
		do_reset_game = true;
		next_reset_game_time = COMMON::current_time() + 10.0;
	}

	//give clients the game over message
	BroadCastNews("The game has ended");

	//tell clients game over
	server_socket.SendMessageAll(END_GAME, NULL, 0);
}

void ZServer::DoResetGame(std::string map_name)
{
	//load next map
	LoadNextMap(map_name);

	//tell everyone the game has reset
	BroadCastNews("A new game has started");

	//tell them to reset
	server_socket.SendMessageAll(RESET_GAME, NULL, 0);
}

void ZServer::CheckResetGame()
{
	if(game_on || !map_list.size() || (COMMON::current_time() < next_reset_game_time))
	{
		return;	
	}

	DoResetGame();
}

void ZServer::CheckEndGame()
{
	if(!game_on)
	{
		return;
	}

	double &the_time = ztime.ztime;
	if(the_time < next_end_game_check_time)
	{
		return;
	}

	next_end_game_check_time = the_time + 1.0;

	if(EndGameRequirementsMet())
	{
		ProcessEndGame();
	}
}

bool ZServer::EndGameRequirementsMet()
{
	bool team_available[MAX_TEAM_TYPES];

	for(ZObject* obj : ols.passive_engagable_olist)
	{
		unsigned char ot, oid;
		obj->GetObjectID(ot, oid);

		if(!(ot == ROBOT_OBJECT || ot == VEHICLE_OBJECT || ot == CANNON_OBJECT))
		{
			continue;
		}

		team_available[obj->GetOwner()] = true;
	}

	//how many teams exist?
	int teams_available = 0;

	for(int i=1;i<MAX_TEAM_TYPES;i++)
	{
		teams_available += team_available[i];
	}

	return (teams_available <= 1);
}

void ZServer::ResetGame()
{
	zmap.ClearMap();
	damage_missile_list.clear();
	ols.DeleteAllObjects();
}

void ZServer::LoadNextMap(std::string override_map_name)
{
	//clear stuff
	ResetGame();

	//load map list
	if(override_map_name.size())
	{
		map_name = override_map_name;
	}
	else if(map_list.size())
	{
		NextInMapList();
		map_name = map_list[current_map_i];
	}

	//load map
	if(!map_name.size())
	{
		spdlog::error("ZServer::LoadNextMap - No map set to load");
	}
	else
	{
		if(!zmap.Read(map_name.c_str()))
		{
			spdlog::error("ZServer::LoadNextMap - Could not load map {}", map_name);
		}
	}
			
	InitObjects();
	InitZones();

	if(psettings.start_map_paused)
	{
		PauseGame();
	}

	game_on = true;
}

int ZServer::NextInMapList()
{
	if(!map_list.size())
	{
		spdlog::error("ZServer::NextInMapList - No maps in the the map list");
		return -1;
	}

	if(load_maps_randomly)
	{
		current_map_i = OpenZod::Util::Random::Int(0, map_list.size() - 1);
	}
	else
	{
		current_map_i++;
		if(current_map_i >= map_list.size())
		{
			current_map_i = 0;
		}
	}

	if(current_map_i != -1)
	{
		spdlog::info("ZServer::NextInMapList - returning ({}) {}", current_map_i, map_list[current_map_i]);
	}

	return current_map_i;
}

bool ZServer::ReadMapList()
{
	char cur_line[500];
	bool loaded = false;

	map_list.clear();

	FILE* fp = fopen(map_list_name.c_str(), "r");
	if(!fp) 
	{
		spdlog::error("ZServer::ReadMapList - Could not load {}", map_list_name);
		return false;
	}

	//random?
	if(fgets(cur_line , 500 , fp))
	{
		COMMON::clean_newline(cur_line, 500);
		load_maps_randomly = atoi(cur_line);
	}

	//all the maps
	while(fgets(cur_line , 500 , fp))
	{
		COMMON::clean_newline(cur_line, 500);
		if(strlen(cur_line))
		{
			map_list.push_back(cur_line);
			loaded = true;
		}
	}

	fclose(fp);

	return loaded;
}

bool ZServer::ReadSelectableMapList()
{
	char cur_line[500];
	bool loaded = false;

	//is there even a file set?
	if(!psettings.selectable_map_list.length())
	{
		return false;
	}

	selectable_map_list.clear();

	FILE* fp = fopen(psettings.selectable_map_list.c_str(), "r");
	if(!fp) 
	{
		spdlog::error("ZServer::ReadSelectableMapList - Could not load {}", psettings.selectable_map_list);
		return false;
	}

	//load the maps
	while(fgets(cur_line , 500 , fp))
	{
		COMMON::clean_newline(cur_line, 500);
		if(strlen(cur_line))
		{
			selectable_map_list.push_back(cur_line);
			loaded = true;
		}
	}

	fclose(fp);

	return loaded;
}

bool ZServer::ReadSelectableMapListFromFolder(std::string foldername)
{
	std::vector<std::string> mlist = COMMON::directory_filelist(foldername);
	//find only .maps
	COMMON::parse_filelist(mlist, ".map");
	//sort list
	sort(mlist.begin(), mlist.end(), COMMON::sort_string_func);

	if(!mlist.size())
	{
		spdlog::error("ZServer::ReadSelectableMapListFromFolder - No *.map files found in {}", foldername);
		return false;
	}

	selectable_map_list = mlist;
	return true;
}

void ZServer::InitZones()
{
	//this function sets all the buildings / zones to their correct initial owners
	//and links them all accordingly
	for(ZObject* i : object_list)
	{
		unsigned char iot, ioid;
		int x, y;
		i->GetObjectID(iot, ioid);
		i->GetCords(x, y);

		map_zone_info *the_zone;

		//is this a fort, then just set the zone to the forts owner
		if(iot == BUILDING_OBJECT && (ioid == FORT_FRONT || ioid == FORT_BACK))
		{
			the_zone = zmap.GetZone(x,y);

			if(the_zone) 
			{
				the_zone->owner = i->GetOwner();

				//init any other buildings in this zone
				for(ZObject* j : object_list)
				{
					unsigned char ot, oid;
					int ox, oy;

					j->GetObjectID(ot, oid);
					j->GetCords(ox, oy);

					if(ot == BUILDING_OBJECT)
					{
						if(the_zone == zmap.GetZone(ox,oy))
						{
							j->SetOwner(i->GetOwner());
						}
					}
				}
			}
		}

		//is this a flag, then let it know its zone
		else if(iot == MAP_ITEM_OBJECT && ioid == FLAG_ITEM)
		{
			the_zone = zmap.GetZone(x,y);

			if(the_zone)
			{
				((OFlag*)i)->SetZone(the_zone, zmap, object_list);
			}
		}
	}

	//now that all the buildings have their correct initial teams, set default productions
	for(ZObject* i : object_list)
	{
		i->SetBuildingDefaultProduction();
	}

	//do this...
	ResetZoneOwnagePercentages();
}

ZObject *ZServer::CreateRobotGroup(unsigned char oid, int x, int y, int owner, std::vector<ZObject*> *obj_list, int robot_amount, int health_percent)
{
	//simple check
	if(oid >= MAX_ROBOT_TYPES)
	{
		spdlog::error("ZServer::CreateRobotGroup - Bad Object ID {}", oid);
		return NULL;
	}

	if(robot_amount == 0)
	{
		return NULL;
	}

	if(robot_amount < 0)
	{
		robot_amount = zsettings.GetUnitSettings(ROBOT_OBJECT, oid).groupAmount;
	}

	//create leader
	ZObject* leader_obj = CreateObject(ROBOT_OBJECT, oid, x, y, owner, obj_list, 0, 0, health_percent);
	if(!leader_obj)
	{
		return NULL;
	}

	robot_amount--;

	//create minions
	for(int i=0;i<robot_amount;i++)
	{
		ZObject* minion_obj = CreateObject(ROBOT_OBJECT, oid, x, y, owner, obj_list, 0, 0, health_percent);
		
		if(minion_obj)
		{
			leader_obj->AddGroupMinion(minion_obj);
			minion_obj->SetGroupLeader(leader_obj);
		}
	}

	return leader_obj;
}

ZObject *ZServer::CreateObject(unsigned char ot, unsigned char oid, int x, int y, int owner, std::vector<ZObject*> *obj_list, int blevel, unsigned short extra_links, int health_percent)
{
	//checks
	if(!CreateObjectOk(ot, oid, x, y, owner, blevel, extra_links))
	{
		return NULL;
	}

	ZObject *new_object_ptr = NULL;
   	
   	switch(ot)
   	{
	case BUILDING_OBJECT:
		switch(oid)
		{
		case FORT_FRONT:
			new_object_ptr = new BFort(&ztime, &zsettings, (planet_type)zmap.GetMapBasics().terrain_type, true);
			break;
		case FORT_BACK:
			new_object_ptr = new BFort(&ztime, &zsettings, (planet_type)zmap.GetMapBasics().terrain_type, false);
			break;
		case RADAR:
			new_object_ptr = new BRadar(&ztime, &zsettings, (planet_type)zmap.GetMapBasics().terrain_type);
			break;
		case REPAIR:
			new_object_ptr = new BRepair(&ztime, &zsettings, (planet_type)zmap.GetMapBasics().terrain_type);
			break;
		case ROBOT_FACTORY:
			new_object_ptr = new BRobot(&ztime, &zsettings, (planet_type)zmap.GetMapBasics().terrain_type);
			break;
		case VEHICLE_FACTORY:
			new_object_ptr = new BVehicle(&ztime, &zsettings, (planet_type)zmap.GetMapBasics().terrain_type);
			break;
		case BRIDGE_VERT:
			new_object_ptr = new BBridge(&ztime, &zsettings, (planet_type)zmap.GetMapBasics().terrain_type, true, extra_links);
			break;
		case BRIDGE_HORZ:
			new_object_ptr = new BBridge(&ztime, &zsettings, (planet_type)zmap.GetMapBasics().terrain_type, false, extra_links);
			break;
		}
		break;
	case CANNON_OBJECT:
		switch(oid)
		{
		case GATLING:
			new_object_ptr = new CGatling(&ztime, &zsettings, false);
			break;
		case GUN:
			new_object_ptr = new CGun(&ztime, &zsettings, false);
			break;
		case HOWITZER:
			new_object_ptr = new CHowitzer(&ztime, &zsettings, false);
			break;
		case MISSILE_CANNON:
			new_object_ptr = new CMissileCannon(&ztime, &zsettings, false);
			break;
		}
		break;
	case VEHICLE_OBJECT:
		switch(oid)
		{
		case JEEP:
			new_object_ptr = new VJeep(&ztime, &zsettings);
			break;
		case LIGHT:
			new_object_ptr = new VLight(&ztime, &zsettings);
			break;
		case MEDIUM:
			new_object_ptr = new VMedium(&ztime, &zsettings);
			break;
		case HEAVY:
			new_object_ptr = new VHeavy(&ztime, &zsettings);
			break;
		case APC:
			new_object_ptr = new VAPC(&ztime, &zsettings);
			break;
		case MISSILE_LAUNCHER:
			new_object_ptr = new VMissileLauncher(&ztime, &zsettings);
			break;
		case CRANE:
			new_object_ptr = new VCrane(&ztime, &zsettings);
			break;
		}
		
		//for fun
		if(new_object_ptr)
		{
			new_object_ptr->SetDirection(OpenZod::Util::Random::Int(0, MAX_ANGLE_TYPES - 1));
		}
		break;
	case ROBOT_OBJECT:
		switch(oid)
		{
		case GRUNT:
			new_object_ptr = new RGrunt(&ztime, &zsettings);
			break;
		case PSYCHO:
			new_object_ptr = new RPsycho(&ztime, &zsettings);
			break;
		case SNIPER:
			new_object_ptr = new RSniper(&ztime, &zsettings);
			break;
		case TOUGH:
			new_object_ptr = new RTough(&ztime, &zsettings);
			break;
		case PYRO:
			new_object_ptr = new RPyro(&ztime, &zsettings);
			break;
		case LASER:
			new_object_ptr = new RLaser(&ztime, &zsettings);
			break;
		}
		break;
	case MAP_ITEM_OBJECT:
		switch(oid)
		{
		case FLAG_ITEM:
			new_object_ptr = new OFlag(&ztime, &zsettings);
			break;
		case ROCK_ITEM:
			new_object_ptr = new ORock(&ztime, &zsettings, (planet_type)zmap.GetMapBasics().terrain_type);
			break;
		case GRENADES_ITEM:
			new_object_ptr = new OGrenades(&ztime, &zsettings);
			break;
		case ROCKETS_ITEM:
			new_object_ptr = new ORockets(&ztime, &zsettings);
			break;
		case HUT_ITEM:
			new_object_ptr = new OHut(&ztime, &zsettings, (planet_type)zmap.GetMapBasics().terrain_type);
			break;
		}

		if(oid >= MAP0_ITEM && oid <= MAP21_ITEM)
			new_object_ptr = new OMapObject(&ztime, &zsettings, oid);

		break;
   	}
   	
   	if(new_object_ptr) 
   	{
		new_object_ptr->SetMap(&zmap);
   		new_object_ptr->SetOwner((team_type)owner);
   		new_object_ptr->SetCords(x,y);
		new_object_ptr->SetRefID(next_ref_id++);
		new_object_ptr->SetBuildList(&buildlist);
		new_object_ptr->SetConnectedZone(zmap);
		new_object_ptr->SetUnitLimitReachedList(unit_limit_reached);
		new_object_ptr->SetLevel(blevel);
		new_object_ptr->SetMapImpassables(zmap);
		new_object_ptr->SetInitialDrivers();
		new_object_ptr->SetHealthPercent(health_percent, zmap);
		new_object_ptr->InitRealMoveSpeed(zmap);
   
		if(obj_list)
		{
   			obj_list->push_back(new_object_ptr);
		}
		else
		{
			ols.AddObject(new_object_ptr);
		}
   		
   	}

	//should work here
	ProcessChangeObjectAmount();

	return new_object_ptr;
}

void ZServer::InitObjects()
{
	for(map_object i : zmap.GetObjectList())
	{
		if(!CheckMapObject(i))
		{
			continue;
		}

		if(i.object_type == ROBOT_OBJECT)
		{
			CreateRobotGroup(i.object_id, i.x*16, i.y*16, i.owner, NULL, -1, i.health_percent);
		}
		else
		{
			CreateObject(i.object_type, i.object_id, i.x*16, i.y*16, i.owner, NULL, i.blevel, i.extra_links, i.health_percent);
		}
	}

	//make sure you can not eject fort turret cannons
	MakeAllFortTurretsUnEjectable();
}

bool ZServer::CheckMapObject(map_object &m_obj)
{
	if(m_obj.object_id < 0)
	{
		spdlog::error("ZServer::CheckMapObject - Unused Object ID {}", m_obj.object_id);
		return false;
	}

	switch(m_obj.object_type)
	{
	case BUILDING_OBJECT:
		if(m_obj.object_id >= MAX_BUILDING_TYPES)
		{
			spdlog::error("ZServer::CheckMapObject - Bad Object ID {}", m_obj.object_id);
			return false;
		}
		break;
	case CANNON_OBJECT:
		if(m_obj.object_id >= MAX_CANNON_TYPES)
		{
			spdlog::error("ZServer::CheckMapObject - Bad Object ID {}", m_obj.object_id);
			return false;
		}
		break;
	case VEHICLE_OBJECT:
		if(m_obj.object_id >= MAX_VEHICLE_TYPES)
		{
			spdlog::error("ZServer::CheckMapObject - Bad Object ID {}", m_obj.object_id);
			return false;
		}
		break;
	case ROBOT_OBJECT:
		if(m_obj.object_id >= MAX_ROBOT_TYPES)
		{
			spdlog::error("ZServer::CheckMapObject - Bad Object ID {}", m_obj.object_id);
			return false;
		}
		break;
	case MAP_ITEM_OBJECT:
		if(m_obj.object_id >= MAX_ITEM_TYPES)
		{
			spdlog::error("ZServer::CheckMapObject - Bad Object ID {}", m_obj.object_id);
			return false;
		}
		break;
	default:
		spdlog::error("ZServer::CheckMapObject - Bad Object Type {}", m_obj.object_type);
		return false;
	}

	if(m_obj.owner < 0 || m_obj.owner >= MAX_TEAM_TYPES)
	{
		spdlog::error("ZServer::CheckMapObject - Bad Team {}", m_obj.owner);
		return false;
	}

	if(m_obj.x < 0 || m_obj.y < 0)
	{
		spdlog::error("ZServer::CheckMapObject - Bad Coordinates {} {}", m_obj.x, m_obj.y);
		return false;
	}

	if(m_obj.object_type == BUILDING_OBJECT && (m_obj.blevel < 0 || m_obj.blevel >= MAX_BUILDING_LEVELS))
	{
		spdlog::error("ZServer::CheckMapObject - Bad Building Level {}", m_obj.blevel);
		return false;
	}

	return true;
}

void ZServer::Run()
{
	while(allow_run)
	{
		ztime.UpdateTime();

		//check for tcp events
		server_socket.Process();
		
		//process events
		ehandler.ProcessEvents();

		//suggest something?
		CheckPlayerSuggestions();

		//vote expired?
		CheckVoteExpired();

		//scuffle
		ScuffleUnits();
		
		//run stuff
		ProcessObjects();

		//process missiles
		ProcessMissiles();

		//check for endgame
		CheckEndGame();
		CheckResetGame();

		//insert history
		CheckUpdateOnlineHistory();
		
		//pause
		COMMON::uni_pause(10);
	}
}

void ZServer::ScuffleUnits()
{
	double &the_time = ztime.ztime;

	if(the_time < next_scuffle_time)
	{
		return;
	}

	next_scuffle_time = the_time + 1.0;

	for(ZObject* i : ols.mobile_olist)
	{
		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		if(!(ot == VEHICLE_OBJECT || ot == ROBOT_OBJECT))
		{
			continue;
		}
		if(i->GetOwner() == NULL_TEAM)
		{
			continue;
		}
		if(i->GetWayPointList().size())
		{
			continue;
		}

		int x, y;
		int w, h;
		i->GetCenterCords(x, y);
		i->GetDimensionsPixel(w, h);

		//ok does it overlap any other moveable units?
		for(ZObject* j : ols.mobile_olist)
		{
			if(j != i && i->GetOwner() == j->GetOwner())
			{
				int shift;
				unsigned char jot, joid;
				j->GetObjectID(jot, joid);

				if(!(jot == VEHICLE_OBJECT || jot == ROBOT_OBJECT))
				{
					continue;
				}
				if(j->GetOwner() == NULL_TEAM)
				{
					continue;
				}
				if(j->GetWayPointList().size())
				{
					continue;
				}

				int jx, jy;
				int jw, jh;
				j->GetCenterCords(jx, jy);
				j->GetDimensionsPixel(jw, jh);

				//are their centers roughly within 2 pixels away?
				if(abs(x - jx) >= 6)
				{
					continue;
				}
				if(abs(y - jy) >= 6)
				{
					continue;
				}

				//scuffle..
				waypoint init_movepoint;
				init_movepoint.mode = MOVE_WP;
				init_movepoint.ref_id = -1;

				shift = OpenZod::Util::Random::Int(0, ((jw >> 1) + (w >> 1)) - 1);
				if(OpenZod::Util::Random::Bool())
				{
					init_movepoint.x = x - shift;
				}
				else
				{
					init_movepoint.x = x + shift;
				}

				shift = OpenZod::Util::Random::Int(0, ((jh >> 1) + (h >> 1)));
				if(OpenZod::Util::Random::Bool())
				{
					init_movepoint.y = y - shift;
				}
				else
				{
					init_movepoint.y = y + shift;
				}

				i->GetWayPointList().push_back(init_movepoint);

				break;
			}
		}
	}
}

void ZServer::ProcessMissiles()
{
	double &the_time = ztime.ztime;

	for(std::vector<damage_missile>::iterator i=damage_missile_list.begin(); i!=damage_missile_list.end();)
	{
		if(the_time >= i->explode_time)
		{
			ProcessMissileDamage(*i);
			i = damage_missile_list.erase(i);
		}
		else
		{
			i++;
		}
	}

	//the objects push new objects into this queue, so we need to move it to the real one
	//also do CheckUnitLimitReached if required
	for(damage_missile i : new_damage_missile_list) {
		damage_missile_list.push_back(i);
	}
	new_damage_missile_list.clear();
}

void ZServer::ProcessMissileDamage(damage_missile &the_missile)
{
	double &the_time = ztime.ztime;
	int &x = the_missile.x;
	int &y = the_missile.y;
	int &radius = the_missile.radius;
	int &team = the_missile.team;
	int &damage = the_missile.damage;

	for(ZObject* obj : object_list)
	{
		if(obj->IsDestroyed()) {
			continue;
		}

		if((team == NULL_TEAM) || (obj->GetOwner() != team))
		{
			int tx, ty;
			obj->GetCenterCords(tx, ty);

			int dx = x - tx;
			int dy = y - ty;

			//quick check
			if(abs(dx) > radius) 
			{
				continue;
			}
			if(abs(dy) > radius)
			{
				continue;
			}
			
			float mag = sqrt((float)((dx * dx) + (dy * dy)));

			//slow check
			if(mag >= radius)
			{
				continue;
			}

			obj->DamageHealth(damage * (1.0 - (mag / radius)), zmap);
			obj->SetDamagedByMissileTime(the_time);

			if(the_missile.attacker_ref_id != -1 && 
				the_missile.attack_player_given && 
				the_missile.target_ref_id == obj->GetRefID() &&
				obj->IsDestroyed())
			{
				RelayPortraitAnim(the_missile.attacker_ref_id, TARGET_DESTROYED_ANIM);
			}

			UpdateObjectHealth(obj, the_missile.attacker_ref_id);
		}
	}
}

void ZServer::ProcessPathFindingResults()
{
	zmap.GetPathFinder().Lock_List();

	for(ZPath_Finding_Response* pf : zmap.GetPathFinder().GetList())
	{
		ZObject* obj = GetObjectFromID(pf->obj_ref_id, object_list);
		if(obj)
		{
			obj->PostPathFindingResult(pf);
		}
	}

	zmap.GetPathFinder().Clear_Response_List();

	zmap.GetPathFinder().Unlock_List();
}

void ZServer::ProcessObjects()
{
	double &the_time = ztime.ztime;

	//kill all those that need to die
	for(ZObject* obj : object_list)
	{
		if(obj->KillMe(the_time)) {
			DeleteObject(obj);
		}
	}

	//process all the objects
	for(ZObject* obj : object_list)  {
		obj->ProcessServer(zmap, ols);
	}

	//see if any pathfinding responses have come back
	//and process them
	ProcessPathFindingResults();

	//check to see what updated and needs to be sent to the clients
	for(ZObject* obj : object_list)
	{
		ZObject *tobj;

		//this case can cut us early
		//because it will delete this object entirely
		if(obj->GetSFlags().entered_target_ref_id != -1)
		{
			tobj = GetObjectFromID(obj->GetSFlags().entered_target_ref_id, object_list);
			RobotEnterObject(obj, tobj);
		}

		//might want to revive it to the client first
		if(obj->GetSFlags().auto_repair)
		{
			obj->SetHasProcessedDeath(false);
			UpdateObjectHealth(obj);
		}

		if(obj->GetSFlags().updated_velocity)
		{
			RelayObjectLoc(obj);
		}

		if(obj->GetSFlags().updated_waypoints)
		{
			RelayObjectWayPoints(obj);

			//also fix minion waypoints
			obj->CloneMinionWayPoints();
		}

		if(obj->GetSFlags().updated_attack_object)
		{
			RelayObjectAttackObject(obj);
		}

		if(obj->GetSFlags().updated_attack_object_health && obj->GetAttackObject())
		{
			UpdateObjectHealth(obj->GetAttackObject(), obj->GetRefID());
		}

		if(obj->GetSFlags().updated_attack_object_driver_health && obj->GetAttackObject())
		{
			UpdateObjectDriverHealth(obj->GetAttackObject());
		}

		if(obj->GetSFlags().destroy_fort_building_ref_id != -1)
		{
			tobj = GetObjectFromID(obj->GetSFlags().destroy_fort_building_ref_id, object_list);
			if(tobj)
			{
				tobj->SetHealth(0, zmap);
				UpdateObjectHealth(tobj);
			}
		}

		if(obj->GetSFlags().fired_missile)
		{
			fire_missile_packet the_data;

			the_data.ref_id = obj->GetRefID();
			the_data.x = obj->GetSFlags().missile_x;
			the_data.y = obj->GetSFlags().missile_y;

			server_socket.SendMessageAll(FIRE_MISSILE, (const char*)&the_data, sizeof(fire_missile_packet));
		}

		if(obj->GetSFlags().build_unit)
		{
			ZObject *new_obj;
			new_obj = BuildingCreateUnit(obj, obj->GetSFlags().bot, obj->GetSFlags().boid);
			obj->ResetProduction();
			RelayBuildingState(obj);
			RelayObjectBuildingQueue(obj);
			RelayObjectManufacturedSound(obj, new_obj);

			//append rally points
			if(new_obj)
			{
				for(waypoint i : obj->GetRallyPointList()) {
					new_obj->GetWayPointList().push_back(i);
				}
			}
		}

		if(obj->GetSFlags().repair_unit)
		{
			BuildingRepairUnit(obj, obj->GetSFlags().rot, obj->GetSFlags().roid, obj->GetSFlags().rdriver_type, obj->GetSFlags().rdriver_info, obj->GetSFlags().rwaypoint_list);
			RelayBuildingState(obj);
		}

		if(obj->GetSFlags().set_crane_anim)
		{
			crane_anim_packet the_data;
			the_data.ref_id = obj->GetRefID();
			the_data.on = obj->GetSFlags().crane_anim_on;
			the_data.rep_ref_id = obj->GetSFlags().crane_rep_ref_id;
			server_socket.SendMessageAll(DO_CRANE_ANIM, (const char*)&the_data, sizeof(crane_anim_packet));
		}

		if(obj->GetSFlags().entered_repair_building_ref_id != -1)
		{
			tobj = GetObjectFromID(obj->GetSFlags().entered_repair_building_ref_id, object_list);
			UnitEnterRepairBuilding(obj, tobj);
		}

		if(obj->GetSFlags().updated_open_lid)
		{
			set_lid_state_packet the_data;
			the_data.ref_id = obj->GetRefID();
			the_data.lid_open = obj->GetLidState();
			server_socket.SendMessageAll(SET_LID_OPEN, (const char*)&the_data, sizeof(set_lid_state_packet));
		}

		if(obj->GetSFlags().updated_grenade_amount)
		{
			RelayObjectGrenadeAmount(obj);
		}

		if(obj->GetSFlags().do_pickup_grenade_anim)
		{
			int_packet the_data;
			the_data.ref_id = obj->GetRefID();
			server_socket.SendMessageAll(PICKUP_GRENADE_ANIM, (const char*)&the_data, sizeof(int_packet));
		}

		if(obj->GetSFlags().updated_leader_grenade_amount && obj->GetGroupLeader())
		{
			RelayObjectGrenadeAmount(obj->GetGroupLeader());
		}

		if(obj->GetSFlags().delete_grenade_box_ref_id != -1)
		{
			tobj = GetObjectFromID(obj->GetSFlags().delete_grenade_box_ref_id, object_list);
			if(tobj)
			{
				tobj->SetHealth(0, zmap);
				UpdateObjectHealth(tobj);
			}
		}

		if(obj->GetSFlags().portrait_anim_ref_id != -1)
		{
			RelayPortraitAnim(obj->GetSFlags().portrait_anim_ref_id, obj->GetSFlags().portrait_anim_value);
		}
	}

	//the objects push new objects into this queue, so we need to move it to the real one
	//also do CheckUnitLimitReached if required
	for(ZObject* i : new_object_list) {
		ols.AddObject(i);
	}

	if(new_object_list.size())
	{
		CheckUnitLimitReached();
	}
	new_object_list.clear();

	//check for flag captures
	CheckFlagCaptures();
}

void ZServer::UnitEnterRepairBuilding(ZObject *unit_obj, ZObject *rep_building_obj)
{
	//checks
	if(!unit_obj || !rep_building_obj)
	{
		return;
	}

	if(!rep_building_obj->SetRepairUnit(unit_obj))
	{
		return;
	}

	//kill the entie
	unit_obj->DoKillMe(0);

	//anounce the animation
	char *data;
	int size;
	rep_building_obj->CreateRepairAnimData(data, size);

	if(!data)
	{
		return;
	}

	server_socket.SendMessageAll(SET_REPAIR_ANIM, (const char*)data, size);
	free(data);
}

void ZServer::RobotEnterObject(ZObject *robot_obj, ZObject *dest_obj)
{
	//checks
	if(!robot_obj || !dest_obj)
	{
		return;
	}

	//can someone enter the target?
	if(!dest_obj->CanBeEntered())
	{
		return;
	}

	//set driver of target
	unsigned char ot, oid;
	robot_obj->GetObjectID(ot, oid);
	dest_obj->SetDriverType(oid);

	//add leader
	dest_obj->AddDriver(robot_obj->GetHealth());

	//kill the entie
	robot_obj->DoKillMe(0);

	//is this an apc that will take the minions too?
	dest_obj->GetObjectID(ot, oid);

	if(ot == VEHICLE_OBJECT && oid == APC)
	{
		for(ZObject* i : robot_obj->GetMinionList())
		{
			ZObject* minion_obj = i;

			//add minion
			dest_obj->AddDriver(minion_obj->GetHealth());

			//kill the entie
			minion_obj->DoKillMe(0);
		}
	}

	//change team of target
	ResetObjectTeam(dest_obj, robot_obj->GetOwner());

	//play portrait anim
	if(ot == VEHICLE_OBJECT)
	{
		RelayPortraitAnim(dest_obj->GetRefID(), VEHICLE_CAPTURED_ANIM);
	}
	else if(ot == CANNON_OBJECT)
	{
		RelayPortraitAnim(dest_obj->GetRefID(), GUN_CAPTURED_ANIM);
	}
}

void ZServer::RelayObjectManufacturedSound(ZObject *building_obj, ZObject *obj)
{
	if(!obj || !building_obj)
	{
		return;
	}

	unsigned char ot, oid;
	obj->GetObjectID(ot, oid);

	computer_msg_packet send_data;
	send_data.ref_id = obj->GetRefID();

	switch(ot)
	{
	case VEHICLE_OBJECT: 
		send_data.sound = COMP_VEHICLE_SND;
		break;
	case ROBOT_OBJECT:
		send_data.sound = COMP_ROBOT_SND;
		break;
	case CANNON_OBJECT:
		send_data.sound = COMP_GUN_SND;
		send_data.ref_id = building_obj->GetRefID();
		break;
	default:
		return;
	}

	RelayTeamMessage(obj->GetOwner(), COMP_MSG, (char*)&send_data, sizeof(computer_msg_packet));
}

void ZServer::RelayObjectLoc(ZObject *obj)
{
	char *data;
	int size;
	obj->CreateLocationData(data, size);

	if(!data)
	{
		return;
	}

	server_socket.SendMessageAll(SEND_LOC, data, size);
	free(data);
}

void ZServer::BuildingCreateCannon(ZObject *obj, unsigned char oid)
{
	//if it stores the cannon for placement, let the people know
	if(!obj->StoreBuiltCannon(oid))
	{
		return;
	}

	RelayBuildingBuiltCannons(obj);
	//have them play the tune too
	computer_msg_packet send_data;
	send_data.ref_id = obj->GetRefID();
	send_data.sound = COMP_GUN_SND;
	RelayTeamMessage(obj->GetOwner(), COMP_MSG, (char*)&send_data, sizeof(computer_msg_packet));
}

void ZServer::RelayBuildingBuiltCannons(ZObject *obj)
{
	char *data;
	int size;
	obj->CreateBuiltCannonData(data, size);

	if(!data)
	{
		return;
	}

	server_socket.SendMessageAll(SET_BUILT_CANNON_AMOUNT, (const char*)data, size);
	free(data);
}

void ZServer::ProcessChangeObjectAmount()
{
	CheckUnitLimitReached();
}

ZObject *ZServer::BuildingRepairUnit(ZObject *obj, unsigned char ot, unsigned char oid, int driver_type, std::vector<driver_info_s> &driver_info, std::vector<waypoint> &rwaypoint_list)
{
	//get the starting cords for the new unit
	int x, y;
	obj->GetRepairCenter(x,y);

	//make the unit
	ZObject *new_obj;
	if(ot == ROBOT_OBJECT)
	{
		new_obj = CreateRobotGroup(oid, x, y, obj->GetOwner(), &new_object_list);
	}
	else
	{
		new_obj = CreateObject(ot, oid, x, y, obj->GetOwner(), &new_object_list);
	}

	//wasn't made?
	if(!new_obj)
	{
		return NULL;
	}

	//does it have drivers?
	if(driver_info.size())
	{
		new_obj->SetDriverType(driver_type);
		new_obj->GetDrivers() = driver_info;
	}

	//shift its location
	int w, h;
	new_obj->GetDimensionsPixel(w, h);
	int new_x = (x - (w >> 1));
	int new_y = (y - (h >> 1));
	new_obj->SetCords(new_x, new_y);

	//shift its minions
	for(ZObject* i : new_obj->GetMinionList())
	{
		i->SetCords(new_x, new_y);
	}

	//set the initial waypoint
	waypoint init_movepoint;

	obj->GetRepairEntrance(x,y);

	init_movepoint.mode = FORCE_MOVE_WP;
	init_movepoint.ref_id = -1;
	init_movepoint.x = x;
	init_movepoint.y = y;

	new_obj->GetWayPointList().push_back(init_movepoint);

	//append old waypoints too
	if(rwaypoint_list.size() > 1)
	{
		for(std::vector<waypoint>::iterator wp=rwaypoint_list.begin()+1;wp!=rwaypoint_list.end();wp++)
		{
			new_obj->GetWayPointList().push_back(*wp);
		}
	}

	//clone dem waypoints
	new_obj->CloneMinionWayPoints();

	//tell everyone it exists
	RelayNewObject(new_obj);

	//and the minions...
	for(ZObject* i : new_obj->GetMinionList())
	{
		RelayNewObject(i);
	}

	//tell them where it is going
	RelayObjectWayPoints(new_obj);

	//tell them who is in it
	ResetObjectTeam(new_obj, new_obj->GetOwner());

	return new_obj;
}

ZObject *ZServer::BuildingCreateUnit(ZObject *obj, unsigned char ot, unsigned char oid)
{
	//cannons are built a little differently
	if(ot == CANNON_OBJECT)
	{
		if(obj->CannonsInZone(ols) < MAX_STORED_CANNONS)
		{
			BuildingCreateCannon(obj, oid);
		}
		return NULL;
	}

	//get the starting cords for the new unit
	int x, y;
	obj->GetBuildingCreationPoint(x,y);

	//make the unit
	ZObject* new_obj;
	if(ot == ROBOT_OBJECT)
	{
		new_obj = CreateRobotGroup(oid, x, y, obj->GetOwner(), &new_object_list);
	}
	else
	{
		new_obj = CreateObject(ot, oid, x, y, obj->GetOwner(), &new_object_list);
	}

	//wasn't made?
	if(!new_obj)
	{
		return NULL;
	}

	//shift its location
	int w, h;
	new_obj->GetDimensionsPixel(w, h);
	int new_x = x - (w >> 1);
	int new_y = y - (h >> 1);
	new_obj->SetCords(new_x, new_y);

	//shift its minions
	for(ZObject* i : new_obj->GetMinionList())
	{
		i->SetCords(new_x, new_y);
	}

	//set the initial waypoint
	waypoint init_movepoint;

	obj->GetBuildingCreationMovePoint(x,y);

	init_movepoint.mode = FORCE_MOVE_WP;
	init_movepoint.ref_id = -1;
	init_movepoint.x = x;
	init_movepoint.y = y;

	new_obj->GetWayPointList().push_back(init_movepoint);

	//clone dem waypoints
	new_obj->CloneMinionWayPoints();

	//tell everyone it exists
	RelayNewObject(new_obj);

	//and the minions...
	for(ZObject* i : new_obj->GetMinionList())
	{
		RelayNewObject(i);
	}

	//tell them where it is going
	RelayObjectWayPoints(new_obj);

	return new_obj;
}

void ZServer::CheckDestroyedBridge(ZObject *obj)
{
	if(!obj)
	{
		return;
	}

	unsigned char ot, oid;
	obj->GetObjectID(ot, oid);
	if(!(ot == BUILDING_OBJECT && (oid == BRIDGE_VERT || oid == BRIDGE_HORZ)))
	{
		return;
	}

	//kill all the robots and vehicles on the bridge right now
	for(ZObject* i : object_list)
	{
		//some checks
		if(i->IsDestroyed())
		{
			continue;
		}

		unsigned char iot, ioid;
		i->GetObjectID(iot, ioid);

		//is it a robot or vehicle?
		if(!(iot == VEHICLE_OBJECT || iot == ROBOT_OBJECT))
		{
			continue;
		}

		//does it intersect with this bridge?
		if(!i->IntersectsObject(*obj))
		{
			continue;
		}

		//destroy it then
		i->SetHealth(0, zmap);
		RelayObjectDeath(i);
	}
}

void ZServer::CheckDestroyedFort(ZObject *obj)
{
	if(!obj)
	{
		return;
	}

	unsigned char ot, oid;
	obj->GetObjectID(ot, oid);

	if(!(ot == BUILDING_OBJECT && (oid == FORT_FRONT || oid == FORT_BACK)))
	{
		return;
	}

	int team = obj->GetOwner();
	if(team == NULL_TEAM)
	{
		return;
	}

	//needed for all the end animations
	RelayTeamEnded(team, false);

	//destroy all the other objects in this list
	for(ZObject* i : object_list)
	{
		if((i->GetOwner() != team) || (i->IsDestroyed()) || (i == obj))
		{
			continue;
		}

		unsigned char iot, ioid;
		i->GetObjectID(iot, ioid);
		//no flags.
		if(iot == MAP_ITEM_OBJECT)
		{
			continue;
		}
		
		//destroy it then
		i->SetHealth(0, zmap);
		RelayObjectDeath(i);
	}

	//lose all the zones
	for(ZObject* i : ols.flag_olist)
	{
		if(i->GetOwner() != team)
		{
			continue;
		}

		AwardZone((OFlag*)i, NULL_TEAM);
	}

	//annouce the news
	BroadCastNews(std::string("The " + team_type_string[team] + " team has been eliminated").c_str());
}

void ZServer::RelayObjectDeath(ZObject *obj, int killer_ref_id)
{
	double &the_time = ztime.ztime;
	int missile_radius, missile_damage;
	destroy_object_packet send_data_base;
	char *send_data;
	int size;
	int i;
	std::vector<fire_missile_info> missile_list;

	send_data_base.ref_id = obj->GetRefID();
	send_data_base.killer_ref_id = killer_ref_id;
	missile_list = obj->ServerFireTurrentMissile(missile_damage, missile_radius);
	send_data_base.fire_missile_amount = missile_list.size();
	send_data_base.destroy_object = false;
	send_data_base.do_fire_death = (the_time - obj->GetDamagedByFireTime() < 1.5);
	send_data_base.do_missile_death = (the_time - obj->GetDamagedByMissileTime() < 1.5);

	//make data
	size = sizeof(destroy_object_packet) + (sizeof(fire_missile_info) * send_data_base.fire_missile_amount);
	send_data = (char*)malloc(size);
	memcpy(send_data, (const char*)&send_data_base, sizeof(destroy_object_packet));

	//turrent damage
	for(i=0; i<send_data_base.fire_missile_amount; i++)
	{
		//push into the send packet
		memcpy(send_data + sizeof(destroy_object_packet) + (sizeof(fire_missile_info) * i), (const char*)&missile_list[i], sizeof(fire_missile_info));

		//push into the server missile list
		damage_missile new_missile;

		new_missile.x = missile_list[i].missile_x;
		new_missile.y = missile_list[i].missile_y;
		new_missile.damage = missile_damage;
		new_missile.radius = missile_radius;
		new_missile.team = NULL_TEAM;
		new_missile.attacker_ref_id = -1;
		new_missile.attack_player_given = false;
		new_missile.target_ref_id = -1;
		new_missile.explode_time = the_time + missile_list[i].missile_offset_time;

		new_damage_missile_list.push_back(new_missile);
	}

	if(obj->CanBeDestroyed())
	{
		obj->DoKillMe(the_time + LIFE_AFTER_DEATH_TIME);
		send_data_base.destroy_object = true;
	}

	server_socket.SendMessageAll(DESTROY_OBJECT, (const char*)send_data, size);

	//clear data
	if(send_data)
	{
		free(send_data);
	}

	//destroying an object stops it's production
	//so announce this too
	if(obj->ProducesUnits())
	{
		RelayBuildingState(obj);
	}
}

void ZServer::RelayObjectAttackObject(ZObject *obj)
{
	if(!obj)
	{
		return;
	}

	char *data;
	int size;
	obj->CreateAttackObjectData(data, size);
	if(!data)
	{
		return;
	}

	server_socket.SendMessageAll(SET_ATTACK_OBJECT, data, size);
	free(data);
}

void ZServer::UpdateObjectDriverHealth(ZObject *obj)
{
	if(obj->GetDrivers().size() && obj->GetDrivers().begin()->driver_health > 0)
	{
		//tell the clients to do the driver hit effect
		driver_hit_packet send_data;
		send_data.ref_id = obj->GetRefID();
		server_socket.SendMessageAll(DRIVER_HIT_EFFECT, (char*)&send_data, sizeof(driver_hit_packet));
	}
	else
	{
		//say it has been sniped
		snipe_object_packet send_data;
		send_data.ref_id = obj->GetRefID();
		server_socket.SendMessageAll(SNIPE_OBJECT, (char*)&send_data, sizeof(snipe_object_packet));

		//disengage
		if(obj->Disengage())
		{
			RelayObjectAttackObject(obj);
		}

		//driver doesn't exist anymore
		ResetObjectTeam(obj, NULL_TEAM);

		//clear waypoints if there are any
		if(obj->GetWayPointList().size())
		{
			obj->GetWayPointList().clear();

			//let folks know about the waypoint updates
			RelayObjectWayPoints(obj);

			//stop movement
			if(obj->StopMove())
			{
				RelayObjectLoc(obj);
			}
		}

		//all objects attacking this obj must quit
		for(ZObject* o : object_list)
		{
			//are we currently attacking this object? then stop
			if(o->GetAttackObject() == obj)
			{
				o->Disengage();
				RelayObjectAttackObject(o);
			}

			//do we have a waypoint looking to kill just this target?
			if(o->GetWayPointList().size() && 
				(o->GetWayPointList().begin()->mode == ATTACK_WP || o->GetWayPointList().begin()->mode == AGRO_WP) &&
				o->GetWayPointList().begin()->ref_id == obj->GetRefID())
			{
				//kill the waypoint
				o->GetWayPointList().erase(o->GetWayPointList().begin());

				//let folks know about the waypoint updates
				RelayObjectWayPoints(o);

				//stop movement
				if(o->StopMove())
				{
					RelayObjectLoc(o);
				}
			}
		}
	}
}

void ZServer::UpdateObjectHealth(ZObject *obj, int attacker_ref_id)
{
	if(!obj)
	{
		return;
	}

	if(obj->IsDestroyed() && !obj->HasProcessedDeath())
	{
		obj->SetHasProcessedDeath(true);

		RelayObjectDeath(obj, attacker_ref_id);

		CheckDestroyedFort(obj);
		CheckDestroyedBridge(obj);

		CheckNoUnitsDestroyFort(obj->GetOwner());
	}
	else
	{
		RelayObjectHealth(obj);
	}

	//if it is a building, then new state info needs to go out
	RelayBuildingState(obj);
}

void ZServer::RelayObjectHealth(ZObject *obj, int player)
{
	if(!obj)
	{
		return;
	}

	object_health_packet send_data;
	send_data.ref_id = obj->GetRefID();
	send_data.health = obj->GetHealth();

	//send data
	if(player == -1)
	{
		server_socket.SendMessageAll(UPDATE_HEALTH, (const char*)&send_data, sizeof(object_health_packet));
	}
	else
	{
		server_socket.SendMessage(player, UPDATE_HEALTH, (const char*)&send_data, sizeof(object_health_packet));
	}
}

void ZServer::CheckNoUnitsDestroyFort(int team)
{
	if(team == NULL_TEAM)
	{
		return;
	}

	//leave if we find a unit in this team that is alive
	for(ZObject* obj : ols.passive_engagable_olist)
	{
		unsigned char ot, oid;
		obj->GetObjectID(ot, oid);

		//only leave the function
		//if we find an alive unit on our team
		if(obj->GetOwner() != team)
		{
			continue;
		}
		if(!(ot == ROBOT_OBJECT || ot == VEHICLE_OBJECT || ot == CANNON_OBJECT))
		{
			continue;
		}
		if(obj->IsDestroyed())
		{
			continue;
		}

		return;
	}

	//ok destroy all the forts
	for(ZObject* obj : object_list)
	{
		unsigned char ot, oid;
		obj->GetObjectID(ot, oid);

		if(obj->GetOwner() != team)
		{
			continue;
		}
		if(!(ot == BUILDING_OBJECT && (oid == FORT_FRONT || oid == FORT_BACK)))
		{
			continue;
		}
		if(obj->IsDestroyed())
		{
			continue;
		}

		//destroy it then
		obj->SetHealth(0, zmap);
		CheckDestroyedFort(obj);
		RelayObjectDeath(obj);
	}
}

void ZServer::CheckFlagCaptures()
{
	static double next_time = 0;
	double &the_time = ztime.ztime;

	if(the_time < next_time)
	{
		return;
	}

	next_time = the_time + 0.2;

	for(ZObject* f : ols.flag_olist)
	{
		for(ZObject* o : ols.mobile_olist)
		{
			if(o->GetOwner() == NULL_TEAM)
			{
				continue;
			}
			if(f->GetOwner() == o->GetOwner())
			{
				continue;
			}

			unsigned char ot, oid;
			o->GetObjectID(ot, oid);

			//only get mobile units
			if(!(ot == VEHICLE_OBJECT || ot == ROBOT_OBJECT))
			{
				continue;
			}

			//they intersect?
			if(!f->IntersectsObject(*o))
			{
				continue;
			}

			//ok they intersect so lets give the team the zone
			AwardZone((OFlag*)f, o);

			//escape looking for objects that could capture this flag
			break;
		}
	}
}

void ZServer::AwardZone(OFlag *flag, ZObject *conquerer)
{
	RelayPortraitAnim(conquerer->GetRefID(), TERRITORY_TAKEN_ANIM);
	AwardZone(flag, conquerer->GetOwner());
}

void ZServer::AwardZone(OFlag *flag, team_type new_team)
{
	team_type old_team = flag->GetLinkedZone()->owner;

	//set the flags team
	ResetObjectTeam(flag, new_team);

	//set all its buildings teams
	for(ZObject* i : flag->GetLinkedObjects())
	{
		ResetObjectTeam(i, new_team);
		
		//start default production
		if(i->SetBuildingDefaultProduction())
		{
			RelayBuildingState(i);
		}
	}

	//set the zones team
	flag->GetLinkedZone()->owner = new_team;

	//send the zone info to everyone
	zone_info_packet packet_info;
	packet_info.zone_number = flag->GetLinkedZone()->id;
	packet_info.owner = flag->GetLinkedZone()->owner;
	server_socket.SendMessageAll(SET_ZONE_INFO, (char*)&packet_info, sizeof(zone_info_packet));

	//tell them to play a tune
	if(old_team != NULL_TEAM)
	{
		computer_msg_packet send_data;
		send_data.ref_id = -1;
		send_data.sound = COMP_TERRITORY_LOST_SND;
		RelayTeamMessage(old_team, COMP_MSG, (char*)&send_data, sizeof(computer_msg_packet));
	}

	if(flag->HasRadar())
	{
		computer_msg_packet send_data;
		send_data.ref_id = -1;
		send_data.sound = COMP_RADAR_ACTIVATED_SND;
		RelayTeamMessage(new_team, COMP_MSG, (char*)&send_data, sizeof(computer_msg_packet));
	}

	//ok do this
	ResetZoneOwnagePercentages(true);
}

void ZServer::RemoveObjectFromGroup(ZObject *obj)
{
	if(!obj->IsApartOfAGroup())
	{
		return;
	}

	if(obj->GetGroupLeader())
	{
		//it has a group leader so remove itself from the leaders list
		obj->GetGroupLeader()->RemoveGroupMinion(obj);

		//update all on the leaders new group info
		RelayObjectGroupInfo(obj->GetGroupLeader());

		//update all on this objects info
	}
	else if(obj->GetMinionList().size())
	{
		//it is a group leader
		ZObject *new_leader = NULL;

		//pick the new leader
		for(ZObject* i : obj->GetMinionList())
		{
			if(i)
			{
				new_leader = i;
				break;
			}
		}

		if(new_leader)
		{
			//tell everyone the new leader
			for(ZObject* i : obj->GetMinionList())
			{
				if(i)
				{
					i->SetGroupLeader(new_leader);
				}
			}

			//clear out new leader info
			new_leader->ClearGroupInfo();

			//set up new_leader's minion list
			for(ZObject* i : obj->GetMinionList())
			{
				if(i)
				{
					new_leader->AddGroupMinion(i);
				}
			}

			//send out new leaders info
			RelayObjectGroupInfo(new_leader);

			//send out minions new info
			for(ZObject* i : new_leader->GetMinionList())
			{
				if(i)
				{
					RelayObjectGroupInfo(i);
				}
			}

			//give new leader the grenades
			if(obj->GetGrenadeAmount())
			{
				new_leader->SetGrenadeAmount(obj->GetGrenadeAmount());
				RelayObjectGrenadeAmount(new_leader);
			}
		}
	}

	//now do its stuff
	obj->ClearGroupInfo();
	RelayObjectGroupInfo(obj);
}

void ZServer::DeleteObject(ZObject *obj)
{
	//Clean up group stuff
	RemoveObjectFromGroup(obj);
	//Clean up map
	obj->UnSetMapImpassables(zmap);

	int ref_id = obj->GetRefID();

	//Make sure no other part of the server is point to this object
	for (ZObject* o : object_list) {
		o->RemoveObject(obj);
	}

	//Clean up server olilsts
	ols.RemoveObject(obj);

	//Do the actual deletion
	for (std::vector<ZObject*>::iterator i = object_list.begin(); i != object_list.end(); i++)
	{
		if (obj == *i)
		{
			object_list.erase(i);
			
		}
	}
	delete obj;

	//Do this so unit production can start again
	CheckUnitLimitReached();

	//tell everyone it is deleted
	server_socket.SendMessageAll(DELETE_OBJECT, (char*)&ref_id, sizeof(int));
}

void ZServer::ResetObjectTeam(ZObject *obj, team_type new_team)
{
	char *data;
	int size;

	obj->SetOwner(new_team);
	obj->CreateTeamData(data, size);

	if(!data)
	{
		return;
	}

	server_socket.SendMessageAll(SET_OBJECT_TEAM, data, size);
	free(data);

	//should need to be here
	ProcessChangeObjectAmount();
}

void ZServer::SetMapName(std::string map_name_)
{
	map_name = map_name_;
}

void ZServer::SetMapList(std::string map_list_name_)
{
	map_list_name = map_list_name_;
}

void ZServer::SetSettingsFilename(std::string settings_filename_)
{
	settings_filename = settings_filename_;
}

void ZServer::SetPerpetualSettingsFilename(std::string p_settings_filename_)
{
	p_settings_filename = p_settings_filename_;
}

void ZServer::SendNews(int player, const char* message, char r, char g, char b)
{
	char *data;
	int message_size = strlen(message) + 1;

	if(message_size <= 1)
	{
		return;
	}

	data = (char*)malloc(message_size+3);

	memcpy(data, &r, 1);
	memcpy(data+1, &g, 1);
	memcpy(data+2, &b, 1);
	memcpy(data+3, message, message_size);

	server_socket.SendMessage(player, NEWS_EVENT, data, message_size+3);

	free(data);
}

void ZServer::BroadCastNews(const char* message, char r, char g, char b)
{  
	int max = server_socket.PlayersConnected();
	for(int i=0;i<max;i++)
	{
		SendNews(i, message, r, g, b);
	}
}

void ZServer::RelayObjectRallyPoints(ZObject *obj, int player)
{
	//sanity
	if(!obj)
	{
		return;
	}

	//it even do rally points?
	if(!obj->CanSetRallypoints())
	{
		return;
	}

	//get data
	char *data;
	int size;
	CreateWaypointSendData(obj->GetRefID(), obj->GetRallyPointList(), data, size);

	//any data made?
	if(!data)
	{
		return;
	}

	//send data
	if(player == -1)
	{
		server_socket.SendMessageAll(SEND_RALLYPOINTS, data, size);
	}
	else
	{
		server_socket.SendMessage(player, SEND_RALLYPOINTS, data, size);
	}

	free(data);
}

void ZServer::RelayObjectBuildingQueue(ZObject *obj, int player)
{
	if(!obj)
	{
		return;
	}

	char *data;
	int size;
	obj->CreateBuildingQueueData(data, size);
	
	if(!data)
	{
		return;
	}

	if(player == -1)
	{
		server_socket.SendMessageAll(SET_BUILDING_QUEUE_LIST, data, size);
	}
	else
	{
		server_socket.SendMessage(player, SET_BUILDING_QUEUE_LIST, data, size);
	}

	free(data);
}

void ZServer::RelayObjectGroupInfo(ZObject *obj, int player)
{
	if(!obj)
	{
		return;
	}

	char *data;
	int size;
	obj->CreateGroupInfoData(data, size);

	if(!data)
	{
		return;
	}

	if(player == -1)
	{
		server_socket.SendMessageAll(OBJECT_GROUP_INFO, data, size);
	}
	else
	{
		server_socket.SendMessage(player, OBJECT_GROUP_INFO, data, size);
	}

	free(data);
}

void ZServer::RelayVersion(int player)
{
	//check
	if(strlen(GAME_VERSION) + 1 >= MAX_VERSION_PACKET_CHARS)
	{
		spdlog::error("ZServer::RelayVersion - {} too large to fit in a {} char array", GAME_VERSION, MAX_VERSION_PACKET_CHARS);
		return;
	}

	version_packet packet;
	strcpy(packet.version, GAME_VERSION);

	if(player == -1)
	{
		server_socket.SendMessageAll(GIVE_VERSION, (char*)&packet, sizeof(version_packet));
	}
	else
	{
		server_socket.SendMessage(player, GIVE_VERSION, (char*)&packet, sizeof(version_packet));
	}
}

void ZServer::RelayObjectWayPoints(ZObject *obj)
{
	if(!obj)
	{
		return;
	}

	char *data;
	int size;
	CreateWaypointSendData(obj->GetRefID(), obj->GetWayPointList(), data, size);
	if(!data)
	{
		return;
	}

	RelayTeamMessage(obj->GetOwner(), SEND_WAYPOINTS, data, size);
	free(data);
}

void ZServer::RelayTeamMessage(team_type the_team, int pack_id, const char *data, int size)
{
	int max = server_socket.PlayersConnected();
	for(int i=0;i<max;i++)
	{
		if(player_info[i].team == the_team)
		{
			server_socket.SendMessage(i, pack_id, data, size);
		}
	}
}

void ZServer::RelayBuildingState(ZObject *obj, int player)
{
	//building info packet
	char *data;
	int size;
	obj->CreateBuildingStateData(data, size);
	if(data)
	{
		if(player == -1)
		{
			server_socket.SendMessageAll(SET_BUILDING_STATE, data, size);
		}
		else
		{
			server_socket.SendMessage(player, SET_BUILDING_STATE, data, size);
		}

		free(data);
	}

	//repair anim info packet
	obj->CreateRepairAnimData(data, size);
	if(data)
	{
		if(player == -1)
		{
			server_socket.SendMessageAll(SET_REPAIR_ANIM, data, size);
		}
		else
		{
			server_socket.SendMessage(player, SET_REPAIR_ANIM, data, size);
		}

		free(data);
	}

	//send queue info
	RelayObjectBuildingQueue(obj, player);
}

void ZServer::RelayNewObject(ZObject *obj, int player)
{
	if(!obj)
	{
		return;
	}

	//pack the info
	object_init_packet object_info;
	obj->GetCords(object_info.x, object_info.y);
	obj->GetObjectID(object_info.object_type, object_info.object_id);
	object_info.owner = obj->GetOwner();
	object_info.ref_id = obj->GetRefID();
	object_info.blevel = obj->GetLevel();
	object_info.extra_links = obj->GetExtraLinks();
	object_info.health = obj->GetHealth();

	//send it
	if(player == -1)
	{
		server_socket.SendMessageAll(ADD_NEW_OBJECT, (char*)&object_info, sizeof(object_init_packet));
	}
	else
	{
		server_socket.SendMessage(player, ADD_NEW_OBJECT, (char*)&object_info, sizeof(object_init_packet));
	}

	//send minion info
	RelayObjectGroupInfo(obj, player);
}

void ZServer::CheckObjectWaypoints(ZObject *obj)
{
	unsigned char ot, oid;
	obj->GetObjectID(ot, oid);

	for(std::vector<waypoint>::iterator wp=obj->GetWayPointList().begin(); wp!=obj->GetWayPointList().end();)
	{
		ZObject *aobj;

		//clients are not allowed to set forcemove waypoints
		if(wp->mode == FORCE_MOVE_WP)
		{
			wp->mode = MOVE_WP;
		}

		//clients are not allowed to set agro waypoints
		if(wp->mode == AGRO_WP)
		{
			wp->mode = ATTACK_WP;
		}
		
		//attacking an object that can only be attacked by explosions?
		if(wp->mode == ATTACK_WP)
		{
			aobj = GetObjectFromID(wp->ref_id, object_list);

			if(!aobj)
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}

			if(!obj->HasExplosives() && aobj->AttackedOnlyByExplosives())
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}

			if(!obj->CanAttack())
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}

		}

		//entering ok?
		if(wp->mode == ENTER_WP)
		{
			//this a robot?
			if(ot != ROBOT_OBJECT)
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}

			//target exist?
			aobj = GetObjectFromID(wp->ref_id, object_list);
			if(!aobj)
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}

			//can the target be entered?
			if(!aobj->CanBeEntered())
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}
		}

		if(wp->mode == CRANE_REPAIR_WP)
		{
			//it a crane?
			if(!(ot == VEHICLE_OBJECT && oid == CRANE))
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}

			//target exist?
			aobj = GetObjectFromID(wp->ref_id, object_list);
			if(!aobj)
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}

			//can it repair that target?
			if(!aobj->CanBeRepairedByCrane(obj->GetOwner()))
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}
		}
	
		if(wp->mode == UNIT_REPAIR_WP)
		{
			//can it be repaired?
			if(!obj->CanBeRepaired())
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}

			//target exist?
			aobj = GetObjectFromID(wp->ref_id, object_list);
			if(!aobj)
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}

			//can the target repair it?
			if(!aobj->CanRepairUnit(obj->GetOwner()))
			{
				wp = obj->GetWayPointList().erase(wp);
				continue;
			}
		}


		//bad mode?
		if(wp->mode < 0 || wp->mode >= MAX_WAYPOINT_MODES)
		{
			wp = obj->GetWayPointList().erase(wp);
			continue;
		}
		
		wp++;
	}
}

void ZServer::ResetZoneOwnagePercentages(bool notify_players)
{
	int zone_ownage[MAX_TEAM_TYPES];

	//special case
	if(!ols.flag_olist.size())
	{
		for(int i=0;i<MAX_TEAM_TYPES;i++)
		{
			team_zone_percentage[i] = 0;
		}

		return;
	}

	//clear
	std::fill(std::begin(zone_ownage), std::end(zone_ownage), 0);

	//tally
	for(ZObject* of : ols.flag_olist)
	{
		zone_ownage[of->GetOwner()]++;
	}

	//percentage
	for(int i=0;i<MAX_TEAM_TYPES;i++)
	{
		team_zone_percentage[i] = 1.0 * zone_ownage[i] / ols.flag_olist.size();
	}

	//tell all the buildings to redo their build orders
	for(ZObject* obj : object_list)
	{
		if(obj->ResetBuildTime(team_zone_percentage[obj->GetOwner()]) && notify_players)
		{
			RelayBuildingState(obj);
		}
	}
}

bool ZServer::LoginPlayer(int player, ZMysqlUser &user)
{
	if(!psettings.use_database)
	{
		return false;
	}

	if(player_info[player].logged_in)
	{
		SendNews(player, "login error: please log off first", 0, 0, 0);
		return false;
	}

	//logout others
	LogoutOthers(user.tabID);

	player_info[player].name = user.username;
	player_info[player].db_id = user.tabID;
	player_info[player].activated = user.activated;
	player_info[player].voting_power = user.voting_power;
	player_info[player].total_games = user.total_games;
	player_info[player].logged_in = true;
	player_info[player].last_time = time(0);

	if(psettings.use_mysql)
	{
		std::string err_msg;

		if(!zmysql.UpdateUserVariable(err_msg, user.tabID, "last_time", time(0)))
		{
			spdlog::error("ZServer::LoginPlayer - MySQL Error - {}", err_msg);
		}

		if(!zmysql.UpdateUserVariable(err_msg, user.tabID, "last_ip", player_info[player].ip))
		{
			spdlog::error("ZServer::LoginPlayer - MySQL Error - {}", err_msg);
		}
	}

	BroadCastNews(std::string(player_info[player].name + " logged in").c_str());

	//tell everyone
	RelayLPlayerName(player);
	RelayLPlayerLoginInfo(player);
	RelayLPlayerVoteChoice(player);

	//check history stuff
	CheckOnlineHistoryPlayerCount();

	//check vote stuff
	CheckVote();

	//tell them to (or not) display the login menu
	SendPlayerLoginRequired(player);

	return true;
}

bool ZServer::LogoutPlayer(int player, bool connection_exists)
{
	if(!psettings.use_database)
	{
		return false;
	}

	if(player_info[player].logged_in)
	{
		//append total_time
		std::string err_msg;

		if(!zmysql.IncreaseUserVariable(err_msg, player_info[player].db_id, "total_time", time(0) - player_info[player].last_time))
		{
			spdlog::error("ZServer::LogoutPlayer - MySQL Error - {}", err_msg);
		}

		BroadCastNews(std::string(player_info[player].name + " logged out").c_str());

		player_info[player].logout();

		//tell everyone
		RelayLPlayerName(player);
		RelayLPlayerLoginInfo(player);
		RelayLPlayerVoteChoice(player);

		//check vote stuff
		CheckVote();

		//tell them to (or not) display the login menu
		if(connection_exists)
		{
			SendPlayerLoginRequired(player);
		}
	}

	return true;
}

void ZServer::LogoutOthers(int db_id)
{
	for(int i=0; i<player_info.size(); i++)
	{
		if(player_info[i].logged_in && player_info[i].db_id == db_id)
		{
			SendNews(i, "warning: someone else has logged in with your user", 0, 0, 0);
			LogoutPlayer(i);
		}
	}

}

void ZServer::RelayLAdd(int player, int to_player)
{
	add_remove_player_packet packet;
	packet.p_id = player_info[player].p_id;

	//add it
	if(to_player == -1)
	{
		server_socket.SendMessageAll(ADD_LPLAYER, (char*)&packet, sizeof(add_remove_player_packet));
	}
	else
	{
		server_socket.SendMessage(to_player, ADD_LPLAYER, (char*)&packet, sizeof(add_remove_player_packet));
	}
}

void ZServer::RelayLPlayerName(int player, int to_player)
{
	int size = sizeof(int) + player_info[player].name.size() + 1;
	char* data = (char*)malloc(size);

	memcpy(data, &(player_info[player].p_id), sizeof(int));
	memcpy(data+sizeof(int), player_info[player].name.c_str(), player_info[player].name.size());
	*(data+sizeof(int)+player_info[player].name.size()) = 0;

	if(to_player == -1)
	{
		server_socket.SendMessageAll(SET_LPLAYER_NAME, data, size);
	}
	else
	{
		server_socket.SendMessage(to_player, SET_LPLAYER_NAME, data, size);
	}

	free(data);
}

void ZServer::RelayLPlayerTeam(int player, int to_player)
{
	set_player_int_packet packet;
	packet.p_id = player_info[player].p_id;
	packet.value = player_info[player].team;

	if(to_player == -1)
	{
		server_socket.SendMessageAll(SET_LPLAYER_TEAM, (char*)&packet, sizeof(set_player_int_packet));
	}
	else
	{
		server_socket.SendMessage(to_player, SET_LPLAYER_TEAM, (char*)&packet, sizeof(set_player_int_packet));
	}
}

void ZServer::RelayLPlayerMode(int player, int to_player)
{
	set_player_int_packet packet;
	packet.p_id = player_info[player].p_id;
	packet.value = player_info[player].mode;

	if(to_player == -1)
	{
		server_socket.SendMessageAll(SET_LPLAYER_MODE, (char*)&packet, sizeof(set_player_int_packet));
	}
	else
	{
		server_socket.SendMessage(to_player, SET_LPLAYER_MODE, (char*)&packet, sizeof(set_player_int_packet));
	}
}

void ZServer::RelayLPlayerIgnored(int player, int to_player)
{
	set_player_int_packet packet;
	packet.p_id = player_info[player].p_id;
	packet.value = player_info[player].ignored;

	if(to_player == -1)
	{
		server_socket.SendMessageAll(SET_LPLAYER_IGNORED, (char*)&packet, sizeof(set_player_int_packet));
	}
	else
	{
		server_socket.SendMessage(to_player, SET_LPLAYER_IGNORED, (char*)&packet, sizeof(set_player_int_packet));
	}
}

void ZServer::RelayLPlayerLoginInfo(int player, int to_player)
{
	set_player_loginfo_packet packet;
	packet.p_id = player_info[player].p_id;
	packet.db_id = player_info[player].db_id;
	packet.voting_power = player_info[player].voting_power;
	packet.total_games = player_info[player].total_games;
	packet.activated = player_info[player].activated;
	packet.logged_in = player_info[player].logged_in;
	packet.bot_logged_in = player_info[player].bot_logged_in;

	if(to_player == -1)
	{
		server_socket.SendMessageAll(SET_LPLAYER_LOGINFO, (char*)&packet, sizeof(set_player_loginfo_packet));
	}
	else
	{
		server_socket.SendMessage(to_player, SET_LPLAYER_LOGINFO, (char*)&packet, sizeof(set_player_loginfo_packet));
	}
}

void ZServer::RelayLPlayerVoteChoice(int player, int to_player)
{
	set_player_int_packet packet;
	packet.p_id = player_info[player].p_id;
	packet.value = player_info[player].vote_choice;

	if(to_player == -1)
	{
		server_socket.SendMessageAll(SET_LPLAYER_VOTEINFO, (char*)&packet, sizeof(set_player_int_packet));
	}
	else
	{
		server_socket.SendMessage(to_player, SET_LPLAYER_VOTEINFO, (char*)&packet, sizeof(set_player_int_packet));
	}
}

bool ZServer::LoginCheckDenied(int player)
{
	return (psettings.require_login && !player_info[player].logged_in && !player_info[player].bot_logged_in);
}

bool ZServer::ActivationCheckPassed(int player)
{
	if(!psettings.require_login)
	{
		return true;
	}
	if(psettings.ignore_activation)
	{
		return true;
	}
	if(player_info[player].bot_logged_in)
	{
		return true;
	}
	if(player_info[player].logged_in && player_info[player].activated)
	{
		return true;
	}

	return false;
}

void ZServer::RelayTeamsWonEnded()
{
	bool team_won[MAX_TEAM_TYPES];

	//clear
	std::fill(std::begin(team_won), std::end(team_won), false);

	//populate team_won[]
	for(ZObject* obj : object_list)
	{
		unsigned char ot, oid;
		obj->GetObjectID(ot, oid);

		if(!(ot == ROBOT_OBJECT || ot == VEHICLE_OBJECT || ot == CANNON_OBJECT))
		{
			continue;
		}

		team_won[obj->GetOwner()] = true;
	}

	for(int i=RED_TEAM;i<MAX_TEAM_TYPES;i++)
	{
		if(team_won[i])
		{
			RelayTeamEnded(i, true);
		}
	}
}

void ZServer::UpdateUsersWinLoses()
{
	if(!psettings.use_database)
	{
		return;
	}
	if(!psettings.use_mysql)
	{
		return;
	}

	bool team_won[MAX_TEAM_TYPES];
	for(ZObject* obj : object_list)
	{
		unsigned char ot, oid;
		obj->GetObjectID(ot, oid);

		if(!(ot == ROBOT_OBJECT || ot == VEHICLE_OBJECT || ot == CANNON_OBJECT))
		{
			continue;
		}

		team_won[obj->GetOwner()] = true;
	}

	//update users
	for(p_info i : player_info)
	{
		if(i.logged_in && i.team != NULL_TEAM)
		{
			std::string err_msg;

			i.total_games++;

			if(!zmysql.IncreaseUserVariable(err_msg, i.db_id, "total_games", 1))
			{
				spdlog::error("ZServer::UpdateUsersWinLoses - MySQL Error - {}", err_msg);
			}

			if(team_won[i.team])
			{
				if(!zmysql.IncreaseUserVariable(err_msg, i.db_id, "wins", 1))
				{
					spdlog::error("ZServer::UpdateUsersWinLoses - MySQL Error - {}", err_msg);
				}
			}
			else
			{
				if(!zmysql.IncreaseUserVariable(err_msg, i.db_id, "loses", 1))
				{
					spdlog::error("ZServer::UpdateUsersWinLoses - MySQL Error - {}", err_msg);
				}
			}
		}
	}

	//send out updates
	for(int i=0;i<player_info.size();i++)
	{
		RelayLPlayerLoginInfo(i);
	}
}

void ZServer::InitOnlineHistory()
{
	if(!psettings.use_mysql)
	{
		return;
	}

	next_online_history_time = COMMON::current_time() + (60 * 15);
	next_online_history_player_count = 0;
	next_online_history_tray_player_count = 0;
}

void ZServer::CheckUpdateOnlineHistory()
{
	if(!psettings.use_mysql)
	{
		return;
	}

	double the_time = COMMON::current_time();
	if(the_time < next_online_history_time)
	{
		return;
	}

	//last double check
	CheckOnlineHistoryPlayerCount();
	CheckOnlineHistoryTrayPlayerCount();

	//insert
	std::string err_msg;
	if(!zmysql.InsertOnlineHistoryEntry(err_msg, time(0), next_online_history_player_count, next_online_history_tray_player_count))
	{
		spdlog::error("ZServer::UpdateUsersWinLoses - MySQL Error - {}", err_msg);
	}

	//clear
	next_online_history_time = the_time + (60 * 15);
	next_online_history_player_count = 0;
	next_online_history_tray_player_count = 0;
}

void ZServer::CheckOnlineHistoryPlayerCount()
{
	if(!psettings.use_mysql)
	{
		return;
	}

	int current_player_count = 0;

	//get
	if(psettings.require_login)
	{
		for(p_info i : player_info)
		{
			if(i.logged_in)
			{
				current_player_count++;
			}
		}
	}
	else
	{
		for(p_info i : player_info)
		{
			if(i.mode == PLAYER_MODE)
			{
				current_player_count++;
			}
		}
	}

	//set
	if(current_player_count > next_online_history_player_count)
	{
		next_online_history_player_count = current_player_count;
	}
}

void ZServer::CheckOnlineHistoryTrayPlayerCount()
{
	if(!psettings.use_mysql)
	{
		return;
	}

	int current_player_count = 0;
	for(p_info i : player_info)
	{
		if(i.mode == TRAY_MODE)
		{
			current_player_count++;
		}
	}

	//set
	if(current_player_count > next_online_history_tray_player_count)
	{
		next_online_history_tray_player_count = current_player_count;
	}
}

void ZServer::AwardAffiliateCreation(std::string ip)
{
	std::string err_msg;
	int users_found;
	if(!zmysql.IPInUserTable(err_msg, ip, users_found))
	{
		spdlog::error("ZServer::AwardAffiliateCreation - MySQL Error - {}", err_msg);
		return;
	}

	//only award if this is the first creation with this ip
	if(users_found != 1)
	{
		return;
	}

	int aff_id;
	bool ip_found;
	if(!zmysql.GetAffiliateId(err_msg, ip, aff_id, ip_found))
	{
		spdlog::error("ZServer::AwardAffiliateCreation - MySQL Error - {}", err_msg);
		return;
	}

	//the ip is in the affiliate table
	//and it was brought in bought another user
	if(ip_found && aff_id != -1)
	{
		//reward the user
		if(!zmysql.IncreaseUserVariable(err_msg, aff_id, "aff_creates", 1))
		{
			spdlog::error("ZServer::AwardAffiliateCreation - MySQL Error - {}", err_msg);
		}
	}
}

void ZServer::PauseGame()
{
	if(ztime.IsPaused())
	{
		return;
	}

	ztime.Pause();
	RelayGamePaused();
}

void ZServer::ResumeGame()
{
	if(!ztime.IsPaused())
	{
		return;
	}

	ztime.Resume();
	RelayGamePaused();
}

void ZServer::RelayObjectGrenadeAmount(ZObject *obj, int player)
{
	if(!obj->CanHaveGrenades())
	{
		return;
	}

	obj_grenade_amount_packet packet;
	packet.ref_id = obj->GetRefID();
	packet.grenade_amount = obj->GetGrenadeAmount();

	if(player == -1)
	{
		server_socket.SendMessageAll(SET_GRENADE_AMOUNT, (const char*)&packet, sizeof(obj_grenade_amount_packet));
	}
	else
	{
		server_socket.SendMessage(player, SET_GRENADE_AMOUNT, (const char*)&packet, sizeof(obj_grenade_amount_packet));
	}
}

void ZServer::RelayPortraitAnim(int ref_id, int anim_id)
{
	do_portrait_anim_packet the_data;
	the_data.ref_id = ref_id;
	the_data.anim_id = anim_id;

	server_socket.SendMessageAll(DO_PORTRAIT_ANIM, (const char*)&the_data, sizeof(do_portrait_anim_packet));
}

void ZServer::RelayTeamEnded(int team, bool won)
{
	team_ended_packet the_data;
	the_data.team = team;
	the_data.won = won;

	server_socket.SendMessageAll(TEAM_ENDED, (const char*)&the_data, sizeof(team_ended_packet));
}

void ZServer::RelayGamePaused(int player)
{
	update_game_paused_packet packet;
	packet.game_paused = ztime.IsPaused();

	if(player == -1)
	{
		server_socket.SendMessageAll(UPDATE_GAME_PAUSED, (const char*)&packet, sizeof(update_game_paused_packet));
	}
	else
	{
		server_socket.SendMessage(player, UPDATE_GAME_PAUSED, (const char*)&packet, sizeof(update_game_paused_packet));
	}
}

void ZServer::RelayGameSpeed(int player)
{
	float_packet packet;
	packet.game_speed = ztime.GameSpeed();

	if(player == -1)
	{
		server_socket.SendMessageAll(UPDATE_GAME_SPEED, (const char*)&packet, sizeof(float_packet));
	}
	else
	{
		server_socket.SendMessage(player, UPDATE_GAME_SPEED, (const char*)&packet, sizeof(float_packet));
	}
}

bool ZServer::VoteRequired()
{
	int player_count = 0;

	for(p_info i : player_info)
	{
		if(i.mode == PLAYER_MODE)
		{
			player_count++;

			if(player_count >= 2)
			{
				return true;
			}
		}
	}

	return false;
}

bool ZServer::StartVote(int vote_type, int value, int player)
{
	if((vote_type < 0) || (vote_type >= MAX_VOTE_TYPES))
	{
		return false;
	}

	//other restrictions
	switch(vote_type)
	{
	case CHANGE_MAP_VOTE:
		if(value < 0 || value >= selectable_map_list.size())
		{
			if(player != -1)
			{
				SendNews(player, "invalid map choice, please type /listmaps", 0, 0, 0);
			}
			return false;
		}
		break;
	case START_BOT:
	case STOP_BOT:
		if((value < 0) || (value >= MAX_TEAM_TYPES))
		{
			return false;
		}
		if(value == NULL_TEAM)
		{
			return false;
		}
		break;
	case CHANGE_GAME_SPEED:
		if(!psettings.allow_game_speed_change)
		{
			if(player != -1)
			{
				SendNews(player, "changing the game speed is not allowed on this server", 0, 0, 0);
			}
			return false;
		}
		if(!value || value < 0)
		{
			if(player != -1)
			{
				SendNews(player, "new game speed must be above zero", 0, 0, 0);
			}
			return false;
		}
		break;
	}

	if(player != -1 && !CanVote(player))
	{
		SendNews(player, "you must be logged in to start a vote, please type /help", 0, 0, 0);
		return false;
	}

	if(zvote.VoteInProgress())
	{
		//it the same vote?
		if(vote_type == zvote.GetVoteType() && value == zvote.GetVoteValue())
		{
			VoteYes(player);
		}

		return false;
	}

	if(!VoteRequired() || (player != -1 && player_info[player].real_voting_power() >= VotesNeeded()))
	{
		ProcessVote(vote_type, value);
		return false;
	}

	if(!zvote.StartVote(vote_type, value))
	{
		return false;
	}

	if(player != -1)
	{
		char message[500];
		std::string append_description;

		append_description = VoteAppendDescription();

		if(append_description.length())
		{
			sprintf(message, "vote started by %s to %s: %s", player_info[player].name.c_str(), vote_type_string[vote_type].c_str(), append_description.c_str());
		}
		else
		{
			sprintf(message, "vote started by %s to %s", player_info[player].name.c_str(), vote_type_string[vote_type].c_str());
		}

		BroadCastNews(message, 0, 0, 0);
	}

	ClearPlayerVotes();

	VoteYes(player);

	RelayVoteInfo();
	return true;
}

bool ZServer::VoteYes(int player)
{
	if(!zvote.VoteInProgress())
	{
		return false;
	}

	if(player_info[player].vote_choice != P_NULL_VOTE)
	{
		SendNews(player, "you have already voted", 0, 0, 0);
		return false;
	}

	if(!CanVote(player))
	{
		SendNews(player, "you must be logged in to vote, please type /help", 0, 0, 0);
		return false;
	}

	player_info[player].vote_choice = P_YES_VOTE;

	RelayLPlayerVoteChoice(player);

	CheckVote();

	SendNews(player, "you have voted yes", 0, 0, 0);

	return true;
}

bool ZServer::VoteNo(int player)
{
	if(!zvote.VoteInProgress())
	{
		return false;
	}

	if(player_info[player].vote_choice != P_NULL_VOTE)
	{
		SendNews(player, "you have already voted", 0, 0, 0);
		return false;
	}

	if(!CanVote(player))
	{
		SendNews(player, "you must be logged in to vote, please type /help", 0, 0, 0);
		return false;
	}

	player_info[player].vote_choice = P_NO_VOTE;

	RelayLPlayerVoteChoice(player);

	CheckVote();

	SendNews(player, "you have voted no", 0, 0, 0);

	return true;
}

bool ZServer::VotePass(int player)
{
	if(!zvote.VoteInProgress())
	{
		return false;
	}

	if(player_info[player].vote_choice != P_NULL_VOTE)
	{
		SendNews(player, "you have already voted", 0, 0, 0);
		return false;
	}

	if(!CanVote(player))
	{
		SendNews(player, "you must be logged in to vote, please type /help", 0, 0, 0);
		return false;
	}

	player_info[player].vote_choice = P_PASS_VOTE;

	RelayLPlayerVoteChoice(player);

	CheckVote();

	SendNews(player, "you have passed on voting", 0, 0, 0);

	return true;
}

void ZServer::CheckVoteExpired()
{
	if(!zvote.VoteInProgress())
	{
		return;
	}

	if(zvote.TimeExpired())
	{
		KillVote();
		BroadCastNews("vote has expired", 0, 0, 0);
	}
}

void ZServer::KillVote()
{
	if(!zvote.VoteInProgress())
	{
		return;
	}

	ClearPlayerVotes();
	zvote.ResetVote();
	RelayVoteInfo();
}

void ZServer::CheckVote()
{
	if(!zvote.VoteInProgress())
	{
		return;
	}

	int votes_needed = VotesNeeded();
	if(VotesFor() >= votes_needed)
	{
		ProcessVote(zvote.GetVoteType(), zvote.GetVoteValue());
		KillVote();
	}
	else if(VotesAgainst() >= votes_needed)
	{
		KillVote();
	}
}

void ZServer::ProcessVote(int vote_type, int value)
{
	switch(vote_type)
	{
	case PAUSE_VOTE:
		PauseGame();
		break;
	case RESUME_VOTE:
		ResumeGame();
		break;
	case CHANGE_MAP_VOTE:
		if((value < 0) || (value >= selectable_map_list.size()))
		{
			break;
		}

		DoResetGame(selectable_map_list[value]);
		break;
	case START_BOT:
		if(!bot_thread[value])
		{
			StartBot(value);
		}
		SetTeamsBotsIgnored(value, false);
		break;
	case STOP_BOT:
		SetTeamsBotsIgnored(value, true);
		break;
	case RESET_GAME_VOTE:
		DoResetGame(map_name);
		break;
	case RESHUFFLE_TEAMS_VOTE:
		ReshuffleTeams();
		break;
	case CHANGE_GAME_SPEED:
		if(value <= 0)
		{
			break;
		}
		ChangeGameSpeed(value / 100.0);
		break;
	}
}

bool ZServer::CanVote(int player)
{
	if(psettings.require_login && !player_info[player].logged_in)
	{
		return false;
	}

	return true;
}

void ZServer::RelayVoteInfo(int player)
{
	vote_info_packet packet;
	packet.in_progress = zvote.VoteInProgress();
	packet.vote_type = zvote.GetVoteType();
	packet.value = zvote.GetVoteValue();

	if(player == -1)
	{
		server_socket.SendMessageAll(VOTE_INFO, (const char*)&packet, sizeof(vote_info_packet));
	}
	else
	{
		server_socket.SendMessage(player, VOTE_INFO, (const char*)&packet, sizeof(vote_info_packet));
	}
}

void ZServer::ClearPlayerVotes()
{
	for(int i=0; i<player_info.size(); i++)
	{
		player_info[i].vote_choice = P_NULL_VOTE;

		RelayLPlayerVoteChoice(i);
	}
}

void ZServer::GivePlayerID(int player)
{
	player_id_packet packet;
	packet.p_id = player_info[player].p_id;

	server_socket.SendMessage(player, GIVE_PLAYER_ID, (const char*)&packet, sizeof(player_id_packet));
}

void ZServer::GivePlayerSelectableMapList(int player)
{
	std::string send_str;

	//populate send_str
	for(std::string i : selectable_map_list)
	{
		if(!send_str.length())
		{
			send_str += i;
		}
		else
		{
			send_str += "," + i;
		}
	}

	//send
	if(send_str.length())
	{
		server_socket.SendMessage(player, GIVE_SELECTABLE_MAP_LIST, send_str.c_str(), send_str.length()+1);
	}
	else
	{
		server_socket.SendMessage(player, GIVE_SELECTABLE_MAP_LIST, NULL, 0);
	}
}

bool ZServer::TeamHasBot(int team, bool active_only)
{
	for(p_info i : player_info)
	{
		if(i.team == team && i.mode == BOT_MODE)
		{
			if(active_only && !i.ignored)
			{
				return true;
			}
			else if(!active_only)
			{
				return true;
			}
		}
	}

	return false;
}

void ZServer::SetTeamsBotsIgnored(int team, bool ignored)
{
	if((team < 0) || (team >= MAX_TEAM_TYPES))
	{
		return;
	}

	bool change_happened = false;
	for(int i=0; i<player_info.size();i++)
	{
		p_info &p = player_info[i];

		if(p.team == team && p.mode == BOT_MODE && p.ignored != ignored)
		{
			player_info[i].ignored = ignored;
			RelayLPlayerIgnored(i);

			change_happened = true;
		}
	}

	if(change_happened)
	{
		std::string send_str;

		if(ignored)
		{
			send_str = "the " + team_type_string[team] + " team bot has been stopped";
		}
		else
		{
			send_str = "the " + team_type_string[team] + " team bot has been started";
		}

		BroadCastNews(send_str.c_str());
	}
}

void ZServer::AttemptPlayerLogin(int player, std::string loginname, std::string password)
{
	//we using a database?
	if(!psettings.use_database)
	{
		SendNews(player, "login error: no database used", 0, 0, 0);
		return;
	}

	//player already logged in?
	if(player_info[player].logged_in)
	{
		SendNews(player, "login error: please log off first", 0, 0, 0);
		return;
	}

	if(!COMMON::good_user_string(loginname.c_str()) || !COMMON::good_user_string(password.c_str()))
	{
		char message[500];
		sprintf(message, "login error: only alphanumeric characters and entries under %d characters long allowed", MAX_PLAYER_NAME_SIZE);
		SendNews(player, message, 0, 0, 0);

		return;
	}

	//if using mysql, then attempt to log in
	if(!psettings.use_mysql)
	{
		return;
	}

	ZMysqlUser user;
	std::string err_msg;

	bool user_found;
	if(!zmysql.LoginUser(err_msg, loginname, password, user, user_found))
	{
		spdlog::error("ZServer::AttemptPlayerLogin - MySQL Error - {}", err_msg);
		SendNews(player, "login error: error with the user database", 0, 0, 0);
		return;
	}

	if(!user_found)
	{
		SendNews(player, "login error: invalid login details", 0, 0, 0);
	}

	LoginPlayer(player, user);
}

void ZServer::AttemptCreateUser(int player, std::string username, std::string loginname, std::string password, std::string email)
{
	if(!username.length() || !loginname.length() || !password.length() || !email.length())
	{
		SendNews(player, "create user error: all entries must be filled", 0, 0, 0);
		return;
	}

	if(!psettings.use_database)
	{
		SendNews(player, "create user error: no database used", 0, 0, 0);
		return;
	}

	//player already logged in?
	if(player_info[player].logged_in)
	{
		SendNews(player, "create user error: please log off first", 0, 0, 0);
		return;
	}

	if(!psettings.use_mysql)
	{
		return;
	}

	ZMysqlUser user;
	user.username = username;
	user.loginname = loginname;
	user.password = password;
	user.email = email;
	user.creation_time = time(0);
	user.creation_ip = player_info[player].ip;

	if(!user.format_okay())
	{
		char message[500];
		sprintf(message, "create user error: only alphanumeric characters and entries under %d characters long allowed", MAX_PLAYER_NAME_SIZE);
		SendNews(player, message, 0, 0, 0);

		return;
	}

	bool user_exists;
	std::string err_msg;
	if(!zmysql.CheckUserExistance(err_msg, user, user_exists))
	{
		spdlog::error("ZServer::AttemptCreateUser - MySQL Error - {}", err_msg);
		SendNews(player, "create user error: error with the user database", 0, 0, 0);
		return;
	}

	if(user_exists)
	{
		SendNews(player, "create user error: user already exists", 0, 0, 0);
		return;
	}

	bool user_added;
	if(!zmysql.AddUser(err_msg, user, user_added))
	{
		spdlog::error("ZServer::AttemptCreateUser - MySQL Error - {}", err_msg);
		SendNews(player, "create user error: error with the user database", 0, 0, 0);
		return;
	}
	
	if(!user_added)
	{
		SendNews(player, "create user error: unknown error", 0, 0, 0);
		return;
	}

	AwardAffiliateCreation(user.creation_ip);
	SendNews(player, std::string("user " + user.username + " created").c_str(), 0, 0, 0);
	AttemptPlayerLogin(player, loginname, password);
}

void ZServer::SendPlayerLoginRequired(int player)
{
	loginoff_packet packet;
	if(!psettings.require_login)
	{
		packet.show_login = false;
	}
	else
	{
		packet.show_login = !player_info[player].logged_in;
	}

	server_socket.SendMessage(player, GIVE_LOGINOFF, (const char*)&packet, sizeof(loginoff_packet));
}

void ZServer::MakeAllFortTurretsUnEjectable()
{
	for(ZObject* i : object_list)
	{
		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		if(ot != CANNON_OBJECT)
		{
			continue;
		}

		int x, y;
		i->GetCords(x, y);

		if(AreaIsFortTurret(x / 16, y / 16))
		{
			i->SetEjectableCannon(false);
		}
	}
}

bool ZServer::ChangePlayerTeam(int player, int team)
{
	if((team < 0) || (team >= MAX_TEAM_TYPES))
	{
		return false;
	}

	//set the team
	player_info[player].team = (team_type)team;

	//update player lists
	RelayLPlayerTeam(player);

	//send team data to player
	SendPlayerTeam(player);

	//send update text if team is new
	std::string send_str = "you have been set to the " + team_type_string[player_info[player].team] + " team";
	SendNews(player, send_str.c_str(), 0, 0, 0);

	return true;
}

void ZServer::SendPlayerTeam(int player)
{
	int_packet packet;
	packet.team = player_info[player].team;
	server_socket.SendMessage(player, SET_TEAM, (const char*)&packet, sizeof(int_packet));
}

void ZServer::ReshuffleTeams()
{
	std::vector<int> loggedin_to_be_changed;
	std::vector<int> players_to_be_changed;
	std::vector<int> teams_available;
	std::vector<int> orig_teams_available;

	//collect players that need changed
	for(int i=0; i<player_info.size(); i++)
	{
		if(player_info[i].mode == PLAYER_MODE)
		{
			if(player_info[i].logged_in)
			{
				loggedin_to_be_changed.push_back(i);
			}
			else
			{
				players_to_be_changed.push_back(i);
			}
		}
	}

	//collect available teams
	bool team_found[MAX_TEAM_TYPES];

	//clear
	std::fill(std::begin(team_found), std::end(team_found), false);

	//populate team_won[]
	for(ZObject* obj : object_list)
	{
		unsigned char ot, oid;
		obj->GetObjectID(ot, oid);

		if(!(ot == ROBOT_OBJECT || ot == VEHICLE_OBJECT || ot == CANNON_OBJECT))
		{
			continue;
		}

		team_found[obj->GetOwner()] = true;
	}

	for(int i=RED_TEAM;i<MAX_TEAM_TYPES;i++)
	{
		if(team_found[i] && !TeamHasBot(i, true))
		{
			teams_available.push_back(i);
		}
	}

	if(!loggedin_to_be_changed.size() && !players_to_be_changed.size())
	{
		BroadCastNews("reshuffle teams error: no players to shuffle");
		return;
	}

	if(!teams_available.size())
	{
		BroadCastNews("reshuffle teams error: no available teams to shuffle to");
		return;
	}

	//set this
	orig_teams_available = teams_available;

	//logged in players first
	for(int i : loggedin_to_be_changed)
	{
		player_info[i].team = NULL_TEAM;

		if(!teams_available.size())
		{
			teams_available = orig_teams_available;
		}
		if(!teams_available.size())
		{
			return;
		}

		int random_c = OpenZod::Util::Random::Int(0, teams_available.size() - 1);

		ChangePlayerTeam(i, teams_available[random_c]);

		teams_available.erase(teams_available.begin() + random_c);
	}

	//other players
	for(int i : players_to_be_changed)
	{
		player_info[i].team = NULL_TEAM;

		if(!teams_available.size())
		{
			teams_available = orig_teams_available;
		}
		if(!teams_available.size())
		{
			return;
		}

		int random_c = OpenZod::Util::Random::Int(0, teams_available.size() - 1);

		ChangePlayerTeam(i, teams_available[random_c]);

		teams_available.erase(teams_available.begin() + random_c);
	}

	BroadCastNews("the teams have been reshuffled");
}

void ZServer::CheckPlayerSuggestions()
{
	double the_time = COMMON::current_time();

	if(the_time < next_make_suggestions_time)
	{
		return;
	}

	next_make_suggestions_time = the_time + 30;

	SuggestReshuffleTeams();
}

bool ZServer::SuggestReshuffleTeams()
{
	int players_on_team[MAX_TEAM_TYPES];
	int bots_on_team[MAX_TEAM_TYPES];
	int only_player_team = -1;

	//clear
	std::fill(std::begin(players_on_team), std::end(players_on_team), 0);
	std::fill(std::begin(bots_on_team), std::end(bots_on_team), 0);

	for(int i=0; i<player_info.size(); i++)
	{
		if(player_info[i].ignored)
		{
			continue;
		}

		switch(player_info[i].mode)
		{
			case PLAYER_MODE:
				if(psettings.require_login && player_info[i].logged_in)
				{
					players_on_team[player_info[i].team]++;
				}
				else if(!psettings.require_login)
				{
					players_on_team[player_info[i].team]++;
				}
				break;
			case BOT_MODE:
				bots_on_team[player_info[i].team]++;
				break;
		}
	}

	//find the only player team
	for(int i=RED_TEAM;i<MAX_TEAM_TYPES;i++)
	{
		if(players_on_team[i])
		{
			if(only_player_team == -1)
			{
				only_player_team = i;
			}
			else
			{
				return false;
			}
		}
	}

	//any players on a team?
	if(only_player_team == -1)
	{
		return false;
	}

	//any bots on other teams?
	for(int i=RED_TEAM;i<MAX_TEAM_TYPES;i++)
	{
		if(bots_on_team[i] && i != only_player_team)
		{
			return false;
		}
	}

	//ok we need to suggest either to reshuffle or turn on bots
	if(players_on_team[only_player_team] > 1)
	{
		BroadCastNews("please type in /help to learn about the commands /changeteam, /reshuffleteams, and /startbot");
	}
	else
	{
		BroadCastNews("please type in /help to learn about the command /startbot");
	}

	return true;
}

void ZServer::ChangeGameSpeed(float new_speed)
{
	ztime.SetGameSpeed(new_speed);
	RelayGameSpeed();

	char send_str[1500];
	spdlog::info("Game speed changed to {}", static_cast<int>(ztime.GameSpeed() * 100));
	sprintf(send_str, "game speed changed to %d%%", (int)(ztime.GameSpeed() * 100));

	BroadCastNews(send_str);
}
