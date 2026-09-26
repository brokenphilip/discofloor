#include <discofloor/timed_interaction.h>

namespace discofloor
{
	void timed_interaction::on_timeout(bool skip_callback)
	{
		std::unique_lock _(mutex_);
		if (timer_handle_)
		{
			timing_out_.store(true);
			on_timeout_(*this);
			timing_out_.store(false);

			owner_->stop_timer(timer_handle_);
			timer_handle_ = 0;
		}
	}

	void timed_interaction::enable_timer(uint32_t lifespan_seconds)
	{
		std::unique_lock _(mutex_);
		timer_handle_ = owner_->start_timer([this](const dpp::timer& timer)
		{ 
			on_timeout(false);
		},
		lifespan_seconds);
	}

	timed_interaction::timed_interaction(dpp::cluster* owner, const std::string& token, dpp::snowflake guild, dpp::snowflake channel, dpp::snowflake message, uint32_t lifespan_seconds, on_timeout_fn on_timeout)
		: owner_(owner), token_(token), on_timeout_(on_timeout), guild_(guild), channel_(channel), message_(message)
	{
		if (lifespan_seconds < minimum_lifespan_seconds)
		{
			lifespan_seconds = minimum_lifespan_seconds;
		}
		if (!is_guild_interaction() && lifespan_seconds > recommended_lifespan_seconds)
		{
			lifespan_seconds = recommended_lifespan_seconds;
		}
		enable_timer(lifespan_seconds);
	}

	timed_interaction::~timed_interaction()
	{
		on_timeout(false);
	}

	void timed_interaction::force_timeout(bool skip_callback)
	{
		on_timeout(skip_callback);
	}

	bool timed_interaction::timed_out() const
	{
		return with_mutex_shared_lock_and_timer_handle_check([this]()
		{
			return timer_handle_ == 0;
		},
		true);
	}

	void timed_interaction::get(dpp::command_completion_event_t callback) const
	{
		with_mutex_shared_lock_and_timer_handle_check([this, &callback]()
		{
			if (is_guild_interaction())
			{
				owner_->message_get(message_, channel_, callback);
			}
			else
			{
				owner_->interaction_response_get_original(token_, callback);
			}
		});
	}

	auto timed_interaction::co_get() const
	{
		return with_mutex_shared_lock_and_timer_handle_check([this]()
		{
			if (is_guild_interaction())
			{
				return owner_->co_message_get(message_, channel_);
			}
			else
			{
				return owner_->co_interaction_response_get_original(token_);
			}
		});
	}

	void timed_interaction::edit(const dpp::message& new_msg, dpp::command_completion_event_t callback) const
	{
		with_mutex_shared_lock_and_timer_handle_check([this, &new_msg, &callback]()
		{
			if (is_guild_interaction())
			{
				auto msg = new_msg;
				msg.guild_id = guild_;
				msg.channel_id = channel_;
				msg.id = message_;
				owner_->message_edit(msg, callback);
			}
			else
			{
				owner_->interaction_response_edit(token_, new_msg, callback);
			}
		});
	}

	auto timed_interaction::co_edit(const dpp::message& new_msg) const
	{
		return with_mutex_shared_lock_and_timer_handle_check([this, &new_msg]()
		{
			if (is_guild_interaction())
			{
				auto msg = new_msg;
				msg.guild_id = guild_;
				msg.channel_id = channel_;
				msg.id = message_;
				return owner_->co_message_edit(msg);
			}
			else
			{
				return owner_->co_interaction_response_edit(token_, new_msg);
			}
		});
	}

	void timed_interaction::remove(dpp::command_completion_event_t callback) const
	{
		with_mutex_shared_lock_and_timer_handle_check([this, &callback]()
		{
			if (is_guild_interaction())
			{
				owner_->message_delete(message_, channel_, callback);
			}
			else
			{
				// i'm not sure if this was named "followup" by mistake but this works just fine for responses
				// it even says that it's for responses in the comments (whereas you can't delete followups)
				owner_->interaction_followup_delete(token_, callback);
			}
		});
	}

	auto timed_interaction::co_remove() const
	{
		return with_mutex_shared_lock_and_timer_handle_check([this]()
		{
			if (is_guild_interaction())
			{
				return owner_->co_message_delete(message_, channel_);
			}
			else
			{
				return owner_->co_interaction_followup_delete(token_);
			}
		});
	}

	void timed_interaction::followup(const dpp::message& followup_msg, dpp::command_completion_event_t callback) const
	{
		with_mutex_shared_lock_and_timer_handle_check([this, &followup_msg, &callback]()
		{
			if (is_guild_interaction())
			{
				auto msg = followup_msg;
				msg.guild_id = guild_;
				msg.channel_id = channel_;
				msg.id = 0;
				owner_->message_create(msg, callback);
			}
			else
			{
				owner_->interaction_followup_create(token_, followup_msg, callback);
			}
		});
	}

	auto timed_interaction::co_followup(const dpp::message& followup_msg) const
	{
		return with_mutex_shared_lock_and_timer_handle_check([this, &followup_msg]()
		{
			if (is_guild_interaction())
			{
				auto msg = followup_msg;
				msg.guild_id = guild_;
				msg.channel_id = channel_;
				msg.id = 0;
				return owner_->co_message_create(msg);
			}
			else
			{
				return owner_->co_interaction_followup_create(token_, followup_msg);
			}
		});
	}

	void timed_interaction::followup_get(const dpp::snowflake& fu_msg_id, dpp::command_completion_event_t callback) const
	{
		with_mutex_shared_lock_and_timer_handle_check([this, &fu_msg_id, &callback]()
		{
			if (is_guild_interaction())
			{
				owner_->message_get(fu_msg_id, channel_, callback);
			}
			else
			{
				owner_->interaction_followup_get(token_, fu_msg_id, callback);
			}
		});
	}

	auto timed_interaction::co_followup_get(const dpp::snowflake& fu_msg_id) const
	{
		return with_mutex_shared_lock_and_timer_handle_check([this, &fu_msg_id]()
		{
			if (is_guild_interaction())
			{
				return owner_->co_message_get(fu_msg_id, channel_);
			}
			else
			{
				return owner_->co_interaction_followup_get(token_, fu_msg_id);
			}
		});
	}

	void timed_interaction::followup_edit(const dpp::snowflake& fu_msg_id, const dpp::message& new_msg, dpp::command_completion_event_t callback) const
	{
		with_mutex_shared_lock_and_timer_handle_check([this, &fu_msg_id, &new_msg, &callback]()
		{
			auto msg = new_msg;
			msg.id = fu_msg_id;
			if (is_guild_interaction())
			{
				msg.guild_id = guild_;
				msg.channel_id = channel_;
				owner_->message_edit(msg, callback);
			}
			else
			{
				owner_->interaction_followup_edit(token_, msg, callback);
			}
		});
	}

	auto timed_interaction::co_followup_edit(const dpp::snowflake& fu_msg_id, const dpp::message& new_msg) const
	{
		return with_mutex_shared_lock_and_timer_handle_check([this, &fu_msg_id, &new_msg]()
		{
			auto msg = new_msg;
			msg.id = fu_msg_id;
			if (is_guild_interaction())
			{
				msg.guild_id = guild_;
				msg.channel_id = channel_;
				return owner_->co_message_edit(msg);
			}
			else
			{
				return owner_->co_interaction_followup_edit(token_, msg);
			}
		});
	}

	/*dpp::message timed_menu::get_active_page(bool expired)
	{
		return dpp::message();
	}
	
	timed_menu::timed_menu(const std::string& content, const std::string& primary_separator, const std::initializer_list<std::string>& secondary_separators)
	{

	}

	dpp::message timed_menu::create(dpp::cluster* owner, const std::string& token)
	{
		if (interaction_)
		{
			throw std::logic_error("")
		}
	}

	dpp::message timed_menu::create(dpp::cluster* owner, const dpp::interaction& interaction)
	{

	}

	dpp::message timed_menu::create(const dpp::interaction_create_t& interaction_create)
	{

	}*/
}