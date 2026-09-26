#pragma once

#include <discofloor/run_event.h>

namespace discofloor
{
	// Type of function used by discofloor commands to receive run events
	using run_fn = std::function<dpp::task<void>(const run_event&)>;

	// Slash command wrapper with an accompanying run function that receives run events (see struct above)
	class bot_command : public dpp::slashcommand
	{
		std::vector<std::string> chat_command_examples_;
		run_fn run_fn_;
	public:
		inline bot_command(const std::string& name, const std::string& description, const dpp::snowflake application_id, run_fn run_fn, const std::vector<std::string>& chat_command_examples = {})
			: dpp::slashcommand(name, description, application_id), run_fn_(run_fn), chat_command_examples_(chat_command_examples) {}

		inline bot_command(const std::string& name, const dpp::slashcommand_contextmenu_type type, const dpp::snowflake application_id, run_fn run_fn)
			: dpp::slashcommand(name, type, application_id), run_fn_(run_fn) {}

		inline std::vector<std::string> chat_command_examples() { return chat_command_examples_; }

		inline auto get_run_fn() const { return run_fn_; }
		inline dpp::task<void> run(const run_event& event) const { co_await run_fn_(event); }
	};
}