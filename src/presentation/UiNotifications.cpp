#include "presentation/UiNotifications.hpp"

#include <algorithm>

namespace Presentation
{
NotificationQueue::NotificationQueue(std::size_t capacity)
	: capacity_(std::max<std::size_t>(1, capacity))
{
}

bool NotificationQueue::sameNotification(const UiNotification &left, const UiNotification &right)
{
	return left.severity == right.severity && left.title == right.title && left.message == right.message
		&& left.actionLabel == right.actionLabel && left.actionId == right.actionId;
}

void NotificationQueue::activateNext(std::chrono::steady_clock::time_point now)
{
	if (pending_.empty())
	{
		current_.reset();
		deadline_ = {};
		return;
	}
	current_ = std::move(pending_.front());
	pending_.pop_front();
	auto duration = std::max(std::chrono::milliseconds(1), current_->duration);
	if (current_->severity == NotificationSeverity::Error)
	{
		duration = std::max(duration, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(8)));
	}
	deadline_ = now + duration;
}

void NotificationQueue::enqueue(UiNotification notification)
{
	if (notification.message.empty())
		return;
	if (current_ && sameNotification(*current_, notification))
		return;
	if (std::find_if(pending_.begin(), pending_.end(), [&notification](const UiNotification &queued)
		{ return sameNotification(queued, notification); }) != pending_.end())
		return;
	if (!current_)
	{
		pending_.push_back(std::move(notification));
		activateNext(std::chrono::steady_clock::now());
		return;
	}
	if (pending_.size() >= capacity_)
		pending_.pop_front();
	pending_.push_back(std::move(notification));
}

void NotificationQueue::tick(std::chrono::steady_clock::time_point now)
{
	if (current_ && now < deadline_)
		return;
	activateNext(now);
}

void NotificationQueue::dismiss()
{
	current_.reset();
	activateNext(std::chrono::steady_clock::now());
}
} // namespace Presentation
