# Hypertube a BitTorrent Client

A lightweight and fast cross-platform BitTorrent client built with C++ and Dear ImGui. This project aims to provide basic BitTorrent functionalities, a user-friendly interface.

## Features

### Already Implemented
- Cross-platform GUI with Dear ImGui and docking support
- Torrent file loading and parsing
- Magnet link support
- Basic torrent management (add, remove, pause, resume)
- Real-time torrent status display (progress, speeds, peers, seeds)
- Peer information viewing
- File details and progress tracking
- Tracker information display
- Context menu operations (open folder, copy magnet URI, queue management)
- File dialog integration for torrent/save path selection
- Configuration management with JSON
- Automatic torrent state persistence
- Integrated torrent search using torrents-csv.com API
- Search history and favorites management
- Thread-safe torrent snapshots for rendering and persistence
- Atomic JSON saves with `.bak` recovery files
- Per-user configuration/data/cache directories with portable mode
- Local structured diagnostics and an in-app diagnostics view
- Pluggable search providers with cancellation and bounded HTTP responses
- Real category filtering (all, downloading, seeding, completed, paused, active, inactive)

### Product roadmap

The current branch contains the production foundations and the first UX hardening pass. The remaining product work is intentionally staged:

- Phase 1: service-level torrent commands, typed events/errors, scheduler, watched folders, and broader persistence migrations.
- Phase 2: onboarding, bulk actions, richer notifications, accessibility, provider configuration, and search caching.
- Phase 3: proxy/blocklist/tracker tools, streaming polish, profiles, and additional providers.

The following capabilities are already present in the codebase and will receive further UX polish:

**High Priority (Core Experience & Convenience)**
- **Sequential Download (Streaming):** Prioritize the first pieces of media files to allow playback while downloading.
- **Media Preview Integration:** Button to launch the default media player for the file currently being streamed.
- **File Priority Control:** Ability to set priority (Skip, Low, Normal, High) for individual files within a torrent.
- **Magnet Link Association:** Integration with the OS to open magnet links directly with Hypertube.

**Medium Priority (Enhancements)**
- **RSS Feed Support:** Automatically download torrents from RSS feeds with basic filtering.
- **IP Filtering / Blocklists:** Support for loading IP blocklists (e.g., eMule format) to block bad peers.
- **Proxy / VPN Configuration:** SOCKS5/HTTP proxy support for privacy.
- **Detailed Peer/Tracker Management:** Manually ban peers or add/remove trackers from the UI.

**Low Priority (Advanced Features)**
- **Web UI / Remote Control:** A web interface for managing the client remotely.
- **Plugin System:** Support for Lua or Python scripts to extend functionality.
- **Scheduling:** Bandwidth scheduler to limit speeds at certain times of day.
- **Theme Customization:** User-selectable themes (Dark/Light) and font scaling.

## Dependencies

- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI library.
- [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog) - File dialog extension for ImGui.
- [libtorrent](https://github.com/arvidn/libtorrent) - BitTorrent library.
- [nlohmann/json](https://github.com/nlohmann/json) - JSON library for configuration management.
- [GLFW](https://www.glfw.org/) - OpenGL framework for window management.
- [OpenGL](https://www.opengl.org/) - Graphics rendering.

## Project Structure

/project-root
|-- /src # Source files
|-- /include # Header files
|-- /config # Configuration files
| |-- settings.json # User settings (versioned schema)
| |-- torrents.json # Torrent state
|-- /build # Build directory
|-- README.md # Project README file
|-- CMakeLists.txt # CMake build configuration

## Configuration and data locations

Hypertube uses versioned JSON files and writes them atomically. A failed write leaves the previous file available as `<file>.bak` and startup tries that backup when the primary file is missing or invalid.

By default, state is stored in the platform user directory:

- Linux: `$XDG_CONFIG_HOME/hypertube` (or `~/.config/hypertube`), with data in `$XDG_DATA_HOME/hypertube` (or `~/.local/share/hypertube`).
- macOS: `~/Library/Application Support/Hypertube` and `~/Library/Caches/Hypertube`.
- Windows: `%APPDATA%/Hypertube` and `%LOCALAPPDATA%/Hypertube`.

For a portable binary, set `HYPERTUBE_PORTABLE=1` or create a `portable.mode` marker in the working directory. Configuration then stays under `./config`, data under `./data`, and diagnostics under `./data/hypertube.log`.

### settings.json

The main configuration file for application settings. It uses a versioned schema to ensure backward compatibility.

**Schema Version 1:**

```json
{
  "version": 1,
  "settings": {
    "speed_limits": {
      "download": 0,
      "upload": 0
    },
    "download_path": "~/Downloads",
    "enable_dht": true,
    "enable_upnp": true,
    "enable_natpmp": true
  }
}
```

**Configuration Options:**

- `version` (integer): Schema version for config format. Current version is 1.
  - The application automatically migrates older config formats to the latest version.

- `settings.speed_limits` (object): Bandwidth limits in bytes per second.
  - `download` (integer): Maximum download speed in bytes/second. 0 means unlimited.
  - `upload` (integer): Maximum upload speed in bytes/second. 0 means unlimited.

- `settings.download_path` (string): Default directory for saving downloaded files.
  - Default: `~/Downloads`
  - Supports tilde (~) expansion for home directory.

- `settings.enable_dht` (boolean): Enable Distributed Hash Table (DHT) for trackerless torrents.
  - Default: `true`
  - DHT helps find peers without relying on trackers.

- `settings.enable_upnp` (boolean): Enable UPnP port mapping for automatic NAT traversal.
  - Default: `true`
  - Automatically forwards ports on UPnP-enabled routers.

- `settings.enable_natpmp` (boolean): Enable NAT-PMP port mapping (alternative to UPnP).
  - Default: `true`
  - Used by some routers as an alternative to UPnP.

**Config Migration:**

The ConfigManager automatically handles migration from older config formats:
- Unversioned configs (version 0) are migrated to version 1
- Old settings are preserved during migration
- Missing settings are filled with defaults
- If no config file exists, a default one is generated automatically

### torrents.json

Stores the state of active torrents to restore them when the application restarts.

```json
{
  "torrents": [
    {
      "magnet_uri": "magnet:?xt=urn:btih:...",
      "save_path": "/path/to/downloads",
      "torrent_path": "/path/to/torrent/file.torrent"
    }
  ]
}
```

**Fields:**

- `magnet_uri` (string): Magnet link for the torrent.
- `save_path` (string): Directory where torrent files are being saved.
- `torrent_path` (string, optional): Path to the .torrent file if one was used.

## Building the Project

1. **Clone the Repository:**
   ```sh
   git clone https://github.com/gkehren/hypertube.git
   cd hypertube
   ```

2. **Install Dependencies:**

   Hypertube is multiplatform and requires C++17 support. If any optional dependency is not found locally, CMake will automatically download and build it using `FetchContent`.

   - **Linux (Ubuntu / Debian):**
     ```sh
     sudo apt update
     sudo apt install -y cmake build-essential libtorrent-rasterbar-dev libcurl4-openssl-dev libgl1-mesa-dev libglfw3-dev libx11-dev
     ```

   - **Linux (Arch Linux):**
     ```sh
     sudo pacman -S cmake base-devel libtorrent-rasterbar curl glfw-x11 mesa
     ```

   - **Linux (Fedora):**
     ```sh
     sudo dnf install cmake gcc-c++ libtorrent-rasterbar-devel libcurl-devel glfw-devel mesa-libGL-devel
     ```

   - **macOS (Homebrew):**
     ```sh
     brew install cmake glfw libtorrent-rasterbar curl nlohmann-json
     ```

   - **Windows (vcpkg):**
     ```cmd
     vcpkg install glfw3 imgui libtorrent curl nlohmann-json
     ```

3. **Build the Project:**

   - **Linux / macOS:**
     ```sh
     cmake -B build -DCMAKE_BUILD_TYPE=Release
     cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
     ```

   - **Windows (with vcpkg):**
     ```cmd
     cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
     cmake --build build --config Release
     ```

4. **Run Tests:**
   ```sh
   ctest --test-dir build --output-on-failure
   ```

5. **Build with sanitizers (recommended during development):**
   ```sh
   cmake -B build-asan -DHYPERTUBE_ENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-asan -j2
   ctest --test-dir build-asan --output-on-failure
   ```

6. **Create a portable ZIP:**
   ```sh
   cmake --install build --prefix dist/hypertube --component runtime
   (cd dist/hypertube && touch portable.mode)
   ```

## Usage

1. **Run the Application:**
   ```sh
   ./hypertube
   ```

2. **Add a Torrent:**
   Use the UI to add a torrent file or magnet link.

3. **Manage Torrents:**
   - Start, pause, resume, or stop torrents.
   - View detailed information about each torrent.
   - Adjust settings such as download/upload speeds and enable/disable DHT.

## Key Components

### UI with Dear ImGui
The user interface is designed using Dear ImGui, providing a lightweight and efficient way to create a user-friendly interface for managing torrents.

### BitTorrent Operations with libtorrent
Libtorrent is used to handle all BitTorrent protocol operations, including peer connections, data transfer, and tracker communication.

### Configuration Management
User settings and torrent states are managed using JSON files. This allows for easy reading and writing of configuration data.

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request for any improvements or bug fixes.
