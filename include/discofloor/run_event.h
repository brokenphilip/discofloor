#pragma once

namespace discofloor
{
    using run_event_base = std::variant<dpp::slashcommand_t, dpp::message_create_t, dpp::message_context_menu_t, dpp::user_context_menu_t>;

    class run_event : public run_event_base
    {
        std::vector<dpp::command_data_option> manual_options_;
    public:
        run_event(const dpp::slashcommand_t& slash_command) : run_event_base(slash_command) {}
        run_event(const dpp::message_context_menu_t& message_context_menu) : run_event_base(message_context_menu) {}
        run_event(const dpp::user_context_menu_t& user_context_menu) : run_event_base(user_context_menu) {}
        run_event(const dpp::message_create_t& message_create, const std::vector<dpp::command_data_option>& manual_options)
            : run_event_base(message_create), manual_options_(manual_options) {}

        bot* get_bot() const;

        inline auto get_slash_command() const { return std::get_if<dpp::slashcommand_t>(this); }
        inline auto get_message_create() const { return std::get_if<dpp::message_create_t>(this); }
        inline auto get_message_context_menu() const { return std::get_if<dpp::message_context_menu_t>(this); }
        inline auto get_user_context_menu() const { return std::get_if<dpp::user_context_menu_t>(this); }

        // For any type of slash command (ie. excluding old-style commands), get the underlying interaction event
        const dpp::interaction_create_t* get_interaction_create() const;

        // For any type of command (including old-style commands), get the underlying event dispatch
        const dpp::event_dispatch_t& event_dispatch() const;

        dpp::user get_command_invoker() const;

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

        // For old-style commands, reply to the user that they should instead use the slash command
        // For "CHAT_INPUT" commands, reply to the user that they should instead use the old-style command
        // If old-style commands are disabled, the user simply gets a "not implemented" reply instead
        // This function is not supported for "MESSAGE" and "USER" commands - will throw std::logic_error
        void reply_not_impl_use_other() const;

        // Given a command parameter name, try to fetch the command parameter value
        // Returns the value if found, or default_value otherwise
        template <typename T>
        T try_get_command_parameter(const std::string& param_name, T default_value) const
        {
            if (auto message_create = get_message_create())
            {
                // todo - manual_options_
                return default_value;
            }
            else if (auto param = get_interaction_create()->get_parameter(param_name); auto value = std::get_if<T>(&param))
            {
                return *value;
            }
            return default_value;
        }
    };
}