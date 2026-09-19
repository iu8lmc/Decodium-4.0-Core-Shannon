# Native AVT protocol notes

Status: implemented normal AVT24, AVT90 and AVT94 modes on the
`feature/native-sstv` development snapshot, 2026-08-24. The core, canonical
registry, shared RX runtime/autodetect, TX coordinator/audio source, atomic WAV
export and Studio descriptors are wired into Decodium's existing SSTV targets.

This note records the clean-room wire contract used by Decodium. It is not a
claim of live-radio or cross-application interoperability.

## Executable normal modes

| Stable ID | Standard VIS | Prepared/wire raster | Effective sampled resolution | Component time | Image time |
|---|---:|---:|---:|---:|---:|
| `avt-24` | 64 | 128 x 120 | 128 x 120 | 62.5 ms | 22.5 s |
| `avt-90` | 68 | 320 x 240 | 256 x 240 | 125 ms | 90 s |
| `avt-94` | 72 | 320 x 200 | 320 x 200 | 156.25 ms | 93.75 s |

AVT90 deliberately has two width fields. The SSTV Handbook records 256
effective columns, while both pinned QSSTV and MMSSTV implementations prepare
and scan 320 columns over the same 125 ms component. Decodium keeps the common
320-column bounded frame/wire raster and separately publishes the 256-column
effective sampled resolution. It does not relabel 320 as the effective
resolution.

## Normal wire sequence

There is one continuous absolute-time sequence:

1. three complete standard VIS frames for the selected normal payload;
2. a 32-frame protected digital countdown;
3. the image as continuous red, green and blue components for every row.

A complete standard VIS frame is 910 ms. The three repetitions therefore last
2.730 s. The countdown lasts exactly 5.3125 s, making the complete normal AVT
header 8.0425 s.

The image contains no line-leading sync, component separator, porch or gap.
RX and TX use one cumulative mapper across the whole image rather than adding
individually rounded line or pixel durations.

Normal pixel values use the full 1500 Hz black to 2300 Hz white range:

```
frequency(value) = 1500 + 800 * value / 255 Hz
```

## Protected countdown

The countdown is transmitted at exactly 102.4 baud. One symbol is 9.765625 ms
and one protected frame is 17 symbols, or 166.015625 ms.

Each frame contains:

1. one 1900 Hz start symbol;
2. one eight-bit normal word, most-significant bit first;
3. the exact bitwise inverse of that word, most-significant bit first.

Normal data bits use 1600 Hz for zero and 2200 Hz for one. The high three bits
of the normal word identify the mode and the low five bits count monotonically
from 0 through 31.

| Mode | Three-bit prefix |
|---|---:|
| AVT24 | `010` |
| AVT90 | `101` |
| AVT94 | `011` |

The decoder accepts a frame only when all 17 tones classify within tolerance,
the second byte is the exact inverse of the first, and the prefix agrees with
the normal VIS mode already selected by the runtime. Any valid protected frame
can recover the remaining time to the image.

Sample boundaries are derived cumulatively. In particular, for countdown
counter `c`, the samples remaining from that frame boundary are calculated as
the rounded total countdown boundary minus the rounded `c`-frame boundary.
Rounding a frame and the remaining duration independently can start the image
one or two samples early and is not used.

The streaming detector retains only the nearest observation for each of the 17
symbols. Its resident candidate state is therefore fixed at 17 slots even
though Decodium's 12 kHz frequency frontend normally emits one observation per
sample.

## Known source divergence

The pinned MMSSTV source starts its exposed AVT90 countdown with the word
`0x5f` followed by `0xa0`. The high prefix of `0x5f` is `010`, which identifies
AVT24 rather than AVT90. Decodium does not copy that defect: native AVT90 uses
the required `101` prefix.

## Catalogue-only variants

The Handbook identifies four header variants for each AVT duration:

| Variant | VIS delta | Header facts currently represented | Execution status |
|---|---:|---|---|
| Normal | 0 | normal countdown tones and 1500-2300 Hz picture range | RX/TX implemented |
| Narrow | 1 | 1700/2100 Hz protected data tones; 1700-2100 Hz picture range described | catalogue only |
| QRM | 2 | interlaced odd/even ordering described | catalogue only |
| Narrow-QRM | 3 | both variant bits present | catalogue only |

The root sync codec can name the variant VIS payloads and narrow countdown
tones. That header knowledge is not promoted to a runnable mode. Narrow, QRM
and Narrow-QRM remain unavailable in RX, TX, automatic selection, WAV export
and Studio until their complete picture ordering, end conditions and legal
interoperability evidence are demonstrated.

## Native Decodium components

The implementation remains inside the existing Decodium SSTV audio and
runtime pipeline:

- `SstvAvtSyncCodec` represents protected countdown words and variant header
  identities;
- `SstvAvtProtocol`, `SstvAvtMapper`, `SstvAvtEncoder` and `SstvAvtDecoder`
  implement the normal wire contract with bounded pull/consume APIs;
- `SstvAvtCountdownDetector` acquires the image phase after normal VIS;
- `SstvAvtRxSession` owns one progressive frame and terminates on completion,
  cancellation or discontinuity;
- the shared TX source/coordinator is used for live audio, PTT lifecycle and
  WAV export;
- the shared RX runtime is used for Decodium capture, resampling,
  preprocessing, frequency demodulation, automatic selection and rendering.

No Python runtime, subprocess, virtual audio cable, external receiver,
external transmitter or separate GUI is part of the subsystem.

Because AVT has no later line anchor, a stream discontinuity closes the current
image as complete or partial. The receiver does not invent a phase and continue
with silently shifted colours.

## Clean-room provenance

The compact source-landmark fixture is
`tests/sstv/fixtures/avt-handbook-qsstv-landmarks.json`.

- SSTV Handbook PDF, creation metadata 2019-11-17, 175 pages, 18,043,795 bytes,
  SHA-256
  `e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d`;
- `ON4QZ/QSSTV@8c27d6d169d8c6c197eb47c2089870e39bc06a02`, audited at
  `src/sstv/sstvparam.cpp` and `src/sstv/modes/modeavt.cpp`;
- `n5ac/mmsstv@8060b5f1e9727b0052d74108081c6db7b26babad`, audited at the
  AVT line and header transmitter paths in `Main.cpp`.

The Decodium C++ implementation was written from the documented behaviour and
independently compared landmarks. No upstream implementation source is copied
into the runtime.

## Test evidence and limits

The isolated tests cover:

- exact triple VIS and 32 x 17 countdown construction;
- correct AVT24, AVT90 and AVT94 prefixes and timings;
- explicit 320 prepared versus 256 effective AVT90 geometry;
- absence of per-line sync/gap regions;
- bounded, chunk-invariant TX and progressive RX for all normal modes;
- every countdown counter at 8 kHz, 12 kHz and 44.1 kHz;
- the actual Decodium frequency frontend at 12 kHz with one-sample hops;
- a complete chunked AVT24 PCM loopback through countdown acquisition and
  progressive frame completion;
- malformed input, wrong-prefix rejection, cancellation and discontinuity;
- strict warning, repeated, AddressSanitizer and UndefinedBehaviorSanitizer
  runs.

The source-landmark fixture is not an independently captured PCM waveform.
The PCM loopback is a Decodium self-test. There has been no live radio,
on-air, hardware, MMSSTV/QSSTV cross-application or propagation verification.
