# Native SSTV release notes

Status: release-readiness draft for the native SSTV feature branch.  These
notes describe only capabilities present in this source tree and must be
reconciled with the final CI run before publication.

Local verification on 2026-11-01: the macOS development build passed all 98
tests labeled `sstv`, including two-application E2EE transfer and tampering
tests, before the subsequent CLI shutdown fix. The CLI test and both E2EE
application tests passed again after that fix. Separate A/B BSR audio
transfers between Decodium processes also passed after the final shutdown
change. These results cover the local working tree, not release packages or
other platforms. The work remains uncommitted and the development build still
reports version 1.0.727; neither fact identifies a release revision.
The development executable's `--help`, `--version`, unknown-option and
post-parser validation exits are covered by `test_hamdrm_lab_cli`: each must
terminate promptly without a running diagnostic-writer thread. This does not
establish command-line behavior in a packaged release.

## What is included

- One in-process Decodium SSTV workspace for receive, transmit, Gallery,
  remote sharing, diagnostics and the separately gated HAMDRM subsystem.
- Analog receive from Decodium's existing live/external audio fan-out and from
  bounded WAV replay.  SSTV does not open a second capture device.
- Native analog transmit through Decodium's existing audio-output and
  CAT/PTT ownership, including cancellation, watchdog, lead/tail timing,
  calibration tones, internal codec loopback and protocol WAV export.
- A canonical registry for the implemented Martin, Scottie, Robot, Wraase,
  Pasokon, PD, AVT and MMSSTV MP/MR/ML wide/narrow modes listed in
  `MODE_MATRIX.md`, with standard/extended VIS and optional FSK ID.
- Progressive receive display, bounded retained-audio re-decode, automatic or
  manual mode selection, AFC/slant controls, partial-image preservation,
  atomic image/metadata storage, thumbnail Gallery and QSO logging.
- Opt-in HTTPS REST, WebDAV and trusted pre-signed PUT sharing providers with a
  durable queue, secure-setting credential handles and a validated incoming
  inbox.  The local integration provider is test/development-only and is not a
  network service.
- Optional end-to-end encrypted sharing with recipient key verification and
  authenticated decryption; local two-application HTTPS transfer and tamper
  tests are documented in `E2EE_IMPLEMENTATION.md`.
- Radiofax and experimental FAX480 profiles, with their individual support
  and verification limits documented in `RADIOFAX_PROTOCOL.md`.
- A separate native HAMDRM implementation for the precise profiles and
  transport subset recorded in `HAMDRM_COMPATIBILITY_MATRIX.md`, including
  MOT objects, integrity checks, BSR, partial retransmission/resume and native
  RX/TX waveform adapters wired to Decodium's shared audio tap and existing TX
  coordinator.

## Deliberate claim boundaries

- “Implemented” in `MODE_MATRIX.md` means native executable coverage; it does
  not mean that every row has been verified on-air or against another SSTV
  application.  Only the independently generated waveform rows identified in
  that matrix have independent PCM evidence.
- No successful real-radio, sound-card, CAT/PTT, DecoPort peer, Internet
  provider, QSSTV/EasyPal HAMDRM or on-air interoperability result is inferred
  from local loopback or deterministic tests.  Connected native HAMDRM
  adapters establish the in-process path only; those independent trials remain
  required before a broader release claim.
- Manual BSR repair has controller, waveform and two-application A/B tests;
  the latter injects the request and seeds the partial object. A complete
  over-audio request/repair exchange remains unverified. Consecutive BSRs in
  one receive session have a separate two-application A/B test.
- Platform support is claimed only for platforms whose final workflow actually
  succeeds.  Workflow definitions and packaging dependencies alone are not
  build evidence; update this note with the final CI run URLs before release.
- FAX480 and HFFAX/WEFAX profiles have local coverage, but exact geometry,
  broader variants and independent application interoperability remain
  bounded by the evidence in `RADIOFAX_PROTOCOL.md`.
- The six AVT24/90/94 QRM and Narrow-QRM variants have local two-application
  A/B checks; independent applications, interference and RF remain unverified.
- Remote sharing is disabled until the operator configures and enables a
  provider.  Decodium does not invent or deploy an Internet relay/backend.
- E2EE is optional and requires recipient identity verification. Local HTTPS
  tests do not establish sender authentication, public-provider interoperability
  or an independent cryptographic security audit. TLS-only sharing remains
  readable by its endpoint; require E2EE when that protection is needed.
- Incoming and outgoing files, metadata, queues and diagnostics are bounded,
  but operators should still treat files from unknown stations as untrusted.

## Operator safety

Confirm the selected radio, output route, frequency, TX permissions and audio
level before transmitting.  Begin with the calibration/loopback tools and low
power into a dummy load where appropriate.  Decodium releases PTT on normal
completion, cancellation and tested error paths, but the first deployment on
each station should still be observed directly.

## Packaging and deployment

The analog subsystem has no Python, Java, external SSTV executable or
platform-specific audio dependency.  HAMDRM requires the optional OpenJPEG
dependency when enabled.  Release packages must include Qt image-format
plugins used by accepted Studio/inbox images.  The required GIF, JPEG, TIFF and
WebP plugins are now explicit packaging dependencies and bundle verification
fails fast when one is missing; runtime input still passes through bounded
`QImageReader` validation.  Remote providers require an operator-controlled
HTTPS/WebDAV/pre-signed endpoint and credentials stored through Decodium's
secure-settings service.

## Before publishing

Replace this draft status with the final commit, exact successful platform
workflow runs, sanitizer/fuzzer results and performance measurements from the
finished tree.  Do not broaden the claims beyond `MODE_MATRIX.md`,
`HAMDRM_COMPATIBILITY_MATRIX.md` or the recorded final report.
