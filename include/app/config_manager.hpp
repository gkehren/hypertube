#pragma once

#include <string>
#include <json.hpp>

using json = nlohmann::json;

class ConfigManager
{
	public:
		void	load(const std::string& path);
		void	save(const std::string& path);

		json&	getConfig();
	
	private:
		json	config;
};