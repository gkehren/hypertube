# Configuration and data

Hypertube stores settings, torrent restoration state, search history, favorites, cache data, and diagnostics outside the source tree by default.

## Directory selection

`Utils::AppPaths` selects directories at runtime.

| Platform | Configuration | Data | Cache |
| --- | --- | --- | --- |
| Linux | `$XDG_CONFIG_HOME/hypertube` or `~/.config/hypertube` | `$XDG_DATA_HOME/hypertube` or `~/.local/share/hypertube` | `$XDG_CACHE_HOME/hypertube` or `~/.cache/hypertube` |
| macOS | `~/Library/Application Support/Hypertube` | `~/Library/Application Support/Hypertube` | `~/Library/Caches/Hypertube` |
| Windows | `%APPDATA%/Hypertube` | `%LOCALAPPDATA%/Hypertube` | `%LOCALAPPDATA%/Hypertube/cache` |

If the relevant environment variables are unavailable, the application uses safe platform or executable-directory fallbacks. `AppPaths::ensureDirectories()` creates the required directories during startup.

## Portable mode

Portable mode is enabled when `HYPERTUBE_PORTABLE=1` or when `portable.mode` exists next to the Hypertube executable.

```text
<executable-directory>/config/settings.json
<executable-directory>/config/torrents.json
<executable-directory>/data/hypertube.log
<executable-directory>/cache/
```

Portable mode is intended for a self-contained distribution. It should not be used in a directory where the user cannot write configuration and data.

## `settings.json`

The current settings schema is version 2. Version 1 files are migrated by adding the nested UI layout defaults:

```json
{
  "version": 2,
  "settings": {
    "speed_limits": {
      "download": 0,
      "upload": 0
    },
    "download_path": "~/Downloads",
    "enable_dht": true,
    "enable_upnp": true,
    "enable_natpmp": true,
    "search": {
      "torznab_enabled": false,
      "torznab_url": "http://127.0.0.1:9696/api/v1/indexer/all/results/torznab/api"
    },
    "proxy": {
      "enabled": false,
      "type": "socks5",
      "host": "127.0.0.1",
      "port": 1080,
      "username": ""
    }
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
| `settings.search.torznab_enabled` | boolean | Use the configured Torznab endpoint, with torrents-csv fallback on initial-page failure. |
| `settings.search.torznab_url` | string | HTTP(S) Jackett/Prowlarr Torznab endpoint. |
| `settings.proxy.enabled` | boolean | Route both search HTTP and BitTorrent traffic through the proxy. |
| `settings.proxy.type` | string | `socks5` or `http`. |
| `settings.proxy.host` | string | Proxy hostname or IP address. |
| `settings.proxy.port` | integer | Proxy port from 1 to 65535. |
| `settings.proxy.username` | string | Optional non-secret proxy username. |

Torznab API keys and proxy passwords are not stored in this file. Preferences writes them to Windows Credential Manager, macOS Keychain, or Linux Secret Service. Linux needs the `secret-tool` command and an unlocked keyring. `HYPERTUBE_TORZNAB_API_KEY` remains a startup-only fallback when no stored Torznab key exists.

Older unversioned configurations are treated as version 0 and migrated to the current structure. Missing defaults are filled by `ConfigManager`; invalid values do not replace a valid backup candidate with defaults without first attempting recovery.

## `torrents.json`

Torrent restoration state has this shape:

```json
{
  "version": 2,
  "torrents": [
    {
      "magnet_uri": "magnet:?xt=urn:btih:...",
      "save_path": "/path/to/downloads",
      "torrent_path": "/path/to/file.torrent",
      "resume_data": "646..."
    }
  ]
}
```

`magnet_uri` identifies a magnet torrent, `save_path` identifies the data directory, `torrent_path` is optional when the torrent was added from a file, and `resume_data` is bounded hex-encoded libtorrent fast-resume state. Invalid resume data is ignored while a valid magnet or torrent-file identity remains usable. Torrent state is refreshed periodically and once more during orderly shutdown.

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
