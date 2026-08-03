#pragma once

#include <string>

enum class ResultCode
{
	None,
	InvalidInput,
	NotFound,
	Duplicate,
	Busy,
	Cancelled,
	Network,
	Unauthorized,
	RateLimited,
	Parse,
	Storage,
	Unavailable,
	Internal
};

class Result
{
public:
	bool success;
	std::string message;
	ResultCode code;
	bool retryable;

	Result(bool success, const std::string &message = "", ResultCode code = ResultCode::None, bool retryable = false)
		: success(success), message(message), code(code), retryable(retryable) {}

	static Result Success() { return Result(true); }
	static Result Failure(const std::string &message, ResultCode code = ResultCode::Internal, bool retryable = false)
	{
		return Result(false, message, code, retryable);
	}

	operator bool() const { return this->success; };
	operator std::string() const { return this->message; }
};
