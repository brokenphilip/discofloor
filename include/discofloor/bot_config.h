#pragma once

namespace discofloor
{
    // Configuration file structure which can be used to load fixedphilip bot/cluster settings
    struct bot_config : public discofloor::file::json
    {
        std::string token = FIXEDPHILIP_DEFAULT_TOKEN;
        bot_settings settings;

        virtual nlohmann::json struct_to_json() const override final;
        virtual bool json_to_struct(const nlohmann::json& data) override final;

        // Use this instead of load() to load the config
        // If it returns true, proceed with instantiating the bot
        bool load_from_file(const std::string& filename);
    };
}