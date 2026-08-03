#pragma once

#include "Result.hpp"
#include <optional>
#include <string>

namespace Utils::CredentialStore
{
	Result store(const std::string &account, const std::string &secret);
	Result erase(const std::string &account);
	std::optional<std::string> load(const std::string &account);
}
