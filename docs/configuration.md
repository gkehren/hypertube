# Configuration and data

Hypertube stores settings, torrent restoration state, search history, favorites, cache data, and diagnostics outside the source tree by default.

## Directory selection

`Utils::AppPaths` selects directories at runtime.

| Platform | Configuration | Data | Cache |
| --- | --- | --- | --- |
| Linux | `$XDG_CONFIG_HOME/hypertube` or `~/.config/hypertube` | `$XDG_DATA_HOME/hypertube` or `~/.local/share/hypertube` | `$XDG_CACHE_HOME/hypertube` or `~/.cache/hypertube` |
| macOS | `~/Library/Application Support/Hypertube` | `~/Library/Application Support/Hypertube` | `~/Library/Caches/Hypertube` |
| Windows | `%APPDATA%/Hypertube` | `%LOCALAPPDATA%/Hypertube` | `%LOCALAPPDATA%/Hypertube/cache` |

If the relevant environment variables are unavailable, the application uses safe platform or current-directory fallbacks. `AppPaths::ensureDirectories()` creates the required directories during startup.

## Portable mode

Portable mode is enabled when `HYPERTUBE_PORTABLE=1` or when `portable.mode` exists in the current working directory.

```text
./config/settings.json
./config/torrents.json
./data/hypertube.log
./cache/
```

Portable mode is intended for a self-contained distribution. It should not be used in a directory where the user cannot write configuration and data.

## `settings.json`

The current settings schema is version 1:

```json
{
  "version": 1,
  "settings": {
    "speed_limits": {
      "download": 0,
      "upload": 0
    },
    "download_path": "~/Downloads",
    "enable_dht": true,
    "enable_upnp": true,
    "enable_natpmp": true
  }
}
```

| Field | Type | Meaning |
| --- | --- | --- |
| `version` | integer | Configuration schema version. |
| `settings.speed_limits.download` | non-negative integer | Download limit in bytes per second; `0` means unlimited. |
| `settings.speed_limits.upload` | non-negative integer | Upload limit in bytes per second; `0` means unlimited. |
| `settings.download_path` | string | Default directory for torrent data. |
| `settings.enable_dht` | boolean | Enable DHT peer discovery. |
| `settings.enable_upnp` | boolean | Enable UPnP port mapping. |
| `settings.enable_natpmp` | boolean | Enable NAT-PMP port mapping. |

Older unversioned configurations are treated as version 0 and migrated to the current structure. Missing defaults are filled by `ConfigManager`; invalid values do not replace a valid backup candidate with defaults without first attempting recovery.

## `torrents.json`

Torrent restoration state has this shape:

```json
{
  "torrents": [
    {
      "magnet_uri": "magnet:?xt=urn:btih:...",
      "save_path": "/path/to/downloads",
      "torrent_path": "/path/to/file.torrent"
    }
  ]
}
```

`magnet_uri` identifies a magnet torrent, `save_path` identifies the data directory, and `torrent_path` is optional when the torrent was added from a file. Entries are validated before being passed to `TorrentManager`.

## Favorites and history

Favorites and search history are persisted through the settings configuration. `SearchEngine` takes synchronized snapshots before passing them to `ConfigManager` so asynchronous saves never read mutable vectors concurrently.

## Atomic writes and recovery

Configuration saves use a temporary file, flush it, retain the previous valid file as `<path>.bak`, and replace the target. On startup, the primary file is tried first, followed by the backup when the primary is missing, malformed, or fails schema validation.

Recovery behavior:

1. Use a valid primary candidate.
2. If the primary cannot be parsed or validated, use a valid `.bak` candidate.
3. Log that recovery occurred.
4. Use defaults only when no usable candidate remains.

Do not edit live user files while the application is running unless you have a backup. When reporting a persistence problem, preserve the primary, `.bak`, and log files for diagnosis.
