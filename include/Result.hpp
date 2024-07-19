#pragma once

#include <string>

class Result
{
	public:
		bool			success;
		std::string		message;

		Result(bool success, const std::string& message = "") : success(success), message(message) {}

		static Result	Success() { return Result(true); }
		static Result	Failure(const std::string& message) { return Result(false, message); }

		operator bool() const { return this->success; };
		operator std::string() const { return this->message; }
};