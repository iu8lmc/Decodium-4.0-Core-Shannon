# Native HAMDRM architecture

Status: active implementation record, 2026-08-24.

HAMDRM is a digital image-transfer subsystem inside the Decodium4 process. It
is deliberately separate from the analog SSTV decoder and encoder, but it must
reuse Decodium's existing audio fan-out, sound output, CAT/PTT ownership,
settings, storage, diagnostics and QML workspace. It never launches QSSTV,
EasyPal, Python or another process, never opens a second capture device and
never treats an OFDM primitive as proof of an interoperable radio backend.

## Native target boundary

The build option is `DECODIUM_ENABLE_HAMDRM`. It requires
`DECODIUM_ENABLE_SSTV=ON`; disabling it removes OpenJPEG and all digital
targets while leaving analog SSTV unchanged.

```text
Decodium application and QML
        |
        v
HamDrmController                 owner-thread state and bounded QML facade
        |
        +-- HamDrmMotCodec       MOT header/body data groups and CRC
        +-- HamDrmObjectAssembler out-of-order segments, duplicates, missing set
        +-- HamDrmBsrCodec       compact/EasyPal-style and QSSTV-extended BSR
        +-- HamDrmPartialStore   atomic resumable state below AppDataLocation
        +-- HamDrmImageValidator bounded type, signature and dimension checks
        +-- HamDrmJpeg2000Codec  target-scoped OpenJPEG memory adapter
        |
        v
channel/                         FAC, Part-B MSC coding/interleaving and pilots
        |
        v
phy/                             narrow HAMDRM QAM and OFDM primitives
        |
        v
waveform/HamDrmWaveformAdapters  bounded native RX worker and 48 kHz TX source
        |
        v
Decodium audio and TX ownership existing shared RX tap, output, CAT/PTT watchdog
```

Core protocol and PHY targets are Qt-free. Persistence links only Qt Core;
JPEG2000 links only the audited OpenJPEG target; the application-facing
controller links Qt Core/Gui and the three smaller targets. No analog target
links OpenJPEG.

## Profile model

The canonical registry contains named typed tuples, never a QML-visible opaque
index. The current narrow amateur profile space is the Cartesian product of:

- robustness A, B and amateur E;
- occupied bandwidth 2.3 and 2.5 kHz;
- high and normal protection;
- 4-, 16- and 64-QAM;
- long and short interleaving.

This yields 72 stable IDs. Payload bytes per 400 ms frame and compatibility
codes are registry data. Amateur robustness E is not advertised as current
broadcast DRM robustness E, and the registry does not claim full ETSI DRM
coverage.

## Object and resume lifecycle

An outbound object is validated before packetisation. Its filename is reduced
to a safe basename, its content type is restricted to JPEG, JP2, PNG, GIF, BMP
or the protocol BSR object, and its object/header/segment counts are checked
before allocation. MOT data groups carry session, user-access and transport
information plus CRC-16/X-25. Header and body groups can arrive out of order.

The assembler accepts an exact duplicate idempotently and rejects a conflicting
duplicate. It reports the exact missing segment set and emits a completed
object only after consistent header metadata, complete segment coverage and
declared-size agreement. Partial state is written through `QSaveFile` using a
transport-ID-derived path, owner-only permissions and an internal SHA-256; a
truncated, modified or symlinked record fails closed.

The BSR parser/generator supports the compact range representation used by
EasyPal-compatible receivers and the extended QSSTV representation. Compact
ranges can intentionally request already received filler around holes; the
controller records this behavior instead of silently treating it as an exact
set. Unsupported profile identifiers and malformed or excessive ranges are
rejected.

## Image boundary

The allocation-free validator checks signature, bounded container structure,
dimensions and pixel ceilings before any full decode. Defaults are:

- 16 MiB object;
- 8 KiB MOT header;
- 8,191 bytes per segment;
- 32,768 segments;
- 80 filename bytes;
- 8,192 pixels on either dimension;
- 16,777,216 total pixels.

The OpenJPEG adapter uses bounded in-memory callbacks. It reads and validates
the JP2 header and components before full decode, emits RGBA for supported
grayscale/RGB/SYCC inputs and provides lossless JP2 encoding. OpenJPEG is a
required dependency only when HAMDRM is enabled; configuration fails with an
actionable error when version 2.5 or newer cannot be found.

## Threading and radio ownership

`HamDrmController` is an owner-thread QObject. Backend callbacks may originate
on worker/audio threads but must be marshalled to the owner and carry a session
identifier so stale callbacks cannot mutate a later operation. Cancellation
must synchronously detach the sink before a backend returns.

A backend may advertise waveform RX or TX only when it spans the complete
application-to-audio boundary. QAM, OFDM, coding, MOT and a self-round-trip do
not individually satisfy that capability. The current Bridge wires
`HamDrmNativeRxBackend` to Decodium's existing shared PCM tap and wires
`HamDrmNativeTxBackend` to `SstvTxCoordinator::startPrepared()`. The controller
therefore reports the corresponding native subset as available only while
those real hooks are installed; a default controller without hooks remains
fail-closed.

The RX adapter performs decode work on one bounded worker. Its cross-thread
producer accepts only configured sample rates/chunk sizes and caps queued
chunks, queued samples and session frames. Cancellation drains active callbacks
before detaching the shared tap. The TX adapter packetises bounded MOT groups,
generates a bounded pull-oriented 48 kHz PCM source and relies on the existing
coordinator for exclusive TX ownership, played-audio completion, cancellation,
watchdog and PTT release. It does not create a second audio or CAT/PTT stack.

## Evidence and non-claims

Unit tests cover all 72 profile records, the standard CRC check value, MOT and
BSR valid/malformed paths, hostile names, out-of-order/duplicate/conflicting
segments, missing tracking, partial-state damage, bounded image containers,
lossless OpenJPEG round-trip, QAM/OFDM primitives, pinned narrow-HAM FAC/MSC
coding and cell plans, and deterministic parser mutation. The application
controller and its QML page also have registered state and offscreen rendering
tests. `test_hamdrm_waveform_adapters` covers the connected controller,
bounded shared-tap RX self-roundtrip, native 48 kHz TX source, stale/completion
handling and the shared PTT release barrier; the full Decodium QML executable
links the same adapters through the Bridge. ASan/UBSan runs cover the pure
core, channel coding and OpenJPEG adapter.

This wiring and self-roundtrip evidence does not establish an independently
decoded QSSTV waveform, EasyPal interoperability, a real-radio transmission,
sample-clock/carrier recovery under RF impairment, or maintained-platform
packaging. Those remain release gates and must stay explicit in
`HAMDRM_COMPATIBILITY_MATRIX.md`.

## Audited references

- ETSI ES 201 980 V4.3.1, DRM system specification:
  <https://www.etsi.org/deliver/etsi_es/201900_201999/201980/04.03.01_60/es_201980v040301p.pdf>
- QSSTV pinned behavior reference:
  `ON4QZ/QSSTV@8c27d6d169d8c6c197eb47c2089870e39bc06a02`
- OpenJPEG 2.5.4 API and codec dependency:
  <https://github.com/uclouvain/openjpeg/tree/v2.5.4>

Restricted and ambiguous QSSTV files listed in `UPSTREAM_PROVENANCE.md` are
excluded. The Decodium implementation is clean-room protocol code plus the
separately licensed OpenJPEG library.
