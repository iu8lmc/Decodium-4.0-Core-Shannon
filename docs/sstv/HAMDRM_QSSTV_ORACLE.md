# QSSTV MOT data-group oracle

## A/2.5/high/64-QAM/short: wide-PPI application A/B

This profile has a 258-byte packet body. Two distinct JPEGs were padded
to 7,413 and 4,341 bytes, making the final 1,024-byte MOT segment 245 bytes;
with 11 bytes of MOT overhead, each final 256-byte group must be split because
PPI has an eight-bit useful-length field. Two Decodium application pairs
reconstructed the exact files over a BlackHole relay without added echo, and
the pinned unmodified QSSTV receiver independently decoded the same captures
to the same bytes. App artifacts are `.../decodium-hamdrm-two-app-a-o9iwhtjt`
and `...-b-t0tawh1n`, relay captures `.../decodium-hamdrm-two-app-echo-jbv7ydpf`
and `...-foh3zbhh`, and QSSTV outputs
`.../decodium-qsstv-drm-decode-slxw6wmj` and `...-6sr7uu5m`. QSSTV reported
51/34 good MSC CRCs and 0/1 bad CRCs for A/B; it identified the expected
`A/2.5/Hi/64/Short`. The source files are
`/tmp/decodium-hamdrm-a25-high-ppi-ab/{a,b}.jpg`. This covers selected virtual
audio, not RF or every profile and channel condition.

The long-interleaver variant was then checked using two different 1,269-byte
JPEGs padded so the last MOT group is 256 bytes and requires split PPI.
Two Decodium application pairs reconstructed each file exactly via a
BlackHole relay without echo; the unmodified pinned QSSTV receiver decoded
the same recordings byte-for-byte and identified `A/2.5/Hi/64/Long`.
Both QSSTV runs reported 21 good MSC CRCs and no bad MSC CRCs; `pkt_bad=5`
has the source-level interpretation and limits described below. Decodium
artifacts are `.../decodium-hamdrm-two-app-a-5rtihwdd` and `...-b-4s0hv4f7`,
relay captures `.../decodium-hamdrm-two-app-echo-vk5b9_x1` and
`...-1s5mscfv`, and QSSTV results `.../decodium-qsstv-drm-decode-7jdfzc_u`
and `...-8g3aay41`. The helper retained logs, output and SHA-256 provenance
while removing its reproducible intermediate WAVs. This is selected
file-mediated virtual-audio evidence.

## A/2.3/normal/64-QAM/short: wide-PPI application A/B

The registered A/2.3/normal/64-QAM profiles have 285-byte packet bodies.
Two different JPEGs were padded with trailing zeros to 7,428 and 4,356 bytes,
so their last 1,024-byte MOT segment contains 260 payload bytes. With the
11-byte MOT group overhead, that final 271-byte group requires splitting
across two padded packets because the PPI useful-length field is one byte.
Two full Decodium application pairs transferred these exact files via a
BlackHole relay without echo, and the pinned unmodified QSSTV receiver decoded
the same recorded signals to the exact source bytes. App artifacts are
`.../decodium-hamdrm-two-app-a-f2jxjx8t` and `...-b-7d_nwzhx`, relay captures
`.../decodium-hamdrm-two-app-echo-woql6350` and `...-l22f2pn9`, and QSSTV
outputs `.../decodium-qsstv-drm-decode-sfh16vxo` and `...-j0jy89sg`. QSSTV
identified `A/2.3/Lo/64/Short`, reporting 44/31 good and zero bad MSC CRCs,
with 22/13 `pkt_bad`; the counter interpretation and its limits are discussed
below. The source files are `/tmp/decodium-hamdrm-a23-ppi-ab/{a,b}.jpg`.
This is selected virtual-audio evidence, not RF or all-profile coverage.

## A/2.5/normal/64-QAM/short: selected bidirectional A/B

An A/B test with larger 6,481- and 3,673-byte JPEGs exposed a TX defect:
the latter failed with `HAMDRM PPI cannot represent the final packet fragment`
because a padded fragment exceeded the one-byte length field. The packetizer
now splits such a fragment into representable padded packets; the receiver
accepts a non-final PPI packet while retaining packet CRC, continuity and
padding checks. The independently inspected pinned QSSTV source handles PPI
independently of the last flag. A/B packet tests exercise 256- and 300-byte
fragments with continuity wrap and exact reassembly.
For both sizes, additional controls corrupt the non-final padded packet's
CRC or set non-zero fill with a valid recalculated CRC. The receiver rejects
each, refuses the following continuation and successfully reassembles a
fresh valid sequence; the waveform CTest passed with these controls.
The waveform suite also modulates and demodulates repeated 256- and 300-byte
MOT groups on A/2.5/normal/64-QAM/short, asserting that each uses two PPI
packets and that every recovered group matches exactly. This automated A/B
loopback complements, rather than replaces, the application and independent
QSSTV tests below.
The same automated A/B waveform check now covers all six wide-packet
profiles, including short and long interleaving: each uses 256 bytes and one
less than its packet-body capacity, explicitly verifies non-final PPI, and
checks exact recovery. This is native synthetic PCM, not independent QSSTV
or RF evidence for every combination.
An independent method-level oracle now passes 256- and 300-byte MOT groups
through the new non-final PPI split and the unmodified packet-reassembly
methods extracted from QSSTV upstream revision
`8c27d6d169d8c6c197eb47c2089870e39bc06a02`. Both payloads and metadata
matched exactly, including continuity wrap; the same run checked 24 existing
corrupt groups and 12 broken-continuity sequences. Reproduce with
`bash tests/sstv/tools/run_qsstv_packet_rx.sh`. This executes extracted
upstream methods in a shim, not an unmodified live application or RF link.
For each split-PPI A/B group, the oracle also sends a group with a damaged
MOT CRC wrapped in valid packet CRCs and a separate packet sequence with
broken continuity and a recomputed packet CRC. Both negative cases were
rejected for each variant by the extracted QSSTV receiver methods.
An adapter A/B regression also drains prepared 48 kHz audio for 255- and
256-byte groups. Both sources produce exactly their declared sample count;
the split case adds exactly one 400 ms frame. With the frame budget set to
the first case's exact length, the adapter accepts that case and rejects
the split case before starting audio. The adapter CTest passed.
The packetizer/reassembler regression also passed two boundary inputs for
each of the 72 registered profiles; six profiles have packet bodies wider
than 256 bytes and exercise the split explicitly. This is a packet-level
matrix, not independent waveform interoperability for every profile.

On the current macOS build both files were transmitted through BlackHole and
the pinned unmodified QSSTV 10.0 receiver reconstructed their original bytes
exactly (`.../decodium-qsstv-drm-decode-i027vxbo` and `...-oribnxrg`). Its
logs report 40/28 good MSC CRCs, zero bad MSC CRCs and 19/11 bad packets;
despite those packet reports each complete image matched. TX artifacts are
`.../decodium-hamdrm-tx-a-6mut4ots` and `...-b-l10p80s8`; the pre-fix
failed B log is `.../decodium-hamdrm-tx-b-dn4ts3m3`. Two Decodium
applications also transferred each JPEG exactly (`.../decodium-hamdrm-two-app-a-aamdi5tb`
and `...-b-c7mgeyxx`). The waveform, waveform-adapter and two independent
QSSTV fixture regressions passed. This establishes selected virtual-audio
interoperability, not the cause of QSSTV's packet reports or RF behaviour.

The pinned QSSTV source revision provides an important interpretation of
`pkt_bad`: `sourceDecoder::decode` increments it when `setupDataBlock`
returns false even for a packet whose CRC passed, and `setupDataBlock`
returns false for an intermediate block before the last block completes a
data group. In the official receiver's A/B logs, `crc_ok` equals
`hdr_seg + data_seg + pkt_bad` (40 = 14 + 7 + 19 and 28 = 13 + 4 + 11),
with zero bad CRCs. This is consistent with intermediate blocks contributing
to the counter. The release binary was not instrumented per block, so the
logs alone do not prove that every `pkt_bad` entry has that cause or rule
out another data-group failure.

Two Decodium application pairs transferred distinct 225-byte JPEGs exactly
over BlackHole (`.../decodium-hamdrm-two-app-a-pci9m8ki` and
`.../decodium-hamdrm-two-app-b-03ghp92o`). Separately captured Decodium TX
A/B signals were decoded exactly by the SHA-256-pinned unmodified QSSTV 10.0
receiver, which identified `A/2.5/Lo/64/Short` with 14/15 good MSC CRCs
and zero bad CRCs or packets. Captures are
`.../decodium-hamdrm-tx-a-leucxyk_` and `...-b-_9civj8_`; receiver outputs
are `.../decodium-qsstv-drm-decode-rlsfodsa` and `...-_8hkns8m`.

In the reverse direction, locally instrumented QSSTV source revision
`a84584248d3307bfa081362031ff20f365337633`, with signed-PCM correction and
hybrid instrumentation disabled, generated two 320x240 WAVs. Although the
CLI requested long, the pinned unmodified receiver identified both as short,
recovering distinct 4,998/4,992-byte JP2 files with 57 good and zero bad CRCs
or packets each. Full Decodium RX replayed each WAV over BlackHole and
reconstructed the corresponding reference exactly with clean audio diagnostics
(`.../decodium-qsstv-drm-app-a3zots1d` and `...-zu8rf7b_`). WAVs and logs
are in `/tmp/decodium-qsstv-a25-normal64-ab-xuqp2avt`; A/B WAV SHA-256 are
`6eeb4658587d2a367a674bc43fab926f05747d45d0e8c00a9efd9bacc7f205f8`
and `2af98aad31d34162e2314a5d0e714dd0c02bc4d903ed96ef99bb71bf297eaa64`.
This is selected file-mediated evidence, not an unmodified live transmitter,
all profiles, EasyPal or RF.

After the non-final PPI receiver change, the retained QSSTV-source A/B WAVs
were replayed again through full Decodium receivers. Both reconstructed the
same independently decoded 4,998- and 4,992-byte JP2 references exactly with
clean audio diagnostics (`.../decodium-qsstv-drm-app-qyrz8xas` and
`...-aaaz9hml`). The inspected upstream transmitter normally sets PPI for its
final packet, so these recordings are a compatibility regression and are not
claimed to contain a QSSTV-generated non-final PPI packet.

Two further Decodium A/B transfers on this profile used a BlackHole relay
with 1 ms echo at gain 0.27, deterministic noise at peak 0.01 and a 1700 Hz
tone at peak 0.01. Both 225-byte JPEGs were recovered exactly; the relay
reported no errors and carried 378,216/378,146 active frames. The pinned
unmodified QSSTV receiver decoded the recorded relay signals to the same
bytes and reported 15 good MSC CRCs, zero bad CRCs and zero bad packets for
each. Decodium app artifacts are `.../decodium-hamdrm-two-app-a-8w00finj`
and `...-b-m6fyuoak`; relay recordings are
`.../decodium-hamdrm-two-app-echo-phqzll5l` and `...-h2a4eo1y`, and QSSTV
results are `.../decodium-qsstv-drm-decode-_fsbl3_p` and `...-pgrky3ci`.
These are selected synthetic conditions and virtual audio, not RF or a
general noise and echo tolerance measurement.

## A/2.5/normal/16-QAM/long: local A/B and QSSTV receiver

Two full Decodium application pairs transmitted distinct 225-byte JPEGs over
BlackHole and recovered their respective input bytes exactly using
`hamdrm-A-2500-normal-16-QAM-long`. Artifacts are
`.../decodium-hamdrm-two-app-a-h_jbehov` and `...-b-2ybvxuzu`.
Separately captured A/B Decodium transmissions at that profile were decoded
by the pinned unmodified QSSTV-Experimental 10.0 batch receiver, which
reported A/2.5/Lo/16/Long and recovered each corresponding JPEG byte-for-byte;
crossed-image comparisons failed as intended. QSSTV reported 15/16 valid MSC
blocks, zero bad MSC CRCs, and one `pkt_bad` in each run. TX artifacts are
`.../decodium-hamdrm-tx-a-ucb2lsmt` and `...-b-wkvi1fl6`, with independent
receiver artifacts `.../decodium-qsstv-drm-decode-ng69woqs` and
`...-zu83m_pi`. The exact-zero diagnostic found no internal run in A's audio
and one 6144-frame run in B's; its cause and whether it is intentional were
not determined. QSSTV nevertheless recovered B exactly. This covers selected
file-mediated Decodium TX to QSSTV RX, not the reverse direction, RF, or all
HAMDRM profiles.

The reverse direction was also tested with two different 320×240 images.
The locally instrumented QSSTV source build at revision
`a84584248d3307bfa081362031ff20f365337633`, including the signed-PCM fix
and hybrid instrumentation (hybrid disabled), generated the WAVs. The
unmodified pinned QSSTV-Experimental receiver identified both as
`A/2.5/Lo/16/Long`, recovered distinct 4,998- and 4,992-byte JP2 objects,
and reported 60 good MSC CRCs, zero bad CRCs and zero bad packets each.
Full Decodium receivers replayed these WAVs over BlackHole and recovered
the respective reference byte sequences exactly, with clean RX audio
diagnostics. Receiver artifacts are `.../decodium-qsstv-drm-app-8h36hc96`
and `.../decodium-qsstv-drm-app-j4_fo083`; WAVs, logs and QSSTV references
are in `/tmp/decodium-qsstv-a25-normal16-ab-t7va6ptx`. The A/B WAV SHA-256
values are `8b7bc782e7e7f33ac330ee5160d0f389a5644cc26c542ce074850ed54694d2bc`
and `bd2ede5098e0d56ad4263b6c4faa5fd78954f6c274377ccfe225b38494c8269d`.
The unmodified receiver's binary SHA-256 is
`b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa`.
This is a file-mediated virtual-audio test using a modified transmitter,
not a live unmodified QSSTV transmitter or RF test.

The A/B WAVs above were also processed with FFmpeg `afreqshift` at -5 Hz
and +5 Hz respectively. The pinned unmodified QSSTV batch receiver recovered
the original JP2 references exactly, with 60 good MSC CRCs and no bad CRCs
or bad packets per file. Full Decodium receivers using the standard profile
also reconstructed the respective 4,998- and 4,992-byte objects exactly over
BlackHole with clean RX audio diagnostics. Their artifacts are
`.../decodium-qsstv-drm-app-w7ptjeqd` and `...-e6ccnlii`. The transformed
WAV SHA-256 values are
`f631ffd5e93575aed565517c40b95eb48e326e1c47036567b255278665392883`
and `57fa0c9a99c986a61b7c4044da351e9482aa7be6430cd4045b7f191ce0ed728d`;
QSSTV logs and WAVs are alongside the unshifted inputs above. FFmpeg's
filter also alters phase, so these selected cases are not a frequency-only
tolerance measurement and do not resolve the distinct A/2.3 failure below.

## A/2.3/normal/16-QAM/short: local A/B and QSSTV receiver

Two Decodium application pairs transferred the A/B 225-byte JPEG fixtures
byte-for-byte using `hamdrm-A-2300-normal-16-QAM-short`, with the laboratory
checking restoration of the macOS legacy monitor. Artifacts are
`.../decodium-hamdrm-two-app-a-r6cw8ezy` and
`.../decodium-hamdrm-two-app-b-2bvf3wo9`.

Two further A/B transfers through the 16-channel-to-2-channel virtual audio
relay at half gain and zero echo also passed, producing recorded audio in
`.../decodium-hamdrm-two-app-echo-umo7zmye` and
`.../decodium-hamdrm-two-app-echo-__h5ewde`. The pinned unmodified
QSSTV-Experimental 10.0 ARM64 batch receiver decoded both recordings to the
exact source bytes and reported `A/2.3/Lo/16/Short` (QSSTV's `Lo` label is
the native profile's normal-protection setting), 16 good CRCs and zero bad
CRCs per recording. Results are `.../decodium-qsstv-drm-decode-i2qpz06z`
and `.../decodium-qsstv-drm-decode-a7bwjmc7`. This is selected one-way,
file-mediated independent receiver evidence, not live QSSTV transmission,
EasyPal or RF interoperability.

For the reverse direction, the locally instrumented QSSTV source build at
revision `a84584248d3307bfa081362031ff20f365337633` generated A/B WAVs
using A/2.3/low/16 and its `long` CLI interleaver argument (which maps to
Short in this build). The source tree includes the signed-PCM correction and
hybrid instrumentation; the hybrid switch was not enabled. The unmodified,
SHA-256-pinned QSSTV-Experimental receiver identified both WAVs as
`A/2.3/Lo/16/Short` and produced 277-byte JP2 references with 51 good and
zero bad CRCs each. Full Decodium receivers replayed each WAV over BlackHole
and recovered its reference byte-for-byte with clean laboratory audio
diagnostics. Receiver artifacts are `.../decodium-qsstv-drm-app-67gk0_ea`
and `.../decodium-qsstv-drm-app-xs2b0a03`; inputs, QSSTV logs and references
are in `/tmp/decodium-qsstv-a23-normal16-ab`. A/B WAV SHA-256 values are
`a0607f3ef17416139143d78b1884d8d6e4473d1afade0fb5f049702bd309f2f3` and
`68d4776e9e51bc52278a174a9596bd122623766275780ae042f417ffce4db59d`.
This adds file-mediated reverse-direction evidence, not an unmodified live
QSSTV transmitter, EasyPal or RF test.

The same two QSSTV-source A/2.3/normal/16/short WAVs were attenuated to
0.35 amplitude with FFmpeg and replayed separately over BlackHole into full
Decodium receivers. Both reconstructed the respective 277-byte unmodified
QSSTV receiver references exactly, with clean RX diagnostics. The source
QSSTV transmitter was instrumented as described above. The attenuated WAVs
are `/tmp/decodium-qsstv-a23-attenuated-ab/{a,b}.wav`, SHA-256 respectively
`6010966db2ff7cab6fed2e11cb7ef0e71f10db4def0b4e8b254aee48e0e0fde5`
and `6690f63d2abc4c5feb14d0436a09a1e48c062761c4f90a5773943201bd8ae3b3`.
Receiver artifacts are `.../decodium-qsstv-drm-app-o7bu5xeo` and
`.../decodium-qsstv-drm-app-uklhm3hf`. This checks selected attenuated
recordings; it does not measure minimum sensitivity or live EasyPal/RF.

An additional independent frequency-offset A/B comparison exposes an open
receiver limitation. A later zero-shift control below shows this cannot be
attributed to frequency offset alone. FFmpeg `afreqshift` shifted the original A WAV by -5 Hz
and B by +5 Hz. The pinned unmodified QSSTV-Experimental 10.0 batch receiver
decoded both shifted files to the same 277-byte references, byte-for-byte.
Full Decodium application receivers replaying the same files via BlackHole
did not produce the expected images. In the A run the diagnostic snapshots
reported no delivered HAMDRM groups and no audio discontinuities, dropped or
rejected chunks, queue overflows or processing failures. This establishes
the outcome for these generated files, not the cause or a general frequency
tolerance limit. Shifted WAV SHA-256 values A/B are
`9a0c1eb48c3a73854bd2b7db9fd2f649ebf0b12cc5a33c0675fcc08d5e1f0bf8` and
`a0a841ec4365aaa018b133826ecb135f62bfcb2a08bd655c0bac0c0faf229d50`;
files are `/tmp/decodium-qsstv-a23-offset-ab/{a,b}.wav`. Decodium artifacts
are `.../decodium-qsstv-drm-app-zfaba8yk` and
`.../decodium-qsstv-drm-app-r271uclq`; QSSTV artifacts are
`.../decodium-qsstv-offset-oracle-xqvp9tom` and
`.../decodium-qsstv-offset-oracle-7xxpw_jv`. Investigate acquisition and
frequency estimation before treating this case as supported.

The bounded offline receiver reproduced the gap after both WAVs were
converted to 12 kHz mono PCM: unshifted A/B each yielded 54 validated groups
and an exact object, while shifted A/B yielded no frames or groups under
the normal pilot limits. A diagnostic trace now includes frequency estimates
for pilot-rejected frames. Their median estimates were -4.979 Hz for A and
+4.993 Hz for B, consistent with the applied shifts; these medians do not
prove every timing candidate is correct. Relaxing pilot limits to 0.30/0.50
or 1.0/0.70 allowed 54 FAC-valid frames per shifted recording but all 54
packets in each failed CRC and no groups were accepted. Raising the pilot
threshold alone therefore does not repair this case. Logs are under
`/tmp/decodium-qsstv-a23-offset-ab/`; this is an offline diagnosis, not a
change to production thresholds or a complete root-cause analysis.

The offline A/B recordings were also checked with the existing receiver
candidates and normal pilot thresholds. Flat dual timing and adaptive
multistage alone did not reconstruct either object; combining adaptive and
early timing with a half-sample or sinc fractional candidate reconstructed
both byte-for-byte. Full Decodium application receivers with the existing
operator-selected experimental adaptive dual/fractional timing option then
replayed the shifted recordings over BlackHole. Both matched their 277-byte
QSSTV references exactly, with no reported dropped/rejected chunks, queue
overflows or processing failures. Artifacts are
`.../decodium-qsstv-drm-app-mc9bdkph` and
`.../decodium-qsstv-drm-app-mm7qm7rn`. These checks show a usable opt-in
path for the two inputs, not which specific candidate was necessary in the
application, a correction to the default receiver, or live/RF interoperability.

Test-only delivery isolation subsequently distinguished the application
candidates. With adaptive equalization, flat and multistage retry enabled,
delivering only the early-timing receiver's CRC-valid groups reconstructed
both offset recordings byte-for-byte (`.../decodium-qsstv-drm-app-27y6lq19`
and `.../decodium-qsstv-drm-app-96c7c7fh`). Delivering only the fractional
receiver's groups reconstructed A (`.../decodium-qsstv-drm-app-tkzn6scg`)
but did not complete B (`.../decodium-qsstv-drm-app-et9o4pc2`), despite 31
fractional groups reported in each run. The early receiver reported 53/49
groups A/B. The receivers still ran together; the switches restrict group
delivery only. This isolates useful paths for these files, not the cause of
default acquisition failure or the cost and false-positive risk of enabling
multiple candidates by default.

An offline diagnostic `adaptive-early` option now runs just one adaptive
receiver with early timing and CRC-gated retries. It reconstructed all eight
selected inputs: original, FFmpeg-zero-filtered, FFmpeg ±5 Hz and FFT ±5 Hz,
each A/B. A test-only application switch
`DECODIUM_LAB_HAMDRM_SINGLE_EARLY=1`, restricted to test mode, HAMDRM lab and
the explicitly enabled adaptive estimator, then replayed the FFmpeg-zero A/B
recordings. A was exact (`.../decodium-qsstv-drm-app-z0fosjmy`) but B did
not complete (`.../decodium-qsstv-drm-app-9qwqppdn`); both showed zero audio
errors and delivered 49/39 groups. This shows that an offline match does not
establish robust single-receiver application behavior. The switch does not
alter ordinary sessions; the normal waveform adapter CTest passed.

The single-early laboratory flag now requires dual timing to be disabled, and
the replay harness rejects both laboratory dual-timing and operator-selected
dual-timing log markers when verifying this mode. Previously the operator
option and single-early flag could coexist while the log misleadingly implied
one receiver. After the guard, original unfiltered A/B QSSTV WAVs were
replayed through the application in single-early mode, both byte-exact with
clean audio diagnostics (`.../decodium-qsstv-drm-app-k6qghgg_` and
`.../decodium-qsstv-drm-app-pu7wc49v`); the native adapter CTest also passed.
This guards laboratory interpretation and does not enable automatic adaptive
reception or resolve the filtered B single-candidate failure above.

To test sensitivity to the 48-to-12 kHz sampling alignment, each FFmpeg-zero
recording was trimmed by 0, 1, 2 or 3 input samples before FFmpeg resampling
to 12 kHz mono. The single adaptive-early offline receiver reconstructed
all four A variants and B at offsets 0, 1 and 3; B offset 2 delivered 39
groups but did not complete. The failed single-receiver application B run
also reported 39 delivered groups. That correspondence is consistent with
an alignment-sensitive failure, but is not proof that the application used
the same sample phase: its actual PCM was not captured and resampling paths
are different. Controls and decoder logs are in
`/tmp/decodium-qsstv-a23-offset-ab/{a,b}-phase*-early.log`.

Using exactly the same 12 kHz phase-2 PCM, offline nominal adaptive decoding
reconstructed both A and B while early adaptive missed B; combining nominal
and early also reconstructed both. Single nominal-adaptive application
replays of the two zero-filtered files subsequently both reconstructed the
277-byte QSSTV references (`.../decodium-qsstv-drm-app-gk2dugk4` and
`.../decodium-qsstv-drm-app-yr_m0cyi`) using strict pilot limits, flat and
multistage CRC-gated retry and no dual timing. These app runs used new audio
captures, not identical PCM to the earlier single-early runs; they do not
establish which timing is generally superior or justify a default change.

A same-PCM offline matrix compared nominal adaptive, early adaptive and their
combined delivery on both zero-filtered recordings at each of the four
48-to-12 kHz input sample offsets. Nominal adaptive reconstructed 6/8 exact
objects, early adaptive 7/8, and combined delivery 8/8. Both nominal failures
were offset 0; the early failure was B at offset 2, which nominal recovered.
The two paths are therefore complementary on this bounded matrix. The
combined diagnostic admits only individually CRC-validated packets and MOT
groups, but this matrix alone does not measure sustained CPU cost or general
false-positive probability. Logs are
`/tmp/decodium-qsstv-a23-offset-ab/{a,b}-phase*-*-matrix.log`.

A sequential process-level cost comparison replayed the same original
QSSTV-source A/B WAVs through the ordinary receiver and through the existing
operator-selected adaptive dual/fractional receiver. All four application
runs reconstructed their 277-byte references exactly and passed the audio
diagnostic checks. Sampling the full Decodium process every two seconds gave
median CPU percentages of one core for normal/adaptive of 36.6/38.2 (A) and
34.4/37.4 (B); sampled maxima were 93.7/86.6 and 92.5/84.9 respectively.
Each run had 19 samples including startup and shutdown, and peak RSS ranged
from 511,680 to 515,248 KiB. Reports are
`.../decodium-hamdrm-measure-standard-ncfj2gen`,
`.../decodium-hamdrm-measure-adaptive-tk_mk47z`,
`.../decodium-hamdrm-measure-standard-jeo_ek93` and
`.../decodium-hamdrm-measure-adaptive-fl8o6bq3`. This coarse single-Mac,
sequential comparison is not a worker-only benchmark, worst-case channel or
evidence of adequate headroom on slower hardware.

Two additional 26-second A/2.3/normal/16/short application controls used
the operator-selected adaptive dual/fractional path: deterministic uniform
noise at peak 0.20 (NumPy seed 88123), and independently seeded noise at peak
0.20 plus a 1700 Hz tone at peak 0.10 (seed 99777). Neither replay delivered
groups or saved an image, and both reported zero audio discontinuities,
dropped/rejected chunks, queue overflows and processing failures. Artifacts
are `.../decodium-qsstv-drm-app-5w0lurqu` and
`.../decodium-qsstv-drm-app-k7sooo7o`; input WAVs are under
`/tmp/decodium-hamdrm-a23-negative/`. The lab's no-image check now explicitly
requires clean audio diagnostics too, so a broken capture is not treated as a
valid negative control. These are bounded synthetic inputs, not a measured
general false-positive rate or evidence for arbitrary RF interference.

The same process sampler compared ordinary and operator-selected adaptive
receivers on the FFmpeg zero-filtered A/B signals that exercise the phase
sensitivity. Each normal run delivered zero groups and no image, with clean
audio counters; each adaptive run reconstructed the 277-byte QSSTV reference
exactly with clean diagnostics. Median CPU percentages of one core for
normal/adaptive were 37.7/38.8 for A and 39.6/37.0 for B; sampled maxima
were 81.8/81.1 and 87.8/96.4 respectively. Each sequential run yielded 19
whole-process samples including startup and shutdown. Reports are
`.../decodium-hamdrm-measure-standard-n_d1dugx`,
`.../decodium-hamdrm-measure-adaptive-r58shush`,
`.../decodium-hamdrm-measure-standard-z23bfkfn` and
`.../decodium-hamdrm-measure-adaptive-eem33jvx`. These numbers are coarse,
single-machine measurements, and do not establish sustained worst-case cost
or a safe automatic-selection policy.

A local diagnostic prototype re-scored the nine sample positions within ±4
of the nominal acquisition candidate using the frequency-corrected
least-squares demodulator. On the same 12 kHz offline inputs, unshifted A/B
remained exact but both ±5 Hz cases still yielded zero frames and zero
groups with the strict flat/multistage path. This limited local re-scoring
did not resolve the problem and was removed; it does not rule out a broader
frequency-aware timing search or another cause.

A subsequent A/B control changes the interpretation of the ±5 Hz result:
applying the same FFmpeg `afreqshift` filter with **zero** frequency shift to
the original recordings also made the strict flat/multistage offline receiver
fail both objects (zero frames and groups). The unmodified QSSTV batch receiver
decoded both zero-shift filtered inputs exactly. Applying a second inverse
shift to the ±5 Hz files likewise failed in the strict offline receiver.
Thus the earlier experiment does not isolate a frequency-offset defect:
the filter's signal transformation, including possible phase effects, is a
confounder. Offline controls are `/tmp/decodium-qsstv-a23-offset-ab/{a,b}-zero.log`;
QSSTV controls are `.../decodium-qsstv-zero-oracle-qxcxyo5r` and
`.../decodium-qsstv-zero-oracle-n0c95i75`. Full-application zero-shift
controls and a frequency shift produced by an independently characterised
method remain to be run.

Full Decodium application controls have now replayed the two zero-shift
FFmpeg-filtered WAVs. Neither reconstructed an image; both reported zero
groups and zero dropped/rejected audio chunks, discontinuities, queue
overflows or processing failures. Artifacts are
`.../decodium-qsstv-drm-app-aeesl9de` and
`.../decodium-qsstv-drm-app-cyakyccw`.

An alternative offline frequency shift was then generated by NumPy FFT:
construct an analytic signal by doubling positive-frequency bins, retaining
DC/Nyquist, zeroing negative bins, inverse FFT, multiply by a complex 5 Hz
oscillator, take the real part and round/clip to signed 16-bit. As a control,
the real part before frequency multiplication was sample-identical to each
FFmpeg-decoded original. The -5 Hz A and +5 Hz B outputs were both decoded
byte-for-byte by the default Decodium offline receiver and by full application
replays via BlackHole, with clean audio diagnostics. Application artifacts are
`.../decodium-qsstv-drm-app-zow0la1s` and
`.../decodium-qsstv-drm-app-6lcqgabt`; WAV SHA-256 values are
`ca994fb6ab174733aebcc1de82679da5f3f867267f450252af23e1dcf5a655f2` and
`2f1e6d4533b1a1541e3eb0901e34816a534a54db9b28ede872d1fa1fdfc7c296`.
This supports ±5 Hz recovery for these FFT-shifted inputs and identifies a
distinct sensitivity to the FFmpeg-filtered signals; it does not characterize
the filter's exact phase response, prove generic frequency tolerance or RF.

An exploratory FFT ratio of each zero-shift FFmpeg output to its input,
using the strongest 10% of input bins from 500 to 2500 Hz, found a median
amplitude ratio near 1.0. A weighted linear-phase fit had residual RMS phase
of approximately 16.7/16.8 degrees for A/B; this whole-recording estimate
includes boundaries and does not independently characterise the filter's
transfer function. Under strict pilot thresholds, offline selective or
adaptive-multistage decoding alone produced partial objects; adaptive dual
timing reconstructed both. With the operator-selected adaptive dual/fractional
option, full Decodium application replays of both zero-shift-filtered files
also matched the 277-byte QSSTV references exactly and reported clean audio
diagnostics (`.../decodium-qsstv-drm-app-uc2rplrs` and
`.../decodium-qsstv-drm-app-3ec5621u`). The single flat receiver used at that
stage did not reconstruct these filtered inputs; later ordinary application
reception also runs independent automatic candidates, as described below.
Causality is not yet proven.

The filter itself was subsequently measured on two phase variants of nine
one-second pure tones from 500 to 2500 Hz. Excluding the first and last 0.2 s,
the measured amplitude ratios were approximately 1.00000 in both sets. The
unwrapped phase at 500/1500/2500 Hz was approximately -32.47/-252.21/-356.16
degrees in both sets; subtracting a linear phase fit leaves residuals of
about +43.53/-22.39/+27.50 degrees. Thus the zero-shift FFmpeg filter does
introduce frequency-dependent phase on these tones, independently of image
data. The reproducible diagnostic is
`python3 tests/sstv/tools/measure_hamdrm_zero_shift_phase.py` (requires NumPy
and FFmpeg). This verifies a property of the filter, not that it is the sole
cause of the receiver failure or representative of all radio channels.

An additional offline A/B experiment used FFmpeg's 1500 Hz second-order
all-pass filter on the original 12 kHz PCM, with Q values 0.5, 1, 2 and 4.
The analyzer used the normal 0.12 pilot-error and 0.30 corrupted-pilot
limits. At Q=0.5 both flat/multistage and adaptive dual timing reconstructed
both objects exactly. At Q=1 the flat path reconstructed A but not B (40/27
validated groups); adaptive dual timing reconstructed both. At Q=2 and Q=4
neither configuration reconstructed an object. Their nominal receivers recorded
495–554 pilot failures and zero frames and groups. The timing-hybrid
analyzer's frame statistics are those of its nominal path; object reconstruction combines validated
groups from both timing paths. These observations isolate a second,
phase-altering filter as a useful acquisition stress case; they do not prove
which part of its transient or phase response causes failure, and are not
full-application, independent-receiver or RF tests.

A separate A/B experiment applied the same all-pass parameters (1500 Hz,
Q=2) to the original 48 kHz stereo QSSTV-source WAVs, instead of the 12 kHz
mono offline PCM. The pinned unmodified QSSTV receiver reconstructed both
277-byte references exactly, reporting 27 good and 6 bad MSC CRCs for A,
39 good and zero bad for B, and zero bad packets for both. Full Decodium
receivers replayed these 48 kHz files through BlackHole: both the standard
receiver and the operator-selected adaptive dual/fractional option recovered
the exact references A/B, with clean RX audio diagnostics. Standard artifacts
are `.../decodium-qsstv-drm-app-hv4e67r6` and `...-bd0626dp`; adaptive
artifacts are `.../decodium-qsstv-drm-app-5z1fd9o0` and `...-484lb3in`.
Files and logs are in `/tmp/decodium-qsstv-a23-allpass-q2-ab`; A/B WAV
SHA-256 values are
`8528e8c430fd67d1fb256d9229bff384b011cca8dbeffa9e8faec06bfe6a50ac`
and `4afdf49b9b473953bab075e81dcc0f973a1a20083b6801e8d08505d16228cab2`.
These inputs differ in filter sampling rate, channel count and resampling
path from the failed 12 kHz offline inputs. Their success does not resolve
the earlier failure or establish general phase tolerance.

The source PCM and JP2 references are in `/tmp/decodium-qsstv-a23-offset-ab`
and `/tmp/decodium-qsstv-a23-normal16-ab/decoded-{a,b}`. For each variant and
Q, generate the input using `ffmpeg -f s16le -ar 12000 -ac 1 -i INPUT
-af allpass=f=1500:width_type=q:width=Q -f s16le OUTPUT`, then run
`build/tests/sstv/analyze_hamdrm_capture OUTPUT A 2300 16 0.12 0.3
REFERENCE short MODE normal`, with MODE `flat-multistage` and
`timing-hybrid`. The generated local controls are
`/tmp/decodium-hamdrm-allpass-{a,b}-{0.5,1,2,4}.s16`.

For the Q=2 pair, another offline control raised the pilot-error and
corrupted-pilot limits to 0.4 and 0.6. With each of selective,
adaptive-multistage and sinc-hybrid modes, both nominal receivers accepted
54 FAC-valid frames but all 54 packets failed CRC; neither object was
reconstructed. The combined sinc-hybrid output also contained no complete
object. Thus relaxing validation alone did not fix these signals. In one
trace the accepted A candidate started at sample 1290 rather than 1299 in
the unfiltered baseline; this observation alone does not establish the
correct timing or the cause of failure.

Two temporary diagnostic prototypes were removed after A/B offline tests.
Interpolating pilot magnitude and unwrapped phase instead of complex values
did not reconstruct either Q=2 or Q=4 object, with either normal or relaxed
pilot-error limits. Forcing the adaptive acquisition candidate nine samples
later also failed on Q=1, Q=2 and Q=4 for both objects, including Q=1 which
the existing adaptive dual-timing path had recovered. These are negative
results for these specific prototypes, not a general rejection of channel
estimation or a fixed statement about all timing offsets. Production pilot
limits and receiver behaviour were restored after the experiment.

The pinned, unmodified QSSTV-Experimental 10.0 receiver was then given the
same Q=2 A/B 12 kHz PCM, converted to 48 kHz stereo WAV with FFmpeg. It
identified both as A/2.3/Lo/16/Short and recovered both 277-byte reference
objects exactly; the first run reported 29/27 good and 5/5 bad MSC CRCs.
Artifacts are `.../decodium-qsstv-allpass-ab-zlabfa52/{a,b}`. A reproducible
repeat using `tests/sstv/tools/run_qsstv_allpass_oracle.py` also passed A/B
and rejected swapped reference files; artifacts are
`.../decodium-qsstv-allpass-ab-n_yyfsw2` and
`.../decodium-qsstv-allpass-ab-23jlrkn5/a`, respectively. Pass the two
`/tmp/decodium-hamdrm-allpass-{a,b}-2.s16` files followed by the matching
`/tmp/decodium-qsstv-a23-normal16-ab/decoded-{a,b}/*_1_drm.jp2` references.
QSSTV receives a resampled version, whereas the Decodium diagnostic receives
the 12 kHz PCM directly, so this comparison establishes successful independent
decoding of these recordings, not identical samples at both demodulators.
On the current build, the exact 48 kHz stereo WAVs from the independent
QSSTV run `.../decodium-qsstv-allpass-ab-n_yyfsw2/{a,b}/capture.wav` were
also replayed to full Decodium receivers over BlackHole. Both the standard
and operator-selected adaptive configurations recovered both 277-byte
references exactly with clean RX audio diagnostics. Standard A/B artifacts
are `.../decodium-qsstv-drm-app-y3n999ma` and `...-9mwrl_1u`; adaptive A/B
artifacts are `.../decodium-qsstv-drm-app-ybqydnwe` and `...-exv1ryer`.
The application receives audio after further virtual-device capture and
resampling, so this does not overturn or identify the cause of the 12 kHz
offline failure. Command results are in `.../decodium-qsstv-q2-current-fkulfux4`.
The pinned QSSTV source uses a multi-symbol two-dimensional pilot window;
Decodium's experimental estimator currently interpolates pilots within each
symbol. This is a candidate direction for investigation, not a proven cause
or permission to copy the reference implementation.

An experimental Decodium estimator now combines pilot gains over the symbols
of one frame. It aligns symbol phases using frequency-reference carriers,
averages gains at each observed carrier and interpolates between carriers.
Pilot validation removes the current observation from the corresponding
carrier average where possible, or uses neighbouring carriers; the common
phase estimate can still depend on frequency pilots, so this is not a fully
independent validation. FAC and packet/MOT CRC checks remain in place. The
option is disabled in default reception. The existing operator-selected
experimental adaptive reception also runs it as an additional independent
candidate; explicit HAMDRM test mode can select the temporal/flat pair with
`DECODIUM_LAB_HAMDRM_TEMPORAL_PILOTS=1`. It does not enable an automatic
production fallback.

Offline A/B controls recovered both clean QSSTV objects and both Q=1, Q=2
and Q=4 all-pass objects exactly with normal pilot limits. With the final
build, Q=2 A/B each produced 54 validated groups and exact objects, where the
flat receiver produced none. Before the final numerical guards were added,
full Decodium RX replays of the Q=2 A/B WAVs over BlackHole also recovered
the 277-byte references exactly with clean audio diagnostics; artifacts are
`.../decodium-qsstv-drm-app-qazrwx0m` and
`.../decodium-qsstv-drm-app-06845_si`. After those guards, two full Decodium
TX/RX pairs with the test estimator transferred clean 225-byte A/B images
exactly (`.../decodium-hamdrm-two-app-a-3207nqwm` and
`.../decodium-hamdrm-two-app-b-6giwi59n`). Their audio path did not include
the Q=2 filter.

Two seeded noise and noise-plus-tone full-application controls produced no
image or groups and had clean audio diagnostics with the estimator enabled
before the final numerical guards (`.../decodium-qsstv-drm-app-d7d09hi1`,
`.../decodium-qsstv-drm-app-1bgpun1g`). The waveform test now includes A/B
clean and synthetic Q=2 all-pass loopbacks plus two seeded noise controls;
the waveform and adapter suites passed after the guards. These bounded tests
do not establish behaviour on time-varying channels, all DRM profiles,
slow hardware, RF, or a general false-positive rate. The extra frame-wide
demodulation and allocations also need performance and robustness evaluation
before production use.

The candidate was compared on 72 clean native profile combinations and 144
synthetic echo cases: it recovered all 72 and 144 respectively, while the
flat receiver completed 120 of the 144 echo cases. Tests reject any
unexpected MOT group. A separate A/B synthetic time-varying gain and echo
test used robustness A/E and variation rates 0.1, 0.5 and 2 Hz. Both
estimators recovered all eight slower cases, but the temporal estimator
missed all four 2 Hz cases that flat recovered. This is a bounded synthetic
comparison, not a general fading-channel model.

Accordingly the laboratory-only temporal adapter also runs an independent
flat receiver and delivers its validated groups; it does not replace flat
reception. Two Decodium TX/RX pairs transferred A/B images exactly through a
2 Hz varying-gain and varying-echo relay, with no reported relay or audio
errors (`.../decodium-hamdrm-two-app-a-xqf772d6`,
`.../decodium-hamdrm-two-app-b-v0w5ixaw`; relay artifacts
`.../decodium-hamdrm-two-app-echo-0ik_dn94` and
`.../decodium-hamdrm-two-app-echo-fjuufj9y`). Their nominal temporal receiver
reported zero validated frames; successful reconstruction therefore came
from the parallel flat receiver. With both receivers enabled, full Decodium
replays of the Q=2 QSSTV A/B recordings also recovered both references
exactly (`.../decodium-qsstv-drm-app-qnllhlol` and
`.../decodium-qsstv-drm-app-nvopbh8p`). The paired paths increase work and
may deliver duplicate groups; throughput, long-running resource use and
broader channels require further evaluation before enabling them normally.

The ordinary and paired temporal receivers were sampled sequentially while
replaying the original clean QSSTV A/B recordings through full Decodium on
macOS ARM64. All four recovered their 277-byte references exactly and
passed the audio checks. Median whole-process CPU, as a percentage of one
core, was 36.25/36.55 for standard/paired on A and 35.85/37.00 on B;
observed maxima were 89.7/84.1 and 94.7/81.0. Peak RSS ranged from 514,112
to 516,928 KiB. Each run had 24 samples two seconds apart, including startup
and shutdown. This coarse, single-machine comparison does not bound worst
case cost or establish performance for long or busy sessions. Reports are
`.../decodium-hamdrm-measure-standard-w1tmzrbk`,
`.../decodium-hamdrm-measure-temporal-r14effyd`,
`.../decodium-hamdrm-measure-standard-ul_7gf31` and
`.../decodium-hamdrm-measure-temporal-_pny3z_3`.

Two full Decodium A/B transfers with both paths active also checked the
completed inbox while further carousel groups arrived. Both transferred 225
bytes exactly; the nominal trace group count increased from 9 to 17 after
save and all five later inbox snapshots in each run remained complete and
error-free. This exercises continued delivery, including possible cross-path
duplicates; the counters do not identify which repeated groups came from
which path. Artifacts are `.../decodium-hamdrm-two-app-a-o_eeh9b0` and
`.../decodium-hamdrm-two-app-b-dct8c_pz`. An earlier attempt was invalid
because the harness had not enabled the trace needed by its own check; the
harness now enables that trace automatically when the check is requested.

The paired test was extended to two different 320×240 JPEGs of 9,454 and
4,078 bytes, using E/2.5/normal/64-QAM/short. Two Decodium processes
recovered each file exactly over BlackHole. After save, the nominal trace
group count rose from 18 to 26 for A and 11 to 19 for B; all 23/20 later
inbox snapshots remained complete and error-free. Across recorded audio
diagnostics, dropped chunks, discontinuities, HAMDRM rejected chunks,
queue overflows and processing failures remained zero. Process observation
lasted approximately 150/120 seconds, including time after TX completed;
it is not a continuous multi-hour stress test or a measured CPU benchmark.
Artifacts are `.../decodium-hamdrm-two-app-a-vfgk7u3y` and
`.../decodium-hamdrm-two-app-b-0bauomar`, with inputs in
`.../decodium-hamdrm-temporal-large-w5z7qmhi`. The normal audio channel was
used; the test does not establish radio-channel interoperability.

The phase-sensitive comparison was then repeated live between Decodium TX
and RX processes. A test-only BlackHole relay applied a 1500 Hz, Q=2
second-order all-pass filter at 48 kHz, with 0.5 input gain and no echo or
added noise. With the paired temporal/flat laboratory receiver, both 225-byte
JPEGs were received byte-for-byte; the nominal temporal trace reached 17
groups. With the ordinary flat receiver, independent A/B runs completed TX
but delivered zero groups and no image. All four runs had zero reported relay
errors, no clipping, and no audio discontinuities, dropped/rejected chunks,
queue overflows or processing failures in the recorded RX diagnostics. The
two modes were run sequentially, so their actual audio alignment was not
identical. Successful application artifacts are
`.../decodium-hamdrm-two-app-a-pgyuzhnf` and
`.../decodium-hamdrm-two-app-b-jpl8j2w4`, with relays
`.../decodium-hamdrm-two-app-echo-wmc0s906` and
`.../decodium-hamdrm-two-app-echo-ycmmjxhm`. Checked flat controls are
`.../decodium-hamdrm-two-app-a-f9x1panl` and
`.../decodium-hamdrm-two-app-b-wjbu2cz2`, with relays
`.../decodium-hamdrm-two-app-echo-_qcxar9x` and
`.../decodium-hamdrm-two-app-echo-wlxy__iq`. Reproduce with
`DECODIUM_LAB_ALLPASS_Q=2`, `DECODIUM_LAB_ECHO_PROFILE=hamdrm-A-2300-normal-16-QAM-short`
and `run_hamdrm_two_app_echo_lab.py a short 0 flat`, repeated for B;
set `DECODIUM_LAB_HAMDRM_TEMPORAL_PILOTS=1` for the paired receiver, or
`DECODIUM_LAB_EXPECT_FLAT_NO_GROUPS=1` for the bounded flat negative control.
The two synthetic noise/noise-plus-tone A/B application controls were also
repeated with both paths active and produced no image or groups, with clean
audio diagnostics (`.../decodium-qsstv-drm-app-0uyeldez` and
`.../decodium-qsstv-drm-app-xm1tey4z`). These are selected local audio
experiments, not a general false-positive rate or RF interoperability proof.

The temporal estimator is now connected to the existing user-selectable
experimental adaptive RX, alongside its original candidates. This does not
change ordinary reception when the option is off. With the operator option
selected through the laboratory controller, two full Decodium A/B transfers
through the Q=2 all-pass relay reconstructed both JPEGs exactly and logged
validated temporal-candidate groups. Artifacts are
`.../decodium-hamdrm-two-app-a-r1h5s32_` and
`.../decodium-hamdrm-two-app-b-_p25tx_q`, with relays
`.../decodium-hamdrm-two-app-echo-dm95bmb9` and
`.../decodium-hamdrm-two-app-echo-9xi3z5kq`. The selected option also passed
two Decodium A/B transfers through the 2 Hz time-varying relay
(`.../decodium-hamdrm-two-app-a-d5t5iwjl` and
`.../decodium-hamdrm-two-app-b-hvcvo197`) and rejected the two existing
noise controls without delivering groups or images
(`.../decodium-qsstv-drm-app-zw_4v_qy` and
`.../decodium-qsstv-drm-app-xu6eihny`). All these runs passed their audio
diagnostic checks. QML toggle, controller and waveform-adapter CTests passed.
The application tests select the option through the controller, not by a
human clicking the QML control. The additional candidate increases processing;
the earlier two-path cost measurements do not quantify the selected option
after this change. Broader performance, channels and independent live peer
interoperability remain unverified.

The additional temporal receiver previously propagated its terminal errors
to the entire adaptive RX session. A bounded test-mode-only frame limit of
one reproduced this: the temporal receiver reached its limit and the
controller stopped RX even though other receivers were active
(`.../decodium-hamdrm-two-app-a-nsffi_dt`). A terminal status from this
optional candidate now logs the reason and removes that candidate for the
remainder of the session, while independent receivers continue. Their own
terminal failures still stop the session. With the same injected limit, two
Decodium A/B sessions received their 225-byte images exactly, observed the
candidate-stop log and had clean checked audio diagnostics
(`.../decodium-hamdrm-two-app-a-0hdw3f41`,
`.../decodium-hamdrm-two-app-b-53co5gl8`). The limit is available only with
test mode and `--lab-hamdrm`; use `DECODIUM_LAB_HAMDRM_USER_TIMING=1` and
`DECODIUM_LAB_TEMPORAL_FRAME_LIMIT=1` with `run_hamdrm_two_app_lab.py a` or
`b`. Without injected failure, selected adaptive A/B transfers through the
Q=2 all-pass relay still reconstructed exact images and reported validated
temporal groups (`.../decodium-hamdrm-two-app-a-s01cqee9` and
`.../decodium-hamdrm-two-app-b-w4td9t7_`). The waveform-adapter CTest passed;
the injected frame limit models that error path, not every possible failure.

The operator-selected adaptive receiver including the temporal candidate was
also exercised in two full Decodium A/B selective-repair sessions. The
virtual-audio relay muted three seconds of the initial transmission; RX then
requested the missing data using an over-audio BSR and received the repair
over audio. Both 2,725-byte files were reconstructed exactly, and the test
confirmed the temporal candidate in both applications. The relay reported
139,508/139,525 muted active frames and no relay errors; active RX diagnostics
had no discontinuities, dropped/rejected chunks, queue overflows or processing
failures. Artifacts are `.../decodium-hamdrm-repair-a-ayvpp18o` and
`.../decodium-hamdrm-repair-b-1ce4pnqc`, with relays
`.../decodium-hamdrm-repair-audio-loss-e8l_c9eo` and
`.../decodium-hamdrm-repair-audio-loss-1f2jeda6`. Reproduce with
`DECODIUM_LAB_HAMDRM_USER_TIMING=1 python3
tests/sstv/tools/run_hamdrm_repair_audio_loss_lab.py a`, then `b`. These
are local two-Decodium tests, not new QSSTV, EasyPal or RF repair evidence.

As a preliminary cost check after the added candidate, full-process samples
of the adaptive option on the same clean QSSTV A/B inputs had median CPU
39.1/35.7 percent of one core, maxima 87.6/84.6 and peak RSS
527,728/522,272 KiB. Both references were reconstructed exactly. Each run
had 24 samples two seconds apart, including startup and shutdown; artifacts
are `.../decodium-hamdrm-measure-adaptive-ljvmbk8w` and
`.../decodium-hamdrm-measure-adaptive-9q010nlq`. The earlier separate flat
runs on these inputs measured median 36.25/35.85 percent and peak RSS
514,112/516,144 KiB. These coarse, non-simultaneous measurements on one Mac
do not bound worker cost, weaker hardware or sustained worst-case load.

## Default CRC-gated multistage packet retry

The native application RX adapter now enables the existing multistage MSC
retry by default, with normal flat equalization and unchanged pilot limits.
It runs only after the ordinary hard and soft packet decoders fail CRC; the
alternate packet is admitted to reassembly only after its own CRC succeeds.
The separate adaptive equalizer and additional timing candidates remain
operator-selected. The codec-level option still defaults off when the codec
is used directly, preserving explicit test controls.

On macOS ARM64, a two-Decodium A/short transmission through a 1 ms echo at
gain 0.27 completed TX but did not reconstruct its image with the previous
ordinary receiver: it reported 18 packet CRC failures and zero groups. The
relay reported no errors; capture and logs are
`.../decodium-hamdrm-two-app-echo-td4g8d3l` and
`.../decodium-hamdrm-two-app-a-vcfapoz_`. With the previously available
test-only flat multistage option, replaying this recording through BlackHole
reconstructed the 225-byte JPEG exactly, with 18 multistage recoveries
(`.../decodium-qsstv-drm-app-r57jktvc`). After enabling the retry in the
native adapter and rebuilding, the ordinary application replayed the same
recording and again produced the exact bytes
(`.../decodium-qsstv-drm-app-uzyq4f8u`). Audio conversion and replay can
change alignment, so the before/after evidence includes that limitation.

The separate B/long baseline already passed without retries. Its recording
also replayed byte-for-byte with the new default
(`.../decodium-hamdrm-two-app-echo-rbfl1dd3` and
`.../decodium-qsstv-drm-app-1agmu3q2`). Fresh two-Decodium A/short and
B/long transmissions with the same echo then produced exact 225-byte images,
with 10 and four retry recoveries respectively; both checked zero audio
discontinuities, dropped/rejected chunks, queue overflows and processing
failures, plus legacy-monitor restoration with fresh PCM. App artifacts are
`.../decodium-hamdrm-two-app-a-4lntux_g` and
`.../decodium-hamdrm-two-app-b-2gfxy0an`; relay captures are
`.../decodium-hamdrm-two-app-echo-8ab7j850` and
`.../decodium-hamdrm-two-app-echo-mxyn4_id`.

Two full-application 12-second noise controls, independent seeds and peak
0.18, produced no groups or images and reported no audio errors
(`.../decodium-qsstv-drm-app-16bi5wz3` and
`.../decodium-qsstv-drm-app-pb8s37g0`). The waveform and native-adapter
CTest targets passed. This selected local evidence does not measure the CPU
cost of sustained CRC failures on slower systems, establish a general false
positive rate or demonstrate live EasyPal or RF interoperability.

The earlier flat-fallback laboratory comparison depended on multistage retry
being disabled in both arms. To preserve that comparison after the default
change, the receiver recognizes `DECODIUM_LAB_HAMDRM_DISABLE_MULTISTAGE=1`
only when Qt test mode and both `--test-mode` and `--lab-hamdrm` are active.
The test checks the explicit log for the disabled baseline; ordinary sessions
do not use this override. Repeating the two-application-receiver laboratory
comparisons for short and long interleaving produced the expected missing-group
baseline and recovered both synthetic groups when flat fallback was enabled.
Artifacts are `.../decodium-hamdrm-fallback-ab-rjsjasb7` and
`.../decodium-hamdrm-fallback-ab-q9r5queq`. These are synthetic body-group
replays into one Decodium receiver, not two live transmitting applications.

The two recorded A/short and B/long echo signals were also replayed with the
default retry while sampling the complete Decodium process every two seconds.
Both images matched exactly with no reported audio errors. On this Mac, A/B
median CPU was 34.8%/35.0% of one core, maximum sampled CPU 86.6%/87.1%,
and maximum sampled RSS 505,728/503,600 KiB over 27 samples each. Samples
include startup, idle and shutdown. Reports are
`.../decodium-hamdrm-measure-standard-lihkjoti` and
`.../decodium-hamdrm-measure-standard-jjje6329`; RX artifacts are
`.../decodium-qsstv-drm-app-gx44f2ry` and
`.../decodium-qsstv-drm-app-biclx6ba`. This is not a before/after CPU
comparison or a sustained worst-case test on slower hardware.

For a more targeted sustained-CRC control, the new
`export_hamdrm_crc_stress` diagnostic generates two equal-length HAMDRM
signals with matching FAC and pilots but different MSC data, then averages
them. This is an artificial collision, not a model of arbitrary RF. Its
24.0-second Short and 25.6-second Long cases use E/2.5/normal/64-QAM. In
each of four full Decodium RX replays (two interleaver depths, multistage
retry enabled and test-only disabled baseline), the trace recorded 60 packet
CRC failures and no validated groups or saved images. Seven diagnostic
snapshots in each run reported zero discontinuities, dropped/rejected chunks,
queue overflows and processing failures.

The same Mac process sampler reported median CPU percentages of one core
of 36.4/36.7 for Short with retry on/off and 34.9/35.75 for Long on/off;
maximum sampled values were 83.1/85.8 and 91.3/86.1 respectively. Each run
had 17 or 18 samples taken two seconds apart, including startup and shutdown.
This coarse, sequential comparison does not establish that the retry is
free, measure only the decode worker, or predict performance on slower
hardware. Reports are `.../decodium-hamdrm-measure-standard-ydtjwqgi`,
`.../decodium-hamdrm-measure-standard-91xu5xlx`,
`.../decodium-hamdrm-measure-standard-a3fymo03` and
`.../decodium-hamdrm-measure-standard-o4zcm6o_`; RX logs are
`.../decodium-qsstv-drm-app-gz6z4na3`,
`.../decodium-qsstv-drm-app-haxchr5o`,
`.../decodium-qsstv-drm-app-qye58k10` and
`.../decodium-qsstv-drm-app-o67v3e0d` respectively. The new
`test_hamdrm_crc_stress` regenerates both inputs and checks at least 50 CRC
failures and zero groups with retry both off and on; it passed locally.
This is a signal-injection test into one receiver, complementing the live
two-Decodium A/B transfer above; it is not live EasyPal or RF evidence.

## B/2.3/high/4-QAM/short: two applications and QSSTV replay

On 2026-11-03, two independent A/B runs sent the 225-byte image fixtures
between two Decodium sessions on macOS using the B/2.3/high/4-QAM/short
profile. Audio was relayed from BlackHole 16ch to BlackHole 2ch at half gain,
without echo or added noise. Both received files matched their respective
fixtures byte for byte; the harness also verified legacy-backend restoration.
All seven receiver diagnostic snapshots in each run reported zero audio
discontinuities, dropped chunks, rejected HAMDRM chunks, queue overflows and
processing failures. The application artifacts are
`.../decodium-hamdrm-two-app-a-t3e4colf` and
`.../decodium-hamdrm-two-app-b-dcbsuhh9`; the recordings are in
`.../decodium-hamdrm-two-app-echo-m7kk0r6y` and
`.../decodium-hamdrm-two-app-echo-q6xp_7a2` respectively.

The unmodified QSSTV-Experimental 10.0 receiver (executable SHA-256
`b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa`)
decoded each recorded signal offline, identified `B/2.3/Hi/4/Short`, and
recovered the exact corresponding image. Its counters were A: `crc_ok=18`,
`crc_bad=0`, `pkt_bad=3`, and B: `crc_ok=16`, `crc_bad=0`, `pkt_bad=3`.
The QSSTV counter `pkt_bad` also counts incomplete packet fragments, as
discussed below; these runs do not establish three corrupted packets. The
QSSTV logs reported duplicate bundled/Homebrew Qt framework warnings.
Artifacts are `.../decodium-qsstv-drm-decode-elnsk8b1` and
`.../decodium-qsstv-drm-decode-myiz4upx`. This verifies selected recordings,
not a live QSSTV peer, an RF link or general EasyPal interoperability.

For the reverse direction, the locally built QSSTV source at revision
`a84584248d3307bfa081362031ff20f365337633` generated two 26-second WAVs
from the different fixtures, scaled to 320 x 240. This build includes the
signed-PCM fix and additional hybrid-test instrumentation; the hybrid test
environment switch was not set. Because this revision's CLI interleaver
mapping is inverted (see below), `--drm-interleave long` produced Short.
The unmodified receiver identified both as `B/2.3/Hi/4/Short` and decoded
their 277-byte JP2 objects, reporting 49 good CRCs, zero bad CRCs and zero
bad packets for each. These JP2 objects are the reference for the replay;
they are recompressed images, not copies of the source JPEGs.

Full Decodium receivers replayed both WAVs over BlackHole 2ch and produced
the respective reference JP2 objects byte for byte. Eight diagnostic
snapshots per run reported zero discontinuities, dropped chunks, rejected
HAMDRM chunks, queue overflows and processing failures. The receiver
artifacts are `.../decodium-qsstv-drm-app-mn67qgfe` and
`.../decodium-qsstv-drm-app-mvboaj4y`; TX, unmodified-receiver logs and
reference objects are in `/tmp/decodium-qsstv-b23-short-ab`. The A/B WAV
SHA-256 values are
`3208bd060bedb00307a08cba6868c8b7406702821de9b35b1ef5306ca3ea9451` and
`005c5b33be17b3948c487d0f6e2bcc74069ddd07cb17454b6452872a4edbf471`.
This is file-mediated interoperability using an instrumented transmitter;
it does not establish operation with the unmodified transmitter or RF.

The replay laboratory now requires clean audio diagnostics for every normal
successful image replay, not only selected experimental or BSR paths. It
rejects absent diagnostics, the wrong input device, missing counters and any
nonzero discontinuity, dropped-chunk, rejected-chunk, overflow or processing
failure counter. A/B unit controls exercise each rejection. Repeating the
above A/B recordings passed the exact-byte and diagnostic checks; artifacts
are `.../decodium-qsstv-drm-app-jetfslf9` and
`.../decodium-qsstv-drm-app-j6yni9zf`. The A process used the equivalent
inline check before it was extracted into the shared validator; its captured
log also passes the final shared validator. These checks do not turn the
file-mediated transmitter into a live or unmodified QSSTV sender.

## B/2.5/normal/4-QAM/short A/B

On 2026-11-04, two isolated Decodium application pairs sent the distinct
225-byte JPEG fixtures over BlackHole with `hamdrm-B-2500-normal-4-QAM-short`;
both receivers reconstructed the appropriate image byte-for-byte. Artifacts
are `.../decodium-hamdrm-two-app-a-xsucsvyh` and
`.../decodium-hamdrm-two-app-b-4tk8tsbw`.

Separate full Decodium transmitter runs were captured over BlackHole and
fed to the pinned unmodified QSSTV-Experimental 10.0 batch receiver. QSSTV
reported `B/2.5/Lo/4/Short` and recovered both input JPEGs byte-for-byte;
each run reported 18 good and zero bad MSC CRCs. Its `pkt_bad=3` counter
includes incomplete fragments as discussed elsewhere in this document.
TX captures are `.../decodium-hamdrm-tx-a-nv9av2yi` and
`.../decodium-hamdrm-tx-b-ysrxjfbr`; QSSTV outputs are
`.../decodium-qsstv-drm-decode-rb1ra160` and
`.../decodium-qsstv-drm-decode-05g_kyy1`. Capture SHA-256 values are
`1278db9d20430416231561780fb1f7d98e71aa1a3d68786a3ebce037e0cea2de` and
`ae8398be213b59d8068a44c929a5648ea33d019969942dedecffaed583a279da`.
These are selected small-image tests in one direction through a recorded
peer, not live EasyPal, reverse-direction, arbitrary-profile or RF evidence.

For the reverse direction, the unmodified official QSSTV-Experimental 10.0
binary encoded two distinct 320x240 JPEGs using B/2.5/low/4-QAM. Its CLI was
requested with `--drm-interleave long`; the receiver identified Short, consistent
with the documented CLI inversion. Encoder A exited normally and wrote a WAV.
Encoder B remained running after 91 seconds and was interrupted; the existing
RIFF repair helper corrected its stale container length without changing PCM.
The unmodified QSSTV batch receiver decoded both recordings as
`B/2.5/Lo/4/Short`, with 120 good and zero bad MSC CRCs and `pkt_bad=0` each.
Its distinct outputs are 4,998 and 5,024 bytes; FFprobe identified both as
320x240 JPEG2000. Full Decodium receivers replayed the recordings over
BlackHole and recovered the respective QSSTV reference bytes exactly, with
clean RX audio diagnostics and both automatic receiver candidates enabled.
Decodium artifacts are `.../decodium-qsstv-drm-app-ozsrprdk` and
`.../decodium-qsstv-drm-app-thgib7ql`; the independent WAVs, references and
logs are under `/tmp/decodium-qsstv-b25-4qam-ab`. The A WAV SHA-256 is
`a0559ebe1c00dbc1c392ae4021e05827fb0c63cb963009312c32e80441a0421e`;
B's original and RIFF-repaired WAV hashes are
`1d03f02ffc5d09c0c6c3c22b4422a849e03183f8398aaebf20d779a7beed0692` and
`a76c990273bb04040c86b924f8ef91897a25469e420e33a4139642b7cffc35cf`.
This establishes selected file-mediated QSSTV-to-Decodium A/B reception on
the same named profile, with B's interrupted encoder explicitly qualified;
it does not establish live EasyPal exchange or RF behaviour.

## FAC interleaver acquisition

On 2026-11-03 the receiver was changed to use the CRC-validated FAC
interleaver depth while still requiring the other configured FAC parameters
to match. The waveform test exercises both mismatch directions, two distinct
groups, repeated depth changes on one receiver, reset and rejection of a
different constellation. Two Decodium sessions over BlackHole 2ch delivered
image A byte-for-byte with TX long/RX initially short and image B with TX
short/RX initially long. Their local artifacts are
`.../decodium-hamdrm-two-app-a-be41d5p5` and
`.../decodium-hamdrm-two-app-b-yncsxm2z`.

The previously captured official QSSTV B transmission described below was
also replayed over BlackHole to Decodium initially set to A/2.3/high/4/short;
the received 225 bytes matched the independent input exactly. Artifacts are
`.../decodium-qsstv-drm-app-syh8uxtz`. This is a replay of one captured
signal, not a new live QSSTV transmission or a physical RF test.

The waveform regression now also interrupts a long-interleaver transmission
after two frames, inserts a silent frame, and verifies that the following
short-interleaver group is the only group delivered, for two different
payloads. The receiver reports the interleaver from its last accepted FAC in
its waveform statistics; reset and incompatible-FAC tests check that this is
not presented as a newly accepted profile. With `DECODIUM_HAMDRM_RX_TRACE`
enabled, the application logs that value. The two-app A/B test was repeated
with an explicit trace assertion: TX long/RX initially short reported long
and delivered image A exactly (`.../decodium-hamdrm-two-app-a-eyuv9d0r`);
TX short/RX initially long reported short and delivered B exactly
(`.../decodium-hamdrm-two-app-b-fw8kf4es`). The interrupted-frame case is a
codec-level synthetic test, not a two-application interruption test.

An additional live interruption test then exposed a receive acquisition bug:
after rejecting an out-of-range frequency estimate, repeated pushes could
retry the same candidate indefinitely. In one failed long-to-short run the
first FAC arrived, the transmitter was stopped, and the second transmission
completed without an image while the receiver repeatedly logged the same
frequency-range error (`.../decodium-hamdrm-interrupted-a-lx55prot`). The
failure is consistent with the stalled candidate; the separate recording did
not establish why that candidate had an out-of-range estimate.

The receiver now advances past that rejected timing candidate while retaining
later samples for acquisition. Two deterministic codec regressions use
frequency offsets of -35 and +35 Hz followed by valid audio: the invalid
signal is rejected and the later data group is recovered. The regression
failed before this change and passed afterwards. With BlackHole 2ch, a single
Decodium receiver then acquired an initial FAC while a sender was forcibly
stopped, and received a different image from a subsequent sender using the
other interleaver depth. Long-to-short and short-to-long both produced one
byte-exact 225-byte image, with zero reported RX discontinuities, dropped or
rejected chunks, queue overflows and processing failures. Artifacts are
`.../decodium-hamdrm-interrupted-a-ehgikmnq` and
`.../decodium-hamdrm-interrupted-b-uc2d1pjs`; the reproducible harness is
`tests/sstv/tools/run_hamdrm_interrupted_change_lab.py`. These local tests do
not establish RF or third-party interoperability under interrupted reception.

The frequency-rejection codec regression was extended to one and five
out-of-range short frames at each of -35 and +35 Hz, with 240- and
977-sample input chunks. With one silent frame after the valid frame, all
eight cases rejected the displaced signal and recovered the subsequent
group without resource failures. Removing the final silence exposed an
end-of-stream sensitivity at that stage: +35 Hz, one displaced frame and
977-sample chunks produced no group from the single valid terminal frame.
This negative result must not be represented as resolved by the continuous
audio tests.

Two additional application replays used previously captured Decodium E/2.5/
high/16-QAM/long transmissions (`.../decodium-hamdrm-tx-a-8035qzr6` and
`.../decodium-hamdrm-tx-b-mns8pien`). FFmpeg `afreqshift` shifted the first
15 seconds of one stereo 48 kHz capture by +35 Hz or -35 Hz, then concatenated
the complete unmodified capture of the other image. Decodium received these
files through BlackHole 2ch. In both cases it logged out-of-range estimates,
completed exactly one image, and the final 225 bytes matched the second
input. All reported RX discontinuity, dropped-chunk, rejected-chunk, queue
overflow and processing-failure counters were zero. +35 Hz used A then B
(`.../decodium-qsstv-drm-app-4zryigd5`); -35 Hz used B then A
(`.../decodium-qsstv-drm-app-52ahlg7x`). The constructed stereo files are
`/tmp/decodium-hamdrm-cfo-ab-ex2JT2/{a,b}.f32`. These are modified recordings
played into a local app, not live frequency-shifted transmitters or RF, and
do not resolve the terminal-frame failure above.

The terminal-frame failure was subsequently reproduced and traced to a
frequency estimate with too few usable pilots. That result previously used
the same `Incomplete` path as a search with no timing candidate, discarding
an entire searched window even though only the best candidate had been
examined. The receiver now advances past that candidate and continues the
search. The waveform A/B matrix covers both signs of the offset, one or
five displaced frames, 240- and 977-sample chunks, and both zero and one
frame of trailing silence. All sixteen combinations recover the complete
terminal group; for both offsets a one-sample-truncated final frame emits no
group. The originally failing +35 Hz/977-sample/no-tail case now passes.
This is a bounded synthetic result, not a claim for every possible timing
offset or a physical radio test.
The full-application interruption A/B check was repeated after the change:
the long-to-short and short-to-long runs both reconstructed the second image
byte-for-byte, with exactly one completed object and clean RX counters.
Artifacts are `.../decodium-hamdrm-interrupted-a-ojtgla1l` and
`.../decodium-hamdrm-interrupted-b-2roz9k20`. These application checks use
continuous virtual audio and do not themselves exercise an exact terminal
frame boundary.

## Official QSSTV application live A/B images

On 2026-11-03 the official macOS ARM64 QSSTV-Experimental 10.0 binary
(SHA-256 `b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa`)
and Decodium ran simultaneously over BlackHole 2ch. The QSSTV executable was
not patched, but an injected laboratory library redirected QSettings to a
temporary INI directory, enabled Qt test-mode paths and set plugin search
paths; therefore the process environment was instrumented even though its
modem and audio implementations were unchanged. The library source and
binary are in `/tmp/decodium-qsstv-official-isolated/`, and its SHA-256 is
`b825bc4e5419198dc3994ec6378da7eb92420de3984cf4de480717214856e7c1`.
The QSSTV UI showed BlackHole 2ch for both input and output, CAT and serial
PTT disabled and `No Rig`. Test directories were explicitly configured in
the isolated INI file, and online status was disabled. The application
reported duplicate Qt-framework warnings, so this result is limited to this
environment. Fusion style avoided a separate macOS slider painting stall;
the initial process sample is retained as `hang-sample.txt` in that directory.

In the Decodium-to-QSSTV direction, two Decodium TX sessions sent distinct
225-byte JPEGs using E/2.5/high/16-QAM/long. The QSSTV DRM receive UI reported
each filename and saved it to the isolated RX directory. The saved A/B files
were byte-identical to the Decodium inputs (SHA-256
`d8f477eda8727eb80f162760af8f93c7d655e5df81566a665cc53ef8a076ec03` and
`14648ca1cfb6c6b186755a23c247bbc3d9b0eacc690897666676750f56b1b3ad`).
Decodium TX captures and logs are `.../decodium-hamdrm-tx-a-8035qzr6` and
`.../decodium-hamdrm-tx-b-mns8pien`; QSSTV outputs are under the isolated
`data/rxDRMImagesPath` directory. The UI reported 34 and 35 dB respectively.

In the opposite direction QSSTV's GUI sent the same A/B files as binary
objects, and separate standard Decodium RX sessions reconstructed all 225
bytes exactly, without adaptive mode. The received files and runtime logs
are `/tmp/decodium-official-live-rx-long-a-20261103/received-a` and
`/tmp/decodium-official-live-rx-long-b-20261103/received`. The diagnostics
reported 48 and 36 delivered groups and zero discontinuities, dropped or
rejected chunks, queue overflows and processing failures. The selected RX
profile was A/2.3/high/4-QAM/long. Although the QSSTV TX panel displayed
`Short`, the first attempts with Decodium's short profile reported FAC profile
mismatch. A separately captured B transmission was decoded by the official
QSSTV batch receiver as A/2.3/Hi/4/Long and produced the exact B file;
its artifacts are `.../decodium-qsstv-drm-decode-14m7a_vd`. This establishes
the observed transmitted profile for that capture, not why the GUI showed a
different selection.

A second QSSTV TX request in the same session did not produce a second
Decodium object; a process sample recorded the GUI inside
`dispatcher::startTX` / `rxFunctions::stopAndWait`, and the application was
restarted before the successful B transmission. The sample is
`hang-fusion.txt` in the isolated directory. This is not a clean sequential
two-transmission result and the stall's cause is unproven. These are selected
single-profile-per-direction, local virtual-audio tests with small synthetic
JPEGs. They do not establish uninstrumented execution, live repair/BSR,
hybrid FTP, other profiles, EasyPal compatibility or physical RF operation.

## Operator-selected experimental adaptive reception

After the CRC-gated multistage retry became the ordinary application default,
the two public EasyPal-labelled recordings below were replayed again without
the operator-selected adaptive mode. A still produced zero delivered groups
and no image; B produced 390 groups and the exact 10,546-byte unpadded
reference. All 28/42 sampled diagnostics respectively reported zero audio
discontinuities, dropped or rejected chunks, queue overflows and processing
failures. Artifacts are `.../decodium-qsstv-drm-app-wzr8a7a1` and
`.../decodium-qsstv-drm-app-4ttr0iqo`. This confirms the first recording's
ordinary-receiver gap remains after that default change; it does not identify
the original transmitter or establish live EasyPal interoperability.

An additional offline comparison on the retained 12 kHz recordings used the
current analyzer with strict 0.12/0.30 pilot limits. Flat plus multistage
produced zero groups on A; the adaptive two-timing comparison delivered 164
groups, and RS2 recovered the exact reference with 47 missing segments.
Both paths recovered the exact unpadded B reference with 390 groups. This
confirms that packet retry alone is insufficient for this A capture and that
the adaptive path supplies useful data before CRC/RS verification. It does
not justify changing the default by itself: these remain two recordings and
do not bound false locks, latency or CPU cost across machines and channels.

The Digital SSTV page now exposes an explicitly experimental adaptive RX
option. It is off at startup, may only be changed while HAMDRM is idle, and
uses strict pilot thresholds with adaptive equalization, three timing
candidates and CRC-gated flat/multistage packet retries. It can require more processing; this is not a claim of complete
EasyPal compatibility. The normal receiver remains the default. Backend
capability is checked before enabling it, and the UI disables the control
when the backend does not support it or an operation is active.

The Digital SSTV page now shows guidance beside the option even while it is
off: stop RX, enable adaptive reception and retry a difficult signal. When
enabled, it instead identifies the additional processing as experimental.
An offscreen QML test checks both messages and the disabled-while-busy state.
After this change, two Decodium A/B sessions with operator-selected adaptive
mode transferred the 225-byte fixtures exactly and verified legacy monitor
restoration; artifacts are `.../decodium-hamdrm-two-app-a-ubjyjj57` and
`.../decodium-hamdrm-two-app-b-ayqub9pk`. These are local transfers, not an
automatic diagnosis or live EasyPal interoperability proof.

On macOS ARM64, controller and adapter tests passed, and an offscreen QML
test clicked the control on and off and verified it is disabled during RX.
The application test opt-in sets the same controller property (it does not
turn on the older lab-only environment switches). Two Decodium application
pairs recovered A/B JPEGs exactly, with no reported audio queue rejection,
overflow, discontinuity, dropped chunk or processing failure:
`.../decodium-hamdrm-two-app-a-9d_47dxu` and
`.../decodium-hamdrm-two-app-b-fpi0xrnw`.

With that property enabled, the two public EasyPal-labelled recordings
described below were replayed through BlackHole into full Decodium receivers.
The first produced the same 21,191 bytes as the independent QSSTV result
(`.../decodium-qsstv-drm-app-65se2xag`). The second produced the same 10,546
bytes as the previous Decodium result (`.../decodium-qsstv-drm-app-xd83lqxa`),
equal to the QSSTV prefix followed there by 199 zero bytes. Both reported zero
audio queue rejection, overflow, discontinuity, dropped chunk and processing
failure. An initial B comparison used the wrong protected-container reference
and failed; its actual received bytes were then checked against the correct
reference before the successful repeat (`.../decodium-qsstv-drm-app-vap4vnq5`).
The page control test is offscreen and the app replay sets its property via a
test-only hook, not a mouse click in those live transfers. Windows, Linux,
real RF, live EasyPal and performance on slower machines remain unverified.

After adding the temporal candidate to the operator-selected path, the same
two public recordings were replayed in full again. Decodium recovered A as
the exact 21,191-byte QSSTV reference and B as the exact 10,546-byte
unpadded reference. The original MP3 SHA-256 hashes and both reference
hashes were checked before the runs. Across 29/43 RX diagnostic snapshots,
there were no reported discontinuities, dropped/rejected chunks, queue
overflows or processing failures. Maximum delivered-group counts were
678/1,560 and can include repeated groups from the four candidates.
Artifacts are `.../decodium-qsstv-drm-app-n2dvj1al` and
`.../decodium-qsstv-drm-app-u3ye5i2`. The laboratory selected the same
controller option and confirmed the new candidate started; it did not
isolate which candidate supplied each group. As above, the B reference is
199 bytes shorter than the zero-padded QSSTV output, and the recordings'
EasyPal versions and original transmission paths are unverified. This is
recorded-signal reception, not an EasyPal application or live RF exchange.

An offline same-PCM comparison isolated the temporal candidate's contribution
on those recordings at strict 0.12/0.30 pilot limits. On A, adaptive plus
multistage alone recovered 152 of 245 body segments and could not complete;
temporal alone recovered 172 and could not complete. Combining validated
groups from those two independent receivers recovered 213 segments, allowing
RS2 to reconstruct the exact 21,191-byte reference. On B, the combined
receivers recovered all 350 segments and the exact unpadded reference.
Swapping the A/B references was rejected in both cases. The analyzer's
printed frame and group statistics refer to the primary receiver, while its
assembler consumes validated groups from both. This comparison does not
isolate their relative contribution in the preceding full-app runs or prove
the original recordings' transmitter provenance.

Reproduce by setting `DECODIUM_HAMDRM_TEMPORAL_MERGE=1` for
`analyze_hamdrm_capture` with `adaptive-multistage`, using the A/B PCM and
references in `/tmp/decodium-qsstv-easypal-check` and profiles
`B 2500 16 ... long ... high` and `E 2300 4 ... long ... high` respectively.
The analyzer regression suite also exercises merged A/B QSSTV fixtures,
crossed references and seeded noise; it passed after the addition.

`tests/sstv/tools/check_hamdrm_easypal_recordings.py ANALYZER DIRECTORY` now
repeats the recorded A/B comparison with SHA-256-pinned PCM and references,
the standard receiver, adaptive/temporal merge, and crossed-reference
negatives. On the current Mac build, standard RX still did not reconstruct A
and did reconstruct B; the adaptive merge reconstructed both byte-for-byte and
rejected both crossed references. The check allows a future standard receiver
to recover A without treating that improvement as failure. It clears diagnostic
environment overrides that could change the mode or write decoded objects;
an inherited output-path negative control created no file. The external
recordings are not distributed with the repository, and this offline check
does not establish their EasyPal provenance or a live peer exchange.

With the current flat/multistage receiver, an offline A/B threshold sweep
used normalized pilot-error limits 0.12, 0.20, 0.30, 0.50 and 1.0, each
with the corrupted-pilot fraction set to 1.0. Recording A yielded no groups
at any of these settings, although the reported pilot failures decreased
from 203 to 101; B yielded 390 groups and its exact unpadded reference at
each setting. Thus simply relaxing these diagnostic thresholds did not
repair A, and the sweep does not justify weakening production checks. The
hash-pinned comparison helper repeats it with `--pilot-sweep`; it reports
outcomes without requiring A to remain a failure in future builds. These
are two selected offline recordings, not an RF tolerance or safety bound.

An offline-only experiment tried reducing the parallel-candidate cost by
using one receiver and selecting, for each symbol, the lowest reported pilot
error among temporal, interpolated and cross-validated flat estimates. On
recording A it delivered 203 groups but retained 172 of 245 body segments,
the same result as temporal alone, and did not reconstruct the image. B
reconstructed its 10,546-byte reference. This selected comparison did not
show an improvement sufficient to replace the existing multi-receiver merge;
the experimental code was removed. In particular, it does not establish
that differently computed pilot-error scores are directly comparable, nor
that no lower-cost estimator could work on these or other signals.

An independent optimization experiment precomputed the temporal estimator's
pilot-neighbour indices once per frame instead of rescanning them for every
symbol. Five alternating offline before/after runs on the same two recordings
produced identical analyzer output and exact recovered objects. Median
wall times were 1.0803/1.0840 seconds for A and 2.1708/2.1546 for B
(before/after). These small changes do not establish a useful performance
improvement; the prototype added four index vectors and was removed. The
original receiver and waveform suite were rebuilt, and the waveform CTest
passed. This small same-machine experiment is not a CPU, energy or
cross-platform benchmark and does not resolve the standard receiver gap.

The two recorded inputs were additionally resampled to simulate opposite
small clock offsets: the 12 kHz PCM was interpreted as 12,001 Hz for A and
11,999 Hz for B, then resampled back to 12 kHz (approximately ±83 ppm;
this also shifts audio frequencies). The offline adaptive and temporal merged
receiver recovered the A reference and B unpadded reference exactly. Full
Decodium application receivers using the operator-selected experimental
adaptive mode replayed those altered files over BlackHole and recovered
21,191 and 10,546 bytes exactly, with no reported dropped or rejected chunks,
discontinuities, queue overflows or processing failures. Artifacts are
`.../decodium-qsstv-drm-app-974w5asx` and
`.../decodium-qsstv-drm-app-u6fvisps`. Resampled s16 SHA-256 values are
`c7598a0687a476dfbeec7575715f79e6b147ffb0402589926490ca53928f1066`
and `3b9e9f2d6ec782eda9afffc2e06823c97ad71306dc1216917e6126727a07cbf2`.
Separately, two Decodium application pairs using adaptive mode and the macOS
legacy monitor exchanged 225-byte A/B fixtures exactly and restored fresh
legacy PCM (`.../decodium-hamdrm-two-app-a-wzxunikz` and
`.../decodium-hamdrm-two-app-b-1pjasimm`). Those local transfers did not
include the clock offset. The recordings' EasyPal provenance remains
unverified; none of these checks is a live EasyPal exchange or an RF test.

Further operator-selected A/B tests checked the macOS legacy-backend handoff:
both 225-byte images arrived exactly, monitoring was suspended while HAMDRM
owned capture, and the legacy monitor and fresh PCM resumed after cancellation.
Artifacts are `.../decodium-hamdrm-two-app-a-gcnenrgn` and
`.../decodium-hamdrm-two-app-b-41cmlk_5`. Longer B/2.5/high/16-QAM/long
transfers of 7,624 and 9,976 bytes also matched exactly with the same option,
with zero reported audio discontinuities, dropped/rejected chunks, queue
overflows and processing failures (`.../decodium-hamdrm-two-app-a-wsbwnf12`
and `.../decodium-hamdrm-two-app-b-6ik_iovz`). The helper now requires those
continuity counters for any operator-selected timing test as well as long
transfers. These are local macOS virtual-audio results, not a CPU benchmark or
proof for other operating systems, live external applications or RF.

The selected adaptive option now also enables the previously laboratory-only
fractional candidate and CRC-gated retries. An initial noisy A/short test
with the fractional candidate alone added to the old operator selection
did not reconstruct an image; its relay reported no audio error and Decodium
reported no rejected chunks or queue overflow (`.../decodium-hamdrm-two-app-a-a65190t1`).
Enabling strict-pilot CRC-gated retries as well recovered the selected test.
After wiring those options into the operator selection, A/short and B/long
were sent between two Decodium sessions over BlackHole with 1 ms echo at
gain 0.27 and deterministic noise and 1700 Hz tone each at peak 0.01. Both
225-byte JPEGs matched exactly, with no reported discontinuity, dropped or
rejected chunks, queue overflows or processing failures, and the legacy
monitor and fresh PCM resumed after cancellation. The normal path, without
the option selected, also passed clean A/B exact-byte transfers and legacy
restoration. The test hook selects the same controller property used by the
UI; the live tests do not click the page control. Fractional-candidate group
counts were one and three, but can duplicate primary results and do not show
that candidate was independently necessary for these particular runs.

The pinned unmodified QSSTV-Experimental 10.0 batch receiver decoded both
recorded relayed signals byte-for-byte. This validates these selected
Decodium-to-QSSTV recordings, not live two-way EasyPal/QSSTV compatibility,
RF operation or sustained CPU headroom. Selected-mode Decodium artifacts are
`decodium-hamdrm-two-app-a-gtvg9o2m` and `decodium-hamdrm-two-app-b-jnnme3dl`,
relay recordings `decodium-hamdrm-two-app-echo-pumceuat` and
`decodium-hamdrm-two-app-echo-a_sgrhgg`, and QSSTV outputs
`decodium-qsstv-drm-decode-qlivsd0s` and `decodium-qsstv-drm-decode-4l00elf9`.
The clean default-mode controls are `decodium-hamdrm-two-app-a-9lqc4xw4`
and `decodium-hamdrm-two-app-b-d3msqmtz`.

The expanded operator-selected receiver was also checked with two larger
320×240 JPEGs, 7,624 and 9,976 bytes, using profile
`hamdrm-B-2500-high-16-QAM-long` and two local Decodium processes on clean
BlackHole audio. Both reconstructed the input byte-for-byte, retained zero
reported discontinuities, dropped/rejected chunks, queue overflows and
processing failures, and restored legacy monitoring and fresh PCM after
cancellation. The sessions accepted up to 1,888 and 2,287 audio chunks;
maximum group counters were 48 and 54 and may include duplicates. Artifacts
are `decodium-hamdrm-two-app-a-o5znek0g` and
`decodium-hamdrm-two-app-b-x_1duqsy`. This is a clean selected-profile A/B,
not a general performance benchmark or evidence for live EasyPal.

The same larger source images were then transferred A/B with operator-selected
adaptive reception through a BlackHole relay adding a 1 ms echo at gain 0.27,
deterministic noise at peak 0.01 and a 1700 Hz tone at peak 0.01. Decodium
reconstructed all 7,624 and 9,976 bytes exactly, with zero reported audio
discontinuities, dropped or rejected chunks, queue overflows and processing
failures; legacy monitoring and fresh PCM resumed after cancellation. The
fractional candidate reported 16 and 18 groups, which may duplicate groups
delivered by the other candidates. The unmodified QSSTV-Experimental 10.0
batch receiver decoded the two saved relayed signals to the same exact
objects. Decodium artifacts are `decodium-hamdrm-two-app-a-4flfzj50` and
`decodium-hamdrm-two-app-b-moiulncw`, relay captures
`decodium-hamdrm-two-app-echo-nhlnojtw` and
`decodium-hamdrm-two-app-echo-rmwhtj12`, and QSSTV results
`decodium-qsstv-drm-decode-n0m0yxhe` and
`decodium-qsstv-drm-decode-o0713kb8`. These are selected simulated channels
and a one-way recorded QSSTV check, not a live two-way interoperability or RF
claim. The relay harness derives its timeout from the explicitly configured
session duration for longer transfers.

After enabling the additional candidates and retries in the operator-selected
path, both previously recorded EasyPal-labelled signals were replayed again
through BlackHole. The first reconstructed 21,191 bytes exactly equal to the
previously QSSTV-checked reference (`decodium-qsstv-drm-app-_96w0mx_`); the
second reconstructed the same 10,546-byte Decodium object as before
(`decodium-qsstv-drm-app-ug7ry91v`), retaining the separately documented
199-byte QSSTV padding difference. Both had zero reported source changes,
discontinuities, dropped or rejected chunks, queue overflows and processing
failures. Their maximum delivered group counts were 472 and 1170, including
possible duplicates across candidates. These are replayed recordings and do
not establish live EasyPal interoperability or original transmitter identity.
The replay helper accepts already-converted 48 kHz stereo float captures and
checks the selected candidate/retry logs and continuity diagnostics.

Two application-level negative controls exercised the same selected path
through BlackHole with 12 seconds of deterministic white noise, peak amplitude
0.18, seeds 305419896 and 2596069104, on A/2.5/normal/16-QAM/short and
B/2.5/high/16-QAM/long respectively. Neither produced a decoded group or
saved image; source/continuity diagnostics reported no dropped or rejected
chunks, queue overflows, discontinuities or processing failures. Artifacts
are `decodium-qsstv-drm-app-b54bbbg_` and
`decodium-qsstv-drm-app-gfc6lvbn`; the generated WAVs are in
`decodium-hamdrm-random-ab-oof9w4k_`. The application harness uses the same
test-only property hook as the positive tests and checks the candidate/retry
logs. These controls do not quantify false-positive rates or prove rejection
of every malformed waveform. Use `DECODIUM_LAB_EXPECT_NO_IMAGE=1` with
`DECODIUM_LAB_HAMDRM_USER_TIMING=1` and an explicit profile to reproduce the
negative check with a generated noise WAV.

## E/2.3/high/64-QAM/long with a synthetic echo

Two different 225-byte JPEGs were transmitted by full Decodium processes on
macOS ARM64 with E/2.3 kHz/high protection/64-QAM/long interleaving. A
BlackHole relay added a 1 ms echo at gain 0.10 without additional noise or
tone. Both ordinary receivers reconstructed their image byte-for-byte;
active diagnostics reported no dropped/rejected chunks, queue overflows,
discontinuities or processing failures, and the legacy monitor and fresh PCM
resumed after cancellation. Relay active frames were 467,998 and 468,231;
no relay errors were recorded. App artifacts are
`.../decodium-hamdrm-two-app-a-xyjljont` and
`.../decodium-hamdrm-two-app-b-uqhn8qau`, and relay captures are
`.../decodium-hamdrm-two-app-echo-ms0tlxrw` and
`.../decodium-hamdrm-two-app-echo-3h7qtlls`.

The pinned unmodified QSSTV-Experimental 10.0 batch receiver independently
identified both saved recordings as E/2.3/Hi/64/Long and recovered the exact
225-byte inputs. Each reported 15 good MSC CRCs, zero bad MSC CRCs and one
bad packet. Its artifacts are `.../decodium-qsstv-drm-decode-f8c928af` and
`.../decodium-qsstv-drm-decode-_93p9ysm`. The first QSSTV batch check ran
while the B audio test was starting; B's own audio diagnostics and relay
reported no errors. These are local A/B transfers and one-way recorded-peer
checks for one profile, not live two-way QSSTV, EasyPal or RF evidence.

The same E/2.3/high/64-QAM/long profile was tested with a 1 ms echo at
gain 0.10 plus deterministic noise and a 1700 Hz tone, each peak 0.01.
With the ordinary receiver neither A nor B reconstructed the image: TX
completed and the relay reported no errors, but RX delivered at most zero
and one groups respectively. The harness ultimately reported a missing legacy
restoration result because image completion did not trigger its restoration
sequence; this is not evidence that a completed reception failed to restore
the legacy backend. Ordinary artifacts are
`.../decodium-hamdrm-two-app-{a-ed5r0w9j,b-6m39qofd}` and relays
`.../decodium-hamdrm-two-app-echo-{r6env3tq,pdpcp8tu}`.

With the operator-selected experimental adaptive option, new A/B sessions
under the same nominal relay parameters reconstructed both 225-byte images
exactly, with zero reported RX discontinuities, dropped/rejected chunks,
queue overflows and processing failures; legacy monitoring and fresh PCM
resumed after cancellation. The early candidate reported nine groups in each
run; counts may duplicate primary results. The unmodified QSSTV batch
receiver recovered both recorded outputs byte-for-byte and identified
E/2.3/Hi/64/Long; it reported 16 good MSC CRCs, zero bad MSC CRCs and one
bad packet per recording. App artifacts are
`.../decodium-hamdrm-two-app-{a-19wj0qk6,b-ul_uy_cs}`, relay captures
`.../decodium-hamdrm-two-app-echo-{g3nwaf6f,y6jqizvf}`, and QSSTV results
`.../decodium-qsstv-drm-decode-{maewm4b0,fm967wwn}`. The runs are separate
transmissions with the same nominal impairment, not sample-identical replay
controls. They do not establish general noise tolerance, RF behaviour or
live two-way interoperability.

The two adaptive-run relay recordings above were then replayed without
modification through BlackHole into separate full Decodium RX sessions,
first with ordinary reception and then with the operator-selected adaptive
option. The ordinary receiver saved no image and reported at most one group
for each recording; the adaptive receiver recovered each 225-byte reference
exactly. All replay runs reported zero discontinuities, dropped/rejected
chunks, queue overflows and processing failures. Artifacts for ordinary A/B
are `.../decodium-qsstv-drm-app-s3m1w6eg` and
`.../decodium-qsstv-drm-app-rna7b3_t`; adaptive A/B are
`.../decodium-qsstv-drm-app-k7v32dev` and
`.../decodium-qsstv-drm-app-at37jseq`. This controlled recorded-signal A/B
comparison keeps the waveform identical; the fresh audio playback sessions
can still introduce timing variation. The harness supports
`DECODIUM_LAB_EXPECT_INCOMPLETE=1` to assert no image and clean audio without
requiring zero decoded groups. It does not claim a general false-positive or
noise-sensitivity rate.

On 2026-11-04 these exact two relay recordings were replayed again through
BlackHole into the current ordinary receiver, now with the automatic early and
temporal candidates enabled. Both A and B reconstructed their respective
225-byte JPEG reference exactly; RX audio diagnostics were clean and logs
confirmed both automatic candidates. The successful runs are
`.../decodium-qsstv-drm-app-im99yuv8` and
`.../decodium-qsstv-drm-app-zc109i8c`. An initial B attempt stopped on a
PortAudio playback error before finishing and is not a receiver result
(`.../decodium-qsstv-drm-app-bvm5kkw8`); the fresh B replay passed. Compared
with the earlier ordinary replays above, this is evidence of recovery for
these two saved signals, not a claim about arbitrary channels, live EasyPal,
or RF. The independent QSSTV result above applies to the saved signals.

Further same-build controls disabled both automatic candidates while retaining
the default primary receiver and frame timing tracking. Both recordings still
reconstructed their respective exact 225-byte JPEGs and had clean audio
diagnostics; the logs confirmed tracking and the absence of both automatic
candidate markers. Artifacts are `.../decodium-qsstv-drm-app-9pbyw_pk` and
`.../decodium-qsstv-drm-app-fkcomiq0`. The A harness had been configured to
expect incomplete reception and therefore exited with an assertion failure
when it unexpectedly produced an image; a separate check verified the exact
bytes, diagnostics and candidate markers. B passed the ordinary exact-image
check. Consequently these recordings do not establish that the automatic
candidates caused the current improvement; the tracked primary alone was
sufficient in these runs. Playback timing varies between separate runs.

## A/2.5/high/64-QAM with a synthetic echo

Two distinct 225-byte JPEGs were transferred between full Decodium processes
on macOS ARM64 using A/2.5 kHz/high protection/64-QAM/short interleaving. A
BlackHole relay added a 1 ms echo at gain 0.1 and saved the relayed signals.
Both ordinary flat-receiver sessions recovered the exact input bytes; their
recorded diagnostics reported no dropped or rejected chunks, queue overflow,
discontinuity or processing failure. The pinned unmodified QSSTV-Experimental
10.0 batch receiver independently decoded both relayed recordings to the
same bytes and identified `A/2.5/Hi/64/Short`. QSSTV reported 13 good and one
bad MSC CRC, and no bad packets, for each capture. This is a one-way
Decodium-to-QSSTV recording comparison and two local Decodium transfers,
not live QSSTV/EasyPal, RF, another OS, or all 64-QAM profiles.

Decodium artifacts are `.../decodium-hamdrm-two-app-a-3s_etg3u` and
`.../decodium-hamdrm-two-app-b-n6bkb7px`; saved relay signals are in
`.../decodium-hamdrm-two-app-echo-4vkbymyo` and
`.../decodium-hamdrm-two-app-echo-3xqo8xxa`. QSSTV artifacts are
`.../decodium-qsstv-drm-decode-0h70gsjs` and
`.../decodium-qsstv-drm-decode-_64pr83t`. The relay helper accepts a checked
`DECODIUM_LAB_ECHO_PROFILE` override and rejects an interleaver mismatch;
without the override its earlier default remains unchanged.

## E/2.5/normal/64-QAM application evidence and partial recovery

On 2026-11-01 two full Decodium TX applications sent distinct 225-byte JPEGs
through BlackHole with E/2.5 kHz/normal protection/64-QAM/short interleaving.
The pinned, unmodified QSSTV-Experimental 10.0 batch receiver identified
`E/2.5/Lo/64/Short` and recovered both objects byte-for-byte. Its log reported
15 valid MSC CRCs and one invalid CRC for A, and 14 valid and one invalid for
B; both reported `pkt_bad=1`. The captures are
`.../decodium-hamdrm-tx-a-6c1o5kdx` and
`.../decodium-hamdrm-tx-b-7cxapg_e`, with independent receiver artifacts
`.../decodium-qsstv-drm-decode-h8w0ihzx` and
`.../decodium-qsstv-drm-decode-c78yx4on` under the macOS temporary folder.

After the receive change below, two full Decodium applications exchanged
the same A/B inputs on this profile and reconstructed both exactly. Their
artifacts are `.../decodium-hamdrm-two-app-a-6b8177yv` and
`.../decodium-hamdrm-two-app-b-i1e85f12` in the macOS temporary folder.

For the reverse direction, the separately built QSSTV-Experimental revision
`a84584248d3307bfa081362031ff20f365337633` with the documented signed-PCM
patch generated two 320×240 images. The official QSSTV receiver reconstructed
4,998- and 5,021-byte JP2 objects and reported 69 valid MSC CRCs, no bad MSC
CRCs and no bad packets for each. Its reported profile was
`E/2.5/Lo/64/Long`, despite the transmitter CLI requesting short interleaving.
The WAVs `/tmp/decodium-qsstv-64-normal-{a,b}.wav` have SHA-256
`79216af71f582093ee643cab2067e387beb4a19869aac169bec345eb6e761d1d`
and `774365d5c256d2908db04c230b1c8a41fc970afdb89f31bcc3b97808c6755601`.

Before the receive change, single transmissions A and B did not complete in
Decodium; repeating each file did not complete it either. The trace on B
showed packet CRC failures resetting both synchronization and the five-frame
interleaver, dropping subsequent valid data. After the change, a rejected
packet or MOT group with CRC mismatch or incomplete continuity still remains
rejected, but the already validated waveform and interleaver timeline is kept.
Other failures retain their previous reset behavior. A single A recording
then reconstructed all 4,998 bytes exactly in
`.../decodium-qsstv-drm-app-1cqo8vbi`. B still failed: with a single replay
and with two repetitions it retained 32 of 34 segments, missing 2 and 10,
despite the receiver reporting five packet CRC failures in the first pass.
Before the change, the repeated B replay had only 24 of 34 segments. Artifacts
are `.../decodium-qsstv-drm-app-wycgw55h` (before),
`.../decodium-qsstv-drm-app-gfeb398n` (after single B) and
`.../decodium-qsstv-drm-app-yid6f_k_` (after repeated B). A/B synthetic
interference tests ensure damaged data is not emitted and later valid groups
are recoverable; those tests do not by themselves distinguish the previous
implementation. The full waveform, adapter and controller test targets pass.

At this stage the observations exposed a 64-QAM receive defect on clean local
audio. They did not establish why the remaining CRC errors occurred, full
bidirectional compatibility, operation with the unmodified QSSTV transmitter,
EasyPal, RF, Windows or Linux.

### Soft-decision retry for the two 64-QAM recordings

The receiver now tries bounded max-log QAM bit reliabilities and soft Viterbi
decoding when the existing hard-decision MSC result fails its packet CRC. It
submits a retry only when its packet CRC is valid; bad data remains rejected.
The normal fast path remains unchanged. All 36 independent pinned QSSTV MSC
cell vectors decode with the new path. In a deterministic synthetic 64-QAM
noise test, the hard path recovered 0/20 inputs and the soft path 20/20 for
the chosen amplitude and seeds; this is a controlled test, not a field BER
measurement. Bounds tests cover non-finite and extreme input, malformed
profiles and invalid soft reliability.

After this change, replaying each original QSSTV-generated WAV once through
BlackHole into separate full Decodium receivers recovered both reference JP2
objects byte-for-byte (4,998 and 5,021 bytes). Both traces reported 72
validated data groups and zero packet CRC failures; rejected pilot candidates
were still observed during acquisition and after the recording. The previous
B recording produced five packet CRC failures and two missing segments even
after the interleaver-preservation fix. Current A/B artifacts are
`.../decodium-qsstv-drm-app-uzvz1t84` and
`.../decodium-qsstv-drm-app-nk_evne4` under the macOS temporary folder.
Two full Decodium applications also exchanged different JPEGs exactly using
E/2.5/normal/64-QAM/long, with artifacts
`.../decodium-hamdrm-two-app-a-uajdpswr` and
`.../decodium-hamdrm-two-app-b-ogenarba`. These local A/B tests do not prove
other channels, other operating systems, RF, EasyPal or the unmodified QSSTV
transmitter.

### B/2.3/normal/64-QAM A/B

Two different 225-byte JPEGs transmitted by full Decodium applications over
BlackHole with B/2.3/normal/64-QAM/short were recovered byte-for-byte by the
pinned unmodified QSSTV-Experimental 10.0 batch receiver. It reported the
expected `B/2.3/Lo/64/Short`, 14 and 15 valid MSC CRCs respectively, no bad
MSC CRCs and one `pkt_bad` in each run. TX captures are
`.../decodium-hamdrm-tx-a-_okhvezv` and
`.../decodium-hamdrm-tx-b-i9e6yenr`; QSSTV outputs/logs are
`.../decodium-qsstv-drm-decode-4mqdlwnb` and
`.../decodium-qsstv-drm-decode-j9wjjbvt` in the macOS temporary folder.

In the opposite direction the locally built QSSTV revision and signed-PCM
patch described above generated two independent images. Its own unmodified
batch receiver recovered the 4,998- and 5,021-byte JP2s and reported
`B/2.3/Lo/64/Long`, 62 valid MSC CRCs and zero bad CRCs or bad packets per
file. The CLI had requested short interleaving, so the observed long profile
was selected in Decodium. Both full Decodium receivers reconstructed those
JP2 bytes exactly; their traces report 65 validated data groups and zero packet
CRC failures, with rejected pilot candidates during acquisition and after the
audio. TX WAV SHA-256 values are
`58bfc07f21fecc4f6812b400c33305b3938b26ad602ded8f1ec67d5a6b13dc63` and
`12cbd9c2f0e81b59d4bbde0dddb8078dd08df4725bc4fb318d93d2c70e47fe56`;
Decodium logs and received objects are
`.../decodium-qsstv-drm-app-h3hwugcq` and
`.../decodium-qsstv-drm-app-2o13_x4z` in the macOS temporary folder.

Two further full Decodium applications exchanged both JPEGs exactly using
B/2.3/normal/64-QAM/long, with artifacts
`.../decodium-hamdrm-two-app-a-5vrbkcrd` and
`.../decodium-hamdrm-two-app-b-dk69njw8`. The short and long directions are
separately observed, not a bidirectional test of either exact tuple. These
file-mediated local tests do not establish RF, EasyPal, other profiles or other
operating systems.

#### Official QSSTV transmitter: recovered PCM from timed-out WAVs

The pinned official, unmodified QSSTV-Experimental 10.0 ARM64 application was
also invoked to generate A/B WAVs for B/2.3/normal/64-QAM on this Mac. Both
processes failed to exit within 45 seconds and were terminated by the test
harness. Each left a 6,102,572-byte WAV with a stale RIFF container length;
the data chunk length matched the retained PCM. Diagnostic copies fixed only
the RIFF length field (changed byte offsets 5 and 6); all PCM bytes remained
identical. This is evidence from a recoverable timed-out recording, not a
successful clean transmitter exit. The original A/B WAV SHA-256 values are
`d1ac8502e14f47847ed8f70afc823544700e6266cc0d45238b1ac9f25a08291e` and
`b360d9ae0d2d015e33758bab4208711c6feaa8e5d11b9de1509553e931df1491`;
the repaired copies are
`4bb6586adce44b28860e513b498dfd0496b4a093443054ff50653d5179c66877` and
`b42f462399ad145cf935251e45bb46e5426b70ff297f8af1d4c8ee9257527e1b`.
They and the transmitter logs are under `/tmp/decodium-qsstv-official-b23-64-*`.
The bounded diagnostic `tests/sstv/tools/repair_qsstv_riff_length.py SOURCE
DESTINATION` reproduces these exact WAV hashes for both inputs, verifies the
PCM format and data length, refuses to overwrite an output, and reports a PCM
SHA-256. Its A/B synthetic and malformed-input checks are registered with
CTest. It repairs no missing samples and does not assert TX completion.

The unmodified QSSTV batch receiver independently recovered the 4,998- and
5,021-byte JP2 objects, identifying `B/2.3/Lo/64/Long` and reporting 62 good
MSC CRCs, no bad MSC CRCs and no bad packets for each. Decodium then recovered
both reference objects exactly from the repaired files played through
BlackHole; both traces show 65 validated groups, zero packet CRC failures and
pilot candidates rejected during acquisition and after the audio. Decodium
artifacts are `.../decodium-qsstv-drm-app-5dpwp29e` and
`.../decodium-qsstv-drm-app-wkvp6fot` in the macOS temporary folder. The
requested QSSTV interleaving was short but the received profile was long.
This limited A/B result does not establish why the official encoder hangs,
nor operation over RF, other QSSTV builds or other operating systems.

### Public audio examples labelled EasyPal: negative A/B reception

Two publicly available MP3 examples were retrieved on 2026-11-01: the
[Ham's Radio EasyPal example](https://hamsradio.net/ham/?p=11258), 115.45 s,
SHA-256 `3780d053fc8e544abec07e485b1fc5f7914fe080f93daac224288bbfd23eb3ae`,
and the [K9ZXO EasyPal 800×600 example](http://www.brainmist.com/k9zxo/index.htm),
188.08 s, SHA-256
`be3fd8e8acbe3b7378805709b411e4addb1567b6d2d0c4865c885222c32a6127`.
The second site's HTTPS certificate could not be validated; it was retrieved
over HTTP, so the hash pins the observed bytes but does not authenticate their
origin. The pages label the recordings EasyPal; the exact transmit versions,
radio path and whether these are complete unedited transmissions are unknown.
They are not redistributed as Decodium fixtures.

After the later standard acquisition changes, both original MP3 files were
retrieved again and matched the SHA-256 values above. The pinned unmodified
QSSTV-Experimental 10.0 receiver regenerated a 21,191-byte A object
(SHA-256 `28b0ceec19c4f068c6363fa34dbd896e16f830dbf52a51932d808ed7f03b51e6`)
and a 10,745-byte B object
(`33897a8f1903c14327eafdd775314da5901eb167ac78b2884b01f387da48b705`).
Its log reported 278/278 and 394/394 good MSC CRCs respectively, but Jasper
reported a component mismatch when rendering each JP2. On B, the last 199
bytes were zero; removing only those bytes yielded 10,546 bytes
(`da0e069f3ae3d11a95fe64cce8aa528c87c8b82f1b8b7dfea8d343ada8c8b667`),
and FFmpeg decoded both padded and unpadded files to the same frame MD5
`5668157c77eece42195b4c780dfc3ecf`.

The current ordinary Decodium receiver replayed both recordings through
BlackHole. A did not produce an image and delivered zero groups; B delivered
390 groups and reconstructed the unpadded 10,546-byte object exactly. Both
reported zero discontinuities, dropped or rejected chunks, queue overflows
and processing failures. Artifacts are
`.../decodium-qsstv-drm-app-6wgjq2s6` and
`.../decodium-qsstv-drm-app-d3aqtd8j`. The A result is a negative test, not
an audio continuity failure; B is not byte-identical to QSSTV's padded file.
These specific public recordings do not authenticate the original EasyPal
versions or establish a live peer exchange. QSSTV's duplicate Qt-framework
warnings in this setup are also retained as a limitation of the regenerated
reference.

An offline isolation using the restored PCM and strict 0.12/0.30 pilot
limits compared flat, flat with multistage retry, flat with a second timing
candidate, adaptive, and adaptive with a second timing candidate. On A, all
three flat variants delivered zero groups. The single adaptive candidate
received 152 of 245 body segments and did not reconstruct the object; the
two-candidate adaptive run received 198 segments and RS2 reconstructed the
exact 21,191-byte reference. On B, flat, adaptive, and two-candidate adaptive
each recovered all 350 segments and the exact 10,546-byte unpadded reference.
Swapping the two expected references caused both two-candidate comparisons to
fail as intended. These are offline comparisons of the same two recordings,
not new signals or an application-level A/B. They isolate a difference in
these conditions but do not establish sufficient CPU headroom or false-lock
behaviour to enable experimental adaptive reception by default.

Two 60-second deterministic offline negative controls were then fed to the
two-candidate adaptive analyzer: seed 103234 with uniform noise at peak 0.20
on B/2.5/high/16-QAM/long, and seed 534292 with the same noise plus a 1900 Hz
tone at peak 0.10 on E/2.3/high/4-QAM/long. Neither produced a validated group
or an object. These two controlled cases do not quantify a general false-lock
rate or establish RF performance.

The automated waveform suite now also exercises six A/B/E profiles covering
both bandwidths, 4/16/64-QAM and short/long interleaving. Each profile is fed
two deterministic eight-frame inputs: noise at peak 0.20, and independent
noise at peak 0.20 with a 1900 Hz tone at peak 0.10. The strict-pilot
adaptive receiver with CRC-gated retries emitted no data groups in any of the
12 cases; the full waveform suite passed. This is a bounded synthetic
single-candidate test, not an estimate of false-positive probability, a
two-candidate application measurement, or live RF evidence.

The two 60-second noise inputs described above were also replayed through
BlackHole into full Decodium applications using the operator-selected
adaptive path (including the independent timing candidates). A/B emitted no
groups and saved no images, with no reported discontinuities, dropped or
rejected chunks, queue overflows or processing failures. Artifacts are
`.../decodium-qsstv-drm-app-7fkjmpoo` and
`.../decodium-qsstv-drm-app-fm1uyy5h`. The inputs were converted from 12 kHz
mono s16 to 48 kHz stereo WAV for replay. These controls strengthen the
selected application-path evidence but do not estimate a general false-lock
rate or establish behaviour under arbitrary RF interference.

For a rough same-machine cost comparison, the same 115.45-second recording A
was replayed in real time into Decodium twice on macOS ARM64, once with the
default receiver and once with the operator-selected adaptive option. A
two-second `ps` sampling interval observed 73 samples for each whole process,
including startup and shutdown. Standard reception failed to recover the
reference and had median/max CPU 37.4%/99.1%, maximum RSS 524,736 KiB;
adaptive reception reconstructed all 21,191 reference bytes with zero reported
audio errors and had 39.1%/98.7%, maximum RSS 531,408 KiB. Percent is relative
to one CPU core. These are single, sequential process-level measurements on
one Mac, affected by UI, other tasks, startup and shutdown; they are not a
worst-case or slower-machine CPU benchmark and do not justify enabling the
experimental path by default. The measurements and samples are
`.../decodium-hamdrm-measure-standard-uy2p2290` and
`.../decodium-hamdrm-measure-adaptive-3qj6ltol`; RX artifacts are
`.../decodium-qsstv-drm-app-2ven0sh8` and
`.../decodium-qsstv-drm-app-3wujsb5z`. The sampling wrapper is
`tests/sstv/tools/measure_hamdrm_replay.py` and preserves the replay harness's
exact-byte and continuity checks.

After conversion to 48 kHz stereo PCM, the pinned unmodified QSSTV batch
receiver extracted a 21 KB JP2 from the first and a 10 KB JP2 from the second,
reporting B/2.5/high/16-QAM/long and E/2.3/high/4-QAM/long respectively.
QSSTV reported 278 and 394 good MSC CRCs with no bad MSC CRCs, and zero and
six bad packets. Its Jasper image renderer reported an IHDR component mismatch
on both; the objects were nonetheless saved. This independently established
byte references, not the authenticity of their source or visual correctness.

Full isolated Decodium receivers replayed each PCM through BlackHole with the
observed profiles. Neither reconstructed the QSSTV object. The first delivered
no data groups; the second delivered 63 groups and retained 62 of 350 body
segments without a complete header. Logs are in
`.../decodium-qsstv-drm-app-b2sc5o3e` and
`.../decodium-qsstv-drm-app-ms9u461g` under the macOS temporary folder.
The findings are failures for these recordings, not proof of an EasyPal TX
version, a generic RF failure or complete EasyPal incompatibility.

The bounded, developer-only `analyze_hamdrm_capture` target accepts a 12 kHz
mono s16le file, explicit profile, diagnostic pilot limits and a reference
object. FFmpeg conversion of the two sources yielded the same failure pattern
offline: first 284 received frames, 203 pilot failures and no groups; second
401 received frames, 639 pilot failures, 66 MSC failures and 63 groups.
Varying the normalized pilot error limit from 0.2 through 5.0, with the
corrupted-pilot fraction allowed up to 1.0, recovered neither object. These
post-hoc variations are diagnostic and are not acceptance thresholds. Two
known independent QSSTV PCM fixtures and their wrong-reference negative cases
exercise the analyzer in `test_hamdrm_capture_analyzer`. The waveform receiver
currently uses a flat per-symbol channel estimate; frequency-selective
equalization and robust synchronization remain candidates to investigate, not
proven fixes. An offline-only interpolated-pilot experiment, selected by the
capture analyzer rather than enabled by default, recovered 44 groups and 41
segments from the first recording with the original pilot limits (the flat
estimator recovered no groups). It did not complete the object; on the second
recording it still recovered only 62 of 350 segments. Two independent QSSTV
control recordings both reconstructed byte-for-byte. A synthetic test also
compares flat and interpolated estimation for two robustness modes and checks
that a corrupted pilot is detected. These results justify further investigation
but do not demonstrate EasyPal interoperability or a working application path.
Using that same leave-one-out pilot error for timing acquisition was also
tried: the two QSSTV controls still reconstructed exactly, but the first
recording fell to 29 groups and 27 segments and the second remained at 62 of
350 segments. The acquisition experiment was removed after this negative A/B
result; it did not establish the cause of either failure.
The receiver reference also uses a multi-symbol channel estimate and tracks
sample-clock offset during reception. As a diagnostic only, both recordings
were resampled with fixed offsets of -1000, -300, +300 and +1000 ppm before
the selective-estimator replay. No offset reconstructed either object: for the
first recording +300 ppm produced 9 groups and the other three produced none;
the second produced none at all four offsets. All are worse than the original
recordings. A fixed-offset sweep cannot rule out time-varying clock or carrier
drift, nor does it replace a receiver that tracks those quantities.

Follow-up: the recorded traces showed regular MSC failures followed by an
interleaver reset. Decodium had required the unused fractional byte after the
decoded packet to contain only zero bits. QSSTV's `sourcedecoder.cpp` instead
converts `floor(length_decoded_data / 8)` bytes and checks the packet CRC on
those bytes. Decodium now uses the complete bytes and still requires packet
and MOT CRCs; A/B tests cover zero and one trailing bits, altered packet bits,
and truncated input. With the flat estimator and default diagnostic limits,
the second recording now yields 390 validated groups and all 350 segments.
After RS decoding the resulting 10,546 bytes exactly match the prefix of the
QSSTV output; its remaining 199 bytes are zero. FFmpeg decodes both files to
the same 320x480 pixel-frame MD5 `5668157c77eece42195b4c780dfc3ecf`.
The diagnostic now decodes RS containers before comparing and reports this
case separately as `reference_zero_padding=199`; it still returns nonzero
unless the output is byte-for-byte equal to the reference.

A subsequent synthetic A/B experiment executed the pinned QSSTV RS1 decoder
on Decodium-encoded objects of 10,546 and 10,547 bytes. On this macOS ARM64
build QSSTV returned 10,745 bytes for each: the original bytes followed by 199
and 198 zero bytes respectively. Decodium recovered both exact inputs. The
result matches the observed 199-byte difference and demonstrates that the
signed-char length calculation can produce it, but the synthetic experiment
alone does not establish the original recording's RS level or prove that it
caused that recording's discrepancy. Reproduce with
`bash tests/sstv/tools/run_qsstv_rs_oracle.sh` against the pinned reference.
Replay of the full second recording through BlackHole 2ch into Decodium's
macOS application then produced the same 10,546-byte image, with 390 groups
delivered and no dropped audio samples. Artifacts are in
`decodium-qsstv-drm-app-3kx11p6x` under the macOS temporary directory. This is
a recorded-signal application test, not a live EasyPal application or RF
proof. The first recording still does not
complete (with the experimental selective estimator it yields 139 of 245
segments); its separate channel issue remains open.
Further offline investigation identifies the first object's `.rs2` container.
At the normal diagnostic pilot limits, 106 segments (12,296 bytes) are
missing. An intentionally permissive post-hoc run (pilot limit 10, corrupted
fraction 1, experimental interpolated channel) yielded 186 of 245 segments;
the RS2 decoder reconstructed the other 59 segments from 6,844 erasures and
the resulting file was byte-for-byte equal to the independent QSSTV result.
An unrelated reference was rejected. This establishes a bounded diagnostic
recovery, not that the relaxed pilot limits are appropriate for normal use.
The application exposes those limits only to an explicitly enabled test-mode
HAMDRM lab through `DECODIUM_LAB_HAMDRM_SELECTIVE=1`; regular sessions retain
their existing pilot validation.
The first recording was then replayed via BlackHole 2ch into the macOS
Decodium application with this diagnostic option and profile
`hamdrm-B-2500-high-16-QAM-long`. The controller's RS recovery produced the
same 21,191 bytes as the QSSTV output; `cmp` and the replay harness both
passed. Artifacts are in `decodium-qsstv-drm-app-seaiggj9` under the macOS
temporary directory. A test with the environment variable set outside the
HAMDRM lab did not activate the experimental path. This app-level result is
still a recorded signal and uses permissive diagnostic limits: it does not
demonstrate robust normal-operation reception, a live EasyPal application,
other operating systems, or an RF link.

With the default 0.12/0.30 pilot limits, adaptive estimation and two
independent timing candidates, the offline analyzer reconstructed the
first EasyPal-labelled recording exactly after RS2 recovery. A single
adaptive candidate obtained 152 of 245 segments; combining validated groups
from both obtained 198 segments and repaired the remainder. Both decoded the
second recording to the same 10,546-byte object, retaining the previously
documented 199-byte zero-padding difference from QSSTV's reference.

An application-level A/B then replayed the same first recording through
BlackHole using the strict pilot limits. The single-candidate control did
not reconstruct the image and delivered 164 groups; the dual-candidate run
reconstructed the independent 21,191-byte QSSTV reference exactly and
delivered 312 groups, including 148 from the early candidate. Both reported
zero rejected chunks and queue overflows. Artifacts are
`.../decodium-qsstv-drm-app-1vveao4b` and
`.../decodium-qsstv-drm-app-eshiny1v`. Adaptive dual timing remains explicitly
test-mode only. These are comparisons of one recorded signal on macOS ARM64,
not evidence of a live EasyPal exchange, other platforms or RF reliability.
With the same strict-pilot dual-candidate laboratory configuration, two
Decodium app pairs also passed the 27% echo A/short and B/long checks with
no rejected chunks or queue overflows and restored legacy PCM
(`.../decodium-hamdrm-two-app-a-d89w2hyb` and
`.../decodium-hamdrm-two-app-b-0a4aa4le`). A full application replay of the
second EasyPal-labelled recording received the same 10,546-byte Decodium
object with no rejected chunks or overflows; the unmodified QSSTV output is
the same prefix followed by 199 zero bytes. The early candidate delivered
390 groups, which may duplicate the primary candidate. Artifact:
`.../decodium-qsstv-drm-app-zmibjy4b`. This is still not a byte-exact match
to the entire QSSTV output or evidence for a live EasyPal application.
Two additional Decodium-to-Decodium control transfers, A and B, also passed
byte-for-byte with the experimental lab option active; their RX logs confirm
that the option was applied. Their artifacts are
`decodium-hamdrm-two-app-a-758riiaf` and
`decodium-hamdrm-two-app-b-su65jp6a` in the macOS temporary directory.
In a subsequent estimator experiment, the leave-one-out pilot residual was
normalised by the larger of predicted and observed energy, bounding an outlier
instead of allowing division by a nearly zero prediction to dominate the
frame score. The pilot-outlier test verifies the bound and detects corruption;
two seeded noise inputs produced no decoded groups with either the normal or
relaxed diagnostic thresholds. On the first recording at 0.12/0.30 the
experimental receiver improved from 139 to 152 received segments, still short
of complete recovery. At the post-hoc 0.40/0.60 setting RS2 recovered an exact
match with 192 segments received; that setting is diagnostic and has not
been adopted for ordinary operation. Both independent QSSTV control images
still reconstructed exactly with the new metric.
With the bounded metric, retaining the old lab threshold of 10/1 could admit
more pilot candidates; a two-application control transferred A but failed
to complete B (`decodium-hamdrm-two-app-b-keia249a`). The precise cause of
that failure was not established. The diagnostic lab now uses 0.40/0.60
instead; this is still a post-hoc experimental setting, not a production
recommendation.
After changing the lab to 0.40/0.60, two Decodium instances transferred both
control images byte-for-byte. The macOS artifacts are
`decodium-hamdrm-two-app-a-uq_z9uzj` and
`decodium-hamdrm-two-app-b-5l5dgz0c` under the temporary directory.
The first EasyPal-attributed recording was also replayed into the application
with 0.40/0.60: the received 21,191-byte file matched the QSSTV reference
exactly. The retained macOS artifact is `decodium-qsstv-drm-app-q1w4suu3`.
This strengthens the recorded-signal evidence only; the estimator and limits
remain experimental and are not active in an ordinary receive session.

#### CRC-gated flat packet retry experiment

An expanded native waveform comparison tested all 72 named profile tuples,
two distinct MOT body groups, and both flat and adaptive estimators. All
clean cases reconstructed both groups with both estimators. For each profile,
two deterministic two-tap channels mixed the waveform with a 1 ms delayed
copy at gains 0.1 and 0.3, then scaled the sum by 0.5: 144 impaired cases in
total. At the ordinary 0.12/0.30 pilot limits the flat receiver recovered both
groups in 120 cases, adaptive in 118. In two A/2500/normal/16-QAM cases,
short and long interleaving with the 0.3 echo, flat recovered both groups
while adaptive lost one due to a packet CRC failure. Neither estimator
emitted a group outside the two inputs. These are native synthetic PCM tests,
not independent transmitters or RF channels.

The experimental adaptive receiver can now retain a second, bounded flat
MSC deinterleaver. When its selected estimate fails both hard and soft packet
CRC checks, it retries the flat cells using the same hard/soft decoding and
accepts only a packet with a valid channel CRC. Both paths feed a single
packet reassembler, so this mechanism does not itself duplicate data groups.
The primary FAC, pilot and MOT checks remain in force. With this option, the
same matrix recovered both groups in 120 cases, with zero regressions against
successful flat or adaptive cases; the two missing packets were recovered by
the retry. The other 24 cases remained incomplete. Noise and damaged-packet
tests did not deliver unexpected groups, and a reset test checked that the
additional deinterleaver does not retain old stream state. CTest
`test_hamdrm_waveform` covers the matrix and integrity controls.
On macOS ARM64 a separate Apple Clang ASan/UBSan build passed both
`test_hamdrm_waveform` and `test_hamdrm_capture_analyzer` (104.80 s and
20.16 s); the CTest log contained no address or undefined-behaviour report.
Leak detection was disabled because it is unsupported by macOS ASan. This
does not establish Windows or Linux sanitizer results.

On the two recordings labelled EasyPal, offline adaptive retry preserved the
previous results: the first reconstructed the 21,191-byte QSSTV reference
after RS2, and the second reconstructed the object with the previously
reported 199-byte trailing-zero difference. Neither recording needed a flat
packet retry. These are recordings of uncertain origin, not live EasyPal
interoperability. Three local runs on this Mac took a median 0.628 s without
the retry and 0.648 s with it for the first recording; this is only a small
offline measurement, not a cross-platform real-time performance guarantee.
The first recording was also replayed through BlackHole into the full Decodium
app with the experimental retry enabled; its 21,191 bytes matched the QSSTV
reference exactly and the process exited successfully. Artifact:
`decodium-qsstv-drm-app-azklky77` in the macOS temporary directory.
The second recording also yielded a 10,546-byte JPEG2000, identical to the
previously extracted Decodium image and to the prefix of the independently
decoded QSSTV file; QSSTV's remaining 199 bytes are zero. FFmpeg decoded both
to the same 320×480 frame MD5 `5668157c77eece42195b4c780dfc3ecf`. The app
exited zero, but the replay harness itself exited nonzero because the command
mistakenly supplied the 12,240-byte RS container as its expected image. The
comparison above was performed separately against the correct extracted image
and the independent QSSTV output; it is not an automatic harness pass.
Artifact: `decodium-qsstv-drm-app-4_i5duxy`. A repeat with the correct
10,546-byte expected image passed the automatic application harness exactly;
its artifact is `decodium-qsstv-drm-app-y64mcup2`. The 199-byte zero-padding
difference from the independent QSSTV file remains as described.

The app enables the experiment only with `--test-mode --lab-hamdrm` and both
`DECODIUM_LAB_HAMDRM_SELECTIVE=1` and
`DECODIUM_LAB_HAMDRM_FLAT_FALLBACK=1`; the normal receiver stays unchanged.
Two isolated Decodium process pairs transferred different JPEGs byte-for-byte
with the option enabled. A exercised a pending legacy monitor start and B an
already active legacy monitor; both verified fresh legacy PCM after the
handoff. Their artifacts are `decodium-hamdrm-two-app-a-63f8snpw` and
`decodium-hamdrm-two-app-b-cf0f49ch`. Repeated BSR A/B tests also passed with
artifacts `decodium-hamdrm-two-app-a-evtia1zg` and
`decodium-hamdrm-two-app-b-pi9rqcus`. These clean transfers prove option
activation and the app paths, not that a retry happened in those transfers.

The diagnostic `export_hamdrm_fallback_fixture` produces A/2500/normal/16-QAM
short and long synthetic test streams. At 12 kHz the analyzer recovered two
body groups with one retry in each, versus one group without it; these are
incomplete objects, not images. Replay through the full app and BlackHole of
the short stream with a 0.3 echo did not recover the groups, either after
12-to-48 kHz conversion or when generated natively at 48 kHz; a matching
clean 48 kHz control did deliver both groups. These negative application
tests are `decodium-qsstv-drm-app-nhhcjhx1`,
`decodium-qsstv-drm-app-h45l2aux`, and
`decodium-qsstv-drm-app-z3c18w6b`; the clean control is
`decodium-qsstv-drm-app-yxifc4wk`. The test fixtures were repeated with
silence gaps in the later attempts. Their failure does not prove which stage
caused the difference from the offline test. This remains experimental until
the impaired application path, additional profiles, CPU limits, Windows and
Linux have been tested.

With the same repeated 48 kHz fixture, 0.2 and 0.25 echo gains yielded groups
in the full application without invoking the fallback; the lab deliberately
reported failure because its acceptance condition required an actual fallback.
Artifacts are `decodium-qsstv-drm-app-65aa2kyp` and
`decodium-qsstv-drm-app-yejelc4c`. These are post-hoc diagnostic gain points,
not operating limits or measured RF propagation conditions.

At a post-hoc 0.27 echo gain, repeated native 48 kHz short- and long-depth
fixtures were replayed through BlackHole into the full macOS application.
With the retry enabled, each run delivered the first two synthetic MOT body
groups and the trace recorded two CRC-validated flat packet recoveries. With
the retry disabled on the same two input WAVs, neither run delivered a group;
both showed packet CRC failures. Retry-on artifacts are
`decodium-qsstv-drm-app-kwbb48ee` and `decodium-qsstv-drm-app-3v05snrb`;
retry-off controls are `decodium-qsstv-drm-app-ln_jaoes` and
`decodium-qsstv-drm-app-andewo77`. The groups intentionally have no object
headers and cannot form images; repeated transmissions can produce repeated
group callbacks. This demonstrates the retry in the actual app's audio path
under this constructed condition, not an on-air image transfer or a general
channel threshold. The paired clean image and BSR two-app tests above cover
separate application paths.

`tests/sstv/tools/run_hamdrm_fallback_app_lab.py short` and `long` reproduce
the enabled/disabled application comparison on macOS with BlackHole and
FFmpeg. The scripts generate the two incomplete body groups, require the
disabled run to show CRC failures and zero delivered groups, and require the
enabled run to show both delivery and a nonzero retry count. Both commands
passed; their orchestration artifacts are
`decodium-hamdrm-fallback-ab-9gjjx4ae` and
`decodium-hamdrm-fallback-ab-m_l7etej` under the macOS temporary directory.
Build `export_hamdrm_fallback_fixture` and `decodium_qml` first. An initial
short run of the wrapper failed because its traceback parser matched a source
line rather than the exception; the parser was corrected before these passes.

#### Two actual Decodium applications through a live echo relay

On macOS, two separate Decodium processes exchanged A/B JPEG images using
`hamdrm-A-2500-normal-16-QAM-short` and `...-long`. The transmitter selected
BlackHole 16ch and the receiver BlackHole 2ch. A PortAudio relay copied the
first stereo pair between the devices, mixed a 48-sample (1 ms) delayed copy
at gain 0.25, and scaled the sum by 0.5. The relay reported no input/output
errors or clipping. With the lab adaptive retry enabled, both images matched
their sources byte-for-byte and the RX trace reported seven recovered packets
in each run. With retry disabled under the same configuration, neither image
completed; each trace recorded seven packet CRC failures. The successful A/B
app artifacts are `decodium-hamdrm-two-app-a-ezhxwjne` and
`decodium-hamdrm-two-app-b-xmap6bbt`; the disabled controls are
`decodium-hamdrm-two-app-a-ir7_snjf` and
`decodium-hamdrm-two-app-b-dxql4ujj`. Relay logs are in
`decodium-hamdrm-two-app-echo-ye90d8g9`,
`decodium-hamdrm-two-app-echo-2bt5qc3b`,
`decodium-hamdrm-two-app-echo-k2lchs7u`, and
`decodium-hamdrm-two-app-echo-x953tdzg` respectively, all under the macOS
temporary directory.

Clean-relay A/B controls also passed, as did 0.2-echo A/B; neither required a
flat retry. A 0.27-echo A/short trial failed to complete despite the retry,
with the TX using the 16-channel device and the relay reporting no error;
its app and relay artifacts are `decodium-hamdrm-two-app-a-2gq1txea` and
`decodium-hamdrm-two-app-echo-dd2agb2o`. This is a limit of the tested path,
not a general propagation threshold or a hardware/RF measurement. The relay
lab `tests/sstv/tools/run_hamdrm_two_app_echo_lab.py` accepts the image,
interleaver and echo gain; adding `off` runs an expected incomplete control.
After adding explicit negative-control checks to the lab, all four runs
passed again:

```sh
python3 tests/sstv/tools/run_hamdrm_two_app_echo_lab.py a short 0.25
python3 tests/sstv/tools/run_hamdrm_two_app_echo_lab.py b long 0.25
python3 tests/sstv/tools/run_hamdrm_two_app_echo_lab.py a short 0.25 off
python3 tests/sstv/tools/run_hamdrm_two_app_echo_lab.py b long 0.25 off
```

The corresponding relay artifact directories are
`decodium-hamdrm-two-app-echo-70acqb4y`,
`decodium-hamdrm-two-app-echo-8djpp6zr`,
`decodium-hamdrm-two-app-echo-fv_93w8l`, and
`decodium-hamdrm-two-app-echo-o8xoy_6o`. Each disabled run verifies that TX
completed, the relay carried audio without error, no image was saved, and
packet CRC failures occurred. Each enabled run checks the received JPEG bytes
and reports seven recovered packets. Both virtual devices are macOS-specific
laboratory dependencies; this is not a Windows/Linux or RF test.
The `off` control above means the adaptive estimator without its flat retry;
it is not the ordinary production flat receiver. With `flat` selected instead,
the ordinary receiver also completed both 0.25-echo images byte-for-byte,
without retries. Its A/B app artifacts are `decodium-hamdrm-two-app-a-1k6wemn9`
and `decodium-hamdrm-two-app-b-vqw9loe5`. Thus this particular experiment
demonstrates recovery of an adaptive-path regression; it does not demonstrate
an improvement over the ordinary flat path.

At the post-hoc 0.27 gain, the ordinary flat receiver failed A/short but
completed B/long; the adaptive receiver with retry also completed B/long,
with ten retry packets, while the earlier adaptive A/short attempt failed.
The flat A/B app artifacts are `decodium-hamdrm-two-app-a-cuahl5wu` and
`decodium-hamdrm-two-app-b-82qw5ojk`; the adaptive B artifact is
`decodium-hamdrm-two-app-b-o7xq4p28`. The official QSSTV batch receiver also
recovered B exactly from the flat test's saved relay audio; its artifact is
`decodium-qsstv-drm-decode-qmwd54ws`. These outcomes depend on image and
interleaver, and are not a general operating threshold.
For the failing A/short 0.27-echo case, the ordinary flat receiver was
repeated with relay recording enabled: the Decodium transfer again did not
complete, while the unmodified QSSTV batch receiver decoded the saved relay
audio to the exact 225-byte JPEG. The relay reported no error or clipping.
The relay and QSSTV artifacts are `decodium-hamdrm-two-app-echo-ov4krxkh`
and `decodium-qsstv-drm-decode-e7gwzwz3`. This is a reproducible receiver
gap for one constructed waveform; it does not by itself establish the cause
or broader on-air performance.
After the lab gained selectable TX output devices, ordinary two-app A/B
transfers with its default BlackHole 2ch settings also passed byte-for-byte.
Their artifacts are `decodium-hamdrm-two-app-a-0wa9r1ei` and
`decodium-hamdrm-two-app-b-jl2ai64y`.

The live relay can optionally retain the exact stereo float samples submitted
to its BlackHole 2ch output by setting `DECODIUM_LAB_ECHO_CAPTURE=1`.
Repeating A/short and B/long with the 0.25 echo again produced byte-exact
images in Decodium and seven retry recoveries each. The unmodified,
SHA-256-pinned QSSTV-Experimental 10.0 application then decoded each saved
relay stream in batch mode and recovered the same 225-byte JPEGs exactly.
QSSTV reported `A/2.5/Lo/16/Short` and `A/2.5/Lo/16/Long`, respectively,
with 15 MSC CRC successes and no MSC CRC failures in each. Its `pkt_bad=1`
includes intermediate fragments as noted elsewhere in this document. The
Decodium/relay artifacts are `decodium-hamdrm-two-app-a-euo13yzi` /
`decodium-hamdrm-two-app-echo-t677uckd` and
`decodium-hamdrm-two-app-b-gldkfur6` /
`decodium-hamdrm-two-app-echo-nkeugo4s`. QSSTV logs and outputs are
`decodium-qsstv-drm-decode-t83svdws` and
`decodium-qsstv-drm-decode-o5fwdqff`. QSSTV decoded a recording of samples
submitted to the virtual output, not live audio or RF. Its released macOS
bundle logged duplicate Qt-framework warnings. These narrow A/B results do
not establish EasyPal, other profiles, difficult channels, or platform-wide
interoperability.

An additional controlled two-tap channel mixed each of two independent QSSTV
PCM captures with a 1 ms delayed copy at amplitude 0.3, then scaled the sum by
0.5. At 0.12/0.30 the flat receiver reconstructed both reference files exactly;
the selective-only receiver reconstructed neither (26 and 38 groups). At
amplitudes 0.1 and 0.2 both estimators reconstructed both captures exactly.
This is a deterministic offline impairment, not an RF measurement. A separate
`aecho` sweep with delays of 1, 2, 4 and 6 ms and feedback parameters 0.4 and
0.8 yielded no complete frames for either capture or estimator; for the 1 ms,
0.4 case the selective diagnostic observed pilot validation and singular
estimate failures. Those observations do not establish the root cause.

The offline analyzer's `hybrid` experiment runs the normal flat receiver and
the experimental selective receiver independently, merging only decoded MOT
groups into the same bounded assembler. The two 0.3-echo controls reconstructed
exactly with no assembler ingest errors; the selective branch alone did not.
On the first EasyPal-attributed recording the selective branch at 0.40/0.60
plus RS2 recovered the exact reference, while the flat branch delivered no
groups. The second recording completed with the same 199-byte zero-padding
difference noted above. Automated tests exercise independent A/B images with
the two-tap echo and reject wrong references. This is an offline experiment:
resource usage, interference between independent receive paths, live capture,
and cross-platform behavior require further validation before considering it
for ordinary application sessions.

An alternative experimental `adaptive` mode uses a single receiver and selects
the flat or interpolated estimate separately for each symbol using the lower
reported pilot error. At diagnostic limits 0.12/0.30 it reconstructed both
0.3-echo QSSTV references byte-for-byte; at 0.40/0.60 it also recovered the
first EasyPal-attributed reference through RS2. The second recording yielded
the same 199-byte reference padding difference described above. A/B test
fixtures and two deterministic noise inputs pass. The two reported error
measures originally had different normalisations. A follow-up evaluates the
flat estimate with the same bounded leave-one-out residual as the interpolated
estimate; the choice and thresholds remain uncalibrated for general
propagation conditions. The application exposes
this adaptive mode only through the explicit experimental lab opt-in; the
ordinary receive path remains unchanged.
The comparable scoring retained exact offline reconstruction of both 0.3-echo
QSSTV references and the first EasyPal-attributed recording after RS2 recovery;
the second recorded reference still differs only by its 199 trailing zero
bytes. A/B tests cover flat and affine synthetic channels and pilot outliers.
With comparable scoring enabled in the lab, two Decodium instances also
transferred both local A/B images byte-for-byte; temporary artifacts are
`decodium-hamdrm-two-app-a-2pif0rca` and
`decodium-hamdrm-two-app-b-2mqeij2a`. Tests additionally reject non-finite
and overflowing equalizer input.
An application replay of the first EasyPal-attributed recording with the
comparable metric recovered exactly 21,191 reference bytes. Its macOS
temporary artifact is `decodium-qsstv-drm-app-mecc88z7`; this remains an
experimental recorded-signal result, not a normal-path or RF claim.
Both 0.3-echo recordings were replayed into the app with the comparable
metric, recovering 4,998 and 5,021 reference bytes exactly; artifacts are
`decodium-qsstv-drm-app-ifborniv` and `decodium-qsstv-drm-app-yp5xf_dd`.
The first readiness snapshot reports `localAudioTapConnected=true` and
`legacyAudioTapConnected=false`. It therefore does not verify reception through
the macOS legacy audio tap; that route requires a separate test.
Follow-up on the legacy transition: the first manual A/B test showed that
legacy monitor startup could still be pending when HAMDRM requested native
capture. The delayed startup was not previously included in SSTV's restore
decision and could reopen the endpoint while the native path owned it. SSTV
now cancels that pending request and restores the legacy monitor on exit.
Two macOS Decodium instances transferred different images while testing both
cases: A started HAMDRM during the pending legacy startup, B after legacy
monitoring had become active. Both transfers were byte-exact, the legacy
backend stayed quiescent during native capture, and fresh legacy PCM samples
arrived after restoration. The adaptive lab estimator was enabled. Temporary
artifacts are `decodium-hamdrm-two-app-a-gua3vxa8` and
`decodium-hamdrm-two-app-b-mwfi4zof`. This proves the tested FT8 legacy-to-
native-to-legacy handoff on this Mac; it is not an RF or Windows/Linux test.
The same pending/active handoff and fresh-PCM checks passed using the ordinary
flat HAMDRM receiver, with artifacts `decodium-hamdrm-two-app-a-vm1o2wt_` and
`decodium-hamdrm-two-app-b-llrjso1x`. As negative controls, the lab explicitly
stopped monitoring immediately before RX and verified that it was actually
off; two further byte-exact A/B transfers then left both monitor states off.
Those artifacts are `decodium-hamdrm-two-app-a-uj0v57l_` and
`decodium-hamdrm-two-app-b-27rdolu5`. An earlier attempt using only
`--lab-no-monitor` was discarded as an off-state test because the legacy
monitor had already requested startup before RX; the baseline was not off.
Two Decodium-to-Decodium app transfers with the adaptive lab mode enabled
matched images A and B byte-for-byte; their temporary artifacts are
`decodium-hamdrm-two-app-a-st7g_b0k` and
`decodium-hamdrm-two-app-b-6saf_qld`. The first EasyPal-attributed recording
was also replayed into the app and produced exactly 21,191 reference bytes;
its temporary artifact is `decodium-qsstv-drm-app-5yvems3c`. These are local
macOS tests and do not establish live RF or Windows/Linux behavior.
The two 0.3-echo QSSTV recordings were subsequently replayed through
BlackHole into the adaptive lab receiver in Decodium. Both resulting images
matched their references byte-for-byte (4,998 and 5,021 bytes). The temporary
artifacts are `decodium-qsstv-drm-app-d6xxfz9v` and
`decodium-qsstv-drm-app-vlq4c8a1`; these are impaired recorded-signal replays,
not proof of RF multipath handling or of the ordinary receiver path.
The corrected receiver was also exercised with two local Decodium instances
over BlackHole 2ch for independent images A and B; both transfers matched their
source bytes (225 bytes each). Artifacts were retained under the macOS temporary
directory as `decodium-hamdrm-two-app-a-mb5on1lh` and
`decodium-hamdrm-two-app-b-_o_7vq27`. This is a native Decodium loopback,
not independent EasyPal or radio evidence.

A developer-only harness executes the actual `CMOTDABEnc` implementation in
QSSTV commit `8c27d6d169d8c6c197eb47c2089870e39bc06a02`. It produces a
synthetic four-byte object named `sample.jpg`; those bytes are not a valid JPEG
and this exercise tests MOT framing rather than image decoding. Its empty
format argument intentionally takes QSSTV's default JPEG subtype, avoiding a
pinned-source undefined string transform for nonempty formats. The harness is
separate from Decodium and does not build or distribute QSSTV code.

On the tested macOS environment with Qt 6 available through pkg-config:

```sh
git clone https://github.com/ON4QZ/QSSTV.git /tmp/decodium-qsstv-reference
git -C /tmp/decodium-qsstv-reference checkout --detach 8c27d6d169d8c6c197eb47c2089870e39bc06a02
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/datadecoding \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core) tests/sstv/tools/qsstv_mot_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/datadecoding/DABMOT.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/util/CRC.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core) -o /tmp/qsstv-mot-oracle
/tmp/qsstv-mot-oracle
/tmp/qsstv-mot-oracle segmented
```

The default output lines are the header and first body group. The optional
`segmented` argument emits a 103-byte object in three segments, including
QSSTV run-in order (last segment first) and a duplicate. They are embedded in
`test_hamdrm_core`, which parses and assembles them and compares Decodium
output as described below. The first comparison exposed an actual
CRC mismatch: QSSTV uses MSB-first polynomial `0x1021`, initial `0xffff`,
final xor `0xffff`, whereas the previous Decodium MOT path used reflected
CRC-16/X-25. The MOT path now uses the existing MSB-first channel CRC routine.
The reflected utility remains available but is not used for MOT. For historical
local partial snapshots, load verifies the full file SHA-256
first, checks each old reflected CRC and translates matching groups in memory;
it leaves the original file unchanged. Live or network groups with the old CRC
are rejected. A focused persistence test covers this recovery.

The segmented fixture is parsed and assembled in QSSTV transmission order;
Decodium-generated headers and segment payloads are compared separately from
continuity indices, which QSSTV increments for every repeated transmission.

The packet oracle executes QSSTV `CDataEncoder::GeneratePacket`, with a stub
only for fetching the next data unit after the three packets under test:

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/datadecoding \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_packet_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/datadecoding/DataDecoder.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/datadecoding/DABMOT.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/util/CRC.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-packet-oracle
/tmp/qsstv-packet-oracle
```

Its three packets, including the padded last packet, are byte-identical to
Decodium output and reassemble into the same 35-byte data unit in
`test_hamdrm_waveform`. These checks prove executed QSSTV-encoder-to-Decodium
MOT and packet vectors plus byte-level generation comparisons. They do not
prove execution of
QSSTV's decoder, a real image, BSR exchange, OFDM waveforms, two applications,
EasyPal or over-the-air interoperability. The restricted upstream files listed
in `UPSTREAM_PROVENANCE.md` are not compiled or copied into Decodium.

## FAC convolution and bit interleaver

The channel oracle invokes the pinned QSSTV `CConvEncoder` for a deterministic
48-bit FAC input (pattern 6, 45 cells), and `CBitInterleaver` on a 254-bit
input with `t0=13`. It can be reproduced on the same macOS/Qt environment:

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/mlc \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_channel_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ChannelCode.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ConvEncoder.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/BitInterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/interleaver/BlockInterleaver.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-channel-oracle
/tmp/qsstv-channel-oracle
```

`test_hamdrm_channel` checks exact output against both independently executed
vectors and recovers each original input using Decodium's inverse operations.
These are two individual coding-layer vectors, not an independent FAC frame,
MSC profile matrix, RF waveform or QSSTV receiver test.

## Complete FAC coding-chain fixture

`qsstv_fac_oracle.cpp` invokes QSSTV's FAC parameter writer, energy dispersal,
convolutional encoder, bit interleaver and QAM mapper independently of the
Decodium implementation. It emits 288 combinations: three frame identities,
two bandwidth flags, two interleaver depths, three MSC constellation flags,
two protection flags and four packet IDs, using `TEST123` as the label. The
fixture stores the complete 48-bit FAC payload and the signs of the 45 QAM
cells; it does not measure their floating-point amplitude or an OFDM signal.
The checked-in `qsstv-fac-8c27d6d.txt` has SHA-256
`73354262eb2e96984084725e21cdfb865bec773a03016770e7e83106b8d0397b`.

The FAC data vector is explicitly zero-initialized before the QSSTV writer
runs, so the reserved bits have a defined value. The harness invokes the same
QSSTV primitives in the order used by its FAC MLC encoder; it does not invoke
the whole QSSTV application or its receiver. Build and regenerate it on macOS:

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/FAC \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/mlc \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_fac_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/FAC/FAC.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/Parameter.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/CellMappingTable.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/util/CRC.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ChannelCode.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ConvEncoder.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/BitInterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/interleaver/BlockInterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/EnergyDispersal.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/QAMMapping.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-fac-oracle
/tmp/qsstv-fac-oracle > /tmp/qsstv-fac-vectors.txt
cmp /tmp/qsstv-fac-vectors.txt tests/sstv/fixtures/qsstv-fac-8c27d6d.txt
```

`test_hamdrm_channel` checks every fixture row for exact payload and QAM sign
agreement and decodes the corresponding normalized cells. This supports FAC
coding compatibility for these fields and input label; it does not establish
cell placement, frame synchronization, MSC waveform or on-air interoperability.

## Complete MSC MLC fixture

`qsstv_msc_oracle.cpp` invokes the pinned QSSTV `CMSCMLCEncoder` initialization
and its actual `ProcessDataInternal` implementation. A developer harness
supplies input/output vectors and stubs only a logging method needed by an
unused inherited buffer-interface vtable. It sets the useful cell count to
339, 369, 455, 530, 647 or 704, then executes QAM4/QAM16/QAM64 at each of two
protection levels. These are 36 combinations using a deterministic LCG input;
the six counts match Decodium's currently documented cell-plan counts, but
this oracle does not independently validate how either implementation derives
those counts or arranges the cells in a frame.

Each output cell is quantized to an integer constellation level, separately
for real and imaginary components, and stored as a letter representing -7..7.
The fixture `qsstv-msc-8c27d6d.txt` has SHA-256
`7d4bfdeabc87c5b3a305c750d7131bde3ee34fb67829f2f6aa8a8bfc2bb5883f`.
Decodium compares every quantized cell and input capacity and decodes the
normalized reference symbols to the original input in `test_hamdrm_channel`.
This establishes individual MLC encoding/decoding comparisons; it is not a
sampled waveform or a QSSTV receiver execution.

On the tested macOS/Qt environment the fixture can be reproduced with:

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/mlc \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_msc_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/MLC.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/Parameter.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/CellMappingTable.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ChannelCode.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ConvEncoder.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/BitInterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/interleaver/BlockInterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/EnergyDispersal.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/QAMMapping.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-msc-oracle
/tmp/qsstv-msc-oracle > /tmp/qsstv-msc-vectors.txt
cmp /tmp/qsstv-msc-vectors.txt tests/sstv/fixtures/qsstv-msc-8c27d6d.txt
```

## Cell-map oracle

The developer-only `tests/sstv/tools/qsstv_cellmap_oracle.cpp` invokes the
pinned QSSTV `CCellMappingTable::MakeTable` for all six A/B/E and 2300/2500 Hz
profiles. The checked-in fixture records the dimensions, useful MSC count,
the QSSTV cell flags for every carrier in every superframe symbol, and
the complex values of the pilot cells.
`test_hamdrm_channel` compares each cell's MSC, FAC, time, frequency,
scattered, and boosted-pilot flags against this independent output. The
pilot complex values are compared with an absolute tolerance of 1e-6. This
does not establish over-the-air reception or full QSSTV interoperability.

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_cellmap_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/CellMappingTable.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-cellmap-oracle
/tmp/qsstv-cellmap-oracle | cmp - tests/sstv/fixtures/qsstv-cellmap-8c27d6d.txt
/tmp/qsstv-cellmap-oracle pilots | cmp - tests/sstv/fixtures/qsstv-pilots-8c27d6d.txt
```

## Independent complex OFDM symbols

`qsstv_ofdm_oracle.cpp` executes QSSTV's `COFDMModulation` at the 350 Hz
carrier offset used by its transmitter. The fixture records every complex
sample, including the guard interval, for two deterministic carrier patterns
in each of the six A/B/amateur-E and 2.3/2.5 kHz profiles. Its SHA-256 is
`afd58978cde134e99bf7d29cdc6e57878e15eb58757dc7f9f50420f938769791`.
`test_hamdrm_phy` compares Decodium's scaled, frequency-shifted symbols to
these samples and demodulates the QSSTV symbols back to their input carriers,
using a 2e-7 absolute complex tolerance. The reference is an independent
executed OFDM modulation stage, not a complete QSSTV transmitter or receiver,
real audio exchange, or evidence of EasyPal interoperability.

To regenerate on the tested macOS/Qt environment, using the pinned checkout:

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network fftw3) \
  tests/sstv/tools/qsstv_ofdm_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/OFDM.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/Parameter.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/CellMappingTable.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/matlib/MatlibStdToolbox.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network fftw3) \
  -o /tmp/qsstv-ofdm-oracle
/tmp/qsstv-ofdm-oracle | cmp - tests/sstv/fixtures/qsstv-ofdm-8c27d6d.txt
```

## Independent symbol cell mapper

`qsstv_mapper_oracle.cpp` calls QSSTV's `COFDMCellMapping` for two
independent input patterns across all six A/B/amateur-E bandwidth profiles and
all symbols in each three-frame superframe. Its fixture has SHA-256
`1e2ec0734118bbaba8042b091cb7b5242537bd1321763792f9352699fe29122a`.
`test_hamdrm_channel` compares each output cell and extracts the original FAC
and MSC from the reference cells while checking pilots. This tests executable
mapping and frame sequencing in addition to the earlier static flag oracle;
it is not a complete QSSTV waveform exchange.

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_mapper_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/OFDMCellMapping.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/Parameter.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/CellMappingTable.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-mapper-oracle
/tmp/qsstv-mapper-oracle | cmp - tests/sstv/fixtures/qsstv-mapper-8c27d6d.txt
```

## Synthetic QSSTV PCM through the native receiver

`qsstv_mapper_ofdm_oracle.cpp` executes QSSTV's FAC coder (when requested),
cell mapper and OFDM modulator together, keeping oscillator phase continuous
for a three-frame 48 kHz, 16-bit little-endian PCM superframe. It uses the
amateur E / 2.5 kHz / QAM16 / high-protection profile. Two input patterns are
provided for each case. With synthetic invalid FAC, Decodium synchronizes,
accepts the pilots and rejects FAC. With separately QSSTV-encoded valid FAC
and short interleaving, Decodium accepts frames and reaches an MSC/packet
integrity failure, as expected for the intentionally random MSC cells. Both
outcomes agree across two PCM chunk sizes and emit no false MOT group.

The fixture SHA-256 values are:

- `qsstv-superframe-a-8c27d6d.pcm`: `e1d715b51f54dc19e2848fb6b608b240e337e2318ef5470773b17cc8f5e1f584`
- `qsstv-superframe-b-8c27d6d.pcm`: `c6cf61c4ac2025a1ed2bd20b93e5af539aab6f1ebb006d0a464d33280f5d1a21`
- `qsstv-valid-fac-a-8c27d6d.pcm`: `9bda9b25e6fa0c9d3424042a09d71c213a8d510a7c8dcd88dbe07ff42ffaf2c0`
- `qsstv-valid-fac-b-8c27d6d.pcm`: `fd6acbe03c201ea260ab472ed13d30bfb28110112c8bb9fda9b7f3826843ddf1`

These are generated stage outputs, not recorded audio or a complete QSSTV
MOT transfer. They establish neither image delivery nor EasyPal compatibility.

## QSSTV-generated MOT over PCM

The same developer oracle now runs QSSTV's packet encoder, MSC MLC encoder,
short symbol interleaver, FAC encoder, cell mapper and OFDM modulator in
sequence. The two input data groups were generated separately by QSSTV's MOT
encoder: the header and body for `sample.jpg` with the synthetic four-byte
body `12 34 56 78`. Separate PCM fixtures repeat each group and alternate the
header and body in both orders. `test_hamdrm_waveform` decodes these PCM
streams with two chunk sizes, compares the exact MOT groups, and assembles
both alternating streams to the expected four-byte object. This is a
synthetic object, not a valid JPEG or an exchange with the QSSTV application.
Only E / 2.5 kHz / QAM16 / high protection / short interleaving is covered.

- `qsstv-mot-a-8c27d6d.pcm`: `6989a825ef8ef789c77cd4be13446445e1e68141b4753f7edf0a45eeaa43c8e1`
- `qsstv-mot-b-8c27d6d.pcm`: `6610915ff6a3432c248f58ed0bd6321972f0c62a7cba9a5218ae40d453481256`
- `qsstv-object-a-8c27d6d.pcm`: `7ce7528714b022b4ba3e2d9cba241ba161658d9b366dba06255ab72792b8d320`
- `qsstv-object-b-8c27d6d.pcm`: `8bd1bc93ab9b089dbace643c156a9c43f961231bdddcf04655b4b142cd89faa9`

## Two JPEG images over QSSTV-generated PCM

The oracle's `image` mode feeds a real JPEG into QSSTV's `CMOTDABEnc`, then
runs QSSTV packet, FAC, MSC, short symbol interleaving, cell mapping and OFDM
stages. Nine frames contain the MOT header and segmented body, including the
QSSTV run-in order. The isolated probe preserves the object's transport ID
when constructing its packet helper because the helper's unused slideshow
constructor resets QSSTV's global ID; this is a harness correction, not a
change to QSSTV or Decodium.

For two independently generated 16x16 JPEGs, `test_hamdrm_waveform` recovers
the QSSTV groups from PCM using two chunk sizes, assembles the original JPEG
bytes exactly, validates its metadata and dimensions, and decodes the expected
red or blue pixels. The test covers E / 2.5 kHz / QAM16 / high protection /
short interleaving. It does not execute the full QSSTV application or prove
the reverse direction, live audio, other profiles, or EasyPal interoperability.

- `qsstv-image-a.jpg`: `d8f477eda8727eb80f162760af8f93c7d655e5df81566a665cc53ef8a076ec03`
- `qsstv-image-a-8c27d6d.pcm`: `c60bcbeb531102b0cad62804a5c13ec01b240b9d2cc2a21f80d7c4f7db79fc25`
- `qsstv-image-b.jpg`: `14648ca1cfb6c6b186755a23c247bbc3d9b0eacc690897666676750f56b1b3ad`
- `qsstv-image-b-8c27d6d.pcm`: `3d6552bc2982a583f0629d058842bfd8805a8ca1f4bdadd7ca2da1b08f18152c`

For both JPEGs, the test also feeds the nine recovered data groups to the
Decodium transmitter and compares every native 48 kHz output sample with the
corresponding QSSTV-generated reference, allowing 1.1/32768 absolute error
for signed 16-bit quantization. Packet continuity advances modulo eight across
the QSSTV probe's independent group helper calls. This sample comparison is
strong reverse-direction evidence for this exact sequence and profile, but it
does not execute QSSTV's receiver or establish real-world interoperability.

## Decodium FAC into the QSSTV receiver

`tests/sstv/tools/run_qsstv_fac_rx.sh` checks the separate QSSTV checkout's
revision and the three reference source files before building a temporary
executable around its actual `msdhardfac`, Viterbi and deinterleaver code.
The separately built Decodium exporter supplies 288 FAC vectors spanning three
frame identities, two bandwidths, two interleavers, three constellations, two
protection levels and four packet IDs. The QSSTV receiver recovers every
48-bit payload both unmodified and with one entire QAM cell inverted: 576
independent decode checks. No QSSTV source is linked into the application or
copied into this repository. This exercises the receive FAC coding stage,
not QSSTV's full waveform acquisition, MSC or image receiver.

```sh
QSSTV_REFERENCE_ROOT=/tmp/decodium-qsstv-reference \
  tests/sstv/tools/run_qsstv_fac_rx.sh
```

## Decodium MSC into the QSSTV receiver

`tests/sstv/tools/run_qsstv_msc_rx.sh` builds Decodium's independent bit/cell
exporter and a temporary program calling the pinned QSSTV `msdhardmsc`
receiver, deinterleaver and Viterbi decoder. The harness derives Part-B lengths
and puncture settings from the reference receiver's tables. It verifies 72
native MSC vectors: two input patterns for each combination of six cell
counts, QAM4/16/64 and high/normal protection. Pattern B adds a small 0.015
perturbation to each component of one cell. Every output bit matches its
native input. The separate checkout revision and used reference files are
checked before compilation; no upstream source is copied or linked into the
application. This does not test QSSTV's waveform acquisition, symbol
interleaving over time, packet/MOT receiver, or a live image exchange.

```sh
QSSTV_REFERENCE_ROOT=/tmp/decodium-qsstv-reference \
  tests/sstv/tools/run_qsstv_msc_rx.sh
```

## QSSTV receive map against Decodium temporal interleaving

`qsstv_msc_map_rx_oracle.cpp` invokes QSSTV's actual `mkmscmap` and related
receiver tables for six profiles and both interleaving depths. The generated
fixture SHA-256 is
`40be9c866811b2253f134adc0a67675158ce67476b217f02220d28968efc5cd9`.
`test_hamdrm_channel` fills a simulated six-frame receive ring with native
Decodium interleaver output in the native cell plan and applies QSSTV's exact
map. It recovers two distinct deterministic source sequences for all twelve
profile/depth combinations, after the relevant startup delay and through
zero-frame flushing. The expected reference-map row is one step ahead of the
circular write slot. This checks the actual QSSTV receive map, but not its
real-time sample acquisition or a running QSSTV application.

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  $(pkg-config --cflags Qt6Core fftw3) \
  tests/sstv/tools/qsstv_msc_map_rx_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmrx/mkmscmap.cpp \
  /tmp/decodium-qsstv-reference/src/drmrx/mkfacmap.cpp \
  /tmp/decodium-qsstv-reference/src/drmrx/deinterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmrx/drm.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core fftw3) \
  -o /tmp/qsstv-msc-map-rx-oracle
/tmp/qsstv-msc-map-rx-oracle | cmp - tests/sstv/fixtures/qsstv-msc-rx-map-8c27d6d.txt
```

## Local two-instance audio test, 2026-10-31

Two Decodium test-mode instances were connected through BlackHole 2ch, with
CAT disabled and VOX selected. The TX completed for each of two different
225-byte JPEG fixtures; RX captured audio, but neither image was reconstructed.
Logs are under `/tmp/decodium-hamdrm-live-a2` and
`/tmp/decodium-hamdrm-live-b`. The B trace showed four pilot failures before
five accepted frames and a packet continuity error. This is a failed live A/B
test, not evidence of QSSTV or EasyPal interoperability.

The test exposed an acquisition bug where the receiver searched the same
initial silence indefinitely. The receiver now advances past fully examined
candidates and retains the incomplete suffix. A waveform regression tests two
different silence prefixes and passes, but this fix alone did not make the
two-instance image transfer succeed. Initial acquisition and packet continuity
still need investigation.

Follow-up: a rejected pilot candidate previously discarded an entire frame,
including the beginning of any valid frame overlapping that candidate. The
receiver now resumes its search after the rejected candidate. Waveform tests
exercise two different partial-frame prefixes, check that pilot candidates
were rejected, and recover both original MOT groups. With this change, the
two-instance BlackHole test succeeded for both JPEG A and JPEG B on profile
`hamdrm-A-2300-high-4-QAM-long`: TX and RX exited successfully, RX saved each
225-byte object, and `cmp` confirmed byte-for-byte identity. Logs are under
`/tmp/decodium-hamdrm-reacq-a` and `/tmp/decodium-hamdrm-reacq-b`.
Each run reported 113 rejected pilot candidates before lock, so acquisition
efficiency and robustness still require work. These local Decodium-to-Decodium
tests do not establish live interoperability with QSSTV or EasyPal.

A follow-up experiment aggregated cyclic-prefix correlations across four
symbols. In a separate BlackHole A transfer the image remained byte-identical,
but rejected pilot candidates rose from 113 in the previous runs to 126;
the timing of separate real-time runs is not controlled well enough to claim
a precise performance regression. The experimental metric was removed because
it did not demonstrate an improvement. The retained waveform regression also
checks recovery of two distinct MOT groups after deterministic broadband-noise
prefixes of 0.4 and 0.6 seconds at 12 kHz. It is a synthetic acquisition test,
not a measurement of RF interference tolerance.

## QSSTV-stage JPEG PCM into the full Decodium application

On macOS, a separate PortAudio process replayed each of the two pinned
QSSTV-generated nine-frame JPEG PCM fixtures into BlackHole 2ch. A full
Decodium test-mode application received through its normal sound input and
HAMDRM controller on `hamdrm-E-2500-high-16-QAM-short`. The process selected a
fresh isolated profile, verified that SSTV actually captured BlackHole, and
played 0.5 seconds of silence followed by four consecutive fixture repetitions.
For both A and B, Decodium saved the JPEG and its 225 bytes matched the
corresponding source file exactly; both application processes exited zero.
A subsequent test with one fixture transmission also reconstructed each JPEG
byte for byte after the same 0.5-second silent lead-in; that is the current
default in the script. Two successful local runs do not establish acquisition
reliability under noise, clock mismatch or other timing conditions.

`--lab-hamdrm-profile` selects a named profile for test-mode HAMDRM RX and TX;
misuse without `--test-mode`, without `--lab-hamdrm`, or without a value is
rejected before startup workers are created. The registered
`test_hamdrm_lab_cli` covers these cases. Reproduce the real-time audio checks
on macOS with BlackHole 2ch and PortAudio:

```sh
python3 tests/sstv/tools/run_qsstv_pcm_app_lab.py a
python3 tests/sstv/tools/run_qsstv_pcm_app_lab.py b
```

The script prints the separate temporary artifact directory for each run.
The source signals were generated by QSSTV encoder stages, but the replay
process is not the full QSSTV application. This demonstrates one-way delivery
of these two fixtures over a local audio device for this one profile, not a
live QSSTV-to-Decodium application exchange, reverse-direction reception by
QSSTV, EasyPal interoperability or RF performance.

## QSSTV-Experimental application DRM TX into Decodium

On 2026-10-31 a locally built QSSTV-Experimental 10.0 at source revision
`a84584248d3307bfa081362031ff20f365337633` encoded two distinct 320×240
test images to DRM WAV. The build contains the one-line signed-PCM patch
`tests/sstv/fixtures/qsstv-experimental-10.0-signed-pcm.patch`; it is not
the official release binary. QSSTV's own batch DRM receiver recovered JPEG2000
objects of 4,998 and 5,021 bytes. Both were then replayed through PortAudio
and BlackHole 2ch into full, isolated Decodium application processes with
CAT disabled. Decodium reconstructed the exact respective JP2 bytes.
The QSSTV batch report identified E/2.5 kHz/high/16-QAM/long; its command
line had requested `--drm-interleave short`, so the observed FAC/profile,
not the requested flag, was used for Decodium RX. The discrepancy in QSSTV's
CLI/profile behavior remains unresolved.

The source WAVs and QSSTV-decoded JP2 files are under
`/tmp/decodium-qsstv-experimental/drm-large-{a,b}.wav` and
`/tmp/decodium-qsstv-experimental/drm-large-{a,b}_1_drm.jp2`; their WAV
SHA-256 hashes are `ed58cde38c579c6d3ba8a94dfb4a8c9d9e6c339f248d5e7415d74552e233a2f7`
and `8e2be29cecd21fb9308babbcc8a19dce9e208cbf9e2bc63e970c3d79e1985490`.
Decodium artifacts and logs are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-olvzabwg`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-d55elcmp`.
Replay either with `tests/sstv/tools/run_qsstv_exp_drm_app_lab.py WAV JP2`.

The first full-application attempt exposed two defects. On loss of lock the
RX backend reported progress 0 after 0.5, violating the controller's
monotonic progress contract and aborting the session. Progress now indicates
that synchronization has been reached and does not decrease when lock is
lost. Then FAC frames with valid CRC but nonzero unused bits were rejected:
the pinned QSSTV encoder writes no explicit value for those nine bits. The
decoder now ignores their value after checking the CRC; it still validates
profile fields, frame identity and payload CRC. A/B reserved-bit patterns
and bit corruption without a recomputed CRC are covered by the channel test;
the adapter test covers progress after a stream reset.

This is one-way, file-mediated application evidence for a particular profile
and a modified QSSTV build, with no RF or independent security implication.
It does not establish reverse reception, EasyPal, all profile combinations,
or performance under fading and clock mismatch.

## Decodium application TX into QSSTV-Experimental receiver

The first Decodium-to-QSSTV experiment sent a 225-byte JPEG in approximately
4.5 seconds. QSSTV-Experimental 10.0's unmodified macOS release synchronized
and checked some MSC CRCs but produced no image; it acquired too late to
recover the sole MOT header. Repeating that captured waveform did not recover
the image. A longer 7.4 kB JPEG likewise produced data segments but no header
or completed image. Repeating the header before the body allowed QSSTV to
recover headers, but not yet the image before transmission ended.

The native TX adapter now sends each MOT header eight times before the body
and eight times after it, keeping valid frames on air while the receiver
acquires and drains its buffers. The repeated groups count against the
existing bounded queue and byte limits. This increases airtime and reduces
available space within those limits. It does not promise recovery when data
segments are missed.

With this change, two full Decodium application transmissions using
`hamdrm-E-2500-high-16-QAM-long` were captured through BlackHole 2ch and fed
to the official, unmodified QSSTV-Experimental 10.0 receiver in batch mode.
QSSTV reconstructed both distinct 16×16 JPEGs, 225 bytes each, byte-for-byte
equal to the inputs. QSSTV reported 17/16 valid outer CRC blocks, zero bad
outer CRC blocks, and `pkt_bad=2` on each run; it nevertheless
completed the respective image. In this QSSTV version `pkt_bad` also counts
valid intermediate fragments, because `setupDataBlock()` returns false while
waiting for the last fragment; it is not a count of proven invalid packets.
TX durations were 10.816 and 10.621 seconds.
The audio captures and Decodium logs are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-a-63r1r6f0`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-b-8rvtvie8`;
the converted WAVs and QSSTV logs are
`/tmp/decodium-qsstv-experimental/decodium-drm-header-tail-{a,b}.wav` and
the corresponding `.log` files. WAV SHA-256 values are
`e83f0d07bdf4ddab880ec5bffab758567ce46f04ed205b6bbde47fe1a2fa59b3` and
`ee2ea52e77da01e5b77475ff2283e88a3ba3af33e6a850691508e1ac3f97edf9`.

Two additional A/B runs used separate full Decodium TX and RX applications
on the same profile; both JPEGs were recovered byte-for-byte. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-a-8jksjd5h`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-b-c8k1mxz5`.
The scripts `run_hamdrm_tx_capture_lab.py a|b` and
`run_hamdrm_two_app_lab.py a|b` reproduce those local transmission paths.
The QSSTV receiver ran on a recorded WAV, not a live sound input. These
small fixtures and clean virtual audio do not establish broad profile,
RF, noisy-channel or EasyPal interoperability.

Two further Decodium-to-QSSTV runs used distinct 320×240 JPEG images of
7,624 and 9,976 bytes. QSSTV recovered both byte-for-byte, with respectively
8 and 10 data segments and 104 and 131 successful outer CRC checks, zero
outer CRC failures. Its `pkt_bad` values were 82 and 107; these include
ordinary incomplete fragments as explained above and cannot be interpreted
as that many invalid packets. Transmissions lasted 46.034 and 56.248 seconds.
The source image SHA-256 values are
`5966159f4d284dd99227b8bf5bb097dc07ae303c7e67643fa57c7f147201fffc` and
`5b33229631c2c56c9456926163e00057cd8ba48c3264f2f7aca6fee44676ccfb`.
Source images, converted WAVs, independent output images and logs are under
`/tmp/decodium-qsstv-experimental/decodium-image-{a,b}.jpg` and
`/tmp/decodium-qsstv-experimental/decodium-drm-large-{a,b}*`; the raw
Decodium captures and logs are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-a-vv49c9rm`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-b-njanc2h9`.
The larger transfers use the same profile and clean, file-mediated path;
they are not evidence for other profiles, interference or RF.
The separate helper `run_qsstv_exp_drm_decode_lab.py CAPTURE.f32 REFERENCE.jpg`
repeated both independent decodes and byte comparisons successfully; its
outputs are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-decode-0q1uwdsh`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-decode-378aj51b`.

### Second bidirectional profile: A/2.3/high/4-QAM/long

Two additional Decodium transmissions on `hamdrm-A-2300-high-4-QAM-long`
were captured through BlackHole; the unmodified QSSTV-Experimental 10.0
batch receiver recovered each distinct 225-byte JPEG exactly. It identified
the expected profile and reported 17 CRC-valid MSC blocks, zero CRC failures,
and `pkt_bad=2` for each run (the fragmented-packet caveat above applies).
Captures are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-a-lv4m_jar`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-b-fsstb7me`;
QSSTV outputs/logs are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-decode-ilyeucf5`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-decode-g9ccvmgv`.

In the reverse direction, the locally rebuilt QSSTV-Experimental application
with the signed-PCM patch encoded two different 320×240 images. Its own DRM
batch receiver identified A/2.3/high/4-QAM/long and recovered JP2 objects of
4,041 and 4,032 bytes; Decodium received both exact byte sequences over
BlackHole. The QSSTV CLI requested `2.2` kHz and `short`, but the actual
receiver report says 2.3/long; these observed values were used for Decodium.
QSSTV artifacts are `/tmp/decodium-qsstv-experimental/drm-mode-a-{a,b}*`;
Decodium artifacts are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-0i_t8tir`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-xfyighxo`.
WAV SHA-256 values are
`fd71e1c745e38f0ba62d14baacfd786bf6379e4d34cbfe24815332878a730ffb` and
`0f7d32ec2f916a0d5808c2db7be7531b6e928d0a511299b3b209f510ab757b92`.
These are clean file-mediated local audio tests, not RF or proof for all
parameter combinations.

### Third profile, Decodium TX to independent QSSTV RX: B/2.5/high/16-QAM/long

Two full Decodium application transmissions through BlackHole were captured
and decoded by the pinned, unmodified QSSTV-Experimental 10.0 batch receiver.
Both distinct 225-byte JPEGs were reconstructed byte-for-byte. QSSTV reported
`B/2.5/Hi/16/Long`, respectively 14 and 15 valid MSC CRCs, and zero bad MSC
CRCs. Each log also reported `pkt_bad=1`, whose fragmented-packet limitation
is described above. Captures and transmitter logs are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-a-13sb82pp`
and `.../decodium-hamdrm-tx-b-fa60msy9`; receiver artifacts are in
`.../decodium-qsstv-drm-decode-_t28plt_` and
`.../decodium-qsstv-drm-decode-mchj4szs` under the same temporary directory.
QSSTV logged duplicate Qt framework warnings. The path is clean, local and
file-mediated; RF, other B profiles and larger images were not tested in
these runs.

The Decodium-to-QSSTV B/2.5/high/16-QAM/long check was extended to the two
320×240 images used above (7,624 and 9,976 bytes). Full Decodium TX
applications sent each image through BlackHole; the pinned unmodified QSSTV
10.0 batch receiver identified `B/2.5/Hi/16/Long` and recovered each file
byte-for-byte. QSSTV reported respectively 81/102 CRC-valid MSC blocks, no
bad MSC CRCs and 8/10 data segments. Its `pkt_bad` counts were 59/78 and
include incomplete fragments as described above. Captures are
`.../decodium-hamdrm-tx-a-y8n17hsx` and
`.../decodium-hamdrm-tx-b-vwiyp0hd`; QSSTV outputs are
`.../decodium-qsstv-drm-decode-gksdz5bg` and
`.../decodium-qsstv-drm-decode-tg6r2c90`. The independent receiver consumed
recorded local audio, not live RF, and these results do not cover other
profiles or EasyPal.

The same A/B images were also sent between two simultaneous Decodium
applications on this B/2.5/high/16-QAM/long profile. Both were recovered
byte-for-byte, and the completed inbox remained error-free after further
groups. RX diagnostic snapshots showed zero discontinuities, dropped chunks,
HAMDRM rejected chunks, queue overflows and processing failures. Artifacts
are `.../decodium-hamdrm-two-app-a-pnewdf6y` and
`.../decodium-hamdrm-two-app-b-k9okvkbf`. The two-app lab accepts an
explicit image and bounded duration for reproducing longer tests; this is a
local macOS virtual-audio test, not live QSSTV or RF.

In the reverse direction, the separately built QSSTV-Experimental revision
`a84584248d3307bfa081362031ff20f365337633`, with the signed-PCM patch
mentioned above, encoded two 320×240 images. Although its CLI was given long
interleaving, its receiver reported `B/2.5/Hi/16/Short` on both WAVs. QSSTV
reconstructed JP2 objects of 4,998 and 5,021 bytes with 83 valid MSC CRCs
and zero reported CRC errors per run. Replaying the WAVs through BlackHole
into two isolated Decodium RX runs configured for the observed short profile
reconstructed those respective JP2 objects byte-for-byte. Artifacts are
`/tmp/decodium-qsstv-experimental/drm-mode-b-{a,b}.wav` and matching
`_1_drm.jp2` files, plus Decodium logs and received objects under
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-sr1us3ke`
and `.../decodium-qsstv-drm-app-6j3moymj`. WAV SHA-256 values are
`d122c31a8d7786c82478f65c87e428d7b503f26d271d0af4069b2c16620c6fb2`
and `49572865a3fea8c5121859080f6884ee5650cf62df698a5b6839a91962c2fcfc`.
This does not demonstrate B/long in the QSSTV-to-Decodium direction, an
unmodified QSSTV transmitter, difficult channels or RF.

For the same B/2.5/high/16-QAM/short profile in the opposite direction,
two full Decodium TX applications sent the two distinct 225-byte JPEG
fixtures through BlackHole. The unmodified QSSTV-Experimental 10.0 batch
receiver reported `B/2.5/Hi/16/Short` and reconstructed both byte-for-byte.
It reported 16 valid MSC CRCs and zero bad CRCs per run; `pkt_bad=1` in each
log has the fragmented-packet interpretation described above. TX artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-a-bgm_u1yf`
and `.../decodium-hamdrm-tx-b-hcig16vs`. QSSTV artifacts:
`.../decodium-qsstv-drm-decode-u6mrvr8j` and
`.../decodium-qsstv-drm-decode-f68bmz9m` under the same temporary directory.
This is a clean file-mediated local test with small fixtures; it does not
establish RF or difficult-channel performance.

The missing B/2.5/high/16-QAM/long reverse direction was subsequently tested
with the same patched QSSTV TX and two 320×240 images. QSSTV decoded each WAV
as `B/2.5/Hi/16/Long`, with 79 valid MSC CRCs and no reported CRC failures;
two isolated Decodium RX runs reconstructed the respective 4,998- and
5,021-byte JP2 objects exactly. QSSTV was invoked with `--drm-interleave short`.
In source revision `a84584248d3307bfa081362031ff20f365337633`, `src/main.cpp`
maps short to index 0 and long to index 1, while
`src/drmtx/drmtransmitter.cpp` maps 0 to long and 1 to short, explaining the
observed inversion for this build. The WAVs are
`/tmp/decodium-qsstv-experimental/drm-mode-b-long-{a,b}.wav`, with SHA-256
`cc5b63dcd6c07dd0982e95f124c53e7511e5eef8afadf637ffd5a1a51da34b65`
and `50b0d462b7f2ea12827f858c1051fabc28dfef65a96679225b7074063b2c559c`.
Decodium artifacts are under
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-eoe2amjq`
and `.../decodium-qsstv-drm-app-47ogxhhf`. These remain clean local,
file-mediated tests with a patched transmitter; RF and unmodified QSSTV TX
are not established.

### A/2.3/normal/4-QAM/short transmit comparison

Two full Decodium application transmissions of the distinct 225-byte JPEG
fixtures used `hamdrm-A-2300-normal-4-QAM-short`. The pinned, unmodified
QSSTV-Experimental 10.0 batch receiver identified `A/2.3/Lo/4/Short` and
reconstructed both files byte-for-byte. It reported 17 and 16 valid MSC
blocks, no bad MSC CRCs, and `pkt_bad=2` each (the fragmented-packet caveat
above applies). TX artifacts are `.../decodium-hamdrm-tx-a-4p7vb42d` and
`.../decodium-hamdrm-tx-b-ejcyv1jp`; QSSTV outputs are
`.../decodium-qsstv-drm-decode-8vk4mnud` and
`.../decodium-qsstv-drm-decode-qnp3b8vy`.

Two simultaneous Decodium TX/RX applications on the same profile also
received both files exactly. Completed inbox entries remained error-free
after further groups; RX diagnostics had no reported discontinuities,
dropped chunks, HAMDRM rejected chunks, queue overflows or processing
failures. Artifacts are `.../decodium-hamdrm-two-app-a-26654bbw` and
`.../decodium-hamdrm-two-app-b-lfisi8mt`. QSSTV used recorded local audio;
these small clean fixtures do not establish reverse interoperability, live
QSSTV, EasyPal, other operating systems, difficult channels or RF.

For the reverse direction, the separately built QSSTV-Experimental source at
revision `a84584248d3307bfa081362031ff20f365337633`, with the signed-PCM
patch described above, encoded the two 320×240 sources. The CLI was given
`--drm-interleave long`; due to the documented inversion in that build,
the unmodified QSSTV 10.0 batch receiver identified both recordings as
`A/2.3/Lo/4/Short`, with 89 CRC-valid MSC blocks and zero bad CRCs each.
It recovered 4,056- and 4,048-byte JP2 objects. Two Decodium RX application
replays over BlackHole reconstructed the corresponding objects byte-for-byte,
with no reported audio drops, discontinuities, queue overflows, rejected
chunks or processing failures. The WAVs are
`/tmp/decodium-qsstv-a23-normal-short-{a,b}.wav` (SHA-256 respectively
`4163d3d364db95bac61231fe9d33b4f6805ecba3ae7f4e25f36488db1f91998d`
and `ef462f921987238af1e1970b8a83e25ce348d14f23164d6465319426ebf0abeb`);
Decodium artifacts are `.../decodium-qsstv-drm-app-c368uwes` and
`.../decodium-qsstv-drm-app-lfdp0j2g`. The first batch decode used a copy
whose SHA-256 matched the completed WAV. QSSTV emitted duplicate Qt framework
warnings. This is clean, file-mediated local audio with a patched QSSTV TX,
not live QSSTV, EasyPal, RF or a test of other platforms.

## Native MOT packets into QSSTV receive methods

`tests/sstv/tools/run_qsstv_packet_rx.sh` builds native Decodium A/B objects,
segments them and packetizes their MOT groups using two different fixed packet
sizes. A separate temporary executable compiles the exact `setupDataBlock`
and `setupDataPacket` method bodies extracted from the pinned QSSTV
`src/drmrx/sourcedecoder.cpp`, plus QSSTV's CRC implementation. The shim
provides the fields and globals those methods use, with logging disabled;
it is not the complete QSSTV source decoder or application. The script
checks the source revision and both reference files before extracting them.

For 24 intact MOT groups (two JPEG images, two packet sizes), all parsed kinds,
segment numbers, last flags, transport IDs and payloads matched. The harness
then reassembled the parsed payloads itself into four objects and compared
all bytes with the original A/B JPEG files. It decoded each reconstructed
16×16 image and checked its center pixel. It also verified that one-bit
corruption in every outer packet fails the reference CRC, and that 24 altered
MOT groups with freshly generated valid packet CRCs are rejected by QSSTV's
inner CRC check. A further 12 A/B cases change a packet's continuity index
while recomputing its packet CRC; the QSSTV method rejects the sequence and
clears continuity state. This covers the indicated QSSTV receive methods and the
harness's assembly; it does not run QSSTV's full object assembly or RX chain.

```sh
QSSTV_REFERENCE_ROOT=/tmp/decodium-qsstv-reference \
  tests/sstv/tools/run_qsstv_packet_rx.sh
```

## BSR generator and repair-request parser comparison

### Reed–Solomon protected image containers

`bash tests/sstv/tools/run_qsstv_rs_oracle.sh` checks the same pinned QSSTV
revision and compares two different inputs for each of `rs1` through `rs4`.
It compiles QSSTV's actual encoder, interleaver and RS routines; only an
unrelated GUI configuration include is replaced in a temporary copy. Complete
container bytes match, and the QSSTV RS decoder repairs ten erasures per
codeword. The native codec also has A/B encode/decode, boundary-size, missing
byte and corruption tests. Its bounded decoder corrects declared erasures
and unknown byte errors within the RS correction budget, and limits repair
work. RS parity is not a cryptographic integrity check. The controller
automatically recovers missing regular MOT segments once the final segment is
received. If the final marker is missing, the operator must stop RX and use
Recover RS; controller A/B tests cover this for all four levels. Mixed
unknown errors and erasures are checked against the original bytes.
The native RS A/B test also exercises all four parity levels at the
correction boundary with distinct damaged positions, including mixed unknown
errors and declared erasures. The instrumented HAMDRM parser and RS decoder
completed 194,996 libFuzzer iterations in 31 seconds on macOS ARM64 with
AddressSanitizer and UndefinedBehaviorSanitizer enabled; this is bounded
fuzzing, not an exhaustive proof.

Full local Decodium application A/B transfers over BlackHole passed for RS1
and RS4 on clean audio. Additional RS4 A/B application runs deliberately
dropped one 1,024-byte body segment after demodulation; both receivers
reconstructed all 2,725 original bytes. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-a-zno8losj`
and `.../decodium-hamdrm-two-app-b-u21ygafc`. A similar RS1 trial had too
little parity for that loss and remained incomplete, as expected. These tests
and the QSSTV method oracle do not prove
interoperability with an unmodified EasyPal application, difficult channels,
RF, or other operating systems.

After the mixed-error decoder change, clean-audio two-application A/B tests
also passed for RS2 and RS3 with exact 225-byte images. Their artifacts are
`.../decodium-hamdrm-two-app-a-2cjvotjj`,
`.../decodium-hamdrm-two-app-b-wbx6niha`,
`.../decodium-hamdrm-two-app-a-me9g1gbs` and
`.../decodium-hamdrm-two-app-b-7jwklb89`, respectively.

A Decodium RS1 and an RS4 capture fed to the unmodified QSSTV-Experimental
10.0 batch receiver did not produce images, despite 20 valid MSC CRCs and no
reported bad MSC CRCs in each. These transmissions used only one MOT body
segment. QSSTV's source sets the normal segment size from a non-final segment;
Decodium now sends at least two for protected objects. The original failures
and logs are in `.../decodium-hamdrm-tx-a-to7kyars`,
`.../decodium-qsstv-drm-decode-rhvobidz`, `.../decodium-hamdrm-tx-b-jvdoyl1u`
and `.../decodium-qsstv-drm-decode-kugk3y8i`. A subsequent independent
application test is required before claiming the change resolved the failure.

After enforcing at least two body segments for protected TX, the unmodified
QSSTV-Experimental 10.0 batch receiver did produce both test images from new
Decodium captures. RS4 B was identical to the 225-byte source. RS1 A decoded
to the same pixels but QSSTV wrote 441 bytes: the original 225 bytes followed
by 216 zero bytes. An isolated run of the pinned QSSTV RS decoder reproduced
the same 441-byte result for a 225-byte input; its length calculation converts
the surplus byte through signed `char` on this macOS build. This is observed
file-mediated compatibility for these two fixtures and one waveform profile,
not byte-exact RS1 exchange or proof for all versions. Artifacts are
`.../decodium-hamdrm-tx-a-0cjzfjuy`, `.../decodium-qsstv-drm-decode-u9800f7y`,
`.../decodium-hamdrm-tx-b-sh9gp68u` and
`.../decodium-qsstv-drm-decode-vghati8a`.

Two subsequent Decodium RS4 A/B captures were decoded by the unmodified QSSTV
10.0 batch receiver as E/2.5/high/16-QAM/long, with both resulting 225-byte
JPEG files matching their inputs exactly. The corresponding captures and
receiver artifacts are `.../decodium-hamdrm-tx-a-d_pthv6t`,
`.../decodium-qsstv-drm-decode-6a5bh5_2`,
`.../decodium-hamdrm-tx-b-k7jajfe8`, and
`.../decodium-qsstv-drm-decode-0fsv5581`. The lab checks the requested RS
level in the sender log so a non-RS transmission cannot silently pass.
After adding that check, image A was captured and independently decoded
again with an exact byte match; artifacts are
`.../decodium-hamdrm-tx-a-8ew8nkp3` and
`.../decodium-qsstv-drm-decode-9revz9th`.

In the reverse direction, the QSSTV-Experimental test transmitter generated
an RS1 WAV from its `drm-test-a.png` fixture. Replayed through BlackHole into
Decodium, the received JP2 had 4,998 bytes and exactly matched the previously
decoded unprotected QSSTV transmission of the same fixture. The unmodified
QSSTV-Experimental batch receiver decoded the protected recording to those
same 4,998 bytes followed by 147 zero bytes; both outputs decode to identical
pixels. The original laboratory comparison against that padded file therefore
reported a failure, despite the independently checked original-byte match.
The recording and receiver artifacts are `/tmp/decodium-qsstv-reverse-rs1.wav`,
`/tmp/decodium-qsstv-reverse-rs1-decoded` and
`.../decodium-qsstv-drm-app-03ixfoxh`. This is a file-mediated, local audio
check for one fixture and waveform profile; it is not an EasyPal, RF, or
cross-platform result.

Two publicly labelled digital-SSTV examples from
`milesburton/easypal-toolkit` commit `60d0be0e89ef7b683750b345ec907c65b578956e`
were evaluated as possible independent samples. The repository includes its
own synthetic example generator and does not establish that these WAVs were
produced by the EasyPal application. The unmodified QSSTV-Experimental 10.0
batch receiver produced no image for either file, with no FAC lock or MSC
blocks. This negative comparison does not establish why they failed or imply
an EasyPal incompatibility; these files are not used as EasyPal evidence.
Their SHA-256 hashes are
`f43e72708a02c68aad2c0a5fe79a61a85a3d10c97f00f1e13a06be87831bad48`
and `eee54a5d860460a3b01b663ab5d100932f40d1a564092f9da2a2ccf6dcaae3a3`.

Two further RS4 reverse-direction tests used the modified QSSTV-Experimental
TX and different 320x240 images. Both recordings were replayed through
BlackHole into separate Decodium sessions. The resulting 4,998-byte and
5,021-byte JP2 files matched the independently decoded unprotected QSSTV
transmissions of the same images byte for byte. Receiver artifacts are
`.../decodium-qsstv-drm-app-psuio27s` and
`.../decodium-qsstv-drm-app-lnx5jwca`; WAV files are
`/tmp/decodium-qsstv-reverse-rs4-a.wav` and `...-b.wav`.
The unmodified QSSTV batch receiver identified the first recording as
E/2.5/high/16-QAM/short and recovered an image with 178 good MSC CRCs and no
bad MSC CRCs. The modified generator was invoked with `--drm-interleave long`,
but its output was identified as short. An initial Decodium run configured
for long received no groups and failed; its artifact is
`.../decodium-qsstv-drm-app-bn77le5x`. The successful runs explicitly selected
short. These results do not prove all profiles, other QSSTV versions, EasyPal,
radio operation or other operating systems.

Two more RS4 application A/B runs discarded the final body segment after
demodulation. After TX finished, the laboratory stopped RX and invoked the
same manual recovery action exposed in the page; both 2,725-byte images were
reconstructed exactly. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-a-e4nszmnu`
and `.../decodium-hamdrm-two-app-b-ia85khg1`. The loss was injected at the
decoded-group boundary, not caused by RF or a noisy channel.

### Experimental CRC-gated multistage retry

At the time of these earlier tests, an iterative multistage MSC decoder was
selectable only in laboratory test mode. It retries a packet after existing
decoders fail and accepts it only when its packet CRC passes. It is now enabled
in ordinary application sessions as described at the start of this document.
Its clean output
matches all 36 independent QSSTV MSC cell vectors. For deterministic 64-QAM
noise trials, it recovered 6, 19 and 8 additional blocks out of 20 at noise
amplitudes 0.21, 0.23 and 0.25 respectively; these are synthetic test cases,
not a general sensitivity guarantee. In a 144-case synthetic waveform
matrix, flat RX completed 120, adaptive RX 118, adaptive plus flat retry 120,
and adaptive plus flat plus multistage 124; 10 packets used the new retry.

Two Decodium sessions connected by BlackHole through a constructed 1 ms,
27% echo received A/short and B/long 225-byte JPEGs byte-for-byte; each
reported 18 packets recovered by the multistage retry. Artifacts are
`.../decodium-hamdrm-two-app-a-xnt608d_` and
`.../decodium-hamdrm-two-app-b-yf98bwd4`. Under the same nominal echo, A
had failed without the multistage retry while B already succeeded with the
ordinary flat estimator. The unmodified QSSTV 10.0 batch receiver recovered
the saved A waveform exactly (`.../decodium-qsstv-drm-decode-e7gwzwz3`).
A subsequent A repetition did not acquire the frame and delivered no image
(`.../decodium-hamdrm-two-app-a-p2yehd24`); the relay reported no errors.
The cause of that acquisition failure is not established. A further A/short
run with the legacy monitor initially pending passed, recovering 10 packets
and verifying that legacy PCM resumed after HAMDRM; artifact
`.../decodium-hamdrm-two-app-a-uhofdzqa`. A B/long run with legacy monitoring
initially active did not complete (`.../decodium-hamdrm-two-app-b-027m_cgr`):
its RX diagnostics reported zero rejected audio chunks, 16 packet CRC
failures, two multistage recoveries and one delivered group. A subsequent
B/long run passed with the legacy monitor initially active, 10 packets
recovered by the retry and legacy PCM restored; artifact
`.../decodium-hamdrm-two-app-b-tqkqbbkk`. These outcomes
do not establish reliable acquisition, RF performance, or Windows/Linux
support. The channel, capture-analyzer and waveform tests also passed under
AddressSanitizer and UndefinedBehaviorSanitizer on macOS ARM64 (leak checking
disabled); this does not substitute for a full-application sanitizer test.

A later live B/long run failed while recording the actual relay output
(`.../decodium-hamdrm-two-app-b-8aa86ggv`,
`.../decodium-hamdrm-two-app-echo-olkoa5u_`). The unmodified pinned QSSTV
batch receiver decoded that recording to the exact 225-byte reference
(`.../decodium-qsstv-drm-decode-x9231bj3`). To isolate sample alignment,
the same 48 kHz stereo float recording was independently resampled by
FFmpeg to 12 kHz mono s16 after discarding respectively 0, 1, 2 or 3 initial
48 kHz samples; this is an offline experiment, not a recording of Decodium's
internal resampler output. In adaptive-plus-multistage mode offsets 0 and 1
decoded exactly without retry; offset 2 produced 16 packet CRC failures,
two multistage recoveries and one group, matching the failed live run's
counts; offset 3 decoded exactly using 18 multistage recoveries. For offset
2 both flat and adaptive-plus-flat also failed with 18 packet CRC failures.
For offset 3 flat and adaptive-plus-flat failed while multistage succeeded.
This exposes sample-alignment sensitivity in this fixture; the precise
mechanism and whether it explains all intermittent live failures remain open.
An offline diagnostic then shifted the selected integer frame start one
12 kHz sample earlier. This recovered offset 2 exactly (18 multistage
packets), but broke offsets 0 and 1, which had previously decoded exactly;
offset 3 remained exact. The unconditional shift is therefore not retained
in the receiver. Any timing retry needs to preserve independently validated
candidate state rather than replace a valid candidate with this fixed shift.

The offline analyzer's `timing-hybrid` diagnostic maintains independent
normal and one-sample-early receivers, and feeds only their validated groups
to the assembler. On this B recording it reconstructed the exact image for
all four offsets; at offset 2 the early receiver delivered 17 groups while
the normal receiver delivered one. The independent A/B reference fixtures
and two deterministic noise inputs also pass the analyzer checks. This is a
diagnostic, not a production receiver. A first live implementation running
both receivers serially on the same audio worker failed its A/short test:
the bounded queue overflowed and the RX diagnostic reported rejected chunks
(`.../decodium-hamdrm-two-app-a-5xp9ujkz`). That live integration was
removed. A bounded real-time implementation and new two-application A/B
proof are still required.

Subsequently the receiver cached cyclic-prefix and nominal pilot scores for
unchanged PCM windows until those samples were discarded. This preserves the
candidate scoring algorithm and bounded input buffer, while avoiding repeated
demodulation after rejected timing candidates. On the captured B fixture the
four `timing-hybrid` offsets retained their earlier decoding outcomes and took
about 1.4 seconds each on this Mac, compared with substantially longer before
the cache. A lab-only dual-timing path was then retried in two local Decodium
processes: A/short with the legacy monitor initially pending and B/long with
it initially active both reconstructed the 225-byte reference exactly,
reported no rejected RX chunks, and restored the legacy PCM feed. The runs
were repeated with the no-rejected-chunks check enforced by the harness;
artifacts are `.../decodium-hamdrm-two-app-a-ue1z6rb9` and
`.../decodium-hamdrm-two-app-b-vopnrbnk`. The dual path remains restricted to
explicit test mode. These particular local tests do not establish general
real-time throughput, RF behavior, or other platforms. The capture-analyzer
and waveform suites passed again on macOS ARM64 with the cache enabled under
AddressSanitizer and UndefinedBehaviorSanitizer (leak checking disabled).
Further A/B runs with explicit per-candidate tracing showed nine groups from
the early receiver in A/short and four in B/long, with exact images, zero
rejected chunks and legacy PCM restoration. Artifacts are
`.../decodium-hamdrm-two-app-a-72wsdm4t` and
`.../decodium-hamdrm-two-app-b-0uz_vhl1`; the B harness explicitly required
early-receiver groups. Those counts can include duplicates of groups accepted
by the normal receiver and do not demonstrate that the early candidate was
necessary for these particular successful live transfers.

A separate app replay used the offline failing B/offset-2 sample stream,
upsampled from 12 kHz mono to a 48 kHz stereo WAV. The single-candidate and
dual-candidate runs both reconstructed the 225-byte image exactly; the dual
run reported zero early groups, zero rejected chunks and zero queue overflows.
Artifacts are `.../decodium-qsstv-drm-app-cmko37c8` and
`.../decodium-qsstv-drm-app-ndgyygdt`. Conversion and the application's audio
capture/resampling can change the alignment, so this is not a reproduction
of the offline failure and does not establish that dual timing was necessary.
The lab replay helper checks explicit opt-in and queue diagnostics when dual
timing is requested.

The CRC-gated multistage packet retry was also decoupled from adaptive
equalization for explicit laboratory tests. With ordinary flat estimation
and normal pilot limits, two Decodium process pairs transmitted A/short and
B/long through the same 1 ms, 27% echo. Both images matched the 225-byte
inputs, with 10 and four multistage packet recoveries, zero rejected chunks
or queue overflows, and restored legacy PCM. Artifacts are
`.../decodium-hamdrm-two-app-a-1cbv0hih` and
`.../decodium-hamdrm-two-app-b-aacj92x8`. The unmodified QSSTV 10.0 batch
receiver decoded each saved relay recording byte-for-byte
(`.../decodium-qsstv-drm-decode-7hhfvb_4` and
`.../decodium-qsstv-drm-decode-vjcd3pk4`).

When the same relay files were converted offline to 12 kHz, both ordinary
flat decoding and flat with multistage retry succeeded without needing the
retry. Thus those conversions do not prove the retry was necessary for the
live transfers. On the earlier four phase-offset versions of the B capture,
with ordinary 0.12/0.30 pilot limits, flat and flat-multistage both decoded
offsets 0 and 1; both failed offset 2, and only flat-multistage reconstructed
offset 3 exactly, with one recovered packet. This is a controlled offline
comparison, not general channel or platform coverage. At the time of these
tests the strict-pilot multistage path was disabled outside explicit test
mode; it is now enabled by the native RX adapter.
The capture analyzer and extended waveform/noise suites passed on macOS
ARM64 with AddressSanitizer and UndefinedBehaviorSanitizer after this change
(leak checking disabled). This does not establish Windows or Linux results.

The two independent recordings labelled EasyPal were also compared offline
with ordinary pilot limits, flat estimation and this retry off versus on.
The first remained incomplete with no groups in both configurations; the
second yielded all 350 segments and the same 10,546-byte object with the
previously documented 199-byte zero-padding difference from QSSTV. No packet
used the multistage retry in either recording. These comparisons therefore
do not demonstrate an improvement for those recordings or full EasyPal
interoperability.

The laboratory also permits combining the two independent timing candidates
with flat estimation, the ordinary pilot limits and CRC-gated multistage
retry. Two Decodium pairs passed A/short and B/long with 27% echo, zero
rejected chunks or queue overflows, and legacy PCM restoration. Artifacts are
`.../decodium-hamdrm-two-app-a-lkjv7o_i` and
`.../decodium-hamdrm-two-app-b-howci5oq`. For the four controlled B offsets,
the combined strict-pilot mode recovered offsets 0, 1 and 3 exactly but still
failed offset 2; it did not improve on the single flat-multistage result.
On both EasyPal-labelled recordings the combined strict-pilot result was also
unchanged: the first produced no groups, and the second retained the known
199-byte QSSTV padding difference. The secondary candidate produced no groups
for the first and 390 for the second, which can duplicate the primary groups.
The combined mode is laboratory-only and is not a full compatibility claim.

For a further real-time stress A/B, the BlackHole relay added deterministic
uniform noise and a 1700 Hz sine, each with peak amplitude 0.01, in addition
to the existing 1 ms, 27% echo. It checked finite samples, clipping and
PortAudio errors. Both Decodium images were incomplete with the experimental
adaptive dual-candidate receiver; the corresponding saved relay recordings
were decoded byte-for-byte by the unmodified QSSTV receiver. A separate B
attempt with a relay input overflow was discarded and repeated; it is not
counted as a receiver result. Valid artifacts are
`.../decodium-hamdrm-two-app-echo-n6vcf9fe`,
`.../decodium-hamdrm-two-app-echo-p6l1ae4s`,
`.../decodium-qsstv-drm-decode-wdd6ievt` and
`.../decodium-qsstv-drm-decode-0ek5nuqv`. At 0.001 for both noise and tone,
both Decodium image transfers completed exactly with the legacy PCM restored
(`.../decodium-hamdrm-two-app-a-oi7kcyqy`,
`.../decodium-hamdrm-two-app-b-4t42jte1`). These are two selected synthetic
levels, not sensitivity limits or RF signal-to-noise measurements.

The two 0.01 recordings were separately converted to 12 kHz after discarding
0, 1, 2 or 3 input samples. Adaptive dual timing decoded both images at
offsets 0 and 1, and failed at offsets 2 and 3. The offline `half-hybrid`
diagnostic adds a separate candidate fed by the average of adjacent 12 kHz
samples. It decoded A/2, B/2 and B/3 exactly, but A/3 remained incomplete.
Adding a fourth early version of that candidate did not recover A/3 and was
removed. An app-level experiment using the additional half-sample receiver
failed A and passed B; QSSTV decoded both corresponding relay recordings
exactly. App artifacts are `.../decodium-hamdrm-two-app-a-jw4wqbpm` and
`.../decodium-hamdrm-two-app-b-82l2zk_x`, with QSSTV artifacts
`.../decodium-qsstv-drm-decode-kjncqhul` and
`.../decodium-qsstv-drm-decode-6ro34c9x`. The two app recordings differed,
so B's success does not prove the extra candidate was necessary. The failed
app integration was removed; the half-sample candidate remains an offline
diagnostic, not a production receiver change.

Two fixed 1700 Hz notch filters (Q 100 and 300) were also tried offline on
the noisy offset-2/3 A/B samples. Only B/3 at Q 300 reconstructed the full
image; the other seven comparisons remained incomplete. In a separate clean
Q 300 control, A/3 failed although its unfiltered counterpart succeeded.
These are selected recordings and filter settings, not a general notch
evaluation; no fixed notch was enabled in the application.

A later fractional-timing experiment used a ten-tap, Hamming-windowed sinc
delay (4.5 samples at 12 kHz) as an additional independent lab candidate.
For the noisy A/B offset-2 and offset-3 files, this `sinc-hybrid` analyzer
reconstructed all four images byte-for-byte. The previous two-candidate
analyzer failed all four on the first captured A/B set; a two-tap half-sample
average recovered three. Independent QSSTV controls, wrong-reference checks
and deterministic noise rejection also passed in the analyzer. An app lab
using the same fractional delay then received A/short and B/long exactly under
the 27% echo plus noise and tone each at 0.01, with zero rejected chunks or
queue overflows and legacy PCM restored. The fractional candidate produced
one and four groups respectively, which may duplicate groups from the other
candidates. Artifacts are `.../decodium-hamdrm-two-app-a-rcbqtvhy` and
`.../decodium-hamdrm-two-app-b-j9ommg7t`; the unmodified QSSTV batch receiver
also decoded their saved relay signals exactly
(`.../decodium-qsstv-drm-decode-i6s2pdtq` and
`.../decodium-qsstv-drm-decode-ai4omb1y`).

For the actual saved signals from those app tests, four offline A/B
comparisons discarded either two or three 48 kHz samples before conversion.
The two-candidate receiver failed A/2, A/3 and B/2 and passed B/3; adding the
fractional candidate passed all four. Conversion and real-time capture can
produce different sample alignments, so this does not by itself prove the
fractional candidate was necessary in each app run. At that point the app path
was explicitly lab-only; the operator-selected path is described above. Fractional-delay
chunking and reset A/B checks, clean reference and wrong-reference tests,
and deterministic noise rejection passed; the analyzer and waveform suites
also passed on macOS ARM64 with ASan/UBSan (leak checking disabled).

The three-candidate lab receiver was also run against the two longer public
EasyPal-labelled recordings, with strict pilot thresholds and no rejected
audio chunks or queue overflows. Recording A reconstructed all 21,191 bytes
identically to the independent QSSTV output; the early and fractional paths
reported 148 and 160 groups respectively
(`.../decodium-qsstv-drm-app-vjmi5xh2`). Recording B reconstructed 10,546
bytes identically to the earlier Decodium result, and those bytes match the
QSSTV output prefix; QSSTV appends 199 zero bytes. Both additional paths
reported 390 groups (`.../decodium-qsstv-drm-app-xw3lyrn9`). Group counts may
include duplicates, and the previous two-candidate receiver already passed
both recordings, so these runs do not show that the fractional path was
necessary or establish general CPU headroom. They do not establish RF,
Windows/Linux, or live EasyPal interoperability.

To isolate the fractional receiver, a further test-only delivery switch keeps
all three DSP candidates running but sends only fractional-candidate groups to
the assembler. With the same 27% echo and noise and tone amplitudes of 0.01,
two local Decodium processes reconstructed A/short and B/long byte-for-byte,
with no rejected chunks or queue overflows and legacy PCM restored. The
fractional path reported four and seven groups respectively. Artifacts are
`.../decodium-hamdrm-two-app-a-zr1zvy02` and
`.../decodium-hamdrm-two-app-b-5sd68byu`; relay captures are
`.../decodium-hamdrm-two-app-echo-yag31kl9` and
`.../decodium-hamdrm-two-app-echo-4o03yup2`. This demonstrates that path can
independently reconstruct these selected signals, not that it is required in
other runs or sufficient for other profiles and channel conditions. The
delivery switch requires test mode, the HAMDRM lab and the experimental dual
and fractional timing options; it is not enabled in ordinary sessions.

The original lab callback removed a completed object while saving its bytes.
Later carousel groups could consequently create a new partial inbox entry;
that partial state did not mean the completed image had been corrupted. The
callback now takes a non-destructive snapshot. Repeating A/short and B/long
with the same channel and fractional-only delivery reconstructed both files
exactly and left both inbox entries complete (`.../decodium-hamdrm-two-app-a-hr4bw9tx`
and `.../decodium-hamdrm-two-app-b-fhiiu4n8`). These runs exposed a separate
remaining issue: repeated groups after completion cause the controller's
global error to say the transport ID already has a terminal inbox object.
The completed image remained available. The controller now retains SHA-256
fingerprints of completed image segments and ignores repeats only when kind,
segment number, last marker and payload match; conflicting groups remain
rejected. Controller A/B tests exercise matching and changed payloads. New
two-app A/short and B/long runs received exact images, observed further
fractional groups after completion, and retained a complete inbox without
the previous error (`.../decodium-hamdrm-two-app-a-7c2bqqpc` and
`.../decodium-hamdrm-two-app-b-71rxfzo4`). These macOS laboratory results do
not cover all carousel sequences, live QSSTV/EasyPal, other platforms or RF.
The normal single-candidate receiver was subsequently checked with two local
Decodium sessions and clean BlackHole audio. Both A and B arrived byte-exact,
remained complete and had no inbox error after additional groups: group
counters advanced from 9 to 17 and 7 to 15 respectively. Artifacts are
`.../decodium-hamdrm-two-app-a-9udb8i4i` and
`.../decodium-hamdrm-two-app-b-nunxvquk`. Further controller tests accept
changed continuity/repetition indexes and reject a changed last marker or an
unknown segment. The fingerprint collection is bounded by the configured MOT
segment and completed-object counts; it incurs additional memory per completed
segment, and its peak allocation has not been benchmarked on other platforms.
Fingerprint generation now visits the assembler's existing segment buffers
directly instead of first allocating a full serialized copy of all groups.
An A/B core test compares each visited segment, payload and final marker with
the canonical serialized snapshot for incomplete and complete objects. The
controller and core tests passed, followed by byte-exact normal-receiver
two-app A/B transfers with post-completion groups and an error-free complete
inbox (`.../decodium-hamdrm-two-app-a-7qw00j_t` and
`.../decodium-hamdrm-two-app-b-rjjbovrs`). This avoids that particular full
snapshot allocation; the remaining fingerprint storage and peak memory have
not been benchmarked on Windows or Linux.
Additional controller A/B tests assembled and replayed a 4,096-byte JPEG
structure using 4,096 one-byte body segments and 64 regular 64-byte segments.
Both kept the completed bytes and emitted completion once. The core and
controller suites passed on macOS ARM64 both normally and with ASan/UBSan
(leak checking disabled). This is a synthetic controller stress check, not
a real-image, RF, cross-platform or measured peak-memory benchmark.

A test-only early-only delivery switch was then used to exclude all primary
candidate groups while keeping both DSP paths active. A/short reconstructed
the reference exactly with nine early groups, zero rejected chunks and zero
queue overflows, and restored legacy PCM (`.../decodium-hamdrm-two-app-a-ce10t2h6`).
Three B/long early-only trials did not meet the same strict acceptance: two
did not complete, while one produced the exact reference and restored legacy
PCM but had one rejected audio chunk (`.../decodium-hamdrm-two-app-b-shqtvf18`).
That run predated the separate queue-overflow counter, so the reason for its
rejected chunk is unknown. The last incomplete B run reported zero rejected
chunks and zero overflows (`.../decodium-hamdrm-two-app-b-18lo6r2l`). These
checks confirm why the early candidate cannot replace the normal one and do
not establish a successful strict B early-only test. With both candidates
enabled and the separate overflow counter present, another B/long run passed
with zero rejected chunks and zero overflows; its early candidate delivered
no groups (`.../decodium-hamdrm-two-app-b-5ahr1i2u`).

### BSR generator and parser

`bash tests/sstv/tools/run_qsstv_bsr_oracle.sh` checks the pinned QSSTV
revision and extracts its unmodified `sourceDecoder::storeBSR` and
`drmTx::processFIX` method bodies into a temporary shim. Six representative
missing-segment sets plus all 255 nonempty patterns in each eight-segment half
of a 16-segment object are checked in both compact and extended forms, for
1,032 cases. The cases alternate two transport IDs, segment sizes and image
filenames. Decodium's generated bytes match
QSSTV's, except for an optional final newline after the extended profile;
both parsers expand the same requested segment numbers, and all truly missing
segments are included. Compact ranges can also request already received
segments, matching the pinned QSSTV generator's behavior. Two malformed
range cases are rejected by Decodium; malformed input was not passed to the
reference methods. This is a method-level differential comparison, not an
on-air BSR exchange, a full QSSTV application test, or independent EasyPal
evidence.

The same differential harness was extended on macOS ARM64 to cover 256
deterministic patterns across 128 segments and five longer boundary patterns,
including the highest segment accepted by the default 32,768-segment limit.
Both compact and extended BSR forms passed, bringing the comparison to 1,554
cases. It compares the complete generated request and the parsed repair list
against the pinned QSSTV methods; it does not expand the interoperability or
platform claims above.

After extending the oracle, the existing two-Decodium BlackHole BSR application
checks were repeated on macOS ARM64: A/extended and B/compact both passed with
exact received bytes. Their artifacts are `.../decodium-hamdrm-two-app-a-kf7x_1cq`
and `.../decodium-hamdrm-two-app-b-8bzv5v35`. These application cases use their
original short requests, not the oracle's maximum-length patterns.

The BSR parser's public custom-limit path now rejects a segment token above
65,535 before converting it to 16 bits. With an oversized caller-supplied
65,537-segment limit, the previous parser accepted a single 65,536 token as
segment zero; the controller already rejects such a configuration, and its
default limit is 32,768. Core A/B boundary checks exercise 65,535 as both a
single segment and a range endpoint, and reject 65,536 in both positions.
The first invalid case failed before the fix. Core and controller tests, the
core ASan/UBSan test (leak detection disabled), and the 1,554-case QSSTV
oracle passed on macOS ARM64 after the fix. Two local Decodium BSR application
checks also passed with exact bytes: extended A
`.../decodium-hamdrm-two-app-a-mezcl6z1` and compact B
`.../decodium-hamdrm-two-app-b-21ezmdap`. These application checks use normal
short requests and do not exercise oversized custom limits over audio.

### BSR separator compatibility

QSSTV's pinned `processFIX` strips carriage returns throughout the input and
skips empty lines. Decodium's parser previously rejected otherwise equivalent
requests containing empty lines or a carriage return within a field. It now
normalizes these separators after validating the original bounded text. Core
A/B cases cover compact CRLF/blank-line input with embedded CR and extended
input with empty lines; they failed before the change and now pass. Negative
controls still reject a missing segment, an unfinished range, and an invalid
control byte. The QSSTV method oracle compares the two formatted requests'
transport IDs and requested segments with Decodium, in addition to its 1,554
existing cases. The core and controller CTests pass.

On macOS ARM64, two-Decodium BlackHole A/B checks transmitted the formatted
extended A and compact B requests as actual HAMDRM BSR objects and verified
the requested segments and metadata. The test-mode-only formatting injection
is enabled with `DECODIUM_LAB_BSR_SEPARATORS=1`; it does not alter normal TX.
Artifacts are `.../decodium-hamdrm-two-app-a-4sxwrlnq` and
`.../decodium-hamdrm-two-app-b-g6zr2isb`.

Two further local A/B application checks seeded a partial object, sent the
formatted BSR over BlackHole, sent the requested repair over BlackHole, and
reconstructed each 2,725-byte image exactly. They did not simulate the
initial image loss over the audio channel. Artifacts are
`.../decodium-hamdrm-repair-a-09bmg_iz` and
`.../decodium-hamdrm-repair-b-ndg1mygy`. Reproduce with the same environment
variable and `python3 tests/sstv/tools/run_hamdrm_repair_two_app_lab.py a air`
then `b air`. These tests establish this local separator and repair behavior;
the QSSTV reference comparison is method-level, not a new live QSSTV or
EasyPal exchange, and does not establish RF behavior or broad interoperability.

### E/2.5/normal/64-QAM/short reverse-direction A/B

The CLI/profile discrepancy above was traced in the pinned QSSTV source:
`src/main.cpp` maps `--drm-interleave short` and `long` to indices 0 and 1,
matching the displayed combo-box order, while `src/drmtx/drmtransmitter.cpp`
selects `SI_LONG` for index 0 and `SI_SHORT` for index 1. Thus, in this
specific source revision, the CLI and actual transmitter interleaver are
inverted. This explains the earlier observed requests and reported FAC
profiles; it is not a change to Decodium or a claim about all QSSTV versions.

For an A/B reverse test, the signed-PCM-patched build of that QSSTV source
was invoked with `--drm-interleave long`, E/2.5 kHz/low protection/64-QAM
and two different 320×240 images. The unmodified QSSTV-Experimental 10.0
receiver independently identified both resulting captures as
`E/2.5/Lo/64/Short`, decoded each JP2 and reported 73 good MSC CRCs and no
bad CRCs or bad packets. These are 35.608-second, file-generated signals;
the transmitter build has the signed-PCM patch and is not the unmodified
release transmitter.

Each capture was then replayed through BlackHole into a full Decodium RX
configured for `hamdrm-E-2500-normal-64-QAM-short`. The 5,025- and
5,005-byte outputs matched the respective QSSTV-decoded objects exactly.
All 12 recorded diagnostic snapshots per run had zero discontinuities,
dropped chunks, processing failures, rejected HAMDRM chunks and queue
overflows; each run reported up to 76 delivered groups. The RX artifacts
are `.../decodium-qsstv-drm-app-anaozlmp` and
`.../decodium-qsstv-drm-app-cpda3rom`. QSSTV receiver images and logs are
`/tmp/decodium-qsstv-e64-short-logged-a` and
`/tmp/decodium-qsstv-e64-short-logged-b`. WAV SHA-256 values A/B are
`8377dbdaf7bea7d1ec98974372bf96e3d7ebbe1c1e267a0c2bfb17869593aed3` and
`1ce31ab4268062a36653cf13f18c06870020814252b33a7943e45b1be6f20898`.

Separate full Decodium TX/RX application A/B tests on the same short profile
transferred 7,624- and 9,976-byte JPEGs exactly. All 18 recorded diagnostics
per run had zero of the same audio/error counters. Artifacts are
`.../decodium-hamdrm-two-app-a-j2rg_2ch` and
`.../decodium-hamdrm-two-app-b-wlyakorl`. These results extend the earlier
short-profile Decodium-to-QSSTV small-image evidence in the reverse direction;
they do not establish live QSSTV/EasyPal exchange or RF performance.

### Flat receiver continuity on the two EasyPal-labelled recordings

The standard flat receiver was traced again on the two SHA-256-pinned
recordings used by `check_hamdrm_easypal_recordings.py`. With normal 0.12/0.30
pilot limits, A accepted 284 frames and recorded 203 pilot failures but no
validated groups; B accepted 401 frames and reconstructed all 350 object
segments. Sampling the trace whenever the accepted-frame counter advanced
showed at most two accepted frames between observed pilot-failure counter
changes on A, versus a run of 400 on B. These are observations at PCM chunk
boundaries, not instrumented exact lock durations. A long interleaver needs
five frames, so frequent loss of the data path is a plausible contributing
factor, not proof of the only cause.

As a diagnostic only, raising both pilot limits to 1.0 extended the longest
observed run on A to six frames but still produced no groups. B still produced
the exact reference. Therefore simply relaxing pilot validation is neither a
demonstrated fix nor appropriate evidence for changing production limits.
The recordings have different DRM profiles and unverified transmitter
provenance, so this A/B comparison does not isolate channel quality or prove
live EasyPal interoperability. Local traces are
`/tmp/decodium-hamdrm-{flat,loose}-{a,b}.trace` and corresponding `.out`
files; they are transient laboratory artifacts.

An offline timing experiment then linearly interpolated each recording to
change its sample count by a fixed ratio, without changing production RX or
its pilot limits. For recording A, shortening the PCM by 130, 140 and
150 ppm made the flat-multistage receiver reconstruct the exact 21,191-byte
reference, with respectively 224, 276 and 220 validated groups. Shortening
by 120 ppm produced 172 groups but no complete match; lengthening by
125 ppm produced no groups. For B, either shortening or lengthening by
125 ppm produced no groups, whereas the unmodified B recording decoded
exactly. Linear interpolation also changes the signal and this sweep does
not identify the original transmitter or establish a physical clock error.
It does show that relative sample timing strongly affects these two
captures, and that an unconditional fixed correction would regress B. A
receiver-side timing tracker must therefore adapt and be tested against
both drift directions, stable clocks, discontinuities and independent peers.
The generated PCM files are transient `/tmp/decodium-hamdrm-*.s16` artifacts.

An offline-only tracking prototype retains up to two PCM samples after each
frame and selects a nearby frame start using the nominal pilot score. Normal
pilot, FAC and packet validation remains in force. With tracking enabled,
the original A and B captures both produced exact output (280 and 390
groups respectively), and both rejected the other capture's reference.
Without tracking A produced no groups and B remained exact. After linear
resampling by +125 and -125 ppm, B still decoded exactly in both cases;
A decoded exactly when shortened by 125 ppm, but lengthening A by 125 ppm
produced only 24 groups and no complete output. This limitation rules out
enabling the prototype in normal RX. The baseline waveform suite also
passed; it does not itself exercise this tracking option. The two pinned
recordings and crossed-reference checks can be repeated with
`check_hamdrm_easypal_recordings.py --frame-tracking`. This prototype is
available only through the offline analyzer's diagnostic environment flag;
it has not been tested in two running Decodium sessions, with live peers,
or against all clock discontinuities and signal profiles.

The later prototype scores pilots at both the first and last symbol of each
frame. With this change A and B each reconstructed their exact reference
after either +125 or -125 ppm linear resampling, and both unmodified
recordings and crossed-reference negatives passed. The earlier failure on
the lengthened A capture therefore applies to the first-symbol-only version.
These selected fixtures do not establish general clock-tracking performance.
The tracker remains experimental; the application exposes it only through a
test-mode HAMDRM laboratory flag, never through the ordinary environment.
Two local Decodium process pairs subsequently transferred fixture A and B
byte-for-byte over clean BlackHole audio with tracking enabled; both RX logs
confirmed the laboratory option and the legacy monitor restoration check
passed. The respective artifacts are `.../decodium-hamdrm-two-app-a-2zj9hblw`
and `.../decodium-hamdrm-two-app-b-ezmi371p`. These clean-audio app tests do
not demonstrate live clock drift or independent EasyPal exchange.
The waveform suite now also exercises tracking with two distinct synthetic
12-group transmissions at nominal rate and +/-125 ppm, using 743-sample
chunks and one trailing frame of silence. All six cases recovered their
groups exactly. Without trailing samples the shortened first fixture lost
its final group; the shortened stream leaves less data at the final boundary,
this test does not establish recovery of a truncated final frame. The
receiver avoids retaining its tracking look-behind when end-of-stream
coincides exactly with the end of a complete frame.

The experimental tracker now pads at most four missing samples only at a
locked end-of-stream frame; normal pilot, FAC and packet CRC checks remain
mandatory. With this change the same synthetic A/B fixtures recovered all
groups at nominal and +/-125 ppm without trailing silence. Exact complete
end boundaries also passed. For each fixture a separate final frame cut by
100 samples remained incomplete and emitted only the correct preceding
groups. Two deterministic noise fixtures produced no groups with tracking
and multistage retry enabled. These checks do not cover every truncation,
noise distribution or possible corruption; the option remains laboratory
only. The two-app clean-audio evidence above predates this end-of-stream
handling change.
After the change, two clean-audio application pairs again delivered both
fixture images byte-for-byte and passed the legacy restoration check:
`.../decodium-hamdrm-two-app-a-yr30eyp5` and
`.../decodium-hamdrm-two-app-b-n3z1s3ji`. These live audio tests do not
exercise a finite truncated PCM file or establish independent-peer support.

The native waveform matrix was extended to exercise experimental tracking
across six mode/bandwidth combinations, three constellations, two protection
levels and both interleavers. All 72 clean cases completed. With two echo
levels, 120 of 144 cases completed, with no regression among the 120 cases
completed by the ordinary flat receiver; neither completed the other 24 in
this matrix. Two distinct synthetic streams also reacquired after eight
frames of deterministic noise and recovered exactly their before/after
groups. These bounded core tests do not establish performance with arbitrary
channels, live timing drift or all third-party implementations.

The local two-app BlackHole relay can now introduce a bounded simulated
sample-rate mismatch by linear interpolation with a prebuffer; it reports
the requested ratio and relay errors. With experimental tracking selected,
fixture A at +250 ppm and fixture B at -250 ppm both arrived byte-for-byte,
with legacy monitor restoration confirmed and no relay errors. These used
mode A, 2.5 kHz, normal protection, 16-QAM and long interleaving, with zero
intentional echo and noise. Relay artifacts are
`.../decodium-hamdrm-two-app-echo-oqskef46` and
`.../decodium-hamdrm-two-app-echo-iekrooi9`; the corresponding app artifacts
are `.../decodium-hamdrm-two-app-a-hzrxdh7_` and
`.../decodium-hamdrm-two-app-b-7ecmbm0p`. This is simulated drift through
two local applications, not independent device clock measurements, an
untracked-receiver comparison or live EasyPal interoperability.
Separate untracked control runs using the same profiles and respective drift
directions completed transmission but received zero validated groups and no
image; the relay reported no errors and RX diagnostic counters were zero.
In the A control, the harness reported the missing legacy-restoration marker
before its image-completion check; B explicitly reported incomplete RX. The
tracked runs did complete and restore legacy monitoring. The untracked app
artifacts are `.../decodium-hamdrm-two-app-a-yytsoxwq` and
`.../decodium-hamdrm-two-app-b-1dun3at4`; relay artifacts are
`.../decodium-hamdrm-two-app-echo-2eu8su4y` and
`.../decodium-hamdrm-two-app-echo-rw3qon4p`. These are separate real-time
runs, not a bit-identical replay or proof across other channel conditions.

The application now enables this frame-timing tracker for its ordinary
single-receiver HAMDRM path; the user-selected multi-candidate adaptive
path keeps its previous settings until its combined cost is evaluated. A
test-mode-only switch can disable tracking for a controlled comparison.
Two application-pair tests with the normal default and simulated +250 ppm
(A) or -250 ppm (B) completed with exact images and restored legacy
monitoring. The respective app artifacts are
`.../decodium-hamdrm-two-app-a-u1pxv165` and
`.../decodium-hamdrm-two-app-b-8fwj06sx`; their logs identify the enabled
default. With tracking explicitly disabled for the laboratory controls,
the same A/B scenarios transmitted fully but produced zero groups, with
clean RX diagnostics. Their app artifacts are
`.../decodium-hamdrm-two-app-a-92_46geu` and
`.../decodium-hamdrm-two-app-b-tvnl84rf`. These are separate live runs.

On this Mac, three offline analyzer runs on the 115-second A recording took
2.938–2.966 s without tracking and 0.328–0.329 s with tracking; only the
tracked runs reconstructed the reference. For the 188-second B recording,
which both decoded, timings were 1.047–1.055 s without tracking and
1.141–1.161 s with tracking. These wall times are specific to this machine
and recording set, not a general CPU benchmark or cross-platform evidence.
The evidence above does not establish live EasyPal interoperability, RF
performance, or recovery for arbitrary profiles and channels.

The two EasyPal-labelled recordings of uncertain provenance were then
replayed through BlackHole into the full Decodium application using its
ordinary single-receiver default, without enabling the adaptive candidates.
A (B/2.5 kHz/high/16-QAM/long) reconstructed all 21,191 reference bytes;
B (E/2.3 kHz/high/4-QAM/long) reconstructed 10,546 bytes equal to the
unpadded reference, itself the prefix of the independent QSSTV output with
199 trailing zero bytes removed. Both RX logs confirmed default timing
tracking, and the replay harness checked source device plus zero
discontinuities, dropped chunks, rejected chunks, queue overflows and
processing failures across its diagnostic snapshots. Artifacts are
`.../decodium-qsstv-drm-app-yiwf64dt` and
`.../decodium-qsstv-drm-app-xniyg806`. These tests strengthen application
replay evidence for the selected recordings; the original transmitter is
unverified and this is not a live EasyPal or RF exchange.

After the ordinary frame tracker was enabled, offline A/B controls revisited
the independently generated A/2.3/normal/16-QAM/short recordings described
at the start of this document. With tracking and strict normal pilot limits,
both original recordings still decoded byte-exactly, but neither recording
shifted by -5/+5 Hz acquired a valid frame. Thus tracking clock drift does
not resolve that earlier frequency/timing acquisition gap.

For a separate experimental interaction check, the existing adaptive-early
single receiver without the new tracker reconstructed both shifted objects;
enabling the tracker as well reconstructed A but not B (38 validated groups
and an incomplete B object). This result is a reason not to enable the
tracker indiscriminately in adaptive candidates. The app's user-selected
multi-candidate path keeps tracking disabled by default. These are offline
selected-recording checks, not a diagnosis of the regression, a new live
peer exchange or a claim about all frequency offsets.

On the current application build, the operator-selected adaptive reception
was rechecked against both shifted WAVs through BlackHole: A (-5 Hz) and B
(+5 Hz) each reconstructed their respective 277-byte QSSTV reference, and
the harness confirmed clean RX audio diagnostics. These runs used the
multi-candidate path, not ordinary single-receiver tracking; artifacts are
`.../decodium-qsstv-drm-app-wgofzzmt` and
`.../decodium-qsstv-drm-app-r5zh1jkm`. This is replay of instrumented
QSSTV-source files, not live EasyPal interoperability or an automatic fix
for the default receiver's acquisition gap.

Two timing-score experiments were rejected: using the frequency-corrected
demodulator for the existing flat pilot score changed the tracked
adaptive-early result from exact A/incomplete B to incomplete A/exact B;
selecting by cross-validated adaptive pilot error failed to reconstruct
either reference. Neither experiment remains in the application. The
bounded helper `tests/sstv/tools/check_qsstv_frequency_offset_recordings.py`
now verifies the SHA-256 of the four original/shifted PCM fixtures and both
references, then repeats the A/B baseline, shifted and crossed-reference
controls. It requires exact results for the previously successful paths but
reports rather than requires failure on the unresolved paths, so a future
improvement is not rejected. A missing-fixture negative control also passed.
The external recordings are not included in the repository; these checks
do not establish live EasyPal operation or all channel conditions.

The experimental early-timing tracker now keeps a one-sample early bias
when following the pilot-score minimum, preserving the candidate's intended
timing rather than converging it to the nominal candidate. Both shifted
QSSTV-source recordings now reconstruct exactly with the adaptive-early
receiver and tracking enabled, and crossed-reference negatives pass. A
synthetic A/B test also passed all combinations of +/-4 Hz carrier offset
and -125/0/+125 ppm sample-rate change, using a long interleaver and twelve
groups. These selected core tests do not establish universal recovery.

Two further full Decodium process pairs used the strict-pilot single
adaptive-early laboratory receiver with tracking enabled. Over BlackHole,
A with +250 ppm and B with -250 ppm reconstructed both 225-byte files
exactly and restored legacy monitoring; their logs confirm the intended
candidate and tracker. App artifacts are
`.../decodium-hamdrm-two-app-a-7agczc9q` and
`.../decodium-hamdrm-two-app-b-hn27tue5`, with relay artifacts
`.../decodium-hamdrm-two-app-echo-q7d33f4u` and
`.../decodium-hamdrm-two-app-echo-ygfu5psj`. Carrier offset and sample-rate
offset were combined in the synthetic tests, but not in these two-app
trials. Adaptive-early tracking remains a laboratory option; the ordinary
receiver still does not acquire the shifted QSSTV recordings and the
multi-candidate UI path has not been changed by this experiment.
The subsequent 144-case native echo matrix found that the early receiver
without frame tracking completed 124 cases, while the biased tracked version
completed 104 and regressed 21 previously successful cases. Therefore the
positive A/B results above are insufficient to enable this combination in
normal or user-selected adaptive reception; it remains laboratory-only.

A subsequent early-candidate tracker keeps its predicted timing unless the
pilot score improves by at least 5%, reducing timing jitter from marginal
score differences. On the same native echo matrix, both the untracked and
tracked early candidates completed 124/144 cases, with zero regressions from
the untracked result; the two shifted QSSTV-source recordings and their
crossed-reference controls also passed. Synthetic A/B checks combining +/-4 Hz
carrier offset with -125/0/+125 ppm sample-rate change passed. The tracker
is now enabled for the early candidate when the operator selects the existing
multi-candidate adaptive mode; the other adaptive candidates are unchanged.

With only the early candidate's groups delivered for testing, two full
Decodium process pairs using the operator-selected adaptive mode transferred
images A/B exactly with simulated +250/-250 ppm respectively and restored
legacy monitoring. App artifacts are `.../decodium-hamdrm-two-app-a-qur2lgtc`
and `.../decodium-hamdrm-two-app-b-4ybusffx`. The same early-only delivery
replayed the instrumented QSSTV-source -5/+5 Hz WAVs through BlackHole and
recovered the respective 277-byte independent references exactly, with
clean RX diagnostic snapshots; artifacts are
`.../decodium-qsstv-drm-app-f2m56n78` and
`.../decodium-qsstv-drm-app-gjq2jq98`. All candidates were still processing;
only group delivery was isolated. These are selected laboratory scenarios,
not RF or live EasyPal evidence. The normal single receiver still fails the
shifted QSSTV files; operator selection remains necessary for those inputs.

The ordinary application receiver now also runs a strict-pilot adaptive
early candidate alongside its primary tracker and delivers independently
validated groups from either. A terminal error in this optional candidate
stops that candidate rather than the primary receiver. The user-selected
multi-candidate mode retains its separate behaviour. On the current build,
the -5/+5 Hz QSSTV-source A/B WAV replays reconstructed both 277-byte
references exactly without operator-selected adaptive mode; primary traces
showed no groups, while the automatic early candidate delivered groups.
RX diagnostic snapshots were clean. Artifacts are
`.../decodium-qsstv-drm-app-l823xsqj` and
`.../decodium-qsstv-drm-app-b7ypr9p_`. Laboratory controls explicitly
disabling the automatic candidate received no groups from those files and
had clean diagnostics (`.../decodium-qsstv-drm-app-nlu9_wm_` and
`.../decodium-qsstv-drm-app-v5tdzxlm`). These are separate replays, not
independent live-transmitter tests.

Two normal Decodium process pairs transferred the A/B files byte-for-byte,
restored the legacy monitor, and reported zero audio diagnostic errors in
the recorded snapshots (`.../decodium-hamdrm-two-app-a-369mkhul` and
`.../decodium-hamdrm-two-app-b-z634kz1a`). Two further pairs forced the
automatic candidate to hit a one-frame test-only limit; both primary
receivers continued and delivered the exact images, with legacy monitoring
restored (`.../decodium-hamdrm-two-app-a-mc06ptev` and
`.../decodium-hamdrm-two-app-b-qkdb4su5`). Two 12-second deterministic
noise replays on A/2.5/normal/16/short and B/2.5/high/16/long produced no
groups or images and had clean RX diagnostics
(`.../decodium-qsstv-drm-app-rdnla2ep` and
`.../decodium-qsstv-drm-app-xrrwpzdw`). These selected controls do not
quantify false positives, CPU cost on slower systems, or live EasyPal/RF
interoperability. Controller and native adapter CTests passed on this Mac.

For an indicative local cost comparison, the same two clean QSSTV-source
A/2.3/normal/16/short WAVs were replayed in separate full-application runs
with the automatic candidate disabled and enabled. All four produced exact
277-byte references and clean RX diagnostics. With 19–20 `ps` samples per
run, process-wide median CPU was 37.9% versus 38.9% of one core for A and
34.2% versus 38.1% for B; peak sampled RSS ranged from 513,552 to 517,040
KiB. The samples include application startup, other application processing
and shutdown, and each condition was run only once per recording; these
figures are not isolated DSP costs, a general performance benchmark or
evidence for slower machines. Measurement artifacts, respectively
A-disabled/A-enabled/B-disabled/B-enabled, are
`.../decodium-hamdrm-measure-standard-n2l0lcj1`,
`.../decodium-hamdrm-measure-standard-l1e22b2m`,
`.../decodium-hamdrm-measure-standard-mxghlnoh` and
`.../decodium-hamdrm-measure-standard-9_vduv6k`.

`measure_hamdrm_replay.py` now records an explicit `--disable-auto-early`
comparison, clears candidate-selection environment variables and checks the
RX log to confirm the selected mode. It was exercised with A disabled and B
enabled; both transfers matched exactly and the selection check passed.
Those repetitions are `.../decodium-hamdrm-measure-standard-vpcg2e_x` and
`.../decodium-hamdrm-measure-standard-94vejmf7`. They do not change the
limits of the preceding measurements or establish live EasyPal support.

Two further local two-application comparisons combined a 0.27-gain 1 ms
echo, 0.01-amplitude deterministic noise, a 0.01-amplitude 1700 Hz tone,
and +125/-125 ppm simulated sample-rate offsets for images A/B respectively.
With the ordinary automatic two-candidate receiver, neither run completed;
the primary reported at most 1/7 validated groups and no image was saved.
In A, the harness reported a missing legacy-restoration marker before its
image-completion check; B explicitly reported incomplete RX. Both TX runs
completed, relay error lists were empty and all captured RX diagnostic
counters were zero. App artifacts are
`.../decodium-hamdrm-two-app-a-9eetnl7r` and
`.../decodium-hamdrm-two-app-b-630c_g72`; relays are
`.../decodium-hamdrm-two-app-echo-rut_wdki` and
`.../decodium-hamdrm-two-app-echo-4lswfbau`.

With the operator-selected multi-candidate adaptive mode, separate A/B
repeats under the same configured impairments transferred the 225-byte
images exactly and restored legacy monitoring; RX diagnostics and relay
error lists were clean. App artifacts are
`.../decodium-hamdrm-two-app-a-6gjf263h` and
`.../decodium-hamdrm-two-app-b-5sa5zhzp`; relays are
`.../decodium-hamdrm-two-app-echo-66zdosxd` and
`.../decodium-hamdrm-two-app-echo-3nzzpi2r`. These were separate real-time
signals, not identical recorded samples. They demonstrate the selected
configuration on these two trials, not a general threshold or proof of live
EasyPal interoperability.

The ordinary receiver now also runs the independently validated temporal
pilot candidate. It retains its existing untracked timing, and a terminal
failure of this optional candidate does not stop the primary receiver.
Repeating the combined echo/noise/tone and +/-125 ppm A/B scenarios above
with ordinary settings reconstructed both 225-byte images exactly, restored
legacy monitoring, and recorded zero RX diagnostic errors. Each temporal
candidate reported nine validated groups. App artifacts are
`.../decodium-hamdrm-two-app-a-tje3lfzs` and
`.../decodium-hamdrm-two-app-b-m1gutmxh`, with relays
`.../decodium-hamdrm-two-app-echo-lp2km2fn` and
`.../decodium-hamdrm-two-app-echo-z8l2nu6n`. The earlier ordinary failures
were separate real-time transmissions, not bit-identical replays.

The independent QSSTV-source -5/+5 Hz A/B files still reconstructed both
277-byte references in full-app replays with clean RX diagnostics
(`.../decodium-qsstv-drm-app-r2m14kbt` and
`.../decodium-qsstv-drm-app-f6qm4upb`). Two further app pairs forced both
optional candidates to reach a one-frame lab limit; the primary continued,
completed each exact A/B transfer and restored legacy monitoring
(`.../decodium-hamdrm-two-app-a-83anuur_` and
`.../decodium-hamdrm-two-app-b-_igiai27`). Two deterministic noise inputs
produced no groups or images, with clean RX diagnostics
(`.../decodium-qsstv-drm-app-o9zw2vam` and
`.../decodium-qsstv-drm-app-_5o_vg63`). These bounded noise checks do not
quantify false-positive probability.

On this Mac, two further clean-file replays with all three automatic
candidates sampled process-wide median CPU at 39.6% and 39.1% of one core;
maximum sampled RSS was 509,920 and 518,608 KiB. These one-run figures
include startup and unrelated application processing and cannot be treated
as isolated DSP benchmarks or evidence for slower hardware. Artifacts are
`.../decodium-hamdrm-measure-standard-zu0qs0ig` and
`.../decodium-hamdrm-measure-standard-85ug6cz4`. The comparison helper now
checks that both automatic-candidate log markers match the requested mode.
Controller and native adapter CTests passed. None of these observations
establishes arbitrary-channel recovery, RF or live EasyPal interoperability.

The explicit test-only temporal-pilot mode now avoids starting a redundant
automatic temporal candidate. Two isolated Decodium A/B transfers completed
exactly and restored legacy monitoring, and their logs confirmed the
temporal test mode without either automatic-candidate marker. Artifacts are
`.../decodium-hamdrm-two-app-a-e6ljukrm` and
`.../decodium-hamdrm-two-app-b-f85k_i58`; the adapter CTest also passed.
The updated measurement helper's disabled-candidate control was checked
against its RX log and received the exact A reference
(`.../decodium-hamdrm-measure-standard-e5de9wzv`).

With the three ordinary automatic candidates, two further full Decodium
process pairs transferred distinct 320x240 JPEGs of 6,481 and 3,673 bytes
over clean BlackHole audio. Both files matched byte-for-byte. After each
object completed, the primary's group counter advanced from 15 to 23 and
12 to 20 respectively, and every sampled inbox snapshot retained a complete
object with no error. All RX diagnostic counters for discontinuities, dropped
or rejected chunks, queue overflow and processing failures remained zero.
Artifacts are `.../decodium-hamdrm-two-app-a-akntckkc` and
`.../decodium-hamdrm-two-app-b-kdlm3xu8`; generated input JPEGs are in
`/tmp/decodium-hamdrm-large-ab`.

An earlier A run combined this post-completion requirement with the legacy
restoration check. The 6,481-byte image was exact and inbox snapshots stayed
complete, but restoration intentionally cancelled RX immediately after
completion, so no later groups could be observed and the harness correctly
failed that combined acceptance (`.../decodium-hamdrm-two-app-a-xjxpl2og`).
The harness now rejects this incompatible combination before launching, with
A/B controls for legacy-restoration and monitor-off modes. The successful
large-image checks above keep RX active and do not themselves test legacy
restoration; separate tests documented above cover restoration. These are
selected local-audio cases, not live EasyPal, RF or general performance proof.
