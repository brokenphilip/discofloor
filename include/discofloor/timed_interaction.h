#pragma once

#include <dpp/dpp.h>

#include <bulbtils/time.h>

namespace discofloor
{
	class timed_interaction;

	// Called when a timed_interaction is timing out, with the (timed-out) interaction itself as the parameter
	// This is also called upon (early/manual) destruction of a timed_interaction (unless timed out already)
	// Use this as a final resort to get, and then edit or delete its contents before it's invalid
	// This will call timed_interaction::lifespan_seconds (10 minutes) after the creation of a timed_interaction
	// Do NOT kill the timed_interaction here - you must manage its lifespan on your own (eg. via GC)
	using on_timeout_fn = std::function<void(timed_interaction&)>;

	// Given a cluster (that owns the interaction), an "on timeout" function, as well as either:
	// - the token of the interaction
	// - the interaction itself
	// - the event that created the interaction
	// ...this class provides a wrapper for interactions that can time out
	class timed_interaction
	{
		mutable std::shared_mutex mutex_;

		dpp::cluster* owner_;
		std::string token_;

		dpp::snowflake guild_;
		dpp::snowflake channel_;
		dpp::snowflake message_;

		on_timeout_fn on_timeout_;
		std::atomic_bool timing_out_ = false;

		dpp::timer timer_handle_ = 0;
	public:
		// Thrown when calling get/edit/remove/followup on a timed-out interaction
		struct timed_out_error : public std::runtime_error
		{
			using runtime_error::runtime_error;
		};
	private:
		template <typename F>
		decltype(auto) with_mutex_shared_lock_and_timer_handle_check(F&& f, bool skip_timer_handle_check = false) const
		{
			auto timing_out = timing_out_.load();
			if (!timing_out)
			{
				mutex_.lock_shared();
				if (!skip_timer_handle_check && timer_handle_ == 0)
				{
					mutex_.unlock_shared();
					throw timed_out_error("Can't use timed-out interaction");
				}
			}
			if constexpr (std::is_void_v<std::invoke_result_t<F&&>>)
			{
				std::invoke(std::forward<F>(f));
				if (!timing_out)
				{
					mutex_.unlock_shared();
				}
			}
			else
			{
				decltype(auto) result = std::invoke(std::forward<F>(f));
				if (!timing_out)
				{
					mutex_.unlock_shared();
				}
				return result;
			}
		}

		void on_timeout(bool skip_callback);
		void enable_timer(uint32_t lifespan_seconds);
	public:
		// Recommended lifespan of a timed_interaction - 10 minutes (they normally last 15)
		// This is also the maximum allowed lifespan for user-installed bot interactions
		// (If you pass a greater value, it will be clamped down to this limit)
		// If the interaction is from a guild-install bot, the lifespan can be infinite
		static constexpr uint32_t recommended_lifespan_seconds = 60 * 10;

		// This is the minimum allowed lifespan for timed interactions
		// (If you pass a lower value, it will be clamped up to this limit)
		static constexpr uint32_t minimum_lifespan_seconds = 30;

		// Infinite lifespan - if passed, only applies to guild-installed bot interactions
		static constexpr uint32_t infinite_lifespan = UINT_MAX;

		timed_interaction(dpp::cluster* owner, const std::string& token, dpp::snowflake guild, dpp::snowflake channel, dpp::snowflake message, uint32_t lifespan_seconds, on_timeout_fn on_timeout);

		timed_interaction(dpp::cluster* owner, const dpp::interaction& interaction, uint32_t lifespan_seconds, on_timeout_fn on_timeout)
			: timed_interaction(owner, interaction.token, interaction.guild_id, interaction.channel_id, interaction.message_id, lifespan_seconds, on_timeout) {}

		timed_interaction(const dpp::interaction_create_t& interaction_create, uint32_t lifespan_seconds, on_timeout_fn on_timeout)
			: timed_interaction(interaction_create.owner, interaction_create.command, lifespan_seconds, on_timeout) {}

		~timed_interaction();

		// Force a timeout, and optionally skip calling the on_timeout callback
		void force_timeout(bool skip_callback);

		// Returns whether this interaction has timed out and is no longer valid
		// Consider deleting timed-out interactions (but NOT in on_timeout_fn)
		// To avoid TOCTOU, if using get/edit/remove/followup, manually catch timed_out_error instead
		bool timed_out() const;

		// Is this timed interaction from a guild-installed bot
		// Returns false if it's from a user-installed bot instead
		auto is_guild_interaction() const { return guild_ && channel_ && message_; }

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
		// Consider calling force_timeout(false) afterwards
		void remove(dpp::command_completion_event_t callback = dpp::utility::log_error()) const;

		// Remove (delete) the message of this timed interaction
		// Returns dpp::confirmation as callback value unless errored
		// Consider calling force_timeout(false) afterwards
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
	/*
	class timed_menu
	{
		uint64_t id_ = std::time(nullptr);

		std::vector<std::string> pages_;
		int active_page_ = 0;

		std::optional<timed_interaction> interaction_ = std::nullopt;

		dpp::message get_active_page(bool expired = false);

		inline void on_timeout(const timed_interaction& response) { response.edit(get_active_page(true)); }
	public:
		timed_menu(const std::string& content, const std::string& primary_separator, const std::initializer_list<std::string>& secondary_separators = { "\n\n", "\n", " " });

		dpp::message create(dpp::cluster* owner, const std::string& token);
		dpp::message create(dpp::cluster* owner, const dpp::interaction& interaction);
		dpp::message create(const dpp::interaction_create_t& interaction_create);
	};*/
}