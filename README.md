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

### Roadmap (Future Features)

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

## Configuration Files

Hypertube uses JSON configuration files stored in the `config/` directory. The configuration system supports versioning for backward compatibility and automatic migration of old configs.

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
   Ensure you have the required dependencies installed. You can use a package manager like vcpkg:
   ```sh
   vcpkg install glfw3 imgui libtorrent curl boost-system boost-filesystem
   ```

3. **Build the Project:**
   ```sh
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
   make
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