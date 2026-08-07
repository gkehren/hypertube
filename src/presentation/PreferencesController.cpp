#include "presentation/PreferencesController.hpp"

#include "AppPaths.hpp"
#include "CredentialStore.hpp"
#include "Logger.hpp"
#include "SearchEngine.hpp"
#include "TorrentManager.hpp"

#include <chrono>

namespace Presentation
{
namespace
{
Result restoreCredentialSet(const PreferencesController::CredentialStoreOps &credentialStore,
	const std::optional<std::string> &previousTorznabCredential,
	const std::optional<std::string> &previousProxyCredential,
	bool torznabChanged, bool proxyChanged)
{
	Result firstFailure = Result::Success();
	if (torznabChanged)
	{
		const Result result = previousTorznabCredential
			? credentialStore.store("torznab_api_key", *previousTorznabCredential)
			: credentialStore.erase("torznab_api_key");
		if (!result)
		{
			Utils::Logger::error("config", "Unable to restore Torznab credential: " + result.message);
			firstFailure = Result::Failure(result.message, ResultCode::Partial);
		}
	}
	if (proxyChanged)
	{
		const Result result = previousProxyCredential
			? credentialStore.store("proxy_password", *previousProxyCredential)
			: credentialStore.erase("proxy_password");
		if (!result && firstFailure)
			firstFailure = Result::Failure(result.message, ResultCode::Partial);
	}
	return firstFailure;
}
}

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
	while (isSaving())
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
	if (isSaving())
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
	pendingPreferences_ = settings;
	pendingSaveKind_ = SaveKind::Preferences;

	pendingSave_ = std::async(std::launch::async, [this, settings, torznabApiKey, proxyPassword]() -> Result {
		const auto torznabLoad = credentialStore.load("torznab_api_key");
		const auto proxyLoad = credentialStore.load("proxy_password");
		const std::optional<std::string> previousTorznab = torznabLoad.hasSecret()
			? std::optional<std::string>(torznabLoad.secret) : std::nullopt;
		const std::optional<std::string> previousProxy = proxyLoad.hasSecret()
			? std::optional<std::string>(proxyLoad.secret) : std::nullopt;
		std::optional<std::string> runtimeTorznab = previousTorznab;
		std::optional<std::string> runtimeProxy = previousProxy;
		bool torznabChanged = false;
		bool proxyChanged = false;

		if (torznabApiKey.has_value())
		{
			const Result torznab = torznabApiKey->empty()
				? credentialStore.erase("torznab_api_key")
				: credentialStore.store("torznab_api_key", *torznabApiKey);
			if (!torznab)
				return torznab;
			torznabChanged = true;
			runtimeTorznab = torznabApiKey->empty() ? std::nullopt : torznabApiKey;
		}

		if (proxyPassword.has_value())
		{
			const Result proxy = proxyPassword->empty()
				? credentialStore.erase("proxy_password")
				: credentialStore.store("proxy_password", *proxyPassword);
			if (!proxy)
			{
				const Result restore = restoreCredentialSet(credentialStore, previousTorznab, previousProxy,
					torznabChanged, false);
				return restore ? proxy : Result::Failure(proxy.message + "; credentials could not be fully restored: "
					+ restore.message, ResultCode::Partial);
			}
			proxyChanged = true;
			runtimeProxy = proxyPassword->empty() ? std::nullopt : proxyPassword;
		}

		const auto candidateSave = configManager.savePreferencesCandidate(settingsPath(), settings);
		const Result candidateResult = candidateSave.get();
		if (!candidateResult)
		{
			const Result restore = restoreCredentialSet(credentialStore, previousTorznab, previousProxy,
				torznabChanged, proxyChanged);
			return restore ? candidateResult : Result::Failure(candidateResult.message
				+ "; credentials could not be fully restored: " + restore.message, ResultCode::Partial);
		}

		// These fields are consumed only after the shared future becomes ready,
		// which synchronizes the worker's writes with the UI-thread completion.
		previousTorznabCredential_ = previousTorznab;
		previousProxyCredential_ = previousProxy;
		runtimeTorznabCredential_ = runtimeTorznab;
		runtimeProxyCredential_ = runtimeProxy.value_or("");
		torznabCredentialChanged_ = torznabChanged;
		proxyCredentialChanged_ = proxyChanged;
		return candidateResult;
	});

	return Result::Success();
}

Result PreferencesController::beginUiStateSave(const PreferencesSettings &settings)
{
	if (pendingCredentialRollback_)
	{
		queuedUiState_ = settings;
		return Result::Success();
	}
	if (pendingSave_)
	{
		queuedUiState_ = settings;
		return Result::Success();
	}
	return beginUiStateSaveNow(settings);
}

Result PreferencesController::beginUiStateSaveNow(const PreferencesSettings &settings)
{
	if (isSaving())
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
	if (pendingCredentialRollback_)
	{
		if (pendingCredentialRollback_->wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			return std::nullopt;
		const Result rollback = pendingCredentialRollback_->get();
		pendingCredentialRollback_.reset();
		if (!rollback)
			Utils::Logger::error("config", "Credential rollback failed: " + rollback.message);
		if (queuedUiState_ && !pendingSave_)
		{
			const auto queued = mergeUiStateIntoCurrent(*queuedUiState_);
			queuedUiState_.reset();
			if (!beginUiStateSaveNow(queued))
				queuedUiState_ = queued;
		}
		return std::nullopt;
	}
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
	{
		if (!pendingCredentialRollback_)
			return Result::Success();
		const Result rollback = pendingCredentialRollback_->get();
		pendingCredentialRollback_.reset();
		if (queuedUiState_)
		{
			const auto queued = mergeUiStateIntoCurrent(*queuedUiState_);
			queuedUiState_.reset();
			if (!beginUiStateSaveNow(queued))
				queuedUiState_ = queued;
		}
		return rollback;
	}
	const Result saveResult = pendingSave_->get();
	const SaveKind completedKind = pendingSaveKind_;
	pendingSave_.reset();
	const Result result = finishSave(saveResult);
	lastCompletedSaveKind_ = completedKind;
	if (!pendingSave_)
		pendingSaveKind_ = SaveKind::None;
	if (pendingCredentialRollback_)
	{
		const Result rollback = pendingCredentialRollback_->get();
		pendingCredentialRollback_.reset();
		if (!rollback)
			return Result::Failure(result.message + "; credentials could not be fully restored: "
				+ rollback.message, ResultCode::Partial);
		if (queuedUiState_)
		{
			const auto queued = mergeUiStateIntoCurrent(*queuedUiState_);
			queuedUiState_.reset();
			if (!beginUiStateSaveNow(queued))
				queuedUiState_ = queued;
		}
	}
	return result;
}

Result PreferencesController::finishSave(const Result &saveResult)
{
	if (!saveResult)
	{
		if (queuedUiState_)
		{
			const auto queued = mergeUiStateIntoCurrent(*queuedUiState_);
			queuedUiState_.reset();
			beginUiStateSaveNow(queued);
		}
		return saveResult;
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

	const auto previousTorznab = previousTorznabCredential_;
	const auto previousProxy = previousProxyCredential_;
	const bool torznabChanged = torznabCredentialChanged_;
	const bool proxyChanged = proxyCredentialChanged_;
	if (torznabChanged || proxyChanged)
	{
		const auto credentialStoreCopy = credentialStore;
		pendingCredentialRollback_ = std::async(std::launch::async,
			[credentialStoreCopy, previousTorznab, previousProxy, torznabChanged, proxyChanged]() {
				return restoreCredentialSet(credentialStoreCopy, previousTorznab, previousProxy,
					torznabChanged, proxyChanged);
			}).share();
	}
	torznabCredentialChanged_ = false;
	proxyCredentialChanged_ = false;
	runtimeTorznabCredential_ = previousTorznab;
	runtimeProxyCredential_ = previousProxy.value_or("");
	configManager.commitPreferences(previousPreferences_);
	const Result runtimeRestore = uiOnly ? applyUiRuntime(previousPreferences_) : applyRuntime(previousPreferences_);
	configManager.savePreferencesCandidate(settingsPath(), previousPreferences_);
	if (!runtimeRestore)
		return Result::Failure("Preferences failed and runtime restoration was incomplete: " + runtime.message, ResultCode::Partial);
	if (!pendingCredentialRollback_ && queuedUiState_)
	{
		const auto queued = mergeUiStateIntoCurrent(*queuedUiState_);
		queuedUiState_.reset();
		if (!beginUiStateSaveNow(queued))
			queuedUiState_ = queued;
	}
	return Result::Failure("Preferences were rolled back: " + runtime.message, runtime.code);
}

Result PreferencesController::applyRuntime(const PreferencesSettings &settings)
{
	if (themeCallback)
		themeCallback(settings.theme);
	torrentManager.setDownloadSpeedLimit(settings.downloadSpeedLimit);
	torrentManager.setUploadSpeedLimit(settings.uploadSpeedLimit);
	torrentManager.configureDiscovery(settings.enableDht, settings.enableUpnp, settings.enableNatPmp);

	const std::string &proxyPassword = runtimeProxyCredential_;
	torrentManager.setProxyConfig(settings.proxyHost, settings.proxyPort, settings.proxyUsername, proxyPassword,
		settings.proxyEnabled ? (settings.proxyType == "http" ? 2 : 1) : 0);
	const Result searchProxy = searchEngine.setProxyConfig(settings.proxyEnabled, settings.proxyType,
		settings.proxyHost, settings.proxyPort, settings.proxyUsername, proxyPassword);
	if (!searchProxy)
		return searchProxy;

	if (settings.torznabEnabled)
	{
		const std::string apiKey = runtimeTorznabCredential_.value_or("");
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
