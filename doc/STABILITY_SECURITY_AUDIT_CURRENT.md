# Stability and Security Audit

This audit replaces the generic "bug bounty" draft with a review aligned to the current Decodium codebase.

## Scope

- Improve diagnosability of CAT and rig-control failures.
- Reduce configuration security regressions where secrets fall back to plain `QSettings`.
- Keep protocol compatibility with existing radio/network integrations.

## Confirmed Findings

### 1. CAT worker blocking exists, but not as a proven GUI-freeze bug

`QThread::msleep()` is still present in:

- `Transceiver/DXLabSuiteCommanderTransceiver.cpp`
- `Transceiver/HRDTransceiver.cpp`
- `Transceiver/OmniRigTransceiver.cpp`

These objects run on the dedicated transceiver thread created in `Configuration.cpp`, so this is primarily a CAT-worker latency/blocking issue, not proof of a main-GUI freeze.

### 2. Exception handling was still weak in parts of rig control

The codebase already logs many failures, but some paths were still too quiet:

- silent attenuation failures in `Transceiver/TransceiverBase.cpp`
- weak poll failure diagnostics in `Transceiver/PollingTransceiver.cpp`
- cleanup paths swallowing exceptions with minimal context

### 3. Secret storage is partially hardened, but fallback still exists

`OTPSeed`, `LotW`, `RemoteToken`, and `CloudLogApiKey` already use the secure settings wrapper in `Configuration.cpp`.

Risk remains when the secure backend is unavailable:

- macOS without usable Keychain access
- Linux without `secret-tool`

In that case the code falls back to plain `QSettings`.

### 4. LoTW default URL was still plain HTTP

The default `LotW` CSV URL in `Configuration.ui` still pointed to:

- `http://lotw.arrl.org/lotw-user-activity.csv`

This was not catastrophic because ARRL redirects to HTTPS, but it was unnecessary and less clean.

## Claims Rejected From The Original Draft

### TCI authentication handshake

Rejected as a direct action item.

`TCITransceiver` is a WebSocket client to the external SDR's TCI server. Decodium is not the TCI server in this path, so adding an app-side authentication handshake would not secure the remote endpoint unless the TCI protocol/server itself supports it.

### HOTP endian bug

Not confirmed.

`otpgenerator.cpp` already converts the counter with `qToBigEndian()` before HMAC input. This area needs RFC vector tests more than speculative rewrites.

### Blanket replacement of all `strcpy/sprintf`

Rejected as a mechanical mass-edit.

Some legacy C code should be reviewed, but a grep-driven global replacement is likely to introduce truncation and behavior regressions. This must be done file-by-file with bounds analysis and tests.

## Changes Applied In This Pass

### Exception and failure logging

- Replaced silent attenuation failure handling in `Transceiver/TransceiverBase.cpp` with explicit `CAT_WARNING` logs.
- Added explicit warning logs for unexpected exceptions in `Transceiver/TransceiverBase.cpp`.
- Added poll failure warning logs in `Transceiver/PollingTransceiver.cpp`.

### Secure settings fallback visibility

- Added one-time warnings in `Configuration.cpp` when secure storage is unavailable and a secret falls back to `QSettings`.

### Safer default URL

- Changed the default LoTW CSV URL in `Configuration.ui` from HTTP to HTTPS.

## Recommended Next Steps

1. Refactor the longest CAT-side waits in `DXLabSuiteCommanderTransceiver` and `OmniRigTransceiver` to asynchronous state progression.
2. Add HOTP/TOTP tests with RFC vectors instead of rewriting the implementation blindly.
3. Audit the remaining legacy C string handling in the decoder/utilities with targeted bounds checks and, where practical, sanitizer-backed testing.
4. Consider making secure-backend failure more visible in the UI for `RemoteToken`, `CloudLogApiKey`, and `OTPSeed`.
