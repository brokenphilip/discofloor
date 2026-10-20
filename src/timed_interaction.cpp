#include <discofloor/timed_interaction.h>

#include <discofloor/utility.h>

namespace discofloor
{
	void timed_interaction::on_timeout(bool skip_callback)
	{
		std::unique_lock _(mutex_);
		if (timer_handle_)
		{
			if (!skip_callback)
			{
				timing_out_.store(true);
				on_timeout_(*this);
				timing_out_.store(false);
			}

			owner_->stop_timer(timer_handle_);
			timer_handle_ = 0;
		}
	}

	void timed_interaction::enable_timer()
	{
		std::unique_lock _(mutex_);
		if (lifespan_seconds_ < minimum_lifespan_seconds)
		{
			lifespan_seconds_ = minimum_lifespan_seconds;
		}
		if (!is_guild_interaction() && lifespan_seconds_ > recommended_lifespan_seconds)
		{
			lifespan_seconds_ = recommended_lifespan_seconds;
		}
		timer_handle_ = owner_->start_timer([this](const dpp::timer& timer)
		{ 
			on_timeout(false);
		},
		lifespan_seconds_);
	}

	timed_interaction::timed_interaction(dpp::cluster* owner, const std::string& token, dpp::snowflake guild, dpp::snowflake channel, dpp::snowflake message,
		bool is_guild_interaction, uint32_t lifespan_seconds, on_timeout_fn on_timeout)
		: owner_(owner), token_(token), on_timeout_(on_timeout), guild_(guild), channel_(channel), message_(message),
		guild_interaction_(is_guild_interaction), lifespan_seconds_(lifespan_seconds)
	{
		if (!owner)
		{
			throw std::invalid_argument("Cluster cannot be null");
		}
		if (token.empty())
		{
			throw std::invalid_argument("Token cannot be empty");
		}
		if (!on_timeout)
		{
			throw std::invalid_argument("Timeout callback cannot be empty");
		}
		enable_timer();
	}

	timed_interaction::~timed_interaction()
	{
		on_timeout(false);
	}

	void timed_interaction::update(const std::string& token, dpp::snowflake guild, dpp::snowflake channel, dpp::snowflake message,
		uint32_t lifespan_seconds, on_timeout_fn on_timeout)
	{
		{
			std::unique_lock _(mutex_);
			if (timer_handle_)
			{
				owner_->stop_timer(timer_handle_);
				timer_handle_ = 0;
			}
			token_ = token;
			guild_ = guild;
			channel_ = channel;
			message_ = message;
			if (lifespan_seconds)
			{
				lifespan_seconds_ = lifespan_seconds;
			}
			if (on_timeout)
			{
				on_timeout_ = on_timeout;
			}
		}
		enable_timer();
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

	void timed_book::update_timed_interaction(const dpp::interaction_create_t& event)
	{
		timed_interaction_.update(event);
		last_update_ = std::time(nullptr);
	}

	dpp::message timed_book::get_page(int index, bool buttons_enabled)
	{
		dpp::component container;
		container.set_type(dpp::cot_container);

		if (!title_.empty())
		{
			dpp::component title_content;
			title_content.set_type(dpp::cot_text_display);
			title_content.set_content(title_);

			dpp::component title_separator;
			title_separator.set_type(dpp::cot_separator);
			title_separator.set_spacing(dpp::sep_small);
			title_separator.set_divider(true);

			container.add_component_v2(title_content);
			container.add_component_v2(title_separator);
		}

		dpp::component main_separator;
		main_separator.set_type(dpp::cot_separator);
		main_separator.set_spacing(dpp::sep_small);
		main_separator.set_divider(true);

		auto expiry = buttons_enabled ?
			"Interaction expires at " + dpp::utility::timestamp(last_update_ + timed_interaction_.lifespan_seconds(), dpp::utility::tf_short_time) :
			"This interaction has expired";

		std::string user = user_ ? "Only " + dpp::utility::user_mention(user_) : "Anyone";

		dpp::component footer_content;
		footer_content.set_type(dpp::cot_text_display);
		footer_content.set_content(std::format("-# {} can control this menu\n-# {}", user, expiry));

		auto page_count = contents_.size();
		auto disable_first_prev = (index == 0) || !buttons_enabled;
		auto disable_next_last = (index == page_count - 1) || !buttons_enabled;

		// buttons need to be added to an action row first
		// they can't be added directly to the container
		// afterwards we add the whole action row to the container
		dpp::component actions;
		actions.set_type(dpp::cot_action_row);

		dpp::component button_first;
		button_first.set_type(dpp::cot_button);
		button_first.set_emoji("⏮️");
		button_first.set_style(dpp::cos_secondary);
		button_first.set_id(generate_first_page_custom_id());
		button_first.set_disabled(disable_first_prev);

		dpp::component button_prev;
		button_prev.set_type(dpp::cot_button);
		button_prev.set_emoji("◀️");
		button_prev.set_style(dpp::cos_secondary);
		button_prev.set_id(generate_prev_page_custom_id());
		button_prev.set_disabled(disable_first_prev);

		dpp::component button_jump;
		button_jump.set_type(dpp::cot_button);
		button_jump.set_label(std::format("{} / {}", index + 1, page_count));
		button_jump.set_style(dpp::cos_primary);
		button_jump.set_id(generate_jump_page_custom_id());
		button_jump.set_disabled(disable_first_prev && disable_next_last);

		dpp::component button_next;
		button_next.set_type(dpp::cot_button);
		button_next.set_emoji("▶️");
		button_next.set_style(dpp::cos_secondary);
		button_next.set_id(generate_next_page_custom_id());
		button_next.set_disabled(disable_next_last);

		dpp::component button_last;
		button_last.set_type(dpp::cot_button);
		button_last.set_emoji("⏭️");
		button_last.set_style(dpp::cos_secondary);
		button_last.set_id(generate_last_page_custom_id());
		button_last.set_disabled(disable_next_last);

		actions.add_component_v2(button_first);
		actions.add_component_v2(button_prev);
		actions.add_component_v2(button_jump);
		actions.add_component_v2(button_next);
		actions.add_component_v2(button_last);

		for (const auto& component : contents_[index])
		{
			container.add_component_v2(component);
		}

		container.add_component_v2(main_separator);
		container.add_component_v2(footer_content);
		container.add_component_v2(actions);

		return dpp::message().set_flags(dpp::m_using_components_v2).add_component_v2(container);
	}

	void timed_book::button_click_event(const dpp::button_click_t& event)
	{
		if (timed_out())
		{
			return;
		}

		// if user is 0, anyone can use the menu, otherwise check for user id
		if (user_ && event.command.usr.id != user_)
		{
			return;
		}

		// only handle this event if it's ours (ie. if it has our custom id)
		if (event.custom_id == generate_first_page_custom_id())
		{
			update_timed_interaction(event);
			timed_interaction_.edit(get_page(active_page_index_ = 0));
			event.reply(dpp::ir_deferred_update_message, "");
		}
		if (event.custom_id == generate_prev_page_custom_id())
		{
			update_timed_interaction(event);
			timed_interaction_.edit(get_page(active_page_index_ = active_page_index_ == 0 ? 0 : active_page_index_ - 1));
			event.reply(dpp::ir_deferred_update_message, "");
		}
		else if (event.custom_id == generate_jump_page_custom_id())
		{
			update_timed_interaction(event);
			dpp::component select_page;
			select_page.set_id("select_page");
			select_page.set_type(dpp::cot_text);
			select_page.set_label(std::format("Enter page number from 1 to {}", contents_.size()));
			select_page.set_text_style(dpp::text_short);
			select_page.set_required(true);
			event.dialog(dpp::interaction_modal_response(event.custom_id, "Jump to Page", { select_page }));
		}
		else if (event.custom_id == generate_next_page_custom_id())
		{
			update_timed_interaction(event);
			timed_interaction_.edit(get_page(active_page_index_ = active_page_index_ == contents_.size() - 1 ? contents_.size() - 1 : active_page_index_ + 1));
			event.reply(dpp::ir_deferred_update_message, "");
		}
		else if (event.custom_id == generate_last_page_custom_id())
		{
			update_timed_interaction(event);
			timed_interaction_.edit(get_page(active_page_index_ = contents_.size() - 1));
			event.reply(dpp::ir_deferred_update_message, "");
		}
	}

	void timed_book::form_submit_event(const dpp::form_submit_t& event)
	{
		if (timed_out())
		{
			return;
		}

		// only handle this event if it's ours (ie. if it has our id)
		if (event.custom_id == generate_jump_page_custom_id())
		{
			auto page_number_str = std::get<std::string>(event.components[0].value);
			try
			{
				auto page_index = std::stoi(page_number_str) - 1;
				if (page_index > -1 && page_index < contents_.size())
				{
					timed_interaction_.edit(get_page(active_page_index_ = page_index));
					event.reply(dpp::ir_deferred_update_message, "");
				}
			}
			catch (std::exception&) {}
			// "silent" (discord-handled) fail if page index is invalid
		}
	}

	void timed_book::on_timeout()
	{
		timed_interaction_.owner()->on_form_submit.detach(form_submit_handle);
		timed_interaction_.owner()->on_form_submit.detach(button_click_handle);

		if (timeout_method_ == tm_disable_buttons)
		{
			timed_interaction_.edit(get_page(active_page_index_, false));
		}
		else if (timeout_method_ == tm_delete_message)
		{
			timed_interaction_.remove();
		}
	}

	std::vector<std::vector<dpp::component>> timed_book::make_component_vectors_from_string_contents_vector(const std::vector<std::string> contents)
	{
		std::vector<std::vector<dpp::component>> new_contents;
		for (const auto& content : contents)
		{
			auto& component_vector = new_contents.emplace_back();
			auto& component = component_vector.emplace_back();
			component.set_type(dpp::cot_text_display);
			component.set_content(content);
		}
		return new_contents;
	}

	timed_book::timed_book(const std::string& title, const std::vector<std::vector<dpp::component>>& contents, timeout_method tm, dpp::snowflake user,
		size_t initial_page, dpp::snowflake interaction_id, dpp::cluster* owner, const std::string& token,
		dpp::snowflake guild, dpp::snowflake channel, dpp::snowflake message, bool is_guild_interaction, uint32_t lifespan_seconds)
		: title_(title), contents_(contents), timeout_method_(tm), user_(user),
		timed_interaction_(owner, token, guild, channel, message, is_guild_interaction, lifespan_seconds, [this](timed_interaction&) { on_timeout(); })
	{
		if (contents.empty())
		{
			throw std::invalid_argument("Contents vector cannot be empty");
		}
		if (tm >= tm_count)
		{
			throw std::invalid_argument("Not a valid timeout method");
		}
		if (initial_page < 0 || initial_page >= contents.size())
		{
			throw std::invalid_argument(std::format("Not a valid initial page index - expected a value between 0 and {} (inclusive), got {}", contents.size(), initial_page));
		}
		if (title.size() > 500)
		{
			throw std::invalid_argument(std::format("Title text cannot be longer than 500 characters - got a length of {}", title.size()));
		}
		for (int i = 0; i < contents.size(); i++)
		{
			size_t size = 0;
			for (auto& component : contents[i])
			{
				size += get_component_size(component);
			}
			if (size > 3'000)
			{
				throw std::invalid_argument(std::format("Per-page component vector content size cannot be longer than 3000 characters - got a length of {} at index {}", size, i));
			}
		}

		button_click_handle = owner->on_button_click.attach([this](const dpp::button_click_t& event) { button_click_event(event); });
		form_submit_handle = owner->on_form_submit.attach([this](const dpp::form_submit_t& event) { form_submit_event(event); });

		auto msg = get_page(active_page_index_ = initial_page);
		msg.guild_id = guild;
		msg.channel_id = channel;
		owner->interaction_response_create(interaction_id, token, dpp::interaction_response(dpp::ir_channel_message_with_source, msg));
	}

	std::vector<std::string> timed_book::split_contents(const std::string& contents, const std::string& main_separator,
		const std::initializer_list<std::string>& backup_separators, size_t min_length, size_t max_length)
	{
		/*
		auto split_using_backup_separators = [&backup_separators](const std::string& contents)
			{

			};

		if (min_length >= max_length)
		{
			throw std::invalid_argument("Minimum cannot be equal or grater than maximum length");
		}
		if (contents.empty())
		{
			return {};
		}

		auto main_separator_len = main_separator.length();

		std::vector<std::string> pages;
		auto contents_copy = contents;

		size_t pos = 0;
		while ((pos = contents_copy.find(main_separator)) != std::string::npos)
		{
			pages.push_back(contents_copy.substr(0, pos));
			contents_copy.erase(0, pos + main_separator_len);
		}

		for (auto& page : pages)
		{
			if (page.size())
		}*/

		return std::vector<std::string>();
	}
}