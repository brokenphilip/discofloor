#include <discofloor/timed_interaction.h>

namespace discofloor
{
	void timed_interaction::on_timeout()
	{
		if (timer_handle_)
		{
			on_timeout_(*this);
			owner_->stop_timer(timer_handle_);
			timer_handle_ = 0;
		}
	}

	void timed_interaction::enable_timer()
	{
		timer_handle_ = owner_->start_timer([this](const dpp::timer& timer)
		{ 
			on_timeout();
		},
		lifespan_seconds);
	}

	timed_interaction::timed_interaction(dpp::cluster* owner, const std::string& token, on_timeout_fn on_timeout)
		: owner_(owner), token_(token), on_timeout_(on_timeout)
	{
		enable_timer();
	}

	timed_interaction::timed_interaction(dpp::cluster* owner, const dpp::interaction& interaction, on_timeout_fn on_timeout)
		: owner_(owner), token_(interaction.token), on_timeout_(on_timeout)
	{
		enable_timer();
	}

	timed_interaction::timed_interaction(const dpp::interaction_create_t& interaction_create, on_timeout_fn on_timeout)
		: owner_(interaction_create.owner), token_(interaction_create.command.token), on_timeout_(on_timeout)
	{
		enable_timer();
	}

	timed_interaction::~timed_interaction()
	{
		on_timeout();
	}

	void timed_interaction::get(dpp::command_completion_event_t callback) const
	{
		owner_->interaction_response_get_original(token_, callback);
	}

	auto timed_interaction::co_get() const
	{
		return owner_->co_interaction_response_get_original(token_);
	}

	void timed_interaction::edit(const dpp::message& new_msg, dpp::command_completion_event_t callback) const
	{
		owner_->interaction_response_edit(token_, new_msg, callback);
	}

	auto timed_interaction::co_edit(const dpp::message& new_msg) const
	{
		return owner_->co_interaction_response_edit(token_, new_msg);
	}

	void timed_interaction::remove(dpp::command_completion_event_t callback) const
	{
		// i'm not sure if this was named "followup" by mistake but this works just fine for responses
		// it even says that it's for responses in the comments (whereas you can't delete followups)
		owner_->interaction_followup_delete(token_, callback);
	}

	auto timed_interaction::co_remove() const
	{
		return owner_->co_interaction_followup_delete(token_);
	}

	void timed_interaction::followup(const dpp::message& followup_msg, dpp::command_completion_event_t callback) const
	{
		owner_->interaction_followup_create(token_, followup_msg, callback);
	}

	auto timed_interaction::co_followup(const dpp::message& followup_msg) const
	{
		return owner_->co_interaction_followup_create(token_, followup_msg);
	}

	void timed_interaction::followup_get(const dpp::snowflake& fu_msg_id, dpp::command_completion_event_t callback) const
	{
		owner_->interaction_followup_get(token_, fu_msg_id, callback);
	}

	auto timed_interaction::co_followup_get(const dpp::snowflake& fu_msg_id) const
	{
		return owner_->co_interaction_followup_get(token_, fu_msg_id);
	}

	void timed_interaction::followup_edit(const dpp::snowflake& fu_msg_id, const dpp::message& new_msg, dpp::command_completion_event_t callback) const
	{
		auto msg = new_msg;
		msg.id = fu_msg_id;
		owner_->interaction_followup_edit(token_, msg, callback);
	}

	auto timed_interaction::co_followup_edit(const dpp::snowflake& fu_msg_id, const dpp::message& new_msg) const
	{
		auto msg = new_msg; msg.id = fu_msg_id; return owner_->co_interaction_followup_edit(token_, msg);
	}


}