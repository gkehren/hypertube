#include "TorrentManager.hpp"
#include "ConfigManager.hpp"
#include "AppPaths.hpp"
#include "Logger.hpp"
#include "StringUtils.hpp"
#include "SystemUtils.hpp"
#include "utils/TorrentIdentity.hpp"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <type_traits>
#include <unordered_set>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/write_resume_data.hpp>

namespace
{
void markStatusCacheStale(std::mutex &cacheMutex, std::chrono::steady_clock::time_point &lastCacheRefresh)
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	lastCacheRefresh = {};
}

std::string hashForLog(const lt::info_hash_t &hash)
{
	const auto id = Utils::TorrentIdentity::id(hash);
	return id.empty() ? "unknown" : id;
}

Result validateAddPaths(std::string &savePath, const std::string *torrentPath = nullptr)
{
	std::error_code error;
	if (torrentPath)
	{
		const std::filesystem::path source(*torrentPath);
		if (!std::filesystem::is_regular_file(source, error))
			return Result::Failure("Torrent file does not exist or is not a regular file", ResultCode::InvalidInput);
	}

	if (savePath.empty())
		return Result::Failure("Download path cannot be empty", ResultCode::InvalidInput);
	savePath = Utils::AppPaths::expandUserPath(savePath).string();
	if (savePath.empty())
		return Result::Failure("Download path cannot be resolved", ResultCode::InvalidInput);
	const std::filesystem::path destination(savePath);
	std::filesystem::create_directories(destination, error);
	if (error || !std::filesystem::is_directory(destination, error))
		return Result::Failure("Download path cannot be created or accessed: " + savePath, ResultCode::Storage);
	return Result::Success();
}

std::size_t detailSectionIndex(TorrentDetailSection section)
{
	return static_cast<std::size_t>(section);
}

TorrentEvent makeTorrentEvent(lt::alert *alert)
{
	TorrentEvent event;
	if (!alert)
		return event;

	if (auto *trackerError = lt::alert_cast<lt::tracker_error_alert>(alert))
	{
		event.category = "tracker";
		event.message = std::string("Tracker error for '") + trackerError->torrent_name() + "': " + trackerError->error_message();
		event.severity = Utils::LogLevel::Error;
		event.hash = trackerError->handle.info_hashes();
	}
	else if (auto *trackerWarning = lt::alert_cast<lt::tracker_warning_alert>(alert))
	{
		event.category = "tracker";
		event.message = std::string("Tracker warning for '") + trackerWarning->torrent_name() + "': " + trackerWarning->warning_message();
		event.severity = Utils::LogLevel::Warning;
		event.hash = trackerWarning->handle.info_hashes();
	}
	else if (auto *fileError = lt::alert_cast<lt::file_error_alert>(alert))
	{
		event.category = "storage";
		event.message = std::string("File error for '") + fileError->torrent_name() + "': " + fileError->error.message();
		event.severity = Utils::LogLevel::Error;
		event.hash = fileError->handle.info_hashes();
	}
	else if (auto *movedFailed = lt::alert_cast<lt::storage_moved_failed_alert>(alert))
	{
		event.category = "storage";
		event.message = std::string("Storage move failed for '") + movedFailed->torrent_name() + "': " + movedFailed->error.message();
		event.severity = Utils::LogLevel::Error;
		event.hash = movedFailed->handle.info_hashes();
	}
	else if (lt::alert_cast<lt::session_stats_alert>(alert))
	{
		event.category = "torrent";
		event.message = "Session stats updated";
		event.severity = Utils::LogLevel::Debug;
	}
	else if (auto *added = lt::alert_cast<lt::add_torrent_alert>(alert))
	{
		event.category = "torrent";
		event.message = added->error ? std::string("Failed to add torrent: ") + added->error.message() : std::string("Torrent added: ") + added->torrent_name();
		event.severity = added->error ? Utils::LogLevel::Error : Utils::LogLevel::Info;
		event.hash = added->handle.info_hashes();
	}
	else if (auto *finished = lt::alert_cast<lt::torrent_finished_alert>(alert))
	{
		event.category = "torrent";
		event.message = std::string("Torrent finished: ") + finished->torrent_name();
		event.severity = Utils::LogLevel::Info;
		event.hash = finished->handle.info_hashes();
	}
	else if (auto *metadata = lt::alert_cast<lt::metadata_received_alert>(alert))
	{
		event.category = "torrent";
		event.message = std::string("Metadata received for: ") + metadata->torrent_name();
		event.severity = Utils::LogLevel::Info;
		event.hash = metadata->handle.info_hashes();
	}
	else if (auto *peerError = lt::alert_cast<lt::peer_error_alert>(alert))
	{
		event.category = "peer";
		event.message = std::string("Peer error for '") + peerError->torrent_name() + "': " + peerError->error.message();
		event.severity = Utils::LogLevel::Warning;
		event.hash = peerError->handle.info_hashes();
	}
	else if (lt::alert_cast<lt::dht_bootstrap_alert>(alert))
	{
		event.category = "torrent";
		event.message = "DHT bootstrap complete";
		event.severity = Utils::LogLevel::Info;
	}
	else
	{
		event.category = "torrent";
		event.message = alert->message();
		event.severity = Utils::LogLevel::Debug;
	}
	return event;
}
}

Result TorrentManager::addTorrent(const std::string &torrentPath, const std::string &savePath)
{
	std::string resolvedSavePath = savePath;
	Result validation = validateAddPaths(resolvedSavePath, &torrentPath);
	if (!validation)
		return validation;
	std::lock_guard<std::mutex> operationLock(operationMutex);
	try
	{
		lt::add_torrent_params params;
		params.save_path = resolvedSavePath;
		params.ti = std::make_shared<lt::torrent_info>(torrentPath);
		params.flags |= lt::torrent_flags::duplicate_is_error;
		const lt::info_hash_t expectedHash = params.ti->info_hashes();
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			if ((expectedHash.has_v1() || expectedHash.has_v2()) && torrents.find(expectedHash) != torrents.end())
				return Result::Failure("Torrent is already added", ResultCode::Duplicate);
		}
		lt::torrent_handle handle = this->session.add_torrent(params);

		lt::info_hash_t hash = handle.info_hashes();
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			const auto [_, inserted] = torrents.emplace(hash, handle);
			torrentFilePaths.emplace(hash, torrentPath);
			if (params.ti && !params.ti->name().empty())
				torrentDisplayNames.emplace(hash, params.ti->name());
			if (inserted)
				++torrentCollectionRevision;
		}

		std::cout << "Added torrent from file: " << handle.status().name << std::endl;
		Utils::Logger::info("torrent", "Added torrent from file: " + torrentPath);
		markStatusCacheStale(cacheMutex, lastCacheRefresh);
		return Result::Success();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to add torrent: " << e.what() << std::endl;
		Utils::Logger::error("torrent", "Failed to add torrent: " + std::string(e.what()));
		return Result::Failure(e.what());
	}
}

Result TorrentManager::addMagnetTorrent(const std::string &magnetUri, const std::string &savePath)
{
	std::string resolvedSavePath = savePath;
	Result validation = validateAddPaths(resolvedSavePath);
	if (!validation)
		return validation;
	if (magnetUri.rfind("magnet:?", 0) != 0)
		return Result::Failure("Invalid magnet URI", ResultCode::InvalidInput);
	std::lock_guard<std::mutex> operationLock(operationMutex);
	try
	{
		lt::add_torrent_params params = lt::parse_magnet_uri(magnetUri);
		if (!params.info_hashes.has_v1() && !params.info_hashes.has_v2())
			return Result::Failure("Magnet URI does not contain a supported info hash", ResultCode::InvalidInput);
		std::cout << "Adding magnet torrent: " << params.name << " (hash: " << params.info_hashes.v1 << ")" << std::endl;
		params.save_path = resolvedSavePath;
		params.flags |= lt::torrent_flags::duplicate_is_error;
		if (params.info_hashes.has_v1() || params.info_hashes.has_v2())
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			if (torrents.find(params.info_hashes) != torrents.end())
				return Result::Failure("Torrent is already added", ResultCode::Duplicate);
		}
		lt::torrent_handle handle = this->session.add_torrent(params);
		const lt::info_hash_t hash = handle.info_hashes();
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			const auto [_, inserted] = torrents.emplace(hash, handle);
			if (!params.name.empty())
				torrentDisplayNames.emplace(hash, params.name);
			if (inserted)
				++torrentCollectionRevision;
		}

		std::cout << "Added magnet torrent: " << handle.status().name << std::endl;
		Utils::Logger::info("torrent", "Added torrent from magnet URI");
		markStatusCacheStale(cacheMutex, lastCacheRefresh);
		return Result::Success();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to add magnet torrent: " << e.what() << std::endl;
		Utils::Logger::error("torrent", "Failed to add magnet torrent: " + std::string(e.what()));
		return Result::Failure(e.what());
	}
}

void TorrentManager::addTorrentsFromConfig(const std::vector<TorrentConfigData> &torrents)
{
	for (const auto &data : torrents)
	{
		if (!data.resumeData.empty())
		{
			std::lock_guard<std::mutex> operationLock(operationMutex);
			try
			{
				lt::add_torrent_params params = lt::read_resume_data(data.resumeData);
				if (!data.savePath.empty())
					params.save_path = Utils::AppPaths::expandUserPath(data.savePath).string();
				params.flags |= lt::torrent_flags::duplicate_is_error;
				lt::torrent_handle handle = session.add_torrent(params);
				const lt::info_hash_t hash = handle.info_hashes();
				{
					std::lock_guard<std::mutex> lock(stateMutex);
					const auto [_, inserted] = this->torrents.emplace(hash, handle);
					if (!data.torrentFilePath.empty())
						torrentFilePaths.emplace(hash, data.torrentFilePath);
					if (inserted)
						++torrentCollectionRevision;
				}
				Utils::Logger::info("torrent", "Restored torrent from fast-resume data");
				continue;
			}
			catch (const std::exception &e)
			{
				Utils::Logger::warning("torrent", "Fast-resume data was rejected; using the persisted source: " + std::string(e.what()));
			}
		}
		// Try to add from file if path exists
		if (!data.torrentFilePath.empty() && std::filesystem::exists(data.torrentFilePath))
		{
			this->addTorrent(data.torrentFilePath, data.savePath);
		}
		else if (!data.magnetUri.empty())
		{
			this->addMagnetTorrent(data.magnetUri, data.savePath);
		}
	}
}

Result TorrentManager::removeTorrent(const lt::info_hash_t &hash, TorrentRemovalMode removeMode)
{
	std::lock_guard<std::mutex> operationLock(operationMutex);
	lt::torrent_handle handle;
	std::string torrentFilePath;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		auto it = torrents.find(hash);
		if (it == torrents.end())
			return Result::Failure("Torrent not found", ResultCode::NotFound);
		handle = it->second;
		auto pathIt = torrentFilePaths.find(hash);
		if (pathIt != torrentFilePaths.end())
			torrentFilePath = pathIt->second;
	}

	if (handle.is_valid())
	{
		std::string sourceRemovalError;
		if (removeMode == TorrentRemovalMode::DeleteSourceTorrent || removeMode == TorrentRemovalMode::DeleteDataAndSourceTorrent)
		{
			if (!torrentFilePath.empty())
			{
				try
				{
					if (std::filesystem::exists(torrentFilePath))
					{
						if (!std::filesystem::remove(torrentFilePath))
							sourceRemovalError = "the source .torrent file could not be removed";
					}
				}
				catch (const std::exception &e)
				{
					sourceRemovalError = "the source .torrent file could not be removed: " + std::string(e.what());
					Utils::Logger::warning("torrent", sourceRemovalError);
				}
			}
		}

		if (removeMode == TorrentRemovalMode::DeleteData || removeMode == TorrentRemovalMode::DeleteDataAndSourceTorrent)
			session.remove_torrent(handle, lt::session::delete_files);
		else
			session.remove_torrent(handle);

		{
			std::lock_guard<std::mutex> lock(stateMutex);
			if (torrents.erase(hash) > 0)
				++torrentCollectionRevision;
			torrentFilePaths.erase(hash);
			torrentDisplayNames.erase(hash);
		}
		markStatusCacheStale(cacheMutex, lastCacheRefresh);
		Utils::Logger::info("torrent", "Removed torrent " + hashForLog(hash));

		if (!sourceRemovalError.empty())
			return Result::Failure("Torrent removed, but " + sourceRemovalError, ResultCode::Partial);
		return Result::Success();
	}
	return Result::Failure("Torrent handle is invalid");
}

Result TorrentManager::executeCommand(const lt::info_hash_t &hash, TorrentCommand command)
{
	std::lock_guard<std::mutex> operationLock(operationMutex);
	lt::torrent_handle handle;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		const auto it = torrents.find(hash);
		if (it == torrents.end())
			return Result::Failure("Torrent not found", ResultCode::NotFound);
		handle = it->second;
	}
	if (!handle.is_valid())
		return Result::Failure("Torrent handle is invalid", ResultCode::Unavailable);

	try
	{
		switch (command)
		{
		case TorrentCommand::Pause:
			handle.set_flags(lt::torrent_flags::paused);
			break;
		case TorrentCommand::Resume:
			handle.set_flags(lt::torrent_flags::auto_managed);
			handle.unset_flags(lt::torrent_flags::paused);
			break;
		case TorrentCommand::ForceStart:
			handle.unset_flags(lt::torrent_flags::auto_managed | lt::torrent_flags::paused);
			break;
		case TorrentCommand::ForceRecheck:
			handle.force_recheck();
			break;
		case TorrentCommand::MoveQueueUp:
			handle.queue_position_up();
			break;
		case TorrentCommand::MoveQueueDown:
			handle.queue_position_down();
			break;
		case TorrentCommand::ForceReannounce:
			handle.force_reannounce();
			break;
		case TorrentCommand::EnableSequential:
			handle.set_flags(lt::torrent_flags::sequential_download);
			break;
		case TorrentCommand::DisableSequential:
			handle.unset_flags(lt::torrent_flags::sequential_download);
			break;
		}
		markStatusCacheStale(cacheMutex, lastCacheRefresh);
		return Result::Success();
	}
	catch (const std::exception &e)
	{
		Utils::Logger::error("torrent", "Torrent command failed: " + std::string(e.what()));
		return Result::Failure("Torrent command failed: " + std::string(e.what()));
	}
}

std::vector<ManagedTorrent> TorrentManager::getTorrentSnapshot() const
{
	std::vector<ManagedTorrent> snapshot;
	std::lock_guard<std::mutex> lock(stateMutex);
	snapshot.reserve(torrents.size());
	for (const auto &[hash, handle] : torrents)
	{
		ManagedTorrent entry{hash, handle, {}, {}, {}};
		auto pathIt = torrentFilePaths.find(hash);
		if (pathIt != torrentFilePaths.end())
			entry.torrentFilePath = pathIt->second;
		auto nameIt = torrentDisplayNames.find(hash);
		if (nameIt != torrentDisplayNames.end())
			entry.displayName = nameIt->second;
		snapshot.push_back(std::move(entry));
	}
	return snapshot;
}

Result TorrentManager::getPersistenceSnapshot(std::vector<ManagedTorrent> &snapshot, std::chrono::milliseconds timeout)
{
	if (shuttingDown_.load())
		return Result::Failure("Torrent manager is shutting down", ResultCode::Unavailable);

	std::lock_guard<std::mutex> operationLock(operationMutex);
	snapshot = getTorrentSnapshot();

	{
		std::lock_guard<std::mutex> lock(alertMutex_);
		resumeDataStore_.clear();
		pendingResumeHashes_.clear();
		for (const auto &torrent : snapshot)
		{
			if (!torrent.handle.is_valid())
				continue;
			try
			{
				torrent.handle.save_resume_data();
				pendingResumeHashes_.insert(torrent.hash);
			}
			catch (const std::exception &e)
			{
				Utils::Logger::warning("torrent", "Unable to request fast-resume data: " + std::string(e.what()));
			}
		}
	}

	const auto deadline = std::chrono::steady_clock::now() + timeout;
	std::unique_lock<std::mutex> lock(alertMutex_);
	alertCv_.wait_until(lock, deadline, [this] {
		return pendingResumeHashes_.empty();
	});

	for (auto &torrent : snapshot)
	{
		auto found = resumeDataStore_.find(torrent.hash);
		if (found != resumeDataStore_.end())
		{
			torrent.resumeData = found->second;
		}
	}

	if (!pendingResumeHashes_.empty())
		return Result::Failure("Timed out while collecting fast-resume data for " + std::to_string(pendingResumeHashes_.size()) + " torrent(s)", ResultCode::Storage, true);
	return Result::Success();
}

PersistedTorrent TorrentManager::toPersistedTorrent(const ManagedTorrent &torrent) const
{
	PersistedTorrent pt;
	pt.torrentFilePath = torrent.torrentFilePath;
	pt.resumeData = torrent.resumeData;
	if (torrent.handle.is_valid())
	{
		try
		{
			pt.magnetUri = lt::make_magnet_uri(torrent.handle);
			lt::torrent_status status = torrent.handle.status(lt::torrent_handle::query_save_path | lt::torrent_handle::query_name);
			pt.savePath = status.save_path;
		}
		catch (const std::exception &) {}
	}
	return pt;
}

std::vector<PersistedTorrent> TorrentManager::toPersistedTorrents(const std::vector<ManagedTorrent> &torrents) const
{
	std::vector<PersistedTorrent> result;
	result.reserve(torrents.size());
	for (const auto &torrent : torrents)
	{
		result.push_back(toPersistedTorrent(torrent));
	}
	return result;
}

Result TorrentManager::requestPersistenceSnapshot()
{
	std::lock_guard<std::mutex> lock(asyncPersistenceMutex_);
	if (shuttingDown_.load())
		return Result::Failure("Torrent manager is shutting down", ResultCode::Unavailable);

	if (asyncPersistencePending_)
		return Result::Failure("Persistence snapshot is already in progress", ResultCode::Busy, true);

	asyncPersistencePending_ = true;
	asyncPersistenceFuture_ = std::async(std::launch::async, [this]() {
		std::vector<ManagedTorrent> snapshot;
		Result res = getPersistenceSnapshot(snapshot);
		PersistenceSnapshotResult result;
		result.success = static_cast<bool>(res);
		if (result.success)
			result.torrents = toPersistedTorrents(snapshot);
		else
			result.errorMessage = res.message;
		return result;
	});
	return Result::Success();
}

std::optional<PersistenceSnapshotResult> TorrentManager::pollPersistenceSnapshot()
{
	std::lock_guard<std::mutex> lock(asyncPersistenceMutex_);
	if (!asyncPersistencePending_ || !asyncPersistenceFuture_.valid())
		return std::nullopt;

	if (asyncPersistenceFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		auto result = asyncPersistenceFuture_.get();
		asyncPersistencePending_ = false;
		return result;
	}
	return std::nullopt;
}

std::vector<TorrentEvent> TorrentManager::drainEvents()
{
	std::lock_guard<std::mutex> lock(alertMutex_);
	std::vector<TorrentEvent> events = std::move(eventsQueue_);
	eventsQueue_.clear();
	return events;
}

void TorrentManager::alertWorkerLoop()
{
	while (!stopAlertWorker_.load())
	{
		session.wait_for_alert(lt::milliseconds(100));
		std::vector<lt::alert *> alerts;
		session.pop_alerts(&alerts);
		if (alerts.empty())
			continue;

		std::lock_guard<std::mutex> lock(alertMutex_);
		for (lt::alert *alert : alerts)
		{
			if (!alert)
				continue;

			if (auto *saved = lt::alert_cast<lt::save_resume_data_alert>(alert))
			{
				const lt::info_hash_t hash = saved->handle.info_hashes();
				resumeDataStore_[hash] = lt::write_resume_data_buf(saved->params);
				pendingResumeHashes_.erase(hash);
				alertCv_.notify_all();
			}
			else if (auto *failed = lt::alert_cast<lt::save_resume_data_failed_alert>(alert))
			{
				const lt::info_hash_t hash = failed->handle.info_hashes();
				pendingResumeHashes_.erase(hash);
				alertCv_.notify_all();
				Utils::Logger::warning("torrent", "Unable to save fast-resume data: " + failed->error.message());
			}

			if (auto *metadata = lt::alert_cast<lt::metadata_received_alert>(alert))
			{
				const auto hash = metadata->handle.info_hashes();
				if (metadata->handle.is_valid())
				{
					try
					{
						const auto tf = metadata->handle.torrent_file();
						if (tf && !tf->name().empty())
						{
							std::lock_guard<std::mutex> stateLock(stateMutex);
							torrentDisplayNames[hash] = tf->name();
						}
					}
					catch (const std::exception &) {}
				}
			}

			TorrentEvent event = makeTorrentEvent(alert);
			eventsQueue_.push_back(std::move(event));
		}
	}
}

void TorrentManager::setDownloadSpeedLimit(int bytesPerSecond)
{
	lt::settings_pack settings;
	settings.set_int(lt::settings_pack::download_rate_limit, std::max(bytesPerSecond, 0));
	session.apply_settings(settings);
}

void TorrentManager::setUploadSpeedLimit(int bytesPerSecond)
{
	lt::settings_pack settings;
	settings.set_int(lt::settings_pack::upload_rate_limit, std::max(bytesPerSecond, 0));
	session.apply_settings(settings);
}

int TorrentManager::getDownloadSpeedLimit() const
{
	return session.get_settings().get_int(lt::settings_pack::download_rate_limit);
}

int TorrentManager::getUploadSpeedLimit() const
{
	return session.get_settings().get_int(lt::settings_pack::upload_rate_limit);
}

void TorrentManager::configureDiscovery(bool enableDht, bool enableUpnp, bool enableNatPmp)
{
	lt::settings_pack settings;
	settings.set_bool(lt::settings_pack::enable_dht, enableDht);
	settings.set_bool(lt::settings_pack::enable_upnp, enableUpnp);
	settings.set_bool(lt::settings_pack::enable_natpmp, enableNatPmp);
	session.apply_settings(settings);
}

std::optional<lt::torrent_status> TorrentManager::getCachedStatus(const lt::info_hash_t &hash) const
{
	auto cache = getStatusCache();
	if (cache)
	{
		auto it = cache->find(hash);
		if (it != cache->end())
		{
			return it->second;
		}
	}
	return std::nullopt;
}

std::shared_ptr<const std::unordered_map<lt::info_hash_t, lt::torrent_status>> TorrentManager::getStatusCache() const
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	return statusCache;
}

std::uint64_t TorrentManager::getStatusRevision() const
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	return statusRevision;
}

void TorrentManager::refreshStatusCache()
{
	auto newCache = std::make_shared<std::unordered_map<lt::info_hash_t, lt::torrent_status>>();
	const auto torrentsSnapshot = getTorrentSnapshot();
	bool complete = true;

	// Refresh all torrent statuses
	for (const auto &torrent : torrentsSnapshot)
	{
		if (torrent.handle.is_valid())
		{
			try
			{
				(*newCache)[torrent.hash] = torrent.handle.status();
			}
			catch (const std::exception &e)
			{
				complete = false;
				Utils::Logger::warning("torrent", "Status refresh skipped a torrent: " + std::string(e.what()));
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		// Keep the last known status for a torrent when libtorrent temporarily
		// refuses to return a status. Replacing the whole cache with a partial
		// snapshot makes existing selections disappear from the UI during a
		// transient refresh failure.
		if (!complete && statusCache)
		{
			for (const auto &torrent : torrentsSnapshot)
			{
				if (newCache->find(torrent.hash) != newCache->end())
					continue;
				const auto previous = statusCache->find(torrent.hash);
				if (previous != statusCache->end())
					(*newCache)[torrent.hash] = previous->second;
			}
		}
		statusCache = std::move(newCache);
		++statusRevision;
		// A partial refresh remains stale so the next UI refresh retries it
		// immediately instead of waiting for the normal interval.
		lastCacheRefresh = complete ? std::chrono::steady_clock::now()
			: std::chrono::steady_clock::time_point{};
	}
}

void TorrentManager::requestStatusRefresh()
{
	if (!shouldRefreshCache() || statusRefreshPending.exchange(true))
		return;
	statusWorkerCv.notify_one();
}

void TorrentManager::statusWorkerLoop()
{
	while (!stopStatusWorker.load())
	{
		std::unique_lock<std::mutex> lock(statusWorkerMutex);
		statusWorkerCv.wait(lock, [this]() { return stopStatusWorker.load() || statusRefreshPending.load(); });
		if (stopStatusWorker.load())
			break;
		statusRefreshPending = false;
		lock.unlock();
		refreshStatusCache();
	}
}

void TorrentManager::setCacheRefreshInterval(int milliseconds)
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	cacheRefreshIntervalMs = std::max(milliseconds, 16);
}

bool TorrentManager::shouldRefreshCache() const
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCacheRefresh);
	return elapsed.count() >= cacheRefreshIntervalMs;
}

void TorrentManager::requestDetailsRefresh(const lt::info_hash_t &hash, TorrentDetailSection section)
{
	const std::size_t index = detailSectionIndex(section);
	{
		std::lock_guard<std::mutex> lock(detailMutex);
		const auto last = detailLastRefresh[index].find(hash);
		const auto interval = section == TorrentDetailSection::Files
			? std::chrono::milliseconds(500)
			: section == TorrentDetailSection::Peers ? std::chrono::seconds(1) : std::chrono::seconds(5);
		if (last != detailLastRefresh[index].end() && std::chrono::steady_clock::now() - last->second < interval)
			return;
		if (pendingDetailRequests[index].insert(hash).second)
			detailRequests.push_back({hash, section});
	}
	detailCv.notify_one();
}

std::shared_ptr<const TorrentDetailsSnapshot> TorrentManager::getDetailsSnapshot(const lt::info_hash_t &hash, TorrentDetailSection section) const
{
	const std::size_t index = detailSectionIndex(section);
	std::lock_guard<std::mutex> lock(detailMutex);
	const auto it = detailCache[index].find(hash);
	return it == detailCache[index].end() ? nullptr : it->second;
}

Result TorrentManager::setFilePriority(const lt::info_hash_t &hash, int fileIndex, int priority)
{
	if (fileIndex < 0 || priority < 0 || priority > 7)
		return Result::Failure("Invalid file priority", ResultCode::InvalidInput);
	std::lock_guard<std::mutex> operationLock(operationMutex);
	lt::torrent_handle handle;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		const auto it = torrents.find(hash);
		if (it == torrents.end())
			return Result::Failure("Torrent not found", ResultCode::NotFound);
		handle = it->second;
	}
	if (!handle.is_valid())
		return Result::Failure("Torrent handle is invalid", ResultCode::Unavailable);
	try
	{
		handle.file_priority(lt::file_index_t(fileIndex), static_cast<lt::download_priority_t>(priority));
		requestDetailsRefresh(hash, TorrentDetailSection::Files);
		return Result::Success();
	}
	catch (const std::exception &e)
	{
		return Result::Failure("Unable to set file priority: " + std::string(e.what()));
	}
}

std::shared_ptr<TorrentDetailsSnapshot> TorrentManager::collectDetails(const DetailRequest &request)
{
	auto snapshot = std::make_shared<TorrentDetailsSnapshot>();
	snapshot->section = request.section;
	// Torrent operations use operationMutex before stateMutex. Keep the same
	// order here so a detail refresh cannot deadlock a concurrent command or
	// removal operation.
	std::lock_guard<std::mutex> operationLock(operationMutex);
	lt::torrent_handle handle;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		const auto it = torrents.find(request.hash);
		if (it == torrents.end())
		{
			snapshot->state = TorrentDetailState::Unavailable;
			snapshot->message = "Torrent was removed before details could be refreshed";
			return snapshot;
		}
		handle = it->second;
	}
	if (!handle.is_valid())
	{
		snapshot->state = TorrentDetailState::Unavailable;
		snapshot->message = "Torrent handle is no longer valid";
		return snapshot;
	}

	try
	{
		if (request.section == TorrentDetailSection::Files)
		{
			auto torrentFile = handle.torrent_file();
			if (!torrentFile)
			{
				snapshot->state = TorrentDetailState::Unavailable;
				snapshot->message = "Metadata not available yet";
				return snapshot;
			}
			snapshot->savePath = handle.status().save_path;
			const auto storage = torrentFile->files();
			std::vector<std::int64_t> progress;
			handle.file_progress(progress);
			constexpr int maxFiles = 10000;
			const int totalFiles = storage.num_files();
			const int count = std::min(totalFiles, maxFiles);
			snapshot->truncated = totalFiles > maxFiles;
			snapshot->files.reserve(count);
			for (int i = 0; i < count; ++i)
			{
				const lt::file_index_t index(i);
				TorrentFileSnapshot file;
				file.index = i;
				file.name = std::string(storage.file_name(index));
				file.relativePath = std::string(storage.file_path(index));
				file.size = storage.file_size(index);
				file.downloaded = i < static_cast<int>(progress.size()) ? progress[static_cast<std::size_t>(i)] : 0;
				const auto priority = handle.file_priority(index);
				file.priority = static_cast<int>(static_cast<lt::aux::underlying_index_t<
					std::remove_cv_t<decltype(priority)>>::type>(priority));
				snapshot->files.push_back(std::move(file));
			}
		}
		else if (request.section == TorrentDetailSection::Peers)
		{
			std::vector<lt::peer_info> peers;
			handle.get_peer_info(peers);
			constexpr std::size_t maxPeers = 2000;
			if (peers.size() > maxPeers)
			{
				peers.resize(maxPeers);
				snapshot->truncated = true;
			}
			snapshot->peers.reserve(peers.size());
			for (const auto &peer : peers)
			{
				TorrentPeerSnapshot item;
				item.address = peer.ip.address().to_string();
				item.client = peer.client.substr(0, 128);
				char flags[32]{};
				Utils::getPeerFlags(peer, flags, sizeof(flags));
				item.flags = flags;
				item.downloadSpeed = peer.payload_down_speed;
				item.uploadSpeed = peer.payload_up_speed;
				snapshot->peers.push_back(std::move(item));
			}
		}
		else
		{
			const auto trackers = handle.trackers();
			constexpr std::size_t maxTrackers = 2000;
			const std::size_t count = std::min(trackers.size(), maxTrackers);
			snapshot->truncated = trackers.size() > maxTrackers;
			snapshot->trackers.reserve(count);
			for (std::size_t i = 0; i < count; ++i)
				snapshot->trackers.push_back({trackers[i].url, trackers[i].verified});
		}
		snapshot->state = TorrentDetailState::Ready;
		return snapshot;
	}
	catch (const std::exception &e)
	{
		snapshot->state = TorrentDetailState::Failed;
		snapshot->message = e.what();
		Utils::Logger::warning("torrent", "Detail snapshot failed: " + std::string(e.what()));
		return snapshot;
	}
}

void TorrentManager::detailWorkerLoop()
{
	while (true)
	{
		DetailRequest request;
		{
			std::unique_lock<std::mutex> lock(detailMutex);
			detailCv.wait(lock, [this] { return stopDetailWorker.load() || !detailRequests.empty(); });
			if (detailRequests.empty() && stopDetailWorker.load())
				return;
			request = detailRequests.front();
			detailRequests.pop_front();
			pendingDetailRequests[detailSectionIndex(request.section)].erase(request.hash);
		}
		const auto snapshot = collectDetails(request);
		{
			std::lock_guard<std::mutex> lock(detailMutex);
			const auto index = detailSectionIndex(request.section);
			const auto revision = ++detailRevisions[index][request.hash];
			if (snapshot)
				snapshot->revision = revision;
			detailCache[index][request.hash] = snapshot;
			detailLastRefresh[index][request.hash] = std::chrono::steady_clock::now();
		}
	}
}



void TorrentManager::setSequentialDownload(const lt::info_hash_t &hash, bool sequential)
{
	lt::torrent_handle handle;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		auto it = torrents.find(hash);
		if (it != torrents.end())
			handle = it->second;
	}
	if (handle.is_valid())
	{
		if (sequential)
			handle.set_flags(lt::torrent_flags::sequential_download);
		else
			handle.unset_flags(lt::torrent_flags::sequential_download);
	}
}

bool TorrentManager::isSequentialDownload(const lt::info_hash_t &hash) const
{
	lt::torrent_handle handle;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		auto it = torrents.find(hash);
		if (it != torrents.end())
			handle = it->second;
	}
	if (handle.is_valid())
	{
		return (handle.flags() & lt::torrent_flags::sequential_download) != lt::torrent_flags_t{};
	}
	return false;
}

void TorrentManager::setProxyConfig(const std::string &hostname, int port, const std::string &username, const std::string &password, int proxyType)
{
	lt::settings_pack settings;
	settings.set_str(lt::settings_pack::proxy_hostname, hostname);
	settings.set_int(lt::settings_pack::proxy_port, port);
	settings.set_str(lt::settings_pack::proxy_username, username);
	settings.set_str(lt::settings_pack::proxy_password, password);
	lt::settings_pack::proxy_type_t libtorrentProxyType = lt::settings_pack::none;
	if (proxyType == 1)
		libtorrentProxyType = username.empty() ? lt::settings_pack::socks5 : lt::settings_pack::socks5_pw;
	else if (proxyType == 2)
		libtorrentProxyType = username.empty() ? lt::settings_pack::http : lt::settings_pack::http_pw;
	settings.set_int(lt::settings_pack::proxy_type, libtorrentProxyType);
	session.apply_settings(settings);
}
TorrentManager::TorrentManager()
{
	alertWorker_ = std::thread(&TorrentManager::alertWorkerLoop, this);
	statusWorker = std::thread(&TorrentManager::statusWorkerLoop, this);
	detailWorker = std::thread(&TorrentManager::detailWorkerLoop, this);
}

TorrentManager::~TorrentManager()
{
	shuttingDown_.store(true);

	stopStatusWorker = true;
	statusWorkerCv.notify_all();
	if (statusWorker.joinable())
		statusWorker.join();

	stopDetailWorker = true;
	detailCv.notify_all();
	if (detailWorker.joinable())
		detailWorker.join();

	{
		std::lock_guard<std::mutex> lock(asyncPersistenceMutex_);
		if (asyncPersistenceFuture_.valid())
			asyncPersistenceFuture_.wait();
	}

	stopAlertWorker_ = true;
	alertCv_.notify_all();
	if (alertWorker_.joinable())
		alertWorker_.join();
}
