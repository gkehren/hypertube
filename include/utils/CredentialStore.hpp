#pragma once

#include "Result.hpp"
#include <functional>
#include <string>
#include <vector>

namespace Utils::CredentialStore
{
	enum class CredentialStatus
	{
		Stored,
		Missing,
		Unavailable,
		PermissionDenied
	};

	struct CredentialLoadResult
	{
		CredentialStatus status = CredentialStatus::Missing;
		std::string secret;

		bool hasSecret() const { return status == CredentialStatus::Stored && !secret.empty(); }
	};

	Result store(const std::string &account, const std::string &secret);
	Result erase(const std::string &account);
	CredentialLoadResult load(const std::string &account);
	bool hasStoredCredential(const std::string &account);
	CredentialStatus cachedStatus(const std::string &account);
	void asyncRefreshStatus(const std::vector<std::string> &accounts, std::function<void()> onComplete = {});
	void shutdown();
}
