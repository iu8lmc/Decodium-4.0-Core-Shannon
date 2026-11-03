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
| EasyPal interoperability | Partial recorded-signal evidence | Two public recordings labelled EasyPal were decoded by QSSTV. Ordinary Decodium RX recovered the second object's unpadded bytes; the explicitly selected experimental adaptive path recovered both, with the first matching QSSTV byte-for-byte and the second matching its prefix before 199 zero bytes. Source versions and transmission paths are unknown. See the oracle. | Ordinary reception of the first recording, independently verified EasyPal versions, live application exchange, compact BSR behavior and RF remain unverified; do not inspect proprietary code. |
| EasyPal-style hybrid radio/file-server mode | Partial: `de_` FTP upload, reference transmission and manual download; `.de` receive | [TRXAMADRM author's documentation](https://pa0mbo.nl/ties/public_html/hamradio/txamadrm/txamanew.pdf) describes a radio header referencing a file uploaded separately to a server; instrumented QSSTV exchanged A/B hybrid messages with Decodium in both directions via WAV and local FTP. | Decodium's separate HTTPS remote sharing and E2EE are not evidence of this on-air format. Unmodified live QSSTV, EasyPal and RF tests remain open. |
| KG-STV | Not implemented | None | HAMDRM is not KG-STV. An authoritative public specification and independent vectors have not been established. |

An archived Windows installer named `EasyPal-07-OCT-2014-Setup.exe` was
downloaded from the [QSL.net public archive](https://qsl.net/lw7dfm/Software/SSTV/)
for later independent testing. Its SHA-256 is
`7190ef73f643d60afa5f064fa238b6055e48f5cd18fbc08cd861ac55cb8d4763`;
`file` identifies a 32-bit Windows GUI executable. The archive is a third-party
mirror, not proof of publisher authenticity. It has not been installed or
executed, and no EasyPal compatibility result follows from its availability.

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
