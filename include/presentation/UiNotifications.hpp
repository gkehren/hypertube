#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>

namespace Presentation
{
enum class NotificationSeverity
{
	Info,
	Success,
	Warning,
	Error
};

struct UiNotification
{
	NotificationSeverity severity = NotificationSeverity::Info;
	std::string title;
	std::string message;
	std::string actionLabel;
	std::string actionId;
	std::chrono::milliseconds duration{5000};
};

class NotificationQueue
{
public:
	explicit NotificationQueue(std::size_t capacity = 8);

	void enqueue(UiNotification notification);
	void tick(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
	void dismiss();
	const std::optional<UiNotification> &current() const { return current_; }
	std::size_t pendingCount() const { return pending_.size(); }

private:
	static bool sameNotification(const UiNotification &left, const UiNotification &right);
	void activateNext(std::chrono::steady_clock::time_point now);

	std::size_t capacity_;
	std::deque<UiNotification> pending_;
	std::optional<UiNotification> current_;
	std::chrono::steady_clock::time_point deadline_{};
};
} // namespace Presentation
