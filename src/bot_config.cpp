#include <discofloor/bot_config.h>

namespace discofloor
{
    nlohmann::json bot_config::struct_to_json() const
    {
        nlohmann::json data
        {
            { "token", token },
        };
        data.update(settings.struct_to_json());
        return data;
    }

    bool bot_config::json_to_struct(const nlohmann::json& data)
    {
        // create a copy of the data we will pass down to settings, but without the token
        auto data_copy = data;
        bool token_valid = fixedphilip::file::json_try_at(data_copy, "token", token, true);
        data_copy.erase("token");
        return settings.json_to_struct(data_copy);
    }

    bool bot_config::load_from_file(const std::string& filename)
    {
        fixedphilip::file::settings config_settings
        {
            .filename = filename,
            .create_if_not_found = true,
            .log = true,
        };

        auto result = load(config_settings);
        if (result == fixedphilip::file::r_file_not_found)
        {
            fixedphilip::log::warning("Default config saved - make sure to update your bot token");
            return false;
        }
        else if (result != fixedphilip::file::r_success)
        {
            // logs are already printed for us
            return false;
        }

        if (token == FIXEDPHILIP_DEFAULT_TOKEN || token.empty())
        {
            fixedphilip::log::error("Bot token not set in config file");
            return false;
        }

        if (settings.prefix.empty())
        {
            fixedphilip::log::info("Old-style commands disabled (prefix is blank)");
        }
        else
        {
            fixedphilip::log::info(std::format("Global prefix for old-style commands set to '{}'", settings.prefix));
        }

        if (settings.disabled_modules.empty())
        {
            fixedphilip::log::info("No modules will be disabled");
        }
        else
        {
            std::string result_log = std::format("If existing and enabled, {} module{} will be disabled", settings.disabled_modules.size(), settings.disabled_modules.size() == 1 ? "" : "s");
            bool first_module = true;
            for (auto& module : settings.disabled_modules)
            {
                if (first_module)
                {
                    result_log += ": '" + module + "'";
                }
                else
                {
                    result_log += ", '" + module + "'";
                }
                first_module = false;
            }
            fixedphilip::log::info(result_log);
        }

        // using this opportunity to add any new keys that might not exist
        save(config_settings);
        return true;
    }
}