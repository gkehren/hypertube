# AGENTS.md

## Purpose and scope

This file is the operating guide for AI agents working in the Hypertube repository. It applies to the whole repository unless a more specific `AGENTS.md` is added in a subdirectory.

Hypertube is a cross-platform C++20 BitTorrent desktop client. It uses Slint
for the interface, libtorrent for torrent operations, cURL for search HTTP
requests, and nlohmann/json for persisted configuration. The project is moving
from a functional demo toward a reliable daily-use application, so correctness,
recoverability, responsiveness, and inspectable behavior take priority over
superficial feature breadth.

Read this file together with `README.md` and the existing code before changing behavior. Keep the README and this file consistent when build, runtime, configuration, or packaging behavior changes.

## Repository map

```text
include/                 Public headers, grouped by subsystem
  app/                   Application orchestration and services
  ui/slint/              Slint controllers and model adapters
  utils/                 Paths, logging, string and system helpers
src/                     Implementations matching include/
  app/App.cpp            Application lifetime and startup/shutdown flow
  app/ConfigManager.cpp  JSON settings/state persistence and migrations
  app/TorrentManager.cpp libtorrent session and torrent state
  app/SearchEngine.cpp   Search providers, HTTP, cancellation, and history
  ui/                    Main-thread rendering and user interaction
  utils/                 Shared infrastructure
tests/                   GoogleTest unit and regression tests
config/                  Versioned seed settings; torrent state is runtime data
CMakeLists.txt           Dependency discovery, targets, tests, and packaging
README.md                User-facing features, configuration, and build guide
```

The build produces the single `hypertube` Slint frontend. The test targets are
`unit_tests`, `config_tests`, `search_tests`, `torrent_tests`, and
`slint_model_tests`. `tests/benchmark_favorites.cpp` is a benchmark source and
is not automatically included in the normal test suite unless the build
configuration is extended.

## Working agreement for agents

1. Inspect the repository before editing: `git status --short`, relevant diffs, nearby code, and existing tests.
2. Treat all existing worktree changes as user-owned. Do not reset, discard, overwrite, or reformat unrelated changes.
3. Make the smallest coherent change that satisfies the request. Do not add speculative architecture or dependencies without a concrete product or correctness reason.
4. Use `apply_patch` for source and documentation edits. Do not generate tracked files through shell redirection or scripts.
5. Do not modify `build/`, CMake FetchContent directories, or other generated artifacts as source changes. Reconfigure the build instead.
6. Do not commit, amend, rebase, force-push, or alter Git metadata unless the user explicitly requests it.
7. Never use destructive commands such as `git reset --hard`, `git checkout --`, or broad recursive deletion to solve a local problem.
8. Do not claim that a feature, package, platform, or runtime path works unless it has been verified or clearly label it as unverified.
9. Keep user-facing errors actionable and preserve diagnostic context. Expected operational failures should normally be returned through `Result` and/or logged, not silently ignored.
10. In the final response, report the behavioral change, the files that matter, validation performed, and any remaining limitation.

When a request is ambiguous, prefer the interpretation that stays within the named subsystem and existing product direction. Ask before taking an action that changes external history, publishes data, deletes user data, or introduces a materially different UX.

## Build and test commands

### Standard local build

Dependencies are discovered from the system or package manager when available. Missing dependencies are downloaded and built with CMake `FetchContent`, so the first configure may require network access.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

For an existing configured tree, a normal incremental validation is:

```sh
cmake --build build -j2
ctest --test-dir build --output-on-failure
git diff --check
```

If all dependencies are already available in the CMake cache, disconnected configuration can avoid network access:

```sh
cmake -S . -B build -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

Do not silently switch to disconnected mode when dependencies are not cached; report the missing dependency instead.

### Sanitizers

Use sanitizers for changes involving ownership, parsing, threading, callbacks, or persistence races:

```sh
cmake -S . -B build-asan \
  -DHYPERTUBE_ENABLE_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan -j2
ctest --test-dir build-asan --output-on-failure
```

The sanitizer option is supported for non-MSVC builds. Keep sanitizer build directories untracked.

### Packaging and portable installation

The runtime install is deliberately isolated from dependency install rules. Use the `runtime` component for a non-root portable install:

```sh
cmake --install build --prefix dist/hypertube --component runtime
touch dist/hypertube/portable.mode
```

To produce the configured ZIP package:

```sh
cpack --config build/CPackConfig.cmake -G ZIP
```

The portable bundle should contain the executable and the seed `config/` directory. Do not require root access or write to `/usr/local` for this workflow. Remove generated package output only when its exact path is known and it is not user-owned.

### Platform notes

- Linux/macOS: use a C++20 compiler and the platform command from `README.md`; use `nproc` or `sysctl` only when available.
- Windows: use the documented vcpkg toolchain and `cmake --build build --config Release`.
- CMake minimum version is 3.21.
- Release builds enable optimization and LTO against the portable compiler baseline. Host-specific ISA tuning is opt-in through `HYPERTUBE_ENABLE_NATIVE_OPTIMIZATIONS` and must not be used for distributed packages. Use Debug for diagnosis and tests unless release behavior is specifically being validated.

## Architecture and ownership

### Application lifecycle

`src/main.cpp` initializes and cleans up cURL globally, uses the toolkit-neutral
`App` with Slint, and waits for asynchronous persistence before shutdown. `App`
creates runtime directories, initializes logging, loads persisted torrent and
settings state, and owns the service objects.

Do not initialize or clean up cURL once per request. Do not destroy the Slint
window or services while worker callbacks can still use them.

### UI

`SlintAppController` coordinates the Slint views and presentation controllers.
Rendering and UI mutation run on the UI/main thread.

- Never perform blocking HTTP, filesystem recovery, or expensive libtorrent work directly inside a render path.
- Pass immutable snapshots or copied values to rendering code.
- Keep Slint callbacks and model mutations on the UI thread.
- Any worker-to-UI callback must have a clear lifetime policy and must not access destroyed UI state.
- Preserve user feedback for loading, empty, filtered, cancelled, and failed states.
- When changing a visible action, update the corresponding success/failure feedback and diagnostics behavior.

### ConfigManager

`ConfigManager` owns versioned JSON settings and torrent/favorites/history persistence. Its `configMutex` protects every access to the shared `config` object, including getters, setters, loading, saving, snapshots, migration, and validation. The async save queue has separate synchronization and must not be confused with `configMutex`.

Important invariants:

- Take a protected JSON snapshot before enqueueing or writing a save. Never copy `config` concurrently without `configMutex`.
- Keep lock scopes coherent; do not introduce a lock-order cycle with queue or service locks.
- Writes are atomic and retain a `<path>.bak` recovery candidate.
- Loading tries the primary file and then the backup when the primary is missing, malformed, or schema-invalid.
- Torrent recovery must use the candidate that was successfully validated; do not parse the primary a second time after a backup was selected.
- Preserve schema migrations and fill missing defaults without discarding valid user settings.
- Keep asynchronous saves alive until the manager is destroyed or `waitForAsyncOperations()` has completed.
- Add regression tests for malformed JSON, invalid schema, backup recovery, concurrent access, and atomic-save behavior when touching this code.

The current settings schema is version 2. Version 1 settings are migrated by adding the nested UI layout defaults. `settings.json` contains application settings; `torrents.json` contains persisted torrent entries. Runtime torrent state must not be committed to the repository.

### TorrentManager

`TorrentManager` owns the libtorrent session and mutable torrent maps. Protect torrent maps with `stateMutex`; protect the status cache with `cacheMutex`. The UI and persistence layers should use `getTorrentSnapshot()` and status-cache snapshots rather than iterating internal containers.

- Do not expose references or iterators to internal maps.
- Preserve duplicate detection and the requested remove semantics.
- Invalidate or refresh the status cache when torrent state changes.
- Keep libtorrent alert handling bounded and compatible with the UI frame loop.
- Validate paths, magnet links, and user-controlled values before passing them to libtorrent or system utilities.

### SearchEngine

`SearchEngine` supports providers, synchronous and asynchronous search, cancellation, pagination, history, favorites, and persistence.

- Only one active search is expected at a time; preserve the existing cancellation/join behavior.
- Protect provider registry, settings, search history, and favorites with their corresponding mutexes.
- Keep HTTP requests bounded: timeout, response-size limit, TLS peer/host verification, and cancellation must remain enabled.
- URL-encode query and pagination parameters; never concatenate untrusted values into shell commands.
- Treat provider errors, malformed JSON, empty results, and cancellation as distinct user-visible outcomes where practical.
- Snapshot favorites/history before passing them to `ConfigManager`.
- Ensure callbacks remain safe if the initiating UI or owner is closing.

### Paths and logging

`Utils::AppPaths` determines platform-specific configuration, data, and cache locations. Portable mode is enabled by `HYPERTUBE_PORTABLE=1` or a `portable.mode` marker beside the Hypertube executable.

Default locations:

- Linux: `XDG_CONFIG_HOME/hypertube` or `~/.config/hypertube`; data uses `XDG_DATA_HOME` or `~/.local/share/hypertube`.
- macOS: `~/Library/Application Support/Hypertube` and `~/Library/Caches/Hypertube`.
- Windows: `%APPDATA%/Hypertube` and `%LOCALAPPDATA%/Hypertube`.
- Portable mode: configuration under `<executable-directory>/config`, data under `<executable-directory>/data`, and logs at `<executable-directory>/data/hypertube.log`.

`Utils::Logger` writes structured records to disk and keeps recent records for the diagnostics UI. Its state is mutex-protected. Use the logger for actionable subsystem context (`app`, `config`, `torrent`, `search`) and keep the Clear action synchronized with both the UI buffer and `Logger::recent()`.

Never log passwords, access tokens, private keys, or sensitive user data. Be careful with full magnet links and filesystem paths in user-facing diagnostics.

## Persistence and compatibility rules

- Treat user files as untrusted input: validate JSON types, schema versions, paths, and sizes before use.
- Prefer additive, versioned migrations over silently changing the meaning of an existing field.
- Keep unknown data when it is safe and practical; do not overwrite a valid file with defaults merely because one field is missing.
- Use temporary files and atomic replacement for durable writes. Keep backup recovery behavior covered by tests.
- Do not use repository `config/settings.json` as the location for live user data during development.
- Tests must use isolated temporary directories and must not depend on the developer's home directory, network, or existing torrents.

## C++ and UI conventions

- Use C++20 and match the surrounding code style: `#pragma once`, tabs in C++ implementation bodies, braces and naming consistent with nearby files.
- Prefer RAII, standard containers, `std::filesystem`, scoped locks, and explicit ownership over raw owning pointers.
- Keep headers self-contained and include what they use. Avoid broad unrelated refactors while changing a feature.
- Use `Result` for expected operational failures and include a useful message. Catch exceptions at subsystem boundaries when external libraries can throw.
- Avoid global mutable state; if a process-wide facility is unavoidable, protect it and document its lifetime.
- Keep UI labels, empty states, error messages, and keyboard/mouse behavior consistent with the existing English UI unless localization is explicitly requested.
- Do not add a new third-party dependency when an existing dependency or standard-library facility is sufficient.

## Testing expectations

For every behavior change:

1. Add or update a focused GoogleTest regression test when the behavior is service-level, persistence-related, parsing-related, or concurrency-sensitive.
2. Build the affected target and run the complete `ctest` suite when practical.
3. Run the sanitizer build for concurrency, memory, lifetime, and parser changes when dependencies/toolchain permit it.
4. Run `git diff --check` before handing off.
5. For CMake install/package changes, validate a non-root `--component runtime` install and inspect the generated package contents.

Tests should assert observable behavior rather than private implementation details. In particular, configuration tests should verify that recovered values are actually used, not only that a warning was emitted.

## Review checklist

Before declaring work complete, check:

- Does the change preserve thread-safety and lock ordering?
- Can a malformed, missing, old, or schema-invalid user file cause data loss?
- Can a worker outlive the object, window, callback target, or logger it uses?
- Does the UI remain responsive during network and disk operations?
- Are errors visible, actionable, and represented in diagnostics?
- Are paths and external inputs validated before filesystem, libtorrent, HTTP, or process operations?
- Are credentials and private data excluded from source, logs, tests, and commits?
- Are new features covered by regression tests and documented in `README.md` when user-facing?
- Did the change accidentally depend on a generated build directory or local machine state?
- Were all relevant build/test/package commands actually run and reported?

## Handoff format

Use a concise final report with:

```text
Summary:
- ...

Validation:
- command: result

Notes:
- unverified platform, dependency, network, or packaging limitation
```

Include clickable repository file links when the client supports them. If history was rewritten or external state must be updated, say so explicitly and provide the safe follow-up command.
