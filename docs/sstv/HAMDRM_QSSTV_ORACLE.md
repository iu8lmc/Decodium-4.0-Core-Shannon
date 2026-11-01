# QSSTV MOT data-group oracle

## Operator-selected experimental adaptive reception

The Digital SSTV page now exposes an explicitly experimental adaptive RX
option. It is off at startup, may only be changed while HAMDRM is idle, and
uses strict pilot thresholds with adaptive equalization and two timing
candidates. It can require more processing; this is not a claim of complete
EasyPal compatibility. The normal receiver remains the default. Backend
capability is checked before enabling it, and the UI disables the control
when the backend does not support it or an operation is active.

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
`5966159f4d284dd99227b8bf5bb097dc07ae303c7e67643bc46944ff0586b20` and
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

An iterative multistage MSC decoder can be selected only in the laboratory
test mode together with the experimental adaptive estimator. It retries a
packet after existing decoders fail and accepts it only when its packet CRC
passes. It is not enabled in ordinary Decodium sessions. Its clean output
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
comparison, not general channel or platform coverage. The strict-pilot
multistage path remains disabled outside explicit test mode.
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
fractional candidate was necessary in each app run. The app path remains
explicitly lab-only, without RF or Windows/Linux validation. Fractional-delay
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
