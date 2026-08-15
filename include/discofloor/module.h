#pragma once

namespace discofloor
{
    // Base module interface - inherit this class to create your own custom module
    class module : public bulbtils::named_node<module>
    {
        // TODO: command which lists all modules, active and inactive
        const char* description_;
    public:
        inline module(const char* name, const char* description) : named_node<module>(name), description_(description) {}
        inline virtual ~module() {}

        inline virtual bool init(bot& bot) { return true; }
        inline virtual std::vector<command> commands(bot& bot) { return {}; }
        inline virtual void destroy(bot& bot) {}

        inline auto description() { return description_; }
    };
}