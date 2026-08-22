#pragma once

#include <discofloor/json.h>

#include <bulbtils/named_node.h>
#include <bulbtils/time.h>

#include <dpp/dpp.h>

#include <variant>

#define DISCOFLOOR_DEFAULT_TOKEN "your_bot_token_here"

namespace discofloor
{
	// Get a user's username, appending the discrim for bots (for humans it does not)
	std::string get_username(const dpp::user& user);

	dpp::message error_message(const std::string& message, uint32_t accent = UINT_MAX);

	class bot;

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

	// Message create event, modified to support commands, used in run_event
	class message_command_t : public dpp::message_create_t
	{
		dpp::command_interaction data_;
    public:
		message_command_t(const dpp::message_create_t& message_create, const dpp::command_interaction& data)
			: dpp::message_create_t(message_create), data_(data) {}

		dpp::command_interaction command_interaction() const { return data_; }
	};

	// DO NOT USE! You most certainly want run_event instead
	using run_event_base = std::variant<discofloor::message_command_t, dpp::slashcommand_t, dpp::message_context_menu_t, dpp::user_context_menu_t>;

	// Event received when a discofloor command's run function is executed (see class below), supporting:
	// - old-style (chat prefix) commands, unless disabled (blank prefix)
    // - "CHAT_INPUT" ie. regular (chat) slash commands
    // - "USER" ie. (right-click) user context menu commands
    // - "MESSAGE" ie. (right-click) message context menu commands
    struct run_event : public run_event_base
    {
        using run_event_base::run_event_base;

        bot* get_bot() const;

        inline auto get_message_command() const { return std::get_if<discofloor::message_command_t>(this); }
        inline auto get_slash_command() const { return std::get_if<dpp::slashcommand_t>(this); }
        inline auto get_message_context_menu() const { return std::get_if<dpp::message_context_menu_t>(this); }
        inline auto get_user_context_menu() const { return std::get_if<dpp::user_context_menu_t>(this); }

        // For any type of slashcommand-based run_event, get the underlying interaction event
		// Returns null if the run_event is not a slashcommand
        const dpp::interaction_create_t* get_interaction_create() const;

        // Get the underlying event dispatch for any run_event variant
        const dpp::event_dispatch_t& event_dispatch() const;

		// Get the user who ran the command
        dpp::user command_invoker() const;

		// Get the guild the command was run in, if any
		dpp::guild* get_guild() const;

		// Get the channel the command was run in, if any
		dpp::channel* get_channel() const;

		// Get the details of the executed command
		dpp::command_interaction command_interaction() const;

		// Reply to the command invoker
        void reply(const dpp::message& msg, dpp::command_completion_event_t callback = dpp::utility::log_error()) const;
        inline void reply(const std::string& msg, dpp::command_completion_event_t callback = dpp::utility::log_error()) const { reply(dpp::message(msg), callback); }

        dpp::async<dpp::confirmation_callback_t> co_reply(const dpp::message& msg) const;
        inline dpp::async<dpp::confirmation_callback_t> co_reply(const std::string& msg) const { return co_reply(dpp::message(msg)); }

        // Remember to use thinking_end() instead of reply() after using thinking_start()
        // Additionally, if using the coroutine, make sure to co_await before ending the think
        void thinking_start() const;
        dpp::async<dpp::confirmation_callback_t> co_thinking_start() const;

        void thinking_end(const dpp::message& msg, dpp::command_completion_event_t callback = dpp::utility::log_error()) const;
        inline void thinking_end(const std::string& msg, dpp::command_completion_event_t callback = dpp::utility::log_error()) const { thinking_end(dpp::message(msg), callback); }

		dpp::command_value get_cmd_param(const std::string& param_name) const;

		// Given the name of a required parameter of a command, fetch its value
		template <typename T>
		T get_cmd_required_param_value(const std::string& param_name) const
		{
			return std::get<T>(get_cmd_param(param_name));
		}

		// Given the name of an optional parameter of a command, try to fetch its value
		// Returns the value if found, or default_value otherwise
		template <typename T>
		T get_cmd_optional_param_value(const std::string& param_name, T default_value) const
		{
			if (auto param = get_cmd_param(param_name); auto value = std::get_if<T>(&param))
			{
				return *value;
			}
			return default_value;
		}
    };

	// Type of function used by discofloor commands to receive run events
	using run_fn = std::function<dpp::task<void>(const run_event&)>;

	// Slash command wrapper with an accompanying run function that receives run events (see struct above)
    class command : public dpp::slashcommand
    {
		std::vector<std::string> chat_command_examples_;
        run_fn run_fn_;
    public:
		inline command(const std::string& name, const std::string& description, const dpp::snowflake application_id, run_fn run_fn, const std::vector<std::string>& chat_command_examples = {})
            : dpp::slashcommand(name, description, application_id), run_fn_(run_fn), chat_command_examples_(chat_command_examples) {}

        inline command(const std::string& name, const dpp::slashcommand_contextmenu_type type, const dpp::snowflake application_id, run_fn run_fn)
            : dpp::slashcommand(name, type, application_id), run_fn_(run_fn) {}

		inline std::vector<std::string> chat_command_examples() { return chat_command_examples_; }

        inline auto get_run_fn() const { return run_fn_; }
        inline dpp::task<void> run(const run_event& event) const { co_await run_fn_(event); }
    };

	// Base module interface - inherit this class to create your own custom module
	//
	// Modules are, by design, meant to be single-instance ("static"), but they don't necessarily have to be
    // For dynamically allocated ("dynamic") modules, you can use bot::add_module() and bot::remove_module()
    // 
    // When a module ("static" or "dynamic") is being instantiated, it gets added to the internal linked list of modules
	// This linked list is iterated for each discofloor bot as described in the virtual functions below
	// Since "dynamic" modules are also added to the linked list, be careful when instantiating them
    struct module : public bulbtils::named_node<module>
    {
        inline module(const char* name) : named_node<module>(name) {}
        inline virtual ~module() {}

		// This function is called when a discofloor bot is being constructed
		// Alternatively, it is also called when the module is being late-loaded using bot::add_module()
        // Modules are always initialized in alphabetical order, based on their name
		//
		// Return true to let the bot load your module, false otherwise (eg. if something goes wrong)
		// If you're returning false, it is advised to use "bot.log(...)" to provide an explanation
		//
		// By default, this virtual function just returns true and doesn't do anything else
		// If you don't need to run any initialization code, you don't need to override it
		// 
        // NOTE: Modules can additionally be disabled using the "disabled_modules" bot setting
		// In that case, this function will NOT be called, and the module will NOT be initialized
		//
        // NOTE: Since this is called during the module internal linked list iteration,
		// ...do NOT create new (dynamic) modules within this function
        inline virtual bool init(bot& bot) { return true; }

		// This function is called when a discofloor bot is requesting all loaded modules' commands
        // This usually happens a short while after init() - during the bot's initial on_ready event
        // For late-loaded commands using bot::add_module(), this function is called right after init()
        inline virtual std::vector<command> commands(bot& bot) { return {}; }

		// This function is called when a discofloor bot is being destructed
        // Alternatively, it is also called when the module is being early-unloaded using bot::remove_module()
        // Modules are always destroyed in reverse-alphabetical order, based on their name
		//
		// By default, this virtual function doesn't do anything
        // If you don't need to run any destruction code, you don't need to override it
        inline virtual void destroy(bot& bot) {}
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
		template <typename T>
		using event_t = dpp::task<void>(const T& event);

		bot_settings settings_;

		const bulbtils::time::raii_stopwatch running_time_;
		const std::chrono::system_clock::time_point start_time_ = std::chrono::system_clock::now();

		std::vector<module*> loaded_modules_;
		std::shared_mutex loaded_modules_mutex_;

		struct module_command : public command
		{
			bot* owner;
			dpp::event_handle event_handles[3] { SIZE_MAX };
			
			inline module_command(const command& cmd, bot* owner)
				: command(cmd), owner(owner) {}

			dpp::message usage(const std::string& subcmd_group = "", const std::string& subcmd = "");

			event_t<dpp::message_create_t> message_create_event;

			void attach_events();
			void detach_events();
			void detach_message_content_events();
		};
		std::vector<module_command> module_commands_;
		std::shared_mutex module_commands_mutex_;

		std::function<void(command&)> for_each_command_;

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

		inline void for_each_command(std::function<void(command&)> callback) { for_each_command_ = std::move(callback); }

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
		bool add_module(module* module_to_add);

		// Remove (early-unload) a module from the bot - returns true on success
		// Returns false if this module is not loaded
		bool remove_module(module* module_to_remove);

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