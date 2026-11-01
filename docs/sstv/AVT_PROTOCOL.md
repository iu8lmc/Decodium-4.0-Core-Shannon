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

## Narrow and QRM variants

| Variant | VIS delta | Picture range and ordering | Local status |
|---|---:|---|---|
| Normal | 0 | 1500–2300 Hz; consecutive rows | RX/TX integrated |
| Narrow | 1 | 1700–2100 Hz; consecutive rows | RX/TX integrated |
| QRM | 2 | 1500–2300 Hz; even rows, then odd rows | RX/TX integrated |
| Narrow-QRM | 3 | 1700–2100 Hz; even rows, then odd rows | RX/TX integrated |

The [AEA AVT System manual](https://bruxy.regnet.cz/hamradio/SSTV_Library/SSTV_Devices/Amiga_Video_Terminal/ATV_System_Amiga_Video_Terminal_Operating_Manual_scan.pdf),
pages 59–60, describes the QRM ordering. The [SSTV Handbook, section 4.2.5](https://www.sstv-handbook.com/download/sstv_04.pdf)
describes narrow frequencies. The registry, Studio, TX source and automatic RX
carry all twelve VIS identities. Tests compare all nine variant TX streams
against explicitly configured encoders, exercise progressive RX for all twelve
identities, and run full in-process Studio PCM loopbacks for all twelve modes, comparing
the received image with the prepared raster (mean absolute RGB error below 35).
Synthetic tests compare decoded rows against normal AVT24. Local two-instance
virtual-audio evidence is detailed below. Independent application, physical
audio device and RF validation remain pending; no interoperability is claimed.

## Native Decodium components

The implementation remains inside the existing Decodium SSTV audio and
runtime pipeline:

- `SstvAvtSyncCodec` represents protected countdown words and variant header
  identities;
- `SstvAvtProtocol`, `SstvAvtMapper`, `SstvAvtEncoder` and `SstvAvtDecoder`
  implement the variant-aware wire contract with bounded pull/consume APIs;
- `SstvAvtCountdownDetector` acquires the image phase after AVT VIS;
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

### Two-instance local AVT-24 QRM check

On 2026-10-31 an isolated pair of the QML applications used BlackHole 2ch,
with CAT disabled and TX in audio-only VOX. Before the manual-mode fix, the
receiver recognized VIS 66 but did not acquire an image. After the fix, an
otherwise equivalent run reported AVT-24 QRM, progressive image updates and
100% completed-pixel coverage; the transmitter reported completion and both
processes exited with code zero. The RX diagnostics reported two startup
discontinuities before acquisition. This is a real two-process virtual-audio
check, not RF or an independent implementation; the image pixels were not
exported for a reference comparison. The lab logs for this run were written to
`/tmp/decodium-sstv-fixed.M6VCz0/` on the test machine and are ephemeral.

Manual AVT mode constrains the selected VIS but still acquires the protected
countdown: unlike line-synchronized modes, it must not wait for a physical
line sync before starting the decoder.

A second offscreen two-process run on the same date exported the prepared TX
image and completed RX snapshot as PNGs using the test-mode-only lab options
`--lab-sstv-tx-image-file` and `--lab-sstv-rx-image-file`. Both applications
exited zero, with TX completion and RX image completion recorded. The source
and received images were 128×120; the per-channel absolute error was 1.890 on
average, median 0, 95th percentile 9, and maximum 70 (8-bit components). The
comparison passed `--max-mae 5 --max-p95 15`; self-comparison returned zero and
a deliberately tighter MAE limit of 1 rejected the received image. These
limits are lab acceptance thresholds, not an external protocol specification.
The temporary artifacts were `/tmp/decodium-sstv-pixels.dvGApF/reference.png`
and `received.png`. Recompare with:

```sh
python3 tests/sstv/tools/compare_sstv_lab_images.py \
  /tmp/decodium-sstv-pixels.dvGApF/reference.png \
  /tmp/decodium-sstv-pixels.dvGApF/received.png --max-mae 5 --max-p95 15
```

The comparison requires ffmpeg and ffprobe. It validates this generated
calibration raster and channel path, not all pictures, QRM conditions, or RF.

### A/B application runs with AVT QRM variants

On 2026-10-31 the reusable `tests/sstv/tools/run_avt_two_app_lab.py`
started two isolated Decodium processes with CAT disabled, audio-only VOX,
manual RX mode, and BlackHole 2ch. It required a completed TX, complete RX
image and a comparison against the TX-prepared PNG with limits MAE 5 and
p95 15. The two input images are different; for the AVT24 QRM prepared
rasters their pixel-wise mean absolute difference was 158.75/255.

| Mode | A MAE / p95 | B MAE / p95 | Coverage / discontinuities |
|---|---:|---:|---|
| AVT24 QRM | 0.354 / 0 | 0.354 / 0 | 100% / 0 for each |
| AVT24 Narrow-QRM | 0.395 / 0 | 0.396 / 0 | 100% / 0 for each passing run |
| AVT90 QRM | 0.174 / 0 | 0.174 / 0 | 100% / 0 for each |

These values are 8-bit component errors. Maximum errors were 136 for
AVT24 QRM, 255 for AVT24 Narrow-QRM, and 67 for AVT90 QRM. The grayscale
input fixtures contain large uniform regions, so their low averages do not
characterize all images. Artifact directories are:

- AVT24 QRM A/B: `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-24-qrm-yauzebbz` and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-24-qrm-0i9l3xjd`.
- AVT24 Narrow-QRM A/B: `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-24-narrow-qrm-3f1obiyt` and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-24-narrow-qrm-au93pkot`.
- AVT90 QRM A/B: `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-90-qrm-qan4r36n` and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-90-qrm-gizb2ii9`.

One AVT24 Narrow-QRM B run failed before VIS recognition: TX completed,
audio was observed, and RX reported zero discontinuities and processing
failures, but no image was acquired. Its logs are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-24-narrow-qrm-1oebnbnl`.
A retry passed, as did a subsequent B run with simultaneous raw audio
capture (`.../decodium-avt-two-app-avt-24-narrow-qrm-gqbqsqmn`). The cause
of the failed acquisition remains undiagnosed. The reusable helper now
records 48 kHz stereo float PCM even when a run fails. These tests do not
establish reliability, interference tolerance, cross-application or RF
interoperability.

### VIS start-boundary correction and AVT90 Narrow-QRM A/B

A subsequent AVT90 Narrow-QRM run also failed to acquire the image, although
the TX completed and the RX reported neither audio discontinuities nor
processing failures. The full two-process capture and logs are at
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-90-narrow-qrm-3oamp6_u`.
In replay, removing 96 of the 48 kHz samples before feeding the same audio
changed a successful detection into a rejection: the 5 ms FFT windows
classified the first VIS data bit 5 ms early, leaving a measured 25 ms start
separator and causing the derived symbol clock to exceed its drift bound.
The runtime's existing boundary correction now accepts up to two 2.5 ms
observation hops for this transition. Normal VIS bit, parity, mode and stop
validation remain required.

The 12-second, mono 48 kHz PCM16 excerpt from the failed capture is stored
as `tests/sstv/fixtures/avt-90-narrow-qrm-captured-48k.s16`, SHA-256
`ba3e6669fd292e4dc98554ac82c3b5421249746998387e43bc46944ff0586b20`.
`avtCapturedHeaderPhaseRegression` replays it at offsets 0 and 96; before the
fix the latter failed, after the fix both detect VIS 71 and acquire an image.
The same diagnostic also passed offsets 24, 48, 192, 384 and 768. This
specific phase-dependent defect is reproduced and corrected; the earlier
AVT24 Narrow-QRM failure had no audio capture, so its cause is not proven.

After rebuilding Decodium, two fresh AVT90 Narrow-QRM transfers through
BlackHole completed with 100% image coverage, no reported discontinuities or
processing failures, and MAEs 0.1870 and 0.1871, p95 0, maximum error 69.
Artifacts are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-90-narrow-qrm-kpo7zjy_`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-90-narrow-qrm-2c7gg11w`.
Six focused VIS/AVT test targets passed; a runtime selection covering the
new captured regression, AVT, Scottie and Robot passed five test functions.
These are local clean-audio tests, not a measure of RF or interference
performance.

### AVT94 QRM and Narrow-QRM A/B

With the corrected VIS runtime, two isolated Decodium applications exchanged
two different images for each AVT94 variant through BlackHole. All four
transmitters completed, each receiver produced a complete 320×200 image with
100% coverage, zero reported discontinuities and zero processing failures.
For QRM the MAEs were 0.1425 and 0.1419, p95 was zero and the maximum
component error was 107. For Narrow-QRM the MAEs were 0.1576 and 0.1576,
p95 was zero and the maximum was 222. Acceptance limits were MAE 5 and p95
15. These mostly uniform test fixtures do not demonstrate visual quality
for all images or performance in interference.

The QRM A/B artifacts and captured audio are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-94-qrm-72sfodz4`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-94-qrm-_yefgnb7`;
the Narrow-QRM A/B artifacts are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-94-narrow-qrm-yivlumwc`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-avt-two-app-avt-94-narrow-qrm-hsvntina`.
All six AVT24/90/94 QRM and Narrow-QRM identities now have at least one
successful local A/B image comparison, but the prior failed acquisition,
independent cross-application testing, realistic interference and RF
validation remain relevant limits.
