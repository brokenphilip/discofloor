#pragma once

//#include <format>

#include <dpp/dpp.h>

#include <fixedphilip/file.h>
#include <fixedphilip/log.h>

#include <bulbtils/named_node.h>
#include <bulbtils/time.h>

#include <variant>

#define FIXEDPHILIP_DEFAULT_TOKEN "your_bot_token_here"

namespace discofloor
{
	// The base of a fixedphilip bot/cluster, expanded to support
	// - Modules and their commands
	// - Global, user or guild-specific bot data management
	// - Additional info such as settings, instance owner, etc...
	class bot : public dpp::cluster
	{
		// Constructed on startup and read-only - no need for a mutex
		bot_settings settings_;

		// Constructed on startup and read-only - no need for a mutex
		const fixedphilip::utils::time::raii_stopwatch running_time_;
		const std::chrono::system_clock::time_point start_time_ = std::chrono::system_clock::now();

		std::vector<bot_module*> loaded_modules_;
		std::shared_mutex loaded_modules_mutex_;

		struct module_command : public command
		{
			dpp::event_handle event_handles[2] { SIZE_MAX };
			dpp::snowflake snowflake {};
			
			inline module_command(const command& cmd) : command(cmd) {}
		};
		std::vector<module_command> module_commands_;
		std::shared_mutex module_commands_mutex_;

		dpp::user app_owner_;
		std::shared_mutex app_owner_mutex_;

		std::atomic_bool ready_init_done_ = false;

		static void logger(const dpp::log_t&);

		template <typename T>
		using event_t = dpp::task<void>(const T& event);
		static event_t<dpp::log_t> log_event;
		static event_t<dpp::ready_t> ready_event;

		void create_commands_async();
		void fetch_app_info_async();

		fixedphilip::file::settings data_file_settings(dpp::snowflake id, const std::string& name);
	public:
		bot(const std::string& token, const bot_settings& settings, uint32_t intents = dpp::i_default_intents,
			uint32_t shards = 0, uint32_t cluster_id = 0, uint32_t maxclusters = 1, bool compressed = true,
			dpp::cache_policy_t policy = dpp::cache_policy::cpol_default, uint32_t pool_threads = std::thread::hardware_concurrency() / 2);

		virtual ~bot();

		// Returns a copy of the bot's settings
		inline auto settings() { return settings_; }

		// Formats bot running time as "??h ??m ??s", or "?d ??h ??m" if over 24 hours have passed
		std::string format_running_time();

		// Returns the unix timestamp when the bot was created
		inline auto start_time_unix() { return std::chrono::duration_cast<std::chrono::seconds>(start_time_.time_since_epoch()).count(); }

		// Returns a copy of the list of loaded modules
		// Should you decide to modify a loaded module, you are responsible for its thread safety
		inline auto loaded_modules() { std::shared_lock _(loaded_modules_mutex_); return loaded_modules_; }

		// Given a slash command name, returns its snowflake
		// Only works for CHAT_INPUT commands (context menu commands will not work)
		dpp::snowflake slash_command_snowflake(const std::string& slash_command);

		// Add (late-load) a module to the bot - returns true on success
		// Returns false if called too early (must be after on_ready_init)
		// Also returns false if the module failed to load or is disabled by config/settings file
		bool add_module(module* module_to_add);

		// Remove (early-unload) a module from the bot - returns true on success
		// Returns false if this module is not loaded
		bool remove_module(module* module_to_remove);

		// Returns a copy of the dpp::user who owns this app/instance
		inline auto app_owner() { std::shared_lock _(app_owner_mutex_); return app_owner_; }

		// Load global, user or guild-specific bot data
		fixedphilip::file::result load_data(dpp::snowflake id, const std::string& name, bot::data& data_out);

		// Save global, user or guild-specific bot data
		// In addition to base::save return values, also returns 'r_write_error' if we're over our size quota
		fixedphilip::file::result save_data(dpp::snowflake id, const std::string& name, const bot::data& data);

		// Returns the current size of all bot data
		// The maximum value can be found under settings()
		uintmax_t data_size_total();

		// Returns the current size of bot data for this ID
		// The maximum value can be found under settings()
		uintmax_t data_size_id(dpp::snowflake id);

		// Given an ID, return the respective bot data folder
		std::filesystem::path data_folder_id(dpp::snowflake id);

		// Server and user counts, for the servers the bot is currently in, as well as all the (guild and user install) users it can see
		struct counts
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
		dpp::task<counts> co_get_counts();
	};

	template <typename T>
	const T* get_if(const std::string& log_prefix, const dpp::confirmation_callback_t& result)
	{
		auto cluster = result.bot;
		if (result.is_error())
		{
			auto error = std::format("{} - {}", log_prefix, result.get_error().human_readable);
			if (cluster)
			{
				cluster->log(dpp::ll_error, error);
			}
			else
			{
				fixedphilip::log::error(error);
			}
			return nullptr;
		}

		if (auto value = std::get_if<T>(&result.value))
		{
			return value;
		}

		throw std::logic_error(std::format("{} - wrong result.value type T", log_prefix));
	}
}