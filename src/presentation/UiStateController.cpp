#include "presentation/UiStateController.hpp"

#include "presentation/PreferencesController.hpp"

namespace Presentation
{
UiStateController::UiStateController(PreferencesController &preferencesController, UiStateSnapshot committed,
	ApplyCallback applyCallback, ErrorCallback errorCallback)
	: preferencesController_(preferencesController), committed_(std::move(committed)),
	  applyCallback_(std::move(applyCallback)), errorCallback_(std::move(errorCallback))
{
}

void UiStateController::request(UiStateSnapshot state)
{
	if (!pending_ && !inFlight_ && state.theme == committed_.theme
		&& state.layout.sidebarWidth == committed_.layout.sidebarWidth
		&& state.layout.bottomPanelHeight == committed_.layout.bottomPanelHeight
		&& state.layout.sidebarCollapsed == committed_.layout.sidebarCollapsed
		&& state.layout.selectedMainTab == committed_.layout.selectedMainTab
		&& state.layout.selectedDetailsTab == committed_.layout.selectedDetailsTab)
		return;

	// Apply immediately for a responsive preview, while the durable write is
	// coalesced behind the debounce window.
	if (applyCallback_)
		applyCallback_(state);
	pending_ = std::move(state);
	deadline_ = std::chrono::steady_clock::now() + debounce;
}

PreferencesSettings UiStateController::mergeIntoPreferences(const UiStateSnapshot &state) const
{
	auto preferences = preferencesController_.current();
	preferences.theme = state.theme;
	preferences.ui = state.layout;
	return preferences;
}

void UiStateController::rollback(const Result &result)
{
	pending_.reset();
	inFlight_.reset();
	waitingForSave_ = false;
	if (applyCallback_)
		applyCallback_(committed_);
	if (errorCallback_)
		errorCallback_(result);
}

Result UiStateController::startPendingSave()
{
	if (!pending_ || waitingForSave_)
		return Result::Success();

	inFlight_ = std::move(pending_);
	pending_.reset();
	const Result begin = preferencesController_.beginUiStateSave(mergeIntoPreferences(*inFlight_));
	if (!begin)
	{
		rollback(begin);
		return begin;
	}
	waitingForSave_ = true;
	return Result::Success();
}

void UiStateController::poll()
{
	if (pending_ && !waitingForSave_ && std::chrono::steady_clock::now() >= deadline_)
	{
		const Result begin = startPendingSave();
		if (!begin)
			return;
	}

	if (!waitingForSave_)
		return;
	const auto result = preferencesController_.pollSave();
	if (!result)
		return;
	if (preferencesController_.lastCompletedSaveKind() == PreferencesController::SaveKind::UiState)
	{
		if (*result)
		{
			if (inFlight_)
				committed_ = *inFlight_;
			inFlight_.reset();
			waitingForSave_ = false;
		}
		else
			rollback(*result);
	}
	else
	{
		// A network transaction may have completed before the queued UI write
		// was started. Keep waiting for that write even if the network save failed.
		if (!*result && errorCallback_)
			errorCallback_(*result);
	}
}

Result UiStateController::flush()
{
	Result firstFailure = Result::Success();
	while (pending_ || waitingForSave_)
	{
		if (pending_ && !waitingForSave_)
		{
			const Result begin = startPendingSave();
			if (!begin)
				return begin;
		}

		const Result result = preferencesController_.waitForSave();
		if (!result && firstFailure)
			firstFailure = result;
		if (preferencesController_.lastCompletedSaveKind() == PreferencesController::SaveKind::UiState)
		{
			if (!result)
			{
				rollback(result);
				return result;
			}
			if (inFlight_)
				committed_ = *inFlight_;
			inFlight_.reset();
			waitingForSave_ = false;
		}
		else if (!result && errorCallback_)
			errorCallback_(result);
	}
	return firstFailure;
}
} // namespace Presentation
