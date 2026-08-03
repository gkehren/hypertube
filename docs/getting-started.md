# Getting started

Hypertube is a cross-platform C++17 desktop BitTorrent client built with Dear ImGui and libtorrent.

## Prerequisites

You need:

- a C++17 compiler;
- CMake 3.20 or newer;
- OpenGL and GLFW development files;
- libtorrent-rasterbar and cURL development files;
- a network connection on the first configure if CMake must download dependencies.

Platform-specific package commands are maintained in [Build](build.md).

## Build and launch

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
./build/hypertube
```

For a release-oriented local build, use `-DCMAKE_BUILD_TYPE=Release`. Run the tests before launching a new build:

```sh
ctest --test-dir build --output-on-failure
```

## Add and manage a torrent

1. Start Hypertube.
2. Use the torrent file dialog to select a `.torrent` file, or use the magnet dialog to paste a magnet URI.
3. Select the directory where data should be saved.
4. Use the torrent table to pause, resume, inspect, filter, or remove torrents.
5. Open the details view to inspect files, peers, trackers, speeds, and progress.

Torrent state and bounded fast-resume data are persisted periodically and during orderly shutdown, then restored at the next startup when the saved entries are valid.

## Search, favorites, and history

The Search view uses the active registered search provider. Search supports pagination, cancellation, history, and favorites. Selecting a result opens the save-path flow before the torrent is added.

Search failures and cancellation are reported in the UI and in the local diagnostics log. See [Troubleshooting](troubleshooting.md) if the provider cannot be reached.

Preferences can switch search to a local Jackett/Prowlarr Torznab endpoint. Enter its API key in the password field; Hypertube stores it in the operating-system credential store. If the initial Torznab request fails, search falls back to torrents-csv and records the provider failure in diagnostics.

The same Preferences dialog can route both search and BitTorrent traffic through an HTTP or SOCKS5 proxy. Proxy passwords use the same native credential store. On Linux, install `secret-tool` and unlock a Secret Service-compatible keyring before saving credentials.

## Portable mode

For a portable installation, either set the environment variable or create a marker next to the executable/current working directory:

```sh
HYPERTUBE_PORTABLE=1 ./build/hypertube
```

or:

```sh
touch portable.mode
./build/hypertube
```

Portable mode stores configuration under `./config`, runtime data under `./data`, cache files under `./cache`, and diagnostics at `./data/hypertube.log`. See [Configuration and data](configuration.md).

## User data and diagnostics

The application creates its platform-specific directories on startup. The in-app Logs window shows recent structured diagnostics. The persistent log path and recovery behavior are documented in [Configuration and data](configuration.md).
