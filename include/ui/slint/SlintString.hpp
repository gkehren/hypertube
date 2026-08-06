#pragma once

#include "StringUtils.hpp"
#include "utils/TorrentIdentity.hpp"

#include <slint.h>

#include <string_view>
#include <cassert>

namespace SlintUi
{
inline slint::SharedString toSharedString(std::string_view value)
{
	return slint::SharedString(Utils::sanitizeUtf8(std::string(value)));
}

inline slint::SharedString toSharedTorrentId(std::string_view value)
{
	assert(Utils::TorrentIdentity::isValid(value));
	return slint::SharedString(value);
}
}
