#pragma once

#include "Result.hpp"
#include <string>

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
}
