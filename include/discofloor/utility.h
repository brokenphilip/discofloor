#pragma once

#include <dpp/dpp.h>

#include <string>

namespace discofloor
{
	// Get a user's username, appending the discrim for bots (for humans it does not)
	std::string get_username(const dpp::user& user);

	// Convenient wrapper for a containerized message
	// discofloor uses this internally to reply to users
	// ...using 0xFF0000 (red) accent for errors, and no accent color otherwise
	dpp::message container_msg(const std::string& message, uint32_t accent = UINT_MAX);

	// Modified version of dpp::message.get_url()/dpp::utility::message_url() to support non-guild ((group) dm) messages
	std::string message_url(const dpp::snowflake& guild_id, const dpp::snowflake& channel_id, const dpp::snowflake& message_id);
}