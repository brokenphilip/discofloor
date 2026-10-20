#include <discofloor/utility.h>

namespace discofloor
{
    std::string get_username(const dpp::user& user)
    {
        auto username = user.username;

        auto discrim = user.discriminator;
        if (discrim)
        {
            username += "#" + dpp::leading_zeroes(discrim, 4);
        }
        return username;
    }

    dpp::message container_msg(const std::string& message, uint32_t accent)
    {
        dpp::component content;
        content.set_type(dpp::cot_text_display);
        content.set_content(message);

        dpp::component container;
        container.set_type(dpp::cot_container);
        container.add_component_v2(content);
        if (accent != UINT_MAX)
        {
            container.set_accent(accent);
        }

        dpp::message msg;
        msg.set_flags(dpp::m_using_components_v2);
        msg.add_component_v2(container);

        return msg;
    }

    std::string message_url(const dpp::snowflake& guild_id, const dpp::snowflake& channel_id, const dpp::snowflake& message_id)
    {
        if (guild_id.empty())
        {
            return dpp::utility::url_host + "/channels/@me/" + std::to_string(channel_id) + "/" + std::to_string(message_id);
        }
        return dpp::utility::message_url(guild_id, channel_id, message_id);
    }

    void log_event(const dpp::event_dispatch_t& event, dpp::loglevel severity, const std::string& message)
    {
        event.owner->get_shard(event.shard)->log(severity, message);
    }

    std::string command_option_name(dpp::command_option_type co)
    {
        switch (co)
        {
            case dpp::co_sub_command: return "subcommand";
            case dpp::co_sub_command_group: return "subcommand group";
            case dpp::co_string: return "string";
            case dpp::co_integer: return "integer";
            case dpp::co_boolean: return "boolean";
            case dpp::co_user: return "user";
            case dpp::co_channel: return "channel";
            case dpp::co_role: return "role";
            case dpp::co_mentionable: return "mentionable";
            case dpp::co_number: return "number";
            case dpp::co_attachment: return "attachment";
        }
        return "";
    }

    size_t get_component_size(const dpp::component& component)
    {
        size_t size = component.type == dpp::cot_text_display ? component.content.size() : 0;
        for (const auto& child : component.components)
        {
            size += get_component_size(child);
        }
        return size;
    }
}