#include <discofloor/run_event.h>
#include <discofloor/bot.h>

namespace discofloor
{
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

    run_event::operator dpp::event_dispatch_t() const
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
                    auto cluster = get_bot();
                    cluster->message_add_reaction(event.msg, "📨");
                    cluster->direct_message_create(event.msg.author.id, msg, [event, cluster](const dpp::confirmation_callback_t& callback)
                    {
                        if (callback.is_error())
                        {
                            event.reply(cluster->format_error_reply("**Ephemeral message:** I wasn't able to message you the response - please ensure I can DM you, or run the slash command instead."));
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
}