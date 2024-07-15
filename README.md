# Hypertube a BitTorrent Client

A lightweight and fast cross-platform BitTorrent client built with C++ and Dear ImGui. This project aims to provide basic BitTorrent functionalities, a user-friendly interface.

## Features

### Must Have
- Support for basic BitTorrent operations (download, upload, pause, resume, stop torrents)
- Cross-platform compatibility (Windows, Linux, MacOS)
- User-friendly GUI with Dear ImGui
- Magnet link support
- Peer discovery and connection management
- Tracker communication (HTTP, UDP)
- Torrent file creation and parsing

### Should Have
- DHT (Distributed Hash Table) support for decentralized peer discovery
- UPnP/NAT-PMP for port forwarding
- Encryption support for peer connections
- Bandwidth management and scheduling
- Logging of torrent activities

### Could Have
- RSS feed support for automatic downloading
- Integrated search functionality
- Web interface for remote management
- Customizable UI themes

## Dependencies

- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI library.
- [libtorrent](https://github.com/arvidn/libtorrent) - BitTorrent library.
- [Boost](https://www.boost.org/) - Libraries for C++ including Boost.Asio used by libtorrent.
- [nlohmann/json](https://github.com/nlohmann/json) - JSON library for configuration management.

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
   vcpkg install imgui libtorrent boost-json
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

## Development Roadmap

### Milestone 1: Project Setup and Initial UI
- Setup development environment and project structure.
- Create basic UI with Dear ImGui.

### Milestone 2: Basic BitTorrent Operations
- Implement and test basic BitTorrent operations with libtorrent.

### Milestone 3: Configuration Management
- Implement loading and saving of configuration data using JSON.

### Milestone 4: Advanced Features and Optimization
- Implement additional features (e.g., DHT support, bandwidth management).
- Optimize performance and test on all platforms.

### Milestone 5: Final Testing and Documentation
- Conduct thorough testing.
- Prepare documentation and user guide.

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request for any improvements or bug fixes.

## Acknowledgements

- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI library.
- [libtorrent](https://github.com/arvidn/libtorrent) - BitTorrent library.
- [Boost](https://www.boost.org/) - C++ libraries.
- [nlohmann/json](https://github.com/nlohmann/json) - JSON library for configuration management.
