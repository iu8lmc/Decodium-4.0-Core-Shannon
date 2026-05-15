# Decodium 4 FT2 1.0.198

Release focused on Windows graphics startup recovery for PCs where Direct3D 12 is unavailable or broken.

## Highlights

- Keep the first Windows startup attempt on Direct3D 12 when no override is configured.
- Detect Qt/Windows Direct3D 12 device creation failures such as `0x887a0004`.
- Retry the next startup with Direct3D 11 hardware instead of jumping straight to the WARP/software renderer.
- Preserve safe graphics as the final fallback only when Direct3D 11 hardware startup also fails.
- Clear stale startup markers after a stable Direct3D 11 fallback launch.

## Assets

Windows and macOS release assets are produced by GitHub Actions. Source archives are attached automatically by GitHub for tag `1.0.198`.
