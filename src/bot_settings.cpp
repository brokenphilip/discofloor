#include <discofloor/bot_settings.h>

namespace discofloor
{
    nlohmann::json bot_settings::struct_to_json() const
    {
        return
        {
            { "prefix", prefix },
            { "disabled_modules", disabled_modules },
            { "data_folder", data_folder },
            { "max_data_size_id", fixedphilip::file::size_to_string(max_data_size_id) },
            { "max_data_size_total", fixedphilip::file::size_to_string(max_data_size_total) },
        };
    }

    bool bot_settings::json_to_struct(const nlohmann::json& data)
    {
        fixedphilip::file::json_try_at(data, "prefix", prefix, true);
        fixedphilip::file::json_try_at(data, "disabled_modules", disabled_modules, true);
        fixedphilip::file::json_try_at(data, "data_folder", data_folder, true);

        std::string max_data_size_id_str;
        if (fixedphilip::file::json_try_at(data, "max_data_size_id", max_data_size_id_str, true))
        {
            try
            {
                fixedphilip::math::number_t max_data_size_id_num;
                fixedphilip::math::conversion::convert(max_data_size_id_str, "b", -1, false, nullptr, nullptr, &max_data_size_id_num);
                max_data_size_id = static_cast<uintmax_t>(max_data_size_id_num);
            }
            catch (std::exception& e)
            {
                fixedphilip::log::error(std::format("Failed to parse 'max_data_size_id' for bot settings - {}", e.what()));
            }
        }

        std::string max_data_size_total_str;
        if (fixedphilip::file::json_try_at(data, "max_data_size_total", max_data_size_total_str, true))
        {
            try
            {
                fixedphilip::math::number_t max_data_size_total_num;
                fixedphilip::math::conversion::convert(max_data_size_total_str, "b", -1, false, nullptr, nullptr, &max_data_size_total_num);
                max_data_size_total = static_cast<uintmax_t>(max_data_size_total_num);
            }
            catch (std::exception& e)
            {
                fixedphilip::log::error(std::format("Failed to parse 'max_data_size_total' for bot settings - {}", e.what()));
            }
        }

        // partial load is okay
        return true;
    }
}