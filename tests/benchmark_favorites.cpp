#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <chrono>
#include <random>
#include <iomanip>

// Mimic the TorrentSearchResult structure
struct TorrentSearchResult {
    std::string name;
    std::string infoHash;
    // ... other fields irrelevant for this benchmark
};

// Original implementation (Linear Search)
bool isInFavorites_Linear(const std::vector<TorrentSearchResult>& favorites, const std::string& infoHash) {
    return std::find_if(favorites.begin(), favorites.end(),
                        [&infoHash](const TorrentSearchResult& fav) {
                            return fav.infoHash == infoHash;
                        }) != favorites.end();
}

// Optimized implementation (Set Search)
bool isInFavorites_Set(const std::unordered_set<std::string>& favoriteHashes, const std::string& infoHash) {
    return favoriteHashes.find(infoHash) != favoriteHashes.end();
}

std::string generateRandomHash(int length = 40) {
    static const char charset[] = "0123456789ABCDEF";
    std::string result;
    result.resize(length);
    for (int i = 0; i < length; i++) {
        result[i] = charset[rand() % 16];
    }
    return result;
}

int main() {
    srand(time(nullptr));

    const std::vector<int> sizes = {10, 100, 1000, 10000};
    const int ITERATIONS = 100000;

    std::cout << std::left << std::setw(10) << "Size"
              << std::setw(20) << "Linear (ns/op)"
              << std::setw(20) << "Set (ns/op)"
              << std::setw(15) << "Speedup" << std::endl;
    std::cout << std::string(65, '-') << std::endl;

    for (int size : sizes) {
        std::vector<TorrentSearchResult> favorites;
        std::unordered_set<std::string> favoriteHashes;

        // Populate data
        for (int i = 0; i < size; ++i) {
            std::string hash = generateRandomHash();
            TorrentSearchResult res;
            res.infoHash = hash;
            res.name = "Torrent " + std::to_string(i);
            favorites.push_back(res);
            favoriteHashes.insert(hash);
        }

        // Test case: Lookup a hash that exists (randomly picked)
        // and one that doesn't.
        // For stable benchmarking, we'll lookup the last element (worst case for linear)
        // and a non-existent element.

        std::string targetHash = favorites.back().infoHash;
        std::string missingHash = generateRandomHash();

        // Benchmark Linear
        auto startLinear = std::chrono::high_resolution_clock::now();
        volatile bool resultLinear = false;
        for (int i = 0; i < ITERATIONS; ++i) {
            resultLinear = isInFavorites_Linear(favorites, targetHash);
            resultLinear = isInFavorites_Linear(favorites, missingHash);
        }
        auto endLinear = std::chrono::high_resolution_clock::now();
        double timeLinear = std::chrono::duration<double, std::nano>(endLinear - startLinear).count() / (ITERATIONS * 2);

        // Benchmark Set
        auto startSet = std::chrono::high_resolution_clock::now();
        volatile bool resultSet = false;
        for (int i = 0; i < ITERATIONS; ++i) {
            resultSet = isInFavorites_Set(favoriteHashes, targetHash);
            resultSet = isInFavorites_Set(favoriteHashes, missingHash);
        }
        auto endSet = std::chrono::high_resolution_clock::now();
        double timeSet = std::chrono::duration<double, std::nano>(endSet - startSet).count() / (ITERATIONS * 2);

        std::cout << std::left << std::setw(10) << size
                  << std::setw(20) << timeLinear
                  << std::setw(20) << timeSet
                  << std::setw(15) << (timeLinear / timeSet) << "x" << std::endl;
    }

    return 0;
}
