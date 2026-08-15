#include <discofloor/run_event.h>

namespace discofloor
{
    bot* bot::command::run_event::get_bot() const
    {
        return std::visit([](auto& event_dispatch)
        {
            return static_cast<bot*>(event_dispatch.owner);
        },
        *this);
    }

    const dpp::interaction_create_t* bot::command::run_event::get_interaction_create() const
    {
        return std::visit([](auto&& arg) -> const dpp::interaction_create_t*
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_base_of_v<dpp::interaction_create_t, T>)
            {
                return &arg;
            }
            else
            {
                return nullptr;
            }
        },
        *this);
    }

    const dpp::event_dispatch_t& bot::command::run_event::event_dispatch() const
    {
        return std::visit([](auto& event_dispatch) -> const dpp::event_dispatch_t&
        {
            return event_dispatch;
        },
        *this);
    }

    dpp::user bot::command::run_event::get_command_invoker() const
    {
        if (auto message_create = get_message_create())
        {
            return message_create->msg.author;
        }
        return get_interaction_create()->command.usr;
    }

    void bot::command::run_event::reply(const dpp::message& msg, dpp::command_completion_event_t callback) const
    {
        if (auto message_create = get_message_create())
        {
            message_create->reply(msg, false, callback);
            return;
        }
        get_interaction_create()->reply(msg, callback);
    }

    dpp::async<dpp::confirmation_callback_t> bot::command::run_event::co_reply(const dpp::message& msg) const
    {
        if (auto message_create = get_message_create())
        {
            return message_create->co_reply(msg);
        }
        return get_interaction_create()->co_reply(msg);
    }

    void bot::command::run_event::thinking_start() const
    {
        if (auto message_create = get_message_create())
        {
            message_create->owner->channel_typing(message_create->msg.channel_id);
            return;
        }
        get_interaction_create()->thinking();
    }

    dpp::async<dpp::confirmation_callback_t> bot::command::run_event::co_thinking_start() const
    {
        if (auto message_create = get_message_create())
        {
            return message_create->owner->co_channel_typing(message_create->msg.channel_id);
        }
        return get_interaction_create()->co_thinking();
    }

    void bot::command::run_event::thinking_end(const dpp::message& msg, dpp::command_completion_event_t callback) const
    {
        if (auto message_create = get_message_create())
        {
            message_create->reply(msg, false, callback);
            return;
        }
        get_interaction_create()->edit_original_response(msg, callback);
    }

    void bot::command::run_event::reply_not_impl_use_other() const
    {
        std::string command_text;

        auto cluster = get_bot();
        auto prefix = cluster->settings().prefix;
        
        if (auto slash_command = get_slash_command())
        {
            if (prefix.empty())
            {
                reply(":warning: **| Not implemented.**");
                return;
            }
            command_text = "`" + prefix + slash_command->command.get_command_name() + "`";
        }
        else if (auto message_create = get_message_create())
        {
            auto prefix_len = prefix.length();
            auto name = message_create->msg.content.substr(prefix_len, message_create->msg.content.find(' ') - prefix_len);

            auto snowflake = cluster->slash_command_snowflake(name);
            if (snowflake == dpp::snowflake(0))
            {
                cluster->log(dpp::ll_error, "Failed to find snowflake for command " + name);
                command_text = "`/" + name + "`";
            }
            else
            {
                command_text = dpp::utility::slashcommand_mention(snowflake, name);
            }
        }
        else
        {
            throw std::logic_error("reply_not_impl_use_other called from unsupported run_event variant");
        }
        reply(std::format(":warning: **| Not implemented, use {} instead.**", command_text));
    }
}