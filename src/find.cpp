#include <discofloor/find.h>
#include <discofloor/utility.h>

namespace discofloor::find
{
    std::vector<user_result> users(const std::string& query, dpp::snowflake guild_id)
    {
        auto user = dpp::find_user(dpp::snowflake(query));
        if (user)
        {
            // the parameter is a snowflake, so we allow direct matches
            return { { user, "User", discofloor::get_username(*user) } };
        }

        auto guild = dpp::find_guild(guild_id);
        if (!guild)
        {
            // for privacy, we only allow searching for users within a guild
            return {};
        }

        auto query_lowercase = query;
        bulbtils::string::inplace::to_lowercase(query_lowercase);

        std::vector<user_result> results;
        for (auto& [user_id, guild_member] : guild->members)
        {
            auto user_it = dpp::find_user(user_id);

            // find a user by their username
            auto username = discofloor::get_username(*user_it);
            auto username_lowercase = username;
            bulbtils::string::inplace::to_lowercase(username_lowercase);
            if (username_lowercase.find(query_lowercase) != std::string::npos)
            {
                results.emplace_back(user_it, "User", username);
                continue;
            }

            // find a user by their "global name" - essentially the "nick" name they have set globally on discord
            auto global_name = user_it->global_name;
            if (!global_name.empty())
            {
                auto global_name_lowercase = global_name;
                bulbtils::string::inplace::to_lowercase(global_name_lowercase);
                if (global_name_lowercase.find(query_lowercase) != std::string::npos)
                {
                    results.emplace_back(user_it, "Global name of user " + username, global_name);
                    continue;
                }
            }

            // find a user by their guild nickname
            auto nickname = guild_member.get_nickname();
            if (!nickname.empty())
            {
                auto nickname_lowercase = nickname;
                bulbtils::string::inplace::to_lowercase(nickname_lowercase);
                if (nickname_lowercase.find(query_lowercase) != std::string::npos)
                {
                    results.emplace_back(user_it, "Nickname of user " + username, nickname);
                    continue;
                }
            }
        }
        return results;
    }

    std::vector<channel_result> channels(const std::string& query, dpp::snowflake guild_id)
    {
        auto channel = dpp::find_channel(dpp::snowflake(query));
        if (channel)
        {
            // the parameter is a snowflake, so we allow direct matches
            return { { channel, "Channel", channel->name } };
        }

        auto guild = dpp::find_guild(guild_id);
        if (!guild)
        {
            // for privacy, we only allow searching for users within a guild
            return {};
        }

        auto query_lowercase = query;
        bulbtils::string::inplace::to_lowercase(query_lowercase);

        std::vector<channel_result> results;
        for (auto& channel_id : guild->channels)
        {
            auto channel_it = dpp::find_channel(channel_id);

            // find a channel by its name
            auto name = channel_it->name;
            auto name_lowercase = name;
            bulbtils::string::inplace::to_lowercase(name_lowercase);
            if (name_lowercase.find(query_lowercase) != std::string::npos)
            {
                results.emplace_back(channel_it, "Role", name);
                continue;
            }
        }
        return results;
    }

    std::vector<role_result> roles(const std::string& query, dpp::snowflake guild_id)
    {
        auto role = dpp::find_role(dpp::snowflake(query));
        if (role)
        {
            // the parameter is a snowflake, so we allow direct matches
            return { { role, "Role", role->name } };
        }

        auto guild = dpp::find_guild(guild_id);
        if (!guild)
        {
            // for privacy, we only allow searching for users within a guild
            return {};
        }

        auto query_lowercase = query;
        bulbtils::string::inplace::to_lowercase(query_lowercase);

        std::vector<role_result> results;
        for (auto& role_id : guild->roles)
        {
            auto role_it = dpp::find_role(role_id);

            // find a role by its name
            auto name = role_it->name;
            auto name_lowercase = name;
            bulbtils::string::inplace::to_lowercase(name_lowercase);
            if (name_lowercase.find(query_lowercase) != std::string::npos)
            {
                results.emplace_back(role_it, "Role", name);
                continue;
            }
        }
        return results;
    }
}
