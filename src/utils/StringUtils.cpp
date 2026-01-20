#include "StringUtils.hpp"
#include <cstdio>
#include <cmath>

namespace Utils {

    void formatBytes(size_t bytes, bool speed, char* buf, size_t buf_size)
    {
        static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
        size_t size = bytes;
        size_t unitIndex = 0;

        while (size >= 1024 && unitIndex < 4)
        {
            size /= 1024;
            unitIndex++;
        }

        snprintf(buf, buf_size, "%zu %s%s", size, units[unitIndex], speed ? "/s" : "");
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

            if (daysLeft > 0)
                snprintf(buf, buf_size, "%lld days", (long long)daysLeft);
            else if (hoursLeft > 0)
                snprintf(buf, buf_size, "%lld hours", (long long)hoursLeft);
            else if (minutesLeft > 0)
                snprintf(buf, buf_size, "%lld minutes", (long long)minutesLeft);
            else
                snprintf(buf, buf_size, "%lld seconds", (long long)secondsLeft);
        }
        else
        {
            if (buf_size > 3) {
                 buf[0] = 'N'; buf[1] = '/'; buf[2] = 'A'; buf[3] = '\0';
            }
        }
    }

}
