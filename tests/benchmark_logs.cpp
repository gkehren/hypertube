#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdarg>

struct LogEntry {
    std::string category;
    std::string message;
    std::chrono::system_clock::time_point timestamp;

    LogEntry(std::string cat, std::string msg)
        : category(cat), message(msg), timestamp(std::chrono::system_clock::now()) {}
};

// Mock filter settings
bool showTrackerErrors = true;
bool showStorageErrors = true;
bool showConnectionStats = true;
bool showGeneralAlerts = true;

bool filter(const LogEntry& entry) {
    if ((entry.category == "Tracker" && !showTrackerErrors) ||
        (entry.category == "Storage" && !showStorageErrors) ||
        (entry.category == "Stats" && !showConnectionStats) ||
        (entry.category == "General" && !showGeneralAlerts))
    {
        return false;
    }
    return true;
}

std::string formatTimestamp(const std::chrono::system_clock::time_point &time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;
    std::stringstream ss;
    // Use thread-safe version if possible, but for benchmark localtime is fine
    struct tm* tm_ptr = std::localtime(&time_t);
    if (tm_ptr)
        ss << std::put_time(tm_ptr, "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// Mock ImGui calls
void MockImGui_TextWrapped(const char* fmt, ...) {
    // Simulate some work that ImGui would do (like layout/vsnprintf)
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    static volatile int sink = 0;
    sink += buf[0];
}

void RenderLogs_Original(const std::deque<LogEntry>& entries) {
    for (const auto &entry : entries) {
        if (!filter(entry)) continue;
        std::string ts = formatTimestamp(entry.timestamp);
        MockImGui_TextWrapped("[%s] [%s] %s", ts.c_str(), entry.category.c_str(), entry.message.c_str());
    }
}

void RenderLogs_Optimized(const std::deque<LogEntry>& entries, std::vector<const LogEntry*>& cache) {
    cache.clear();
    for (const auto &entry : entries) {
        if (filter(entry)) {
            cache.push_back(&entry);
        }
    }

    // Simulate clipper: only 20 items visible
    int start = 0;
    int end = 20;
    for (int i = start; i < end && i < (int)cache.size(); i++) {
        const auto& entry = *cache[i];
        std::string ts = formatTimestamp(entry.timestamp);
        MockImGui_TextWrapped("[%s] [%s] %s", ts.c_str(), entry.category.c_str(), entry.message.c_str());
    }
}

int main() {
    const int NUM_ENTRIES = 1000;
    const int ITERATIONS = 100;

    std::deque<LogEntry> entries;
    for (int i = 0; i < NUM_ENTRIES; i++) {
        entries.emplace_back("General", "Log message " + std::to_string(i));
    }

    std::vector<const LogEntry*> cache;
    cache.reserve(NUM_ENTRIES);

    // Warm up
    RenderLogs_Original(entries);
    RenderLogs_Optimized(entries, cache);

    // Baseline
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        RenderLogs_Original(entries);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto durationOriginal = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / ITERATIONS;

    // Optimized
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        RenderLogs_Optimized(entries, cache);
    }
    end = std::chrono::high_resolution_clock::now();
    auto durationOptimized = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / ITERATIONS;

    std::cout << "Entries: " << NUM_ENTRIES << std::endl;
    std::cout << "Original rendering:  " << durationOriginal << " us/frame" << std::endl;
    std::cout << "Optimized rendering: " << durationOptimized << " us/frame" << std::endl;
    std::cout << "Speedup:             " << (float)durationOriginal / durationOptimized << "x" << std::endl;

    return 0;
}
