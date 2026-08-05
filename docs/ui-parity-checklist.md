# UI parity checklist

This checklist tracks observable behavior while the ImGui and Slint frontends
coexist. Mark an item only after a manual smoke test and, where applicable, an
automated regression test.

## Startup and shell

- [ ] Startup and clean close
- [ ] Main menu and Help menu
- [ ] Categories: All, Downloading, Seeding, Completed, Paused, Active, Inactive
- [ ] Main tabs: Torrents, Search, Favorites
- [ ] Details tabs: General, Files, Peers, Trackers, Settings
- [ ] Sidebar and bottom-panel resizing, collapse, and persistence
- [ ] Narrow-window layout and 100/150/200% scaling

## Torrents

- [ ] Add `.torrent` file
- [ ] Add magnet link
- [ ] Add a selected search result
- [ ] Name, size, progress, status, seeds, peers, down, up, and ETA
- [ ] Numeric sorting and stable selection across refreshes
- [ ] Pause, resume, force start, recheck, queue, reannounce
- [ ] Sequential download and file priorities
- [ ] Open data directory and largest media file
- [ ] Copy magnet
- [ ] Remove while keeping files
- [ ] Remove data, source torrent, or both

## Details and search

- [ ] General details do not show stale torrent data
- [ ] Files, peers, and trackers loading/ready/error states
- [ ] Search idle/loading/results/empty/cancelled/failed states
- [ ] Pagination, load more, deduplication, and stale-request rejection
- [ ] Favorites, history, provider fallback, and result selection

## Preferences and diagnostics

- [ ] Transactional preferences save and rollback
- [ ] Proxy and Torznab validation
- [ ] Credential rollback on failed save
- [ ] Dark, Ocean, Nord, Dracula, and CyberPunk themes
- [ ] Logs, level filters, clear, bounded retention, and autoscroll
- [ ] Keyboard navigation, visible focus, and Escape handling

## Persistence and packaging

- [ ] Settings, torrents, favorites, and history survive restart
- [ ] Portable mode works outside the build tree
- [ ] Packaged startup without `.slint` files or Rust tooling
- [ ] Linux X11
- [ ] Linux Wayland
- [ ] Windows 10/11
- [ ] macOS Intel or Apple Silicon

## Evidence

Record the date, frontend, platform, build type, and any limitation beside the
item in the release or validation notes. Do not mark a platform as supported
from a successful CMake configure alone.
