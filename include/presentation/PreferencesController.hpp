#pragma once

#include "ConfigManager.hpp"
#include "Result.hpp"

#include <functional>
#include <future>
#include <optional>
#include <string>

class TorrentManager;
class SearchEngine;

namespace Presentation
{
class PreferencesController
{
public:
	using ThemeCallback = std::function<void(int)>;

	PreferencesController(TorrentManager &torrentManager, SearchEngine &searchEngine,
		ConfigManager &configManager, ThemeCallback themeCallback = {});
	~PreferencesController();

	PreferencesSettings current() const;
	bool isSaving() const { return pendingSave_.has_value(); }
	Result beginSave(const PreferencesSettings &settings,
		std::optional<std::string> torznabApiKey = std::nullopt,
		std::optional<std::string> proxyPassword = std::nullopt);
	std::optional<Result> pollSave();
	Result waitForSave();
	Result applyRuntime(const PreferencesSettings &settings);
	Result restoreCredentials();

private:
	TorrentManager &torrentManager;
	SearchEngine &searchEngine;
	ConfigManager &configManager;
	ThemeCallback themeCallback;
	std::optional<SaveHandle> pendingSave_;
	PreferencesSettings pendingPreferences_;
	PreferencesSettings previousPreferences_;
	std::optional<std::string> previousTorznabCredential_;
	std::optional<std::string> previousProxyCredential_;
	bool torznabCredentialChanged_ = false;
	bool proxyCredentialChanged_ = false;

	Result finishSave(const Result &saveResult);
};
} // namespace Presentation
