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
| `slint_controller_tests` | Real Slint callback-to-presenter-to-details integration and refresh coordination. |
| `slint-preview-check` | Compiles static Slint preview sources with the pinned compiler. |
| `slint-renderer-software-benchmark` | Renders a bounded software-backend workload and writes CPU, memory, frame-time, and stability metrics. |
| `slint-renderer-comparison` | When enabled at configure time, runs the same workload with software and FemtoVG and writes comparable JSON reports. |

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

## Renderer benchmark

Software renderer smoke benchmark:

```sh
cmake --build build --target slint-renderer-software-benchmark
```

For an equivalent software/FemtoVG comparison, configure with
`-DHYPERTUBE_ENABLE_SLINT_GPU_BENCHMARK=ON` and build
`slint-renderer-comparison`. Both processes run 12 warm-up cycles followed by
120 measured redraw cycles at 1280×760 while alternating production views and
themes. A backend fails the target if its event loop cannot complete every
scheduled cycle within the stability timeout. GPU presentation timing remains
backend-owned and is not inferred from Slint's software-only snapshot API.
The reports are measurements, not an automatic renderer-selection policy.

## Torrent presentation benchmark

The cached torrent presenter has a small opt-in workload for checking rebuild,
filter, sort, and unchanged-revision paths:

```sh
cmake --build build --target torrent-presentation-benchmark
./build/torrent-presentation-benchmark
```

It prints CSV measurements for 100, 1,000, and 10,000 synthetic rows. The
numbers are machine- and build-dependent; on the reference Debug build, the
10,000-row workload measured approximately 16 ms for a full rebuild, 8 ms for
filtering, and 70 ms for sorting. Treat a sustained result above 50 ms for a
rebuild, 25 ms for filtering, or 100 ms for sorting as a regression requiring
investigation. The unchanged-revision path should remain below 1 ms.

## Visual snapshots

The `slint-visual-snapshots-run` target renders the production shell with mock
data for Torrents, Search, Favorites, Logs, Preferences, and both torrent
dialogs. It writes inspectable BMP artifacts for six viewport sizes and all
eight runtime themes (`Dark`, `Ocean`, `Nord`, `Dracula`, `CyberPunk`, `System`,
`Light`, and `High contrast`) under `build/visual-artifacts`:

```sh
cmake --build build --target slint-visual-snapshots-run
```

On headless Linux, run the target through `xvfb-run -a`. CI uploads the 624
generated images as the `slint-visual-snapshots` artifact. The matrix includes
empty Torrent, Search, Favorites, Logs, and Details models plus a 10,000-row
torrent model, in addition to long, Unicode, loading, error, v2, and hybrid examples.
For deterministic system-theme smoke checks, set `HYPERTUBE_SYSTEM_THEME=dark`
or `HYPERTUBE_SYSTEM_THEME=light` before launching the application.

For the native System theme, perform one smoke check on each target desktop:

| Platform | Expected check |
| --- | --- |
| Windows | Change Windows Personalization between light and dark, wait for the next refresh, and confirm the Hypertube System theme follows it. |
| macOS | Change Appearance between Light and Dark in System Settings, wait for the next refresh, and confirm the System theme follows it. |
| Linux GNOME/KDE | Change the desktop color scheme, wait for the next refresh, and confirm the System theme follows it; also verify the dark fallback when no desktop backend is available. |

Run the Preferences smoke test on each platform with Torznab enabled and
disabled: **Test Torznab** must use the current form without saving it, **Test
proxy** must remain independently available when Torznab is disabled, and
**Cancel test** must return a visible cancellation result. A successful network
test should display its measured latency.

## Accessibility smoke matrix

Run the following on each desktop target with the platform screen reader or
accessibility inspector enabled:

| Surface | Expected behavior |
| --- | --- |
| Sidebar, categories, menu, and dialogs | Controls expose names and roles; Tab and Shift+Tab reach every action; Escape closes an open dialog. |
| Torrent table and search results | Lists expose item counts and labels; Up/Down changes the primary torrent; Ctrl/Cmd-click toggles, Shift-click ranges, Ctrl/Cmd+A selects visible torrents, and Enter toggles the selected torrent between Pause and Resume. |
| Details tabs and data lists | Tabs expose the selected tab and lists expose item labels; file, peer, and tracker actions remain keyboard reachable. |
| Resize handles and notifications | Sidebar/details handles expose slider values and bounds; toast notifications are announced politely by screen readers. |
| Toast overlay | Severity is conveyed by the accessible title/message and visible color; Dismiss and any action button are keyboard reachable. |

The `slint-preview-check` target validates the semantic markup at build time;
screen-reader announcements and focus order remain target-desktop smoke tests.

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

Presentation unit tests also cover notification deduplication, bounded
queueing, expiry, and dismissal. Slint preview validation covers the toast
overlay through the production shell preview.

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
