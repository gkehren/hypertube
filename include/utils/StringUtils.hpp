#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/peer_info.hpp>

namespace Utils {

    // Formats bytes into a human-readable string (e.g., "1.5 MB").
    // Writes to the provided buffer.
    void formatBytes(size_t bytes, bool speed, char* buf, size_t buf_size);

    // Returns a string representation of the torrent state.
    // Returns a pointer to a static string literal.
    const char* torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags);

    // Computes the ETA and writes it to the provided buffer.
    void computeETA(const lt::torrent_status &status, char* buf, size_t buf_size);

    // Generates a string of flags representing the peer's state (e.g., "D U E").
    // Writes to the provided buffer.
    void getPeerFlags(const lt::peer_info& p, char* buf, size_t buf_size);

    // Encodes a string for use in a URL.
    std::string urlEncode(const std::string &value);

    // Formats a magnet URI from an infoHash and a name.
    std::string formatMagnetUri(const std::string &infoHash, const std::string &name);

}
