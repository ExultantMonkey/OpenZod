#ifndef _ZMYSQL_H_
#define _ZMYSQL_H_

#ifndef DISABLE_MYSQL
	#ifdef _WIN32
	#include <windows.h>
	#include <stdio.h>
	#include <string.h>
	#include <mysql.h>
	#else
	#include <mysql/mysql.h>
	#endif
#endif

#include <string>
#include <stdio.h>

#include "zpsettings.h"
#include "constants.h"
#include "common.h"

using namespace COMMON;

class ZMysqlUser
{
public:
	ZMysqlUser() {clear();}

	int tabID;
	std::string username;
	std::string loginname;
	std::string password;
	std::string email;
	bool activated;
	int voting_power;
	int wins;
	int loses;
	int total_games;
	std::string creation_ip;
	std::string last_ip;
	int creation_time;
	int last_time;
	int total_time;
	int aff_creates;
	int aff_buys;

	void clear()
	{
		tabID = -1;
		username.clear();
		loginname.clear();
		password.clear();
		email.clear();
		activated = false;
		voting_power = 0;
		wins = loses = total_games = 0;
		creation_ip = "";
		last_ip = "";
		creation_time = 0;
		last_time = 0;
		total_time = 0;
		aff_creates = 0;
		aff_buys = 0;
	}

	void format()
	{
		if(username.length() > MAX_PLAYER_NAME_SIZE) username = username.substr(0,MAX_PLAYER_NAME_SIZE);
		if(loginname.length() > MAX_PLAYER_NAME_SIZE) loginname = loginname.substr(0,MAX_PLAYER_NAME_SIZE);
		if(password.length() > MAX_PLAYER_NAME_SIZE) password = password.substr(0,MAX_PLAYER_NAME_SIZE);
		if(email.length() > 250) email = email.substr(0,250);
	}

	bool format_okay()
	{
		if(!username.length()) return false;
		if(!loginname.length()) return false;
		if(!password.length()) return false;
		if(!email.length()) return false;

		if(username.length() > MAX_PLAYER_NAME_SIZE) return false;
		if(loginname.length() > MAX_PLAYER_NAME_SIZE) return false;
		if(password.length() > MAX_PLAYER_NAME_SIZE) return false;
		if(email.length() > 250) return false;

		if(!good_user_string(username.c_str())) return false;
		if(!good_user_string(loginname.c_str())) return false;
		if(!good_user_string(password.c_str())) return false;
		if(!good_user_string(email.c_str())) return false;

		return true;
	}

	void debug()
	{
		printf("ZMysqlUser Debug:\n");
		printf("\t tabID:%d\n", tabID);
		printf("\t username:'%s'\n", username.c_str());
		printf("\t loginname:'%s'\n", loginname.c_str());
		printf("\t password:'%s'\n", password.c_str());
		printf("\t email:'%s'\n", email.c_str());
		printf("\t activated:%d\n", activated);
		printf("\t voting_power:%d\n", voting_power);
		printf("\t wins:%d\n", wins);
		printf("\t loses:%d\n", loses);
		printf("\t total_games:%d\n", total_games);
		printf("\t creation_ip:'%s'\n", creation_ip.c_str());
		printf("\t last_ip:'%s'\n", last_ip.c_str());
		printf("\t creation_time:%d\n", creation_time);
		printf("\t last_time:%d\n", last_time);
		printf("\t total_time:%d\n", total_time);
		printf("\t aff_creates:%d\n", aff_creates);
		printf("\t aff_buys:%d\n", aff_buys);
	}
};

class ZMysql
{
public:
	ZMysql();
	~ZMysql();

	bool LoadDetails(std::string &error_msg, std::string filename = "mysql.txt");
	bool LoadDetails(std::string &error_msg, ZPSettings &psettings);

	bool Connect(std::string &error_msg);
	bool Disconnect(std::string &error_msg);
	bool CreateUserTable(std::string &error_msg);
	bool AddUser(std::string &error_msg, ZMysqlUser &new_user, bool &user_added);
	bool CheckUserExistance(std::string &error_msg, ZMysqlUser &user, bool &user_exists);
	bool LoginUser(std::string &error_msg, std::string loginname, std::string password, ZMysqlUser &user, bool &user_found);
	bool UpdateUserVariable(std::string &error_msg, int tabID, std::string varname, std::string value);
	bool UpdateUserVariable(std::string &error_msg, int tabID, std::string varname, int value);
	bool IncreaseUserVariable(std::string &error_msg, int tabID, std::string varname, int amount);
	bool IPInUserTable(std::string &error_msg, std::string ip, int &users_found);

	bool CreateOnlineHistoryTable(std::string &error_msg);
	bool InsertOnlineHistoryEntry(std::string &error_msg, int time_stamp, int player_count, int tray_player_count);

	bool CreateAffiliateTable(std::string &error_msg);
	bool GetAffiliateId(std::string &error_msg, std::string ip, int &aff_id, bool &ip_found);
private:
	bool details_loaded;

	std::string host_name;
	std::string database_name;
	std::string login_name;
	std::string login_password;
	std::string root_password;
	
#ifndef DISABLE_MYSQL
	MYSQL *mysql_conn;
#endif
};

#endif
