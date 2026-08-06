# Build system

Hypertube uses CMake 3.21 or newer and requires C++20. The project prefers
system or package-manager dependencies and falls back to CMake `FetchContent`
when a dependency is not found.

## Dependency resolution

The build searches for the shared dependencies:

- nlohmann/json;
- libtorrent-rasterbar;
- cURL.

The default configuration is `HYPERTUBE_BUILD_SLINT=ON` and
`HYPERTUBE_BUILD_IMGUI=OFF`. Slint additionally requires Rust 1.88 or newer
for the pinned Slint 1.16.1 build and Fontconfig development files on Linux
for its software renderer. ImGui additionally requires OpenGL, GLFW3, Dear
ImGui, ImGuiFileDialog, and X11 on Linux when available. These frontend-only
dependencies are not discovered when the corresponding option is disabled.

When a package is not available locally, CMake can download a pinned dependency revision where configured. The first configure may therefore require network access and additional build time.

## Linux dependencies

Ubuntu/Debian:

```sh
sudo apt update
sudo apt install -y cmake build-essential libtorrent-rasterbar-dev \
  libcurl4-openssl-dev libfontconfig1-dev
```

Arch Linux:

```sh
sudo pacman -S cmake base-devel libtorrent-rasterbar curl glfw-x11 mesa
```

Fedora:

```sh
sudo dnf install cmake gcc-c++ libtorrent-rasterbar-devel libcurl-devel \
  fontconfig-devel glfw-devel mesa-libGL-devel
```

Enable `HYPERTUBE_BUILD_IMGUI=ON` when a legacy ImGui build is required. If a
distribution package does not provide a compatible ImGui or ImGuiFileDialog
target, the CMake fallback may fetch it.

## macOS and Windows dependencies

macOS with Homebrew:

```sh
brew install cmake glfw libtorrent-rasterbar curl nlohmann-json
```

Windows with vcpkg:

```cmd
vcpkg install libtorrent curl nlohmann-json
# Only needed for the legacy frontend:
vcpkg install glfw3
```

Configure Windows with the vcpkg toolchain:

```cmd
cmake -S . -B build ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DHYPERTUBE_BUILD_SLINT=ON -DHYPERTUBE_BUILD_IMGUI=OFF ^
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The exact runtime OpenGL/GLFW setup remains platform-dependent and must be smoke-tested on the target operating system.

## Debug build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
```

Frontend-specific configurations:

```sh
# Slint only (default)
cmake -S . -B build-slint-only -DHYPERTUBE_BUILD_SLINT=ON -DHYPERTUBE_BUILD_IMGUI=OFF
cmake --build build-slint-only --target hypertube-slint slint-preview-check -j2

# Legacy ImGui only
cmake -S . -B build-imgui-only -DHYPERTUBE_BUILD_SLINT=OFF -DHYPERTUBE_BUILD_IMGUI=ON
cmake --build build-imgui-only --target hypertube-imgui -j2

# Both frontends
cmake -S . -B build-full -DHYPERTUBE_BUILD_SLINT=ON -DHYPERTUBE_BUILD_IMGUI=ON
cmake --build build-full --target hypertube-slint hypertube-imgui slint-preview-check -j2
```

Use Debug for normal development, parser diagnosis, race investigation, and test failures.

## Release build

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j2
```

Non-MSVC Release builds enable optimization, LTO, and platform-specific tuning. Do not use a Release failure to diagnose a problem before reproducing it in Debug.

## Offline or disconnected configure

Only use disconnected mode when all FetchContent sources are already present in the CMake cache:

```sh
cmake -S . -B build -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

If disconnected configuration fails because a dependency is missing, reconfigure with network access or install the dependency through the platform package manager. Do not commit `_deps` or other generated dependency trees.

## Sanitizers

AddressSanitizer and UndefinedBehaviorSanitizer are available on non-MSVC toolchains:

```sh
cmake -S . -B build-asan \
  -DHYPERTUBE_ENABLE_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan -j2
ctest --test-dir build-asan --output-on-failure
```

Use this configuration for changes involving concurrency, ownership, parsing, callbacks, filesystem recovery, or external library boundaries.

## Build outputs

Single-config builds produce `build/hypertube-slint` and the transitional
`build/hypertube-imgui`. Multi-config generators place them under the selected
configuration directory, for example `build/Release/hypertube-slint.exe`.

Build directories, generated CPack output, downloaded dependencies, and runtime data must remain untracked.

## Common configure failures

| Symptom | Likely cause | Action |
| --- | --- | --- |
| OpenGL package not found | Missing graphics development package | Install the platform OpenGL/Mesa development package. |
| GLFW target not found | Missing package or incomplete vcpkg setup | Check package-manager installation and toolchain path. |
| FetchContent cannot clone/download | Network or proxy restriction | Install the dependency locally or configure with network access. |
| Libtorrent target mismatch | Incompatible package config | Check the discovered CMake target and package version. |
| Build succeeds but the window fails | Runtime display/backend issue | Run `hypertube-slint` from a terminal first; if testing the legacy frontend, inspect GLFW/OpenGL diagnostics. |

## Build validation

For a normal change:

```sh
cmake --build build -j2
ctest --test-dir build --output-on-failure
git diff --check
```

`slint-preview-check` is registered as a CTest test whenever Slint is enabled,
so `ctest` validates the preview sources as part of the configured frontend.

For CMake or packaging changes, also follow [Release](release.md) and verify a non-root runtime install.
