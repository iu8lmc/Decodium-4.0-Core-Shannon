# Native HAMDRM compatibility matrix

Evidence updated through 2026-11-01. This file records executable evidence,
not planned capability. `Implemented` below means bounded native code and local
deterministic tests. It does not mean independent over-the-air interoperability.

## Feature matrix

| Layer or capability | Native status | Executed evidence | Interoperability status / remaining gate |
|---|---|---|---|
| Named profile registry | Implemented | 72 unique typed records validated by `test_hamdrm_core` | Payload table audited against the pinned QSSTV lineage; no independent waveform for all tuples. |
| CRC/integrity | Implemented | MSB-first CRC validated against executed QSSTV MOT groups and corruption tests | Old reflected-CRC local snapshots are recovered in memory after file checksum verification; network groups use the correct CRC. |
| MOT header/body groups | Implemented | Separately executed QSSTV single- and three-segment vectors parse and assemble, including duplicate and out-of-order groups; Decodium output is compared at byte/segment level. See [oracle](HAMDRM_QSSTV_ORACLE.md). | QSSTV receiver, real image and waveform exchange remain untested. |
| Object segmentation/reassembly | Implemented | Out-of-order, exact duplicate, conflicting duplicate, missing and completion tests | No live carrier-loss exchange yet. |
| BSR generation/parsing and transport | Implemented | Compact and extended forms were compared against extracted pinned QSSTV methods. Two Decodium applications transmitted A/B images, exchanged both BSR dialects and retransmitted requested blocks over virtual audio; one decoded segment was deliberately discarded. See the oracle and architecture. | An external application exchange, actual carrier loss, EasyPal and RF remain unverified. |
| Reed–Solomon RS1–RS4 | Implemented, bounded | Encoded containers match pinned QSSTV methods for two inputs at each level. Two Decodium applications exchanged A/B fixtures for every level; additional RS4 tests recovered injected missing segments. Modified QSSTV TX supplied RS4 A/B JP2 recordings received exactly by Decodium over virtual audio. | CPU work limits reject some large objects. Broader profiles, native EasyPal, real radio and other operating systems remain unverified. |
| Partial persistence/resume | Implemented | Atomic write/load, checksum damage, truncation, replay and remove tests | Local AppData behavior verified on macOS only; packaged Windows/Linux storage not verified. |
| JPEG/PNG/GIF/BMP boundary checks | Implemented | Signature, container, dimension, pixel and allocation-limit tests | Image acceptance boundary only, not proof that every type is an interoperable HAMDRM payload. |
| JPEG2000 decode/encode | Implemented | Exact lossless RGB round-trip, malformed/truncated/limit tests with OpenJPEG 2.5.4; modified QSSTV TX supplied RS4 JP2 recordings recovered by Decodium | Packaged OpenJPEG dependencies, broad JP2 variants and unmodified live QSSTV JP2 exchange remain unverified. |
| QAM 4/16/64 | Implemented primitive | Mapping/demapping deterministic tests | A clean symbol round-trip is not a channel interoperability claim. |
| Narrow amateur OFDM A/B/E | Implemented primitive | FFT/DFT oracle, cyclic prefix, modem loopback and fractional-CFO/CP-sync tests; 12 complete complex symbols from the independent QSSTV OFDM stage match and demodulate | Amateur E is not broadcast DRM E; complete external waveform and receiver exchange still required. |
| FAC and MSC channel coding | Implemented pinned subset | Registered `test_hamdrm_channel`: DRM channel CRC-8/CRC-16, dispersal, K=7 convolutional coding/puncturing, hard Viterbi and a CRC-gated soft MSC retry, bit/cell interleaving, 45-cell FAC and Part-B-only 4/16/64-QAM MSC across all six A/B/amateur-E bandwidth tuples; 288 QSSTV FAC payload/QAM-sign combinations and 36 full QSSTV MSC MLC cell vectors match, plus standalone convolution/interleaver vectors. A lab-only adaptive path tests CRC-gated flat-packet and iterative multistage retries; see the oracle. | Exact narrow QSSTV-pinned subset. The adaptive and multistage retries are lab-only; no SDC, MSC Part A, VSPP, hierarchical mapping or channel-state-weighted decoder. |
| FAC/pilot/MSC cell plan | Implemented pinned subset | Exact carrier classification and precedence, time/frequency/scattered/boosted pilots, and A/B symbol mapping compared with executable QSSTV for complete superframes at all six profiles | Cell planning is not yet a complete synchronized waveform decoder. |
| Native waveform RX backend | Implemented for the pinned native subset and wired locally | `HamDrmNativeRxBackend` consumes the existing Decodium shared PCM tap on a bounded worker; `test_hamdrm_waveform_adapters` covers loopback group delivery, queue/sample/session bounds, invalid rates, stream reset and synchronous cancellation. The Bridge installs the real hooks and the controller advertises RX only when they are connected. | QSSTV-generated MOT/packet/FAC/MSC/interleaver/mapper/OFDM PCM decodes and assembles two exact 16x16 JPEGs, with pixel checks; A separate playback process delivered two QSSTV-stage JPEG PCM fixtures through BlackHole into the full Decodium app for E/2.5 kHz/16-QAM/high/short, byte-exact after repetitions; full QSSTV application, RF, fading and clock-drift interoperability remain unverified. |
| Native waveform TX backend | Implemented for the pinned native subset and wired locally | `HamDrmNativeTxBackend` converts validated MOT groups to bounded 48 kHz PCM and hands a pull source to `SstvTxCoordinator`; adapter tests cover source bounds, played-audio completion, cancellation, configured FAC identity and the shared PTT release barrier. The Bridge supplies the existing output/CAT/PTT hooks. | For two nine-frame JPEG sequences, native TX samples match independently generated QSSTV-stage PCM within signed-16-bit quantization; QSSTV FAC and MSC receiver stages decoded native vectors, and its actual receive map recovered native interleaved frames across all six profiles and both depths; full QSSTV RX, physical output, CAT/radio and on-air exchange remain unverified. |
| Native Decodium QML page | Implemented, capability-gated | `test_hamdrm_controller`, `test_hamdrm_waveform_adapters` and offscreen `test_sstv_digital_qml` pass; the full `decodium_qml` target compiles and links with the Bridge-owned native RX/TX backends | The page exposes named profiles, real connected waveform capability, object/missing/BSR/resume state and cancellation. Packaged-platform rendering and live station checks remain. |
| QSSTV interoperability | Partial evidence | File-mediated A/B in both directions for E/2.5/high/16-QAM/long, A/2.3/high/4-QAM/long and B/2.5/high/16-QAM with short and long interleaving. Modified QSSTV TX supplied additional RS4 and 64-QAM A/B recordings. Official QSSTV TX A/B B/2.3/normal/64-QAM/long PCM was recovered from timed-out WAVs by correcting only their RIFF lengths, then decoded byte-exactly by QSSTV and Decodium. Its unmodified receiver also recovered Decodium JPEGs, including A/2.5/normal/16-QAM short and long recordings with a constructed echo. See the oracle. | Other profiles, clean official QSSTV TX completion, EasyPal and RF remain unverified. |
| EasyPal interoperability | Partial recorded-signal evidence | Two public recordings labelled EasyPal were decoded by QSSTV. After the packet-tail correction, ordinary Decodium RX reconstructed the second object's pixels, with a 199-byte zero-padding difference from QSSTV. The first recording reconstructed byte-exactly only with experimental adaptive equalization and diagnostic pilot thresholds. Source versions and transmission paths are unknown. See the oracle. | Ordinary reception of the first recording, independently verified EasyPal versions, live application exchange, compact BSR behavior and RF remain unverified; do not inspect proprietary code. |
| KG-STV | Not implemented | None | HAMDRM is not KG-STV. An authoritative public specification and independent vectors have not been established. |

An archived Windows installer named `EasyPal-07-OCT-2014-Setup.exe` was
downloaded from the [QSL.net public archive](https://qsl.net/lw7dfm/Software/SSTV/)
for later independent testing. Its SHA-256 is
`7190ef73f643d60afa5f064fa238b6055e48f5cd18fbc08cd861ac55cb8d4763`;
`file` identifies a 32-bit Windows GUI executable. The archive is a third-party
mirror, not proof of publisher authenticity. It has not been installed or
executed, and no EasyPal compatibility result follows from its availability.

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
