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
information plus MSB-first CRC-16 (polynomial 0x1021, initial and final xor 0xffff). Header and body groups can arrive out of order.

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

The controller can transmit a generated compact or extended request as a
`bsr.bin` MOT object through the existing waveform/PTT backend. Received BSR
objects are parsed and shown separately from images; malformed requests are
rejected before publication. A received request is untrusted and does not
automatically trigger retransmission. The operator can explicitly retransmit
requested segments of the last successfully transmitted image kept in memory,
when its transport identifier, segment size, profile, optional filename and
segment indices match. Identifiers can collide; the request does not
authenticate its sender. The cached source is lost when the application exits.
Controller-level A/B tests cover both dialects, rejected mismatches and exact
reassembly after repair. `test_hamdrm_waveform` also encodes the headers and
two selected missing segments as PCM for two JPEG fixtures, including a
missing final segment, then decodes the resulting
waveform in different chunk sizes and verifies exact reassembly. This test
seeds the other segments directly.

The A/B lab `python3 tests/sstv/tools/run_hamdrm_repair_two_app_lab.py a|b`
also verifies selective repair with two full Decodium processes over BlackHole.
The sender first transmits a JPEG and retains its source; after that TX the
receiver is seeded with all but segment 1 and started. The harness injects a
valid BSR into the sender (extended for A, compact for B), then explicitly
starts repair. Both transmitters completed, and both received JPEG files
matched the original 2,725 bytes exactly. Fixtures contain a small JPEG with
zero padding to cross the 1,024-byte segment boundary. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-repair-a-u_1i30tz`
and `.../decodium-hamdrm-repair-b-gyjs240_`. This verifies the application
repair waveform path; the initial receive loss and BSR arrival are simulated,
so it is not a complete over-audio BSR/repair exchange or external interoperability.

An additional experimental `a air` variant sends the BSR over BlackHole and
then changes the same two applications back to repair TX/RX. The BSR was
received and repair was requested, but the image was not reconstructed in
the first runs. With digital-only RX after BSR TX, diagnostics showed the
legacy source selected, with zero accepted HAMDRM chunks; with the analog
local capture started first, the local tap received 40 chunks and then
stopped advancing despite RX being active, monitoring enabled, and neither TX
suspension nor sample discard being set. The cause is not yet established.
`sstvRxDiagnostics` exposes accepted HAMDRM chunks and groups, source and tap
selection, monitor state, TX suspension and sample-discard state to support
further investigation. The tests above are macOS-only; Linux and Windows,
including their legacy-backend paths, require independent verification.
The current harness waits for three consecutive advances in the HAMDRM chunk
counter instead of treating an active state or a single callback as proof of
readiness. Representative failed artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-repair-a-oy4i7rig`
and `.../decodium-hamdrm-repair-a-ftprwylr`. The legacy-mode activation now
reuses native SSTV capture arbitration when no analog receiver is already
active, with ownership intended to restore the preceding monitor on stop.
In subsequent A/B trials, digital-only RX selected the local tap and accepted
43 and 41 chunks respectively, then both stalled before readiness; the full
audio repair test therefore still fails. Artifacts:
`.../decodium-hamdrm-repair-a-21oybrat` and
`.../decodium-hamdrm-repair-b-ojwi0_hn`. The BSR-only two-application A/B
tests passed after this change, but automatic capture ownership/restoration
and operation on Linux and Windows need further independent tests. These are
local BlackHole observations; they do not establish a hardware-radio failure.

Completed `bsr.bin` objects can be followed by another request using the same
transport ID in the same receive session. The last complete request stays
visible while the following candidate is assembled; a differing header can
replace a stale BSR candidate left by the previous carousel. A/B tests with
two Decodium processes sent two different requests in one RX session over
BlackHole, using extended and compact dialects respectively. The partial MOT
source was seeded by the test, so these checks do not demonstrate complete
image retransmission or external application interoperability. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-a-is2m3aw9`
and `.../decodium-hamdrm-two-app-b-ch1n_mcp`. Reproduce with
`python3 tests/sstv/tools/run_hamdrm_two_app_lab.py a bsr-repeat` and
`python3 tests/sstv/tools/run_hamdrm_two_app_lab.py b bsr-repeat`.

Two isolated Decodium application pairs exchanged A/B BSR objects over
BlackHole without radio: A used the extended form for transport `0x2451`, B
the compact form for `0x5372`; both requested the expected missing segments.
The lab seeded a synthetic partial MOT object before TX, so this is an
application waveform test of BSR transfer, not an end-to-end repair of a
previously transmitted image. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-a-x9patv3y`
and `.../decodium-hamdrm-two-app-b-0ix120us`. Reproduce with
`python3 tests/sstv/tools/run_hamdrm_two_app_lab.py a bsr` and `b bsr`.
After the receive-session cleanup change, these A/B audio transfers were
repeated successfully with artifacts at
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-a-8_yiuxsk`
and `.../decodium-hamdrm-two-app-b-uvi30h6j`. The cleanup itself was exercised
by the A/B controller tests; the application audio tests transfer one BSR each.

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
