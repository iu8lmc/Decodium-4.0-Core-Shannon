# Native HAMDRM compatibility matrix

Implementation snapshot: 2026-08-24. This file records executable evidence,
not planned capability. `Implemented` below means bounded native code and local
deterministic tests. It does not mean independent over-the-air interoperability.

## Feature matrix

| Layer or capability | Native status | Executed evidence | Interoperability status / remaining gate |
|---|---|---|---|
| Named profile registry | Implemented | 72 unique typed records validated by `test_hamdrm_core` | Payload table audited against the pinned QSSTV lineage; no independent waveform for all tuples. |
| CRC/integrity | Implemented | CRC-16/X-25 check value, valid/corrupt group tests | MOT data-group scope only; full channel integrity still depends on the completed waveform stack. |
| MOT header/body groups | Implemented | Encode/parse, truncation, malformed fields, size and transport mismatch tests | Clean-room QSSTV-compatible subset; independent QSSTV object exchange not yet executed. |
| Object segmentation/reassembly | Implemented | Out-of-order, exact duplicate, conflicting duplicate, missing and completion tests | No live carrier-loss exchange yet. |
| BSR generation/parsing | Implemented | Compact and extended forms, ranges, malformed and unsupported-profile tests | EasyPal behavior is a documented expectation only; no closed-source code was inspected or copied. |
| Partial persistence/resume | Implemented | Atomic write/load, checksum damage, truncation, replay and remove tests | Local AppData behavior verified on macOS only; packaged Windows/Linux storage not verified. |
| JPEG/PNG/GIF/BMP boundary checks | Implemented | Signature, container, dimension, pixel and allocation-limit tests | Image acceptance boundary only, not proof that every type is an interoperable HAMDRM payload. |
| JPEG2000 decode/encode | Implemented | Exact lossless RGB round-trip, malformed/truncated/limit tests with OpenJPEG 2.5.4 | OpenJPEG packaging and independent QSSTV JP2 exchange remain unverified. |
| QAM 4/16/64 | Implemented primitive | Mapping/demapping deterministic tests | A clean symbol round-trip is not a channel interoperability claim. |
| Narrow amateur OFDM A/B/E | Implemented primitive | FFT/DFT oracle, cyclic prefix, modem loopback and fractional-CFO/CP-sync tests | Amateur E is not advertised as current broadcast DRM E; independent waveform/vector still required. |
| FAC and MSC channel coding | Implemented pinned subset | Registered `test_hamdrm_channel`: DRM channel CRC-8/CRC-16, dispersal, K=7 convolutional coding/puncturing, terminated hard Viterbi, bit/cell interleaving, 45-cell FAC and Part-B-only 4/16/64-QAM MSC across all six A/B/amateur-E bandwidth tuples; five repeated passes | Exact narrow QSSTV-pinned subset. No SDC, MSC Part A, VSPP, hierarchical mapping or soft/CSI decoder. |
| FAC/pilot/MSC cell plan | Implemented pinned subset | Exact carrier classification and precedence, time/frequency/scattered/boosted pilots, mapper/extractor and pilot-corruption tests for A/B/amateur-E at 2.3/2.5 kHz | Cell planning is not yet a complete synchronized waveform decoder. |
| Native waveform RX backend | Implemented for the pinned native subset and wired locally | `HamDrmNativeRxBackend` consumes the existing Decodium shared PCM tap on a bounded worker; `test_hamdrm_waveform_adapters` covers loopback group delivery, queue/sample/session bounds, invalid rates, stream reset and synchronous cancellation. The Bridge installs the real hooks and the controller advertises RX only when they are connected. | The executable self-roundtrip proves the Decodium encoder/decoder pair, not independent QSSTV, RF, fading, clock-drift or sound-card interoperability. |
| Native waveform TX backend | Implemented for the pinned native subset and wired locally | `HamDrmNativeTxBackend` converts validated MOT groups to bounded 48 kHz PCM and hands a pull source to `SstvTxCoordinator`; adapter tests cover source bounds, played-audio completion, cancellation, configured FAC identity and the shared PTT release barrier. The Bridge supplies the existing output/CAT/PTT hooks. | No independently captured QSSTV waveform, physical output-level, CAT/radio or on-air exchange has been demonstrated. |
| Native Decodium QML page | Implemented, capability-gated | `test_hamdrm_controller`, `test_hamdrm_waveform_adapters` and offscreen `test_sstv_digital_qml` pass; the full `decodium_qml` target compiles and links with the Bridge-owned native RX/TX backends | The page exposes named profiles, real connected waveform capability, object/missing/BSR/resume state and cancellation. Packaged-platform rendering and live station checks remain. |
| QSSTV interoperability | Not verified | None | Execute legally obtained independent waveform/object vectors in both directions. |
| EasyPal interoperability | Not verified | None | Test documented compact BSR/object expectations with legally obtained files or behavior; do not inspect proprietary code. |
| KG-STV | Not implemented | None | HAMDRM is not KG-STV. An authoritative public specification and independent vectors have not been established. |

## Profile-space status

The registry contains A/B/amateur-E x 2.3/2.5 kHz x high/normal protection x
4/16/64-QAM x long/short interleaving, for 72 stable named tuples. Registry
presence alone is not an RX/TX capability claim. The native adapters execute
the registered subset through Decodium's shared audio and TX authority, but no
tuple is marked independently interoperable because QSSTV vectors and an
external RF/application exchange are not yet available.

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
