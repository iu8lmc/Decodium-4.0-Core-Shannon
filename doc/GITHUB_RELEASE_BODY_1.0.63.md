# Decodium 4 FT2 1.0.63

Release 1.0.63 raccoglie gli ultimi fix per FT8/FT2, GPU/waterfall, grafica Qt6 e UX TX/reporting.

## Highlights

- FT8: decode anticipato in backend legacy embedded e filtro anti-risultati tardivi oltre 6.5s.
- GPU/UI: indicatore GPU/render activity nella status bar.
- Waterfall: pipeline Qt ShaderTools/QSB per palette GPU con fallback CPU.
- Grafica: fallback multipiattaforma per backend Qt Quick, incluso Windows safe graphics/WARP.
- TX: controlli manuali per messaggi, report, commento e reset messaggi standard.
- Spettrogramma/waterfall: overlay RX/TX e filtro RX piu' leggibili.

## Validazione

- Build locale `decodium_qml` completata su macOS.
- Test runtime FT8 in `--test-mode`: 8 slot con `nzhsym=48`, decode completati in 123-137 ms.

## Assets attesi

- `Decodium_1.0.63_Setup_x64.exe`
- `decodium4-ft2-1.0.63-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.63-macos-tahoe-arm64.zip`
- `decodium4-ft2-1.0.63-macos-tahoe-arm64-sha256.txt`
- `decodium4-ft2-1.0.63-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.63-macos-sequoia-arm64.zip`
- `decodium4-ft2-1.0.63-macos-sequoia-arm64-sha256.txt`
