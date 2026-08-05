# Features and status

This page is an evidence-based status matrix. `Implemented` means the behavior is available through the current application flow. `Partial` means code exists but the user-facing flow, persistence, or hardening is incomplete. `Planned` is not shipped. `Known limitation` describes a current constraint rather than a missing feature.

## Current capabilities

| Area | Capability | Status | Evidence | Limitations or next step |
| --- | --- | --- | --- | --- |
| UI | Cross-platform GLFW/OpenGL Dear ImGui interface with persistent docking | Implemented | `src/ui/UIManager.cpp`, `src/ui/Theme.cpp` | Platform-specific rendering still needs release validation. |
| UI | File, magnet, and preferences keyboard shortcuts | Implemented | `UIManager::handleKeyboardShortcuts` | Shortcuts are suppressed while editing text. |
| Torrents | Add `.torrent` files | Implemented | `TorrentManager::addTorrent`, `ModalDialogs` | Requires a readable file and writable save path. |
| Torrents | Add magnet links | Implemented | `TorrentManager::addMagnetTorrent` | Peer discovery and metadata acquisition are network-dependent. |
| Torrents | BitTorrent v1/v2 identity and duplicate prevention | Implemented | `lt::info_hash_t`, `torrent_tests` | Hybrid torrents follow libtorrent identity semantics. |
| Torrents | Typed pause, resume, force-start, recheck, queue, reannounce, sequential, and remove actions | Implemented | `TorrentTableUI`, `TorrentManager` | Remove behavior depends on the selected remove mode. |
| Torrents | Progress, speed, peers, seeds, ETA, and status | Implemented | `TorrentManager` status cache, `TorrentTableUI` | Status snapshots are refreshed asynchronously at a bounded interval. |
| Torrents | File, peer, tracker, and torrent details | Implemented | `TorrentManager` detail snapshots, `TorrentDetailsUI.cpp` | Files, peers, and trackers are collected off the UI thread and capped where necessary. |
| Torrents | Open data location, copy magnet, and context actions | Implemented | `TorrentTableUI.cpp`, `SystemOpener`, `SystemUtils.cpp` | Results are drained on the UI thread; OS integration varies by platform. |
| Torrents | Sequential media preview in an external player | Implemented | `TorrentTableUI::openLargestMediaFile` | Playback depends on the external player and available pieces. |
| Torrents | Category filters | Implemented | `TorrentTableUI`, `UIManager` | Categories are based on the current libtorrent status. |
| Search | torrents-csv and configurable Torznab search | Implemented | `SearchEngine`, Preferences, `search_tests` | Jackett/Prowlarr remains an external local service. |
| Search | Pagination, deduplication, stable re-sorting, and URL-encoded parameters | Implemented | `SearchEngine::SearchResponse`, `SearchUI`, `StringUtils` | Provider response formats remain provider-specific. |
| Search | Loading, empty, cancelled, failed, and results states | Implemented | `SearchUI::State` | One active search is supported at a time. |
| Search | Cancellation, timeout, response-size limit, and TLS checks | Implemented | `SearchEngine.cpp` | Network reliability and provider rate limits remain external concerns. |
| Search | Retry, provider fallback, and bounded five-minute cache | Implemented | `SearchEngine::performSearch`, `makeHttpRequest` | A later-page Torznab error is returned instead of mixing provider pages. |
| Search | Main-thread-safe asynchronous results | Implemented | `startSearch`, `takeCompletedSearch`, `shutdown` | One active search is supported at a time. |
| Search | Search history and favorites | Implemented | `SearchEngine`, `ConfigManager` | Stored locally in the settings data. |
| Persistence | Versioned JSON settings and migrations | Implemented | `ConfigManager`, schema version 1 | Future schema changes must add migration tests. |
| Persistence | Atomic saves, `.bak` recovery, and transactional preferences | Implemented | `ConfigManager.cpp`, `UIManager.cpp`, `test_config_manager.cpp` | Preferences are committed only after the candidate is durably written; credential/runtime rollback is best-effort if restoration itself fails. |
| Persistence | Periodic torrent autosave and fast-resume data | Implemented | `App.cpp`, torrent schema version 2 | Resume data is bounded and invalid blobs fall back to magnet/torrent identity. |
| Runtime | Per-user and portable data locations | Implemented | `AppPaths.cpp` | Portable mode is based on the current working directory. |
| Diagnostics | Structured file logging and in-app recent diagnostics | Implemented | `Logger`, `LogsUI` | Log retention and export workflows are still limited. |
| Proxy | Validated SOCKS5/HTTP proxy for search and torrent traffic | Implemented | Preferences, `SearchEngine`, `TorrentManager` | End-to-end behavior depends on the configured proxy. |
| Security | Native credential storage for API keys and proxy passwords | Implemented | `CredentialStore` | Linux requires `secret-tool` and an unlocked Secret Service keyring. |

## Planned product work

The following items are product targets, not currently shipped guarantees:

- typed UI notifications and richer diagnostics export;
- bandwidth scheduling and watched folders;
- onboarding, bulk actions, richer notifications, and accessibility improvements;
- IP blocklists and detailed peer/tracker management;
- RSS feeds, profiles, and plugin support;
- theme customization, integrated playback progress, and media-preview polish.

Planned work must be moved to the current-capabilities table only after the user-facing path, error handling, persistence impact, and tests are verified.
