#pragma once

#include <libtorrent/info_hash.hpp>

#include <string>
#include <string_view>

namespace Utils::TorrentIdentity
{
inline std::string hex(std::string_view bytes)
{
	static constexpr char digits[] = "0123456789abcdef";
	std::string result(bytes.size() * 2, '\0');
	for (std::size_t index = 0; index < bytes.size(); ++index)
	{
		const auto byte = static_cast<unsigned char>(bytes[index]);
		result[index * 2] = digits[byte >> 4];
		result[index * 2 + 1] = digits[byte & 0x0f];
	}
	return result;
}

inline std::string id(const lt::info_hash_t &hash)
{
	if (hash.has_v1())
		return hex(hash.v1.to_string());
	if (hash.has_v2())
		return hex(hash.v2.to_string());
	return {};
}

inline bool matches(const lt::info_hash_t &hash, const std::string &candidate)
{
	return (hash.has_v1() && hex(hash.v1.to_string()) == candidate)
		|| (hash.has_v2() && hex(hash.v2.to_string()) == candidate);
}
}
