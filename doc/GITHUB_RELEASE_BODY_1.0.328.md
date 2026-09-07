# Decodium 4 FT2 1.0.328

This release focuses on panadapter/waterfall latency, macOS FT8 decode timing, and QSO sequencing correctness.

## Panadapter and Waterfall

- Added the GPU-direct panadapter/waterfall path. The visual FFT now writes the spectrum, waterfall rows, peak-hold data, and row normalization parameters into shared GPU R32F textures for the QSG renderer.
- Removed the normal CPU readback from the GPU FFT path. Readback is now reserved for diagnostics/fallback.
- Added the `panadapter_fft_direct` compute shader and spectrum QSG shaders for direct texture sampling.
- Kept CPU/FFTW fallback available for unsupported GPU paths and for remote waterfall data that still requires CPU dB rows.
- Fixed GPU-direct waterfall controls so palette, black level, gain, contrast, auto noise floor, peak hold, zoom, and speed are applied correctly.
- Fixed persistence for the waterfall palette and related graph controls across restarts.
- Detached the GPU-direct waterfall node from structural QImage fallback ownership so the direct path can render without depending on CPU image allocation.
- Added panadapter metrics for QSG frame timing, paint timing, overlay timing, and spectrum timer activity.

## Audio and Visual Feed

- Bypassed the Bridge -> QML -> invokable path for panadapter PCM frames. The bridge now registers the `PanadapterItem` directly and feeds it through a C++ route.
- Replaced the per-frame `short -> QVector<float> -> pending frame` allocation path with a direct I16 ring/staging path and a reused upload buffer.
- Added safeguards for duplicate `SoundInput::start()` calls and improved stream health-check behavior on macOS.

## Overlay and UI Responsiveness

- Moved panadapter grid, labels, RX/TX markers, decode labels, and DX cluster markers into a batched C++/QSG overlay texture path.
- Added throttled/batched overlay refresh so heavy decode bursts create less QML binding and repeater pressure.
- Kept QML controls and interaction behavior unchanged while reducing the amount of work done by QML repeaters.

## FT8 and QSO Sequencing

- Added a TX-boundary guard for active FT8 QSOs: if Decodium receives `MYCALL DXCALL R+nn` just before transmit starts, it now forces TX4/RR73 and invalidates the cached repeat-report message.
- Fixed worked-before/LotW status evaluation for third-party/direct messages so a visible DX call is not marked worked only because the other station in the message was already worked.
- Improved embedded macOS legacy FT8 timing. Existing default legacy configs are migrated from normal single-thread decode timing to the staged/early multithreaded timing profile used to reduce late decode delivery.
- Added `DECODIUM_LEGACY_FT8_NORMAL_TIMING=1` as an opt-out for the macOS legacy FT8 timing migration.

## Packaging and Release

- Bumped Decodium to version `1.0.328`.
- Updated Windows release workflow handling so numeric release tags such as `1.0.328` can build and publish the signed Windows installer to the same release used by macOS and Linux assets.
- Updated macOS packaging scripts for the current Apple Silicon release flow.

## Diagnostics

- Added clearer logs for GPU-direct panadapter activation, QML bypass PCM feed, waterfall direct texture usage, legacy FT8 timing migration, and panadapter performance metrics.

## Build Verification

- Local macOS build verified with:

```bash
cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml
```
