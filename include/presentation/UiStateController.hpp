#pragma once

#include "ConfigManager.hpp"
#include "Result.hpp"

#include <chrono>
#include <functional>
#include <optional>

namespace Presentation
{
class PreferencesController;

struct UiStateSnapshot
{
	int theme = 0;
	PreferencesSettings::UiLayout layout;
};

class UiStateController
{
public:
	using ApplyCallback = std::function<void(const UiStateSnapshot &)>;
	using ErrorCallback = std::function<void(const Result &)>;

	UiStateController(PreferencesController &preferencesController, UiStateSnapshot committed,
		ApplyCallback applyCallback = {}, ErrorCallback errorCallback = {});

	void request(UiStateSnapshot state);
	void poll();
	Result flush();
	bool hasPending() const { return pending_.has_value() || inFlight_.has_value() || waitingForSave_; }
	const UiStateSnapshot &committed() const { return committed_; }

private:
	PreferencesController &preferencesController_;
	UiStateSnapshot committed_;
	std::optional<UiStateSnapshot> pending_;
	std::optional<UiStateSnapshot> inFlight_;
	std::chrono::steady_clock::time_point deadline_{};
	ApplyCallback applyCallback_;
	ErrorCallback errorCallback_;
	bool waitingForSave_ = false;

	static constexpr std::chrono::milliseconds debounce{400};
	PreferencesSettings mergeIntoPreferences(const UiStateSnapshot &state) const;
	Result startPendingSave();
	void rollback(const Result &result);
};
} // namespace Presentation
