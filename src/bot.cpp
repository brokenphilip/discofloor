#include <discofloor/bot.h>

#include <bulbtils/string.h>
#include <bulbtils/time.h>

namespace discofloor
{
    nlohmann::json bot_settings::struct_to_json(const bulbtils::file::settings& save_settings) const
    {
        return
        {
            { "prefix", prefix },
            { "disabled_modules", disabled_modules },
            { "data_folder", data_folder },
            { "max_data_size_id", bulbtils::file::size_to_string(max_data_size_id) },
            { "max_data_size_total", bulbtils::file::size_to_string(max_data_size_total) },
        };
    }

    bool bot_settings::json_to_struct(const nlohmann::json& data, const bulbtils::file::settings& load_settings)
    {
        json_try_at(data, load_settings, "prefix", prefix, true);
        json_try_at(data, load_settings, "disabled_modules", disabled_modules, true);
        json_try_at(data, load_settings, "data_folder", data_folder, true);

        std::string max_data_size_id_str;
        if (json_try_at(data, load_settings, "max_data_size_id", max_data_size_id_str, true))
        {
            try
            {
                max_data_size_id = static_cast<uintmax_t>(bulbtils::file::string_to_size(max_data_size_id_str));
            }
            catch (std::exception& e)
            {
                load_settings.error(std::format("Failed to parse 'max_data_size_id' for bot settings - {}", e.what()));
            }
        }

        std::string max_data_size_total_str;
        if (json_try_at(data, load_settings, "max_data_size_total", max_data_size_total_str, true))
        {
            try
            {
                max_data_size_id = static_cast<uintmax_t>(bulbtils::file::string_to_size(max_data_size_total_str));
            }
            catch (std::exception& e)
            {
                load_settings.error(std::format("Failed to parse 'max_data_size_total' for bot settings - {}", e.what()));
            }
        }

        // partial load is okay
        return true;
    }

    nlohmann::json bot_config::struct_to_json(const bulbtils::file::settings& save_settings) const
    {
        nlohmann::json data
        {
            { "token", token },
        };
        data.update(settings.struct_to_json(save_settings));
        return data;
    }

    bool bot_config::json_to_struct(const nlohmann::json& data, const bulbtils::file::settings& load_settings)
    {
        // create a copy of the data we will pass down to settings, but without the token
        auto data_copy = data;
        bool token_valid = json_try_at(data_copy, load_settings, "token", token, true);
        data_copy.erase("token");
        return settings.json_to_struct(data_copy, load_settings);
    }

    bool bot_config::load_check_save(const bulbtils::file::settings& file_settings)
    {
        bulbtils::file::settings new_file_settings = file_settings;
        new_file_settings.create_if_not_found = true;

        auto result = load(new_file_settings);
        if (result == bulbtils::file::r_file_not_found)
        {
            new_file_settings.warning("Default config saved - make sure to update your bot token");
            return false;
        }
        else if (result != bulbtils::file::r_success)
        {
            // logs are already printed for us
            return false;
        }

        if (token == DISCOFLOOR_DEFAULT_TOKEN || token.empty())
        {
            new_file_settings.error("Bot token not set in config file");
            return false;
        }

        // using this opportunity to add any new keys that might not exist
        save(new_file_settings);
        return true;
    }

    bot* run_event::get_bot() const
    {
        return std::visit([](auto& event_dispatch)
        {
            return static_cast<bot*>(event_dispatch.owner);
        },
        *this);
    }

    const dpp::interaction_create_t* run_event::get_interaction_create() const
    {
        return std::visit([](auto&& event) -> const dpp::interaction_create_t*
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                return &event;
            }
            else
            {
                return nullptr;
            }
        },
        *this);
    }

    const dpp::event_dispatch_t& run_event::event_dispatch() const
    {
        return std::visit([](auto& event_dispatch) -> const dpp::event_dispatch_t&
        {
            return event_dispatch;
        },
        *this);
    }

    dpp::user run_event::get_command_invoker() const
    {
        return std::visit([](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                return event.command.usr;
            }
            else
            {
                return event.msg.author;
            }
        },
        *this);
    }

    void run_event::reply(const dpp::message& msg, dpp::command_completion_event_t callback) const
    {
        std::visit([&msg, &callback](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                event.reply(msg, callback);
            }
            else
            {
                event.reply(msg, false, callback);
            }
        },
        *this);
    }

    dpp::async<dpp::confirmation_callback_t> run_event::co_reply(const dpp::message& msg) const
    {
        return std::visit([&msg](auto&& event)
        {
            return event.co_reply(msg);
        },
        *this);
    }

    void run_event::thinking_start() const
    {
        std::visit([](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                event.thinking();
            }
            else
            {
                event.owner->channel_typing(event.msg.channel_id);
            }
        },
        *this);
    }

    dpp::async<dpp::confirmation_callback_t> run_event::co_thinking_start() const
    {
        return std::visit([](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                return event.co_thinking();
            }
            else
            {
                return event.owner->co_channel_typing(event.msg.channel_id);
            }
        },
        *this);
    }

    void run_event::thinking_end(const dpp::message& msg, dpp::command_completion_event_t callback) const
    {
        return std::visit([&msg, &callback](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                return event.edit_original_response(msg, callback);
            }
            else
            {
                return event.reply(msg, false, callback);
            }
        },
        *this);
    }

    void run_event::reply_not_impl_use_other() const
    {
        std::visit([](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_same_v<dpp::slashcommand_t, T>)
            {
                auto prefix = static_cast<bot*>(event.owner)->settings().prefix;
                if (prefix.empty())
                {
                    event.reply(":warning: **| Not implemented.**");
                    return;
                }
                event.reply(":warning: **| Not implemented, use `" + prefix + event.command.get_command_name() + "` instead.**");
            }
            else if constexpr (std::is_same_v<discofloor::message_command_t, T>)
            {
                event.reply(":warning: **| Not implemented, use " + event.command_interaction().get_mention() + " instead.**");
            }
            else
            {
                throw std::logic_error("This function is not supported for context menu slashcommands");
            }
        },
        *this);
    }

    dpp::task<void> bot::ready_event(const dpp::ready_t& event)
    {
        if (dpp::run_once<struct ready_event_init>())
        {
            auto cluster = static_cast<bot*>(event.owner);
            cluster->ready_init_done_ = true;
            cluster->log(dpp::ll_info, std::format("Connected and logged in as: {} ({})", cluster->me.format_username(), std::to_string(cluster->me.id)));
            cluster->create_commands_async();
        }
        co_return;
    }

    void bot::create_commands_async()
    {
        // we must create a local copy specifically to pass to global_bulk_command_create()
        std::vector<dpp::slashcommand> commands;

        // iteratively erase all module commands in case we're updating them via module late-load
        {
            std::unique_lock _(module_commands_mutex_);

            auto module_command = module_commands_.begin();
            while (module_command != module_commands_.end())
            {
                if (module_command->type == dpp::ctxm_chat_input)
                {
                    on_slashcommand.detach(module_command->event_handles[0]);
                    if ((intents & dpp::i_message_content) != 0)
                    {
                        on_message_create.detach(module_command->event_handles[1]);
                    }
                }
                else if (module_command->type == dpp::ctxm_message)
                {
                    on_message_context_menu.detach(module_command->event_handles[0]);
                }
                else if (module_command->type == dpp::ctxm_user)
                {
                    on_user_context_menu.detach(module_command->event_handles[0]);
                }
                module_command = module_commands_.erase(module_command);
            }

            {
                std::shared_lock _(loaded_modules_mutex_);

                for (auto& loaded_module : loaded_modules_)
                {
                    for (auto& command : loaded_module->commands(*this))
                    {
                        module_commands_.emplace_back(command);
                        commands.push_back(command);
                    }
                }
            }
        }

        // as this function's name implies, the lambda will run asynchronously(!!!) and NOT when this function is called
        global_bulk_command_create(commands, [](const dpp::confirmation_callback_t& result) -> dpp::task<void>
        {
            // HACK: you can't really do any of this safely anyways, might as well cast away the const
            auto cluster = static_cast<bot*>(const_cast<dpp::cluster*>(result.bot));

            if (result.is_error())
            {
                cluster->log(dpp::ll_error, "Command creation failed - " + result.get_error().human_readable);
                co_return;
            }
            auto command_map = result.get<dpp::slashcommand_map>();

            // we take the command_map results instead of our own (later down the line)
            // because we want to informatively print which commands in specific discord has created
            // ie. if any mistakes or errors happen below, we'll just print logs separately
            auto result_log = std::format("Created {} command{}", command_map.size(), command_map.size() == 1 ? "" : "s");

            bool first_command = true;
            {
                std::unique_lock _(cluster->module_commands_mutex_);

                for (const auto& [snowflake, command] : command_map)
                {
                    if (first_command)
                    {
                        result_log += ": ";
                    }
                    else
                    {
                        result_log += ", ";
                    }
                    first_command = false;

                    std::string type;
                    switch (command.type)
                    {
                        case dpp::ctxm_chat_input: type = "CHAT_INPUT"; break;
                        case dpp::ctxm_user: type = "USER"; break;
                        case dpp::ctxm_message: type = "MESSAGE"; break;
                    }
                    result_log += "'" + command.name + "' (" + type + ")";

                    // find module command from the slashcommand map we're given (they're identical if their names AND TYPES match)
                    auto module_command = std::find_if(cluster->module_commands_.begin(), cluster->module_commands_.end(), [&command](const bot::module_command& other)
                    {
                        return command.name == other.name && command.type == other.type;
                    });

                    // note that we're creating a copy of the module command, not keeping a reference to it,
                    // ...because this lambda will run asynchronously at a later point, and the references would dangle in that case
                    auto event_router_async = [prefix = cluster->settings().prefix, command = *module_command](const auto& event) -> dpp::task<void>
                    {
                        using T = std::decay_t<decltype(event)>;

                        if constexpr (std::is_same_v<T, dpp::message_create_t>)
                        {
                            // we don't want bots to run our commands
                            if (event.msg.author.is_bot())
                            {
                                co_return;
                            }

                            if (!prefix.empty())
                            {
                                auto chat_command = std::format("{}{}", prefix, command.name);

                                //dpp::command_interaction command; // todo

                                if (event.msg.content == chat_command)
                                {
                                    co_await command.run(run_event(discofloor::message_command_t(event, {})));
                                }
                                else if (event.msg.content.starts_with(chat_command + " "))
                                {
                                    // the first token will always be the command itself, since slashcommands can't have spaces
                                    // message/user context menu commands can, however, have spaces, but we don't care about those here
                                    auto chat_tokens = bulbtils::string::split_by_whitespace(event.msg.content);
                                    //std::vector<dpp::command_data_option> options;

                                    /*
                                    
                                        TODO

                                        if core_cmd.options.size == 0, don't bother checking for chat tokens

                                        if core_cmd.options.size > 0, there are two possibilities:
                                        - if the first option is a subcmd (group), all the others are too
                                        - if it's not, all the options are params

                                        if core_cmd.options[i] is a subcmd group, all of its options must be subcmds, and all subcmd options must be params

                                        if core_cmd.options[i] is a subcmd, all of its options must be params

                                    */

                                    co_await command.run(run_event(discofloor::message_command_t(event, {})));
                                }
                            }
                        }
                        else if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
                        {
                            if (event.command.get_command_name() == command.name)
                            {
                                co_await command.run(run_event(event));
                            }
                        }
                        else
                        {
                            // can't use false here, or it will never compile (as always, thanks raymond chen :3)
                            // https://devblogs.microsoft.com/oldnewthing/20200311-00/?p=103553
                            static_assert(!sizeof(T*), "Unsupported type T");
                        }
                    };

                    if (module_command->type == dpp::ctxm_chat_input)
                    {
                        module_command->event_handles[0] = cluster->on_slashcommand.attach(event_router_async);

                        if ((cluster->intents & dpp::i_message_content) != 0)
                        {
                            module_command->event_handles[1] = cluster->on_message_create.attach(event_router_async);
                        }
                    }
                    else if (module_command->type == dpp::ctxm_message)
                    {
                        module_command->event_handles[0] = cluster->on_message_context_menu.attach(event_router_async);
                    }
                    else if (module_command->type == dpp::ctxm_user)
                    {
                        module_command->event_handles[0] = cluster->on_user_context_menu.attach(event_router_async);
                    }
                }
            }

            // all commands iterated, print resulting log
            cluster->log(dpp::ll_info, result_log);
        });
    }

    void bot::fetch_app_info_async()
    {
        // as this function's name implies, the lambda will run asynchronously(!!!) and NOT when this function is called
        // it CAN'T be run synchronously - if we block the thread, the REST API request queue NEVER gets serviced !!!
        current_application_get([](const dpp::confirmation_callback_t& result) -> dpp::task<void>
        {
            // HACK: you can't really do any of this safely anyways, might as well cast away the const
            auto cluster = static_cast<bot*>(const_cast<dpp::cluster*>(result.bot));

            if (result.is_error())
            {
                cluster->log(dpp::ll_error, "Failed to fetch app info - " + result.get_error().human_readable);
                co_return;
            }
            auto app = result.get<dpp::application>();

            auto& app_owner = app.owner;
            cluster->app_owner_ = app_owner;
            cluster->log(dpp::loglevel::ll_info, std::format("Application (instance) owner is: {} ({})", app_owner.username, std::to_string(app_owner.id)));

            // check for any privileged intents - if we don't have permission to use them, disable them
            uint32_t intents_to_disable = 0;

            if (!(app.flags & (dpp::apf_gateway_guild_members_limited | dpp::apf_gateway_guild_members)) && ((cluster->intents & dpp::i_guild_members) != 0))
            {
                cluster->log
                (
                    dpp::ll_warning,
                    "The 'Guild Members' privileged intent was requested but is not enabled for this application. "
                    "Features that require 'on_guild_member_add/remove' (when users join or leave a server), "
                    "'on_guild_member_update' (when a user's server info is updated) or complete member lists of servers, "
                    "such as displaying accurate statistics as to how many users the bot is serving, will not work for this session. "
                    "Visit the Discord Developer Portal page for your application/bot to enable the intent and fix this issue."
                );
                intents_to_disable |= dpp::i_guild_members;
            }

            if (!(app.flags & (dpp::apf_gateway_presence_limited | dpp::apf_gateway_presence)) && ((cluster->intents & dpp::i_guild_presences) != 0))
            {
                cluster->log
                (
                    dpp::ll_warning,
                    "The 'Guild Presences' privileged intent was requested but is not enabled for this application. "
                    "Features that require user presence (status, activities) updates will not work for this session. "
                    "Visit the Discord Developer Portal page for your application/bot to enable the intent and fix this issue."
                );
                intents_to_disable |= dpp::i_guild_presences;
            }

            if (!(app.flags & (dpp::apf_gateway_message_content_limited | dpp::apf_gateway_message_content)) && ((cluster->intents & dpp::i_message_content) != 0))
            {
                cluster->log
                (
                    dpp::ll_warning,
                    "The 'Message Content' privileged intent was requested but is not enabled for this application. "
                    "Features that require 'on_message_create' (when a message is sent) or 'on_message_update' "
                    "(when a message is edited), such as old-style prefix commands, will not work for this session. "
                    "Visit the Discord Developer Portal page for your application/bot to enable the intent and fix this issue."
                );
                intents_to_disable |= dpp::i_message_content;

                // disable on_message_create to prevent log spam
                std::shared_lock _(cluster->module_commands_mutex_);
                for (auto& command : cluster->module_commands_)
                {
                    if (command.type == dpp::ctxm_chat_input)
                    {
                        cluster->on_message_create.detach(command.event_handles[1]);
                    }
                }
            }

            // shards that have already started will be stuck in a reconnect loop if we don't fix their intents
            // we don't need to reconnect them manually - they'll automatically reconnect anyways
            // or, if we manage to update the intent before the initial connection, there won't be a need for a reconnect
            if (intents_to_disable)
            {
                cluster->intents &= ~intents_to_disable;

                // HACK: ideally we'd use a unique_lock for the cluster's shards_mutex, but it is inaccessible (private)
                for (auto& shard : cluster->get_shards())
                {
                    auto client = shard.second;
                    if (client)
                    {
                        client->intents &= ~intents_to_disable;
                    }
                }
            }
        });
    }

    bulbtils::file::settings bot::data_file_settings(dpp::snowflake id, const std::string& name)
    {
        auto filename = name + ".json";
        auto data_path = data_folder_id(id) / filename;

        bulbtils::file::settings file_settings
        {
            .filename = data_path.string(),
            .create_if_not_found = true,
        };
        return append_loggers(file_settings);
    }

    void bot::after_construction()
    {
        // attach our own events first (modules do it in their own inits, but we do their commands ourselves later)
        on_ready.attach(ready_event);

        // we're doing this here instead of on_ready_init because we want this to run as soon as possible
        // to ideally avoid restarting clusters/shards after potentially fixing up their intents
        fetch_app_info_async();

        if (settings_.prefix.empty())
        {
            log(dpp::ll_info, "Old-style commands disabled (prefix is blank)");
        }
        else
        {
            log(dpp::ll_info, "Global prefix for old-style commands set to '" + settings_.prefix + "'");
        }

        bool first_module = true;
        std::string module_names = "";

        // initialize modules alphabetically by their name
        // their commands are created in on_ready_init
        auto iter = module::first();
        if (!iter)
        {
            log(dpp::ll_info, "No modules to load");
            return;
        }

        do
        {
            std::string name = iter->name();
            if (std::find(settings_.disabled_modules.begin(), settings_.disabled_modules.end(), name) != settings_.disabled_modules.end())
            {
                log(dpp::ll_info, "Module '" + name + "' is listed in disabled_modules and will not be initialized");
                continue;
            }

            if (!iter->init(*this))
            {
                continue;
            }

            if (first_module)
            {
                module_names += ": '" + name + "'";
            }
            else
            {
                module_names += ", '" + name + "'";
            }
            first_module = false;
            loaded_modules_.push_back(iter);
        }
        while (iter = iter->next());

        log(dpp::ll_info, std::format("Loaded {} module{}{}", loaded_modules_.size(), loaded_modules_.size() == 1 ? "" : "s", module_names));
    }

    bot::~bot()
    {
        // destroy loaded modules in reverse order of initialization
        for (auto& loaded_module : std::views::reverse(loaded_modules_))
        {
            loaded_module->destroy(*this);
        }
    }

    bulbtils::file::settings& bot::append_loggers(bulbtils::file::settings& file_settings)
    {
        file_settings.warning_callback = [&cluster = *this](const std::string& msg) { cluster.log(dpp::ll_warning, msg); };
        file_settings.error_callback = [&cluster = *this](const std::string& msg) { cluster.log(dpp::ll_error, msg); };
        return file_settings;
    }

    std::string bot::format_running_time()
    {
        std::string str;
        auto elapsed = running_time_.elapsed<std::chrono::seconds>();
        if (elapsed > std::chrono::days(1))
        {
            auto days = std::chrono::duration_cast<std::chrono::days>(elapsed);
            elapsed -= days;
            str = std::format("{} {:%Hh %Mm}", days, elapsed);
        }
        else if (elapsed > std::chrono::hours(1))
        {
            str = std::format("{:%Hh %Mm %Ss}", elapsed);
        }
        else if (elapsed > std::chrono::minutes(1))
        {
            str = std::format("{:%Mm %Ss}", elapsed);
        }
        else
        {
            str = std::format("{:%Ss}", elapsed);
        }
        return str;
    }

    bool bot::add_module(module* module_to_add)
    {
        if (!module_to_add)
        {
            log(dpp::ll_error, "Attempted to add a null module");
            return false;
        }
        auto name = module_to_add->name();
        if (!ready_init_done_)
        {
            log(dpp::ll_error, std::format("Attempted to add module '{}' before the bot's on_ready_init completed", name));
            return false;
        }
        if (std::find(settings_.disabled_modules.begin(), settings_.disabled_modules.end(), name) != settings_.disabled_modules.end())
        {
            log(dpp::ll_error, std::format("Attempted to add module '{}' which is listed in disabled_modules", name));
            return false;
        }
        if (!module_to_add->init(*this))
        {
            // module itself did not want to be added
            return false;
        }
        loaded_modules_.insert(std::lower_bound(loaded_modules_.begin(), loaded_modules_.end(), module_to_add, [](const module* a, const module* b) { return strcmp(a->name(), b->name()) < 0; }), module_to_add);
        create_commands_async();
        return true;
    }

    bool bot::remove_module(module* module_to_remove)
    {
        if (!module_to_remove)
        {
            log(dpp::ll_error, "Attempted to remove a null module");
            return false;
        }
        auto it = std::find(loaded_modules_.begin(), loaded_modules_.end(), module_to_remove);
        if (it == loaded_modules_.end())
        {
            log(dpp::ll_error, std::format("Attempted to remove module '{}' which isn't even loaded", module_to_remove->name()));
            return false;
        }
        module_to_remove->destroy(*this);
        loaded_modules_.erase(it);
        return true;
    }

    bulbtils::file::result bot::load_data(dpp::snowflake id, const std::string& name, bot_data& data_out)
    {
        return data_out.load(data_file_settings(id, name));
    }

    bulbtils::file::result bot::save_data(dpp::snowflake id, const std::string& name, const bot_data& data)
    {
        auto file_settings = data_file_settings(id, name);

        try
        {
            auto data_size = data.save_from_struct(file_settings).size();
            if (data_size_id(id) + data_size > settings_.max_data_size_id
                || data_size_total() + data_size > settings_.max_data_size_total)
            {
                return bulbtils::file::r_write_error;
            }
        }
        catch (std::exception& e)
        {
            return bulbtils::file::r_write_error;
        }
        return data.save(file_settings);
    }

    uintmax_t bot::data_size_total()
    {
        return bulbtils::file::get_folder_size(settings_.data_folder);
    }

    uintmax_t bot::data_size_id(dpp::snowflake id)
    {
        return bulbtils::file::get_folder_size(data_folder_id(id));
    }

    std::filesystem::path bot::data_folder_id(dpp::snowflake id)
    {
        return std::filesystem::path(settings_.data_folder) / std::to_string(id);
    }

    dpp::task<bot_counts> bot::co_get_counts()
    {
        bot_counts counts;
        auto guild_cache = dpp::get_guild_cache();
        if (!guild_cache)
        {
            co_return counts;
        }

        std::vector<dpp::snowflake> users;

        int server_count = 0;

        // this fallback is used in case we lack the necessary intent for accurate results
        int fallback_user_count = 0;

        // we must lock the mutex while we're using the cache
        {
            std::shared_lock _(guild_cache->get_mutex());
            auto& guilds = guild_cache->get_container();

            for (const auto& [guild_snowflake, guild] : guilds)
            {
                if (!guild)
                {
                    continue;
                }
                for (const auto& [member_snowflake, member] : guild->members)
                {
                    auto user = member.get_user();
                    if (!user)
                    {
                        continue;
                    }
                    if (user->is_bot())
                    {
                        continue;
                    }
                    users.push_back(member_snowflake);
                }
                fallback_user_count += guild->member_count;
            }
            server_count = guilds.size();
        }

        std::sort(users.begin(), users.end());
        auto last = std::unique(users.begin(), users.end());
        users.erase(last, users.end());

        // cache these for later use, as we will only call the api once per interval
        static int user_install_count = -1;
        static bool has_guild_members_intent = false;

        static auto next_call = std::chrono::minutes(1);
        if (bulbtils::time::run_if_passed<struct fetch_app_data>(next_call))
        {
            auto result = co_await co_current_application_get();
            if (result.is_error())
            {
                // cached values are good enough, but try to update them again a bit later
                next_call = std::chrono::minutes(1);

                log(dpp::ll_error, "Failed to fetch app counts - " + result.get_error().human_readable);
                co_return counts;
            }
            auto app = result.get<dpp::application>();

            // these update daily, so one hour is generous enough
            next_call = std::chrono::minutes(60);

            user_install_count = app.approximate_user_install_count;
            has_guild_members_intent = (app.flags & (dpp::apf_gateway_guild_members_limited | dpp::apf_gateway_guild_members));
        }

        counts.servers = server_count;
        counts.users = has_guild_members_intent ? users.size() : fallback_user_count;
        counts.users_fallback = !has_guild_members_intent;
        counts.user_installs = user_install_count;
        counts.total_users = user_install_count >= 0 ? counts.users + user_install_count : -1;
        co_return counts;
    }
}