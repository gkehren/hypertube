# Slint migration

## Scope

Hypertube is being migrated incrementally from Dear ImGui/GLFW/OpenGL to a
Slint 1.16.1 frontend. The torrent, search, persistence, credential, logging,
and system services remain shared by both frontends during the transition.

The migration is intentionally staged. Slint is now the default frontend;
the ImGui frontend remains buildable as an opt-in target until the [parity
checklist](ui-parity-checklist.md) is complete. No JSON format or
network/torrent behavior is changed by the UI migration.

## Baseline

The baseline was checked on 2026-08-05 from the existing configured build tree:

```text
cmake --build build -j2                 passed
ctest --test-dir build --output-on-failure  66/66 passed
```

A fresh `build-baseline` configure could not download the existing FetchContent
dependencies because this environment could not resolve `github.com`. It did
not change the source tree. A clean configure with network access is still
required before publishing baseline build claims for a new machine.

## Phase gates

1. CMake 3.21 and C++20, with the ImGui frontend still working.
2. Toolkit-neutral presenters and controllers, covered by focused tests.
3. A pinned Slint 1.16.1 frontend running beside ImGui.
4. App shell, torrent table, selection, sorting, actions, refresh, and details.
5. Go/no-go review before migrating search, preferences, and diagnostics.
6. Final parity validation before removing ImGui, GLFW, OpenGL, and the old UI.

Each phase must leave the existing service libraries and persisted JSON
formats compatible with the previous release.

## Implementation status (2026-08-05)

Phases 0 through 4 are implemented: the C++20/CMake 3.21 baseline is
documented, presentation DTOs/controllers are shared by both frontends, Slint
is pinned to v1.16.1, and `App` no longer owns GLFW. Phases 5 through 11 have
an operational Slint bootstrap covering the shell, torrent rows, filtering,
sorting, stable-ID selection, confirmation-based removal, details,
typed-path/magnet addition, native picker adapters, search, favorites, themes,
preferences persistence, per-torrent settings, and bounded diagnostics. The
model adapters now reconcile insert/update/delete/reorder changes by stable ID,
the Slint controller is split into targeted UI façades, refreshes are gated by
monotonic revisions and the active tab, compact layouts and modal scrims are
validated by the Slint compiler, and static previews are checked by the pinned
compiler through both a build target and CTest.

The native picker boundary is implemented with Windows `IFileOpenDialog`,
macOS `osascript`, and Linux `zenity`/`kdialog` fallbacks; availability and
runtime behavior still need desktop validation. Interactive sidebar and
details-panel splitters are implemented and persisted through the shared
preferences controller. The remaining work is intentionally visible:
screen-reader and 10,000-row runtime validation, visual/platform smoke tests,
final ImGui removal, and packaged runtime verification. Until those gates
pass, `hypertube-imgui` remains available as an opt-in target beside
`hypertube-slint`.

## Licensing gate

The repository currently has no project license file. The Slint distribution
license has not been selected yet. Before distributing a Slint binary, the
project owner must choose a Slint commercial/community/GPLv3 regime, review
the exact terms applicable to the release, add the project license and
third-party notices, and record the choice in `docs/release.md`. Until then,
the migration may be built and tested locally but must not be presented as a
distributable Slint release.
