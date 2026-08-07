#pragma once

#include "Result.hpp"
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/info_hash.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <memory>
#include <optional>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <deque>
#include <array>

#include "PersistedTorrent.hpp"
#include "Logger.hpp"
#include <future>

struct TorrentConfigData;

enum class TorrentEventType
{
	Log,
	ResumeData,
	ResumeFailed,
	StatusChanged
};

struct TorrentEvent
{
	TorrentEventType type = TorrentEventType::Log;
	Utils::LogLevel severity = Utils::LogLevel::Info;
	std::string category;
	std::string message;
	std::optional<lt::info_hash_t> hash;
};

// A value snapshot used by the UI and persistence layers. The map containing
// these entries never escapes TorrentManager, so callers cannot race a map
// mutation while rendering or saving the application state.
struct ManagedTorrent
{
	lt::info_hash_t hash;
	lt::torrent_handle handle;
	std::string torrentFilePath;
	std::vector<char> resumeData;
	std::string displayName;
};

struct PersistenceSnapshotResult
{
	bool success = false;
	std::vector<PersistedTorrent> torrents;
	std::string errorMessage;
};

enum class TorrentRemovalMode
{
	KeepAllFiles,
	DeleteData,
	DeleteSourceTorrent,
	DeleteDataAndSourceTorrent
};

enum class TorrentCommand
{
	Pause,
	Resume,
	ForceStart,
	ForceRecheck,
	MoveQueueUp,
	MoveQueueDown,
	ForceReannounce,
	EnableSequential,
	DisableSequential
};

enum class TorrentDetailSection
{
	Files = 0,
	Peers = 1,
	Trackers = 2
};

enum class TorrentDetailState
{
	Loading,
	Ready,
	Unavailable,
	Failed
};

struct TorrentFileSnapshot
{
	int index = -1;
	std::string name;
	std::string relativePath;
	std::int64_t size = 0;
	std::int64_t downloaded = 0;
	int priority = 0;
};

struct TorrentPeerSnapshot
{
	std::string address;
	std::string client;
	std::string flags;
	int downloadSpeed = 0;
	int uploadSpeed = 0;
};

struct TorrentTrackerSnapshot
{
	std::string url;
	bool verified = false;
};

struct TorrentDetailsSnapshot
{
	TorrentDetailSection section = TorrentDetailSection::Files;
	TorrentDetailState state = TorrentDetailState::Loading;
	std::string savePath;
	std::string message;
	bool truncated = false;
	std::uint64_t revision = 0;
	std::vector<TorrentFileSnapshot> files;
	std::vector<TorrentPeerSnapshot> peers;
	std::vector<TorrentTrackerSnapshot> trackers;
};

class TorrentManager
{
public:
	TorrentManager();
	~TorrentManager();
	TorrentManager(const TorrentManager &) = delete;
	TorrentManager &operator=(const TorrentManager &) = delete;
	Result addTorrent(const std::string &torrentPath, const std::string &savePath = "./downloads");
	Result addMagnetTorrent(const std::string &magnetUri, const std::string &savePath = "./downloads");
	void addTorrentsFromConfig(const std::vector<TorrentConfigData> &torrents);
	Result removeTorrent(const lt::info_hash_t &hash, TorrentRemovalMode removeMode);
	Result executeCommand(const lt::info_hash_t &hash, TorrentCommand command);
	std::vector<ManagedTorrent> getTorrentSnapshot() const;
	std::uint64_t getTorrentCollectionRevision() const { return torrentCollectionRevision.load(); }
	Result getPersistenceSnapshot(std::vector<ManagedTorrent> &snapshot, std::chrono::milliseconds timeout = std::chrono::seconds(5));
	PersistedTorrent toPersistedTorrent(const ManagedTorrent &torrent) const;
	std::vector<PersistedTorrent> toPersistedTorrents(const std::vector<ManagedTorrent> &torrents) const;

	// Speed limit methods
	void setDownloadSpeedLimit(int bytesPerSecond); // 0 means unlimited
	void setUploadSpeedLimit(int bytesPerSecond);	// 0 means unlimited
	int getDownloadSpeedLimit() const;
	int getUploadSpeedLimit() const;
	void configureDiscovery(bool enableDht, bool enableUpnp, bool enableNatPmp);

	// Status cache methods
	std::optional<lt::torrent_status> getCachedStatus(const lt::info_hash_t &hash) const;
	std::shared_ptr<const std::unordered_map<lt::info_hash_t, lt::torrent_status>> getStatusCache() const;
	std::uint64_t getStatusRevision() const;
	void refreshStatusCache();
	void requestStatusRefresh();
	void setCacheRefreshInterval(int milliseconds);
	bool shouldRefreshCache() const;

	// Asynchronous, UI-safe snapshots for potentially expensive detail views.
	void requestDetailsRefresh(const lt::info_hash_t &hash, TorrentDetailSection section);
	std::shared_ptr<const TorrentDetailsSnapshot> getDetailsSnapshot(const lt::info_hash_t &hash, TorrentDetailSection section) const;
	Result setFilePriority(const lt::info_hash_t &hash, int fileIndex, int priority);

	// Sequential download (streaming) methods
	void setSequentialDownload(const lt::info_hash_t &hash, bool sequential);
	bool isSequentialDownload(const lt::info_hash_t &hash) const;

	// Proxy configuration methods
	// proxyType: 0 disables the proxy, 1 selects SOCKS5, 2 selects HTTP.
	void setProxyConfig(const std::string &hostname, int port, const std::string &username = "", const std::string &password = "", int proxyType = 0);

	// Asynchronous persistence snapshot methods
	Result requestPersistenceSnapshot();
	std::optional<PersistenceSnapshotResult> pollPersistenceSnapshot();

	// Event draining methods (replaces raw pollAlerts)
	std::vector<TorrentEvent> drainEvents();

private:
	lt::session session;
	mutable std::mutex operationMutex;
	mutable std::mutex stateMutex;
	std::unordered_map<lt::info_hash_t, lt::torrent_handle> torrents;
	std::unordered_map<lt::info_hash_t, std::string> torrentFilePaths;
	std::unordered_map<lt::info_hash_t, std::string> torrentDisplayNames;
	std::atomic<std::uint64_t> torrentCollectionRevision{0};

	// Alert pump & persistence synchronization
	mutable std::mutex alertMutex_;
	std::condition_variable alertCv_;
	std::atomic<bool> stopAlertWorker_{false};
	std::vector<TorrentEvent> eventsQueue_;
	std::unordered_map<lt::info_hash_t, std::vector<char>> resumeDataStore_;
	std::unordered_set<lt::info_hash_t> pendingResumeHashes_;
	std::thread alertWorker_;
	void alertWorkerLoop();

	// Async persistence task
	mutable std::mutex asyncPersistenceMutex_;
	std::future<PersistenceSnapshotResult> asyncPersistenceFuture_;
	bool asyncPersistencePending_{false};
	std::atomic<bool> shuttingDown_{false};

	// Status cache
	mutable std::mutex cacheMutex;
	std::shared_ptr<const std::unordered_map<lt::info_hash_t, lt::torrent_status>> statusCache = std::make_shared<std::unordered_map<lt::info_hash_t, lt::torrent_status>>();
	std::uint64_t statusRevision = 0;
	std::chrono::steady_clock::time_point lastCacheRefresh;
	int cacheRefreshIntervalMs = 250; // Default 250ms
	std::mutex statusWorkerMutex;
	std::condition_variable statusWorkerCv;
	std::atomic<bool> stopStatusWorker{false};
	std::atomic<bool> statusRefreshPending{false};
	std::thread statusWorker;
	void statusWorkerLoop();

	struct DetailRequest
	{
		lt::info_hash_t hash;
		TorrentDetailSection section;
	};
	mutable std::mutex detailMutex;
	std::condition_variable detailCv;
	std::deque<DetailRequest> detailRequests;
	std::array<std::unordered_set<lt::info_hash_t>, 3> pendingDetailRequests;
	std::array<std::unordered_map<lt::info_hash_t, std::shared_ptr<const TorrentDetailsSnapshot>>, 3> detailCache;
	std::array<std::unordered_map<lt::info_hash_t, std::uint64_t>, 3> detailRevisions;
	std::array<std::unordered_map<lt::info_hash_t, std::chrono::steady_clock::time_point>, 3> detailLastRefresh;
	std::atomic<bool> stopDetailWorker{false};
	std::thread detailWorker;
	void detailWorkerLoop();
	std::shared_ptr<TorrentDetailsSnapshot> collectDetails(const DetailRequest &request);
};
