#include "zbot.h"

#include "Util/Math.h"
#include "Util/Random.h"

#include <spdlog/spdlog.h>

ZBot::ZBot() : ZClient()
{
	player_name = "Bot";
	our_mode = BOT_MODE;

	next_ai_time = 0;
	last_order_time = 0;

	//bots are registered
	is_registered = true;
	
	SetupEHandler();
	
	client_socket.SetEventList(&ehandler.GetEventList());

	preferred_build_list.push_back(PreferredUnit(VEHICLE_OBJECT, MEDIUM));
	preferred_build_list.push_back(PreferredUnit(VEHICLE_OBJECT, LIGHT));
	preferred_build_list.push_back(PreferredUnit(VEHICLE_OBJECT, JEEP));
	preferred_build_list.push_back(PreferredUnit(ROBOT_OBJECT, PYRO));
	preferred_build_list.push_back(PreferredUnit(ROBOT_OBJECT, TOUGH));
	preferred_build_list.push_back(PreferredUnit(ROBOT_OBJECT, SNIPER));
	preferred_build_list.push_back(PreferredUnit(ROBOT_OBJECT, TOUGH));
	preferred_build_list.push_back(PreferredUnit(ROBOT_OBJECT, PSYCHO));
	preferred_build_list.push_back(PreferredUnit(ROBOT_OBJECT, GRUNT));
}

void ZBot::Setup()
{
	//randomizer
	SetupRandomizer();

	if(!client_socket.Start(remote_address.c_str()))
	{
		spdlog::warn("ZBot::Setup:socket not setup");
	}
}

void ZBot::Run()
{
	while(allow_run)
	{
		ztime.UpdateTime();

		//check for tcp events
		ProcessSocketEvents();
		
		//process events
		ehandler.ProcessEvents();

		//do bot AI
		ProcessAI();
		
		COMMON::uni_pause(10);
	}
}

void ZBot::ProcessSocketEvents()
{
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
		char* message;
		int size;
		int pack_id;
		while(shandler->DoFastProcess(&message, &size, &pack_id))
		{
			ehandler.ProcessEvent(TCP_EVENT, pack_id, message, size, 0);
		}
		shandler->ResetFastProcess();
	}
}

void ZBot::ProcessAI()
{
	double &the_time = ztime.ztime;

	for(ZObject* o : object_list)
	{
		o->SmoothMove(the_time);
	}
	
	//process gun placement heatmap
	gp_heatmap.Process(the_time, ols, zmap, our_team);

	if(the_time < next_ai_time)
	{
		return;
	}

	next_ai_time = the_time + 1;

	//are we ignored?
	if(OurPInfo().ignored)
	{
		return;
	}

	Stage1AI();
	
	PlaceCannons();

	//set new build orders
	ChooseBuildOrders();
}

bool ZBot::Stage1AI()
{
	double &the_time = ztime.ztime;
	std::vector<ZObject*> units_list;
	std::vector<ZObject*> targeted_list;
	std::vector<ZObject*> targets_list;
	double percent_to_order, order_delay;

	bool all_out = GoAllOut(percent_to_order, order_delay);

	//ordering too fast?
	if(the_time < last_order_time + order_delay)
	{
		return true;
	}

	//collect our units
	CollectOurUnits(units_list, targeted_list);

	//collect our targets
	CollectOurTargets(targets_list, all_out);

	//remove some units
	ReduceUnitsToPercent(units_list, percent_to_order);

	//remove already targeted
	std::vector<ZObject*> targets_orig_list = targets_list;
	RemoveTargetedFromTargets(targets_list, targeted_list);

	//match targets to units
	MatchTargets(units_list, targets_list);

	//give out orders
	GiveOutOrders(units_list, targets_list);

	//did not some units not get orders because some targets were removed?
	if(units_list.size() && targets_list.size() != targets_orig_list.size())
	{
		//match targets to units
		MatchTargets(units_list, targets_orig_list);

		//give out orders
		GiveOutOrders(units_list, targets_orig_list);
	}

	last_order_time = the_time;

	return true;
}

void ZBot::ReduceUnitsToPercent(std::vector<ZObject*> &units_list, double max_percent)
{
	std::vector<ZObject*> new_units_list;
	bool own_repairable_buildings = false;
	bool crane_in_new_list = false;

	if(max_percent >= 0.95)
	{
		return;
	}
	if(units_list.size() <= 1)
	{
		return;
	}

	int max_units = max_percent * units_list.size();
	max_units++;

	if(max_units > units_list.size())
	{
		max_units = units_list.size();
	}
	
	//do we own destroyed buildings?
	for(ZObject* o : object_list)
	{	
		//is it ours
		if(o->GetOwner() != our_team)
		{
			continue;
		}

		unsigned char ot, oid;
		o->GetObjectID(ot, oid);
		
		//a building?
		if((ot != BUILDING_OBJECT) || (oid == BRIDGE_VERT) || (oid == BRIDGE_HORZ))
		{
			continue;
		}
		
		//can be repaired by a crane?
		if(o->CanBeRepairedByCrane(our_team))
		{
			own_repairable_buildings = true;
			break;
		}
	}

	//select the percentage of units
	while(new_units_list.size() < max_units && units_list.size())
	{
		std::vector<ZObject*>::iterator u = units_list.begin() + OpenZod::Util::Random::Int(0, units_list.size() - 1);

		//adding a crane to the new list?
		unsigned char ot, oid;
		(*u)->GetObjectID(ot, oid);
		if(ot == VEHICLE_OBJECT && oid == CRANE)
		{
			crane_in_new_list = true;
		}
		
		new_units_list.push_back(*u);
		units_list.erase(u);
	}
	
	//add in a crane if we have destroyed buildings and we didn't add a crane
	if(own_repairable_buildings && !crane_in_new_list && OpenZod::Util::Random::Int(0, 5))
	{
		std::vector<std::vector<ZObject*>::iterator> cu_list;
		
		//build list of available cranes
		for(std::vector<ZObject*>::iterator u=units_list.begin();u!=units_list.end();++u)
		{
			unsigned char ot, oid;

			//not a crane?
			(*u)->GetObjectID(ot, oid);
			if(!(ot == VEHICLE_OBJECT && oid == CRANE))
			{
				continue;
			}
			
			cu_list.push_back(u);
		}
		
		//choose one from random
		if(cu_list.size())
		{
			std::vector<ZObject*>::iterator u = cu_list[OpenZod::Util::Random::Int(0, cu_list.size() - 1)];
			
			new_units_list.push_back(*u);
			units_list.erase(u);
		}
	}

	units_list = new_units_list;
}

void ZBot::RemoveTargetedFromTargets(std::vector<ZObject*> &targets_list, std::vector<ZObject*> &targeted_list)
{
	//no targets?
	if(!targets_list.size())
	{
		return;
	}

	//incase we want to undo
	std::vector<ZObject*> targets_orig_list = targets_list;

	for(std::vector<ZObject*>::iterator t=targeted_list.begin(); t!=targeted_list.end();t++)
	{
		ZObject::RemoveObjectFromList(*t, targets_list);
	}

	//if we removed more then 75% of targets
	//then nevermind
	if(1.0 * targets_list.size() / targets_orig_list.size() <= 0.25)
	{
		targets_list = targets_orig_list;
	}
}

void ZBot::GiveOutOrders(std::vector<ZObject*> &units_list, std::vector<ZObject*> &targets_list)
{
	//set orig lists
	for(ZObject* u : units_list)
	{
		u->GetAIOrigList() = u->GetAIList();
	}

	while(units_list.size())
	{
		bool order_given = false;

		for(std::vector<ZObject*>::iterator u=units_list.begin(); u!=units_list.end();)
		{
			ZObject *uc_obj;
			ZObject *tc_obj;

			//we have no targets?
			if(!(*u)->GetAIList().size())
			{
				if((*u)->GetAIOrigList().size())
				{
					(*u)->GetAIList() = (*u)->GetAIOrigList();
				}
				else
				{
					u = units_list.erase(u);
					continue;
				}
			}

			//find nearest target
			{
				uc_obj = (*u)->NearestObjectFromList((*u)->GetAIList());

				//didn't find one?
				if(!uc_obj)
				{
					spdlog::debug("ZBot::GiveOutOrders - !uc_obj");
					u = units_list.erase(u);
					continue;
				}
			}

			//find that targets nearest unit
			{
				tc_obj = uc_obj->NearestObjectFromList(uc_obj->GetAIList());

				//didn't find one?
				if(!tc_obj)
				{
					spdlog::debug("ZBot::GiveOutOrders - !tc_obj");
				}
				//do we match?
				else if(tc_obj != *u)
				{
					u++;
					continue;
				}
			}

			//give command
			{
				unsigned char tot, toid;
				bool set_waypoint = false;
				waypoint new_waypoint;

				uc_obj->GetObjectID(tot, toid);

				switch(tot)
				{
				case MAP_ITEM_OBJECT:
					//capture flag
					if(toid == FLAG_ITEM && uc_obj->GetOwner() != our_team) 
					{
						new_waypoint.mode = MOVE_WP;
						set_waypoint = true;
					}
					//grenades
					if(toid == GRENADES_ITEM)
					{
						new_waypoint.mode = PICKUP_GRENADES_WP;
						set_waypoint = true;
					}
					break;
				case BUILDING_OBJECT:
					switch(toid)
					{
					case FORT_FRONT:
					case FORT_BACK:
						//enter fort
						if(uc_obj->GetOwner() != our_team && uc_obj->GetOwner() != NULL_TEAM && !uc_obj->IsDestroyed())
						{
							new_waypoint.mode = ENTER_FORT_WP;
							set_waypoint = true;
						}
						break;
					case REPAIR:
						//repair building
						if(uc_obj->GetOwner() == our_team && !uc_obj->IsDestroyed())
						{
							new_waypoint.mode = UNIT_REPAIR_WP;
							set_waypoint = true;
						}
					}

					//crane repairable
					if(uc_obj->CanBeRepairedByCrane(our_team))
					{
						new_waypoint.mode = CRANE_REPAIR_WP;
						set_waypoint = true;
					}

					break;
				case CANNON_OBJECT:
					//enter vehicle / cannon?
					if(uc_obj->GetOwner() == NULL_TEAM && !uc_obj->IsDestroyed())
					{
						new_waypoint.mode = ENTER_WP;
						set_waypoint = true;
					}

					//attack_object
					else if((*u)->CanAttackObject(uc_obj))
					{
						new_waypoint.mode = ATTACK_WP;
						set_waypoint = true;
					}
					break;
				case VEHICLE_OBJECT:
					//enter vehicle / cannon?
					if(uc_obj->GetOwner() == NULL_TEAM && !uc_obj->IsDestroyed())
					{
						new_waypoint.mode = ENTER_WP;
						set_waypoint = true;
					}

					//attack_object
					else if((*u)->CanAttackObject(uc_obj))
					{
						new_waypoint.mode = ATTACK_WP;
						set_waypoint = true;
					}
					break;
				case ROBOT_OBJECT:
					//attack_object
					if((*u)->CanAttackObject(uc_obj))
					{
						new_waypoint.mode = ATTACK_WP;
						set_waypoint = true;
					}
					break;
				}

				if(set_waypoint)
				{
					if(new_waypoint.mode != ATTACK_WP)
					{
						uc_obj->GetCenterCords(new_waypoint.x, new_waypoint.y);
					}
					else
					{
						uc_obj->GetCords(new_waypoint.x, new_waypoint.y);
						new_waypoint.x += 8;
						new_waypoint.y += 8;
					}
					new_waypoint.ref_id = uc_obj->GetRefID();
					new_waypoint.attack_to = true;
					new_waypoint.player_given = true;

					//add waypoint to the list
					(*u)->GetWayPointDevList().clear();
					(*u)->GetWayPointDevList().push_back(new_waypoint);

					//send
					SendBotDevWaypointList(*u);
				}
			}

			//take this unit off all targets lists
			for(ZObject* t2 : targets_list)
			{
				ZObject::RemoveObjectFromList(*u, t2->GetAIList());
			}

			//next u
			order_given = true;
			u = units_list.erase(u);

			//take this target off every units list
			for(ZObject* u2 : units_list)
			{
				ZObject::RemoveObjectFromList(uc_obj, u2->GetAIList());
			}
		}

		if(!order_given)
		{
			break;
		}
	}
}

bool ZBot::CullTargetFromCrane(ZObject* u, ZObject* t, std::vector<ZObject*> &ct_list)
{
	const int max_line_dist = 14*16;
	const int max_total_dist = 42*16;
	int ucx, ucy;
	int tcx, tcy;
	
	u->GetCenterCords(ucx, ucy);
	t->GetCenterCords(tcx, tcy);
	
	//see if it is along the way to any of the high priority targets
	for(ZObject* ct : ct_list)
	{
		//is t one of the high priority targets?
		if(t == ct)
		{
			return false;
		}
		
		int ctcx, ctcy;
		double u_ct_dist = u->DistanceFromObject(*ct);
		double t_ct_dist = t->DistanceFromObject(*ct);
		double u_t_dist  = u->DistanceFromObject(*t);
		
		ct->GetCenterCords(ctcx, ctcy);
		
		//too far in the other direction past the crane?
		if(t_ct_dist > u_ct_dist + max_line_dist)
		{
			continue;
		}
		
		//too far past the priority target?
		if(u_t_dist > u_ct_dist + max_line_dist)
		{
			continue;
		}
		
		//too far to trust is on the way to the high priority target?
		if(u_t_dist > max_total_dist)
		{
			continue;
		}
		
		//too far away from the line in between the crane and the high priority target?
		if(OpenZod::Util::Math::PointDistanceFromLine(ucx, ucy, ctcx, ctcy, tcx, tcy) > max_line_dist)
		{
			continue;
		}
		
		//is within the line between the crane and the high priority target
		//and not too far past the crane or target, or just too far away from crane
		return false;
	}
	
	//it wasn't along the way to any of the high priority targets so remove
	return true;
}

void ZBot::MatchTargets(std::vector<ZObject*> &units_list, std::vector<ZObject*> &targets_list)
{
	//clear the ai lists
	for(ZObject* u : units_list)
	{
		u->GetAIList().clear();
	}
	for(ZObject* t : targets_list)
	{
		t->GetAIList().clear();
	}
	
	//crane repair list needed to cull crane targets
	std::set<ZObject*> cu_set;

	//do the matching
	for(ZObject* u : units_list)
	{
		unsigned char uot, uoid;
		int ux, uy;

		u->GetObjectID(uot, uoid);
		u->GetCords(ux, uy);
		ux += 8;
		uy += 8;

		for(ZObject* t : targets_list)
		{
			unsigned char tot, toid;
			t->GetObjectID(tot, toid);

			int tx, ty;
			if(tot == BUILDING_OBJECT)
			{
				t->GetCenterCords(tx, ty);
			}
			else
			{
				t->GetCords(tx, ty);
				tx += 8;
				ty += 8;
			}

			//can this unit go to this region?
			if(!zmap.GetPathFinder().InSameRegion(ux, uy, tx, ty, (uot == ROBOT_OBJECT)))
			{
				continue;
			}

			switch(tot)
			{
			case MAP_ITEM_OBJECT:
				//capture flag
				if(toid == FLAG_ITEM && t->GetOwner() != our_team)
				{
					u->AddToAIList(t);
				}
				//grenades
				if(toid == GRENADES_ITEM)
				{
					if(u->CanPickupGrenades())
					{
						u->AddToAIList(t);
					}
				}
				break;
			case BUILDING_OBJECT:
				switch(toid)
				{
				case FORT_FRONT:
				case FORT_BACK:
					//enter fort
					if(t->GetOwner() != our_team && t->GetOwner() != NULL_TEAM && !t->IsDestroyed())
					{
						u->AddToAIList(t);
					}
					break;
				case REPAIR:
					//repair building
					if(t->GetOwner() == our_team && !t->IsDestroyed())
					{
						if(u->CanBeRepaired())
						{
							u->AddToAIList(t);
						}
					}
					break;
				}

				//crane repairable
				if(t->CanBeRepairedByCrane(our_team))
				{
					if(uot == VEHICLE_OBJECT && uoid == CRANE)
					{
						u->AddToAIList(t);
						
						//this a high priority crane target that needs to cull out other targets?
						if(tot == BUILDING_OBJECT && toid != BRIDGE_VERT && toid != BRIDGE_HORZ && our_team == t->GetOwner())
						{
							cu_set.insert(u);
						}
					}
				}

				break;
			case CANNON_OBJECT:
				//enter vehicle / cannon?
				if(t->CanBeEntered())
				{
					if(uot == ROBOT_OBJECT)
					{
						u->AddToAIList(t);
					}
				}

				//attack_object
				else if(u->CanAttackObject(t))
				{
					u->AddToAIList(t);
				}
				break;
			case VEHICLE_OBJECT:
				//enter vehicle / cannon?
				if(t->CanBeEntered())
				{
					if(uot == ROBOT_OBJECT)
					{
						u->AddToAIList(t);
					}
				}

				//attack_object
				else if(u->CanAttackObject(t))
				{
					u->AddToAIList(t);
				}
				break;
			case ROBOT_OBJECT:
				//attack_object
				if(u->CanAttackObject(t))
				{
					u->AddToAIList(t);
				}
				break;
			}
		}
	}
	
	//do we have high priority crane targets that need to cull out other targets?
	if(!cu_set.size())
	{
		return;
	}

	for(std::set<ZObject*>::iterator u=cu_set.begin();u!=cu_set.end();++u)
	{
		std::vector<ZObject*> ct_list;
		std::vector<ZObject*> &u_ai_list = (*u)->GetAIList();
		
		//build high priority target list for this crane
		for(ZObject* t : u_ai_list)
		{			
			if(!t->CanBeRepairedByCrane(our_team))
			{
				continue;
			}
		
			unsigned char tot, toid;	
			t->GetObjectID(tot, toid);
			
			if(tot != BUILDING_OBJECT)
			{
				continue;
			}
			if((toid == BRIDGE_VERT) || (toid == BRIDGE_HORZ))
			{
				continue;
			}
			if(our_team != t->GetOwner())
			{
				continue;
			}
			
			ct_list.push_back(t);
		}
		
		//this crane doesn't have high priority targets?
		if(!ct_list.size())
		{
			continue;
		}
		
		//cull targets
		for(std::vector<ZObject*>::iterator t=u_ai_list.begin();t!=u_ai_list.end();)
		{
			if(CullTargetFromCrane(*u, *t, ct_list))
			{
				std::vector<ZObject*> &t_ai_list = (*t)->GetAIList();
				
				//remove from lists
				for(std::vector<ZObject*>::iterator tu=t_ai_list.begin();tu!=t_ai_list.end();)
				{
					if(*tu == *u)
					{
						tu=t_ai_list.erase(tu);
					}
					else
					{
						++tu;
					}
				}
				
				t=u_ai_list.erase(t);
			}
			else
			{
				++t;
			}
		}
	}
}

void ZBot::CollectOurTargets(std::vector<ZObject*> &targets_list, bool all_out)
{
	for(ZObject* t : object_list)
	{
		unsigned char tot, toid;
		t->GetObjectID(tot, toid);

		switch(tot)
		{
		case MAP_ITEM_OBJECT:
			//capture flag
			if(toid == FLAG_ITEM && t->GetOwner() != our_team)
			{
				targets_list.push_back(t);
			}
			//grenades?
			if(toid == GRENADES_ITEM)
			{
				targets_list.push_back(t);
			}
			break;
		case BUILDING_OBJECT:
			switch(toid)
			{
			case FORT_FRONT:
			case FORT_BACK:
				//enter fort
				if(t->GetOwner() != our_team && t->GetOwner() != NULL_TEAM && !t->IsDestroyed())
				{
					targets_list.push_back(t);
				}
				break;
			case REPAIR:
				//repair building
				if(t->GetOwner() == our_team && !t->IsDestroyed())
				{
					targets_list.push_back(t);
				}
			}

			//crane repairable
			if(t->CanBeRepairedByCrane(our_team))
			{
				targets_list.push_back(t);
			}

			break;
		case CANNON_OBJECT:
			//enter vehicle / cannon?
			if(t->CanBeEntered())
			{
				targets_list.push_back(t);
			}

			//attack object
			else if(all_out && t->GetOwner() != our_team && t->GetOwner() != NULL_TEAM && !t->IsDestroyed())
			{
				targets_list.push_back(t);
			}
			break;
		case VEHICLE_OBJECT:
			//enter vehicle / cannon?
			if(t->CanBeEntered())
			{
				targets_list.push_back(t);
			}
			//attack object
			else if(all_out && t->GetOwner() != our_team && t->GetOwner() != NULL_TEAM && !t->IsDestroyed())
			{
				targets_list.push_back(t);
			}
			break;
		case ROBOT_OBJECT:
			//attack object
			if(all_out && t->GetOwner() != our_team && t->GetOwner() != NULL_TEAM && !t->IsDestroyed())
			{
				targets_list.push_back(t);
			}
			break;
		}
	}
}

void ZBot::CollectOurUnits(std::vector<ZObject*> &units_list, std::vector<ZObject*> &targeted_list)
{
	for(ZObject* o : ols.mobile_olist)
	{
		if(o->GetOwner() != our_team)
		{
			continue;
		}

		unsigned char ot, oid;
		o->GetObjectID(ot, oid);

		//minions can't be given waypoints
		if(ot == ROBOT_OBJECT && o->IsMinion())
		{
			continue;
		}

		//already got a target?
		if(o->GetWayPointList().size())
		{
			int ref_id = o->GetWayPointList().begin()->ref_id;
			ZObject* tobj = this->GetObjectFromID(ref_id, object_list);

			//if we have a target skip this unit
			//unless it is going to a flag we own
			if(tobj)
			{
				unsigned char tot, toid;
				tobj->GetObjectID(tot, toid);

				if(tot == MAP_ITEM_OBJECT && toid == FLAG_ITEM)
				{
					if(tobj->GetOwner() != our_team)
					{
						targeted_list.push_back(tobj);
						continue;
					}
				}
				else
				{
					//do not flag a repair station as "targeted"
					//(and therefor to be possibly ignored)
					if(!(tot == BUILDING_OBJECT && toid == REPAIR))
					{
						targeted_list.push_back(tobj);
					}
					continue;
				}
			}

			//if this is a dodge wp then let the unit be
			if(o->GetWayPointList().begin()->mode == DODGE_WP)
			{
				continue;
			}
		}

		units_list.push_back(o);
	}
}

bool ZBot::GoAllOut(double &percent_to_order, double &order_delay)
{
	bool team_exists[MAX_TEAM_TYPES];
	int our_flags = 0;
	int teams_existing = 0;
	bool all_out = false;

	percent_to_order = 0.35;
	order_delay = 4;

	//clear team exists
	for(int i=0;i<MAX_TEAM_TYPES;i++)
	{
		team_exists[i] = false;
	}

	for(ZObject* o : ols.flag_olist)
	{
		team_exists[o->GetOwner()] = true;

		if(o->GetOwner() == our_team)
		{
			our_flags++;
		}
	}

	for(int i=0;i<MAX_TEAM_TYPES;i++)
	{
		if(team_exists[i])
		{
			teams_existing++;
		}
	}

	//eh?
	if(!teams_existing)
	{
		return true;
	}
	if(!ols.flag_olist.size())
	{
		return true;
	}

	//do we own our fair portion of the zones?
	{
		double percent_owned = 1.0 * our_flags / ols.flag_olist.size();
		double percent_half = 1.0 / teams_existing;

		if(percent_owned >= percent_half)
		{
			all_out = true;
			percent_to_order = 0.15;
			order_delay = 12;
		}
		else if(percent_owned >= percent_half * 0.5)
		{
			percent_to_order = 0.15;
			order_delay = 8;
		}
		else if(percent_owned >= percent_half * 0.25)
		{
			percent_to_order = 0.25;
			order_delay = 5;
		}
	}

	return all_out;
}

bool ZBot::GetBestBuildComboIncCI(std::vector<int> &ci, int max_pu)
{
	int i=0;
	
	while(1)
	{
		//incremented ci's past the max number of ci's?
		if(i>=ci.size())
		{
			return false;
		}
		
		//increment this ci
		ci[i]++;
		
		//has this ci gone over it's max?
		//if no then ci increment complete and return true
		if(ci[i] >= max_pu)
		{
			//reset this ci to zero and set i to inc the next ci
			ci[i] = 0;
			i++;
		}
		else
		{
			return true;
		}
	}
	
	return false;
}

bool ZBot::GetBestBuildComboBuildretn(std::vector<int> &ci, std::vector<ZObject*> &b_list, std::vector<PreferredUnit> &pu_list, BuildCombo &retn)
{
	//clear
	retn.b_list.clear();
	
	if(!pu_list.size() || !b_list.size())
	{
		return false;
	}
	
	std::vector<int> pu_count(pu_list.size(), 0);
	
	//build b_list for this combination
	std::vector<int>::iterator cii=ci.begin();
	for(std::vector<ZObject*>::iterator bi=b_list.begin(); bi!=b_list.end() && cii!=ci.end(); ++bi, ++cii)
	{
		ZObject* &b = *bi;
		int cci = *cii;
		
		if(!b)
		{
			return false;
		}
		
		if(cci < 0 || cci >= pu_list.size())
		{
			spdlog::error("ZBot::GetBestBuildComboBuildretn - Bad cci {}", cci);
			return false;
		}
		
		PreferredUnit &pu = pu_list[cci];
		unsigned char ot, oid;
		b->GetObjectID(ot, oid);
		
		//does this building have this unit?
		if(!buildlist.UnitInBuildList(oid, b->GetLevel(), pu.ot, pu.oid))
		{
			return false;
		}
		
		//has unit type so add building unit to build combo
		BuildingUnit new_bu;
		new_bu.b = b;
		new_bu.pot = pu.ot;
		new_bu.poid = pu.oid;
		retn.b_list.push_back(new_bu);
		
		//increment build count of this prefered unit
		pu_count[cci]++;
	}
	
	//create actual sums of prefered units building
	//based on units already building not in the b_list
	int total_units_building = 0;
	for(int i=0;i<pu_count.size() && i<pu_list.size();i++)
	{
		pu_count[i] += pu_list[i].in_production;
		total_units_building += pu_count[i];
	}
	
	//calculate target_distance
	{
		double squared_dist_sum = 0;
		
		for(int i=0;i<pu_list.size() && i<pu_count.size(); i++)
		{
			double diff = ((pu_count[i] * 1.0 / total_units_building) - pu_list[i].p_value);
			
			squared_dist_sum += diff * diff;
		}
			
		retn.target_distance = sqrt(squared_dist_sum);
	}
	
	return true;
}

BuildCombo ZBot::GetBestBuildCombo(std::vector<ZObject*> &b_list, std::vector<PreferredUnit> &pu_list)
{
	BuildCombo ret;
	
	//checks
	if(!b_list.size() || !pu_list.size())
	{
		return ret;
	}
	
	int max_pu = pu_list.size();
	std::vector<int> ci(b_list.size(), 0);
	
	while(1)
	{
		BuildCombo retn;
		
		//build retn
		if(GetBestBuildComboBuildretn(ci, b_list, pu_list, retn))
		{
			//it's value better than store ret build combo?
			if(!ret.b_list.size())
			{
				ret = retn;
			}
			else
			{
				//if the values are effectively the same, then choose it randomly
				if(fabs(retn.target_distance - ret.target_distance) <= 0.001)
				{
					if(OpenZod::Util::Random::Bool())
					{
						ret = retn;
					}
				}
				else if(retn.target_distance < ret.target_distance)
				{
					ret = retn;
				}
			}
		}
		
		//increment ci
		if(!GetBestBuildComboIncCI(ci, max_pu))
		{
			break;
		}
	}
	
	return ret;
}

bool ZBot::CanBuildAt(ZObject *b)
{
	double &the_time = ztime.ztime;
	
	if(!b)
	{
		return false;
	}
	
	//if building nothing then true
	if(b->GetBuildState() == BUILDING_SELECT)
	{
		return true;
	}
	
	double percentage_produced = b->PercentageProduced(the_time);
	double btotal_time = b->ProductionTimeTotal();
	double last_set_build_time = b->GetLastSetAIBuildTime();
	
	if(percentage_produced >= 0.25)
	{
		return false;
	}
	
	if(the_time < last_set_build_time + (btotal_time * 0.35))
	{
		return false;
	}
	
	return true;
}

void ZBot::AddBuildingProductionSums(ZObject *b, std::vector<PreferredUnit> &preferred_build_list)
{
	if(!b)
	{
		return;
	}

	unsigned char cot, coid;	
	if(!b->GetBuildUnit(cot, coid))
	{
		return;
	}
	
	for(std::vector<PreferredUnit>::iterator p=preferred_build_list.begin(); p!=preferred_build_list.end(); p++)
	{
		if(p->ot == cot && p->oid == coid)
		{
			p->in_production++;
			break;
		}
	}
}

void ZBot::PlaceCannons()
{
	for(ZObject* o : object_list)
	{	
		//is it ours
		if(o->GetOwner() != our_team)
		{
			continue;
		}

		unsigned char ot, oid;
		o->GetObjectID(ot, oid);

		//a building that can build?
		if(ot != BUILDING_OBJECT || !o->ProducesUnits())
		{
			continue;
		}
		
		//is destroyed?
		if(o->IsDestroyed())
		{
			continue;
		}
		
		std::vector<unsigned char> c_list = o->GetBuiltCannonList();
		if(!c_list.size())
		{
			continue;
		}
		
		//only place the first one
		//this way the heatmap doesn't try to place them all in the same place
		int tx, ty;
		if(gp_heatmap.FindCannonPlace(this, zmap, zsettings, o, c_list[0], tx, ty))
		{
			struct place_cannon_packet the_data;
			
			the_data.ref_id = o->GetRefID();
			the_data.oid = c_list[0];
			the_data.tx = tx;
			the_data.ty = ty;

			client_socket.SendMessage(PLACE_CANNON, (const char*)&the_data, sizeof(place_cannon_packet));
		}
	}
}

std::vector<ZObject*> ZBot::ChooseGunBuildOrders(std::map<ZObject*,BuildingUnit> &fb_list, std::vector<ZObject*> &guns_building_list, std::vector<ZObject*> &total_buildings_list)
{
	std::vector<ZObject*> gb_list;
	std::vector<ZObject*> gb_plist;
	const double percent_guns_building_max = 0.35;
	int total_buildings = total_buildings_list.size();
	bool map_cant_build_gatling = false;
	std::map<map_zone_info*,int> guns_building_in_zone;
	
	//if the map can build a light or toughs and up then never build a gatling gun 
	for(ZObject* o : object_list)
	{
		unsigned char ot, oid;
		o->GetObjectID(ot, oid);
		
		//a building that can build?
		if(ot != BUILDING_OBJECT || !o->ProducesUnits())
		{
			continue;
		}
		
		if(buildlist.UnitInBuildList(oid, o->GetLevel(), ROBOT_OBJECT, TOUGH) ||
			buildlist.UnitInBuildList(oid, o->GetLevel(), VEHICLE_OBJECT, LIGHT))
		{
			map_cant_build_gatling = true;
			break;
		}
	}
	
	//build list to pull from randomly
	for(std::map<ZObject*,BuildingUnit>::iterator fb=fb_list.begin();fb!=fb_list.end();++fb)
	{
		ZObject *bo = fb->first;
		if(!bo)
		{
			continue;
		}
		
		//this building already building / finished a cannon?
		unsigned char cot, coid;
		if(bo->GetBuildUnit(cot, coid) && cot == CANNON_OBJECT)
		{
			continue;
		}
		
		gb_plist.push_back(fb->first);
	}
	
	//pull from list until the next one would go over 20%
	while((1.0 + guns_building_list.size()) / total_buildings <= percent_guns_building_max && gb_plist.size())
	{
		std::vector<ZObject*>::iterator gb = gb_plist.begin() + OpenZod::Util::Random::Int(0, gb_plist.size() - 1);
		ZObject *gbo = *gb;
		
		//remove from potential list
		gb_plist.erase(gb);
		
		if(!gbo)
		{
			continue;
		}
		
		map_zone_info *gb_zone = gbo->GetConnectedZone();
		
		//already too many cannons building in this zone?
		{
			int c_in_zone = gbo->CannonsInZone(ols);
			int c_building_in_zone = 0;
			
			for(ZObject* o : guns_building_list)
			{
				if(gb_zone == o->GetConnectedZone())
				{
					c_building_in_zone++;
				}
			}
				
			if(c_in_zone + c_building_in_zone >= MAX_STORED_CANNONS)
			{
				continue;
			}
		}
		
		//choose cannon to build
		unsigned char coid = GATLING;
		bool cchoosen = false;
		
		{
			unsigned char bot, boid;
			gbo->GetObjectID(bot, boid);
			
			//start with the highest gun available then choose howitzer 1/6 % if choose missle cannon
			for(int oid=MAX_CANNON_TYPES-1;oid>=0;oid--)
			{
				if(buildlist.UnitInBuildList(boid, gbo->GetLevel(), CANNON_OBJECT, oid))
				{
					coid = oid;
					cchoosen = true;
					
					break;
				}
			}
				
			if(buildlist.UnitInBuildList(boid, gbo->GetLevel(), CANNON_OBJECT, HOWITZER) && !(rand()%6))
			{
				coid = HOWITZER;
				cchoosen = true;
			}
		}
		
		//didn't choose a cannon?
		if(!cchoosen)
		{
			continue;
		}
		
		//choose a gatling when they aren't allowed?
		if(coid == GATLING && map_cant_build_gatling)
		{
			continue;
		}
		
		//choose a gatling or gun when a howitzer or missle can be built in the zone?
		if(coid == GATLING || coid == GUN)
		{
			bool can_build_h_or_m_in_zone = false;
			
			for(ZObject* to : total_buildings_list)
			{			
				if(!to)
				{
					continue;
				}
				
				//not in same zone or are the same building?
				if((gb_zone != to->GetConnectedZone()) || (gbo == to))
				{
					continue;
				}
				
				unsigned char tot, toid;
				to->GetObjectID(tot, toid);
				
				if(buildlist.UnitInBuildList(toid, to->GetLevel(), CANNON_OBJECT, HOWITZER) ||
					buildlist.UnitInBuildList(toid, to->GetLevel(), CANNON_OBJECT, MISSILE_CANNON))
				{
					can_build_h_or_m_in_zone = true;
					break;
				}
			}
			
			if(can_build_h_or_m_in_zone)
			{
				continue;
			}
		}
		
		//set cannon build info
		BuildingUnit &bu = fb_list[gbo];
		bu.pot = CANNON_OBJECT;
		bu.poid = coid;
		
		//add this building to the list that had their build changed a cannon
		gb_list.push_back(gbo);
		
		//add to guns building list
		guns_building_list.push_back(gbo);
	}
	
	return gb_list;
}

void ZBot::ChooseBuildOrders()
{
	double &the_time = ztime.ztime;
	const int max_combo_check = 6;
	
	//set preferences
	std::vector<PreferredUnit> vehicle_build_list;
	std::vector<PreferredUnit> robot_build_list;
	
	vehicle_build_list.push_back(PreferredUnit(VEHICLE_OBJECT, LIGHT, 0.1));
	vehicle_build_list.push_back(PreferredUnit(VEHICLE_OBJECT, MEDIUM, 0.5));
	vehicle_build_list.push_back(PreferredUnit(VEHICLE_OBJECT, HEAVY, 0.2));
	vehicle_build_list.push_back(PreferredUnit(VEHICLE_OBJECT, MISSILE_LAUNCHER, 0.1));
	
	robot_build_list.push_back(PreferredUnit(ROBOT_OBJECT, PYRO, 0.5));
	robot_build_list.push_back(PreferredUnit(ROBOT_OBJECT, TOUGH, 0.2));
	robot_build_list.push_back(PreferredUnit(ROBOT_OBJECT, LASER, 0.2));
	
	//get robot, vehicle, and fort factory lists
	std::vector<ZObject*> vb_list;
	std::vector<ZObject*> rb_list;
	
	//crane data
	bool building_crane = false;
	bool own_crange = false;
	std::vector<ZObject*> cb_list;
	bool own_repairable_buildings = false;
	
	//gun data
	std::vector<ZObject*> guns_building_list;
	std::vector<ZObject*> total_buildings_list;
	
	//final build list
	std::map<ZObject*,BuildingUnit> fb_list;
	
	for(ZObject* o : object_list)
	{	
		//is it ours
		if(o->GetOwner() != our_team)
		{
			continue;
		}

		unsigned char ot, oid;
		o->GetObjectID(ot, oid);
		
		//is a crane?
		if(ot == VEHICLE_OBJECT && oid == CRANE)
		{
			own_crange = true;
		}

		//a building?
		if(ot != BUILDING_OBJECT)
		{
			continue;
		}
		
		//can be repaired by a crane?
		if(o->CanBeRepairedByCrane(our_team) && oid != BRIDGE_VERT && oid != BRIDGE_HORZ)
		{
			own_repairable_buildings = true;
		}
		
		//building can build?
		if(!o->ProducesUnits())
		{
			continue;
		}
		
		//is destroyed?
		if(o->IsDestroyed())
		{
			continue;
		}
		
		//building a crane?
		unsigned char cot, coid;
		bool got_build_unit = false;
		if((got_build_unit = o->GetBuildUnit(cot, coid)) && cot == VEHICLE_OBJECT && coid == CRANE)
		{
			building_crane = true;
		}
		
		//needed for guns 
		total_buildings_list.push_back(o);
		
		//enough time hasn't passed?
		if(!CanBuildAt(o))
		{
			//is it building one of the prefered units?
			AddBuildingProductionSums(o, robot_build_list);
			AddBuildingProductionSums(o, vehicle_build_list);
			
			if(got_build_unit && cot == CANNON_OBJECT)
			{
				guns_building_list.push_back(o);
			}
		}
		else
		{
			//vehicle factory? (consider forts a vehicle factory)
			if(oid == VEHICLE_FACTORY || (oid == FORT_FRONT || oid == FORT_BACK))
			{
				for(std::vector<PreferredUnit>::iterator p=vehicle_build_list.begin(); p!=vehicle_build_list.end(); p++)
				{
					if(buildlist.UnitInBuildList(oid, o->GetLevel(), p->ot, p->oid))
					{
						vb_list.push_back(o);
						break;
					}
				}
			}
			
			//robot factory?
			if(oid == ROBOT_FACTORY)
			{
				for(std::vector<PreferredUnit>::iterator p=robot_build_list.begin(); p!=robot_build_list.end(); p++)
				{
					if(buildlist.UnitInBuildList(oid, o->GetLevel(), p->ot, p->oid))
					{
						rb_list.push_back(o);
						break;
					}
				}
			}
			
			//can build a crane?
			if(buildlist.UnitInBuildList(oid, o->GetLevel(), VEHICLE_OBJECT, CRANE))
			{
				cb_list.push_back(o);
			}
			
			//add it to the final list with a simple prefered ordered choice
			for(std::vector<PreferredUnit>::iterator p=preferred_build_list.begin(); p!=preferred_build_list.end(); p++)
			{
				if(buildlist.UnitInBuildList(oid, o->GetLevel(), p->ot, p->oid))
				{
					BuildingUnit new_bu;
					new_bu.b = o;
					new_bu.pot = p->ot;
					new_bu.poid = p->oid;
					
					fb_list[o] = new_bu;
					break;
				}
			}
		}
	}
	
	//choose gun building
	{
		std::vector<ZObject*> gb_list = ChooseGunBuildOrders(fb_list, guns_building_list, total_buildings_list);
		
		//remove gun builds from vehicle / robot potential build lists
		for(ZObject* gb : gb_list)
		{
			//remove building from preferred building lists
			for(std::vector<ZObject*>::iterator b=vb_list.begin();b!=vb_list.end();)
			{
				if(*b==gb)
				{
					b=vb_list.erase(b);
				}
				else
				{
					++b;
				}
			}
			
			for(std::vector<ZObject*>::iterator b=rb_list.begin();b!=rb_list.end();)
			{
				if(*b==gb)
				{
					b=rb_list.erase(b);
				}
				else
				{
					++b;
				}
			}
		}
	}
	
	//build a crane?
	if(own_repairable_buildings && !building_crane && !own_crange && cb_list.size())
	{
		std::vector<ZObject*>::iterator cb = cb_list.begin() + (OpenZod::Util::Random::Int(0, cb_list.size() - 1));
		
		//set to crane
		BuildingUnit &bu = fb_list[*cb];
		bu.pot = VEHICLE_OBJECT;
		bu.poid = CRANE;
		
		//remove building from preferred building lists
		for(std::vector<ZObject*>::iterator b=vb_list.begin();b!=vb_list.end();)
		{
			if(*b==*cb)
			{
				b=vb_list.erase(b);
			}
			else
			{
				++b;
			}
		}
		
		for(std::vector<ZObject*>::iterator b=rb_list.begin();b!=rb_list.end();)
		{
			if(*b==*cb)
			{
				b=rb_list.erase(b);
			}
			else
			{
				++b;
			}
		}
	}
	
	//reduce full build list to max size processable
	if(max_combo_check > 0)
	{
		while(vb_list.size() > max_combo_check)
		{
			std::vector<ZObject*>::iterator b = vb_list.begin() + OpenZod::Util::Random::Int(0, vb_list.size() - 1);
			
			//add it to the production sums
			AddBuildingProductionSums(*b, vehicle_build_list);
			
			//erase from fb_list and this list
			fb_list.erase(*b);
			vb_list.erase(b);
		}
		
		while(rb_list.size() > max_combo_check)
		{
			std::vector<ZObject*>::iterator b = rb_list.begin() + OpenZod::Util::Random::Int(0, rb_list.size() - 1);
			
			//add it to the production sums
			AddBuildingProductionSums(*b, robot_build_list);
			
			//erase from fb_list and this list
			fb_list.erase(*b);
			rb_list.erase(b);
		}
	}
	
	//get prefered build lists for robot and vehicle factories
	BuildCombo vb_combo = GetBestBuildCombo(vb_list, vehicle_build_list);
	BuildCombo rb_combo = GetBestBuildCombo(rb_list, robot_build_list);
	
	//set prefered build combos to final build list
	{
		for(std::vector<BuildingUnit>::iterator bu=vb_combo.b_list.begin();bu!=vb_combo.b_list.end();++bu)
		{
			fb_list[bu->b] = *bu;
		}
		
		for(std::vector<BuildingUnit>::iterator bu=rb_combo.b_list.begin();bu!=rb_combo.b_list.end();++bu)
		{
			fb_list[bu->b] = *bu;
		}
	}
	
	//make the build orders
	for(std::map<ZObject*,BuildingUnit>::iterator fb=fb_list.begin();fb!=fb_list.end();++fb)
	{
		BuildingUnit &bu = fb->second;
		unsigned char cot, coid;
		
		//sanity check?
		if(bu.b != fb->first)
		{
			spdlog::error("ZBot::ChooseBuildOrders - bu.b != fb->first");
			continue;
		}
		
		//get current production
		if(!bu.b->GetBuildUnit(cot, coid))
		{
			continue;
		}
		
		//perfered different than current?
		//then send request
		if(!(cot == bu.pot && coid == bu.poid))
		{
			//stop production
			{
				int the_data = bu.b->GetRefID();
				client_socket.SendMessage(STOP_BUILDING, (const char*)&the_data, sizeof(int));
			}

			//start new
			{
				start_building_packet the_data;
				the_data.ref_id = bu.b->GetRefID();
				the_data.ot = bu.pot;
				the_data.oid = bu.poid;

				client_socket.SendMessage(START_BUILDING, (const char*)&the_data, sizeof(start_building_packet));
			}
		}
		
		//set building time
		bu.b->SetLastSetAIBuildTime(the_time);
	}
}

void ZBot::SendBotDevWaypointList(ZObject *obj)
{
	if(!obj)
	{
		return;
	}

	char *data;
	int size;
	CreateWaypointSendData(obj->GetRefID(), obj->GetWayPointDevList(), data, size);

	if(data)
	{
		//send
		client_socket.SendMessage(SEND_WAYPOINTS, data, size);

		//free data
		free(data);
	}
}
