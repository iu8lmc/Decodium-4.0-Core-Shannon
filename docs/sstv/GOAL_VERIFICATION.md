# SSTV goal verification

This record distinguishes the requested functions from the scope of the
available evidence. It is not a claim of universal interoperability.

| Requirement | Current evidence | Remaining limitation |
| --- | --- | --- |
| Radiofax / HFFAX / WEFAX | Configurable experimental Radiofax RX/TX; selected Decodium A/B and independent comparisons in [RADIOFAX_PROTOCOL.md](RADIOFAX_PROTOCOL.md). Explicit FAX480 profiles are separate. | Generic registry entries do not establish every profile. The user accepted the documented QSSTV FAX480 comparison limit. |
| AVT-QRM variants | AVT24/90/94 QRM and Narrow-QRM RX/TX; selected A/B Decodium and independent codec comparisons in [AVT_PROTOCOL.md](AVT_PROTOCOL.md). | File-mediated comparisons and virtual audio do not prove all signal conditions or RF. The AVT24 last-pixel timing candidate is not enabled in normal use. |
| E2EE sharing | Encrypted A/B application transfers, including signed and unsigned envelopes, using two processes and isolated local HTTPS/Keychains; see [E2EE_IMPLEMENTATION.md](E2EE_IMPLEMENTATION.md). | The local fixture does not establish public-provider operation or an independent security audit. |
| HAMDRM interoperability | Selected A/B QSSTV and EasyPal evidence in [HAMDRM_COMPATIBILITY_MATRIX.md](HAMDRM_COMPATIBILITY_MATRIX.md). | EasyPal was accepted as partial by the user; BSR-originated audio, Hybrid and other cases remain explicitly unverified. Full interoperability is not claimed. |

After the wide-PPI packetizer change, the linked HAMDRM test executables were
rebuilt and fourteen focused CTests passed on macOS ARM64: core, partial
store, JPEG2000, controller, hybrid FTP, digital QML, PHY, channel, CRC
stress, capture analyzer, upstream BSR timing, RS, fuzz smoke and lab CLI.
The waveform and adapter suites, two QSSTV-source fixture regressions, and
two-Decodium plus QSSTV A/B image transfers are recorded separately in the
[QSSTV oracle](HAMDRM_QSSTV_ORACLE.md). This is local test evidence, not a
claim about all platforms, providers, profiles or RF channels.

Source inspection also confirms the application exposes Radiofax and FAX480
receive controls and a Radiofax transmit action, HAMDRM receive/transmit and
profile controls, and encrypted-sharing upload and receive-policy controls.
These are conditional on the relevant runtime capabilities and configuration.
Inspection of QML and bridge wiring alone does not prove interactive GUI
operation; the process-pair and QML tests below have their own stated scope.

On the current macOS ARM64 build, ten focused CTests passed: AVT RX session,
Radiofax, receive QML, sharing queue, HAMDRM core, partial store, controller,
digital QML and RS, and sharing AEAD. The two-process signed and unsigned E2EE
A/B transfers also passed after the test-only monitor-startup change. The
selected CTests exercise their stated components; they are not independent
software, radio, Windows or Linux proof.

The four full-application signed E2EE corrupted-pin A/B regressions also
passed after that change. They inject corrupt recipient pins at the sender or
corrupt sender pins at the receiver in isolated test Keychains, check that
the bad records are detected and not overwritten, remove only those test
records, and then verify normal A/B transfer after re-pinning. See the E2EE
implementation record for the precise fault-injection limits. These are
not a security audit or a test with a public provider.

After the test-only monitor-startup correction, fresh centered Radiofax
IOC 576 / 240 LPM A/B transfers between two Decodium processes completed
with full coverage, no reported RX discontinuities, and grayscale MAE
0.172/0.106 against distinct prepared references. The TX logs confirmed
the test-only auto-monitor suppression. Artifacts are
`.../decodium-radiofax-centered-a-pvhwpn_x` and `...-b-x14ndmel`. These
selected virtual-audio runs do not broaden the independent Radiofax or
FAX480 interoperability claims.

After the test-only monitor-startup correction, fresh HAMDRM A/B transfers
between two Decodium processes over BlackHole each recovered the corresponding
225-byte JPEG exactly. The laboratory also observed active legacy FT8 RX
before HAMDRM, its suspension while HAMDRM owned capture, and resumed legacy
monitoring with new PCM after the HAMDRM session ended. Artifacts are
`.../decodium-hamdrm-two-app-a-3uzuhyol` and `...-b-7b12b80j`. This selected
macOS virtual-audio regression is not an independent-client or RF test.

Fresh selective-repair A/B tests also passed between two Decodium processes
over BlackHole 16ch: each intentionally dropped a decoded segment, sent the
BSR and repair over the virtual audio device, and reconstructed the 2725-byte
test payload exactly. Artifacts are `.../decodium-hamdrm-repair-a-77ghwlyb`
and `.../decodium-hamdrm-repair-b-r1huhc2g`. The loss was injected after
demodulation, not as RF or missing audio; these tests do not extend the
accepted EasyPal scope.

The live local repair sequence was repeated on the current build for both
BSR dialects. Two Decodium processes sent each initial image over BlackHole,
the lab discarded one decoded body segment, and the processes exchanged the
BSR and selective repair over virtual audio. Both 2,725-byte images were
reconstructed exactly (`.../decodium-hamdrm-repair-a-sumq6bc1` and
`.../decodium-hamdrm-repair-b-sa73red4`). `test_hamdrm_core` and the pinned
QSSTV upstream BSR waveform A/B timing regression also passed. The injected
loss follows demodulation; the independent QSSTV test replays recordings and
does not turn these into a live independent-peer or RF repair test.

Both retained instrumented-upstream QSSTV BSR waveform fixtures were also
replayed over BlackHole to the current full Decodium receiver. The 39-byte
extended and 24-byte compact requests were identified as BSR and matched
their respective reference files exactly, with clean RX audio diagnostics.
Artifacts are `.../decodium-qsstv-drm-app-60u5biiw` and
`.../decodium-qsstv-drm-app-uqcux9dy`; converted inputs and logs are in
`.../decodium-qsstv-bsr-replay-v_yu7q0x`. This repeats selected recorded
requests, not an unmodified live QSSTV repair exchange or RF test.

A further independent QSSTV RX comparison covered HAMDRM
A/2.5/normal/16-QAM/long: two Decodium application pairs and the pinned
QSSTV-Experimental 10.0 batch receiver each recovered both 225-byte A/B
images exactly. The reverse A/B direction also passed: WAVs from a locally
instrumented QSSTV transmitter were independently decoded by the unmodified
QSSTV receiver and reproduced byte-for-byte by full Decodium receivers over
BlackHole. The [QSSTV oracle record](HAMDRM_QSSTV_ORACLE.md) retains the
separate capture, packet diagnostics and transmitter modifications. This is
selected file-mediated evidence, not RF or coverage of all HAMDRM profiles.

The A/2.5/normal/64-QAM/short profile also passed A/B between two Decodium
applications and in both file-mediated QSSTV directions. The independent
receiver decoded two further Decodium A/B signals with a synthetic echo,
noise and tone, while the Decodium receiver also reconstructed both. The
QSSTV transmitter used for the reverse direction was locally instrumented;
the independent QSSTV receiver was the pinned unmodified release. Automated
offline A/B regressions now retain compressed QSSTV-source PCM and distinct
reference objects for this profile and for the A/2.3 phase-impaired case;
both tests check wrong-reference rejection. See the
[oracle record](HAMDRM_QSSTV_ORACLE.md) and fixture READMEs for their exact
conditions and limits.

Whole-store rollback remains a limitation of the E2EE key and pin successor
chains: authenticated local storage alone does not provide a monotonic trust
anchor. Selected application tests do not establish behaviour of every
sharing provider or constitute an independent security audit. These are
separate from the accepted Radiofax and EasyPal limitations above.
Following the HAMDRM PPI rebuild, the unsigned and signed two-process
encrypted A/B transfer CTests were rerun on the current macOS build and both
passed using the isolated local HTTPS fixture and temporary Keychains. This
does not change the provider, rollback or security-audit limitations.

After the wide-PPI change, two full Decodium pairs again exchanged A/B
Hybrid references over BlackHole and uploaded and downloaded the respective
225-byte JPEGs exactly using an independently implemented local pyftpdlib
server. App artifacts are `.../decodium-hamdrm-two-app-a-ocngojxa` and
`...-b-pmp20s4b`; local FTP roots are `.../decodium-real-hybrid-ftp-c_m8axga`
and `...-w8qay65d`. This is a local server and virtual audio, not an
unmodified live QSSTV Hybrid exchange, a public FTP server or RF proof.

To reduce test storage, the generated `capture.wav` conversions in twelve
recent `decodium-qsstv-drm-decode-*` result directories were removed after
checking that the original `capture.f32` or `relay.f32`, QSSTV log and
decoded JPEG remained. The WAV is reproducible from the original float stereo
48 kHz capture with `run_qsstv_exp_drm_decode_lab.py`; the source captures,
decoded images and logs remain evidence.
The same helper now deletes its derived WAV automatically only after a
successful comparison, and records the source/reference paths and SHA-256
digests, conversion interval and derived WAV digest in `retention.json`.
`--keep-wav` retains the WAV; failed comparisons retain it for diagnosis.
The two large-image A/B captures verified default and explicit retention,
and crossed-reference A/B failures retained their WAVs and logs. This is a
laboratory-storage change and does not affect normal application data.

For fourteen specifically identified closed PPI laboratory profiles, Qt
cache files and thirteen copies each of three byte-identical FT2/grid
resources were removed (131,088,753 bytes total). One identical resource set
was retained, and the corresponding SSTV data, application logs and received
images were preserved. These were uniquely named test profiles associated
with the A/B artifacts above, not the operator's ordinary profile.

After the wide-PPI change, the Decodium-to-QSSTV live repair A/B tests were
repeated using isolated upstream QSSTV commit
`8c27d6d169d8c6c197eb47c2089870e39bc06a02` and the instrumented
`qsstv-upstream-bidirectional-lab.patch`. The rebuilt executable, after fixing
its temporary Qwt library path, had SHA-256
`5635f093a5917602dcd6dcbcf51bb597ea8243ac92c076e63823a7771842068d`.
In each run, the relay deliberately muted three seconds of the initial audio;
QSSTV produced and transmitted a BSR, and QSSTV and Decodium both recovered
the original 2,725-byte image exactly. A used extended BSR and B used compact
BSR; the PulseAudio output bridge reported zero underflows in both. The active
muted sample counts were 139,486 and 139,444 respectively. Application
artifacts are `.../decodium-hamdrm-repair-a-3uys09cx` and
`.../decodium-hamdrm-repair-b-y1tilf6n`; relay artifacts are
`.../decodium-hamdrm-repair-audio-loss-1wq99m4l` and
`.../decodium-hamdrm-repair-audio-loss-t3o3dseb`. These are virtual-audio
tests of selected profiles, not RF or complete interoperability coverage.
The private PulseAudio server was stopped after the tests.

The reverse live repair sequence was also rerun after that change using the
same rebuilt instrumented upstream QSSTV. QSSTV transmitted distinct A/B
images, the Decodium receiver intentionally dropped an initial decoded MOT
body segment, and Decodium sent an extended A or compact B BSR over virtual
audio. QSSTV received the exact generated BSR and sent the requested repair;
Decodium recovered the 4,032-byte A and 1,765-byte B JPEG 2000 objects
byte-for-byte. The QSSTV output bridge reported zero underflows and active
Decodium receive diagnostics reported no dropped chunks or discontinuities.
Artifacts are `.../decodium-qsstv-upstream-live-a-r9wflz_9` and
`.../decodium-qsstv-upstream-live-b-5pd52p6h`. The loss was injected after
demodulation, and the QSSTV control hooks are laboratory instrumentation;
these results do not establish RF or all-profile interoperability. The
private PulseAudio server was stopped following both runs.

On the current macOS build, the AVT core and AVT receive-session CTests
passed again, as did the signed E2EE expiry-pending-restart A/B CTests. The
sharing cases use two application processes, isolated Keychains and a local
HTTPS fixture; they inject lookup denial and a test-only clock advance. They
verify the pending state, preserved staged data, expiry and managed cleanup,
including a separate sentinel. These are local functional and regression
checks, not a public-provider or independent security audit.

The current build also passed the live Hybrid A/B paths against adapted,
hash-pinned TRXAMADRM 3.6 components and a loopback pyftpdlib server. In the
incoming direction the independent transmitter framed synthetic reference
bodies, and Decodium recovered those bodies and downloaded the corresponding
225-byte images exactly (`.../decodium-trxamadrm-live-a-tqpnc2gj`,
`.../decodium-trxamadrm-live-b-wjsryll8`). In the outgoing direction
Decodium uploaded each exact image and transmitted its reference to the
independent receiver (`.../decodium-trxamadrm-live-rx-a-v9nxn81w`,
`.../decodium-trxamadrm-live-rx-b-g130ynhc`); each received reference used
the name of the uploaded object. The historical RX did not download the
image, and the incoming reference bodies and FTP files were prepared by the
laboratory. These are selected virtual-audio tests, not an EasyPal, public
FTP, unmodified historical-runtime or RF exchange.

The current registry documentation gate, Radiofax receive QML and sharing
QML tests also passed. Source inspection confirms that Radiofax and FAX480
use separate application controls rather than the generic catalogue rows,
which remain marked catalogued or blocked; those rows must not be represented
as general profile coverage. Encrypted sharing is available only with a
provider advertising encrypted-envelope support and an available secure
key-storage backend. The real HTTPS fixture A/B results establish that path
locally; they do not establish that any particular public provider implements
the required capability.

The Radiofax RX and TX explanations now identify the existing controls as
covering selected HFFAX/WEFAX profiles; this is guidance to the separately
configured experimental path, not a change to the generic catalogue or a
claim covering every profile. The receive and Studio QML tests passed, and
the application build synchronized the changed QML. Existing A/B audio and
independent-decoder evidence remains as stated in the Radiofax record.

After the QML guidance change and app build synchronization, WMO Radiofax
A/B exchanges were repeated between two full Decodium processes over
BlackHole. Both used 240 LPM, the 4.5% dead sector and timed WMO phasing;
A used IOC 576 and B IOC 288. Both completed 128 x 120 images with full
coverage, zero reported discontinuities and MAE 0.309 / 0.248 (p95 zero).
Artifacts are `.../decodium-radiofax-centered-a-i7xzsr64` and
`.../decodium-radiofax-centered-b-ypwii3y1`. These are virtual-audio
regression results, not new independent-decoder or RF tests.

The four uniquely named test profiles belonging to those two Radiofax runs
contained identical copies of three unrelated, regenerable grid/FT2 resource
files. Their SHA-256 values were compared before removing three of the four
copies of each resource (20,245,143 bytes); one verified set remains. Both
Radiofax runs retain their reference and received images and RX/TX logs.
