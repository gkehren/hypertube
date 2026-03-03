#include "StringUtils.hpp"
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Utils {

    void formatBytes(size_t bytes, bool speed, char* buf, size_t buf_size)
    {
        static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
        double size = static_cast<double>(bytes);
        size_t unitIndex = 0;

        while (size >= 1024.0 && unitIndex < 4)
        {
            size /= 1024.0;
            unitIndex++;
        }

        std::string numStr;
        if (unitIndex == 0)
        {
            numStr = std::to_string(bytes);
        }
        else
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << size;
            numStr = oss.str();

            // Remove trailing zeros
            size_t dotPos = numStr.find('.');
            if (dotPos != std::string::npos)
            {
                size_t lastNonZero = numStr.find_last_not_of('0');
                if (lastNonZero != std::string::npos && lastNonZero >= dotPos)
                {
                    if (lastNonZero == dotPos)
                        numStr.erase(dotPos);
                    else
                        numStr.erase(lastNonZero + 1);
                }
            }
        }

        std::string result = numStr + " " + units[unitIndex] + (speed ? "/s" : "");
        if (buf && buf_size > 0)
        {
            size_t copyLen = result.copy(buf, buf_size - 1);
            buf[copyLen] = '\0';
        }
    }

    const char* torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags)
    {
        if (flags & lt::torrent_flags::paused)
            return "Paused";

        switch (state)
        {
        case lt::torrent_status::downloading_metadata:
            return "Downloading metadata";
        case lt::torrent_status::downloading:
            return "Downloading";
        case lt::torrent_status::finished:
            return "Finished";
        case lt::torrent_status::seeding:
            return "Seeding";
        case lt::torrent_status::checking_files:
            return "Checking files";
        case lt::torrent_status::checking_resume_data:
            return "Checking resume data";
        default:
            return "Unknown";
        }
    }

    void computeETA(const lt::torrent_status &status, char* buf, size_t buf_size)
    {
        if (status.state == lt::torrent_status::downloading && status.download_payload_rate > 0)
        {
            int64_t secondsLeft = (status.total_wanted - status.total_wanted_done) / status.download_payload_rate;
            int64_t minutesLeft = secondsLeft / 60;
            int64_t hoursLeft = minutesLeft / 60;
            int64_t daysLeft = hoursLeft / 24;

            std::string eta;
            if (daysLeft > 0)
                eta = std::to_string(daysLeft) + " days";
            else if (hoursLeft > 0)
                eta = std::to_string(hoursLeft) + " hours";
            else if (minutesLeft > 0)
                eta = std::to_string(minutesLeft) + " minutes";
            else
                eta = std::to_string(secondsLeft) + " seconds";

            if (buf && buf_size > 0)
            {
                size_t copyLen = eta.copy(buf, buf_size - 1);
                buf[copyLen] = '\0';
            }
        }
        else
        {
            if (buf_size > 3) {
                 buf[0] = 'N'; buf[1] = '/'; buf[2] = 'A'; buf[3] = '\0';
            }
        }
    }

    void getPeerFlags(const lt::peer_info& p, char* buf, size_t buf_size)
    {
        std::string flags;

        if (p.flags & lt::peer_info::interesting) {
            if (p.flags & lt::peer_info::choked) flags += 'd';
            else flags += 'D';
        }

        if (p.flags & lt::peer_info::remote_interested) {
            if (p.flags & lt::peer_info::remote_choked) flags += 'u';
            else flags += 'U';
        }

        if (p.flags & lt::peer_info::optimistic_unchoke) flags += 'O';
        if (p.flags & lt::peer_info::snubbed) flags += 'S';
        if (p.flags & lt::peer_info::local_connection) flags += 'l';

        if (p.flags & lt::peer_info::supports_extensions) flags += 'E';
        if (p.flags & lt::peer_info::utp_socket) flags += 'P';

        if ((p.flags & lt::peer_info::rc4_encrypted) || (p.flags & lt::peer_info::plaintext_encrypted)) {
             flags += 'e';
        }

        if (buf && buf_size > 0)
        {
            size_t copyLen = flags.copy(buf, buf_size - 1);
            buf[copyLen] = '\0';
        }
    }

    std::string urlEncode(const std::string &value)
    {
        static const char lookup[] = "0123456789ABCDEF";
        std::string escaped;
        escaped.reserve(value.length() * 3);

        for (char c : value)
        {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            {
                escaped += c;
            }
            else
            {
                escaped += '%';
                escaped += lookup[(static_cast<unsigned char>(c) >> 4) & 0x0F];
                escaped += lookup[static_cast<unsigned char>(c) & 0x0F];
            }
        }

        return escaped;
    }

    std::string formatMagnetUri(const std::string &infoHash, const std::string &name)
    {
        std::string magnet = "magnet:?xt=urn:btih:" + infoHash;
        if (!name.empty())
        {
            magnet += "&dn=" + urlEncode(name);
        }
        // Add some popular trackers
        magnet += "&tr=udp://tracker.openbittorrent.com:80"
                  "&tr=udp://tracker.opentrackr.org:1337"
                  "&tr=udp://tracker.coppersurfer.tk:6969";
        return magnet;
    }

}
