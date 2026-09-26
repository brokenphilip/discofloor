#pragma once

#include <bulbtils/string.h>

#include <dpp/dpp.h>

namespace discofloor::find
{
    // Additional information/data about a search result
    struct result_data
    {
        // user-friendly info about what kind of result this is
        std::string type;

        // the value of this kind of result (ie. the thing the user searched for)
        std::string value;
    };

    // A single result of a discofloor::find::* search
    template <typename T>
    struct result
    {
        // the resulting value of the search, ie. what the user wanted to find
        T value;

        // the result of the search, ie. what the user searched for
        result_data data;

        result(const T& value, const std::string& data_type, const std::string& data_value)
            : value(value), data(data_type, data_value) {
        }
    };

    // Formal specifications for individual results
    using user_result = result<dpp::user*>;
    //using guild_result = result<dpp::guild*>;
    using channel_result = result<dpp::channel*>;
    using role_result = result<dpp::role*>;
    using id_result = result<dpp::snowflake>;

    //user_results id_to_user_results(const id_results& results);
    //guild_results id_to_guild_results(const id_results& results);
    //channel_results id_to_channel_results(const id_results& results);
    //role_results id_to_role_results(const id_results& results);

    // Given a dpp::managed value type (ie. non-pointer) T, convert the results from T* to dpp::snowflake
    template <typename T>
    std::vector<id_result> to_id_results(const std::vector<result<T*>>& results)
    {
        if constexpr (std::is_base_of_v<dpp::managed, T>)
        {
            std::vector<id_result> new_results;
            for (const auto& result : results)
            {
                new_results.emplace_back(result.value->id, result.data.type, result.data.value);
            }
            return new_results;
        }
        else
        {
            static_assert(!sizeof(T*), "T is not of dpp::managed type");
        }
    }

    std::vector<user_result> users(const std::string& query, dpp::snowflake guild_id);
    std::vector<channel_result> channels(const std::string& query, dpp::snowflake guild_id);
    std::vector<role_result> roles(const std::string& query, dpp::snowflake guild_id);

    // Given a search result vector, returns the found value if it's the only element
    // Otherwise, if there are no elements, or two or more elements, throws a detailed std::invalid_argument
    template <typename T>
    T parse_results(const std::vector<result<T>>& results)
    {
        constexpr bool by_id = std::is_same_v<dpp::snowflake, T>;
        if constexpr (!by_id && !std::is_same_v<dpp::user*, T> && !std::is_same_v<dpp::channel*, T> && !std::is_same_v<dpp::role*, T>)
        {
            static_assert(!sizeof(T*), "T (of result<T>) is not of valid type");
        }

        std::string blablabla = "matches found for the given input (try narrowing your search, providing an ID, or running the command in a guild (that i'm in) if you aren't already)";
        if (results.empty())
        {
            throw std::invalid_argument("No " + blablabla + ".");
        }
        if (results.size() > 1)
        {
            std::string matches = "";
            for (int i = 0; i < results.size(); i++)
            {
                auto& find_result = results[i];

                std::string id_str = "";
                if constexpr (by_id)
                {
                    id_str = std::to_string(find_result.value);
                }
                else
                {
                    id_str = std::to_string(find_result.value->id);
                }
                matches += std::format("\n{}. \"{}\" ({}) - `{}`", i + 1, dpp::utility::markdown_escape(find_result.data.value, true), find_result.data.type, id_str);
            }
            throw std::invalid_argument("Multiple " + blablabla + ":" + matches);
        }
        return results[0].value;
    }

    //id_results users(const std::string& query, dpp::snowflake guild_id = 0);
    //inline id_results users(const std::string& query, const dpp::guild& guild) { return users(query, guild.id); }
}