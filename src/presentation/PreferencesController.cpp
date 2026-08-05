#include "presentation/PreferencesController.hpp"

#include "AppPaths.hpp"
#include "CredentialStore.hpp"
#include "Logger.hpp"
#include "SearchEngine.hpp"
#include "TorrentManager.hpp"

#include <chrono>

namespace Presentation
{
PreferencesController::PreferencesController(TorrentManager &torrentManager, SearchEngine &searchEngine,
	ConfigManager &configManager, ThemeCallback themeCallback)
	: torrentManager(torrentManager), searchEngine(searchEngine), configManager(configManager),
	  themeCallback(std::move(themeCallback))
{
}

PreferencesController::~PreferencesController()
{
	if (pendingSave_)
		waitForSave();
}

PreferencesSettings PreferencesController::current() const
{
	return configManager.getPreferencesSettings();
}

Result PreferencesController::beginSave(const PreferencesSettings &settings,
	std::optional<std::string> torznabApiKey, std::optional<std::string> proxyPassword)
{
	if (pendingSave_)
		return Result::Failure("Preferences are already being saved", ResultCode::Busy, true);

	const std::string proxyType = settings.proxyType.empty() ? "socks5" : settings.proxyType;
	const Result proxyValidation = SearchEngine::validateProxyConfig(settings.proxyEnabled, proxyType,
		settings.proxyHost, settings.proxyPort);
	if (!proxyValidation)
		return proxyValidation;
	if (settings.torznabEnabled)
	{
		const Result providerValidation = SearchEngine::validateTorznabConfig(settings.torznabUrl);
		if (!providerValidation)
			return providerValidation;
	}

	previousPreferences_ = configManager.getPreferencesSettings();
	previousTorznabCredential_ = Utils::CredentialStore::load("torznab_api_key");
	previousProxyCredential_ = Utils::CredentialStore::load("proxy_password");
	torznabCredentialChanged_ = false;
	proxyCredentialChanged_ = false;

	const std::string torznabSecret = torznabApiKey.value_or(previousTorznabCredential_.value_or(""));
	const Result torznab = !settings.torznabEnabled || torznabSecret.empty()
		? Utils::CredentialStore::erase("torznab_api_key")
		: Utils::CredentialStore::store("torznab_api_key", torznabSecret);
	if (!torznab)
		return torznab;
	torznabCredentialChanged_ = true;

	const std::string proxySecret = proxyPassword.value_or(previousProxyCredential_.value_or(""));
	const Result proxy = !settings.proxyEnabled || proxySecret.empty()
		? Utils::CredentialStore::erase("proxy_password")
		: Utils::CredentialStore::store("proxy_password", proxySecret);
	if (!proxy)
	{
		restoreCredentials();
		return proxy;
	}
	proxyCredentialChanged_ = settings.proxyEnabled && !proxySecret.empty();

	pendingPreferences_ = settings;
	pendingSave_ = configManager.savePreferencesCandidate(Utils::AppPaths::settingsConfigPath().string(), settings);
	return Result::Success();
}

std::optional<Result> PreferencesController::pollSave()
{
	if (!pendingSave_ || pendingSave_->wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return std::nullopt;
	const Result saveResult = pendingSave_->get();
	pendingSave_.reset();
	return finishSave(saveResult);
}

Result PreferencesController::waitForSave()
{
	if (!pendingSave_)
		return Result::Success();
	const Result saveResult = pendingSave_->get();
	pendingSave_.reset();
	return finishSave(saveResult);
}

Result PreferencesController::finishSave(const Result &saveResult)
{
	if (!saveResult)
	{
		const Result restore = restoreCredentials();
		return restore ? saveResult : Result::Failure(saveResult.message + "; credentials could not be fully restored: " + restore.message, ResultCode::Partial);
	}

	const Result commit = configManager.commitPreferences(pendingPreferences_);
	const Result runtime = commit ? applyRuntime(pendingPreferences_) : commit;
	if (runtime)
	{
		torznabCredentialChanged_ = false;
		proxyCredentialChanged_ = false;
		return Result::Success();
	}

	const Result credentialRestore = restoreCredentials();
	configManager.commitPreferences(previousPreferences_);
	const Result runtimeRestore = applyRuntime(previousPreferences_);
	configManager.savePreferencesCandidate(Utils::AppPaths::settingsConfigPath().string(), previousPreferences_);
	if (!credentialRestore || !runtimeRestore)
		return Result::Failure("Preferences failed and runtime restoration was incomplete: " + runtime.message, ResultCode::Partial);
	return Result::Failure("Preferences were rolled back: " + runtime.message, runtime.code);
}

Result PreferencesController::restoreCredentials()
{
	Result firstFailure = Result::Success();
	if (torznabCredentialChanged_)
	{
		const Result result = previousTorznabCredential_
			? Utils::CredentialStore::store("torznab_api_key", *previousTorznabCredential_)
			: Utils::CredentialStore::erase("torznab_api_key");
		if (!result)
		{
			Utils::Logger::error("config", "Unable to restore Torznab credential: " + result.message);
			firstFailure = Result::Failure(result.message, ResultCode::Partial);
		}
	}
	if (proxyCredentialChanged_)
	{
		const Result result = previousProxyCredential_
			? Utils::CredentialStore::store("proxy_password", *previousProxyCredential_)
			: Utils::CredentialStore::erase("proxy_password");
		if (!result && firstFailure)
			firstFailure = Result::Failure(result.message, ResultCode::Partial);
	}
	torznabCredentialChanged_ = false;
	proxyCredentialChanged_ = false;
	return firstFailure;
}

Result PreferencesController::applyRuntime(const PreferencesSettings &settings)
{
	if (themeCallback)
		themeCallback(settings.theme);
	torrentManager.setDownloadSpeedLimit(settings.downloadSpeedLimit);
	torrentManager.setUploadSpeedLimit(settings.uploadSpeedLimit);
	torrentManager.configureDiscovery(settings.enableDht, settings.enableUpnp, settings.enableNatPmp);

	const std::string proxyPassword = Utils::CredentialStore::load("proxy_password").value_or("");
	torrentManager.setProxyConfig(settings.proxyHost, settings.proxyPort, settings.proxyUsername, proxyPassword,
		settings.proxyEnabled ? (settings.proxyType == "http" ? 2 : 1) : 0);
	const Result searchProxy = searchEngine.setProxyConfig(settings.proxyEnabled, settings.proxyType,
		settings.proxyHost, settings.proxyPort, settings.proxyUsername, proxyPassword);
	if (!searchProxy)
		return searchProxy;

	if (settings.torznabEnabled)
	{
		const std::string apiKey = Utils::CredentialStore::load("torznab_api_key").value_or("");
		const Result provider = searchEngine.configureTorznabProvider(settings.torznabUrl, apiKey);
		if (!provider)
			return provider;
		return searchEngine.setActiveSearchProvider("torznab");
	}
	return searchEngine.setActiveSearchProvider("torrents-csv");
}
} // namespace Presentation
