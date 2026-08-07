# Features and status

This page is an evidence-based status matrix. `Implemented` means the behavior
is available through the current application flow. `Partial` means code exists
but the user-facing flow, persistence, or hardening is incomplete.

## Current capabilities

| Area | Capability | Status | Evidence | Limitations or next step |
| --- | --- | --- | --- | --- |
| UI | Slint shell, torrent table, selection, sorting, details, search, preferences, and logs | Implemented | `src/ui/slint/SlintAppController.cpp`, `ui/` | Screen-reader and platform runtime validation remain ongoing. |
| UI | File, magnet, preferences, and search keyboard shortcuts | Implemented | Slint callbacks and `SlintAppController` | Shortcuts are suppressed while editing text. |
| Torrents | Add `.torrent` files and magnet links | Implemented | `TorrentManager`, `DialogService` | Metadata acquisition is network-dependent for magnets. |
| Torrents | BitTorrent v1/v2 identity and duplicate prevention | Implemented | `TorrentManager`, `torrent_tests` | Hybrid torrents follow libtorrent identity semantics. |
| Torrents | Pause, resume, force-start, recheck, queue, reannounce, sequential, file priority, speed limits, and removal | Implemented | `SlintAppController`, `TorrentManager` | Removal behavior depends on the selected mode. |
| Torrents | Progress, speed, peers, seeds, ETA, status, files, trackers, and details | Implemented | Status and detail snapshots | External tracker and peer behavior varies by torrent. |
| Torrents | Open data location, copy magnet, media preview, and context actions | Implemented | `SystemOpener`, `SystemUtils`, Slint actions | OS integration varies by platform. |
| Torrents | Category filters | Implemented | Slint category model and `TorrentManager` | Categories are based on current torrent status. |
| Search | torrents-csv and configurable Torznab search | Implemented | `SearchEngine`, Preferences, `search_tests` | Jackett/Prowlarr remains an external local service. |
| Search | Pagination, deduplication, stable sorting, URL encoding, cancellation, and history | Implemented | `SearchEngine`, Slint search models | One active search is supported at a time. |
| Search | Favorites and Add/Download actions | Implemented | Search controllers and persistence | Stored locally in settings data. |
| Persistence | Versioned JSON settings, migrations, atomic saves, and backup recovery | Implemented | `ConfigManager`, `config_tests` | Future schema changes require migration tests. |
| Persistence | Periodic torrent, search, favorites, history, settings, and UI-state saves | Implemented | `App`, Slint timers, persistence controllers | Resume data is bounded. |
| Runtime | Per-user and portable data locations | Implemented | `AppPaths` | Portable mode uses a marker beside the executable. |
| Diagnostics | Structured file logging and in-app recent diagnostics | Implemented | `Logger`, Slint Logs view | Retention and export workflows remain limited. |
| Proxy | Validated SOCKS5/HTTP proxy for search and torrent traffic | Implemented | Preferences, `SearchEngine`, `TorrentManager` | End-to-end behavior depends on the configured proxy. |
| Security | Native credential storage for API keys and proxy passwords | Implemented | `CredentialStore` | Linux requires an unlocked Secret Service keyring. |

## Planned product work

- typed UI notifications and richer diagnostics export;
- bandwidth scheduling and watched folders;
- onboarding, bulk actions, accessibility improvements, and richer notifications;
- IP blocklists and detailed peer/tracker management;
- RSS feeds, profiles, and plugin support;
- theme customization and media-preview polish.

Planned work should move to the current-capabilities table only after the
user-facing path, error handling, persistence impact, and tests are verified.
