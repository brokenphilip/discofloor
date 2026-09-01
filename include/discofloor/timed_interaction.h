#pragma once

#include <dpp/dpp.h>

namespace discofloor
{
	class timed_interaction;

	// Called when a timed_interaction times out, with the (timed-out) interaction itself as the parameter
	// This is also called when the timed_interaction class in question is about to be destroyed manually
	// Do NOT delete the interaction from here, instead use this as a final resort to edit or delete its contents before it's invalid
	// This will call timed_interaction::lifespan_seconds (14 minutes) after the creation of a timed_interaction
	using on_timeout_fn = std::function<void(const timed_interaction&)>;

	// Given a cluster (that owns the interaction), an "on timeout" function, as well as either:
	// - the token of the interaction
	// - the interaction itself
	// - the event that created the interaction
	// ...this class provides a wrapper for interactions that can timeout
	class timed_interaction
	{
		dpp::cluster* owner_;
		std::string token_;
		on_timeout_fn on_timeout_;

		dpp::timer timer_handle_ = 0;

		void on_timeout();
		void enable_timer();
	public:
		static constexpr int lifespan_seconds = 60 * 14;

		timed_interaction(dpp::cluster* owner, const std::string& token, on_timeout_fn on_timeout);
		timed_interaction(dpp::cluster* owner, const dpp::interaction& interaction, on_timeout_fn on_timeout);
		timed_interaction(const dpp::interaction_create_t& interaction_create, on_timeout_fn on_timeout);
		~timed_interaction();

		// Returns whether this interaction has timed out and is no longer valid
		// Calling any of get/edit/remove/followup will return errors if this is true
		// Consider deleting timed interactions once they're invalid (but NOT in on_timeout_fn)
		inline bool timed_out() const { return timer_handle_ == 0; }

		// Get the owning cluster of this timed interaction
		inline auto owner() const { return owner_; }

		// Get the message of this timed interaction
		// Returns dpp::message as callback value unless errored
		void get(dpp::command_completion_event_t callback = dpp::utility::log_error()) const;

		// Get the message of this timed interaction
		// Returns dpp::message as callback value unless errored
		auto co_get() const;

		// Edit the message of this timed interaction
		// Returns dpp::confirmation as callback value unless errored
		void edit(const dpp::message& new_msg, dpp::command_completion_event_t callback = dpp::utility::log_error()) const;

		// Edit the message of this timed interaction
		// Returns dpp::confirmation as callback value unless errored
		auto co_edit(const dpp::message& new_msg) const;

		// Remove (delete) the message of this timed interaction
		// Returns dpp::confirmation as callback value unless errored
		void remove(dpp::command_completion_event_t callback = dpp::utility::log_error()) const;

		// Remove (delete) the message of this timed interaction
		// Returns dpp::confirmation as callback value unless errored
		auto co_remove() const;

		// For this timed interaction, create a followup message
		// Returns dpp::confirmation as callback value unless errored
		void followup(const dpp::message& followup_msg, dpp::command_completion_event_t callback = dpp::utility::log_error()) const;

		// For this timed interaction, create a followup message
		// Returns dpp::confirmation as callback value unless errored
		auto co_followup(const dpp::message& followup_msg) const;

		// For this timed interaction, given a followup message ID, get its contents
		// Returns dpp::message as callback value unless errored
		void followup_get(const dpp::snowflake& fu_msg_id, dpp::command_completion_event_t callback = dpp::utility::log_error()) const;

		// For this timed interaction, given a followup message ID, get its contents
		// Returns dpp::message as callback value unless errored
		auto co_followup_get(const dpp::snowflake& fu_msg_id) const;

		// For this timed interaction, given a followup message ID, edit it with new contents
		// Returns dpp::confirmation as callback value unless errored
		void followup_edit(const dpp::snowflake& fu_msg_id, const dpp::message& new_msg, dpp::command_completion_event_t callback = dpp::utility::log_error()) const;

		// For this timed interaction, given a followup message ID, edit it with new contents
		// Returns dpp::confirmation as callback value unless errored
		auto co_followup_edit(const dpp::snowflake& fu_msg_id, const dpp::message& new_msg) const;
	};
}