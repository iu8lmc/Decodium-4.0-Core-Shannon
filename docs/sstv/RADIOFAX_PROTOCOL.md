# Experimental radiofax core

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

The experimental stop detector requires two matching one-second windows of
450 Hz modulation and keeps a bounded three-second observation buffer. It
backtracks short windows and aligns the boundary to the nearest raster line.
A/B tests cover chunking, 60/90/120/240 lines per minute, generated PCM, and
negative 300/675 Hz and constant patterns. A matching picture pattern can
still cause a false stop, and noisy or weak stop tones may be missed. The UI
reports stop detection and retains missing rows as uncovered; the resulting
image is partial when shorter than the configured maximum.

Remaining work includes reliable start and stop acquisition, more robust
image-start identification and line/slant
handling, real image dead-sector
handling, broader TX integration, further two-instance and independent-signal
coverage, and FAX480 geometry/profile reconciliation. A 675 Hz IOC selector
is ambiguous between IOC 288 and IOC 576 with alternate-line scanning; it
must not be classified solely from that selector.

### Two-instance local Radiofax check

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
