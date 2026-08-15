#pragma once

namespace discofloor
{
    // Settings stored and used inside each fixedphilip bot/cluster
	// These settings can be loaded from and saved to a config file, see the config struct below
	struct bot_settings
	{
		// Chat prefix for old-style commands (can be set to blank to disable)
		std::string prefix = "fp!";

		// List of disabled modules by name
		// Accepts wildcards ('*') - todo
		std::vector<std::string> disabled_modules = {};

		// Folder where bot (user/guild/global) data should be stored
		// Can be absolute or relative
		std::string data_folder = "data";

		// Maximum size of bot (user/guild/global) data for any given snowflake ID
		uintmax_t max_data_size_id = 1024 * 1024;

		// Maximum total size of bot (user/guild/global) data
		uintmax_t max_data_size_total = 1024 * 1024 * 1024;

		// Modify this function to return json data of this structure
		nlohmann::json struct_to_json() const;

		// Modify this function to read structure data from json
		bool json_to_struct(const nlohmann::json& data);
	};
}