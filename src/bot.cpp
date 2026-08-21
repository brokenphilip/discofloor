#include <discofloor/bot.h>

#include <bulbtils/string.h>
#include <bulbtils/time.h>

namespace discofloor
{
    std::string get_username(const dpp::user& user)
    {
        auto username = user.username;

        auto discrim = user.discriminator;
        if (discrim)
        {
            username += "#" + dpp::leading_zeroes(discrim, 4);
        }
        return username;
    }

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

        // using this opportunity to add any new keys that might not exist
        save(new_file_settings);

        if (token == DISCOFLOOR_DEFAULT_TOKEN || token.empty())
        {
            new_file_settings.error("Bot token not set in config file");
            return false;
        }

        std::filesystem::path path = settings.data_folder;
        if (!path.empty())
        {
            try
            {
                if (std::filesystem::create_directories(path))
                {
                    file_settings.warning("Missing path directories for data folder '" + settings.data_folder + "' - creating...");
                }
            }
            catch (std::exception& e)
            {
                file_settings.error(std::format("Failed to create missing path directories for data folder '{}' - {}", settings.data_folder, e.what()));
                return false;
            }
        }
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

    dpp::guild* run_event::get_guild() const
    {
        return std::visit([](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                return dpp::find_guild(event.command.guild_id);
            }
            else
            {
                return dpp::find_guild(event.msg.guild_id);
            }
        },
        *this);
    }

    dpp::command_interaction run_event::command_interaction() const
    {
        return std::visit([](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                return event.command.get_command_interaction();
            }
            else
            {
                return event.command_interaction();
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

    dpp::command_value run_event::get_cmd_param(const std::string& param_name) const
    {
        return std::visit([&param_name](auto&& event) -> dpp::command_value
        {
            using T = std::decay_t<decltype(event)>;

            // can't use is_base_of_v here because of a bug pertaining to get_parameter incorrectly being classified as inaccessible
            if constexpr (std::is_same_v<dpp::slashcommand_t, T>)
            {
                return event.get_parameter(param_name);
            }
            else if constexpr (std::is_same_v<discofloor::message_command_t, T>)
            {
                auto command_interaction = event.command_interaction();
                for (const auto& option : command_interaction.options)
                {
                    if (option.type != dpp::co_sub_command && option.type != dpp::co_sub_command_group && option.name == param_name)
                    {
                        return option.value;
                    }
                }

                /* if not found in the first level, go one level deeper */
                for (const auto& option : command_interaction.options)
                {
                    // command
                    for (const auto& sub_option : option.options)
                    {
                        // subcommands
                        if (sub_option.type != dpp::co_sub_command && sub_option.type != dpp::co_sub_command_group && sub_option.name == param_name)
                        {
                            return sub_option.value;
                        }
                    }
                }

                /* if not found in the second level, search it in the third dimension */
                for (const auto& option : command_interaction.options)
                {
                    // command
                    for (const auto& sub_group_option : option.options)
                    {
                        // subcommand groups
                        for (const auto& sub_option : sub_group_option.options)
                        {
                            // subcommands
                            if (sub_option.type != dpp::co_sub_command && sub_option.type != dpp::co_sub_command_group && sub_option.name == param_name)
                            {
                                return sub_option.value;
                            }
                        }
                    }
                }
                return {};
            }
            else
            {
                throw std::logic_error("get_cmd_param is not supported for context menu slashcommands");
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
            cluster->log(dpp::ll_info, std::format("Connected and logged in as: {} ({})", discofloor::get_username(cluster->me), std::to_string(cluster->me.id)));
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

                    // note that we're creating a copy of the module command (which is perfectly valid), not keeping a reference to it
                    // ...because this lambda will run asynchronously at a later point, and the references would dangle in that case
                    auto event_router_async = [prefix = cluster->settings().prefix, command = *module_command](const auto& event) -> dpp::task<void>
                    {
                        using T = std::decay_t<decltype(event)>;

                        // this isn't a mistake - it's still a dpp::message_create_t at this point
                        // ...but we will turn it into a discofloor::message_command_t later down the road
                        if constexpr (std::is_same_v<T, dpp::message_create_t>)
                        {
                            if (prefix.empty())
                            {
                                // old-style commands are disabled
                                co_return;
                            }

                            if (event.msg.author.is_bot())
                            {
                                // bots can't run commands
                                co_return;
                            }

                            auto default_cmd_interaction = [&command, &prefix, &event]() -> dpp::command_interaction
                            {
                                auto prefix_len = prefix.length();

                                dpp::command_interaction cmd_interaction
                                {
                                    .id = command.id,
                                    .name = event.msg.content.substr(prefix_len, event.msg.content.find(' ') - prefix_len),
                                    .options = {},
                                    .type = dpp::ctxm_chat_input,
                                    .target_id = dpp::snowflake(0),
                                };
                                return cmd_interaction;
                            };

                            bool command_has_no_required_params = command.options.empty() || 
                                (!command.options[0].required && command.options[0].type != dpp::co_sub_command_group && command.options[0].type != dpp::co_sub_command);

                            auto chat_command = std::format("{}{}", prefix, command.name);
                            if (event.msg.content == chat_command)
                            {
                                if (command_has_no_required_params)
                                {
                                    // command has no (required) params, we just run as-is
                                    co_await command.run(run_event(discofloor::message_command_t(event, default_cmd_interaction())));
                                    co_return;
                                }

                                // user ran the command without params but this command has at least one required param - print usage help
                                event.reply("usage - todo");
                                co_return;
                            }

                            // we know this command was definitely passed with parameters
                            // ...since discord doesn't allow trailing whitespace at the end of messages
                            if (event.msg.content.starts_with(chat_command + " "))
                            {
                                if (command_has_no_required_params)
                                {
                                    // command has no (required) params, we just run as-is
                                    co_await command.run(run_event(discofloor::message_command_t(event, default_cmd_interaction())));
                                    co_return;
                                }

                                // special case - this command's only option is a string
                                // in which case pass the entire string and run the command
                                if (command.options.size() == 1 && command.options[0].type == dpp::co_string)
                                {
                                    auto cmd_interaction = default_cmd_interaction();

                                    auto& option = cmd_interaction.options.emplace_back();
                                    option.focused = false;
                                    option.name = command.options[0].name;
                                    option.options = {};
                                    option.type = dpp::co_string;
                                    option.value = event.msg.content.substr(event.msg.content.find(' ') + 1);

                                    co_await command.run(run_event(discofloor::message_command_t(event, cmd_interaction)));
                                    co_return;
                                }

                                // the first parameter will always be the command itself, since CHAT_INPUT commands can't have spaces
                                // message/user context menu commands can, however, have spaces, but we don't care about those here
                                auto params = bulbtils::string::split_parameters(event.msg.content);

                                dpp::command_interaction cmd_interaction
                                {
                                    .id = command.id,
                                    .name = params[0].substr(prefix.length()),
                                    .options = {},
                                    .type = dpp::ctxm_chat_input,
                                    .target_id = dpp::snowflake(0),
                                };

                                // this lambda function handles iterating command parameters (ie. non-subcommand(-group)s)
                                auto iterate_command_params = [&params, &event](
                                    const std::vector<dpp::command_option>& source_options,
                                    std::vector<dpp::command_data_option>& dest_options,
                                    int level)
                                {
                                    if (source_options.empty())
                                    {
                                        // this (sub)command doesn't have any parameters, feel free to run as-is
                                        return;
                                    }

                                    auto first_optional_param = std::find_if(source_options.begin(), source_options.end(), [](const dpp::command_option& option)
                                    {
                                        return option.required == false;
                                    });

                                    int required_levels = level + std::distance(source_options.begin(), first_optional_param);
                                    if (params.size() < required_levels)
                                    {
                                        throw std::invalid_argument(std::format("This (sub)command expects at least {} parameter{}, but only {} {} passed.", 
                                            (required_levels - 1), (required_levels - 1) == 1 ? "" : "s", (params.size() - 1), (params.size() - 1) == 1 ? "was" : "were"));
                                    }

                                    struct find_result
                                    {
                                        dpp::snowflake id;
                                        std::string type;
                                        std::string value;

                                        find_result(dpp::snowflake id, const std::string& type, const std::string& value)
                                            : id(id), type(type), value(value) {}
                                    };

                                    using find_results_base = std::vector<find_result>;

                                    struct find_results : public find_results_base
                                    {
                                        using find_results_base::find_results_base;

                                        bool add_if_match(const find_result& find_result, const std::string& needle)
                                        {
                                            if (find_result.value.empty())
                                            {
                                                return false;
                                            }

                                            auto needle_lowercase = needle;
                                            bulbtils::string::inplace::to_lowercase(needle_lowercase);

                                            auto haystack_lowercase = find_result.value;
                                            bulbtils::string::inplace::to_lowercase(haystack_lowercase);

                                            if (haystack_lowercase.find(needle_lowercase) == std::string::npos)
                                            {
                                                return false;
                                            }

                                            push_back(find_result);
                                            return true;
                                        }
                                    };

                                    auto find_users = [&event](const std::string& param) -> find_results
                                    {
                                        auto user = dpp::find_user(dpp::snowflake(param));
                                        if (user)
                                        {
                                            return { { user->id, "User", discofloor::get_username(*user) } };
                                        }

                                        auto guild = dpp::find_guild(event.msg.guild_id);
                                        if (!guild)
                                        {
                                            return {};
                                        }

                                        find_results results;
                                        for (auto& [user_id, guild_member] : guild->members)
                                        {
                                            auto& user_it = *dpp::find_user(user_id);

                                            if (results.add_if_match({ user_id, "User", discofloor::get_username(user_it) }, param))
                                            {
                                                continue;
                                            }

                                            if (results.add_if_match({ user_id, "Global name of user " + discofloor::get_username(user_it), user_it.global_name }, param))
                                            {
                                                continue;
                                            }

                                            if (results.add_if_match({ user_id, "Nickname of user " + discofloor::get_username(user_it), guild_member.get_nickname() }, param))
                                            {
                                                continue;
                                            }
                                        }
                                        return results;
                                    };

                                    auto find_roles = [&event](const std::string& param) -> find_results
                                    {
                                        auto role = dpp::find_role(dpp::snowflake(param));
                                        if (role)
                                        {
                                            return { { role->id, "Role", role->name } };
                                        }

                                        auto guild = dpp::find_guild(event.msg.guild_id);
                                        if (!guild)
                                        {
                                            return {};
                                        }

                                        find_results results;
                                        for (auto& role_id : guild->roles)
                                        {
                                            auto& role_it = *dpp::find_role(role_id);

                                            if (results.add_if_match({ role_id, "Role", role_it.name }, param))
                                            {
                                                continue;
                                            }
                                        }
                                        return results;
                                    };

                                    auto find_channels = [&event](const std::string& param) -> find_results
                                    {
                                        auto channel = dpp::find_channel(dpp::snowflake(param));
                                        if (channel)
                                        {
                                            return { { channel->id, "Channel", channel->name } };
                                        }

                                        auto guild = dpp::find_guild(event.msg.guild_id);
                                        if (!guild)
                                        {
                                            return {};
                                        }

                                        find_results results;
                                        for (auto& channel_id : guild->channels)
                                        {
                                            auto& channel_it = *dpp::find_channel(channel_id);

                                            if (results.add_if_match({ channel_id, "Channel", channel_it.name }, param))
                                            {
                                                continue;
                                            }
                                        }
                                        return results;
                                    };

                                    auto parse_find_results = [](const std::vector<find_result>& results) -> dpp::snowflake
                                    {
                                        std::string blablabla = "matches found for the given input (try narrowing your search, providing an ID, or running the command in a guild (that i'm in) if you aren't already)";
                                        if (results.empty())
                                        {
                                            throw std::invalid_argument("no " + blablabla + ".");
                                        }

                                        if (results.size() > 1)
                                        {
                                            std::string matches = "";
                                            for (int i = 0; i < results.size(); i++)
                                            {
                                                auto& find_result = results[i];
                                                matches += std::format("\n{}. \"{}\" ({}) - `{}`", i + 1, dpp::utility::markdown_escape(find_result.value, true), find_result.type, std::to_string(find_result.id));
                                            }
                                            throw std::invalid_argument("multiple " + blablabla + ":" + matches);
                                        }
                                        return results[0].id;
                                    };

                                    // since attachments aren't part of the message, we need to manually track their index
                                    int current_attachment_index = 0;

                                    for (auto& source_option : source_options)
                                    {
                                        if (level == params.size())
                                        {
                                            // we're out of parameters, but they're optional now so it's fine
                                            break;
                                        }

                                        auto chat_param = params[level];
                                        auto chat_param_lowercase = chat_param;
                                        bulbtils::string::inplace::to_lowercase(chat_param_lowercase);

                                        dpp::command_data_option dest_option_data;
                                        dest_option_data.focused = false;
                                        dest_option_data.name = source_option.name;
                                        dest_option_data.options = {};
                                        dest_option_data.type = source_option.type;

                                        std::string type_name;
                                        switch (dest_option_data.type)
                                        {
                                            case dpp::co_string: type_name = "string"; break;
                                            case dpp::co_integer: type_name = "integer"; break;
                                            case dpp::co_boolean: type_name = "boolean"; break;
                                            case dpp::co_user: type_name = "user"; break;
                                            case dpp::co_channel: type_name = "channel"; break;
                                            case dpp::co_role: type_name = "role"; break;
                                            case dpp::co_mentionable: type_name = "mentionable"; break;
                                            case dpp::co_number: type_name = "number"; break;
                                            case dpp::co_attachment: type_name = "attachment"; break;
                                        }

                                        auto parse_error_msg = std::format("Failed to parse {} parameter `#{}` ({})", 
                                            type_name, dest_option_data.type == dpp::co_attachment ? (current_attachment_index + 1) : level, dest_option_data.name);

                                        if (source_option.type == dpp::co_string)
                                        {
                                            dest_option_data.value = chat_param;
                                        }
                                        else if (source_option.type == dpp::co_integer)
                                        {
                                            try
                                            {
                                                dest_option_data.value = std::stoll(chat_param);
                                            }
                                            catch (std::invalid_argument& e)
                                            {
                                                // people can just escape the backtick and ping everyone, todo fixme EVERYWHERE
                                                throw std::invalid_argument(std::format("{} - \"{}\" is not a valid int64 number.", 
                                                    parse_error_msg, dpp::utility::markdown_escape(chat_param, true)));
                                            }
                                            catch (std::out_of_range& e)
                                            {
                                                throw std::invalid_argument(std::format("{} - \"{}\" is outside the int64 range (-2^63 to 2^63-1).", parse_error_msg, chat_param));
                                            }
                                        }
                                        else if (source_option.type == dpp::co_boolean)
                                        {
                                            static std::vector<std::string> true_values { "true", "yes", "1" };
                                            static std::vector<std::string> false_values { "false", "no", "0" };

                                            if (std::find(true_values.begin(), true_values.end(), chat_param_lowercase) != true_values.end())
                                            {
                                                dest_option_data.value = true;
                                            }
                                            else if (std::find(false_values.begin(), false_values.end(), chat_param_lowercase) != false_values.end())
                                            {
                                                dest_option_data.value = false;
                                            }
                                            else
                                            {
                                                auto values_str = [](const std::vector<std::string>& values)
                                                {
                                                    std::string str;
                                                    for (int i = 0; i < values.size(); i++)
                                                    {
                                                        if (i > 0)
                                                        {
                                                            str += "/";
                                                        }
                                                        str += "`" + values[i] + "`";
                                                    }
                                                    return str;
                                                };
                                                static std::string true_values_str = values_str(true_values);
                                                static std::string false_values_str = values_str(false_values);

                                                throw std::invalid_argument(std::format("{} - \"{}\" is not valid, you must pass either {} or {}.", 
                                                    parse_error_msg, dpp::utility::markdown_escape(chat_param, true), true_values_str, false_values_str));
                                            }
                                        }
                                        else if (source_option.type == dpp::co_user)
                                        {
                                            try
                                            {
                                                dest_option_data.value = parse_find_results(find_users(chat_param));
                                            }
                                            catch (std::invalid_argument& e)
                                            {
                                                throw std::invalid_argument(std::format("{} - {}", parse_error_msg, e.what()));
                                            }
                                        }
                                        else if (source_option.type == dpp::co_channel)
                                        {
                                            try
                                            {
                                                dest_option_data.value = parse_find_results(find_channels(chat_param));
                                            }
                                            catch (std::invalid_argument& e)
                                            {
                                                throw std::invalid_argument(std::format("{} - {}", parse_error_msg, e.what()));
                                            }
                                        }
                                        else if (source_option.type == dpp::co_role)
                                        {
                                            try
                                            {
                                                dest_option_data.value = parse_find_results(find_roles(chat_param));
                                            }
                                            catch (std::invalid_argument& e)
                                            {
                                                throw std::invalid_argument(std::format("{} - {}", parse_error_msg, e.what()));
                                            }
                                        }
                                        else if (source_option.type == dpp::co_mentionable)
                                        {
                                            try
                                            {
                                                auto user_results = find_users(chat_param);
                                                auto role_results = find_roles(chat_param);

                                                find_results results;
                                                results.reserve(user_results.size() + role_results.size());
                                                results.insert(user_results.end(), user_results.begin(), user_results.end());
                                                results.insert(role_results.end(), role_results.begin(), role_results.end());

                                                dest_option_data.value = parse_find_results(results);
                                            }
                                            catch (std::invalid_argument& e)
                                            {
                                                throw std::invalid_argument(std::format("{} - {}", parse_error_msg, e.what()));
                                            }
                                        }
                                        else if (source_option.type == dpp::co_number)
                                        {
                                            try
                                            {
                                                dest_option_data.value = std::stod(chat_param);
                                            }
                                            catch (std::invalid_argument& e)
                                            {
                                                throw std::invalid_argument(std::format("{} - \"{}\" is not a valid (double-precision floating-point) number.", 
                                                    parse_error_msg, dpp::utility::markdown_escape(chat_param, true)));
                                            }
                                            catch (std::out_of_range& e)
                                            {
                                                throw std::invalid_argument(std::format("{} - \"{}\" is outside the double-precision floating-point range.", parse_error_msg, chat_param));
                                            }
                                        }
                                        else if (source_option.type == dpp::co_attachment)
                                        {
                                            auto& attachments = event.msg.attachments;

                                            if (attachments.size() == current_attachment_index)
                                            {
                                                throw std::invalid_argument(std::format("{} - not enough files attached to message (missing attachment `#{}`).", parse_error_msg, current_attachment_index + 1));
                                            }

                                            dest_option_data.value = attachments[current_attachment_index++].id;

                                            // since attachments aren't part of the message, do not increment level
                                            level--;
                                        }

                                        dest_options.push_back(dest_option_data);
                                        level++;
                                    }
                                };

                                auto iterate_subcommands = [&params, &iterate_command_params](
                                    const std::vector<dpp::command_option>& source_options,
                                    std::vector<dpp::command_data_option>& dest_options,
                                    int level)
                                {
                                    for (auto& source_option : source_options)
                                    {
                                        if (params[level] == source_option.name)
                                        {
                                            auto& dest_option_data = dest_options.emplace_back();
                                            dest_option_data.focused = false;
                                            dest_option_data.name = source_option.name;
                                            dest_option_data.options = {};
                                            dest_option_data.type = dpp::co_sub_command;
                                            dest_option_data.value = {};

                                            iterate_command_params(source_option.options, dest_option_data.options, level + 1);
                                            return;
                                        }
                                    }
                                    throw std::invalid_argument(std::format("This command doesn't have a subcommand named \"{}\".", dpp::utility::markdown_escape(params[level], true)));
                                };

                                std::string error = "";

                                // at this point all of these are true:
                                // - command options are NOT empty (there is at least 1 or more)
                                // - first command option is required
                                // - first command option is NOT a string
                                try
                                {
                                    auto& level_1_options = command.options;

                                    // if the first option is a subcommand group, all the others are too
                                    // all of its options are subcommands, and, in turn, all subcommand options are parameters
                                    if (level_1_options[0].type == dpp::co_sub_command_group)
                                    {
                                        if (params.size() < 3)
                                        {
                                            throw std::invalid_argument("This command expects at least 2 parameters, but only 1 was passed.");
                                        }

                                        for (auto& level_1_option : level_1_options)
                                        {
                                            if (params[1] == level_1_option.name)
                                            {
                                                auto& level_1_data = cmd_interaction.options.emplace_back();
                                                level_1_data.focused = false;
                                                level_1_data.name = level_1_option.name;
                                                level_1_data.options = {};
                                                level_1_data.type = dpp::co_sub_command_group;
                                                level_1_data.value = {};

                                                iterate_subcommands(level_1_option.options, level_1_data.options, 2);
                                                goto found_subcmd_group;
                                            }
                                        }
                                        throw std::invalid_argument(std::format("This command doesn't have a subcommand group named \"{}\".", dpp::utility::markdown_escape(params[1], true)));
                                    }
                                    // if the first option is a subcommand, all the others are too
                                    // in turn, all subcommand options are parameters
                                    else if (level_1_options[0].type == dpp::co_sub_command)
                                    {
                                        //if (params.size() < 2)
                                        //{
                                        //    // unreachable
                                        //    throw std::invalid_argument("This command expects at least 1 parameter, but none were passed.");
                                        //}
                                        iterate_subcommands(level_1_options, cmd_interaction.options, 1);
                                    }
                                    else
                                    {
                                        iterate_command_params(level_1_options, cmd_interaction.options, 1);
                                    }

                                found_subcmd_group:
                                    // a label appearing at the end of a compound statement requires at least '/std:c++23preview'
                                    void(0);
                                }
                                catch (std::invalid_argument& e)
                                {
                                    error = std::format(":x: **| Invalid argument error:** {}", e.what());
                                }
                                catch (std::exception& e)
                                {
                                    error = std::format(":x: **| Error:** {}", e.what());
                                }

                                if (!error.empty())
                                {
                                    event.reply(error);
                                    co_return;
                                }

                                co_await command.run(run_event(discofloor::message_command_t(event, cmd_interaction)));
                                co_return;
                            }

                            // the user didn't type out our command, so we're not running anything
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
                            static_assert(!sizeof(T*), "Unsupported run_event type for event_router_async");
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

        try
        {
            auto size = data_size_total();
            auto max = settings_.max_data_size_total;

            log(dpp::ll_info, std::format("Bot data storage usage: {} / {} ({:.2f} %)",
                bulbtils::file::size_to_string(size),
                bulbtils::file::size_to_string(max),
                ((double)size / (double)max) * 100.0));
        }
        catch (std::exception& e)
        {
            log(dpp::ll_error, std::format("Failed to get bot data storage usage (bot data functionality might not work properly) - {}", e.what()));
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