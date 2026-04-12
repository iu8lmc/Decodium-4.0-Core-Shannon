# FT8 / FT4 / FT2 / Q65 / MSK144 / SuperFox / FST4 Residue Inventory

Snapshot of the promoted runtime after the current C++ migration work.

The split is deliberate:

- `active runtime residue`: Fortran still reached by the shipped app/runtime path
- `non-runtime residue`: Fortran still present in the tree or build, but no longer used by the promoted runtime path

## FT8

Active runtime residue:

- none

Runtime notes:

- the promoted decode path is `Detector/FT8DecodeWorker.cpp` -> `ftx_ft8_async_decode_stage4_c` in [Detector/FtxFt8Stage4.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FtxFt8Stage4.cpp)
- active stage-4 helpers are native C++ in `Detector/FtxFt8Search.cpp`, `Detector/FtxDownsample.cpp`, `Detector/FtxBitmetrics.cpp`, `Detector/FtxLdpc.cpp`, `Detector/FtxSubtract.cpp`, `Detector/FtxTweak.cpp`, and `Detector/FtxDecodeBookkeeping.cpp`

Non-runtime residue:

- none in the FT8 build/runtime path

Removed from tree:

- `lib/legacy_pack77_compat.f90`
- `lib/legacy_pack77_bridge.f90`
- `lib/77bit/pack77_c.f90`
- `lib/77bit/encode77.f90`
- `lib/77bit/hash22calc.f90`

## FT4

Active runtime residue:

- none

Runtime notes:

- the promoted decode path is `Detector/FT4DecodeWorker.cpp` -> `ftx_ft4_decode_c` in [Detector/FtxFt4Decoder.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FtxFt4Decoder.cpp)
- active TX/message helpers are native in [Modulator/FtxMessageEncoder.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Modulator/FtxMessageEncoder.cpp) and [Modulator/FtxWaveformGenerator.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Modulator/FtxWaveformGenerator.cpp)

Non-runtime residue:

- none in the FT4 build/runtime path

## FT2

Active runtime residue:

- none

Runtime notes:

- the promoted decode path is `Detector/FT2DecodeWorker.cpp` -> `ftx_ft2_async_decode_stage7_c` in [Detector/FtxFt2Stage7.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FtxFt2Stage7.cpp)
- active TX/message helpers are native in [Modulator/FtxMessageEncoder.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Modulator/FtxMessageEncoder.cpp) and [Modulator/FtxWaveformGenerator.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Modulator/FtxWaveformGenerator.cpp)

Non-runtime residue:

- none in the FT2 build/runtime path

## Q65

Active runtime residue:

- none in Fortran

Runtime notes:

- the promoted decode path is native C++ in [Detector/FtxQ65Decoder.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FtxQ65Decoder.cpp) and [Detector/FtxQ65Core.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FtxQ65Core.cpp)
- the former low-level C residue from `lib/qra/q65/q65_subs.c` has been absorbed into the native exports in [Detector/FtxQ65Core.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FtxQ65Core.cpp)

Non-runtime residue:

- none in the shipped build for Q65

Promoted to native C++:

- `Detector/FtxQ65Decoder.cpp` now also owns the legacy compatibility exports `ana64_` and `ftx_q65_ana64_c`
- `lib/ana64.f90` has been removed from the tree

## MSK144

Active runtime residue:

- none

Runtime notes:

- the promoted decode path is native in [Detector/MSK144DecodeWorker.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/MSK144DecodeWorker.cpp)
- the GUI live path uses `decodeMsk144RealtimeBlock()`
- the batch/CLI path uses `decodeMsk144Rows()`
- `mskrtd_` is now a native compatibility export in [Detector/MSK144DecodeWorker.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/MSK144DecodeWorker.cpp)

Non-runtime residue:

- none in the shipped build for MSK144

Tree-only residue:

- none in the dedicated MSK/MSK144 subtree

Promoted to native C++:

- `msk144code` now builds from [utils/msk144code.cpp](/Users/salvo/Desktop/ft2/decodium3-build/utils/msk144code.cpp)
- `msk144sim` now builds from [utils/msk144sim.cpp](/Users/salvo/Desktop/ft2/decodium3-build/utils/msk144sim.cpp)
- the old Fortran tool `lib/msk144code.f90` has been removed from the tree
- the legacy compatibility exports `mskrtd_` and `genmsk_128_90_` are owned by [Detector/MSK144DecodeWorker.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/MSK144DecodeWorker.cpp) and [Modulator/FtxMessageEncoder.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Modulator/FtxMessageEncoder.cpp)

## SuperFox

Active runtime residue:

- none

Runtime notes:

- the promoted RX helper path in [Detector/FtxFt8Stage4.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FtxFt8Stage4.cpp) is native for analytic, remove-tone, QPC sync/likelihoods/SNR and `qpc_decode2`
- the promoted TX path uses native generators in [Modulator/FtxWaveformGenerator.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Modulator/FtxWaveformGenerator.cpp)

Non-runtime residue:

- none in the shipped build for SuperFox

Tree-only residue:

- no Fortran residue remains in the dedicated [lib/superfox](/Users/salvo/Desktop/ft2/decodium3-build/lib/superfox) subtree
- non-Fortran QPC support files and data remain there by design

Promoted to native C++:

- `sfoxsim` now builds from [utils/sfoxsim.cpp](/Users/salvo/Desktop/ft2/decodium3-build/utils/sfoxsim.cpp)
- `sfrx` now builds from [utils/sfrx.cpp](/Users/salvo/Desktop/ft2/decodium3-build/utils/sfrx.cpp)
- `sftx` now builds from [utils/sftx.cpp](/Users/salvo/Desktop/ft2/decodium3-build/utils/sftx.cpp)
- the legacy compatibility exports `sftx_sub_`, `sfox_wave_gfsk_`, `sfox_gen_gfsk_`, `sfox_remove_tone_` and `qpc_decode2_` are owned by [Modulator/FtxWaveformGenerator.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Modulator/FtxWaveformGenerator.cpp) and [Detector/FtxFt8Stage4.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FtxFt8Stage4.cpp)

## FST4 / FST4W

Active runtime residue:

- none in the shipped FST4/FST4W path

Runtime notes:

- the promoted TX/message path is now native in [Modulator/FtxMessageEncoder.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Modulator/FtxMessageEncoder.cpp) and [Modulator/FtxWaveformGenerator.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Modulator/FtxWaveformGenerator.cpp)
- the shipped `fst4sim` helper is now native in [utils/fst4sim.cpp](/Users/salvo/Desktop/ft2/decodium3-build/utils/fst4sim.cpp)
- the shipped `ldpcsim240_101` and `ldpcsim240_74` helpers are now native in [utils/ldpcsim240_101.cpp](/Users/salvo/Desktop/ft2/decodium3-build/utils/ldpcsim240_101.cpp) and [utils/ldpcsim240_74.cpp](/Users/salvo/Desktop/ft2/decodium3-build/utils/ldpcsim240_74.cpp)
- [Detector/FST4DecodeWorker.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FST4DecodeWorker.cpp) now owns the active worker orchestration, async export compatibility, candidate search/downsample/sync search, and the FST4-specific baseline path
- shared decode-side helpers `get_crc24`, `get_fst4_bitmetrics` and `get_fst4_bitmetrics2` are now native in [Detector/FtxFst4Core.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FtxFst4Core.cpp)
- the LDPC/OSD decode core and Doppler spread helpers are now native in [Detector/FtxFst4Ldpc.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FtxFst4Ldpc.cpp)
- the remaining shared DSP helpers used by the worker are now native in [Detector/FtxSharedDsp.cpp](/Users/salvo/Desktop/ft2/decodium3-build/Detector/FtxSharedDsp.cpp)
- the old `multimode_decoder` wrapper has been retired from the tree; FST4/FST4W now enter through the native bridge `ftx_fst4_decode_and_emit_params_c`

Non-runtime residue:

- none in the dedicated `lib/fst4/` subtree
- no tree-only FST4-specific helper/oracle residue remains outside the shipped path

## Remaining generic residue

The remaining residue that still matters after the FTX migration is broader repository cleanup, not FTX runtime code:

- [lib/wavhdr.f90](/Users/salvo/Desktop/ft2/decodium3-build/lib/wavhdr.f90) is still compiled for legacy non-FTX simulator/utilities
- other legacy Fortran/C files under `lib/` belong to non-FTX modes and shared historical tooling
