# Native SSTV user guide

This guide covers the SSTV workspace built into Decodium4.  It uses the radio,
audio, CAT/PTT, logbook and storage configuration already selected in
Decodium; it is not a separate decoder application and does not open another
audio input.

## Open and configure the workspace

Open **SSTV** from Decodium's normal navigation.  The workspace contains
Receive, Transmit Studio, Gallery, Remote Sharing, Digital HAMDRM, Settings and
Diagnostics pages.  If the build does not include native SSTV, the workspace
reports that explicitly instead of presenting inactive controls.

Before first use, select and verify Decodium's ordinary RX input, output device
and radio/CAT profile.  SSTV Settings changes only SSTV policy: receiver AFC
and slant defaults, replay retention, receive-without-VIS, raw diagnostic audio,
TX gain and lead/tail or VOX timing, storage retention, sharing opt-in and the
HAMDRM profile.  It does not replace an existing Decodium audio or radio
profile.

## Receive analog SSTV

1. On **Receive**, choose automatic mode for normal operation, then start the
   native receiver.  Audio comes from Decodium's active mono PCM route.
2. A valid standard, extended or narrow VIS selects the canonical mode.  The
   page shows raw/mapped VIS, confidence, current line, signal/sync quality,
   AFC, slant and any decoded FSK ID while the image is rendered progressively.
3. Use a manual mode when the transmitting station announces it.  **Lock**
   prevents later detection from switching the selected mode.
4. **Receive without VIS** permits the bounded line-timing fallback.  It starts
   only when one canonical signature is unambiguous; otherwise it fails closed.
5. **Abort frame** preserves a coherent partial image.  **Reset** clears the
   active acquisition/detector state.  Stop reception before changing the
   underlying Decodium audio route.

For a recording, choose **Replay WAV**.  The same worker/runtime used by live
audio decodes the bounded PCM WAV; cancel stops the replay cleanly.  This is
not a second decoder path.  If diagnostic-audio retention is enabled, changing
mode, AFC or slant can re-decode the bounded retained acquisition.  Raw WAV
export is explicit and stays below Decodium's SSTV export root.

### AFC and slant

- **AFC automatic** estimates only from trusted leader/header/sync references.
- **AFC manual** applies the entered bounded frequency offset; **off** applies
  none.
- **Slant automatic** estimates sample-clock error from line anchors.
- **Slant manual** applies the entered ppm correction; **off** leaves the
  nominal clock.

If an image leans steadily, try automatic slant, then a small manual correction
with the opposite visual slope.  If tones are displaced but geometry is
stable, use AFC.  Do not use AFC to correct a slanted image.

Enable **Auto-save received images** only if every completed reception should
enter the Gallery.  Otherwise use the explicit save action.  Partial and
diagnostic-audio policies remain independent of auto-save.

## Prepare and transmit an image

1. On **Transmit Studio**, choose a local image, paste one, open a Gallery item
   or use an available native source action.  Decodium validates and decodes it
   asynchronously with strict size, dimension, metadata and animation limits.
   The source picker exposes PNG, JPEG, WebP, BMP and TIFF; Qt's required
   GIF/JPEG/TIFF/WebP image-format plugins are checked by release bundles and a
   bundle fails fast if a required plugin is missing.  File extensions never
   bypass `QImageReader` content validation.
2. Select the analog mode.  The prepared preview is the exact transmitted
   raster for that mode, not a QML approximation.
3. Apply fit, fill/crop or explicit stretch, rotation/flip, exposure, colour,
   sharpness, grayscale/dither, transparency background and border as needed.
4. Add bounded callsign, grid, UTC, frequency, mode, message, signal-report or
   watermark overlays.  Check the final prepared preview at its real geometry.
5. Optionally enable FSK ID and enter a valid callsign/permitted identifier.
6. Use **WAV export** for a protocol-only file, or **internal loopback** to pass
   the native encoder PCM through the native receiver without radio/PTT.
7. Verify the displayed Decodium output route, rig, attenuation, PTT policy,
   peak/headroom and clipping counter.  Use a two-second 1200, 1500, 1900 or
   2300 Hz calibration reference before first live operation.
8. Start TX only after checking frequency, permissions, radio state and load.
   Decodium acquires its exclusive TX lease, confirms or applies configured PTT
   policy, observes lead/tail timing, streams bounded PCM and releases PTT on
   completion, cancellation and tested error paths.

Cancel is always available.  A template stores only the selected mode and
allow-listed preparation controls; it never stores the source path, audio or
radio identifiers, credentials or image content.

Internal loopback, calibration metrics and deterministic tests do not prove a
physical sound-card level, another decoder, CAT wiring or RF result.  Observe
the first test on each real station directly, preferably at low power into a
dummy load where appropriate.

## Gallery, metadata and logging

The **Gallery** supports list/grid views, search and filters, lazy thumbnails,
quality/signal badges, selection, favourite protection and keyboard
navigation.  A record may contain mode/VIS, source, timestamps, RF frequency,
callsign/FSK ID, AFC/slant, quality, tags, QSO relation and sharing state.  Raw
audio and filesystem paths are not exposed as ordinary UI identifiers.

Selecting a record opens its bounded metadata/quality panel, including VIS,
sync, slant, audio offset, coverage and partial status when those measurements
exist.  The action menu can export a real PNG, prepare the stored image in
Transmit Studio, re-decode an actually retained WAV, open the explicit QSO-log
workflow, prepare remote sharing, remove only the index record, or delete the
record and files owned by Decodium.  Disabled actions mean the required native
asset or capability is absent; no Gallery button simulates a decode, upload or
transmission.

Use **Log SSTV QSO** only when you want an explicit Decodium logbook entry or
association with an existing entry.  Review callsign, grid, frequency, UTC,
reports and comment, then confirm.  ADIF exports `MODE=SSTV`; the precise image
mode stays in Gallery metadata/comment because ADIF 3.1.6 defines no SSTV
submode.  Image paths and attachment identifiers are never written to ADIF.

Automatic retention is off by default.  Gallery can preview quota/age cleanup
without mutation.  Favourites, QSO-related items and, by default, shared or
in-flight items are protected.  Manual deletion requires the displayed strong
confirmation; enabled automatic retention uses the same bounded, journalled
worker path.

## Private remote sharing and inbox

Remote sharing is Internet/LAN transfer and never keys the radio.  It is off by
default and Decodium supplies no public relay.  Configure an operator-controlled
HTTPS REST, WebDAV or trusted pre-signed PUT provider; save authentication only
when the OS secure credential store is available.

The pre-signed option remains unavailable unless a maintained trusted broker
supplies a short-lived opaque target lease; there is intentionally no field for
pasting or persisting a signed URL.  Peer/relay production service is likewise
unavailable.  A deterministic process-local provider is compiled only for
developer/test injection; it opens no socket and never appears in the normal
provider selector.

To upload, choose a validated Gallery image, enter the private recipient and
optional expiry/message, choose whether callsign/grid are included, confirm the
recipient and queue the transfer.  EXIF is removed.  Pause/resume/cancel and
provider-side delete depend on the provider's advertised capability.  Metered
networks require explicit permission.

Inbox objects are downloaded into private staging and checked for declared
size/hash, MIME/signature, dimensions, pixel/decode bounds, animation and
metadata before preview.  **Accept** performs a second validation and atomic
Gallery import.  Reject, acknowledge, local-copy deletion, provider deletion
and sender blocking are distinct explicit actions; none deletes an unrelated
Gallery item or transmits RF.

TLS certificate and hostname validation cannot be disabled in production.
TLS protects transport, but a normal provider endpoint can read the object;
end-to-end encryption is not currently advertised, and an E2EE-required
transfer fails closed.

## Digital HAMDRM

HAMDRM is a separate digital object mode, not an analog SSTV family.  When the
build includes it, **Digital HAMDRM** shows the named profile and the actual RX,
TX, JPEG2000 and resume capabilities.  Start RX/TX only when the page reports a
complete waveform backend; choose and validate an outbound image before TX.
Partial objects show segment/missing counts and may be resumed or used to build
a BSR request.  Discard removes only that digital partial object.

The page's local profile/MOT/BSR/loopback status is not a claim of QSSTV,
EasyPal, sound-card, on-air or real-radio interoperability.  Refer to
`HAMDRM_COMPATIBILITY_MATRIX.md` for the exact tested subset.

## Diagnostics and troubleshooting

**Diagnostics** exposes bounded scalar counters for RX queues/drops, DSP time,
progressive rate, storage jobs, sharing throughput, TX state and HAMDRM state.
Its export omits image/audio payloads, paths, device IDs, credentials, signed
URLs and operator identity.  Export a snapshot after reproducing a problem;
avoid repeatedly resetting counters before collecting evidence.

The test-tone control requests the existing native calibration path; it does
not synthesize a result in QML.  JSON export runs off the GUI thread and uses an
atomic `QSaveFile` publication to a new local `.json` selected in the dialog.
Success or failure is announced accessibly.  An export is a versioned bounded
snapshot, not an archive: it contains no image, thumbnail, audio or free-form
personal metadata.

- No image activity: confirm that native SSTV is available, RX is started and
  Decodium's selected mono route has signal; inspect queued/dropped samples.
- Wrong or no mode: inspect raw VIS and confidence, try the announced manual
  mode/lock, or explicitly enable no-VIS timing fallback.
- Slanted image: use automatic or bounded manual slant; verify the source
  sample rate and that the radio/audio path is not resampling unpredictably.
- Shifted or washed-out tones: inspect AFC and signal level, reset its history,
  and check receiver filtering; avoid clipping or aggressive audio processing.
- Noisy/incomplete image: keep partial save enabled, check sync/quality/drop
  counters, widen the receiver passband appropriately and reduce local load.
- TX clipping: reduce SSTV output gain/attenuation and rerun calibration.  Do
  not compensate by increasing an already clipping radio input.
- TX/PTT error: stop, confirm no other transmission owns Decodium, verify CAT,
  selected output and SWR policy, then retry; ensure PTT has physically released.
- Sharing failure: verify opt-in, provider HTTPS endpoint/capabilities, secure
  credential availability, recipient confirmation, expiry and metered policy.
- HAMDRM unavailable: use the reported capability reason; do not infer waveform
  support merely because profile or object tools are present.

For implementation boundaries, exact mode evidence and security limits, see
`DEVELOPER_GUIDE.md`, `MODE_MATRIX.md`, `THREAT_MODEL.md` and
`REMOTE_SHARING_PROTOCOL.md` in this directory.
