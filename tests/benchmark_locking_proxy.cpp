#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <thread>
#include <random>

// Simulate libtorrent types
struct sha1_hash {
    std::size_t hash;
    bool operator==(const sha1_hash& other) const { return hash == other.hash; }
};

struct torrent_status {
    int queue_position;
    std::string name;
    float progress;
    int state;
    long long total_wanted;
    long long total_done;
    long long download_payload_rate;
    long long upload_payload_rate;
    int num_seeds;
    int num_peers;
};

// Custom hash for sha1_hash
struct sha1_hash_hasher {
    std::size_t operator()(const sha1_hash& h) const { return h.hash; }
};

// Current approach: Mutex per lookup
class CurrentManager {
    std::mutex m_mutex;
    std::unordered_map<sha1_hash, torrent_status, sha1_hash_hasher> m_cache;

public:
    void setCache(const std::unordered_map<sha1_hash, torrent_status, sha1_hash_hasher>& cache) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache = cache;
    }

    const torrent_status* getCachedStatus(const sha1_hash& hash) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(hash);
        if (it != m_cache.end()) {
            return &it->second;
        }
        return nullptr;
    }
};

// Proposed approach: SharedPtr Snapshot (RCU-like)
class ProposedManager {
    mutable std::mutex m_mutex;
    std::shared_ptr<const std::unordered_map<sha1_hash, torrent_status, sha1_hash_hasher>> m_cache;

public:
    ProposedManager() {
        m_cache = std::make_shared<std::unordered_map<sha1_hash, torrent_status, sha1_hash_hasher>>();
    }

    void setCache(const std::unordered_map<sha1_hash, torrent_status, sha1_hash_hasher>& cache) {
        auto newCache = std::make_shared<std::unordered_map<sha1_hash, torrent_status, sha1_hash_hasher>>(cache);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache = std::move(newCache);
    }

    std::shared_ptr<const std::unordered_map<sha1_hash, torrent_status, sha1_hash_hasher>> getStatusCache() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_cache;
    }
};

int main() {
    const int NUM_ITEMS = 1000;
    const int NUM_ITERATIONS = 10000;

    // Setup data
    std::vector<sha1_hash> hashes;
    std::unordered_map<sha1_hash, torrent_status, sha1_hash_hasher> initialData;
    for (int i = 0; i < NUM_ITEMS; ++i) {
        sha1_hash h{static_cast<std::size_t>(i)};
        hashes.push_back(h);
        initialData[h] = torrent_status{i, "Torrent " + std::to_string(i), 0.5f, 1, 1000, 500, 100, 50, 10, 20};
    }

    CurrentManager currentMgr;
    currentMgr.setCache(initialData);

    ProposedManager proposedMgr;
    proposedMgr.setCache(initialData);

    std::cout << "Benchmarking " << NUM_ITERATIONS << " iterations of iterating " << NUM_ITEMS << " items..." << std::endl;

    // Benchmark Current
    auto startCurrent = std::chrono::high_resolution_clock::now();
    long long checksumCurrent = 0;
    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        for (const auto& h : hashes) {
            const torrent_status* status = currentMgr.getCachedStatus(h);
            if (status) {
                checksumCurrent += status->queue_position;
            }
        }
    }
    auto endCurrent = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> durationCurrent = endCurrent - startCurrent;

    // Benchmark Proposed
    auto startProposed = std::chrono::high_resolution_clock::now();
    long long checksumProposed = 0;
    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        auto cache = proposedMgr.getStatusCache(); // Lock once
        for (const auto& h : hashes) {
            auto it = cache->find(h); // No lock
            if (it != cache->end()) {
                checksumProposed += it->second.queue_position;
            }
        }
    }
    auto endProposed = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> durationProposed = endProposed - startProposed;

    std::cout << "Current Approach (Mutex per Item): " << durationCurrent.count() << " ms" << std::endl;
    std::cout << "Proposed Approach (SharedPtr Snapshot): " << durationProposed.count() << " ms" << std::endl;
    std::cout << "Speedup: " << durationCurrent.count() / durationProposed.count() << "x" << std::endl;

    if (checksumCurrent != checksumProposed) {
        std::cerr << "Error: Checksums do not match!" << std::endl;
        return 1;
    }

    return 0;
}
