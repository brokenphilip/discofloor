#pragma once

#include <bulbtils/file.h>

#include <dpp/json.hpp>

#include <format>
#include <filesystem>

namespace discofloor
{
	// An extension of the base file structure, which saves compact json data to a file
	// If you're looking for something more user-friendly (eg. for configuration files), use the json_pretty_print
	// Or, you may also supply your own indentation count and indentation character in the template parameters
	// Additionally, you may override 'save_from_struct'/'load_to_struct' if necessary, but this struct already does it for you
	template <int indent = -1, char indent_char = ' '>
	struct json_file : public bulbtils::file::base
	{
		// Use this function to convert your data structure to a json, which will be written to a file
		virtual nlohmann::json struct_to_json(const bulbtils::file::settings& save_settings) const = 0;

		// Use this function to convert the string data loaded from a file to your data structure.
		// Return false to pass 's_parse_error' to the load() function
		// (if you're okay with partial loads, particularly if you make use of default values, return true)
		virtual bool json_to_struct(const nlohmann::json& data, const bulbtils::file::settings& load_settings) = 0;

		//
		virtual std::string save_from_struct(const bulbtils::file::settings& save_settings) const override //final
		{
			return struct_to_json(save_settings).dump(indent, indent_char);
		}

		virtual bool load_to_struct(const std::string& data, const bulbtils::file::settings& load_settings) override //final
		{
			try
			{
				return json_to_struct(nlohmann::json::parse(data), load_settings);
			}
			catch (const std::exception& e)
			{
				if (load_settings.error_callback)
				{
					load_settings.error_callback(std::format("Exception parsing json file: {}", e.what()));
				}
				return false;
			}
		}
	};

	// An extension of the base file structure, which saves pretty-printed json data to a file
	// If you're looking for something more compact, use 
	using pretty_print_json_file = json<4, ' '>;

	// Helper wrapper function for json.at() with exception handling and logging output
	// Setting not_found_warning to true will gracefully handle missing keys (instead of an exception)
	// Returns true if json.at() was successful, false otherwise
	template <typename T>
	bool json_try_at(const nlohmann::json& data, const bulbtils::file::settings& load_settings, 
		const std::string& key, T& member_variable, bool not_found_warning = false)
	{
		if (not_found_warning && !data.contains(key))
		{
			if (load_settings.warning_callback)
			{
				load_settings.warning_callback(std::format("'{}' json key not found, using default value instead", key));
			}
			return false;
		}
    
		try
		{
			member_variable = data.at(key);
			return true;
		}
		catch (const std::exception& e)
		{
			if (load_settings.error_callback)
			{
				load_settings.error_callback(std::format("Exception reading '{}' json key: {}", key, e.what()));
			}
			return false;
		}
	}
}