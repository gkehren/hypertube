# Getting started

Hypertube is a cross-platform C++20 desktop BitTorrent client with a Slint
desktop interface.

## Prerequisites

You need:

- a C++20 compiler;
- CMake 3.21 or newer;
- Rust 1.88 or newer and Fontconfig development files on Linux;
- libtorrent-rasterbar and cURL development files;
- a network connection on the first configure if CMake downloads dependencies.

Platform-specific package commands are maintained in [Build](build.md).

## Build and launch

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
./build/hypertube
```

Run the tests before launching a new build:

```sh
ctest --test-dir build --output-on-failure
```

## Add and manage a torrent

1. Start Hypertube.
2. Use the torrent file dialog to select a `.torrent` file, or use the magnet dialog to paste a magnet URI.
3. Select the directory where data should be saved.
4. Use the torrent table to pause, resume, inspect, filter, or remove torrents.
5. Open the details view to inspect files, peers, trackers, speeds, and progress.

Torrent state and bounded fast-resume data are persisted periodically and during
orderly shutdown, then restored at the next startup when the saved entries are
valid.

## Search, favorites, and history

The Search view uses the active registered search provider. Search supports
pagination, cancellation, history, and favorites. Selecting a result opens the
save-path flow before the torrent is added.

Preferences can switch search to a local Jackett/Prowlarr Torznab endpoint or
route search and BitTorrent traffic through an HTTP or SOCKS5 proxy. Secrets
are stored in the operating-system credential store and are never displayed in
plain text.

## Portable mode

Set the environment variable or create a marker next to the executable:

```sh
HYPERTUBE_PORTABLE=1 ./build/hypertube
```

or:

```sh
touch build/portable.mode
./build/hypertube
```

Portable mode stores configuration under the executable directory's `config`,
runtime data under `data`, cache files under `cache`, and diagnostics at
`data/hypertube.log`.

## User data and diagnostics

The application creates its platform-specific directories on startup. The
in-app Logs view shows recent structured diagnostics. The persistent log path
and recovery behavior are documented in [Configuration and data](configuration.md).
