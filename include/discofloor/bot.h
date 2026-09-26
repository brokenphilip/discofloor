#pragma once

#include <discofloor/bot_command.h>
#include <discofloor/bot_module.h>
#include <discofloor/json.h>
#include <discofloor/utility.h>

#include <bulbtils/time.h>

#define DISCOFLOOR_DEFAULT_TOKEN "your_bot_token_here"

namespace discofloor
{
	// Settings stored and used inside each discofloor bot
	// These settings can be loaded from and saved to a config file, see the config struct below
	struct bot_settings
	{
		// Chat prefix for old-style commands (can be set to blank to disable)
		std::string prefix = "";

		// List of disabled modules by name
		// Accepts wildcards ('*') - todo
		std::vector<std::string> disabled_modules = {};

		// Folder where bot snowflake-specific data should be stored
		// Can be absolute or relative
		std::string data_folder = "data";

		// Maximum size of bot snowflake-specific data for any given ID
		uintmax_t max_data_size_id = 1024 * 1024;

		// Maximum total size of bot snowflake-specific data
		uintmax_t max_data_size_total = 1024 * 1024 * 1024;

		// Modify this function to return json data of this structure
		nlohmann::json struct_to_json(const bulbtils::file::settings& save_settings) const;

		// Modify this function to read structure data from json
		bool json_to_struct(const nlohmann::json& data, const bulbtils::file::settings& load_settings);
	};

	// Configuration file class used to load discofloor bot token and settings from a file
	struct bot_config : public pretty_print_json_file
	{
		std::string token = DISCOFLOOR_DEFAULT_TOKEN;
		bot_settings settings;

		virtual nlohmann::json struct_to_json(const bulbtils::file::settings& save_settings) const override final;
		virtual bool json_to_struct(const nlohmann::json& data, const bulbtils::file::settings& load_settings) override final;

		// Loads a discofloor bot config, performs value checks, and saves if necessary
		// This is preferred over manual calls to load()/save() and manually checking values
		// 
		// Returns false if loading the config or a value check failed
		// Only instantiate a discofloor bot if this returns true
		//
		// If warning_callback/error_callback are provided in file_settings
		// ...they will be used to print warnings/errors for checked values
		//
		// NOTE: Forces file_settings.create_if_not_found to true, for saving to work
		bool load_check_save(const bulbtils::file::settings& file_settings);
	};

	// File-based (JSON) data structure for storing snowflake-specific discofloor bot data
	// Inherit this class and use bot::load_data and bot::save_data to manipulate data
    class bot_data : public json_file<-1, ' '>
    {
        using bulbtils::file::base::save;
        using bulbtils::file::base::load;
        friend class bot;
    };

	// Server and user counts, for the servers the discofloor bot is currently in
	// ...as well as all the (guild and user install) users it can see
	struct bot_counts
	{
		// Amount of servers we're currently in
		int servers = -1;

		// Amount of unique, non-bot users in the servers we're in
		int users = -1;

		// If true, approximate user counts are used instead of exact (unique, non-bot)
		bool users_fallback = false;

		// Amount of users that installed our app
		int user_installs = -1;

		// Total amount of users from servers and app installs
		// NOTE: if either values are invalid, this is invalid too
		int total_users = -1;
	};

	// The base of a discofloor bot/cluster, expanded to support:
	// - Modules and their commands
	// - Snowflake-based bot data management
	// - Additional info such as settings, instance owner, etc...
	class bot : public dpp::cluster
	{
	public:
		static inline const std::function<dpp::message(const std::string&)> default_reply_error_message = [](const std::string& error) { return container_msg(error, 0xFF0000); };
	private:
		template <typename T>
		using event_t = dpp::task<void>(const T& event);

		bot_settings settings_;

		const bulbtils::time::raii_stopwatch running_time_;
		const std::chrono::system_clock::time_point start_time_ = std::chrono::system_clock::now();

		std::vector<bot_module*> loaded_modules_;
		std::shared_mutex loaded_modules_mutex_;

		struct module_command : public bot_command
		{
			bot* owner;
			dpp::event_handle event_handles[2] { SIZE_MAX };
			
			inline module_command(const bot_command& cmd, bot* owner)
				: bot_command(cmd), owner(owner) {}

			dpp::message usage_options(const std::string& prefix, const std::vector<dpp::command_option>& options, const std::string& subcmd_group_and_or_subcmd, const std::string& cmd_desc);
			dpp::message usage_subcmds(const std::string& prefix, const std::vector<dpp::command_option>& options, const std::string& subcmd_group);
			dpp::message usage_subcmd_groups(const std::string& prefix);
			dpp::message usage(const std::string& prefix, const std::string& subcmd_group = "", const std::string& subcmd = "");

			// This error is thrown for malformed commands - prints the error message
			struct msg_create_malformed_cmd_error : public std::runtime_error
			{
				using runtime_error::runtime_error;
			};

			// This error is thrown for blank commands - prints usage help
			struct msg_create_blank_cmd_error : public std::runtime_error
			{
				std::string subcmd_group;
				std::string subcmd;

				msg_create_blank_cmd_error(const std::string& error, const std::string& subcmd_group = "", const std::string& subcmd = "")
					: runtime_error(error), subcmd_group(subcmd_group), subcmd(subcmd) {
				}
			};

			static void iterate_msg_create_cmd_params(const dpp::message_create_t& event, const std::vector<std::string>& params,
				const std::vector<dpp::command_option>& source_options, std::vector<dpp::command_data_option>& dest_options, int level);

			static void iterate_msg_create_subcmds(const dpp::message_create_t& event, const std::vector<std::string>& params,
				const std::vector<dpp::command_option>& source_options, std::vector<dpp::command_data_option>& dest_options, int level);

			event_t<dpp::message_create_t> message_create_event;

			void attach_events();
			void detach_events();
			void detach_message_content_events();
		};
		std::vector<module_command> module_commands_;
		std::shared_mutex module_commands_mutex_;

		std::function<void(bot_command&)> for_each_command_;
		std::function<dpp::message(const std::string&)> reply_error_message_ = default_reply_error_message;

		dpp::user app_owner_;
		std::shared_mutex app_owner_mutex_;

		std::atomic_bool ready_init_done_ = false;

		static event_t<dpp::ready_t> ready_event;

		void create_commands_async();
		void fetch_app_info_async();

		bulbtils::file::settings data_file_settings(dpp::snowflake id, const std::string& name);

		void after_construction();
	public:
		// Create a discofloor bot using loaded configuration data based on a D++ cluster, with NO initial logging functionality
		inline bot(const bot_config& config, uint32_t intents = dpp::i_default_intents,
			uint32_t shards = 0, uint32_t cluster_id = 0, uint32_t maxclusters = 1, bool compressed = true,
			dpp::cache_policy_t policy = dpp::cache_policy::cpol_default, uint32_t pool_threads = std::thread::hardware_concurrency() / 2)
				: dpp::cluster(config.token, intents, shards, cluster_id, maxclusters, compressed, policy, pool_threads), settings_(config.settings)
					{ after_construction(); }

		// Create a discofloor bot using loaded configuration data based on a D++ cluster, with initial logging functionality
		template <typename Callable>
		bot(const bot_config& config, Callable&& logger, uint32_t intents = dpp::i_default_intents,
			uint32_t shards = 0, uint32_t cluster_id = 0, uint32_t maxclusters = 1, bool compressed = true,
			dpp::cache_policy_t policy = dpp::cache_policy::cpol_default, uint32_t pool_threads = std::thread::hardware_concurrency() / 2)
				: dpp::cluster(config.token, intents, shards, cluster_id, maxclusters, compressed, policy, pool_threads), settings_(config.settings)
		{
			on_log.attach(logger);
			after_construction();
		}

		virtual ~bot();

		// Perform a full shutdown of a discofloor bot, terminating all websocket connections (making them unrecoverable)
		// Kind of hacky, not particularly clean (with messy logs), but it's the only way to get it to show offline
		// Not necessary to shut down the bot - you can also call shutdown() or delete the cluster/discofloor::bot instead
		void full_shutdown();

		// Supply a callback function that gets run for each command that a module sends to the bot
		// Use this function to conveniently modify all commands for a discofloor bot
		inline void for_each_command(std::function<void(bot_command&)> callback) { for_each_command_ = std::move(callback); }

		// Supply a callback function that wraps error message strings into your own custom dpp::message
		// Use this function to control how the bot prints internal error messages upon replying to users
		inline void reply_error_message(std::function<dpp::message(const std::string&)> callback) { reply_error_message_ = std::move(callback); }

		// Using the callback function for wrapping error message strings, return the resulting dpp::message
		// By default, this uses "default_reply_error_message", ie. a "container_msg" with a red accent
		inline dpp::message format_error_reply(const std::string& error) { return reply_error_message_(error); }

		// Given the command name and type, get a copy of the command
		std::optional<bot_command> get_command(const std::string& name, dpp::slashcommand_contextmenu_type type);

		// Append the bot's own logging functions to a file settings structure
		// This, of course, overwrites any logging functions already set in the struct
		bulbtils::file::settings& append_loggers(bulbtils::file::settings& file_settings);

		// Returns a copy of the bot's settings
		inline auto settings() { return settings_; }

		// Formats bot running time as "??h ??m ??s", or "?d ??h ??m" if over 24 hours have passed
		std::string format_running_time();

		// Returns the unix timestamp when the bot was created
		inline auto start_time_unix() { return std::chrono::duration_cast<std::chrono::seconds>(start_time_.time_since_epoch()).count(); }

		// Are old-style (chat prefix) commands enabled on this bot?
		inline bool old_style_commands_enabled() { return ((intents & dpp::i_message_content) != 0) && !settings_.prefix.empty(); }

		// Returns a copy of the list of loaded modules
		// Should you decide to modify a loaded module, you are responsible for its thread safety
		inline auto loaded_modules() { std::shared_lock _(loaded_modules_mutex_); return loaded_modules_; }

		// Add (late-load) a module to the bot - returns true on success
		// Returns false if called too early (must be after on_ready_init)
		// Also returns false if the module failed to load or is disabled by config/settings file
		bool add_module(bot_module* module_to_add);

		// Remove (early-unload) a module from the bot - returns true on success
		// Returns false if this module is not loaded
		bool remove_module(bot_module* module_to_remove);

		// Returns a copy of the dpp::user who owns this app/instance
		inline auto app_owner() { std::shared_lock _(app_owner_mutex_); return app_owner_; }

		// Load snowflake-specific bot data
		bulbtils::file::result load_data(dpp::snowflake id, const std::string& name, bot_data& data_out);

		// Save snowflake-specific bot data
		// In addition to file::base::save return values, also returns 'r_write_error':
		// - If we're over the size quota
		// - For any unhandled OS I/O errors
		bulbtils::file::result save_data(dpp::snowflake id, const std::string& name, const bot_data& data);

		// Returns the current size of all bot data
		// The maximum value can be found under settings()
		inline uintmax_t data_size_total() { return bulbtils::file::get_folder_size(settings_.data_folder); }

		// Returns the current size of bot data for this ID
		// The maximum value can be found under settings()
		inline uintmax_t data_size_id(dpp::snowflake id) { return bulbtils::file::get_folder_size(data_folder_id(id)); }

		// Given an ID, return the respective bot data folder
		std::filesystem::path data_folder_id(dpp::snowflake id) { return std::filesystem::path(settings_.data_folder) / std::to_string(id); }

		// Returns this bot's counts, see the bot_counts data structure above
		dpp::task<bot_counts> co_get_counts();
	};
}