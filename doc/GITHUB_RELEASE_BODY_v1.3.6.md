# Decodium Fork v1.3.6 (base: Decodium v3.0 SE Raptor)

## English

This release includes:

- Diagnostic mode menu fixed to behave as true ON/OFF (no forced restart to disable).
- `--language` override fix: explicit CLI language now has strict priority over locale autoload.
- Band Activity country-name visibility fix in normal FT2/FT4/FT8 use.
- `cty.dat` parser hardening for modern large prefix-detail blocks.
- Restored title branding (`Fork by Salvatore Raccampo 9H1SR`).
- PSKReporter `Using:` branding aligned with app title, without legacy appended revision suffix.

Release assets:

- `decodium3-ft2-v1.3.6-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.6-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.6-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.6-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.6-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.6-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.6-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.6-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.6-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.6-macos-monterey-x86_64.dmg` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.6-macos-monterey-x86_64.zip` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.6-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.6-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.6-linux-x86_64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64` CPU, 4 GB RAM minimum (8 GB recommended)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio or PipeWire

If macOS blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

No `.pkg` installers are generated for this fork release line.

## Italiano

Questa release include:

- Correzione menu modalita' diagnostica: ON/OFF reale (disattivabile senza riavvio).
- Correzione override lingua `--language`: priorita' assoluta rispetto al locale automatico.
- Correzione visualizzazione nomi country in Band Activity in uso normale FT2/FT4/FT8.
- Hardening parser `cty.dat` per blocchi prefissi moderni molto grandi.
- Ripristino branding titolo (`Fork by Salvatore Raccampo 9H1SR`).
- Allineamento branding PSKReporter `Using:` al titolo app, senza suffisso revision legacy.

Artifact release:

- `decodium3-ft2-v1.3.6-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.6-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.6-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.6-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.6-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.6-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.6-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.6-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.6-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.6-macos-monterey-x86_64.dmg` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.3.6-macos-monterey-x86_64.zip` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.3.6-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.3.6-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.6-linux-x86_64.AppImage.sha256.txt`

Requisiti minimi Linux:

- CPU `x86_64`, minimo 4 GB RAM (consigliati 8 GB)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per questa linea release fork non vengono generati installer `.pkg`.
