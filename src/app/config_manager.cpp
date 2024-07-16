#include "config_manager.hpp"
#include <fstream>

void	ConfigManager::load(const std::string& path)
{
	std::ifstream	file(path);
	if (file.is_open())
	{
		file >> this->config;
		file.close();
	}
}

void	ConfigManager::save(const std::string& path)
{
	std::ofstream	file(path);
	if (file.is_open())
	{
		file << config.dump(4);
		file.close();
	}
}

json&	ConfigManager::getConfig()
{
	return this->config;
}