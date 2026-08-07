# Troubleshooting

Start by collecting the terminal output, the persistent log, the platform, and the exact build command. Do not share private magnet links, passwords, access tokens, or full private filesystem paths in a public issue.

## The project does not configure

Run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

If CMake cannot find a package:

1. Check the platform dependency commands in [Build](build.md).
2. Confirm the compiler and CMake versions.
3. Check the vcpkg toolchain path on Windows.
4. If FetchContent is being used, confirm network and proxy access.
5. Use disconnected mode only when the dependency sources are already cached.

## The build fails after dependency changes

Reconfigure the same build directory first. If its cache points to an obsolete package or toolchain, create a new explicitly named build directory such as `build-debug-clean`; do not delete broad directories or reset source changes.

## The application does not open a window

Run `hypertube` from a terminal and inspect its stderr first. Check:

- display/session environment;
- whether another process or window manager issue prevents window creation.

The application logs startup failures to stderr before the normal logger is available.

## A native picker does not open

The Slint frontend uses the platform picker boundary for `.torrent` files and
directories. Linux tries `zenity` and then `kdialog`; macOS uses the system
script bridge; Windows uses the native file-open dialog. If the picker is not
installed, unavailable in the current desktop session, or cancelled, enter
the path manually in the same dialog and continue. This does not change the
stored torrent or preference data.

## Settings or torrents are missing after restart

Check the active path mode first:

```sh
HYPERTUBE_PORTABLE=1 ./build/hypertube
```

Then inspect the corresponding `settings.json`, `torrents.json`, and `.bak` files. The application tries the primary file and then the backup when the primary is missing, malformed, or schema-invalid. Preserve both candidates before editing them.

## Configuration is reported as invalid

Do not replace the file immediately. Check that:

- the JSON is complete and syntactically valid;
- `version` is an integer;
- speed limits are non-negative integers;
- the settings object has the expected types;
- the process has permission to read and write the directory.

If the primary is invalid and `.bak` is valid, startup should log a recovery warning and use the backup. A failure with both candidates indicates that defaults may be generated; save the original files for diagnosis.

## Search fails or remains unavailable

Search requires network access to the active provider. Check:

- DNS, proxy, and TLS connectivity;
- the provider URL and HTTP response code;
- whether cancellation was requested;
- whether the provider returned an unexpected JSON shape;
- the Search category in the diagnostics view and log file.

Requests have timeout, response-size, TLS, and cancellation safeguards. Do not disable TLS verification to work around a provider problem.

## A torrent does not start

Check the torrent status and details view, then verify:

- save-path existence and write permission;
- metadata availability for magnets;
- tracker/DHT connectivity;
- whether the torrent was paused or removed;
- disk space and file permissions.

Use a disposable test directory when reproducing removal or file-priority behavior.

## Where to find diagnostics

The Logs window displays recent application and libtorrent diagnostics. The persistent file is `hypertube.log` under the platform data directory, or `./data/hypertube.log` in portable mode. Relevant categories include `app`, `config`, `torrent`, and `search`.

## Reporting a bug

Include:

- platform and compiler;
- commit or build version;
- exact reproduction steps;
- expected and actual behavior;
- sanitized terminal/log excerpt;
- whether the issue reproduces in Debug or with sanitizers.
