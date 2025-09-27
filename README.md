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
| |-- config.json # User settings
| |-- torrents.json # Torrent state
|-- /build # Build directory
|-- README.md # Project README file
|-- CMakeLists.txt # CMake build configuration

## Configuration Files

### config.json
```json
{
  "download_path": "/path/to/downloads",
  "max_download_speed": 1000,
  "max_upload_speed": 500,
  "enable_dht": true
}
```

### torrents.json
```json
[
  {
    "info_hash": "abcdef1234567890",
    "name": "Example Torrent",
    "status": "Downloading",
    "downloaded": 1024,
    "uploaded": 512,
    "peers": 10
  }
]
```

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