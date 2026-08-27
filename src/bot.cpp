#include <discofloor/bot.h>
#include <discofloor/utility.h>

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

    dpp::user run_event::command_invoker() const
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

    dpp::channel* run_event::get_channel() const
    {
        return std::visit([](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                return dpp::find_channel(event.command.channel_id);
            }
            else
            {
                return dpp::find_channel(event.msg.channel_id);
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
        std::visit([this, &msg, &callback](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                event.reply(msg, callback);
            }
            else
            {
                if (msg.is_ephemeral())
                {
                    get_bot()->message_add_reaction(event.msg, "📨");
                    get_bot()->direct_message_create(event.msg.author.id, msg, [event](const dpp::confirmation_callback_t& callback)
                    {
                        if (callback.is_error())
                        {
                            event.reply(container_msg("I wasn't able to message you the response - please ensure I can DM you, or run the slash command instead.", 0xFF0000));
                        }
                    });
                }
                else
                {
                    event.reply(msg, false, callback);
                }
            }
        },
        *this);
    }

    dpp::async<dpp::confirmation_callback_t> run_event::co_reply(const dpp::message& msg) const
    {
        return std::visit([this, &msg](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                return event.co_reply(msg);
            }
            else
            {
                if (msg.is_ephemeral())
                {
                    // error handling not possible here
                    get_bot()->message_add_reaction(event.msg, "📨");
                    return get_bot()->co_direct_message_create(event.msg.author.id, msg);
                }
                else
                {
                    return event.co_reply(msg);
                }
            }
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
        std::visit([&msg, &callback](auto&& event)
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                event.edit_original_response(msg, callback);
            }
            else
            {
                event.reply(msg, false, callback);
            }
        },
        *this);
    }

    dpp::command_value run_event::get_cmd_param(const std::string& param_name) const
    {
        return std::visit([&param_name](auto&& event) -> dpp::command_value
        {
            using T = std::decay_t<decltype(event)>;

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

    dpp::message bot::module_command::usage(const std::string& subcmd_group, const std::string& subcmd)
    {
        auto prefix = owner->old_style_commands_enabled() ? owner->settings().prefix : "/";

        auto usage_options = [cmd_id = id, cmd_name = name, cmd_examples = chat_command_examples(), prefix]
            (const std::vector<dpp::command_option>& options, const std::string& subcmd_group_and_or_subcmd, const std::string& cmd_desc)
        {
            auto mention = dpp::utility::slashcommand_mention(cmd_id, cmd_name, subcmd_group_and_or_subcmd);

            std::string usage_cmd = prefix + cmd_name;
            if (!subcmd_group_and_or_subcmd.empty())
            {
                usage_cmd += " " + subcmd_group_and_or_subcmd;
            }

            std::string usage_params = "";
            for (auto& option : options)
            {
                std::vector<std::string> cmd_brackets { "<", ">" };
                if (!option.required)
                {
                    cmd_brackets = { "[", "]" };
                }
                usage_cmd += " " + cmd_brackets[0] + option.name + cmd_brackets[1];

                std::string type_name;
                switch (option.type)
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
                usage_params += std::format("\n- **`{}` ({})** - {}", option.name, type_name, option.description);
            }

            std::string usage_examples = "";
            for (auto& example : cmd_examples)
            {
                if (example.starts_with(subcmd_group_and_or_subcmd))
                {
                    usage_examples += std::format("\n- `{}{} {}`", prefix, cmd_name, example);
                }
            }

            auto content_str = std::format(
                "## {}\n"
                "{}\n"
                "### Usage: `{}`{}\n"
                "{}",
                mention,
                cmd_desc,
                usage_cmd, usage_params,
                usage_examples.empty() ? "" : "### Examples:" + usage_examples
            );

            dpp::component content;
            content.set_type(dpp::cot_text_display);
            content.set_content(content_str);

            dpp::component separator;
            separator.set_type(dpp::cot_separator);
            separator.set_spacing(dpp::sep_small);
            separator.set_divider(false);

            dpp::component legend;
            legend.set_type(dpp::cot_text_display);
            legend.set_content("-# **Legend:** <required parameter>, [optional parameter]");

            dpp::component container;
            container.set_type(dpp::cot_container);
            container.add_component_v2(content);
            container.add_component_v2(separator);
            container.add_component_v2(legend);

            dpp::message msg;
            msg.set_flags(dpp::m_using_components_v2);
            msg.add_component_v2(container);

            return msg;
        };

        auto usage_subcmds = [cmd_id = id, cmd_name = name, prefix](const std::vector<dpp::command_option>& options, const std::string& subcmd_group)
        {
            auto content_str = std::format("**The \"{}{}\" {} has {} subcommand{}:**",
                cmd_name,
                subcmd_group.empty() ? "" : " " + subcmd_group,
                subcmd_group.empty() ? "command" : "subcommand group",
                options.size(),
                options.size() == 1 ? "" : "s"
            );

            for (int i = 0; i < options.size(); i++)
            {
                auto& option = options[i];

                content_str += std::format(
                    "\n### {}\\. `{}{} {}{}`"
                    "\n - {}"
                    "\n - {}",
                    i+1, prefix, cmd_name, subcmd_group.empty() ? "" : subcmd_group + " ", option.name,
                    option.description,
                    dpp::utility::slashcommand_mention(cmd_id, cmd_name, std::format("{}{}", subcmd_group.empty() ? "" : subcmd_group + " ", option.name))
                );
            }

            dpp::component content;
            content.set_type(dpp::cot_text_display);
            content.set_content(content_str);

            dpp::component container;
            container.set_type(dpp::cot_container);
            container.add_component_v2(content);

            dpp::message msg;
            msg.set_flags(dpp::m_using_components_v2);
            msg.add_component_v2(container);

            return msg;
        };

        if (options[0].type == dpp::co_sub_command_group)
        {
            // if the specific subcommand is passed, get the subcommand's usage
            if (!subcmd.empty())
            {
                auto subcmd_group_it = std::find_if(options.begin(), options.end(), [&subcmd_group](const dpp::command_option& option)
                {
                    return option.name == subcmd_group;
                });
                auto subcmd_it = std::find_if(subcmd_group_it->options.begin(), subcmd_group_it->options.end(), [&subcmd](const dpp::command_option& option)
                {
                    return option.name == subcmd;
                });
                return usage_options(subcmd_it->options, subcmd_group_it->name + " " + subcmd_it->name, subcmd_it->description);
            }

            // if it isn't, but the subcommand group is passed, list all available subcommands for this group
            if (!subcmd_group.empty())
            {
                auto subcmd_group_it = std::find_if(options.begin(), options.end(), [&subcmd_group](const dpp::command_option& option)
                {
                    return option.name == subcmd_group;
                });

                return usage_subcmds(subcmd_group_it->options, subcmd_group_it->name);
            }

            // otherwise list all available subcommand groups
            auto content_str = std::format("**The \"{}\" command has {} subcommand group{}:**",
                name,
                options.size(),
                options.size() == 1 ? "" : "s"
            );

            for (int i = 0; i < options.size(); i++)
            {
                auto& option = options[i];
                content_str += std::format("\n### {}\\. `{}{} {}`", i + 1, prefix, name, option.name);
            }

            dpp::component content;
            content.set_type(dpp::cot_text_display);
            content.set_content(content_str);

            dpp::component container;
            container.set_type(dpp::cot_container);
            container.add_component_v2(content);

            dpp::message msg;
            msg.set_flags(dpp::m_using_components_v2);
            msg.add_component_v2(container);

            return msg;
        }

        if (options[0].type == dpp::co_sub_command)
        {
            // if the specific subcommand is passed, get the subcommand's usage
            if (!subcmd.empty())
            {
                auto subcmd_it = std::find_if(options.begin(), options.end(), [&subcmd](const dpp::command_option& option)
                {
                    return option.name == subcmd;
                });
                return usage_options(subcmd_it->options, subcmd_it->name, subcmd_it->description);
            }
            // otherwise list all available subcommands
            return usage_subcmds(options, name);
        }
        return usage_options(options, "", description);
    }

    dpp::task<void> bot::module_command::message_create_event(const dpp::message_create_t& event)
    {
        if (!owner->old_style_commands_enabled())
        {
            co_return;
        }
        if (event.msg.author.is_bot())
        {
            // bots can't run commands
            co_return;
        }

        auto prefix = owner->settings().prefix;
        auto prefix_len = prefix.size();

        dpp::command_interaction cmd_interaction
        {
            .id = id,
            .name = event.msg.content.substr(prefix_len, event.msg.content.find(' ') - prefix_len),
            .options = {},
            .type = dpp::ctxm_chat_input,
            .target_id = dpp::snowflake(0),
        };

        // Interestingly enough, even though subcommand (groups) are technically required, the bool is set to false
        bool command_has_no_required_params = options.empty() ||
            (!options[0].required && options[0].type != dpp::co_sub_command_group && options[0].type != dpp::co_sub_command);

        auto chat_command = std::format("{}{}", prefix, name);
        if (event.msg.content == chat_command)
        {
            // the user typed out exactly our command with no parameters
            if (command_has_no_required_params)
            {
                co_await run(run_event(discofloor::message_command_t(event, cmd_interaction)));
                co_return;
            }

            // if this command has at least one required parameter, obviously we can't run it on its own
            // in this special case, instead of erroring, we print command usage help to the user, to teach them how to run the command
            event.reply(usage());
            co_return;
        }

        if (!event.msg.content.starts_with(chat_command + " "))
        {
            // the user isn't running our command at all
            co_return;
        }

        // from this point on, we know that the user typed out our command with parameters
        // (discord doesn't allow trailing whitespace in messages)
        if (command_has_no_required_params)
        {
            co_await run(run_event(discofloor::message_command_t(event, cmd_interaction)));
            co_return;
        }

        // special case - this command's only option is a string
        // in which case pass the entire string and run the command
        if (options.size() == 1 && options[0].type == dpp::co_string)
        {
            auto& option = cmd_interaction.options.emplace_back();
            option.focused = false;
            option.name = options[0].name;
            option.options = {};
            option.type = dpp::co_string;
            option.value = event.msg.content.substr(event.msg.content.find(' ') + 1);

            co_await run(run_event(discofloor::message_command_t(event, cmd_interaction)));
            co_return;
        }

        // This error is thrown for malformed commands - prints the error message
        struct malformed_command : public std::runtime_error
        {
            using runtime_error::runtime_error;
        };

        // This error is thrown for blank commands - prints usage help
        struct blank_command : public std::runtime_error
        {
            std::string subcmd_group;
            std::string subcmd;

            blank_command(const std::string& error, const std::string& subcmd_group = "", const std::string& subcmd = "")
                : runtime_error(error), subcmd_group(subcmd_group), subcmd(subcmd) {}
        };

        // the first parameter will always be the command itself, since CHAT_INPUT commands can't have spaces
        // message/user context menu commands can, however, have spaces, but we don't care about those here
        auto params = bulbtils::string::split_parameters(event.msg.content);

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

            // all required parameters always precede all optional parameters
            // when we've found the first optional parameter, all parameters up until that point are mandatory
            auto required_param_count = std::find_if(source_options.begin(), source_options.end(), [](const dpp::command_option& option)
            {
                return option.required == false;
            });

            int required_levels = level + std::distance(source_options.begin(), required_param_count);
            if (params.size() < required_levels)
            {
                auto error = std::format("This (sub)command expects at least {} parameter{}, but only {} {} passed.",
                    (required_levels - 1), (required_levels - 1) == 1 ? "" : "s", (params.size() - 1), (params.size() - 1) == 1 ? "was" : "were");

                if (params.size() == level)
                {
                    throw blank_command(error);
                }
                throw malformed_command(error);
            }

            // the result of an ID-able (user, role, channel) in a dpp::find_*() call
            struct find_result
            {
                // this ID gets passed to the dpp::command_data_option
                dpp::snowflake id;

                // user-friendly info about what type of result this is
                std::string type;

                // the value of this type of result (ie. the thing the user searched for)
                std::string value;

                find_result(dpp::snowflake id, const std::string& type, const std::string& value)
                    : id(id), type(type), value(value) {
                }
            };

            using find_results_base = std::vector<find_result>;
            struct find_results : public find_results_base
            {
                // required so our initializer-list constructors work
                using find_results_base::find_results_base;

                // add the find_result to the list if the needle can be found in the haystack
                // in this case, the haystack is the value of the find_result
                // returns false if the needle was not found in the haystack, or if the haystack is empty
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

            // given a parameter, this lambda finds all eligible users based on various user-specific criteria
            auto find_users = [&event](const std::string& param) -> find_results
            {
                auto user = dpp::find_user(dpp::snowflake(param));
                if (user)
                {
                    // the parameter is a snowflake, so we allow direct matches
                    return { { user->id, "User", discofloor::get_username(*user) } };
                }

                auto guild = dpp::find_guild(event.msg.guild_id);
                if (!guild)
                {
                    // for privacy, we only allow searching for users within a guild
                    return {};
                }

                find_results results;
                for (auto& [user_id, guild_member] : guild->members)
                {
                    auto& user_it = *dpp::find_user(user_id);

                    // find a user by their username
                    if (results.add_if_match({ user_id, "User", discofloor::get_username(user_it) }, param))
                    {
                        continue;
                    }

                    // find a user by their "global name" - essentially the "nick" name they have set globally on discord
                    if (results.add_if_match({ user_id, "Global name of user " + discofloor::get_username(user_it), user_it.global_name }, param))
                    {
                        continue;
                    }

                    // find a user by their guild nickname
                    if (results.add_if_match({ user_id, "Nickname of user " + discofloor::get_username(user_it), guild_member.get_nickname() }, param))
                    {
                        continue;
                    }
                }
                return results;
            };

            // given a parameter, this lambda finds all eligible roles based on various user-specific criteria
            auto find_roles = [&event](const std::string& param) -> find_results
            {
                auto role = dpp::find_role(dpp::snowflake(param));
                if (role)
                {
                    // the parameter is a snowflake, so we allow direct matches
                    return { { role->id, "Role", role->name } };
                }

                auto guild = dpp::find_guild(event.msg.guild_id);
                if (!guild)
                {
                    // for privacy, we only allow searching for users within a guild
                    return {};
                }

                find_results results;
                for (auto& role_id : guild->roles)
                {
                    auto& role_it = *dpp::find_role(role_id);

                    // find a role by its name
                    if (results.add_if_match({ role_id, "Role", role_it.name }, param))
                    {
                        continue;
                    }
                }
                return results;
            };

            // given a parameter, this lambda finds all eligible channels based on various user-specific criteria
            auto find_channels = [&event](const std::string& param) -> find_results
            {
                auto channel = dpp::find_channel(dpp::snowflake(param));
                if (channel)
                {
                    // the parameter is a snowflake, so we allow direct matches
                    return { { channel->id, "Channel", channel->name } };
                }

                auto guild = dpp::find_guild(event.msg.guild_id);
                if (!guild)
                {
                    // for privacy, we only allow searching for users within a guild
                    return {};
                }

                find_results results;
                for (auto& channel_id : guild->channels)
                {
                    auto& channel_it = *dpp::find_channel(channel_id);

                    // find a channel by its name
                    if (results.add_if_match({ channel_id, "Channel", channel_it.name }, param))
                    {
                        continue;
                    }
                }
                return results;
            };

            // this lambda parses the find_results, throwing an error if 0 or 2+ results are found
            // if only 1 result is found, returns its snowflake to be used for the dpp::command_data_option
            auto parse_find_results = [](const find_results& results) -> dpp::snowflake
            {
                std::string blablabla = "matches found for the given input (try narrowing your search, providing an ID, or running the command in a guild (that i'm in) if you aren't already)";
                if (results.empty())
                {
                    throw malformed_command("no " + blablabla + ".");
                }
                if (results.size() > 1)
                {
                    std::string matches = "";
                    for (int i = 0; i < results.size(); i++)
                    {
                        auto& find_result = results[i];
                        matches += std::format("\n{}. \"{}\" ({}) - `{}`", i + 1, dpp::utility::markdown_escape(find_result.value, true), find_result.type, std::to_string(find_result.id));
                    }
                    throw malformed_command("multiple " + blablabla + ":" + matches);
                }
                return results[0].id;
            };

            // in case we hit a parsing error, print where it happened
            // useless for subcommands, subcommand groups and strings because they don't error when parsing
            auto parse_error_msg = [](const dpp::command_data_option& data_option, int index)
            {
                std::string type_name;
                switch (data_option.type)
                {
                    case dpp::co_integer: type_name = "integer"; break;
                    case dpp::co_boolean: type_name = "boolean"; break;
                    case dpp::co_user: type_name = "user"; break;
                    case dpp::co_channel: type_name = "channel"; break;
                    case dpp::co_role: type_name = "role"; break;
                    case dpp::co_mentionable: type_name = "mentionable"; break;
                    case dpp::co_number: type_name = "number"; break;
                    case dpp::co_attachment: type_name = "attachment"; break;
                }
                return std::format("Failed to parse {} from parameter `#{}` ({})", type_name, index, data_option.name);
            };

            // since attachments aren't part of the message, we need to manually track their index
            int current_attachment_index = 0;

            for (auto& source_option : source_options)
            {
                if (level == params.size())
                {
                    // the user didn't supply any more parameters, but thanks to the required_levels check
                    // ...we know all the parameters are optional by this point, so it's perfectly fine
                    break;
                }

                // the user-supplied parameter for the given option
                // including a lowercase equivalent for case-insensitive comparison
                auto chat_param = params[level];
                auto chat_param_lowercase = chat_param;
                bulbtils::string::inplace::to_lowercase(chat_param_lowercase);

                // the command_data_option we will be passing to the command_interaction
                dpp::command_data_option dest_option_data
                {
                    .name = source_option.name,
                    .type = source_option.type,
                    .options = {},
                    .focused = false,
                };

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
                        throw malformed_command(std::format("{} - \"{}\" is not a valid int64 number.",
                            parse_error_msg(dest_option_data, level), dpp::utility::markdown_escape(chat_param, true)));
                    }
                    catch (std::out_of_range& e)
                    {
                        throw malformed_command(std::format("{} - \"{}\" is outside the int64 range (-2^63 to 2^63-1).", parse_error_msg(dest_option_data, level), chat_param));
                    }
                }
                else if (source_option.type == dpp::co_boolean)
                {
                    // these are the aliases we accept for boolean parameters
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
                        // tell the user about the boolean aliases we expect from them
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

                        throw malformed_command(std::format("{} - \"{}\" is not valid, you must pass either {} or {}.",
                            parse_error_msg(dest_option_data, level), dpp::utility::markdown_escape(chat_param, true), true_values_str, false_values_str));
                    }
                }
                else if (source_option.type == dpp::co_user)
                {
                    try
                    {
                        dest_option_data.value = parse_find_results(find_users(chat_param));
                    }
                    catch (malformed_command& e)
                    {
                        throw malformed_command(std::format("{} - {}", parse_error_msg(dest_option_data, level), e.what()));
                    }
                }
                else if (source_option.type == dpp::co_channel)
                {
                    try
                    {
                        dest_option_data.value = parse_find_results(find_channels(chat_param));
                    }
                    catch (malformed_command& e)
                    {
                        throw malformed_command(std::format("{} - {}", parse_error_msg(dest_option_data, level), e.what()));
                    }
                }
                else if (source_option.type == dpp::co_role)
                {
                    try
                    {
                        dest_option_data.value = parse_find_results(find_roles(chat_param));
                    }
                    catch (malformed_command& e)
                    {
                        throw malformed_command(std::format("{} - {}", parse_error_msg(dest_option_data, level), e.what()));
                    }
                }
                else if (source_option.type == dpp::co_mentionable)
                {
                    try
                    {
                        // mentionables = users + roles
                        auto user_results = find_users(chat_param);
                        auto role_results = find_roles(chat_param);

                        find_results results;
                        results.reserve(user_results.size() + role_results.size());
                        results.insert(user_results.end(), user_results.begin(), user_results.end());
                        results.insert(role_results.end(), role_results.begin(), role_results.end());

                        dest_option_data.value = parse_find_results(results);
                    }
                    catch (malformed_command& e)
                    {
                        throw malformed_command(std::format("{} - {}", parse_error_msg(dest_option_data, level), e.what()));
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
                        throw malformed_command(std::format("{} - \"{}\" is not a valid (double-precision floating-point) number.",
                            parse_error_msg(dest_option_data, level), dpp::utility::markdown_escape(chat_param, true)));
                    }
                    catch (std::out_of_range& e)
                    {
                        throw malformed_command(std::format("{} - \"{}\" is outside the double-precision floating-point range.", parse_error_msg(dest_option_data, level), chat_param));
                    }
                }
                else if (source_option.type == dpp::co_attachment)
                {
                    auto& attachments = event.msg.attachments;

                    if (attachments.size() == current_attachment_index)
                    {
                        throw malformed_command(std::format("{} - missing attachment (not enough files attached to message).",
                            parse_error_msg(dest_option_data, current_attachment_index + 1), current_attachment_index + 1));
                    }

                    // increment attachment-specific level separately
                    dest_option_data.value = attachments[current_attachment_index++].id;

                    // since attachments aren't part of the message, do not increment chat parameter level
                    level--;
                }

                // no exceptions, data value is set, we can continue
                dest_options.push_back(dest_option_data);
                level++;
            }
        };

        // this lambda function handles iterating non-group subcommands
        auto iterate_subcommands = [&params, &iterate_command_params](
            const std::vector<dpp::command_option>& source_options,
            std::vector<dpp::command_data_option>& dest_options,
            int level)
        {
            for (auto& source_option : source_options)
            {
                auto name_lowercase = params[level];
                bulbtils::string::inplace::to_lowercase(name_lowercase);

                if (name_lowercase == source_option.name)
                {
                    auto& dest_option_data = dest_options.emplace_back();
                    dest_option_data.focused = false;
                    dest_option_data.name = source_option.name;
                    dest_option_data.options = {};
                    dest_option_data.type = dpp::co_sub_command;
                    dest_option_data.value = {};

                    // all options of a subcommand are parameters
                    try 
                    {
                        iterate_command_params(source_option.options, dest_option_data.options, level + 1);
                    }
                    catch (blank_command& e)
                    {
                        e.subcmd = source_option.name;
                        throw e;
                    }
                    return;
                }
            }
            throw malformed_command(std::format("This command doesn't have a subcommand named \"{}\".", dpp::utility::markdown_escape(params[level], true)));
        };

        // get error message separately since we can't co-await inside a catch block
        std::string error;

        // at this point all of these are true:
        // - command options are NOT empty (there is at least 1 or more)
        // - first command option is required
        // - first command option is NOT a string
        try
        {
            // if the first option is a subcommand group, all the others are too
            // all of its options are subcommands, and, in turn, all subcommand options are parameters
            if (options[0].type == dpp::co_sub_command_group)
            {
                for (auto& option : options)
                {
                    auto name_lowercase = params[1];
                    bulbtils::string::inplace::to_lowercase(name_lowercase);

                    if (name_lowercase == option.name)
                    {
                        // a subcommand group must have at least one subcommand
                        if (params.size() < 3)
                        {
                            throw blank_command("This command expects at least 2 parameters, but only 1 was passed.", option.name);
                        }

                        auto& option_data = cmd_interaction.options.emplace_back();
                        option_data.focused = false;
                        option_data.name = option.name;
                        option_data.options = {};
                        option_data.type = dpp::co_sub_command_group;
                        option_data.value = {};

                        // all options of a subcommand group are subcommands
                        try
                        {
                            iterate_subcommands(option.options, option_data.options, 2);
                        }
                        catch (blank_command& e)
                        {
                            e.subcmd_group = option.name;
                            throw e;
                        }
                        goto found_subcmd_group;
                    }
                }
                throw malformed_command(std::format("This command doesn't have a subcommand group named \"{}\".", dpp::utility::markdown_escape(params[1], true)));
            }
            else if (options[0].type == dpp::co_sub_command)
            {
                iterate_subcommands(options, cmd_interaction.options, 1);
            }
            else
            {
                iterate_command_params(options, cmd_interaction.options, 1);
            }

        found_subcmd_group:
            // a label appearing at the end of a compound statement requires at least '/std:c++23preview'
            void(0);
        }
        catch (malformed_command& e)
        {
            event.reply(container_msg(std::format("**Malformed command:** {}", e.what()), 0xFF0000));
            co_return;
        }
        catch (blank_command& e)
        {
            event.reply(usage(e.subcmd_group, e.subcmd));
            co_return;
        }
        catch (std::exception& e)
        {
            event.reply(container_msg(std::format("**Error:** {}", e.what()), 0xFF0000));
            co_return;
        }
        co_await run(run_event(discofloor::message_command_t(event, cmd_interaction)));
    }

    void bot::module_command::attach_events()
    {
        auto slashcmd_event_router = [this](const auto& event) -> dpp::task<void>
        {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                // for slash commands it's simple - just check their name
                if (event.command.get_command_name() == name)
                {
                    co_await run(run_event(event));
                }
            }
            else
            {
                // can't use false here, or it will never compile (as always, thanks raymond chen :3)
                // https://devblogs.microsoft.com/oldnewthing/20200311-00/?p=103553
                static_assert(!sizeof(T*), "Unsupported run_event type for slashcmd_event_router");
            }
        };

        if (type == dpp::ctxm_chat_input)
        {
            event_handles[0] = owner->on_slashcommand.attach(slashcmd_event_router);
            if ((owner->intents & dpp::i_message_content) != 0)
            {
                event_handles[1] = owner->on_message_create.attach([this](const dpp::message_create_t& e) -> dpp::task<void> { co_await message_create_event(e); });
            }
        }
        else if (type == dpp::ctxm_message)
        {
            event_handles[0] = owner->on_message_context_menu.attach(slashcmd_event_router);
        }
        else if (type == dpp::ctxm_user)
        {
            event_handles[0] = owner->on_user_context_menu.attach(slashcmd_event_router);
        }
    }

    void bot::module_command::detach_events()
    {
        if (type == dpp::ctxm_chat_input)
        {
            owner->on_slashcommand.detach(event_handles[0]);
            if ((owner->intents & dpp::i_message_content) != 0)
            {
                detach_message_content_events();
            }
        }
        else if (type == dpp::ctxm_message)
        {
            owner->on_message_context_menu.detach(event_handles[0]);
        }
        else if (type == dpp::ctxm_user)
        {
            owner->on_user_context_menu.detach(event_handles[0]);
        }
    }

    void bot::module_command::detach_message_content_events()
    {
        if (type == dpp::ctxm_chat_input)
        {
            owner->on_message_create.detach(event_handles[1]);
        }
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
                module_command->detach_events();
                module_command = module_commands_.erase(module_command);
            }

            // now re-fetch them
            {
                std::shared_lock _(loaded_modules_mutex_);

                for (auto& loaded_module : loaded_modules_)
                {
                    for (auto& command : loaded_module->commands(*this))
                    {
                        if (for_each_command_)
                        {
                            for_each_command_(command);
                        }
                        module_commands_.emplace_back(command, this);
                        commands.push_back(command);
                    }
                }
            }
        }

        // as this function's name implies, the lambda will run asynchronously(!!!) and NOT when create_commands_async is called
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
                    module_command->id = snowflake;
                    module_command->attach_events();
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
                    command.detach_message_content_events();
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
        else if ((intents & dpp::i_message_content) == 0)
        {
            log(dpp::ll_info, "Old-style commands disabled ('Message Content' intent not provided)");
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
        // if modules aren't destroyed yet, do it now
        for (auto& loaded_module : std::views::reverse(loaded_modules_))
        {
            loaded_module->destroy(*this);
        }
    }

    void bot::full_shutdown()
    {
        // destroy loaded modules in reverse order of initialization
        for (auto& loaded_module : std::views::reverse(loaded_modules_))
        {
            loaded_module->destroy(*this);
        }
        loaded_modules_.clear();

        // HACK: ideally we'd use a unique_lock for the cluster's shards_mutex, but it is inaccessible (private)
        const auto& shards = get_shards();

        // also kind of a hack? set our token to something bogus so the bot (and its shards) can't reconnect
        // we can't use an empty string here because of on_log causing a memory flood trying to find/hide the token
        token = ":3";

        for (auto& shard : shards)
        {
            auto client = shard.second;
            if (client)
            {
                // make sure we knock our bot offline
                client->send_close_packet();
                client->token = ":3";
            }
        }

        while (true)
        {
            // give the shards time to die (checking every second should be good enough)
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (std::all_of(shards.begin(), shards.end(), [](const auto& it) { return !it.second->is_connected(); }))
            {
                break;
            }
        }

        // now let the bot gracefully shut down
        shutdown();
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