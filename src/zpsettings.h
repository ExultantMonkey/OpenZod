#ifndef _ZPSETTINGS_H_
#define _ZPSETTINGS_H_

#include <string>

class ZPSettings
{
public:
	ZPSettings();

	void LoadDefaults();
	bool LoadSettings(std::string filename);
	bool SaveSettings(std::string filename);

	bool loaded_from_file;

	bool ignore_activation;
	bool require_login;
	bool use_database;
	bool use_mysql;
	bool start_map_paused;
	bool bots_start_ignored;
	bool allow_game_speed_change;
	std::string selectable_map_list;

	std::string mysql_root_password;
	std::string mysql_user_name;
	std::string mysql_user_password;
	std::string mysql_hostname;
	std::string mysql_dbname;
};

#endif
