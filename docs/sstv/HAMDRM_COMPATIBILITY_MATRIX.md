# Native HAMDRM compatibility matrix

Implementation snapshot: 2026-08-24. This file records executable evidence,
not planned capability. `Implemented` below means bounded native code and local
deterministic tests. It does not mean independent over-the-air interoperability.

## Feature matrix

| Layer or capability | Native status | Executed evidence | Interoperability status / remaining gate |
|---|---|---|---|
| Named profile registry | Implemented | 72 unique typed records validated by `test_hamdrm_core` | Payload table audited against the pinned QSSTV lineage; no independent waveform for all tuples. |
| CRC/integrity | Implemented | MSB-first CRC validated against executed QSSTV MOT groups and corruption tests | Old reflected-CRC local snapshots are recovered in memory after file checksum verification; network groups use the correct CRC. |
| MOT header/body groups | Implemented | Separately executed QSSTV single- and three-segment vectors parse and assemble, including duplicate and out-of-order groups; Decodium output is compared at byte/segment level. See [oracle](HAMDRM_QSSTV_ORACLE.md). | QSSTV receiver, real image and waveform exchange remain untested. |
| Object segmentation/reassembly | Implemented | Out-of-order, exact duplicate, conflicting duplicate, missing and completion tests | No live carrier-loss exchange yet. |
| BSR generation/parsing and transport | Implemented | Compact and extended forms; 12 cases compared against extracted pinned QSSTV methods. Separate Decodium applications exchanged A/B BSR objects over virtual audio. See the oracle and architecture. | BSR-triggered block retransmission, an external application exchange, EasyPal and RF remain unverified. |
| Partial persistence/resume | Implemented | Atomic write/load, checksum damage, truncation, replay and remove tests | Local AppData behavior verified on macOS only; packaged Windows/Linux storage not verified. |
| JPEG/PNG/GIF/BMP boundary checks | Implemented | Signature, container, dimension, pixel and allocation-limit tests | Image acceptance boundary only, not proof that every type is an interoperable HAMDRM payload. |
| JPEG2000 decode/encode | Implemented | Exact lossless RGB round-trip, malformed/truncated/limit tests with OpenJPEG 2.5.4 | OpenJPEG packaging and independent QSSTV JP2 exchange remain unverified. |
| QAM 4/16/64 | Implemented primitive | Mapping/demapping deterministic tests | A clean symbol round-trip is not a channel interoperability claim. |
| Narrow amateur OFDM A/B/E | Implemented primitive | FFT/DFT oracle, cyclic prefix, modem loopback and fractional-CFO/CP-sync tests; 12 complete complex symbols from the independent QSSTV OFDM stage match and demodulate | Amateur E is not broadcast DRM E; complete external waveform and receiver exchange still required. |
| FAC and MSC channel coding | Implemented pinned subset | Registered `test_hamdrm_channel`: DRM channel CRC-8/CRC-16, dispersal, K=7 convolutional coding/puncturing, terminated hard Viterbi, bit/cell interleaving, 45-cell FAC and Part-B-only 4/16/64-QAM MSC across all six A/B/amateur-E bandwidth tuples; 288 QSSTV FAC payload/QAM-sign combinations and 36 full QSSTV MSC MLC cell vectors match, plus standalone convolution/interleaver vectors | Exact narrow QSSTV-pinned subset. No SDC, MSC Part A, VSPP, hierarchical mapping or soft/CSI decoder. |
| FAC/pilot/MSC cell plan | Implemented pinned subset | Exact carrier classification and precedence, time/frequency/scattered/boosted pilots, and A/B symbol mapping compared with executable QSSTV for complete superframes at all six profiles | Cell planning is not yet a complete synchronized waveform decoder. |
| Native waveform RX backend | Implemented for the pinned native subset and wired locally | `HamDrmNativeRxBackend` consumes the existing Decodium shared PCM tap on a bounded worker; `test_hamdrm_waveform_adapters` covers loopback group delivery, queue/sample/session bounds, invalid rates, stream reset and synchronous cancellation. The Bridge installs the real hooks and the controller advertises RX only when they are connected. | QSSTV-generated MOT/packet/FAC/MSC/interleaver/mapper/OFDM PCM decodes and assembles two exact 16x16 JPEGs, with pixel checks; A separate playback process delivered two QSSTV-stage JPEG PCM fixtures through BlackHole into the full Decodium app for E/2.5 kHz/16-QAM/high/short, byte-exact after repetitions; full QSSTV application, RF, fading and clock-drift interoperability remain unverified. |
| Native waveform TX backend | Implemented for the pinned native subset and wired locally | `HamDrmNativeTxBackend` converts validated MOT groups to bounded 48 kHz PCM and hands a pull source to `SstvTxCoordinator`; adapter tests cover source bounds, played-audio completion, cancellation, configured FAC identity and the shared PTT release barrier. The Bridge supplies the existing output/CAT/PTT hooks. | For two nine-frame JPEG sequences, native TX samples match independently generated QSSTV-stage PCM within signed-16-bit quantization; QSSTV FAC and MSC receiver stages decoded native vectors, and its actual receive map recovered native interleaved frames across all six profiles and both depths; full QSSTV RX, physical output, CAT/radio and on-air exchange remain unverified. |
| Native Decodium QML page | Implemented, capability-gated | `test_hamdrm_controller`, `test_hamdrm_waveform_adapters` and offscreen `test_sstv_digital_qml` pass; the full `decodium_qml` target compiles and links with the Bridge-owned native RX/TX backends | The page exposes named profiles, real connected waveform capability, object/missing/BSR/resume state and cancellation. Packaged-platform rendering and live station checks remain. |
| QSSTV interoperability | Partial evidence | File-mediated A/B in both directions for E/2.5/high/16-QAM/long, A/2.3/high/4-QAM/long and B/2.5/high/16-QAM with short and long interleaving: a modified QSSTV-Experimental TX produced exact JP2 images in Decodium; its unmodified receiver recovered exact Decodium JPEGs. See the oracle. | Other profiles, unmodified QSSTV TX, EasyPal and RF remain unverified. |
| EasyPal interoperability | Not verified | None | Test documented compact BSR/object expectations with legally obtained files or behavior; do not inspect proprietary code. |
| KG-STV | Not implemented | None | HAMDRM is not KG-STV. An authoritative public specification and independent vectors have not been established. |

## Profile-space status

The registry contains A/B/amateur-E x 2.3/2.5 kHz x high/normal protection x
4/16/64-QAM x long/short interleaving, for 72 stable named tuples. Registry
presence alone is not an RX/TX capability claim. The native adapters execute
the registered subset through Decodium's shared audio and TX authority, but no
tuple is marked independently interoperable because broad profile coverage and external RF exchange remain unverified; one QSSTV-Experimental application TX profile has file-mediated A/B evidence.

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
