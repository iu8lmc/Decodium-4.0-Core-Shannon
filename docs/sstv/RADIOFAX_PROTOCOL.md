# Experimental radiofax core

## FAX480 consecutive reception

The FAX480 runtime previously discarded its receiver after a completed frame,
so subsequent frames required restarting monitoring. It now publishes the
completed image and rearms acquisition, passing observations after the frame
boundary to the new receiver without discarding the rest of the audio block.
An integration test feeds two different back-to-back frames for Dayton and
VIS 85 and requires distinct acquisition IDs and both image values. The core
suite also checks that an invalid-observation gap separates an earlier 1900 Hz
carrier from a later VIS leader; brief dropouts remain tolerated.

Two Decodium application sessions on macOS/BlackHole received consecutive
Dayton images A then B in one continuous RX session, with complete coverage,
distinct acquisition IDs and no reported dropped chunks, discontinuities or
processing failures. Their MAE/p95 were 0.110/0 and 0.065/0 respectively;
artifacts are `.../decodium-fax480-consecutive-a-a2yzxbk9`. The two sender
processes ran sequentially; this does not prove RF or other implementations.
The reversed B then A run also passed, with MAE/p95 0.065/0 and 0.110/0,
full coverage and distinct IDs; its artifacts are
`.../decodium-fax480-consecutive-b-zzilk3oc`.

VIS 85 remains unresolved in this scenario. Before the invalid-gap change,
one application run acquired only the second frame and another acquired only
the first. After that change, a further attempt was interrupted once its first
header had not been acquired; it is not a completed A/B test. The negative
artifacts are `.../decodium-fax480-consecutive-a-h763ochc` and
`.../decodium-fax480-consecutive-a-tnj_de16`. The synthetic gap test does not
establish why the live headers were missed or that this change fixes them.

An additional controlled test found a separate VIS defect: a continuous
four-second 1900 Hz VOX carrier followed by the VIS leader was submitted as
one event longer than the shared detector's two-second event limit and was
rejected. The adapter now bounds that aggregated leader to its last second
before passing it to the shared detector, retaining its actual end time; VIS
break, data and parity validation remain in the shared detector. A 750 ms / 4 s
A/B synthetic test failed on the latter before the change and passes after it.
The FAX480 core and consecutive-runtime tests passed on macOS ARM64.

Following this change, a local VIS 85 A then B application run acquired and
saved both images with complete coverage, distinct IDs and zero reported audio
discontinuities, dropped chunks or processing failures. The harness initially
reported failure because image files were written before its periodic
diagnostic snapshot; after inspecting the final logs, the saved images were
compared separately and passed MAE/p95 0.237/0 and 0.138/0. Artifacts are
`.../decodium-fax480-consecutive-a-3qc4v21k`. The harness now waits a bounded
interval for both completed diagnostic snapshots. The reversed B then A run
passed the corrected harness with MAE/p95 0.138/0 and 0.237/0; artifacts are
`.../decodium-fax480-consecutive-b-setdhdca`. Header recordings were captured
for diagnosis. The measured first A-run leader, including VOX, was about
1.06 seconds; consequently the four-second regression does not establish the
cause of the earlier intermittent missed headers. These selected local runs
are not independent interoperability or RF evidence.

The four-second VOX case was subsequently tested through two full Decodium
application sessions on macOS using BlackHole. The test-mode sender set the
same `SSTV/VoxPreKeyMs` property available to the application and confirmed
the selected 4,000 ms in its log. Two different VIS 85 images A/B were
transmitted and reconstructed with complete reported coverage, no reported
discontinuities, dropped chunks or processing failures, and MAE/p95 0.237/0
and 0.138/0 respectively. Artifacts are
`.../decodium-fax480-two-app-libsstv-vis-480-geyt67g6` and
`.../decodium-fax480-two-app-libsstv-vis-480-spgnajxh`. Reproduce using
`DECODIUM_LAB_SSTV_VOX_PREKEY_MS=4000` with
`run_fax480_two_app_lab.py libsstv-vis-480 a` and then `b`. This property
override is accepted only in test mode. These are local audio tests, not RF or
independent receiver evidence, and do not establish the cause of the previous
intermittent missed headers.

The `vis85LeaderDropoutAndRecoveryAB` core test also checks a 1 ms and an
8 ms low-confidence interval inside the second VIS leader. The shorter
interruption is tolerated; the longer one rejects that header, and a later
complete VIS 85 header is acquired. This is a synthetic observation-level
test, not an audio-path measurement of dropout tolerance or RF performance.

## Phasing-level calibration

The automatic receiver now estimates the observed black and white frequencies
from bounded, high-confidence phasing observations and maps that interval
back to 1500–2300 Hz for grayscale decoding. It uses the median in bounded
one-hertz histograms, requires at least 64 observations per level with at
least half near each median, and requires a valid multi-pulse clock and a
plausible frequency span. If those checks fail it retains nominal levels.
The calibration is fixed for the received image; it does not track a
changing carrier during the image and cannot establish that a noisy or
misidentified phasing sequence is authentic. Test-mode diagnostics report
the black and white levels chosen for each image.

On macOS ARM64, synthetic A/B checks with frequency scale 0.99 and 1.01
previously produced incorrect middle-grey values; the calibrated result is
within one level of the expected value. Low-confidence phasing keeps the
nominal levels, and out-of-range levels are rejected. The core suite passed
normally and under ASan/UBSan (leak detection disabled); the full RX runtime
integration suite also passed.

The two FLdigi captures described in the following section, extended and
resampled to simulate approximately ±1% timing and frequency changes, were
replayed again. Both had 120 complete lines and passed the unchanged MAE 8 /
p95 4 quality limits: A 0.343/1/max 27 and B 0.419/1/max 29. Artifacts are
`.../decodium-fldigi-radiofax-replay-w47vfkg8` and
`.../decodium-fldigi-radiofax-replay-b9q8b2up`. Before calibration the same
modified inputs failed the p95 limit (6 and 9 respectively). Two normal
IOC 576/240 LPM Decodium-to-Decodium transfers also passed with full coverage
and MAE/p95 0.172/0 and 0.106/0; artifacts with raw audio are
`.../decodium-radiofax-centered-a-agq3w2yt` and
`.../decodium-radiofax-centered-b-fafk2ury`. These selected local virtual
audio cases do not establish RF reliability, all signal impairments or
Windows/Linux behavior.

Two full Decodium processes also exchanged A/B images with IOC 288, 240 LPM,
centered phasing and a configured 128×120 raster on macOS/BlackHole. Both
reported complete coverage, zero discontinuities, dropped chunks and
processing failures. Against the prepared TX images, MAE/p95/max were
0.172/0/11 and 0.106/0/6. Artifacts are
`.../decodium-radiofax-centered-a-d8f0hbqs` and
`.../decodium-radiofax-centered-b-50g4jeb4`. Reproduce with
`DECODIUM_LAB_RADIOFAX_IOC=288` and
`python3 tests/sstv/tools/run_radiofax_centered_two_app_lab.py a 240`, then
`b 240`. This validates a selected local IOC 288 configuration, not all raster
dimensions, other line speeds, independent receivers or RF performance.

IOC 288 was also tested A/B at 60 LPM with 120 transmitted lines and a
150-line receive canvas. Both local two-Decodium runs detected the stop,
published exactly 120 lines and retained 80% coverage as partial rather than
complete; no audio discontinuities, dropped chunks or processing failures were
reported. The transmitted 120 rows had MAE/p95/max 0.034/0/2 and 0.016/0/1
against the prepared images. Artifacts are
`.../decodium-radiofax-centered-a-jw7zr41p` and
`.../decodium-radiofax-centered-b-tkifwjxt`. Reproduce with both
`DECODIUM_LAB_RADIOFAX_IOC=288` and
`DECODIUM_LAB_RADIOFAX_PARTIAL_STOP=1`, passing `a 60` and `b 60` to
`run_radiofax_centered_two_app_lab.py`. This validates stop and partial
coverage for the selected local setup, not independent receivers or RF.

Two additional IOC 288/240 LPM local two-Decodium sessions used WMO dead-sector
mode and asymmetric 320×256 grayscale patterns with detail in both axes.
Both reached complete coverage with no reported discontinuities, dropped
chunks or processing failures. Full-raster A/B MAE/p95/max were 1.218/7/51
and 1.365/9/48, within the existing 8/15 gates. Crossed-reference controls
failed with MAE 69.95/69.98 and p95 168/170. Artifacts are
`.../decodium-radiofax-centered-a-2n_dmdsx` and
`.../decodium-radiofax-centered-b-8dcbwowm`. The laboratory selected the
320×256 Martin M1 studio raster solely as the prepared source image; the
transmitted protocol was Radiofax, not Martin SSTV. Reproduce with
`DECODIUM_LAB_RADIOFAX_IOC=288`,
`DECODIUM_LAB_RADIOFAX_STUDIO_MODE=martin-m1`,
`DECODIUM_LAB_RADIOFAX_WMO_DEAD_SECTOR=1`, and a matching 320×256 source via
`DECODIUM_LAB_RADIOFAX_IMAGE`. These are local virtual-audio tests with
selected patterns, not independent receiver or RF interoperability evidence.

An independent live IOC 288 check used FLdigi 4.2.11 WEFAX288 at 60 LPM
alongside two Decodium applications, all on macOS/BlackHole. The TX images
were distinct asymmetric 128×120 patterns and Decodium used WMO dead-sector
mode. Its receiver reported complete coverage and zero discontinuities;
A/B full-image MAE/p95 were 0.106/1 and 0.108/1. FLdigi saved one 904×1300
PNG per run with metadata `WEFAX-IOC288`, 60 LPM, auto-centering on and
450 Hz APT stop. FLdigi was explicitly terminated after each test; clean
natural application shutdown was not tested.

After resizing FLdigi's width to 128 pixels without changing its row count,
direct full-raster A/B MAE/p95 were 22.390/115 and 20.580/114, failing the
8/15 Decodium comparison limits. An exploratory search excluding 24 columns
at each edge selected row +2, column 0 for A and row +2, column -1 for B;
its MAE/p95 were 6.185/36 and 6.833/43. The post-hoc offsets and cropped
metrics do not override the failed direct comparison. Automatic centering,
dead-sector display and the already documented FLdigi grayscale arithmetic
remain confounders. These cases establish stop detection on this setup, not
fully aligned independent interoperability or RF performance. Artifacts are
`.../decodium-fldigi-live-vqfhzl2v` and
`.../decodium-fldigi-live-htpyfnpt`, with simultaneous Decodium runs
`.../decodium-radiofax-centered-a-rigoztwt` and
`.../decodium-radiofax-centered-b-dkrvbjfq`. Set
`DECODIUM_LAB_RADIOFAX_IOC=288` when running
`run_fldigi_live_radiofax_lab.py a`, then `b`, to reproduce the live setup.

The live IOC 288 A/B comparison was repeated with FLdigi auto-centering
explicitly disabled in its isolated configuration. Both PNG metadata records
confirm `Auto-Center:Off`, IOC 288, 60 LPM and 450 Hz APT stop. Decodium's
simultaneous receive passed with A/B MAE/p95 0.106/1 and 0.108/1, complete
coverage and no reported discontinuities. After the same width-only resize,
FLdigi direct A/B MAE/p95 were 22.392/115 and 20.582/114, nearly unchanged
from 22.390/115 and 20.580/114 with auto-centering on. Post-hoc cropped
search found row +2 and columns 0/-1 with MAE/p95 6.187/35 and 6.826/43;
these diagnostic shifts do not constitute a direct pass. The on/off runs
were sequential, and the new isolated preference file also explicitly set
FLdigi version and dual-channel markers. In this specific setup, disabling
auto-centering did not resolve the misalignment or grayscale discrepancy;
it does not identify a universal cause. Artifacts are
`.../decodium-fldigi-live-f9vk709o` and
`.../decodium-fldigi-live-lnwxeklc`, with Decodium runs
`.../decodium-radiofax-centered-a-swhgpy4b` and
`.../decodium-radiofax-centered-b-pcuj6ayr`. Use
`DECODIUM_LAB_RADIOFAX_IOC=288 DECODIUM_FLDIGI_AUTOCENTER=0` with
`run_fldigi_live_radiofax_lab.py a` and then `b` to repeat.

As a normal-clock regression control after calibration, the two original,
unmodified FLdigi recordings were replayed into Decodium on macOS ARM64.
Both had 120 complete lines, detected stop and no reported dropped chunks,
discontinuities or processing failures. A measured MAE/p95/max 0.233/1/12
and B 0.249/1/12 against the prepared references, within the unchanged 8/4
thresholds. The prior uncalibrated results were 0.290/1 and 0.302/1. The
30 unused rows of the 150-line receive canvas remain uncovered as expected.
Artifacts are `.../decodium-fldigi-radiofax-replay-76f4dkjv` and
`.../decodium-fldigi-radiofax-replay-_5mg30p8`. This is a replay of the same
two local recordings, not additional RF or cross-platform evidence.

## Phasing clock tolerance and wider-offset checks

The phasing lock accepts periods within 2% of a named line rate. The later
clock tracker previously rejected deviations above 0.5%, which could make the
start detector mistake continuing phasing for image data. Clock estimation,
start detection and raster mapping now use the same bounded 2% nominal-rate
limit. The tracker additionally rejects a measured period that differs by
more than 0.5% of nominal from its previously tracked average. For a larger
initial offset, the most recent period from the already recognized phasing
seeds the start detector until a longer multi-pulse estimate is ready; normal
small offsets retain the nominal initial anchor. An invalidated tracker
clears the seed. The long estimate still requires six pulses over eight lines
and two seconds.

Synthetic tests on macOS ARM64 include -1%, -0.1%, +0.1% and +1% clocks,
all four line rates, both phasing patterns and 180-line phasing sequences;
an irregular period is still rejected. Before the fix, the new -1% tracker
and auto-start checks failed. Core Radiofax tests also passed under
ASan/UBSan (leak detection disabled), and the full RX runtime integration
suite passed on macOS ARM64. Normal-speed
two-Decodium A/B transmissions at IOC 576/240 LPM both reconstructed all
128×120 pixels, with zero reported discontinuities and MAE/p95 0.172/0 and
0.105/0 (`.../decodium-radiofax-centered-a-8ljj_u13`,
`.../decodium-radiofax-centered-b-1yybeo8i`). These are local virtual-audio
tests; the two-app runs did not introduce clock offset.

For a harder application-level probe, existing FLdigi A/B captures were
extended with 30 copies of a manually selected phasing line and resampled
using interpreted rates of 48,480 and 47,520 Hz respectively. This changes
both timing and audio frequencies. Each Decodium replay published 120 lines
with no reported input discontinuity, but both **failed** the existing p95
image-quality limit of 4: A MAE/p95/max 4.826/6/27 and B 2.096/9/36. The
replays are `.../decodium-fldigi-radiofax-replay-e2_ojzy1` and
`.../decodium-fldigi-radiofax-replay-coi8h1vn`, with manipulated signals
`/tmp/decodium-radiofax-clock-plus1-a.f32` and
`/tmp/decodium-radiofax-clock-minus1-b.f32`. These partial results do not
establish satisfactory image quality at ±1%, RF behavior, or operation on
other operating systems.

The new `SstvRadiofaxMapper`, `SstvRadiofaxEncoder`, `SstvRadiofaxPhasingDetector`, `SstvRadiofaxImageStartDetector`
`SstvRadiofaxRxSession` and `SstvRadiofaxAutoRx` are development
components. An opt-in, explicitly configured worker path publishes images
through the RX runtime. The Receive page exposes an experimental Radiofax
profile, with IOC and fixed output dimensions configured while monitoring is
stopped.
FAX480 is being developed separately as described below; its application
catalogue entry remains unavailable. Narrow FLdigi transmit-to-Decodium receive tests are documented
below; they do not establish general interoperability.

The FM profile uses the analog meteorological specifications in [WMO No. 386,
2009, section 5](https://www.weather.gov/media/marine/WMO_386_Vol_I_2009_en.pdf):
IOC 288/576, 60/90/120/240 lines per minute, 300 Hz or 675 Hz black/white
IOC selection, 30 seconds of symmetric or 5%-white phasing, and a 450 Hz
stop followed by black. This implementation chooses five seconds for the IOC
signal. FM black and white are 1500 and 2300 Hz. The IOC is not a fixed pixel
width; the image dimensions are supplied explicitly. In particular, the
conflicting 512×480 / 512×500 FAX480 observations are not silently resolved.

`test_sstv_radiofax` checks both IOC values, both phasing patterns and all four
rates against those landmarks, all line boundaries over 8,192 lines at 90
lines/minute, and a normal-versus-fragmented PCM A/B comparison. For all 16 IOC, speed and phasing combinations a locally
produced 32×4 grayscale image is passed through the native frequency
demodulator and two progressive RX sessions at the known image phase.
The received rasters must match across chunk sizes and stay within a bounded
mean grayscale error from the source. Missing observations remain uncovered;
regressing timestamps and cancellation are tested.
The phasing detector also identifies line rate and white duty cycle for all
16 generated profiles, invariant to observation chunking. Negative tests
cover constant, out-of-band and irregular signals. Its lock indicates a
repeating pattern only. An additional detector proposes the beginning of the
first observed line inconsistent with that pattern, and the test replays
buffered observations through RX using that candidate. All 16 locally
generated profiles reconstruct with the bounded grayscale error; a missing
observation interval invalidates the candidate detector. The experimental
continuous pipeline now handles lock, bounded two-second replay and progressive
RX, with A/B chunk-invariance tests across those same profiles. Its buffer
has a hard count limit and fails closed on excessive duplicate observations. An image can mimic
phasing, and long low-confidence gaps can prevent detection, so the candidate
is not an unambiguous image-start marker; IOC and output geometry remain
operator-supplied. The configured height is a maximum. A sustained 450 Hz
stop detector can terminate earlier, leaving uncovered rows marked partial.
These component tests are
not a two-instance test, an independent waveform, or an RF result.

An additional two-Decodium application A/B check used centered IOC 576 at
90 lines/minute, 128×120 and BlackHole on macOS ARM64. B completed with full
coverage and MAE 0.031/p95 0/max 2, while A stopped with a partial image,
118 complete lines and 0.990039 coverage. A detected stop; neither test
reported input discontinuities, dropped chunks or processing failures.
Artifacts are `.../decodium-radiofax-centered-a-06pc6pok` and
`.../decodium-radiofax-centered-b-ufxhi0sa`. This is a failed A/B check, not
evidence that the 90 LPM path is reliable; the image-dependent failure
needs investigation. `run_radiofax_centered_two_app_lab.py` accepts an
explicit 60/90/120/240 rate and checks source and audio continuity.
Repeating A and B with an additional simultaneous BlackHole capture passed
both: A MAE 0.060/p95 0/max 3, B 0.031/0/2, each with full coverage and no
reported capture or RX audio loss. Artifacts, including the raw audio, are
`.../decodium-radiofax-centered-a-lefllmai` and
`.../decodium-radiofax-centered-b-7w7plpu3`. The successful A's reported TX
duration was shorter than that of the earlier partial A, so wall-clock TX
duration alone does not explain the failure. The first failed A had no raw
audio capture; the defect remains intermittent and its cause is unproven.
Set `DECODIUM_LAB_FAX_CAPTURE=1` for future two-app runs to retain the signal.
That lab option now also logs the detected rate, image start and stop sample,
clock estimate, stop origin and counts for incomplete scanlines. A/B
two-application repeats at 90 LPM both completed with full coverage and no
reported audio loss. Each estimated 320,000 samples over 40 lines (8,000
samples/line), reported frequency-derived or absent direct-audio stop, and
had no incomplete rows. Artifacts, including captured PCM, are
`.../decodium-radiofax-centered-a-a7q8qx0h` and
`.../decodium-radiofax-centered-b-_emy3e5r`. The diagnostic's two stop-origin
paths are covered by `test_sstv_radiofax`. These successful repetitions do not
explain or erase the earlier intermittent partial A result.
Another A repeat (`.../decodium-radiofax-centered-a-il49j0sw`) also passed
with captured PCM and the same 8,000-sample line estimate. A post-hoc cyclic
row comparison of the original failed A found zero best horizontal shift on
rows 80–82, large mismatch and ambiguous shifts on rows 83–84, then a best
shift of +21 of 128 pixels on rows 85–119. The successful repeat had zero
best shift on those rows. The margin between the best and next candidate in
the shifted region is only 0.078 grayscale units per pixel, so this describes
the selected raster rather than proving a unique offset, lost samples or
their cause. `analyze_radiofax_row_shifts.py` reproduces the diagnostic;
its A/B synthetic-shift and constant-row ambiguity checks are in CTest.
No image pixels are altered or filled by this analysis.
The TX coordinator already fails a tracked SSTV session on an explicit Qt
audio underrun; an error-free Qt `IdleState` during priming is not by itself
treated as one. A test-mode trace now records source, processed, expected and
pending frames plus Qt error on tracked idle transitions when
`DECODIUM_LAB_TX_IDLE_TRACE=1`. The 90 LPM capture helper enables this trace.
Two subsequent A/B two-application runs completed with full coverage and no
reported capture or RX loss; their logs recorded only the initial idle at
zero source frames and final drained idle, both `NoError`. Artifacts are
`.../decodium-radiofax-centered-a-hbmwwjsz` and
`.../decodium-radiofax-centered-b-v85czd7a`. The TX coordinator test suite
passed. These successful traces do not establish whether the earlier failed
run had an unreported pause, and an idle transition alone is not proof of an
underrun. The intermittent failure remains open.

An additional two-Decodium A/B run on macOS ARM64 used IOC 576, centered
phasing and 60 LPM with two different 128×120 images. Both reached full
coverage with zero reported input discontinuities, dropped chunks or
processing failures. A measured MAE 0.0336/p95 0/max 2, B measured MAE
0.0157/p95 0/max 1. Both reported a clock of 300,000 samples per 25 lines
at the 12 kHz processing rate, and only initial and fully drained TX idle
transitions, both with `NoError`. Raw BlackHole captures and logs are in
`.../decodium-radiofax-centered-a-r9bole9c` and
`.../decodium-radiofax-centered-b-fs70r84q`. These checks cover local audio
on this Mac, not RF, other platforms or the earlier intermittent 90 LPM
failure.

As a controlled causal probe, 37,250 silent stereo frames (0.776042 s) were
inserted at 100 s in the successful A capture, leaving the remaining samples
in order. Replaying that manipulated signal to Decodium produced a partial
image with 119 complete lines; the row analyzer found best shift zero through
row 80 and +21 pixels after the damaged rows. The same recording without the
inserted silence replayed with 120 complete lines and zero shift on those
rows. Both replay paths reported no dropped audio chunks or discontinuities:
the injected silence was itself valid PCM, not a missing callback. Artifacts
are `.../decodium-fldigi-radiofax-replay-c1fh_aov` (modified) and
`.../decodium-fldigi-radiofax-replay-bqenqfgr` (unmodified); the modified
PCM is `/tmp/decodium-radiofax-90-gap-a.f32`. This shows that an inserted
pause can create a similar shift without RX transport loss, but the original
failed A was not captured, so its actual cause is still unknown. The replay
helper's `--expect-partial` validates an incomplete result for such synthetic
tests without treating image completion or visual similarity as a pass.

The opt-in RX runtime path takes an explicit IOC, image geometry and nominal
rate, runs the experimental phasing/image-start pipeline on preprocessed audio,
and publishes progressive image snapshots. A runtime A/B test reconstructs a
locally generated raster with two different PCM chunk sizes and compares the
images. This uses heuristic image-start detection; the runtime test is not a
live UI or independent-signal test. Discontinuities terminate the active
acquisition. The QML controls are exercised with a fixture, not a real receiver.

The Transmit Studio offers a separate experimental Radiofax TX action, using
the prepared image in grayscale and the selected IOC and line rate. It uses
the existing prepared TX coordinator and has no FSK ID. The encoder has a
bounded PCM adapter for that coordinator. Its cancellation and reset behavior and the complete generated
PCM stream are compared against the direct encoder. A coordinator test checks
the audio device, preflight/PTT lifecycle, 35-second header watchdog allowance,
and PTT release when a long header stalls. The stop and black tail are included in the protected protocol phase. A local
in-process test passes the 48 kHz TX PCM source through the 12 kHz RX worker
and reconstructs a small image. No physical PTT was exercised.

The experimental stop detector requires two matching one-second windows in
the same 440–460 Hz modulation bin (0.5 Hz spacing), and keeps a bounded three-second
observation buffer. It locates the first matching short window, backtracks
where applicable and aligns the boundary to the nearest raster line.
A/B tests cover chunking, 60/90/120/240 lines per minute, generated PCM, and
negative 300/675 Hz and constant patterns. A matching picture pattern can
still cause a false stop, and noisy or weak stop tones may be missed. The UI
reports stop detection and retains missing rows as uncovered; the resulting
image is partial when shorter than the configured maximum.

Remaining work includes reliable start and stop acquisition, more robust
image-start identification and line/slant
handling, additional dead-sector and phasing conventions, broader TX integration, further two-instance and independent-signal
coverage, and FAX480 geometry/profile reconciliation. A 675 Hz IOC selector
is ambiguous between IOC 288 and IOC 576 with alternate-line scanning; it
must not be classified solely from that selector.

### Two-instance local Radiofax check

#### Optional WMO white dead sector

The TX and RX pages offer an explicit, disabled-by-default 4.5% white dead
sector. In this profile the configured width describes the active image: TX
places white at the beginning of each raster line and maps the image across
the remainder; RX excludes the dead sector from image coverage and maps the
remaining samples to the configured width. The implementation uses a fixed
4.5%, rounded down to a whole number of samples per line. WMO No. 386,
sections 5.1.3.3 and 5.2.3.4, specifies a predominantly white dead sector of
4.5% ± 0.5%, with the leading edge of the white phase signal corresponding to
entry into that sector: [WMO No. 386, Volume I (2009)](https://www.weather.gov/media/marine/WMO_386_Vol_I_2009_en.pdf).
The option cannot be combined with the separate FLdigi centred-phasing
convention. The receiver does not automatically detect whether a station
uses this sector; the transmitter always makes the sector white.

Core A/B tests exercise 120 and 240 LPM, including a fractional 11025 Hz
line clock, raster boundaries, ignored observations in the sector, invalid
profile combinations, and insufficient active samples. On macOS ARM64, two
isolated Decodium processes using BlackHole 2ch, disabled CAT and audio VOX
also exchanged two 128×120 images at IOC 576 and 240 LPM. Both images had
complete coverage and no reported dropped chunks or discontinuities. The
aligned active-image MAE was 0.309 and 0.248 on a 0–255 scale for A and B;
both had p95 0 and maximum error 15. Reproduce with
`DECODIUM_LAB_RADIOFAX_WMO_DEAD_SECTOR=1 python3 tests/sstv/tools/run_radiofax_centered_two_app_lab.py a`
and the same command with `b`. These results do not establish interoperability
with independent WMO transmitters or receivers, RF operation, or behaviour on
other operating systems.

An additional receiver-core A/B uses WMO dead-sector observations with
estimated clocks at -0.6% and +0.6% relative to nominal 120 LPM. For each,
120 lines of an alternating 128-pixel pattern reconstructed exactly with the
matching supplied clock, while a nominal-clock control produced more than one
line's worth of mismatched pixel values. This is a synthetic observation-level
test of the supplied-clock raster mapping; it does not prove clock acquisition,
audio-device performance or interoperability at those offsets.

A further synthetic observation-level A/B exercises the automatic phasing
and image-start detector with WMO dead sector and clocks at -0.6%/+0.6%.
Both estimate the line period to within 0.2 samples, locate image start
within 10 samples, and publish 120 complete lines. After ten initial gray
lines, the remaining checkerboard pixels match exactly. The test now also
generates shifted 447.3/452.7 Hz FM stop modulation and checks its detected
boundary; it does not exercise raw-audio stop detection, real sound devices
or independent transmitters.

Two structured WMO recordings made from live Decodium TX/RX sessions were
resampled from 47712 and 48288 Hz to 48000 Hz, changing both their timing and
audio frequencies by approximately -0.6%/+0.6%. Before the modulation-bin
change, the unmodified recording A replayed with 120 lines, detected stop and
MAE/p95/max 0.226/2/6, while both resampled replays failed to detect stop and
continued to the configured maximum of 150 lines. Their first 120 image lines
had MAE/p95/max 0.254/2/11 and 0.195/2/5. A first detector change recognised
stop but truncated A to 119 lines; locating the short-window boundary fixed
that observed case. After both changes, the same resampled A/B files replayed
through BlackHole to Decodium with 120 lines, stop detected, no reported
discontinuities or dropped chunks, and MAE/p95/max 0.254/2/11 and 0.195/2/5.
The image limit was 150 lines, hence reported coverage 0.8; the other 30
lines remain uncovered. The negative baseline artifacts are
`decodium-fldigi-radiofax-replay-tkzf8mvy` and
`decodium-fldigi-radiofax-replay-bkspdb0o`; the corrected A/B are
`decodium-fldigi-radiofax-replay-42d_vmk2` and
`decodium-fldigi-radiofax-replay-9l0pu_g7`. This does not establish operation
on RF, other operating systems, or all clock offsets.

After the change two live Decodium processes also exchanged different
128×120 structured WMO images at 240 LPM over BlackHole. Both had full
coverage, no reported discontinuities and MAE/p95/max 0.461/4/13 and
0.486/5/13. Artifacts are `decodium-radiofax-centered-a-st0rfz3p` and
`decodium-radiofax-centered-b-xgueopbz`. No hardware PTT was exercised.

An additional boundary test exposed a gap at half-Hz offsets in the
original 1 Hz grid. The grid now uses 0.5 Hz spacing. Observation-level A/B
checks cover both 440 and 460 Hz edges, half-Hz points, the 447.3/452.7 Hz
stress cases, out-of-band 438/439/461/462 Hz, and alternating 444/456 Hz
one-second windows. On macOS ARM64 after this change, two live Decodium
sessions again passed the structured 240 LPM WMO A/B: full coverage, no
reported discontinuities, MAE/p95/max 0.461/4/13 and 0.486/5/13. Artifacts
are `decodium-radiofax-centered-a-12bqwmaf` and
`decodium-radiofax-centered-b-vi9xpodj`. The same -0.6%/+0.6% capture
replays passed at 120 lines with detected stop, no reported audio
discontinuity or dropped chunks, MAE/p95/max 0.254/2/11 and 0.195/2/5;
artifacts are `decodium-fldigi-radiofax-replay-m8sws1ze` and
`decodium-fldigi-radiofax-replay-hpj4vh4s`. These are local audio tests, not
an RF or independent-transmitter interoperability result.

Further A/B tests exercised the shifted FM stop under controlled noise. At
observation level, both 447.3 and 452.7 Hz cases used a ±75 Hz carrier offset,
20% low-signal observations and deterministic ±30 Hz perturbations; whole
and fragmented inputs detected the same stop. A negative control reducing
valid confidence below 60% did not detect stop. For an application-level
test, deterministic white noise was mixed across the entire -0.6%/+0.6%
captured audio, including the stop. Noise peak amplitudes were 0.015 and
0.025, with measured difference RMS 0.008656 and 0.014443 in a one-second
sample. Both BlackHole replays detected stop with 120 lines, no reported
discontinuities or dropped chunks, and MAE/p95/max 0.254/2/11 and 0.195/2/6.
They were transmitted previously by two local Decodium sessions; these
replays use the recorded signals rather than a live transmitter. Input copies
are `decodium-wmo-stop-noise-csh8p4f7/{a,b}.f32` in the macOS temporary
directory; RX artifacts are `decodium-fldigi-radiofax-replay-k8k_i84j` and
`decodium-fldigi-radiofax-replay-a9p2lwx5`. The deterministic noise was
generated by FFmpeg `anoisesrc` at 48 kHz with seeds 321/654, duplicated to
stereo and mixed without normalisation. These low-noise local checks do not
establish a minimum SNR, radio performance or general QRM resilience.

On 2026-11-02 an independent receive experiment replayed the two WMO-profile
Decodium captures through BlackHole to isolated, unmodified FLdigi 4.2.11
WEFAX576 sessions, with carrier 1900 Hz and AFC off. FLdigi saved one
1809×1300 PNG in each run; its trace then returned to `Receiving` after the
transmission. For comparison, the first 180 rows of each FLdigi canvas were
resized to 128 pixels wide, and the first 120 rows were compared without
alignment or dead-sector cropping against the TX-prepared references. A had
component MAE 65.349, p95 128 and maximum 255; B had MAE 52.602, p95 179 and
maximum 255. This comparison includes the sector. PNG metadata revealed that FLdigi
received at 120 LPM, while the source transmitted at 240 LPM. The mismatch
makes these image comparisons unsuitable as a controlled interoperability
verdict; the source of the rate mismatch remains to be investigated. The trace also does not prove reliable
stop/rearm. The TX captures and references are in the macOS temporary
directories `decodium-radiofax-centered-a-q38gejez` and
`decodium-radiofax-centered-b-4c8eu3im`; FLdigi outputs and state traces are
in `decodium-fldigi-phase-87jmkfrt` and `decodium-fldigi-phase-mvphq_2j`.
The replay command is `DECODIUM_FLDIGI_LAB_SECONDS=100 python3
tests/sstv/tools/run_fldigi_phasing_replay_lab.py CAPTURE.f32`; the sender
used the two-instance command above with `DECODIUM_LAB_FAX_CAPTURE=1` added.

The recorded audio first exceeded RMS 0.1 in a 10 ms block at 9.03 s (A)
and 9.37 s (B). Adding the nominal 35-second start and phasing interval
places image onset at approximately 44.03 s and 44.37 s; the first observed
FLdigi `Receiving` states occurred at 21.377 s and 21.913 s relative to
playback. This polling-based observation indicates an advance of about 22.65
and 22.46 seconds, subject to audio buffering and polling uncertainty.

As a diagnostic only, a replay skipped capture seconds 18–38, inside the
nominal phasing interval, without modifying the original captures. FLdigi
entered `Receiving` at 21.442 s and 21.928 s in the shortened A/B replays.
Both saved PNGs still report 120 LPM and the same unaligned comparison gave
MAE/p95/max 71.580/128/255 and 90.842/179/255. These are not valid
matched-rate interoperability checks. The diagnostic artifacts are
`decodium-fldigi-phase-_3v6jiqc` and `decodium-fldigi-phase-xwuoz68t`.
The replay helper accepts `DECODIUM_FLDIGI_SKIP_FROM_SECONDS=18` and
`DECODIUM_FLDIGI_SKIP_SECONDS=20` to reproduce this skip. The skip is
experimental and does not change Decodium transmission.

After the earlier 240/120 LPM mismatch, a 120 LPM live A/B used the same
BlackHole signal simultaneously in two Decodium processes and an isolated
FLdigi 4.2.11 WEFAX576 receiver on macOS ARM64. The Decodium receiver had
complete coverage and zero reported discontinuities: A MAE/p95/max
0.150/0/7, B 0.118/0/7. FLdigi saved one image per run with metadata LPM 120
and APT-stop frequencies 448 and 450 Hz. The same unaligned comparison as
above, including the dead sector, gave A MAE/p95/max 16.414/125/255 and B
14.788/81/255. Both traces subsequently returned to `Receiving`. Therefore
matched line rate and stop detection alone do not establish correctly aligned
FLdigi interoperability or reliable rearming. Automatic centering was on.
Artifacts are `decodium-fldigi-live-iuh0bfe2` and
`decodium-fldigi-live-6413ks6u`; their `decodium.log` files identify the
corresponding reference and Decodium output directories. Reproduce each case
with `python3 tests/sstv/tools/run_fldigi_live_radiofax_lab.py a` or `b`;
the laboratory requires FLdigi, BlackHole and at least 1 GiB free temporary
storage and does not capture raw audio.

The references in the preceding A/B had only one distinct row each, repeated
120 times; they could not reveal vertical misalignment. The live lab now
generates two distinct asymmetric 128×120 grayscale patterns with horizontal
and vertical detail. A new 120 LPM WMO-profile live A/B gave Decodium complete
coverage and zero reported discontinuities, with component MAE/p95/max
0.226/2/6 and 0.236/2/7. FLdigi saved both images with LPM 120, APT stop
450 Hz and auto-centering on. The grayscale diagnostic resized each
1809-pixel FLdigi canvas to 128 pixels in width, retaining its rows. The
direct full-raster MAE/p95/max was 31.290/122/215 for A and 28.206/122/215
for B. A post-hoc search over row offsets 0–20 and column offsets ±24,
excluding 24 columns on each side, selected row +4 and column 0 for both;
cropped MAE/p95/max were 6.077/31/122 and 6.857/43/131. These selected
offsets and cropped metrics are diagnostic, not a passing criterion; they
show a vertical alignment problem that the older fixtures could not expose.
The independent receiver's auto-centering and image conversion remain
confounding factors. Artifacts are `decodium-fldigi-live-mgm_tvgd` and
`decodium-fldigi-live-nfnwpycv`, with the Decodium paths in their logs.
No RF or other-platform behaviour was tested.

To test whether the length of the phasing interval alone explains the four
rows, the same structured A/B sources were transmitted again at 120 LPM and
recorded while Decodium received them with complete coverage, no reported
discontinuities, and MAE 0.226/0.236. Audio onset, measured in 10 ms RMS
blocks, was approximately 9.00/8.91 s in A/B. During isolated FLdigi
replay, capture seconds 18–38 were skipped: this lies inside both nominal
phasing intervals (14.00–44.00 and 13.91–43.91 s). The original captures
were retained unmodified. FLdigi's saved images reported 120 LPM, APT stop
450 Hz and auto-centering on. The same width-resized grayscale diagnostic
gave direct MAE/p95/max 31.282/122/215 and 28.199/122/215. Its post-hoc
search again selected row +4, column 0 for each, with cropped MAE/p95/max
6.073/31/122 and 6.857/43/131. Within the limits of separate transmissions,
audio buffering and FLdigi's auto-centering, skipping these 20 seconds did
not materially change the observed offset; it does not prove why the offset
occurs. Capture artifacts are `decodium-radiofax-centered-a-ohisvr14` and
`decodium-radiofax-centered-b-7xvp8z7i`, FLdigi replay artifacts
`decodium-fldigi-phase-hunrhpei` and `decodium-fldigi-phase-g9moe9gw`.
Use `DECODIUM_FLDIGI_LAB_SECONDS=150 DECODIUM_FLDIGI_SKIP_FROM_SECONDS=18
DECODIUM_FLDIGI_SKIP_SECONDS=20 python3
tests/sstv/tools/run_fldigi_phasing_replay_lab.py CAPTURE.f32` to repeat.
This artificially shortened replay is not evidence of standards compliance.

The first shortened replay did not test skipping phasing before FLdigi's
first `Receiving` transition: its transition occurred at 17.412/17.299 s
after playback start, whereas the skipped range began at capture second 18.
The same captures were therefore replayed again with seconds 15–35 skipped,
still inside the nominal phasing intervals and now before those transitions.
FLdigi's first observed `Receiving` state was at 17.352/17.288 s after
playback start. Both resulting images reported 120 LPM, APT stop 450 Hz and
auto-centering on; their direct and post-hoc metrics were numerically identical
to the previous shortened A/B, including row +4 and column 0. The two skipped
ranges differ by three seconds, an integer number of nominal 120 LPM phasing
lines, so this is not an independent proof of every phasing configuration.
The A/B artifacts are `decodium-fldigi-phase-zp803py1` and
`decodium-fldigi-phase-48_bre12`. Repeat with the previous replay command but
set `DECODIUM_FLDIGI_SKIP_FROM_SECONDS=15`.

An additional phasing-only probe shifted samples 16–43 seconds in copies of
the same A/B recordings 600 samples earlier at 48 kHz (12.5 ms, 2.5% of a
nominal line). Samples before and after that interval, including the image and
stop, were retained; the splice itself is artificial. Both unmodified and
modified captures were replayed to separate FLdigi 4.2.11 sessions, with
auto-centering on. Using the same grayscale width-resizing and ±24-column,
0–20-row diagnostic search as above, the unmodified A/B gave direct MAE
31.282/28.199 and best post-hoc row +4, column 0. The phasing-shifted A/B
gave direct MAE 36.755/35.374 and best post-hoc row +4, column +3; their
cropped MAE/p95/max were 6.160/32/122 and 6.904/44/131. Thus this probe
changed the observed horizontal position but did not correct the vertical
offset. Search results exclude the outer 24 columns and are not acceptance
tests. FLdigi reported 120 LPM and APT stop for both shifted replays.
Artifacts for unmodified replay are `decodium-fldigi-phase-_pflt708` and
`decodium-fldigi-phase-va6r8vyk`; modified replay artifacts are
`decodium-fldigi-phase-790veqll` and `decodium-fldigi-phase-g1f7a9xm`.
The shifted copies are in the original A/B capture directories as
`shifted-phase.f32`; reproduce with
`python3 tests/sstv/tools/shift_radiofax_phasing_probe.py INPUT OUTPUT
--start 16 --end 43 --shift-samples 600`. The helper was checked on synthetic
A/B buffers in both shift directions, including unchanged outside regions
and refusal to overwrite an existing output. No transmitter change is enabled.

The current public [FLdigi WEFAX source](https://raw.githubusercontent.com/w1hkj/fldigi/master/src/wefax/wefax.cxx)
contains logic that can reinitialise image positioning when additional
phasing is recognised, even after image reception has begun. This is a
possible explanation for a constant raster offset despite earlier reported
`Receiving` states, not proof of the execution path in the installed 4.2.11
binary. The state trace does not expose those internal resets.

The preceding 20-second cuts removed 40 whole lines at 120 LPM, a multiple
of four. A more discriminating A/B replay of the same original recordings
skipped exactly one 0.5-second line at capture seconds 16–16.5, within the
nominal phasing interval. Relative to the unmodified replay, the post-hoc
best row moved from +4 to +3 in both cases, with column 0. Direct full-raster
grayscale MAE/p95/max was 26.849/121/215 for A and 24.343/119/215 for B;
the cropped search gave 6.071/31/122 and 6.858/43/131. FLdigi metadata
reported 120 LPM, 126 rows, 450 Hz APT stop and automatic centering in both.
The original A/B files reported 127 rows. This result supports sensitivity
to the number of preceding phasing lines; it does not directly observe the
receiver's internal resets and does not constitute an interoperable waveform
or a valid acceptance criterion.

The [FLdigi 4.2.11 release commit](https://github.com/w1hkj/fldigi/blob/8745c4c4195232c42de9ce50e7d58c8d208bfb85/src/wefax/wefax.cxx)
contains a four-line phasing threshold and a reset path when more phasing is
recognised in image state. The release commit is source evidence, not a
verified build identity for the locally installed binary. Replay artifacts
are `decodium-fldigi-phase-bknrr8qi` and `decodium-fldigi-phase-ntlvzadd`.
To repeat, set `DECODIUM_FLDIGI_SKIP_FROM_SECONDS=16` and
`DECODIUM_FLDIGI_SKIP_SECONDS=0.5` in the replay command above. Fractional
skip settings are accepted only when they represent whole 48 kHz samples.

Two further A/B replays used the same captures and skipped three or four
phasing lines starting at second 16. With 1.5 seconds skipped (three lines),
the direct full-raster grayscale MAE/p95/max became 17.991/107/215 (A) and
16.640/100/215 (B); the diagnostic search selected row +1, column 0 for both,
with cropped MAE/p95/max 6.066/32/122 and 6.859/43/130. FLdigi's metadata
reported 124 rows. With two seconds skipped (four lines), the output returned
to the unmodified metrics: direct 31.282/122/215 and 28.199/122/215, search
row +4, column 0, and metadata 127 rows. All four files reported 120 LPM,
450 Hz APT stop and auto-centering enabled. Together with the single-line
experiment, the observed offset sequence is 4, 3, 1, 4 for 0, 1, 3, 4
removed lines. This supports periodic dependence on the phasing line count,
but the internal reset path has not been instrumented and the post-hoc crop
is not a passing result. Do not infer general WMO/FLdigi interoperability or
change the production phasing length based on this one receiver experiment.
Three-line artifacts are `decodium-fldigi-phase-bv21ayr5` and
`decodium-fldigi-phase-z2w1mc0_`; four-line artifacts are
`decodium-fldigi-phase-srn61zsl` and `decodium-fldigi-phase-4xskofjl`.
Reproduce using `DECODIUM_FLDIGI_SKIP_FROM_SECONDS=16` and
`DECODIUM_FLDIGI_SKIP_SECONDS=1.5` or `2` with the replay command above.

The preceding comparisons resized the complete FLdigi canvas, including the
WMO dead sector, to the active-image reference width. An exploratory analysis
instead read the 1809-pixel canvas as a circular raster and sampled 1728
columns (approximately 95.5%) from column 42, starting at received row 4.
These parameters were selected after inspecting these outputs; in particular,
column 42 is not an independently established automatic-centering rule.
Grayscale MAE/p95/max for A was 3.905/5/186 and for B 3.935/5/194. As a
comparison using the same row offset and nearest-neighbour sampling, sampling
all 1809 columns from column 0 gave 13.381/98/208 and 12.612/86/208. The
result is consistent with significant distortion from including the sector in
the width mapping, but the post-hoc offsets, auto-centering, large maxima and
filter/sampling differences prevent an interoperability pass. It is not a
correction applied to FLdigi or Decodium.

`tests/sstv/tools/analyze_radiofax_active_sector.py REF RECEIVED --first-row 4
--first-column 42 --active-columns 1728` reproduces the calculation, using
nearest samples and wrapping columns; it makes no automatic alignment claim.
`tests/sstv/tools/test_radiofax_active_sector.py` checks two synthetic active
rasters, including a wrap at the canvas edge and an unaligned negative control.


On 2026-10-31 two isolated QML processes used BlackHole 2ch, CAT disabled,
and audio-only VOX for a 576 IOC, 240 lines/minute, 128×120 transmission.
An initial attempt exposed a worker failure when an input discontinuity reset
the demodulator's sample coordinates without resetting the Radiofax detector.
A test reproduces that failure before the fix; the worker now publishes an
interrupted partial image and starts a fresh detector after a discontinuity.
The repeated local transmission completed, the receiver reported 100% image
coverage and no processing failures, and three discontinuities were recorded
before acquisition. The grayscale TX reference and completed RX image had a
mean component error of 0.270/255, median 0, 95th percentile 1 and maximum 18;
the acceptance limits were mean 2 and 95th percentile 5. The test uses the
same implementation at both ends, not an independent radiofax decoder or RF.

The test-mode-only `--lab-sstv-fax-rx IOC,width,height` and
`--lab-sstv-fax-tx IOC,lines-per-minute` options exercise the normal bridge
and worker paths. The test artifacts, including both PNGs and logs, were saved
temporarily in `/tmp/decodium-fax-fixed.XNGlP6/`. Compare the PNGs with
`tests/sstv/tools/compare_sstv_lab_images.py --max-mae 2 --max-p95 5`.

A follow-up two-process test used the same 576 IOC / 240 lines/minute TX
and 120-line image but configured the receiver for a maximum of 140 lines.
The 450 Hz stop was detected, the receiver published exactly 120 complete
lines and left the remaining 20 uncovered, and the result was marked partial
because the configured maximum was not filled. Comparing the received first
120 lines to the grayscale TX reference gave mean component error 0.270/255,
95th percentile 1 and maximum 18. Both processes exited zero. Three startup
audio discontinuities were recorded and no RX processing failure was reported.
The matching limits were mean 2 and 95th percentile 5. Temporary artifacts:
`/tmp/decodium-fax-stop.qkiMua/`. This test does not eliminate false-stop or
missed-stop risks on external/noisy signals.

### Independent FLdigi audio stop

An isolated FLdigi 4.2.11 session transmitted two different 128×40 grayscale
images in WEFAX576 through BlackHole 2ch to Decodium, with rig control and
hardware PTT disabled. The initial test received the raster but did not
recognize FLdigi's continuous 450 Hz audio stop. Decodium now also checks raw
12 kHz audio for a sustained narrow 450 Hz tone while receiving an image;
the existing black/white modulation detector remains in place. The direct
tone path uses ten 100 ms windows and requires sufficient amplitude and
spectral concentration. A bank of coherent detectors centered every 5 Hz
from 430 to 470 Hz tolerates modest tuning offsets; the same bin must match
throughout the ten windows. A sustained interfering tone in or near that band
can still cause a false stop, and weak or more widely shifted tones may be
missed. The onset estimate can lead or lag the actual boundary by up to one
100 ms window in the tested conditions.

Both repeated FLdigi-to-Decodium transmissions reported stop, saved a partial
128×60 output with 41 received lines and no worker processing failures.
Decodium exited zero in each run and the FLdigi XML-RPC send calls returned
success. FLdigi later exited with signal 11 when asked to terminate, so clean
FLdigi shutdown was not demonstrated. The original images have 40 lines;
both outputs include one extra leading line and approximately three pixels
of horizontal offset. Applying that alignment only for analysis produced
grayscale mean absolute errors 4.254/255 and 4.730/255, with 95th-percentile
errors 1 and 2 respectively; maximum error was 255. Thus exact image geometry
and alignment remain unresolved and this is not a complete interoperability
claim. Logs and images: `/tmp/decodium-fldigi-wefax/fixed-a` and `fixed-b`.

The component A/B test varies PCM chunking and leading offsets, accepts the
450 Hz tone and rejects 300, 675, 1500 and 2300 Hz. A focused runtime A/B
test exercises the new detector through the resampler, demodulator and image
handoff. The complete RX runtime suite initially had a reproducible Robot-C12
back-to-back acquisition failure outside the radiofax configuration. The
subsequent Robot B/W 8 timing fix resolved it; both the full RX runtime and
Robot RX session suites passed, as detailed below.

### Explicit FLdigi phasing alignment

The Receive page now offers an opt-in FLdigi phasing convention. Its 5%-white
stripe straddles the raster boundary, and its transmitter sends one all-white
line at the end of phasing. For this explicit convention the receiver moves
the candidate start by one line plus 2.5% of a line. The default convention
remains unchanged. This is a user-selected heuristic: an all-white first image
line is not distinguishable from an all-white phasing-end line on waveform
evidence alone, so it must not be auto-selected for arbitrary senders.

With this option enabled, a repeated FLdigi 4.2.11 → BlackHole → Decodium A/B
test used the same two 128×40 sources and a 128×60 receive ceiling. Both runs
reported 40 lines, stop detected, no worker processing failures, and zero exit
from Decodium and the XML-RPC sender. Without row or horizontal translation,
the first 40 received rows compared to the sources gave component MAE 5.509
and 6.110, p95 1 and 2, and maximum errors 190 and 189 respectively, passing
the comparison limits MAE 8 and p95 4, chosen after initial inspection rather
than independently in advance. These are lossy audio comparisons;
sharp edges still have large individual errors. The receiver deliberately
marks the 60-line canvas partial because the final 20 rows are uncovered.
Artifacts: `/tmp/decodium-fldigi-wefax/aligned-a` and `aligned-b`. Synthetic
two-pattern tests verify the expected start shift, and a QML test checks that
the selected convention reaches the receive configuration. A direct signal
from a radio and broader WEFAX variants remain unverified.

### Decodium TX into FLdigi receive: exploratory A/B

Two independent full Decodium test-mode transmissions sent different images
over BlackHole 2ch to an isolated FLdigi 4.2.11 receiver in WEFAX576 mode,
120 LPM, with CAT and hardware PTT disabled. Both Decodium processes reported
TX completion and exited zero. FLdigi saved two 1809×1300 PNG canvases, but
the received images were not aligned with the 128×120 TX-prepared references;
FLdigi was again in `Receiving` state after transmission. Its XML-RPC
`get_received_file(1)` returned an empty string even though files existed in
the isolated image directory. Consequently this is not a clean A/B pass or
proof of stop/rearm behavior.

For analysis only, the top 180 rows of each FLdigi canvas were resized to
128 pixels wide. Comparing the first 120 rows without alignment gave MAEs
46.41 and 42.47/255, p95 244 and 248. A search over row offsets 0–39 and
cyclic horizontal shifts -40–40 pixels found MAEs 10.35 and 7.68 after
shifts (row +4, horizontal -25) and (row +4, horizontal -1), respectively;
these shifts were selected after observing the output and do not constitute
an independently set acceptance criterion. The grayscale fixtures contain
large uniform areas, and the horizontal estimate may be ambiguous. Further
work must establish phasing, raster alignment, stop/rearm handling and the cause
of the difference between runs before interoperability can be claimed.

The isolated FLdigi configuration used BlackHole input/output, disabled
Hamlib, RigCAT, flrig and serial PTT, and bound XML-RPC to loopback only.
Artifacts are `/tmp/decodium-fldigi-rx-lab/a`,
`/tmp/decodium-fldigi-rx-lab/b`, and the two PNGs under
`/tmp/decodium-fldigi-rx-lab/config/images`. The local FLdigi instance was
stopped after the experiment. Its XML-RPC behavior is described in the
[FLdigi XML-RPC manual](https://www.w1hkj.org/FldigiHelp/xmlrpc_control_page.html).

### Explicit centered-phasing TX and follow-up A/B

The TX mapper and Studio now expose an explicit experimental centered-phasing
option: the 5%-white stripe is split across each line boundary and an
additional all-white line precedes the image. The original TX convention
remains the default. The test-only TX argument accepts an optional third
value, e.g. `--lab-sstv-fax-tx 576,120,1`. Mapper tests check both
conventions at 12/48 kHz and 120/240 LPM; a QML test checks both checkbox
states reach the bridge.

Two isolated Decodium processes transmitted and received two different
128×120 images using centered phasing, IOC 576, 240 LPM, CAT disabled,
audio-only VOX and BlackHole. Both completed with 100% coverage, no reported
processing failures or discontinuities, MAE 0.172 and 0.105/255, p95 zero,
and maximum errors 11 and 7. Artifacts are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-radiofax-centered-a-iaph5727`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-radiofax-centered-b-4rcva34c`;
`tests/sstv/tools/run_radiofax_centered_two_app_lab.py a|b` repeats the test.

The independent FLdigi 4.2.11 A/B experiment was repeated at 120 LPM with
centered phasing. Decodium reported completion for both, and FLdigi saved
1809×1300 PNGs, but the unaligned comparison still failed: after resizing
the first 180 rows to 128 pixels wide, the first 120 rows had MAE 58.95 and
49.01, p95 248 for both. A post-hoc alignment search found MAE 9.82 and 9.51
at row shift +5 and horizontal shifts -30 and -25 pixels. This option alone
therefore did not establish FLdigi RX interoperability. FLdigi had re-entered
`Receiving` after both transmissions. Subsequent PNG metadata inspection
confirmed `APT Stop` at 450 and 448 Hz; later rearm and image alignment
remain open. Artifacts are `/tmp/decodium-fldigi-tx-aligned/a`,
`/tmp/decodium-fldigi-tx-aligned/b`, and the two saved PNGs in the adjacent
`config/images` directory. The isolated FLdigi process was stopped afterward.

### FLdigi stop metadata and automatic centering investigation

The earlier uncentered A/B PNG metadata also records `APT Stop` at 450 Hz
for both images. Thus the previous observation that the receiver was again
active after TX does not mean it missed the stop. The PNG metadata records
`Auto-Center:On`; FLdigi's image code shows that automatic centering can
shift the raster, so a post-hoc horizontal shift is not a direct measurement
of transmitter phase alone.

A further A/B used centered TX at IOC 576/120 LPM, FLdigi carrier fixed at
1900 Hz and AFC off. Both transmitters completed and FLdigi saved images
with APT-stop metadata at 450 Hz. Unaligned MAE was 48.31 and 48.86/255;
the exploratory best shifts (+5 rows, -25 columns) gave 9.21 and 9.65/255.
An attempt to disable Auto during A reception did not survive through the
saved image: both PNGs recorded `Auto-Center:On`. These tests therefore do
not isolate its effect or demonstrate an alignment fix. A state trace for B
shows `Receiving` → `APT reception start` at the stop, then `Phasing
reception` and `Receiving` again during the remainder of the session.
The later rearm is observed but its precise trigger is not established.
Artifacts and trace are under `/tmp/decodium-fldigi-autooff`; no physical
radio or PTT was used.

### Centered TX phasing measurements from captured audio

Two additional full Decodium application transmissions of different images
used IOC 576, 120 LPM and centered phasing. The actual BlackHole output was
recorded as 48 kHz stereo float32. Both TX processes reported completion;
captures and logs are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-a-e9bfat6_`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-radiofax-tx-b-sx_qyjf2`.
The capture helper is `tests/sstv/tools/run_radiofax_tx_capture_lab.py`.

For diagnosis, the first 45 seconds were converted with ffmpeg to 11,025 Hz
mono float32 and passed to `tests/sstv/tools/analyze_fldigi_phasing.cpp`, an
approximation of the narrow-filter FM discriminator and threshold decisions
in the public [FLdigi WEFAX source](https://github.com/steliosxx/fldigi/blob/master/src/wefax/wefax.cxx).
For each capture, 46 complete phasing periods between seconds 12 and 35
measured 0.500 s; the measured white fractions ranged 4.99–5.01% for A and
were 5.01% for B. This calculation supports that the recorded clean TX
signal has the intended timing under the approximate detector. It does not
run FLdigi's state machine, reproduce its live sound-device path, or explain
why FLdigi later enabled auto-centering. The full application A/B result
above remains disaligned and is not overridden by these measurements.

#### FLdigi phasing-state replay

The recorded centered-phasing A/B transmissions above were replayed through
BlackHole to an isolated FLdigi 4.2.11 WEFAX576 receiver with carrier 1900 Hz,
AFC off, and CAT/PTT disabled. XML-RPC state polling at 100 ms observed
`Phasing reception` → `Receiving` after approximately 5.56 s for A and 5.37 s
for B. A repeat of B recorded the playback start: FLdigi entered `Receiving`
about 13.27 s into the recorded file, while the phasing analysis of that file
places the 30-second phasing interval approximately at 10.5–40.5 s. Thus,
in this specific replay FLdigi began receiving image rows while the source was
still transmitting phasing. State polling and audio buffering limit the exact
timing; these observations do not alone explain horizontal alignment or prove
that the receiver acquired all phasing correctly. State traces are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fldigi-phase-hbxa15ns`,
`.../decodium-fldigi-phase-l9kkhsno` and `.../decodium-fldigi-phase-sgldt3ib`.
The last folder includes `playback-start.txt` in the same monotonic clock as
`states.txt`. `tests/sstv/tools/run_fldigi_phasing_replay_lab.py CAPTURE.f32`
repeats the state capture; it requires FLdigi and BlackHole on macOS.

To investigate only, 25 seconds were cut from late phasing in each recording,
at whole line intervals, then replayed to FLdigi. Both reached APT stop and
saved images. The first 120 rows, after resizing the 1809-pixel-wide canvas to
128 pixels, still failed an unaligned visual comparison: A MAE 44.29/p95 248,
B MAE 36.34/p95 248. A post-hoc search found A/B approximate MAEs 7.99/6.71
with row offset +3 and horizontal shifts -24/-1 pixels, respectively. These
offsets were selected after observing output and are diagnostic, not an A/B
pass. Manipulated audio: `/tmp/decodium-fldigi-short-phase-{a,b}.f32`;
FLdigi images and traces: `.../decodium-fldigi-phase-ooxzhc3n` and
`.../decodium-fldigi-phase-tnlam993` under the same temporary folder. The
experiment does not justify changing the transmit protocol or establish RF
interoperability.

An attempted manual Auto-Center toggle during additional isolated A/B
replays could not be verified: both saved PNGs still report `Auto-Center:On`.
Consequently these runs cannot isolate the centering effect. The replay tool
now binds an ephemeral loopback XML-RPC port, checks the connected FLdigi
configuration directory against its isolated temporary directory, records
both status fields alongside the state, and bounds playback duration. Two
20-second A/B executions of the revised tool reached phasing and image
reception, respectively 13.014 and 13.276 seconds after playback began.
Artifacts are `.../decodium-fldigi-phase-wzq7rl_7` and
`.../decodium-fldigi-phase-tdra5i70` under the macOS temporary folder; these
short runs validate the trace workflow, not whole-image decoding.

```sh
c++ -O2 -std=c++17 tests/sstv/tools/analyze_fldigi_phasing.cpp -o /tmp/decodium-fldigi-phasing
ffmpeg -v error -f f32le -ar 48000 -ac 2 -i CAPTURE.f32 -t 45 -ac 1 -ar 11025 -f f32le /tmp/fax-phase-11025.f32
/tmp/decodium-fldigi-phasing /tmp/fax-phase-11025.f32
```

#### FLdigi-to-FLdigi self-loop control

On 2026-11-01, isolated FLdigi 4.2.11 transmitted two different 128×40
images through BlackHole. Their captured audio was replayed to a separate
FLdigi instance. Both saved images with APT-stop metadata. After scaling the
1,809-pixel receive width to 128, direct unshifted MAE/p95 values were
121.180/248 and 125.068/248. An exploratory search of row offsets 0–15 and
column offsets -32–32, excluding 32 columns on each side, found row +5,
column 0 and MAE/p95 8.049/9 and 8.289/9 respectively. These offsets were
selected after reception; they are diagnostic and are not an acceptance pass.
TX capture artifacts are `.../decodium-fldigi-tx-capture-4xq1g7x2` and
`.../decodium-fldigi-tx-capture-9mx0dwhn`; receive artifacts are
`.../decodium-fldigi-phase-19z8arj5` and
`.../decodium-fldigi-phase-69rcswo8` under the macOS temporary folder.

The control was repeated using the same 128×120 prepared reference images
as the earlier Decodium-to-FLdigi test. For image B, FLdigi-to-FLdigi direct
MAE/p95 was 48.401/247, compared with 49.009/248 for Decodium-to-FLdigi.
The post-hoc search above found row +5/column -24 for FLdigi TX and +5/-25
for Decodium TX, with cropped MAE 7.530 and 6.970 respectively. This one
comparison shows a similar receiver-side offset for both senders, but is not
an unshifted pass or proof for other images.

Image A did not provide a valid full-raster control: on both the initial
replay and an independent replay of the same recording, FLdigi left image
reception for APT at about 38.4 seconds and saved an image whose metadata
reported 80 rows, before a later APT transition near 79 seconds. In the raw
capture, one-second coherent 450 Hz amplitude was approximately 0.000051 at
38 seconds and 0.227867 at 77 seconds, against RMS 0.5006 in both windows.
This supports a premature stop classification in this local receiver test,
but does not establish its internal cause. TX captures are
`.../decodium-fldigi-tx-capture-24sf_s_l` and
`.../decodium-fldigi-tx-capture-_il1fhwc`; receiver artifacts are
`.../decodium-fldigi-phase-sj5lj76d`, `.../decodium-fldigi-phase-t2lj31h8`
and the A repeat `.../decodium-fldigi-phase-u39z_u88`.

The independent FLdigi TX capture can be repeated with
`tests/sstv/tools/run_fldigi_tx_capture_lab.py IMAGE`, followed by
`run_fldigi_phasing_replay_lab.py CAPTURE.f32`. The separate
`analyze_radiofax_2d_alignment.py` and `analyze_radiofax_tones.py` report
the measurements above; their A/B diagnostic self-tests are registered in
CTest. The observations do not demonstrate RF, all profiles, or full FLdigi
interoperability.

The same two 128×120 FLdigi captures were also replayed through BlackHole to
separate Decodium receive sessions with the explicit FLdigi phasing option and
a 128×150 canvas. Both detected stop with no reported processing failures;
each reported 15,359 covered pixels and 119 complete lines, one pixel short
of 120 complete lines. Unshifted comparisons of the first 120 rows gave
MAE/p95 3.304/1 and 3.534/3, within the previously used image-error limits of
8/4, but both runs **failed** the additional requirement of 120 complete lines.
Repeating both runs with laboratory coverage diagnostics located the missing
pixel at x=127, y=119 in each case. A further temporary diagnostic in case B
counted 46 observations in the final pixel interval, all classified out of
band; the last valid observation was 75 samples before the calculated stop
boundary at 12 kHz (6.25 ms). This narrows the investigation to the end
timing and demodulation, but does not establish the cause or justify filling
the missing pixel. The image-error measurements do not override the failure.
In the two original TX captures, the transition from the final white phasing
line to the black first image line and the transition from the black final
image line to the 450 Hz stop were also measured using positive zero crossings
in narrow, manually selected windows. The estimated intervals were 59.994452 s
and 59.995051 s, with bracketing intervals 59.993496–59.995407 s and
59.993773–59.996330 s. The nominal duration for 120 lines at 120 LPM is 60 s.
The diagnostic `analyze_radiofax_duration.py` reproduces this measurement;
its two synthetic timelines and missing-transition negative case are in CTest.
These are sample-clock-relative measurements: a short transmitted image,
audio clock mismatch, or other capture timing effects have not been separated.
The windows are based on the known black edge pixels in these test images and
are not a general-purpose Radiofax boundary detector. The duration difference
is consistent with the missing terminal coverage but does not by itself prove
its cause.
An independent measurement of the captured phasing, using stable
1500-to-2300 Hz crossings in a manually selected phasing-only window, found
18 edges spanning 19 nominal lines in each capture. The measured mean period
was 0.499943126 s in both recordings, approximately -114 ppm relative to
0.5 s; extrapolated across 120 lines this is 59.993175 s. This agrees in
order of magnitude with the independently measured image duration. The
diagnostic `analyze_radiofax_phasing_clock.py` has A/B synthetic clock and
insufficient-data tests. It tolerates missing edge detections by counting
nominal whole-line gaps, and rejects irregular gaps; it does not establish
which part of the audio path introduced the discrepancy.
The canvas is partial
because its remaining 30 rows were not transmitted. Artifacts are
`.../decodium-fldigi-radiofax-replay-u4bttoo9` and
`.../decodium-fldigi-radiofax-replay-7wqq1xeu`, with coverage repeats
`.../decodium-fldigi-radiofax-replay-us88vy0t` and
`.../decodium-fldigi-radiofax-replay-yulncxe6`, and the instrumented B run
`.../decodium-fldigi-radiofax-replay-lj248m38`; the replay script is
`tests/sstv/tools/run_fldigi_radiofax_replay_lab.py`. This is a macOS local
audio test and does not establish Windows, Linux, or radio operation.

The automatic receiver now estimates a rational line period from confirmed
phasing pulses when at least six pulses spanning eight lines and two
seconds are available. It rejects irregular gaps and estimates beyond the
bounded nominal-rate tolerance, retaining nominal timing if the tracker is
not confident. The estimated clock is used for the image anchor, raster
boundaries, and stop alignment; pixels still require valid observations.
Synthetic tests cover 60, 90, 120 and 240 LPM, both phasing patterns, faster
and slower clocks, rejected pulses, and direct raster mapping. The exact
capture replay above was repeated on macOS after this change: both cases
reported 120 complete lines and no processing failures. The unshifted A/B
MAE/p95 were 0.290/1 and 0.302/1, maximum error 12 in both, within the same
pre-existing MAE 8/p95 4 limits. The unused 30 rows remain uncovered. Artifacts
are `.../decodium-fldigi-radiofax-replay-5vvs647h` and
`.../decodium-fldigi-radiofax-replay-nk5we4v3`. This is evidence for the
specific recordings and configuration, not all senders, RF conditions, or
operating systems.
The focused Radiofax CTests and the full `test_sstv_rx_runtime` integration
suite also passed on this build.

The same independent captures were replayed again with stricter lab checks:
every RX diagnostic snapshot must report BlackHole 2ch as the source and zero
discontinuities, dropped chunks and processing failures. Both runs passed,
with 120 complete lines, stop detected and an 80% covered 150-line canvas.
Unshifted A/B MAE was 0.290/0.302, p95 was 1 for each, and max was 12.
Artifacts are `.../decodium-fldigi-radiofax-replay-1w699050` and
`.../decodium-fldigi-radiofax-replay-hgypsck6`. These are the same two
previously recorded FLdigi transmissions, not new transmitters, RF tests, or
evidence for other operating systems.

The phasing-start detector now also follows the validated rational clock
estimate while waiting for image data. Previously it used nominal timing
until image acquisition, so long phasing with clock drift could be mistaken
for image content. Synthetic A/B checks cover 180 phasing lines, clock offsets
of -0.3% and +0.3%, all four line rates and both phasing patterns; the earlier
implementation detected image start prematurely in the test. The clock
estimate still requires at least six confirmed pulses spanning eight lines,
at least two seconds, and bounded regularity; nominal timing is retained
when no valid estimate is available. These synthetic checks are not RF proof.
After the final two-second threshold change, two full Decodium process pairs
again passed 576/240 centered-phasing A/B over BlackHole, with complete
coverage, no reported discontinuities, MAE/p95 0.172/0 and 0.105/0, and
maximum errors 11 and 7. Artifacts are
`.../decodium-radiofax-centered-a-astdpgau` and
`.../decodium-radiofax-centered-b-zmqcbbms`. The Radiofax suite also passed
under AddressSanitizer and UndefinedBehaviorSanitizer on macOS ARM64 (leak
checking disabled). Before the final threshold adjustment, both independent
FLdigi 576/120 recordings were replayed to Decodium with 120 complete lines,
MAE/p95 0.290/1 and 0.302/1 (`.../decodium-fldigi-radiofax-replay-y8_op0h5`,
`.../decodium-fldigi-radiofax-replay-qoh3_07l`). Eight lines at 120 LPM span
four seconds, so the two-second minimum does not shorten that specific
threshold.

After the final adjustment the original FLdigi A/B captures were replayed
again with 120 complete lines and MAE/p95 0.290/1 and 0.302/1
(`.../decodium-fldigi-radiofax-replay-8b3v8ly3` and
`.../decodium-fldigi-radiofax-replay-0wieu1mz`). A further A/B stress test
modified the two independent FLdigi captures: 30 copies of a manually selected
phasing line added 14.99875 seconds, then FFmpeg changed the interpreted
48 kHz sample rate to 48,144 Hz for A and 47,856 Hz for B before resampling
to 48 kHz. This simulates a clock offset and also shifts audio frequencies;
it is not an RF recording or a clean independent transmitter test. Both
replays produced 120 complete lines and passed the existing unshifted MAE 8,
p95 4 thresholds: A MAE/p95 1.030/1, maximum 14; B 0.799/3, maximum 20.
Artifacts are `.../decodium-fldigi-radiofax-replay-7rl3m_1y` and
`.../decodium-fldigi-radiofax-replay-jg1und89`.

`tests/sstv/tools/prepare_radiofax_clock_stress.py` reconstructs the modified
files from the original stereo float captures, with `--edge-seconds
8.8316503041` for A or `8.7547336374` for B, `--line-seconds 0.49995603717`,
`--copies 30`, and `--asetrate 48144` or `47856`, respectively. The selected
edges and period come from the phasing diagnostic; the helper does not detect
or validate phasing itself. Both recreated files matched the original
manipulated files byte-for-byte, and an overwrite attempt was rejected. Their
SHA-256 values are `b1a5c7662eeec5f26a6dc6f11d2f8b08ad86fb829e6d8c03cc4681ad21e50319`
and `a57aa461a18b6cebec35cae2305bd1196508e9607085330282a654aed7080678`.

On the same final build, two complete Decodium-to-Decodium WEFAX 576,
240 LPM transfers over BlackHole passed using different images. Both had
full coverage, no reported audio discontinuities, and no reported processing
failures. Unshifted A/B MAE/p95 were 0.172/0 and 0.105/0 against the
transmitter-prepared references, below the lab limits of 8/15. TX and RX
processes exited successfully. Artifacts are
`.../decodium-radiofax-centered-a-l68b119u` and
`.../decodium-radiofax-centered-b-rfowefop` in the macOS temporary folder.
This does not prove RF transmission or other platform backends.

### Robot B/W 8 terminal timing regression

The first of two immediately consecutive frames could remain active for the
67 ms-profile terminal guard and consume the beginning of the next VIS header.
The runtime now permits a confident observed 66 ms line-period estimate to
shorten that guard. The alternate 67 ms decision is measured against its
actual line period, excluding the terminal guard. Tests feed 120 lines at
each timing and check that the image is not clipped. The existing back-to-back
B/W 8 → Robot-C12 runtime test now passes. The independent pinned PySSTV Robot
B/W 8 WAV, with verified SHA-256, also passed with 120 lines, full coverage
and 18.678 dB PSNR. The full RX runtime and Robot session suites passed.

Two isolated Decodium-to-Decodium Robot B/W 8 transfers through BlackHole
received all 120 lines and saved complete images; both TX and RX processes
exited zero. Against the TX-prepared grayscale reference, component MAE was
1.480/255 for image A and 0.249/255 for image B, with p95 1 and 0 and maximum
errors 78 and 29. These are measured values, not a pre-registered threshold.
Logs and images are in `/tmp/decodium-robot-isolated-a-ref` and
`/tmp/decodium-robot-isolated-b`. Two earlier runs while the RX suite was also
running terminated after 8 partial lines when the audio timeline reported a
gap of about 9.6 ms. The source of those gaps remains unproven; the isolated
runs still reported smaller gaps before image acquisition. This does not
prove reliable operation under heavy load or with physical radio hardware.

### Capture clock and callback jitter

For native macOS AudioQueue input, the audio sink now passes a valid capture
sample position to the SSTV relay. The relay anchors that device clock to the
local monotonic domain rather than treating the callback arrival time as the
start of the block. For sources without a valid device position, the existing
callback-time behavior remains in use. A detected backward or repeated source
clock marks an explicit discontinuity and forwards the block rather than
silently dropping it. The device-clock timeline allows 100 microseconds for
decimation and integer rounding; larger sample-clock gaps remain visible. The
100 microsecond allowance means smaller real gaps may be absorbed.

A deterministic A/B timing test shows that a late callback with contiguous
device samples does not create a gap, while a 5 ms gap in device sample time
does. A separate runtime test checks that a source-clock reset marks a
discontinuity without losing the received block. With the full RX suite
running concurrently, two Decodium Robot B/W 8 sessions through BlackHole
received images A and B completely: both reported 120 lines, no timeline
discontinuity, active capture sample clock and zero TX/RX process exits.
Against the TX-prepared grayscale references, mean component errors were
1.319/255 and 0.602/255, p95 1 for each, and maxima 78 and 30. These are
observations, not pre-registered acceptance limits. The earlier loaded runs
using callback arrival time ended after 8 lines with a 9.6 ms reported gap;
this comparison is consistent with callback scheduling jitter, but the old
logs did not record device sample positions and cannot prove its cause.
Artifacts: `/tmp/decodium-sourceclock-final-a` and `-b`. This macOS virtual
audio test does not establish behavior for every device, other operating
systems, RF, or overload beyond the tested conditions.


### Frequency-shifted direct stop tests

The direct audio stop detector searches nine coherent bins between 430 and
470 Hz using ten successive 100 ms windows. In the detector A/B test,
synthetic 430, 435, 437, 450, 463, 465 and 470 Hz tones were detected with
two input chunking patterns and two leading offsets; 300, 400, 500, 675,
1500 and 2300 Hz tones were rejected. An alternating 430/470 Hz signal
failed the persistence requirement, and disabling the detector cleared its
accumulated matches. In the RX runtime, synthetic image audio followed by a
437 Hz stop and a separate case followed by a 463 Hz stop both reached the
partial-image handoff with stop detected, using different chunk sizes. These
are generated-audio tests; a subsequent separate-process virtual-audio test
is described below. The detection band increases the
risk of a sustained interfering tone being mistaken for a stop.

The detector now also clears a previously latched detection when disabled,
and advances its absolute sample clock across the remainder of a block after
detection and across later blocks while latched. Previously disabling after
detection returned the stale result instead of rearming. A/B component tests
cover two block sizes, detection, disable and a second detection at the exact
expected stream sample. The complete Radiofax and SSTV RX runtime suites
passed locally. Repeated virtual-audio tests detected 437 and 463 Hz with 16
lines and no discontinuities (`.../decodium-shift-stop-437-6eqqemec` and
`.../decodium-shift-stop-463-05cmgc0d`). Two Decodium applications also passed
centered Radiofax A/B with complete 120-line images and no discontinuities
(`.../decodium-radiofax-centered-a-w49x7z6v` and
`.../decodium-radiofax-centered-b-a7chxq8q`). These app tests check normal
reception, not detector rearming across two receptions in the same session.

The runtime processes frequency observations before passing the associated
PCM chunk to the direct-audio stop detector. If image acquisition begins in
that chunk, samples preceding the image boundary must not latch a 450 Hz stop
candidate: such a stale candidate is rejected by the image receiver and can
otherwise mask the real stop. The detector now takes the absolute image-start
sample, skips earlier PCM while maintaining its stream clock, and clears
partial windows when the boundary changes. A synthetic test using two chunk
sizes placed a sustained 450 Hz tone before the image and another afterwards;
before this change it returned sample 2,400, and afterwards it returned the
actual later stop at sample 24,000. It also checks a boundary learned after
partial windows. These are detector tests, not a two-application pre-image
interference reproduction.

The direct-stop runtime A/B test and 437/463 Hz virtual-audio application
checks passed after the final change (`.../decodium-shift-stop-437-v4zxtzq9`,
`.../decodium-shift-stop-463-xxzssr6k`). Clean two-Decodium centered
Radiofax A/B checks also passed with complete 120-line images and no reported
discontinuities (`.../decodium-radiofax-centered-a-jq_zyy16`,
`.../decodium-radiofax-centered-b-bkuoikbs`). The complete Radiofax and RX
runtime suites passed before the last boundary-update refinement, and the
focused runtime test passed after it. These results do not establish noisy
RF, other operating systems, or freedom from false stop signals within the
image itself.

A subsequent macOS A/B test exercised the application and actual virtual
sound-device path: a separate PortAudio process generated a 16-line grayscale
raster, phasing and an audio stop at 437 Hz (rising ramp) or 463 Hz (falling
ramp), and sent it to Decodium over BlackHole 2ch. Each run used a new
Decodium test profile. Both runs reported stop detected, 16 received lines,
zero processing failures and no capture discontinuities. The first 16 rows
had mean grayscale errors 0.154/255 and 0.120/255 respectively. This is a
synthetic transmitter in another process, not a second Decodium TX, an
independent radiofax application, or an RF test.

A failed initial script rerun reused a profile and opened `USB Audio CODEC`
for SSTV receive despite a BlackHole monitor override. The script now creates
a unique profile and checks the actual SSTV `sourceDevice` in readiness and
final diagnostics; it rejects the wrong device before playback. Reproduce on
macOS with BlackHole 2ch, PortAudio and ffmpeg installed:

```sh
python3 tests/sstv/tools/run_radiofax_shifted_stop_lab.py 437
python3 tests/sstv/tools/run_radiofax_shifted_stop_lab.py 463
```

Each run prints its temporary artifact directory containing the RX log and
image. The signal uses a shortened synthetic phasing interval and does not
establish robustness against noisy or off-frequency image modulation.


### Experimental FAX480 core

A separate FAX480 mapper, streaming encoder and explicitly anchored receiver
now model two selectable conventions. `Dayton480` models the 5.002-second
start, 20 white phasing lines and 480 image lines described in the
[Dayton mode proposal](https://www.classicsstv.com/downloads/daytonpaper.pdf).
`Qsstv500` models the pinned QSSTV source table's 500 image lines, its
133.633-second image time and the source's image-then-sync line ordering. The
start tone polarity also follows each respective source. These are explicit
development profiles; the two geometries are not treated as interchangeable.
A cumulative integer sample clock avoids per-line rounding drift.

`test_sstv_fax480` verifies both profiles at 12 and 48 kHz, line boundaries,
header polarity and invalid input. For each profile, two grayscale patterns
are encoded using two PCM block sizes; the resulting streams are identical.
The native frequency demodulator and the anchored receiver reconstruct each
pattern with two observation chunk sizes, at least 90% pixel coverage and
mean covered-pixel error below 12/255. Missing observations remain marked
uncovered and timestamp regressions are rejected.

These are in-process, generated-signal tests. The receiver requires the exact
header anchor; acquisition, synchronization correction, application RX
integration, two-session operation and comparison against independent FAX480
audio remained open at that stage. The experimental live RX path and local
application tests are described below. Source-code-derived QSSTV timing is not
a claim of demonstrated QSSTV interoperability. The standard mode catalogue
entry remains blocked; the separate experimental actions are described below.


### Experimental FAX480 transmit path

The Transmit Studio offers a separate experimental FAX480 TX action for the
Dayton 512×480, QSSTV-style 512×500 and VIS 85 512×480 conventions. It resizes the prepared
image, converts it to grayscale, and sends it through the existing audio and
PTT coordinator without FSK ID. Focused A/B tests cover both UI selections,
source output, phase boundaries, simulated normal completion and PTT
release, cancellation, and test-mode command-line validation.

Two full Decodium test-mode sessions separately transmitted prepared images
to BlackHole 2ch with CAT disabled and audio-only VOX. A separate process
captured the actual device audio and decoded it offline using the native
frequency demodulator and known profile. For Dayton, 99.9988% of pixels had
coverage and covered-pixel MAE was 0.184/255. For QSSTV-style, coverage was
100% and MAE was 0.126/255; the app reported `Completed`. A subsequent
QSSTV-style run using the corrected analyzer and explicit completion check
passed with coverage 99.9996% and MAE 0.110/255. The input images
were two different small color fixtures, mostly uniform after preparation,
with dark side bars. These are local TX-to-offline-decoder checks, not two
Decodium sessions, independent FAX480 implementations, physical PTT or RF.

An initial analysis of the QSSTV-style capture gave MAE 19.48 because the
header-only detector locked early on a VOX pre-key tone. The analysis now
refines its anchor against the first line sync and the same recording gives
the result above. The A/B analyzer test includes a VOX-like pre-key and two
synthetic striped images and rejects truncated captures. The refinement is a laboratory heuristic, not a
general live FAX480 acquisition algorithm. Artifacts from the first real
captures: `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-tx-dayton-480-b_adm2sw`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-tx-qsstv-500-e1bw_kg0`.
Each capture is roughly 60 MB and the manual script retains it for inspection.

To repeat on macOS with BlackHole 2ch and PortAudio installed:

```sh
cmake --build build --target decodium_qml decode_fax480_capture
python3 tests/sstv/tools/run_fax480_tx_lab.py dayton-480
python3 tests/sstv/tools/run_fax480_tx_lab.py qsstv-500
```

Independent FAX480 interoperability remains open.


### Experimental FAX480 receive path and two-app check

The Receive page permits explicit Dayton, QSSTV-style or VIS 85 FAX480
configuration while RX is stopped. This is exclusive with the existing
weather Radiofax receive configuration. For Dayton and QSSTV-style, the worker searches for an alternating
start tone and first 1200 Hz line sync, then replays up to one second of
observations to retain the beginning of the QSSTV-style image. It publishes
progressive snapshots, keeps missing pixels uncovered, and publishes a partial
image on an input discontinuity. After one image, stop and restart RX to arm
another. Those profiles require the start tone and a selected convention; clock/slant
correction, difficult channels and independent application interoperability
have not been established.

`test_sstv_fax480` exercises both formats with different chunk sizes and
rejects a constant tone and interrupted sync. The focused
`fax480AutomaticRuntimeProfilesAB` test passes both profiles through the
normal resampler, preprocessor, demodulator and worker image handoff. The QML
test checks both profile selections and prevents changes during active RX.

Two separate offscreen Decodium processes were connected over BlackHole 2ch
with CAT disabled and audio-only VOX. A first Dayton run received all 480
lines, 100% coverage and no processing failures or discontinuities; compared
to the TX-prepared image, component MAE was 0.110/255, p95 0 and maximum 39.
A QSSTV-style run received all 500 lines, 100% coverage and no processing
failures or discontinuities; MAE was 0.065/255, p95 0 and maximum 17. Both
TX processes reported completion and both RX/TX pairs exited zero. The
Dayton run was repeated with the final receiver binary and again achieved
480 lines, full coverage, zero discontinuities and MAE 0.110/255. The
acceptance limits (MAE 8 and p95 10) were set in the harness before these
runs. These are local Decodium-to-Decodium tests with two different, mostly
uniform image fixtures and dark side bars; they do not establish interoperation
with QSSTV or another implementation, noisy-signal reliability, physical PTT,
or RF behavior.

Reproduce each roughly three-minute run on macOS with BlackHole 2ch, ffmpeg
and ffprobe:

```sh
python3 tests/sstv/tools/run_fax480_two_app_lab.py dayton-480
python3 tests/sstv/tools/run_fax480_two_app_lab.py qsstv-500
```

The script creates isolated test profiles and prints the log and image
artifact directory. Final A/B artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-two-app-dayton-480-3bkeud78`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-two-app-qsstv-500-y8y9jx62`.

The receiver distinguishes the end of the timed FAX480 frame from full pixel
coverage. A frame ending with missing pixels is marked partial, not complete;
the laboratory PNG export also saves that terminal partial image. The focused
`fax480MissingSignalEndsPartialAB` runtime test injects silence in two profiles
with continuous timestamps. On macOS, two offscreen Decodium processes were
also connected through BlackHole with a relay that passed 70 seconds of signal
and then substituted silence. Both image fixtures ended partial at 53.27%
coverage, saved a PNG, and reported no processing failures, dropped chunks or
discontinuities. Clean VIS 85 A/B runs received 100% coverage, with component
MAE 0.237 and 0.138 respectively (p95 zero). These are virtual-audio tests;
they do not establish RF performance or cross-platform runtime behaviour.

The partial A/B checks require BlackHole 16ch and 2ch plus PortAudio:

```sh
python3 tests/sstv/tools/run_fax480_partial_two_app_lab.py libsstv-vis-480 a
python3 tests/sstv/tools/run_fax480_partial_two_app_lab.py libsstv-vis-480 b
```

The successful partial application artifacts are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-two-app-libsstv-vis-480-g8ljv840`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-two-app-libsstv-vis-480-rp9lfkow`.


### FAX480 with VIS 85 and libsstv reference

The pinned [rimio/libsstv encoder](https://github.com/rimio/libsstv/tree/193157a993ac34bfa074074004c9ddadcfe6fd15)
sends FAX480 with a normal SSTV
leader and VIS 85 followed immediately by 480 sync-and-scan lines. It does
not send the five-second FAX start and 20 phasing lines used by the Dayton
profile. Decodium now offers an explicit `VIS 85 512×480` TX/RX profile; it
does not silently treat the three conventions as equivalent. RX validates
the VIS payload and parity and rejects a different valid VIS. A short
transient between tone runs is filtered before the existing VIS decoder.

The separate libsstv reference encoder generated two different 512×480
striped grayscale images at 12 kHz. The native receiver reconstructed both
with 99.81% and 99.77% covered pixels and mean covered-pixel errors of
0.622/255 and 0.634/255. Reproduce this component-level differential test
with a separate checkout at the pinned revision:

```sh
LIBSSTV_REFERENCE_ROOT=/tmp/decodium-libsstv-reference \
  tests/sstv/tools/run_libsstv_fax_oracle.sh
```

Two isolated Decodium processes also exchanged two different images over
BlackHole 2ch using VIS 85, CAT disabled and audio-only VOX. Both runs
received 480 lines with 100% coverage and no processing failures or
discontinuities. The component MAEs were 0.237/255 and 0.138/255; p95 was
0 in both cases and maximum errors were 44 and 25. The prepared images were
mostly uniform with dark side bars. Both TX processes reported completion
and both process pairs exited zero. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-two-app-libsstv-vis-480-itlncqnx`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-two-app-libsstv-vis-480-lmruu70h`.

A separate process then replayed each independently generated libsstv signal
through BlackHole to a full Decodium RX process. Both striped images produced
480 complete lines, 100% coverage, zero processing failures and zero
discontinuities. MAEs were 0.897/255 and 0.675/255, with p95 0 and maximum
individual component errors 169 and 120 respectively. The acceptance limits
were MAE 8 and p95 10, set before the tests. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-libsstv-app-0-bt13d7wc`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-libsstv-app-1-xa6bgav1`.

```sh
python3 tests/sstv/tools/run_fax480_two_app_lab.py libsstv-vis-480 a
python3 tests/sstv/tools/run_fax480_two_app_lab.py libsstv-vis-480 b
python3 tests/sstv/tools/run_libsstv_fax_app_lab.py 0
python3 tests/sstv/tools/run_libsstv_fax_app_lab.py 1
```

The independent libsstv evidence covers encoding into Decodium receive;
libsstv is encoder-only for this purpose.

### Decodium FAX480 TX into QSSTV-Experimental 10.0

The independent [QSSTV-Experimental 10.0 macOS ARM64 release](https://github.com/jbdubbs/QSSTV-Experimental/releases/tag/10.0)
was checked against its published archive SHA-256
`600e9ba493030a0c6aabc4f2daa0b3385bfb1dd47e6b288d94a1ee8f8c906384`.
Its application executable SHA-256 is
`b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa`;
the corresponding source tag resolves to `a84584248d3307bfa081362031ff20f365337633`.

Two different images were transmitted by a full Decodium application using
the `qsstv-500` profile and captured through BlackHole 2ch. The captured
audio was converted to 12 kHz mono WAV and decoded by the independent QSSTV
application in batch mode with FAX480 selected explicitly. For the 512×500
results, mean absolute errors were 1.103 and 0.752 on a 0–255 scale;
the 95th-percentile error was zero for both, and the maximum individual
errors were 132 and 130. The acceptance limits were MAE 8 and p95 10.
This is evidence for this particular QSSTV-style 500-line convention; it
does not establish VIS 85 interoperability with QSSTV.

The source captures and reference images are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-tx-qsstv-500-emofbn2j`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-tx-qsstv-500-oi1knl9s`;
the independent decoded images and logs are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-exp-decode-q6qq73cw`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-exp-decode-14pg6kxo`.
Run `tests/sstv/tools/run_qsstv_exp_decode_lab.py CAPTURE.f32 REFERENCE.png`
to repeat the independent decode. The release lacks Qt's offscreen plugin,
so the helper uses the Cocoa backend; the batch application displayed Qt
duplicate-framework warnings. These local audio tests do not establish other
FAX variants, weak/noisy signals, PTT hardware or RF behavior.

The first reverse application test failed. On this machine the same
QSSTV release's `--encode` wrote a 140.945-second WAV but did not exit within
240 seconds. QSSTV's own batch decoder produced a 512×500 image from that
file (relative to the reference, MAE 5.037 and p95 24, outside the pre-set
p95 limit). A diagnostic replay of a copy through BlackHole to Decodium
produced no completed image, despite zero reported audio discontinuities or
processing failures. The QSSTV WAV and logs are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-exp-app-0-pq02ss_r`;
the Decodium replay logs are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-exp-app-0-p4ybfmjs`.
This is a failed single-image diagnostic, not an A/B pass or proof that
the file represents a successfully completed QSSTV transmission. The
acquisition failure and encoder timeout require further investigation.

The release WAV's first 10 seconds contain only nonnegative samples, with
50.17% exactly zero. The QSSTV synthesizer casts a rounded negative floating
sample directly to `quint32`; on the tested ARM64 build this produced
half-wave clipping. A separately compiled copy of source tag 10.0, with only
the signed-PCM conversion changed as in
`tests/sstv/fixtures/qsstv-experimental-10.0-signed-pcm.patch`, produced a
waveform containing both polarities. It was built against the installed Qt
and run with `QT_QPA_PLATFORM=offscreen`; in this configuration its encoder
exited successfully. The locally built binary's SHA-256 was
`8254e164cd2982728d6cf3372654d10a3a85e725305a867a349db2d74d930368`.
The switch from the release's Cocoa setup to this local offscreen build
also changes the execution environment, so it does not isolate the cause
of the release encoder's failure to exit.

Using that modified independent QSSTV application, two different striped
512×500 images were encoded to WAV and replayed through PortAudio and
BlackHole 2ch into a full isolated Decodium application, with the
`qsstv-500` RX profile selected. Both completed with 100% coverage, zero
reported processing failures and zero discontinuities. The decoded images
had MAE 0.833 and 0.859, p95 0 and maximum component error 169 in both
cases; acceptance limits MAE 8 and p95 10 were chosen beforehand. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-exp-app-0-s97yy97q`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-exp-app-1-xccfdqpq`.
This establishes the described file-mediated local audio path for the
patched build, not reception from the unmodified release, over-the-air
interoperability, or all QSSTV FAX conventions.

To reproduce, check out the pinned QSSTV source revision, apply the fixture
patch with `git apply`, and build it with its documented CMake/Qt setup.
Set `QSSTV_EXPERIMENTAL_BINARY` to that build's executable,
`QSSTV_EXPERIMENTAL_SHA256` to its independently checked SHA-256 and
`QSSTV_QPA_PLATFORM=offscreen`, then run
`python3 tests/sstv/tools/run_qsstv_exp_fax_app_lab.py 0` and again with `1`.

### FAX480 detailed raster A/B between two applications

Two additional Decodium TX/RX process pairs used the `qsstv-500` profile over
BlackHole 2ch, with CAT disabled and audio-only VOX. Unlike the nearly uniform
fixtures above, the generated grayscale inputs contain gradients and blocks
distributed across the full raster. The 512×500 receiver output was compared
directly with the TX-prepared image using pre-existing limits MAE 8 and p95 10,
without post-hoc alignment. A and B respectively measured MAE 0.315 and 0.403,
p95 2 and 2, maximum component errors 37 and 38; both had complete coverage,
zero reported processing failures and zero discontinuities. Artifacts are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-two-app-qsstv-500-ftc3fzcf`
and `.../decodium-fax480-two-app-qsstv-500-dgpav9xh`.

```sh
python3 tests/sstv/tools/run_fax480_two_app_lab.py qsstv-500 a pattern
python3 tests/sstv/tools/run_fax480_two_app_lab.py qsstv-500 b pattern
```

These tests cover local clean audio between two Decodium instances, not the
independent FLdigi alignment problem or RF.

### Detailed raster Decodium TX into QSSTV: failed A/B

The same two patterned inputs were then sent by separate Decodium TX runs on
`qsstv-500`, recorded through BlackHole and decoded from the capture by the
unmodified QSSTV-Experimental 10.0 batch receiver. The independent 512×500
images failed the existing pre-set p95 limit of 10: A had MAE 5.202, p95 24,
maximum 240; B had MAE 4.210, p95 25, maximum 225. Both TXs completed and
Decodium's offline decoder reported 99.9996% pixel coverage. This failure is
not overridden by the previous passing tests on almost uniform images.

An exploratory grayscale comparison excluding four border columns searched
horizontal offsets -3 through +3: offset -2 minimized the mean error in both
captures (1.935 and 1.653 respectively). This is a post-hoc diagnostic, not
an acceptance criterion or an established cause; the receive synchronization
and pixel timing require further investigation. Capture/reference artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-fax480-tx-qsstv-500-90w5ggin`
and `.../decodium-fax480-tx-qsstv-500-1ej1hau7`. Independent images/logs:
`.../decodium-qsstv-exp-decode-5yvk9pge` and
`.../decodium-qsstv-exp-decode-6xqabwv2` under the same temporary directory.
`run_fax480_tx_lab.py` accepts an optional image path as its third argument;
`run_qsstv_exp_decode_lab.py CAPTURE.f32 REFERENCE.png` repeats the original
unaligned comparison. QSSTV also logged duplicate Qt framework warnings.

The offset analysis is reproducible with
`tests/sstv/tools/analyze_fax_alignment.py REFERENCE.png RECEIVED.png`. It
reports errors for each bounded shift with the same border excluded, and is
diagnostic only; it does not change the unshifted acceptance test. On these
images the best post-hoc shift was -2 pixels for both (MAE 1.935/1.653,
p95 5/3). A separate exploratory manipulation delayed each captured scan
line by 49 samples at 48 kHz while leaving its sync region in place. QSSTV's
unaligned comparisons of those modified recordings measured A MAE 3.634,
p95 15 (fail), and B MAE 2.517, p95 9 (pass). This manipulation introduces
artificial sample discontinuities and is not a transmitter fix or a new
application A/B pass. Both observations support investigating relative scan
timing but do not identify the exact source of the mismatch. The modified
recordings are `/tmp/decodium-fax-shift-{a,b}.f32`; the QSSTV output directories
are `.../decodium-qsstv-exp-decode-7lc7nx0p` and
`.../decodium-qsstv-exp-decode-j1b7n64l` under the same temporary directory.

#### Independent QSSTV self-loop control for the detailed rasters

The locally built QSSTV-Experimental transmitter with the signed-PCM patch
encoded the same TX-prepared 512×500 reference images. The unmodified QSSTV
10.0 release receiver decoded those WAVs directly. Against the references,
its own TX-to-RX outputs measured A MAE 5.303/p95 24 and B MAE 4.276/p95 26,
both outside the earlier p95 10 limit. Comparing the QSSTV-received image from
Decodium TX with the QSSTV-received image from its own TX, without shifts,
gave MAE 0.307/p95 0 for A and MAE 0.318/p95 1 for B (maximum errors 128 and
130). Thus the independent receiver shows a similar raster discrepancy on
both transmitters in these cases. This is evidence against attributing that
discrepancy solely to the Decodium transmitter; it does not prove its exact
cause, RF behavior, or all-image interoperability. The QSSTV TX WAVs are
`/tmp/decodium-qsstv-pattern-{a,b}.wav` and the receiver images are in
`/tmp/decodium-qsstv-self-pattern-{a,b}`. The release receiver emitted the
same duplicate Qt framework warnings described above.

As a further diagnostic, sample delays of 25, 35, 60 and 70 at 48 kHz were
applied to the scan regions of each original capture, preserving their sync
regions but truncating the ends of the scan regions. The pre-existing p95
values were respectively 14/16/10/5 for A and 12/13/11/3 for B. A separate
nearest-sample time compression intended to preserve the entire scan yielded
p95 19 and 18. These are post-hoc manipulated WAVs with possible phase and
frequency artifacts, not valid application-level transmission tests or a
basis for silently cropping pixels. Raw files are
`/tmp/decodium-fax-probe-{a,b}-{25,35,60,70}.f32` and
`/tmp/decodium-fax-compress-{a,b}.f32`.

#### QSSTV receive timing diagnostic

To investigate the shared discrepancy, a separate build of the pinned
QSSTV-Experimental 10.0 source was changed only in `modeBW`'s FAX480 RX pixel
positions: each sample target was moved 12 samples earlier at 12 kHz and
clamped at zero. Its TX retained the earlier signed-PCM patch. A same-source,
same-Qt baseline retained the original RX timing. Both builds decoded the
same two Decodium TX captures and the same two patched-QSSTV TX recordings;
results below use the original, unshifted comparison against the prepared
images, with MAE 8 and p95 10 as before.

| Waveform | Original RX MAE / p95 | Earlier RX MAE / p95 | Later RX MAE / p95 |
|---|---|---|---|
| Decodium A | 5.202 / 24 | 1.956 / 5 | 8.518 / 40 |
| Decodium B | 4.210 / 25 | 1.651 / 3 | 7.785 / 62 |
| QSSTV A | 5.303 / 24 | 2.045 / 5 | 8.618 / 40 |
| QSSTV B | 4.276 / 26 | 1.689 / 3 | 7.857 / 63 |

The last column is a negative control made by moving the RX targets 12
samples later instead. The improvement with earlier targets on both sources,
and deterioration in the opposite direction, identify a receive-timing
sensitivity in this tested QSSTV path. QSSTV's source itself notes that its
sync and video demodulator filter chains have different group delays for
another mode; these measurements alone do not identify the exact cause for
FAX480. The earliest targets cannot retrieve samples already consumed before
the line starts, and maximum component errors remain as high as 254.

The early diagnostic source change is saved as
`tests/sstv/fixtures/qsstv-experimental-10.0-fax-rx-timing-diagnostic.patch`;
it is not a Decodium or upstream QSSTV change. The baseline, early and late
binary SHA-256 values were respectively
`8254e164cd2982728d6cf3372654d10a3a85e725305a867a349db2d74d930368`,
`37e21392b60fd7ffe5943a30681340a58cf60cb309435ba0f97021469eb0812b`, and
`325fae1ee75dc041c59555a36f0837b6be444d63adec4cb452cc90e616e6404b`.
Artifacts are `decodium-fax-qsstv-timing-c7tv3wzc` and
`decodium-fax-qsstv-late-control-mvlmx9oi` in the macOS temporary directory.
The modified receiver results do not replace the failing unmodified-release
A/B results above, nor prove interoperability on RF or other platforms.

A further waveform-only diagnostic replaced the final 48 samples of each
predicted 1200 Hz line-sync pulse in both Decodium captures with a continuous
phase 1500 Hz tone, using the lab analyzer's estimated header anchors. A
three-sample sinusoid residual classified all 500 original replaced windows
in each capture as closest to 1200 Hz among 1200/1500/1900/2300 Hz. The
replacement is artificial and the anchors are estimates; it is not a
standards-conformant transmitter implementation. The unmodified release RX
still failed both comparisons: A MAE 5.202/p95 24/max 240, B MAE 3.938/p95
23/max 212. Decodium's offline auto-acquiring receiver produced coverage
0.999797/0.999098 and covered-pixel MAE 1.787/0.913, respectively. Thus this
experiment does not establish a useful TX-side fix. The modified captures are
`/tmp/decodium-fax-short-sync-{a,b}.f32`; the QSSTV output directories are
`decodium-qsstv-exp-decode-i5m932uw` and
`decodium-qsstv-exp-decode-6_oblm53` in the macOS temporary directory. No
modified sync pulse has been enabled in the application.
`tests/sstv/tools/prepare_fax480_sync_probe.py CAPTURE OUTPUT HEADER_SAMPLE`
reproduces this diagnostic using estimated header samples 194210 and 205982
for A and B. It validates all predicted 1200 Hz windows and refuses to
overwrite an existing output. Recreated A/B files matched the originals
byte-for-byte; the script itself is a lab tool, not a production filter.

#### Off-centre frequency-modulated stop boundary

The FM stop detector accepts modulation frequencies from 440 to 460 Hz, but
its short-window boundary refinement previously correlated only at 450 Hz. In
the WMO dead-sector clock A/B test, a synthetic 440 Hz stop with a faster
clock was detected but the refined boundary landed 17,892 samples (approximately
three lines) late. The refinement now tests 440, 450 and 460 Hz correlations,
covering the detector's band without changing its initial detection threshold.
The expanded synthetic test passes at 440, 445, 447.3, 450, 452.7, 455 and
460 Hz for both faster and slower image clocks; it checks the boundary within
10 samples, all 120 transmitted lines, and their pixel values. The full
`test_sstv_radiofax` suite also passed.

With the modified Decodium executable, two separate local TX/RX pairs sent
WMO dead-sector images through BlackHole 2ch. The receiver used a 150-line
canvas for a 120-line transmission. Both runs detected the nominal 450 Hz FM
stop, published 120 lines and 80% coverage, with zero reported discontinuities,
dropped chunks or processing failures. Comparing the first 120 rows with the
TX-prepared reference gave A/B component MAE 0.309/0.248 and p95 0/0.
Artifacts are `.../decodium-radiofax-centered-a-3oltcw4s` and
`.../decodium-radiofax-centered-b-ncab8ut3`. The test can be repeated with
`DECODIUM_LAB_RADIOFAX_WMO_DEAD_SECTOR=1` and
`DECODIUM_LAB_RADIOFAX_PARTIAL_STOP=1` using
`tests/sstv/tools/run_radiofax_centered_two_app_lab.py a` and `b`.
The two-Decodium tests exercise the nominal stop. Separately, a synthetic
continuous-phase audio generator sent 16 raster lines followed by alternating
1500/2300 Hz stop modulation at 440 and 460 Hz over BlackHole 2ch into a full
Decodium RX process. Both ended with exactly 16 complete lines, 50% coverage
of a 32-line canvas, a detected stop, and no processing failures, dropped
chunks or discontinuities in any recorded diagnostics. Mean pixel error was
0.153/255 at 440 Hz and 0.119/255 at 460 Hz. The stricter repeat artifacts are
`.../decodium-fm-shift-stop-440-0d3tau_l` and
`.../decodium-fm-shift-stop-460-aq619s1n`. Reproduce with
`python3 tests/sstv/tools/run_radiofax_fm_shifted_stop_lab.py 440` and `460`.
The off-centre cases use a separate synthetic audio generator rather than a
second Decodium transmitter; these tests do not establish RF performance.

#### Continuous Radiofax reception

After a complete image or a detected stop, the runtime publishes the terminal
image and rearms the Radiofax acquisition without stopping monitoring. The last
image remains available until the next image starts publishing. The receiver
passes unconsumed observations from the same audio block to the new detector;
the audio stop detector retains its sample timeline. The focused runtime A/B
test sends two differently coloured images separated by stop and phasing,
using two input block sizes, and checks acquisition identity, the second image,
and audio error counters. The Radiofax and receive-QML test suites also pass.

On macOS ARM64, `tests/sstv/tools/run_radiofax_consecutive_two_app_lab.py`
ran two orderings, A then B and B then A, through BlackHole 2ch. In each run
one uninterrupted Decodium receiver acquired both transmissions and recorded
two distinct acquisition IDs. Each stop left exactly 120 complete rows on a
150-row canvas, and recorded diagnostics reported no processing failures,
discontinuities or dropped chunks. The received 120-row rasters matched their
TX-prepared references with mean absolute component errors 0.172 and 0.106
out of 255 respectively; p95 was zero for both. Artifacts are
`.../decodium-radiofax-consecutive-a-twyi7g2y` and
`.../decodium-radiofax-consecutive-b-nl45v1sh`. The script checks this local
virtual-audio path; it is not RF or independent-application interoperability
evidence. It uses a test-mode-only image sequence capture option to preserve
both received terminal images.
The complete `test_sstv_rx_runtime` CTest suite also passed after the change
on this macOS ARM64 build.
The same A/B test also passed with a 120-row receive canvas, so each image
finished by reaching the configured height instead of relying on a stop to
end a 150-row partial image. Both orders had two complete, distinct
acquisitions and the same image error metrics and zero audio error counters.
The corresponding artifacts are
`.../decodium-radiofax-consecutive-a-omx7o5cg` and
`.../decodium-radiofax-consecutive-b-9lw5fxno`; append `complete` to the lab
command to repeat this case. The runtime A/B regression covers both terminal
conditions with both block sizes.

Two further local process-pair experiments kept one receiver monitoring while
the transmitters sent 120 then 240 LPM, and 240 then 120 LPM, respectively.
Both orders produced two distinct acquisition IDs, detected each stop after
120 rows and reported no processing failures, discontinuities or dropped
chunks. Direct A/B image MAE was 0.086/0.106 and p95 was zero for both,
independent of the ordering. The helper uses
`DECODIUM_LAB_RADIOFAX_MIXED_RATE=1` to select these sequences. After the
actual TX-completed log record, it terminates a laboratory sender that has
not exited, because a first attempt completed transmission but hung during
legacy/QML shutdown and timed out before the second image. This is a local
virtual-audio result, not evidence that application teardown has been fixed,
or an RF or independent-receiver test. Artifacts are
`.../decodium-radiofax-consecutive-a-tlwz7nik` and
`.../decodium-radiofax-consecutive-b-x63ywkia` under the macOS temporary
directory; the initial failed harness run was
`.../decodium-radiofax-consecutive-a-tjdhamfm`.

The slow shutdown was separately reproduced twice with no SSTV transmission
and the legacy backend selected. In both runs the laboratory quit was
requested, `app.exec()` exited and the SSTV receiver reported shutdown, but
the process remained alive while macOS `sample` showed the main thread in
Qt's `QThreadPool::waitForDone` and a pooled thread in
`QHostInfoRunnable`/`getaddrinfo`. Both processes subsequently exited with
status zero. The logs also report an automatic US-state-data download and
network lookup failures, but the thread samples do not identify the queried
host or establish which request caused the wait. Thus the observation is a
delayed shutdown under these network conditions, not proof of an SSTV TX or
legacy-backend defect. Samples are `/tmp/decodium-shutdown-lab.sample` and
`/tmp/decodium-shutdown-lab-b.sample`; no production shutdown code was changed.

#### Simultaneous independent FLdigi and Decodium receive control

The two structured 128×120 A/B images from the WMO live experiment above were
also sent by an isolated FLdigi 4.2.11 WEFAX576 transmitter. Each resulting
BlackHole capture was then played once into both a Decodium receiver and an
independent FLdigi receiver at the same time. The transmitter reported
success for both images. Decodium received 120 complete lines from each,
recognised stop, and reported no processing failures, dropped chunks or
input discontinuities. Unaligned A/B component MAE was 0.460/0.576, p95 1/1,
and maximum error 9/13, against the original images. The receive canvas was
128×150, so its coverage was 80%.

FLdigi's outputs both reported 120 LPM, automatic centering and an APT stop.
Their full 1809-pixel canvases, resized to 128 pixels wide and compared
without alignment, gave grayscale MAE/p95 28.799/114 and 26.122/119. A
post-hoc search excluding 24 columns on each side selected row +5, column 0
for both, with MAE/p95 4.268/15 and 4.739/22. The search is diagnostic, not
an unaligned pass. This is stronger evidence that FLdigi's own TX/RX path can
also produce an apparent vertical offset for these detailed images, while
Decodium received the same captures without it. It does not explain FLdigi's
internal state or prove Decodium-to-FLdigi interoperability: the previous
Decodium signals used the WMO dead sector, whereas these FLdigi transmissions
used the ordinary full-width image, and independent RF behavior remains
untested.

TX captures A/B are `.../decodium-fldigi-tx-capture-5mxmfp20` and
`.../decodium-fldigi-tx-capture-o1d60usx`. Simultaneous RX artifacts are
`.../decodium-fldigi-dual-rx-8l0_x4y1` and
`.../decodium-fldigi-dual-rx-z1h6eo1m`; their `decodium.log` files identify
the Decodium image and full diagnostics. Repeat with
`tests/sstv/tools/run_fldigi_dual_rx_lab.py CAPTURE.f32 REFERENCE.png` on a
Mac with BlackHole and an isolated FLdigi installation. The helper requires
one saved FLdigi PNG with 120 LPM and APT-stop metadata and checks the
Decodium replay separately. These cases were run before that final metadata
assertion was added; the assertion was subsequently checked against both
saved PNGs.

#### Simultaneous receive of Decodium TX and FLdigi raster gate

Two earlier Decodium IOC 576/120 LPM centered-phasing TX captures were each
replayed once through BlackHole into simultaneous Decodium and FLdigi 4.2.11
receivers. The inputs were different 128×40 images expanded by Decodium to
128×120; this is the full-width centered-phasing convention, not the WMO
dead-sector profile. Decodium detected stop and completed 120 rows in each
case, with 0.8 coverage of the 150-row canvas and no reported processing
failures, dropped chunks or discontinuities. Its A/B unaligned component
MAE/p95/max versus the prepared references was 0.0956/0/8 and 0.0935/0/8.

FLdigi saved an APT-stop image at 120 LPM in both cases, but its direct
unaligned grayscale comparison after resizing its 1,809-pixel width to 128
failed the MAE 8/p95 4 limits: A 48.314/248, B 48.837/248. A diagnostic search
excluding 32 columns on either side selected row +5/column -25 for both,
with MAE/p95 7.134/7 and 7.151/7. These post-hoc offsets are not acceptance
passes. Earlier FLdigi self-loop controls also showed offsets, but these
results do not establish the root cause or justify claiming Decodium to
FLdigi visual interoperability. Artifacts are
`.../decodium-fldigi-dual-rx-r8z_851n` and
`.../decodium-fldigi-dual-rx-r2dxepat`; the Decodium replays are
`.../decodium-fldigi-radiofax-replay-auoxmp7p` and
`.../decodium-fldigi-radiofax-replay-f3v_x50p`.

The first laboratory run originally reported a metadata failure despite
FLdigi's valid `APT Stop` comment: the helper checked the wrong letter case.
A bounded PNG text-chunk check now accepts the two observed stop spellings,
requires the expected LPM, and rejects missing/truncated metadata. A/B and
negative metadata checks pass in `test_sstv_fldigi_radiofax_metadata`; the
first run's saved PNG was checked again after the fix. The helper now also
applies the direct FLdigi raster thresholds before reporting a pass, rather
than treating image creation and stop metadata alone as a successful visual
interoperability test. The B run was executed before that quality gate was
added; both saved A/B outputs were subsequently evaluated using the same gate
and fail it as reported above. No production waveform was changed.

#### Three phasing lines and white end-line removed together

FLdigi 4.2.11 source contains a four-line phasing threshold and can re-enter
phasing detection while already receiving. To test a narrower hypothesis,
two existing 120 LPM centered-phasing Decodium recordings were copied with
three whole phasing lines removed at sample 768,000 and the entire 24,000-sample
white end line removed separately. The white-line boundaries were estimated
from the captured phasing rising edges, at samples 1,937,700 (A) and 1,943,844
(B). No original samples outside those two intervals were altered; the cuts
are post-capture interventions, not an actual TX profile or a standards
claim. The helper `tests/sstv/tools/prepare_radiofax_phasing_variant.py`
recreated both copies byte-for-byte; its A/B tests also reject overlapping
cuts and attempts to overwrite an existing file.

Each modified copy was replayed into an isolated FLdigi 4.2.11 receiver over
BlackHole. Both outputs reported 120 LPM, 124 rows and APT stop at 450 Hz.
Compared unaligned with the prepared 128×120 references, A had grayscale
MAE/p95/max 38.634/248/248 and B 18.247/97/255, so neither meets MAE 8/p95 4.
A post-hoc search excluding 32 border columns selected row +1, column -24
for A (MAE/p95 7.268/7) and row +1, column -1 for B (6.750/7). Those shifts
were chosen after observing the images and are not passing comparisons. This
experiment reduces the observed vertical offset relative to the unmodified
replays but does not resolve horizontal alignment or justify a production
waveform change. The underlying cause and broad FLdigi interoperability remain
open.

For the next investigation, the public FLdigi source mirror at revision
`61b97f4133c488063f3de1795c894d22d5032e8a` calls its phasing decoder in
both the phasing and image states. After four detected lines, it resets the
image sample position; if already receiving, it can save the current image
and start another one. This provides a concrete mechanism by which a long
phasing preamble can affect the saved raster, but it is only a source-level
observation: the mirror revision has not been matched to the installed 4.2.11
binary and the existing traces do not prove that this branch caused the
observed offsets. Future A/B diagnosis should record those transitions and
the saved-image boundaries before changing the transmitted preamble.

The replay trace now also records the first observation of each PNG in the
isolated FLdigi image directory, with the same monotonic clock as the receiver
state. Two 20-second replays of the original centered-phasing A/B recordings
reached receiving approximately 13.05 and 13.12 seconds after playback
started, respectively. Neither created a PNG during these short runs. This
does not exclude an internal rephase that does not save a file, or one later
in the transmission; the runs ended before the whole image and APT stop.
The empty standard FLdigi logs from earlier runs contain no branch-level
evidence. Artifacts are `.../decodium-fldigi-phase-9kk7bp97` and
`.../decodium-fldigi-phase-d37mc6i5` under the macOS temporary directory.

Full-duration A/B replays of the same original captures with this extended
trace each produced exactly one PNG, first observed approximately 102.96 and
103.03 seconds after playback began. Both PNGs contain the expected 120 LPM
and APT-stop metadata. No intermediate PNG was observed by the 100 ms polling
loop in either isolated image directory. This establishes only that no
intermediate PNG was observed. As explained below, FLdigi can discard an
intermediate image during a rephase, so the absence of a PNG does not rule
out an internal reset or another cause of raster displacement.
The trace detects file creation, not atomic completion of PNG writing; the
metadata checks were performed after each process exited. Artifacts are
`.../decodium-fldigi-phase-nz258l8m` and
`.../decodium-fldigi-phase-x0vcxet_` under the macOS temporary directory.
These runs test the capture replay and receiver state, not RF or visual
interoperability acceptance.

The published FLdigi 4.2.11 source archive (SHA-256
`762b370ffe9cae773a2a03b512d0b7242e7e90407c11d5abbd2d31ae6501664f`,
[MacPorts source mirror](https://vie.at.distfiles.macports.org/fldigi/fldigi-4.2.11.tar.gz))
contains the phasing decoder in both the phasing and image paths, and resets
the image sample position after four recognised phasing lines. Its automatic
save function can discard small, blank, low-correlation or otherwise
insignificant intermediate images before resetting reception. Therefore a
rephase may leave no PNG, and the A/B file-polling observations above cannot
establish that no reset occurred. The installed app reports version 4.2.11;
the archive was not used to build or instrument that binary, so matching the
version does not prove identical executable source or runtime branch events.
Instrumenting that path or obtaining equivalent reliable logs is needed to
test the hypothesis directly.

The isolated replay helper now enables FLdigi's debug categories in a private
`fldigi.prefs`, including the version and dual-channel markers required for
FLdigi to load those preferences. In 20-second A/B replays of the original
centered-phasing captures with automatic centering off, the installed 4.2.11
binary wrote logs to `config/debug/status_log.txt`. Each recorded four center
settings, including three `Detected phasing when in image state` events;
each of those three events was accompanied by `Do not save small image`.
The logs directly establish repeated rephasing and suppressed intermediate
images in these two replays, not their exact contribution to the final visual
offset. The short runs did not reach the whole image or APT stop, and verbose
logging changes the test conditions. Artifacts are
`.../decodium-fldigi-phase-hqz7cxlg` and
`.../decodium-fldigi-phase-hlbr3txi` under the macOS temporary directory.

Full-duration A/B replays with the same verbose isolated configuration and
automatic centering off each recorded 14 center settings, including 13
rephasings while receiving, and 13 small intermediate images discarded.
Each produced one final PNG with 120 LPM, `Auto-Center:Off`, and APT-stop
metadata. Against the earlier Decodium-received proxy references, direct
MAE/p95 was 52.450/248 and 53.701/248; post-hoc best offset was row +5,
column 0 for both, with MAE 7.177/7.185 and p95 7. These repeat the earlier
visual failure while confirming the rephasing events in the same complete
runs. The log alone does not establish the exact contribution of the final
rephase to the five-row offset or prove a correction. Artifacts are
`.../decodium-fldigi-phase-hqr97pyq` and
`.../decodium-fldigi-phase-bcn_87nm` under the macOS temporary directory.

The replay helper now also stores paired wall and monotonic clocks immediately
before writing audio, allowing FLdigi's wall-clock log entries to be compared
with positions in the capture. In two further full replays, the last logged
rephase occurred about 39.000/39.083 seconds after the playback clock sample.
The estimated first image samples in the corresponding recorded files are
at 40.869/40.997 seconds, about 1.868/1.914 seconds later. At 120 LPM this
is approximately 3.74/3.83 lines; FLdigi's source initializes its image
counter to about 1.025 lines at rephase, giving roughly 4.76/4.85 lines before
the image. This is consistent with the observed five-row diagnostic offset,
but audio buffering, wall-clock log precision and estimated audio boundaries
prevent treating it as an exact sample-level measurement or a proven fix.
The original phasing and one-line white terminator were unchanged. The 4.2.11
source also transmits a white end line after its 20 phasing lines; that fact
alone does not establish WMO compliance of either implementation. Artifacts
are `.../decodium-fldigi-phase-i3vn1l2w` and
`.../decodium-fldigi-phase-xjaopgfc` under the macOS temporary directory.

`tests/sstv/tools/analyze_fldigi_phasing_anchor.py` makes this timing-based
comparison reproducible. It predicts the image row from the independently
estimated image-start sample, the last preceding FLdigi rephase log and the
1.025-line counter initialization, then compares the entire raster width;
it does not search for a pixel offset. For A/B, image-start samples 1,961,700
and 1,967,844 predict row 5 in both cases. Against the previously decoded
Decodium proxy images, A/B MAE was 7.519/7.528 and p95 7/7. Crossed-reference
controls produced MAE 27.505/27.508 and p95 174/174. Even this independent
timing anchor fails the existing p95 <= 4 quality gate. The measurement uses
estimated audio boundaries and uncalibrated playback latency; it is not a
general receiver alignment algorithm, independent protocol certification or
justification to silently crop received images.

`test_sstv_fldigi_phasing_anchor` separately exercises the diagnostic with
two synthetic rasters and known timing-derived start rows. It checks exact
full-width reconstruction, verifies that an event after image start cannot
replace the preceding anchor, and rejects a log with no preceding event. The
diagnostic, 2D alignment and FAX480 core tests passed on macOS ARM64. These
synthetic checks validate the analysis tool, not FLdigi interoperability.

The residual grayscale error has a strong level-dependent pattern: in the A
timing-anchored comparison, 11,363 reference-black pixels decoded as 7,
and 1,062 reference-white pixels decoded as 248; B shows the same general
pattern. This is an observation after resizing FLdigi's PNG, not a direct
frequency measurement. The isolated receiver configuration was then changed
to request `WEFAXSHIFT=800` explicitly, rather than relying on the 4.2.11
source's stated default. Full A/B replays again produced row 5, MAE
7.519/7.528 and p95 7/7 against the same proxy images. Explicitly setting
800 therefore did not improve these results. The saved PNG metadata does not
record the active shift, so this comparison alone does not prove that the
setting was read or determine the source of the level compression. Artifacts
are `.../decodium-fldigi-phase-8vgwvil3` and
`.../decodium-fldigi-phase-iu0160a9` under the macOS temporary directory.
The replay helper now writes a bounded `DECODIUM_FLDIGI_SHIFT` setting,
defaulting to 800 Hz; the controlled variation below checks its effect.

As a controlled negative comparison, the same A/B captures were replayed
with `WEFAXSHIFT=700`, keeping automatic centering off. Both saved 120 LPM
APT-stop PNGs. The same timing anchor predicted row 5, but MAE/p95 changed
to 2.217/8 and 2.228/8 respectively, compared with 7.519/7 and 7.528/7 at
the requested 800 Hz. The visible difference establishes that the setting
affects these received outputs; it does not show that 700 Hz is the correct
deviation or explain why the nominal 800 Hz images have compressed levels.
Neither case satisfies the existing p95 <= 4 quality gate, and a lower MAE
alone must not be interpreted as a passing test. Artifacts are
`.../decodium-fldigi-phase-sa4sm5ls` and
`.../decodium-fldigi-phase-2ra_le2j` under the macOS temporary directory.

The FLdigi 4.2.11 source provides a specific explanation for these levels.
In `fax_implementation::init_rx`, both `m_sample_rate` and
`progdefaults.WEFAX_Shift` are integers, and `deviation_ratio` is assigned
`(m_sample_rate / progdefaults.WEFAX_Shift) / TWOPI`. The first division is
therefore integer division. At 11,025 samples/s and an 800 Hz shift it yields
13 instead of 13.78125. Applying the source's discriminator formula to
ideal 1500/2300 Hz tones predicts 7/248 rather than 0/255; using floating-point
division predicts 0/255. The actual source captures measured 1500.0 Hz black
and approximately 2300.03 Hz white in selected stable intervals. In the
timing-anchored A/B comparisons at requested 800 Hz, the modal outputs for
reference black were 7 in both and for reference white 248 in both. At
requested 700 Hz the modal extremes instead clipped to 0/255; in A, input
gray 64 mapped predominantly to 58, demonstrating that restoring extremes
by reducing the receiver shift distorts intermediate levels. The observed
values are consistent with the source-level integer-division defect; the
installed binary has not been rebuilt from this archive or instrumented at
the arithmetic expression, so an exact source-to-binary match is not proven.
Changing Decodium's standard 800 Hz output to compensate would affect other
receivers and is not justified by this experiment.

For a direct control, a separate macOS ARM64 FLdigi 4.2.11 source build was
created using Homebrew FLTK and libsamplerate. Its only source edit was to
cast the sample rate to `double` before that division; the diagnostic patch
is `tests/sstv/fixtures/fldigi-4.2.11-float-deviation-diagnostic.patch`.
The modified binary SHA-256 was
`7bdc50fc74eebdcf407720c3fc8473757945440dbb8aa3978b3c2dfa731f66e8`.
Full A/B replays of the same Decodium captures at requested 800 Hz and with
automatic centering off both saved a 120 LPM APT-stop PNG. The independently
timing-predicted row was 5 for both; full-width MAE/p95/max against the same
Decodium-received proxy references was 1.392/1/143 and 1.411/1/144. This is
within the 8/4 image gate once the separately predicted phasing-header rows
are excluded, whereas the installed unmodified binary produced p95 7. It is
not an unaligned pass: the received PNGs still contain the phasing rows and
the reference boundaries are estimated from recorded audio. The build also
uses different dependencies from the distributed application, and the large
maximum errors remain. Artifacts are `.../decodium-fldigi-phase-tv_qqbcn`
and `.../decodium-fldigi-phase-rkuhnil1` in the macOS temporary directory.

An earlier full replay with an intermediate binary produced no PNG and did
not detect a valid stop; another short run encountered a PortAudio write
failure. These failed attempts are not used as evidence of the fix. The final
binary above was explicitly rebuilt after the source edit; short unmodified
source-build controls received the phasing in both A and B. This experiment
does not change the installed FLdigi, Decodium's transmitted waveform, RF
behaviour, or prove a universal interoperability fix.

As a build-environment control, the unmodified 4.2.11 source was compiled with
the same dependencies and options as the modified version. The control binary
SHA-256 was
`f5b878af27f8dad4e4013a31aa71ad096b177172665c2bde3466ff801e38ff7f`.
Full A/B replays at 800 Hz with automatic centering off both saved a 120 LPM
APT-stop PNG. The timing anchor again predicted row 5. Against the same proxy
references, the full-width MAE/p95/max was 7.506/7/141 and 7.528/7/143,
compared with 1.392/1/143 and 1.411/1/144 for the one-line arithmetic change.
This paired build control strengthens the causal evidence for the integer
division's effect on these inputs. It does not eliminate the phasing-header
rows or demonstrate unmodified FLdigi meets the unaligned quality gate.
Control artifacts are `.../decodium-fldigi-phase-oxe9o30q` and
`.../decodium-fldigi-phase-pjbys8e7` under the macOS temporary directory.

#### Controlled FLdigi automatic-centering comparison

The replay helper accepts `DECODIUM_FLDIGI_AUTOCENTER=0` or `1` and writes
the explicit `WEFAX_AUTOCENTER` value into its isolated FLdigi configuration.
The two original centered-phasing captures were replayed again with this set
to zero. Both resulting PNGs report `Auto-Center:Off`, 120 LPM and APT stop;
the earlier full-duration controls report automatic centering on. These tests
ran sequentially on macOS with BlackHole and no physical radio.

For comparison, use the first 120 rows of the earlier Decodium replay as a
proxy reference: those images were separately measured against their prepared
transmit references with component MAE below 0.10. Resizing each FLdigi
image to 128 pixels wide gives direct full-raster A/B MAE 48.216/48.744 with
centering on and 52.450/53.707 with it off; p95 is 248 in all four cases.
The post-hoc search excluding 32 columns per edge found row +5, column -25
for both on images, and row +5, column 0 for both off images. Best MAE was
7.05/7.07 on and 7.18/7.17 off, with p95 7 throughout. Thus this controlled
experiment associates the horizontal displacement with FLdigi automatic
centering in these recordings, while the vertical offset remains. Post-hoc
alignment is diagnostic and none of the four direct metrics pass the visual
gate. Proxy references, resampling and sequential playback limit the causal
claim; independent RF operation remains untested. Off artifacts are
`.../decodium-fldigi-phase-w5rib2p1` and
`.../decodium-fldigi-phase-m7955fwp` under the macOS temporary directory.

With automatic centering explicitly off, the two previously prepared
post-capture variants with three phasing lines and the white end line removed
were replayed through the same isolated FLdigi receiver. Both PNGs reported
`Auto-Center:Off`, 120 LPM and APT stop. Against the same Decodium-received
proxy references, direct A/B MAE was 16.013/16.267 and p95 69/72. The
post-hoc search selected row +1, column 0 for both, with MAE 7.061/7.069 and
p95 7. Compared with row +5 for the original recordings, this supports an
effect from the combined phasing edits in this setup, but cannot attribute it
to either edit separately. Neither direct comparison passes the quality gate;
the manipulated files are not evidence of an actual Decodium TX profile or
standards interoperability. Artifacts are
`.../decodium-fldigi-phase-kwgvwfof` and
`.../decodium-fldigi-phase-yxfm_hhy` under the macOS temporary directory.

To separate the two edits, the original A/B recordings were replayed with
automatic centering off and only three whole phasing lines skipped at sample
768,000 (1.5 seconds at 48 kHz); the white end line was retained. Both PNGs
report `Auto-Center:Off`, 120 LPM and APT stop. Against the same proxy
references, direct A/B MAE was 26.556/27.057 and p95 was 248 for both. The
post-hoc search selected row +2, column 0 for both, MAE 7.090/7.098 and p95
7. Together with the original +5 and combined-edit +1 observations, this
supports an approximately three-row effect from shortening the phasing and a
further one-row effect from removing the end line in this specific setup.
These are observations from replayed, altered audio and a post-hoc search,
not a direct quality pass or a basis by themselves for changing the TX
protocol. Artifacts are `.../decodium-fldigi-phase-b7fu4scl` and
`.../decodium-fldigi-phase-evcnsbit` under the macOS temporary directory.

A further control shows that the preceding offsets do not extrapolate
linearly. The combined-edit A/B recordings were replayed with one additional
0.5-second interval skipped at second 20 and automatic centering off. Both
PNGs report `Auto-Center:Off`, 120 LPM and APT stop, but the post-hoc best
offset became row +4, column 0 for both, rather than the predicted zero; best
MAE was 7.148/7.145 and p95 7. Direct MAE was 41.892/42.890 and p95 248.
The change of offset suggests acquisition or image-reset state is sensitive
to the extra cut; it does not prove the internal cause. In particular, the
earlier comparison alone does not justify assuming each removed line reduces
the final offset by one. The audio was manipulated and no production waveform
was changed. Artifacts are `.../decodium-fldigi-phase-swjk2qil` and
`.../decodium-fldigi-phase-fyy7v_as` under the macOS temporary directory.

The modified audio is `/tmp/decodium-fldigi-three-line-no-white-{a,b}.f32`;
FLdigi outputs and traces are `.../decodium-fldigi-phase-_ebzmueu` and
`.../decodium-fldigi-phase-cxeh0pc0`. The original captures were
`.../decodium-hamdrm-tx-a-e9bfat6_/capture.f32` and
`.../decodium-radiofax-tx-b-sx_qyjf2/capture.f32`. Reproduce with the helper
using `--first-frame 768000 --removed-lines 3 --line-frames 24000` and the
respective `--end-line-frame` values above. `test_sstv_radiofax_phasing_variant`
and `test_sstv_radiofax_alignment_analyzer` passed on this Mac. FLdigi was
stopped after each isolated replay; this is not RF evidence.
