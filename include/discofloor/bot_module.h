#pragma once

#include <discofloor/bot.h>
#include <discofloor/bot_command.h>

#include <bulbtils/named_node.h>

namespace discofloor
{
	// Base module interface - inherit this class to create your own custom module
	//
	// Modules are, by design, meant to be single-instance ("static"), but they don't necessarily have to be
	// For dynamically allocated ("dynamic") modules, you can use bot::add_module() and bot::remove_module()
	// 
	// When a module ("static" or "dynamic") is being instantiated, it gets added to the internal linked list of modules
	// This linked list is iterated for each discofloor bot as described in the virtual functions below
	// Since "dynamic" modules are also added to the linked list, be careful when instantiating them
	struct bot_module : public bulbtils::named_node<bot_module>
	{
		inline bot_module(const char* name) : named_node<bot_module>(name) {}
		inline virtual ~bot_module() {}

		// This function is called when a discofloor bot is being constructed
		// Alternatively, it is also called when the module is being late-loaded using bot::add_module()
		// Modules are always initialized in alphabetical order, based on their name
		//
		// Return true to let the bot load your module, false otherwise (eg. if something goes wrong)
		// If you're returning false, it is advised to use "bot.log(...)" to provide an explanation
		//
		// By default, this virtual function just returns true and doesn't do anything else
		// If you don't need to run any initialization code, you don't need to override it
		// 
		// NOTE: Modules can additionally be disabled using the "disabled_modules" bot setting
		// In that case, this function will NOT be called, and the module will NOT be initialized
		//
		// NOTE: Since this is called during the module internal linked list iteration,
		// ...do NOT create new (dynamic) modules within this function
		inline virtual bool init(bot& bot) { return true; }

		// This function is called when a discofloor bot is requesting all loaded modules' commands
		// This usually happens a short while after init() - during the bot's initial on_ready event
		// For late-loaded commands using bot::add_module(), this function is called right after init()
		inline virtual std::vector<bot_command> commands(bot& bot) { return {}; }

		// This function is called when a discofloor bot created all loaded modules' commands
		// Use this to conveniently get the mention of your module's commands
		inline virtual void commands_created(bot& bot) {}

		// This function is called when a discofloor bot is being destructed
		// Alternatively, it is also called when the module is being early-unloaded using bot::remove_module()
		// Modules are always destroyed in reverse-alphabetical order, based on their name
		//
		// By default, this virtual function doesn't do anything
		// If you don't need to run any destruction code, you don't need to override it
		inline virtual void destroy(bot& bot) {}
	};
}