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

template <typename Digest>
inline std::string digestHex(const Digest &digest)
{
	return hex(std::string_view(reinterpret_cast<const char *>(digest.data()), digest.size()));
}

inline std::string id(const lt::info_hash_t &hash)
{
	std::string result;
	if (hash.has_v1())
		result = "v1:" + digestHex(hash.v1);
	if (hash.has_v2())
	{
		if (!result.empty()) result += '|';
		result += "v2:" + digestHex(hash.v2);
	}
	return result;
}

inline bool matches(const lt::info_hash_t &hash, const std::string &candidate)
{
	return id(hash) == candidate;
}

inline bool isValid(std::string_view candidate)
{
	auto validPart = [](std::string_view part, std::string_view prefix, std::size_t hexSize)
	{
		if (!part.starts_with(prefix) || part.size() != prefix.size() + hexSize)
			return false;
		for (const unsigned char character : part.substr(prefix.size()))
			if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')))
				return false;
		return true;
	};
	const auto separator = candidate.find('|');
	if (separator == std::string_view::npos)
		return validPart(candidate, "v1:", 40) || validPart(candidate, "v2:", 64);
	if (candidate.find('|', separator + 1) != std::string_view::npos)
		return false;
	return validPart(candidate.substr(0, separator), "v1:", 40)
		&& validPart(candidate.substr(separator + 1), "v2:", 64);
}
}
