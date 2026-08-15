#pragma once

namespace discofloor
{
    // Slash command wrapper with an accompanying run function that supports:
    // - old-style (chat prefix) commands, unless disabled (blank prefix)
    // - "CHAT_INPUT" ie. regular (chat) slash commands
    // - "USER" ie. (right-click) user context menu commands
    // - "MESSAGE" ie. (right-click) message context menu commands
    class command : public dpp::slashcommand
    {
    public:
        using run_fn = std::function<dpp::task<void>(const run_event&)>;
    private:
        run_fn run_fn_;
    public:
        inline command(const std::string& name, const std::string& description, const dpp::snowflake application_id, run_fn run_fn)
            : dpp::slashcommand(name, description, application_id), run_fn_(run_fn) {}

        inline command(const std::string& name, const dpp::slashcommand_contextmenu_type type, const dpp::snowflake application_id, run_fn run_fn)
            : dpp::slashcommand(name, type, application_id), run_fn_(run_fn) {}

        inline auto get_run_fn() const { return run_fn_; }
        inline dpp::task<void> run(const run_event& event) const { co_await run_fn_(event); }

        template <typename T>
        static T try_get_parameter(const dpp::slashcommand_t& command, const std::string& param_name, T default_value) 
        {
            if (auto param = command.get_parameter(param_name); auto value = std::get_if<T>(&param))
            {
                return *value;
            }
            return default_value;
        }
    };
}