# Build system

The application and all previews use Slint's `fluent-dark` widget style by
default. This keeps standard controls consistent with Hypertube's dark runtime
themes. Pass `-DSLINT_STYLE=<style>` at configure time to test another style;
the same cached value is used by preview compilation.

Hypertube uses CMake 3.21 or newer and requires C++20. The project prefers
system or package-manager dependencies and falls back to CMake `FetchContent`
when a dependency is not found.

## Dependencies

The build searches for nlohmann/json, libtorrent-rasterbar, cURL, and Slint.
Slint 1.17.1 requires Rust 1.88 or newer for its build and Fontconfig
development files on Linux for the software renderer. Slint is the only
frontend and is always configured.

When a package is not available locally, CMake can download a pinned dependency
revision where configured. The first configure may therefore require network
access and additional build time.

## Linux

Ubuntu/Debian:

```sh
sudo apt update
sudo apt install -y cmake build-essential libtorrent-rasterbar-dev \
  libcurl4-openssl-dev libfontconfig1-dev libxcb-shape0-dev \
  libxcb-xfixes0-dev libxkbcommon-dev libxkbcommon-x11-dev \
  libwayland-dev libx11-xcb-dev libx11-dev libxi-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev
```

Arch Linux:

```sh
sudo pacman -S cmake base-devel libtorrent-rasterbar curl fontconfig \
  libx11 libxcursor libxi libxrandr libxinerama libxkbcommon wayland
```

Fedora:

```sh
sudo dnf install cmake gcc-c++ libtorrent-rasterbar-devel libcurl-devel \
  fontconfig-devel libX11-devel libXcursor-devel libXi-devel \
  libXrandr-devel libXinerama-devel libxkbcommon-devel wayland-devel
```

## macOS and Windows

macOS with Homebrew:

```sh
brew install cmake libtorrent-rasterbar curl nlohmann-json
```

Windows with vcpkg:

```cmd
vcpkg install libtorrent curl nlohmann-json
cmake -S . -B build ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Debug and release builds

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

For a release build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j2
```

Release builds enable optimization and LTO but target the compiler's portable
baseline ISA so packaged binaries can run on a different machine. Host-specific
optimizations are development-only and must never be used for distributed
packages:

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release \
  -DHYPERTUBE_ENABLE_NATIVE_OPTIMIZATIONS=ON
```

This opt-in currently enables `/arch:AVX2` with MSVC and `-march=native` with
GCC or Clang. Use Debug for diagnosis and test failures.

## Slint renderers

The production build compiles and uses Slint's software renderer. FemtoVG is
available only for an explicit comparison build, so enabling the benchmark
cannot silently change release packaging:

```sh
cmake -S . -B build-renderers -DCMAKE_BUILD_TYPE=Release \
  -DHYPERTUBE_ENABLE_SLINT_GPU_BENCHMARK=ON
cmake --build build-renderers --target slint-renderer-comparison -j2
```

The comparison writes JSON reports under `build-renderers/renderer-reports`.
It measures redraw-cycle latency, process CPU time, peak resident memory, and
event-loop stability. Run it on representative hardware;
headless CI exercises FemtoVG through Mesa and proves backend stability, but is
not evidence that a hardware GPU is faster on end-user systems.

## Offline configure

Only use disconnected mode when all FetchContent sources are already present:

```sh
cmake -S . -B build -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

If this fails because a dependency is missing, reconfigure with network access
or install the dependency through the platform package manager. Do not commit
`_deps` or other generated dependency trees.

## Sanitizers

AddressSanitizer and UndefinedBehaviorSanitizer are available on non-MSVC
toolchains:

```sh
cmake -S . -B build-asan \
  -DHYPERTUBE_ENABLE_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan -j2
ctest --test-dir build-asan --output-on-failure
```

## Build outputs and packaging

The executable is `build/hypertube` for single-config generators or
`build/Release/hypertube.exe` for a multi-config Windows build.

Install the portable runtime component without requiring administrator access:

```sh
cmake --install build-release --prefix dist/hypertube --component runtime
touch dist/hypertube/portable.mode
```

The runtime component contains `hypertube`, required runtime libraries, and the
seed `config/` directory. Generate a ZIP package with:

```sh
cpack --config build-release/CPackConfig.cmake -G ZIP
```

## Common configure failures

| Symptom | Likely cause | Action |
| --- | --- | --- |
| FetchContent cannot clone/download | Network or proxy restriction | Install the dependency locally or configure with network access. |
| Libtorrent target mismatch | Incompatible package config | Check the discovered CMake target and package version. |
| Fontconfig target not found on Linux | Missing development package | Install the platform Fontconfig development package. |
| Build succeeds but the window fails | Runtime display/backend issue | Run `hypertube` from a terminal and inspect its diagnostics. |

For a normal change, run the full build, CTest, and `git diff --check`.
