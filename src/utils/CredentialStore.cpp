#include "CredentialStore.hpp"
#include <cstring>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#elif defined(__APPLE__)
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <spawn.h>
#include <cerrno>
extern char **environ;
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
	CFStringRef serviceCF = CFStringCreateWithCString(nullptr, serviceName, kCFStringEncodingUTF8);
	CFStringRef accountCF = CFStringCreateWithCString(nullptr, account.c_str(), kCFStringEncodingUTF8);
	CFDataRef secretCF = CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(secret.data()), secret.size());

	const void *keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
	const void *values[] = { kSecClassGenericPassword, serviceCF, accountCF };
	CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	const void *updateKeys[] = { kSecValueData };
	const void *updateValues[] = { secretCF };
	CFDictionaryRef updateDict = CFDictionaryCreate(nullptr, updateKeys, updateValues, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	OSStatus status = SecItemUpdate(query, updateDict);
	if (status == errSecItemNotFound)
	{
		const void *addKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData };
		const void *addValues[] = { kSecClassGenericPassword, serviceCF, accountCF, secretCF };
		CFDictionaryRef addDict = CFDictionaryCreate(nullptr, addKeys, addValues, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		status = SecItemAdd(addDict, nullptr);
		if (addDict) CFRelease(addDict);
	}
	if (query) CFRelease(query);
	if (updateDict) CFRelease(updateDict);
	if (serviceCF) CFRelease(serviceCF);
	if (accountCF) CFRelease(accountCF);
	if (secretCF) CFRelease(secretCF);

	return status == errSecSuccess ? Result::Success()
		: Result::Failure("macOS Keychain rejected the credential", ResultCode::Storage);
#else
	int pipes[2] = {-1, -1};
	if (pipe(pipes) != 0)
		return Result::Failure("Unable to open Secret Service input channel", ResultCode::Storage);

	posix_spawn_file_actions_t actions;
	if (posix_spawn_file_actions_init(&actions) != 0) {
		close(pipes[0]);
		close(pipes[1]);
		return Result::Failure("Unable to initialize spawn actions", ResultCode::Storage);
	}
	posix_spawn_file_actions_adddup2(&actions, pipes[0], STDIN_FILENO);
	posix_spawn_file_actions_addclose(&actions, pipes[1]);

	char *const argv[] = {
		const_cast<char *>("secret-tool"),
		const_cast<char *>("store"),
		const_cast<char *>("--label=Hypertube credential"),
		const_cast<char *>("service"),
		const_cast<char *>(serviceName),
		const_cast<char *>("account"),
		const_cast<char *>(account.c_str()),
		nullptr
	};

	pid_t pid = -1;
	int spawnErr = posix_spawnp(&pid, "secret-tool", &actions, nullptr, argv, environ);
	posix_spawn_file_actions_destroy(&actions);
	close(pipes[0]);

	if (spawnErr != 0) {
		close(pipes[1]);
		return Result::Failure("Secret Service is unavailable; secret-tool not found", ResultCode::Unavailable);
	}

	const std::string input = secret + "\n";
	const char *ptr = input.data();
	std::size_t remaining = input.size();
	while (remaining > 0) {
		ssize_t written = write(pipes[1], ptr, remaining);
		if (written <= 0)
			break;
		ptr += written;
		remaining -= static_cast<std::size_t>(written);
	}
	close(pipes[1]);

	int status = 0;
	waitpid(pid, &status, 0);
	return (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		? Result::Success()
		: Result::Failure("Secret Service rejected credential or keyring is locked", ResultCode::Unavailable);
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
	CFStringRef serviceCF = CFStringCreateWithCString(nullptr, serviceName, kCFStringEncodingUTF8);
	CFStringRef accountCF = CFStringCreateWithCString(nullptr, account.c_str(), kCFStringEncodingUTF8);

	const void *keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
	const void *values[] = { kSecClassGenericPassword, serviceCF, accountCF };
	CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	OSStatus status = SecItemDelete(query);
	if (query) CFRelease(query);
	if (serviceCF) CFRelease(serviceCF);
	if (accountCF) CFRelease(accountCF);

	return (status == errSecSuccess || status == errSecItemNotFound) ? Result::Success()
		: Result::Failure("macOS Keychain could not delete the credential", ResultCode::Storage);
#else
	char *const argv[] = {
		const_cast<char *>("secret-tool"),
		const_cast<char *>("clear"),
		const_cast<char *>("service"),
		const_cast<char *>(serviceName),
		const_cast<char *>("account"),
		const_cast<char *>(account.c_str()),
		nullptr
	};

	pid_t pid = -1;
	int spawnErr = posix_spawnp(&pid, "secret-tool", nullptr, nullptr, argv, environ);
	if (spawnErr != 0)
		return Result::Failure("Unable to start Secret Service client", ResultCode::Unavailable);

	int status = 0;
	waitpid(pid, &status, 0);
	return (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		? Result::Success()
		: Result::Failure("Secret Service could not delete the credential", ResultCode::Unavailable);
#endif
}

CredentialLoadResult load(const std::string &account)
{
	if (account.empty())
		return {CredentialStatus::Missing, ""};
#ifdef _WIN32
	const std::string target = std::string(serviceName) + "/" + account;
	PCREDENTIALA credential = nullptr;
	if (!CredReadA(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
		const DWORD err = GetLastError();
		if (err == ERROR_NOT_FOUND)
			return {CredentialStatus::Missing, ""};
		if (err == ERROR_ACCESS_DENIED)
			return {CredentialStatus::PermissionDenied, ""};
		return {CredentialStatus::Unavailable, ""};
	}
	std::string secret(reinterpret_cast<char *>(credential->CredentialBlob), credential->CredentialBlobSize);
	CredFree(credential);
	return {CredentialStatus::Stored, secret};
#elif defined(__APPLE__)
	CFStringRef serviceCF = CFStringCreateWithCString(nullptr, serviceName, kCFStringEncodingUTF8);
	CFStringRef accountCF = CFStringCreateWithCString(nullptr, account.c_str(), kCFStringEncodingUTF8);

	const void *keys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData, kSecMatchLimit };
	const void *values[] = { kSecClassGenericPassword, serviceCF, accountCF, kCFBooleanTrue, kSecMatchLimitOne };
	CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, values, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	CFTypeRef dataTypeRef = nullptr;
	OSStatus status = SecItemCopyMatching(query, &dataTypeRef);
	if (query) CFRelease(query);
	if (serviceCF) CFRelease(serviceCF);
	if (accountCF) CFRelease(accountCF);

	if (status == errSecItemNotFound)
		return {CredentialStatus::Missing, ""};
	if (status == errSecAuthFailed || status == errSecInteractionNotAllowed)
		return {CredentialStatus::PermissionDenied, ""};
	if (status != errSecSuccess || !dataTypeRef)
		return {CredentialStatus::Unavailable, ""};

	CFDataRef dataRef = static_cast<CFDataRef>(dataTypeRef);
	std::string secret(reinterpret_cast<const char *>(CFDataGetBytePtr(dataRef)), CFDataGetLength(dataRef));
	CFRelease(dataRef);
	return {CredentialStatus::Stored, secret};
#else
	int pipes[2] = {-1, -1};
	if (pipe(pipes) != 0)
		return {CredentialStatus::Unavailable, ""};

	posix_spawn_file_actions_t actions;
	if (posix_spawn_file_actions_init(&actions) != 0) {
		close(pipes[0]);
		close(pipes[1]);
		return {CredentialStatus::Unavailable, ""};
	}
	posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
	posix_spawn_file_actions_addclose(&actions, pipes[0]);

	char *const argv[] = {
		const_cast<char *>("secret-tool"),
		const_cast<char *>("lookup"),
		const_cast<char *>("service"),
		const_cast<char *>(serviceName),
		const_cast<char *>("account"),
		const_cast<char *>(account.c_str()),
		nullptr
	};

	pid_t pid = -1;
	int spawnErr = posix_spawnp(&pid, "secret-tool", &actions, nullptr, argv, environ);
	posix_spawn_file_actions_destroy(&actions);
	close(pipes[1]);

	if (spawnErr != 0) {
		close(pipes[0]);
		return {CredentialStatus::Unavailable, ""};
	}

	std::string secret;
	char buffer[1024];
	ssize_t bytesRead = 0;
	while ((bytesRead = read(pipes[0], buffer, sizeof(buffer))) > 0) {
		secret.append(buffer, static_cast<std::size_t>(bytesRead));
	}
	close(pipes[0]);

	int status = 0;
	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return {CredentialStatus::Missing, ""};

	while (!secret.empty() && (secret.back() == '\n' || secret.back() == '\r'))
		secret.pop_back();

	return secret.empty() ? CredentialLoadResult{CredentialStatus::Missing, ""}
						  : CredentialLoadResult{CredentialStatus::Stored, std::move(secret)};
#endif
}

bool hasStoredCredential(const std::string &account)
{
	return load(account).status == CredentialStatus::Stored;
}

}
