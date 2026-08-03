# Build and release

This document describes the release workflow that exists today and separates it from the automation that is still planned.

## Current release facts

- The project version used by CPack is currently `0.1.0` in `CMakeLists.txt`.
- The configured package generator is ZIP.
- The installable runtime component contains the `hypertube` executable and seed `config/` directory.
- There is currently no repository CI workflow, release tag policy, checksum generation, or automated publication workflow.

## Prepare a release build

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j2
ctest --test-dir build-release --output-on-failure
```

For concurrency, persistence, or parser changes, validate the Debug sanitizer configuration as described in [Testing](testing.md).

## Create a portable runtime directory

Use the runtime component so dependency install rules are not included:

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

Runtime data and logs are created under `data/` when the bundle is first run. Do not place a user's `torrents.json` in the release artifact.

## Generate the ZIP package

```sh
cpack --config build-release/CPackConfig.cmake -G ZIP
```

Inspect the archive before publication:

```sh
unzip -l Hypertube-0.1.0-Linux.zip
```

The exact archive name is generator- and platform-dependent. Confirm that it contains only the executable and intended seed configuration files.

## Manual release checklist

- [ ] Update the project version intentionally.
- [ ] Update [Features and status](features.md) for changed capabilities.
- [ ] Update [Configuration and data](configuration.md) for schema changes.
- [ ] Run a clean Release configure and build.
- [ ] Run CTest and relevant sanitizer tests.
- [ ] Validate the non-root `runtime` install.
- [ ] Inspect the ZIP contents.
- [ ] Run the packaged executable in portable mode.
- [ ] Verify startup, torrent add, search, persistence, and diagnostics.
- [ ] Record platform, compiler, dependency, and test results.
- [ ] Create a release note describing user-visible changes and known limitations.

## Future release automation

The intended future workflow is to add CI jobs that build and test supported Linux, macOS, and Windows configurations, create reproducible runtime artifacts, generate checksums, and publish artifacts only after all required checks pass. Signing keys and credentials must remain outside the repository and CI logs.

Until that automation exists, do not describe a build as cross-platform released merely because the CMake conditionals exist. A platform is release-supported only after its build and runtime smoke test are recorded.
