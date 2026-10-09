# SSTV mode catalogue

Status: discovery catalogue, not a support claim. Snapshot 2026-08-24.

Decodium contained no SSTV implementation at the starting commit. Every row is
therefore initially `catalogued/unimplemented`; `MODE_MATRIX.md` is the
authoritative statement of verified capability. Exact timing and VIS fields
will live in one C++ registry and generated documentation once conflicts below
are resolved. They will not be copied into independent hand-maintained tables.

## Classification and source keys

- `analog`: amateur FM-tone SSTV with VIS or a documented no-VIS structure.
- `related-fax`: image facsimile sharing DSP components but not advertised as
  standard amateur analog SSTV.
- `digital`: HAMDRM or another separately specified digital object protocol.
- Source keys: `H` pinned SSTV Handbook PDF, `M` pinned MMSSTV source mirror, `Q` QSSTV, `S` current
  SlowRX, `R` Robot36, `L` libsstv, `P` pySSTV. QT6SSTV is the same QSSTV
  lineage and is not an independent key.
- `path` below means observed implementation path, not executed interoperability.

## Mandatory analog families

| Stable candidate ID | Display name | Class | Observed independent paths | Discovery note/blocker |
|---|---|---|---|---|
| `martin-m1` | Martin M1 | analog | Q RX/TX; S RX; R RX; L/P TX | Native bounded RX/TX/VIS uses the Handbook's exact 4.862 ms sync; QSSTV's rounded 5 ms value remains a documented divergence. |
| `martin-m2` | Martin M2 | analog | Q RX/TX; S/R RX; L/P TX | Native bounded RX/TX/VIS uses a 320-column wire/display raster and records the Handbook's 160-column effective sampled resolution separately; pinned libsstv landmarks are developer evidence. |
| `martin-m3` | Martin M3 | analog | S RX; L TX | Native bounded RX/TX/VIS uses the Handbook/libsstv 457.6 us pixel and 320x128 raster, resolving SlowRX's internally inconsistent field; pinned libsstv landmarks are not on-air proof. |
| `martin-m4` | Martin M4 | analog | S RX; L TX | Native bounded RX/TX/VIS uses M2 line timing at 320x128 with effective sampled width 160; pinned libsstv landmarks are developer evidence. |
| `scottie-s1` | Scottie S1 | analog | Q RX/TX; S/R RX; L/P TX | Scottie first-line/sync ordering must be encoded explicitly. |
| `scottie-s2` | Scottie S2 | analog | Q RX/TX; S/R RX; L/P TX | pySSTV width 160 conflicts with 320 elsewhere. |
| `scottie-dx` | Scottie DX | analog | Q RX/TX; S/R RX; L/P TX | Long-duration performance and cancellation required. |
| `scottie-s3` | Scottie S3 | analog | L TX | Native bounded RX/TX/VIS uses Handbook S1 timing at 320x128; pinned libsstv landmarks are developer evidence, not on-air verification. |
| `scottie-s4` | Scottie S4 | analog | L TX | Native bounded RX/TX/VIS uses Handbook S2 timing at 320x128; effective sampled width 160 remains a separate registry field. |
| `robot-c12` | Robot 12 Colour | analog | L TX | Historical name/VIS/line-pair behaviour need independent documentation; absent from Q/S/R. |
| `robot-c24` | Robot 24 Colour | analog | Q RX/TX; S RX; L TX | Geometry conflict: 160x120, 320x120 and 320x240 across sources. |
| `robot-c36` | Robot 36 Colour | analog | Q RX/TX; S/R RX; L/P TX | Robot alternating chroma and displayed/transmitted rows need exact tests. |
| `robot-c72` | Robot 72 Colour | analog | Q RX/TX; S/R RX; L TX | Robot line-pair chroma requires mode-specific tests. |
| `robot-bw8` | Robot B/W 8 | analog | Q RX/TX; S RX; L/P TX | Native TX uses the canonical 10 ms sync + 56 ms scan. RX also selects the independently exercised PySSTV 7+60 ms profile from observed anchors; R/G/B VIS aliases remain explicit. |
| `robot-bw12` | Robot B/W 12 | analog | Q RX/TX; S RX; L TX | Multiple VIS aliases in libsstv. |
| `robot-bw24` | Robot B/W 24 | analog | S RX; L/P TX | Multiple VIS aliases; absent from QSSTV. |
| `robot-bw36` | Robot B/W 36 | analog | L TX | No audited decoder path; multiple VIS aliases. |
| `wraase-sc2-60` | Wraase SC2-60 | analog | Q RX/TX | Native bounded RX/TX/VIS selects the explicitly named QSSTV RX-side 61.5435 s equal-RGB compatibility profile. QSSTV's own TX porches/scans differ; only self-generated tests exist and no QSSTV-TX, independent-waveform or interoperability equivalence is claimed. |
| `wraase-sc2-120` | Wraase SC2-120 | analog | Q RX/TX; S RX; P TX | Native bounded RX/TX/VIS selects pySSTV's coherent 475.5225 ms equal-RGB profile, agreeing with SlowRX/QSSTV at line level. The conflicting rounded Handbook 2:4:2 component row is recorded, not blended into the implementation. |
| `wraase-sc2-180` | Wraase SC2-180 | analog | Q RX/TX; S/R RX; P TX | Native bounded RX/TX/VIS uses the coherent pySSTV/SlowRX 711.0225 ms line and cumulative 235/320 ms pixel map; SlowRX's incompatible pixel-duration field is excluded explicitly. |
| `pasokon-p3` | Pasokon P3 | analog | Q RX/TX; S RX; P TX | Native bounded RGB RX/TX/VIS uses the exact 1/4800 s time unit and 640x496 wire/display raster, while retaining 320 as effective sampled width. The Handbook reserves the upper 16 lines for a calibration grayscale, but does not specify a unique pixel pattern; Decodium preserves the prepared RGB rows and does not invent one. |
| `pasokon-p5` | Pasokon P5 | analog | Q RX/TX; S RX; P TX | Native bounded RGB RX/TX/VIS uses the exact 1/3200 s time unit and 640x496 raster; pinned pySSTV timing landmarks are developer evidence, not live interoperability. The upper 16 calibration rows are not synthesized implicitly. |
| `pasokon-p7` | Pasokon P7 | analog | Q RX/TX; S RX; P TX | Native bounded RGB RX/TX/VIS uses the exact 1/2400 s time unit and cumulative fractional-sample boundaries at 640x496; pinned pySSTV landmarks are not an on-air claim. The upper 16 calibration rows are not synthesized implicitly. |
| `pd-50` | PD50 | analog | Q RX/TX; S RX; R RX; L TX | Native bounded 320x256 RX/TX/VIS uses 128 exact Y-even/Cr-average/Cb-average/Y-odd pairs. pySSTV omits PD50; pinned libsstv pre-defect landmarks are developer evidence only. |
| `pd-90` | PD90 | analog | Q RX/TX; S/R RX; L/P TX | Native bounded 320x256 RX/TX/VIS uses 532 us pixels and stops after 128 pairs; pinned pySSTV plus pre-defect libsstv landmarks are not live interoperability proof. |
| `pd-120` | PD120 | analog | Q RX/TX; S/R RX; L/P TX | Native bounded 640x496 RX/TX/VIS uses 190 us pixels and all 248 transmitted row pairs, including calibration rows. |
| `pd-160` | PD160 | analog | Q RX/TX; S/R RX; L/P TX | Native bounded 512x400 RX/TX/VIS selects the coherent 195.584 ms component/804.416 ms pair; the Handbook's conflicting isolated 195.854 ms entry is excluded. |
| `pd-180` | PD180 | analog | Q RX/TX; S/R RX; L/P TX | Native bounded 640x496 RX/TX/VIS uses 286 us pixels and exact cumulative pair boundaries; pinned pySSTV/libsstv landmarks remain developer-only evidence. |
| `pd-240` | PD240 | analog | Q RX/TX; S/R RX; L/P TX | Native bounded 640x496 RX/TX/VIS has an exact 1 s pair and fixed memory independent of the 248 s image duration. |
| `pd-290` | PD290 | analog | Q RX/TX; S/R RX; L/P TX | Native bounded 800x616 RX/TX/VIS stops at 308 pairs; cancellation and exact 289.59224 s protocol boundary are tested without whole-waveform buffering. |
| `avt-24` | AVT24 | analog | H; Q/M path audit | Native bounded 128x120 normal-mode RX/TX/autodetect uses three complete standard VIS 64 headers, the exact 32x17-symbol protected countdown at 102.4 baud and continuous 62.5 ms R/G/B components with no per-line sync or gap. Source landmarks and native PCM loopback are developer evidence only. |
| `avt-90` | AVT90 | analog | H; Q/M path audit | Native bounded normal-mode RX/TX/autodetect uses VIS 68, protected countdown prefix `101` and continuous 125 ms R/G/B components. The Handbook's 256x240 effective resolution remains distinct from the audited common 320x240 prepared/wire raster; MMSSTV's conflicting `010` prefix defect is not copied. |
| `avt-94` | AVT94 | analog | H; Q/M path audit | Native bounded 320x200 normal-mode RX/TX/autodetect uses VIS 72, protected countdown prefix `011` and continuous 156.25 ms R/G/B components without line sync. No independent PCM, live-radio or cross-application proof exists. |
| `avt-24-narrow` | AVT24 Narrow | analog | H/Q/M header audit | Catalogue-only standard VIS 65 identity. Complete Narrow countdown/carrier and picture semantics are not independently established; RX, TX and auto-detect are deliberately unavailable. |
| `avt-24-qrm` | AVT24 QRM | analog | H/Q/M header audit | Catalogue-only standard VIS 66 identity. Complete QRM waveform/picture semantics and independent evidence are missing; no executable capability is claimed. |
| `avt-24-narrow-qrm` | AVT24 Narrow QRM | analog | H/Q/M header audit | Catalogue-only standard VIS 67 identity. Complete combined Narrow-QRM semantics and independent evidence are missing; no executable capability is claimed. |
| `avt-90-narrow` | AVT90 Narrow | analog | H/Q/M header audit | Catalogue-only standard VIS 69 identity. The header identity does not prove complete Narrow AVT90 picture semantics; RX, TX and auto-detect remain unavailable. |
| `avt-90-qrm` | AVT90 QRM | analog | H/Q/M header audit | Catalogue-only standard VIS 70 identity. Complete QRM waveform/picture semantics and independent evidence are missing; no executable capability is claimed. |
| `avt-90-narrow-qrm` | AVT90 Narrow QRM | analog | H/Q/M header audit | Catalogue-only standard VIS 71 identity. Complete combined Narrow-QRM semantics and independent evidence are missing; no executable capability is claimed. |
| `avt-94-narrow` | AVT94 Narrow | analog | H/Q/M header audit | Catalogue-only standard VIS 73 identity. Complete Narrow countdown/carrier and picture semantics are not independently established; no executable capability is claimed. |
| `avt-94-qrm` | AVT94 QRM | analog | H/Q/M header audit | Catalogue-only standard VIS 74 identity. Complete QRM waveform/picture semantics and independent evidence are missing; no executable capability is claimed. |
| `avt-94-narrow-qrm` | AVT94 Narrow QRM | analog | H/Q/M header audit | Catalogue-only standard VIS 75 identity. Complete combined Narrow-QRM semantics and independent evidence are missing; no executable capability is claimed. |
| `mp-73` | MP73 | analog | M/Q RX/TX | Native bounded 320x256 RX/TX/autodetect uses wide extended VIS raw `0x25` and 128 paired Y-first/Cr-average/Cb-average/Y-second scans. The pinned source-landmark fixture is developer evidence, not independent PCM or live proof. |
| `mp-115` | MP115 | analog | M/Q RX/TX | Native bounded 320x256 paired-line RX/TX/autodetect uses wide extended VIS raw `0x29`; exact cumulative timing is covered by the pinned source-landmark fixture. |
| `mp-140` | MP140 | analog | M/Q RX/TX | Native bounded 320x256 paired-line RX/TX/autodetect uses wide extended VIS raw `0x2A`; no independent waveform/on-air proof is claimed. |
| `mp-175` | MP175 | analog | M/Q RX/TX | Native bounded 320x256 paired-line RX/TX/autodetect uses wide extended VIS raw `0x2C`; no independent waveform/on-air proof is claimed. |
| `mr-73` | MR73 | analog | M/Q RX/TX | Native bounded 320x256 RX/TX/autodetect uses full-width Y, half-width horizontal Cr/Cb and explicit 0.1 ms holds; extended VIS raw `0x45`. |
| `mr-90` | MR90 | analog | M/Q RX/TX | Native bounded 320x256 Y/Cr-half/Cb-half RX/TX/autodetect uses extended VIS raw `0x46`; pinned landmarks are not a captured waveform. |
| `mr-115` | MR115 | analog | M/Q RX/TX | Native bounded 320x256 Y/Cr-half/Cb-half RX/TX/autodetect uses extended VIS raw `0x49`; pinned landmarks are not live proof. |
| `mr-140` | MR140 | analog | M/Q RX/TX | Native bounded 320x256 Y/Cr-half/Cb-half RX/TX/autodetect uses extended VIS raw `0x4A`. |
| `mr-175` | MR175 | analog | M/Q RX/TX | Native bounded 320x256 RX/TX/autodetect uses raw `0x4C`, selected from the original MMSSTV transmitter, `mode.txt` and Handbook. QSSTV's duplicate `0x4A` row is retained as typo evidence and is not accepted for MR175. |
| `ml-180` | ML180 | analog | M/Q RX/TX | Native bounded 640x496 RX/TX/autodetect uses full-width Y, half-width horizontal Cr/Cb, 0.1 ms holds and extended VIS raw `0x85`. |
| `ml-240` | ML240 | analog | M/Q RX/TX | Native bounded 640x496 Y/Cr-half/Cb-half RX/TX/autodetect uses extended VIS raw `0x86`; no independent PCM/live proof is claimed. |
| `ml-280` | ML280 | analog | M/Q RX/TX | Native bounded 640x496 Y/Cr-half/Cb-half RX/TX/autodetect uses extended VIS raw `0x89`; no independent PCM/live proof is claimed. |
| `ml-320` | ML320 | analog | M/Q RX/TX | Native bounded 640x496 Y/Cr-half/Cb-half RX/TX/autodetect uses extended VIS raw `0x8A`; no independent PCM/live proof is claimed. |
| `mp-73-narrow` | MP73-Narrow | analog | M/Q RX/TX | Native bounded 320x256 paired-line RX/TX/autodetect uses the four-group N-VIS payload `0x02`. MP73-Narrow is explicitly present in the pinned canonical source fixture; it was not inferred. |
| `mp-110-narrow` | MP110-Narrow | analog | M/Q RX/TX | Native bounded 320x256 paired-line RX/TX/autodetect uses four-group N-VIS payload `0x04`; pinned source landmarks are not independent PCM. |
| `mp-140-narrow` | MP140-Narrow | analog | M/Q RX/TX | Native bounded 320x256 paired-line RX/TX/autodetect uses four-group N-VIS payload `0x05`; pinned source landmarks are not independent PCM. |
| `mc-110-narrow` | MC110-Narrow | analog | M/Q RX/TX | Native bounded 320x256 sequential RGB RX/TX/autodetect uses N-VIS payload `0x14` and the MMSSTV executable's 140 ms components/428.5 ms scan. The conflicting 143 ms `mode.txt` prose is documented, not blended. |
| `mc-140-narrow` | MC140-Narrow | analog | M/Q RX/TX | Native bounded 320x256 sequential RGB RX/TX/autodetect uses four-group N-VIS payload `0x15`; no independent waveform/on-air proof is claimed. |
| `mc-180-narrow` | MC180-Narrow | analog | M/Q RX/TX | Native bounded 320x256 sequential RGB RX/TX/autodetect uses four-group N-VIS payload `0x16`; no independent waveform/on-air proof is claimed. |

Additional Wraase and MMSSTV variants found later will be added only with
a citable specification and distinct identity. Similar line duration is not
enough to invent a mode.

## Related image modes

| Stable candidate ID | Display name | Class | Observed paths | Discovery note/blocker |
|---|---|---|---|---|
| `fax-480` | FAX480 | related-fax | Q RX/TX; L TX | Geometry conflict: QSSTV 512x500 versus libsstv 512x480. Must not be labelled standard analog SSTV. |
| `hffax` | HFFAX | related-fax | R RX | This is a category until IOC/line-rate variants are authoritatively enumerated. |
| `wefax` | WEFAX | related-fax | none in audited active paths | Research IOC/RPM variants and legal vectors before adding registry rows. |

## Digital protocols

| Stable candidate ID | Display name | Class | Observed paths | Discovery note/blocker |
|---|---|---|---|---|
| `hamdrm` | HAMDRM / digital SSTV | digital | Q RX/TX | Separate subsystem. One lineage only, no audited independent vector, and restricted QSSTV source exclusions require clean-room implementation. |
| `kg-stv` | KG-STV | digital | none in audited open references | Do not equate with HAMDRM. Requires a public authoritative specification and legal validation vectors. |

HAMDRM profile IDs will be catalogued separately by occupied bandwidth,
robustness, constellation, protection, coding, interleaver and source coding.

## VIS and timing normalization rules

The eventual `SstvModeSpec` must store separately:

- seven-bit standard VIS payload, parity bit and bit order;
- extended-VIS bytes/encoding, not an ambiguous packed integer;
- transmitted, sampled and displayed dimensions;
- rational microseconds for every segment and derived line/image duration;
- colour system, component order, subsampling and lines represented per scan;
- provenance/evidence identifiers and explicit RX/TX/auto-detect states.

Nominal leader/header, black/white/sync/separator frequencies are protocol data
in the core registry, never UI literals. A fractional sample accumulator maps
rational durations to integer sample blocks without line-by-line drift.

## Discovery exit criteria

A row may move from discovery to implementation only after its conflicts are
resolved or explicitly represented. A row may move to `verified` only when:

1. timing, VIS and colour/line sequence are documented;
2. RX and/or TX implementation has deterministic tests;
3. an independent implementation or legally usable reference vector was run;
4. the test result and commit are named in `MODE_MATRIX.md`;
5. release/user documentation states only the verified direction/capability.
