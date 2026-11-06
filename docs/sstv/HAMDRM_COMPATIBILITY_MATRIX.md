# Native HAMDRM compatibility matrix

Evidence updated through 2026-11-03. This file records executable evidence,
not planned capability. `Implemented` below means bounded native code and local
deterministic tests. It does not mean independent over-the-air interoperability.

## Feature matrix

| Layer or capability | Native status | Executed evidence | Interoperability status / remaining gate |
|---|---|---|---|
| Named profile registry | Implemented | 72 unique typed records validated by `test_hamdrm_core` | Payload table audited against the pinned QSSTV lineage; no independent waveform for all tuples. |
| CRC/integrity | Implemented | MSB-first CRC validated against executed QSSTV MOT groups and corruption tests | Old reflected-CRC local snapshots are recovered in memory after file checksum verification; network groups use the correct CRC. |
| MOT header/body groups | Implemented | Separately executed QSSTV single- and three-segment vectors parse and assemble, including duplicate and out-of-order groups; selected Decodium image recordings also decoded in unmodified QSSTV batch RX. See [oracle](HAMDRM_QSSTV_ORACLE.md). | These selected file-mediated waveform checks do not establish uninstrumented live repair, all profiles or RF interoperability. |
| Object segmentation/reassembly | Implemented | Out-of-order, exact duplicate, conflicting duplicate, missing and completion tests; instrumented upstream QSSTV A/B repairs after a three-second virtual-audio mute. | Actual RF carrier loss and broad peer coverage remain unverified. |
| BSR generation/parsing and transport | Implemented | Compact and extended forms were compared against extracted pinned QSSTV methods. Two Decodium applications and an instrumented upstream QSSTV peer exchanged A/B requests and repairs over virtual audio. See the oracle and architecture. | These controlled tests are not proof of EasyPal, RF or every QSSTV version. |
| Reed–Solomon RS1–RS4 | Implemented, bounded | Encoded containers match pinned QSSTV methods for two inputs at each level. Two Decodium applications exchanged A/B fixtures for every level; additional RS4 tests recovered injected missing segments. Modified QSSTV TX supplied RS4 A/B JP2 recordings received exactly by Decodium over virtual audio. | CPU work limits reject some large objects. Broader profiles, native EasyPal, real radio and other operating systems remain unverified. |
| Partial persistence/resume | Implemented | Atomic write/load, checksum damage, truncation, replay and remove tests | Local AppData behavior verified on macOS only; packaged Windows/Linux storage not verified. |
| JPEG/PNG/GIF/BMP boundary checks | Implemented | Signature, container, dimension, pixel and allocation-limit tests | Image acceptance boundary only, not proof that every type is an interoperable HAMDRM payload. |
| Received image access | Manual save and Gallery insertion implemented | A/B controller and QML tests plus two Decodium sessions saved byte-exact 225-byte images; the Mac legacy monitor resumed with fresh PCM. Artifacts `.../decodium-hamdrm-two-app-a-6gq0__5e` and `.../decodium-hamdrm-two-app-b-p1ufr0ph`. | The manual file picker was not visually exercised; the Gallery results below cover local virtual audio only. |
| JPEG2000 decode/encode | Implemented | Exact lossless RGB round-trip, malformed/truncated/limit tests with OpenJPEG 2.5.4; modified QSSTV TX supplied RS4 JP2 recordings recovered by Decodium | Packaged OpenJPEG dependencies, broad JP2 variants and unmodified live QSSTV JP2 exchange remain unverified. |
| QAM 4/16/64 | Implemented primitive | Mapping/demapping deterministic tests | A clean symbol round-trip is not a channel interoperability claim. |
| Narrow amateur OFDM A/B/E | Implemented primitive | FFT/DFT oracle, cyclic prefix, modem loopback and fractional-CFO/CP-sync tests; 12 complete complex symbols from the independent QSSTV OFDM stage match and demodulate | Amateur E is not broadcast DRM E; complete external waveform and receiver exchange still required. |
| FAC and MSC channel coding | Implemented pinned subset | Registered `test_hamdrm_channel`: DRM channel CRC-8/CRC-16, dispersal, K=7 convolutional coding/puncturing, hard Viterbi and a CRC-gated soft MSC retry, bit/cell interleaving, 45-cell FAC and Part-B-only 4/16/64-QAM MSC across all six A/B/amateur-E bandwidth tuples; 288 QSSTV FAC payload/QAM-sign combinations and 36 full QSSTV MSC MLC cell vectors match, plus standalone convolution/interleaver vectors. Operator-selected experimental adaptive reception includes CRC-gated flat and multistage retries; see the oracle. | Exact narrow QSSTV-pinned subset. Adaptive reception is opt-in and experimental; no SDC, MSC Part A, VSPP, hierarchical mapping or channel-state-weighted decoder. |
| FAC/pilot/MSC cell plan | Implemented pinned subset | Exact carrier classification and precedence, time/frequency/scattered/boosted pilots, and A/B symbol mapping compared with executable QSSTV for complete superframes at all six profiles | The separate native waveform receiver supports the documented subset; cell-plan checks alone do not establish interoperability for all profiles. |
| Native waveform RX backend | Implemented for the pinned native subset and wired locally | `HamDrmNativeRxBackend` consumes the existing Decodium shared PCM tap on a bounded worker; `test_hamdrm_waveform_adapters` covers loopback group delivery, queue/sample/session bounds, invalid rates, stream reset and synchronous cancellation. The Bridge installs the real hooks and the controller advertises RX only when they are connected. | QSSTV-generated MOT/packet/FAC/MSC/interleaver/mapper/OFDM PCM decodes and assembles two exact 16x16 JPEGs, with pixel checks; a playback process delivered two QSSTV-stage JPEG PCM fixtures through BlackHole into Decodium for E/2.5 kHz/16-QAM/high/short. Selected live A/B reception from the official QSSTV application, using a settings-isolation shim, is detailed below. Other profiles, RF, fading and clock-drift interoperability remain unverified. |
| Native waveform TX backend | Implemented for the pinned native subset and wired locally | `HamDrmNativeTxBackend` converts validated MOT groups to bounded 48 kHz PCM and hands a pull source to `SstvTxCoordinator`; adapter tests cover source bounds, played-audio completion, cancellation, configured FAC identity and the shared PTT release barrier. The Bridge supplies the existing output/CAT/PTT hooks. | For two nine-frame JPEG sequences, native TX samples match independently generated QSSTV-stage PCM within signed-16-bit quantization; QSSTV FAC and MSC receiver stages decoded native vectors, and its receive map recovered native interleaved frames across all six profiles and both depths. Selected live A/B reception by the official QSSTV application with the settings-isolation shim is detailed below; other profiles, physical output, CAT/radio and on-air exchange remain unverified. |
| Native Decodium QML page | Implemented, capability-gated | `test_hamdrm_controller`, `test_hamdrm_waveform_adapters` and offscreen `test_sstv_digital_qml` pass; the full `decodium_qml` target compiles and links with the Bridge-owned native RX/TX backends | The page exposes named profiles, real connected waveform capability, object/missing/BSR/resume state and cancellation. Packaged-platform rendering and live station checks remain. |
| QSSTV interoperability | Partial evidence | File-mediated A/B in multiple profiles in both directions; A/B live image exchange with the official QSSTV-Experimental 10.0 binary in both directions over virtual audio, with an injected settings-isolation shim; and live receive/request/repair with an instrumented upstream QSSTV peer after a three-second audio mute. Both peers reconstructed selected images exactly. See the oracle and architecture. | The isolation shim changes process configuration, and other profiles, uninstrumented live repair, EasyPal and RF remain unverified. |
| EasyPal interoperability | Partial live bidirectional evidence | EasyPal 07/OCT/2014 under Wine and Decodium exchanged two images in each direction over BlackHole, with operator confirmation of both EasyPal RX images. Pixel comparisons and one independently decoded captured signal are detailed below. Additional public recordings were compared with QSSTV references. | One profile and adapted runtime only; live EasyPal BSR repair, hybrid exchange, other profiles, native Windows and RF remain unverified; do not inspect proprietary code. |
| EasyPal-style hybrid radio/file-server mode | Partial: `de_` FTP upload, reference transmission and manual download; `.de` receive | [TRXAMADRM author's documentation](https://pa0mbo.nl/ties/public_html/hamradio/txamadrm/txamanew.pdf) describes a radio header referencing a file uploaded separately to a server; instrumented QSSTV exchanged A/B hybrid messages with Decodium in both directions via WAV and local FTP. | Decodium's separate HTTPS remote sharing and E2EE are not evidence of this on-air format. Unmodified live QSSTV, EasyPal and RF tests remain open. |
| KG-STV | Not implemented | None | HAMDRM is not KG-STV. An authoritative public specification and independent vectors have not been established. |

The Windows 11 EasyPal 07/OCT/2014 experiment over SonoBus PCM and Wi-Fi
did not reconstruct an image in Decodium. The same recorded audio was also
processed by QSSTV-Experimental 10.0: it reported six good and six bad CRC
blocks, no header segment and no decoded image. Thus this experiment does not
isolate a Decodium interoperability defect from the audio path. The WAV and
QSSTV log were retained locally at `/tmp/decodium-native-easypal-rx4/`;
they are temporary laboratory artifacts, not committed fixtures. The user
accepted partial EasyPal interoperability as the completion criterion; the
unverified cases in the table remain limitations, not passing results.

The corresponding Decodium log explicitly reports HAMDRM RX started with
`hamdrm-B-2500-high-16-QAM-long`; the unrelated `profile=W2300` in the
general laboratory audio overrides is not the HAMDRM RX profile. This confirms
the receiver configuration in that run, not the actual transmitted FAC
parameters or the cause of the failed transfer. A direct Windows WAV bypassing
SonoBus is still needed to distinguish source and network effects.
The two channels of the retained 120-second, 48 kHz signed-16-bit recording
are sample-for-sample identical, so selecting the other recorded channel cannot
recover information absent from this capture. This does not rule out loss or
distortion before recording.
Five-second RMS windows show a sustained higher-level interval roughly from
20 to 100 seconds within the 120-second capture, with lower-level audio before
and after it. This makes a simple start/end truncation less evident, but level
alone cannot prove that the complete EasyPal transmission or its header was
captured; no conclusion about signal completeness follows from this check.

As a decoder control, the current offline analyzer was run with the same
B/2500/high/16-QAM/long settings and pilot thresholds on the retained EasyPal
Wine B recording and on the Windows/SonoBus capture. The Wine recording
reconstructed its reference exactly (101 groups, one complete object); the
network capture produced no accepted frames or objects and 1,391 pilot
failures. Logs are in the temporary directory
`.../decodium-easypal-compare.wzAQey`. The two recordings are different
transmissions, so this comparison checks the analyzer and describes the
observed failure; it cannot attribute the failure to Windows, the source,
the network or a particular receiver limitation. QSSTV's independently
reported CRC blocks above remain a separate observation.

The same two recordings were then analyzed with `adaptive-early` under the
same profile and thresholds. The Wine control again recovered its reference
exactly (101 groups). The Windows/SonoBus capture yielded 69 accepted frames
and five data groups for a partial transport, but no header and no complete
image; the analyzer reported `exact=0`. This is closer to QSSTV's partial
result and shows that the flat-only offline diagnostic above does not describe
all receiver candidates. It does not prove that the live application would
reconstruct the image. Logs are in
`.../decodium-easypal-adaptive.BfcGtL`.
The `timing-hybrid` diagnostic also recovered the Wine control exactly,
and reported 69 frames and five groups without a header for the network
capture; it did not complete the image. Its logs are in
`.../decodium-easypal-timing.NiKqiZ`.
An exploratory A/B check with `adaptive-early` raised the pilot-error limit
from 0.12 to 0.30 and 1.00 and used a 0.70 corrupted-pilot fraction. The
Wine control still decoded exactly for both settings, while the network
capture still produced only five groups without a header; FAC failures rose
to 4 and 52 respectively. Logs are in
`.../decodium-easypal-pilots.JeCoaX`. These two trials provide no basis for
loosening production thresholds or for diagnosing the audio path.
With frame tracking and temporal-candidate merging enabled in the offline
analyzer, the Wine control again decoded exactly; the network recording
yielded five groups without a header, so the additional candidate did not
complete that image. Logs are retained in
`.../decodium-easypal-temporal.z1uWOK`. The recordings are different
transmissions and this remains a diagnostic rather than a controlled test of
the same signal before and after SonoBus.

Controller-level compact and extended BSR A/B tests additionally switch to
another valid profile after receiving a repair request. Repair is refused
while that profile is selected; after restoring the original profile, the
same request repairs the image byte-for-byte. These tests use in-process
controller backends and do not establish over-the-air behaviour or define
how another application's extended BSR profile field should be interpreted.

For an in-memory partial object, Decodium now retains the profile selected
when the first MOT group was received. The extended BSR uses that stored
profile even if the operator selects another one before requesting repair.
Controller tests cover compact and extended requests, with two different
images, and verify that switching profiles leaves the request unchanged and
that the normal selective repair reconstructs the original bytes. The
version-2 partial-object persistence format now records the profile; older
version-1 snapshots remain readable and, because they have no recorded
profile, still fall back to the currently selected profile. Older Decodium
versions are not guaranteed to read new version-2 snapshots. The stored
profile is the locally selected receiver profile, not independently
detected FAC metadata. A locally available instrumented QSSTV Experimental
source snapshot associates its extended BSR mode code with the receive-side
transport block; its inspected `processFIX` does not use the extension.
That source snapshot is modified, so this inspection is not a new live peer
interoperability result.

The partial-store A/B tests save and restore two different profiles and
reject an unknown profile even when the file checksum has been recalculated.
The controller A/B tests destroy and recreate the receiver controller,
select a different profile, resume the partial image and verify that its
extended BSR still matches the original. They also finish the image; existing
version-1 and historical CRC snapshot tests continue to pass. These are
local controller/storage tests, not an RF test.
An additional A/B controller test starts a separate process to save each
partial and a second process to resume it with a different selected profile.
The second process checks the original extended BSR profile, completes the
object and compares its bytes. These are real process boundaries around the
controller and store. The later application test described below also covers
full Decodium process restarts.
The digital page can now discover saved partial transport IDs and place a
selected ID into the existing manual resume field. The discovery list only
includes regular, non-symlink files whose names match the partial-store naming
scheme; listing a name does not validate its contents. Resume still performs
the full checksum and structural checks. Local A/B QML tests create saved
partials in an isolated directory, choose between two IDs through the page
and resume the chosen object; a store test checks two IDs and ignores an
unrelated filename. Two-Decodium A/B transmissions after this change both
reconstructed the expected bytes over virtual audio; they did not exercise
the discovery controls in the full application's visible interface.
The A/B QML tests also discard the first restored object, refresh the list,
resume the remaining object, and verify that an empty refresh preserves a
manually entered ID. An attempt to inspect the full macOS application via
the available UI automation timed out, so it provides no visual evidence.
The local storage test additionally created a symbolic link with a valid
partial filename: it was excluded from discovery and rejected by load. On
platforms where the test cannot create a symbolic link, that specific check
is skipped rather than asserting unsupported filesystem behaviour.
The same separate-process A/B check also rewrites each saved snapshot in
the legacy version-1 format before restarting. Both legacy objects resume
and complete; their extended BSR uses the newly selected profile, because
the legacy snapshot contains no original-profile information.

Two additional A/B tests now launch the full Decodium application twice in
test mode, using an isolated HAMDRM partial-store root. The first process
seeds a partial image with one profile; after it exits, the second selects
the other profile, locates and resumes the saved partial, checks that the
extended BSR contains the first profile's compatibility code, and completes
the image with byte-for-byte verification. The tests use generated MOT
groups and an application-only laboratory hook; they do not exercise radio,
the visible QML controls or a peer transmitter. The A/B cases are registered
as `test_hamdrm_partial_restart_app_a` and `_b` on macOS.
The laboratory now sets an isolated macOS home and verifies that application
data and the HAMDRM store are inside its temporary directory, preventing
successive runs from accumulating persistent operator-profile data.
The partial-store size calculation now checks additions and multiplication
before evaluating the maximum snapshot size. A/B tests use overflowing
object/header limits, require a bounded failure on save and load, and verify
the pre-existing snapshot is unchanged. The isolated full-application
restart A/B regressions also passed after this change.
The per-group limit check likewise avoids overflowing when adding protocol
overhead to an extreme configured segment limit. A/B storage tests save and
reload a normal partial with two such limits; ordinary HAMDRM and the two
isolated application restart regressions passed after the change.
After adding version-2 persistence, the two-Decodium audio-loss A/B repair
regressions also passed again, reconstructing 7,745/3,905 bytes exactly.
Those audio runs exercise repair with the new build; they do not restart the
receiver or verify profile changes across processes.

On completion, the controller now also preserves the stored receive profile
when emitting the decoded image to the Gallery. Previously it replaced that
profile with the current selection. A/B controller tests with real JPEGs
reproduced the wrong Gallery profile before the fix and verify the correct
profile after restoring a partial with a different current selection. After
the change, two Decodium processes exchanged A/B images exactly over virtual
audio. Those application runs did not change profiles or resume partials;
those operations were tested in the controller.

After the in-memory profile fix, two local application sessions repeated
the A/B selective-repair laboratory with ten seconds of the saved initial
waveform muted. BSR and repair travelled over BlackHole 16ch and the
reconstructed A/B files matched 7,745/3,905 expected bytes respectively.
Those live runs did not change profiles during reception; profile switching
is covered by the controller tests above, not by these audio runs.

On 2026-11-04 two further public recordings were checked. The uploader of
[sample A](https://www.youtube.com/watch?v=bRqvrdQeC8Q) says it was produced
with EasyPal; [sample B](https://www.youtube.com/watch?v=OB2Tp2opBV4) is
presented by its uploader as an EasyPal reception test, but does not explicitly
identify the transmitter. Neither identifies the EasyPal version, and these
claims were not independently verified. The downloaded audio hashes are
`4be316b6c8d528dcaba379690d408a8e4fdea4bda2c10065c99a025f4bc5ce3a`
(A, WebM) and
`cdd0467ecd7a495813725242740991b7163456eeccb27c188d22399f73c9a6eb`
(B, M4A). After conversion to 48 kHz mono PCM WAV, their SHA-256 values are
`5ba69af79820c9bbe068d685e94866810384ba2686b22326c1e50b0a7bb0219f` and
`c342ca241620c5278197b8604e42e54d71eafbb3300134100c72409dbd37074d`.
The locally built QSSTV-Experimental batch decoder recovered reference JP2
objects, identifying A/2.5/high/64-QAM/long and B/2.5/high/16-QAM/long.
QSSTV reported Jasper image-decoding errors for both, despite saving the JP2
objects; FFmpeg identified their image dimensions. The QSSTV reference hashes
are `55fa95f664d82cf0d0cb5a57a545cb661e071aa3c33de431cedb9db7960a62fd`
(105,957 bytes) and
`a2642ca6c2244134bc04ad7d6922465200274d82b56bbf0a71120862ad65e38a`
(21,220 bytes). The native offline analyzer matched both references exactly;
the A case reconstructed an RS2 container with 83 missing body segments.
Crossed-reference negative controls reported `exact=0` for both recordings.
The full Decodium application also recovered both exact files through
BlackHole with its ordinary receiver; artifacts are
`.../decodium-qsstv-drm-app-3q2i0pnh` and
`.../decodium-qsstv-drm-app-91gzr569`. Audio and reference files are retained
under `/tmp/decodium-easypal-video-oracle` and
`/tmp/decodium-easypal-video-oracle-b`. These are recorded-signal comparisons
against QSSTV, not original source-file comparisons, independently verified
EasyPal versions, a live two-way EasyPal exchange, or RF tests.

`tests/sstv/tools/check_public_easypal_recordings.py` checks the four external
WAV/reference hashes, repeats both offline comparisons and the crossed-reference
negative controls, and optionally repeats both full-application BlackHole
replays with `--live`. It does not download or redistribute third-party audio.
The offline A/B and hash-mismatch negative control passed on 2026-11-04.
Both full-app logs also reported successful Gallery storage. The resulting
Gallery PNGs have dimensions 3,648×2,736 and 480×480, and their decoded RGB
pixel hashes match the respective QSSTV JP2 references exactly. The optional
`--gallery-a` and `--gallery-b` arguments repeat this pixel check; crossed
Gallery images were rejected as a negative control. These checks validate the
stored images, not interactive rendering of the Gallery UI.

An archived Windows installer named `EasyPal-07-OCT-2014-Setup.exe` was
downloaded from the [QSL.net public archive](https://qsl.net/lw7dfm/Software/SSTV/)
for later independent testing. Its SHA-256 is
`7190ef73f643d60afa5f064fa238b6055e48f5cd18fbc08cd861ac55cb8d4763`;
`file` identifies a 32-bit Windows GUI executable. On 2026-11-04 the installer
linked by the [Gympie Communications & Electronics Group](https://www.gceginc.org.au/sstv/sstv.php),
which identifies EasyPal's author as a former member, and the independent
[Western Australian SSTV mirror](https://www.wasstv.net/wasstv.net/EasyPal-07-OCT-2014-Setup.exe)
both had the same SHA-256 as the QSL.net copy. This corroborates distribution
history but is not a publisher signature or proof that the binary is safe.
On 2026-11-04 the installer was run into an isolated temporary Wine prefix on
Apple Silicon macOS 27.0.1, using MetalSharp Wine 5.0 (Wine 11.20). The Wine
archive matched its published SHA-256 and macOS Gatekeeper accepted its
notarized Developer ID signature. A sandbox denied IP network access and reads
or writes under the user's home directory; a TCP connection to an external IP
was rejected. The installer exited successfully, but EasyPal itself reported
unhandled exception `0x0eedfade` during startup. The same exception occurred
when launching the extracted executable directly, including after copying its
supplied user data. As a basic 32-bit control in the same sandbox, Wine's
`syswow64\cmd.exe /c echo` returned the expected output and exit code zero.
The cause has not been established: this is not evidence
of an EasyPal protocol incompatibility or of successful audio exchange.

A Wine startup trace identified `REGDB_E_CLASSNOTREG` for
`{22D6F312-B0F6-11D0-94AB-0080C74C7E95}` immediately before the first
exception. This is the Windows Media Player 6 ActiveX class; a historical
[WineHQ report](https://www.winehq.org/pipermail/wine-bugs/2015-August/419602.html)
describes the same missing class in an older EasyPal version. The `msdxmocx`
package listed by [Winetricks](https://github.com/Winetricks/winetricks/blob/master/src/winetricks)
was downloaded from its listed mirror and matched its published SHA-256
`a39b2b9735cedd513fcb78f8634695d35073e9d7e865e536a0da6db38c7225e4`.
Only `msdxm.ocx` was extracted and registered inside the temporary prefix.
The next startup passed that failure but hit an unimplemented Wine
`msvcp60.dll` export. The Microsoft VC6 redistributable identified by the
same Winetricks source matched SHA-256
`c2eb91d9c4448d50e46a32fecbcc3b418706d002beab9b5f4981de552098cee7`;
only its `msvcp60.dll` was extracted into the isolated prefix and selected
as a native override. EasyPal then remained running without either reported
exception, but its usable GUI and audio have not yet been verified. These
steps change only the test environment, not Decodium or the independent
application's protocol implementation.

The macOS automation interface timed out when selecting the Wine application.
The same timeout occurred for Wine's Notepad control both with the sandbox and
in a separate unsandboxed control run; the EasyPal run itself remained confined.
Thus the timeout alone does not identify an EasyPal or sandbox failure. The
Notepad control process and its Wine server were stopped after the comparison.

On 2026-11-04, a separate temporary macOS application bundle containing the
current Decodium development executable was launched with an isolated test
profile and CAT and monitoring disabled. macOS listed that process as running,
but the same automation interface timed out when asked to select its window.
The observation was repeated with both isolated Gallery test profiles and
both processes were stopped afterwards. This establishes that the observed
window-selection timeout also affects this separate Qt application in the
current automation environment; it does not establish why it happens, whether
either window was visually usable, or that EasyPal itself is malfunctioning.
The same automation interface subsequently selected macOS Finder and a
separately running DecoDXLog Qt application successfully. Thus this is not a
general inability to select every macOS or Qt window; the cause for the two
timed-out processes remains unconfirmed.

The same Mac has VirtualBox 7.2.18 installed, but `VBoxManage list vms`
returned no configured guests on 2026-11-04. This does not supply a Windows
environment for the missing EasyPal test. A sandboxed EasyPal process was
subsequently launched and the operator was asked whether its window is visible;
until that observation is available, process liveness alone is not GUI proof.

The operator subsequently reported that the EasyPal window is visible. The
automation interface still could not select it. A separate test-mode Decodium
receiver was started on BlackHole 2ch with E/2.5 kHz/high/16-QAM/long, CAT
disabled, and synthetic A/B JPEG files were copied into the isolated EasyPal
`TxFiles` directory. Manual transmitter configuration and transmission were
requested; until actual audio and received file contents are observed, this
preparation is not a successful direct interoperability test. The receiver's
temporary logs and readiness record are in `/tmp/decodium-easypal-live-manual`.

The operator later configured the original EasyPal 07/OCT/2014 application
running under the isolated MetalSharp Wine prefix to send audio to BlackHole
2ch and manually transmitted two distinct 320×240 24-bit BMP test images.
The full Decodium receiver, on the same virtual device and configured for
B/2.5 kHz/high/16-QAM/long, received both and saved JPEG2000 objects and
Gallery images. Comparing RGB pixels with the actual BMP inputs gave A MAE
0 and maximum difference 0; B MAE 0.000143 and maximum difference 2 on the
0–255 scale. Crossed-image controls gave MAE 137.519 for both. The received
JP2 SHA-256 values are
`0a83a79ebab42a2e2ffd28510131f2c5843fe35f595c48c5f1078b06227f168c` and
`f76a3ea88d28465a166c5bab991c188e8a4133afdec201d238fb72135d8fe2a3`.
The receiver's diagnostic snapshots reported zero discontinuities, dropped
chunks, rejected HAMDRM chunks, queue overflows and processing failures.

The B audio was separately captured from BlackHole through PortAudio; the
locally built QSSTV-Experimental batch receiver decoded the identical JP2
bytes and identified B/2.5/high/16-QAM/long. Its Jasper display decoder
reported an IHDR component-type warning after saving the JP2. The earlier A
capture through AVFoundation did not acquire in QSSTV and remains an
unresolved capture or decoding limitation, although the full Decodium RX
received A correctly. Test files and logs are in
`/tmp/decodium-easypal-live-mode-b`. This proves two live EasyPal-to-Decodium
transfers for one profile through local virtual audio, not yet the reverse
direction, other profiles, native Windows or RF operation.

For the reverse direction, two 320×240 JPEG images were sent by full Decodium
application instances through BlackHole to that same EasyPal installation,
using B/2.5 kHz/high/16-QAM/long and no Decodium CAT. Both transmitters
reported `Completed`, EasyPal saved `reverse-a.jpg` and `reverse-b.jpg` under
its isolated `RxFiles`, and the operator reported seeing the colour grid and
colour bars in the RX window. The saved JPEGs are not byte-identical to the
transmitted JPEGs: their pixel MAE is 2.01056 and 1.00608 respectively,
with p95 differences 7 and 5 on the 0–255 scale. Crossed-reference pixel MAEs
were 136.662 and 137.028. The output hashes are
`5f4db3445e8ee295a72459ea8dbf29d0061ae34f6d00d6ca81181833bb2fa83c`
and `aa3db457a84de0e49e16e7d6d0dd324139c01a86be8c90a8dfbf5edaa9476c0a`;
source hashes are
`df0749a94eff9d9f7219087d9b5c3bc60dcd3cf0942ee53abee0b8a7c02720c0`
and `6d0c7174ebb82b3f6f0eba3a70456b86dd215fd27db23c8c0d78c64bde9cf4ec`.
Preserved copies are in `/tmp/decodium-easypal-live-mode-b`, and TX logs are
under `/tmp/decodium-easypal-live-reverse-a2` and
`/tmp/decodium-easypal-live-reverse-b`. An initial BMP transmission was
stopped because the uncompressed file would take too long; it is not counted.
These A/B checks demonstrate selected live bidirectional image interoperability
with EasyPal 07/OCT/2014 under Wine via virtual audio. They do not establish
EasyPal-originated BSR audio, hybrid exchange, other settings, native Windows or RF.
The synthetic input images, received image files and a lossless cropped B
audio capture are also retained under `tests/sstv/fixtures/easypal-2014-live`
(approximately 1.5 MiB total), with provenance in its README. The cropped
capture was decoded byte-exactly by both QSSTV and the Decodium analyzer.

The public [easypal-toolkit](https://github.com/milesburton/easypal-toolkit)
was inspected as another possible independent peer at commit
`60d0be0e89ef7b683750b345ec907c65b578956e`. Its README describes an
EasyPal encoder and decoder, but its own `drmFramer.ts` calls its framing a
simplified self-consistent subset, and its `drmConstants.ts` specifies two
FAC cells per frame; the QSSTV-pinned Decodium cell plan uses 45. Its encoder
also serialises its own segment-number/total/data/CRC structure. This is a
source inspection, not an executed cross-decoding test or a conclusion about
the original EasyPal program. Until its actual signals and provenance are
validated against an independent EasyPal exchange, its name alone cannot be
used as evidence of EasyPal interoperability.

An A/B self-roundtrip probe was then run on macOS with Node 22.22.3 against
that exact checkout. A disposable copy of its TypeScript utilities had only
the local import suffixes adjusted for Node's type stripping. Browser image
conversion was replaced by a test stub returning each existing 225-byte JPEG
fixture unchanged; the toolkit's encoder and decoder algorithms were not
edited. Each WAV contained 2.40 seconds of mono 12 kHz PCM, but its synchronous
decoder returned 346 bytes rather than the 225 input bytes for both images;
neither output matched the input. This is a negative result for that specific
Node harness, not an independent comparison with EasyPal or proof that the
browser workflow fails. The generated files and probe are under
`/tmp/decodium-easypal-toolkit-execution`; no toolkit code was imported into
Decodium. The toolkit cannot yet serve as a trustworthy independent oracle
for this goal on this evidence.

Another potential independent implementation is described in the
[Rafe HamDRM documentation](https://patrick.lidstone.net/wiki/protocols/hamdrm/),
which gives Python TX/RX APIs and says its tables are extracted from QSSTV.
The document describes internal loopback and explicitly leaves live
EasyPal/QSSTV interoperability unverified. Its installation page uses the
placeholder `git clone <this-repo>` rather than a public repository URL; no
executable source or pinned release was identified in this investigation.
It is a lead, not an executed comparison or proof of compatibility.

Another possible independent peer, [OnAir v3.5.0](https://github.com/adswill/OnAir/tree/v3.5.0)
(commit `b7cf9ac054e6e392e30e6c265b5810071644f6f1`), was inspected on
2026-11-04. Although its UI describes HamDRM/EasyPal, its
`core/include/dect2/hfdig/hfdig_hamdrm.h` explicitly says its cell layout and
data groups do not follow QSSTV/EasyPal's exact bit layout and were not tested
against their recordings. Its `hfdig_hamdrm_int.h` uses 38 FAC cells rather
than the 45 in Decodium's independently QSSTV-checked cell plan. This is
source inspection, not an executed cross-decoding test; the application cannot
be counted as evidence of EasyPal compatibility merely from its mode name.

Another candidate is PA0MBO's TRXAMADRM: its
[author's documentation](https://www.pa0mbo.nl/ties/public_html/hamradio/txamadrm/index.html)
describes HAMDRM A/B/E and claims compatibility with the October 2014 EasyPal
hybrid mode. On 2026-11-02 the linked `trxamadrmv3_7.tgz` returned HTTP 404
from the author's server, and no copy was executed. The published description
is a lead for a future independent test, not proof of Decodium/EasyPal
interoperability; the program also targets Linux/ALSA, so a suitable isolated
environment and provenance check would be needed before use.

The author's older [RXAMADRM download page](https://pa0mbo.nl/ties/public_html/hamradio/rxamadrm/index.html)
links a 2014 version 3.6 archive; its original URL returned 404 on
2026-11-03. An [Internet Archive capture](https://web.archive.org/web/20160316111635id_/http://pa0mbo.nl/ties/public_html/hamradio/rxamadrm/trxamadrmv3_6_18nov14_64b.tgz)
was recovered with SHA-256
`e001e592dfe742a435bd22220ed24b3c0ad9bf669e3559230f8f0aaca9939dcd`.
Its SHA-1 matches the archive's CDX digest, establishing consistency with
that capture, not a publisher signature. The archive contains GPL-labelled
source. Its 64-bit Linux executable was not run; the hybrid sources were
compiled locally on macOS for an isolated MOT comparison.

The unmodified `CMOTDABEnc` in `DABMOThybrid.cpp` generated two MOT header
groups for synthetic `de_TEST1-1-image-a.jpg` and `de_TEST2-1-image-b.jpg`
objects with 16 arbitrary body bytes. Decodium parsed both group CRCs,
filenames, body sizes and transport IDs 21808/64816; corruption of either
CRC was rejected by `test_hamdrm_core`. The 50-byte fixtures are
`tests/sstv/fixtures/trxamadrm-3.6-hybrid-header-{a,b}.bin`, SHA-256
`92091a8106e4ff31e0c68ff1618316e6b939f50fa47911fa16838f09c9cf5b20`
and `a6ff717d1181251dfc5775366fd71604276ef0bcb01b8ea2a8cfe5e25fbaccd2`.
The same encoder also generated the corresponding 27-byte body groups,
`trxamadrm-3.6-hybrid-body-{a,b}.bin`, SHA-256
`ec25ff61416919877d302745f5ac2e09262b057a1029bd92c5fb020853b51823`
and `98a1cdc8a723cd3da7cdfec44dec267eb99db208b21a86765a6835613cd75310`.
The Decodium assembler reconstructed both original 16-byte synthetic bodies;
body CRC corruption was rejected in each case.
`tests/sstv/tools/run_trxamadrm_hybrid_header_oracle.sh ARCHIVE` checks the
archive hash, rebuilds a small adapter against the unmodified source in a
temporary directory, and reproduces all four groups; it needs a C++ compiler and
FFTW headers. These are generated MOT objects with synthetic bodies, not
captured EasyPal hybrid transmissions or evidence of file-server exchange.
These fixtures do not prove upload, download, credential handling or two-way
EasyPal hybrid interoperability.

The same hash-pinned archive's unmodified `rs2encode.c` and `rs2.c` were
compiled separately and run on the 225-byte A/B JPEG fixtures in an isolated
temporary directory. Each produced an RS2 container byte-identical to
Decodium's encoder; Decodium decoded both containers back to the exact input
with the `jpg` extension. A crossed-image control failed as expected.
`tests/sstv/tools/run_trxamadrm_rs_oracle.sh ARCHIVE` verifies the archive
hash, builds both utilities, and repeats the comparison. The historical C
source generates compiler warnings; this is a selected file-format comparison,
not evidence that its GUI or an actual EasyPal peer exchanged these files.
Separately, two current-build Decodium application pairs sent the same A/B
images with RS2 over virtual audio, recovering each 225-byte file exactly;
artifacts are `.../decodium-hamdrm-two-app-a-h02i_bap` and
`.../decodium-hamdrm-two-app-b-1v040sj4`. Those transfers did not run the
TRXAMADRM executable as an on-air peer.

The same independent comparison was extended to the archive's RS1, RS3 and
RS4 encoders. For each level and both JPEG fixtures, the archived program's
container matched Decodium's encoding byte-for-byte and Decodium recovered
the original file and extension. Crossed-image controls failed for all four
levels. The hash-pinned `run_trxamadrm_rs_oracle.sh` now reproduces the full
RS1–RS4 comparison. This covers these two small files and the archived source
build; it is not a live TRXAMADRM or EasyPal exchange.

An isolated BSR comparison on 2026-11-04 extracted the segment-reading loop
unchanged from this hash-pinned TRXAMADRM 3.6 `DABMOTbsr.cpp`. A guarded
array made the effect of out-of-range indices observable without corrupting
the harness. With separated missing segments, Decodium compact and extended
requests both yielded the expected indices. With consecutive missing
segments, the compact request contains `-1` as a range marker: this historic
reader instead uses it directly as an array index and writes before the
array, while the extended request lists each missing segment and is read
correctly. Actual A/B BSRs received by two Decodium app pairs over BlackHole
showed the same result: extended A requested 1, 2 and 3 successfully; compact
B requested 1 through 3 but the historical loop recorded only 1 and 3 and
touched the guard. App artifacts are
`.../decodium-hamdrm-two-app-a-w326lord` and
`.../decodium-hamdrm-two-app-b-h06rqbz0`. The default Decodium selection is
QSSTV extended. `tests/sstv/tools/run_trxamadrm_bsr_oracle.sh ARCHIVE [A B]`
reproduces the isolated checks; this is not a live TRXAMADRM repair and does
not establish how EasyPal handles its compact range marker.

For a live transmitter comparison, the same hash-pinned TRXAMADRM 3.6
archive was built natively on Apple Silicon with PortAudio. The laboratory
patch `tests/sstv/fixtures/trxamadrm-3.6-macos-lab.patch` corrects a reversed
`memset` in that historical PortAudio backend and requires an explicitly
supplied temporary configuration instead of reading the user's home. The
unsupported Clang option `-fno-for-scope` was removed through a build flag;
the HAMDRM codec was not edited. The resulting `drm` executable has SHA-256
`ce071e9babd926f769dc7935ac5bf6f5f6a9f057ca8e576a41f5b5c6697ec0e5`.

Two distinct 225-byte JPEGs were transmitted by that running independent
application over BlackHole 2ch into full Decodium receivers using the selected
E/2.5/high/16-QAM/long profile. Both outputs matched byte-for-byte and RX
audio diagnostics were clean; the transmitter logs confirmed BlackHole.
CAT and PTT were disabled in the lab. The repeatable runs are
`.../decodium-trxamadrm-live-a-zv6qwk50` and
`.../decodium-trxamadrm-live-b-4mkuaeow`, made with
`tests/sstv/tools/run_trxamadrm_live_tx_lab.py`. An earlier run passed an
absolute source filename, which the historical transmitter put in the MOT
header; Decodium rejected this unsafe path. The successful tests use a plain
filename. This is live one-way TRXAMADRM-to-Decodium audio with a modified
laboratory build, not a live repair, EasyPal exchange, or RF test.

The reverse direction was also tested with the archive's RXAMADRM receiver
built natively for this Mac. The separate laboratory patch
`tests/sstv/fixtures/trxamadrm-3.6-macos-rx-lab.patch` replaces the ALSA
capture with blocking PortAudio input restricted to the unique BlackHole 2ch
device, disables X11 waterfall drawing, and supplies a macOS `malloc.h`
compatibility include. Its existing signal conversion, demodulation, channel
decoding and MOT assembly are retained. This is therefore an instrumented
historical receiver, not the unmodified Linux application. The receiver was
confined to a temporary working directory, with IP networking and the user's
home denied; CAT and PTT were disabled in Decodium.

In two separate live tests Decodium transmitted distinct 225-byte JPEGs over
BlackHole using E/2.5/high/16-QAM/long. The historical receiver reconstructed
each exact file; Decodium reported its TX completed in both tests. The
prepared receiver SHA-256 is
`81de7bc40d3ae716aca8924ee3fba89a5d0ea3f6b7f858eb36fdcd7101120dbc`;
the filter file is checked separately by the harness. Artifacts are
`.../decodium-trxamadrm-live-rx-a-2n4crxj9` and
`.../decodium-trxamadrm-live-rx-b-aa7p93yp`, and
`tests/sstv/tools/run_trxamadrm_live_rx_lab.py` repeats the test with that
prepared receiver. Together with the two preceding TX tests, this establishes
selected live virtual-audio exchange in both directions with adapted
TRXAMADRM/RXAMADRM builds; it does not establish BSR repair, unmodified Linux
runtime behaviour, EasyPal, other profiles or RF.

The selected live image exchange was additionally repeated in both directions
using E/2.5 kHz/4-QAM/high/long instead of 16-QAM. Two different 225-byte
images arrived byte-for-byte at the receiving application in each direction.
TRXAMADRM-to-Decodium artifacts are `.../decodium-trxamadrm-live-a-6_yxz0rt`
and `.../decodium-trxamadrm-live-b-tk7b3hrc`; the reverse artifacts are
`.../decodium-trxamadrm-live-rx-a-9jwkjj2i` and
`.../decodium-trxamadrm-live-rx-b-fe4yulm7`. Both live harnesses accept
`--qam 4` for this profile; their default remains 16-QAM. These selected
virtual-audio transfers do not establish other profiles or EasyPal exchange.

The same four-directional A/B check also passed with E/2.5 kHz/64-QAM/
high/long: each 225-byte image was recovered exactly. Independent transmitter
artifacts are `.../decodium-trxamadrm-live-a-12qvn6qn` and
`.../decodium-trxamadrm-live-b-g3_m5u73`; independent receiver artifacts are
`.../decodium-trxamadrm-live-rx-a-zdc6bsks` and
`.../decodium-trxamadrm-live-rx-b-m1dpy9nf`. The live harnesses repeat these
cases with `--qam 64`. Together with the selected 4- and 16-QAM tests, this
covers the three constellations for this particular profile and clean virtual
audio path, not all profiles, RF conditions or a live EasyPal exchange.

The E/2.5 kHz/16-QAM/normal/short profile was then exercised with two
distinct images in both live virtual-audio directions. TRXAMADRM-to-Decodium
artifacts are `.../decodium-trxamadrm-live-a-0cp0ouhb` and
`.../decodium-trxamadrm-live-b-8dqg9fkp`; the reverse artifacts are
`.../decodium-trxamadrm-live-rx-a-ywxqsxxq` and
`.../decodium-trxamadrm-live-rx-b-w6z8a_b3`. Each receiver recovered the exact
225 bytes. The live harnesses expose `--interleaver short --protection normal`
for repetition; previous defaults remain long and high. This is one additional
profile and does not establish direct EasyPal or RF interoperability.

Decodium then sent its extended A and compact B BSR fixtures over BlackHole
to the adapted historical receiver. It saved both `bsr.bin` objects exactly
and Decodium reported completed TX. Artifacts are
`.../decodium-trxamadrm-live-rx-a-tq1ly1pq` and
`.../decodium-trxamadrm-live-rx-b-d8mhw2yd`; the existing live RX harness
repeats these with `--bsr`. Passing these received objects to the guarded
historical TX parser reproduced the extended-success and compact-range
out-of-bounds results described above.

For a further selected-transmission check, the extended BSR recovered by the
historical receiver was placed in the isolated historical transmitter's
`picsrx/bsr.bin`. On two different 2,225-byte source JPEG fixtures, the
running transmitter confirmed acceptance of its three requested indices and
logged data segments 0, 1, 2, 3 and 14, skipping 4 through 13. The filename
was chosen to match the BSR transport ID. Notably, this test BSR advertises
64-byte segments while the transmitter uses 154-byte segments; its historical
reader does not validate that field. The process was stopped after observing
selection, and the repaired image was not reconstructed. Artifacts are
`/tmp/dtrxs-a-hqfcuaao` and `/tmp/dtrxs-b-5an7y_2s`;
`tests/sstv/tools/run_trxamadrm_selective_bsr_lab.py` repeats the check.
These are transport and selected-transmission observations, not proof of a
complete BSR repair exchange or of EasyPal interoperability.

The size discrepancy was then removed in a new A/B laboratory run. The
test-mode-only `DECODIUM_LAB_BSR_SEGMENT_SIZE=154` fixture creates five
154-byte segments; without it the original 64-byte fixture remains the
default. Two Decodium pairs transmitted the corresponding extended and compact
requests exactly (`.../decodium-hamdrm-two-app-a-3nojks66` and
`.../decodium-hamdrm-two-app-b-8_iwgvld`). Two further default-size controls
also passed (`.../decodium-hamdrm-two-app-a-_np523c2` and
`.../decodium-hamdrm-two-app-b-mhu8l9f2`).

The adapted historic receiver recovered both 154-byte BSR objects over
BlackHole exactly (`.../decodium-trxamadrm-live-rx-a-wgzl_2o0` and
`.../decodium-trxamadrm-live-rx-b-x88p4n50`). The recovered extended request
was then used with two distinct historical TX image fixtures, whose actual
body segment size is also 154 bytes. The TX accepted the request and logged
segments 0, 1, 2, 3 and 14 while skipping 4–13; artifacts are
`/tmp/dtrxs-a-hahppaw6` and `/tmp/dtrxs-b-o5aizia_`. The compact request was
not handed to the unsafe historical TX parser outside the guarded comparison.
These checks eliminate the size mismatch but still do not reconstruct an
incomplete image after repair or establish live EasyPal interoperability.

A later full repair test does reconstruct the missing image with the adapted
historical peer. On each of two distinct 2,225-byte images, Decodium discarded
body segment 1, sent its extended BSR over BlackHole, and the historical
receiver saved the exact request. The historical transmitter accepted that
request and sent a repair; Decodium reconstructed the original bytes exactly.
The historical sender's laboratory-only PortAudio adapter now waits for buffer
space rather than dropping generated samples; its codec remains unchanged.
The pinned transmitter SHA-256 for this run is
`0c3ddf320a11a8e47db62ea0eab79430af0a25bbe63a7fdc4b2f8c5dccc07ee0`.
The repository harness `tests/sstv/tools/run_trxamadrm_live_repair_lab.py`
reproduced A and B (`.../decodium-trxamadrm-repair-a-gwoiz0sx` and
`.../decodium-trxamadrm-repair-b-yc5bjzou`). This establishes the selected
extended-BSR exchange with the adapted historical software, not EasyPal,
compact BSR interoperability, or RF performance.

The archived transmitter's separate hybrid executable was compiled on macOS
with the same laboratory PortAudio and configuration adaptations; its SHA-256
is `46100c5bc9b76d511d0979fd9d7ba3e3fbfa926e82c3da8f23bfe4a730e744e0`.
In two live virtual-audio runs, the sandboxed executable transmitted distinct
synthetic `de_` references. Decodium identified each as a hybrid reference
and recovered its 186 or 210 body bytes exactly. The input bodies are generated
by the local test harness, so this proves selected independent historical MOT
framing and waveform reception, not independent encoding of the reference body.
Artifacts are `.../decodium-trxamadrm-live-a-8kzgfhyp` and
`.../decodium-trxamadrm-live-b-x3yq3z4k`; the existing live TX harness supports
repetition with `--hybrid`. Ordinary historical image transmission controls
also passed for A and B after the harness changes. These tests do not prove an
FTP transaction, live EasyPal interoperability, or RF behaviour.

The same independently framed historical hybrid transmitter was then paired
with a loopback-only pyftpdlib 2.1.0 server and the full Decodium receiver.
For two distinct image/reference pairs, Decodium decoded the audio reference,
authenticated with the synthetic credentials it contained, and downloaded
the corresponding 225-byte image exactly. The server logs confirm the two
separate directories and successful RETR operations. TX remained sandboxed
against IP access; the server and download were local to the Mac. Artifacts
are `.../decodium-trxamadrm-live-a-vv28y17m`,
`.../decodium-trxamadrm-live-b-a_fdtuca`,
`.../decodium-trx-hybrid-ftp-a-7128tg62` and
`.../decodium-trx-hybrid-ftp-b-um6sh_hv`.
`tests/sstv/tools/run_trxamadrm_hybrid_ftp_lab.py` repeats the test with the
hash-pinned transmitter and pyftpdlib. The harness generated the reference
body and placed the images on the FTP server: this is not an independent
historical upload, an EasyPal exchange, or an RF test.

For the reverse audio direction, Decodium transmitted two synthetic hybrid
references to the adapted historical RX. The independent receiver saved
`de_TEST1-1-image-a.jpg` and `de_TEST2-1-image-b.jpg` with the exact 186 and
210 reference bytes, and Decodium reported completed transmission. Artifacts
are `.../decodium-trxamadrm-live-rx-a-caw5xumb` and
`.../decodium-trxamadrm-live-rx-b-gfmk8wmf`. The RX harness supports `--hybrid`
to repeat these checks. The saved objects are references, not image downloads;
this does not prove that the historical receiver performs an FTP retrieval.

The reverse path was also exercised with a real Decodium hybrid upload to an
independent loopback pyftpdlib server followed by audio reference transmission
to the historical RX. In each A/B run, the server received the exact 225-byte
image and the RX saved the exact reference body with the same generated
filename as the uploaded object. The historical RX was sandboxed from IP
network access; its FTP download was not exercised. Artifacts are
`.../decodium-trxamadrm-live-rx-a-k3jd0zif`,
`.../decodium-trxamadrm-live-rx-b-mlqlpnzv`,
`.../decodium-trx-hybrid-upload-a-gniwllhk`, and
`.../decodium-trx-hybrid-upload-b-ac7kf8jg`. The test is repeatable via
`tests/sstv/tools/run_trxamadrm_hybrid_upload_lab.py` with the pinned RX and
pyftpdlib. It does not establish direct EasyPal interoperability or RF use.

The BSR selector now has a tooltip directing TRXAMADRM 3.6 operators to the
extended format. The QML test exercising both selections passed, and after
rebuilding, two new Decodium pairs transmitted the extended and compact A/B
requests exactly (`.../decodium-hamdrm-two-app-a-627u0jyf` and
`.../decodium-hamdrm-two-app-b-2imyeg5w`). Feeding those actual received
requests to the same guarded historical loop repeated the results above.
The A/B QML controller test also evaluates the tooltip text and confirms it
mentions both TRXAMADRM 3.6 and the extended choice. An attempted synthetic
hover did not reach the selector in the test window, so the tooltip has not
been visually checked by hovering in the application. The local build is not
a cross-platform packaging test.

The archived encoders were additionally compared on 28 deterministic
synthetic payloads at `k-7`, `k-1`, `k`, `k+1`, `2k-7`, `2k` and `2k+1`
bytes for each level's RS data size `k`. All containers matched and decoded
exactly. The `k-7` lower bound reflects this historical encoder's minimum
input, not a Decodium limit. These payloads are not valid image fixtures;
the A/B application exchanges below provide separate image-path evidence.
For each of those containers and the A/B JPEGs, ten unknown byte errors were
then injected into one RS codeword. Decodium reconstructed the full original
container and payload, while the separately compiled archived TRXAMADRM RS
decoder independently reconstructed the same original codeword. The archived
decoder and Decodium were called separately; this is a selected synthetic
corruption test, not a simulated radio channel or a claim about all errors.
As a separate functional control, two Decodium pairs sent RS4 images A/B
while the receiver's laboratory hook discarded one MOT body segment. Both
recovered all 2,725 bytes exactly; artifacts are
`.../decodium-hamdrm-two-app-a-weefygyt` and
`.../decodium-hamdrm-two-app-b-s79s17od`. This drops a segment in the receiver,
not ten bytes in an RS codeword, and does not simulate RF fading.
Current-build two-Decodium A/B virtual-audio transfers also recovered each
225-byte fixture exactly for RS1 (`.../decodium-hamdrm-two-app-a-nmgo1g_k`,
`.../decodium-hamdrm-two-app-b-4ridrv92`), RS3
(`.../decodium-hamdrm-two-app-a-rcvoin8c`,
`.../decodium-hamdrm-two-app-b-stevvnfv`) and RS4
(`.../decodium-hamdrm-two-app-a-62ptudfv`,
`.../decodium-hamdrm-two-app-b-x02q79jy`), complementing the RS2 run above.

QSSTV revision `31c323ccb848bade1dbab0a96890073f7b9455ea` independently
documents the server body in `src/utils/hybridcrypt.cpp`: the radio body
reversibly encodes FTP host, login, password and directory; the port is fixed
to 21. This is obfuscation, not encryption: both the radio body and ordinary
FTP expose credentials. It is unrelated to Decodium's E2EE sharing. QSSTV
recognises both `de_` and `.de` filename prefixes, then uses a separate FTP
transfer; a prefix alone does not establish that a damaged object is hybrid.

The new bounded `HamDrmHybridCodec` encodes and decodes printable ASCII
server fields and rejects malformed bodies, delimiter ambiguity and excessive
length. Its A/B synthetic fixtures were independently generated from QSSTV
and match Decodium byte for byte; QSSTV also decodes them back to the original
synthetic fields. The fixtures are `qsstv-hybrid-server-{a,b}.bin`, SHA-256
`4bac230e3a8a13573653cde349b83c3fe6be26a7debeaefa2ebdef1db95b453c`
and `d3f15dacc31900e1285f3e23060013a8d72e74ebd4182cbaea932c6170aa7831`.
They exercise both even-length and padded odd-length plaintext.
`tests/sstv/tools/run_qsstv_hybrid_oracle.sh` rebuilds the reference at that
revision, with a narrowly scoped allocation fix to its string-reversal helper
needed by Qt 6. No real account or network server is used.

The controller now identifies a complete `de_` object as a hybrid reference
only when its body passes the bounded server codec. It publishes a separate
inbox state and signal rather than emitting an image-completed signal; ordinary
valid images beginning `de_` still take the image path. The UI offers an
explicit download and destination picker, displaying the host and warning
that FTP credentials are unencrypted. QSSTV's alternative `.de` prefix is
also accepted as a single filename, with path separators and unrelated hidden
names still rejected. Partial objects may be written to the
existing partial store; the encoded server body is reversible and must be
treated as containing credentials.

In test mode only, two Decodium processes sent synthetic A/B server bodies
over BlackHole 2ch using the selected experimental adaptive receiver. Both
references arrived byte-exact (186 and 210 bytes) without an image-completed
event. The fixtures used only dummy `.invalid` hosts. Artifacts:
`.../decodium-hamdrm-two-app-a-501kk57a` and
`.../decodium-hamdrm-two-app-b-x96q3v05`. This demonstrates the radio
reference path between local applications, not FTP, actual server credentials,
live external interoperability or completed hybrid image transfer.

The Qt Network FTP downloader supports EPSV and PASV fallback, uses the control
peer address rather than the server-provided PASV IP, bounds file and response
sizes and verifies image structure and decoding before atomically saving the
user-selected destination. Local A/B tests cover both passive modes, rejected
login, unsafe paths, oversized and invalid image data. No public server was
contacted. Two additional test-mode Decodium application pairs transmitted a
synthetic `127.0.0.1` reference and retrieved a byte-identical image from a
local FTP fixture: `.../decodium-hamdrm-two-app-a-c3cb2a8u` and
`.../decodium-hamdrm-two-app-b-t7xjcmv9`. The test automatically starts the
download; production requires user action. This is a local receive-path test,
not evidence of FTP upload, external EasyPal/QSSTV use or real RF.

The transmitter now validates the source image, uploads it under a unique
`de_CALL-1-...` filename, waits for successful FTP completion and only then
starts HAMDRM waveform transmission of the encoded server reference. A failed
upload does not start radio TX. The credentials are entered for the operation
and the interface explicitly warns they are reversible over radio and sent via
unencrypted FTP. EPSV and PASV upload A/B and rejected-upload tests pass.
Two application pairs exercised this actual upload-to-radio-to-download path
against a local FTP fixture and verified the uploaded and received images
byte-for-byte: `.../decodium-hamdrm-two-app-a-qabmd7py` and
`.../decodium-hamdrm-two-app-b-dp9du8si`. RX used the experimental adaptive
setting, and the receiver's download was initiated automatically by the test
harness. These tests do not establish EasyPal/QSSTV application interoperability
or RF operation.

The hybrid FTP timeout now measures inactivity instead of imposing a fixed
30-second total transfer deadline. Actual control/data progress renews the
30-second timer. After an upload's local data socket closes, the client can
no longer observe the server draining buffered bytes; it waits at most five
minutes for the final server acknowledgement. A/B EPSV and PASV unit tests
exercise slow progressive downloads, slow uploads, stalled downloads and
missing upload acknowledgements with shortened test deadlines. The latter
two fail rather than report a completed transfer.

Two Decodium application pairs also uploaded A/B images to a local FTP
fixture, transmitted the hybrid reference and downloaded the original image
byte-for-byte. In both runs, the server deliberately delayed the upload's
final acknowledgement by about 35 seconds and sent the downloaded image in
chunks over another 35 seconds. RX used the operator-selected experimental
adaptive setting. Artifacts are
`.../decodium-hamdrm-two-app-a-_17g377f` and
`.../decodium-hamdrm-two-app-b-pfnowyei`; reproduce with
`DECODIUM_LAB_FTP_SLOW_INTERVAL_MS=5000`,
`DECODIUM_LAB_FTP_UPLOAD_ACK_DELAY_MS=35000`,
`DECODIUM_LAB_HAMDRM_DURATION_MS=120000` and
`DECODIUM_LAB_HAMDRM_USER_TIMING=1`, running
`tests/sstv/tools/run_hamdrm_two_app_lab.py a hybrid-upload` and then `b`.
The server is a test fixture, not an external EasyPal/QSSTV peer or an RF test.

EPSV responses are parsed with the repeated printable ASCII delimiter defined
by [RFC 2428 sections 2–3](https://www.rfc-editor.org/rfc/rfc2428), rather
than accepting only `|`. Local FTP A/B upload and download tests exercised
`!` and `#`, with additional parentheses, digit and `~` delimiter cases.
Mismatched delimiters, zero and out-of-range ports, and a space delimiter
were rejected before issuing RETR. Two Decodium pairs completed hybrid
upload, reference transmission and byte-exact download with the alternate
delimiters; A/B application artifacts are
`.../decodium-hamdrm-two-app-a-dhfnfyic` and
`.../decodium-hamdrm-two-app-b-o0gbr2p8`. A repeat additionally asserted
that EPSV was issued for both upload and download
(`.../decodium-hamdrm-two-app-a-2x15u1o8`). These are test-server responses,
not evidence of a live EasyPal or QSSTV hybrid exchange.

With the Mac legacy monitor active, the hybrid upload/reference/download A/B
application tests also observed the monitor stopped during native HAMDRM RX,
then restored with fresh PCM after RX cancellation. Artifacts are
`.../decodium-hamdrm-two-app-a-4nz69m4k` and
`.../decodium-hamdrm-two-app-b-z0oitn36`. This is local virtual-audio evidence,
not hardware RF coverage.

Hybrid UI selection disables ordinary TX and RS controls until hybrid is
deselected. `cancelAll()` now cancels an in-progress FTP upload before radio
TX starts and also cancels pending hybrid downloads; discarding the associated
reference cancels its download. A/B controller and QML tests cover these
states, and separate two-application A/B tests used a deliberately stalled
local FTP server to confirm cancelled uploads did not transmit and cancelled
downloads did not save a file. Artifacts:
`.../decodium-hamdrm-two-app-a-b4bb5yep`,
`.../decodium-hamdrm-two-app-b-fnw2avpp`,
`.../decodium-hamdrm-two-app-a-xvg4d7a6`, and
`.../decodium-hamdrm-two-app-b-botnecy5`.

The same upload/reference/download path was repeated A/B using the independent
`pyftpdlib` 2.1.0 server on loopback with a synthetic account and a real
`radio/HybridFiles1` directory. Both the FTP server's completed STOR/RETR logs
and Decodium's byte comparisons confirmed the 225-byte images. The runnable
test is `tests/sstv/tools/run_hamdrm_real_ftp_lab.py a|b` (requires pyftpdlib);
artifacts include `.../decodium-hamdrm-two-app-a-2a37x_a7` and
`.../decodium-hamdrm-two-app-b-_lsu0gtu`. This independently checks the FTP
server boundary, not an independent HAMDRM peer.

Separate A/B test-mode application pairs transmitted `.de` references and
downloaded the corresponding byte-identical images from a local FTP server:
`.../decodium-hamdrm-two-app-a-sb5yusju` and
`.../decodium-hamdrm-two-app-b-gd71xrp7`. These are synthetic references,
not captures or runtime tests from an independent transmitter.

For an independent receive-side waveform comparison, Decodium transmitted
two synthetic server references over BlackHole; the captured PCM was converted
to mono WAV and decoded by QSSTV-Experimental source revision
`a84584248d3307bfa081362031ff20f365337633`. Its receiver reached the hybrid
codec and recovered the exact A/B host, user, password and directory values.
The QSSTV build has the diagnostic patch
`tests/sstv/fixtures/qsstv-experimental-hybrid-oracle.patch`: it fixes the
reference's invalid empty-string indexing under Qt 6 and, when a laboratory
environment flag is set, compares synthetic values and returns before FTP.
`tests/sstv/tools/run_qsstv_hybrid_waveform_oracle.py` checks both recordings;
its expected QSSTV exit code is 1 because the diagnostic deliberately does
not fetch or save an image. Captures are
`.../decodium-hamdrm-tx-a-wwm178au/hybrid.wav` and
`.../decodium-hamdrm-tx-b-p6senom9/hybrid.wav`. This is file-mediated
Decodium-to-instrumented-QSSTV reference decoding, not independent FTP or
unmodified live QSSTV interoperability.

The comparison was extended in both directions with `pyftpdlib` on loopback.
For Decodium-to-QSSTV, the instrumented QSSTV receiver decoded two Decodium
recordings, then its own FTP client retrieved the referenced images; both
225-byte downloads matched exactly. The receiver patch
`tests/sstv/fixtures/qsstv-experimental-hybrid-ftp-oracle.patch` fixes the
Qt 6 reversal fault and adds a test-only loopback/port/path guard and bounded
wait. It uses QSSTV's FTP implementation, not Decodium's. Reproduce using
`tests/sstv/tools/run_qsstv_hybrid_ftp_oracle.py`; artifacts are
`.../decodium-qsstv-hybrid-ftp-ovlk5jhf` and the source recordings
`.../decodium-hamdrm-tx-a-wdnlio9y/hybrid.wav` and
`.../decodium-hamdrm-tx-b-i9yih75r/hybrid.wav`.

For the reverse direction, patched QSSTV uploaded two JPEG2000 objects to
the local server and generated hybrid WAVs. Decodium replayed those WAVs over
BlackHole with experimental adaptive RX, decoded the QSSTV references, and
downloaded byte-exact 4,992- and 4,997-byte images. QSSTV's laboratory patch
`tests/sstv/fixtures/qsstv-experimental-hybrid-tx-oracle.patch` sets synthetic
local server settings for headless TX, corrects string reversal and includes
the previously documented signed-PCM fix. The test harness automatically
starts Decodium's download. Run `tests/sstv/tools/run_qsstv_hybrid_tx_ftp_lab.py`
for A/B; artifacts are `.../decodium-qsstv-hybrid-tx-i1mdnlam` and
`.../decodium-qsstv-hybrid-tx-o4pk_s0_`. These file-mediated, instrumented
tests do not establish unmodified live QSSTV, EasyPal or RF interoperability.

The separately downloaded QSSTV-Experimental 10.0 macOS app was also tried
on the A hybrid recording: it exited with SIGBUS in
`hybridCrypt::reverseString` while processing the hybrid object. A known
ordinary DRM recording completed in that same explicitly isolated Qt plugin
environment. This observation does not establish the cause in every QSSTV
version or prove a complete hybrid exchange.

Decodable HAMDRM images now produce a bounded Gallery snapshot; hybrid
references do so only after a successful, validated download. Received records
are marked `received`, `digital`, `complete` and `source=hamdrm`; if storage
is unavailable the user receives a status instead of a false save claim.
A/B application tests inspected the actual SQLite record and stored PNG
pixels for ordinary images (`.../decodium-hamdrm-two-app-a-k8awt_1a`,
`.../decodium-hamdrm-two-app-b-lbsbohyk`) and downloaded hybrid images
(`.../decodium-hamdrm-two-app-a-5ytzb6aq`,
`.../decodium-hamdrm-two-app-b-uizpx9ta`). Two further A/B application tests
downloaded the same hybrid image twice and observed one Gallery entry
(`.../decodium-hamdrm-two-app-a-vbp_8jj6`,
`.../decodium-hamdrm-two-app-b-oo8uwddw`). The negative A/B controls inspected
the database and found no received HAMDRM image when only a hybrid reference
arrived (`.../decodium-hamdrm-two-app-a-zigs5jjv`,
`.../decodium-hamdrm-two-app-b-tm2y04bf`). Structurally valid but undecodable
images may still be saved manually, but are not added to Gallery. Their inbox
state is `complete-structural` and the UI explicitly distinguishes them from
decoded images. Controller and QML A/B tests exercise both the notice and
manual byte-preserving save; two additional application pairs transmitted
distinct synthetic JPEG containers that pass the structural boundary but
cannot be decoded, confirmed the structural-only state and found no received
HAMDRM Gallery record (`.../decodium-hamdrm-two-app-a-80uij69i` and
`.../decodium-hamdrm-two-app-b-f1e6xynr`). These negative fixtures were injected
only in test mode and are not examples of over-the-air source authenticity.

Images received while Gallery storage initialises are retained in a bounded
queue (at most eight images and 64 MiB) and saved once storage is ready.
With initialisation deliberately delayed by 20 seconds, two-application A/B
tests checked the stored PNG pixels and database records for ordinary images
(`.../decodium-hamdrm-two-app-a-4n4audjh`,
`.../decodium-hamdrm-two-app-b-du9oj5ia`) and hybrid downloads
(`.../decodium-hamdrm-two-app-a-h875j1ok`,
`.../decodium-hamdrm-two-app-b-fz1y7o3z`). Negative A/B controls forced
storage initialisation to fail after reception, verified the failure path and
retained the manually saved received bytes
(`.../decodium-hamdrm-two-app-a-5u9wg_3a`,
`.../decodium-hamdrm-two-app-b-5kru1gx1`). These are local test-mode fixtures,
not evidence of live radio reception.

As a separate regression, two Decodium application pairs used the selected
experimental adaptive mode to exchange 225-byte images A and B over
BlackHole, verified exact bytes and restored the macOS legacy monitor with
fresh PCM. Artifacts are `.../decodium-hamdrm-two-app-a-31ezaq_q` and
`.../decodium-hamdrm-two-app-b-wqfyy0nr`. These are ordinary native HAMDRM
transfers, not hybrid or external-peer tests.

## Profile-space status

### EasyPal RS2 receive experiment

In the opposite direction, EasyPal's own AutosaveTX RS2 containers from the
two earlier live transmissions decode in Decodium exactly to the A/B JP2
objects it received; Decodium also reproduces both containers byte-for-byte
from those complete objects. EasyPal appended 11 bytes after each original
JP2 before protection, and those bytes are retained in the received fixtures.
The regression `test_hamdrm_rs` checks these two independent containers and
crossed-image negatives. This evidence covers the saved EasyPal A/B output;
the earlier audio capture independently covers only B.

Two distinct Decodium JPEGs were transmitted as RS2 over local BlackHole audio
to EasyPal 07/OCT/2014 under Wine, using B/2.5 kHz/high/16-QAM/long. For A,
EasyPal's log reports OK and its saved image has 3.080 RGB mean absolute error
on a 0–255 scale relative to the source; the received JPEG is retained as
`received-rs2-a.jpg`. For B, Decodium reported TX complete but EasyPal's log
reports FAIL and no decoded JPEG appeared. No cause has been established, so
this is not a passing A/B RS2 interoperability result. The operator could not
confirm the display because the Wine window disappeared after being moved to
a second monitor; restarting the application restored the window.

Both RS2 files in EasyPal's Autosave are byte-identical to Decodium's RS2
encoding of the respective source JPEGs, including the failed B. The archived
TRXAMADRM 3.6 RS2 encoder independently generated those same bytes for A and
B, and its decoder recovered both source JPEGs byte-for-byte from EasyPal's
saved containers. This rules out an altered saved container as the explanation
for B's failure; it does not establish what EasyPal's UI or internal receive
state did at the time. The two captured containers are retained as
`received-rs2-a.rs2` and `received-rs2-b.rs2`.

Repeating B after restarting EasyPal again produced FAIL. A subsequent test
using the same B JPEG with appended padding was interrupted during reception
after EasyPal displayed an access violation in `rscoder.dll` under Wine.
The operator dismissed the dialog and reported the application usable again.
The interrupted padded test is not a successful or completed transfer, and
the error does not by itself distinguish a file-dependent issue, the EasyPal
DLL, or the Wine runtime.

As a controlled comparison, two Decodium applications exchanged these same
7,606-byte A and 3,900-byte B JPEGs with RS2 using B/2.5 kHz/high/16-QAM/long
over BlackHole 16ch. Both receivers saved byte-exact originals, and their
monitor was kept off to avoid sending the test into EasyPal. Artifacts are
`.../decodium-hamdrm-two-app-a-9x7bq1h3` and
`.../decodium-hamdrm-two-app-b-5e5p6_ya`. This isolates a successful local
Decodium path for both inputs; it does not explain the EasyPal B failure.

The same A/B RS2 waveforms were independently decoded by the pinned official
QSSTV-Experimental 10.0 batch receiver. A was byte-exact. B began with the
entire 3,900-byte original but had 125 additional zero bytes; therefore B is
not an exact interoperability pass. The RS2 padding header byte is 74 for A
and 132 for B. In a locally available QSSTV source tree, the padding byte is
converted from `char` to `int` without an unsigned conversion; on this Mac,
`char` is signed, so 132 becomes -124. This is consistent with the observed
extra bytes, but the source-to-binary match was not established for this
specific receiver and it does not establish the cause of EasyPal's FAIL.
The exact 16-bit PCM recordings are preserved in lossless FLAC and the pinned
receiver observation is repeatable with
`python3 tests/sstv/tools/check_qsstv_rs2_padding_observation.py`.
Two further controls changed only the end-of-file padding on B before RS2
encoding. Adding five zero bytes produced RS2 padding field 127 and QSSTV
returned the 3,905-byte input exactly. Adding four produced field 128 and
QSSTV returned the complete 3,904-byte input plus 121 zeros. The same
recording with field 127 initially failed acquisition at one start offset;
other offsets decoded exactly, and the regression allows up to three attempts
only when QSSTV returns no image. This controlled boundary strengthens the
signed-byte hypothesis for QSSTV, not for EasyPal. The source JPEGs were
deliberately changed for the test; this is not a production workaround.

For a direct A/B test of that hypothesis, two local receivers were built with
the same compiler and source tree at base commit
`a84584248d3307bfa081362031ff20f365337633`, including the pre-existing local
hybrid laboratory changes. They differed only in interpreting the RS2 padding
byte as unsigned before subtraction. With the same four saved recordings,
the baseline produced exact A and field-127 results, B plus 125 zeros, and
field-128 plus 121 zeros. The patched receiver returned all four inputs
byte-exactly. All eight decodes succeeded on the first acquisition attempt.
The minimal experimental patch is `fixtures/qsstv-rs2-unsigned-padding.patch`
under `tests/sstv`, and `check_qsstv_rs2_unsigned_fix.py` repeats the
comparison when given both executable paths. This establishes the cause in
these local QSSTV builds, not in EasyPal, and does not modify Decodium's
standard RS2 payload or claim that the external project has applied the fix.
The Decodium TX page displays a scoped QSSTV trailing-zero notice when RS1–RS4
is selected; it does not block TX or change the file. The QML regression checks
all levels, no protection and hybrid mode. After rebuilding, two Decodium
application pairs exchanged separate 225-byte RS2 images exactly over
BlackHole 16ch with the monitor off (`.../decodium-hamdrm-two-app-a-5l867cfi`,
`.../decodium-hamdrm-two-app-b-x5u8yku8`). These are local audio controls,
not evidence that EasyPal's independent RX issue is resolved.

Using a synthetic waveform exporter and the same two local QSSTV builds, the
127/128 padding boundary was independently exercised for RS1, RS3 and RS4.
For each level the baseline returned the field-127 input exactly and the
field-128 input with 121 extra zeros, whereas the patched build returned both
inputs exactly. All twelve decodes succeeded on their first acquisition
attempt. These are file-mediated waveform checks, not live external audio or
EasyPal evidence. They can be reproduced with
`check_qsstv_rs_levels_boundary.py` after building
`export_hamdrm_rs_boundary_wave` and providing the two receiver paths.
After extending the notice, further two-application controls transmitted
image A with RS1 and image B with RS4 over BlackHole 16ch; both 225-byte
images were recovered byte-exactly (`.../decodium-hamdrm-two-app-a-qpp112jh`
and `.../decodium-hamdrm-two-app-b-jza4ay6m`).

The TX page also offers an explicitly selected, default-off QSSTV RS
compatibility option for JPEG. When the original RS padding field would be at
least 128, it appends enough zero bytes after the JPEG data to reduce the field
to 127. The pixels remain the same, but the transmitted file is not
byte-identical to the selected file; the UI identifies the added bytes. The
default path leaves every byte unchanged. Non-JPEG input is rejected when the
option is selected. Controller A/B tests cover all four RS levels at fields
127 and 128, with and without the option and with pixel comparison. Two full
Decodium app pairs transmitted distinct RS2 JPEGs whose field was 128 and
received the deliberately one-byte-longer transmitted files exactly
(`.../decodium-hamdrm-two-app-a-14uh7jbl`,
`.../decodium-hamdrm-two-app-b-y7kd8fnc`). The pinned unmodified
QSSTV-Experimental 10.0 batch receiver also recovered those A/B transmitted
bytes exactly from actual BlackHole 16ch recordings; their pixels match the
original images. `check_qsstv_rs_compat_ab.py` replays the saved FLAC files.
The same end-to-end control was then run for A and B at RS1, RS3 and RS4,
with the field again set to 128 before the opted-in one-byte adjustment. The
pinned official QSSTV batch receiver reconstructed all six recordings exactly
and their decoded pixels matched the original JPEGs. Combined with RS2 this
provides A/B evidence at all four levels for the selected B/2.5/high/16/long
profile, not for all profiles, EasyPal repair, native Windows or RF. The
same replay script covers all eight recordings.
The complementary no-change control used two actual Decodium application
pairs with the option enabled and two JPEGs whose RS2 padding field was already
127. Both were received byte-for-byte unchanged over BlackHole 16ch
(`.../decodium-hamdrm-two-app-a-lo2qiind`,
`.../decodium-hamdrm-two-app-b-e_w5jfni`).
The QML regression also invokes the actual Start TX button with the checkbox
both off and on for two JPEGs. The controller's captured RS objects decode to
the exact unchanged source in the default case and to the source plus the
single expected zero when compatibility is selected. This exercises the
selection-to-controller wiring in addition to checking checkbox visibility.
The new compatibility labels and error messages have active Italian catalog
entries. The QML regression loads the compiled Italian catalog and verifies
the notice against English and Italian controls, including the explanation
that selected and received file bytes may differ.
For a selective-repair interaction, two full Decodium application pairs used
the opted-in RS2 path with A/B JPEGs set to padding field 128. The laboratory
dropped MOT body segment 1 after demodulation during initial RX; each receiver
then sent a BSR over BlackHole 16ch and obtained the repair over the same
virtual audio. Both reconstructed the exact 2,753-byte transmitted JPEG,
including the one intentionally appended byte. A used the extended BSR form
and B used the compact form. Artifacts are
`.../decodium-hamdrm-repair-a-yqsid68g` and
`.../decodium-hamdrm-repair-b-ftr24x7k`; an unprotected default-path control
also passed (`.../decodium-hamdrm-repair-a-lr_mz58i`). The test-only RS
segment drop is guarded by Qt test mode and is not an audio-loss or external
peer test. Repeat with `DECODIUM_LAB_HAMDRM_RS_LEVEL=2`,
`DECODIUM_LAB_HAMDRM_QSSTV_COMPAT=1`,
`DECODIUM_LAB_REPAIR_DEVICE='BlackHole 16ch'`, and
`run_hamdrm_repair_two_app_lab.py a live` or `b live`.

An additional A/B test injected loss before demodulation: previously recorded
Decodium compatibility-mode A/B waveforms were replayed to a fresh Decodium
receiver on BlackHole 16ch with a ten-second interval muted, containing
467,987 and 467,945 active 48 kHz frames respectively. The receiver reported
four missing segments in each case, transmitted a BSR over virtual audio, and
the live sender transmitted the requested repair. The resulting 7,745-byte A
and 3,905-byte B files matched the exact transmitted compatibility payloads.
The sender and receiver were checked for the intended virtual device. Evidence
is `.../decodium-rs-compat-audio-loss-a-8y8o_wn9` and
`.../decodium-rs-compat-audio-loss-b-exqsc1xa`. The repeatable laboratory is
`run_hamdrm_rs_compat_audio_loss_lab.py a|b` and needs numpy, PortAudio, ffmpeg,
and BlackHole 16ch. The initial waveform is a recording, while the BSR and
repair are live between the applications. It is not an RF or EasyPal test.

One later replay trial requested BlackHole 16ch but the RX log reported that
the requested device was not found and the ready marker identified the
fallback BlackHole 2ch. That run was stopped and was not counted. Two fresh,
isolated startup controls subsequently opened the requested BlackHole 16ch
without fallback (`.../decodium-audio-device-start-ab-_pcsxy5z/a` and `/b`).
The device was visible to PortAudio during investigation, but the cause of
the intermittent mismatch was not determined. The replay harness checks the
actual sender and receiver devices before proceeding; the healthy A/B controls
do not prove that the intermittent fallback cannot recur.

Maximum-adjustment controls used A/B JPEGs positioned at the largest padding
field for each RS level. The explicit compatibility transformation added
97, 65, 33 or 1 zeros for RS1–RS4 respectively. In sixteen file-mediated
synthetic waveform decodes, pinned QSSTV 10.0 reconstructed each compatible
payload byte-exactly with the original pixels; the corresponding standard
controls showed its 217, 185, 153 or 121 additional zeros. This comparison is
repeatable with `check_qsstv_rs_compat_max_padding.py`. It generates the
adjusted input in the harness; the separate controller and full-application
tests above verify the production option.

### EasyPal 2014 partial-image repair, local audio

For two different JPEGs, EasyPal created extended BSR files after deliberate
audio loss in Decodium's B/2.5 kHz/high/16-QAM/long-interleaver transmission.
Their exact bytes were then encoded as HAMDRM audio by a Decodium laboratory
helper and played into the sending Decodium instance. It accepted each BSR
and transmitted the requested repair through BlackHole to EasyPal. EasyPal
saved both repaired images after the repair, with pixel mean absolute errors
of 3.29 and 1.52 levels out of 255 versus their respective inputs. For A,
the observed EasyPal display progressed from five of eight segments initially
to eight of eight after repair. For B, the operator likewise confirmed zero
segments remaining after the repair.
EasyPal's BSR list selection was not recognised in this Wine environment and
its Fast BSR attempt yielded no measurable audio. Consequently these tests do
not establish EasyPal-originated BSR audio transmission, RF operation, or
general interoperability across profiles. The saved BSR payloads are retained
in `tests/sstv/fixtures/easypal-2014-live/`.

An additional interrupted image C generated a fresh extended BSR in EasyPal.
Its list did not recognise even a double-click. Fast BSR returned to RX after
an instant and a simultaneous BlackHole 2ch capture recorded peak amplitude
0.0; Decodium logged no incoming BSR. This does not identify the cause or test
Decodium's behaviour for an EasyPal-transmitted BSR signal.
An isolated clone of the Wine prefix was also launched using a Wine virtual
desktop. The operator reported the BSR entry still was not recognised as
selected there. Attempting to load the saved request as `bsr.bin` through
EasyPal's LoadAny produced an `illegal filename` message upon confirming the
selected file, as reported by the operator. Neither observation
establishes the underlying cause.

A targeted search for an independently attributable EasyPal BSR waveform did
not produce a usable request recording. The `BSR.wav` mentioned in an
[older EasyPal guide](https://manualzz.com/doc/4700768/vk4aes-easypal-digital-sstv-anleitung)
is described as an optional waterfall-text sound played before the request;
it is not evidence of the encoded BSR itself. This search does not establish
that no suitable independent recording exists.

The registry contains A/B/amateur-E x 2.3/2.5 kHz x high/normal protection x
4/16/64-QAM x long/short interleaving, for 72 stable named tuples. Registry
presence alone is not an RX/TX capability claim. The native adapters execute
the registered subset through Decodium's shared audio and TX authority, but no
tuple is marked fully interoperable because broad profile coverage and external RF exchange remain unverified. Several QSSTV-Experimental profiles have limited file-mediated A/B evidence; one official transmitter profile was recovered from timed-out recordings as described in the oracle.

## Build status

On macOS ARM64, the enabled configuration found OpenJPEG 2.5.4, the focused
HAMDRM tests passed, and the complete `decodium_qml` executable compiled and
linked with the Bridge-owned controller, native waveform adapters and QML page.
The adapter suite exercised bounded RX self-roundtrip and TX through the shared
coordinator/PTT state machine. An analog-only
configuration built and tested without
discovering or linking OpenJPEG. Negative configuration tests confirmed that
HAMDRM cannot be enabled without SSTV and that missing OpenJPEG produces an
actionable fatal error. Windows, Linux and release-bundle dependency inspection
remain unverified and cannot be inferred from the macOS result.
