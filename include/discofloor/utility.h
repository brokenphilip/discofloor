#pragma once

#include <dpp/dpp.h>

#include <string>

namespace discofloor
{
	// Get a user's username, appending the discrim for bots (for humans it does not)
	std::string get_username(const dpp::user& user);

	// Convenient wrapper for a containerized message
	// discofloor bots (by default) use this internally to reply to users
	dpp::message container_msg(const std::string& message, uint32_t accent = UINT_MAX);

	// Modified version of dpp::message.get_url()/dpp::utility::message_url() to support non-guild ((group) dm) messages
	std::string message_url(const dpp::snowflake& guild_id, const dpp::snowflake& channel_id, const dpp::snowflake& message_id);

	// Send a log message to the cluster/shard that's responding to the event
	// Preferrable over event.owner->log(...) because this retains cluster/shard info
	void log_event(const dpp::event_dispatch_t& event, dpp::loglevel severity, const std::string& message);
}