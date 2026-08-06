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
	ConfigManager &configManager, ThemeCallback themeCallback, CredentialStoreOps credentialStore,
	std::string settingsPath)
	: torrentManager(torrentManager), searchEngine(searchEngine), configManager(configManager),
	  themeCallback(std::move(themeCallback)), credentialStore(std::move(credentialStore)),
	  settingsPath_(std::move(settingsPath))
{
	if (!this->credentialStore.store)
		this->credentialStore.store = [](const std::string &account, const std::string &secret) {
			return Utils::CredentialStore::store(account, secret);
		};
	if (!this->credentialStore.erase)
		this->credentialStore.erase = [](const std::string &account) {
			return Utils::CredentialStore::erase(account);
		};
	if (!this->credentialStore.load)
		this->credentialStore.load = [](const std::string &account) {
			return Utils::CredentialStore::load(account);
		};
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

const std::string &PreferencesController::settingsPath() const
{
	if (!settingsPath_.empty())
		return settingsPath_;
	static const std::string defaultPath = Utils::AppPaths::settingsConfigPath().string();
	return defaultPath;
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
	previousTorznabCredential_ = credentialStore.load("torznab_api_key");
	previousProxyCredential_ = credentialStore.load("proxy_password");
	torznabCredentialChanged_ = false;
	proxyCredentialChanged_ = false;

	const std::string torznabSecret = torznabApiKey.value_or(previousTorznabCredential_.value_or(""));
	const Result torznab = !settings.torznabEnabled || torznabSecret.empty()
		? credentialStore.erase("torznab_api_key")
		: credentialStore.store("torznab_api_key", torznabSecret);
	if (!torznab)
		return torznab;
	torznabCredentialChanged_ = true;

	const std::string proxySecret = proxyPassword.value_or(previousProxyCredential_.value_or(""));
	const Result proxy = !settings.proxyEnabled || proxySecret.empty()
		? credentialStore.erase("proxy_password")
		: credentialStore.store("proxy_password", proxySecret);
	if (!proxy)
	{
		restoreCredentials();
		return proxy;
	}
	proxyCredentialChanged_ = true;

	pendingPreferences_ = settings;
	pendingSave_ = configManager.savePreferencesCandidate(settingsPath(), settings);
	pendingSaveKind_ = SaveKind::Preferences;
	return Result::Success();
}

Result PreferencesController::beginUiStateSave(const PreferencesSettings &settings)
{
	if (pendingSave_)
	{
		queuedUiState_ = settings;
		return Result::Success();
	}
	return beginUiStateSaveNow(settings);
}

Result PreferencesController::beginUiStateSaveNow(const PreferencesSettings &settings)
{
	if (pendingSave_)
		return Result::Failure("Preferences are already being saved", ResultCode::Busy, true);
	pendingPreferences_ = settings;
	previousPreferences_ = configManager.getPreferencesSettings();
	pendingSave_ = configManager.savePreferencesCandidate(settingsPath(), settings);
	pendingSaveKind_ = SaveKind::UiState;
	return Result::Success();
}

PreferencesSettings PreferencesController::mergeUiStateIntoCurrent(const PreferencesSettings &uiState) const
{
	auto merged = configManager.getPreferencesSettings();
	merged.theme = uiState.theme;
	merged.ui = uiState.ui;
	return merged;
}

std::optional<Result> PreferencesController::pollSave()
{
	if (!pendingSave_ || pendingSave_->wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return std::nullopt;
	const Result saveResult = pendingSave_->get();
	const SaveKind completedKind = pendingSaveKind_;
	pendingSave_.reset();
	const Result result = finishSave(saveResult);
	lastCompletedSaveKind_ = completedKind;
	if (!pendingSave_)
		pendingSaveKind_ = SaveKind::None;
	return result;
}

Result PreferencesController::waitForSave()
{
	if (!pendingSave_)
		return Result::Success();
	const Result saveResult = pendingSave_->get();
	const SaveKind completedKind = pendingSaveKind_;
	pendingSave_.reset();
	const Result result = finishSave(saveResult);
	lastCompletedSaveKind_ = completedKind;
	if (!pendingSave_)
		pendingSaveKind_ = SaveKind::None;
	return result;
}

Result PreferencesController::finishSave(const Result &saveResult)
{
	if (!saveResult)
	{
		const Result restore = restoreCredentials();
		if (queuedUiState_)
		{
			const auto queued = mergeUiStateIntoCurrent(*queuedUiState_);
			queuedUiState_.reset();
			beginUiStateSaveNow(queued);
		}
		return restore ? saveResult : Result::Failure(saveResult.message + "; credentials could not be fully restored: " + restore.message, ResultCode::Partial);
	}

	const bool uiOnly = pendingSaveKind_ == SaveKind::UiState;
	const Result commit = configManager.commitPreferences(pendingPreferences_);
	const Result runtime = commit
		? (uiOnly ? applyUiRuntime(pendingPreferences_) : applyRuntime(pendingPreferences_))
		: commit;
	if (runtime)
	{
		torznabCredentialChanged_ = false;
		proxyCredentialChanged_ = false;
		if (queuedUiState_)
		{
			const auto queued = mergeUiStateIntoCurrent(*queuedUiState_);
			queuedUiState_.reset();
			beginUiStateSaveNow(queued);
		}
		return Result::Success();
	}

	const Result credentialRestore = restoreCredentials();
	configManager.commitPreferences(previousPreferences_);
	const Result runtimeRestore = uiOnly
		? applyUiRuntime(previousPreferences_) : applyRuntime(previousPreferences_);
	configManager.savePreferencesCandidate(settingsPath(), previousPreferences_);
	if (!credentialRestore || !runtimeRestore)
		return Result::Failure("Preferences failed and runtime restoration was incomplete: " + runtime.message, ResultCode::Partial);
	if (queuedUiState_)
	{
		const auto queued = mergeUiStateIntoCurrent(*queuedUiState_);
		queuedUiState_.reset();
		beginUiStateSaveNow(queued);
	}
	return Result::Failure("Preferences were rolled back: " + runtime.message, runtime.code);
}

Result PreferencesController::restoreCredentials()
{
	Result firstFailure = Result::Success();
	if (torznabCredentialChanged_)
	{
		const Result result = previousTorznabCredential_
			? credentialStore.store("torznab_api_key", *previousTorznabCredential_)
			: credentialStore.erase("torznab_api_key");
		if (!result)
		{
			Utils::Logger::error("config", "Unable to restore Torznab credential: " + result.message);
			firstFailure = Result::Failure(result.message, ResultCode::Partial);
		}
	}
	if (proxyCredentialChanged_)
	{
		const Result result = previousProxyCredential_
			? credentialStore.store("proxy_password", *previousProxyCredential_)
			: credentialStore.erase("proxy_password");
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

	const std::string proxyPassword = credentialStore.load("proxy_password").value_or("");
	torrentManager.setProxyConfig(settings.proxyHost, settings.proxyPort, settings.proxyUsername, proxyPassword,
		settings.proxyEnabled ? (settings.proxyType == "http" ? 2 : 1) : 0);
	const Result searchProxy = searchEngine.setProxyConfig(settings.proxyEnabled, settings.proxyType,
		settings.proxyHost, settings.proxyPort, settings.proxyUsername, proxyPassword);
	if (!searchProxy)
		return searchProxy;

	if (settings.torznabEnabled)
	{
		const std::string apiKey = credentialStore.load("torznab_api_key").value_or("");
		const Result provider = searchEngine.configureTorznabProvider(settings.torznabUrl, apiKey);
		if (!provider)
			return provider;
		return searchEngine.setActiveSearchProvider("torznab");
	}
	return searchEngine.setActiveSearchProvider("torrents-csv");
}

Result PreferencesController::applyUiRuntime(const PreferencesSettings &settings)
{
	if (themeCallback)
		themeCallback(settings.theme);
	return Result::Success();
}
} // namespace Presentation
