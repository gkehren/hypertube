# Hypertube

Hypertube is a cross-platform C++20 BitTorrent desktop client using libtorrent,
cURL, nlohmann/json, and a Slint frontend. The legacy Dear ImGui/GLFW/OpenGL
frontend remains available as an opt-in migration target; see [the migration
notes](docs/slint-migration.md).

The project provides torrent file and magnet-link management, live torrent status, integrated search, favorites, history, filtering, configurable download behavior, persistent state, portable mode, and in-app diagnostics.

## Quick start

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/hypertube-slint
```

By default CMake builds Slint (`HYPERTUBE_BUILD_SLINT=ON`) and does not build
ImGui (`HYPERTUBE_BUILD_IMGUI=OFF`). To build both frontends for migration
parity checks, configure with:

```sh
cmake -S . -B build-full -DHYPERTUBE_BUILD_SLINT=ON -DHYPERTUBE_BUILD_IMGUI=ON
cmake --build build-full --target hypertube-slint hypertube-imgui slint-preview-check -j2
```

The Slint preview compiler check is also registered in CTest. Slint-only and
ImGui-only commands are documented in [the build guide](docs/build.md).

The first CMake configure may download missing dependencies. Platform package prerequisites and alternative build configurations are documented in [docs/build.md](docs/build.md).

## Documentation

The complete documentation is maintained in [`docs/`](docs/):

- [Getting started](docs/getting-started.md)
- [Features and status](docs/features.md)
- [Configuration and data](docs/configuration.md)
- [Architecture](docs/architecture.md)
- [Build system](docs/build.md)
- [Testing](docs/testing.md)
- [Build and release](docs/release.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Security and privacy](docs/security.md)
- [Contributing](docs/contributing.md)

For AI-agent operating instructions, see [AGENTS.md](AGENTS.md).

## Project status

Hypertube has production foundations and an initial UX-hardening pass. The current feature state, evidence, limitations, and planned work are maintained in [docs/features.md](docs/features.md).

## License

See the repository license file when one is added or supplied with a release artifact.
