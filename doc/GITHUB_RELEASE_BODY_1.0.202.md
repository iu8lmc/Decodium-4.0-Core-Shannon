# Decodium 4 FT2 1.0.202

Maintenance release with the current local fixes.

- HRD TCP v5 framing is now encoded and decoded explicitly in little-endian form, avoiding platform-dependent struct layout.
- HRD host parsing accepts a bare port such as `7810`, matching the diagnostic tap workflow.
- `hrd_tap` diagnostics were improved for bare-port input and richer frame parsing.
- Linux/OpenGL AppImage startup stability was improved by disabling the experimental OpenGL GPU panadapter FFT path by default and falling back to CPU FFT.

Windows, macOS Apple Silicon, and Linux x86_64 Qt 6.11 assets are produced by GitHub Actions.
