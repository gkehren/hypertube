# Security and privacy

Hypertube handles untrusted torrent metadata, magnet links, provider responses, filesystem paths, and optional network settings. Security changes must preserve validation, bounded resource use, and clear diagnostics.

## Current protections

- Search query and pagination values are URL-encoded before request construction.
- cURL requests use timeouts, a response-size limit, TLS peer verification, and TLS host verification.
- Search cancellation is connected to the cURL progress callback.
- Environment-derived paths are checked before being used as application directories.
- System helpers validate user-controlled paths before launching platform actions.
- JSON files are parsed and schema-validated before values are applied.
- Configuration writes use temporary files and a backup candidate to reduce corruption risk.
- Shared service state and logger state are synchronized before being copied or displayed.
- Torznab API keys and proxy passwords use the operating-system credential store rather than JSON configuration.
- Search redirects are restricted to HTTP(S), and torrent/search proxy settings are applied together.

## Data handling rules

- Never commit passwords, access tokens, private keys, personal torrent state, or user logs.
- Never log proxy passwords or credentials.
- Avoid logging complete private magnet links or unnecessary personal filesystem paths.
- Keep runtime state in platform data directories or explicit portable directories, not in tracked seed configuration.
- Treat provider responses and torrent metadata as untrusted input even when they come through HTTPS.

## Network and provider boundaries

HTTPS certificate verification must remain enabled. Provider errors, malformed responses, oversized responses, and cancellation must be handled as expected failures rather than causing unbounded retries or UI blocking.

Adding a provider requires:

1. a stable identifier and explicit selection behavior;
2. bounded request and response handling;
3. URL encoding for all user-controlled parameters;
4. parser tests for valid, empty, malformed, and unexpected responses;
5. user-visible errors without leaking sensitive request data.

## Filesystem and process boundaries

Validate paths before filesystem or OS integration operations. Do not concatenate untrusted values into shell commands. Prefer direct platform APIs or existing validated `SystemUtils` helpers.

Atomic persistence protects against interruption and partial writes, but it is not encryption. Users who need confidentiality must protect their profile or portable directory using operating-system controls.

## Credential stores

- Windows uses Credential Manager generic credentials.
- macOS uses Keychain generic passwords.
- Linux invokes `secret-tool` directly without a shell and sends secrets over standard input. An unlocked Secret Service-compatible keyring is required.

Stored secrets are scoped to the `Hypertube` service and separate account names. They are not included in portable bundles or configuration backups.

## Known limitations

- BitTorrent traffic and downloaded content are not made anonymous by Hypertube.
- A proxy improves routing control but does not by itself guarantee anonymity or prevent all metadata leakage.
- A valid HTTPS connection does not make third-party torrent metadata trustworthy.
- The current release workflow does not yet provide automated artifact signing or checksums.

Security fixes must include a regression test or a reproducible validation case and must update this document when the protection or limitation changes.
