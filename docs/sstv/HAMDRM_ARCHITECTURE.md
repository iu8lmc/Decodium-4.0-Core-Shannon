# Native HAMDRM architecture

## Hybrid save-selection identity

The same token is now retained through the FTP operation and checked again
after the completion notification and before validation or saving. A handler
of that notification can synchronously remove the original inbox record and
create another with the same transport ID; the downloaded bytes must not then
be attached to the replacement. A/B controller tests force exactly that
sequence at completion and verify failure, no saved file, no Gallery event and
an unchanged replacement. These tests simulate the reentrant sequence; they
do not claim it was observed in an ordinary user session.

After rebuilding, normal A/B hybrid transfers between two Decodium pairs over
BlackHole and an independent local pyftpdlib FTP server still downloaded the
corresponding 225-byte image exactly. Seven RX snapshots per run reported
zero discontinuities, dropped/rejected chunks, queue overflows and processing
failures. Artifacts are `.../decodium-hamdrm-two-app-a-dv7j2ukn` and
`.../decodium-hamdrm-two-app-b-7f2ys5ky`. The complete hybrid FTP suite passed.

The Digital page now snapshots an ephemeral token for the selected hybrid
inbox record when opening its save dialog. The guarded download checks both
the transport ID and token before initiating FTP. If the original record was
discarded and the ID reused while the dialog was open, the stale selection is
rejected rather than starting a download for the replacement. The token is
generated per in-memory inbox record and is not an authentication credential.
The existing direct controller method remains available to callers that
already hold a current record; the page and application laboratory now use
the guarded method.

Controller A/B tests discard and recreate a hybrid record with the same ID,
verify the old and empty tokens cannot start a connection, then download the
image successfully with the new token. QML A/B tests verify that the page
snapshots the token and retains it after replacement; these invoke the page's
preparation function directly rather than operating a native file dialog.
The first attempt to locate an offscreen list delegate timed out because the
delegate was not created while outside the viewport; the revised QML suite
passes, as does the hybrid FTP suite.

Two full Decodium pairs then transmitted A/B hybrid references over BlackHole
and used the guarded laboratory path to upload and download each exact
225-byte image through a local pyftpdlib server. Seven diagnostic snapshots per
run had zero discontinuities, dropped/rejected chunks, queue overflows and
processing failures. Artifacts are
`.../decodium-hamdrm-two-app-a-76t88dqs` and
`.../decodium-hamdrm-two-app-b-zf94cvlt`. This does not constitute a manual
native-file-dialog test, a live EasyPal/QSSTV exchange or RF verification.

## Hybrid FTP control reply bounds

The hybrid FTP client now caps Qt's control-socket read buffer at 16,385
bytes and checks the available bytes against the remaining 16,384-byte reply
budget before reading. One extra byte allows the client to reject an oversized
reply without first copying the entire reply into its parser buffer. The limit
applies to the Qt control socket and parser buffer, not to all operating-system
network buffers. A/B component controls sent an unterminated greeting of
16,385 bytes and a 2 MiB greeting; both were rejected before USER and without
returning a payload. The full `test_hamdrm_hybrid_ftp` suite passed, including
normal transfers and timeout cases. The older implementation also rejected
oversized replies after reading them; these tests exercise rejection behavior,
not a measured before/after peak-memory comparison.

After rebuilding, two local Decodium application pairs sent hybrid references
over BlackHole and used an independently implemented pyftpdlib 2.1.0 loopback
FTP server to upload and retrieve the respective 225-byte image. Both images
matched exactly; seven RX snapshots per run reported zero discontinuities,
dropped/rejected chunks, queue overflows and processing failures. Application
artifacts are `.../decodium-hamdrm-two-app-a-kgbg238a` and
`.../decodium-hamdrm-two-app-b-uroz24ce`; isolated FTP roots are
`.../decodium-real-hybrid-ftp-w9kerskz` and
`.../decodium-real-hybrid-ftp-0g081roh`. These results cover local FTP and
Decodium virtual audio, not unmodified QSSTV/EasyPal or RF interoperability.

Status: active implementation record, 2026-08-24.

HAMDRM is a digital image-transfer subsystem inside the Decodium4 process. It
is deliberately separate from the analog SSTV decoder and encoder, but it must
reuse Decodium's existing audio fan-out, sound output, CAT/PTT ownership,
settings, storage, diagnostics and QML workspace. It never launches QSSTV,
EasyPal, Python or another process, never opens a second capture device and
never treats an OFDM primitive as proof of an interoperable radio backend.

## Native target boundary

The build option is `DECODIUM_ENABLE_HAMDRM`. It requires
`DECODIUM_ENABLE_SSTV=ON`; disabling it removes OpenJPEG and all digital
targets while leaving analog SSTV unchanged.

```text
Decodium application and QML
        |
        v
HamDrmController                 owner-thread state and bounded QML facade
        |
        +-- HamDrmMotCodec       MOT header/body data groups and CRC
        +-- HamDrmObjectAssembler out-of-order segments, duplicates, missing set
        +-- HamDrmBsrCodec       compact/EasyPal-style and QSSTV-extended BSR
        +-- HamDrmPartialStore   atomic resumable state below AppDataLocation
        +-- HamDrmImageValidator bounded type, signature and dimension checks
        +-- HamDrmJpeg2000Codec  target-scoped OpenJPEG memory adapter
        |
        v
channel/                         FAC, Part-B MSC coding/interleaving and pilots
        |
        v
phy/                             narrow HAMDRM QAM and OFDM primitives
        |
        v
waveform/HamDrmWaveformAdapters  bounded native RX worker and 48 kHz TX source
        |
        v
Decodium audio and TX ownership existing shared RX tap, output, CAT/PTT watchdog
```

Core protocol and PHY targets are Qt-free. Persistence links only Qt Core;
JPEG2000 links only the audited OpenJPEG target; the application-facing
controller links Qt Core/Gui and the three smaller targets. No analog target
links OpenJPEG.

## Profile model

The canonical registry contains named typed tuples, never a QML-visible opaque
index. The current narrow amateur profile space is the Cartesian product of:

- robustness A, B and amateur E;
- occupied bandwidth 2.3 and 2.5 kHz;
- high and normal protection;
- 4-, 16- and 64-QAM;
- long and short interleaving.

On receive, a CRC-validated FAC selects short or long interleaving even if
the initially selected profile specifies the other depth. A depth change
clears partial packet and deinterleaver state. Bandwidth, constellation,
protection and packet ID must still match the selected profile; this is not
blind profile acquisition.

This yields 72 stable IDs. Payload bytes per 400 ms frame and compatibility
codes are registry data. Amateur robustness E is not advertised as current
broadcast DRM robustness E, and the registry does not claim full ETSI DRM
coverage.

## Object and resume lifecycle

An outbound object is validated before packetisation. Its filename is reduced
to a safe basename, its content type is restricted to JPEG, JP2, PNG, GIF, BMP
or the protocol BSR object, and its object/header/segment counts are checked
before allocation. MOT data groups carry session, user-access and transport
information plus MSB-first CRC-16 (polynomial 0x1021, initial and final xor 0xffff). Header and body groups can arrive out of order.

The assembler accepts an exact duplicate idempotently and rejects a conflicting
duplicate. It reports the exact missing segment set and emits a completed
object only after consistent header metadata, complete segment coverage and
declared-size agreement. Partial state is written through `QSaveFile` using a
transport-ID-derived path, owner-only permissions and an internal SHA-256; a
truncated, modified or symlinked record fails closed.

The BSR parser/generator supports the compact range representation used by
EasyPal-compatible receivers and the extended QSSTV representation. Compact
ranges can intentionally request already received filler around holes; the
controller records this behavior instead of silently treating it as an exact
set. Unsupported profile identifiers and malformed or excessive ranges are
rejected.

BSR generation rejects an unknown dialect value explicitly rather than
silently producing a different dialect. Core negative checks cover two
invalid enum values; valid compact and extended requests remain covered.
On macOS ARM64 the core, controller, waveform and upstream BSR capture
regressions passed after the guard was added. Two Decodium application pairs
also reconstructed the 2,725-byte A/B images exactly after selective repair:
the A request used the extended form and B the compact form. These two
application tests inject the BSR rather than sending it over audio, so they
are not new independent QSSTV/EasyPal interoperability evidence. Artifacts are
`.../decodium-hamdrm-repair-a-uf3go4vo` and
`.../decodium-hamdrm-repair-b-hxlz8ycd` under the macOS temporary directory.
After the same change, `run_qsstv_bsr_oracle.sh` also passed 1,554
differential cases against the unchanged methods in pinned QSSTV source
revision `8c27d6d169d8c6c197eb47c2089870e39bc06a02`, checking exact BSR wire
content and requested blocks. This is source-level QSSTV evidence, not a live
EasyPal exchange. At the time of this check no EasyPal application or Wine
runtime was found locally; network search did not return before timeout.

The controller can transmit a generated compact or extended request as a
`bsr.bin` MOT object through the existing waveform/PTT backend. Received BSR
objects are parsed and shown separately from images; malformed requests are
rejected before publication. A received request is untrusted and does not
automatically trigger retransmission. The operator can explicitly retransmit
requested segments of the last successfully transmitted image kept in memory,
when its transport identifier, segment size, profile, optional filename and
segment indices match. Identifiers can collide; the request does not
authenticate its sender. The cached source is lost when the application exits.
Controller-level A/B tests cover both dialects, rejected mismatches and exact
reassembly after repair. `test_hamdrm_waveform` also encodes the headers and
two selected missing segments as PCM for two JPEG fixtures, including a
missing final segment, then decodes the resulting
waveform in different chunk sizes and verifies exact reassembly. This test
seeds the other segments directly.

The A/B lab `python3 tests/sstv/tools/run_hamdrm_repair_two_app_lab.py a|b`
also verifies selective repair with two full Decodium processes over BlackHole.
The sender first transmits a JPEG and retains its source; after that TX the
receiver is seeded with all but segment 1 and started. The harness injects a
valid BSR into the sender (extended for A, compact for B), then explicitly
starts repair. Both transmitters completed, and both received JPEG files
matched the original 2,725 bytes exactly. Fixtures contain a small JPEG with
zero padding to cross the 1,024-byte segment boundary. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-repair-a-u_1i30tz`
and `.../decodium-hamdrm-repair-b-gyjs240_`. This verifies the application
repair waveform path; the initial receive loss and BSR arrival are simulated,
so it is not a complete over-audio BSR/repair exchange or external interoperability.

An additional experimental `a air` variant sends the BSR over BlackHole and
then changes the same two applications back to repair TX/RX. The BSR was
received and repair was requested, but the image was not reconstructed in
the first runs. With digital-only RX after BSR TX, diagnostics showed the
legacy source selected, with zero accepted HAMDRM chunks; with the analog
local capture started first, the local tap received 40 chunks and then
stopped advancing despite RX being active, monitoring enabled, and neither TX
suspension nor sample discard being set. The cause is not yet established.
`sstvRxDiagnostics` exposes accepted HAMDRM chunks and groups, source and tap
selection, monitor state, TX suspension and sample-discard state to support
further investigation. The tests above are macOS-only; Linux and Windows,
including their legacy-backend paths, require independent verification.
The current harness waits for three consecutive advances in the HAMDRM chunk
counter instead of treating an active state or a single callback as proof of
readiness. Representative failed artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-repair-a-oy4i7rig`
and `.../decodium-hamdrm-repair-a-ftprwylr`. The legacy-mode activation now
reuses native SSTV capture arbitration when no analog receiver is already
active, with ownership intended to restore the preceding monitor on stop.
In subsequent A/B trials, digital-only RX selected the local tap and accepted
43 and 41 chunks respectively, then both stalled before readiness; the full
audio repair test therefore still fails. Artifacts:
`.../decodium-hamdrm-repair-a-21oybrat` and
`.../decodium-hamdrm-repair-b-ojwi0_hn`. The BSR-only two-application A/B
tests passed after this change, but automatic capture ownership/restoration
and operation on Linux and Windows need further independent tests. These are
local BlackHole observations; they do not establish a hardware-radio failure.

On 2026-11-01 the `a air` failure was reproduced again: delayed legacy
panadapter rearm calls stopped the dedicated SSTV capture after BSR TX. The
rearm now leaves the active native SSTV capture alone. With that change,
both `a air` (extended BSR) and `b air` (compact BSR) completed on macOS:
each sent the BSR and repair over BlackHole between two Decodium processes,
and the received 2,725-byte JPEG matched its input byte for byte. The initial
partial object was still seeded by the laboratory, and this does not verify
Linux, Windows, RF or interoperability with another application. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-repair-a-ed1e39hv`
and `.../decodium-hamdrm-repair-b-p3iufbu8`.

The subsequent `a live` and `b live` laboratory runs exercise the complete
local sequence without seeding the partial image: the receiver starts first,
the sender transmits the image over BlackHole, and the test-only receiver
discards decoded MOT body segment 1 until the sender finishes and is listening.
The receiver then sends an extended or compact BSR over BlackHole; the sender
receives the request and transmits the selected repair segment over the same
virtual audio path. Both 2,725-byte images were reconstructed exactly.
Artifacts are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-repair-a-39gja431`
and `.../decodium-hamdrm-repair-b-20r9qrgg`.
Reproduce with `python3 tests/sstv/tools/run_hamdrm_repair_two_app_lab.py a live`
and the corresponding `b live` command. The missing segment is injected
after demodulation, not caused by RF; Windows, Linux, physical PTT and
external-application repair remain unverified. A controller test checks both
fixtures with test mode off and on, including acceptance of the segment after
the sender-ready marker exists.

The live A/B checks were repeated on macOS ARM64 with the harness also
checking each available active RX diagnostic snapshot for BlackHole 2ch,
zero processing failures, discontinuities, dropped chunks, HAMDRM rejected
chunks and queue overflows. Both repaired images matched all 2,725 bytes.
The snapshots are periodic, not a continuous measurement, and the missing
segment is still injected after demodulation. Artifacts are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-repair-a-6fmkn7jr`
and `.../decodium-hamdrm-repair-b-tfyadaqs`. This does not demonstrate live
QSSTV/EasyPal repair, RF behaviour, or other platforms.

An additional local A/B test replaced three seconds of the initial audio
signal with silence in a BlackHole 16ch-to-2ch relay, leaving the later BSR and
repair transmissions unmodified. Both receiver instances retained only the
final body segment and requested the missing segments 0 and 1. Before the fix,
the BSR carried the final segment's 677-byte length instead of the regular
1,024-byte segment length and the sender rejected the request. The receiver
now infers the regular length from the total MOT body size, final segment
length and final index when no regular segment was received; non-integral or
inconsistent inference is rejected. Controller A/B tests cover compact and
extended BSR generation. After the fix both two-application runs transmitted
the BSR and repair over virtual audio and reconstructed the 2,725-byte images
exactly. Relay diagnostics confirmed 144,000 muted frames containing over
139,000 active source frames and no reported relay errors. The core and
controller suites and the existing 1,554-case QSSTV BSR method oracle passed.
These are local macOS ARM64 synthetic audio interruptions, not proof of live
QSSTV/EasyPal exchange, physical PTT, RF or Windows/Linux behaviour.

```sh
python3 tests/sstv/tools/run_hamdrm_repair_audio_loss_lab.py a
python3 tests/sstv/tools/run_hamdrm_repair_audio_loss_lab.py b
```

Successful app artifacts are `.../decodium-hamdrm-repair-a-1hmpfpct` and
`.../decodium-hamdrm-repair-b-mxroepak`; relay artifacts are
`.../decodium-hamdrm-repair-audio-loss-2ryschmh` and
`.../decodium-hamdrm-repair-audio-loss-7zgteyu8` in the macOS temporary
directory. Failed pre-fix relays are `.../decodium-hamdrm-repair-audio-loss-gnkvz_jg`
and `.../decodium-hamdrm-repair-audio-loss-btljf8sx`.

The pinned QSSTV `storeBSR` and `processFIX` method oracle was also run on
the BSR text logged by the sending Decodium process after its audio reception.
Both observed requests matched the QSSTV-generated text and requested blocks
0 and 1; the harness supplies the 1,024-byte regular segment length explicitly
to the reference generator. As a negative control, the pre-fix B log with
677-byte segment length was rejected by the harness. The independent methods
do not establish that an unmodified QSSTV receiver can itself infer the missing
segment length, or prove an application-to-application exchange.

BSR construction also checks any received non-final body segment lengths
against one another and, when the final segment is present, against the size
implied by the MOT header and final index. Inconsistent lengths are rejected
instead of emitting a repair request with an arbitrary size. Controller A/B
tests cover the rejection with compact and extended BSRs and different normal
segment sizes. After this change, both virtual-audio interruption and repair
tests again reconstructed their images exactly, and their observed BSRs passed
the pinned QSSTV method comparison. App artifacts are
`.../decodium-hamdrm-repair-a-ln_si4my` and
`.../decodium-hamdrm-repair-b-8s3_1k_v`; relay artifacts are
`.../decodium-hamdrm-repair-audio-loss-8w_9lv7u` and
`.../decodium-hamdrm-repair-audio-loss-_d_i_gqm`. The incompatible-length
negative cases are controller tests, not two-application transmissions.

The controller now uses the same segment-length validation for the
`canBuildBsr` UI capability and actual BSR generation. Previously, the UI
capability remained true for inconsistent metadata even though generation
rejected the request. A/B controller tests first reproduced that mismatch and
now verify that inconsistent sizes disable the capability while valid final-only
objects remain enabled. Core, controller and offscreen digital QML tests passed.
After the change, the two-Decodium audio-loss A/B repair checks again recovered
the exact files (`.../decodium-hamdrm-repair-a-7jgnh406` and
`.../decodium-hamdrm-repair-b-ilebt48g`; relay artifacts
`.../decodium-hamdrm-repair-audio-loss-z60441_s` and
`.../decodium-hamdrm-repair-audio-loss-lnj3r1_c`). The negative capability
cases are controller checks, not observed interactive GUI sessions.

When the last body segment is missing, its terminal marker and the exact
extent are unavailable. The BSR path now infers a candidate segment count
from the validated MOT body length and consistent received regular-segment
lengths. It rejects inconsistent sizes, indices beyond the inferred last
segment, and counts outside the configured bounds. A correctly sized last
segment received without its terminal marker is accepted as partial data but
is requested again, so completion still requires the explicit marker. The missing-segment
text marks the count as inferred; this assumes the equal-size regular segment
convention and is not proof that an arbitrary sender uses that convention.
The assembler still requires the actual terminal segment before declaring an
object complete. Controller A/B tests exercise extended and compact requests,
missing last and intermediate segments, and rejection of inconsistent lengths
and indices.

This unmarked-final case was observed with an independent TRXAMADRM 3.6
transmitter: a short final body segment was transmitted without its terminal
bit during the initial pass, and the earlier controller could not generate a
BSR. Controller tests now cover short and full-sized unmarked final segments,
both request dialects, and inconsistent lengths. Two full Decodium application
pairs completed the repair over virtual audio with extended and compact BSRs,
respectively (`decodium-hamdrm-repair-a-h2qxfx77` and
`decodium-hamdrm-repair-b-neiax9nh`). In separate A/B tests an independent
TRXAMADRM transmitter produced the partial image and its receiver recovered
the exact extended BSR sent by Decodium (`/tmp/dtrxrepair-a-jp_hauln` and
`/tmp/dtrxrepair-b-e0pbjfe3`). These latter tests establish BSR transport,
not a completed independent repair. A subsequent longer independent repair
initially failed (`/tmp/dtrxrepair-a-28i5n0d0`): the historical transmitter's
laboratory PortAudio adapter dropped newly generated audio when its ring buffer
filled. With laboratory-only audio pacing, the unmodified historical codec
transmitted the complete signal. Two separate 2,225-byte A/B transfers then
completed over virtual audio: Decodium requested the deliberately discarded
segment, the historical receiver obtained the BSR, the historical transmitter
accepted it and sent a repair, and Decodium reconstructed the exact original
file (`/tmp/dtrxrepair-a-7i949z70`, `/tmp/dtrxrepair-b-ritt4p93`). The BSR
was extended in both runs, as required by this historical parser. This is not
evidence of an EasyPal exchange.

A further assembler regression was found when a previously unmarked final
segment was retransmitted with a terminal marker but different bytes: the
duplicate was rejected, yet its marker had already changed assembly state.
The assembler now checks duplicate bytes before committing the marker. An
identical retransmission can establish the marker and reports completion when
all data are present; inconsistent complete collections roll back that marker.
Core A/B tests with different segment sizes failed before this correction and
now verify the rejected duplicate leaves the extent unknown, followed by
byte-exact completion with the valid retransmission. Two full Decodium pairs
again completed extended and compact repair over virtual audio with an
initially unmarked final segment (`.../decodium-hamdrm-repair-a-ejxhvcqc`,
`.../decodium-hamdrm-repair-b-37li6aai`). The conflicting duplicate cases are
core tests, not injected live-audio transmissions.

The same rollback property now applies to newly received segments: a terminal
segment rejected for exceeding the object limit, failing length consistency,
or producing invalid metadata cannot leave its bytes or a new terminal marker
in the assembler. Core A/B tests first reproduced the state corruption with
two segment sizes and both oversized and undersized final bodies; a subsequent
valid final segment now reconstructs each original object exactly. Following
the correction, both two-Decodium virtual-audio repair controls again passed
(`.../decodium-hamdrm-repair-a-etx9dv7h`,
`.../decodium-hamdrm-repair-b-qxpy5hzf`). Malformed segments were injected
in the core tests, not into these audio controls.

On 2026-11-04 the named HAMDRM test executables were rebuilt against the
current source and the complete `ctest -R '^test_hamdrm_'` selection passed
15/15 on macOS. This includes the core, persistence, JPEG2000, controller,
FTP, PHY, channel, waveform, RS, fuzz-smoke and lab CLI checks; it is not an
independent EasyPal application exchange or a cross-platform runtime test.

An additional A/B check exercises this recovery inside full Decodium
application pairs: after dropping the original final segment and requesting
repair, the test-mode receiver submits a shortened copy of the audio-decoded
repair segment to the assembler, confirms rejection without changes to the
stored bytes, count or extent, and then accepts the unmodified audio-decoded
segment. Both extended and compact BSR cases reconstructed the 2,725-byte
images exactly (`.../decodium-hamdrm-repair-a-fmxnrnvm` and
`.../decodium-hamdrm-repair-b-x7q1fm00`). The malformed copy is injected
after demodulation rather than transmitted over audio. This hook is gated by
Qt test mode and explicit laboratory environment variables.

Two full Decodium application pairs on macOS ARM64 transmitted different
2,725-byte JPEG fixtures. The test-only receiver hook discarded the final MOT
segment after demodulation; the initial image, BSR and repair otherwise passed
through BlackHole. Both inferred the missing segment, sent a BSR in the
extended or compact dialect, and reconstructed the exact original bytes. RX
audio diagnostics reported no processing failures, discontinuities, dropped
chunks or HAMDRM queue overflows. Artifacts are
`.../decodium-hamdrm-repair-a-t1a9oetw` and
`.../decodium-hamdrm-repair-b-c0bubwj3`. These runs do not simulate RF loss
or establish live QSSTV/EasyPal repair or other platform behavior. Reproduce
with `DECODIUM_LAB_DROP_MOT_FINAL=1 python3
tests/sstv/tools/run_hamdrm_repair_two_app_lab.py a live` and the same command
with `b`.

Those final-segment repair tests were repeated with the sender logging the BSR
after receiving it over virtual audio. Both images again matched byte-for-byte
(`.../decodium-hamdrm-repair-a-ratunkce` and
`.../decodium-hamdrm-repair-b-5k00lxrk`). The pinned QSSTV `processFIX` and
`storeBSR` method oracle then accepted each observed message and produced the
same request for segment 2, including compact and extended forms. The oracle
was supplied the known three-segment extent and 1,024-byte regular segment
length; it does not show QSSTV inferring a lost terminal marker. Two negative
controls changed that length to 1,023 in copies of the logs and were rejected
by the harness. The oracle's full 1,554-case differential suite also passed.
This is method-level comparison of BSRs from a Decodium-to-Decodium exchange,
not a live repair exchange with QSSTV or EasyPal. Reproduce with
`bash tests/sstv/tools/run_qsstv_bsr_oracle.sh --missing-final A_TX_LOG B_TX_LOG`.

#### QSSTV requests with unknown regular segment size

An isolated build of QSSTV-Experimental source
`a84584248d3307bfa081362031ff20f365337633` decoded the first 43 seconds of
the two earlier audio-loss recordings. This includes the interrupted initial
image and may include the Decodium BSR, but excludes the repair. Its real
receive pipeline reported a header and final segment, with the first two body
segments missing. Its unmodified `storeBSR` generated extended and compact
requests with a zero regular-segment size for both recordings. The diagnostic
patch `tests/sstv/fixtures/qsstv-experimental-10.0-bsr-observer.patch` adds
only end-of-batch reporting and isolated settings/data paths; it does not
change the receive decoder or BSR algorithm. The resulting binary SHA-256 is
`5140245a0faa68389527d462e716f69673b282c2369f98638ffd1563ee276d8b`.
When the complete recordings including repair were decoded, the same build
reconstructed both source JPEGs byte-for-byte and reported no remaining BSR.

Decodium's strict general BSR parser still rejects zero segment size unless
the caller explicitly enables the unknown-size form. The controller accepts
that form as an untrusted incoming BSR and allows **manual** repair only if
transport ID, optional filename, active TX profile and every requested index
match the retained last transmission. It uses that transmission's own stored
segments; a nonzero size must still match exactly. The compact dialect has no
filename, so its match relies on the transport ID and segment bounds; a zero
size adds no independent size confirmation. No incoming BSR automatically
starts a transmission.

Two fresh Decodium application pairs used the actual QSSTV-generated extended
and compact BSR bytes, respectively. A test-only hook verified that each
external BSR matched the live partial object and sent it through the normal
modem. In each run the initial Decodium audio had three seconds muted, the
sender received the QSSTV BSR over BlackHole and manually triggered repair,
and the receiver recovered all 2,725 bytes exactly. The received BSR bytes
were compared with the QSSTV output. Artifacts are
`.../decodium-hamdrm-repair-a-1b0__n9b` and
`.../decodium-hamdrm-repair-b-vmdvgehw`; relay artifacts are
`.../decodium-hamdrm-repair-audio-loss-g413in5e` and
`.../decodium-hamdrm-repair-audio-loss-8g6ujz_a`. The QSSTV logs, WAVs and
BSRs are under `/tmp/decodium-qsstv-bsr-rx.qJ8Gj0`. Core and controller A/B
tests cover strict versus opt-in parsing and rejection of mismatched IDs,
filenames and segment indices; the QML test also passed.
The parser fuzz target exercises both strict and unknown-size modes; a
10,000-run ASan/libFuzzer smoke run seeded with these two BSRs completed on
macOS ARM64 with leak detection disabled and no reported crash. This bounded
run is not a comprehensive security assessment.

This is a file-mediated independent QSSTV receive followed by a two-Decodium
audio exchange, not a live QSSTV BSR transmission, physical RF, or Windows
or Linux verification. The QSSTV build and output paths are local diagnostics,
not dependencies of Decodium.

#### QSSTV-generated BSR waveform in the repair path

The isolated QSSTV-Experimental build was extended with a laboratory-only
command-line hook (`tests/sstv/fixtures/qsstv-experimental-10.0-bsr-tx-lab.patch`)
to pass the BSR files produced above into its own `sendBSR` transmitter and
write WAV files. The build also used the previously documented signed-PCM
patch and the observer/isolation patch; its executable SHA-256 was
`f606a8b61a102f082bc68ad1ac49076c2884251e8e1d7efb20fcaecd5148dd26`.
The A/B recordings requested as `--drm-interleave long` were actually decoded
as E/2.5/high/16-QAM/**short**, and both full Decodium RX applications
reconstructed their respective BSRs byte-for-byte through BlackHole with no
reported audio/processing errors (`.../decodium-qsstv-drm-app-b562qf_d` and
`.../decodium-qsstv-drm-app-ld8zvqpv`). The same two recordings failed with
the long profile and delivered zero groups (`.../decodium-qsstv-drm-app-w_kaf8li`
and `.../decodium-qsstv-drm-app-x79mnsx7`); an adaptive long-profile A trial
also failed. The source maps the CLI strings `short,long` to indices 0,1,
while its transmitter maps index 0 to long and 1 to short. This accounts for
the reversal in this lab build; the earlier failed comparisons used the wrong
profile and are not evidence of a Decodium payload failure.

Two further QSSTV WAVs were generated using CLI `--drm-interleave short`,
which produced the actual **long** profile. Their SHA-256 values are
`c0bbdb6083f256904997fcc2ee877c0b527a170aaf62deb327e2c6795b97b6f8`
and `897761d1f05efdba879bd7e2446db28c25a05114be55e414faff34cc8b4fbe4b`.
The offline Decodium waveform decoder independently reconstructed both BSRs
exactly under the long profile. Two fresh Decodium application pairs then
sent images with three seconds of controlled audio loss, replayed these
QSSTV-generated long-profile BSR WAVs to the original sender over BlackHole,
and retransmitted the requested segments. Both receivers recovered their
2,725-byte inputs exactly; the helper also matched the sender's received BSR
to the independent file and checked RX continuity. Artifacts are
`.../decodium-hamdrm-repair-a-bippd8pd` and
`.../decodium-hamdrm-repair-b-lpgdot1h`, relay directories
`.../decodium-hamdrm-repair-audio-loss-pnokpmd9` and
`.../decodium-hamdrm-repair-audio-loss-dr3q83nw`.

QSSTV received the earlier partial transmissions and generated BSRs in
separate file-mediated runs; its BSR transmitter was also run beforehand and
the resulting audio replayed. The repair was triggered by the laboratory hook
after the received BSR was checked, not by an operator in the GUI. This is not
a live unmodified QSSTV session, a full QSSTV/EasyPal interoperability claim,
RF testing, or verification on Windows/Linux.

The QSSTV transmitter was then changed to use its live Qt Multimedia output
instead of writing a WAV, using the additional diagnostic patch
`tests/sstv/fixtures/qsstv-experimental-10.0-bsr-live-lab.patch`. The isolated
build SHA-256 was
`65323d875958145662127871b3198a11ccbfd37694fc737c4310187ed311700f`.
It explicitly checked and opened BlackHole 2ch for input/output with CAT and
serial PTT disabled. In two local A/B tests, an image was first transmitted
between Decodium processes with three seconds of audio silenced. A separate
QSSTV process then transmitted the corresponding previously generated BSR
live on BlackHole; the original Decodium sender received and matched its
bytes, waited for QSSTV playback to finish, and sent the requested segments.
Both receivers reconstructed their 2,725-byte image exactly. The muted windows
contained 139,508 and 139,525 active source frames respectively. RX diagnostics
reported no discontinuities, dropped chunks, processing failures or HAMDRM
queue overflows. Application artifacts are
`.../decodium-hamdrm-repair-a-hhf4iero` and
`.../decodium-hamdrm-repair-b-aeqlj9jf`; relay artifacts are
`.../decodium-hamdrm-repair-audio-loss-vmq4az7r` and
`.../decodium-hamdrm-repair-audio-loss-3r60ryk9`. The relay monitored audio
without saving a mixed capture for these runs. Each receiver logged 18 BSR
notifications during QSSTV's repeated carousel, while the sender acted once;
the repeated notifications were subsequently addressed.

The receiver now suppresses repeated notifications for an identical BSR with
the same MOT transport ID within an RX session; a changed request or a new RX
session can produce a new notification. This does not suppress carousel assembly.
The controller A/B tests exercise repeated and changed requests and restarting
RX. The live-audio A/B repair tests were repeated with the same setup: each
receiver logged exactly one BSR notification, each sender acted once, and both
2,725-byte images were reconstructed exactly. Their application artifacts are
`.../decodium-hamdrm-repair-a-bxx8a8kh` and
`.../decodium-hamdrm-repair-b-h3rt3ede`; relay artifacts are
`.../decodium-hamdrm-repair-audio-loss-8m8av5h7` and
`.../decodium-hamdrm-repair-audio-loss-e5y0aw_y`.
The controller also distinguishes an identical request carried by a different
MOT transport ID: it emits the object-received signal but does not emit a
text-change notification when the text is unchanged. A/B controller tests
cover this distinction and the repeated new object. Following that change,
the normal live-audio repair A/B tests passed again with exact images and one
BSR notification per receiver (`.../decodium-hamdrm-repair-a-wwr_s47v`,
`.../decodium-hamdrm-repair-b-h25ffron`; relays
`.../decodium-hamdrm-repair-audio-loss-gh22su71` and
`.../decodium-hamdrm-repair-audio-loss-jh3n7kqp`). These audio tests exercise
the normal repair flow, not a change of BSR MOT transport ID over the air.

These tests use the real QSSTV transmission process and live audio, but its
BSRs came from earlier file-mediated QSSTV receive sessions; the command-line
hook and isolated build are not an unmodified GUI workflow. The Decodium lab
automatically triggers repair after the request and playback complete. RF,
EasyPal and other operating systems are not covered. A first trial failed
before QSSTV started when the relay capture hit a full disk
(`.../decodium-hamdrm-repair-audio-loss-q0v2jg_0`); it is not counted as a pass.

The A/B experiment was extended so an isolated QSSTV-Experimental process
listens to the same live, deliberately damaged initial Decodium transmission
as the Decodium receiver. After initial TX, the diagnostic hook asks QSSTV's
source decoder for its actual incomplete object and writes its generated BSR;
a second QSSTV process transmits that BSR live, and Decodium sends the
requested repair. QSSTV reported one of three segments received in both
cases. Both Decodium receivers reconstructed all 2,725 original image bytes,
and no repeated-BSR terminal-inbox errors appeared. A used the extended BSR
and B the compact BSR. Artifacts are `.../decodium-hamdrm-repair-a-aiqvb9ms`,
`.../decodium-hamdrm-repair-b-z3v79yqf`, and relays
`.../decodium-hamdrm-repair-audio-loss-vzlcmsb2`,
`.../decodium-hamdrm-repair-audio-loss-p74oftr0`. The diagnostic binary SHA-256
was `a26cc87e2727c7eaef00ec9f98efef70804916d16c27ddfc8aa708555b39f73b`;
the extra patch is `tests/sstv/fixtures/qsstv-experimental-10.0-bsr-live-rx-lab.patch`.
The two QSSTV processes exchange the generated request through a local file;
the receiving QSSTV process exits before the repair, so this does not prove
QSSTV reconstructs the image or an unmodified interactive QSSTV exchange.
An initial trial (`.../decodium-hamdrm-repair-a-by7eajze`) reconstructed the
image but failed the harness check because QSSTV's request lacked a final
newline; that check was corrected before the A/B passes. It also revealed
QSSTV repeating completed BSR body segments without a new header. The
controller now fingerprints BSR segments and ignores matching repeats while
rejecting conflicting data; both behaviours have controller A/B tests.
Reproduce with `DECODIUM_LAB_REPAIR_QSSTV_BINARY=<diagnostic-binary>` and
`DECODIUM_LAB_REPAIR_QSSTV_LIVE_RX=1` running
`python3 tests/sstv/tools/run_hamdrm_repair_audio_loss_lab.py a` (then `b`).

The next A/B run kept the same QSSTV receiver alive after creating its BSR,
through the subsequent Decodium repair. A diagnostic hook saved the bytes
when QSSTV's source decoder reported the original image complete; both those
files and the Decodium receiver's files exactly matched the 2,725-byte input.
QSSTV had reported one of three image segments received before requesting
repair, and later reported all 2,725 bytes completed in each run. Both BSR
dialects were exercised; RX audio diagnostics had no discontinuities,
dropped or rejected chunks, overflows or processing failures. Artifacts are
`.../decodium-hamdrm-repair-a-u4xzghb0` and
`.../decodium-hamdrm-repair-b-i9gygqq8`; relays are
`.../decodium-hamdrm-repair-audio-loss-pfzgi90_` and
`.../decodium-hamdrm-repair-audio-loss-z2fwai4j`. The diagnostic binary
SHA-256 was `e78a6c2ab9d82a79c6e552ac91ef3098b719b43c289ee7f3d62997661ddb8f59`.
The additional changes are recorded in
`tests/sstv/fixtures/qsstv-experimental-10.0-bsr-live-repair-lab.patch`.
These runs still use a second QSSTV process to transmit the file-mediated BSR,
an automated Decodium repair, and an instrumented receiver; they are not an
unmodified single-process QSSTV operator workflow, RF, EasyPal, or Windows/Linux
evidence. Reproduce with the preceding environment plus
`DECODIUM_LAB_REPAIR_QSSTV_LIVE_REPAIR=1`.

The A/B tests were then repeated with one QSSTV process for the complete
receive, request and receive-again sequence. The diagnostic hook selects the
incomplete object, calls the same BSR transmission function used by the QSSTV
form with the received transmission parameters, and records the TX-stopped
event after QSSTV restarts RX. The request bytes stay in that process's memory;
the copy written to a lab file is for verification only. On the same live
audio relay, QSSTV first reported one of three segments, transmitted its own
BSR, restarted RX and completed the 2,725-byte image exactly. Decodium also
reconstructed the exact input, with zero RX audio discontinuities, dropped or
rejected chunks, queue overflows and processing failures. A exercised the
extended request and B the compact request. Application artifacts are
`.../decodium-hamdrm-repair-a-wigeq86y` and
`.../decodium-hamdrm-repair-b-09rt2c9i`; relay artifacts are
`.../decodium-hamdrm-repair-audio-loss-wksb0dbq` and
`.../decodium-hamdrm-repair-audio-loss-7njsksqw`. Diagnostic QSSTV binary
SHA-256: `b0b60b649101aad0a0e939aa38b4d5f43a5f8d62d8339b3f32281f347f185319`.
The additional hook is recorded in
`tests/sstv/fixtures/qsstv-experimental-10.0-bsr-single-lab.patch`.
The form selection and Decodium repair are automated, and this is still a
patched QSSTV build and a local macOS virtual-audio experiment. It does not
establish an unmodified GUI workflow, RF, EasyPal or Windows/Linux support.
To reproduce, add `DECODIUM_LAB_REPAIR_QSSTV_SINGLE=1` to the preceding
live-repair environment and run both A and B.

#### Reverse direction: QSSTV image, Decodium BSR, QSSTV repair

A separate live-audio lab starts the image at QSSTV, receives it in Decodium,
sends Decodium's BSR back to the same QSSTV process, and checks Decodium's
repaired file against the original JPEG 2000 bytes saved from QSSTV's transmit
session. A three-second initial audio mute is combined with an explicit lab
hook discarding every initial MOT body segment 1 until initial TX completes;
the latter, not the mute alone, guarantees a missing segment. The initial
audio-only A trial recovered the whole image through the carousel and did not
produce a BSR (`.../decodium-qsstv-reverse-a-8bml086f`). An earlier attempt
used an unsuitable 16x16 input and produced no audio
(`.../decodium-qsstv-reverse-a-z6p_0mlh`). Neither is a repair pass.

With QSSTV's original `startDRMFIXTx` busy check intact, both A (extended BSR)
and B (compact BSR) received a complete `bsr.bin` and its content matched the
Decodium request, ignoring final newline. In each, QSSTV called the handler
with `busy=1` and immediately discarded the request. Thus neither image was
repaired. Artifacts: `.../decodium-qsstv-reverse-a-gg5hov4j` and
`.../decodium-qsstv-reverse-b-wpeywq5k`; relay artifacts:
`.../decodium-hamdrm-repair-audio-loss-b_wt7_d1` and
`.../decodium-hamdrm-repair-audio-loss-tblypo1u`. The source check and these
observations identify the immediate reason in this instrumented build; they
do not by themselves prove the outcome of every unmodified QSSTV release or
operator workflow.

As a separate diagnostic, a QSSTV-only lab switch resubmitted that same event
once per second while busy; the test automatically accepted the repair
dialog after five seconds. QSSTV then reported `busy=-1`, matched the original
TX session, and transmitted the requested segment. Decodium reconstructed
exactly 4,032 bytes for A and 4,044 bytes for B. Both requests matched the
bytes received by QSSTV apart from the final newline, and Decodium's active RX
diagnostics reported no discontinuities, dropped or rejected chunks, queue
overflows or processing failures. Artifacts:
`.../decodium-qsstv-reverse-a-ykd8uidw`,
`.../decodium-qsstv-reverse-b-ctn01we9`; relay artifacts:
`.../decodium-hamdrm-repair-audio-loss-pc6ee98g`,
`.../decodium-hamdrm-repair-audio-loss-c22byn1r`. Diagnostic QSSTV SHA-256:
`0dc1c620e406a06e7306a91c29b19aa9f371cdd9d059c3da6f9614caa7541f6f`.
This establishes the selected reverse protocol path only under the diagnostic
busy deferral, not interoperability with unmodified QSSTV, EasyPal, RF or
other operating systems. Patches are
`tests/sstv/fixtures/qsstv-experimental-10.0-bsr-reverse-lab.patch` and
`tests/sstv/fixtures/qsstv-experimental-10.0-bsr-reverse-observer.patch`,
applied after the earlier diagnostic patches. Reproduce using
`DECODIUM_LAB_REPAIR_QSSTV_BINARY=<diagnostic-binary>`,
`DECODIUM_LAB_REPAIR_QSSTV_REVERSE=1` and
`DECODIUM_LAB_QSSTV_REVERSE_DEFER=1` with
`python3 tests/sstv/tools/run_hamdrm_repair_audio_loss_lab.py a` then `b`;
omit the last variable to test the original busy check.

On 2026-11-02, after rebuilding Decodium on v1.0.734 with the local SSTV
changes, the reverse test was repeated with the observer-enabled QSSTV binary
and deferral disabled. Both the extended A and compact B requests were
received byte-for-byte apart from a final newline, then discarded with
`busy=1`; neither run completed repair. The relay reported no errors. The
artifacts are `.../decodium-qsstv-reverse-a-j_ni36fw` and
`.../decodium-qsstv-reverse-b-9qkm0z5t`, with relay directories
`.../decodium-hamdrm-repair-audio-loss-an_rqr9y` and
`.../decodium-hamdrm-repair-audio-loss-kcqq45a4`. These are negative
results from the instrumented build, not a general claim about all versions.

In the corresponding QSSTV source, `busyMode()` includes the DRM receiver's
`drmBusy`; a valid FAC sets it and the status code retains it across ten
invalid-FAC updates. The observation is consistent with BSR completion while
the receiver is still busy, but the original logs do not separately expose
TX-busy and RX-busy flags, so the exact flag was not directly observed. The
laboratory now supports `DECODIUM_LAB_QSSTV_REVERSE_EXPECT_BUSY=1` to check an
expected rejection, matching BSR bytes, loss injection and audio counters;
this mode is mutually exclusive with the diagnostic deferral switch. A pass
in that mode confirms the negative scenario and does not mean a repair passed.
The new expected-rejection checks passed A and B on macOS ARM64 with exact
BSR-byte comparisons and zero reported relay errors; artifacts are
`.../decodium-qsstv-reverse-a-vz66hfk9` and
`.../decodium-qsstv-reverse-b-d_8gz_oj`, with relays
`.../decodium-hamdrm-repair-audio-loss-ixhfmpv9` and
`.../decodium-hamdrm-repair-audio-loss-uh_tiekt`.

A further diagnostic added only TX/RX busy-flag logging to the instrumented
QSSTV build. With deferral off, both A and B again received the exact BSR and
reported `busy=1`, specifically `tx=0 rx=1`. The isolated relay reported no
errors. The test's optional `DECODIUM_LAB_QSSTV_REVERSE_EXPECT_RX_BUSY=1`
requires that observation together with the existing expected-rejection checks.
Artifacts are `.../decodium-qsstv-reverse-a-8cnjzl4r`,
`.../decodium-qsstv-reverse-b-n0snt5gu`, and relay directories
`.../decodium-hamdrm-repair-audio-loss-zz8fzq11` and
`.../decodium-hamdrm-repair-audio-loss-gqgsyfuv`. The observer patch is
`tests/sstv/fixtures/qsstv-experimental-10.0-bsr-busy-flags-observer.patch`,
applied after the earlier reverse-lab patches; this binary's SHA-256 was
`789d91cb1a636f5e3b4580518f4f8d184932c3056f7dbabde27a94dae8d8e3bd`.
These results identify the flag for this experimental build, not every QSSTV
version or radio channel. The separate pinned upstream QSSTV source at commit
`8c27d6d169d8c6c197eb47c2089870e39bc06a02` has no busy guard in its
`dispatcher::startDRMFIXTx`; that is a source comparison, not a live test of
that unmodified application.

A second diagnostic bypassed only the experimental busy guard when the
explicit laboratory switch was set; it retained the busy-flag observer and the
existing automatic acceptance of the repair dialog. Both A/extended and
B/compact then matched their BSR to the original QSSTV TX session, requested
one missing segment and reconstructed the complete 4,032- and 4,044-byte
images byte-for-byte in Decodium. The BlackHole relay reported no errors and
the receiver diagnostics reported no audio discontinuities, drops, rejected
chunks, queue overflows or processing failures. Artifacts are
`.../decodium-qsstv-reverse-a-8reno9ia` and
`.../decodium-qsstv-reverse-b-u1gmc526`, with relays
`.../decodium-hamdrm-repair-audio-loss-omwt3zis` and
`.../decodium-hamdrm-repair-audio-loss-5mz6x5wu`. The diagnostic patch is
`tests/sstv/fixtures/qsstv-experimental-10.0-bsr-busy-bypass-diagnostic.patch`,
applied after the busy-flags observer; the diagnostic binary SHA-256 was
`de5fe14e67dcb8382c7336ace4e3fc190a1fe8d938bfba5dc15636baf16465c9`.
Reproduce the modified-peer control by setting
`DECODIUM_LAB_QSSTV_REVERSE_BYPASS_BUSY=1` in the reverse-lab command above.
This experiment isolates the observed guard as sufficient to stop these two
exchanges and shows that their remaining repair paths work when it is bypassed.
It does not change QSSTV or Decodium production behavior, nor prove an
unmodified peer or EasyPal exchange.

The digital page now explicitly tells the operator when an incoming BSR has
no segment size and that repair uses the last transmission's segment layout.
Its visibility follows the received BSR, including when a new RX session
clears the request. Offscreen QML tests with the real controller cover the
extended and compact zero-size messages from the QSSTV test and a nonzero
control; these tests inspect QML state rather than an interactive desktop.
After this UI change, two Decodium application pairs repeated the loss and
repair with the actual QSSTV BSR bytes; both images were byte-exact and the
helper checked the received request against the independent BSR file.
Artifacts are `.../decodium-hamdrm-repair-a-ja47eoeq` and
`.../decodium-hamdrm-repair-b-y5h3_s3v` with relay directories
`.../decodium-hamdrm-repair-audio-loss-ngy1od0h` and
`.../decodium-hamdrm-repair-audio-loss-9s2kkxxa`. These remain local macOS
virtual-audio and file-mediated QSSTV tests.

The full local audio-loss repair sequence was subsequently captured from
BlackHole 2ch, including the altered initial transmission, the BSR, and the
repair transmission. The relay independently reported no input overflow,
output underflow, or mixed-audio capture overflow. Both Decodium receivers
reconstructed their respective 2,725-byte files exactly. The same two mixed
recordings were then decoded sequentially by the pinned QSSTV-Experimental
10.0 release executable in the disposable bundle without the PDF plugin;
each produced a file byte-for-byte identical to the corresponding original.
As controls, QSSTV decoded each recording only through second 43 and only from
second 46 onwards: all four runs reported no image while still reporting
valid MSC CRCs and data segments. The cuts were selected from observed silent
gaps; they are diagnostic, not a general on-air frame splitter. The data show
that, for these two recorded exchanges, neither portion alone produced an
image but their combined sequence did. They do not show QSSTV generating the
BSR or a live QSSTV-to-Decodium exchange. The batch receiver reported an
existing `bsr.bin` in its normal application-support directory, which was not
isolated for this test. The PDF-plugin isolation removes
the documented duplicate Qt frameworks; the executable itself is unchanged.

Decodium artifacts are `.../decodium-hamdrm-repair-a-7597vd5o` and
`.../decodium-hamdrm-repair-b-gc9uo35j`, with mixed recordings in
`.../decodium-hamdrm-repair-audio-loss-k2ob_6wj` and
`.../decodium-hamdrm-repair-audio-loss-icacheqi`. Sequential QSSTV complete
decodes are `.../decodium-qsstv-drm-decode-6rdprgdn` and
`.../decodium-qsstv-drm-decode-wkd_3wra`; pre-repair A/B controls are
`.../decodium-qsstv-drm-decode-2gqps616` and
`.../decodium-qsstv-drm-decode-dv5h9kd2`; repair-only A/B controls are
`.../decodium-qsstv-drm-decode-ir8xtvxz` and
`.../decodium-qsstv-drm-decode-bcjz4e2h`, under the macOS temporary directory.
The capture helper is `tests/sstv/tools/run_hamdrm_repair_audio_loss_lab.py`;
`run_qsstv_exp_drm_decode_lab.py` accepts `--duration 43` or `--start 46`
and `--expect-no-image` to reproduce the control cuts. QSSTV uses its normal
local settings and these tests do not establish RF, other operating systems,
or live EasyPal/QSSTV repair.

An additional A/B offscreen QML test instantiates the actual digital page with
the real HAMDRM controller and a rendered window. It injects MOT headers and a
valid final segment, checks that Build BSR and the dialect selector become
enabled, then injects an inconsistent non-final segment and checks both become
disabled. The A/extended and B/compact cases passed on macOS ARM64 along with
the complete core, controller and digital QML suites. This is an automated
offscreen property check, not an observed manual GUI session or a new audio
transmission; the preceding two-Decodium A/B results cover the valid audio path.

```sh
bash tests/sstv/tools/run_qsstv_bsr_oracle.sh \
  /path/to/decodium-hamdrm-repair-a-1hmpfpct/tx.log \
  /path/to/decodium-hamdrm-repair-b-mxroepak/tx.log
```

Completed `bsr.bin` objects can be followed by another request using the same
transport ID in the same receive session. The last complete request stays
visible while the following candidate is assembled; a differing header can
replace a stale BSR candidate left by the previous carousel. A/B tests with
two Decodium processes sent two different requests in one RX session over
BlackHole, using extended and compact dialects respectively. The partial MOT
source was seeded by the test, so these checks do not demonstrate complete
image retransmission or external application interoperability. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-a-is2m3aw9`
and `.../decodium-hamdrm-two-app-b-ch1n_mcp`. Reproduce with
`python3 tests/sstv/tools/run_hamdrm_two_app_lab.py a bsr-repeat` and
`python3 tests/sstv/tools/run_hamdrm_two_app_lab.py b bsr-repeat`.

Two isolated Decodium application pairs exchanged A/B BSR objects over
BlackHole without radio: A used the extended form for transport `0x2451`, B
the compact form for `0x5372`; both requested the expected missing segments.
The lab seeded a synthetic partial MOT object before TX, so this is an
application waveform test of BSR transfer, not an end-to-end repair of a
previously transmitted image. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-a-x9patv3y`
and `.../decodium-hamdrm-two-app-b-0ix120us`. Reproduce with
`python3 tests/sstv/tools/run_hamdrm_two_app_lab.py a bsr` and `b bsr`.
After the receive-session cleanup change, these A/B audio transfers were
repeated successfully with artifacts at
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-a-8_yiuxsk`
and `.../decodium-hamdrm-two-app-b-uvi30h6j`. The cleanup itself was exercised
by the A/B controller tests; the application audio tests transfer one BSR each.

## Image boundary

The allocation-free validator checks signature, bounded container structure,
dimensions and pixel ceilings before any full decode. Defaults are:

- 16 MiB object;
- 8 KiB MOT header;
- 8,191 bytes per segment;
- 32,768 segments;
- 80 filename bytes;
- 8,192 pixels on either dimension;
- 16,777,216 total pixels.

The OpenJPEG adapter uses bounded in-memory callbacks. It reads and validates
the JP2 header and components before full decode, emits RGBA for supported
grayscale/RGB/SYCC inputs and provides lossless JP2 encoding. OpenJPEG is a
required dependency only when HAMDRM is enabled; configuration fails with an
actionable error when version 2.5 or newer cannot be found.

## Threading and radio ownership

`HamDrmController` is an owner-thread QObject. Backend callbacks may originate
on worker/audio threads but must be marshalled to the owner and carry a session
identifier so stale callbacks cannot mutate a later operation. Cancellation
must synchronously detach the sink before a backend returns.

A backend may advertise waveform RX or TX only when it spans the complete
application-to-audio boundary. QAM, OFDM, coding, MOT and a self-round-trip do
not individually satisfy that capability. The current Bridge wires
`HamDrmNativeRxBackend` to Decodium's existing shared PCM tap and wires
`HamDrmNativeTxBackend` to `SstvTxCoordinator::startPrepared()`. The controller
therefore reports the corresponding native subset as available only while
those real hooks are installed; a default controller without hooks remains
fail-closed.

The RX adapter performs decode work on one bounded worker. Its cross-thread
producer accepts only configured sample rates/chunk sizes and caps queued
chunks, queued samples and session frames. Cancellation drains active callbacks
before detaching the shared tap. The TX adapter packetises bounded MOT groups,
generates a bounded pull-oriented 48 kHz PCM source and relies on the existing
coordinator for exclusive TX ownership, played-audio completion, cancellation,
watchdog and PTT release. It does not create a second audio or CAT/PTT stack.

## Evidence and non-claims

Unit tests cover all 72 profile records, the standard CRC check value, MOT and
BSR valid/malformed paths, hostile names, out-of-order/duplicate/conflicting
segments, missing tracking, partial-state damage, bounded image containers,
lossless OpenJPEG round-trip, QAM/OFDM primitives, pinned narrow-HAM FAC/MSC
coding and cell plans, and deterministic parser mutation. The application
controller and its QML page also have registered state and offscreen rendering
tests. `test_hamdrm_waveform_adapters` covers the connected controller,
bounded shared-tap RX self-roundtrip, native 48 kHz TX source, stale/completion
handling and the shared PTT release barrier; the full Decodium QML executable
links the same adapters through the Bridge. ASan/UBSan runs cover the pure
core, channel coding and OpenJPEG adapter.

The independent recorded-signal evidence described above is limited to the
tested profiles and fixtures. It does not establish full EasyPal
interoperability, a real-radio transmission, sample-clock/carrier recovery
under RF impairment, or maintained-platform packaging. Those remain release gates and must stay explicit in
`HAMDRM_COMPATIBILITY_MATRIX.md`.

## Audited references

- ETSI ES 201 980 V4.3.1, DRM system specification:
  <https://www.etsi.org/deliver/etsi_es/201900_201999/201980/04.03.01_60/es_201980v040301p.pdf>
- QSSTV pinned behavior reference:
  `ON4QZ/QSSTV@8c27d6d169d8c6c197eb47c2089870e39bc06a02`
- OpenJPEG 2.5.4 API and codec dependency:
  <https://github.com/uclouvain/openjpeg/tree/v2.5.4>

Restricted and ambiguous QSSTV files listed in `UPSTREAM_PROVENANCE.md` are
excluded. The Decodium implementation is clean-room protocol code plus the
separately licensed OpenJPEG library.

#### QSSTV upstream build for a second live-peer investigation

On macOS ARM64, the unmodified pinned upstream checkout at commit
`8c27d6d169d8c6c197eb47c2089870e39bc06a02` was compiled successfully with
Homebrew Qt 5.15.18, PulseAudio 17.0, OpenJPEG, FFTW and Hamlib. Qwt 6.3.0
was independently built against Qt 5 under `/tmp/decodium-qwt-qt5`; its source
archive matched SHA-256
`dcb085896c28aaec5518cbc08c0ee2b4e60ada7ac929d82639f6189851a6129a`.
The upstream build directory is `/tmp/decodium-qsstv-upstream-build`; its
binary SHA-256 was
`954436a79aae1d5cebf98c018e58f2e1b9d80ef9af6c4168ec25aea06e75d5f9`.
This source's `startDRMFIXTx` calls `prepareFIX` without the busy guard seen
in the experimental build. That source difference is an opportunity for a
separate live-peer test, not evidence that the peer actually receives or
repairs images. The binary has not yet been launched: its default configuration
and image directories need isolation and its PulseAudio routing needs controlled
setup before it is safe to run alongside the user's applications. Compilation
produced deployment-target and SDK warnings and does not establish portability.

Installing PulseAudio also upgraded several Homebrew dependencies and installed
OpenSSL 4. Homebrew replaced the default OpenSSL 3 link; this was restored
with `brew unlink openssl@4` and `brew link openssl@3` after installation.

A separate laboratory copy now redirects QSSTV's QSettings, image directories,
logs and home-path fallbacks into an explicitly supplied existing absolute
`DECODIUM_QSSTV_LAB_ROOT`. It exits with code 2 if that root is absent. The
changes are retained as
`tests/sstv/fixtures/qsstv-upstream-isolated-paths-lab.patch`; they do not
modify the HAMDRM, BSR, sound or dispatcher algorithms. The resulting build's
SHA-256 is `681f0b881b2fb5e39646ef85134dfcdc4705007fa8530c037a48caa70e431f03`.
It ran offscreen for 12 seconds with an isolated PulseAudio server and opened
one mono input and one stereo output stream. It was then explicitly stopped.
The default rig settings inspected in the source disable CAT and serial PTT.
This is startup and stream-opening evidence, not a transmitted image.

The private PulseAudio server used only BlackHole 2ch for input, a local null
sink for output, and a socket under the private lab directory. The PulseAudio
CoreAudio module could not create a sink for BlackHole 16ch. A preliminary
buffered user-space bridge from the null sink monitor to the first two
BlackHole 16ch channels reproduced 1,200 and 1,900 Hz test tones. A separate
A/B source check captured the same two tones from BlackHole 2ch through the
PulseAudio input. The bridge observed an output-underflow report at startup
before its first block in both final trials; these tone checks do not establish
continuous lossless audio, BSR operation or QSSTV interoperability. No
production Decodium audio path was changed. The private server and QSSTV
process were stopped after the checks. Lab artifacts and diagnostic scripts
are in `/tmp/decodium-qsstv-upstream-run`, `/tmp/decodium-pulse-check.py` and
`/tmp/decodium-pulse-output-check.py`.

#### Live upstream QSSTV and small PNG labelled JP2

The isolated upstream QSSTV build was extended with test-only controls to
select an image and start its ordinary transmission after the Decodium RX is
ready, and to save the original bytes sent. The combined laboratory patch is
`tests/sstv/fixtures/qsstv-upstream-live-tx-lab.patch`; it does not modify
HAMDRM encoding, the BSR handler or the busy check. Its executable SHA-256 was
`00d4764e24fdd0780b6f131b6253dd0a4e456882678805d36a85643a98e58a12`.
A private PulseAudio monitor-to-BlackHole bridge starts its output only after
buffering, and A/B 1,200/1,900 Hz checks saw no output underflow and no silent
10 ms window within the steady tone. These checks are not an RF channel test.

A first live reference-QSSTV to Decodium image succeeded with byte-exact 4,032
byte JPEG 2000. A second, small PNG was transmitted by QSSTV as `*.jp2` even
though the payload retained its PNG signature; Decodium rejected it with
`JPEG2000 JP2 signature is missing`. This was an actual incompatibility in
the tested upstream path, not an audio failure. Decodium now recognizes the
exact PNG signature only in a declared `.jp2` object, changes the exposed
filename and MIME to PNG, and applies its ordinary bounded PNG checks. It does
not accept a malformed IHDR merely because the signature matches. Core and
controller tests passed, including two distinct PNGs and invalid-IHDR
negative controls; the controller suite also passed ASan/UBSan on this Mac
with leak detection disabled.

After the fix, live upstream QSSTV transmitted two different PNGs advertised
as JP2 through the private audio bridge into separate Decodium receivers.
Both outputs matched the original bytes exactly: 957 and 1,046 bytes. The
bridge recorded 0 output underflows; all sampled RX diagnostics reported zero
discontinuities, dropped chunks, processing failures, rejected HAMDRM chunks
and queue overflows. These are one-way live application tests for one profile,
not a BSR repair, EasyPal, other channels or RF proof. Artifacts are
`.../decodium-qsstv-upstream-live-a-efyyd8uj` and
`.../decodium-qsstv-upstream-live-b-pl2u2mgo`; the earlier successful JP2
control is `.../decodium-qsstv-upstream-live-a-d799p9g4` and the failing PNG
baseline is `.../decodium-qsstv-upstream-live-b-x1liw3ig`. The live helper is
`tests/sstv/tools/run_qsstv_upstream_live_lab.py`; it requires the explicitly
prepared isolated QSSTV binary and PulseAudio server. The B run's PNG signature
check was confirmed against the retained original after the run.

Two Decodium sessions also repeated A/B transmissions of these 957- and
1,046-byte PNGs over BlackHole, with a test-mode-only switch causing the TX
to advertise a `.jp2` name. Both receivers reconstructed the exact original
bytes; artifacts are `.../decodium-hamdrm-two-app-a-3zm70sjg` and
`.../decodium-hamdrm-two-app-b-3tpg5836`. The mismatch injection is guarded
by Qt test mode and `DECODIUM_LAB_HAMDRM_PNG_AS_JP2=1`; ordinary transmission
keeps the image's correct metadata. These tests do not replace the independent
QSSTV tests or establish BSR repair interoperability.

#### Live upstream QSSTV repair with Decodium BSR

The isolated upstream QSSTV peer was instrumented to save the BSR bytes,
log its ordinary `processFIX` result and matching transmission session,
record TX-to-RX transitions, and accept the existing repair dialog after five
seconds. Its repair handler and codec were not modified; the upstream source
has no experimental busy guard. The combined laboratory patch is
`tests/sstv/fixtures/qsstv-upstream-live-repair-lab.patch`, built binary
SHA-256 `06f38c251dcc3f623775a392034daca6ae268002b8ad4ed0bd6ed756ac6212dc`.

On macOS ARM64, upstream QSSTV transmitted distinct images A/B over the
isolated PulseAudio/BlackHole bridge; the Decodium test-mode receiver
intentionally discarded initial MOT body segment 1 until the initial TX
completed. Decodium then transmitted an extended BSR for A and a compact BSR
for B. QSSTV received the exact generated BSR bytes (allowing only a trailing
newline difference), matched the original TX session, requested one missing
block, and transmitted its standard repair. Decodium reconstructed all 4,032
bytes of A and 1,765 bytes of B exactly. The bridge reported zero output
underflows, and the active RX diagnostic snapshots reported zero audio
discontinuities, dropped chunks, processing failures, HAMDRM rejected chunks
and queue overflows. Artifacts are
`.../decodium-qsstv-upstream-live-a-h7s7um8n` and
`.../decodium-qsstv-upstream-live-b-u5b3qcco`; reproduce with
`tests/sstv/tools/run_qsstv_upstream_repair_lab.py a` and `b` after preparing
the isolated binary and PulseAudio server. The private server was stopped
following the tests.

The loss was injected at the MOT receiver, not induced by RF fading or by
muting the audio; the instrumented peer auto-accepted the normal dialog. These
results establish the selected live QSSTV-upstream/Decodium repair sequence,
not uninstrumented GUI operation, EasyPal, every HAMDRM profile or RF
performance. The first A attempt reconstructed identical bytes as well but
its test harness incorrectly applied the input-device assertion to snapshots
where Decodium had suspended RX during BSR transmission; that check was
corrected to inspect active RX snapshots and the passing A run above was
repeated.

#### Forward live repair using the upstream QSSTV peer

A further isolated upstream QSSTV diagnostic keeps one process alive for
receiving the initial Decodium image, generating its own BSR, transmitting it,
restarting RX and receiving the repair. The hook uses the source decoder's
normal BSR builder and transmitter and saves diagnostic copies; it does not
replace the BSR codec or transmit algorithm. Its complete source patch is
`tests/sstv/fixtures/qsstv-upstream-bidirectional-lab.patch`, and the binary
SHA-256 was `49cc1a70b1c1749c947d2e18125325b3bfb4efa6f50809982b501c099688b0be`.
The isolated PulseAudio sink was bridged to BlackHole 2ch while a separate
BlackHole 16-to-2 relay muted three seconds of the initial Decodium image.

In variant A, QSSTV received one of three segments, generated an extended
BSR, transmitted it over audio and resumed RX. Decodium received the same BSR,
transmitted the requested repair and both QSSTV and Decodium reconstructed
all 2,725 original bytes. The audio-loss relay muted 144,000 frames, including
139,508 active frames; the separate PulseAudio bridge reported no underflows.
Artifacts: `.../decodium-hamdrm-repair-a-soe0sugu` and
`.../decodium-hamdrm-repair-audio-loss-fczx3cbr`. This uses automatic form
selection and Decodium repair, a diagnostic QSSTV build and virtual audio.

At this stage variant B remained open: in two consecutive attempts QSSTV received one of
three segments, generated the same 24-byte compact BSR, transmitted it and
returned to RX, but Decodium did not report a received BSR or repair within
the harness timeout. The PulseAudio bridge reported zero output underflows in
both. The application artifacts are `.../decodium-hamdrm-repair-b-eys4u89z`
and `.../decodium-hamdrm-repair-b-9n1cg7nk`; relays are
`.../decodium-hamdrm-repair-audio-loss-ple8zssq` and
`.../decodium-hamdrm-repair-audio-loss-6mfd8rvu`. The first QSSTV output was
retained as `/tmp/decodium-qsstv-upstream-run/bridge-b-failed.f32`. A 28-second
cut around that BSR was replayed into separate Decodium application sessions
using the E/2.5/high/16-QAM long and short profiles; neither produced a
completed BSR or delivered groups (`.../decodium-qsstv-drm-app-45f4xr38` and
`.../decodium-qsstv-drm-app-riy7wfik`). The unmodified QSSTV-Experimental
batch receiver saw 51 good MSC CRCs, no bad MSC CRCs and 17 header/34 data
segments in the same cut, but reported no saved image for the BSR. Its log is
`/tmp/decodium-qsstv-upstream-run/batch-b.log`. This shows that receiver could
synchronize and decode data in the recording; it does not prove the precise
reason Decodium failed or independently verify the BSR's saved contents.
The independent replay and original failures are negative evidence, not a
successful compact-BSR repair; the subsequent default-timing A/B result is
documented under “Standard acquisition retry” below. The diagnostic runner is
`tests/sstv/tools/run_qsstv_upstream_forward_repair_lab.py`; it requires the
separately prepared isolated QSSTV binary and PulseAudio server. The server
was stopped after the runs.

#### Compact-BSR acquisition and opt-in timing recovery

A diagnostic QSSTV-Experimental build isolated its settings and output in
`/tmp/decodium-qsstv-fac-isolated`, then logged the actual FAC for the
previously failing compact BSR recording. It identified robustness E,
2.5 kHz occupancy, 16-QAM, high protection and long interleaving; its
frequency estimate was 350.416 Hz. The diagnostic build additionally had
its existing signed-PCM TX patch, but this experiment used its receiver only.
It produced 51 good and zero bad MSC CRC blocks on that recording. This
confirms the selected long profile for this capture, not the cause of
Decodium's primary timing failure.

On the same 12 kHz mono A/extended and B/compact captures, Decodium's offline
flat primary receiver produced no groups. Its independent early-timing receiver
recovered the exact A and B BSR payloads, with 58 valid frames and 54 groups
from the secondary candidate in each diagnostic run. Crossed expected-payload
controls fail as intended. The retained captures and BSR text are
`tests/sstv/fixtures/qsstv-upstream-live-bsr-{a,b}.s16` and `.txt`; their
A/B PCM SHA-256 values are
`900bf054ee8e0ae8e7b3f38af86da50d4e3e2135f6070a7199263910be754204` and
`61d8481a0adb3847be6f792a5eb3365d11e8196416724a1031fe2414c99deb12`.
`test_hamdrm_upstream_bsr_timing` runs the A/B and crossed-reference controls.
The primary-only failure is documented, not asserted as a permanent expected
result; a future improvement may make it pass.

The full Decodium application then replayed the previously failing compact BSR
with the existing operator-selectable **Experimental adaptive reception**
setting; the received 24 bytes matched exactly and the recorded RX diagnostics
had zero discontinuities, dropped/rejected chunks, queue overflows and
processing failures (`.../decodium-qsstv-drm-app-cwm3wqd1`). In new live
experiments, the same setting was explicitly selected before the initial TX,
so the sender used it when it switched to receive the BSR. A/extended and
B/compact both completed the full upstream-QSSTV receive/request/receive-again
sequence and both QSSTV and Decodium reconstructed each 2,725-byte image
exactly. The initial three-second audio mute contained 139,508 and 139,525
active samples; the PulseAudio bridge reported zero underflows. Artifacts are
`.../decodium-hamdrm-repair-a-t2p3lr25` and
`.../decodium-hamdrm-repair-b-8txtxfik`, with audio relays
`.../decodium-hamdrm-repair-audio-loss-vbkkm2wn` and
`.../decodium-hamdrm-repair-audio-loss-w7xw53mj`. The test-mode environment
`DECODIUM_LAB_HAMDRM_USER_TIMING=1` selects the same existing controller
option before a lab transmission; it does not select it for ordinary users.
The QML tooltip advises enabling it before TX or RX for difficult QSSTV repair
requests. The adaptive path remains explicitly experimental and costs extra
processing; this is not a default-receiver fix, an RF test, an EasyPal result,
or proof of all QSSTV variants. The diagnostic FAC binary SHA-256 was
`3005ac3b63695eecd9d69af26ac1a9e33c8eac7c709c2377cd93f274d568bbb2`.

#### Standard acquisition retry for the upstream BSR recordings

The nominal pilot-minimum candidate can give a frequency estimate outside the
configured acquisition range even though the immediately preceding sample
has a valid estimate. The standard receiver now retries that single preceding
sample only when the original estimate reports an out-of-range frequency and
the candidate has a predecessor. A successful estimate does not bypass pilot,
FAC, MSC or packet validation. It does not replace the normal candidate when
its original frequency estimate is valid. This is a targeted acquisition
fallback; the exact physical reason for the capture's one-sample sensitivity
has not been established.

The retained upstream A/extended and B/compact captures now each decode to
exact BSR bytes through the standard flat receiver: 58 valid frames and 54
validated groups per offline run. Crossed-reference negative controls remain
rejected. `test_hamdrm_upstream_bsr_timing` exercises both the standard and
independent early-candidate paths; the full waveform and existing capture
analyzer suites also passed on macOS ARM64. The previously failing B recording
was replayed through BlackHole into a full Decodium RX with default timing;
it saved the correct 24-byte BSR, with no reported RX discontinuities, dropped
or rejected chunks, overflows or processing failures. Its artifacts are
`.../decodium-qsstv-drm-app-vkv72v1b`.

The same upstream A/B recordings were also checked offline with 1, 7, 37 and
120 silent samples prepended. Each reconstructed the corresponding BSR
byte-for-byte with zero ingest errors in the standard receiver; the test is
part of `test_hamdrm_upstream_bsr_timing`. This checks selected start offsets,
not clock drift, RF fading, live EasyPal or every QSSTV profile.
Separate current-build two-Decodium virtual-audio controls transferred BSR
fixtures A and B byte-for-byte (41 and 25 bytes respectively); artifacts are
`.../decodium-hamdrm-two-app-a-zpi05z3o` and
`.../decodium-hamdrm-two-app-b-u86oylis`. Those exchanges do not inject the
offline sample offsets or constitute an external-peer test.

The same upstream A/B BSR recordings were frequency-shifted offline by -15,
-5, 0, +5 and +15 Hz using a whole-recording analytic FFT, and the ordinary
flat receiver reconstructed the corresponding BSR exactly in all ten cases.
The zero-shift transform reproduced both original PCM inputs sample-for-sample.
The -15 Hz A and +15 Hz B WAVs were additionally replayed over BlackHole into
full Decodium receivers, yielding the exact 39- and 24-byte BSRs with clean
audio diagnostics. Their artifacts are `.../decodium-qsstv-drm-app-0t4tmyji`
and `.../decodium-qsstv-drm-app-tq7750md`. The WAV SHA-256 hashes are
`d04b2c8556adade21f74910590378de73a38e296efb5e78ffc0e28b853bf2eb7`
and `6269ebd27d7c090e962eb85d12b05dbaad3b6024bc0cb12e06a34aaca0ad240a`.
`tests/sstv/tools/prepare_hamdrm_frequency_shift.py` reproduces the shift and
requires NumPy; A/B checks confirmed zero-shift identity, byte-identical
shifted WAV output and overwrite protection. The transform is a controlled
offline modification of recordings, not live RF or a general tolerance limit.

The full upstream-QSSTV receive/request/receive-again experiment was then
repeated A/B with default Decodium timing and a three-second initial audio
mute. Both BSRs were received over audio, QSSTV and Decodium reconstructed the
2,725-byte A/B images exactly, and the separate PulseAudio bridge reported
zero output underflows. The relay reported 139,508/139,525 muted active
frames, respectively. Artifacts are
`.../decodium-hamdrm-repair-a-qt2qw9pg` and
`.../decodium-hamdrm-repair-b-eohth_9e`, with relays
`.../decodium-hamdrm-repair-audio-loss-2khfovtc` and
`.../decodium-hamdrm-repair-audio-loss-lin5hbmv`. These tests use an
instrumented QSSTV peer and automatic request/repair; they do not establish
RF, EasyPal, every QSSTV version or every channel. The earlier two failed
compact-BSR runs are retained above as pre-fix evidence.
The standard A/B BSR regression and the full waveform suite also passed in
the macOS ARM64 ASan/UBSan build after this fallback (leak detection disabled).

After the later laboratory snapshot and FAX changes, both retained upstream
BSR captures were independently replayed over BlackHole into full Decodium
application sessions with the default E/2.5/high/16-QAM/long receiver. A and
B were identified as BSR and matched their 39- and 24-byte reference files
exactly; every checked diagnostic reported the expected source and zero
discontinuities, dropped or rejected chunks, queue overflows and processing
failures. Artifacts are `.../decodium-qsstv-drm-app-v9iuvu9h` and
`.../decodium-qsstv-drm-app-kohyc84u`. The standard/early timing A/B and
crossed-reference CTest also passed. These are replays of retained signals,
not fresh live QSSTV transmissions, complete repair exchanges or RF tests.
