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
virtual-audio evidence is detailed below. A selected file-mediated A/B exchange
with the separate Revenant codec is documented below; physical audio devices,
RF and broad independent interoperability remain unverified.

## AVT24 QRM A/B with the Revenant codec

### AVT24 Narrow A/B without QRM interlacing

The same pinned Revenant codec was also compared in the separate AVT24
Narrow mode, VIS 65. Two Decodium process pairs sent detailed A/B colour
patterns over BlackHole with complete coverage and no reported audio
discontinuities: MAE 1.265/1.437 and p95 5/6. Artifacts are
`.../decodium-avt-two-app-avt-24-narrow-2ujmeqan` and
`.../decodium-avt-two-app-avt-24-narrow-nw92osg_`.

Revenant decoded their captured audio as AVT24 Narrow, 120 rows, with A/B
MAE 0.896/1.052 and p95 10/13. In the reverse direction its generator encoded
the prepared images; its own A/B controls passed at MAE 0.916/1.046, p95
10/13. Replays into full Decodium receivers passed with complete images,
no reported discontinuities, dropped chunks or processing failures, and MAE
1.307/1.468, p95 3/6. Artifacts are
`.../decodium-avt-interference-i79gqa3t` and
`.../decodium-avt-interference-_pn_hpyw`; generated audio and decoder output
are in `/tmp/decodium-avt24-narrow-independent`. These selected file-mediated
comparisons use the codec adapter, not a live external GUI or RF channel.
The two-app and replay helpers now also accept AVT Narrow mode IDs.

A separately maintained open-source project,
[Revenant](https://github.com/Locke-Werks/Revenant), provides AVT QRM
transmit and receive code. At pinned commit
`67da9f8767b2a250792a6f5a5b2a69f5bc992b6d`, its codec and signal generator
were compiled unchanged with the small Decodium laboratory adapter
`tests/sstv/tools/revenant_sstv_oracle.cpp`. The source comments cite the
SSTV Handbook; shared specifications do not by themselves establish independent
implementation provenance. The project codec was used through this adapter,
not its complete GUI application. The codec source SHA-256 values for
`core/decode/sstv.cpp` and `core/dsp/synth/sstv_mod.cpp` were
`997cff83edd61da15bcf961a69810e825aa8faaeb7d3e9a284d5c34605735dc9` and
`2dbecf44ff3cf67571fbf4aa37b40c5189a35f1e1c0d294cf55093eeb0a5a467`.

As self-controls, the Revenant transmitter and receiver processed the two
128×120 detailed prepared images. Both detected `AVT 24 QRM`, VIS 66,
completed 120 rows, and passed the full-raster MAE 5 / p95 15 limits:
A/B MAE was 0.943/1.087 and p95 was 10/13. This confirms the adapter and
selected codec path on those inputs, not every supported mode.

For Decodium TX to Revenant RX, two Decodium application pairs transmitted
A and B through BlackHole with full reported coverage and no RX
discontinuities. The unedited captured stereo audio was converted to mono
and decoded by Revenant. Both outputs reported AVT24 QRM, VIS 66, complete
120 rows and full-raster A/B MAE 0.931/1.087 and p95 11/13 against the
Decodium TX-prepared images. Decodium's own A/B RX comparisons were
1.211/1.387 MAE and 4/6 p95. Artifacts are
`.../decodium-avt-two-app-avt-24-qrm-gfj3m9ar` and
`.../decodium-avt-two-app-avt-24-qrm-y8u28_2q`; the separately decoded
PPMs and converted audio are under `/tmp/decodium-revenant-avt-test`.

For Revenant TX to Decodium RX, the independent generator encoded those same
two prepared images. Its mono audio was duplicated to stereo and played at
gain 0.8 through BlackHole into full Decodium receivers. Both A and B reached
complete coverage without reported discontinuities, dropped chunks or
processing failures; full-raster MAE was 1.300/1.429 and p95 3/6. Artifacts
are `.../decodium-avt-interference-iknmd13t` and
`.../decodium-avt-interference-nhayrh92`. The test used zero added tone.

The exact source revision can be cloned separately and the adapter rebuilt
with `tests/sstv/tools/build_revenant_sstv_oracle.sh REVENANT_DIR OUTPUT`;
the script rejects a different commit or modified codec files. Render accepts
raw RGB pixels and outputs 48 kHz float mono, while decode accepts float
mono and writes PPM (colour) or PGM (grayscale): `OUTPUT render IMAGE.rgb AUDIO.f32 'AVT 24 QRM'` and
`OUTPUT decode AUDIO.f32 IMAGE.ppm 'AVT 24 QRM'`. RGB input must match the
mode dimensions. These are selected A/B, file-mediated codec comparisons on
macOS with local audio playback; they are not live GUI-to-GUI, RF, physical
audio or proof of the other QRM/Narrow variants; the AVT24 Narrow-QRM follow-up is below.
`python3 tests/sstv/tools/test_revenant_sstv_oracle.py OUTPUT` runs two
generated AVT24 QRM self-controls and rejects crossed-image and wrong-mode
controls; the build wrapper also rejects an unexpected source revision.
After extending the audio replay helper to Dayton FAX480, its AVT24 QRM A/B
replays were repeated successfully: `.../decodium-avt-interference-8kfefzu7`
and `.../decodium-avt-interference-fovb9k_y`.

After adding the 10 ms terminal-tone hold described below, two fresh
AVT24 QRM application pairs transmitted detailed A/B images. Decodium RX
reported complete coverage and no discontinuities; MAE/p95 were 1.211/4 and
1.387/6. A freshly rebuilt adapter from the pinned Revenant revision decoded
the recorded audio, recognised VIS 66 and completed all 120 rows in each run.
Against the TX reference, Revenant MAE/p95 were 0.940/10 and 1.084/13,
respectively. Its A/B self-controls and crossed-image/wrong-mode negatives
also passed. Application artifacts are
`.../decodium-avt-two-app-avt-24-qrm-m591j5fm` and
`.../decodium-avt-two-app-avt-24-qrm-nhe52s9y`; converted audio and decoded
images are `/tmp/decodium-avt-qrm-current-hold-check`. This is a file-mediated
independent codec check for the current TX path, not a live external GUI or
proof of RF behaviour or every AVT variant.

### AVT24 Narrow-QRM A/B with the same codec

The same pinned Revenant codec and detailed A/B images were exercised in
AVT24 Narrow QRM. Its self-generated and self-received images identified
VIS 67, completed 120 rows, and measured A/B MAE 0.906/1.045 and p95 10/13.
Decodium TX to Revenant RX, via separately captured unmodified audio
converted from stereo to mono, also identified VIS 67 and completed 120
rows; A/B MAE was 0.921/1.046 and p95 10/13. Decodium's own two-application
RX comparisons on those transmissions measured 1.274/1.786 MAE and 5/7 p95,
with complete coverage and no reported discontinuities. Artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-f2miizdk` and
`.../decodium-avt-two-app-avt-24-narrow-qrm-bxnqknqm`.

In the reverse direction, the Revenant generator encoded both prepared
images; its float audio was duplicated to stereo and replayed at gain 0.8
through BlackHole into full Decodium RX. Both completed with no reported
discontinuities, dropped chunks or processing failures. Full-raster A/B MAE
was 1.321/1.512 and p95 was 3/6. Artifacts are
`.../decodium-avt-interference-ezskabr1` and
`.../decodium-avt-interference-labc_rze`; source audio and decoded PPMs are
under `/tmp/decodium-revenant-avt-test`. The same 5 MAE / 15 p95 limits
were used throughout. These are selected file-mediated A/B codec checks, not
live GUI-to-GUI, RF, or evidence for AVT90/94 variants.

### AVT90 Narrow A/B with different horizontal resolutions

The separate AVT90 Narrow mode (VIS 69) was also checked A/B in both
directions. Two Decodium session pairs exchanged 320x240 prepared images,
with complete coverage and no reported audio discontinuities; MAE was
0.638/0.736 and p95 1/3. The A maximum component error was 255, so the
aggregate metric does not imply pixel-perfect output. Application artifacts
are `.../decodium-avt-two-app-avt-90-narrow-8pprps1a` and
`.../decodium-avt-two-app-avt-90-narrow-sfz3lgml`.

The pinned Revenant decoder identified both captured signals as AVT90
Narrow, producing complete 256x240 images. Against the explicitly bilinear
resized Decodium references, MAE was 0.207/0.451 and p95 1/2. Its generator
and decoder self-controls passed at MAE 0.249/0.303 and p95 2/3. Those
generated signals were replayed to Decodium, with references resized back to
320x240: both images completed with no reported discontinuities, dropped
chunks or processing failures, MAE 0.665/0.732 and p95 2/4. RX artifacts are
`.../decodium-avt-interference-t3vj4fen` and
`.../decodium-avt-interference-o9fad7sd`; files are in
`/tmp/decodium-avt90-narrow-independent`. These file-mediated codec tests
include explicit resampling and do not prove native-width pixel equivalence,
live GUI-to-GUI interoperability or RF operation.

One earlier Decodium AVT90 Narrow A capture had a final blue component of
255 where the prepared reference was 0, despite complete reported coverage.
The TX coordinator now also appends the existing 10 ms final-tone hold to
normal and Narrow AVT modes; the core encoder still defaults to no hold.
All twelve AVT builder variants are compared with their direct encoders and
the coordinator and core AVT suites pass.

Two fresh process-pair tests used 320x240 images with the final blue
component 0 and 255 respectively. Both received the exact terminal value,
with complete coverage, no reported audio discontinuity and A/B MAE
0.769/0.953, p95 1/1. Artifacts are
`.../decodium-avt-two-app-avt-90-narrow-mwrsub3a` and
`.../decodium-avt-two-app-avt-90-narrow-7l6yaclc`. The pinned Revenant
decoder also completed both captures, VIS 69, with explicitly resized
256x240 references: MAE 0.289/0.916, p95 1/2. The input images differ from
the earlier failure and the runs are not sample-identical, so they do not
prove the hold caused the improvement or cover all audio devices or RF.

The original failing A input and its B control were subsequently retransmitted
with the final-tone hold. Prepared-reference SHA-256 values matched their
respective earlier runs. The final blue value in A was 0/0 received/reference
instead of the earlier 255/0; B remained 0/0. Both passed the terminal error
limit of 20, completed with zero reported discontinuities, and had MAE
0.636/0.736 and p95 1/3. Artifacts are
`.../decodium-avt-two-app-avt-90-narrow-d45x19ji` and
`.../decodium-avt-two-app-avt-90-narrow-709popea`. These are separate
real-time transmissions of identical prepared rasters, not sample-identical
audio replays, so runtime timing differences remain a limitation.

Revenant also decoded those same new A/B captures as VIS 69, with complete
256x240 output and MAE 1.963/0.690, p95 3/2 against bilinearly resized
references (`/tmp/decodium-avt90-identical-input-oracle`). To isolate the
terminal effect, diagnostic copies of the two original recordings replaced
only the first 10 ms at the measured black-to-VOX boundary with the preceding
black tone. Byte comparisons confirmed every other sample remained unchanged.
Full Decodium replays of original versus modified A produced last blue 255
versus 0 (reference 0); B produced 0 in both cases. All four completed with
no reported audio discontinuities. Artifacts in that order are
`.../decodium-avt-interference-8dskxzye`,
`.../decodium-avt-interference-ruq3di_f`,
`.../decodium-avt-interference-u5u6vlhv` and
`.../decodium-avt-interference-fi2scte5`. Playback timing still differs
between real-time replays, and this selected manipulation is not an RF test.

The replay helper now accepts the same optional
`DECODIUM_LAB_AVT_MAX_LAST_BLUE_ERROR` guard as the two-application helper.
With a limit of 20, the modified-boundary A/B recordings were replayed again
and both reported 0/0 for the last blue component while passing the complete
image and audio checks. Artifacts are
`.../decodium-avt-interference-vk8nmm7i` and
`.../decodium-avt-interference-37opd1de`. The comparison helper's A/B negative
controls for opposite terminal values also passed. This is a laboratory
guard; it does not claim that every pixel is correct.

### AVT90 QRM A/B with different horizontal resolutions

The same independent codec was tested on AVT90 QRM. Decodium prepares 320×240
pixels, while the pinned Revenant codec uses 256×240; both use the same
125 ms per component. Therefore these comparisons require explicit bilinear
resizing and do not establish pixel-for-pixel equivalence at native width.

Decodium A/B transmissions through two local sessions completed without
reported discontinuities, with full-raster MAE 0.630/0.996 and p95 1/3
at 320×240. Revenant decoded both separately captured, mono-converted audio
files as AVT90 QRM, VIS 70, with complete 256×240 images. Against the
Decodium prepared references explicitly resized to 256×240, A/B MAE was
0.973/0.155 and p95 2/1. Decodium artifacts are
`.../decodium-avt-two-app-avt-90-qrm-w37bhgig` and
`.../decodium-avt-two-app-avt-90-qrm-cu5qo074`.

For the reverse direction, the 256×240 resized references were encoded by
the Revenant generator; its own decoder completed both self-controls with
MAE 0.257/0.310 and p95 2/3. The same generated audio was replayed via
BlackHole into Decodium. Against the 256-pixel inputs explicitly resized to
320 pixels, both images completed with A/B MAE 0.607/0.721 and p95 2/4,
without reported audio discontinuities, dropped chunks or processing
failures. Artifacts are `.../decodium-avt-interference-fzfti51s` and
`.../decodium-avt-interference-vn60z7b5`; generated signals and PPMs are in
`/tmp/decodium-revenant-avt-test`. Limits were MAE 5 and p95 15. These
selected file-mediated checks do not cover RF, live GUI-to-GUI operation,
or native-width image equivalence.

### AVT90 Narrow-QRM A/B with different horizontal resolutions

The same A/B comparison was repeated for AVT90 Narrow QRM, with the same
explicit bilinear conversion between Decodium's 320 and Revenant's 256
columns. Both Decodium-to-Decodium images completed with MAE 0.642/0.737 and
p95 1/3. The separately captured audio was decoded by Revenant as AVT90
Narrow QRM, VIS 71, with complete 256×240 images; against the references
resized to 256 columns the A/B MAE was 0.206/0.451 and p95 1/2. Decodium
artifacts are `.../decodium-avt-two-app-avt-90-narrow-qrm-e1gbtpja` and
`.../decodium-avt-two-app-avt-90-narrow-qrm-ik63e0pv`.

The independent generator and receiver self-controls completed A/B with MAE
0.249/0.303 and p95 2/3. Its two signals then replayed through BlackHole into
full Decodium RX with complete coverage, no reported discontinuities,
dropped chunks or processing failures, and MAE 0.665/0.734, p95 2/4
against the 256-column inputs resized to 320. Artifacts are
`.../decodium-avt-interference-nm88vu59` and
`.../decodium-avt-interference-zjrjb70e`; generated signals and PPMs are in
`/tmp/decodium-revenant-avt-test`. These A/B results remain file-mediated,
resized comparisons, not live GUI-to-GUI or RF evidence.

### AVT94 Narrow A/B between Decodium applications

Two distinct images were transmitted through BlackHole between separate
Decodium applications in AVT94 Narrow, with full 320×200 coverage and no
reported audio discontinuities. A/B whole-image MAE was 0.127599/0.127688,
p95 was 0/0, and maximum component error was 58/58. Prepared reference PNGs
had distinct SHA-256 values `362d80384a34c9e9be34d636519be592a9dcabb6196b4048c1d30de70098862c`
and `e30845fe7326926a4c699a0b6e37d4844552c6d5dce373e80abe4fca6a24e54a`.
Artifacts are `.../decodium-avt-two-app-avt-94-narrow-15btpbb_` and
`...-v_6zqy4m`. The selected input images are not exhaustive, edge maxima
were not gated, and this is virtual-audio Decodium-to-Decodium evidence,
not independent codec, external application or RF verification.

The captured A/B signals were then decoded by the pinned Revenant codec
rebuilt from revision `67da9f8767b2a250792a6f5a5b2a69f5bc992b6d` after
checking its codec sources. Both were identified as AVT94 Narrow, VIS 73,
and completed all 200 rows. Against the corresponding prepared images,
A had MAE/p95/max 0.510/2/38 and passed the exploratory 5/15 gates; B had
6.928/9/80 and failed the MAE gate. The crossed B-versus-A comparison had
MAE 109.183. Revenant's own generated A/B signals decoded to MAE 0.099/0.099
and p95 0/0 against the same respective inputs. Its separate general A/B
self-controls and wrong-mode/crossed negatives also passed. These results
show a selected independent-decoder discrepancy for B, without identifying
its cause or proving an implementation defect. The decoded files and logs
are under `.../decodium-avt94n-independent.91JDTR`; no RF is involved.

The diagnostic adapter now also prints the independent decoder's reported
frequency offset without changing its codec. For the recorded A/B signals it
reported +3.502/-13.128 Hz respectively, versus effectively zero for each
of its own self-generated signals. Re-decoding with this print-only change
produced byte-identical A/B PPM files and the existing self/negative controls
passed. On B, the reference red and green channels are both zero, whereas
the independently decoded values are 8 or 9 throughout those channels.
This is consistent with a frequency-estimation difference but does not prove
why the offset estimates differ or establish which side is responsible.

As a diagnostic only, a temporary copy of the Revenant decoder was rebuilt
with its measured VIS-leader offset forced to zero; the captured signals
were unchanged. For A/B it then produced MAE 0.100/0.119, p95 0/0 and
maxima 36/76. The separate generic A/B self-controls and crossed-image /
wrong-mode negatives passed. Fourier inspection found stable 100 ms windows
near 1900 Hz in both captures, but does not establish which exact window the
decoder used. This links the B discrepancy to that decoder's offset value on
these recordings; it does not justify disabling frequency correction for real
signals or turn B into an unmodified-decoder pass. The diagnostic source and
binary are `/tmp/decodium-revenant-zero-offset.cpp` and
`/tmp/decodium-revenant-zero-offset`; output PPMs are in the artifact
directory above.

In the reverse file-mediated direction, unmodified Revenant generated A/B
AVT94 Narrow signals from the same distinct prepared images. Both were
replayed through BlackHole to full Decodium receivers and completed with no
reported discontinuities; MAE/p95 was 0.128/0 for each, with isolated
maximum component errors of 77. Crossed-reference comparisons failed the
5/15 gates as intended, with MAE about 105.8. The received artifacts are
`.../decodium-avt-interference-2tfpwq5m` and `...-qa6bbp4x`. This verifies
selected independent signals through application RX; it does not establish
live GUI-to-GUI or RF operation or make the original captured B a passing
unmodified Revenant RX result.

As an input-alignment control, one and three silent 48 kHz samples were
prepended to each captured A/B signal before decoding with unmodified
Revenant. For A the estimated offsets were +3.993/+3.502 Hz and MAE
0.713/0.510; for B they were -13.127/-13.128 Hz and MAE 6.928/6.928.
Both B checks remained outside the 5-level exploratory MAE gate. The original
audio bytes after the inserted silence were unchanged. Artifacts are
`.../decodium-avt94n-alignment-7ln60uwe`. These two alignments do not explain
the offset discrepancy, nor rule out other acquisition timing effects.

A trace-only build of the pinned independent decoder located its VIS-leader
measurement windows. Each original capture contains exactly 512 consecutive
zero samples (10.667 ms at 48 kHz) within that window: at 10.496 s for A and
10.005333 s for B. On diagnostic copies only, those zeros were replaced with
a 1900 Hz sinusoid fitted to the preceding 50 ms; bytes outside the interval
were checked identical to the originals. The unmodified decoder then reported
offsets approximately zero and MAE/p95 0.100/0 and 0.119/0, respectively.
This isolates the observed offset estimates on these captures to the missing
leader samples and the decoder's handling of them. It does not establish why
the zeros were recorded, prove normal receiver behaviour on real dropouts, or
turn the original B recording into a passing unmodified-decoder result.
The filled diagnostic audio and images are in the artifact directory above;
the trace-only source is `/tmp/decodium-revenant-trace-offset.cpp`.

`tests/sstv/tools/check_avt_capture_integrity.py` now reports exact internal
stereo-zero runs of at least 480 frames, ignoring initial and final silence;
`--fail-on-gap` makes such a finding fail a capture-integrity check. On the
original A/B captures it correctly reported the 512-frame runs at 503808 and
480256. On the two diagnostic filled captures converted to stereo it reported
no internal zero runs. Internal silence can be intentional, so this check
flags evidence requiring investigation rather than diagnosing its source. RX
diagnostics and PortAudio capture status had reported no errors; that does not
identify whether the silence arose in TX, the virtual device or capture.
The two-application laboratory now runs this capture check after closing the
capture and labels receiver-reported discontinuities separately. Fresh AVT24
Narrow-QRM A/B transfers both completed with full coverage and zero reported
RX discontinuities, but their captured audio each contained an internal
512-frame zero run (at frames 513536 and 496640). Artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-_in2_qlr` and `...-hn0uxnhn`.
The successful image results and capture findings describe different paths;
these observations do not locate the source of the silence.

For an A/B source-side control, `render_avt94_native` was extended to render
AVT94 Narrow directly from the same two prepared RGB images. Neither direct
encoder output contained an internal run of 480 or more exact zero frames.
The unmodified pinned Revenant decoder completed both with approximately zero
estimated offset and MAE/p95 0.100/0 for each; maximum component error was 36.
The existing-output guard also rejected an overwrite. These direct encoder
files and PPMs are in `.../decodium-avt94n-independent.91JDTR`. This excludes
the direct encoder as the cause for these inputs and pull sizes, but does not
exercise the live TX coordinator, sink, virtual device or capture; it cannot
identify which later stage produced the missing 512-frame blocks.

The coordinator integration suite now also reads the first two seconds of
AVT94 Narrow through the real `SstvTxAudioDevice` with a 750 ms VOX pre-key,
for red and blue A/B inputs. Neither stream contains a 480-frame zero run;
the complete coordinator CTest passed. This exercises the in-process VOX,
protocol source and QIODevice under the test's pull pattern. It does not
exercise SoundOutput, the operating-system sink, virtual audio or PortAudio,
and does not reproduce or exclude runtime scheduling-dependent faults.

As a separate virtual-device control, two three-second 48 kHz stereo tones
at 1900 and 1700 Hz were played and captured using a PortAudio full-duplex
stream on BlackHole 2ch, without Decodium. Both captures had nonzero signal,
no PortAudio status flags and no internal exact-zero run of 480 frames or
more. Artifacts are `.../decodium-blackhole-control-xn0x2g42`. This does
not reproduce the application arrangement: the original tests used Qt
SoundOutput and a separate PortAudio input stream for substantially longer.
It only shows that the selected full-duplex control did not reproduce the gap.

An attempted separate-input/separate-output PortAudio control in a single
Python process was inconclusive: CoreAudio reported `-10863` (cannot do in
current context), the blocking capture did not stop within five seconds, and
the isolated process was terminated. No comparison capture was produced. This
observation is limited to that control arrangement and does not establish the
cause of the 512-frame gaps or a general BlackHole incompatibility.

A separate bounded control used a standalone Qt `QAudioSink` with a QBuffer
to play three-second 1900/1700 Hz stereo tones through BlackHole while a
PortAudio input callback captured them in another process. Both Qt runs
consumed the complete buffer with `NoError`; capture reported no input flags,
the expected tone frequencies were observed, and neither recording contained
an internal exact-zero run of 480 frames or more. The temporary control source
is `/tmp/decodium-qt-tone-control.cpp`, with captures under
`.../decodium-qt-audio-control-k0g96jty`. Unlike the Decodium test this did
not use its streaming QIODevice, legacy backend or concurrent RX application;
it does not exclude a scheduling or interaction-dependent issue there.

The standalone Qt/PortAudio control was repeated with a sequential tone
QIODevice returning at most 4096 frames per read instead of QBuffer. Both
1900/1700 Hz A/B outputs delivered 144000 frames without Qt or capture
errors; frequency checks passed and neither capture had an internal exact-zero
run of 480 or more frames. Source is `/tmp/decodium-qt-stream-control.cpp`
and captures are `.../decodium-qt-stream-control-stn7gyi8`. This is a simple
synthetic device with a bounded pull, not the actual `SstvTxAudioDevice` or
the complete legacy application setup, and is not a long-duration test.

Another A/B synthetic control started a second Qt audio input one second
after starting the sequential output, while PortAudio captured independently.
Both Qt input and output reported no error; the input reported active and
250 ms processed (its buffer was not drained). The 1900/1700 Hz outputs
completed and neither capture contained an internal exact-zero run of at
least 480 frames. Artifacts are
`.../decodium-qt-second-input-verified-cqpsl6eu`; temporary source is
`/tmp/decodium-qt-stream-second-input.cpp`. This short control does not
simulate the continuously drained Decodium legacy input or its application
startup, so it cannot exclude an interaction there.

The second-input control was then repeated with `readyRead` draining its Qt
input throughout the test. A/B input processing reached 2.84/2.68 seconds,
Qt and PortAudio reported no errors, and neither capture had an internal
exact-zero run of 480 or more frames. Artifacts are
`.../decodium-qt-drained-input-5olklwyo`; source is
`/tmp/decodium-qt-stream-drained-input.cpp`. This remains a short synthetic
Qt input and does not use Decodium's macOS AudioQueue legacy input, its
startup timers or a concurrent full receiver application.

A further isolated control replaced the Qt input with a native macOS
AudioQueue input matching the legacy path's 48 kHz stereo Int16 format,
four 960-frame buffers and BlackHole UID; it started one second after the Qt
tone output while PortAudio captured independently. Native callbacks ran and
Qt, AudioQueue and PortAudio reported no errors in all four A/B trials. One
of four captures (B in the first pair) contained a 7680-frame / 160 ms exact
zero run approximately 0.94 seconds after its first nonzero output; the other
three contained no internal runs of 480 or more frames. The first pair's
artifacts are `.../decodium-qt-native-input-tvatki5j`, the repeat is
`.../decodium-qt-native-repeat-fw4vpg_e`, and the temporary source is
`/tmp/decodium-qt-native-input.cpp`. This reproduces an intermittent capture
gap in a standalone native-input/Qt-output setup near the scheduled input
start, not the original 512-frame gap or its precise cause. It does not
establish whether the interruption is in Qt output, AudioQueue, BlackHole,
PortAudio capture, or their interaction.

An A/B startup-timing experiment delayed Decodium's lab TX request from 4 to
14 seconds. The TX logs show its legacy input starting before the TX request,
whereas it started during TX in the earlier 4-second A/B runs. Both delayed
AVT24 Narrow-QRM images completed with full coverage and no reported RX
discontinuities; both captures had no internal zero run of 480 or more
frames. This result was observed once with a temporary lab copy and repeated
once using the updated lab script; final artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-4i2nthqw` and `...-olc7vsfe`.
The earlier 4-second runs each contained a 512-frame zero run. These limited
observations associate the capture gap with startup timing but do not prove
which component produced it or guarantee the delay prevents every gap.
The two-application lab now defaults to 14 seconds and supports
`DECODIUM_LAB_AVT_TX_MS` from 1000 through 30000; it still reports capture
integrity separately. The timing change is test-only and does not delay
normal product TX.

The earlier lab's `--lab-no-monitor` was found to stop current monitoring
but not suppress a pending startup auto-monitor timer. In test mode only,
the option now sets an application property checked by that timer, so the
transmitter lab does not start its legacy input after TX begins. Fresh early
(4-second) AVT24 Narrow-QRM A/B application pairs both reported the suppression
marker, did not log a transmitter `SoundInput::start`, completed their images,
and passed the opt-in continuous-capture check. Artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-ol6von52` and `...-itnjpvsh`.
This corrects a test-mode startup race and supports the startup-interaction
hypothesis for these runs; it does not prove a general audio root cause or
change ordinary startup and TX behavior.

The lab now offers `DECODIUM_LAB_VERIFY_MONITOR_ISOLATION=1` to check that
the TX startup-suppression marker is present, TX did not start SoundInput,
and the RX monitor was requested and active. With that check and the
continuous-capture gate both enabled, early-start AVT24 Narrow-QRM A/B
transfers passed with complete RX images and no internal exact-zero runs.
Artifacts are `.../decodium-avt-two-app-avt-24-narrow-qrm-oz3_xf3l` and
`...-4ktgxzo7`. This verifies the lab's monitored RX and suppressed TX
startup in these runs, not ordinary production startup or physical audio.

The same two-application lab optionally enforces the capture check with
`DECODIUM_LAB_AVT_REQUIRE_CONTINUOUS_CAPTURE=1`; it rejects disabling capture
at the same time and prints PASS only after the integrity check succeeds.
Fresh AVT24 Narrow-QRM A/B process-pair runs passed with this option, full
coverage and no reported RX discontinuities; artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-j8ta2ls0` and `...-6lte4bn3`.
The diagnostic's synthetic A/B positive and negative controls also passed.
Exact digital silence may be intentional for other test scenarios, so the
enforcement remains opt-in and does not establish general signal quality.

With that lab startup ordering, fresh full AVT94 Narrow A/B Decodium TX/RX
transfers both completed with no reported RX discontinuity, and the two
captured signals had no internal exact-zero run of 480 frames or more. The
pinned, unmodified Revenant codec decoded both original captures as VIS 73
and completed 200 rows. A/B MAE/p95/max were 0.120/0/78 and 0.119/0/76;
both passed the exploratory 5/15 gates. Crossed-reference comparisons failed
as intended with MAE about 105.8. Application artifacts are
`.../decodium-avt-two-app-avt-94-narrow-8b0ns5tg` and `...-2qxz6yka`;
independent outputs are `.../decodium-avt94n-clean-oracle.MoH4Sq`. This is
selected file-mediated A/B evidence over virtual audio. It does not identify
the original zero-block source, establish RF or live GUI-to-GUI behaviour, or
erase the earlier failing capture.

### AVT94 QRM A/B at native 320×200 resolution

Both Decodium-to-Decodium A/B transmissions completed at 320×200 with no
reported discontinuities; MAE was 0.450/0.539 and p95 1/1. The independent
Revenant decoder received both captured audio files as AVT94 QRM, VIS 74,
and produced complete 320×200 images. Direct comparison against the prepared
references gave MAE 0.964/1.103, p95 1/1 and maximum component errors 158/144.
Thus the 5 MAE / 15 p95 gate passes, but it does not hide the large isolated
errors. Decodium artifacts are `.../decodium-avt-two-app-avt-94-qrm-pgkeng1s`
and `.../decodium-avt-two-app-avt-94-qrm-kanxirmz`.

The large errors are concentrated at sharp transitions: 348/312 RGB
components exceed 64, of which 312/305 are on odd image rows. Since QRM
interlaces the image, those rows were transmitted in the later field. Mean
error also rises through the transmitted fields. The same Revenant decoder's
high-precision countdown period estimates were 1.00000350179/1.00000350178
for these Decodium captures, compared with 0.999999861998 for both of its own
generated-signal self-controls. For Decodium Narrow-QRM captures the estimated
ratios were 1.00000025876/1.00000025876. The narrow adapter now prints the
estimated ratio with 12 significant digits; its rebuilt output was byte-identical
to the previous unmodified decoder output for both QRM captures.

As an isolated A/B diagnostic, the independently maintained decoder was
rebuilt with the `--nominal-avt-clock` option of
`tests/sstv/tools/build_revenant_sstv_oracle.sh`. This applies
`tests/sstv/fixtures/revenant-avt-nominal-clock-diagnostic.patch` to a
temporary source copy, forcing nominal AVT raster period while leaving the
recorded signals and the rest of the decoder unchanged. Its printed ratio is
still the estimated countdown ratio, not the applied diagnostic period. On
the same captured signals, MAE/p95/max became 0.288/1/29 and 0.356/2/34,
with no components above 64. On Revenant's own A/B generated signals the
nominal diagnostic gave MAE/p95/max 0.283/1/55 and 0.343/2/36, versus
0.284/1/52 and 0.340/2/37 with the unmodified decoder. This ties the large
errors in these comparisons to the period used by that receiver. It does not
prove which transmitter or estimator causes the difference, establish RF
clock accuracy, or justify forcing a nominal clock in production, where real
clock error can occur. The unmodified codec's own A/B and negative controls
passed after the adapter rebuild.

Further controls isolate sensitivity to where the identical waveform starts
in the decoder input. The Decodium core encoder generated AVT94 QRM directly
at 48 kHz with 0.5 s of silence, using the same A/B RGB fixtures; the
unmodified Revenant decoder estimated 0.999999793006 for both and produced
MAE/p95/max 0.295/1/32 and 0.335/2/37, with no components above 64.
`render_avt94_native`, built from
`tests/sstv/tools/render_avt94_native.cpp`, reproduces these input files; its
A/B output matched the initial direct render byte-for-byte and its
existing-output and short-input controls rejected the requests.

Keeping the generated waveform unchanged but inserting one additional silent
48 kHz sample before it made the unmodified Revenant decoder estimate
1.00000350179 in both A/B, with MAE/p95/max 0.964/1/158 and 1.115/1/144,
and 348/310 components above 64. Three extra samples instead gave estimates
0.999999792985 and restored max 32/37 and no components above 64. Four or
256 extra samples repeated the one-sample result. Removing the captured
application preamble while keeping its original sample position did not
change the estimate; relocating either the captured or direct signal to the
capture's original sample position reproduced the higher estimate. The
input relocation controls preserve the waveform after the inserted silence.
These observations demonstrate sensitivity to input sample alignment in this
specific external decoder and setup, not that a receiver must use a fixed
clock or that every implementation has the same issue. RF, independent
hardware and live GUI-to-GUI operation remain untested.

The same two direct A/B signals with one additional leading silent sample
were converted to 48 kHz stereo and replayed through BlackHole to full
Decodium receiver sessions without added interference. Both images were
complete, with MAE/p95/max 0.398/1/42 and 0.476/1/41 and no reported RX
discontinuities. Artifacts are `.../decodium-avt-interference-zcsjy5f9` and
`.../decodium-avt-interference-7sgaftd6`. This verifies these application
replays, but virtual-device buffering and resampling mean that the inserted
sample does not prove identical internal demodulator alignment to the
file-based Revenant test. The originating A/B TX/RX application captures
remain the separate tests listed above.

The Revenant generator/receiver self-controls completed both images with
MAE 0.284/0.340 and p95 1/2. Replaying those independent signals through
BlackHole into full Decodium RX produced complete A/B images with MAE
0.454/0.552 and p95 1/1, and no reported discontinuities, dropped chunks or
processing failures. Artifacts are `.../decodium-avt-interference-z40r__71`
and `.../decodium-avt-interference-5il5cvgk`; source audio and PPMs are in
`/tmp/decodium-revenant-avt-test`. These are selected file-mediated codec
checks, not live GUI-to-GUI or RF evidence.

### AVT94 Narrow-QRM A/B at native 320×200 resolution

Both Decodium-to-Decodium A/B transmissions completed at 320×200 with no
reported discontinuities; MAE was 0.463/0.547 and p95 1/1. The pinned
Revenant decoder received both captured audio files as AVT94 Narrow QRM, VIS
75, producing complete images with direct-reference MAE 0.578/0.940 and
p95 1/2. Decodium artifacts are
`.../decodium-avt-two-app-avt-94-narrow-qrm-rn_hc_la` and
`.../decodium-avt-two-app-avt-94-narrow-qrm-blyziosd`.

The Revenant generator and receiver self-controls completed both images
with MAE 0.272/0.330 and p95 1/2. Replaying the independent A/B signals
through BlackHole into full Decodium RX yielded complete images with MAE
0.461/0.555 and p95 1/1; there were no reported discontinuities, dropped
chunks or processing failures. Artifacts are
`.../decodium-avt-interference-zf0jn7kc` and
`.../decodium-avt-interference-4rf923up`; generated audio and PPMs are in
`/tmp/decodium-revenant-avt-test`. Limits were MAE 5 and p95 15. This is
selected file-mediated codec evidence, not live GUI-to-GUI or RF evidence.

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

#### AVT90 Narrow-QRM: two-application notch comparison

Two full Decodium pairs on macOS ARM64 transmitted the detailed A/B images
above using AVT90 Narrow-QRM. A BlackHole relay added a continuous 1900 Hz
tone at peak 0.10 and applied gain 0.8. With the manual Q=300 notch enabled,
both images completed with full coverage and passed MAE 5 / p95 15: A
3.076/10/max 139, B 2.959/8/max 74. With the notch disabled, both images
completed but failed the MAE limit: A 6.742/14/max 139, B 5.728/14/max 80.
All four cases reported the expected mode, zero RX drops, discontinuities and
processing failures, with no relay overflow, underflow or clipping. Enabled
application artifacts are `.../decodium-avt-two-app-avt-90-narrow-qrm-id145vd4`
and `.../decodium-avt-two-app-avt-90-narrow-qrm-875_mc5i`. The enabled relay
recordings are `.../decodium-avt-live-interference-_idnn0vz` and
`.../decodium-avt-live-interference-aja3r4v1`; disabled controls are
`.../decodium-avt-live-interference-iiigw2td` and
`.../decodium-avt-live-interference-s9vu26e3`. The helper now accepts AVT90
and AVT94 QRM variants as well as AVT24 and extends its timeout accordingly.
These selected synthetic-tone results do not prove RF, automatic notch
selection, arbitrary images or Windows/Linux behaviour.

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

### AVT24 QRM and Narrow-QRM with detailed colour rasters

Two additional distinct 128×120 RGB patterns with gradients and blocks were
sent by two isolated Decodium processes over BlackHole for each mode. All
four receivers reported complete images, 100% coverage, zero discontinuities,
dropped chunks and processing failures; all transmitters completed. Against
the TX-prepared raster, without alignment, QRM A/B MAE was 1.440/1.700,
p95 8/10 and maximum component error 69/70. Narrow-QRM A/B MAE was
1.486/1.738, p95 8/11 and maximum 105/144. All met the existing MAE 5 and
p95 15 limits, although the maximum errors show individual pixels can differ
substantially. These are clean local macOS ARM64 audio tests, not RF or
independent-application interoperability evidence.

The deterministic inputs are reproducible with the `pattern` argument:
`python3 tests/sstv/tools/run_avt_two_app_lab.py avt-24-qrm a pattern` and
the same command with `b`, or with `avt-24-narrow-qrm` as the mode. Input
SHA-256 values are `4ac13fa7a9c4c419d57a146ac849e28b204b5e79c306b65fe09d481cb5e3f2ab`
and `9afd7b7b575e0ff6ecd2d41588ad0798f220930d8c2da576d129e5faaa7b6176`.
QRM artifacts are `.../decodium-avt-two-app-avt-24-qrm-6zxc0f7j` and
`.../decodium-avt-two-app-avt-24-qrm-oenxggwb`; Narrow-QRM artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-5ajz1xqi` and
`.../decodium-avt-two-app-avt-24-narrow-qrm-aaqei1sb` in the macOS temporary
directory. The QRM runs used separate files byte-identical to the generated
patterns, as checked by SHA-256.

The AVT receive session previously closed as soon as an observation's span
reached the image end. If another observation whose centre was still inside
the image arrived in the next chunk, it was discarded; the same observations
in one chunk produced a different last pixel. The session now waits for an
observation centre at or beyond the image end, or an explicit finish or
discontinuity. A deterministic regression uses QRM and Narrow-QRM, with the
same two final observations combined and split across calls; it failed before
this change and passes afterwards. The AVT session test and full RX runtime
suite passed on macOS ARM64. Two subsequent Narrow-QRM application transfers
with the detailed patterns completed with full coverage and zero reported
discontinuities, dropped chunks and processing failures; A/B MAE was
1.487/1.879, p95 8/11, and maxima 105/144. Artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-ivjj9dc7` and
`.../decodium-avt-two-app-avt-24-narrow-qrm-5dk50rv4`. The application
results do not prove that the chunk-boundary defect caused the larger errors
at colour transitions, and do not cover RF or other platforms.

The detailed-pattern comparisons showed many large errors next to sharp
component transitions. The AVT decoder now prefers observations away from
the first and last sample of a pixel for dense one-sample-hop streams when
the pixel spans at least four samples. It retains the boundary observations
as a fallback if no interior observation is valid; sparse streams preserve
the former weighting. QRM/Narrow-QRM A/B tests check both interior preference
and edge-only fallback without interpolation. The prior chunk-boundary test,
AVT codec/session suites, focused runtime tests and all twelve Studio AVT
loopbacks also passed on macOS ARM64.

Two new full Decodium process-pair transfers of the detailed AVT24 Narrow-QRM
patterns over BlackHole produced complete images with no reported audio or
processing errors. A/B MAE was 1.226/1.493 and p95 5/7, versus 1.487/1.879
and 8/11 in the preceding local runs; maximum errors were 111/67 versus
105/144. They are separate real-time runs rather than a sample-identical
replay, so the comparison is evidence for these inputs and conditions, not a
guarantee of improved performance in noise, other modes or RF. Artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-o8zm7goy` and
`.../decodium-avt-two-app-avt-24-narrow-qrm-usb5p8up`. Windows and Linux
execution remain unverified for this change.

### Detailed colour rasters for the longer QRM modes

Three additional local macOS ARM64 A/B comparisons used two Decodium
processes and deterministic colour gradients and blocks over BlackHole 2ch.
The input patterns are 128×120; the comparison is against the prepared
320×240 AVT90 or 320×200 AVT94 TX raster. All six images had full coverage,
no reported discontinuities, dropped chunks or processing failures, and met
the pre-existing MAE 5 / p95 15 limits:

| Mode | Input | MAE | p95 | Maximum |
|---|---|---:|---:|---:|
| AVT90 QRM | A | 0.631 | 1 | 125 |
| AVT90 QRM | B | 0.725 | 3 | 83 |
| AVT94 Narrow-QRM | A | 0.455 | 1 | 56 |
| AVT94 Narrow-QRM | B | 0.551 | 1 | 50 |
| AVT94 QRM | A | 0.700 | 1 | 50 |
| AVT94 QRM | B | 0.534 | 1 | 48 |

The respective application artifacts are
`.../decodium-avt-two-app-avt-90-qrm-vhv_fqh8`,
`.../decodium-avt-two-app-avt-90-qrm-q0ftrvwg`,
`.../decodium-avt-two-app-avt-94-narrow-qrm-_hyh2del`,
`.../decodium-avt-two-app-avt-94-narrow-qrm-cfhp3_62`,
`.../decodium-avt-two-app-avt-94-qrm-ujajquet`, and
`.../decodium-avt-two-app-avt-94-qrm-r_35w87c`. Raw loopback PCM was retained
for the first four; the final pair drained the monitored audio without saving
PCM because the disk was nearly full. These clean-audio tests do not establish
independent-application interoperability, interference resilience, RF or
Windows/Linux behaviour. Reproduce with
`python3 tests/sstv/tools/run_avt_two_app_lab.py <mode> <a|b> pattern`;
`DECODIUM_LAB_AVT_NO_CAPTURE=1` disables only raw PCM retention.

### AVT24 normal independent receiver attempt

Two Decodium sessions on macOS ARM64 exchanged normal AVT24 with fixtures A
and B. Both images reached full coverage with no reported dropped chunks,
processing failures or discontinuities. Component MAE was 0.35434 for each,
p95 was zero and maximum error was 136, against limits MAE 5/p95 15. The
captured audio and prepared images are in
`.../decodium-avt-two-app-avt-24-00yeywou` and
`.../decodium-avt-two-app-avt-24-e4joy_o_` under the macOS temporary directory.

The pinned unmodified QSSTV-Experimental 10.0 ARM64 release was asked to
decode those captures in batch mode with AVT24 selected. Neither run produced
an image within 120 seconds; a further A run ended at QSSTV's own 96-second
timeout without an image. The receiver logs report duplicate Qt framework
classes. A sample of the B process first showed it in the filesystem watcher,
then in its SSTV receive code; that does not identify the cause. As an
independent control, the same QSSTV binary generated AVT24 WAVs for both images,
but its batch receiver reported no image for either its own WAV or copies with
eight seconds of appended silence. Both self-generated WAVs had nonzero
audio, with peak -2.5 dBFS; this does not establish that the transmitted PCM is
correct. Thus these controls do not prove a Decodium/QSSTV wire mismatch and
do not establish interoperability. QSSTV logs and WAVs are
`/tmp/decodium-qsstv-self-avt24-{a,b}*`; the independent Decodium capture
attempts are `decodium-qsstv-avt24-decode-o4i1jotz`,
`decodium-qsstv-avt24-decode-dog_9v5e` and
`decodium-qsstv-avt24-decode-k1e36o5l` in the macOS temporary directory.

The Decodium A/B captures can be reproduced with
`python3 tests/sstv/tools/run_avt_two_app_lab.py avt-24 a` and the same
command with `b`; `tests/sstv/tools/run_qsstv_exp_avt_decode_lab.py` attempts
the QSSTV comparison using a captured float PCM file and prepared image.
This test covers the normal mode, not QRM variants, RF or other platforms.

Further controls identified the duplicate Qt warning's source: loading the
bundled `imageformats/libqpdf.dylib` pulls Homebrew QtPdf and its separate Qt
Core/Gui frameworks into the release process. A disposable copy of the bundle
was made at `/tmp/decodium-qsstv-isolated.vQpVBu`, and only that copy's PDF
plugin was moved out of the bundle. The executable SHA-256 remained
`b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa`;
the original bundle was not modified. The duplicate-class warnings disappeared,
but its receiver still reported no image for both release-generated AVT24
controls padded with silence. The captured Decodium A/B signals were also
replayed after removing only external silence: both 41-second files reached
QSSTV's 71-second internal timeout without images, without duplicate-class
warnings. Their output directories are `decodium-qsstv-avt24-decode-uii4mowz`
and `decodium-qsstv-avt24-decode-8rsnzasg` in the macOS temporary directory.

The official release's A/B self-generated WAVs had nonnegative samples only
in the first second, with approximately 51.4% zero samples. A separate build
of QSSTV source revision `a84584248d3307bfa081362031ff20f365337633`, carrying
only `tests/sstv/fixtures/qsstv-experimental-10.0-signed-pcm.patch`, generated
new AVT24 A/B WAVs whose first second had both polarities (approximately 2.8%
zero samples). After repairing the stale RIFF size in copies without changing
PCM, the original receiver binary in the isolated bundle reached its own
60-second timeout without an image for both. Logs and files are
`/tmp/decodium-qsstv-pcmfix-avt24-{a,b}*`. The PCM correction and Qt isolation
are diagnostic controls, not a proof of why AVT24 decoding fails in this
environment. They do not establish incompatibility with Decodium or a general
QSSTV defect; independent exchange with unmodified QSSTV remains unresolved.

Inspection of the same pinned QSSTV source found that its `sstvTx::sendImage`
uses the generic preamble and one VIS before `transmitImage`, while its
`modeAVT::txSetupLine` sends G/B/R lines with gaps and sync; its AVT receiver's
`modeAVT::process` searches for the protected countdown. A bounded audio
diagnostic examined the first 15 seconds of the two actual Decodium captures
and the two PCM-corrected QSSTV transmissions. A first 4 ms sampling grid
found 16 increasing AVT24 countdown words per Decodium capture. A finer 1 ms
grid found all 32 values, 0 through 31, in both; neither grid found matching
protected words in either QSSTV WAV. The diagnostic's thresholds and first
15-second search bound mean that a zero count is not general proof that any
arbitrary recording lacks a countdown. Synthetic A/B, invalid-inverse,
silence and steady-tone negative controls pass in
`test_sstv_avt_countdown_audio_diagnostic`.

The two PCM-corrected QSSTV WAVs were also replayed through BlackHole into
full Decodium RX instances. Both recognized the AVT24 VIS, published no image,
and reported zero processing failures, dropped chunks and discontinuities.
Artifacts are `.../decodium-avt-interference-q82axwsy` and
`.../decodium-avt-interference-jj3eci5n`. The transmitter was a modified
QSSTV build and the files were replayed, not a live application-to-application
exchange. This is evidence of a difference in these tested transmit paths,
not proof of every QSSTV version or of RF performance.

As a further controlled diagnostic, the two Decodium AVT24 captures were
cut to retain the third complete VIS and countdown, with approximately 100 ms
of the preceding VIS tail, the complete image and trailing silence. The
countdown analyzer found all 32 words in each cut. With AVT24 explicitly
selected, the original QSSTV-Experimental 10.0 binary decoded both recordings:
A/B component MAE was 3.624/3.644, p95 1/1 and maximum 255/255, within
the pre-existing MAE 5/p95 15 limits but with large individual pixel errors.
The same results were obtained in the PDF-plugin-isolated copy. Original
release artifacts are `decodium-qsstv-avt24-decode-khd6fa92` and
`decodium-qsstv-avt24-decode-0a1ui4ek`; isolated-bundle artifacts are
`decodium-qsstv-avt24-decode-gz93c9sf` and
`decodium-qsstv-avt24-decode-k2nxfbse` in the macOS temporary directory.

The isolated QSSTV binary with automatic mode detection reported no image for
either cut. Replacing the initial 100 ms VIS tail with one second of silence
also yielded no image in automatic mode; logs are
`/tmp/decodium-qsstv-avt24-{singlevis,cleanvis}-auto-{a,b}.log`. These are
post-capture diagnostic edits and a manually selected receiver mode, not a
successful exchange of the original full signal. They show the selected-mode
receiver can decode the retained countdown and image; they do not establish
why automatic detection or the original triple-VIS capture failed.

The pinned QSSTV source places AVT24/90/94 after `FAX480` in the mode enum,
while its automatic wide-mode range ends at `FAX480`. To test that finding,
a separate build from source revision `a84584248d3307bfa081362031ff20f365337633`
changed only that upper bound to `AVT94`, in addition to the existing signed-PCM
TX patch. Its binary SHA-256 is
`1e0b1674a14d1a5ea35d9cf7d8bccceaea8d635f045dd79559dc4eafa8ab0f2c`.
The upper-bound change is preserved as
`tests/sstv/fixtures/qsstv-experimental-10.0-avt-auto-diagnostic.patch` and
was applied only to a temporary source copy. Against the same two recordings
with the 100 ms preceding-VIS tail, this modified build automatically selected
AVT24 and decoded A/B at MAE/p95 3.624/1 and 3.644/1, respectively. The
original isolated QSSTV binary reported no image for the same automatic tests.
Both modified-build tests with one second of initial silence reached the
60-second internal timeout, as did both uncut triple-VIS captures; one of those
processes remained active after reporting timeout and was terminated by the
harness operator. The modified build also timed out on the silence-prefixed
inputs when AVT24 was selected manually, while manually selected tests on the
100 ms tail succeeded. Thus widening the range helps this specific automatic
case but does not resolve acquisition in general or establish interoperability
of unmodified QSSTV with the original signal. Logs, source and images are in
`/tmp/decodium-qsstv-avt-auto.1HtFgo`.

`tests/sstv/tools/prepare_avt_single_vis_lab.py` reproduces the exact PCM cuts
from the externally measured countdown times 4.620 s and 4.492 s in the
previously trimmed captures. Its A/B sample-exact copy, overwrite rejection
and invalid-time controls pass in CTest. The resulting SHA-256 values were
`36774bed02ab95cf7aa6c8f061a66aae49c0c9bc078c6a0f1afeafe14339d7a9`
and `04b7403462d3277ff44b16fdf7d6985295347b327bae193b1bcf035c3087662c`.
These tests do not validate QRM interoperability, RF, or other platforms.

### Acquisition after a damaged first countdown word

A focused decoder A/B regression now feeds two consecutive AVT24 countdown
words for QRM and Narrow-QRM, corrupts one symbol in the first protected
word, and checks that the immediately following word is accepted with the
correct counter and image anchor. Feeding one observation at a time and in
193-observation chunks gives the same result. The AVT unit suite passes.

A separate local macOS ARM64 application experiment connected two Decodium
sessions through a BlackHole relay. The relay muted 110 ms inside the first
countdown word, without adding an interference tone. A separate offline tone
analyzer checked each saved pair of source and relayed recordings: the source
contained counter 0, the first valid relayed word was counter 1, and at least
five following words formed a chain. The analyzer also has synthetic A/B
controls for the narrow 1700/2100 Hz tones. All four received images had
complete coverage and no reported discontinuities, dropped chunks, processing
failures, relay clipping, overruns or underruns:

| Mode | Image | MAE | p95 | Maximum |
|---|---|---:|---:|---:|
| AVT24 QRM | A | 1.311 | 9 | 44 |
| AVT24 QRM | B | 0.724 | 5 | 27 |
| AVT24 Narrow-QRM | A | 1.361 | 9 | 65 |
| AVT24 Narrow-QRM | B | 0.735 | 5 | 28 |

Application artifacts in table order are
`.../decodium-avt-two-app-avt-24-qrm-o8mrsjxk`,
`.../decodium-avt-two-app-avt-24-qrm-2gda1ni9`,
`.../decodium-avt-two-app-avt-24-narrow-qrm-d3lvu794`, and
`.../decodium-avt-two-app-avt-24-narrow-qrm-tdfv27fr`; relay artifacts are
`.../decodium-avt-live-interference-sfxhudpu`,
`.../decodium-avt-live-interference-jj_zl9t5`,
`.../decodium-avt-live-interference-6l66jsms`, and
`.../decodium-avt-live-interference-wuo8b9hb`. Run the existing two-app
interference lab with `DECODIUM_LAB_AVT_FIRST_FRAME_MUTE=1`, notch `0`, and
one of these modes and images; in this mode no continuous tone is added.
The application logs do not record which valid word was used for acquisition;
the immediate-successor assertion is made by the component test, while the
application test establishes complete reception after this controlled loss.
These laboratory cases do not establish independent application, general QRM,
RF or arbitrary longer-burst performance.

### Single-VIS prefix sensitivity in the QSSTV batch receiver

The exact A/B single-VIS recordings above were copied with only their first
100 ms replaced. A generated 1100 Hz or 1500 Hz tone at amplitude 0.3 replaced
the retained tail of the preceding VIS; a zero-sample prefix served as control.
Everything from 100 ms onward, including the complete last VIS, countdown,
image and tail, remained byte-identical to the respective input. The original
unmodified QSSTV-Experimental 10.0 release, with AVT24 selected manually,
decoded A and B for both tones at component MAE/p95 3.624/1 and 3.644/1;
maximum component error was 255 in both. Both silent-prefix controls reported
an internal timeout and produced no image. These outcomes show sensitivity to
the initial audio in this batch-receiver setup; they do not determine the
internal cause, establish live reception, or establish that the original
triple-VIS signal is interoperable. No production waveform was changed.

The 1100 Hz receiver artifacts are `.../decodium-qsstv-avt24-decode-avo5nk5m`
and `.../decodium-qsstv-avt24-decode-x3mklk8q`; 1500 Hz artifacts are
`.../decodium-qsstv-avt24-decode-k2zv8v_9` and
`.../decodium-qsstv-avt24-decode-px01z0d4`. Silent controls are
`.../decodium-qsstv-avt24-decode-pqgeewqe` and
`.../decodium-qsstv-avt24-decode-xgkqs0qd`. All are under the macOS temporary
directory. `tests/sstv/tools/prepare_avt_qsstv_prefix_probe.py` reproduces
the replacement; its synthetic silence/tone controls verify stereo samples,
byte-exact preservation after the prefix and refusal to overwrite. Generated
1100 Hz A/B copies matched the original probes byte-for-byte. The diagnostic
CTest passes; these artificial prefixes are not a standards-conformance test.

### Two consecutively damaged initial countdown words

The local BlackHole relay can now mute approximately 276 ms, damaging both
initial AVT24 countdown words. Independent analysis of the source and relayed
recordings found counter 0 in the original and counter 2 as the first valid
word in the relayed signal, with at least five consecutive later words. Two
Decodium processes transmitted and received each of two 128x120 images for
both QRM variants. QRM A/B MAE was 1.075/1.229, p95 11/13 and maximum
41/48; Narrow-QRM A/B MAE was 1.324/1.536, p95 6/8 and maximum 67/68.
All four completed with full coverage and no reported discontinuity, relay
clipping, overflow or underflow. These observations do not prove which word
the application used for acquisition, independent implementation
interoperability, RF behaviour, or arbitrary burst resilience.

Application artifacts are `.../decodium-avt-two-app-avt-24-qrm-6o_5xf_f`,
`.../decodium-avt-two-app-avt-24-qrm-lsb8o_92`,
`.../decodium-avt-two-app-avt-24-narrow-qrm-8v2o7py6`, and
`.../decodium-avt-two-app-avt-24-narrow-qrm-nzh7vvlx`. Corresponding relay
artifacts are `.../decodium-avt-live-interference-zo596lz0`,
`.../decodium-avt-live-interference-0nu_l4b3`,
`.../decodium-avt-live-interference-20r4rway`, and
`.../decodium-avt-live-interference-g5qp4ikx`, under the macOS temporary
directory. Set `DECODIUM_LAB_AVT_FIRST_FRAME_MUTE=2` in the existing relay
helper to repeat this controlled loss; `1` retains the earlier one-word case.

### AVT90 QRM with a continuous 1900 Hz tone

Two full Decodium process pairs transmitted different 320x240 images in
AVT90 QRM through the BlackHole relay, with a continuous 1900 Hz peak-0.10
tone, gain 0.8 and the receiver's manual 1900 Hz notch enabled. A/B completed
with full coverage, MAE 0.346/0.352, p95 1/1 and maximum component error
64/64. The relays reported no overflow, underflow or clipping; RX diagnostics
reported no processing failures, dropped chunks or discontinuities. These
fixtures are simple images and do not establish general interference or RF
performance, other variants or independent interoperability. Artifacts are
`.../decodium-avt-two-app-avt-90-qrm-5y2fo_oi` and
`.../decodium-avt-two-app-avt-90-qrm-ngtkh5uf`, with relays
`.../decodium-avt-live-interference-6jh9ef_0` and
`.../decodium-avt-live-interference-1mv6bzmb` in the macOS temporary directory.

An earlier A attempt completed TX and RX but timed out while awaiting process
shutdown; two subsequent attempts failed with audio capture/relay overflow
under heavy machine load. None was counted as a successful run. The helper
now waits for actual TX completion and a complete received image before its
existing lab-process cleanup, rather than relying on application exit. When
the interference relay already records its mixed signal, it skips an extra
BlackHole capture and uses a larger relay buffering allowance; any PortAudio
error still fails the run. It does not test normal application shutdown.
The AVT core and RX session tests also passed after the helper changes.

### AVT94 Narrow-QRM: terminal-pixel discrepancy

Two additional AVT24 Narrow-QRM A/B application pairs exercised the same
terminal-component guard with distinct 128 x 120 colour gradients. A ends
with blue 0 after 255; B ends with blue 255 after 0. A 1900 Hz peak-0.10
tone was added through the BlackHole relay, with the receiver's manual notch
enabled and gain 0.8. Both pairs received complete images with no reported
audio discontinuities or relay errors. Last-blue received/reference values
were 0/0 and 247/255, within the independently checked limit of 20. Full
image MAE/p95/max were 4.520/14/71 and 4.483/14/71; both pass the existing
5/15 limits with limited margin. Crossed-reference MAEs were 86.876 and
86.757, confirming the selected A/B images were distinguishable. Artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-8ikz2l0t` and
`.../decodium-avt-two-app-avt-24-narrow-qrm-ooyh3_vz`; relay captures are
`.../decodium-avt-live-interference-c58wx8o7` and
`.../decodium-avt-live-interference-4df0yax8`. The selected input fixtures are
`/tmp/decodium-avt24-narrow-terminal/{a,b}.ppm`. This verifies local
virtual-audio behaviour for these patterns and interference, not arbitrary
RF conditions or independent-receiver interoperability.

Two local Decodium process pairs transmitted A/B images using AVT94
Narrow-QRM through the 1900 Hz peak-0.10 interference relay and receiver
notch. Both had full reported coverage, no relay errors, and MAE/p95
0.452/2 and 0.447/2, within the existing 5/15 limits. The maximum errors,
however, were 222/221. Comparing all 192,000 RGB components showed exactly
one component above 64 in each: blue at pixel (319,199), expected 0 and
received 222/221. The same two images were then transmitted by two Decodium
processes without the tone or notch. Those direct audio tests had MAE/p95
0.129/0 and 0.129/0, but again exactly one component above 64 at the same
location, expected 0 and received 224 in both. Thus the issue is not specific
to the interference setup. The mechanism remains unconfirmed; no blind pixel
replacement or protocol change is justified by the current evidence.

Interfered application artifacts are
`.../decodium-avt-two-app-avt-94-narrow-qrm-xy2qnowy` and
`.../decodium-avt-two-app-avt-94-narrow-qrm-q6ucbkq0`, with relays
`.../decodium-avt-live-interference-l9vsl60n` and
`.../decodium-avt-live-interference-dye53unp`. Clean application artifacts are
`.../decodium-avt-two-app-avt-94-narrow-qrm-uh0s0mpt` and
`.../decodium-avt-two-app-avt-94-narrow-qrm-og_61puh`, all under the macOS
temporary directory. These controlled local tests are not independent
interoperability or RF evidence.

To test one possible boundary cause, diagnostic copies of the two interfered
relay recordings were made with an additional 10 ms 1700 Hz tone after the
last sample above 0.1 amplitude. The threshold identifies an estimated end
of the stronger TX signal, not a verified protocol boundary; the existing
1900 Hz interferer was already present in the recordings. On replay into
Decodium with its 1900 Hz notch, the final blue values were still 222/221
against zero, respectively. Both replays had complete reported coverage,
MAE/p95 0.451/2 and 0.442/2 and no reported RX discontinuities. This does not
exclude a different timing error or prove that the added tone covered the
actual final pixel; it only shows that this particular estimated extension did
not resolve the defect. No production waveform was changed.

The copies were made by `tests/sstv/tools/prepare_avt_terminal_tone_probe.py`;
its synthetic A/B test checks that the only modified interval is the selected
tail and that existing files cannot be overwritten. That test, the AVT core
suite and RX session suite passed. Modified audio is
`/tmp/decodium-avt94-terminal-probe-{a,b}.f32`; replay artifacts are
`.../decodium-avt-interference-indyz0av` and
`.../decodium-avt-interference-cuu6r2xn` under the macOS temporary directory.

Subsequent inspection located the actual transition at the start of the
500 ms VOX hang. In diagnostic copies, replacing its first 10 ms with the
preceding 1700 Hz image tone changed the final blue component from 222/221
to 0/2 in the two interfered replays. The diagnostic is implemented in
`prepare_avt_vox_boundary_probe.py`; its synthetic A/B test checks the changed
interval and refuses to overwrite an existing output.

The TX coordinator now appends 10 ms of the actual last component's tone to
AVT QRM and Narrow-QRM audio before any following VOX hang or FSK ID. The core
encoder defaults to no hold. Encoder tests compare the protocol samples with
and without the hold, check black and white terminal tones, and verify that
the coordinator only adds this duration to QRM variants.

Two fresh AVT94 Narrow-QRM Decodium TX/RX process pairs used 1900 Hz
peak-0.10 interference and the RX notch. In A, the last blue component was 0
with a preceding blue of 255; RX gave 4 and 193. In B, the values were 255
and 0; RX gave 254 and 63. Both images had full reported coverage, zero
reported discontinuities and no relay errors; MAE/p95 were 1.473/9 and
1.187/7 respectively. Application artifacts are
`.../decodium-avt-two-app-avt-94-narrow-qrm-e9al6gug` and
`.../decodium-avt-two-app-avt-94-narrow-qrm-0aqybfkc`; relay artifacts are
`.../decodium-avt-live-interference-ag4qp5s5` and
`.../decodium-avt-live-interference-c8wq0c7f` in the macOS temporary
directory. The preceding pixel and other components still show demodulation
error at abrupt colour transitions. These tests establish local application
behaviour under the stated conditions, not RF performance or independent
receiver interoperability of the extended tail.

The two-app harness can enforce this individual component using
`DECODIUM_LAB_AVT_MAX_LAST_BLUE_ERROR=10`. The image comparator reports the
reference, received value and error, and fails independently of the overall
MAE/p95 limits. `test_sstv_lab_image_terminal_comparison` exercises near and
opposite terminal values for both black and white, including invalid limits.
With this guard active, fresh A/B two-application interference tests passed:
last blue was 4/0 and 254/255 received/reference, with full reported
coverage and zero discontinuities. MAE/p95 were 1.449/9 and 1.180/7.
Application artifacts are `.../decodium-avt-two-app-avt-94-narrow-qrm-3lcve56r`
and `.../decodium-avt-two-app-avt-94-narrow-qrm-nb8ezyks`; relays are
`.../decodium-avt-live-interference-6ey_m719` and
`.../decodium-avt-live-interference-ggqay3zg` in the macOS temporary directory.

Two further local Decodium TX/RX pairs checked the terminal-tone hold on
AVT24 QRM, using 1900 Hz peak-0.10 interference and the RX notch. The last
blue component was 0 with preceding blue 255 for A, and 255 with preceding
blue 0 for B. The last component received was 13 and 255 respectively; both
passed the individually configured maximum error of 20. A/B image MAE/p95
were 1.863/11 and 1.641/10, coverage was complete, and no discontinuities or
relay errors were reported. Application artifacts are
`.../decodium-avt-two-app-avt-24-qrm-3gfmj2va` and
`.../decodium-avt-two-app-avt-24-qrm-j8hl60kv`; relay captures are
`.../decodium-avt-live-interference-erpxcjnf` and
`.../decodium-avt-live-interference-v9jnauzv` in the macOS temporary directory.
This checks the selected local QRM mode and signal, not independent receiver
or RF interoperability.

The same terminal-component guard was applied to two 320×240 AVT90 QRM
transmissions through a 1900 Hz peak-0.10 interferer and the receiver's
manual notch. The A input had blue 255 except for the final pixel at 0; B
had blue 0 except for the final pixel at 255. Received final blue was 20/251,
for errors 20/4 against the configured limit of 20. Thus A passed exactly at
the threshold and should not be interpreted as a margin below it. Both
images had full coverage, MAE 0.812/0.329, p95 5/1, and no reported audio
discontinuities or relay errors. These are selected two-Decodium audio tests,
not an independent receiver or RF result. Application artifacts are
`.../decodium-avt-two-app-avt-90-qrm-59swfaje` and
`.../decodium-avt-two-app-avt-90-qrm-2prpzbmp`; relay artifacts are
`.../decodium-avt-live-interference-je64mvmu` and
`.../decodium-avt-live-interference-ndi3h83a`. The input PPMs are
`/tmp/decodium-avt90-terminal-current/{a,b}.ppm`. The laboratory command is
`DECODIUM_LAB_AVT_MAX_LAST_BLUE_ERROR=20 python3
tests/sstv/tools/run_avt_two_app_interference_lab.py a IMAGE 1 1900
avt-90-qrm`, repeated with B and its image.

An additional clean A/B comparison used the same inputs and terminal limit
with two Decodium applications over BlackHole 2ch, without the interference
relay or notch. Both runs reported full coverage and zero discontinuities.
The final blue component was 20 versus reference 0 for A, and 250 versus
reference 255 for B; image MAE was 0.138 and 0.155, respectively. Therefore
the A terminal error at the configured threshold also occurs without the
test interferer and notch. These observations do not establish the cause or
justify changing pixel values. Artifacts are
`.../decodium-avt-two-app-avt-90-qrm-o664kbym` and
`.../decodium-avt-two-app-avt-90-qrm-sfzfo6hs` under the macOS temporary
directory. This is a local virtual-audio comparison, not an RF or independent
receiver test.

For these clean AVT90 QRM results, an additional RGB comparison of the last
five blue components on the final line found A reference/received pairs
`255/255, 255/255, 255/252, 255/185, 0/20` and B pairs
`0/0, 0/0, 0/0, 0/64, 255/250`. The largest nearby discrepancy therefore
appears at the abrupt transition, not only in the final component. This is
consistent with transition smearing but does not establish whether its source
is timing, demodulation, resampling or another part of the path; no production
correction is inferred from this observation alone.

The clean A/B images were subsequently compared over all blue components.
Each image has exactly 240 blue components with absolute error above 20:
239 occur at x=319 on lines 0 through 238, and the remaining one is x=318
on the final line. The mean absolute blue error in column 319 is 57.01 for
A and 48.79 for B; green also exceeds 20 at 240 components in each image.
This reveals a broader line-boundary effect in these fixtures, not merely
an isolated terminal pixel. It does not yet identify whether the transmitter,
timing mapper or receiver demodulator is responsible. Source and received
images are the clean A/B artifacts identified above; no pixel correction
was applied.

For the first 238 lines of the same clean fixtures, the final red, green and
blue component values were compared with the first value of the following
component in transmission order (R to G, G to B, B to R). The average
fractional shift toward that following value was 0.303/0.301/0.327 for A
and 0.314/0.303/0.288 for B. The patterns have different component levels;
both show a similar direction and magnitude of cross-boundary mixing. This
is evidence for a systematic component-boundary timing or filtering effect,
not proof of a particular implementation defect. In particular, applying a
fixed colour correction from these two constant-colour fixtures would be
unjustified.

An experimental AVT24 in-memory encoder/demodulator comparison decoded the
same observations twice: once using the detected countdown image start and
once one 12 kHz sample earlier. The earlier start increased both the total
RGB absolute error and the last-column error for normal, narrow, QRM and
narrow-QRM in the existing gradient fixture. For example, QRM total error
changed from 71,330 to 191,841 and last-column error from 534 to 1,219;
narrow-QRM changed from 72,387 to 213,115 and 602 to 1,421. Thus the one-sample
countdown offset alone does not justify advancing the production receiver.
This controlled comparison is AVT24 in memory, not a measurement of the
complete AVT90 application path. The test reports both totals in
`fullAvt24PcmLoopbackIsChunkedAndCompletesProgressively`.

The same diagnostic also tested a start one sample later than detected.
This increased the total and last-column errors in all four variants of the
same gradient fixture as well: for QRM the sums were 187,910 and 1,384,
compared with 71,330 and 534 at the detected start; for Narrow-QRM they were
207,212 and 1,725, compared with 72,387 and 602. This rules out either
one-sample direction as a general improvement for this controlled AVT24 case;
it does not rule out a separate timing issue in the AVT90 application path.

A follow-up synthetic transition probe uses the same generated 48 kHz input
for both paths: direct every-fourth-sample decimation and the production
48-to-12 kHz resampler. For transitions A/B and offsets 0, 1 and 2 output
samples, the measured fractional shifts toward the next tone differed by at
most 0.01 between paths. The previous diagnostic generated the two inputs
at different rates and phases, so its larger differences cannot be attributed
to the resampler alone. This probe uses clean synthetic tones and does not
exclude resampling effects for other signals or in the complete application.

The same three-way in-memory comparison now also covers AVT90 QRM constant
colour patterns A/B with opposite final-blue transitions. The detected start
was again one sample later than the encoder header. Moving it one sample
earlier reduced last-column error sums from 1,570 to 847 for A and 2,520 to
1,203 for B, but increased total RGB error from 5,077 to 15,743 and 7,387 to
18,694, respectively. A one-sample later start worsened both metrics. The
improvement at one edge therefore comes with a substantial whole-image
regression; neither shift is adopted as a production fix. This in-memory
comparison does not include the resampler or the application's audio path.

An additional test-only session retained the detected image start but omitted
the latter half of observations in each last component pixel (for pixels at
least four samples long). For the in-memory AVT90 QRM A/B constant-colour
patterns, total RGB error decreased from 5,077/7,387 to 4,518/6,617 and
last-column error from 1,570/2,520 to 1,011/1,750. However, on the existing
AVT24 gradient, QRM last-column error increased from 534 to 873 and
Narrow-QRM from 602 to 818. This is a diagnostic tradeoff, not a production
algorithm or evidence of a general improvement. The test-only filter does not
modify the encoded signal or the production decoder.

Two further AVT90 QRM in-memory patterns use a varying RGB gradient and its
inverse. The same filter worsened last-column error from 2,191 to 2,495 for
A and from 2,164 to 2,295 for B; total error also increased in both cases.
Therefore even restricting the filter to AVT90 QRM would not be justified
by the preceding constant-colour improvement. Both generated streams reached
complete receiver snapshots; no production filtering change was made.

A synthetic boundary diagnostic compared direct decimation of a 48 kHz tone
with the production 48-to-12 kHz resampler, feeding the same input samples
to both paths and the production frequency demodulator afterwards. Across
eight initial phases, two opposite frequency transitions and observation
shifts of zero to two output samples, the maximum difference in the measured
next-tone fraction was 0.0092 for A and 0.0107 for B. This narrow synthetic
experiment does not reproduce the full application audio path and does not
establish the cause of the observed AVT90 edge mixing. Direct decimation is
used here only as a comparison for these band-limited test tones.

The same diagnostic was extended to extreme 1500-to-2300 Hz and
2300-to-1500 Hz transitions (C/D). Among windows with both observations
valid in both paths, maximum differences were 0.0075/0.0133 respectively.
However, C had 2/24 incomplete windows (direct 0, resampled 2), while D
had 4/24 (direct 4, resampled 2; counts overlap). A window is incomplete
when either path supplies fewer than two valid observations. These cases
must not be silently treated as comparable frequency estimates: the result
also exposes a validity difference near the extreme transitions, and does
not by itself determine its effect on a decoded image.

To check whether this observation carries over to a complete AVT stream, the
in-memory encoder-to-demodulator loopback now counts valid and invalid
observations in the interior of the final pixel of every component and in
other pixels. It includes AVT90 QRM uniform A/B, inverse gradient A/B, and
two additional extreme-colour images, RGB (0,255,0) and (255,0,255). Each
AVT90 run counted 2,157 valid and zero invalid final-pixel interior
observations, and 617,033 valid and zero invalid other-pixel interior
observations before the encoder ended. The extreme A/B runs completed;
their aggregate final-column RGB errors were 2,419 and 2,439 respectively.
Thus the visible border error in these clean, directly generated 12 kHz
streams occurs without rejected interior observations. These counts exclude
pixel edges and trailing flush observations, and the loopback does not
include the application audio path or resampler. They do not rule out
invalid observations in those other conditions.

For the two extreme images, the test also measures the valid interior
observations in the final red and green pixels, normalising each frequency
between the current and following component tones. At two samples before the
detected pixel end the mean fraction towards the next tone was 0.11152 for
both A and B; at three samples before the end it was -0.00163 for both
(480 observations per distance and image). This directly shows a bias near
the estimated component boundary in these clean full-image loopbacks.
These are ensemble means for constant-colour inputs, not a guaranteed
correction coefficient: the gradient tests above already demonstrate that
discarding late observations can worsen other images. The detected image
start is one sample after the generated header in this setup; distances are
relative to that detected boundary, not necessarily the transmitted one.

An offline, test-only candidate attempted to undo an 11% contribution from
the next component tone, using a future demodulated observation. In AVT90
QRM clean loopbacks, final-column aggregate RGB error decreased from
1,570/2,520 to 1,490/2,387 on uniform A/B and from 2,419/2,439 to 259/279
on extreme A/B. However, inverse gradient A/B errors increased from
2,191/2,164 to 2,563/2,360; their total image errors also increased. The
coefficient was chosen from the preceding simple experiment and is not a
calibrated channel model. This candidate is therefore rejected for
production. It uses future observations offline and is not a streaming or
two-application test.

The same offline experiment now compares each adjusted observation against
the known source frequencies. Across 719 component boundaries per AVT90
image, the mean absolute frequency error of the chosen next-tone observation
was 15.35/17.15 Hz for gradient A/B. The corrected observation itself
improved from 18.79/20.58 Hz to 17.67/18.04 Hz on average, even though the
final-column image error worsened as recorded above. This demonstrates that
an improvement in a single observation's mean error is not sufficient to
justify changing the aggregated pixel output. The measurements use known
synthetic source pixels and do not establish a correction for unknown images.

The earlier gradient fixture changes by 29 levels per adjacent pixel with
8-bit wraparound; it is a rapidly varying pattern, not a smooth gradient.
Two further AVT90 QRM in-memory A/B fixtures use a gradual horizontal red
ramp, vertical green ramp and mixed blue ramp, plus their inverse. The
ordinary decoder's aggregate final-column RGB errors were 1,877/2,385; the
offline de-mix candidate changed them to 2,198/2,099. Whole-image mean
absolute error for the ordinary decoder remained below 0.05 RGB levels in
both cases. Since the candidate worsens A, these additional cases still do
not support enabling it. This is a clean in-memory comparison, not a
two-process or RF test.

The smooth A/B gradients were then transmitted between two Decodium
applications over BlackHole 2ch, without added interference. Both runs had
full coverage and no reported discontinuities, with overall MAE 0.159/0.137
and p95 0/0. These aggregate metrics conceal a significant boundary defect:
the maximum absolute component error in the final column was 83/75, at red
pixel (x=319, y=1) in each image. There were 568/524 components with error
greater than 20, of which 532/524 lay in the last column. They do not pass a
20-level last-column limit, despite passing the previous global limits.
Artifacts are `.../decodium-avt-two-app-avt-90-qrm-u6nt8u_g` and
`.../decodium-avt-two-app-avt-90-qrm-ve17byjv` in the macOS temporary
directory. These are virtual-audio observations, not RF or independent-peer
tests. The image comparator now always reports the last-column maximum and
accepts an optional `--max-last-column-error` guard; the AVT laboratory
exposes it through `DECODIUM_LAB_AVT_MAX_LAST_COLUMN_ERROR`. Comparator A/B
tests confirm that localised column errors can fail the guard even when the
global mean and p95 pass. No production decoder correction is implied.

The same captured A/B transmissions were independently decoded by a freshly
rebuilt Revenant adapter from pinned commit `67da9f8767b2a250792a6f5a5b2a69f5bc992b6d`.
Its AVT and FAX A/B self-controls and crossed-image/wrong-mode negatives
passed first; its codec source hashes matched those documented above. After
mono conversion, it reported complete AVT90 QRM images, VIS 70, 256×240.
Against the prepared references explicitly bilinearly resized to 256×240,
the A/B MAE was 0.116/0.212, p95 0/1 and last-column maxima 25/21. These
do not meet a 20-level last-column guard either, but are lower than the
83/75 maxima observed in Decodium's native 320×240 images. Because the
decoders use different horizontal resolutions and explicit resizing, this
is supporting independent evidence, not a pixel-identical attribution of
all error to either transmitter or receiver. Outputs and resized references
are in `/tmp/decodium-avt90-smooth-independent`.

The image comparator now also reports `first_column_maximum_error` and
accepts `--max-first-column-error`; the two-application and replay labs
expose it as `DECODIUM_LAB_AVT_MAX_FIRST_COLUMN_ERROR`. Their success lines
show the configured first and last edge limits, including `None` when no
limit was requested, so an aggregate-only pass is identifiable. Synthetic
A/B positive and negative controls exercise both edge guards independently.
Applied to the earlier shifted two-application replays, a 20-level first
column limit rejects A/B with errors 70/76, although the broad MAE/p95
limits pass. This is a measurement guard, not a production decoding fix.

An offline diagnostic fed each captured waveform through the production
resampler, frequency demodulator, countdown detector and AVT session, while
varying the detected anchor by -2 through +2 samples. For A/B, the detected
anchor produced overall MAE 0.0262/0.0270 and last-column maximum 8/14;
the +2 shift produced MAE 0.1388/0.1370 and last-column maximum 88/84.
The offline path does not reproduce every runtime audio-processing step or
the runtime's VIS-driven arming, so similarity to the live image is an
investigative clue, not proof of the cause.

The identical saved waveforms were also replayed through the full Decodium
receiver over BlackHole at gain 0.8, without added interference. A
test-mode-only bounded anchor adjustment compared zero against -2 samples.
First/last/middle-column maximum errors were 24/83/5 versus 70/23/4 for A,
and 11/75/7 versus 76/22/7 for B. At -2, both images still exceeded a
20-level last-column guard, and first-column errors increased substantially.
Thus moving the whole anchor is not an acceptable production correction.
Replay files are `.../decodium-avt-interference-s7f9ht3a` and
`.../decodium-avt-interference-3s4btcy7` (A zero/-2), and
`.../decodium-avt-interference-9g78r8cd` and
`.../decodium-avt-interference-6ee2v_ea` (B zero/-2). These are separate
virtual-audio replays, not sample-identical receiver input or RF tests.

An offline composite kept the zero-shift replay image and replaced only its
last column with the -2 replay column. Its overall maximum component error
was 24/22 for the smooth A/B images (MAE 0.06734/0.05063), compared with
83/75 in the zero-shift images. This is an analysis of two separately
received images, not a streaming implementation.

The same idea was checked against complete in-memory AVT90 QRM A/B streams
using a second decoder with an anchor two samples earlier. Replacing only
the last column improves the smooth and constant-colour cases, but raises
aggregate last-column error on the rapidly varying A/B images from 2,191/2,164
to 10,637/11,323. In another A image with an intentionally distinct final
blue pixel the early column has a maximum error of 101. Therefore this
spatially selective shift cannot be enabled generally without damaging
legitimate detail; no production decoder change was made.

The captured A/B waveforms were analysed offline through the actual 12 kHz
preprocessor before demodulation. With the default preprocessor, last-column
maximum errors were 94/80. Disabling only its band-pass reduced these to
8/14; disabling only the DC blocker gave 95/81, and disabling only automatic
level control gave 94/80. Widening the band-pass to 500–3500 Hz gave 36/34;
300–4500 Hz gave 19/21. These comparisons use the same captured source and
indicate the band-pass is a major contributor under these conditions.

Two full-application replays of the same captured A/B signals with the
band-pass disabled by a test-mode-only switch produced last-column maxima
9/16, versus 83/75 in the normal-filter replays. Overall maximum errors were
33/28; no discontinuities were reported. These are separately replayed
virtual-audio signals, not identical receiver samples or RF proof. Disabling
the band-pass is not adopted in production: an offline stress comparison
with a 400 Hz peak-0.15 interferer failed to acquire the countdown for both
images with the band-pass disabled and with the 300–4500 Hz band. The
default filter acquired both but still produced high MAE 7.51/7.39 and
last-column maxima 106/89. These results test only the specified interference
and do not establish general noise resilience. The test-only application
replays are `.../decodium-avt-interference-3obt4hhe` and
`.../decodium-avt-interference-tm05w13q`.

Further offline A/B tests widened one cutoff at a time. With 900–4500 Hz,
clean last-column maxima were 35/32 and a 400 Hz peak-0.15 interferer still
allowed countdown acquisition, though MAE was 6.57/6.54. With 300–2500 Hz,
clean maxima were 71/59 and that interferer prevented acquisition. Raising
the upper cutoff further to 5000 or 5500 Hz yielded clean maxima 38/34 and
41/37, respectively; neither met the 20-level guard. At 3300 Hz peak 0.15,
900–4500 Hz failed to acquire either countdown, while the default band
acquired both but had MAE 16.42/16.35. With the wider upper cutoff and an
additional -1 sample shift, clean first/last-column maxima were 39/14 and
46/13: the gain at one boundary still damages the other. These are bounded
offline experiments, not a general interference test or a production change.

An offline diagnostic also compared test-only 9-, 17- and 33-tap symmetric
Hamming-window FIR band-passes (900–2500 Hz, normalised at 1900 Hz). On the
clean captured A/B signals, first/last-column maximum errors were 41/27 and
41/29 with 9 taps; 37/81 and 36/73 with 17; 29/113 and 14/111 with 33.
With the 400 Hz peak-0.15 interferer, 9 taps failed countdown acquisition for
both images; 17 taps acquired both with MAE 7.61/7.53, and 33 taps acquired
both with MAE 1.00/0.99, but its last-column maxima remained 113/113.
Thus linear phase alone does not resolve the tradeoff: the longer tested
filter rejects the chosen interferer better while smearing the short AVT90
pixels. This filter exists only in the offline diagnostic and is not a
production implementation or a proof for other signals.

Another offline A/B experiment used the default filtered path to acquire the
countdown and a parallel, unfiltered-band image path, with a -4 sample
relative anchor. On the clean captured A/B signals the resulting first/last
column maxima were 21/8 and 17/14, with overall MAE 0.0349/0.0341. With a
peak-0.15 interferer at 400 Hz, the countdown still acquired but image MAE
rose to 34.53/34.16; at 3300 Hz it rose to 36.30/36.36. Retaining selective
acquisition alone therefore does not protect the image path. This diagnostic
does not include automatic path selection, application runtime or RF tests;
the split is not enabled in production.

An offline-only second-order notch experiment disabled the normal band-pass
and used a known interferer frequency (Q=20). With a peak-0.15 tone exactly
at the notch, 400 Hz produced first/last-column maxima 22/8 and 18/14 on
A/B; 3300 Hz produced 28/8 and 22/14. This presupposes perfect knowledge of
the tone. With the 400 Hz notch and a 410 or 430 Hz tone, neither countdown
was acquired. With the 3300 Hz notch and a 3320 Hz tone, A/B acquired but
image MAE rose to 14.11/14.15; a 3400 Hz tone prevented acquisition. This
does not demonstrate a general interference solution, a frequency estimator,
or an application/RF test, and no production filter was changed.

Wider test-only notches (Q=2 and Q=5) tolerate more detuning but do not
solve the tradeoff. With a 400 Hz notch and a 410 Hz peak-0.15 tone, Q=2
acquired both A/B images with MAE 3.41/3.40; a 430 Hz tone raised MAE to
9.63/9.63. For a 3300 Hz notch and a 3320 Hz tone, Q=2 gave MAE 1.66/1.65;
a 3400 Hz tone raised it to 8.03/8.02. Q=5 was worse in these detuned
examples, and one 430 Hz case failed acquisition. Even without interference,
Q=2 changed first/last-column maxima to 28/8 and 27/13 for the 400 Hz notch,
and 38/28 and 34/32 for the 3300 Hz notch. This remains an offline test
using known candidate frequencies, not automatic detection or a safe
replacement for the band-pass.

A separate offline spectral diagnostic measures the largest 100–800 Hz or
3000–5000 Hz peak in one second of captured 48 kHz audio, relative to the
largest 1500–2300 Hz peak. Synthetic A/B tests verify the frequency-bin
measurement for two fractional-frequency tones. On the two smooth captured
images, four clean windows each had outside/inside peak ratios from 0.00128
to 0.00196. Adding a single peak-0.15 tone at any of 400, 410, 430, 3300,
3320 or 3400 Hz yielded the injected integer frequency in the inspected
25-second window; adding 410.37 or 3320.37 Hz yielded the nearest 1 Hz bin.
The corresponding peak ratios were approximately 0.60–0.77, depending on
the image and tone. These numbers depend on this signal and window; they are
not an operating threshold or evidence against false positives.

Using the nearest-bin frequencies as manually supplied Q=20 notch centres,
separate offline A/B decodes with injected 410.37 Hz produced MAE 1.2804/
1.2794 and first/last-column maxima 24/10 and 20/16. With 3320.37 Hz, MAE was
0.1483/0.1422 and first/last maxima 28/9 and 23/14. These are two-stage
laboratory experiments with a stationary injected tone; the estimator and
notch are not connected in a streaming receiver. Multiple, drifting,
in-band or broadband interferers, false positives and real RF remain
unverified. Neither component changes production reception.

The offline spectrum diagnostic now also reports integrated outside/inside
band power, because a single-peak measurement alone can miss distributed
noise. In one-second windows from the captured smooth A/B signals, adding
independent Gaussian noise with RMS 0.08 changed the outside/inside peak
ratios from 0.00170/0.00156 to 0.01265/0.01064, while integrated power
ratios changed from approximately 0.00002/0.00003 to 0.00223/0.00232.
Separately, full offline A/B decodes with deterministic uniform noise of
peak 0.13856 (approximately the same RMS) gave MAE 4.57/4.56 with the
default band-pass and 8.47/8.46 with the band-pass disabled. The noise
realisations and distributions differ between the measurement and decode
experiments. These examples show why an isolated spectral peak cannot be the
only criterion for enabling a less-filtered image path; no production
threshold or automatic switching policy has been established.

The previous spectrum diagnostic also omitted the adjacent ranges 801–1400
and 2400–2999 Hz. On the captured A/B image windows, adding a 0.15-peak
900 or 2600 Hz tone barely changed its distant-band ratios: it reported
peaks around 779/3021 Hz rather than the injected tones. With the 900 Hz
tone, neither the default nor the bypassed offline frontend acquired the
countdown. With 2600 Hz, the default path acquired but produced whole-image
MAE 17.67/17.58; the bypassed path did not acquire. The diagnostic now
reports adjacent guard-band peak and power ratios separately: on these
windows it identified 900/2600 Hz and the peak ratio rose from clean
0.0231/0.0188 to 0.623–0.763. A/B pure-tone controls cover both adjacent
bands and are registered as `test_sstv_avt_spectral_guard_bands` when
NumPy and the diagnostic dependencies are present. These ranges can also
contain legitimate SSTV synchronisation or transition energy; the figures
are not a validated classifier or a production switching rule.

`DECODIUM_SSTV_NUMPY_PYTHON` is an optional CMake path to a Python that can
import NumPy for the AVT spectral diagnostic; other tests retain their
existing interpreter. With it unset the diagnostic uses the usual Python if
NumPy is available. An explicitly configured interpreter that cannot import
NumPy fails configuration rather than silently omitting the diagnostic.
On the local Mac, selecting the existing NumPy-enabled Python registered the
test and CTest passed its A/B controls; explicitly selecting `/usr/bin/python3`
without NumPy failed configuration as intended, after which the working
configuration was restored. This affects diagnostic test discovery only.

For the default 900/2500 Hz second-order Butterworth cascade, numerical
evaluation of its coefficients gives approximate group delays of 2.677,
2.290 and 2.073 samples at 1500, 1900 and 2300 Hz respectively, at 12 kHz.
To test whether flattening that variation is sufficient, the offline
diagnostic added four, six or eight first-order all-pass stages (coefficient
0.4) after the existing preprocessor. On the clean captured A/B signals, the
unshifted first/last-column maxima were 14/85 and 9/73 with four stages,
13/81 and 9/71 with six, and 11/79 and 9/69 with eight. For eight stages,
a -1 sample shift yielded 31/44 and 32/41; -2 yielded 65/20 and 78/23.
None met a 20-level bound at both edges. These are small exploratory
all-pass configurations, not an exhaustive phase-equaliser design or an
application/RF experiment; no production filter was changed.

An additional least-squares FIR equalisation probe kept the existing
band-pass and used nine, thirteen or seventeen real taps. The reproducible
coefficient generator is `tests/sstv/tools/design_avt_equalizer_probe.py`;
the coefficients are supplied only to the optional offline capture test.
At its acquired anchor, first/last-column maxima for clean A/B were 33/111
and 31/112 with nine taps, 38/115 and 38/114 with thirteen, and 19/136 and
15/136 with seventeen. All three worsen the last-column error compared with
the unequalised offline result (94/80). No interference tests or production
changes are justified by these clean-signal results; these few designs do
not exhaust equalisation possibilities.

The optional offline capture diagnostic also permits varying the Hilbert
transform length using `DECODIUM_AVT_HILBERT_TAPS` (odd values 15 through
255). With the default preprocessor, the same clean A/B captures and the
acquired anchor, 15 taps produced first/last-column maxima of 18/95 and
8/82; 23 taps produced 16/96 and 10/80; and 47 taps produced 16/94 and
10/80. The standard 31-tap setup had last-column maxima 94/80. The test
run reports successful execution, not passage of the image-quality bound:
all these last-column results exceed 20. Changing only the Hilbert length
therefore does not establish a fix. These are offline diagnostics, not new
application or RF tests, and no production demodulator setting was changed.

At the acquired anchor, per-channel inspection of the captured smooth A/B
last columns found errors above 20 on respectively 186/178 red, 125/126
green, and 239/239 blue rows out of 240. Maximum errors were 94/80 red,
49/41 green, and 93/79 blue. With only the band-pass disabled in this
offline diagnostic, none of the 240 rows in any channel exceeded 20 for
either capture; the overall last-column maxima were 8/14. This identifies
the extent and colour distribution of the defect for these two patterns,
and supports a filtering contribution. It does not establish that removing
the band-pass is safe: the interference counterexamples above still apply.

For these known smooth inputs, a further offline diagnostic compares the
signed last-pixel error with the known jump to the next transmitted colour
component. A zero-intercept least-squares slope for red/green/blue was
0.363/0.353/0.344 for A and 0.310/0.327/0.311 for B with the default filter;
without the band-pass it was 0.014/0.012/0.005 and 0.030/0.022/0.016.
This is consistent with substantial next-component leakage into the last
pixel under the tested filter. The calculation uses known source pixels,
includes the last transmitted image line, and is not a causal proof or a
validated correction: arbitrary images, noise, and the final boundary need
separate testing before applying any compensation to production decoding.

An exploratory image-level correction uses 0.33 as the estimated leakage
and the decoded first pixel of the next transmitted component, excluding
the final blue boundary because there is no next image component. On the
filtered smooth captures A/B, the sum of last-column component errors fell
from 32234/28935 to 4961/1372, but the corrected maxima were 23/12 and A
still had 14 components over 20. On the same captures with the band-pass
disabled, the correction made the sums worse, from 1689/2229 to
17683/17899, with maxima of 85. On generated filtered A/B rapidly varying
patterns, the sums fell from 19289/18210 to 9168/6826, but corrected maxima
remained 56/51; on the two mostly constant contrast patterns they fell
from 28951/31595 to 3108/2591 with maxima of 16/16. These are offline or
in-memory diagnostics, and their QTest pass status only confirms execution.
They do not validate a general correction: the fixed coefficient depends
on the signal path, the next decoded pixel may itself be erroneous, and
the final image boundary is untreated. No production change was made.

The offline capture diagnostic accepts `bandpass-custom` with bounded
`DECODIUM_AVT_BANDPASS_LOW` and `DECODIUM_AVT_BANDPASS_HIGH` values to
compare intermediate filter bands. On clean A/B captures, first/last-column
maxima at the acquired anchor were 14/63 and 11/56 for 700–3000 Hz,
21/57 and 14/51 for 900–3000 Hz, and 21/60 and 10/51 for 700–2700 Hz.
For 900–3000 Hz, shifting the anchor one sample earlier gave 43/27 and
46/24, while two samples earlier gave 78/10 and 90/10. None meets 20 at
both edges on both images. These clean-only experiments do not demonstrate
interference resistance and did not alter the production band-pass.

The extreme constant-colour in-memory A/B patterns also measure the mean
fraction of an 800 Hz transition visible in the last pixel's interior
observations. At two/three samples before the component end, the fractions
were 0.1115/-0.0016 without the preprocessor and 0.5212/0.3318 with its
default settings, matching across opposite A/B patterns. The exploratory
fixed correction gave maximum last-column error 18 with preprocessing and
zero without on these saturated inputs. Saturation and clamping make that
result insufficient for general images, as the rapidly varying and smooth
counterexamples above show. The optional preprocessed loop skips the strict
unprocessed whole-image MAE assertion, so a successful test execution there
must not be interpreted as passing that quality bound.

An offline dual-path probe retains the default preprocessor for countdown
acquisition and all image pixels except the last pixel of each component,
where it substitutes observations from a second path with the band-pass
disabled. It explicitly carries up to five prior observations across
chunks and tests a whole-sample delay when aligning the paths; only the
zero-anchor session uses the substitution. With zero delay the clean A/B
last-column maxima were 240/244. At delays two, three and four samples they
were 86/81, 31/35 and 8/14 respectively; at delay four, first-column
maxima were 16/10. The 1500-sample component and 320-column selection is
specific to this 12 kHz AVT90 diagnostic. With a 0.15-peak 400 Hz added
tone, delay four gave last-column maxima 58/61 and whole-image MAE
7.466/7.341; with a 3300 Hz tone the maxima were 61/62 and MAE
16.418/16.374. Those interference results do not meet the clean-edge
quality bound. These figures do not prove general signal-path alignment or
safe switching, and no production decoder path was changed.

The dual-edge probe can also enable a configurable band-pass on its
secondary path. With a 500–3500 Hz secondary band and four-sample delay,
clean A/B last-column maxima were 3/5 and first-column maxima 16/10.
Adding a 0.15-peak 400 Hz tone yielded last-column maxima 29/32 and MAE
7.414/7.312; a 3300 Hz tone yielded 47/48 and MAE 16.408/16.353. With
deterministic uniform noise at peak 0.13856, last-column maxima were 24/24
and MAE 4.444/4.455. Although these are improvements over the unfiltered
secondary path in the tone examples, all three interference cases still
exceed the clean-edge bound and the same 20-level limit may not be a fair
criterion for noisy signals. These are A/B offline probes with a fixed
delay, not an application, RF, or arbitrary-image validation.

The 500–3500 Hz dual-edge probe was subsequently applied to the generated
rapidly varying and extreme A/B images. The diagnostic was corrected to
feed its final silence through the same preprocessors and to apply the
secondary path during the frontend flush; earlier optional preprocessed
loop metrics above were measured before that correction, so their final
pixel results should not be used as a corrected-flush comparison. With the
corrected flush, baseline versus dual-edge last-column maxima were 142/58
and 73/64 for the rapidly varying A/B, and 92/8 and 160/22 for extreme
A/B. The last case misses the 20-level clean bound, and the varying
patterns remain substantially above it. The dual-edge path is therefore
not a general fix even in these clean in-memory tests; successful QTest
execution does not assert this quality bound.

The corrected-flush, in-memory dual-edge diagnostic compared whole-sample
delays 2, 3, 4 and 5. Last-column maxima on rapid A/B were 35/32, 30/29,
58/64 and 112/120 respectively; on extreme A/B they were 32/56, 24/37,
8/22 and 0/10. A 50% blend of the frequency estimates at delays 3 and 4
gave maxima 44/45 and 16/30. The optimum changes with image content; none
of these choices met the 20-level bound for all four images. The fractional
blend is a diagnostic interpolation of already-demodulated observations,
not a validated fractional-delay filter. The results are consistent with
the risk of mixing the previous pixel when delaying too far, but do not by
themselves prove that mechanism.

An optional in-memory adaptive probe chooses between delays 3 and 4 for
each last-pixel observation, preferring the demodulated frequency nearest
the observation eight samples earlier. This heuristic produced maximum
last-column errors 58/64 on rapid A/B, 13/22 on extreme A/B, and 100/70
on A/B patterns whose final blue pixel contrasts with its neighbours.
In particular, the rapid A/B result is worse than fixed delay three
(30/29); these failures rule out adopting this simple continuity-based
selector. These are diagnostic executions, not image-quality passes.

Another in-memory probe aligns last-pixel secondary observations to the
same time six samples before the component end (bounded delays 0–5).
It produced last-column maxima 24/25 for rapid A/B, 26/36 for extreme A/B,
and 78/27 for the A/B inputs with a contrasting final blue pixel. Although
the rapid cases improved over fixed delay three (30/29), this still fails
the 20-level clean-edge bound and the final blue A case exceeds its 40-level
unfiltered regression bound. It is an experimental sample selection, not a
production change or a general image-quality pass.

Moving that diagnostic target to five or seven samples before the component
end did not resolve the trade-off: rapid A/B last-column maxima were 32/29
and 59/65, while the contrasting-terminal-pixel A/B maxima were 101/28 and
82/72. The target at six samples above remains the best of these three for
the rapid images, but none meets both image families' stated bounds. These
limited timing sweeps do not validate a correction; further work needs a
different reconstruction approach rather than assuming one fixed sample
position is sufficient.

As a further two-observation probe, linear extrapolation using 1.5 times
the delay-three estimate minus 0.5 times the delay-four estimate yielded
last-column maxima 20/23 on rapid A/B, 31/45 on extreme A/B and 82/16 on
the contrasting-final-pixel A/B patterns. This modestly improves the rapid
cases but fails the other cases; its negative coefficient can also amplify
noise, which was not measured here. It is not a validated reconstruction
method and has not been enabled in production.

The optional `DECODIUM_AVT_BOUNDARY_TRACE` path exports 13 valid filtered
frequency observations around each nonterminal AVT90 component boundary,
with source ground truth, for offline model evaluation. The helper
`tests/sstv/tools/analyze_avt_boundary_trace.py` fits 14 linear coefficients
on an A trace and evaluates separately on B traces. Training on smooth A
gave maximum error 5.15 on smooth B but 117.92 on rapidly varying B; training
on rapid A gave maximum 53.78 on rapid B. Training on extreme A was only
rank three and gave maximum 86.85 on rapid B. Each comparison covered 719
component boundaries and deliberately excluded the final boundary. The
estimator uses future observations and known source values during training;
it is not a production decoder, an independent oracle, or RF evidence.
These results demonstrate that this unconstrained linear fit does not
generalise sufficiently across the tested image families.

Training the same linear estimator on the combined rapid, smooth and
extreme A traces and testing on the three untouched B traces gave maximum
errors 42.53, 8.96 and 1.71 respectively. Reversing the split (combined B
to A) gave 15.14, 6.75 and 0.44. Each test covers 719 nonterminal
component boundaries. The asymmetric A/B result still fails the rapid-B
20-level bound, despite the broader training set, and does not include
noisy inputs or the final pixel. This is diagnostic evidence only; future
observations are used and no model is applied in production.

Two further deterministic A/B fixtures use independently seeded random
colours in eight-pixel horizontal blocks. Their unfiltered in-memory
last-column maxima were 34/12; the ordinary test now bounds both by 40.
The corrected-flush default-preprocessor path gave 92/130. The linear
models fitted on the three earlier families, without training on these
random traces, gave maxima 6.00 on random B and 5.97 on random A over 719
nonterminal boundaries. This does not reverse the rapid-pattern failure,
does not cover the final boundary or disturbed signals, and is not a
production or independent-peer test.

The generated eight-pixel random-block A/B fixtures were also exported as
mono 12 kHz float PCM by the optional in-memory test, resampled to 48 kHz
and given one second of trailing silence for the independent Revenant
adapter at the pinned revision above. Both decoded as complete AVT90 QRM,
VIS 70, 256×240. Against the original 320×240 references resized
bilinearly to 256×240, MAE was 1.168/1.177, p95 5/5 and maximum error
38/36, in each case at the last column. A crossed reference/image comparison
gave MAE 83.28 and failed a 10-level guard; the independent adapter's AVT
and FAX A/B self-controls also passed. The first attempt without trailing
silence reported only 239 complete rows, so its result was discarded.
This is an independent file-mediated decoder comparison at a different
horizontal resolution, not a direct 320-pixel comparison, an application
exchange or RF evidence. The optional PCM export uses the host float byte
order; the above run was on a little-endian Mac.

For the reverse file-mediated direction, the pinned Revenant encoder
transmitted two independently seeded random-block images at its native
256×240 resolution. Its own decoder reconstructed A/B with MAE 1.886/1.871
and p95 9/9 against the exact native inputs. The same encoded audio, with a
one-second silent tail, was played through BlackHole 2ch into the running
Decodium receiver with no injected interference. Against explicitly
bilinearly upscaled 320×240 references, Decodium completed both images with
MAE 2.738/2.751 and p95 15/15; its last-column maxima were 96/91. Both
fail the optional 20-level last-column guard even though the broad replay
guard passed. The crossed A-reference/B-reception control failed with MAE
81.54. The playback reported zero discontinuities. Artifacts are
`.../decodium-avt-interference-47lt32xt` and
`.../decodium-avt-interference-omkthzq_`. This shows the same boundary
symptom with an independently generated signal, but differing raster
resolutions and resampling prevent attributing the entire difference to a
single stage. It is file-mediated virtual audio, not a live independent GUI
or RF test.

The same independent A/B signals were replayed again through the application
with its test-mode-only band-pass bypass, and the lab now checks the
corresponding diagnostic log marker before accepting that setting. With the
normal filter, first/last-column maxima were 17/96 and 18/91; with the
bypass they were 23/28 and 24/18. B therefore passes a 20-level last-column
guard but fails a 20-level first-column guard. Whole-image MAE with bypass
was 1.589/1.607 and p95 8/8, with zero reported audio discontinuities.
Artifacts are `.../decodium-avt-interference-usclq8rh` and
`.../decodium-avt-interference-wzpltcg4`. This clean, file-mediated A/B
test supports a filter contribution for independent transmitted signals,
but does not remove differences caused by 256-to-320 resizing and does not
make the bypass safe under interference. The production filter is unchanged.

The boundary-trace helper also has a `--quadratic` option adding all pairwise
products of the 13 observed frequencies. With combined A training, maxima
on rapid/smooth/extreme/random B were 122.47/33.00/0.10/25.19; with combined
B training, maxima on the corresponding A cases were 87.32/18.01/0.01/14.39.
The fit rank was 103 out of 105 nominal coefficients. This unconstrained
nonlinear fit generalises worse than the earlier linear fit on several
held-out inputs and is not suitable for production. As before, the trace
omits the final boundary, uses future observations, and contains clean
synthetic data only.

Quadratic ridge fits with strengths 0.001, 0.1, 1 and 10 were also trained
on the combined A or B families and evaluated on the opposite side, with
the random-block traces held out of training. The rapid-pattern maxima were
respectively 93.58/75.11, 82.10/60.37, 78.71/47.49 and 86.97/64.12; random
holdout maxima ranged from 8.04 to 10.77. These parameter choices do not
meet the 20-level rapid-pattern bound and do not establish a robust model.
The diagnostic's optional ridge fitting has separate synthetic A/B controls
for exact recovery, an unpenalised intercept and shrinkage. This remains an
offline experiment using future observations and excludes the terminal
boundary and interference.

The ordinary unfiltered 12 kHz in-memory AVT90 QRM test now asserts an
explicit maximum error on the last column, in addition to whole-image MAE:
20 levels for both rapidly varying and both smooth A/B inputs, and 40 for
the A/B inputs with a deliberately contrasting terminal blue pixel.
Observed maxima before adding these bounds were 14/14, 9/14 and 37/23,
respectively. This regression gate covers the unfiltered in-memory frontend
only; the filtered application-path defect described above remains open.

The frontend test now measures A/B slowly rising and falling 1700–2100 Hz
chirps against the observation timestamps, with and without the default
preprocessor. On the local Mac, mean offsets were -0.648/-0.352 samples
without preprocessing and -2.961/-2.665 samples with it. The phase
discriminator naturally measures between adjacent samples, while its
timestamp is integer-valued; the A/B average without preprocessing is
approximately -0.5 sample. The extra delay with the preprocessor was about
2.31 samples for each chirp. These smooth-signal averages support the filter
delay analysis above but do not model abrupt colour changes or establish an
appropriate global anchor correction. The frontend suite including all four
chirp cases passed; production timing was not changed.

As an exploratory 320-column comparison, the pinned Revenant decoder was
built from a temporary copy with only its AVT90 output width changed from
256 to 320 (`fixtures/revenant-avt90-320-diagnostic.patch`); the checkout
itself remained clean. On the captured smooth A/B signals, the modified
decoder completed at 320×240 with MAE 0.125/0.150, first-column maxima
37/94 and last-column maxima 35/28 against the original 320-column images.
On the generated random-block A/B audio, it produced MAE 3.472/3.491,
first-column maxima 48/48 and last-column maxima 48/42. The crossed smooth
reference/image failed with MAE 113.69; the adapter's separate AVT24 and
FAX480 self-controls passed. This is a deliberately modified external
decoder, not evidence of interoperability with the unmodified application.
The experiment shows that merely requesting 320 columns from its 16 kHz
frontend does not satisfy the 20-level edge guard across these inputs, and
does not establish a ready-to-use production fix.

Two additional application-to-application AVT24 Narrow-QRM transfers used
the detailed A/B patterns and BlackHole 2ch, with capture disabled to limit
disk use and explicit 20-level guards on both image edges. Both receivers
reported complete 128×120 images, the requested mode and no audio
discontinuities or dropped chunks. Their MAE was 1.220/1.434 and p95 5/7,
but first-column maxima were 21/62 and last-column maxima 66/62. Both
therefore failed the explicit edge guards, even though the aggregate limits
would pass. Artifacts are `.../decodium-avt-two-app-avt-24-narrow-qrm-5xix0iqi`
and `.../decodium-avt-two-app-avt-24-narrow-qrm-836j6scu`. This is local
virtual audio, not RF. It shows that the edge-quality issue is not confined
to AVT90; it does not establish that both modes have the same cause.

Two further AVT24 Narrow-QRM application-pair A/B runs retained their
BlackHole captures and reference images. Decodium completed both with MAE
1.219/1.536 and first/last-column maxima 21/66 and 62/61. The unmodified
pinned Revenant adapter decoded those same captures as complete 128×120,
VIS 67, with MAE 0.892/1.942 and first/last-column maxima 28/27 and 55/31.
No image resizing was required. Both receivers exceed the 20-level edge
guard on these inputs; the independent decoder therefore does not isolate
the whole discrepancy to Decodium's receiver. Its separate AVT/FAX A/B
self-controls passed, and a crossed A-reference/B-output comparison failed
with MAE 84.15. The captures and Decodium output are under
`.../decodium-avt-two-app-avt-24-narrow-qrm-6xctaue3` and
`.../decodium-avt-two-app-avt-24-narrow-qrm-6bu5n25l`; independent outputs
are `/tmp/decodium-narrow-qrm-6xctaue3-revenant.ppm` and
`/tmp/decodium-narrow-qrm-6bu5n25l-revenant.ppm`. This comparison does not
by itself identify transmitter, channel or decoder causation and is not RF.

The inverse experiment used the exact same 128×120 reference images as input
to the unmodified pinned Revenant encoder. Revenant decoded its own files
with MAE 0.906/1.045 and first/last-column maxima 20/63 and 49/31. The same
audio, played through BlackHole into Decodium with no injected interference,
produced complete A/B images with MAE 1.305/1.468 and first/last-column
maxima 21/73 and 60/70, with no reported discontinuities. Both Decodium
results fail the 20-level edge guards; the A-reference/B-reception negative
control failed with MAE 83.93. No resizing was used. Artifacts for Decodium
are `.../decodium-avt-interference-wih9g025` and
`.../decodium-avt-interference-t3801wv1`; the independently decoded images
are `/tmp/revenant-narrow-a-self.ppm` and `/tmp/revenant-narrow-b-self.ppm`.
The independent self-decode was file-mediated, while Decodium used virtual
audio, so this does not equalise every channel condition or establish RF
performance. It does show that the chosen 20-level edge bound also fails
for the independent encoder/decoder on both of these images.

An in-memory AVT24 Narrow-QRM A/B loopback now recreates those two exact
128×120 references; exported PPMs were compared pixel-for-pixel with the
application-run references. With the production preprocessor bypassed,
first/last-column maximum component errors were 13/9 and 11/9; with the
preprocessor enabled, they were 33/73 and 85/67. The test asserts the
20-level bound on both edges for the unfiltered clean control; its generic
PASS in the filtered diagnostic does not assert that bound. This isolates a
reproducible contribution from preprocessing in this in-memory path, but
does not prove that it explains all discrepancies in the application and
independent-decoder experiments. Bypassing the filter is not a production
fix: prior interference controls show it compromises rejection.

As a further filtered in-memory diagnostic, the detected AVT24 image anchor
was shifted by -4 through +2 samples for both patterns. No shift brought both
edge maxima within 20 levels. At -2 samples the first/last maxima were
46/36 and 53/32; at -1 they were 23/41 and 55/37. This rules out a simple
integer anchor offset in that range as a sufficient fix for these cases;
it does not rule out other timing or filter-compensation approaches. The
`DECODIUM_AVT_LOOP_ANCHOR_SHIFT` switch affects only the test diagnostic.

An in-memory A/B sweep tested three preprocessing passbands on the same
AVT24 references, both clean and with a continuous 0.1-peak out-of-band tone
mixed into the encoder audio. With 900–2500 Hz, first/last maxima for A/B
were 33/73 and 85/67 clean, 52/88 and 103/79 at 430 Hz, and 57/113 and
130/111 at 2900 Hz. With 500–3500 Hz they were 11/58 and 32/35 clean,
63/66 and 85/83 at 430 Hz, and 129/72 and 128/111 at 2900 Hz; both 2900 Hz
tests also failed their generic image-quality assertion. With 300–4500 Hz
they were 10/40 and 17/27 clean; 430 Hz prevented acquisition for both
patterns, and 2900 Hz produced 104/75 and 97/85 with both generic quality
assertions failing. Thus widening the filter improves these clean edges but
is not a robust fix for these interference controls. These are controlled
in-memory examples, not an exhaustive interference or RF characterisation.

The two wider passbands were also tested with detected-anchor shifts of -2,
-1, 0 and +1 samples on both A/B references, clean and with the same 0.1-peak
430 Hz or 2900 Hz tone. None passed the 20-level bound at both edges on both
clean images: for example, 500–3500 Hz at -1 gave A 29/27 and B 27/16.
At 2900 Hz every 500–3500 Hz run failed the generic quality assertion; at
300–4500 Hz every 430 Hz run failed acquisition and every 2900 Hz run failed
the generic quality assertion. The other shifts did not address those
failures. This matrix excludes these tested fixed filter/timing combinations
as sufficient solutions, but does not exclude adaptive or other approaches.

The in-memory loopback can also discard selected leading or trailing
observations of the final pixel of each AVT24 component, before the normal
decoder aggregation. With the default filter, shifts -2, -1 and 0, and
lead/tail exclusions (0,0), (0,2), (0,3), (2,0), (3,0), (2,2), none of the
clean A/B combinations met the 20-level first/last-column bound. For example,
shift -2 and lead 2 gave A 46/28 and B 53/23; shift -1 and tail 2 gave
A 23/33 and B 55/23. These numbers do not prove anything about untested
windows or interference; the diagnostic only filters test observations and
does not alter the production decoder.

The same diagnostic was extended to exclude leading or trailing observations
of the first pixel independently. For each previously tested shift -2, -1
and 0, six first-pixel windows were combined with a last-pixel window that
had improved that shift's last-column result. No combination reached the
20-level bound on both edges for both clean references. For example, shift
-2 with two leading first-pixel and two leading last-pixel observations
excluded gave A 32/28 and B 44/23; shift -1 with two leading and two trailing
first-pixel observations and two trailing last-pixel observations excluded
gave A 20/33 and B 50/23. These remain in-memory diagnostic results, and the
test's generic PASS is not a pass of the 20-level edge bound.

The same captured two-application AVT24 runs were also inspected beyond the
single maximum. Out of 360 RGB component values per edge, Decodium's first
column exceeded 20 in 1/360 and 3/360 values (A/B), while its last column
exceeded 20 in 184/360 and 159/360. Last-column mean absolute error was
24.639/20.169 for Decodium versus 8.442/6.628 for unmodified Revenant on the
same captures. Thus the 20-level maximum is an exploratory guard, not an
independently established protocol acceptance limit, but the observed
last-column degradation is also distributed across many pixels and is not
merely an isolated maximum. The image comparator now reports per-edge mean
and p95 as well as maximum; these metrics do not themselves impose new gates.

The filtered in-memory AVT24 loopback now also reports mean absolute error
per edge. With default timing and aggregation the clean first/last means
were 1.836/26.764 (A) and 1.978/21.778 (B). Shifting the anchor -1 sample
and excluding two trailing observations of the last pixel gave 6.983/8.314
and 5.261/6.486: this markedly improves the last edge but worsens the first.
With a 0.1-peak 2900 Hz tone, the same candidate gave 29.822/29.978 and
30.044/29.814, versus baseline 27.569/37.056 and 29.856/34.525. These are
diagnostic sample cases, not application or RF proof; no fixed policy has
been selected and neither the generic test PASS nor these means prove that
the edge maxima or interference requirements pass.

To reduce that first-edge trade-off, the same -1-sample, last-pixel-tail-2
in-memory candidate was evaluated with six independently selected first-pixel
windows on both images and the 430/2900 Hz tone controls. Dropping the first
two observations of the first pixel changed clean first/last edge MAE from
6.983/8.314 and 5.261/6.486 to 2.958/8.314 and 2.142/6.486. The original
unshifted baseline was 1.836/26.764 and 1.978/21.778. For 430 Hz, the
candidate was 13.639/15.639 and 15.142/13.281, against baseline
12.300/28.356 and 13.572/25.003. For 2900 Hz it was 30.139/29.978 and
31.539/29.814, against baseline 27.569/37.056 and 29.856/34.525. This
candidate trades first-edge error against a lower last-edge mean and does
not establish a production improvement across interference, image content,
all modes, maxima, or real audio paths. It is still test-only.

Two additional deterministic 128×120 block-pattern A/B images, distinct from
the images used to choose the candidate, now exercise the same AVT24
Narrow-QRM in-memory loopback. Clean baseline first/last edge means were
1.244/23.219 and 1.250/23.500; the candidate gave 1.767/6.869 and
1.839/6.969. Last-edge maxima changed from 67/133 to 22/68. With a 0.1-peak
430 Hz tone the candidate means were 14.892/13.911 and 14.650/14.506,
versus 12.486/24.711 and 12.775/25.333 for baseline. With 2900 Hz they
were 30.725/30.192 and 30.989/30.703, versus 29.344/32.847 and
28.903/34.906. The candidate improves last-edge means in these examples but
does not establish acceptable maxima, first-edge preservation, broader
interference performance or application-level operation.

The -1 anchor and first-leading-two/last-trailing-two window have also been
exercised as a test-mode-only runtime diagnostic on the same recorded
AVT24 Narrow-QRM A/B captures, replayed through BlackHole into separate
Decodium receiver processes. Baseline versus candidate first/last column
mean errors were A 3.778/24.686 versus 6.139/8.553, and B 2.839/20.142
versus 4.600/6.808. All four received complete images with zero reported
discontinuities and no added interference; baseline/candidate logs confirm
that only the candidate applied the diagnostic. Baseline artifacts are
`.../decodium-avt-interference-lj0y96q0` and `...-k_of2egp`; candidate
artifacts are `.../decodium-avt-interference-z1bdp1fd` and `...-49_vp4hr`.
No maximum-edge guards were enabled in these runs. This is replay over
virtual audio, not RF, and it does not establish performance under
interference, slant correction, other modes or arbitrary imagery. The
diagnostic is gated by Qt test mode and leaves ordinary reception unchanged.

The same four application replay comparisons were repeated with a 2900 Hz
continuous tone at peak 0.1 mixed into the original capture (nominal active
signal-to-interference ratio about 18 dB, gain 0.8). All four images completed
without reported discontinuities, but all failed the lab's whole-image
MAE ≤ 5 and p95 ≤ 15 limits. Baseline versus candidate whole-image MAE was
21.234 versus 21.109 (A) and 20.998 versus 21.186 (B). First/last edge MAE
was 23.067/30.956 versus 24.639/22.783 (A) and 24.822/23.767 versus
27.819/23.794 (B). Baseline artifacts are `.../decodium-avt-interference-i_2qfs_i`
and `...-9jl9v6q_`; candidate artifacts are `...-npynvfe9` and `...-5nhcqjya`.
This shows substantial sensitivity to this interference for both paths and
does not justify promotion of the edge candidate to ordinary reception.
The full local `test_sstv_rx_runtime` and AVT session tests passed after the
test-only runtime diagnostic was added.

An additional controlled A/B replay used a 2400 Hz, 0.1-peak continuous
tone and the test-only edge candidate, comparing the existing manually
configured 2400 Hz interference notch against no notch. Whole-image
MAE/p95 with the notch was 1.518/10 (A) and 1.723/14 (B), passing the
lab's aggregate limits; without the notch it was 18.966/36 and 18.806/36,
failing them. All images completed without reported discontinuities and
the logs confirmed the notch and candidate. Notch artifacts are
`.../decodium-avt-interference-ont61zw5` and `...-2r69gudx`; controls are
`...-u7fc9noh` and `...-n67qc9yg`. No edge maximum guards were enabled.
This tests a known, stationary tone only: it is not proof of automatic
interference detection, arbitrary interference suppression, or RF operation.
An attempted 2900 Hz manual setting was rejected before RX because the
then-current runtime accepted notch settings only from 900 to 2500 Hz; it was not
a valid 2900 Hz notch trial.

As a negative A/B control for the same 2400 Hz tone and edge candidate, the
manual notch was deliberately set to 2300 Hz. Both images completed with
no reported discontinuities but failed the aggregate quality limits, with
whole-image MAE/p95 18.777/36 and 18.823/36, close to the corresponding
no-notch controls. Artifacts are `.../decodium-avt-interference-8uyw0ax1`
and `...-fo3wbnte`. This supports that the earlier improvement depended on
placing the notch near the known tone; it is not a measurement of its full
frequency response or an automatic tuning test.

The manual notch range is now extended from 900–2500 to 900–3500 Hz in the
UI, runtime and DSP validation, with an additional Nyquist bound. Existing
settings are preserved and the filter remains manually controlled. In the
same A/B application replay with a known 2900 Hz, 0.1-peak tone, ordinary
reception with the manually configured 2900 Hz notch produced whole-image
MAE/p95 1.305/5 and 1.522/7, versus 21.234/35 and 20.998/35 without the
notch. All four images completed without reported discontinuities. The
notch runs are `.../decodium-avt-interference-8oihhiq6` and `...-l982o8hj`;
the no-notch controls are `...-i_2qfs_i` and `...-9jl9v6q_`. Aggregate
limits passed with the notch; no edge maximum limits were enabled and the
unrelated AVT edge issue remains. These are virtual-audio replays with a
known stationary tone, not automatic tuning or RF proof. Local DSP tests
cover notch rejection and adjacent-tone preservation at 1700, 1900, 2900
and 3500 Hz; runtime bounds and QML configuration also passed.

The runtime acquisition regression now also tests AVT24 QRM and Narrow-QRM
prefixes at manual notch frequencies 1700, 1900, 2900 and 3500 Hz. Each
case verifies a valid VIS, the requested image mode and progressive image
coverage; all eight passed locally. These are bounded replay-prefix tests,
not full-image, interference or RF proofs.

Clean-signal A/B controls were then replayed with the new 2900 Hz notch
active and no injected tone. Whole-image MAE/p95 was 1.358/5 and 1.588/7,
versus 1.262/5 and 1.433/7 without the notch; all four images completed
with no reported discontinuities. Notch runs are
`.../decodium-avt-interference-qnnn53_y` and `...-3djmeetl`; previous
no-notch controls are `...-lj0y96q0` and `...-k_of2egp`. These controls
bound the observed change on these captures, not all SSTV modes or signals.

The upper endpoint of the extended manual range was exercised with the same
A/B application replays and a known 3500 Hz, 0.1-peak tone. With the notch
set to 3500 Hz, whole-image MAE/p95 was 1.229/5 and 1.479/7, passing the
aggregate limits; without the notch it was 13.934/25 and 13.975/25, failing
them. All four received complete images with no reported discontinuities.
Notch artifacts are `.../decodium-avt-interference-7lmq34jh` and
`...-n_z69_66`; no-notch controls are `...-9y9g5hwr` and `...-jt62lv8t`.
No edge maxima were gated; this is a known stationary tone over virtual
audio, not an automatic tuning or RF test.

The AVT24 edge candidate was also exercised in live A/B transmissions
between two Decodium processes over BlackHole, without injected interference.
Both completed with full coverage and no reported discontinuities; logs
confirm the -1 anchor and edge window were applied. First/last column means
were 6.147/8.567 and 4.628/6.781; maxima were 20/33 and 50/36. The broad
whole-image limits passed, with no edge maximum guards enabled. Artifacts
are `.../decodium-avt-two-app-avt-24-narrow-qrm-ne2mu3bj` and
`...-5_h3eyfe`. These data are local virtual-audio operation, not RF, and
do not resolve the first-edge trade-off or establish interference robustness.

The test-only AVT24 edge candidate was also applied to clean A/B waveforms
produced by the unmodified pinned Revenant encoder, replayed through BlackHole
into Decodium. Baseline versus candidate last-column means were 28.211 vs
10.489 and 23.108 vs 8.381; first-column means were 2.506 vs 4.469 and
1.936 vs 3.303. Whole-image MAE improved from 1.305/1.468 to 1.001/1.206,
but whole-image p95 worsened from 3/6 to 9/13. All four application outputs
were complete with no reported discontinuities and passed broad aggregate
limits; no edge maxima were gated. Candidate artifacts are
`.../decodium-avt-interference-hnxi379i` and `...-0c7g7dro`; baseline
artifacts are `...-wih9g025` and `...-t3801wv1`. This is useful independent
transmitter evidence but also confirms a quality trade-off away from the
edge; it is not a production fix or RF validation.

A subsequent experimental decoder option applies the -1-sample timing and
two-sample tail exclusion only to the last pixel of each component, keeping
the original mapping for every other pixel. It is disabled by default and
is exposed to the application only in Qt test mode for AVT24 Narrow-QRM,
12 kHz, dense observations and zero clock correction. Four automated
clean in-memory A/B cases assert that every non-final-column RGB value is
identical to the ordinary decoder and that total last-column error is less
than 60% of its control; all passed. Configuration tests reject unsupported
modes, sparse observations and nonzero clock correction.

In two live Decodium virtual-audio A/B transfers the experimental option
produced first/last edge mean errors 3.781/8.567 and 2.808/6.817, with
whole-image MAE/p95 1.139/4 and 1.511/6. The earlier ordinary runs on the
same patterns had edge means 3.794/24.639 and 2.817/20.169, and whole-image
MAE/p95 1.219/5 and 1.536/7. The candidate artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-3ns61fst` and `...-u37_4qsm`.
Both completed with no reported discontinuities and no edge maxima gates.
The same option receiving unmodified Revenant A/B waveforms yielded last
edge means 10.489/8.364 versus ordinary 28.211/23.108, while whole-image
p95 stayed at 3/6. Candidate artifacts are
`.../decodium-avt-interference-fk5g4ymh` and `...-f5m6a762`. The standard
AVT suites and four experimental tests passed locally. This is not yet a
production fix: missing/sparse observations, interference, clock mismatch,
other modes and RF require additional evidence before enabling it by default.

The in-memory split-edge test now runs twenty cases: four A/B patterns each
with no loss, two periodic dropped-observation patterns and 430/2900 Hz
0.1-peak tones. Each compares against a simultaneous ordinary decoder fed
the same observations and asserts identical non-final columns; all passed.
The clean and dropped-observation cases require total last-column error
below 60% of control, while the two specified tone cases require it below
98%. Dropping demodulated observations periodically is not equivalent to
an audio discontinuity, arbitrary bursts or RF fading.

The experimental last-pixel path previously discarded the normal final-pixel
observations, leaving sparse one-observation-per-pixel partial frames without
published coverage. It now retains a bounded normal-timing fallback for each
colour component, used only when the adjusted window has no valid observation.
Two synthetic A/B full frames with one observation per pixel match the ordinary
decoder exactly in pixels and coverage, despite different input chunk sizes.
Separate in-memory discontinuity cases after three and five lines stop as
partial frames and ignore subsequent observations. These simulate session
discontinuities; they do not establish behaviour under real audio dropouts.

After this change, two Decodium applications transferred patterns A and B
over virtual audio with full coverage and no reported discontinuities. Their
first/last column mean absolute errors were 3.789/8.544 and 2.819/6.781;
whole-image MAE/p95 were 1.094/4 and 1.338/6. Artifacts are
`.../decodium-avt-two-app-avt-24-narrow-qrm-wnidgj5b` and `...-umqvf30d`.
Both standard AVT suites and all twenty split-edge cases passed. These
virtual-audio tests did not gate maximum edge errors or demonstrate RF, and
the timing option remains experimental and disabled in normal application use.

The split-edge candidate was also replayed with the manual 2900 Hz notch and
the same 0.1-peak 2900 Hz tone used for the ordinary A/B controls above. Both
images completed without reported discontinuities. Last-column mean errors
were 8.539/6.856 versus 24.583/20.158 for the ordinary notch controls; the
first-column means were 3.886/2.978 versus 3.886/3.000. Whole-image MAE/p95
was 1.145/4 and 1.682/6, versus 1.305/5 and 1.522/7 for the controls: B's
overall MAE therefore worsened despite its improved final column. The
candidate artifacts are `.../decodium-avt-interference-qjrvxpmx` and
`...-j_1uigvt`. Maximum edge errors were not gated. This is virtual-audio
replay with one known stationary tone, not RF or automatic notch tuning.

With the same manual notch enabled but no injected tone, the split-edge
candidate completed both A/B images without reported discontinuities.
Last-column mean errors were 8.569/6.872 versus 24.583/20.158 for the
ordinary notch controls; whole-image MAE/p95 was 1.233/4 and 1.462/6 versus
1.358/5 and 1.588/7. Candidate artifacts are
`.../decodium-avt-interference-r6pbb0g9` and `...-4w0w_y_q`. The ordinary
and candidate runs are separate virtual-audio replays, not identical
demodulated observations, and no maximum edge errors were gated.

Post-hoc pixel comparison of these two separate replay runs found that the
maximum control-versus-candidate difference outside the first and last columns
was one RGB level for both A and B. For B, 17,493 of 45,360 interior colour
components differed, yielding interior MAE 1.631 versus 1.362 in the ordinary
control. For A, 2,438 interior components differed. These are comparisons of
separately played audio, not identical demodulated observations; they do not
identify the cause of the one-level differences or prove that the candidate
itself changes interior pixels. The simultaneous in-memory A/B cases above
remain the evidence that its mapping leaves non-final pixels unchanged when
both decoders receive identical observations.

In full Decodium replay with a 2900 Hz, 0.1-peak tone and no notch, the
experimental split-edge path completed both images without discontinuities
but failed the broad image-quality limits: whole-image MAE/p95 was
21.169/35 and 20.999/35. Ordinary controls were 21.234/35 and 20.998/35.
Last-column means were 22.769/23.797 for split versus 30.956/23.767 for
ordinary. Artifacts are `.../decodium-avt-interference-33bipx3y` and
`...-o_a29qf3`; controls are `...-i_2qfs_i` and `...-9jl9v6q_`. The
experimental path does not solve this interfering tone, and the small B
edge difference is not evidence of an improvement. No notch was used in
these comparisons.
