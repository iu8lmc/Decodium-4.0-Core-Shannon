# Experimental radiofax core

## Additional independent automatic-decoder observation

The unmodified [WEFAX Decoder](https://github.com/televisualarts-cloud/wefax_decoder)
at commit `99bfc22824dc970276d7c70b984df578bb7b6b11` was run offline on
the two previously recorded full-Decodium WMO 5%-white captures
`/tmp/decodium-fax90-independent/wmo-{a,b}.wav` (IOC 576, 90 LPM). Both
automatically detected the start and stop tones, IOC 576 and 90 LPM, and
produced a raw 1810 x 121 image from a 128 x 120 reference. No image height,
IOC or LPM was supplied to the decoder. This confirms automatic acquisition
on these two recordings, but not accurate unassisted image alignment.

Comparing the raw PNGs with the corresponding reference after resizing only
their width to 128 yielded full-raster direct MAE/p95 of 24.47/154 (A) and
23.48/135 (B); cross-image MAE was approximately 94. A diagnostic search
using reference pixels found a one-row offset and three-pixel circular shift
for both images, giving MAE/p95 16.98/70 and 12.26/40 respectively. These
post-hoc fits are exploratory, not successful automatic-alignment tests. The
audio was recorded from Decodium app pairs, but the independent decoder was
run on the saved files, not as a live peer. Neither off-air reception nor
general interoperability follows from these observations.

The runs used Python 3.10, NumPy 1.26.4, SciPy 1.12.0 and OpenCV
5.0.0.93 in an isolated environment. SciPy 1.15.3 failed to load on this
macOS installation. For each capture, the command was `OPENBLAS_NUM_THREADS=1
python wefax_decoder.py --wav INPUT --out OUTPUT --port PORT --no-browser
--no-ml --exit`. Logs, metadata and raw outputs are in
`/tmp/decodium-wefax-test-{a,b}`; reference paths are recorded in
`/tmp/decodium-fax90-independent/wmo-compare-{a,b}-{a,b}.json`. The
post-hoc diagnostic is `tests/sstv/tools/analyze_radiofax_partial_alignment.py`
with `--first-row 0 --rows 120 --column-step 1`.

Replaying the same A/B files with the independent decoder's explicit
`--align center` option, while still allowing it to determine IOC, LPM and
image boundaries, yielded exactly 120 raw lines for both captures. Full-image
direct MAE/p95 were 16.89/70 and 12.32/37. Most of the remaining error is
at the horizontal margins: these Decodium captures use a 4.5% dead sector,
whereas the independent raw output retains that sector rather than expanding
the 95.5% active picture. The center setting puts approximately half of the
dead sector at each edge. Sampling the active sector from column 41 of 1810
(round(1810 * 0.045 / 2)), at row zero and without searching reference
pixels, gives A/B MAE/p95/max 6.006/22/31 and 4.392/11/24. Crossed-reference
MAE is 94.367 and 93.532. This is an offline, source-informed geometry
comparison of the full active picture, not a claim that the third-party UI
removes the dead sector automatically. Logs and raw images are under
`/tmp/decodium-wefax-test-{a,b}-center`; the measurement uses
`analyze_radiofax_dead_sector.py REFERENCE RAW --row 0 --start 41`.
The decoder's default `--align end` and alternative `--align start` use
different phasing conventions. Selecting center was an explicit configuration
change; its successful geometry must not be presented as default behaviour.

A further A/B replay used the two captured 240 LPM, 5%-white, dead-sector
Decodium transmissions with three phase-like initial image rows mentioned
below (IOC 576 A, IOC 288 B). The original `capture.f32` files are stereo
48 kHz float audio; each was downmixed and resampled to mono 12 kHz WAV with
FFmpeg before feeding the same unmodified decoder with `--align center`.
Its front end reported both start and stop for both captures and selected the
correct IOC; its decode stage reported `stop=no`, acquired 240 LPM, and
produced 180 rows instead of the 120-row reference. Direct comparison of the
first 120 rows gave MAE/p95 22.34/138 and 25.17/170. A reference-driven
alignment search reduced this to 17.69/118 and 15.88/101, and remains only
an exploratory result. These cases therefore do not establish correct page
termination or geometry in the independent decoder. Outputs and logs are in
`/tmp/decodium-wefax-stereo-a-snpiwv50` and
`/tmp/decodium-wefax-stereo-b-egj91avv`.

The independent decoder's `segment_tones` ignores stop runs whose beginning
is not **strictly later** than 60 seconds after the classified end of the
start tone. On the complete 12 kHz recordings, its own classifier found a
stop run at 75.50–80.75 seconds for A against a 75.75-second threshold, and
74.25–79.50 seconds for B against a 74.50-second threshold. Thus these
correctly classified stop runs are rejected because their first blocks fall
0.25 seconds before that threshold; the separate frontend uses a different
elapsed-time check. This explains why its frontend said `stop tone` while
the decoder reported `stop=no` for these short pages.

As a controlled diagnostic, one second of image audio was duplicated near
the end of each mono recording, moving the unchanged stop waveform across the
threshold. With the same unmodified decoder and `--align center`, the decoder
then reported `stop=yes` in both cases and produced 121 rows for each
124-row modified page instead of 180. A reference-driven search of the first
90 rows after the first three original image rows matches candidate row zero
in both results (MAE/p95 13.25/63 A and 12.18/42 B). This is consistent
with the deliberately phasing-like initial three rows being removed as
phasing; it is a diagnostic inference, not proof that every remaining row
was recovered. The inserted audio was not transmitted by Decodium, and this
experiment must not be counted as a live A/B interoperability pass. See
`/tmp/decodium-wefax-long-{a-snpiwv50,b-egj91avv}` for logs and outputs.

Two additional unmodified 240 LPM, 256-line Decodium transmissions were
recorded from full application pairs, using timed WMO phasing and dead sector:
IOC 576 A had MAE/p95/max 0.340/0/45, IOC 288 B 0.266/0/45, with full
coverage and zero reported audio discontinuities. Artifacts are
`.../decodium-radiofax-centered-a-lg3ito10` and
`.../decodium-radiofax-centered-b-dbm6e773`. An offline replay to the same
independent decoder with `--align center` identified both IOC values, 240
LPM, start and stop. It returned 257 rows for A, but just 58 for B; for B it
reported 300 phasing lines and then removed 19 additional lines as phasing.
The laboratory's default images vary horizontally but have identical rows,
making them poor evidence for vertical alignment and, in B, ambiguous with
phasing. Consequently these observations do not demonstrate independent
recovery of complete image geometry. Independent logs are in
`/tmp/decodium-wefax-long-real-{a,b}`.

A stronger A/B repeated those 256-line transmissions with vertically varying
images, using distinct input PNGs derived from the earlier A/B phase-like
fixtures (nearest-neighbour resized to 320 x 256 and the first ten rows
replaced by row 20). The full Decodium pairs both completed with zero
reported audio discontinuities, timed WMO phasing and complete coverage:
IOC 576 A had MAE/p95/max 0.726/1/61; IOC 288 B 0.316/1/61. Captures,
reference and received images are in
`.../decodium-radiofax-centered-a-ib1t2yu9` and
`.../decodium-radiofax-centered-b-3esjrqjo`.

Offline replay of their original audio to the independent decoder with
`--align center` detected both IOC values, 240 LPM, start and stop, and
returned 256 rows in each case. Sampling the active sector from column 41 of
1810 for A and column 20 of 905 for B, both at row zero, produced full-image
MAE/p95/max 3.606/6/255 and 5.059/11/255. Crossed-reference MAE was 92.406
and 94.107. The maximum error shows that some pixels still differ sharply;
the results are not lossless or evidence of RF performance. The independent
decoder runs on saved recordings and requires explicit center alignment;
its output retains the dead sector, which the comparison removes according
to the transmitted profile, without a reference-driven alignment search.
Logs and outputs are `/tmp/decodium-wefax-detail-{a,b}`.

Two further full Decodium pairs tested IOC 288 / 90 LPM, 5%-white WMO
phasing, the 4.5% dead sector and timed acquisition with distinct structured
128 x 120 patterns. Both received all 120 rows, detected stop, used timed
phasing and reported no audio discontinuities; A/B MAE/p95/max were
0.117/0/8 and 0.083/0/8. Artifacts are
`.../decodium-radiofax-centered-a-5_feozzh` and
`.../decodium-radiofax-centered-b-w1bhynzl`.

The unmodified independent WEFAX Decoder described above replayed each
captured stereo stream after downmixing and resampling to 12 kHz mono. With
`--align center` selected and no IOC, LPM or image height supplied, both runs
reported IOC 288, 90 LPM, start and stop and returned exactly 120 rows.
Full-active-sector sampling from column 20 of its 905-column raw image,
at row zero and without a reference-driven alignment search, gave A/B
MAE/p95/max 6.001/22/31 and 3.254/7/21. The crossed-reference MAEs were
94.357 and 92.673. A's p95 exceeds the separate 15-level Decodium image
gate; no claim of identical pixels or universal interoperability follows.
The independent results are offline, and explicit center alignment and
source-informed dead-sector sampling are necessary. Logs and raw images are
in `/tmp/decodium-wefax-ioc288-90-{a,b}`.

For these two recordings, further diagnostics identify the independent
decoder's adaptive contrast as the principal source of the residual image
error. Its `level_scale` estimates a time-varying deviation from local tone
histograms; during approximately image rows 10–30 its estimate for A was
about 303–313 Hz, while B remained near 378 Hz. The transmitted black/white
deviation is 400 Hz. In a separate **modified-decoder diagnostic**, a
wrapper substituted a fixed 400 Hz level in memory, with the external source
unchanged. With the same row-zero, column-20 active-sector sampling, A/B
MAE/p95/max improved to 0.070/0/14 and 0.055/0/14. This supports the contrast
explanation for these clean known-level inputs; it does not prove that fixed
levels are appropriate for noisy RF, nor does it change the unmodified
decoder results above. Reproduce with
`tests/sstv/tools/run_wefax_fixed_level_diagnostic.py CHECKOUT` followed by
the same decoder arguments; the diagnostic outputs are in
`/tmp/decodium-wefax-ioc288-90-{a,b}-fixedlevel-verified`.

## Direct independent transmitter to Decodium receiver

Unmodified FLdigi 4.2.11 transmitted two distinct structured 128 x 120
images directly through BlackHole to a concurrently running Decodium
receiver, using IOC 576, 120 LPM and the explicit centered-phasing profile.
The receiver was ready before the independent transmitter started; this was
a live virtual-audio transfer, not a replay. Both cases saved 120 lines,
detected stop and reported no audio discontinuities, dropped chunks or
processing failures. Direct image MAE/p95/max was 0.460/1/9 for A and
0.576/1/13 for B. Crossed references gave MAE 43.622 and 43.794, confirming
the two patterns are distinguishable. The configured 150-line canvas
includes 30 unused rows. No radio or physical PTT was used.

Repeat on a Mac with BlackHole and isolated FLdigi using
`python3 tests/sstv/tools/run_fldigi_tx_live_decodium_lab.py a` and `b`
sequentially. The harness starts Decodium first, runs the existing isolated
FLdigi transmitter/capture tool, and checks RX diagnostics and image quality.
Artifacts are `.../decodium-fldigi-tx-live-tv4xtib0` and
`.../decodium-fldigi-tx-live-24nhrp2x`; their `tx.log` files identify the
corresponding independent audio captures. This proves the stated direction
and profile, not all Radiofax variants or reception over RF.

## Live Decodium transmitter to independent WEFAX receiver

The unmodified WEFAX Decoder at commit
`99bfc22824dc970276d7c70b984df578bb7b6b11` was also run as a live audio
receiver on BlackHole 2ch, concurrently with the Decodium transmitter and
receiver. Two separate 128 x 120, 5%-white WMO-dead-sector transmissions
used distinct vertically varying patterns, IOC 576 and 120 LPM. Both
Decodium pairs received all rows, detected stop and reported zero input
discontinuities; A/B MAE/p95/max were 0.155/0/11 and 0.111/0/11.

The independent receiver detected the start, IOC, 120 LPM and stop in each
live transfer and saved 120 rows. Its metadata reported zero audio overflows
and no inserted gaps. It was launched with `--device 'BlackHole 2ch'
--align center --no-browser --no-ml`; IOC, LPM and page height were not
supplied. The decoder retains the dead sector in its 1810-column raw image;
sampling the active sector at row zero and column 41, as derived from the
profile and without a reference-driven search, gave A/B MAE/p95/max
1.200/3/15 and 4.001/9/19. Crossed-reference MAEs were 91.214 and 93.246.
The explicit center selection and geometric sampling are part of the test;
they are not automatic dead-sector removal by the independent application.
The Decodium artifacts are `.../decodium-radiofax-centered-a-m55a2yci` and
`.../decodium-radiofax-centered-b-x7fkbh8v`, and independent outputs and logs
are `/tmp/decodium-wefax-live-{a,b}`. This is live virtual audio with
independent software, not an RF or physical PTT test.

The same live test is now repeatable with
`python3 tests/sstv/tools/run_wefax_live_two_app_lab.py a PYTHON` and then
`b PYTHON`, where PYTHON is the isolated independent decoder environment.
The harness checks that the external checkout is clean and pinned, waits for
its live BlackHole input before launching the Decodium pair, checks the
independent start/stop/IOC/LPM/row and audio-overflow metadata, and enforces
the active-sector MAE 8 / p95 15 gate. The repeated A/B runs passed with
MAE/p95/max 1.601/4/16 and 5.070/11/24; crossed-reference MAEs were 91.496
and 94.038. Artifacts are `.../decodium-wefax-live-lab-a-er6xfnpo` and
`.../decodium-wefax-live-lab-b-hymss2xl`. The first harness launch failed
because resolving the virtual-environment Python symlink discarded its
environment; preserving its original executable path fixed that setup error.

The receive page offers an opt-in **30-second WMO phasing after IOC start**
mode. WMO No. 386 describes an IOC-selection signal followed by 30 seconds
of phasing; either symmetric or 5%-white phasing is permitted. When the
selected IOC's start signal is recognised and phasing locks shortly afterward,
the receiver counts the standard phasing lines and anchors image start at the
expected boundary. This preserves initial image lines that resemble phasing.
The selection is off by default and cannot be combined with the centered
white-end-line convention. Without a recognised IOC start it falls back to
the existing heuristic. With the option selected, non-standard phasing
durations can yield an incorrect boundary. A recent confirmed or provisional
phasing clock is required at the deadline; if the signal vanished, the timer
is discarded and acquisition is restarted without publishing an image.

The core A/B tests cover both IOC signals, both phasing patterns, 60/90/120/
240 LPM at nominal and approximately ±0.3% line periods, phase-like initial image rows, wrong-IOC and 450 Hz
negative controls, early false image content, missing IOC and signal loss.
Two full Decodium application pairs used symmetric phasing with three initial
image rows matching phasing, at 240 LPM: IOC 576 A and IOC 288 B both used
the WMO timer, received all 120 rows, detected stop, reported no audio
discontinuities and achieved MAE/p95/max 0.3091/0/21 and 0.1592/0/21.
Artifacts are `.../decodium-radiofax-centered-a-s95p52r1` and
`.../decodium-radiofax-centered-b-lgewzi22`. On the same earlier A/B audio,
an offline native acquisition diagnostic without the timer started 9000
samples, or three lines, later; the timed diagnostic started at the expected
boundary. Those offline starts do not substitute for the full application
tests.

Two additional full Decodium pairs exercised the timed option at 90 LPM with
the same three phase-like first rows: IOC 576 A and IOC 288 B both used the
timer, received 120 complete lines and detected stop without reported audio
discontinuities. A/B MAE/p95/max were 0.1066/0/7 and 0.0569/0/7. On the same
captures the offline diagnostic again started three lines later without the
timed option. Artifacts are `.../decodium-radiofax-centered-a-xnxfv47y` and
`.../decodium-radiofax-centered-b-h8fn9qhk`. Synthetic IOC signal tests also
cover approximately ±1% start-modulation frequency, in addition to wrong IOC
and stop-frequency controls. These rates and tolerances are not a guarantee of
reception over RF or of automatic alignment in independent software.

Before the final missing-signal guard, two full Decodium pairs also tested
the 5%-white profile with WMO dead sector and phase-like first rows, IOC 576 A
and IOC 288 B; both used timed phasing, achieved 120 lines and stop without
reported discontinuities, and had MAE/p95/max 0.3201/0/22 and 0.2311/0/22.
Artifacts are `.../decodium-radiofax-centered-a-snpiwv50` and
`.../decodium-radiofax-centered-b-egj91avv`. Offline comparisons of those
same recordings again found the ordinary heuristic three lines later than the
timed path. After the guard, A/B application replays which retained the start
and first 15 seconds of phasing but replaced the remainder with silence both
logged that the timer was armed and produced no image; artifacts are
`.../decodium-fldigi-radiofax-replay-jq8vqe89` and
`.../decodium-fldigi-radiofax-replay-bc2s6qqp`. The altered inputs are
`/tmp/decodium-fax90-independent/wmo-signal-loss-{a,b}.f32`.

Two further full receiver replays with eight broad false-white disturbances
within the 5%-white phasing before the final missing-signal guard completed
120 lines with stop and no reported audio loss; A/B MAE/p95/max were
0.3201/0/22 and 0.2311/0/22. Artifacts are
`.../decodium-fldigi-radiofax-replay-2or2mhts` and
`.../decodium-fldigi-radiofax-replay-jpe32zkb`. These are controlled edited
recordings, not RF results. The runtime's consecutive-image A/B regression
passed with and without timed phasing; the receive-page offscreen test covers
the new control. All application evidence above is local virtual audio and
does not establish independent-peer or arbitrary-interference performance.

The transmit page now exposes the existing 50%-white symmetric phasing
profile, which was previously implemented in the encoder but inaccessible
there. It is mutually exclusive with centered 5%-white phasing and may be
combined with the explicit WMO dead sector. The previous four-argument bridge
entry point continues to select 5%-white phasing. The offscreen Studio QML
test checks both selections, mutual exclusion and the dead-sector combination;
invalid centered/symmetric and invalid boolean test-only profiles are rejected.

Two full Decodium application pairs exchanged distinct 128×120 grayscale
patterns at IOC 576 / 240 LPM with symmetric phasing over BlackHole. Both
detected stop, reported complete coverage and no audio discontinuities or
dropped chunks; A/B MAE/p95/max were 0.3104/0/21 and 0.1577/0/21. A coarse
10 ms block analysis of ten seconds of each recorded phasing signal found
48% and 52% white-dominant blocks, consistent with the selected symmetric
profile but not an exact duty-cycle measurement. Artifacts, including the
recorded stereo audio, are `.../decodium-radiofax-centered-a-iyvjr_25` and
`.../decodium-radiofax-centered-b-a4hp0r16`. Reproduce with
`DECODIUM_LAB_RADIOFAX_SYMMETRIC=1` and
`run_radiofax_centered_two_app_lab.py a 240`, then `b 240`; set
`DECODIUM_LAB_FAX_CAPTURE=1` and `DECODIUM_LAB_RADIOFAX_REQUIRE_STOP=1` to
retain audio and enforce the stop check.

The selectable combination of symmetric phasing and the WMO 4.5% dead sector
was also exercised between two full Decodium sessions for both patterns at
IOC 576 / 240 LPM. Both detected stop and reported complete coverage without
audio discontinuities or dropped chunks; A/B MAE/p95/max were 0.3181/0/22
and 0.2256/0/22. Artifacts are
`.../decodium-radiofax-centered-a-aakdd6sv` and
`.../decodium-radiofax-centered-b-jpjlpcv0`. Set both
`DECODIUM_LAB_RADIOFAX_SYMMETRIC=1` and
`DECODIUM_LAB_RADIOFAX_WMO_DEAD_SECTOR=1` in the same two-app harness. This
local A/B test does not establish independent-peer or RF interoperability.

The independent `radio_fax_decoder` build described below also decoded both
recordings offline with 240 LPM explicitly supplied. It logged start, a
phasing estimate and stop and produced 403/402 rows including headers. After
discarding 80 leading rows and searching the next 100 rows and horizontal
offsets post hoc, the diagnostic compared only the centre half of each image:
matching A/B MAE/p95 were 2.34/2 and 2.02/2, versus crossed MAE 76.79/80.48.
Direct full-raster comparisons before the search were 118.74 and 118.43 MAE.
This confirms recognisable A/B content in that offline decoder, not automatic
image-boundary alignment, full-width quality, live peer or RF interoperability.
Outputs and comparison JSON are under
`/tmp/decodium-fax90-independent/symmetric-auto-{a,b}` and
`/tmp/decodium-fax90-independent/symmetric-{compare,cross}-*.json`.

After the later white-line acquisition change, the previously intermittent
IOC 576 / 90 LPM centered profile was repeated with two Decodium application
pairs and recorded virtual audio. A and B each recovered 120 rows with full
coverage and stop detected; both reported zero discontinuities and processing
failures. MAE/p95/max were 0.0602/0/3 and 0.0313/0/2. The terminal clock
estimate was 320,000 samples over 40 lines in each run with no tracker
restarts. Artifacts, including PCM captures, are
`.../decodium-radiofax-centered-a-1_ra3osr` and
`.../decodium-radiofax-centered-b-floh3r6o`. The current FAX480 and Radiofax
component CTests also passed. These two successful attempts do not determine
the cause or eliminate the earlier intermittent failure, nor establish RF
or independent-peer interoperability.

Those two IOC 576 / 90 LPM captures were replayed into an isolated unmodified
FLdigi 4.2.11 receiver with automatic centering off. Both logs recorded 44
phasing lines identified as 90 LPM, but each of 11 logged four-line
transitions applied 120 LPM. Neither produced a PNG; both reported discarding
an image for low correlation. A logged receipt of the 450 Hz APT stop while
receiving; B did not log that same event. These are failed independent-peer
tests, not a visual quality pass. The installed binary was not built from
the source during this experiment. The current public
[FLdigi WEFAX source](https://raw.githubusercontent.com/w1hkj/fldigi/master/src/wefax/wefax.cxx)
sets a default of 120 LPM for IOC 576 and uses the default on transition to
image reception, consistent with but not proof of the installed binary's
exact implementation. Manual 90 LPM selection has not been tested here.
`analyze_fldigi_lpm.py` reports detected and applied rates, stop and
low-correlation markers, and exits nonzero for a mismatch or absent evidence;
its synthetic 90/120, mixed-rate and missing-evidence controls pass. Logs
are `.../decodium-fldigi-phase-ch2ympq7` and
`.../decodium-fldigi-phase-9tu5kr__` in the macOS temporary directory.

The same two Decodium recordings were also decoded offline with the independent
[radio_fax_decoder](https://github.com/darauble/radio_fax_decoder) at commit
`c5321bcb2e196bf33bf34c11cd3a4c86b0bd5358`. Its unmodified source was built
with `clang++ -std=c++20 -O2 -include cstdlib src/fax.cpp src/avg.cpp
src/FaxDecoder.cpp`; the forced standard header supplies declarations needed by
this macOS build. Recordings were converted from stereo float 48 kHz to mono
16-bit 12 kHz WAV and decoded with `--lpm 90 --pixels 1809 -n`. The `-n`
option disables automatic phasing: this is a manually configured offline
decode, not evidence of automatic acquisition or a live independent receiver.

The 1809-pixel-wide output includes the preamble. The diagnostic
`analyze_radiofax_2d_alignment.py --max-rows 100 --max-columns 32` resized its
width to 128 and searched offsets after decoding, scoring only the middle 64
columns of the 120 reference rows. The best A-to-A and B-to-B MAEs were 1.01
and 2.00 on the 0–255 scale; crossed A-to-B and B-to-A comparisons were 50.25
and 49.53. As negative rate controls, decoding the same recordings at 120 LPM
yielded best MAEs 6.24 and 5.08. These values are diagnostic, not full-image
quality measurements: the images are simple, half the width was excluded,
and B's best 90 LPM horizontal shift reached the search boundary. Unaligned
full-raster MAEs at 90 LPM were 65.32 and 45.87 because preamble and alignment
remain in the output. The result supports manual-rate waveform decoding but
does not resolve FLdigi's automatic-rate mismatch, establish 90 LPM automatic
independent reception, or prove RF interoperability. Inputs, PGM output,
decoder logs and comparison JSON are in `/tmp/decodium-fax90-independent`;
the original two-app captures are identified above.

With `-n` removed, the same independent decoder also logged start, a phasing
estimate and stop for both captures at the explicitly configured 90 LPM.
It wrote 224 and 223 rows, including headers and trailing samples. The same
post-hoc search gave A-to-A/B-to-B MAE 1.01/2.00 versus crossed MAE
50.25/49.81; direct full-raster MAE was 60.64/43.16. Repeating with
`--no_header --auto_stop` produced 151/152 rows and best matching MAE
2.03/4.07, but both best row offsets hit the 30-row search boundary and the
direct full-raster MAE was 32.76/23.33. The logs demonstrate detected events,
not correctly bounded or automatically aligned images. The manually supplied
rate, post-hoc alignment, cropped diagnostic and simple input images remain
material limitations. These runs do not establish automatic LPM selection,
FLdigi compatibility, or complete independent-receiver interoperability.

Two additional centered IOC 576 / 90 LPM runs used distinct 128×120
spatially varying grayscale patterns. Each ran between two full Decodium
sessions over virtual audio, requiring stop detection, full coverage and no
reported discontinuities; A/B MAE was 0.1064/0.0547, p95 zero and maximum
error 7/7. Captures and reference images are in
`.../decodium-radiofax-centered-a-12rfcpv1` and
`.../decodium-radiofax-centered-b-wor3homq`. After conversion as above,
the independent decoder, with 90 LPM explicitly configured and phasing
enabled, logged start, a phasing estimate and stop for both. Its output had
223 rows including headers. The same diagnostic chose row 66 and column 0 for
both matches, with centre-half MAE 1.48/1.51 versus crossed MAE 79.21/77.85.
At those post-hoc offsets, full-width MAE was 1.60/1.52, p95 was 2/2 and
maximum error was 28/56. Full-width errors include the higher transition
errors omitted by a centre-only measure. These A/B captures give stronger
waveform evidence than uniform images, but the row was still selected after
decoding and the output was not automatically bounded to 120 image rows.
The independent replay outputs and comparison JSON are in
`/tmp/decodium-fax90-independent/pattern-auto-{a,b}` and
`/tmp/decodium-fax90-independent/pattern-compare-*.json`.

The two 90 LPM patterned recordings were independently checked again using
the pinned `radio_fax_decoder` source, rebuilt without edits by
`bash tests/sstv/tools/build_radiofax_decoder_oracle.sh CHECKOUT OUTPUT`.
The builder rejects an incorrect revision, modified source or an existing
output; the latter two negative controls passed. The rebuilt executable had
SHA-256 `e6e454170f59f67dd63663b56523f3edd6aa283db7a9b4b801475d473266e2aa`;
its decoded A/B PGM files were byte-identical to the previous build's output.

`analyze_radiofax_decoder_stop_anchor.py` obtains the image row from the
independent decoder's first sustained stop sequence and its documented
header-inclusive 1809-pixel output accounting, subtracting the known 120-line
transmitted height. It does not search pixels or choose a horizontal shift.
For both recordings it obtained row 66 and compared the entire 128×120
reference width after resizing. A/B MAE/p95/max were 1.600/2/28 and
1.519/2/56; crossed references gave MAE 90.815 and 90.675. Unanchored
full-raster MAE remained 122.709/105.484 because output contains the header.
The helper's synthetic A/B and missing/broken-stop controls passed. This is
an independent offline receive comparison with the rate and height supplied
and the boundary derived after stop, not automatic image-boundary acquisition
or a live peer/RF test. Rebuilt artifacts are
`/tmp/decodium-fax90-independent/pinned-rebuild-{a,b}`.

Applying the same stop-and-known-height method to the earlier symmetric
240 LPM independent-decoder outputs did **not** pass a full-width comparison:
A/B MAE/p95/max were 27.552/144/246 and 16.382/10/252. The decoder's output
includes a horizontal displacement in those cases; no pixel-derived shift is
applied by this diagnostic. The prior cropped post-hoc comparison remains a
separate observation, not a full-width independent interoperability pass.

An additional diagnostic derives horizontal alignment without consulting
reference pixels: `analyze_radiofax_decoder_stop_anchor.py --symmetric-phasing`
uses the median of 25 preceding phasing rows to locate the leading white edge,
requires a single stripe occupying 40–60% of the line, and applies that edge
to the continuous output raster. Image height remains supplied and the row
comes from the logged stop as described above. For both symmetric A/B outputs
the phasing-derived shift was -63 of 1809 decoder pixels. Full-width
stop-and-phasing-anchored MAE/p95/max was 1.550/3/36 and 1.498/3/25;
crossed-reference MAE was 90.790/90.772. Synthetic A/B positive and negative
controls reject missing, multiple or narrow stripes. The rebuilt pinned
decoder produced byte-identical PGM output for both recordings. The direct
unaligned and stop-only metrics remain failures as recorded above: this is an
offline, post-decoding alignment derived from signal and known height, not a
claim that the unmodified independent application automatically aligns or
bounds the final image, nor evidence of live or RF interoperability.

For the WMO 4.5% dead-sector profile, two further IOC 576 / 90 LPM full
Decodium exchanges used the same varying patterns and retained virtual audio.
Both had 120 complete rows, detected stop and reported zero discontinuities,
dropped chunks or processing failures. A/B image MAE/p95/max were
0.1167/0/8 and 0.0833/0/8. Artifacts are
`.../decodium-radiofax-centered-a-mk7gn6a1` and
`.../decodium-radiofax-centered-b-2uw6724_`. The independent decoder above,
using the same conversion and explicit 90 LPM with phasing enabled, logged
start, phasing and stop for both and wrote 223 rows including headers. A/B
post-hoc centre-half comparisons had MAE 4.50/3.26, p95 6/4 and maximum
254/255; crossed comparisons had MAE 79.02/78.11. Direct full-raster MAE
was 123.21/110.20. The diagnostic resized the entire fax line, including
the dead sector, to the image width and searched row/column offsets; it did
not correct dead-sector geometry or define the image boundaries automatically.
These numbers demonstrate recognisable content and control events, not a
successful full-image interoperability or quality test. Independent output,
logs and comparison JSON are `/tmp/decodium-fax90-independent/wmo-auto-{a,b}`
and `/tmp/decodium-fax90-independent/wmo-compare-*.json`.

As a separate post-hoc diagnostic, `analyze_radiofax_dead_sector.py` samples
the active 95.5% of each received line at reference-pixel centres on the
continuous raster. Row and horizontal sample offset are supplied explicitly;
they were selected by a search against the reference, not inferred by the
decoder. At row 64 and offsets 32/36 of 1809 samples, A/B full-width MAE was
1.56/1.61, p95 3/3 and max 14/14; crossed references at those offsets gave
MAE 90.98/91.04. A/B synthetic checks cover different offsets, crossing a
line boundary, a mismatched reference and invalid inputs. This supports the
presence of the expected active-sector content but does not demonstrate that
an operator's independent decoder automatically removes the dead sector or
aligns and bounds the final image.

A further replay of capture A again detected 44 phasing lines at 90 LPM but
applied 120 at all 11 logged four-line transitions and discarded the image
for low correlation (`.../decodium-fldigi-phase-syq4gbi4`). During that run,
the isolated FLdigi receive window exposed no LPM control in the visible
controls, and its XML-RPC method list exposed no LPM setter. This observation
does not rule out a control elsewhere or a different FLdigi version. The
Decodium transmit page now shows a targeted notice for centered IOC 576 / 90
LPM, asking the sender to check the receiving station's rate. The offscreen
Studio QML test checks this notice and the IOC 288 / 90 and IOC 576 / 120
controls; it passed. The earlier two-Decodium A/B transfers above remain the
functional evidence for the unchanged 90 LPM waveform.

Separate two-Decodium A/B application runs exercised IOC 576 / 90 LPM with
the WMO 4.5% dead sector instead of centered phasing. Both produced 120
complete rows, detected stop and reported zero discontinuities. The received
images had MAE 0.1070/0.0856, p95 0/0 and maximum error 5/5. Artifacts are
`.../decodium-radiofax-centered-a-4y54to6k` and
`.../decodium-radiofax-centered-b-brnbju7z`. These selected local transfers
do not resolve the independent FLdigi rate mismatch or establish RF reception.

With the later white-line confirmation enabled, unmodified FLdigi 4.2.11
transmitted two fresh 128×120 reference images at IOC 576 / 120 LPM into
virtual-audio recordings. The complete, unedited recordings were replayed
into separate current-build Decodium receivers using the explicit centered
phasing option and a 150-line canvas. Both recovered 120 complete rows,
detected stop and reported no audio discontinuities, dropped chunks or
processing failures. Without pixel translation, MAE/p95/max was
0.0774/0/6 and 0.0451/0/4. The 80% canvas coverage is expected because
30 of the 150 configured rows were not transmitted. Captures are
`.../decodium-fldigi-tx-capture-t3puml1u` and
`.../decodium-fldigi-tx-capture-oovb39d2`; Decodium receive artifacts are
`.../decodium-fldigi-radiofax-replay-m_3u8_to` and
`.../decodium-fldigi-radiofax-replay-f35yeezw`. This is a file-mediated
FLdigi TX to Decodium RX test, not live RF or the reverse-direction 90 LPM
interoperability that remains unresolved.

Two full Decodium-to-Decodium sessions used centered phasing with IOC 288 at
90 LPM and 128×120 pixels. Both A/B received complete images, reported all
120 lines and stop detection, and showed zero reported audio discontinuities,
dropped chunks or processing failures. Image MAE was 0.0602/0.0313 on the
0–255 scale, p95 was zero, and maximum error was 3/2. The ordinary two-app
harness checks completion, source and continuity; terminal diagnostics were
additionally inspected for line count and stop. Artifacts are
`.../decodium-radiofax-centered-a-fx8sqbps` and
`.../decodium-radiofax-centered-b-ul3314td`. These selected virtual-audio
transfers do not establish independent-peer or RF reliability and do not
explain the earlier IOC 576/90 intermittent failure.

The centered two-app lab now offers `DECODIUM_LAB_RADIOFAX_REQUIRE_STOP=1`
to require a terminal stop, complete coverage and the expected line count
instead of accepting completeness alone. Synthetic A/B controls reject
missing or false stop, incorrect count or coverage, including a later
regressing terminal snapshot. The check also accepted the prior IOC 288/90
application logs above and rejected copies with stop removed. Two fresh
IOC 288/240 application A/B transmissions passed the new explicit stop
check, with full coverage, no reported audio discontinuities or dropped chunks,
MAE 0.1721/0.1059, p95 zero and max 11/6. Artifacts are
`.../decodium-radiofax-centered-a-onwn8rto` and
`.../decodium-radiofax-centered-b-lxafqnf_`. This is a stricter lab assertion,
not a change to stop detection or proof of arbitrary RF behavior.

Two further two-Decodium IOC 288/240 LPM sessions configured a 150-line RX
canvas for 120 transmitted lines. Both detected stop, reported exactly 120
published lines and 80% coverage, and marked the image partial rather than
complete. No audio discontinuities, dropped chunks or processing failures
were reported. The received 120-row images had A/B MAE 0.1721/0.1059,
p95 zero and maximum error 11/6. Artifacts are
`.../decodium-radiofax-centered-a-jvlttqst` and
`.../decodium-radiofax-centered-b-20segezx`. These are selected virtual-audio
tests, not independent-peer or RF stop-reliability evidence.

The partial-stop lab now additionally requires the exact canvas dimensions,
15,360 completed pixels and 46,080 covered RGB components for that 128×150,
120-line case, instead of allowing a one-percentage-point coverage tolerance.
Both retained two-application diagnostic logs above passed the stricter
check; A/B controls with altered pixel counts failed. The component test for
four positive and negative clock drifts now examines every coverage mask:
all RGB components of rows 0–119 are covered and every component of rows
120–149 is uncovered. The Radiofax component suite passed. The exact per-pixel
mask check is component-level; the two-application logs report aggregate
coverage and line counts, not the individual masks.

## Clock drift and reacquisition after phasing interference

Two earlier 240-LPM A/B virtual-audio captures were copied with their
48 kHz rate interpreted as 48,144 Hz and 47,856 Hz respectively, then
resampled to 48 kHz. This simulates opposite sample-clock offsets and also
shifts audio frequencies. An independent phasing-edge measurement found
approximately -2,991 ppm and +3,009 ppm in the resulting recordings. The
A capture initially failed: the receiver treated phasing as image data and
did not report the expected stop. The B capture also failed after an initial
mean-period change: its first measured phasing period straddled the APT-to-
phasing transition and biased the estimated direction of drift. These
failures are `.../decodium-fldigi-radiofax-replay-mnvws2xy` and
`.../decodium-fldigi-radiofax-replay-r7xryznx`.

The initial image-start anchor now uses the median of three accepted phasing
periods, rather than a single period or their mean, when the measured period
differs from nominal by more than 0.2%. Smaller offsets retain nominal timing
until the longer multi-pulse estimate is ready, avoiding an observed clean
loopback regression from early jitter. A/B component tests cover both drift
directions and a single atypical period among the three. With the corrected
anchor, the clean-drift A/B recordings reached 120 complete lines and detected
stop; artifacts are `.../decodium-fldigi-radiofax-replay-9czhpa7u` and
`.../decodium-fldigi-radiofax-replay-0_rlnh1d`. A was run with the earlier
three-period mean, while B used the final median; the final median was also
exercised in the combined A/B tests below.

When two irregular pulses invalidate an established clock during phasing,
the automatic receiver retries clock acquisition, up to 1,800 times, after
at least one estimate has been confirmed. A new estimate requires at least
two seconds of observations. It
temporarily retains the last confirmed period only as an image-start search
anchor; the image decoder uses a clock only if the new tracker has acquired
its own valid estimate. With no prior valid estimate it falls back to nominal
timing. After the restart limit, the last confirmed estimate remains only as
an image-start anchor; image decoding still falls back to nominal timing. A synthetic A/B regression previously
acquired image content during phasing; it now locates the actual image start,
reacquires the correct clock and records one restart in each direction.

The same two recordings with both the rate change and the two injected false
phasing pulses were replayed into full Decodium receivers. Both reacquired a
clock after one restart, reported 120 complete lines and detected stop, with
no reported discontinuities, dropped chunks or processing failures. A/B MAE
was 0.142/0.101 and p95 zero; artifacts are
`.../decodium-fldigi-radiofax-replay-f30b3ug0` and
`.../decodium-fldigi-radiofax-replay-_ikyymi4`. Two fresh ordinary
Decodium-to-Decodium A/B transfers with the final code also passed with stop,
120 lines and no reported discontinuities; artifacts are
`.../decodium-radiofax-centered-a-2dkvulog` and
`.../decodium-radiofax-centered-b-k54hudlq`.

The final build also passed the `test_sstv_radiofax`,
`test_sstv_radiofax_phasing_glitch` and `test_sstv_rx_runtime` CTest targets
together (three of three; the runtime integration test took 333 seconds).
These tests complement the selected application transfers above and do not
extend their interoperability or RF coverage.

These checks use selected 128-pixel local images and manipulated recordings;
they do not establish general RF resilience or independent-sender
interoperability. `analyze_radiofax_acquisition` is an offline native-frontend
diagnostic, not a replacement for the full application replay. The restart
bound limits reacquisition work, not the duration or quality of all possible
disturbances. An additional observation-level A/B test applies four pairs of
irregular pulses, separated by enough valid phasing to reacquire the clock,
at both clock offsets. It verifies four restarts, a newly valid clock and the
expected image start. Reception after exhausting the new limit remains unverified.

The subsequent full-application A/B replay used the two opposite-drift
recordings with four pairs of inserted phasing pulses. Both runs reported
three restarts, an invalid tracker, zero validated image-clock samples, 120
lines and detected stop without reported audio drops. However, image comparison
failed: A/B mean absolute error was 16.50/10.41 and p95 was 129/76, above the
8/4 limits. Artifacts are `.../decodium-fldigi-radiofax-replay-uoho83bq` and
`.../decodium-fldigi-radiofax-replay-wlsm0vf7`. Nominal image timing after
the former three-retry limit was therefore insufficient for these signals.
The impairment helper's `--four-pairs`
mode and its A/B tests check all eight modified intervals and unchanged audio.

With the revised bound, the same A/B recordings passed full Decodium replay:
each reported four restarts, a valid new estimate, 120 lines and detected
stop, without reported dropped chunks or discontinuities. Mean absolute error
was 0.145/0.162 and p95 zero. Artifacts are
`.../decodium-fldigi-radiofax-replay-y3ldyn5a` and
`.../decodium-fldigi-radiofax-replay-9pwq7k22`. These are selected altered
local recordings, not RF or general interference guarantees.

An observation-level A/B regression then exposed a separate failure when
disturbances recur before the restarted tracker confirms its estimate. Once
an initial clock has been confirmed, the receiver now permits another bounded
restart in that situation, retaining the old clock only as a provisional
start anchor. The new tracker must still independently confirm its estimate
before image decoding uses it. The synthetic A/B test and Radiofax component
suite pass; the full-application replay is described below.

The closer-spaced A/B recordings were subsequently replayed through full
Decodium receivers. Both reported seven restarts, a valid new estimate, 120
lines and stop, without reported dropped chunks or discontinuities. Mean
absolute error was 0.145/0.104 and p95 zero. Artifacts are
`.../decodium-fldigi-radiofax-replay-zuiqfiuf` and
`.../decodium-fldigi-radiofax-replay-ms149va_`. The helper's `--close-pairs`
mode has A/B checks for all modified and unchanged intervals. These are
selected altered local recordings, not RF tests.

An additional synthetic A/B negative control applies repeated pulse-shaped
disturbances through the remainder of phasing. After multiple restarts it
finds the expected image-start position in both drift directions, but no
unconfirmed clock is passed to the image decoder.
The tracker may still be valid in the sense that it has not been rejected,
without yet having enough observations for a clock estimate; validity alone
does not establish an estimate. This observation-level control does not
establish application-level image quality under persistent interference.

Full-application A/B replays then inserted 204 short false pulses, two on
each of 102 consecutive phasing lines, into the opposite-drift recordings.
Both reported 153 tracker restarts and zero validated clock samples, then
detected stop and 120 lines, but image comparison failed: mean absolute
error was 16.41/10.41 and p95 129/76. Artifacts are
`.../decodium-fldigi-radiofax-replay-9s2cp4m_` and
`.../decodium-fldigi-radiofax-replay-jb95m10f`. The first run stopped at
the explicit reacquisition assertion; its saved image was compared separately.
The helper's `--persistent-pairs` mode and A/B interval controls reproduce
the impairment. Persistent pulse rejection and image clock recovery remain
open; these manipulated recordings do not represent all RF interference.

An optional phase-reference filter is used after a confirmed clock is lost
and the automatic receiver restarts its tracker. In an observation-level A/B
test it ignores repeated off-phase pulses and independently estimates the
clock; a deliberately wrong reference produces no estimate. The component
suite passes. The tracker invalidates its estimate after 4.5 periods
without an accepted pulse. Changing clock rates and broader application-level
behaviour remain to be verified.

With the filter enabled, the two persistent-interference recordings above
passed full Decodium replay: each reported one restart, 203 ignored irregular
pulses, a newly valid clock, 120 lines and stop without reported audio
discontinuities. Mean absolute image error was 1.051/0.111, p95 1/0; artifacts
are `.../decodium-fldigi-radiofax-replay-xt6yf9z9` and
`.../decodium-fldigi-radiofax-replay-irm0snhr`. These remain selected local
recordings, not a general claim about RF interference.

An observation-level A/B test changes the line period from 2,991 to 3,009
samples and vice versa after an initial disturbance and confirmed clock.
The clock tracker eventually invalidates its old reference, but the automatic
receiver identifies image start prematurely in both scenarios. The test
currently records this known failure rather than asserting correct reception;
it must be replaced by a correct-start regression when fixed. No full-app
or RF result for this changing-rate scenario has been established.
At 12 kHz, the observed candidate starts were 104,688 and 105,317 samples,
versus expected image starts of 363,549 and 362,451 respectively. The
change occurs at 89,730/90,270 samples, so the false candidate is roughly
five lines after the change. This points to image-start alignment becoming
stale before the clock tracker rejects its old phase reference; it does not
by itself prove the best correction.
Both early starts occur while a restarted tracker is valid but has not yet
confirmed a new estimate; each reports two restarts and no image clock. An
experimental alignment adjustment applied only to confirmed edges did not
prevent either failure and was removed. A correction must also account for
the period while reacquisition has no confirmed estimate, including an actual
image beginning during that interval.

The centered white-end-line profile now checks buffered observations for a
predominantly white candidate after a clock has been confirmed, rejecting
candidates with insufficient evidence or an inconsistent line. A newly
confirmed clock anchors image timing to the tracker's latest confirmed edge
when that edge precedes the candidate. The changing-rate synthetic A/B test
now checks the correct image start, and the immediate-image A/B control still
passes without an unconfirmed clock. The complete component suite passes.
Replays of the earlier constant-rate persistent-interference recordings also
passed with 120 lines and stop: A/B MAE 1.051/0.111, p95 1/0; artifacts are
`.../decodium-fldigi-radiofax-replay-b5qcrlij` and
`.../decodium-fldigi-radiofax-replay-pfdd2mpg`. Full-application tests with an
actual mid-phasing rate change remain open. The historical failure and
experiments above describe the earlier implementation.

The mid-phasing change was subsequently tested in full Decodium A/B replays.
The original two-application stereo captures were split at 22.06/21.08 s;
FFmpeg `asetrate`/`aresample` applied 48,144 then 47,856 Hz for A and the
reverse for B, with the segments concatenated. The existing two-pulse helper
inserted two earlier false pulses. Both receivers reported four restarts,
a valid final clock, 120 lines and detected stop without reported audio
discontinuity. A/B MAE was 0.158/0.086 and p95 zero; artifacts are
`.../decodium-fldigi-radiofax-replay-43vnbdwb` and
`.../decodium-fldigi-radiofax-replay-cmfopn70`. This is a selected local
resampling experiment with a splice, not a measurement of real RF or proof
for every possible clock transition.

With a 20% black interruption in the white end line, the synthetic A/B
changing-clock test initially failed to acquire the image. The candidate
threshold was adjusted from 75% to 60% white observations; the changing-clock,
immediate-start and full Radiofax component tests pass. In full Decodium A/B
replays, 50 ms of 1500 Hz replaced part of the measured white end line in
the changing-rate recordings above. Both produced 120 lines and stop with no
reported audio discontinuity; MAE was 0.158/0.086 and p95 zero. Artifacts are
`.../decodium-fldigi-radiofax-replay-kev0tk5a` and
`.../decodium-fldigi-radiofax-replay-ue1rskwm`. These selected disturbances
do not establish a general tolerance to damaged or missing end lines.

An observation-level A/B negative control also adds a 40%-line false white
interval to eight phasing lines after the simulated rate change. The receiver
still identifies the later end line at the expected position in both
directions. This synthetic pattern is not a general bound on all false-white
interference. The application replays below use a comparable audio impairment.

Two subsequent full Decodium replays inserted eight 100 ms, 2300 Hz
intervals into measured black phasing after the rate change in the A/B
recordings. Each produced 120 lines and stop without reported audio drops or
discontinuities; A/B MAE was 0.161/0.082 and p95 zero. Artifacts are
`.../decodium-fldigi-radiofax-replay-f5ii88cg` and
`.../decodium-fldigi-radiofax-replay-ja4fpy58`. The terminal logs do not
identify which particular candidate was rejected, so these replays show the
outcome for the selected manipulated audio, not classification of each
inserted interval or general RF resistance.

The observation-level `autoReceiverStartsImageDuringClockReacquisitionAB`
control now places the white end line immediately after a disturbance, before
the new tracker has a valid estimate. Both rate directions identify the
expected image start and use no unconfirmed clock. This protects a real-start
case while the changing-rate false-start bug remains open; it does not prove
image quality or full-application behaviour for this short phasing sequence.

To exercise that short sequence with recorded audio, two 240 LPM Decodium
captures were altered with `prepare_radiofax_phasing_glitch.py --two-pulses`,
then `shorten_radiofax_phasing.py` removed 101 phasing lines so the recorded
white end line follows the disturbed line. The helper's outputs were checked
byte-for-byte against the two earlier hand-spliced files. Replays through a
full Decodium receiver produced 120 lines and stop, with no reported audio
discontinuities; A/B MAE/p95/max were 0.172/0/11 and 0.105/0/7. Both terminal
logs show one clock restart and no freshly confirmed image clock. Artifacts
are `.../decodium-fldigi-radiofax-replay-p1pnypjv` and
`.../decodium-fldigi-radiofax-replay-5f76fdk7`. The replay harness now waits
through its configured minimum application lifetime for shortened captures;
the earlier fixed 30-second post-playback wait timed out prematurely.

Resampling those same altered files from 48,144 and 47,856 Hz to 48,000 Hz
then exposed a remaining defect in both directions: 120 lines and stop were
reported, but A/B image MAE/p95/max were 16.45/129/152 and 10.39/76/76,
failing the 8/4 quality limits. Both logs show one restart and no confirmed
image clock; the phasing lock periods were 2991/3009 rather than the nominal
3000 samples. Artifacts are `.../decodium-fldigi-radiofax-replay-jixfw8py`
and `.../decodium-fldigi-radiofax-replay-r9acy41q`; the modified inputs are
`/tmp/decodium-fax90-independent/short-drift-{a,b}.f32`. This result is
consistent with falling back to nominal image timing after losing the clock,
but the experiment does not isolate that as the only cause. Reusing a stale
estimate without signalling it or testing an actual rate change could produce
another wrong image; this case remains open. The resampling changes both
timing and audio frequency and is not an RF test.

The receiver now retains the last confirmed clock as a **provisional** raster
hint only if the candidate image begins within four line periods of its last
confirmed edge and no fresh estimate is available. The image-start anchor uses
that same edge. The initial tracker also expires its estimate after 4.5
periods without a pulse; previously that timeout applied only to restarted
trackers. A later independently confirmed estimate takes precedence. The
image summary exposes `imageRadiofaxClockProvisional`, and the receive page
shows a warning-coloured provisional-timing label. A sudden clock change
immediately before image start remains indistinguishable from a transient
disturbance in this short window, so the fallback may still be wrong; it is
not represented as a newly confirmed estimate.

The two failed drift recordings were replayed again on the updated full
receiver. Both reported one restart, provisional clock, 120 rows, stop and no
audio discontinuity; A/B MAE/p95/max were 2.043/2/15 and 1.935/2/7,
within the 8/4 quality thresholds. Artifacts are
`.../decodium-fldigi-radiofax-replay-sgo0djl5` and
`.../decodium-fldigi-radiofax-replay-mub87u7j`. Two separate clean 240 LPM
transmissions between full Decodium applications, using varying A/B patterns,
also reached complete coverage with stop and no discontinuities; MAE/p95/max
were 0.311/0/21 and 0.158/0/21, with confirmed rather than provisional
clocks. Artifacts are `.../decodium-radiofax-centered-a-yd_6fhbt` and
`.../decodium-radiofax-centered-b-ibzcl4sl`. Component A/B tests check the
short interrupted phasing, stale-clock expiry and changed-clock reacquisition;
an offscreen QML test checks the provisional warning and normal label. These
selected cases do not establish resilience to arbitrary rate changes, RF or
independent-decoder operation.
The runtime's 48 kHz transmit-audio, consecutive-image rearm A/B and direct
audio-stop chunk-size checks also passed after the change.

The immediate clock-change limitation was then exercised with full receivers,
not only the synthetic tracker. The shortened two-pulse A/B recordings above
were split at 19.08/18.05 seconds respectively; FFmpeg interpreted the first
segment at 48,144/47,856 Hz and the second at 47,856/48,144 Hz, resampling
both back to 48 kHz before concatenation. The cut occurs just before the
disturbed phasing line. Both detected stop and flagged their image clock as
provisional, but the replays failed: A published 121 lines with MAE/p95/max
17.40/127/253, and B published 119 with 11.66/78/78. Their terminal clock
estimates remained near the earlier period, 2991/3009 samples, respectively.
Artifacts are `.../decodium-fldigi-radiofax-replay-wej_tbmd` and
`.../decodium-fldigi-radiofax-replay-qzofgjqh`; inputs are
`/tmp/decodium-fax90-independent/short-switch-{a,b}.f32`. These deliberately
resampled and spliced recordings shift audio frequencies as well as timing;
the experiment does not isolate a physical source or prove RF behaviour.

`autoReceiverMarksUnconfirmedImmediateClockChangeAB` now documents the
corresponding observation-level limitation in both directions: a recent
pre-change clock can still be selected, but must be marked provisional rather
than newly confirmed. This regression intentionally checks truthful state;
it does not assert correct raster timing or claim that the defect is fixed.

The opt-in `DECODIUM_RADIOFAX_TRACE_LEVELS=1` offline acquisition diagnostic
now reports median high-confidence black and white observations before and
after the disturbance. On the two switched-rate recordings it measured
old/new black/white levels of 1504.49/2306.92 to 1495.49/2293.10 Hz for A,
and 1495.49/2293.13 to 1504.49/2306.89 Hz for B. If the entire frequency
scale is assumed to follow the line clock, `analyze_radiofax_level_scaling.py`
gives periods 3009.01 and 2991.03 samples, close to the intended switched
rates. On stable-clock controls the levels remain essentially unchanged.

That inference is not safe on its own. As A/B counterexamples, the same
shortened recordings were split just before the disturbance; the later
segment was pitch-shifted with FFmpeg `asetrate`/`aresample` and compensated
with `atempo` to retain approximately its original duration. The observed
recent black/white levels shifted from 1499.99/2300.03 to 1504.46/2306.89 Hz
and 1495.47/2293.14 Hz. The frequency-only hypothesis suggests line periods
2991.08 and 3009.04, although the final observed phasing-edge gaps were
approximately 2999 and 3001 samples. These are deliberately transformed
local audio counterexamples, not measurements of a particular transmitter.
They show why proportional tone changes alone must not automatically replace
the provisional clock. The diagnostic's synthetic A/B, stable-level,
common-frequency-offset and invalid-input tests pass. Inputs are
`/tmp/decodium-fax90-independent/short-toneonly-{a,b}.f32`.

After a clock restart, the receiver now prefers a **provisional** period from
three newly accepted consecutive phasing edges, if the last edge is within two
periods of the candidate. Missing-line gaps or a pending irregular pulse reject
this short estimate. A fully confirmed estimate still takes precedence; if no
short estimate is available, the bounded previous-clock fallback described
above remains in use. The short estimate is labelled provisional in the UI,
because three edges are less conclusive than the normal acquisition threshold.
It uses timing of phasing edges, not the tone-scaling hypothesis.

Full receiver A/B replays of the previously failing switched-rate files now
reported 120 lines, stop and zero audio discontinuities. A/B MAE/p95/max were
1.994/2/12 and 1.990/2/10. Both reported provisional fresh periods of
6018/2 and 5982/2 samples, rather than the earlier pre-switch periods;
artifacts are `.../decodium-fldigi-radiofax-replay-f3ech0w3` and
`.../decodium-fldigi-radiofax-replay-bhu6j_e2`. The component A/B regression
checks insufficient edges, missing edges and a pending irregular pulse.
Offline replays of the pitch-only counterexamples chose approximately 2999
and 3001 samples from edge timing, not the misleading tone-scale hypothesis.
This fixes these selected inputs, not the case where image start follows so
quickly that there are fewer than three valid new edges, nor arbitrary RF.
Two fresh clean 240 LPM A/B transmissions between full Decodium sessions also
passed after the change, with full coverage, stop, zero reported audio
discontinuities and MAE/p95/max 0.311/0/21 and 0.158/0/21. Artifacts are
`.../decodium-radiofax-centered-a-hh_xo_l9` and
`.../decodium-radiofax-centered-b-09ehx72l`.
The 48 kHz audio path, consecutive-image rearm and direct-stop runtime
regressions also passed with this version.

To exercise a shorter reacquisition, one additional phasing line was removed
from each switched-rate recording after its two false pulses. The cuts used
estimated 12 kHz edge positions 231326–234335 for A and 220229–223220 for B,
multiplied by four for the stereo 48 kHz recordings; the retained samples
were concatenated. Before the following change, the offline acquisition
diagnostic selected the older 2991/3009-sample clocks. These are edited audio
files, and the estimated cuts do not establish a standards-compliant TX.

Following a previously confirmed phase reference, two consecutive accepted
edges may now provide a provisional clock; without that reference the three
edge minimum remains. Missing periods, a pending irregular pulse, stale edges
and an out-of-range rate still reject the hint. This is less certain than the
fully confirmed clock and remains visibly provisional. Component A/B tests
exercise both rates, missing pulses, absent reference and an isolated edge.
Full Decodium receiver replays of the edited A/B audio detected stop and 120
lines without reported discontinuities; MAE/p95/max were 1.994/2/12 and
1.990/2/10. Artifacts are `.../decodium-fldigi-radiofax-replay-h054k65h`
and `.../decodium-fldigi-radiofax-replay-9fwy8c7y`. The modified input files
are `/tmp/decodium-fax90-independent/short-switch-fewer-{a,b}.f32`. These
selected cases do not guarantee accuracy with jitter or impulsive interference
and do not identify an abrupt change when only one reliable new edge exists.
Two fresh clean 240 LPM Decodium-to-Decodium A/B exchanges also passed, with
full coverage, stop and no reported discontinuities; MAE/p95/max were
0.311/0/21 and 0.158/0/21. Artifacts are
`.../decodium-radiofax-centered-a-anaxuw13` and
`.../decodium-radiofax-centered-b-co5zjdbm`.
The 48 kHz audio path, consecutive-image rearm and direct-stop runtime
regressions passed again after the two-edge change.

## One isolated false phasing pulse

After a clock has accumulated at least six confirmed pulses spanning eight
lines, the clock tracker now ignores one isolated pulse whose timing is
inconsistent with the established period. The pulse is not added to the
estimate; a following valid pulse confirms the existing clock, while a
second consecutive irregular pulse invalidates the tracker. Before an
estimate is established, an irregular pulse still invalidates it immediately.

The new observation-level A/B regression inserts a 5%-width false white pulse
at two positions in otherwise regular 120-LPM phasing. Before this change the
tracker became invalid; after it, the estimate remains within one sample of
the intended line period. Two early-pulse controls still reject acquisition,
and the existing sustained-irregular-period test still invalidates the clock.
The full Radiofax component suite passed.

Two full Decodium TX/RX pairs then exchanged A/B images over BlackHole at
IOC 576 and 240 LPM, detecting stop after 120 lines with no reported dropped
chunks, discontinuities or processing failures. A/B MAE was 0.172/0.106,
p95 zero; artifacts are `.../decodium-radiofax-centered-a-r6lb8we1` and
`.../decodium-radiofax-centered-b-1vcse52v`. Separately recorded audio was
copied with a 12.5 ms 2300 Hz pulse inserted into a confirmed 1500 Hz phasing
interval, in a different location for each capture. Byte comparisons
confirmed no other samples changed. Replays of these two modified recordings
through full Decodium RX recovered 120 lines and the same A/B MAE/p95, with
stop detected and clean audio diagnostics; each reported a 348,000/116
sample-per-line estimate. Replays are
`.../decodium-fldigi-radiofax-replay-jrxqezph` and
`.../decodium-fldigi-radiofax-replay-sbz3w2me`.

The impairment helper is `tests/sstv/tools/prepare_radiofax_phasing_glitch.py`;
its A/B, unchanged-sample, overwrite and silent-input controls pass. These
are selected recordings modified after capture, not RF, and the application
logs do not establish whether the demodulator classified the inserted tone
as the particular false pulse used by the component test. The recorded
successful replays therefore complement, rather than replace, the focused
regression or prove general resistance to interference.

The tracker now exposes its count of tolerated irregular pulses to the
test-mode terminal diagnostic. With the same receiver build, the two altered
recordings above each reported one ignored pulse, while separate replays of
their original unmodified captures each reported zero. All four produced
120 complete lines, detected stop, had no reported audio discontinuities or
dropped chunks, and retained A/B MAE 0.172/0.106 with p95 zero. The altered
A/B runs are `.../decodium-fldigi-radiofax-replay-u_gmi3i_` and
`.../decodium-fldigi-radiofax-replay-tya56_3_`; their respective unmodified
controls are `.../decodium-fldigi-radiofax-replay-cf7a0uft` and
`.../decodium-fldigi-radiofax-replay-w4uiknqj`. The helper can require a
zero or nonzero count with `DECODIUM_LAB_EXPECT_IGNORED_PHASING_PULSE=0` or
`1`. The counter records a count rather than a timestamp, and replay timing
can vary, so the paired result associates the inserted pulse with the
additional anomaly but is not sample-level proof of its classification.
These remain local virtual-audio recordings, not RF tests.

An additional A/B negative control inserts two 12.5 ms white pulses into the
same otherwise black phasing line of those recordings. The component test
requires that, once a clock was established, the first irregular pulse is
ignored and the second invalidates the tracker. In both full Decodium replay
tests the terminal diagnostics recorded one ignored pulse, an invalid clock
tracker and no retained measured clock; the receiver used nominal timing.
Each produced 120 complete lines and detected stop with no reported audio
discontinuities, dropped chunks or processing failures. A/B MAE was
0.1720/0.1050, p95 zero. Artifacts are
`.../decodium-fldigi-radiofax-replay-6tygkolm` and
`.../decodium-fldigi-radiofax-replay-cua_vw0u`.

The impairment helper supports `--two-pulses`; its automated A/B checks
verify that only the two requested PCM intervals change, along with overwrite
and silence rejection. `test_sstv_radiofax` and
`test_sstv_radiofax_phasing_glitch` passed. The test uses a nominal-clock
local recording, so it does not establish image quality after clock fallback
when an actual sender has sample-clock drift, nor does it establish RF or
independent-transmitter interoperability.

## Radiofax stop confidence bounds

The FM-observation stop detector and its short-window boundary refinement now
ignore observations with confidence above 1.0, consistent with the other
Radiofax acquisition stages. Previously such invalid observations could
contribute to a stop decision or boundary estimate. The A/B detector regression
uses 447.3 and 452.7 Hz signals with frequency offsets, noise and dropouts;
it verifies valid-confidence detection and rejects the same observations with
confidence 1.01. The regression failed before the detector change and passes
afterward, along with the full `test_sstv_radiofax` suite. This is a synthetic
invalid-input control, not an observation that a sound device produced an
out-of-range confidence value.

After rebuilding Decodium, two local TX/RX application pairs sent A and B at
IOC 576/240 LPM using BlackHole. Each RX was configured for 150 lines and
required to detect the stop after the transmitted 120 lines, preserving the
other 30 lines as uncovered. Both passed with no reported discontinuities;
A/B image MAE was 0.172/0.106 and p95 was zero. Artifacts are
`.../decodium-radiofax-centered-a-sixnm6hq` and
`.../decodium-radiofax-centered-b-iiznll4d`. These runs exercise normal audio,
not the synthetic invalid-confidence condition or RF interoperability.

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

After correcting the Receive page's obsolete instruction to restart RX,
both orders were repeated through full Decodium processes. A→B and B→A
each produced two complete 512×480 images with distinct acquisition IDs and
clean checked RX audio diagnostics. Image A/B component MAE was 0.110/0.065
and p95 was zero in both orders. Artifacts are
`.../decodium-fax480-consecutive-a-1f9iit40` and
`.../decodium-fax480-consecutive-b-bctj4day`. The offscreen receive-page
CTest also passed. The application transfer checks the existing automatic
rearm; it does not establish independent transmitter or RF interoperability.

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

### VIS leader dropout and short classification spikes

A further local consecutive VIS 85 run failed: only the second image began
acquisition before the receiver laboratory timeout. In its separately captured
first header, the 1900 Hz leader contained an approximately 10 ms interruption.
With the receiver's fast three-sample, one-sample-hop demodulator configuration,
the captured header did not acquire offline. An initial leader-only tolerance
recovered that capture with a slower demodulator but did not solve another
application run: brief classified-tone spikes split the leader around the gap.
These failed runs are `.../decodium-fax480-consecutive-a-7zgx0edh`,
`.../decodium-fax480-consecutive-a-3szka593` (interrupted after its first
image was missed), `.../decodium-fax480-consecutive-a-liqgynmm` and
`.../decodium-fax480-consecutive-b-e7itu2hs` in the macOS temporary directory.

The FAX480 VIS adapter now tolerates gaps shorter than 15 ms within a 1900 Hz
leader and requires a different classified tone to persist for one millisecond
before leaving the leader. Start, break, data and parity validation remain in
the shared VIS detector. Synthetic tests exercise dropouts and unclassified
observations in either leader, short classified spikes followed by a dropout,
longer gaps, missing break/data tones and wrong payload/parity. The synthetic
spike regression failed before the change and passed afterwards. The first
header from the failed B run was also acquired by an offline diagnostic with
the native resampler, preprocessor and fast demodulator after the change; the
separate capture is not guaranteed byte-identical to the application's input.

The leader-dropout observation test now covers eight positions across both
leaders, six dropout durations, one- and six-sample observation hops, and
both low-confidence and valid but unclassified observations. The expected outcome distinguishes a 20 ms gap
near the beginning of the first leader: the acquisition state resets, but
the remaining 260 ms is still accepted as a leader. Longer interruptions in
the other tested positions reject the first header, and a subsequent intact
header is acquired. These are synthetic observations, not measured audio or
proof that intermittent live acquisition has been eliminated.
The data-dropout test also exercises all seven VIS payload bits, including
wrong-bit controls, rather than only the first two bits.

Two fresh full Decodium sessions exchanged consecutive VIS 85 images over
BlackHole on macOS ARM64. A then B and B then A both produced two completed
images with distinct acquisition IDs, full coverage and no reported dropped
chunks, discontinuities or processing failures. Image A/B mean absolute errors
were 0.237/0.138 on the 0–255 scale, with p95 zero for both. Artifacts are
`.../decodium-fax480-consecutive-a-nhrzcako` and
`.../decodium-fax480-consecutive-b-1qbswlzx`. The lab now retains audio for
both headers and allows more time for two real-time transmissions. These
selected local results do not prove immunity to all interference, physical
radio performance or interoperability with independent applications.

A follow-up regression confirmed the tone-change candidate must be cleared
when an invalid or unclassified observation interrupts it; two separated
sub-millisecond 1200 Hz spikes must not be treated as a continuous VIS break.
The negative synthetic test passes. After that refinement, a further A then B
application run acquired only the first image (`.../decodium-fax480-consecutive-a-raej8val`);
that run did not retain the second header, so its exact cause is unknown. The
next runs with continuous capture passed A then B and B then A, with full
coverage, distinct IDs, zero reported RX discontinuities or dropped chunks,
and the same A/B MAE/p95 values 0.237/0 and 0.138/0. Their artifacts are
`.../decodium-fax480-consecutive-a-_q2sm0v5` and
`.../decodium-fax480-consecutive-b-baumsvqo`. This mix of results means
intermittent acquisition is not yet established as fully resolved.

The harness now checks each acquisition shortly after image transmission
starts, so a missed header ends the run early while retaining its capture.
A further captured A/B run with this guard passed, with the same image metrics
and clean RX counters; artifacts are
`.../decodium-fax480-consecutive-a-5lrk3dm1` and
`.../decodium-fax480-consecutive-b-ug28q28_`. Further captured repeats passed
the same checks: A then B in `.../decodium-fax480-consecutive-a-k872nf_9`
and `.../decodium-fax480-consecutive-a-tjirk62t`, and B then A in
`.../decodium-fax480-consecutive-b-4tszzoky`. The earlier unrecorded
second-header failure remains unexplained.

On 2026-11-03 the consecutive full-application test was repeated in both
orders with continuous BlackHole capture and receiver-side recent-audio
exports. A then B and B then A both completed with full coverage, distinct
acquisition IDs, and no reported dropped chunks, discontinuities or
processing failures. The four exported receiver recordings each contained a
recognised VIS 85 header. A/B image MAE was 0.237/0.138 and p95 was zero
for both, on the 0–255 scale. Artifacts are
`.../decodium-fax480-consecutive-a-dpbcvk28` and
`.../decodium-fax480-consecutive-b-rhn9idn3`. These extra passes provide
local repeatability evidence but do not identify the cause of the earlier
uncaptured failure or establish general RF and independent interoperability.

### Diagnostic export after a missed header

The Receive page now offers an explicit **Save recent audio** action. It
exports the bounded recent receiver buffer even if another image has already
been recognised, which can preserve audio following that image when a later
header is missed. The image-oriented export resolves the retained descriptor
by its generation-namespaced identifier rather than treating the image
identifier as the same value. An expired or unavailable descriptor falls
back to the recent buffer and is not labelled as that acquisition; the bridge
associates an exported recording with an image only while the image and
retained descriptor still match. The export is post-resampler,
post-preprocessing audio quantised to 16-bit WAV, not a bit-exact capture of
the original sound-card input or the demodulator's float samples. Retention
limits still apply.

Controller A/B regressions acquire two synthetic Martin M1 examples and
compare the image-only and recent exports, verify automatic descriptor
selection and generation/ID correspondence, and exercise expired-descriptor
and no-image fallbacks. The QML test exercises both save actions. Two local
Decodium Radiofax transmissions also exported recent audio after a completed
image; the WAVs were checked for 12 kHz mono samples, preamble-length audio,
non-silent signal and an unassociated acquisition ID. A/B image MAE/p95 was
0.172/0 and 0.106/0. Artifacts are
`.../decodium-radiofax-centered-a-56213b63` and
`.../decodium-radiofax-centered-b-eqmq3ssv`. The A WAV's signal check was
run separately; the B harness checks it directly. These are local virtual
audio controls, not an explanation of the intermittent missed VIS header or
evidence of physical radio interoperability.

### Captured missing VIS data symbol

A later A-then-B two-application run reproduced a missed second VIS 85
header. The receiver had completed the first image; it reported no dropped
chunks, processing failures or discontinuities, but no second acquisition.
The test retained its own postprocessed receiver audio for both headers in
`.../decodium-fax480-consecutive-a-2pfdb3lj`. A separate coarse spectral
diagnostic found VIS 85 in both recordings; it is not the receiver's decoder.

Four-second 12 kHz signed-16-bit excerpts of that receiver audio are retained
as `fax480-retained-vis-success.s16` (SHA-256
`722c5e97b6f9648499d36c3b545daf7f22e38f833bd328f4963b40e9d7eaff4d`)
and `fax480-retained-vis-missed.s16` (SHA-256
`6e112f1ccb4d16ce77c18ad397d16d6cb19b1e72ee797ebe0d455a0e4b755909`).
The native fast demodulator and FAX480 VIS adapter acquired the first but
not the second before the fix. Tracing the latter showed that an approximately
11 ms interruption and a brief classified spike split the first 30 ms data
symbol into separate events. The adapter now confirms tone changes for one
millisecond across symbols as well as the leader, and tolerates gaps shorter
than 12 ms within symbols; the existing VIS payload, parity and control-tone
checks remain in force. Both excerpts now acquire in the component regression.
Synthetic A/B controls cover two data bits, 8/10 ms gaps and a short spike;
16/30 ms gaps, wrong bits, wrong parity and missing control tones are rejected.
The separately exported PCM is quantised, so this reproduces the failure in
the selected offline path but does not prove the exact floating-point samples
or establish that every intermittent missed header had the same cause.

After the change, two Decodium application sessions sent A then B and B then
A in continuous reception over BlackHole on macOS ARM64. All four headers
were recognised and found by the separate coarse VIS diagnostic in the
receiver's retained WAVs; all four images had full coverage, distinct
acquisition IDs, no reported audio discontinuities or dropped chunks, and
A/B MAE/p95 of 0.237/0 and 0.138/0. Artifacts are
`.../decodium-fax480-consecutive-a-wh454u7a` and
`.../decodium-fax480-consecutive-b-ltt37zwy`. The test-mode harness can save
the receiver buffer at each header with `DECODIUM_LAB_FAX480_RETAIN_RX_AUDIO=1`.
As A/B negative controls, a VIS transmitter was paired with a Dayton-profile
receiver; both runs exited with the expected missing-acquisition error after
saving a WAV and metadata, and the coarse diagnostic found VIS 85 in each
(`.../decodium-fax480-consecutive-a-1ueufyxi`,
`.../decodium-fax480-consecutive-b-209w_9a5`). Those selected local tests
do not establish immunity to all dropouts, RF performance or independent
application interoperability.

The interrupted-symbol fix was then exercised through two full Decodium
application pairs on macOS, using a BlackHole 16-channel to 2-channel relay.
For each A/B image the relay inserted a 0.5 ms 1200 Hz spike followed by
10 ms of silence seven milliseconds into the first VIS data bit. Separate
source and altered relay captures confirmed that the affected original
segment was predominantly 1100 Hz, that all 480 dropout samples per channel
were zero, and that the 24 spike frames matched the requested waveform. The
relay reported no clipping, underflow or overflow. Both images completed with
100% reported coverage and no receiver discontinuities or dropped chunks;
A/B MAE/p95 were 0.237/0 and 0.138/0. App artifacts are
`.../decodium-fax480-two-app-libsstv-vis-480-iyjffggc` and
`.../decodium-fax480-two-app-libsstv-vis-480-gk2xot30`; relay artifacts are
`.../decodium-fax480-vis-dropout-a-z3fusvfd` and
`.../decodium-fax480-vis-dropout-b-rnqmgzn_`. The automatic capture check
ran within the B harness; the same validation was applied separately to both
recordings after extending it to check the spike waveform. Reproduce with
`python3 tests/sstv/tools/run_fax480_vis_dropout_two_app_lab.py a` and then
`b`. A/B negative checks modified one dropout sample in temporary copies of
the recorded relay audio; the validation rejected both. This is a controlled
virtual-audio impairment, not an RF or independent
application test, and does not establish recovery from every possible gap.

Another captured A/B repetition passed in both orders with the same MAE/p95
of 0.237/0 for A and 0.138/0 for B, full image checks and clean RX counters.
Artifacts are `.../decodium-fax480-consecutive-a-qn4hjb4p` and
`.../decodium-fax480-consecutive-b-gzzf6z0m`. These additional successes do
not identify the cause of the earlier unrecorded failure.

The first headers from the failed `a-7zgx0edh` and `b-e7itu2hs` runs are
retained as five-second, mono 48 kHz signed-16-bit little-endian fixtures
`tests/sstv/fixtures/fax480-vis-dropout-{a,b}.s16`. They are excerpts of the
separately recorded BlackHole signal, not samples captured inside the RX
application. SHA-256 A/B are
`1abdc704748ccb936b45ef490679179c5e970418bbf3170ef5ddbac23463c439` and
`45a9b2701a512cf0bc296e18ade295e27065e5e851051591716784b2d1562dac`.
`test_sstv_fax480` replays both through the native resampler, preprocessor
and fast demodulator, requires acquisition, and repeats each with the VIS
break silenced as a negative control. Both original and negative A/B cases
pass. This does not establish that the separately captured audio is identical
to the application's input or explain the later unrecorded missed header.

The `qsstv-500` convention was also exercised with consecutive images in
one local Decodium RX session. A then B and B then A both completed with
500 lines, distinct acquisition IDs and no reported processing failures,
dropped chunks or discontinuities. Image A/B MAE/p95 were 0.109/0 and
0.065/0 on the 0–255 scale. Artifacts are
`.../decodium-fax480-consecutive-a-b89xseqr` and
`.../decodium-fax480-consecutive-b-63p5sy96`. This is a Decodium-to-Decodium
virtual-audio test of the named profile, not independent QSSTV or RF evidence.
The `fax480ConsecutiveImagesRearmAB` runtime regression now also covers the
500-line profile alongside Dayton and VIS 85, and passed on macOS ARM64.

### Dayton FAX480 A/B with an independent codec

The separately maintained [Revenant](https://github.com/Locke-Werks/Revenant)
SSTV codec at pinned commit `67da9f8767b2a250792a6f5a5b2a69f5bc992b6d`
was compiled unchanged with the narrow laboratory adapter documented in
[AVT protocol notes](AVT_PROTOCOL.md). Its FAX480 profile uses 512×480 gray
pixels, a 5.12 ms sync and 262.144 ms scan. The adapter writes a PGM for
gray output. It invokes the codec, not the project's complete application.

Two detailed, generated grayscale A/B rasters were encoded and decoded by
the independent codec as self-controls. Both completed 480 rows; against
the input rasters, A/B MAE was 0.297/0.365, p95 0/0 and maximum error
63/37. The self-controls establish the selected codec and adapter path only.

In the external-TX-to-Decodium direction, the same generated audio files
were converted from mono to stereo and replayed at gain 0.8 through BlackHole
into full Decodium RX sessions configured for `dayton-480`. Both completed
with full reported coverage and zero reported discontinuities, dropped
chunks and processing failures. Direct A/B image MAE was 0.420/0.503, p95
0/0 and maximum error 79/82 against the original rasters. Artifacts are
`.../decodium-avt-interference-ydblgjxv` and
`.../decodium-avt-interference-xfcy98qz`.

In the Decodium-TX-to-external-RX direction, two Decodium TX sessions
transmitted the same A/B rasters and were captured through BlackHole. The
separately recorded stereo audio was converted to mono and the external
codec decoded complete 512×480 FAX480 images; direct MAE was 0.134/0.174,
p95 1/1 and maximum error 16/15 against Decodium's prepared references. TX
artifacts are `.../decodium-fax480-tx-dayton-480-lil3nwv8` and
`.../decodium-fax480-tx-dayton-480-hhi5g_8d`; generated audio and PGM files
are under `/tmp/decodium-revenant-avt-test`. The 8 MAE / 10 p95 gate was
used throughout. Earlier two-Decodium continuous RX A/B results are recorded
above. These are file-mediated codec comparisons and virtual audio, not live
GUI-to-GUI, WEFAX/HFFAX, other FAX480 conventions, physical radio or RF
evidence.

### Independent Isobar WMO radiofax investigation

The separate [Isobar](https://github.com/skgsara/isobar) WEFAX project at
commit `111bd184a962b82ebde72aa07a34f9905e5b2f89` was built from unchanged
C++17 CLI and core sources using
`tests/sstv/tools/build_isobar_oracle.sh CHECKOUT OUTPUT`. Its `demod-test`
passed for 1500/2300 Hz, and its `invphasing-test` passed batch and live
chunk controls on the project's own VMW off-air excerpt. The unmodified CLI
also produced images from its JMH and VMW sample WAVs. The sample provenance
is as described by that project; those runs do not provide independent
ground-truth images. The local unmodified CLI SHA-256 was
`f7c243d695fca0357e6cb45617c29c738a6bbba13a43ca46b6b36bbcadad4996`.

Two detailed 128×120 grayscale images were sent by Decodium using IOC 576,
120 LPM and the explicit 4.5% WMO dead sector. Two local Decodium receivers
detected the stop and reconstructed 120 lines with A/B MAE 0.306/0.359 and
p95 2/3, without reported discontinuities, dropped chunks or processing
failures. Their separately captured audio was converted to mono 22050 Hz
16-bit WAV for Isobar. Artifacts are
`.../decodium-radiofax-centered-a-uubgtgbx` and
`.../decodium-radiofax-centered-b-3g6a4g6_`; inputs and Isobar outputs are
under `/tmp/decodium-isobar-test`.

The unmodified Isobar CLI emitted 1500×262 and 1500×261 rasters including
the preamble. Its reported lock/corrected/coasted counts were 60/3/199 and
59/2/200. Estimating the image start as the first captured 10 ms RMS block
above 0.1 plus the nominal 35-second preamble gives row 87 in both outputs;
this estimate includes unknown VOX/capture timing and is not an exact anchor.
After resizing the full 1500-pixel line to 128 pixels, the unshifted
estimated-row comparisons gave A/B MAE 50.499/39.577 and p95 207/187.
These fail the earlier 8/15 gate, although the full-line comparison also
includes the 4.5% dead sector and should not be mistaken for a definitive
standard-aligned raster.

A separate **post-hoc diagnostic** searched rows ±12 around the estimated
row and circular horizontal starts in five-pixel steps, mapping 1433 of the
1500 output columns to the approximately 95.5% active image sector. Best A/B
MAE was 26.810/2.418, p95 175/4, at row 89 and columns 110/65. The A result fails
and the B offset was selected after seeing the image, so neither establishes
a direct passing interoperability test. `analyze_isobar_radiofax.py` performs
this diagnostic; `test_analyze_isobar_radiofax.py` covers synthetic A/B
full-line and active-sector controls and rejects silence. NumPy and FFmpeg
are laboratory dependencies, not application dependencies.

The original analysis mistakenly described the configured dead sector as 5%
and used 1425 active columns, yielding post-hoc A/B MAE 27.551/2.427. The
TX/RX mapper and UI actually use 4.5%; the 1433-column results above were
recomputed from the same original recordings. At a 1500-column output,
95.5% is 1432.5 columns, so 1433 is an approximation, not an exact sample
boundary or an independent standard-alignment proof. Repeating the same
post-hoc A/B search with 1432 columns gave MAE/p95 27.292/175 and 2.418/4,
at the same row 89 and columns 110/65; this sensitivity does not change the
conclusion that A fails and B is post-hoc aligned.

To explore tracking, a disposable isolated CLI build changed only the call
to `scan_lines` to disable tracking; the documented patch is
`tests/sstv/fixtures/isobar-no-tracking-diagnostic.patch` and the build
wrapper accepts `--no-tracking`. Its binary SHA-256 was
`e5bb0df900d9bd9f11aa35c51b594e567ce774e62a98389b47848a4d386863e2`.
On the same A/B WAVs, the corrected post-hoc active-sector search gave MAE
1.968/2.423 and p95 4/4, at row 89 and columns 1440/1375. It suggests
Isobar's tracking affects the observed A discrepancy, but the manually
selected offsets, separate receiver, resampling, partial/estimated
preamble timing and altered CLI do not establish a direct standard-aligned
pass or a Decodium or Isobar defect. The two original TX captures have not
been modified. RF and live GUI interoperability remain unverified.

A further disposable instrumentation of Isobar's line emission logged its
internal phase, state and inverted-phasing mode without changing the decode
logic. Its A/B output rasters were byte-identical to the unmodified CLI's.
The wrapper's `--trace-phase` option applies
`tests/sstv/fixtures/isobar-phase-trace-diagnostic.patch` to a temporary copy.
In the post-hoc selected image rows 89–208, phase changed seven times in A
and zero times in B; all those A changes occurred while the receiver reported
coasting in inverted-phasing mode. The source includes both measured-period
phase updates and content-based dropout correction in that mode, so these
logs alone do not attribute the changes to either mechanism. A second
instrumentation logged the nonzero return from the content-based dropout
correction: A produced corrections of +386, -476, +469 and -485 samples on
output rows 119, 134, 149 and 164; B produced none. Both instrumented
rasters were again byte-identical to the corresponding unmodified output.
This second diagnostic can be rebuilt with the wrapper's `--trace-lag`
option and `tests/sstv/fixtures/isobar-lag-trace-diagnostic.patch`; it patches
a temporary copy, not the checked-out external source.
This identifies the source of those four phase jumps in this experiment,
but does not establish whether the correction is appropriate for other
recordings or why these particular image features triggered it. Segment-wise
post-hoc searches on A showed large errors concentrated in the middle of
the image; with the diagnostic tracking disabled, all five segments had
MAE below 2.1 after individually selected horizontal alignment. That earlier
segment analysis used a 1425-column (95%) approximation, not the actual
4.5% dead-sector setting, and is retained only as a historical diagnostic. The trace
is additional evidence about this receiver and these captures, not a direct
interoperability pass or proof of a defect in either implementation.

#### Partial off-air observations

The same pinned Isobar checkout supplies two WAV excerpts described by that
project as off-air JMH and VMW receptions. They are different stations and
images, not a controlled A/B pair with identical conditions, and no original
image is supplied as ground truth. Each was resampled to 48 kHz stereo and
replayed over BlackHole into a separate full Decodium receiver on macOS. A
test-mode-only `DECODIUM_LAB_FAX_PARTIAL_SNAPSHOT=1` saves the latest partial
Radiofax image every five seconds; it does not require or imply an APT stop.
`tests/sstv/tools/run_radiofax_offair_observation.py` records the input hash,
profile, output, receiver logs and summary, and rejects continuity failures.

* `jmh-phasing-16k.wav`, SHA-256
  `72ad54738f3e0b7aea715c5c1fe87ba903e86323a8eef4ce199fbde69977c23a`,
  90 s, centered-phasing profile: one acquisition, 94 complete published
  lines, no detected stop; artifacts `.../decodium-radiofax-offair-_bes7pg9`
  and, after the final snapshot change, `.../decodium-radiofax-offair-q8b9u9dr`.
* `vmw-phasing-12k.wav`, SHA-256
  `e0d5c9b0b76fa8c47e53171c3d0d02451dafdbc25b4189857717f304f2ef3b6e`,
  62 s, WMO-dead-sector profile: one acquisition, 41 complete published
  lines, no detected stop; artifacts `.../decodium-radiofax-offair-am62csd1`
  and, after the final snapshot change, `.../decodium-radiofax-offair-mgvfwdth`.

Both snapshots contain visible content corresponding to the separate
unmodified Isobar decodes of the same WAVs, with preamble and horizontal
alignment differences. For each signal, the repeat's PNG was byte-identical
to the earlier snapshot. No objective unshifted image-quality gate or full
transmission was tested, and the recordings' RF origin is the external
project's attribution. An eight-second silent WAV with the WMO profile
produced no acquisition, no lines and no snapshot
(`.../decodium-radiofax-offair-klhzut0j`; repeated after the final snapshot
change in `.../decodium-radiofax-offair-jt0l95c_`). None of these tests verifies a
physical radio, general standards compliance or full interoperability.

`analyze_radiofax_partial_alignment.py` compares selected Decodium rows with
the Isobar raster after resizing it to 512 columns. For JMH rows 35–89 and
VMW rows 0–39, direct same-row MAE/p95 were 114.694/254 and 127.614/254.
Searching every candidate row and circular horizontal shift in four-column
steps after reception gave MAE/p95 14.348/54 and 19.569/140, at Isobar row
120/83 and shifts 476/492 respectively. Mapping 486 of 512 candidate columns
instead gave 31.398/109 and 14.776/95, showing sensitivity to the assumed
active sector. For the actual 4.5% VMW setting, 489 of 512 columns approximate
the 95.5% active sector and give post-hoc MAE/p95 19.446/145 at row 83 and shift
488 on the coarse search. Crossing the two images under the full-width search gave
MAE 108.181 and 69.928. These are comparisons between two decoders, not
ground truth; the chosen row ranges, post-hoc search, resampling, large
white areas and differing sector conventions limit their interpretation.
Even the best fits do not pass the earlier 8/15 image gate. Synthetic A/B,
crossed-image, active-sector and flat-input controls pass for the analysis
tool. NumPy, FFmpeg and FFprobe are diagnostic dependencies only; CTest
registers the synthetic check when its configured Python has NumPy.

The optional `--refine` control searches individual columns near the winning
coarse-row offset; it does not search every fine offset at every candidate
row. With full width for JMH and 489 active columns for VMW, this gives
row/shift 120/477 and 83/489, MAE/p95 10.903/38 and 9.241/44 respectively.
Both still fail the 8/15 gate and remain post-hoc comparisons. Synthetic A/B
tests with sub-step shifts verify that this local refinement can recover the
known shift; they do not validate the selected row on real recordings.
The corresponding aligned Pearson correlations are 0.971 and 0.941. These
describe association between the two rasters and do not identify a correct
decoder, remove the absolute-error failure or prove live interoperability.

With partial snapshots enabled, two additional Decodium-to-Decodium 576/240
transmissions of prepared images A and B verified that intermediate saves do
not replace the terminal image. Both had full coverage, no reported audio
discontinuities, and MAE/p95 0.172/0 and 0.106/0; the harness required both
partial-snapshot and terminal-save log entries. Artifacts are
`.../decodium-radiofax-centered-a-wkzo3inu` and
`.../decodium-radiofax-centered-b-0_x84kel`. The A log was checked separately
because its process was started just before the harness assertion was added.

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
image on an input discontinuity. After a completed frame, the receiver rearms
automatically as described in the consecutive-reception section above. Those
profiles require the start tone and a selected convention; clock/slant
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

The detailed Decodium TX to unmodified QSSTV 10.0 failure was repeated after
the earlier temporary captures had been cleaned up. Fresh generated A/B
patterns were transmitted through BlackHole, and Decodium's offline capture
decoder reported coverage 0.999996 for both and MAE 0.615/0.956. The pinned
unmodified QSSTV receiver again failed the original 8/10 image gate: A/B
MAE/p95/max were 5.202/24/240 and 4.210/25/225. Fresh capture directories
are `.../decodium-fax480-tx-qsstv-500-85krfe8n` and
`.../decodium-fax480-tx-qsstv-500-cbpky5qn`; QSSTV receiver directories
are `.../decodium-qsstv-exp-decode-af16f6_g` and
`.../decodium-qsstv-exp-decode-xj745ptf`. This repeat does not itself
reproduce the separate QSSTV self-loop control below or prove RF behaviour.

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

After the previous temporary files were cleaned up, the control was repeated
against the two fresh Decodium references above. A fresh isolated checkout of
QSSTV source revision `a84584248d3307bfa081362031ff20f365337633`, with only
`qsstv-experimental-10.0-signed-pcm.patch` applied, was built with installed
Qt and used solely to generate the A/B WAVs. Its binary SHA-256 was
`0b5f9be85f09208e60405fc46c64af45544e59080b227637240acdce26c719e6`.
The unmodified 10.0 release decoded both WAVs. Against the prepared images,
its own TX/RX result was MAE/p95/max 5.303/24/254 and 4.276/26/246. Without
alignment, comparing the unmodified receiver's Decodium-TX image with its
QSSTV-TX image gave 0.307/0/128 and 0.318/1/130. This reproduces the shared
receive-side discrepancy and argues against changing Decodium's standard
transmitter solely to improve this one receiver. It does not meet the 8/10
original-reference gate, and the locally patched transmitter is not the
unmodified release. The WAVs are `/tmp/decodium-qsstv-fax-self-{a,b}.wav`,
the receiver outputs are `/tmp/decodium-qsstv-fax-self-{a,b}`, and the
fresh build is `/tmp/decodium-qsstv-fax-self-build`.

A further external-only diagnostic tested whether horizontally rotating the
input by two pixels would safely compensate the QSSTV receive offset. Two
new images retained the detailed patterns but deliberately used contrasting
four-pixel strips on both edges. The signed-PCM-patched QSSTV transmitter
encoded each image both normally and with a two-column circular rotation;
the unmodified 10.0 receiver decoded all four WAVs. Against the original
images, the unshifted A/B full-image MAE/p95 were 7.01/26 and 5.98/37; with
rotated inputs they were 3.20/9 and 2.77/6. However, the four rightmost
columns still had MAE/p95 82.89/255 in both rotated cases, and the four
leftmost had 30.74/106. The full-image percentile therefore masks severe
edge loss. These results do not justify silently rotating or truncating
Decodium's standard transmission to make the aggregate gate pass. They are
offline independent-software diagnostics, not Decodium A/B application
tests. Inputs and waveforms are `/tmp/fax-edge-{a,b}*.{png,wav}` and
receiver images are `/tmp/fax-edge-decode-*`.

The same two edge-marked WAVs were decoded with two freshly built,
same-source QSSTV binaries: baseline and the existing experimental patch
that moves FAX480 RX sampling 12 samples earlier at 12 kHz. Against the
originals, baseline A/B full-raster MAE/p95 were 7.011/26 and 5.984/37;
earlier sampling gave 2.566/7 and 2.210/4. Nevertheless, the earlier RX
still had left-edge MAE/p95 22.59/98 and 22.58/98, and right-edge 28.55/120
for both. The baseline RX had edge p95 255 on both sides. In the examined
QSSTV state machine, `getPixels()` consumes the current sample and cannot
retrieve samples before entering the pixel state; moving target timestamps
alone therefore cannot establish complete edge recovery. This is source
inspection and a modified-independent-receiver diagnostic, not a change to
Decodium or a successful unmodified QSSTV gate. Outputs are in
`/tmp/fax-edge-{baseline,earlier}-{a,b}`.

The same fresh QSSTV-generated A/B WAVs were also replayed through BlackHole
into separate full Decodium receivers configured for `qsstv-500`. For these
detailed 512 x 500 references, Decodium received full coverage with no
reported dropped chunks, discontinuities or processing failures. Direct
A/B MAE/p95/max were 0.337/2/39 and 0.432/2/39; crossed-reference MAE was
74.842 and 74.849. Artifacts are
`.../decodium-qsstv-exp-app-0-2l30fvde` and
`.../decodium-qsstv-exp-app-1-8dinbl4c`. This is file-mediated audio from
the signed-PCM-patched independent transmitter, not an unmodified release
transmitter or live RF. The replay harness accepts `QSSTV_EXISTING_WAV` and
`QSSTV_REFERENCE_IMAGE` together and checks continuity throughout RX.

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
The pixel-coverage assertions were also applied to the WMO clock test: all
three components of every pixel in the 120 received rows must be covered,
while every pixel in the remaining 30 rows must be uncovered, and the frame
must be partial. A further two-application A/B run used IOC 288 at 240 LPM;
the terminal diagnostics passed the exact 120-of-150-row coverage checks, with
no reported audio discontinuities. Component MAE was 0.309 and 0.248,
respectively, with p95 zero for both images. Artifacts are
`.../decodium-radiofax-centered-a-pbee_25q` and
`.../decodium-radiofax-centered-b-_3rkkkzu`. Reproduce by setting
`DECODIUM_LAB_RADIOFAX_IOC=288` alongside the two variables above and passing
`a 240` or `b 240` to the laboratory script.
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

### Open acquisition issue: wide white phasing interference

`autoReceiverRejectsBroadFalseWhitePhasingAB` exercises a clock change in both
directions and repeated white interference spanning 25–90% of a phasing line.
The current receiver starts the image prematurely in both cases; these two
assertions are recorded as expected failures. Shorter 25–65% interference is
rejected in the same test. This is synthetic observation-level evidence, not
an audio or two-application proof.

A provisional detector that delayed a candidate and compared the following
line rejected repeated interference, but still accepted the final disturbed
line when normal phasing resumed. With clock displacement, the next candidate
window contained 63 white observations out of 500 in one false case, while a
valid end line produced 88 out of 500 in another case. A simple white-fraction
threshold therefore risks rejecting valid signals. The provisional detector
was removed; a solution needs to retain the original raster anchor while
distinguishing resumed phasing from legitimate image content, and must be
verified with real audio and two application sessions.

Subsequent development added confirmation over a one-line window searched
within one quarter line of the candidate, retaining the original candidate
for raster alignment. With at least 75% white in one such window, the
observation-level A/B test above now passes, as do controls with a noisy end
line and image rows resembling phasing. The earlier expected-failure statement
describes the pre-change receiver. This threshold does not guarantee rejection
of all longer or differently positioned disturbances, and the synthetic result
alone does not demonstrate performance on an audio path.

On the source build with this confirmation, centered IOC 288 / 240 LPM
ordinary audio passed between two Decodium sessions over BlackHole 2ch for
fixtures A and B. Both reported complete coverage and zero discontinuities;
mean absolute pixel errors were 0.1721 and 0.1059 respectively. These clean
audio runs are regression controls, not a demonstration of the broad
interference fix. The Radiofax core suite and the runtime audio, consecutive
image and direct-stop tests also passed. Artifacts for the two local app runs
are `.../decodium-radiofax-centered-a-obw6u6d2` and
`.../decodium-radiofax-centered-b-7vlsoyv8` in the macOS temporary directory.

An additional real-audio control replaced 65% of eight consecutive phasing
lines with 2300 Hz audio in each of the original centered A/B captures. The
helper `prepare_radiofax_phasing_glitch.py --broad-white` checks that every
replacement targets a black tone; its tests check the exact changed intervals.
With the preceding build, both modified captures caused premature reception
of 150 rows and no Radiofax stop. This exposed a second path: before a clock
was confirmed, candidate verification was bypassed. The receiver now applies
the white-line check during acquisition too; the observation-level early
interference regression passes at both clock offsets. The modified A capture
then recovered 120 rows with stop detected, zero reported discontinuities and
MAE 0.1721 (p95 0); its replay is
`.../decodium-fldigi-radiofax-replay-x3re9jy_`. These are recordings modified
after capture, not RF or proof for arbitrary interference.
The modified B capture likewise recovered 120 rows with stop detected, zero
reported discontinuities and MAE 0.1059 (p95 0); its replay is
`.../decodium-fldigi-radiofax-replay-0yt5rcj_`.

After enabling the confirmation during initial clock acquisition, clean
IOC 288 / 240 LPM A/B exchanges were repeated between two full Decodium
applications. Both achieved full coverage, detected stop and reported zero
discontinuities; MAE was 0.1721 and 0.1059 with p95 zero. Artifacts are
`.../decodium-radiofax-centered-a-_ginwv6x` and
`.../decodium-radiofax-centered-b-nej2v4mg`. The updated component and
impairment-helper CTests passed, as did the rebuilt runtime's Radiofax audio,
consecutive-image and direct-stop cases. These clean-signal regression checks
complement the modified-recording tests above; they are not RF evidence.
