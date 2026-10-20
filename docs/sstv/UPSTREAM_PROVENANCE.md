# SSTV upstream provenance

Audit snapshot: 2026-08-23/24, reconciled with the integrated source tree on
2026-08-24. All repositories were inspected in temporary directories outside
the Decodium checkout. No upstream implementation source has been copied or
adapted into this snapshot; the native files below are clean-room code informed
by public protocol behaviour and pinned developer-only reference executions.

## Audited revisions

| Project | Audited revision | Licence finding | Relevant paths and permitted use |
|---|---|---|---|
| SSTV Handbook | PDF retrieved 2026-08-24; SHA-256 `e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d`; 18,043,795 bytes; 175 pages; embedded creation date 2019-11-17 | Publicly distributed reference document; no text, figures or tables are redistributed in Decodium | AVT chapter 4.2.5/figure 4.7/table 4.6 and chapter 5 identities were audited as protocol behaviour. The exact hash pins the document consulted; the PDF is not a build/runtime dependency. |
| QSSTV | `ON4QZ/QSSTV@8c27d6d169d8c6c197eb47c2089870e39bc06a02` | Root GPL-3.0; many analog files GPL-2.0-or-later; project notice requires attribution for work based in whole or part on QSSTV | `src/sstv/sstvparam.*`, `src/sstv/modes/*`, `src/sstv/{sstvrx,sstvtx,syncprocessor,visfskid}.*`, `src/dsp/*`, `src/drmrx/*`, `src/drmtx/*`. Behaviour/tables may be audited; any adaptation needs file-level review, SPDX/notice and attribution. Do not transplant the application architecture. |
| SlowRX mission fork | `dnet/slowrx@a50a4e2c291d852a950f25e77d411e77efd9cd89` | ISC, compatible when notice is retained | `modespec.c`, `common.h`, `vis.c`, `fsk.c`, `sync.c`, `video.c`, `pcm.c`. RX robustness/behaviour reference. The fork stopped in 2013. |
| SlowRX current upstream | `windytan/slowrx@ca6d7012ae788b5057646170bd86590a7f68bd69` | ISC | Same paths. Unlike the old fork, current upstream contains active PD decoding and a corrected Robot BW8 timing. Prefer this revision while preserving the mission fork in the comparison. |
| Robot36 | `xdsopl/robot36@75146a5342bf27a165f8790bcb33b56a6d96a2f8` (`v2`) | 0BSD | `Decoder.java`, `Mode.java`, `BaseMode.java`, `RGBModes.java`, Robot/PD/HFFax classes and streaming DSP classes. Behaviour-only/clean-room C++ reference; no Java runtime. |
| libsstv | `rimio/libsstv@193157a993ac34bfa074074004c9ddadcfe6fd15` | MIT | `src/libsstv.template.h`, `src/sstv.{h,c}`, `src/encoder.c`, `src/luts.*`, `util/genluts.py`. Encoder-only phase/timing oracle; retain MIT notice if adapted. |
| pySSTV | `dnet/pySSTV@d998fad154d3e6ad2d73af5add49beec0d2ab59f` | MIT | `pysstv/{sstv,color,grayscale}.py`, tests and CLI. Developer-only fixture/timing comparison; never a runtime dependency. |
| MMSSTV source mirror | `n5ac/mmsstv@8060b5f1e9727b0052d74108081c6db7b26babad` | `sstv.cpp` and `Main.cpp` state LGPL-3.0-or-later; the repository includes GPLv3/LGPLv3 licence texts | `mode.txt`, `sstv.cpp` and `Main.cpp`. Audited only for protocol landmarks and executable behaviour. No expression or implementation code is copied; the pinned source is never a runtime/build dependency. |
| QT6SSTV | `pa2eon/QT6SSTV@6ae74b786af926d080bf97ac707395a806cf8e91` | No root licence file; manual says GPLv3 and inherited files are often GPL-2.0-or-later | Same QSSTV lineage under `src/sstv`, `src/dsp`, `src/drmrx`, `src/drmtx`; use only to study Qt6 migration. It is not an independent protocol confirmation. |
| OpenJPEG | `uclouvain/openjpeg@v2.5.4` | BSD-2-Clause; binary redistribution requires its copyright, conditions and disclaimer | Public `openjpeg.h` API only, linked as a separately licensed HAMDRM dependency. Decodium's bounded memory adapter is original code; no OpenJPEG source is vendored or modified. |

Repository URLs are recorded with the commit SHA in the audit evidence; all
future copied/adapted files must additionally record original path, author,
file-level licence, destination, reuse type and modifications in the table
below before they are committed.

## Imported/adapted component ledger

| Decodium destination | Upstream/path/SHA | Author and file licence | Reuse type | Modifications/notices |
|---|---|---|---|---|
| _None_ | — | — | — | No upstream code has been imported. |

The ledger is a merge gate. A component absent from it may be a clean-room
implementation based on public protocol behaviour, but it may not silently
contain upstream expressions or tables.

## Clean-room behaviour components

| Decodium component | Audited behaviour sources | Current evidence and limit |
|---|---|---|
| `src/sstv/core/SstvVisCodec.*`, `SstvNarrowVisCodec.*` and `rx/SstvNarrowVisDetector.*` | QSSTV `8c27d6d`, MMSSTV mirror `8060b5f`, the MMSSTV manual and SSTV Handbook | Original C++17 framing/detection code. `SstvVisCodec` owns standard and MMSSTV wide extended VIS; the separate narrow codec/detector owns four six-bit-group N-VIS framing. Deterministic parity/checksum, truncation, invalid-candidate and runtime coexistence tests pass. |
| `src/sstv/core/SstvFskIdCodec.*` | QSSTV `8c27d6d`, SlowRX `a50a4e2c`; pySSTV `d998fad1` used only to expose its incomplete framing | Original C++17 bit/symbol codec and tone plan. Tests cover framing, sanitisation, raw diagnostics and checksum failures. Checksum and complete tone envelope still have only one audited implementation lineage, so this is not independent on-air interoperability proof. |
| `src/sstv/core/SstvModeRegistry.*` and `SstvTimingAccumulator.*` | Mission catalogue, SSTV Handbook tables 4.5/mode list, this audit and public protocol units | Original validation/fixed-point infrastructure. The canonical registry claims native support only for implemented modes and keeps sampled, transmitted and display geometry distinct. Unresolved catalogue rows remain unimplemented or blocked. |
| `src/sstv/dsp/SstvResampler.*`, `rx/SstvAudioRingBuffer.*` and `rx/SstvReplayBuffer.*` | Standard FIR/sample-rate-conversion practice and the audited Decodium audio contract | Original bounded C++17 streaming implementation. Unit, sanitizer and thread-sanitizer evidence covers rate conversion, chunk continuity, queue overflow/cancellation and replay retention. `SstvAudioIngress` and the Bridge-owned bounded relay now connect the existing local/RTL-SDR/TCI/DecoPort PCM fan-out and WAV replay to the same runtime; no second capture device is opened. Hardware sources remain unverified. |
| `src/sstv/tx/SstvToneGenerator.*` and `SstvTxStream.*` | Public DDS and fixed-point timing principles; protocol tones remain registry/codec data | Original phase-continuous bounded pull generator. It is now used by the family encoders, four calibration sources, optional FSK-ID tail, atomic WAV export and `SstvTxCoordinator`; deterministic tests cover chunk invariance, headroom, cancellation and long streams. Existing Decodium SoundOutput/CAT/PTT ownership is used, but no real keyed-radio trial is claimed. |
| `src/sstv/dsp/SstvToneDetector.*` and `rx/SstvRxStateMachine.*` | Public Goertzel/filter-bank principles and the mission RX lifecycle | Original bounded tone classifier and typed deterministic acquisition state machine. Tests cover the required discrete tone bank, offset tracking, hostile bounds, VIS/lock/timing-fallback/manual transitions, sync recovery and terminal paths. `SstvRxRuntime` now dispatches complete progressive sessions for every implemented analog family; external RF and most cross-application vectors remain unverified. |
| `src/sstv/image/SstvColourConverter.*` and `SstvImageFrame.*` | Published full-range BT.601/JPEG equations and generic progressive-frame assembly | Original fixed-point colour primitives and bounded progressive RGB frame. Tests cover independent equation vectors, component coverage, caller-supplied pass/interlace mapping, dirty events and coherent snapshots. Family modules own the exact Robot and PD chroma schedules. |
| `src/sstv/dsp/SstvPreprocessor.*`, `SstvFrequencyDemodulator.*` and `SstvSignalMetrics.*` | Public DC-blocking, biquad, AGC, analytic-signal phase-discriminator and log-domain signal-measurement principles | Original bounded 12 kHz RX frontend. Unit and sanitizer tests cover chunk invariance, canonical tones, gain/DC/noise/clipping, optional impulse and 50/60 Hz suppression, reset, bounded AFC, hostile numeric input and hard work/output ceilings. The production runtime feeds its observations into native VIS/N-VIS/AVT/timing acquisition and family decoders through the single existing-audio fan-out. |
| `src/sstv/tx/SstvWavStreamWriter.*` and `integration/SstvWavExporter.*` | Public RIFF/WAVE PCM format and generic bounded streaming-I/O practice | Original streaming mono PCM16 writer plus a Qt `QSaveFile` adapter. Golden-byte, state, failure, cancellation, short-write, no-clobber, exact family length and simulated 4 GiB overflow tests pass; RF64 is deliberately rejected. Export never asserts PTT. |
| `src/sstv/integration/SstvRxRuntime.*`, RX correction/detector classes and `SstvRxAudioJobController.*` | Public streaming-DSP/state-machine practice, protocol data named in the family rows and the audited Decodium PCM contract | Original worker-owned integration. One AFC authority, bounded timing fallback, automatic/manual slant, FSK-ID association, retained-audio re-decode and atomic raw-WAV jobs are connected to Bridge controls. Deterministic -/+100 Hz and -/+300 ppm tests are not live RF evidence. |
| `src/sstv/integration/SstvTxCoordinator.*`, `SstvTxAudioDevice.*`, `SstvTxSources.*` and `SstvStudioController.*` | Existing Decodium SoundOutput/CAT/PTT policy plus the native family encoders recorded below | Original fail-safe integration and Studio pipeline. Tests cover exclusive ownership, delayed audio, watchdog, underrun/disconnect, exactly-once release, cancellation in header/image/FSK-ID, image preparation, loopback and atomic WAV. The fake coordinator and self-loopback do not prove physical PTT or interoperability. |
| `src/sstv/storage/*`, `src/sstv/models/SstvGalleryModel.*` and `SstvThumbnailProvider.*` | Qt `QStandardPaths`, `QSaveFile`, SQLite and model/view APIs; no SSTV upstream source | Original bounded storage/gallery implementation with versioned migrations, metadata, atomic files, retention/delete journal, paged queries and lazy thumbnails. Local deterministic tests include a 5,000-row model; release-platform filesystem/plugin checks remain separate. |
| `src/sstv/sharing/*` and `src/sstv/models/SstvShareController.*` | Mission's provider-neutral protocol requirements, Qt Network/SQL and standard HTTPS/WebDAV/pre-signed PUT semantics; no audited SSTV application's network code was copied | Original versioned manifest/provider/queue/inbox implementation. Schema v3, bounded provider sessions, exact JSON including escaped duplicate-key rejection, TLS/redirect policy, secure credential leases and validated Gallery handoff have focused tests. The process-local provider is test-only; no production relay, E2EE or Internet interoperability is claimed. |
| `src/sstv/diagnostics/*` | Existing Decodium/Qt logging and atomic-file facilities; no upstream SSTV code | Original strict allowlist, bounded 512-event ring and version-1 diagnostic exporter. The final Bridge projection uses stable ASCII source tokens rather than translated/slash-delimited labels, refreshes active TX at no more than 4 Hz plus terminal state, represents unavailable HAMDRM explicitly rather than fabricating zero metrics, and preserves the last terminal test-tone result. The test-tone action is guarded and labelled as real PTT/RF transmission. Images, audio, paths, URLs, credentials and personal text remain rejected. Rebuilt controller/QML coverage is included in the final 83/83 SSTV-labelled portion of the 120/120 normal CTest pass. |
| `src/sstv/analog/SstvMartinM1.*`, `SstvMartinM1RxSession.*` and native RX/TX integration | SSTV Handbook table 4.4/chapter 5 mode list plus the pinned behaviour sources above | Original bounded C++17 Martin M1/M2/M3/M4 mapper, RX/TX, session, coordinator/WAV and Studio pipeline. All modes transmit/display 320 columns; M2/M4 retain effective sampled width 160 in registry metadata. VIS 44/40/36/32, GBR order, explicit clock error, 256/128-row bounds and exact TX frame boundaries are deterministically tested. M2/M3/M4 use compact pinned libsstv landmarks. No external runtime is loaded. |
| `src/sstv/analog/SstvScottie.*`, `SstvScottieRxSession.*` and native integration | SSTV Handbook table 4.5/mode list plus the pinned behaviour sources above | Original bounded C++17 Scottie S1/S2/S3/S4/DX RX/TX. S3/S4 are 320x128 wire/display rasters with VIS 52/48 and S1/S2 scanline timings; S4's 160 effective sampled width is retained separately in the registry. Runtime and Studio execute these modes inside Decodium; no external decoder/encoder is loaded at runtime. |
| `src/sstv/analog/SstvRobot.*`, `SstvRobotRxSession.*` and native integration | SSTV Handbook table 4.1, pinned libsstv `193157a9`, Robot36 `75146a5` and pySSTV `d998fad1` | Original bounded C++17 Robot colour/monochrome RX/TX. Robot B/W 8 native TX retains the canonical 10 ms sync plus 56 ms scan (66 ms line), while RX explicitly recognises pySSTV's independent 7 ms plus 60 ms compatibility waveform (67 ms line) without changing the canonical encoder. The 66/67 ms selection passed 40 repeated runs and ASan; the pinned PySSTV Robot B/W 8 vector remains one of the three independent WAV replays. This is not another decoder or live-radio proof. |
| `src/sstv/analog/SstvSequentialRgb.*`, `SstvSequentialRgbRxSession.*` and native integration | SSTV Handbook tables 4.7/4.8 and chapter 5, QSSTV `8c27d6d`, SlowRX `ca6d7012`, Robot36 `75146a5` and pySSTV `d998fad1` | Original table-driven bounded C++17 Wraase SC2-60/120/180 and Pasokon P3/P5/P7 RGB RX/TX. One cumulative fractional-sample mapper drives streaming encoding and one-line-bounded decoding. Runtime automatic VIS, coordinator, WAV and Studio remain in-process. SC2-60 has no independent executable landmark; the other five use compact executed pySSTV timing landmarks only. No live-radio or cross-application verification is claimed. |
| `src/sstv/analog/SstvPd.*`, `SstvPdRxSession.*` and native integration | SSTV Handbook PD table/chapter 5, QSSTV `8c27d6d`, current SlowRX, Robot36 `75146a5`, pySSTV `d998fad1` and libsstv `193157a9` | Original table-driven bounded C++17 PD50/90/120/160/180/240/290 RX/TX. One cumulative mapper implements 20 ms sync, 2.08 ms porch and Y-even/vertical-average Cr/vertical-average Cb/Y-odd, stopping at exactly height/2 pairs. Runtime VIS, TX coordinator, WAV and Studio stay in-process. Compact pySSTV landmarks cover PD90-PD290; libsstv landmarks cover the compatible prefix of all seven while tests explicitly reject its extra-pair/OOB defect. No live-radio or cross-application verification is claimed. |
| `src/sstv/analog/SstvAvtSyncCodec.*`, `SstvAvt.*`, `SstvAvtRxSession.*` and native integration | Pinned SSTV Handbook PDF hash above, QSSTV `8c27d6d` and MMSSTV mirror `8060b5f` | Original bounded C++17 normal AVT24/90/94 RX/TX. Three complete standard VIS headers precede a fixed-memory 32x17 protected countdown at 102.4 baud; one cumulative mapper then decodes/transmits continuous R/G/B without line sync. AVT90 retains 256 effective columns separately from the audited 320 prepared/wire raster and uses correct prefix `101`, not MMSSTV's conflicting `010`. Runtime autodetect, TX coordinator, WAV and Studio stay in-process. Narrow/QRM identities are catalogue-only. The fixture contains source landmarks, not copied implementation or independent PCM; no live/cross-application proof is claimed. |
| `src/sstv/analog/SstvMmsstvExtended.*`, `SstvMmsstvExtendedRxSession.*`, the N-VIS components above and native integration | MMSSTV mirror `8060b5f`, SSTV Handbook and QSSTV `8c27d6d` | Original table-driven bounded C++17 implementation of MP73/115/140/175, MR73/90/115/140/175, ML180/240/280/320, MP73N/110N/140N and MC110N/140N/180N. MP uses paired Y/Cr/Cb/Y with vertically shared chroma; MR/ML use full Y plus half-width Cr/Cb and 0.1 ms holds; MC uses sequential RGB. Cumulative mapping, streaming TX/WAV, progressive RX/runtime/autodetect and Studio remain in-process. The fixture contains pinned source landmarks only, no copied implementation or PCM; live/cross-application verification is not claimed. |
| `src/sstv/digital/HamDrm*.{h,cpp}`, `digital/phy/*` and `digital/channel/*` | Pinned QSSTV behavior audit, ETSI ES 201 980 V4.3.1 and public MOT/DRM behavior | Original bounded C++17 HAMDRM profile, MOT, BSR, partial-state, channel and OFDM code. Restricted/ambiguous upstream files remain excluded. Local deterministic tests are not an independent QSSTV waveform claim. |
| `src/sstv/digital/HamDrmJpeg2000Codec.*` | OpenJPEG 2.5.4 public API | Original bounded in-memory adapter linked to unmodified OpenJPEG. Header/component limits precede full decode. Exact local lossless round-trip and malformed-input tests pass; packaging and cross-application JP2 exchange remain separate evidence. |

## Functional coverage observed upstream

- QSSTV/QT6SSTV contain RX and TX paths for M1/M2, S1/S2/DX,
  SC2-60/120/180, Robot C24/C36/C72 and BW8/BW12, P3/P5/P7, all required PD,
  AVT24/90/94, the required MP/MR/ML and narrow sets, and FAX480. They do not
  contain M3/M4, S3/S4, Robot C12, BW24/BW36 or an active broad HFFAX/WEFAX
  catalogue.
- SlowRX's old dnet fork contains RX paths for M1-M4, S1/S2/DX, Robot
  C24/C36/C72, BW8/BW12/BW24, SC2-120/180 and P3/P5/P7. Its PD rows do not
  prove PD RX: the decoder path is commented out. Current windytan upstream
  implements PD RX.
- Robot36 contains RX for M1/M2, S1/S2/DX, SC2-180, Robot C36/C72, all required
  PD modes and HFFax/raw fallback. It has no TX, FSK ID or HAMDRM.
- libsstv is explicitly encoder-only. It generates FAX480, M1-M4, S1-S4/DX,
  Robot C12/C24/C36/C72, BW8/BW12/BW24/BW36 (with R/G/B VIS aliases) and all
  required PD modes.
- pySSTV is TX-only for M1/M2, S1/S2/DX, C36, BW8/BW24, SC2-120/180,
  P3/P5/P7 and PD90-PD290 except PD50.
- HAMDRM is available only in the single QSSTV/QT6SSTV lineage. This is not
  independent interoperability evidence.

"Contains a path" means an upstream code path was observed, not that Decodium
supports the mode or that upstream interoperability was executed.

## Audited conflicts and resolutions

1. QSSTV assigns extended VIS `0x4A23` to both MR140 and MR175; QT6SSTV repeats
   the collision. The original MMSSTV transmitter and `mode.txt`, independently
   cross-checked against the SSTV Handbook, identify MR175 as raw extension
   `0x4C` (`0x4C23` when displayed extension-first). Decodium therefore maps
   MR140 to raw `0x4A`, maps MR175 to raw `0x4C`, and rejects the duplicate
   QSSTV row. This resolves the implementation mapping, not live or
   cross-application verification.
2. QSSTV/libsstv often store parity-inclusive eight-bit VIS values while
   SlowRX/Robot36/pySSTV store the seven-bit payload. The Decodium schema must
   separate payload, parity and extended encoding before comparing values.
3. pySSTV declares width 160 for Martin M2 and Scottie S2; QSSTV, SlowRX,
   Robot36 and libsstv use 320. Handbook table 4.4 resolves Martin M2 by
   distinguishing 160 effective sampled columns from the 320-column
   wire/display raster; the same distinction is retained in the registry.
4. SlowRX M3 declares 0.2288 ms pixels with a 446.446 ms line, while the SSTV
   Handbook and libsstv both use 0.4576 ms pixels. The internally consistent
   Handbook/libsstv value controls the native M3 implementation.
5. Robot C24 is 160x120 in QSSTV, 320x120 in libsstv and 320x240 in SlowRX.
   Sampled, transmitted and displayed geometry must be distinguished and tested.
6. FAX480 is 512x500 in QSSTV and 512x480 in libsstv.
7. Robot monochrome has R/G/B VIS aliases in libsstv while most other sources
   implement a single alias. Aliases must be catalogued, not collapsed silently.
8. QSSTV rounds some analog values (Martin sync to 5 ms) where other references
   use 4.862 ms. TX timing needs a normative source or independent capture.
9. Handbook table 4.7 presents rounded Wraase SC2 component times in a 2:4:2
   pattern, while QSSTV, SlowRX and pySSTV executable paths use equal RGB scans
   for SC2-120/180. QSSTV itself gives SC2-60 different RX-side and TX-side
   porch/scan fields. Decodium therefore names and tests explicit compatibility
   profiles: QSSTV's RX-side 61.5435 s equal-RGB profile for SC2-60, pySSTV's
   coherent 475.5225 ms line for SC2-120, and the pySSTV/SlowRX 711.0225 ms
   line for SC2-180. Values from incompatible rows are not combined, and no
   equivalence with QSSTV's SC2-60 TX waveform is claimed.
10. Handbook table 4.7's Wraase resolution column says 256/320/512, while its
    chapter 5 mode list and the executable implementations carry a 320-column
    raster. The registry records 320 transmitted/display columns and retains
    256/320/512 only as effective sampled widths.
11. Handbook table 4.8 labels P3 as 320x496, while its chapter 5 list, SlowRX
    and pySSTV carry 640 wire/display columns. P3 therefore records 320 as its
    effective sampled width and 640x496 as the image raster. P5/P7 are 640x496.
    The same Handbook prose says 20 sync units while its numeric table and the
    executable SlowRX/pySSTV paths resolve to 25. Decodium selects 25 units and
    derives cumulative boundaries from 1/4800, 1/3200 and 1/2400 s rather than
    independently accumulating rounded per-pixel constants. The Handbook also
    reserves the upper 16 rows for a calibration grayscale without specifying
    one canonical pixel pattern. The audited executable pySSTV path accepts all
    496 prepared RGB rows and does not synthesize that content. Decodium follows
    that executable behaviour instead of silently inventing a calibration strip.
12. The Handbook's PD visible-area prose omits calibration rows while its mode
    list and the executable implementations carry 320x256, 640x496, 512x400
    and 800x616 transmitted rasters. Decodium retains the full waveform rows
    and does not synthesize their contents. PD160's isolated 195.854 ms
    component entry conflicts with 512 pixels at 382 us and the coherent
    804.416 ms pair, so the derived 195.584 ms component controls. The pinned
    libsstv encoder also has a verified terminal defect: after the last valid
    Y-odd scan it increments to row `height`, starts one extra pair and reads
    beyond the supplied raster. Decodium's immutable `height/2` pair count
    excludes that suffix; tests retain it only as negative evidence.
13. QSSTV FSK ID uses six-bit LSB symbols, 22 ms bits, 1900/2100 Hz, `0x2A`
    header, `0x01` end and XOR checksum. SlowRX does not validate the checksum;
    pySSTV omits checksum and the complete preamble. A separate vector is needed.
14. MMSSTV `mode.txt` prose describes MC110 as 143 ms per component, while the
    pinned executable uses 140 ms and QSSTV's quantised line is
    428.52734375 ms. Decodium follows the executable 140 ms component and exact
    428.5 ms structural scan (8 ms sync, 0.5 ms porch, three components). The
    prose value remains divergence evidence; independent PCM remains required
    before an interoperability claim.
15. The Handbook records AVT90 as 256x240 effective resolution while pinned
    QSSTV and MMSSTV prepare and scan 320 columns over the same 125 ms
    component. Decodium records 256 effective columns and a 320x240
    prepared/transmitted raster rather than silently choosing one meaning.
    The pinned MMSSTV AVT90 transmitter also begins with protected word
    `0x5f`/inverse `0xa0`; its `010` high prefix names AVT24. Decodium uses the
    AVT90 `101` prefix and retains the MMSSTV value only as negative evidence.

All divergences remain visible in `MODE_CATALOG.md` and `MODE_MATRIX.md`.
Items 1, 3, 4, 9, 10, 11, 12, 14, 15 and the Martin part of item 8 now have either an
authoritative Handbook resolution or an explicitly named, internally coherent
compatibility profile plus executed pinned landmarks where available. That
permits the corresponding implemented claims, not verified live-radio or
cross-application interoperability claims. Other unresolved conflicts, such as
FAX480 geometry, continue to block their affected modes.

## Fixture provenance

The only complete upstream audio fixture found inside the audited repositories
was libsstv's self-generated PD180
`docs/sample.wav`, PCM16 mono at 48 kHz, SHA-256:

```text
aced520cf24941e55868045793d7e67449210ef49c496c307912abf7b45be25b
```

Its source bitmap SHA-256 is:

```text
8485f8bc22caa5fdc122c39aaa46253490bad173bfb6dc6c1d8f788e5e4b8e15
```

It may be used as a cross-decoder stimulus after redistribution review, but it
is not independent proof of libsstv-compatible TX. pySSTV contains limited
self-golden data, mainly Martin M1, and no multi-mode WAV set. No audited repo
contained independent AVT, MP/MR/ML, narrow-mode or HAMDRM audio vectors.

AVT documentary/source landmarks are recorded in
`tests/sstv/fixtures/avt-handbook-qsstv-landmarks.json`. It pins the Handbook
hash and the two source commits above, records the exact triple-VIS/countdown
contract, mode prefixes, geometry and the AVT90 divergences. It contains no
PCM or copied source expression. The complete AVT24 PCM exercised by tests is
generated by Decodium during the test run and therefore remains a
self-generated loopback, not an independent fixture.

Fixture metadata must record producer/version, legal redistribution status,
source image hash, audio hash, sample format/rate, claimed mode and whether the
producer shares lineage with the implementation under test.

For Scottie S3/S4, a developer-only helper compiled and executed the public
libsstv API at exactly
`rimio/libsstv@193157a993ac34bfa074074004c9ddadcfe6fd15` outside the Decodium
checkout. It encoded a constant 320x128 RGB8 image to 12 kHz mono PCM16. The
complete PCM was not imported; the compact, reproducible metadata fixture is
`tests/sstv/fixtures/libsstv-193157-scottie-s3-s4-landmarks.json`. It records:

- S3: 668,773 libsstv frames, SHA-256
  `77f7b16e5a6e24f7436e1da72f0c9f3ce8e90c9c0e2508f6efc8c0b3ad76dd7c`;
- S4: 437,562 libsstv frames, SHA-256
  `82a8dafcdd350483089d517f36d8707b622fd41886c746c1690a968a4d6c0495`;
- first green/blue, embedded-sync, red and second-line frame landmarks;
- libsstv's extra 108-frame (9 ms) sync after VIS, kept explicitly distinct
  from Decodium's Handbook-selected first-line order.

The fixture is consumed only by native tests and creates no Decodium runtime
dependency. It is concrete independent developer evidence, not proof of live
radio, on-air reception or cross-application interoperability.

For the sequential-RGB family, the pinned MIT pySSTV path at
`dnet/pySSTV@d998fad154d3e6ad2d73af5add49beec0d2ab59f` was executed outside
the Decodium runtime with mode-sized RGB images at 12 kHz. No upstream image,
PCM or Python code is copied into Decodium. The compact behavioural fixture
`tests/sstv/fixtures/pysstv-d998fad-sequential-rgb-landmarks.json` records VIS,
first-line component starts, line frames and image frames:

- SC2-120: 5,706 frames/line and 1,460,805 image frames;
- SC2-180: 8,532 frames/line and 2,184,261 image frames;
- P3: 4,912 frames/line and 2,436,600 image frames;
- P5: 7,368 frames/line and 3,654,900 image frames;
- P7: 9,825 frames/line and 4,873,200 image frames.

SC2-60 is deliberately absent because pySSTV has no implementation of it and
no second executable audited lineage was available. The landmark fixture is
independent developer timing evidence for the five listed modes, not a full
waveform oracle, live-radio result or cross-application interoperability test.

For PD, two compact fixtures were produced outside the Decodium runtime. The
pinned MIT pySSTV path at `d998fad154d3e6ad2d73af5add49beec0d2ab59f`
records cumulative first-pair starts and canonical image/frame counts for
PD90/120/160/180/240/290 in
`tests/sstv/fixtures/pysstv-d998fad-pd-landmarks.json`; PD50 is intentionally
absent because pySSTV does not implement it. The public libsstv API at
`193157a993ac34bfa074074004c9ddadcfe6fd15` was separately compiled with
`tests/sstv/tools/libsstv_pd_fixture.c` for all seven modes. Its compact fixture
is `tests/sstv/fixtures/libsstv-193157-pd-landmarks.json`. At 12 kHz the native
canonical header-plus-image counts are 607,133; 1,090,789; 1,524,156;
1,941,518; 2,255,538; 2,986,920; and 3,475,106 frames respectively.

The libsstv run agrees through each canonical end with the 20 ms sync, 2.08 ms
porch and Y-even/Cr-average/Cb-average/Y-odd order, but then emits one complete
additional pair and reads outside its input raster. Its full hashes and the
4,658/8,436/6,102/9,653/9,051/12,000/11,248-frame defective suffix lengths are
retained solely so the native test can prove that Decodium stops earlier at
`height/2`. Neither fixture ships upstream PCM or images, creates a runtime
dependency, or proves live-radio/cross-application interoperability.

For Martin M2/M3/M4, the same developer-only process compiled and executed the
public libsstv API at the exact pinned commit outside the Decodium checkout.
It encoded constant RGB8 images at the public 320-column geometry to 12 kHz
mono PCM16. The PCM was not imported; compact reproducible metadata lives in
`tests/sstv/fixtures/libsstv-193157-martin-m2-m3-m4-landmarks.json`:

- M2: 707,643 frames, SHA-256
  `04ff8c0da3db0e3c30ff81281b8720f4e77fd91bf99480298b3a6862358e378e`;
- M3: 696,661 frames, SHA-256
  `16467badd85868026f1f9b16a9363f88be154480c7946a66e9d2e7681523179a`;
- M4: 359,281 frames, SHA-256
  `64e68d8316ac8be79bb0cf65fdf96a08a4abd5c33a4070c8eecb7888024945b2`;
- first sync, green, blue, red and second-line frame landmarks, with their
  short-window discriminated tones.

The fixture is consumed only by native tests and creates no Decodium runtime
dependency. It is concrete independent developer evidence, not proof of live
radio, on-air reception or cross-application interoperability.

## HAMDRM exclusions

The following QSSTV material must not be copied:

- `src/drmrx/newfft.cpp`: restricted to research/education and no-fee use;
- `src/drmrx/lubksb.cpp`, `ludcmp.cpp` and `nrutil.*`: Numerical Recipes
  derivation without a suitable free redistribution grant;
- `src/utils/rs.cpp`: ambiguous "GNU public license" without a version;
- DRM/JP2 files with insufficient file-level provenance until authorship and
  licence are resolved.

HAMDRM therefore uses a clean-room protocol implementation and the target-
scoped OpenJPEG dependency. The audit of behaviour does not grant a right to
copy restricted source. OpenJPEG 2.5.4 is BSD-2-Clause and its complete binary
redistribution notice is installed from
`doc/THIRD_PARTY_LICENSES_OPENJPEG.md`.

## Notice and release requirements

Before merging any adaptation:

- add SPDX identifiers/copyright notices required by the source file;
- preserve ISC/MIT/0BSD text where applicable;
- add the QSSTV attribution when QSSTV expression is adapted;
- update source and binary third-party notices and packaging manifests;
- verify GPL compatibility for each file, not only the repository root;
- include generator/tool provenance for developer-only Python scripts;
- repeat the audit if an upstream revision is changed.
