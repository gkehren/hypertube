#include "CredentialStore.hpp"
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#elif defined(__APPLE__)
#include <Security/Security.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#endif

namespace
{
constexpr const char *serviceName = "Hypertube";
}

namespace Utils::CredentialStore
{
Result store(const std::string &account, const std::string &secret)
{
	if (account.empty())
		return Result::Failure("Credential account cannot be empty", ResultCode::InvalidInput);
#ifdef _WIN32
	const std::string target = std::string(serviceName) + "/" + account;
	CREDENTIALA credential{};
	credential.Type = CRED_TYPE_GENERIC;
	credential.TargetName = const_cast<char *>(target.c_str());
	credential.CredentialBlobSize = static_cast<DWORD>(secret.size());
	credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(secret.data()));
	credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
	return CredWriteA(&credential, 0)
		? Result::Success()
		: Result::Failure("Windows Credential Manager rejected the credential", ResultCode::Storage);
#elif defined(__APPLE__)
	SecKeychainItemRef item = nullptr;
	UInt32 existingLength = 0;
	void *existingData = nullptr;
	OSStatus status = SecKeychainFindGenericPassword(nullptr, std::strlen(serviceName), serviceName,
		static_cast<UInt32>(account.size()), account.data(), &existingLength, &existingData, &item);
	if (existingData)
		SecKeychainItemFreeContent(nullptr, existingData);
	if (status == errSecSuccess)
	{
		status = SecKeychainItemModifyAttributesAndData(item, nullptr, static_cast<UInt32>(secret.size()), secret.data());
		CFRelease(item);
	}
	else
	{
		status = SecKeychainAddGenericPassword(nullptr, std::strlen(serviceName), serviceName,
			static_cast<UInt32>(account.size()), account.data(), static_cast<UInt32>(secret.size()), secret.data(), nullptr);
	}
	return status == errSecSuccess ? Result::Success()
		: Result::Failure("macOS Keychain rejected the credential", ResultCode::Storage);
#else
	int inputSocket[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, inputSocket) != 0)
		return Result::Failure("Unable to open Secret Service input channel", ResultCode::Storage);
	const pid_t child = fork();
	if (child == 0)
	{
		dup2(inputSocket[0], STDIN_FILENO);
		close(inputSocket[0]);
		close(inputSocket[1]);
		execlp("secret-tool", "secret-tool", "store", "--label=Hypertube credential",
			"service", serviceName, "account", account.c_str(), static_cast<char *>(nullptr));
		_exit(127);
	}
	close(inputSocket[0]);
	if (child < 0)
	{
		close(inputSocket[1]);
		return Result::Failure("Unable to start Secret Service client", ResultCode::Storage);
	}
	const std::string input = secret + "\n";
	std::size_t written = 0;
	while (written < input.size())
	{
		const ssize_t count = send(inputSocket[1], input.data() + written, input.size() - written, MSG_NOSIGNAL);
		if (count <= 0)
			break;
		written += static_cast<std::size_t>(count);
	}
	close(inputSocket[1]);
	int status = 0;
	waitpid(child, &status, 0);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0
		? Result::Success()
		: Result::Failure("Secret Service is unavailable; install secret-tool and unlock a keyring", ResultCode::Unavailable);
#endif
}

Result erase(const std::string &account)
{
	if (account.empty())
		return Result::Failure("Credential account cannot be empty", ResultCode::InvalidInput);
#ifdef _WIN32
	const std::string target = std::string(serviceName) + "/" + account;
	if (CredDeleteA(target.c_str(), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND)
		return Result::Success();
	return Result::Failure("Windows Credential Manager could not delete the credential", ResultCode::Storage);
#elif defined(__APPLE__)
	SecKeychainItemRef item = nullptr;
	OSStatus status = SecKeychainFindGenericPassword(nullptr, std::strlen(serviceName), serviceName,
		static_cast<UInt32>(account.size()), account.data(), nullptr, nullptr, &item);
	if (status == errSecItemNotFound)
		return Result::Success();
	if (status == errSecSuccess)
	{
		status = SecKeychainItemDelete(item);
		CFRelease(item);
	}
	return status == errSecSuccess ? Result::Success()
		: Result::Failure("macOS Keychain could not delete the credential", ResultCode::Storage);
#else
	if (!load(account))
		return Result::Success();
	const pid_t child = fork();
	if (child == 0)
	{
		execlp("secret-tool", "secret-tool", "clear", "service", serviceName,
			"account", account.c_str(), static_cast<char *>(nullptr));
		_exit(127);
	}
	if (child < 0)
		return Result::Failure("Unable to start Secret Service client", ResultCode::Storage);
	int status = 0;
	waitpid(child, &status, 0);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0
		? Result::Success()
		: Result::Failure("Secret Service could not delete the credential", ResultCode::Unavailable);
#endif
}

std::optional<std::string> load(const std::string &account)
{
	if (account.empty())
		return std::nullopt;
#ifdef _WIN32
	const std::string target = std::string(serviceName) + "/" + account;
	PCREDENTIALA credential = nullptr;
	if (!CredReadA(target.c_str(), CRED_TYPE_GENERIC, 0, &credential))
		return std::nullopt;
	std::string secret(reinterpret_cast<char *>(credential->CredentialBlob), credential->CredentialBlobSize);
	CredFree(credential);
	return secret;
#elif defined(__APPLE__)
	UInt32 length = 0;
	void *data = nullptr;
	OSStatus status = SecKeychainFindGenericPassword(nullptr, std::strlen(serviceName), serviceName,
		static_cast<UInt32>(account.size()), account.data(), &length, &data, nullptr);
	if (status != errSecSuccess)
		return std::nullopt;
	std::string secret(static_cast<char *>(data), length);
	SecKeychainItemFreeContent(nullptr, data);
	return secret;
#else
	int outputPipe[2];
	if (pipe(outputPipe) != 0)
		return std::nullopt;
	const pid_t child = fork();
	if (child == 0)
	{
		dup2(outputPipe[1], STDOUT_FILENO);
		close(outputPipe[0]);
		close(outputPipe[1]);
		execlp("secret-tool", "secret-tool", "lookup", "service", serviceName,
			"account", account.c_str(), static_cast<char *>(nullptr));
		_exit(127);
	}
	close(outputPipe[1]);
	if (child < 0)
	{
		close(outputPipe[0]);
		return std::nullopt;
	}
	std::string secret;
	std::array<char, 4096> buffer{};
	while (secret.size() < 64 * 1024)
	{
		const ssize_t count = read(outputPipe[0], buffer.data(), buffer.size());
		if (count <= 0)
			break;
		secret.append(buffer.data(), static_cast<std::size_t>(count));
	}
	close(outputPipe[0]);
	int status = 0;
	waitpid(child, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return std::nullopt;
	while (!secret.empty() && (secret.back() == '\n' || secret.back() == '\r'))
		secret.pop_back();
	return secret.empty() ? std::nullopt : std::optional<std::string>(std::move(secret));
#endif
}
}
