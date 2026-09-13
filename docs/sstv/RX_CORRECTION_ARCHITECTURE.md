# Native analog SSTV RX correction architecture

Decodium owns one analog SSTV receive path. Audio from the already selected
Decodium source is copied into `SstvAudioIngress`; there is no second capture
device, external decoder process, or receiver UI. The producer boundary only
moves bounded PCM16 blocks. Resampling, preprocessing, demodulation, mode
detection, scan-line mapping and diagnostics run on `SstvRxRuntime`'s worker.

## Correction authority

`SstvFrequencyDemodulator` is the only AFC authority in the runtime. It keeps
both the measured raw frequency and the corrected frequency in every
observation. All analog family session configurations therefore use a zero
`frequencyOffsetHz`; applying an additional session offset would be a bug.

Automatic AFC accepts only protocol references with known nominal frequency:

- the validated 1900 Hz leader;
- the validated header break and VIS control tones; and
- 1200 Hz line sync inside a trusted image window.

Image luminance/chroma observations are explicitly classified as image data
and rejected by the AFC controller. The retained counters distinguish accepted
references, rejected references and rejected image observations. Off mode
forces zero correction. Manual mode installs the bounded user correction
without allowing signal content to update it. Reset clears estimator history,
not the chosen mode.

## Clock and slant correction

Manual slant is installed as `clockErrorPpm` when the family session is
created. Automatic slant is estimated from bounded, confidence-weighted sync
anchors. Native sessions consume real embedded line anchors, so their
scan-line mapping corrects accumulated line timing instead of merely reporting
an estimate. The measured and applied ppm values remain separate diagnostics.

Changing correction parameters for an already retained acquisition uses a
real re-decode through the same native runtime. The Bridge first asks the
runtime to record the bounded mode/AFC/slant parameter set, prepares a private
PCM16 WAV on the audio-job worker, and sends that WAV through the existing
native replay route. Live routing and the previous controls are restored after
completion or cancellation.

## Mode control and missing VIS

Automatic mode selection uses canonical standard, extended and narrow VIS.
Manual mode and mode lock use stable identifiers from
`SstvModeRegistry::canonical()`; the Bridge never accepts an unimplemented RX
mode.

Receive-without-VIS is opt-in. Its timing fallback compares a bounded history
of observed sync pulse duration and line period against canonical registry
signatures. A unique compatible mode may start a session and the detector
returns its retained early anchors, so the first lines are not silently lost.
Zero matches and multiple matches fail closed. A user mode lock may resolve an
otherwise ambiguous canonical timing signature.

## Retained audio and raw WAV export

The worker retains normalized audio at 12 kHz. Retention is configurable from
5 to 600 seconds and `SstvReplayBuffer` enforces an absolute sample bound.
Acquisition descriptors are also bounded. Snapshots record whether either edge
was truncated and associate mode, AFC, slant and FSK ID with the acquisition.

`SstvRxAudioJobController` performs ring snapshots, float-to-PCM16 conversion,
gap placement, SHA-protected atomic WAV writing and metadata sidecar I/O through
QtConcurrent. None of these operations runs in an audio callback or QML/GUI
handler. Cancellation is atomic and shutdown joins the outstanding future.
Diagnostic raw WAV files live below the native SSTV storage export root and
are associated with an image only when their acquisition identifiers match.

## FSK ID and diagnostics

The native `SstvFskIdDetector` runs on the same tone stream and associates a
valid identifier with the active or most recently completed acquisition. Image
storage records that identifier together with VIS, correction, signal, sync
and quality fields.

The runtime publishes bounded scalar snapshots containing raw and mapped VIS,
FSK ID, AFC and slant confidence, signal metrics, current line, replay depth
and capacity, queue/drop counters, average and maximum DSP block time, and
progressive update rate. The optional live scope contains at most the runtime
configuration's point limit; QML paints this scalar list and never receives
audio or pixel arrays.

## Verification boundary

Automated tests cover AFC acquisition at both -100 Hz and +100 Hz, rejection of
image-driven AFC, automatic and manual slant at both -300 ppm and +300 ppm,
fail-closed timing ambiguity, bounded retained audio, parameterized re-decode,
atomic raw WAV export, cancellation/shutdown, arbitrary chunking, partial and
back-to-back frames, every native runtime mode family, and rendered Receive and
Settings QML. Independent PySSTV WAV fixtures cover Robot 36, Robot 8 B/W and
Martin M2.

These tests do not claim a live radio/audio-device reception, RF calibration,
or cross-application on-air interoperability session. Those require explicit
hardware and operator validation.
