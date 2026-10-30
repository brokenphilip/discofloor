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
	// When creating or updating this object, make sure to do it as soon as the interaction is created
	class timed_interaction
	{
		mutable std::shared_mutex mutex_;

		dpp::cluster* owner_;
		std::string token_;

		dpp::snowflake guild_;
		dpp::snowflake channel_;
		dpp::snowflake message_;
		bool guild_interaction_;

		on_timeout_fn on_timeout_;
		std::atomic_bool timing_out_ = false;

		uint32_t lifespan_seconds_;

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
		void enable_timer();
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

		// Construct a timed interaction using the standard lifespan (in seconds) and an on_timeout callback function
		// This function accepts manually supplying interaction ID, owning cluster and token, as well as guild, channel and message IDs
		timed_interaction(dpp::cluster* owner, const std::string& token, dpp::snowflake guild, dpp::snowflake channel, dpp::snowflake message, bool is_guild_interaction, uint32_t lifespan_seconds, on_timeout_fn on_timeout);

		// Construct a timed interaction using the standard lifespan (in seconds) and an on_timeout callback function
		// This function accepts the owning cluster as well as interaction object
		timed_interaction(dpp::cluster* owner, const dpp::interaction& interaction, uint32_t lifespan_seconds, on_timeout_fn on_timeout)
			: timed_interaction(owner, interaction.token, interaction.guild_id, interaction.channel_id, interaction.message_id,
				interaction.is_guild_interaction(), lifespan_seconds, on_timeout) {}

		// Construct a timed interaction using the standard lifespan (in seconds) and an on_timeout callback function
		// This function accepts an interaction_create event
		timed_interaction(const dpp::interaction_create_t& interaction_create, uint32_t lifespan_seconds, on_timeout_fn on_timeout)
			: timed_interaction(interaction_create.owner, interaction_create.command, lifespan_seconds, on_timeout) {}

		~timed_interaction();

		// Update a timed interaction with new interaction data, resetting the timer (and reviving the timed interaction if it timed out already)
		// You may also optionally supply a new lifespan or timeout callback - blank these values out to use the already existing ones
		void update(const std::string& token, dpp::snowflake guild, dpp::snowflake channel, dpp::snowflake message,
			uint32_t lifespan_seconds = 0, on_timeout_fn on_timeout = {});

		// Update a timed interaction with new interaction data (from an interaction), resetting the timer (and reviving the timed interaction if it timed out already)
		// You may also optionally supply a new lifespan or timeout callback - blank these values out to use the already existing ones
		void update(const dpp::interaction& interaction, uint32_t lifespan_seconds = 0, on_timeout_fn on_timeout = {})
			{ update(interaction.token, interaction.guild_id, interaction.channel_id, interaction.message_id, lifespan_seconds, on_timeout); }

		// Update a timed interaction with new interaction data (from an interaction creation event), resetting the timer (and reviving the timed interaction if it timed out already)
		// You may also optionally supply a new lifespan or timeout callback - blank these values out to use the already existing ones
		void update(const dpp::interaction_create_t& interaction_create, uint32_t lifespan_seconds = 0, on_timeout_fn on_timeout = {})
			{ update(interaction_create.command.token, interaction_create.command.guild_id, interaction_create.command.channel_id, interaction_create.command.message_id, lifespan_seconds, on_timeout); }

		// Force a timeout, and optionally skip calling the on_timeout callback
		void force_timeout(bool skip_callback);

		// Returns whether this interaction has timed out and is no longer valid
		// Consider deleting timed-out interactions (but NOT in on_timeout_fn)
		// To avoid TOCTOU, if using get/edit/remove/followup, manually catch timed_out_error instead
		bool timed_out() const;

		// Is this timed interaction from a guild-installed bot
		// Returns false if it's from a user-installed bot instead
		auto is_guild_interaction() const { return guild_ && channel_ && message_ && guild_interaction_; }

		// Get the owning cluster of this timed interaction
		inline auto owner() const { return owner_; }

		// Get the lifespan of this timed interaction, in seconds
		inline auto lifespan_seconds() const { return lifespan_seconds_; }

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

	class timed_book
	{
	public:
		enum timeout_method : uint8_t
		{
			// on timeout, disable page control buttons
			tm_disable_buttons,

			// on timeout, delete the entire message
			tm_delete_message,

			// not a valid choice - only used to check input
			tm_count,
		};

		// Callback function for modifying this timed book
		// Use the std::string reference to modify the title
		// Use the vector of component vectors to modify the contents
		// Return the new page number - make sure it is valid
		// If changing the active page is undesirable at this momennt, directly pass current_page() instead
		using modify_fn = std::function<int(std::string&, std::vector<std::vector<dpp::component>>&)>;
	private:
		// next available timed book id
		// cannot use 0 because the bot will start overwriting books upon restart
		// cannot use std::time(nullptr) as id directly in case 2 books are created at once
		static inline uint64_t next_id_ = std::time(nullptr);
		uint64_t id_;
		
		std::string title_;
		std::vector<std::vector<dpp::component>> contents_;

		timeout_method timeout_method_;

		dpp::snowflake user_;

		time_t last_update_ = std::time(nullptr);
		timed_interaction timed_interaction_;

		std::string generate_first_page_custom_id() { return "f_" + std::to_string(id_); }
		std::string generate_prev_page_custom_id() { return "p_" + std::to_string(id_); }
		std::string generate_jump_page_custom_id() { return "j_" + std::to_string(id_); }
		std::string generate_next_page_custom_id() { return "n_" + std::to_string(id_); }
		std::string generate_last_page_custom_id() { return "l_" + std::to_string(id_); }

		int active_page_index_;
		dpp::message get_page(int index, bool buttons_enabled = true);

		dpp::event_handle button_click_handle = SIZE_MAX;
		void button_click_event(const dpp::button_click_t& event);

		dpp::event_handle form_submit_handle = SIZE_MAX;
		void form_submit_event(const dpp::form_submit_t& event);

		void on_timeout();

		int modify_internal(modify_fn callback);

		static void validate_contents(const std::vector<std::vector<dpp::component>>& contents);
		static void validate_title(const std::string& title);
	public:
		// This function converts a vector of strings into a vector of component vectors - essentially, each string will be its own page
		static std::vector<std::vector<dpp::component>> make_component_vectors_from_string_contents_vector(const std::vector<std::string> contents);

		// Recommended main_separator for split_contents()
		static inline const std::string unit_separator = "\x1F";

		// Construct a timed book using the standard title, timeout method, owning user, initial page and lifespan (in seconds)
		// This function accepts a vector of component vectors for contents - each component vector will be its own page
		// This function accepts manually supplying interaction ID, owning cluster and token, as well as guild, channel and message IDs
		timed_book(const std::string& title, const std::vector<std::vector<dpp::component>>& contents, timeout_method tm, dpp::snowflake user, size_t initial_page,
			dpp::snowflake interaction_id, dpp::cluster* owner, const std::string& token, dpp::snowflake guild, dpp::snowflake channel, dpp::snowflake message,
			bool is_guild_interaction, uint32_t lifespan_seconds);

		// Construct a timed book using the standard title, timeout method, owning user, initial page and lifespan (in seconds)
		// This function accepts a vector of component vectors for contents - each component vector will be its own page
		// This function accepts the owning cluster as well as interaction object
		timed_book(const std::string& title, const std::vector<std::vector<dpp::component>>& contents, timeout_method tm, dpp::snowflake user, size_t initial_page,
			dpp::cluster* owner, const dpp::interaction& interaction, uint32_t lifespan_seconds)
			: timed_book(title, contents, tm, user, initial_page, interaction.id, owner, interaction.token,
				interaction.guild_id, interaction.channel_id, interaction.message_id, interaction.is_guild_interaction(), lifespan_seconds) {}

		// Construct a timed book using the standard title, timeout method, owning user, initial page and lifespan (in seconds)
		// This function accepts a vector of component vectors for contents - each component vector will be its own page
		// This function accepts an interaction_create event
		timed_book(const std::string& title, const std::vector<std::vector<dpp::component>>& contents, timeout_method tm, dpp::snowflake user, size_t initial_page,
			const dpp::interaction_create_t& interaction_create, uint32_t lifespan_seconds)
			: timed_book(title, contents, tm, user, initial_page, interaction_create.owner, interaction_create.command, lifespan_seconds) {}

		~timed_book();

		// Modify the title and contents of this timed book using the provided callback
		// Will throw timed_out_error if the timed book has timed out
		// Will throw invalid_argument errors for ANY incorrect input, and NO content will be updated even if correct
		void modify(modify_fn callback);

		// How many pages does this timed book have?
		// This is also the last valid page
		inline auto page_count() { return contents_.size(); }

		// What page is this timed book currently on?
		// Pass this value as "page_number" on any functions below to essentially refresh the book
		// You can also use this value to do manual relative calculations based on the current page
		inline auto current_page() { return active_page_index_ + 1; }

		// Is the provided page number valid for this timed book?
		inline auto valid_page_number(int page_number) { return page_number > 0 && page_number < page_count() + 1; }

		// Set the active page of this timed book to the number passed in this function
		// Will throw timed_out_error if the timed book has timed out
		// Will throw invalid_argument if the page number is out of bounds
		void set_page(int page_number);

		// Update this book's timed interaction using a fresh interaction event, essentially reviving it if timed out
		// Upon updating the underlying timed interaction, this function also sets the new active page
		// If changing the active page is undesirable at this momennt, directly pass current_page() instead
		// Will throw invalid_argument if the page number is out of bounds
		void update_timed_interaction(const dpp::interaction_create_t& event, int page_number);

		// Update this book's timed interaction using a fresh interaction event, essentially reviving it if timed out
		// Upon updating the underlying timed interaction, this function also modifies the pages
		// If changing the active page is undesirable at this momennt, directly pass current_page() instead
		// Will throw invalid_argument if the page number is out of bounds
		void update_timed_interaction(const dpp::interaction_create_t& event, modify_fn callback);

		// Once you've confirmed that your button_click event belongs to this timed_book, you MUST call this function
		// If it returns true, early return from your button_click event IMMEDIATELY
		// This function handles checking for timed_menu ownership and timeout for you, replies included
		bool should_early_return_from_button_click(const dpp::button_click_t& event);

		// Returns the ID of this timed book. Useful for button IDs
		inline auto id() { return id_; }

		// Force this timed book to timeout, using the method specified on construction
		inline auto force_timeout() { timed_interaction_.force_timeout(false); }

		// Has this book timed out?
		inline auto timed_out() const { return timed_interaction_.timed_out(); }

		// Get the owning cluster of this timed book. Use this to register additional button events
		inline auto owner() const { return timed_interaction_.owner(); }

		// Get the lifespan of this timed book, in seconds
		inline auto lifespan_seconds() const { return timed_interaction_.lifespan_seconds(); }

		// Given a large string of "contents", this function splits the string into multiple smaller content strings - pages - and returns a vector of them
		// This utility function is useful when you have a large string that you want to easily split into pages to pass to a timed_book's constructor
		//
		// If "main_separator" is provided (ie. is not empty), all instances of the string will be removed from the large "contents" string
		// The large "contents" string will be split in exactly all the positions where the "main_separator" was found, with the following exceptions:
		// 1. If any of the resulting pages are empty (eg. two separators in a row, or a separator at the beginning/end), they will be excluded
		// 2. If any of the resulting pages are over "max_length" characters long, they will be further split using backup separators as explained below
		// 
		// Note that if any of the resulting pages are under "min_length" characters long, they will NOT be joined together
		// 
		// If "main_separator" is not provided (is empty), or the aforementioned exception #2 occurs, every time a split is about to be performed (near "max_length")...
		// ...the function will search for the first "ideal" backup separator from the initializer list - or, in other words, it won't necessarily just use the same one
		// 
		// A backup separator is "ideal" when it can produce pages between "min_length" and "max_length" characters long
		// If no backup separator is ideal, it picks the first one that can produce a page below "max_length" characters in length
		// 
		// If no backup separator can produce a page below "max_length" characters in length, the string gets cut at "max_length"...
		// ...while also respecting the "min_length" of any potential leftover page
		// 
		// For best results, supply:
		// - a "main_separator" whenever possible for full control over paging, but ensure it can't come from regular user input
		// - backup separators whenever possible, ideally using whitespace, to avoid awkward content string cutoffs
		// - sensible minimum and maximum lengths for best output results, while also not being blocked by the Discord API
		static std::vector<std::string> split_contents(const std::string& contents, const std::string& main_separator = "",
			const std::initializer_list<std::string>& backup_separators = { "\n\n", "\n", " " }, size_t min_length = 100, size_t max_length = 3'000);
	};
}