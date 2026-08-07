#pragma once

#include "ConfigManager.hpp"
#include "Result.hpp"

#include <functional>
#include <future>
#include <optional>
#include <string>

class TorrentManager;
class SearchEngine;

#include "CredentialStore.hpp"

namespace Presentation
{
class PreferencesController
{
public:
	using ThemeCallback = std::function<void(int)>;
	struct CredentialStoreOps
	{
		std::function<Result(const std::string &, const std::string &)> store;
		std::function<Result(const std::string &)> erase;
		std::function<Utils::CredentialStore::CredentialLoadResult(const std::string &)> load;
	};
	enum class SaveKind
	{
		None,
		Preferences,
		UiState
	};

	PreferencesController(TorrentManager &torrentManager, SearchEngine &searchEngine,
		ConfigManager &configManager, ThemeCallback themeCallback = {}, CredentialStoreOps credentialStore = {},
		std::string settingsPath = {});
	~PreferencesController();

	PreferencesSettings current() const;
	bool isSaving() const { return pendingSave_.has_value() || pendingCredentialRollback_.has_value(); }
	SaveKind saveKind() const { return pendingSave_ ? pendingSaveKind_ : SaveKind::None; }
	SaveKind lastCompletedSaveKind() const { return lastCompletedSaveKind_; }
	Result beginSave(const PreferencesSettings &settings,
		std::optional<std::string> torznabApiKey = std::nullopt,
		std::optional<std::string> proxyPassword = std::nullopt);
	// UI-only saves deliberately skip network validation and credential access.
	// If a network transaction is already in flight, the newest UI snapshot is
	// queued and committed immediately after it completes.
	Result beginUiStateSave(const PreferencesSettings &settings);
	std::optional<Result> pollSave();
	Result waitForSave();
	Result applyRuntime(const PreferencesSettings &settings);

private:
	TorrentManager &torrentManager;
	SearchEngine &searchEngine;
	ConfigManager &configManager;
	ThemeCallback themeCallback;
	CredentialStoreOps credentialStore;
	std::string settingsPath_;
	std::optional<SaveHandle> pendingSave_;
	std::optional<std::shared_future<Result>> pendingCredentialRollback_;
	SaveKind pendingSaveKind_ = SaveKind::None;
	SaveKind lastCompletedSaveKind_ = SaveKind::None;
	std::optional<PreferencesSettings> queuedUiState_;
	PreferencesSettings pendingPreferences_;
	PreferencesSettings previousPreferences_;
	std::optional<std::string> previousTorznabCredential_;
	std::optional<std::string> previousProxyCredential_;
	std::optional<std::string> runtimeTorznabCredential_;
	std::string runtimeProxyCredential_;
	bool torznabCredentialChanged_ = false;
	bool proxyCredentialChanged_ = false;

	Result finishSave(const Result &saveResult);
	Result beginUiStateSaveNow(const PreferencesSettings &settings);
	const std::string &settingsPath() const;
	Result applyUiRuntime(const PreferencesSettings &settings);
	PreferencesSettings mergeUiStateIntoCurrent(const PreferencesSettings &uiState) const;
};
} // namespace Presentation
