# Build and release

This document describes the release workflow that exists today and separates it
from automation that is still planned.

## Current release facts

- The project version used by CPack is currently `0.1.0` in `CMakeLists.txt`.
- The configured package generator is ZIP.
- The installable runtime component contains `hypertube`, its required runtime
  libraries, and the seed `config/` directory.
- There is currently no repository release publication workflow.

## Licensing and notices

The repository does not currently contain a project license or third-party
notice file. Before publishing a binary, choose the applicable Slint license
regime, add the project license and required third-party notices, and record
that decision in the release materials. Until this gate is complete, builds
and packages are for local development and testing only.

## Prepare a release build

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j2
ctest --test-dir build-release --output-on-failure
```

Do not enable `HYPERTUBE_ENABLE_NATIVE_OPTIMIZATIONS` for this build. The
default Release configuration deliberately omits `/arch:AVX2` and
`-march=native` so the artifact is not tied to the packaging host's CPU.

## Create a portable runtime directory

```sh
cmake --install build-release \
  --prefix dist/hypertube \
  --component runtime
touch dist/hypertube/portable.mode
```

The resulting layout should be:

```text
dist/hypertube/
├── hypertube
└── config/
    └── settings.json
```

Runtime data and logs are created under `data/` when the bundle first runs. Do
not place a user's `torrents.json` in the release artifact.

## Generate the ZIP package

```sh
cpack --config build-release/CPackConfig.cmake -G ZIP
```

Inspect the archive before publication and confirm that it contains the
executable, required runtime libraries, and only seed configuration files.

## Manual release checklist

- [ ] Update the project version intentionally.
- [ ] Update [Features and status](features.md) for changed capabilities.
- [ ] Update [Configuration and data](configuration.md) for schema changes.
- [ ] Run a clean Release configure and build.
- [ ] Confirm `HYPERTUBE_ENABLE_NATIVE_OPTIMIZATIONS=OFF` in the release cache.
- [ ] Run CTest and relevant sanitizer tests.
- [ ] Review recent software/FemtoVG reports before changing the production renderer.
- [ ] Validate the non-root `runtime` install.
- [ ] Inspect the ZIP contents.
- [ ] Run the packaged executable in portable mode.
- [ ] Verify startup, torrent add, search, persistence, and diagnostics.
- [ ] Record platform, compiler, dependency, and test results.
- [ ] Resolve licensing and third-party notice requirements before publication.
