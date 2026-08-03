# Contributing

Contributions should improve a concrete user or maintainer outcome while preserving responsiveness, persistence safety, and cross-platform behavior.

## Before editing

```sh
git status --short
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Read the relevant document in `docs/` and inspect nearby code and tests. Preserve unrelated worktree changes.

## Change workflow

1. Define the user-visible or correctness outcome.
2. Identify the owning subsystem and its synchronization boundary.
3. Add or update a focused regression test before or with the implementation.
4. Implement the smallest coherent change.
5. Update the relevant documentation for user-facing, schema, build, or release changes.
6. Run the affected tests and then the complete CTest suite.
7. Run sanitizers for memory, lifetime, parsing, callback, or concurrency changes.
8. Run `git diff --check` and inspect the final diff.

## Code guidelines

- Use C++17 and match the surrounding C++ style.
- Prefer RAII, standard containers, `std::filesystem`, and scoped locks.
- Keep UI calls on the main/render thread.
- Use snapshots when crossing from mutable services to UI or persistence.
- Use `Result` for expected operational failures with actionable messages.
- Do not hold locks while performing network or filesystem I/O unless the existing invariant explicitly requires it.
- Do not introduce a dependency when the standard library or an existing project dependency is sufficient.

## Subsystem-specific changes

### Configuration and persistence

Protect all `ConfigManager::config` access with `configMutex`, including saves and snapshots. Preserve atomic writes, `.bak` recovery, migrations, and async worker shutdown. Add tests for malformed, schema-invalid, missing, recovered, and concurrent files.

### Torrent management

Protect torrent maps and status cache with their existing mutexes. Return snapshots rather than internal references. Validate paths and preserve remove semantics.

### Search

Keep requests bounded and cancellable. Protect provider, settings, history, and favorites state. Do not let worker callbacks access destroyed UI state.

### UI

Keep blocking work out of the frame loop. Provide loading, empty, cancellation, success, and failure states. Ensure diagnostics and Clear actions remain synchronized with their backing stores.

## Commit and review expectations

Prefer a focused commit with a clear Conventional Commit-style subject, for example `fix(config): recover schema-invalid settings from backup`. Do not mix unrelated formatting or generated files into a feature change.

A review-ready change should state:

- what behavior changed;
- which files and subsystems changed;
- which tests and commands passed;
- which platforms or external services were not verified;
- whether a migration, release-note, or user-data impact exists.

History rewriting, signing, force-pushes, and external publication require explicit authorization.

## Documentation maintenance

All detailed product and technical documentation belongs in `docs/`. Keep the root README as a portal. Update `docs/features.md` when feature availability changes and update `docs/configuration.md`, `docs/build.md`, or `docs/release.md` when their corresponding behavior changes.
