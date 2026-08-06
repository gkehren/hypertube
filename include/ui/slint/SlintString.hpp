#pragma once

#include "StringUtils.hpp"

#include <slint.h>

#include <string_view>

namespace SlintUi
{
inline slint::SharedString toSharedString(std::string_view value)
{
	return slint::SharedString(Utils::sanitizeUtf8(std::string(value)));
}
}
