# Architecture

Hypertube is a single desktop process with shared torrent/search/persistence
services and two frontends during the Slint migration. `hypertube-slint` uses
the toolkit-neutral presenters and a Slint event loop; `hypertube-imgui` keeps
the legacy Dear ImGui/GLFW/OpenGL loop available for parity checks.

## Component overview

```mermaid
flowchart TD
    Main[src/main_slint.cpp] --> App[App]
    Legacy[src/main.cpp] --> ImGuiApp[ImGuiApp]
    App --> Presenters[Toolkit-neutral presenters]
    Presenters --> Slint[Slint controller and views]
    ImGuiApp --> UI[Legacy UIManager]
    App --> Config[ConfigManager]
    App --> Torrents[TorrentManager]
    App --> Search[SearchEngine]
    UI --> TorrentUI[Torrent views and dialogs]
    UI --> SearchUI[SearchUI]
    UI --> LogsUI[LogsUI]
    TorrentUI --> Torrents
    SearchUI --> Search
    Search --> Config
    Torrents --> Libtorrent[libtorrent session]
    Search --> Curl[cURL HTTP client]
    Config --> Json[nlohmann/json files]
    Config --> Logger[Logger]
    Torrents --> Logger
    Search --> Logger
    App --> Paths[AppPaths]
    Paths --> Filesystem[User or portable directories]
```

## Responsibilities

### Application lifecycle

`main_slint.cpp` initializes cURL globally, constructs `App`, binds the Slint
controller, runs the Slint event loop, and shuts services down in order.
`main.cpp` follows the same service lifecycle through `ImGuiApp` for the
transitional frontend. `App` itself is toolkit-independent: it creates runtime
directories, initializes logging, loads settings and torrents, and waits for
asynchronous persistence during shutdown.

### UI layer

`SlintAppController` owns Slint property/callback wiring and polls owned DTO
snapshots from the presenters. The specialized Slint views cover the shell,
torrent table, details, search, preferences, and diagnostics. `UIManager`
remains the legacy frame orchestrator and coordinates:

- `TorrentTableUI`: list, filtering, selection, and torrent actions;
- `TorrentDetailsUI`: files, peers, trackers, status, and settings details;
- `SearchUI`: provider search, pagination, favorites, and result selection;
- `ModalDialogs`: add/remove/save-path flows;
- `LogsUI`: libtorrent alerts and structured application diagnostics;
- `Theme`: reusable Dear ImGui styling.

Dear ImGui calls stay on the main/rendering thread. Views consume copied data or immutable snapshots instead of accessing mutable service containers directly.

### TorrentManager

`TorrentManager` owns the libtorrent session and the maps of handles and torrent file paths. It provides torrent operations, speed limits, sequential-download configuration, proxy configuration, alert polling, and cached status.

- `stateMutex` protects torrent maps and related state;
- `cacheMutex` protects the status cache;
- `getTorrentSnapshot()` provides persistence/UI-safe copies;
- status refresh is bounded by a configurable cache interval.

### SearchEngine

`SearchEngine` owns provider registration, active-provider selection, HTTP search, pagination, cancellation, history, favorites, retries, fallback, and a bounded in-memory cache. Search settings, providers, favorites, history, completions, and cache entries have separate synchronization boundaries.

Search requests are bounded by timeout and response-size limits, validate TLS peers/hosts, URL-encode query parameters, and expose cancellation to the cURL progress callback.

### ConfigManager

`ConfigManager` owns JSON schema handling, migration, atomic writes, backup recovery, favorites/history persistence, and an asynchronous save queue.

The configuration mutex protects all access to the shared JSON object. Save requests contain snapshots, so the worker never copies the live configuration while another thread mutates it.

## Startup and shutdown

```mermaid
sequenceDiagram
    participant M as main
    participant A as App
    participant P as AppPaths
    participant L as Logger
    participant C as ConfigManager
    participant T as TorrentManager
    participant S as SearchEngine
    participant U as UIManager

    M->>A: construct
    A->>P: ensureDirectories
    A->>L: initialize log path
    A->>C: load settings configuration
    A->>T: apply discovery, limits, and proxy settings
    A->>S: configure proxy and optional Torznab provider
    A->>C: load torrent configuration
    A->>T: restore valid torrent entries
    A->>S: restore favorites/history
    A->>U: initialize window and UI
    loop frames
        U->>T: read snapshots/status cache
        U->>S: submit or consume search state
    end
    U-->>A: shutdown requested
    A->>T: collect bounded fast-resume snapshots
    A->>C: enqueue torrent and settings snapshots
    A->>C: waitForAsyncOperations
    A-->>M: destroy and clean up
```

## Search flow

```mermaid
sequenceDiagram
    participant UI as SearchUI
    participant E as SearchEngine
    participant W as Search worker
    participant P as Provider

    UI->>E: startSearch(query, requestId)
    E->>W: start one active search
    W->>P: bounded HTTP/provider request
    P-->>W: response or failure
    W-->>E: publish owned CompletedSearch
    UI->>E: takeCompletedSearch()
    UI->>UI: update rendered state on main thread
```

The worker never captures UI state. `SearchUI` polls an owned completion and ignores stale request identifiers, so rendering and UI mutation remain on the main thread. Shutdown cancels and joins the worker before owners are destroyed.

## Persistence flow

```mermaid
flowchart LR
    Mutator[Setter or service snapshot] --> Lock[Acquire service/config mutex]
    Lock --> Copy[Copy immutable JSON/vector snapshot]
    Copy --> Queue[ConfigManager save queue]
    Queue --> Temp[Write and flush .tmp]
    Temp --> Backup[Copy previous target to .bak]
    Backup --> Replace[Atomically replace target]
    Replace --> Log[Log failure or recovery context]
```

Locking must remain local to the owning component. Avoid holding a service mutex while waiting on file I/O, network I/O, or another subsystem lock.

## Build target boundaries

The CMake project builds:

- `hypertube_utils`: paths, logger, credential storage, string helpers, and system helpers;
- `hypertube_config`: configuration and persistence service;
- `hypertube_torrent`: libtorrent session and torrent operations;
- `hypertube_search`: search provider and HTTP service;
- `hypertube`: UI and application executable;
- `unit_tests`, `config_tests`, `search_tests`, and `torrent_tests`: GoogleTest executables.

New services should be isolated behind a small library when they need independent tests. UI code should depend on service interfaces and snapshots, not implementation details of persistence or libtorrent internals.
