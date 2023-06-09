#include "zplayer.h"
#include <math.h>

#include "Util/Random.h"

#include <spdlog/spdlog.h>

void selection_info::DeleteObject(ZObject *obj)
{
	ZObject::RemoveObjectFromList(obj, selected_list);

	for(int i=0;i<10;i++)
	{
		ZObject::RemoveObjectFromList(obj, quick_group[i]);
	}

	SetupGroupDetails(false);
}

void selection_info::RemoveFromSelected(ZObject *obj)
{
	ZObject::RemoveObjectFromList(obj, selected_list);
	SetupGroupDetails(false);
}

bool selection_info::AverageCoordsOfSelected(int &x, int &y)
{
	if(!selected_list.size())
	{
		return false;
	}

	x=0;
	y=0;

	for(ZObject* i : selected_list)
	{
		int cx, cy;
		i->GetCenterCords(cx, cy);

		x+=cx;
		y+=cy;
	}

	x/=selected_list.size();
	y/=selected_list.size();

	return true;
}

bool selection_info::GroupIsSelected(int group)
{
	if(!selected_list.size())
	{
		return false;
	}
	if(selected_list.size() != quick_group[group].size())
	{
		return false;
	}

	for(int i=0;i<selected_list.size();i++)
	{
		if(selected_list[i] != quick_group[group][i])
		{
			return false;
		}
	}

	return true;
}

void selection_info::LoadGroup(int group)
{
	selected_list = quick_group[group];

	//set the group number for all of the units
	for(ZObject* i : selected_list)
	{
		i->SetGroup(group);
	}

	SetupGroupDetails();
}

void selection_info::SetGroup(int group)
{
	std::vector<ZObject*>::iterator i, e;

	//clear out the list as it stands
	i=quick_group[group].begin();
	e=quick_group[group].end();
	for(; i!=e; i++)
	{
		(*i)->SetGroup(-1);
	}
	

	//save this list
	quick_group[group] = selected_list;

	//set the group number for all of the units
	i=selected_list.begin();
	e=selected_list.end();
	for(; i!=e; i++)
	{
		(*i)->SetGroup(group);
	}
}

bool selection_info::UpdateGroupMember(ZObject *obj)
{
	std::vector<ZObject*>::iterator i, e;

	i=selected_list.begin();
	e=selected_list.end();
	for(; i!=e; i++)
	{
		if(obj == *i)
		{
			SetupGroupDetails();
			return true;
		}
	}

	return false;
}

void selection_info::SetupGroupDetails(bool show_waypoints)
{
	have_explosives = false;
	can_pickup_grenades = false;
	can_move = false;
	can_equip = false;
	can_attack = false;

	std::vector<ZObject*>::iterator i=selected_list.begin();
	std::vector<ZObject*>::iterator e=selected_list.end();
	for(; i!=e; i++)
	{
		unsigned char ot, oid;
		
		(*i)->GetObjectID(ot, oid);

		if(ot == ROBOT_OBJECT)
		{
			can_equip = true;
		}
		if(ot != CANNON_OBJECT)
		{
			can_move = true;
		}
		if(ot == VEHICLE_OBJECT && oid == CRANE)
		{
			can_repair = true;
		}
		
		if((*i)->HasExplosives())
		{
			have_explosives = true;
		}
		if((*i)->CanAttack())
		{
			can_attack = true;
		}
		if((*i)->CanBeRepaired())
		{
			can_be_repaired = true;
		}
		if((*i)->CanPickupGrenades())
		{
			can_pickup_grenades = true;
		}

		//we need to reshow their waypoints and this is a good solve all location
		(*i)->ShowWaypoints();
	}
}

bool selection_info::ObjectIsSelected(ZObject *obj)
{
	if(!obj)
	{
		return false;
	}

	for(ZObject* i : selected_list)
	{
		if(i == obj)
		{
			return true;
		}
	}

	return false;
}

ZPlayer::ZPlayer() : ZClient()
{
	gload_thread = 0;
	graphics_loaded = false;
	prev_w = init_w = 800;
	prev_h = init_h = 600;
	mouse_x = 0;
	mouse_y = 0;
	splash_fade = 255;
	player_name = "Player";
	our_mode = PLAYER_MODE;
	hover_object = NULL;
	disable_zcursor = false;
	lshift_down = false;
	rshift_down = false;
	right_down = false;
	left_down = false;
	up_down = false;
	down_down = false;
	lctrl_down = false;
	rctrl_down = false;
	lalt_down = false;
	ralt_down = false;
	pcursor_death_time = 0;
	gui_window = NULL;
	place_cannon = false;
	collect_chat_message = false;
	do_focus_to = false;
	is_windowed = true;
	use_opengl = true;
	loaded_percent = 0;
	show_chat_history = false;
	fort_ref_id = -1;

	ClearAsciiStates();

	select_info.SetZTime(&ztime);
	zhud.SetZTime(&ztime);

	//setup minimap
	zhud.GetMiniMap().Setup(&zmap, &object_list);
	
	//setup events
	SetupEHandler();

	//setup the comp message system
	zcomp_msg.SetObjectList(&object_list);
	zcomp_msg.SetZTime(&ztime);
	
	//give the tcp socket the event list so it can cram in events
	client_socket.SetEventList(&ehandler.GetEventList());
	ZEffect::SetSettings(&zsettings);
	ZEffect::SetEffectList(&new_effect_list);
	ZEffect::SetMap(&zmap);

	//alloc menu memory
	InitMenus();

	//create and setup factory gui
	gui_factory_list = new GWFactoryList(&ztime);
	gui_factory_list->Hide();
	gui_factory_list->SetOList(&ols);
}

void ZPlayer::ProcessResetGame()
{
	//unset this
	fort_ref_id = -1;

	//clear stuff out
	zmap.ClearMap();
	
	//clear object list
	ols.DeleteAllObjects();

	//selection info
	select_info.ClearAll();

	//effect list
	for(ZEffect* e : effect_list)
	{
		delete e;
	}

	effect_list.clear();

	//space bar event list
	space_event_list.clear();

	//no more animals
	ClearAnimals();

	hover_object = NULL;

	//hud
	zhud.ResetGame();

	//gui window ie production
	if(gui_window)
	{
		DeleteCurrentGuiWindow();
	}

	//ask for the map
	client_socket.SendMessage(REQUEST_MAP, NULL, 0);
}

void ZPlayer::SetUseOpenGL(bool use_opengl_)
{
	use_opengl = use_opengl_;
}

void ZPlayer::SetLoginName(std::string login_name_)
{
	login_name = login_name_;
}

void ZPlayer::SetLoginPassword(std::string login_password_)
{
	login_password = login_password_;
}

void ZPlayer::SetWindowed(bool is_windowed_)
{
	is_windowed = is_windowed_;
}

void ZPlayer::DisableCursor(bool disable_zcursor_)
{
	disable_zcursor = disable_zcursor_;
}

void ZPlayer::SetSoundsOff(bool setoff)
{
	ZSDL_SetMusicOn(!setoff);
}

void ZPlayer::SetMusicOff(bool setoff)
{
	ZMusicEngine::SetMusicOn(!setoff);
}

void ZPlayer::SetPlayerTeam(team_type player_team)
{
	cursor.SetTeam(player_team);
	zhud.SetTeam(player_team);
	zcomp_msg.SetTeam(player_team);
	if(gui_factory_list)
	{
		gui_factory_list->SetTeam(player_team);
	}

	RefindOurFortRefID();

	//unselect stuff
	select_info.ClearAll();
	zhud.SetSelectedObject(NULL);
	zhud.ReRenderAll();

	//clear
	space_event_list.clear();

	//reset cursor
	DetermineCursor();
	
	//make the buttons available that let you cycle through your units
	//and set unit amount
	ProcessChangeObjectAmount();
}

void ZPlayer::SetDimensions(int w, int h)
{
	if(w > 0)
	{
		prev_w = init_w = w;
	}
	if(h > 0)
	{
		prev_h = init_h = h;
	}
}

void ZPlayer::Setup()
{
	//randomizer
	SetupRandomizer();

	//registered?
	CheckRegistration();

	if(!client_socket.Start(remote_address.c_str()))
	{
		spdlog::warn("ZPlayer::Setup - Socket not setup");
	}
	
	InitSDL();

	ZMusicEngine::Init();
	ZFontEngine::Init();

	//get it on
	ZMusicEngine::PlaySplashMusic();
	DoSplash();
	if(use_opengl)
	{
		SDL_GL_SwapBuffers();
	}
	else
	{
		SDL_Flip(screen);
	}

	//important to keep the server from crashing us
	ZTeam::Init();
	ZMap::Init();
	zhud.Init();
	ORock::Init();
	SetupSelectionImages();

	gload_thread = SDL_CreateThread(Load_Graphics, this);
}

void ZPlayer::InitSDL()
{
	int audio_rate = 22050;
	Uint16 audio_format = AUDIO_S16; /* 16-bit stereo */
	int audio_channels = 2;
	int audio_buffers = 4096;

	//init SDL
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);

	//some stuff that just has to be right after init
	game_icon = IMG_Load("assets/icon.png");
	//ffuts

	if(game_icon)
	{
		SDL_WM_SetIcon(game_icon, NULL);
	}
	SDL_WM_SetCaption("Zod Engine", "Zod Engine");
	atexit(ZSDL_Quit);
	SDL_EnableUNICODE(SDL_ENABLE);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

#ifdef DISABLE_OPENGL
	use_opengl = false;
#endif

	ZSDL_Surface::SetUseOpenGL(use_opengl);
	ZSDL_Surface::SetScreenDimensions(init_w, init_h);

	if(use_opengl)
	{
		if(is_windowed)
		{
			screen = SDL_SetVideoMode(init_w, init_h, 0, SDL_OPENGL | SDL_RESIZABLE);
		}
		else
		{
			screen = SDL_SetVideoMode(init_w, init_h, 0, SDL_OPENGL | SDL_RESIZABLE | SDL_FULLSCREEN);
		}

		InitOpenGL();
		ResetOpenGLViewPort(init_w, init_h);
	}
	else
	{
		if(is_windowed)
		{
			screen = SDL_SetVideoMode(init_w, init_h, 32, SDL_HWSURFACE|SDL_DOUBLEBUF|SDL_RESIZABLE);
		}
		else
		{
			screen = SDL_SetVideoMode(init_w, init_h, 32, SDL_HWSURFACE|SDL_DOUBLEBUF|SDL_RESIZABLE|SDL_FULLSCREEN);
		}

		ZSDL_Surface::SetMainSoftwareSurface(screen);
	}

	if(!disable_zcursor)
	{
		SDL_ShowCursor(SDL_DISABLE);
	}

	//some initial mouse stuff
	SDL_WM_GrabInput(SDL_GRAB_ON);
	SDL_WarpMouse(init_w>>1, init_h>>1);

	if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) == -1)
	{
		spdlog::error("ZPlayer::InitSDL - Mix_OpenAudio() error");
	}

	Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);
	Mix_Volume(-1, 128);
	Mix_VolumeMusic(80);
	sound_setting = SOUND_100;

	//TTF
	TTF_Init();
	ttf_font = TTF_OpenFont("assets/arial.ttf",10);
	ttf_font_7 = TTF_OpenFont("assets/arial.ttf",7);
	if (!ttf_font)
	{
		spdlog::error("ZPlayer::InitSDL = Could not load assets/arial.ttf");
	}

	//splash sound best loaded here
	splash_screen.LoadBaseImage("assets/splash.bmp");// = IMG_Load("assets/splash.bmp");
	splash_screen.UseDisplayFormat(); //Regular needs this to do fading
}

int ZPlayer::Load_Graphics(void *p)
{
	const int max_items = 81;
	int loaded_items = 0;

	ZCompMessageEngine::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ZSoundEngine::Init(&((ZPlayer*)p)->zmap);
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	if(!((ZPlayer*)p)->disable_zcursor)
	{
		ZCursor::Init();
		((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;
	}
	ZObject::Init(((ZPlayer*)p)->ttf_font_7);
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ZBuilding::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ZCannon::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ZVehicle::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ZRobot::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ABird::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	AHutAnimal::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	BFort::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	BRepair::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	BRadar::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	BRobot::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	BVehicle::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	BBridge::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	OFlag::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	OGrenades::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ORockets::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	OHut::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	OMapObject::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	CGatling::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	CGun::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	CHowitzer::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	CMissileCannon::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	VJeep::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	VLight::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	VMedium::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	VHeavy::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	VAPC::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	VMissileLauncher::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	VCrane::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	RGrunt::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	RPsycho::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	RLaser::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	RPyro::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	RSniper::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	RTough::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ELaser::Init(); 
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EFlame::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EPyroFire::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EToughRocket::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EToughMushroom::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EToughSmoke::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ELightRocket::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ELightInitFire::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EMoMissileRockets::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EMissileCRockets::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ERobotDeath::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ERobotTurrent::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EUnitParticle::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EDeath::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EStandard::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EDeathSparks::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ETurrentMissile::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ESideExplosion::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ECannonDeath::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ERockParticle::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ERockTurrent::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EMapObjectTurrent::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	EBridgeTurrent::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ECraneConco::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ETrack::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ETankDirt::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ETankSmoke::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ETankOil::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ETankSpark::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	GWProduction::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	GWFactoryList::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	GWLogin::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	GWCreateUser::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ZGuiMainMenuBase::Init(); 
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	GMMWButton::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	GMMWList::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	GMMWRadio::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	GMMWTeamColor::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	GMMWTextBox::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ZGuiButton::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ZGuiScrollBar::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ZPortrait::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;

	ZVote::Init();
	((ZPlayer*)p)->loaded_percent = 100 * ++loaded_items / max_items;


	//this should only ever
	//need to be done once...
#ifdef COLLECT_TEAM_COLORS
	ZTeam::SaveAllPalettes();
#endif	

	spdlog::info("ZPlayer::Load_Graphics - Graphics loaded");
	((ZPlayer*)p)->graphics_loaded = true;

	return 1;
}
void ZPlayer::InitAnimals()
{
	const int sq_tile_per_bird = 650;

	ClearAnimals();

	int birds = (zmap.GetMapBasics().height * zmap.GetMapBasics().width) / sq_tile_per_bird;

	for(int i=0;i<birds;i++)
	{
		bird_list.push_back(new ABird(&ztime, &zsettings, (planet_type)zmap.GetMapBasics().terrain_type, zmap.GetMapBasics().width * 16, zmap.GetMapBasics().height * 16));
	}
}

void ZPlayer::ClearAnimals()
{
	bird_list.clear();
}

void ZPlayer::ProcessDisconnect()
{
	AddNewsEntry("Disconnected from the game server, please restart the client.");
}

void ZPlayer::AddNewsEntry(std::string message, int r, int g, int b)
{
	const double lasting_time = 17.0;

	if(!message.length())
	{
		return;
	}

	//alloc
	news_entry* new_entry = new news_entry;

	//extract
	new_entry->r = r;
	new_entry->g = g;
	new_entry->b = b;
	new_entry->message = message;

	//a hack to get software renderer working again...
	//(don't ever allow setting to the invisible "color key" color
	if(!new_entry->r && !new_entry->g && !new_entry->b)
	{
		new_entry->r = 1;
	}

	//make surface
	new_entry->text_image.LoadBaseImage(ZFontEngine::GetFont(SMALL_WHITE_FONT).Render(new_entry->message.c_str()));
	
	//a hack to get software renderer working again...
	new_entry->text_image.MakeAlphable();

	//set death time
	new_entry->death_time = COMMON::current_time() + lasting_time;

	if(new_entry->text_image.GetBaseSurface())
	{
		news_list.insert(news_list.begin(), new_entry);
	}
	else
	{
		spdlog::warn("ZPlayer::AddNewsEntry - Was not about to render new message \"{}\"", new_entry->message);
	}
}

void ZPlayer::DisplayPlayerList()
{
	std::string team_players[MAX_TEAM_TYPES];
	std::string spectators;
	bool bot_player[MAX_TEAM_TYPES];
	bool bot_player_ignored[MAX_TEAM_TYPES];
	char c_message[50];

	if(!player_info.size())
	{
		AddNewsEntry("No one is currently connected");
		return;
	}

	//init
	int tray_players = 0;
	int nobodies = 0;
	for(int i=0;i<MAX_TEAM_TYPES;i++)
	{
		bot_player[i] = false;
		bot_player_ignored[i] = false;
	}

	for(p_info i : player_info)
	{
		switch(i.mode)
		{
		case NOBODY_MODE:
			nobodies++;
			break;
		case PLAYER_MODE:
			if(team_players[i.team].size())
			{
				team_players[i.team] += ", " + i.name;
			}
			else
			{
				team_players[i.team] += i.name;
			}
			break;
		case BOT_MODE:
			bot_player[i.team] = true;
			bot_player_ignored[i.team] = i.ignored;
			break;
		case SPECTATOR_MODE:
			if(spectators.size())
			{
				spectators += ", " + i.name;
			}
			else
			{
				spectators += i.name;
			}
			break;
		case TRAY_MODE:
			tray_players++;
			break;
		}
	}

	AddNewsEntry("--- Players connected ---");
	for(int i=0; i<MAX_TEAM_TYPES; i++)
	{
		if(team_players[i].size() || bot_player[i])
		{
			if(bot_player[i])
			{
				if(bot_player_ignored[i])
				{
					AddNewsEntry(team_type_string[i] + " team -b: " + team_players[i]);
				}
				else
				{
					AddNewsEntry(team_type_string[i] + " team +b: " + team_players[i]);
				}
			}
			else
			{
				AddNewsEntry(team_type_string[i] + " team: " + team_players[i]);
			}
		}
	}

	if(spectators.size())
	{
		AddNewsEntry("spectators: " + spectators);
	}

	if(tray_players)
	{
		sprintf(c_message, "tray players: %d", tray_players);
		AddNewsEntry(c_message);
	}

	if(nobodies)
	{
		sprintf(c_message, "nobodies: %d", nobodies);
		AddNewsEntry(c_message);
	}

	AddNewsEntry("---------------------");

}

void ZPlayer::DisplayFactoryProductionList()
{
	std::vector<ZObject*> fort_list;
	std::vector<ZObject*> robot_list;
	std::vector<ZObject*> vehicle_list;

	AddNewsEntry("------ Factories ------");

	for(ZObject* i : object_list)
	{
		if(i->GetOwner() != our_team) 
		{
			continue;
		}

		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		if(ot != BUILDING_OBJECT)
		{
			continue;
		}

		switch(oid)
		{
		case FORT_FRONT:
		case FORT_BACK:
			fort_list.push_back(i);
			break;
		case ROBOT_FACTORY:
			robot_list.push_back(i);
			break;
		case VEHICLE_FACTORY:
			vehicle_list.push_back(i);
			break;
		}
	}

	for(ZObject* i : fort_list)
	{
		DisplayFactoryProductionListUnit(i);
	}

	for(ZObject* i : robot_list)
	{
		DisplayFactoryProductionListUnit(i);
	}

	for(ZObject* i : vehicle_list)
	{
		DisplayFactoryProductionListUnit(i);
	}

	AddNewsEntry("--------------------");
}

void ZPlayer::DisplayFactoryProductionListUnit(ZObject *obj)
{
	std::string display_msg;
	std::string build_unit;
	char num_c[50];

	if(!obj)
	{
		return;
	}

	unsigned char fot, foid;
	obj->GetObjectID(fot, foid);

	switch(foid)
		{
		case FORT_FRONT:
		case FORT_BACK:
			display_msg = "fort ";
			break;
		case ROBOT_FACTORY:
			display_msg = "robot ";
			break;
		case VEHICLE_FACTORY:
			display_msg = "vehicle ";
			break;
		}

	sprintf(num_c, "L%d: ", obj->GetLevel() + 1);
	display_msg += num_c;

	unsigned char ot, oid;
	obj->GetBuildUnit(ot, oid);

	switch(ot)
	{
	case ROBOT_OBJECT:
		if(oid >= 0 && oid < MAX_ROBOT_TYPES)
		{
			build_unit = robot_type_string[oid];
		}
		break;
	case VEHICLE_OBJECT:
		if(oid >= 0 && oid < MAX_VEHICLE_TYPES)
		{
			build_unit = vehicle_type_string[oid];
		}
		break;
	case CANNON_OBJECT:
		if(oid >= 0 && oid < MAX_CANNON_TYPES)
		{
			build_unit = cannon_type_string[oid];
		}
		break;
	}

	switch(obj->GetBuildState())
	{
	case BUILDING_PLACE:
		display_msg += "placing cannon";
		break;
	case BUILDING_SELECT:
		display_msg += "no production";
		break;
	case BUILDING_BUILDING:
		display_msg += build_unit;
		break;
	case BUILDING_PAUSED:
		display_msg += "paused";
		break;
	}

	AddNewsEntry(display_msg);
}

void ZPlayer::SetupSelectionImages()
{
	SDL_Rect the_box;
	the_box.x = 0;
	the_box.y = 0;
	the_box.w = 2;
	the_box.h = 2;

	for(int t=0;t<MAX_TEAM_TYPES;t++)
	{
		int r = team_color[t].r - (int)(team_color[t].r * 0.2);
		int g = team_color[t].g - (int)(team_color[t].g * 0.2);
		int b = team_color[t].b - (int)(team_color[t].b * 0.2);

		selection_img[t].LoadBaseImage(SDL_CreateRGBSurface(SDL_HWSURFACE | SDL_SRCALPHA, 4, 4, 32, 0xFF000000, 0x0000FF00, 0x00FF0000, 0x000000FF));
		ZSDL_FillRect(&the_box, r, g, b, &selection_img[t]);
	}
}

void ZPlayer::Run()
{
	while(allow_run)
	{
		ztime.UpdateTime();

		//check for tcp events
		ProcessSocketEvents();
		
		//check for sdl events
		ProcessSDL();
		
		//process events
		ehandler.ProcessEvents();
		
		//do stuff
		ProcessGame();
		
		//render
		RenderScreen();
		
		COMMON::uni_pause(10);
	}
}

void ZPlayer::ProcessSocketEvents()
{
	char *message;
	int size;
	int pack_id;

	SocketHandler* shandler = client_socket.GetHandler();
	if(!shandler)
	{
		return;
	}
	
	//not connected, free it up
	if(!shandler->Connected())
	{
		client_socket.ClearConnection();
		
		ehandler.ProcessEvent(OTHER_EVENT, DISCONNECT_EVENT, NULL, 0, 0);
	}
	
	else if(shandler->DoRecv())
	{
		while(shandler->DoFastProcess(&message, &size, &pack_id))
		{
			ehandler.ProcessEvent(TCP_EVENT, pack_id, message, size, 0);
		}
		shandler->ResetFastProcess();
	}
}

void ZPlayer::ProcessGame()
{
	double &the_time = ztime.ztime;
	
	ZMusicEngine::Process(object_list, zmap, our_team, fort_ref_id);

	//fort under attack etc?
	ProcessVerbalWarnings();

	cursor.Process(the_time);
	Pcursor.Process(the_time);

	zcomp_msg.Process(COMMON::current_time());

	zhud.Process(the_time, object_list);
		
	//sort objects
	sort (ols.prender_olist.begin(), ols.prender_olist.end(), sort_objects_func);

 	for(ZObject* i : ols.prender_olist)
	{
		i->ProcessObject();
 		i->Process();
	}

	//kill effects
	for(std::vector<ZEffect*>::iterator i=effect_list.begin(); i!=effect_list.end();)
	{
		if((*i)->KillMe())
		{
			delete *i;
			i=effect_list.erase(i);
		}
		else
		{
			i++;
		}
	}

	//process effects
	for(ZEffect* i : effect_list)
	{
		i->Process();

		if(i->GetEFlags().unit_particles)
		{
			MissileObjectParticles(i->GetEFlags().x, i->GetEFlags().y, i->GetEFlags().unit_particles_radius, i->GetEFlags().unit_particles_amount);
		}
	}

	//the zeffects push new effects into this queue, so we need to move it to the real one
	for(ZEffect* i : new_effect_list)
	{
		effect_list.push_back(i);
	}
	new_effect_list.clear();

	//animals
	for(ZObject* i : bird_list)
	{
		i->Process();
	}

	//gui
	if(gui_window) 
	{
		if(gui_window->KillMe())
		{
			DeleteCurrentGuiWindow();
		}
		else
		{
			gui_window->Process();
		}
	}

	if(gui_factory_list)
	{
		gui_factory_list->Process();
	}

	//main menus
	for(std::vector<ZGuiMainMenuBase*>::iterator i=gui_menu_list.begin();i!=gui_menu_list.end();)
	{
		if((*i)->KillMe())
		{
			delete *i;
			i = gui_menu_list.erase(i);
		}
		else
		{
			++i;
		}
	}

	for(ZGuiMainMenuBase* i : gui_menu_list)
	{
		i->Process();
	}

	//placing a cannon?
	SetPlaceCannonCords();

	//scroll
	ProcessScroll();
	ProcessFocusCamerato();
	
	#ifdef ENABLE_HEATMAP_DEBUG
	//for testing purposes
	gp_heatmap.Process(the_time, ols, zmap, our_team);
	#endif
}

void ZPlayer::ProcessVerbalWarnings()
{
	double &the_time = ztime.ztime;

	static double next_fort_under_attack_msg_time = 0;
	static double next_your_losing_msg_time = 0;

	if(!graphics_loaded)
	{
		return;
	}

	if(team_units_available[our_team] <= 0)
	{
		return;
	}

	//it in fort crazy mode and we need to redisplay the fort msg?
	if(ZMusicEngine::GetDangerLevel() == M_FORT && the_time >= next_fort_under_attack_msg_time)
	{
		next_fort_under_attack_msg_time = the_time + 10;

		ZSoundEngine::PlayWav(COMP_FORT_UNDER_ATTACK);
		zcomp_msg.DisplayMessage(FORT_MSG, fort_ref_id);

		//save space bar event
		AddSpaceBarEvent(SpaceBarEvent(fort_ref_id));
	}

	if(the_time >= next_your_losing_msg_time)
	{
		int next_worst_unit_count = 0;
		double next_worst_territory_percentage;

		int our_unit_count = team_units_available[our_team];
		double our_territory_percentage = team_zone_percentage[our_team];

		//find next worst
		for(int i=RED_TEAM;i<MAX_TEAM_TYPES;i++)
		{
			if(i == our_team)
			{
				continue;
			}
			if(team_units_available[i])
			{
				//first enemy?
				if(!next_worst_unit_count)
				{
					next_worst_unit_count = team_units_available[i];
					next_worst_territory_percentage = team_zone_percentage[i];

					continue;
				}
				
				if(next_worst_unit_count > team_units_available[i])
				{
					next_worst_unit_count = team_units_available[i];
				}

				if(next_worst_territory_percentage > team_zone_percentage[i])
				{
					next_worst_territory_percentage = team_zone_percentage[i];
				}
			}
		}

		our_unit_count *= 1.7;
		our_territory_percentage *= 1.7;

		if(next_worst_unit_count > our_unit_count && 
			next_worst_territory_percentage > our_territory_percentage)
		{
			next_your_losing_msg_time = the_time + 8;
		
			ZSoundEngine::PlayWav(COMP_YOUR_LOSING_0 + OpenZod::Util::Random::Int(0, MAX_COMP_LOSING_MESSAGES - 1));
		}
	}
}

void ZPlayer::PlayBuildingSounds()
{
	bool do_play_radar = false;
	bool do_play_robot = false;

	for(ZObject* i : object_list)
	{
		int x, y, w, h;

		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		if(ot != BUILDING_OBJECT)
		{
			continue;
		}
		if(i->IsDestroyed())
		{
			continue;
		}

		//radar
		if(oid == RADAR && i->GetOwner() != NULL_TEAM)
		{
			i->GetCords(x, y);
			i->GetDimensionsPixel(w, h);

			if(zmap.WithinView(x, y, w, h))
			{
				do_play_radar = true;
			}
		}

		//robot factory
		if(oid == ROBOT_FACTORY || oid == VEHICLE_FACTORY)
		{
			i->GetCords(x, y);
			i->GetDimensionsPixel(w, h);

			if(zmap.WithinView(x, y, w, h))
			{
				if(i->GetBuildState() != BUILDING_SELECT)
				{
					if(i->GetOwner() != NULL_TEAM)
					{
						do_play_robot = true;
					}
				}
			}
		}
	}

	if(do_play_radar)
	{
		ZSoundEngine::RepeatWav(RADAR_SND);
	}
	else
	{
		ZSoundEngine::StopRepeatWav(RADAR_SND);
	}

	if(do_play_robot)
	{
		ZSoundEngine::RepeatWav(ROBOT_FACTORY_SND);
	}
	else
	{
		ZSoundEngine::StopRepeatWav(ROBOT_FACTORY_SND);
	}
}

void ZPlayer::MissileObjectParticles(int x_, int y_, int radius, int particles)
{
	radius *= 0.8;

	for(ZObject* i : object_list)
	{
		int ox, oy;
		int w, h;

		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		if(!(ot == CANNON_OBJECT || ot == VEHICLE_OBJECT || ot == ROBOT_OBJECT))
		{
			continue;
		}

		i->GetCords(ox, oy);
		i->GetDimensionsPixel(w, h);


		if(ox > x_ + radius)
		{
			continue;
		}
		if(ox + w < x_ - radius)
		{
			continue;
		}
		if(oy > y_ + radius)
		{
			continue;
		}
		if(oy + h < y_ - radius)
		{
			continue;
		}

		int particle_amount = 14 + OpenZod::Util::Random::Int(0, particles - 1);

		if(ot == ROBOT_OBJECT)
		{
			particle_amount /= 2;
		}

		for(int i=0;i<particle_amount;i++)
		{
			new_effect_list.push_back((ZEffect*)(new EUnitParticle(&ztime, ox + OpenZod::Util::Random::Int(0, w - 1), oy + OpenZod::Util::Random::Int(0, h -1))));
		}
	}
}

void ZPlayer::RenderScreen()
{
	double &the_time = ztime.ztime;

	if(graphics_loaded && zmap.Loaded())
	{
		//render base
		//we do not keep track if the image is in the map area in opengl
		//so we just render it all full then put the hud back over it
		if(splash_fade >= 5 || use_opengl)
		{
			zhud.ReRenderAll();
		}
		
		zmap.DoRender(screen);
		zmap.DoEffects(the_time, screen);
		RenderSmallMapFiller();

		//draw zones
		zmap.DoZoneEffects(the_time, screen);

		//render objects
		RenderObjects();
		
		#ifdef ENABLE_HEATMAP_DEBUG
		//render test gun placement heatmap 
		gp_heatmap.DoRender(zmap);
		#endif

		//mouse stuff
		RenderMouse();
		RenderPreviousCursor();

		//render gui?
		RenderGUI();

		//place cannon?
		RenderPlaceCannon();

		//vote
		zvote.DoRender(zmap);

		//comp messages
		zcomp_msg.DoRender(zmap, screen);

		//render news
		RenderNews();

		//render a main menu like login
		if(active_menu)
		{
			active_menu->DoRender(zmap, screen);
		}

		//(almost)last because opengl uses it to render over overflows
		zhud.DoRender(screen, init_w, init_h);

		//main menus go over everything
		RenderMainMenu();

		//render cursor
		if(!disable_zcursor) 
		{
			cursor.Render(zmap, screen, mouse_x, mouse_y);
			if(mouse_x > init_w - (HUD_WIDTH + 16) || mouse_y > init_h - (HUD_HEIGHT + 16))
			{
				zhud.ReRenderAll();
			}
		}

		//place some sounds
		PlayBuildingSounds();
	}

	DoSplash();

	if(use_opengl)
	{
		SDL_GL_SwapBuffers();
	}
	else
	{
		SDL_Flip(screen);
	}
}

void ZPlayer::RenderSmallMapFiller()
{
	SDL_Rect box;

	int map_w = zmap.GetMapBasics().width * 16;
	int map_h = zmap.GetMapBasics().height * 16;

	int w_dif = (init_w - HUD_WIDTH) - map_w;
	int h_dif = (init_h - HUD_HEIGHT) - map_h;

	//side need blacked out?
	if(w_dif > 0)
	{
		box.x = map_w;
		box.y = 0;
		box.w = w_dif;
		box.h = init_h - HUD_HEIGHT;

		ZSDL_FillRect(&box, 0, 0, 0);
	}

	//bottom need blacked out?
	if(h_dif > 0)
	{
		box.x = 0;
		box.y = map_h;
		box.w = init_w - HUD_WIDTH;
		box.h = h_dif;

		ZSDL_FillRect(&box, 0, 0, 0);
	}
}

void ZPlayer::RenderMouse()
{
	double &the_time = ztime.ztime;
	static int draw_shift = 0;
	static double next_shift_time = 0;
	const double shift_tick = 0.1;
	SDL_Rect dim;
	SDL_Rect to_rect;
	int shift_x, shift_y;
	int x, y;

	if(lbutton.down && !lbutton.started_over_hud && !lbutton.started_over_gui)
	{
		//move forward
		if(the_time > next_shift_time)
		{
			draw_shift++;
			if(draw_shift>=4)
			{
				draw_shift = 0;
			}

			next_shift_time = the_time + shift_tick;
		}

		zmap.GetViewShift(shift_x, shift_y);
		x = lbutton.map_x - shift_x;
		y = lbutton.map_y - shift_y;

		if(x < mouse_x) 
		{
			dim.x = x;
			dim.w = mouse_x - x;
		}
		else 
		{
			dim.x = mouse_x;
			dim.w = x - mouse_x;
		}

		if(y < mouse_y)
		{
			dim.y = y;
			dim.h = mouse_y - y;
		}
		else 
		{
			dim.y = mouse_y;
			dim.h = y - mouse_y;
		}

		//set max distants we can travel while drawing base on the actual screen
		int max_x = init_w - HUD_WIDTH - 1;
		int max_y = init_h - HUD_HEIGHT - 1;
		int start_x, start_y;

		if(dim.y < 0)
		{
			start_y = 0;
		}
		else
		{
			start_y = dim.y;
		}

		if(dim.x < 0)
		{
			start_x = 0;
		}
		else
		{
			start_x = dim.x;
		}

		//to optimise, force use the var - but set it to the orig if it is within the max's
		if(dim.w+dim.x < max_x)
		{
			max_x = dim.w+dim.x;
		}
		if(dim.h+dim.y < max_y)
		{
			max_y = dim.h+dim.y;
		}

		to_rect.y = dim.y;
		if(to_rect.y < init_h - HUD_HEIGHT - 1 && to_rect.y > 0)
		{
			for(to_rect.x=start_x+(4-draw_shift);to_rect.x<max_x;to_rect.x+=4)
			{
				selection_img[our_team].BlitSurface(NULL, &to_rect);
			}
		}
		
		to_rect.y = dim.y + dim.h;
		if(to_rect.y < init_h - HUD_HEIGHT - 1 && to_rect.y > 0)
		{
			for(to_rect.x=start_x+draw_shift;to_rect.x<max_x;to_rect.x+=4)
			{
				selection_img[our_team].BlitSurface(NULL, &to_rect);
			}
		}

		to_rect.x = dim.x;
		if(to_rect.x < init_w - HUD_WIDTH - 1 && to_rect.x > 0)
		{
			for(to_rect.y=start_y+draw_shift;to_rect.y<max_y;to_rect.y+=4)
			{
				selection_img[our_team].BlitSurface(NULL, &to_rect);
			}
		}

		to_rect.x = dim.x + dim.w;
		if(to_rect.x < init_w - HUD_WIDTH - 1 && to_rect.x > 0)
		{
			for(to_rect.y=start_y+(4-draw_shift);to_rect.y<max_y;to_rect.y+=4)
			{
				selection_img[our_team].BlitSurface(NULL, &to_rect);
			}
		}

	}
}

void ZPlayer::ReSetupButtons()
{
	bool robot_available = false;
	bool vehicle_available = false;
	bool gun_available = false;
	bool building_available = false;
	bool change_made = false;
	int tbut;

	//check button availability
	if(our_team != NULL_TEAM)
	{
		for(ZObject* i : object_list)
		{
			if(i->GetOwner() == our_team)
			{
				unsigned char ot, oid;
				i->GetObjectID(ot, oid);

				switch(ot)
				{
				case BUILDING_OBJECT:
					building_available = true;
					break;
				case CANNON_OBJECT:
					gun_available = true;
					break;
				case VEHICLE_OBJECT:
					vehicle_available = true;
					break;
				case ROBOT_OBJECT:
					robot_available = true;
					break;
				}
			}
		}
	}

	//set button availability
	tbut = B_BUTTON;
	if(building_available)
	{
		if(zhud.GetButton(tbut).CurrentState() == B_INACTIVE)
		{
			zhud.GetButton(tbut).SetState(B_ACTIVE);
			change_made = true;
		}
	}
	else
	{
		if(zhud.GetButton(tbut).CurrentState() != B_INACTIVE)
		{
			zhud.GetButton(tbut).SetState(B_INACTIVE);
			change_made = true;
		}
	}

	tbut = G_BUTTON;
	if(gun_available)
	{
		if(zhud.GetButton(tbut).CurrentState() == B_INACTIVE)
		{
			zhud.GetButton(tbut).SetState(B_ACTIVE);
			change_made = true;
		}
	}
	else
	{
		if(zhud.GetButton(tbut).CurrentState() != B_INACTIVE)
		{
			zhud.GetButton(tbut).SetState(B_INACTIVE);
			change_made = true;
		}
	}

	tbut = V_BUTTON;
	if(vehicle_available)
	{
		if(zhud.GetButton(tbut).CurrentState() == B_INACTIVE)
		{
			zhud.GetButton(tbut).SetState(B_ACTIVE);
			change_made = true;
		}
	}
	else
	{
		if(zhud.GetButton(tbut).CurrentState() != B_INACTIVE)
		{
			zhud.GetButton(tbut).SetState(B_INACTIVE);
			change_made = true;
		}
	}

	tbut = R_BUTTON;
	if(robot_available)
	{
		if(zhud.GetButton(tbut).CurrentState() == B_INACTIVE)
		{
			zhud.GetButton(tbut).SetState(B_ACTIVE);
			change_made = true;
		}
	}
	else
	{
		if(zhud.GetButton(tbut).CurrentState() != B_INACTIVE)
		{
			zhud.GetButton(tbut).SetState(B_INACTIVE);
			change_made = true;
		}
	}

	if(change_made)
	{
		zhud.ReRenderAll();
	}
}

void ZPlayer::HandleButton(hud_buttons button)
{
	switch(button)
	{
	case A_BUTTON: 
		A_Button();
		break;
	case B_BUTTON:
		B_Button();
		break;
	case D_BUTTON:
		D_Button();
		break;
	case G_BUTTON:
		G_Button();
		break;
	case MENU_BUTTON:
		Menu_Button();
		break;
	case R_BUTTON:
		R_Button();
		break;
	case T_BUTTON:
		T_Button();
		break;
	case V_BUTTON:
		V_Button();
		break;
	case Z_BUTTON:
		Z_Button();
		break;
	}
}

void ZPlayer::A_Button()
{

}

void ZPlayer::B_Button()
{
	if(gui_factory_list)
	{
		gui_factory_list->ToggleShow();
	}
}

void ZPlayer::D_Button()
{

}

void ZPlayer::G_Button()
{
	OrderlySelectUnitType(CANNON_OBJECT);
}

void ZPlayer::Menu_Button()
{
	LoadMainMenu(GMM_MAIN_MAIN);
}

void ZPlayer::R_Button()
{
	OrderlySelectUnitType(ROBOT_OBJECT);
}

void ZPlayer::T_Button()
{

}

void ZPlayer::V_Button()
{
	OrderlySelectUnitType(VEHICLE_OBJECT);
}

void ZPlayer::Z_Button()
{

}

void ZPlayer::OrderlySelectUnitType(int type)
{
	static double last_time = -100;
	double the_time = COMMON::current_time();
	static int last_ref_id_default = -1;
	static int last_ref_id_cannon = -1;
	static int last_ref_id_robot = -1;
	static int last_ref_id_vehicle = -1;
	int *last_ref_id = &last_ref_id_default;

	if(our_team == NULL_TEAM)
	{
		return;
	}

	//different id for each type
	switch(type)
	{
		case CANNON_OBJECT:
			last_ref_id=&last_ref_id_cannon;
			break;
		case ROBOT_OBJECT:
			last_ref_id=&last_ref_id_robot;
			break;
		case VEHICLE_OBJECT:
			last_ref_id=&last_ref_id_vehicle;
			break;
	}

	ZObject* the_choice = NULL;

	//do next in line like normal
	if(the_time - last_time < 7)
	{
		the_choice = ZObject::NextSelectableObjectAboveID(object_list, type, our_team, *last_ref_id);
		if(!the_choice)
		{
			the_choice = ZObject::NextSelectableObjectAboveID(object_list, type, our_team, -1);
		}
	}
	else
	{
		//choose the nearest to mouse
		int map_x, map_y;
		zmap.GetMapCoords(mouse_x, mouse_y, map_x, map_y);

		the_choice = ZObject::NearestSelectableObject(object_list, type, our_team, map_x, map_y);
	}

	//make the selection
	if(!the_choice)
	{
		return;
	}

	select_info.Clear();
	select_info.selected_list.push_back(the_choice);
	select_info.SetupGroupDetails();
	DetermineCursor();
	GiveHudSelected();

	int ox, oy;
	the_choice->GetCenterCords(ox, oy);
	FocusCameraTo(ox, oy);

	*last_ref_id = the_choice->GetRefID();
	last_time = the_time;
}

void ZPlayer::RandomlySelectUnitType(int type)
{
	std::vector<ZObject*> temp_choice_list;
	ZObject* the_choice = NULL;

	if(our_team == NULL_TEAM)
	{
		return;
	}

	for(ZObject* i : object_list)
	{
		if(i->GetOwner() == our_team)
		{
			unsigned char ot, oid;
			i->GetObjectID(ot, oid);

			if(ot == type)
			{
				temp_choice_list.push_back(i);
			}
		}
	}

	if(temp_choice_list.size())
	{
		the_choice = temp_choice_list[OpenZod::Util::Random::Int(0, temp_choice_list.size() - 1)];
	}

	if(the_choice)
	{
		select_info.Clear();
		select_info.selected_list.push_back(the_choice);
		select_info.SetupGroupDetails();
		DetermineCursor();
		GiveHudSelected();

		int ox, oy;
		the_choice->GetCenterCords(ox, oy);
		FocusCameraTo(ox, oy);
	}
}

void ZPlayer::FocusCameraToFort()
{
	int shift_x, shift_y, view_w, view_h;
	int goto_x, goto_y;

	if(!zmap.Loaded())
	{
		return;
	}
	if(our_team == NULL_TEAM)
	{
		return;
	}

	//find our fort
	for(ZObject* i : object_list)
	{
		if(i->GetOwner() != our_team)
		{
			continue;
		}

		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		if(ot != BUILDING_OBJECT)
		{
			continue;
		}
		if(!(oid == FORT_FRONT || oid == FORT_BACK))
		{
			continue;
		}

		zmap.GetViewShiftFull(shift_x, shift_y, view_w, view_h);

		i->GetCenterCords(goto_x, goto_y);

		goto_x = goto_x - (view_w >> 1);
		goto_y = goto_y - (view_w >> 1);

		zmap.SetViewShift(goto_x, goto_y);
		return;
	}
}

void ZPlayer::FocusCameraTo(int map_x, int map_y)
{
	int shift_x, shift_y, view_w, view_h;

	zmap.GetViewShiftFull(shift_x, shift_y, view_w, view_h);

	//set desired location
	focus_to_x = map_x - (view_w >> 1);
	focus_to_y = map_y - (view_h >> 1);

	//are we already there?
	if(shift_x == focus_to_x && shift_y == focus_to_y)
	{
		return;
	}

	//set original distance
	double dx = focus_to_x - shift_x;
	double dy = focus_to_y - shift_y;
	focus_to_original_distance = sqrt((dx *dx) + (dy * dy));

	last_focus_to_time = COMMON::current_time();

	final_focus_to_time = last_focus_to_time + 0.7;

	do_focus_to = true;
}

void ZPlayer::ProcessFocusCamerato()
{
	int shift_x, shift_y, view_w, view_h;
	bool shifted_x, shifted_y;

	if(!do_focus_to)
	{
		return;
	}

	zmap.GetViewShiftFull(shift_x, shift_y, view_w, view_h);

	double dx = focus_to_x - shift_x;
	double dy = focus_to_y - shift_y;

	//exit conditions
	if((!dx && !dy))
	{
		do_focus_to = false;
		return;
	}

	//normal move
	double end_dx = dx * 0.1;
	double end_dy = dy * 0.1;

	if(!((int)end_dx) && dx)
	{
		if(dx > 0)
		{
			end_dx = 1;
		}
		else
		{
			end_dx = -1;
		}
	}

	if(!((int)end_dy) && dy)
	{
		if(dy > 0)
		{
			end_dy = 1;
		}
		else
		{
			end_dy = -1;
		}
	}
	
	if(end_dx > 0)
	{
		shifted_x = zmap.ShiftViewRight((int)end_dx);
	}
	else
	{
		shifted_x = zmap.ShiftViewLeft((int)-end_dx);
	}

	if(end_dy > 0)
	{
		shifted_y = zmap.ShiftViewDown((int)end_dy);
	}
	else
	{
		shifted_y = zmap.ShiftViewUp((int)-end_dy);
	}

	if(!shifted_x && !shifted_y)
	{
		do_focus_to = false;
	}
	else if(!shifted_x && !dy)
	{
		do_focus_to = false;
	}
	else if(!shifted_y && !dx)
	{
		do_focus_to = false;
	}
}

void ZPlayer::StartMouseScrolling(int new_mouse_x, int new_mouse_y)
{
	if(SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_OFF)
	{
		return;
	}

	if(!(mouse_x < 10) && (new_mouse_x < 10)) 
	{
		horz_scroll_over = 0;
		last_horz_scroll_time = COMMON::current_time();
	}
	else if(!(mouse_x > screen->w - 10) && (new_mouse_x > screen->w - 10)) 
	{
		horz_scroll_over = 0;
		last_horz_scroll_time = COMMON::current_time();
	}

	if(!(mouse_y < 10) && (new_mouse_y < 10)) 
	{
		vert_scroll_over = 0;
		last_vert_scroll_time = COMMON::current_time();
	}
	else if(!(mouse_y > screen->h - 10) && (new_mouse_y > screen->h - 10)) 
	{
		vert_scroll_over = 0;
		last_vert_scroll_time = COMMON::current_time();
	}
}

bool ZPlayer::DoMouseScrollRight()
{
	return (mouse_x > screen->w - 10 && SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON);
}

bool ZPlayer::DoMouseScrollLeft()
{
	return (mouse_x < 10 && SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON);
}

bool ZPlayer::DoMouseScrollUp()
{
	return (mouse_y < 10 && SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON);
}

bool ZPlayer::DoMouseScrollDown()
{
	return (mouse_y > screen->h - 10 && SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON);
}

bool ZPlayer::DoKeyScrollRight()
{
	return (right_down && !left_down);
}

bool ZPlayer::DoKeyScrollLeft()
{
	return (!right_down && left_down);
}

bool ZPlayer::DoKeyScrollUp()
{
	return (up_down && !down_down);
}

bool ZPlayer::DoKeyScrollDown()
{
	return (!up_down && down_down);
}

void ZPlayer::ProcessScroll()
{
	double the_time = COMMON::current_time();
	const double shift_speed = 400;
	double time_diff;
	double the_shift;

	//no scrolling when we are
	//currently flying towards a location
	//(such as when jumping to a selected unit)
	if(do_focus_to)
	{
		return;
	}

	//dont scroll if it isn't all loaded
	if(!graphics_loaded)
	{
		return;
	}

	if(DoKeyScrollUp() || DoMouseScrollUp())
	{
		time_diff = the_time - last_vert_scroll_time;

		the_shift = time_diff * shift_speed;
		the_shift += vert_scroll_over;

		if(the_shift >= 1)
		{
			last_vert_scroll_time = the_time;
			vert_scroll_over = the_shift - (int)the_shift;
			zmap.ShiftViewUp((int)the_shift);
		}
	}
	else if(DoKeyScrollDown() || DoMouseScrollDown())
	{
		time_diff = the_time - last_vert_scroll_time;

		the_shift = time_diff * shift_speed;
		the_shift += vert_scroll_over;

		if(the_shift >= 1)
		{
			last_vert_scroll_time = the_time;
			vert_scroll_over = the_shift - (int)the_shift;
			zmap.ShiftViewDown((int)the_shift);
		}
	}

	if(DoKeyScrollRight() || DoMouseScrollRight())
	{
		time_diff = the_time - last_horz_scroll_time;

		the_shift = time_diff * shift_speed;
		the_shift += horz_scroll_over;

		if(the_shift >= 1)
		{
			last_horz_scroll_time = the_time;
			horz_scroll_over = the_shift - (int)the_shift;
			zmap.ShiftViewRight((int)the_shift);
		}
	}
	else if(DoKeyScrollLeft() || DoMouseScrollLeft())
	{
		time_diff = the_time - last_horz_scroll_time;

		the_shift = time_diff * shift_speed;
		the_shift += horz_scroll_over;

		if(the_shift >= 1)
		{
			last_horz_scroll_time = the_time;
			horz_scroll_over = the_shift - (int)the_shift;
			zmap.ShiftViewLeft((int)the_shift);
		}
	}
}

void ZPlayer::RenderPreviousCursor()
{
	double &the_time = ztime.ztime;

	if(the_time < pcursor_death_time)
	{
		Pcursor.Render(zmap, screen, pcursor_x, pcursor_y, true);
	}
}

void ZPlayer::SetPcursor()
{
	switch(cursor.GetCursor())
	{
	case PLACE_C:
		Pcursor.SetCursor(PLACED_C);
		break;
	case ATTACK_C:
		Pcursor.SetCursor(ATTACKED_C);
		break;
	case GRAB_C:
		Pcursor.SetCursor(GRABBED_C);
		break;
	case GRENADE_C:
		Pcursor.SetCursor(GRENADED_C);
		break;
	case REPAIR_C:
		Pcursor.SetCursor(REPAIRED_C);
		break;
	case ENTER_C:
		Pcursor.SetCursor(ENTERED_C);
		break;
	case EXIT_C:
		Pcursor.SetCursor(EXITED_C);
		break;
	case CANNON_C:
		Pcursor.SetCursor(CANNONED_C);
		break;
	default:
		Pcursor.SetCursor(PLACED_C);
		break;
	}
}

void ZPlayer::ShowPcursor(int mx, int my)
{
	double &the_time = ztime.ztime;

	pcursor_death_time = the_time + 3.0;
	pcursor_x = mx;
	pcursor_y = my;
}

void ZPlayer::RenderNews()
{
	double the_time = COMMON::current_time();
	const int y_int = 15;
	const double start_fade_time = 5;
	const int max_news_history = 50;
	double time_left;
	SDL_Rect from_rect, to_rect;

	int max_news_width = init_w - (5 + 100);

	from_rect.x = 0;
	from_rect.y = 0;
	from_rect.h = 40;
	to_rect.x = 5;
	to_rect.y = init_h - (36 + y_int);
	to_rect.w = 0;
	to_rect.h = 0;

	if(gui_factory_list && gui_factory_list->IsVisible())
	{
		to_rect.x += 142;
	}

	//cut down to size
	while(news_list.size() > max_news_history)
	{
		delete news_list.back();
		news_list.pop_back();
	}

	for(std::vector<news_entry*>::iterator i=news_list.begin(); i!=news_list.end();)
	{
		news_entry *cur_entry = *i;

		time_left = cur_entry->death_time - the_time;

		//do not draw this news piece?
		if(!show_chat_history && time_left <= 0)
		{
			i++;
			continue;
		}

		//start fading?
		if(!show_chat_history && time_left < start_fade_time)
		{
			double fade_alpha = (time_left / start_fade_time) * 255;
			cur_entry->text_image.SetAlpha(fade_alpha);
		}
		else
		{
			cur_entry->text_image.SetAlpha(255);
		}

		//make sure it does not draw over the hud

		from_rect.w = max_news_width;
		cur_entry->text_image.BlitSurface(&from_rect, &to_rect);

		to_rect.y -= y_int;
		i++;
	}
}

void ZPlayer::RenderObjects()
{
	//draw effects pre stuff
	for(ZEffect* i : effect_list)
	{
		i->DoPreRender(zmap, screen);
	}

	//draw objects pre stuff
	for(ZObject* i : ols.prender_olist)
	{
		i->DoPreRender(zmap, screen);
	}

	//draw rallypoints of "selected" building
	if(gui_window && gui_window->GetBuildingObj())
	{
		gui_window->GetBuildingObj()->DoRenderWaypoints(zmap, screen, object_list, true);
	}
	
	//draw object's waypoints
	for(ZObject* i : ols.non_mapitem_olist)
	{
		i->DoRenderWaypoints(zmap, screen, object_list);
	}
	
	//draw objects
	for(ZObject* i : ols.prender_olist)
	{
		i->DoRender(zmap, screen);
	}
	
	//draw after effects
	for(ZObject* i : ols.prender_olist)
	{
		i->DoAfterEffects(zmap, screen);
	}
	
	//effects
	for(ZEffect* i : effect_list)
	{
		i->DoRender(zmap, screen);
	}

	//animals
	for(ZObject* i : bird_list)
	{
		i->DoRender(zmap, screen);
	}
	
	//draw selection stuff
	for(ZObject* i : select_info.selected_list)
	{
		i->RenderSelection(zmap, screen);
		i->RenderAttackRadius(zmap, screen, select_info.selected_list);
	}
	
	//draw hover names
	if(hover_object)
	{
		//if it has a leader render it instead
		if(hover_object->GetGroupLeader())
		{
			hover_object->GetGroupLeader()->RenderHover(zmap, screen, our_team);
		}
		else
		{
			hover_object->RenderHover(zmap, screen, our_team);
		}
	}
}

void ZPlayer::RenderGUI()
{
	if(gui_window)
	{
		gui_window->DoRender(zmap, screen);
	}

	if(gui_factory_list)
	{
		gui_factory_list->DoRender(zmap, screen);
	}
}

void ZPlayer::RenderMainMenu()
{
	//render so first in list is rendered last
	for(int i=gui_menu_list.size()-1;i>=0;i--)
	{
		gui_menu_list[i]->DoRender(zmap, screen);
	}
}

void ZPlayer::DetermineCursor()
{
	//only one cursor when we are selecting
	if(lbutton.down)
	{
		cursor.SetCursor(CURSOR_C);
	}

	else if(select_info.selected_list.size())
	{
		if(hover_object)
		{
			unsigned char ot, oid;
			hover_object->GetObjectID(ot, oid);

			if(select_info.can_repair && hover_object->CanBeRepairedByCrane(our_team))
			{
				cursor.SetCursor(REPAIR_C);
			}
			else if(select_info.can_be_repaired && hover_object->CanRepairUnit(our_team))
			{
				cursor.SetCursor(REPAIR_C);
			}
			else if(hover_object->GetOwner() != our_team)
			{
				if(select_info.can_move)
				{
					if(ot == MAP_ITEM_OBJECT && oid == GRENADES_ITEM && select_info.can_pickup_grenades)
					{
						cursor.SetCursor(GRAB_C);
					}
					else if(ot == MAP_ITEM_OBJECT && oid == FLAG_ITEM && select_info.can_move)
					{
						cursor.SetCursor(GRAB_C);
					}
					else if((ot == CANNON_OBJECT || ot == VEHICLE_OBJECT) && hover_object->GetOwner() == NULL_TEAM && select_info.can_equip)
					{
						cursor.SetCursor(ENTER_C);
					}
					else
					{
						if(hover_object_can_enter_fort)
						{
							cursor.SetCursor(PLACE_C);
						}
						else
						{
							if(!select_info.can_attack || (!select_info.have_explosives && hover_object->AttackedOnlyByExplosives()))
							{
								cursor.SetCursor(NONO_C);
							}
							else
							{
								cursor.SetCursor(ATTACK_C);
							}
						}
					}
				}
				else
				{
					//we have an object under us, but we only have cannons selected
					if(!(ot == MAP_ITEM_OBJECT && oid == FLAG_ITEM))
					{
						if(hover_object_can_enter_fort)
							cursor.SetCursor(PLACE_C);
						else
						{
							if(!select_info.can_attack || (!select_info.have_explosives && hover_object->AttackedOnlyByExplosives()))
							{
								cursor.SetCursor(NONO_C);
							}
							else
							{
								cursor.SetCursor(ATTACK_C);
							}
						}
					}
					else
					{
						cursor.SetCursor(CANNON_C);
					}
				}

			}
			else
			{
				int i = GetObjectIndex(hover_object, select_info.selected_list);

				if(hover_object->CanEjectDrivers() && select_info.selected_list.size() == 1 && i!= -1)
				{
					cursor.SetCursor(EXIT_C);
				}
				else
				{
					cursor.SetCursor(PLACE_C);
				}
			}
		}
		else
		{
			if(select_info.can_move)
			{
				cursor.SetCursor(PLACE_C);
			}
			else
			{
				cursor.SetCursor(CANNON_C);
			}
		}
	}
	else
	{
		//normal cursor
		cursor.SetCursor(CURSOR_C);
	}
}

void ZPlayer::ExitProgram()
{
	Mix_CloseAudio();
	exit(0);
}

void ZPlayer::ProcessSDL()
{
	SDL_Event event;
	key_event the_key;
	int shift_x, shift_y;
	
	while(SDL_PollEvent(&event))
	{
		switch(event.type) 
		{
		case SDL_QUIT:
			ExitProgram();
			break;
		case SDL_VIDEORESIZE:
			init_w = event.resize.w;
			init_h = event.resize.h;
			ehandler.ProcessEvent(SDL_EVENT, RESIZE_EVENT, NULL, 0, 0);
			break;
		case SDL_MOUSEMOTION:
			StartMouseScrolling(event.motion.x, event.motion.y);
			mouse_x = event.motion.x;
			mouse_y = event.motion.y;
			ehandler.ProcessEvent(SDL_EVENT, MOTION_EVENT, NULL, 0, 0);
			break;
		case SDL_MOUSEBUTTONDOWN:
			zmap.GetViewShift(shift_x, shift_y);
			switch(event.button.button)
			{
			case SDL_BUTTON_LEFT:
				lbutton.x = event.button.x;
				lbutton.y = event.button.y;
				lbutton.map_x = lbutton.x + shift_x;
				lbutton.map_y = lbutton.y + shift_y;
				ehandler.ProcessEvent(SDL_EVENT, LCLICK_EVENT, NULL, 0, 0);
				break;
			case SDL_BUTTON_RIGHT:
				rbutton.x = event.button.x;
				rbutton.y = event.button.y;
				rbutton.map_x = rbutton.x + shift_x;
				rbutton.map_y = rbutton.y + shift_y;
				ehandler.ProcessEvent(SDL_EVENT, RCLICK_EVENT, NULL, 0, 0);
				break;
			case SDL_BUTTON_MIDDLE:
				mbutton.x = event.button.x;
				mbutton.y = event.button.y;
				mbutton.map_x = ((init_w - HUD_WIDTH) >> 1) + shift_x;
				mbutton.map_y = ((init_h - HUD_HEIGHT) >> 1) + shift_y;
				ehandler.ProcessEvent(SDL_EVENT, MCLICK_EVENT, NULL, 0, 0);
				break;
			case SDL_BUTTON_WHEELUP:
				ehandler.ProcessEvent(SDL_EVENT, WHEELUP_EVENT, NULL, 0, 0);
				break;
			case SDL_BUTTON_WHEELDOWN:
				ehandler.ProcessEvent(SDL_EVENT, WHEELDOWN_EVENT, NULL, 0, 0);
				break;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			switch(event.button.button)
			{
			case SDL_BUTTON_LEFT:
				ehandler.ProcessEvent(SDL_EVENT, LUNCLICK_EVENT, NULL, 0, 0);
				break;
			case SDL_BUTTON_RIGHT:
				ehandler.ProcessEvent(SDL_EVENT, RUNCLICK_EVENT, NULL, 0, 0);
				break;
			case SDL_BUTTON_MIDDLE:
				ehandler.ProcessEvent(SDL_EVENT, MUNCLICK_EVENT, NULL, 0, 0);
				break;
			}
			break;
		case SDL_KEYDOWN:
			the_key.the_key = event.key.keysym.sym;
			the_key.the_unicode = event.key.keysym.unicode;
			ehandler.ProcessEvent(SDL_EVENT, KEYDOWN_EVENT_, (char*)&the_key, sizeof(key_event), 0);
			break;
		case SDL_KEYUP:
			the_key.the_key = event.key.keysym.sym;
			the_key.the_unicode = event.key.keysym.unicode;
			ehandler.ProcessEvent(SDL_EVENT, KEYUP_EVENT_, (char*)&the_key, sizeof(key_event), 0);
			break;
		}
	}
}

void ZPlayer::DoSplash()
{
	const float fade_per_second = 5;
	static double last_time;
	static bool did_init = false;

	if(!splash_screen.GetBaseSurface())
	{
		return;
	}

	//draw?
	if(splash_fade >= 5)
	{
		SDL_Rect to_rect;
		
		//decrease fade?
		if(graphics_loaded)// && zmap.Loaded())
		{
			if(!did_init)
			{
				last_time = COMMON::current_time();
				did_init = true;
			}
			
			splash_fade -= (float)(COMMON::current_time() - last_time) * fade_per_second;
			if(splash_fade < 0)
			{
				splash_fade = 0;
			}
			
			switch(sound_setting)
			{
			case SOUND_25:
				Mix_VolumeMusic((80 / 4) * splash_fade / 255);
				break;
			case SOUND_50:
				Mix_VolumeMusic((80 / 2) * splash_fade / 255);
				break;
			case SOUND_75:
				Mix_VolumeMusic((80 * 3 / 4) * splash_fade / 255);
				break;
			case SOUND_100:
				Mix_VolumeMusic((80) * splash_fade / 255);
				break;
			}

			splash_screen.SetAlpha(splash_fade);
		}
		
		//render
		to_rect.x = (init_w - splash_screen.GetBaseSurface()->w) >> 1;
		to_rect.y = (init_h - splash_screen.GetBaseSurface()->h) >> 1;

		splash_screen.BlitSurface(NULL, &to_rect);


		//what the fuck loading
		{
			ZSDL_Surface loading_text;
			char loading_c[500];

			if(loaded_percent > 100) loaded_percent = 100;
			sprintf(loading_c,"LOADING %d%c", loaded_percent, '%');

			loading_text.LoadBaseImage(ZFontEngine::GetFont(LOADING_WHITE_FONT).Render(loading_c));
			loading_text.MakeAlphable();
			loading_text.SetAlpha(splash_fade / 1.5);
			if(loading_text.GetBaseSurface())
			{
				to_rect.x += 430;
				to_rect.y += 300;
				loading_text.BlitSurface(NULL, &to_rect);
			}
		}
	}

	//load the normal music
	else
	{
		ZMusicEngine::PlayPlanetMusic(zmap.GetMapBasics().terrain_type);
		switch(sound_setting)
		{
		case SOUND_25:
			Mix_VolumeMusic((80 / 4));
			break;
		case SOUND_50:
			Mix_VolumeMusic((80 / 2));
			break;
		case SOUND_75:
			Mix_VolumeMusic((80 * 3 / 4));
			break;
		case SOUND_100:
			Mix_VolumeMusic((80));
			break;
		}
	}
}

void ZPlayer::SelectZObject(ZObject *obj)
{
	if(!obj)
	{
		return;
	}

	//select leader instead
	if(obj->GetGroupLeader())
	{
		obj = obj->GetGroupLeader();
	}

	if(!obj->Selectable())
	{
		return;
	}
	if(obj->GetOwner() != our_team)
	{
		return;
	}

	select_info.Clear();

	select_info.selected_list.push_back(obj);
	select_info.SetupGroupDetails();

	DetermineCursor();
	GiveHudSelected();
	ClearDevWayPointsOfSelected();
}

void ZPlayer::SelectAllOfType(int type)
{
	if(our_team == NULL_TEAM)
	{
		return;
	}
	if(type != -1 && !(type == ROBOT_OBJECT || type == VEHICLE_OBJECT || type == CANNON_OBJECT))
	{
		return;
	}

	select_info.Clear();

	for(ZObject* i : ols.passive_engagable_olist)
	{
		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		if(type != -1)
		{
			if(ot != type)
			{
				continue;
			}
		}
		else
		{
			if(ot != ROBOT_OBJECT && ot != VEHICLE_OBJECT)
			{
				continue;
			}
		}
		if(i->GetOwner() != our_team)
		{
			continue;
		}
		if(!i->Selectable())
		{
			continue;
		}

		select_info.selected_list.push_back(i);
	}

	select_info.SetupGroupDetails();

	DetermineCursor();
	GiveHudSelected();
}

void ZPlayer::CollectSelectables()
{
	int shift_x, shift_y;
	int mouse_x_map, mouse_y_map;
	int map_left, map_right, map_top, map_bottom;

	if(!lbutton.down)
	{
		return;
	}

	select_info.Clear();

	zmap.GetViewShift(shift_x, shift_y);

	mouse_x_map = mouse_x + shift_x;
	mouse_y_map = mouse_y + shift_y;

	if(mouse_x_map < lbutton.map_x)
	{
		map_left = mouse_x_map;
		map_right = lbutton.map_x;
	}
	else
	{
		map_left = lbutton.map_x;
		map_right = mouse_x_map;
	}

	if(mouse_y_map < lbutton.map_y)
	{
		map_top = mouse_y_map;
		map_bottom = lbutton.map_y;
	}
	else
	{
		map_top = lbutton.map_y;
		map_bottom = mouse_y_map;
	}

	//are we only selecting one unit?
	if(abs(lbutton.map_x - mouse_x_map) <= 1 && abs(lbutton.map_y - mouse_y_map) <= 1)
	{
		std::vector<ZObject*> choice_list;

		for(std::vector<ZObject*>::iterator i=object_list.begin(); i!=object_list.end(); i++)
		{
			ZObject* pot_obj = *i;

			//ordering of checks is important, arg
			if(pot_obj->GetOwner() != our_team)
			{
				continue;
			}
			if(!pot_obj->WithinSelection(map_left, map_right, map_top, map_bottom))
			{
				continue;
			}

			//if it is a minion, select its leader for a potential selection choice
			if(pot_obj->GetGroupLeader())
			{
				pot_obj = pot_obj->GetGroupLeader();
			}

			if(!pot_obj->Selectable())
			{
				continue;
			}

			//it already in the list?
			for(ZObject* j : choice_list)
			{
				if(j == pot_obj) 
				{
					pot_obj = NULL;
					break;
				}
			}

			//put it in the list
			if(pot_obj)
			{
				choice_list.push_back(pot_obj);
			}
		}

		if(choice_list.size())
		{
			select_info.selected_list.push_back(choice_list[OpenZod::Util::Random::Int(0, choice_list.size() - 1)]);
		}

	}
	else //selecting all possible units
	{
		//so who have we selected
		for(ZObject* i : object_list)
		{
			if(!i->Selectable())
			{
				continue;
			}
			if(i->GetOwner() != our_team)
			{
				continue;
			}
			if(!i->WithinSelection(map_left, map_right, map_top, map_bottom))
			{
				continue;
			}
			select_info.selected_list.push_back(i);
		}
	}

	//now do this basically to get the correct mouse cursor
	select_info.SetupGroupDetails();
}

bool ZPlayer::CouldCollectSelectables()
{
	int shift_x, shift_y;
	int map_left, map_right, map_top, map_bottom;
	bool would_be_single_unit;

	zmap.GetViewShift(shift_x, shift_y);

	int mouse_x_map = mouse_x + shift_x;
	int mouse_y_map = mouse_y + shift_y;

	if(mouse_x_map < lbutton.map_x)
	{
		map_left = mouse_x_map;
		map_right = lbutton.map_x;
	}
	else
	{
		map_left = lbutton.map_x;
		map_right = mouse_x_map;
	}

	if(mouse_y_map < lbutton.map_y)
	{
		map_top = mouse_y_map;
		map_bottom = lbutton.map_y;
	}
	else
	{
		map_top = lbutton.map_y;
		map_bottom = mouse_y_map;
	}

	if(abs(lbutton.map_x - mouse_x_map) <= 1 && abs(lbutton.map_y - mouse_y_map) <= 1)
	{
		would_be_single_unit = true;
	}
	else
	{
		would_be_single_unit = false;
	}

	//so who have we selected
	for(ZObject* i : object_list)
	{
		ZObject* obj = i;

		//the group leader would be chosen
		//if we were only selecting one unit
		if(would_be_single_unit && obj->GetGroupLeader())
		{
			obj = obj->GetGroupLeader();
		}
		
		if(!obj->Selectable())
		{
			continue;
		}
		if(obj->GetOwner() != our_team)
		{
			continue;
		}
		if(!obj->WithinSelection(map_left, map_right, map_top, map_bottom))
		{
			continue;
		}
		
		return true;
	}

	return false;
}

void ZPlayer::ClearDevWayPointsOfSelected()
{
	for(ZObject* i : select_info.selected_list)
	{
		i->GetWayPointDevList().clear();
	}
}

void ZPlayer::MapCoordsOfMouseWithHud(int &map_x, int &map_y)
{
	zmap.GetMapCoords(mouse_x, mouse_y, map_x, map_y);
	zhud.OverMiniMap(mouse_x, mouse_y, init_w, init_h, map_x, map_y);
}

void ZPlayer::LoadControlGroup(int n)
{
	if(n < 0 || n >= 10)
	{
		return;
	}

	//select the group or jump to them?
	if(select_info.GroupIsSelected(n))
	{
		int tx, ty;
		if(select_info.AverageCoordsOfSelected(tx, ty))
		{
			FocusCameraTo(tx, ty);
		}
	}
	else
	{
		select_info.LoadGroup(n);
		DetermineCursor();
		ClearDevWayPointsOfSelected();
		GiveHudSelected();
	}
}

bool ZPlayer::UnitNearHostiles(ZObject *obj)
{
	if(!obj)
	{
		return false;
	}

	for(ZObject* i : ols.passive_engagable_olist)
	{
		if(obj==i)
		{
			continue;
		}
		if((obj->GetOwner() == i->GetOwner()) || (i->GetOwner() == NULL_TEAM))
		{
			continue;
		}
		if(i->IsDestroyed())
		{
			continue;
		}
		if(!obj->WithinAgroRadius(i))
		{
			continue;
		}

		return true;
	}

	return false;
}

void ZPlayer::AddDevWayPointToSelected()
{
	waypoint new_waypoint;

	zmap.GetMapCoords(mouse_x, mouse_y, new_waypoint.x, new_waypoint.y);
	bool coords_from_mini_map = zhud.OverMiniMap(mouse_x, mouse_y, init_w, init_h, new_waypoint.x, new_waypoint.y);

	new_waypoint.player_given = true;

	for(ZObject* i : select_info.selected_list)
	{
		unsigned char hot, hoid;
		int hcx, hcy;

		new_waypoint.mode = MOVE_WP;
		new_waypoint.ref_id = -1;
		new_waypoint.attack_to = true;

		if(UnitNearHostiles(i))
		{
			new_waypoint.attack_to = false;
		}
		if(CtrlDown())
		{
			new_waypoint.attack_to = true;
		}
		if(AltDown())
		{
			new_waypoint.attack_to = false;
		}

		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		//set the mode
		if(hover_object && !coords_from_mini_map)
		{
			new_waypoint.ref_id = hover_object->GetRefID();

			hover_object->GetObjectID(hot, hoid);
			hover_object->GetCenterCords(hcx, hcy);

			if(i->CanBeRepaired() && hover_object->CanRepairUnit(our_team))
			{
				new_waypoint.mode = UNIT_REPAIR_WP;
				new_waypoint.x = hcx - 8;
				new_waypoint.y = hcy;
			}
			else
			{
				switch(ot)
				{
				case CANNON_OBJECT:
					if(hot == MAP_ITEM_OBJECT && hoid == FLAG_ITEM)
					{
						break;
					}

					//enemy for attacking
					if(hover_object->GetOwner() != our_team)
					{
						new_waypoint.mode = ATTACK_WP;
					}
					break;
				case VEHICLE_OBJECT:
					if(hot == MAP_ITEM_OBJECT && hoid == FLAG_ITEM)
					{
						break;
					}

					//it a crane?
					if(oid == CRANE && hover_object->CanBeRepairedByCrane(our_team))
					{
						new_waypoint.mode = CRANE_REPAIR_WP;
						hover_object->GetCraneCenter(new_waypoint.x, new_waypoint.y);
						break;
					}

					//enter fort?
					if(hover_object_can_enter_fort)
					{
						new_waypoint.mode = ENTER_FORT_WP;
						break;
					}

					//they attack everything, unless you are selecting an APC
					if(hover_object->GetOwner() != our_team)
					{
						new_waypoint.mode = ATTACK_WP;
					}

					break;
				case ROBOT_OBJECT:
					if(hot == MAP_ITEM_OBJECT && hoid == FLAG_ITEM)
					{
						break;
					}

					//grenades?
					if((hot == MAP_ITEM_OBJECT && hoid == GRENADES_ITEM) && i->CanPickupGrenades())
					{
						new_waypoint.mode = PICKUP_GRENADES_WP;
						break;
					}

					//capturable item?
					if(hover_object->GetOwner() == NULL_TEAM &&
						(hot == CANNON_OBJECT || hot == VEHICLE_OBJECT))
					{
						new_waypoint.mode = ENTER_WP;
						break;
					}

					//enter fort?
					if(hover_object_can_enter_fort)
					{
						new_waypoint.mode = ENTER_FORT_WP;
						break;
					}

					//attack
					if(hover_object->GetOwner() != our_team)
					{
						new_waypoint.mode = ATTACK_WP;
					}


					break;
				}
			}
		}

		//add it to the list
		i->GetWayPointDevList().push_back(new_waypoint);
	}

	//setting a rally point?
	if(gui_window && gui_window->GetBuildingObj() && !select_info.selected_list.size())
	{
		waypoint new_rallypoint;

		MapCoordsOfMouseWithHud(new_rallypoint.x, new_rallypoint.y);
		new_rallypoint.ref_id = -1;
		new_rallypoint.mode = MOVE_WP;
		new_rallypoint.player_given = true;
		new_rallypoint.attack_to = true;

		gui_window->GetBuildingObj()->GetWayPointDevList().push_back(new_rallypoint);
	}

	SetPcursor();
}

bool ZPlayer::DevWayPointsNoWay()
{
	if(select_info.selected_list.size() != 1)
	{
		return false;
	}

	ZObject* obj = select_info.selected_list[0];

	if(!obj->GetWayPointDevList().size())
	{
		return false;
	}
	if(obj->GetWayPointDevList()[0].mode != ATTACK_WP)
	{
		return false;
	}

	ZObject* vobj = ZObject::GetObjectFromID(obj->GetWayPointDevList()[0].ref_id, object_list);
	if(!vobj)
	{
		return false;
	}

	unsigned char a_ot, a_oid, v_ot, v_oid;
	obj->GetObjectID(a_ot, a_oid);
	vobj->GetObjectID(v_ot, v_oid);

	return zunitrating.CrossReference(a_ot, a_oid, v_ot, v_oid) == UCR_WILL_DIE;

}

void ZPlayer::SendDevWayPointsOfObj(ZObject *obj)
{
	if(!obj)
	{
		return;
	}

	//are we registered?
	if(!is_registered)
	{
		unsigned char ot, oid;

		obj->GetObjectID(ot, oid);

		if(UnitRequiresActivation(ot, oid))
		{
			AddNewsEntry("move unit error: registration required, please visit www.nighsoft.com");
			return;
		}
	}

	char *data;
	int size;
	CreateWaypointSendData(obj->GetRefID(), obj->GetWayPointDevList(), data, size);
	if(!data)
	{
		return;
	}

	//send
	client_socket.SendMessage(SEND_WAYPOINTS, data, size);

	//free data
	free(data);
}

void ZPlayer::SendDevWayPointsOfSelected()
{
	std::vector<ZObject*>::iterator i;
	std::vector<waypoint>::iterator j;
	ZObject *remove_obj_from_selected = NULL;

	bool no_way = DevWayPointsNoWay();

	if(AsciiDown('z'))
	{
		//send only the nearest to its target

		if(select_info.selected_list.size() && (*select_info.selected_list.begin())->GetWayPointDevList().size())
		{
			waypoint &wp = *(*select_info.selected_list.begin())->GetWayPointDevList().begin();
			
			remove_obj_from_selected = ZObject::NearestObjectToCoords(select_info.selected_list, wp.x, wp.y);

			if(remove_obj_from_selected)
			{
				SendDevWayPointsOfObj(remove_obj_from_selected);
			}
		}
	}
	else
	{
		for(i=select_info.selected_list.begin(); i!=select_info.selected_list.end(); i++)
		{
			SendDevWayPointsOfObj(*i);
		}
	}

	i=select_info.selected_list.begin();
	if(i!=select_info.selected_list.end() && (*i)->GetWayPointDevList().size())
	{
		j = (*i)->GetWayPointDevList().end();
		j--;

		ShowPcursor(j->x, j->y);
	}

	//ok its sent, so clear the lists
	ClearDevWayPointsOfSelected();

	//tell the hud we gave a command
	zhud.GiveSelectedCommand(no_way);

	//did a one unit per target?
	if(remove_obj_from_selected)
	{
		select_info.RemoveFromSelected(remove_obj_from_selected);
		if(zhud.GetSelectedObject() == remove_obj_from_selected || !select_info.selected_list.size())
		{
			GiveHudSelected();
		}
		DetermineCursor();
	}

	//setting a rally point?
	if(gui_window && gui_window->GetBuildingObj() && !select_info.selected_list.size())
	{
		ZObject* obj = gui_window->GetBuildingObj();

		char *data;
		int size;
		CreateWaypointSendData(obj->GetRefID(), obj->GetWayPointDevList(), data, size);

		//no data made?
		if(data)
		{
			//send
			client_socket.SendMessage(SEND_RALLYPOINTS, data, size);

			//free data
			free(data);
		}

		obj->GetWayPointDevList().clear();
	}
}

void ZPlayer::GiveHudSelected()
{
	if(select_info.selected_list.size())
	{
		zhud.SetSelectedObject(select_info.selected_list[OpenZod::Util::Random::Int(0, select_info.selected_list.size() -1)]);
	}
	else
	{
		zhud.SetSelectedObject(NULL);
	}
}

void ZPlayer::DeleteObjectCleanUp(ZObject *obj)
{
	if(!obj)
	{
		return;
	}

	unsigned char ot, oid;
	obj->GetObjectID(ot, oid);

	if(ot == MAP_ITEM_OBJECT && oid == ROCK_ITEM)
	{
		ORock::EditRockRender(zmap, object_list, obj, false);
	}

	obj->DeathMapEffects(zmap);

	if(hover_object == obj)
	{
		hover_object = NULL;
	}

	select_info.DeleteObject(obj);

	zhud.DeleteObject(obj);

	for(ZObject* i : object_list)
	{
		i->RemoveObject(obj);
	}

	//make the buttons available that let you cycle through your units
	//and set unit amount
	ProcessChangeObjectAmount();
}

void ZPlayer::ProcessChangeObjectAmount()
{
	//make the buttons available that let you cycle through your units
	ReSetupButtons();

	//reset hud etc
	CheckUnitLimitReached();
	zhud.SetUnitAmount(team_units_available[our_team]);
}

void ZPlayer::ClearAsciiStates()
{
	for(int i=0;i<ASCII_DOWN_MAX;i++)
	{
		ascii_down[i] = false;
	}
}

void ZPlayer::SetAsciiState(int c, bool is_down)
{
	c -= 'a';

	if((c<0) || (c>=ASCII_DOWN_MAX))
	{
		return;
	}

	ascii_down[c] = is_down;
}

bool ZPlayer::AsciiDown(int c)
{
	c -= 'a';

	if((c<0) || (c>=ASCII_DOWN_MAX))
	{
		return false;
	}

	return ascii_down[c];
}

bool ZPlayer::ShiftDown()
{
	return lshift_down || rshift_down;
}

bool ZPlayer::CtrlDown()
{
	return lctrl_down || rctrl_down;
}

bool ZPlayer::AltDown()
{
	return lalt_down || ralt_down;
}

bool ZPlayer::IsOverHUD(int x, int y, int w, int h)
{
	if(x + w >= init_w - HUD_WIDTH)
	{
		return true;
	}
	if(y + h >= init_h - HUD_HEIGHT)
	{
		return true;
	}

	return false;
}

void ZPlayer::MainMenuMove(double px, double py)
{
	for(ZGuiMainMenuBase* i : gui_menu_list)
	{
		i->Move(px, py);
	}
}

bool ZPlayer::MainMenuMotion()
{
	for(ZGuiMainMenuBase* i : gui_menu_list)
	{
		int px, py;
		i->GetCoords(px, py);

		if(i->Motion(mouse_x, mouse_y))
		{
			int x,y,w,h;
			i->GetCoords(x, y);
			i->GetDimensions(w, h);

			if(IsOverHUD(x,y,w,h))
			{
				zhud.ReRenderAll();
			}
			else if(IsOverHUD(px,py,w,h))
			{
				zhud.ReRenderAll();
			}

			return true;
		}
	}

	return false;
}

bool ZPlayer::MainMenuWheelUp()
{
	for(ZGuiMainMenuBase* i : gui_menu_list)
	{
		if(i->WheelUpButton())
		{
			return true;
		}
	}

	return false;
}

bool ZPlayer::MainMenuWheelDown()
{
	for(ZGuiMainMenuBase* i : gui_menu_list)
	{
		if(i->WheelDownButton())
		{
			return true;
		}
	}

	return false;
}

bool ZPlayer::MainMenuKeyPress(int c)
{
	for(ZGuiMainMenuBase* i : gui_menu_list)
	{
		if(i->KeyPress(c))
		{
			return true;
		}
	}

	return false;
}

bool ZPlayer::MainMenuAbsorbLClick()
{
	for(std::vector<ZGuiMainMenuBase*>::iterator i=gui_menu_list.begin(); i!=gui_menu_list.end(); ++i)
	{
		if((*i)->Click(mouse_x, mouse_y))
		{
			gmm_flag &the_flags = (*i)->GetGMMFlags();

			//move this to the front
			if(gui_menu_list.begin() != i)
			{
				//this method would be a hell of a no no
				//if we weren't immediately leaving the loop anyways
				ZGuiMainMenuBase* temp = *i;
				gui_menu_list.erase(i);
				gui_menu_list.insert(gui_menu_list.begin(), temp);
			}

			if(the_flags.set_volume)
			{
				SetSoundSetting(the_flags.set_volume_value);
			}

			if(the_flags.set_game_speed) 
			{
				float_packet the_data;

				the_data.game_speed = the_flags.set_game_speed_value;
				client_socket.SendMessage(SET_GAME_SPEED, (char*)&the_data, sizeof(float_packet));
			}

			return true;
		}
	}

	return false;
}

bool ZPlayer::MainMenuAbsorbLUnClick()
{
	for(ZGuiMainMenuBase* i : gui_menu_list)
	{
		if(i->UnClick(mouse_x, mouse_y))
		{
			gmm_flag &the_flags = i->GetGMMFlags();

			if(the_flags.open_main_menu)
			{
				LoadMainMenu(the_flags.open_main_menu_type, false, the_flags.warning_flags);
			}

			if(the_flags.reshuffle_teams)
			{
				client_socket.SendMessage(RESHUFFLE_TEAMS, NULL, 0);
			}

			if(the_flags.change_team)
			{
				SendPlayerTeam(the_flags.change_team_type);
			}

			if(the_flags.reset_map)
			{
				client_socket.SendMessage(RESET_MAP, NULL, 0);
			}

			if(the_flags.quit_game)
			{
				ExitProgram();
			}

			if(the_flags.pause_game)
			{
				SendSetPaused(true);
			}

			if(the_flags.start_bot) 
			{
				int_packet the_data;
				the_data.team = the_flags.start_bot_team;
				client_socket.SendMessage(START_BOT_EVENT, (char*)&the_data, sizeof(int_packet));
			}

			if(the_flags.stop_bot) 
			{
				int_packet the_data;

				the_data.team = the_flags.stop_bot_team;
				client_socket.SendMessage(STOP_BOT_EVENT, (char*)&the_data, sizeof(int_packet));
			}

			if(the_flags.change_map) 
			{
				int_packet the_data;

				the_data.map_num = the_flags.change_map_number;
				client_socket.SendMessage(SELECT_MAP, (char*)&the_data, sizeof(int_packet));
			}

			return true;
		}
	}

	return false;
}

bool ZPlayer::GuiAbsorbLClick()
{
	int shift_x, shift_y;
	int map_x, map_y;

	//do we have a menu window?
	if(active_menu)
	{
		zmap.GetViewShift(shift_x, shift_y);

		map_x = mouse_x + shift_x;
		map_y = mouse_y + shift_y;

		if(active_menu->Click(map_x, map_y))
		{
			return true;
		}
	}

	//what about factory list?
	if(gui_factory_list && gui_factory_list->IsVisible())
	{
		zmap.GetViewShift(shift_x, shift_y);

		map_x = mouse_x + shift_x;
		map_y = mouse_y + shift_y;

		if(gui_factory_list->Click(map_x, map_y))
		{
			if(gui_factory_list->GetGFlags().jump_to_building)
			{
				//run to it
				ZObject* obj = GetObjectFromID(gui_factory_list->GetGFlags().bref_id, object_list);
				if(obj)
				{
					int ox, oy;
					obj->GetCenterCords(ox, oy);
					FocusCameraTo(ox, oy);

					//make a gui window for this building
					ObjectMakeGuiWindow(obj);
				}
			}

			return true;
		}
	}

	//do we have a gui window, and did it take the click?
	if(gui_window)
	{
		zmap.GetViewShift(shift_x, shift_y);

		map_x = mouse_x + shift_x;
		map_y = mouse_y + shift_y;

		if(gui_window->Click(map_x, map_y))
		{
			return true;
		}
		else
		{
			DeleteCurrentGuiWindow();
		}
	}
	

	//move forward?
	if(CouldCollectSelectables())
	{
		return false;
	}

	//do we start another gui window?
	if(hover_object && ObjectMakeGuiWindow(hover_object))
	{
		return true;
	}

	return false;
}

bool ZPlayer::GuiAbsorbLUnClick()
{
	int shift_x, shift_y;
	int map_x, map_y;

	//do we have a menu window?
	if(active_menu)
	{
		zmap.GetViewShift(shift_x, shift_y);

		map_x = mouse_x + shift_x;
		map_y = mouse_y + shift_y;

		if(active_menu->UnClick(map_x, map_y))
		{
			gui_flags &glfags = active_menu->GetGFlags();

			if(glfags.do_login)
			{
				login_name = active_menu->GetGFlags().login_name;
				login_password = active_menu->GetGFlags().login_password;

				SendLogin();
			}

			if(glfags.open_createuser)
			{
				active_menu = create_user_menu;
			}

			if(glfags.do_createuser)
			{
				SendCreateUser(
					active_menu->GetGFlags().user_name,
					active_menu->GetGFlags().login_name,
					active_menu->GetGFlags().login_password,
					active_menu->GetGFlags().email);
			}

			if(glfags.open_login)
			{
				active_menu = login_menu;
			}

			return true;
		}
	}

	//what about factory list?
	if(gui_factory_list && gui_factory_list->IsVisible())
	{
		zmap.GetViewShift(shift_x, shift_y);

		map_x = mouse_x + shift_x;
		map_y = mouse_y + shift_y;

		if(gui_factory_list->UnClick(map_x, map_y))
		{
			return true;
		}
	}

	//do we have a gui window, and did it take the click?
	if(gui_window)
	{
		zmap.GetViewShift(shift_x, shift_y);

		map_x = mouse_x + shift_x;
		map_y = mouse_y + shift_y;

		if(gui_window->UnClick(map_x, map_y))
		{
			if(gui_window->GetGFlags().send_new_production)
			{
				start_building_packet the_data;
				the_data.ref_id = gui_window->GetGFlags().pref_id;
				the_data.ot = gui_window->GetGFlags().pot;
				the_data.oid = gui_window->GetGFlags().poid;
				client_socket.SendMessage(START_BUILDING, (const char*)&the_data, sizeof(start_building_packet));
			}

			if(gui_window->GetGFlags().send_stop_production)
			{
				int the_data = gui_window->GetGFlags().pref_id;
				client_socket.SendMessage(STOP_BUILDING, (const char*)&the_data, sizeof(int));
			}

			if(gui_window->GetGFlags().place_cannon)
			{
				place_cannon_ref_id = gui_window->GetGFlags().cref_id;
				place_cannon_oid = gui_window->GetGFlags().coid;
				place_cannon_left = gui_window->GetGFlags().cleft;
				place_cannon_right = gui_window->GetGFlags().cright;
				place_cannon_top = gui_window->GetGFlags().ctop;
				place_cannon_bottom = gui_window->GetGFlags().cbottom;

				InitPlaceCannon();
			}

			if(gui_window->GetGFlags().send_new_queue_item)
			{
				add_building_queue_packet the_data;
				the_data.ref_id = gui_window->GetGFlags().qref_id;
				the_data.ot = gui_window->GetGFlags().qot;
				the_data.oid = gui_window->GetGFlags().qoid;
				client_socket.SendMessage(ADD_BUILDING_QUEUE, (const char*)&the_data, sizeof(add_building_queue_packet));
			}

			if(gui_window->GetGFlags().send_cancel_queue_item)
			{
				cancel_building_queue_packet the_data;
				the_data.ref_id = gui_window->GetGFlags().qcref_id;
				the_data.ot = gui_window->GetGFlags().qcot;
				the_data.oid = gui_window->GetGFlags().qcoid;
				the_data.list_i = gui_window->GetGFlags().qc_i;
				client_socket.SendMessage(CANCEL_BUILDING_QUEUE, (const char*)&the_data, sizeof(cancel_building_queue_packet));
			}

			return true;
		}
	}

	return false;
}

void ZPlayer::InitPlaceCannon()
{
	place_cannon_ok_img = NULL;
	place_cannon_nok_img = NULL;

	//get images
	switch(place_cannon_oid)
	{
	case GATLING:
		place_cannon_ok_img = CGatling::GetPlaceImage(our_team);
		place_cannon_nok_img = CGatling::GetNPlaceImage(our_team);
		break;
	case GUN:
		place_cannon_ok_img = CGun::GetPlaceImage(our_team);
		place_cannon_nok_img = CGun::GetNPlaceImage(our_team);
		break;
	case HOWITZER:
		place_cannon_ok_img = CHowitzer::GetPlaceImage(our_team);
		place_cannon_nok_img = CHowitzer::GetNPlaceImage(our_team);
		break;
	case MISSILE_CANNON:
		place_cannon_ok_img = CMissileCannon::GetPlaceImage(our_team);
		place_cannon_nok_img = CMissileCannon::GetNPlaceImage(our_team);
		break;
	default:
		return;
		break;
	}

	//make it real
	place_cannon = true;

	//set cords
	SetPlaceCannonCords();
}

void ZPlayer::SetPlaceCannonCords()
{
	if(!place_cannon)
	{
		return;
	}

	int shift_x, shift_y;
	zmap.GetViewShift(shift_x, shift_y);

	int map_x = mouse_x + shift_x;
	int map_y = mouse_y + shift_y;

	place_cannon_tx = map_x / 16;
	place_cannon_ty = map_y / 16;

	//ok or not ok?
}

void ZPlayer::RenderPlaceCannon()
{
	if(!place_cannon)
	{
		return;
	}

	int map_x = (place_cannon_tx * 16);
	int map_y = (place_cannon_ty * 16);

	zmap.RenderZSurface(&place_cannon_ok_img, map_x, map_y);
}

bool ZPlayer::DoPlaceCannon()
{
	if(!place_cannon)
	{
		return false;
	}

	place_cannon = false;

	struct place_cannon_packet the_data;
	the_data.ref_id = place_cannon_ref_id;
	the_data.oid = place_cannon_oid;
	the_data.tx = place_cannon_tx;
	the_data.ty = place_cannon_ty;

	client_socket.SendMessage(PLACE_CANNON, (const char*)&the_data, sizeof(place_cannon_packet));

	return true;
}

void ZPlayer::CloseCurrentMainMenuEtc()
{
	//Close windows in order of visibility 
	
	//main menu loaded?
	if(gui_menu_list.size())
	{
		(*gui_menu_list.begin())->DoKillMe();
		return;
	}
	
	//factory list?
	if(gui_factory_list && gui_factory_list->IsVisible())
	{
		gui_factory_list->ToggleShow();
		return;
	}
	
	//factory selected?
	if(gui_window)
	{
		gui_window->DoKillMe();
	}
}

void ZPlayer::DeleteCurrentGuiWindow()
{
	if(gui_window)
	{
		delete gui_window;
		gui_window = NULL;
	}
}

void ZPlayer::ProcessUnicode(int key)
{
	//main menu took it?
	if(MainMenuKeyPress(key))
	{

	}

	//exit out if the menu took the key
	if(active_menu && active_menu->KeyPress(key))
	{
		gui_flags &glfags = active_menu->GetGFlags();

		if(glfags.do_login)
		{
			login_name = active_menu->GetGFlags().login_name;
			login_password = active_menu->GetGFlags().login_password;

			SendLogin();
		}
		//SendCreateUser

		if(glfags.do_createuser)
		{
			SendCreateUser(
				active_menu->GetGFlags().user_name,
				active_menu->GetGFlags().login_name,
				active_menu->GetGFlags().login_password,
				active_menu->GetGFlags().email);
		}

		return;
	}

	if(key == 13)
	{
		if(collect_chat_message)
		{
			//send it
			client_socket.SendMessageAscii(SEND_CHAT, chat_message.c_str());

			chat_message.clear();
			collect_chat_message = false;
		}
		else
		{
			collect_chat_message = true;
		}

		zhud.ShowChatMessage(collect_chat_message);
	}
	else
	{
		if(collect_chat_message)
		{
			if(key == 8) //delete key
			{
				if(chat_message.length())
					chat_message.erase(chat_message.length()-1,1);
			}
			else
			{
				//add it to the string
				chat_message += key;
			}

			zhud.SetChatMessage(chat_message);
		}
		else if(key == '/')
		{
			//start chat message into a command
			collect_chat_message = true;

			chat_message = "/";
			zhud.ShowChatMessage(collect_chat_message);
			zhud.SetChatMessage(chat_message);
		}
		else if(key == 'p' || key == 'P')
		{
			LoadMainMenu(GMM_PLAYER_LIST, true);
		}
		else if(key == 'h' || key == 'H')
		{
			//toggle on/off
			show_chat_history = !show_chat_history;
		}
		else if(key == 'm' || key == 'M')
		{
			if(SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON)
			{
				SDL_WM_GrabInput(SDL_GRAB_OFF);
				AddNewsEntry("mouse released");
			}
			else
			{
				SDL_WM_GrabInput(SDL_GRAB_ON);
				AddNewsEntry("mouse taken");
			}
		}
		else if(!AltDown() && (key == 'v' || key == 'V'))
		{
			V_Button();
		}
		else if(AltDown() && (key == 'v' || key == 'V'))
		{
			SetNextSoundSetting();
		}
		else if(key == 22) //means ctrl + v ?
		{
			SelectAllOfType(VEHICLE_OBJECT);
		}
		else if(key == 18) //means ctrl + r ?
		{
			SelectAllOfType(ROBOT_OBJECT);
		}
		else if(key == 3) //means ctrl + c ?
		{
			SelectAllOfType(CANNON_OBJECT);
		}
		else if(key == 1) //means ctrl + a ?
		{
			SelectAllOfType();
		}
		else if(key == 'r' || key == 'R')
		{
			R_Button();
		}
		else if(key == 'g' || key == 'G')
		{
			G_Button();
		}
		else if(key == 'b' || key == 'B')
		{
			if(gui_factory_list) gui_factory_list->ToggleShow();
		}
		else if(key == ' ')
		{
			DoSpaceBarEvent();
		}
	}

}

void ZPlayer::SendVoteYes()
{
	client_socket.SendMessage(VOTE_YES, NULL, 0);
}

void ZPlayer::SendVoteNo()
{
	client_socket.SendMessage(VOTE_NO, NULL, 0);
}

void ZPlayer::SendVotePass()
{
	client_socket.SendMessage(VOTE_PASS, NULL, 0);
}

void ZPlayer::SendLogin()
{
	//ask if we need to display the login menu
	client_socket.SendMessage(REQUEST_LOGINOFF, NULL, 0);

	//send a login if we have it
	if(login_name.length() && login_password.length())
	{
		std::string send_str = login_name + "," + login_password;
		client_socket.SendMessageAscii(SEND_LOGIN, send_str.c_str());
	}
}

void ZPlayer::SendCreateUser(std::string username, std::string lname, std::string lpass, std::string email)
{
	std::string send_str = username + "," + lname + "," + lpass + "," + email;
	client_socket.SendMessageAscii(CREATE_USER, send_str.c_str());
}

void ZPlayer::SendSetPaused(bool paused)
{
	update_game_paused_packet packet;
	packet.game_paused = paused;
	client_socket.SendMessage(SET_GAME_PAUSED, (const char*)&packet, sizeof(update_game_paused_packet));
}

void ZPlayer::InitMenus()
{
	active_menu = NULL;
	login_menu = new GWLogin(&ztime);
	create_user_menu = new GWCreateUser(&ztime);
}

void ZPlayer::LoadMainMenu(int menu_type, bool kill_if_open, gmm_warning_flag warning_flags)
{
	ZGuiMainMenuBase *new_menu = NULL;

	//already loaded?
	for(std::vector<ZGuiMainMenuBase*>::iterator i=gui_menu_list.begin(); i!=gui_menu_list.end(); ++i)
	{
		if((*i)->GetMenuType() == menu_type)
		{
			if(kill_if_open)
			{
				delete *i;
				gui_menu_list.erase(i);
			}
			else
			{
				//move this to the front
				if(gui_menu_list.begin() != i)
				{
					//this method would be a hell of a no no
					//if we weren't immediately leaving the loop anyways
					ZGuiMainMenuBase* temp = *i;
					gui_menu_list.erase(i);
					gui_menu_list.insert(gui_menu_list.begin(), temp);
				}
			}

			return;
		}
	}

	//load it
	switch(menu_type)
	{
	case GMM_MAIN_MAIN:
		new_menu = new GMMMainMenu();
		break;
	case GMM_CHANGE_TEAMS:
		new_menu = new GMMChangeTeams();
		break;
	case GMM_MANAGE_BOTS:
		new_menu = new GMMManageBots();
		break;
	case GMM_PLAYER_LIST:
		new_menu = new GMMPlayerList();
		break;
	case GMM_SELECT_MAP:
		new_menu = new GMMSelectMap();
		break;
	case GMM_OPTIONS:
		new_menu = new GMMOptions();
		break;
	case GMM_WARNING:
		new_menu = new GMMWarning(warning_flags);
		break;
	case GMM_MULTIPLAYER:
		new_menu = new GMMMultiplayer();
		break;
	default:
		spdlog::error("ZPlayer::LoadMainMenu - Bad menu_type {}", menu_type);
		break;
	}

	if(new_menu) 
	{
		new_menu->SetCenterCoords(init_w >> 1, init_h >> 1);
		new_menu->SetPlayerInfoList(&player_info);
		new_menu->SetSelectableMapList(&selectable_map_list);
		new_menu->SetPlayerTeam((int*)&our_team);
		new_menu->SetSoundSetting(&sound_setting);
		new_menu->SetZTime(&ztime);

		gui_menu_list.insert(gui_menu_list.begin(), new_menu);
	}
}

void ZPlayer::RefindOurFortRefID()
{
	fort_ref_id = -1;

	for(ZObject* i : object_list)
	{
		if(i->GetOwner() != our_team)
		{
			continue;
		}

		unsigned char ot, oid;
		i->GetObjectID(ot, oid);

		if(ot == BUILDING_OBJECT && (oid == FORT_FRONT || oid == FORT_BACK))
		{
			fort_ref_id = i->GetRefID();
			break;
		}
	}
}

void ZPlayer::SetNextSoundSetting()
{
	SetSoundSetting(sound_setting+1);
}

void ZPlayer::SetSoundSetting(int sound_setting_)
{
	sound_setting = sound_setting_;

	if((sound_setting < 0) || (sound_setting >= MAX_SOUND_SETTINGS))
	{
		sound_setting = 0;
	}

	switch(sound_setting)
	{
	case SOUND_0: 
		Mix_Volume(-1, 0); 
		Mix_VolumeMusic(0);
		AddNewsEntry("volume off");
		break;
	case SOUND_25: 
		Mix_Volume(-1, 128 / 4); 
		Mix_VolumeMusic(80 / 4);
		AddNewsEntry("volume 25%");
		break;
	case SOUND_50: 
		Mix_Volume(-1, 128 / 2); 
		Mix_VolumeMusic(80 / 2);
		AddNewsEntry("volume 50%");
		break;
	case SOUND_75: 
		Mix_Volume(-1, 128 * 3 / 4); 
		Mix_VolumeMusic(80 * 3 / 4);
		AddNewsEntry("volume 75%");
		break;
	case SOUND_100: 
		Mix_Volume(-1, 128); 
		Mix_VolumeMusic(80);
		AddNewsEntry("volume full");
		break;
	}
}

void ZPlayer::AddSpaceBarEvent(SpaceBarEvent new_event)
{
	//delete duplicates
	for(std::vector<SpaceBarEvent>::iterator i=space_event_list.begin(); i!=space_event_list.end();)
	{
		if(new_event == *i)
		{
			i = space_event_list.erase(i);
		}
		else
		{
			++i;
		}
	}

	space_event_list.insert(space_event_list.begin(), new_event);

	if(space_event_list.size() >= MAX_STORED_SPACE_BAR_EVENTS)
	{
		space_event_list.resize(MAX_STORED_SPACE_BAR_EVENTS);
	}
}

void ZPlayer::DoSpaceBarEvent()
{
	SpaceBarEvent process_event;

	while(space_event_list.size())
	{
		//get event
		process_event = space_event_list[0];

		//get object
		ZObject* obj = GetObjectFromID(process_event.ref_id, object_list);

		//if not good any more, delete this event and try again
		if(!obj || process_event.past_lifetime())
		{
			space_event_list.erase(space_event_list.begin());
			continue;
		}

		//focus on its leader instead?
		if(obj->GetGroupLeader())
		{
			obj = obj->GetGroupLeader();
		}

		//process event
		int x, y;
		obj->GetCenterCords(x, y);
		FocusCameraTo(x, y);

		if(process_event.select_obj && !select_info.ObjectIsSelected(obj))
		{
			SelectZObject(obj);
		}

		if(process_event.open_gui)
		{
			ObjectMakeGuiWindow(obj);
		}

		//move event to end of list
		space_event_list.erase(space_event_list.begin());
		space_event_list.push_back(process_event);

		//exit
		break;
	}
}

bool ZPlayer::ObjectMakeGuiWindow(ZObject *obj)
{
	if(!obj)
	{
		return false;
	}

	//only one can exist
	DeleteCurrentGuiWindow();

	//checks
	if((obj->GetOwner() == NULL_TEAM) || (obj->GetOwner() != our_team))
	{
		return false;
	}

	//make it
	gui_window = obj->MakeGuiWindow();

	//it get made?
	if(!gui_window)
	{
		return false;
	}

	//set its build list
	gui_window->SetBuildList(&buildlist);

	//clear rally points
	obj->GetWayPointDevList().clear();

	return true;
}
