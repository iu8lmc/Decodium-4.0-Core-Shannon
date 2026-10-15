# Decodium native SSTV compatibility matrix

Canonical registry-synchronised matrix, implementation snapshot 2026-08-24.
Dimensions, nominal picture duration, VIS and native capability cells are
emitted by the checked-in `generate_sstv_mode_matrix` developer tool and
verified against every registry entry by `test_sstv_mode_docs`; evidence and
external-interoperability notes remain deliberately reviewed text.

The starting commit had no SSTV code. The current checkout has native,
deterministically tested RX/TX/automatic-VIS paths for Martin M1/M2/M3/M4 and
Scottie S1/S2/S3/S4/DX, plus Robot C12/C24/C36/C72 and B/W 8/12/24/36.
The native catalogue also includes Wraase SC2-60/120/180, Pasokon P3/P5/P7,
PD50/90/120/160/180/240/290, AVT24/90/94 normal and all nineteen required
MMSSTV wide/narrow extended modes. Those fifty-two rows remain **implemented,
not live/cross-application verified**.  Three common rows additionally pass an
independent encoder-to-native-decoder WAV replay described below.
Martin M2/M3/M4,
Scottie S3/S4 and all eight Robot modes have compact landmarks produced by a
separately executed, exactly pinned libsstv encoder. Five Sequential RGB modes
have timing and component-start landmarks from pinned pySSTV `d998fad`;
SC2-60 has deterministic native/self-generated evidence only. PD90-PD290 use
the independent pySSTV timing landmarks and every PD mode uses the compatible
prefix of a separately executed libsstv path; its known extra-pair/OOB suffix
is explicitly rejected. This is
developer evidence, not a captured on-air waveform or cross-application
interoperability result. For Robot colour the fixture records an incompatible
upstream layout/timing path instead of pretending it is an on-air oracle. The
other four implemented Martin/Scottie modes also have self-generated fixtures
only. AVT has a pinned Handbook/source-landmark fixture plus deterministic
native PCM loopback, but no independent recording. The MMSSTV fixture records exact landmarks audited from the pinned source
mirror and QSSTV, but it contains no PCM and is not an independent live or
cross-application vector. Every other catalogue row remains unavailable or
explicitly blocked.

The optional hash-pinned PySSTV `d998fad` WAV pack was replayed through
`SstvWavReplayController` and the production RX runtime at 12 kHz.  Robot 36
completed at 320x240 with 18.024 dB PSNR, Robot B/W 8 completed at 160x120
with 18.661 dB, and Martin M2 completed at the native 320x256 wire/display
raster with 24.250 dB.  All three had valid mapped VIS, coverage 1.0, no
ingress drops and no processing failures.  This is independent waveform
evidence, but still not an RF, sound-card, another-decoder or on-air result.

The Martin, Scottie, Robot, Sequential RGB, PD, AVT and MMSSTV encoders, RX runtime,
TX coordinator/source builder, WAV export and Studio mode descriptors are all
in-process Decodium components. Their exact mode IDs,
160x120/320x120/320x128/320x240/320x256/512x400/640x496/800x616 preparation geometries and
protocol frame boundaries are covered by coordinator, WAV and Studio tests.

Legend: `impl.` means native implementation plus deterministic tests, without a
release/interoperability claim; `—` means unavailable/unimplemented; `blocked`
means a known protocol or evidence conflict must be resolved. Dimensions,
picture-scan duration and VIS are generated from the canonical C++ registry by
the `generate_sstv_mode_matrix` developer target. The duration is the registry's
image duration and therefore excludes leader/VIS/countdown framing. The
`test_sstv_mode_docs` gate checks every one of the 64 rows and every generated
cell. `unverified` in either external-application column is deliberate: pinned
source landmarks or a native replay are not a QSSTV, Robot36 or SlowRX
interoperability run.

| Mode | Registry ID | Family/class | Dimensions | Nominal duration (s) | VIS | RX | TX | Auto detect | QSSTV | Robot36 / SlowRX | Registry interoperability | Evidence status | Independent Decodium vector/test | Current blocker or next proof |
|---|---|---|---:|---:|---|---:|---:|---:|---|---|---|---|---|---|
| Martin M1 | `martin-m1` | Martin/analog | 320×256 | 114.290176 | standard 44 | impl. | impl. | impl. | unverified | unverified | not tested | deterministic tests | deterministic native/self-generated tests only | Obtain and verify an independent waveform plus interoperability/on-air evidence. |
| Martin M2 | `martin-m2` | Martin/analog | 320×256 (effective 160×256) | 58.060288 | standard 40 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned PySSTV `d998fad` 12 kHz WAV replay through native runtime, coverage 1.0 and 24.250 dB PSNR; pinned libsstv landmarks | Obtain another-decoder/RF/on-air evidence; 320 wire/display columns remain distinct from effective width 160. |
| Martin M3 | `martin-m3` | Martin/analog | 320×128 | 57.145088 | standard 36 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned libsstv frame count, SHA-256 and timing landmarks; native deterministic coordinator/WAV/Studio tests | Handbook/libsstv resolve SlowRX's inconsistent pixel field, but live interoperability remains unverified. |
| Martin M4 | `martin-m4` | Martin/analog | 320×128 (effective 160×128) | 29.030144 | standard 32 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned libsstv frame count, SHA-256 and timing landmarks; native deterministic coordinator/WAV/Studio tests | Obtain cross-application/on-air evidence. |
| Scottie S1 | `scottie-s1` | Scottie/analog | 320×256 | 109.624320 | standard 60 | impl. | impl. | impl. | unverified | unverified | not tested | deterministic tests | deterministic native/self-generated tests only | Verify first-line ordering with an independent captured waveform and interoperability evidence. |
| Scottie S2 | `scottie-s2` | Scottie/analog | 320×256 | 71.089152 | standard 56 | impl. | impl. | impl. | unverified | unverified | not tested | deterministic tests | deterministic native/self-generated tests only | Verify the audited 320-pixel geometry with an independent waveform/interoperability result. |
| Scottie DX | `scottie-dx` | Scottie/analog | 320×256 | 268.876800 | standard 76 | impl. | impl. | impl. | unverified | unverified | not tested | deterministic tests | deterministic native/self-generated tests only | Obtain an independent long-duration waveform plus interoperability/on-air evidence. |
| Scottie S3 | `scottie-s3` | Scottie/analog | 320×128 | 54.812160 | standard 52 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned libsstv frame counts, SHA-256 and timing landmarks; native deterministic tests | Verify the documented first-line-order difference with a captured cross-application/on-air waveform. |
| Scottie S4 | `scottie-s4` | Scottie/analog | 320×128 (effective 160×128) | 35.544576 | standard 48 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned libsstv frame counts, SHA-256 and timing landmarks; native deterministic tests | Verify 320 wire/display columns versus effective sampled width 160 with cross-application/on-air evidence. |
| Robot 12 Colour | `robot-c12` | Robot colour/analog | 160×120 | 12.000000 | standard 0 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned libsstv geometry/frame/hash landmarks plus native 160x120 4:2:0 deterministic tests | Pinned libsstv uses a conflicting colour layout/timing path; obtain a Handbook-compatible cross-application/on-air capture. |
| Robot 24 Colour | `robot-c24` | Robot colour/analog | 320×120 | 24.000000 | standard 4 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned libsstv geometry/frame/hash landmarks plus native 320x120 4:2:2 deterministic tests | QSSTV's 160-column declaration and libsstv colour timing diverge from the Handbook-selected 320x120 path; live proof remains pending. |
| Robot 36 Colour | `robot-c36` | Robot colour/analog | 320×240 | 36.000000 | standard 8 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned PySSTV `d998fad` 12 kHz WAV replay through native runtime, coverage 1.0 and 18.024 dB PSNR; libsstv landmarks | Obtain another-decoder/RF/on-air evidence; PySSTV's compatible structural partition remains distinct from the Handbook-selected TX partition. |
| Robot 72 Colour | `robot-c72` | Robot colour/analog | 320×240 | 72.000000 | standard 12 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned libsstv geometry/frame/hash landmarks plus native 320x240 4:2:2 tests | Verify the Handbook structural timing and both half-width chroma scans with an independent compatible capture. |
| Robot B/W 8 | `robot-bw8` | Robot monochrome/analog | 160×120 | 7.920000 | standard 2 (aliases 1,3) | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned PySSTV `d998fad` 7+60 ms 12 kHz WAV replay through native runtime, coverage 1.0 and 18.661 dB PSNR; canonical 10+56 ms and VIS-alias tests | Obtain another-decoder/RF/on-air evidence and verify all documented colour-channel VIS aliases. |
| Robot B/W 12 | `robot-bw12` | Robot monochrome/analog | 160×120 | 12.000000 | standard 6 (aliases 5,7) | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned libsstv frame/hash/timing landmarks; native canonical VIS plus deterministic 5/6/7 alias mapping tests | Obtain cross-application/on-air evidence for all documented colour-channel VIS aliases. |
| Robot B/W 24 | `robot-bw24` | Robot monochrome/analog | 320×240 | 25.200000 | standard 10 (aliases 9,11) | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned libsstv frame/hash/timing landmarks; native 105 ms structural line and 9/10/11 alias tests | Confirm the retained 12 ms sync + 93 ms scan against a real decoder despite the historical 24-second/600-lpm naming conflict. |
| Robot B/W 36 | `robot-bw36` | Robot monochrome/analog | 320×240 | 36.000000 | standard 14 (aliases 13,15) | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned libsstv frame/hash/timing landmarks; native canonical VIS plus deterministic 13/14/15 alias mapping tests | Obtain cross-application/on-air evidence for all documented colour-channel VIS aliases. |
| Wraase SC2-60 | `wraase-sc2-60` | Wraase/analog | 320×256 (effective 256×256) | 61.543500 | standard 59 | impl. | impl. | impl. | unverified | unverified | not tested | deterministic tests | native 320x256, 61.543500 s picture-scan deterministic/self-generated tests only | Decodium explicitly selects QSSTV's RX-side 61.5435 s image profile; QSSTV's TX-side gaps/scan timing differ, so no TX-waveform equivalence is claimed. Obtain a second executable lineage or independent waveform, then cross-application/on-air evidence. |
| Wraase SC2-120 | `wraase-sc2-120` | Wraase/analog | 320×256 | 121.733760 | standard 63 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned pySSTV `d998fad` timing/component-start landmarks; native 320x256, 121.733760 s picture-scan deterministic tests | The coherent equal-RGB 475.5225 ms profile is selected without mixing in the Handbook's conflicting rounded 2:4:2 fields; live/cross-application proof remains pending. |
| Wraase SC2-180 | `wraase-sc2-180` | Wraase/analog | 320×256 (effective 512×256) | 182.021760 | standard 55 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned pySSTV `d998fad` timing/component-start landmarks; native 320x256, 182.021760 s picture-scan deterministic tests | The coherent 711.0225 ms line is selected and SlowRX's inconsistent pixel field excluded; live/cross-application proof remains pending. |
| Pasokon P3 | `pasokon-p3` | Pasokon/analog | 640×496 (effective 320×496) | 203.050000 | standard 113 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned pySSTV `d998fad` timing/component-start landmarks; native 640x496 display, effective width 320, 203.050000 s picture-scan deterministic tests | Obtain cross-application/on-air evidence for the exact Pasokon time-unit and RGB ordering path. The unspecified upper-16-line calibration pattern is not synthesized. |
| Pasokon P5 | `pasokon-p5` | Pasokon/analog | 640×496 | 304.575000 | standard 114 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned pySSTV `d998fad` timing/component-start landmarks; native 640x496, 304.575000 s picture-scan deterministic tests | Obtain cross-application/on-air evidence for the exact Pasokon time-unit and RGB ordering path. The unspecified upper-16-line calibration pattern is not synthesized. |
| Pasokon P7 | `pasokon-p7` | Pasokon/analog | 640×496 | 406.100000 | standard 115 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned pySSTV `d998fad` timing/component-start landmarks; native 640x496, 406.100000 s picture-scan deterministic tests | Obtain cross-application/on-air evidence for the exact Pasokon time-unit and RGB ordering path. The unspecified upper-16-line calibration pattern is not synthesized. |
| PD50 | `pd-50` | PD/analog | 320×256 | 49.684480 | standard 93 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned libsstv pre-defect timing/frame landmarks; native exact 320x256/128-pair tests | pySSTV has no PD50. Obtain compatible cross-application/on-air evidence; the libsstv extra pair/OOB suffix is not an oracle and is never emitted. |
| PD90 | `pd-90` | PD/analog | 320×256 | 89.989120 | standard 99 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned pySSTV component landmarks plus libsstv pre-defect frames; native 320x256 tests | Obtain live/cross-application proof for the exact 532 us pixel and vertical chroma average. |
| PD120 | `pd-120` | PD/analog | 640×496 | 126.103040 | standard 95 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned pySSTV component landmarks plus libsstv pre-defect frames; native 640x496 tests | Verify all 248 transmitted pairs, including calibration rows, with a compatible captured waveform. |
| PD160 | `pd-160` | PD/analog | 512×400 | 160.883200 | standard 98 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned pySSTV component landmarks plus libsstv pre-defect frames; native 512x400 tests | Live proof remains pending for the selected coherent 195.584 ms component/804.416 ms pair; the conflicting Handbook typo is excluded. |
| PD180 | `pd-180` | PD/analog | 640×496 | 187.051520 | standard 96 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned pySSTV component landmarks plus libsstv pre-defect frames; native 640x496 tests | Obtain cross-application/on-air evidence; the repository sample WAV and generated upstream path are not independent live proof. |
| PD240 | `pd-240` | PD/analog | 640×496 | 248.000000 | standard 97 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned pySSTV component landmarks plus libsstv pre-defect frames; native exact 1 s pair and bounded-stream tests | Measure live long-duration AFC/slant and cross-application compatibility. |
| PD290 | `pd-290` | PD/analog | 800×616 | 288.682240 | standard 94 | impl. | impl. | impl. | unverified | unverified | upstream path observed | independent vector | pinned pySSTV component landmarks plus libsstv pre-defect frames; native 800x616/308-pair bounded-stream tests | Measure live long-duration AFC/slant and cross-application compatibility. |
| AVT24 | `avt-24` | AVT/analog | 128×120 | 22.500000 | standard 64 | impl. | impl. | impl. | unverified | unverified | not tested | deterministic tests | pinned Handbook/QSSTV/MMSSTV source-landmark fixture; exact triple-VIS/countdown tests; full native PCM-to-demodulator/runtime 128x120 loopback | Obtain independently generated PCM plus cross-application/on-air evidence; deterministic self-loopback is not interoperability proof. |
| AVT90 | `avt-90` | AVT/analog | 320×240 (effective 256×240) | 90.000000 | standard 68 | impl. | impl. | impl. | unverified | unverified | not tested | deterministic tests | pinned source landmarks; native prefix/runtime, countdown-prefix `101`, coordinator/WAV/Studio and 320-wire/256-effective geometry tests | Execute an independent waveform to confirm the selected 320 prepared/wire versus 256 effective interpretation; MMSSTV's erroneous `010` prefix is excluded. |
| AVT94 | `avt-94` | AVT/analog | 320×200 | 93.750000 | standard 72 | impl. | impl. | impl. | unverified | unverified | not tested | deterministic tests | pinned source landmarks; native prefix/runtime, countdown-prefix `011`, coordinator/WAV/Studio and exact-duration tests | Obtain independent PCM plus cross-application/on-air proof for the continuous no-line-sync 320x200 scan. |
| AVT24 Narrow | `avt-24-narrow` | AVT variant/analog | — | — | standard 65 | — | — | — | not available | not available | not tested | audited sources | standard VIS 65 source landmark only | Complete Narrow countdown/carrier and picture semantics require independent evidence before implementation. |
| AVT24 QRM | `avt-24-qrm` | AVT variant/analog | — | — | standard 66 | — | — | — | not available | not available | not tested | audited sources | standard VIS 66 source landmark only | Complete QRM waveform/picture semantics require independent evidence before implementation. |
| AVT24 Narrow QRM | `avt-24-narrow-qrm` | AVT variant/analog | — | — | standard 67 | — | — | — | not available | not available | not tested | audited sources | standard VIS 67 source landmark only | Complete combined Narrow-QRM semantics require independent evidence before implementation. |
| AVT90 Narrow | `avt-90-narrow` | AVT variant/analog | — | — | standard 69 | — | — | — | not available | not available | not tested | audited sources | standard VIS 69 source landmark only | Complete Narrow AVT90 picture semantics and the 320/256 relationship require independent evidence. |
| AVT90 QRM | `avt-90-qrm` | AVT variant/analog | — | — | standard 70 | — | — | — | not available | not available | not tested | audited sources | standard VIS 70 source landmark only | Complete QRM waveform/picture semantics require independent evidence before implementation. |
| AVT90 Narrow QRM | `avt-90-narrow-qrm` | AVT variant/analog | — | — | standard 71 | — | — | — | not available | not available | not tested | audited sources | standard VIS 71 source landmark only | Complete combined Narrow-QRM semantics require independent evidence before implementation. |
| AVT94 Narrow | `avt-94-narrow` | AVT variant/analog | — | — | standard 73 | — | — | — | not available | not available | not tested | audited sources | standard VIS 73 source landmark only | Complete Narrow countdown/carrier and picture semantics require independent evidence before implementation. |
| AVT94 QRM | `avt-94-qrm` | AVT variant/analog | — | — | standard 74 | — | — | — | not available | not available | not tested | audited sources | standard VIS 74 source landmark only | Complete QRM waveform/picture semantics require independent evidence before implementation. |
| AVT94 Narrow QRM | `avt-94-narrow-qrm` | AVT variant/analog | — | — | standard 75 | — | — | — | not available | not available | not tested | audited sources | standard VIS 75 source landmark only | Complete combined Narrow-QRM semantics require independent evidence before implementation. |
| MP73 | `mp-73` | MMSSTV extended/analog | 320×256 | 72.960000 | extended VIS 0x23/0x25 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native core/runtime/coordinator/WAV/Studio tests | Obtain independently generated PCM plus cross-application/on-air evidence for paired Y/Cr/Cb/Y and raw `0x25`. |
| MP115 | `mp-115` | MMSSTV extended/analog | 320×256 | 115.456000 | extended VIS 0x23/0x29 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native deterministic tests | Obtain independently generated PCM plus cross-application/on-air evidence. |
| MP140 | `mp-140` | MMSSTV extended/analog | 320×256 | 139.520000 | extended VIS 0x23/0x2a | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native deterministic tests | Obtain independently generated PCM plus cross-application/on-air evidence. |
| MP175 | `mp-175` | MMSSTV extended/analog | 320×256 | 175.360000 | extended VIS 0x23/0x2c | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native deterministic tests | Obtain independently generated PCM plus cross-application/on-air evidence. |
| MR73 | `mr-73` | MMSSTV extended/analog | 320×256 | 73.292800 | extended VIS 0x23/0x45 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native deterministic Y/Cr-half/Cb-half tests | Obtain independently generated PCM plus cross-application/on-air evidence. |
| MR90 | `mr-90` | MMSSTV extended/analog | 320×256 | 90.188800 | extended VIS 0x23/0x46 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native deterministic tests | Obtain independently generated PCM plus cross-application/on-air evidence. |
| MR115 | `mr-115` | MMSSTV extended/analog | 320×256 | 115.276800 | extended VIS 0x23/0x49 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native deterministic tests | Obtain independently generated PCM plus cross-application/on-air evidence. |
| MR140 | `mr-140` | MMSSTV extended/analog | 320×256 | 140.364800 | extended VIS 0x23/0x4a | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native raw-`0x4A` tests | Obtain independently generated PCM plus cross-application/on-air evidence. |
| MR175 | `mr-175` | MMSSTV extended/analog | 320×256 | 175.180800 | extended VIS 0x23/0x4c | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | original MMSSTV transmitter/`mode.txt` and Handbook agree on raw `0x4C`; native negative test rejects QSSTV's duplicate `0x4A` mapping | Execute an independent MR175 waveform; the resolved QSSTV typo still precludes a verified interoperability claim. |
| ML180 | `ml-180` | MMSSTV extended/analog | 640×496 | 180.196800 | extended VIS 0x23/0x05 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native bounded 640x496 tests | Obtain independently generated PCM plus cross-application/on-air evidence. |
| ML240 | `ml-240` | MMSSTV extended/analog | 640×496 | 239.716800 | extended VIS 0x23/0x06 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native bounded 640x496 tests | Obtain independently generated PCM plus cross-application/on-air evidence. |
| ML280 | `ml-280` | MMSSTV extended/analog | 640×496 | 280.388800 | extended VIS 0x23/0x09 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native bounded 640x496 tests | Obtain independently generated PCM plus cross-application/on-air evidence. |
| ML320 | `ml-320` | MMSSTV extended/analog | 640×496 | 320.068800 | extended VIS 0x23/0x0a | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV `8060b5` source-landmark fixture; native bounded 640x496 tests | Obtain independently generated PCM plus cross-application/on-air evidence. |
| MP73-Narrow | `mp-73-narrow` | MMSSTV narrow/analog | 320×256 | 72.960000 | N-VIS 0x2d/0x15/0x02/0x17 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned canonical MMSSTV payload `0x02`; native N-VIS detector/runtime and paired-line tests | MP73-Narrow is source-mapped, not invented; obtain an independent N-VIS PCM/cross-application capture. |
| MP110-Narrow | `mp-110-narrow` | MMSSTV narrow/analog | 320×256 | 109.824000 | N-VIS 0x2d/0x15/0x04/0x11 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV payload `0x04`; native N-VIS detector/runtime and paired-line tests | Obtain independent N-VIS PCM plus cross-application/on-air evidence. |
| MP140-Narrow | `mp-140-narrow` | MMSSTV narrow/analog | 320×256 | 139.520000 | N-VIS 0x2d/0x15/0x05/0x10 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV payload `0x05`; native N-VIS detector/runtime and paired-line tests | Obtain independent N-VIS PCM plus cross-application/on-air evidence. |
| MC110-Narrow | `mc-110-narrow` | MMSSTV narrow/analog | 320×256 | 109.696000 | N-VIS 0x2d/0x15/0x14/0x01 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned executable 140 ms timing and payload `0x14`; native sequential-RGB/N-VIS tests | Execute independent PCM to validate the selected executable 428.5 ms scan against conflicting 143 ms prose. |
| MC140-Narrow | `mc-140-narrow` | MMSSTV narrow/analog | 320×256 | 140.416000 | N-VIS 0x2d/0x15/0x15/0x00 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV payload `0x15`; native sequential-RGB/N-VIS tests | Obtain independent N-VIS PCM plus cross-application/on-air evidence. |
| MC180-Narrow | `mc-180-narrow` | MMSSTV narrow/analog | 320×256 | 180.352000 | N-VIS 0x2d/0x15/0x16/0x03 | impl. | impl. | impl. | unverified | unverified | upstream path observed | deterministic tests | pinned MMSSTV payload `0x16`; native sequential-RGB/N-VIS tests | Obtain independent N-VIS PCM plus cross-application/on-air evidence. |
| FAX480 | `fax-480` | FAX/related FAX | — | — | — | blocked | blocked | blocked | not available | not available | blocked | none | none | Resolve 512x500 versus 512x480; label separately. |
| HFFAX | `hffax` | FAX/related FAX | — | — | — | — | — | — | not available | not available | not tested | none | none | Enumerate authoritative IOC/RPM variants. |
| WEFAX | `wefax` | FAX/related FAX | — | — | — | — | — | — | not available | not available | not tested | none | none | Enumerate authoritative IOC/RPM variants. |

## Native analog Studio TX integration

The post-VOX integration is complete without an external utility: coordinator
enums and ID mapping cover all 52 native analog modes, source builders select
the matching Martin/Scottie/Robot/Sequential RGB/PD/AVT/MMSSTV specification and geometry,
Studio derives its descriptors from those specifications, and
WAV/Coordinator/Studio tests assert exact mode-specific frame boundaries. The
live-only VOX envelope remains outside protocol WAV exports and does not alter
their sample counts.

## FSK ID

| Capability | RX | TX | Evidence | Status |
|---|---:|---:|---|---|
| Six-bit identifier framing | impl. | impl. | `test_sstv_fskid`, `test_sstv_fskid_detector`, `test_sstv_fskid_tx_stream`, coordinator tests | Native detector and optional streaming TX source are implemented; no independent waveform/on-air verification claim. |
| Raw symbols/confidence/checksum diagnostics | impl. | n/a | codec and detector tests | Native bounded diagnostics are implemented; checksum still has only one complete audited producer lineage. |
| Sanitised callsign/custom text | n/a | impl. | codec, TX stream and coordinator tests | Native validation and optional post-image FSK ID are implemented; real-radio/interoperability verification remains pending. |

## Digital compatibility

| Protocol/profile | RX | TX | BSR/resume | Independent interoperability | Status |
|---|---:|---:|---:|---|---|
| HAMDRM pinned native subset | impl. | impl. | impl. | unverified | Native clean-room MOT/BSR/channel/waveform adapters are wired to Decodium's shared RX tap and TX coordinator and pass deterministic self-roundtrip/controller tests. No independent QSSTV, RF, sound-card or on-air interoperability is claimed. |
| KG-STV | — | — | — | none | Public authoritative protocol/vector not yet established. |

## Registry synchronization contract

Every canonical analog row includes dimensions, nominal duration,
standard/extended/N-VIS, RX, TX, auto-detect, independent source/vector status,
test evidence, QSSTV interoperability, Robot36/SlowRX interoperability and a
precise remaining proof. Regenerate after a registry change, then reconcile the
hand-reviewed evidence columns. A capability-state change requires both the
registry change and recorded executable evidence; local loopback never changes
an external-interoperability cell from `unverified`.
