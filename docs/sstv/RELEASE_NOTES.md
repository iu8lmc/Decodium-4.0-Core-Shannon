# Native SSTV release notes

Status: release-readiness draft for the native SSTV feature branch.  These
notes describe only capabilities present in this source tree and must be
reconciled with the final CI run before publication.

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
- Platform support is claimed only for platforms whose final workflow actually
  succeeds.  Workflow definitions and packaging dependencies alone are not
  build evidence; update this note with the final CI run URLs before release.
- FAX480 and HFFAX/WEFAX are related image modes, not implemented analog-SSTV
  rows.  Their exact geometry/profile evidence remains unresolved as recorded
  in the catalogue.
- AVT Narrow/QRM variants remain catalogue-only because the audited references
  do not establish enough picture semantics for a defensible implementation.
- Remote sharing is disabled until the operator configures and enables a
  provider.  Decodium does not invent or deploy an Internet relay/backend.
- End-to-end encryption is not advertised: production providers use validated
  TLS, but a TLS endpoint can read the transferred object.  A request that
  requires E2EE fails closed.
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
