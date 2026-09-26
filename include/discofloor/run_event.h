#pragma once

#include <dpp/dpp.h>

#include <variant>

namespace discofloor
{
	class bot;

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

		// Return the bot which owns this run event
		bot* get_bot() const;

		inline auto get_message_command() const { return std::get_if<discofloor::message_command_t>(this); }
		inline auto get_slash_command() const { return std::get_if<dpp::slashcommand_t>(this); }
		inline auto get_message_context_menu() const { return std::get_if<dpp::message_context_menu_t>(this); }
		inline auto get_user_context_menu() const { return std::get_if<dpp::user_context_menu_t>(this); }

		// For any type of slashcommand-based run_event, get the underlying interaction event
		// Returns null if the run_event is not a slashcommand
		const dpp::interaction_create_t* get_interaction_create() const;

		// Get the underlying event dispatch for any run_event variant
		operator dpp::event_dispatch_t() const;

		// Get the user who ran the command
		dpp::user command_invoker() const;

		// Get the guild the command was run in, if any
		// Returns null for user-app commands
		dpp::guild* get_guild() const;

		// Get the channel the command was run in, if any
		// Returns null for user-app commands
		dpp::channel* get_channel() const;

		// Get the details of the executed command
		dpp::command_interaction command_interaction() const;

		// Reply to the command invoker
		// For old-style (chat prefix) commands, ephemeral messages get DM'd instead
		void reply(const dpp::message& msg, dpp::command_completion_event_t callback = dpp::utility::log_error()) const;
		inline void reply(const std::string& msg, dpp::command_completion_event_t callback = dpp::utility::log_error()) const { reply(dpp::message(msg), callback); }

		dpp::async<dpp::confirmation_callback_t> co_reply(const dpp::message& msg) const;
		inline dpp::async<dpp::confirmation_callback_t> co_reply(const std::string& msg) const { return co_reply(dpp::message(msg)); }

		// idk how to conveniently implement this yet, todo
		//void edit_original_response(const dpp::message& msg, dpp::command_completion_event_t callback = dpp::utility::log_error()) const;

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
}