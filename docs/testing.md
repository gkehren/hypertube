# Testing and validation

The project uses GoogleTest and CTest. Tests are service-level and deterministic;
they should not require a live torrent session, a real user home directory, or
an external search provider.

## Test targets

| Target | Scope |
| --- | --- |
| `unit_tests` | String formatting, URL encoding, magnet formatting, ETA helpers, paths, and persistence controllers. |
| `config_tests` | Defaults, migration, schema validation, atomic saves, concurrency, and backup recovery. |
| `search_tests` | Response parsing, malformed data, pagination, duplicate handling, URL construction, and custom providers. |
| `torrent_tests` | Input validation, duplicate prevention, v2 magnets, status refresh, and fast-resume restoration. |
| `slint_model_tests` | Slint torrent/search model reconciliation, stable IDs, and large-model updates. |
| `slint-preview-check` | Compiles static Slint preview sources with the pinned compiler. |

Run the full suite:

```sh
ctest --test-dir build --output-on-failure
```

Validate the preview target explicitly when diagnosing Slint markup:

```sh
cmake --build build --target slint-preview-check
ctest --test-dir build -R slint-preview-check --output-on-failure
```

The preview check is also part of the normal CTest run.

## Test design rules

- Use isolated temporary directories for every persistence test.
- Do not read or overwrite the developer's home directory or repository `config/` during a test.
- Do not require a network connection for parser and provider tests.
- Assert observable results and recovered values, not only log messages.
- Cover successful and expected failure `Result` paths.
- Use bounded waits and explicit synchronization for asynchronous tests.
- Clean up temporary files, including `.tmp` and `.bak` candidates.

## Regression coverage

Persistence tests cover malformed data, migrations, backup recovery, atomic
writes, concurrent updates, and orderly worker shutdown. Search tests cover
provider errors, cancellation, pagination, URL encoding, proxy validation,
fallback, and cache behavior. UI boundary tests cover snapshot consistency,
callback lifetime, model revisions, and non-blocking refresh behavior.

Native file and directory pickers are OS-boundary calls from the Slint callback
path. Their availability and cancellation behavior must be smoke-tested on the
target desktop environment; automated tests use manual path fields where needed.

## Sanitizers

```sh
cmake -S . -B build-asan \
  -DHYPERTUBE_ENABLE_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan -j2
ctest --test-dir build-asan --output-on-failure
```

## Manual smoke test

When a change affects the executable or UI:

1. Start the application from a terminal.
2. Add a test torrent or magnet link using a disposable save directory.
3. Verify status, filtering, details, pause/resume, and removal.
4. Exercise the changed error path.
5. Open Logs and verify that diagnostics are visible and clearable.
6. Restart when persistence is involved and verify restoration.

Before handoff, run the full CTest suite and `git diff --check`. CMake install
changes additionally require a runtime component install and inspection.
