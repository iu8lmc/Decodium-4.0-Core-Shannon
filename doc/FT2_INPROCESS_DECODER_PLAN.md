## FT2 In-Process Decoder Status

Date: 2026-03-28

The original FT2 in-process migration plan is complete.

### Final state

- `Detector/FT2DecodeWorker.cpp` enters FT2 through the native export `ftx_ft2_async_decode_stage7_c`
- `Detector/FtxFt2Stage7.cpp` owns the promoted FT2 runtime path:
  - candidate search
  - downsample/sync/bitmetrics
  - LDPC invocation
  - decode orchestration
  - result formatting and `decoded.txt` emission
- `lib/decoder.f90` no longer contains FT2-specific algorithmic logic; it dispatches native FTX modes through the shared native entrypoint `ftx_native_decode_and_emit_params_c`
- FT2 TX helpers are native C++:
  - `Modulator/FtxMessageEncoder.cpp`: `genft2_`, `get_ft2_tones_from_77bits_`
  - `Modulator/FtxWaveformGenerator.cpp`: `gen_ft2wave_`, `foxgenft2_`
  - `Detector/FtxSubtract.cpp`: `subtractft2_`

### Removed legacy FT2 Fortran

The old FT2 Fortran decoder sources have been removed from the tree, including:

- `lib/ft2/*.f90`
- `lib/ft2_decode.f90`

The shared replacement for the previously FT2-local pulse helper now lives in:

- `lib/gfsk_pulse.f90`

### Validation

The native-only FT2 path is validated in this tree with:

- `build/tests/ft2_stage_compare`
- `build/tests/ft2_equalized_compare`
- the FT2 subset in `build/tests/test_qt_helpers`

### Residual items

There is no FT2-specific Fortran left on the promoted runtime path.

The only remaining FT2 mentions in `.f90` files are nominal/shared references such as:

- mode labels in `lib/wavhdr.f90`

Any future cleanup is no longer FT2 algorithm migration; it is shared compatibility cleanup.
