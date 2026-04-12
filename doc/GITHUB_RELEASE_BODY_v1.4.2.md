# Decodium 3 FT2 v1.4.2 (Fork 9H1SR)

## English

Release highlights (`v1.4.1 -> v1.4.2`):

- LotW security hardening:
  - switched credentialed LotW requests from GET URL to HTTPS POST body,
  - credentials removed from URL-style logs,
  - strict redirect policy for credentialed requests (HTTPS + expected host only).
- Remote Web security hardening:
  - LAN/WAN bind refused if token length is below 12 characters,
  - explicit plaintext HTTP/WS warning shown on remote exposure.
- FT2/runtime behavior fixes:
  - Async L2 defaults ON in FT2 and auto-OFF outside FT2,
  - `Lock Tx Freq` and `Tx even/1st` hidden (and forced OFF) in FT2,
  - unified band-button frequency handling to avoid re-click deselection/re-tune anomalies.
- Linux reliability:
  - settings dialog now auto-fits screen geometry (buttons always reachable),
  - async decode moved to dedicated thread pool with larger Linux stack,
  - decoder overlap guards and bounded async parameters before Fortran calls.
- UI/tool windows:
  - restored `View -> Ionospheric Forecast` and `View -> DX Cluster` open/toggle behavior.
- Web dashboard:
  - TX events preserved across refresh cycles and first TX transition reported correctly.
- C/C++ hardening:
  - bounded string operations in WSPR helpers,
  - Windows-specific helper inclusion removed from this fork build config.

Release assets:

- `decodium3-ft2-v1.4.2-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.2-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.2-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.2-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.2-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.2-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.2-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.2-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.2-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.2-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.2-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.2-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.2-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.2-linux-x86_64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64` CPU (2 cores, 2.0 GHz+)
- 4 GB RAM minimum (8 GB recommended)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire

If macOS blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Linux AppImage recommendation (read-only FS workaround):

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No `.pkg` installers are produced.

## Italiano

Punti principali release (`v1.4.1 -> v1.4.2`):

- Hardening LotW:
  - richieste LotW con credenziali migrate da GET URL a POST HTTPS,
  - credenziali non piu' esposte nei log URL,
  - redirect policy stretta per richieste con credenziali (solo HTTPS + host atteso).
- Hardening Remote Web:
  - bind LAN/WAN rifiutato con token inferiore a 12 caratteri,
  - avviso esplicito su traffico HTTP/WS in chiaro su esposizione remota.
- Fix comportamento FT2/runtime:
  - Async L2 ON di default in FT2 e OFF automatico fuori FT2,
  - `Blocca la Freq Tx` e `Tx pari/1°` nascosti (e spenti) in FT2,
  - gestione frequenze pulsanti banda unificata per evitare anomalie su re-click.
- Affidabilita' Linux:
  - finestra impostazioni adattata alla geometria schermo (pulsanti sempre accessibili),
  - decode async su thread pool dedicato con stack Linux piu' ampio,
  - guardie anti-overlap e parametri async bounded prima delle chiamate Fortran.
- Finestre strumenti/UI:
  - ripristinato funzionamento `View -> Ionospheric Forecast` e `View -> DX Cluster`.
- Dashboard web:
  - eventi TX preservati durante i refresh e prima transizione TX tracciata correttamente.
- Hardening C/C++:
  - operazioni stringa bounded nei moduli WSPR,
  - esclusione helper specifici Windows dalla build del fork.

Asset release:

- `decodium3-ft2-v1.4.2-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.2-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.2-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.2-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.2-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.2-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.2-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.2-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.2-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.2-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.2-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.2-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.2-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.2-linux-x86_64.AppImage.sha256.txt`

Requisiti minimi Linux:

- CPU `x86_64` (2 core, 2.0 GHz+)
- RAM minima 4 GB (consigliati 8 GB)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Avvio consigliato AppImage Linux (workaround filesystem read-only):

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

I pacchetti `.pkg` non vengono prodotti.

## Espanol

Resumen release (`v1.4.1 -> v1.4.2`):

- Hardening LotW:
  - peticiones LotW con credenciales migradas de GET URL a POST HTTPS,
  - credenciales fuera de logs tipo URL,
  - politica de redirect estricta para peticiones con credenciales (solo HTTPS + host esperado).
- Hardening Remote Web:
  - bind LAN/WAN rechazado si el token tiene menos de 12 caracteres,
  - aviso explicito de trafico HTTP/WS en claro en exposicion remota.
- Fixes FT2/runtime:
  - Async L2 ON por defecto en FT2 y OFF automatico fuera de FT2,
  - `Bloquear Freq Tx` y `Tx par/1°` ocultos (y apagados) en FT2,
  - manejo de frecuencia de botones de banda unificado para evitar anomalias al reclick.
- Fiabilidad Linux:
  - ventana de ajustes ajustada a geometria de pantalla (botones siempre accesibles),
  - decode async en thread pool dedicado con stack Linux mayor,
  - guardas anti-solapamiento y parametros async acotados antes de Fortran.
- Ventanas UI/herramientas:
  - restaurado `View -> Ionospheric Forecast` y `View -> DX Cluster`.
- Dashboard web:
  - eventos TX preservados durante refresh y primera transicion TX reportada correctamente.
- Hardening C/C++:
  - operaciones string acotadas en modulos WSPR,
  - exclusion de helpers especificos Windows en la build del fork.

Artefactos release:

- `decodium3-ft2-v1.4.2-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.2-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.2-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.2-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.2-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.2-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.2-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.2-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.2-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.2-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.2-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.2-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.2-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.2-linux-x86_64.AppImage.sha256.txt`

Requisitos minimos Linux:

- CPU `x86_64` (2 nucleos, 2.0 GHz+)
- RAM 4 GB minimo (8 GB recomendado)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Si macOS bloquea el inicio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Recomendacion AppImage Linux (workaround filesystem solo lectura):

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No se generan instaladores `.pkg`.
