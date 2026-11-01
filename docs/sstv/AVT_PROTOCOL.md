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

### AVT24 QRM detailed-image A/B

On the current macOS build, the two-application BlackHole test was repeated
with two more detailed colour inputs, converted by the TX studio to the mode's
grayscale raster. They were generated with FFmpeg's `testsrc2` and `smptebars`
at 320×240 and have SHA-256
`bc69dcb27bf2c1879e2e03a778e2aafaeb76d1bfcaf70fdd3b9eb0c69137c45d` and
`99024c802a429ebdf1286c1deec6a32bb893dba12fd7e86a83c9ccf676c1a3e9`.
Both applications exited successfully, coverage was 100%, and the receiver
reported no discontinuities or processing failures. Error is measured against
the transmitter-prepared image, in 8-bit components:

| Mode | Input | Mean absolute error | p95 | Maximum |
|---|---|---:|---:|---:|
| AVT24 QRM | A | 1.866 | 11 | 70 |
| AVT24 QRM | B | 1.079 | 7 | 51 |
| AVT24 Narrow-QRM | A | 1.969 | 12 | 73 |
| AVT24 Narrow-QRM | B | 1.134 | 7 | 54 |

The existing thresholds were MAE 5 and p95 15. These results add detail to
the mostly uniform fixtures above; they do not establish interference or RF
performance. The reusable two-app lab now accepts an optional image path.
Reproduction with FFmpeg, BlackHole and PortAudio:

```sh
ffmpeg -v error -y -f lavfi -i testsrc2=size=320x240:rate=1 -frames:v 1 /tmp/decodium-avt-detail-a.png
ffmpeg -v error -y -f lavfi -i smptebars=size=320x240:rate=1 -frames:v 1 /tmp/decodium-avt-detail-b.png
python3 tests/sstv/tools/run_avt_two_app_lab.py avt-24-qrm a /tmp/decodium-avt-detail-a.png
python3 tests/sstv/tools/run_avt_two_app_lab.py avt-24-qrm b /tmp/decodium-avt-detail-b.png
python3 tests/sstv/tools/run_avt_two_app_lab.py avt-24-narrow-qrm a /tmp/decodium-avt-detail-a.png
python3 tests/sstv/tools/run_avt_two_app_lab.py avt-24-narrow-qrm b /tmp/decodium-avt-detail-b.png
```

The four macOS temporary artifacts, in table order, are
`decodium-avt-two-app-avt-24-qrm-n1jo_3ze`,
`decodium-avt-two-app-avt-24-qrm-qik8xbvt`,
`decodium-avt-two-app-avt-24-narrow-qrm-4r1phzd1` and
`decodium-avt-two-app-avt-24-narrow-qrm-5idfck3p`.

### Controlled in-band interference replay

The captured 48 kHz stereo audio from those four two-app transfers was
subsequently replayed to Decodium through BlackHole with a continuous 1900 Hz
tone added at peak amplitude 0.10. Both signal and tone were then scaled by
0.8, preventing clipping; the active captured signal RMS was about 0.56,
giving a nominal signal-to-tone RMS ratio of about 18 dB. This is a synthetic
single-tone laboratory condition, not an RF measurement. The application
reported complete images, full coverage, zero discontinuities and no
processing failures in all four interfered replays. Against the same prepared
references, however, all four exceeded the previously used MAE 5 limit:

| Mode | Input | MAE | p95 | Maximum | Result against MAE 5 / p95 15 |
|---|---|---:|---:|---:|---|
| AVT24 QRM | A | 6.734 | 15 | 77 | fail |
| AVT24 QRM | B | 5.717 | 14 | 58 | fail |
| AVT24 Narrow-QRM | A | 6.807 | 15 | 86 | fail |
| AVT24 Narrow-QRM | B | 5.820 | 14 | 60 | fail |

The QRM A/B captures replayed without the tone through the same path passed,
with MAE 1.866/1.079, p95 11/7 and maxima 70/51. The interfered artifacts in
table order are `decodium-avt-interference-d7d5urtu`,
`decodium-avt-interference-qst2lwcw`, `decodium-avt-interference-ph0fo18n`
and `decodium-avt-interference-rsi2eo4z` under the macOS temporary directory;
the clean QRM replays are `decodium-avt-interference-9oi8zztg` and
`decodium-avt-interference-yl3h8cwe`. The Narrow-QRM captures also passed
clean replay: A/B MAE 1.969/1.136, p95 12/7, maxima 73/54, artifacts
`decodium-avt-interference-prgxu54_` and `decodium-avt-interference-9q80izcd`.
The test runner is
`tests/sstv/tools/run_avt_interference_replay_lab.py`; it reports the actual
audio device, complete RX status and image error and rejects clipped mixtures.
These failures identify a reproducible quality limit for subsequent receiver
work; image completion alone is not considered a passing quality result.

The same captured QRM A/B images were also replayed with a continuous 1700 Hz
tone at peak 0.10 and output gain 0.8, using the same MAE 5 / p95 15 limits.
Both completed with full coverage and no reported discontinuities, dropped
chunks or processing failures, but both failed the image comparison: A MAE
6.193/p95 21/max 84 and B MAE 5.861/p95 16/max 60. The nominal signal-to-tone
RMS ratios were about 17.98 dB and the mixed peaks about 0.722. Artifacts are
`.../decodium-avt-interference-8sw7secv` and
`.../decodium-avt-interference-ht3froo4`. The replay harness now requires the
BlackHole source and zero dropped chunks, discontinuities and processing
failures in every diagnostic snapshot, and checks the received mode. This
shows a second controlled interference condition needing receiver work; it
does not establish a sensitivity threshold or RF performance.

An offline Q=100 notch at 1700 Hz was then gated during the countdown at
13.8213541667 s for A and 13.8320208333 s for B. With the 0.10 tone it
improved A to MAE 3.416/p95 16/max 93, still outside the p95 15 limit, and
B to MAE 3.045/p95 11/max 66, within both limits. Matched no-tone filtered
controls gave A 3.419/16/93 (fail) and B 3.032/11/66 (pass); unfiltered
clean controls above were 1.866/11/70 and 1.079/7/51. The four replays used
the same 0.8 preparation gain, had no clipping or reported dropped chunks,
discontinuities or processing failures, and completed the expected mode.
Artifacts in order are `.../decodium-avt-interference-mofbry55`,
`.../decodium-avt-interference-6po0dnp7`,
`.../decodium-avt-interference-988jijwy`, and
`.../decodium-avt-interference-kz67wc7m`. This manually chosen filter
suppresses the selected tone but also worsens the clean signal enough for A
to fail; it is not a safe general receiver improvement and was not enabled.

The same diagnostic with a narrower Q=300 notch was compared at identical
gates and levels. With the tone, A measured MAE 2.968/p95 15/max 87 and B
2.136/10/55; matching no-tone filtered controls measured A 2.972/15/87 and
B 2.127/10/55. All four met the existing MAE 5/p95 15 thresholds and had
zero reported discontinuities, dropped chunks and processing failures. A is
exactly at the p95 limit, and both clean controls remain worse than the
unfiltered clean controls. In order, artifacts are
`.../decodium-avt-interference-i7vm3mug`,
`.../decodium-avt-interference-6jov4pnx`,
`.../decodium-avt-interference-6dvrj13u`, and
`.../decodium-avt-interference-5ygo6bu7`. The lab-only preparation helper
now accepts Q up to 500 and its stereo, gate and suppression checks cover
Q=300. These tests know both tone frequency and gate beforehand; they do not
implement automatic detection or establish safe behaviour on other signals.

An offline tone survey (`tests/sstv/tools/analyze_avt_stationary_tone.py`)
measured one-second coherent bins at 20 Hz spacing from 1500 to 2300 Hz in
seconds 20–24 of the same two unfiltered captures, and ranked the minimum
amplitude across four windows. For A, with a synthetic 0.10 peak tone added
at 1700 or 1900 Hz, the added tone was the most persistent bin (minimum
0.0984 or 0.0976). For B, even without interference the 2080 Hz and 1520 Hz
bins had minima 0.2005 and 0.1683; with either added tone they still ranked
above it (the added bin measured approximately 0.098). A strongest-persistent
bin rule would therefore select the wrong frequency for B. The one-second
maximum was also often dominated by image tones. These are two selected
windows, not a general signal classification or online detector. The CTest
`test_sstv_avt_stationary_tone_diagnostic` checks synthetic 1700/1900 Hz A/B,
an intentionally stronger legitimate tone and short-input rejection. No
automatic notch was enabled on this evidence.

An opt-in narrow interference notch has now been added to the 12 kHz SSTV
preprocessor, disabled by default and separately bounded to 900–2500 Hz and
Q 2–500. Component A/B tests check 1700/1900 Hz rejection, nearby-tone
preservation, chunk invariance, reset and invalid configurations. A lab-only
runtime path permits a fixed Q=300 notch at an explicit frequency. The RX
page now exposes a manual switch and 900–2500 Hz selector, applicable only
while reception and replay are stopped. The control warns that image tones
at that frequency are suppressed. It is off by default and is not an
automatic detector. The lab switch uses the same bridge configuration path.

For the captured AVT24 QRM A/B transmissions above, the Decodium receiver
was run over BlackHole with the internal 1700 Hz notch active throughout,
including acquisition, and with a 0.10 peak tone added during replay. A/B
MAE/p95/max were 2.924/14/86 and 2.072/10/54; both met MAE 5/p95 15.
Matched clean replays with the notch enabled gave 2.925/14/86 and
2.067/10/54, also within those limits but worse than the unfiltered clean
results of 1.866/11/70 and 1.079/7/51. No dropped chunks, discontinuities
or processing failures were reported. In order, artifacts are
`.../decodium-avt-interference-fq2c1c0n`,
`.../decodium-avt-interference-toasz_sv`,
`.../decodium-avt-interference-3jshwucw`, and
`.../decodium-avt-interference-xqjb1ie3`. These are replays of previously
captured two-Decodium transmissions, not simultaneous TX/RX with the new
filter, RF, or Windows/Linux validation. Other tones, clean images and modes
may be affected, and the frequency must not be chosen automatically from
the strongest spectral bin.

The internal notch was subsequently tested with simultaneous Decodium TX
and RX rather than replay: a PortAudio relay read BlackHole 16ch, added a
continuous 1700 Hz peak-0.10 tone, applied gain 0.8 and wrote BlackHole 2ch.
With the explicitly enabled lab notch, A/B completed with full coverage,
zero reported discontinuities, dropped chunks or processing failures, and
MAE/p95/max 2.924/14/86 and 2.068/10/54. The relay reported no overruns,
underruns or clipping. Matching runs with the notch disabled completed the
images but failed the same MAE 5/p95 15 comparison: A 6.180/21/84, B
5.864/17/60; the relay again reported no errors. Passing application artifacts
are `.../decodium-avt-two-app-avt-24-qrm-wt3ei63x` and
`.../decodium-avt-two-app-avt-24-qrm-qp520ox9`; relay artifacts for enabled A/B
and disabled A/B are `.../decodium-avt-live-interference-suxybvs7`,
`.../decodium-avt-live-interference-bvd0zjvv`,
`.../decodium-avt-live-interference-jqag9qoa` and
`.../decodium-avt-live-interference-eb5skn3d`. The helper
`tests/sstv/tools/run_avt_two_app_interference_lab.py` preserves both app
logs and relay recordings. These controlled macOS results do not establish
automatic frequency selection, all content or profiles, RF, Linux or Windows.

After adding the manual bridge and RX-page control, the full RX runtime suite
and the QML fixture test passed on macOS ARM64; they check 1700/1900 Hz,
disabling, invalid values and rejection of changes during active reception.
Two further simultaneous application transfers used the same bridge method
to enable the notch: A/B MAE/p95/max were 2.924/14/86 and 2.072/10/54,
respectively, with full coverage, no reported audio drops or discontinuities,
and a clean relay. App artifacts are
`.../decodium-avt-two-app-avt-24-qrm-sto77xah` and
`.../decodium-avt-two-app-avt-24-qrm-jl_0_dtd`; relay artifacts are
`.../decodium-avt-live-interference-1i7o4hx3` and
`.../decodium-avt-live-interference-bh5ewaqs`. The QML controls were tested
with a fixture, not by clicking a visible window during those transfers.
The filter controls now use a stacked layout; the offscreen QML fixture
renders at widths 940 and 640 and checks that the checkbox, frequency and
apply controls remain inside their container. This is not a visual inspection
of the native application or a Windows/Linux UI test.

A further live A/B trial at 1900 Hz exposed a regression: with the notch
applied to the whole signal path, neither application received a valid VIS
or image, despite clean relay operation. The RX runtime now keeps an
unfiltered preprocessor for VIS/FSK tone acquisition when the notch is
enabled, while the frequency demodulator uses the selected notch. The
separate acquisition path is reset on discontinuities. A/B repeated live
transfers then acquired and completed with MAE/p95/max 2.696/11/74 and
2.304/10/52, no reported dropped chunks, processing failures, discontinuities
or relay errors. Failed relay artifacts are
`.../decodium-avt-live-interference-rd_jigo3` and
`.../decodium-avt-live-interference-buke0ri4`; passing application artifacts
are `.../decodium-avt-two-app-avt-24-qrm-ybads8i3` and
`.../decodium-avt-two-app-avt-24-qrm-isy0trmv`, with relay artifacts
`.../decodium-avt-live-interference-a_h5zc5c` and
`.../decodium-avt-live-interference-yz8svso4`. A focused RX runtime test also
verifies AVT VIS/countdown acquisition at 1700 and 1900 Hz with the notch
enabled. This is not proof for every mode, waveform or RF channel.
After the path separation, the full RX runtime, RX frontend and receive-page
QML test suites passed on macOS ARM64.

The same simultaneous two-application test was extended to AVT24
Narrow-QRM with a 1900 Hz, peak-0.10 interfering tone and gain 0.8. With the
manual notch enabled, A/B were complete with MAE/p95/max 3.068/12/81 and
3.537/14/55, within MAE 5/p95 15. With the notch disabled, both images were
complete but failed the MAE limit at 6.799/15/79 and 5.826/14/61. All four
relays reported no overflow, underflow or clipping, and the application
checks found no dropped chunks, discontinuities or processing failures.
Enabled application artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-91omzhlf` and
`.../decodium-avt-two-app-avt-24-narrow-qrm-e4cp82iw`. Enabled and disabled
relay artifacts, in A/B order, are
`.../decodium-avt-live-interference-5f4h3atb`,
`.../decodium-avt-live-interference-8j9w9ybn`,
`.../decodium-avt-live-interference-ga2spm2s`, and
`.../decodium-avt-live-interference-xsym2lca`. These controlled macOS
results do not demonstrate general RF, all images, automatic detection or
other platform behaviour.

#### Image-only notch diagnostic

An offline experiment applied a 1900 Hz, Q=100 notch only after the expected
image start, using the captured QRM A/B transfers. The manually chosen gate
uses prior knowledge of each capture and is not a live receiver algorithm.
All mixtures had zero clipped samples, used the same final 0.8 gain as the
unfiltered comparison above, and produced complete images without reported
discontinuities or processing failures. Compared with the prepared images:

| Input | Notch with tone MAE / p95 / max | Notch without tone MAE / p95 / max |
|---|---|---|
| A | 3.117 / 13 / 78 | 3.119 / 13 / 78 |
| B | 5.744 / 19 / 159 | 2.944 / 10 / 52 |

The interfered B result still fails both existing quality thresholds. The
clean B result is also worse than the unfiltered clean replay, so this
experiment does not justify enabling the notch in Decodium. For B, after
the manually gated notch, the interfered and clean audio differed by RMS
approximately 0.0000061 in a 17–39 second image interval. The resulting
images were more similar when shifted horizontally by one pixel; this is
an indication of alignment sensitivity, not proof of its cause.

Further QRM A/B controls added the same tone only before 16.8813541667 and
16.8920208333 seconds respectively, with no notch and with the subsequent
audio unchanged. A passed with MAE 1.866, p95 11 and maximum 70; B passed
with MAE 1.079, p95 7 and maximum 51. These essentially match the clean
replays. Thus the first-part tone alone does not reproduce the notch result;
the interaction with the filter or its activation timing remains unresolved.
Both replays reported zero discontinuities and their peaks were below 0.722.
Their artifacts are `decodium-avt-interference-03_8ark0` and
`decodium-avt-interference-d7zji6n6` in the macOS temporary directory. These
are diagnostic controls, not a complete interference test.

`tests/sstv/tools/prepare_avt_notch_lab.py` reproduces the offline mixing and
gated filter; `test_sstv_avt_notch_diagnostic` verifies that filtering
actually affects both channels after the gate. This matters because an early
FFmpeg filter experiment silently left one channel layout unfiltered. The
matched-level interfered A/B artifacts are
`decodium-avt-interference-_rnz3y_g` and
`decodium-avt-interference-r_or5mlz`, and their clean counterparts are
`decodium-avt-interference-dds9fg74` and
`decodium-avt-interference-kk5k1qbu` in the macOS temporary directory.

The AVT session unit suite also checks both QRM variants with the same complete
synthetic image versus a missing middle row. A low-signal observation at the
image end closes the first as complete and the second as partial, without
inventing the missing row. This protects coverage reporting; it is not a
claim that the receiver can recover the content of a lost row.

For an application-level A/B control, 0.3 seconds around the middle of the
two QRM captures were silenced with FFmpeg and the files replayed into the
full Decodium receiver over BlackHole. Both runs completed without reported
processing failures or audio discontinuities. Each reported an available,
partial 128×120 image with 15,104 completed pixels and component coverage
0.9861, rather than a complete image. Their artifacts are
`decodium-avt-interference-j8y5dp7o` and
`decodium-avt-interference-_mmad_am`. The replay helper's
`--expect-partial` checks the reported mode, nonzero incomplete coverage and
partial status. These are manipulated recordings from the earlier two-app
transmissions; they are not a physical carrier-loss or RF test.

For a further diagnostic, the same Q=100 notch and 1900 Hz interference were
applied starting earlier in the countdown: at 13.8213541667 seconds for A
and 13.8320208333 for B. Both completed without reported discontinuities or
processing failures and passed the original thresholds: A MAE 3.118, p95 13,
maximum 78; B MAE 2.941, p95 10, maximum 52. The respective peak sample
amplitudes were 0.898 and 0.901. Artifacts are
`decodium-avt-interference-u7izrf2h` and
`decodium-avt-interference-5dog9ien`. Matching no-tone controls with the
same early notch also passed: A MAE 3.118/p95 13/max 78, B MAE 2.940/p95
10/max 52; their artifacts are `decodium-avt-interference-s08cj1jf` and
`decodium-avt-interference-rky3bi6q`. Both clean results are worse than the
unfiltered clean baseline (1.866/1.079 MAE). This timing is chosen with knowledge of
the recordings and of the interferer's frequency. It does not implement live
interference detection or establish that the filter is safe for other tones,
images, AVT modes or propagation conditions.

The same interfered test on AVT24 Narrow-QRM also passed: A MAE 3.757,
p95 14, maximum 83; B MAE 4.323, p95 15, maximum 58. The manually selected
gate times were 13.8213541667 and 13.7680208333 seconds, respectively; peak
amplitudes were 0.926 and 0.952. Both receivers completed without reported
discontinuities or processing failures. The corresponding artifacts are
`decodium-avt-interference-n3pfaals` and
`decodium-avt-interference-55vm_uss`. The matching no-tone controls with
the same filter and gate passed with A MAE 3.752/p95 14/max 83 and B MAE
4.329/p95 15/max 58; their artifacts are
`decodium-avt-interference-jfd1p5t1` and
`decodium-avt-interference-neqr6swi`. Both clean results are worse than the
unfiltered clean baseline (1.969/1.136 MAE). B is exactly at the p95 limit,
leaving little margin in this particular condition. This is not an automatic
or production filter.

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
