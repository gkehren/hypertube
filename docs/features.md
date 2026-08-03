# Features and status

This page is an evidence-based status matrix. `Implemented` means the behavior is available through the current application flow. `Partial` means code exists but the user-facing flow, persistence, or hardening is incomplete. `Planned` is not shipped. `Known limitation` describes a current constraint rather than a missing feature.

## Current capabilities

| Area | Capability | Status | Evidence | Limitations or next step |
| --- | --- | --- | --- | --- |
| UI | Cross-platform GLFW/OpenGL Dear ImGui interface with docking | Implemented | `src/ui/UIManager.cpp`, `src/ui/Theme.cpp` | Platform-specific rendering still needs release validation. |
| Torrents | Add `.torrent` files | Implemented | `TorrentManager::addTorrent`, `ModalDialogs` | Requires a readable file and writable save path. |
| Torrents | Add magnet links | Implemented | `TorrentManager::addMagnetTorrent` | Peer discovery and metadata acquisition are network-dependent. |
| Torrents | Pause, resume, remove, and queue actions | Implemented | `TorrentTableUI`, `TorrentManager` | Remove behavior depends on the selected remove mode. |
| Torrents | Progress, speed, peers, seeds, ETA, and status | Implemented | `TorrentManager` status cache, `TorrentTableUI` | Status is refreshed from a bounded UI cache interval. |
| Torrents | File, peer, tracker, and torrent details | Implemented | `TorrentDetailsUI.cpp` | Large torrent detail sets need continued UI performance review. |
| Torrents | Open data location, copy magnet, and context actions | Implemented | `TorrentTableUI.cpp`, `SystemUtils.cpp` | OS integration varies by platform. |
| Torrents | Category filters | Implemented | `TorrentTableUI`, `UIManager` | Categories are based on the current libtorrent status. |
| Search | Provider-based torrent search | Implemented | `SearchEngine`, `SearchUI` | The default provider is external and can change availability. |
| Search | Pagination and URL-encoded query/token parameters | Implemented | `SearchEngine::SearchResponse`, `StringUtils` | Provider response formats remain provider-specific. |
| Search | Cancellation, timeout, response-size limit, and TLS checks | Implemented | `SearchEngine.cpp` | Network reliability and provider rate limits remain external concerns. |
| Search | Search history and favorites | Implemented | `SearchEngine`, `ConfigManager` | Stored locally in the settings data. |
| Persistence | Versioned JSON settings and migrations | Implemented | `ConfigManager`, schema version 1 | Future schema changes must add migration tests. |
| Persistence | Atomic saves and `.bak` recovery | Implemented | `ConfigManager.cpp`, `test_config_manager.cpp` | Recovery is best-effort when both candidates are unusable. |
| Runtime | Per-user and portable data locations | Implemented | `AppPaths.cpp` | Portable mode is based on the current working directory. |
| Diagnostics | Structured file logging and in-app recent diagnostics | Implemented | `Logger`, `LogsUI` | Log retention and export workflows are still limited. |
| Streaming | Sequential download API | Partial | `TorrentManager::setSequentialDownload` | A complete user-facing streaming workflow is still required. |
| Proxy | Proxy configuration API | Partial | `TorrentManager::setProxyConfig` | Preferences UI, secure credential storage, and end-to-end validation are incomplete. |

## Planned product work

The following items are product targets, not currently shipped guarantees:

- service-level torrent commands and typed events/errors;
- bandwidth scheduling and watched folders;
- onboarding, bulk actions, richer notifications, and accessibility improvements;
- provider configuration and search caching;
- IP blocklists and detailed peer/tracker management;
- proxy/VPN UX and secure credential handling;
- RSS feeds, profiles, web/remote control, and plugin support;
- theme customization and media-preview polish.

Planned work must be moved to the current-capabilities table only after the user-facing path, error handling, persistence impact, and tests are verified.
