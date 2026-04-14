# FTX C++ Migration Status

This document records the actual migration state of FT8, FT4, FT2, Q65, MSK144, SuperFox and the adjacent FST4/FST4W cleanup.

The rule is simple:

- "default C++ stage" means the promoted runtime path uses some migrated C++ components
- it does **not** mean that the mode is already free of Fortran
- the migration is complete only when the production decode path no longer depends on Fortran except for optional reference-oracle code kept behind explicit test/debug wrappers

## FT8

Current default:

- promoted runtime path is the C++ stage-4 decoder used by `Detector/FT8DecodeWorker.cpp`
- migrated FT8 components are active for sync search, sync metric, candidate encoding, A7/A8 helpers, false-decode filtering, LDPC bridges, downsample/subtract/saved-subtractions, and waveform/message utility tools
- the standalone frontends `jt9` and `jt9a` are now native C++ in `utils/jt9.cpp`

What is still Fortran in the active FT8 path:

- the app/runtime decoder path no longer depends on the old `ft8var`, `ft8_core`, or the retired `multimode_decoder` wrapper path
- no FT8-specific Fortran remains on the promoted runtime path
- `lib/rtty_spec.f90` is no longer part of the build and has been replaced by `utils/rtty_spec.cpp`

Already removed from the active FT8 path:

- `lib/ft8/decode174_91.f90` and `lib/ft8/ldpc_174_91_c_api.f90` have been replaced by C++ exports in `Detector/FtxLdpc.cpp`
- `lib/ft8var/chkfalse8var.f90`, `chkflscallvar.f90`, `chkgridvar.f90`, `chkspecial8var.f90`, `datacor.f90`, `filtersfreevar.f90`, and `searchcallsvar.f90` have been replaced by C++ bookkeeping in `Detector/FtxDecodeBookkeeping.cpp`
- `lib/ft8var/sync8var.f90`, `sync8dvar.f90`, and `subtractft8var.f90` have been replaced by C++ search/sync/subtract bridges
- `lib/ft8var/gen_ft8wavevar.f90` has been replaced by the C++ waveform generator bridge in `Modulator/FtxWaveformGenerator.cpp`
- `lib/ft8var/encode174_91var.f90` and `lib/ft8var/ft8apsetvar.f90` are no longer part of the production path
- `lib/ft8var/ft8_decodevar.f90`, `ft8bvar.f90`, `cwfilter.f90`, `agccft8.f90`, `filbigvar.f90`, `four2avar.f90`, `ft8_mod1.f90`, `jt65_mod*var.f90`, and the other `ft8var` support files have been removed from the active build
- `lib/ft8/ft8_async_decode.f90`, `lib/ft8_decode_core.f90`, and `lib/ft8/ft8b_core.f90` are no longer on the promoted runtime path
- `lib/rtty_spec.f90` has been replaced by the C++ tool `utils/rtty_spec.cpp`

Migration still required for FT8:

- optional cleanup only outside the active runtime path:
  - finish retiring shared compatibility bridges that are now only used by non-FTX legacy/oracle code

## FT4

Current default:

- the production decode path enters native C++ only:
  - `Detector/FT4DecodeWorker.cpp` calls the native export `ftx_ft4_decode_c`
  - `utils/jt9.cpp` dispatches FT4 through the shared native entrypoint `ftx_native_decode_and_emit_params_c`
- promoted C++ FT4 pieces now cover candidate search, sync metric, bitmetrics, downsample, subtract, LDPC bridge, SNR computation, result formatting and `decoded.txt` emission
- FT4 TX/message helpers already use the native exports `genft4_` and `gen_ft4wave_` from `Modulator/FtxMessageEncoder.cpp` and `Modulator/FtxWaveformGenerator.cpp`

What is still Fortran in the active FT4 path:

- no FT4-specific Fortran remains on the promoted runtime decode/TX path
- no FT4-specific or shared Fortran bridge remains on the promoted FT4 runtime path
- the only remaining FT4 mentions in `.f90` files are nominal strings/comments in shared utility code, not active decode/TX logic

Migration still required for FT4:

- optional cleanup only:
  - decide whether to retire the remaining non-FTX Fortran compatibility/oracle layers globally
  - keep parity/reference tests for FT4 as long as they remain useful, but they are no longer required for the active FT4 runtime path

## FT2

Current default:

- the production decode path is native C++:
  - `Detector/FT2DecodeWorker.cpp` calls `ftx_ft2_async_decode_stage7_c`
  - `Detector/FtxFt2Stage7.cpp` owns orchestration, LDPC invocation, result formatting and emission
- the old `multimode_decoder` wrapper has been removed from the tree; native FTX modes dispatch through `ftx_native_decode_and_emit_params_c`

What is still Fortran in or around the active FT2 path:

- no FT2-specific Fortran remains on the promoted runtime decode/TX path
- FT2 transmit generation and subtraction are handled by C++:
  - `Modulator/FtxWaveformGenerator.cpp` provides `foxgenft2_` and `gen_ft2wave_`
  - `Modulator/FtxMessageEncoder.cpp` provides `genft2_` and `get_ft2_tones_from_77bits_`
  - `Detector/FtxSubtract.cpp` provides `subtractft2_`
- the old FT2 Fortran decoder sources under `lib/ft2/` plus `lib/ft2_decode.f90` have been removed from the tree
- the only remaining FT2 mentions in `.f90` files are nominal mode labels in shared code such as `lib/wavhdr.f90`

Migration still required for FT2:

- optional cleanup only:
  - decide whether compatibility exports such as `ft2_async_decode_` and `ft2_triggered_decode_` should remain for external callers or be retired once every caller uses the native C ABI directly
  - decide whether to retire the remaining non-FTX compatibility/oracle layers globally

## Q65

Current default:

- the production decode worker is native C++:
  - `Detector/Q65DecodeWorker.cpp` calls `ftx_q65_async_decode_c`
  - `Detector/FtxQ65Decoder.cpp` now owns the active orchestration previously carried by `lib/q65_decode.f90`, `lib/qra/q65/q65.f90` and `lib/qra/q65/q65_loops.f90`
- the active build no longer compiles `lib/qra/q65/q65.f90`
- the old `multimode_decoder` wrapper has been removed from the tree; native FTX dispatch covers Q65 alongside FT8/FT4/FT2
- the active shared Q65 helpers used by the native decoder are now native C++ in `Detector/FtxQ65Core.cpp`

What is still Fortran in or around the active Q65 path:

- no Q65-specific Fortran remains on the active decode path
- `Detector/FtxQ65Decoder.cpp` now also owns the old `ana64_` compatibility entrypoint, and `lib/ana64.f90` has been removed from the tree
- `qmap` no longer uses Q65-specific Fortran frontend code:
  - `qmap/libqmap/q65c.f90`, `decode0.f90`, `qmapa.f90`, `q65b.f90`, `q65_sync.f90` and `getcand2.f90` have been removed from the tree and replaced by native code in `Detector/FtxQ65Frontend.cpp`
- the active Map65-side Q65 path is now native/generic:
  - `Detector/FtxMap65Q65.cpp` owns candidate search, decode, output accounting and `wb_ftx.txt` emission
  - `map65/libm65/decode0.f90` and `map65/libm65/map65a.f90` no longer contain Q65-specific orchestration
  - `map65/libm65/wideband_sync.f90` is JT65-only again
  - `map65/libm65/q65b.f90` and `map65/libm65/q65b_for_Linux.f90` have been removed from the tree
- the former offline/reference utilities are now native:
  - `utils/q65sim.cpp`
  - `utils/q65code.cpp`
  - `utils/q65_ftn_test.cpp`
  - `utils/q65params.cpp`
  - `utils/test_q65.cpp`
  - `map65/libm65/mapsim.cpp`
- the remaining Q65 mentions in `.f90` are comments, sample strings, or non-Q65 shared code paths

Migration still required for Q65:

- optional cleanup only:
  - remove nominal/comment-only Q65 strings from shared legacy Fortran files if a zero-string audit is desired
  - decide whether dead compatibility aliases such as the old `ftx_map65_q65_*` C exports should remain for external callers

## MSK144

Current default:

- the production decode path is native C++ in `Detector/MSK144DecodeWorker.cpp`
- the `jt9 -k` offline decode path dispatches to the native MSK144 runner in `utils/jt9.cpp`
- the encoder and helper tools are native:
  - `utils/msk144code.cpp`
  - `utils/msk144sim.cpp`

What is still Fortran in the active MSK144 path:

- none

Migration still required for MSK144:

- none in the dedicated MSK/MSK144 subtree:
  - the remaining work is broader repository cleanup outside the shipped MSK144 path

## SuperFox

Current default:

- the production RX helper path is native in `Detector/FtxFt8Stage4.cpp`
- the production TX/helper path is native in `Modulator/FtxWaveformGenerator.cpp`
- the helper tools are native:
  - `utils/sfoxsim.cpp`
  - `utils/sfrx.cpp`
  - `utils/sftx.cpp`

What is still Fortran in the active SuperFox path:

- none

Migration still required for SuperFox:

- none in the dedicated SuperFox subtree:
  - the remaining work is broader repository cleanup outside the shipped SuperFox path

## FST4 / FST4W

Current default:

- the production TX/message path is native C++:
  - `Modulator/FtxMessageEncoder.cpp` now owns `genfst4_` and `get_fst4_tones_from_bits_`
  - `Modulator/FtxWaveformGenerator.cpp` now owns `gen_fst4wave_`
  - `widgets/mainwindow.cpp` uses the native FST4 encoder/wave generator path
- the `fst4sim` helper is now native C++ in `utils/fst4sim.cpp`
- the `ldpcsim240_101` and `ldpcsim240_74` helpers are now native C++ in `utils/ldpcsim240_101.cpp` and `utils/ldpcsim240_74.cpp`

What is still Fortran in the active FST4 path:

- no active FST4/FST4W runtime Fortran remains:
  - `Detector/FST4DecodeWorker.cpp` owns the active worker orchestration and the compatibility export `fst4_async_decode_`
  - `Detector/FST4DecodeWorker.cpp` also owns the native emit bridge `ftx_fst4_decode_and_emit_params_c` that replaced the removed legacy `multimode_decoder` wrapper
  - candidate search, de-duplication, downsample, sync search, async collection and the FST4-specific baseline helper are native in `Detector/FST4DecodeWorker.cpp`
  - shared FST4 decode-side helpers `get_crc24`, `get_fst4_bitmetrics` and `get_fst4_bitmetrics2` are native in `Detector/FtxFst4Core.cpp`
  - the FST4 LDPC/OSD decode core, the legacy `decode240_*`/`osd*` compatibility exports, and the Doppler-spread helper are native in `Detector/FtxFst4Ldpc.cpp`
  - the last shared DSP helpers (`blanker`, `four2a`, `pctile`, `polyfit`) are now native in `Detector/FtxSharedDsp.cpp`

Migration still required for FST4:

- none in the FST4/FST4W subtree:
  - the remaining work is broader non-FTX cleanup outside the shipped FST4 path

Residual generic legacy files still present:

- `lib/wavhdr.f90` is still built for legacy non-FTX simulator/utilities

## Recommended Order

1. FST4 / FST4W decode core

- the active FST4/FST4W runtime path is now fully native
- the next cleanup is tree-only helper/oracle residue, not user-facing runtime code

2. Non-FTX modes and utilities

- JT/JT65/MSK/WSPR/FST4 cleanup is now separate from the completed FT8/FT4/FT2/Q65/MSK144/SuperFox migration
- those modes can be handled independently without reopening the FTX decoder architecture

## Definition Of Done

Migration is complete only when all of the following are true:

- the production decode worker for FT8, FT4, FT2, Q65, MSK144 and SuperFox enters through native C++ only
- no promoted runtime path calls a Fortran decode/orchestration/downsample/subtract/LDPC routine
- any remaining Fortran code is isolated to shared compatibility layers or non-FTX modes
- stage-compare and real-recording parity tests pass against the final C++ path
- FT8/FT4/FT2/Q65-specific Fortran production sources can be deleted without changing decode behavior
