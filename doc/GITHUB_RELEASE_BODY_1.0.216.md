# Decodium 4 FT2 1.0.216

Release focused on stabilizing and promoting the GPU Live Map path.

## Highlights

- Enable the QSG/GPU Live Map by default without requiring an environment variable.
- Keep the CPU `WorldMapItem` fallback available through `LiveMapUseGpu=false` and automatic QML Loader fallback.
- Align GPU Live Map contact retention with the legacy CPU map: 40 stored contacts, 120s expiry, directional downgrade after 75s, and bounded visible labels.
- Fix QSG texture lifetime issues in the GPU map label and marker paths.
- Keep Live Map shader assets packaged for Qt RHI backends including Metal, Vulkan, Direct3D 11/12, and OpenGL-compatible shader targets.

Windows, macOS Apple Silicon, and Linux x86_64 Qt 6.11 assets are produced by GitHub Actions.
