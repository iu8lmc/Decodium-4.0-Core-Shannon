# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.4.8

Date: 2026-03-15  
Scope: update cycle from `v1.4.7` to `v1.4.8`.

## English

### Summary

`v1.4.8` focuses on FT2 operator/timing refinements, FT2 QSO/signoff correctness, remote web hardening, bounded string-safety fixes, and release pipeline robustness.

### Detailed Changes (`v1.4.7 -> v1.4.8`)

- FT2 timing/operator workflow:
- aligned `NUM_FT2_SYMBOLS` from `105` to `103`.
- refined FT2 transmit timing margin from `0.5 s` to `0.2 s` above waveform duration.
- added FT2 `Speedy`, `D-CW`, and `TX NOW` support for immediate or preloaded operator-controlled transmit.
- FT2 auto-sequence logic now uses an FT2-specific enable path instead of relying on a hidden standard AutoSeq checkbox.
- FT2 QSO/signoff correctness:
- FT2 no longer auto-logs when only the local `RR73/73` has been transmitted and no final partner acknowledgment has been received.
- FT2 now waits for real partner final acknowledgment before logging.
- if the FT2 signoff retry budget is exhausted without partner acknowledgment, the QSO is not logged and TX is stopped or CQ resumes according to mode.
- FT2 async duplicate suppression now removes repeated same-payload hypotheses across nearby audio bins.
- Remote Web / dashboard security:
- RemoteCommandServer is disabled on non-loopback bind without an access token.
- non-loopback access tokens must now be at least `12` characters.
- permissive wildcard CORS headers were removed from the remote HTTP API.
- WebSocket clients must now present an allowed same-origin `Origin`.
- String/buffer safety:
- fixed the concrete COM port formatting overflow risk in `lib/ptt.c`.
- migrated related PTT formatting in `lib/ft2` and `map65` to bounded `snprintf` handling.
- hardened `map65` device-info, label text, and astronomical formatting paths.
- Release/build:
- macOS release packaging now tolerates leftover CPack DMG mounts and continues when staged output is valid.
- release defaults, notes fallbacks, and workflow version strings aligned to `v1.4.8`.

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Linux Minimum Requirements

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free
- Runtime/software:
- Linux with `glibc >= 2.35`
- `libfuse2` / FUSE2 support
- ALSA, PulseAudio, or PipeWire audio stack

### If macOS blocks startup

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Linux AppImage recommendation

To avoid issues caused by the AppImage read-only filesystem, start Decodium by extracting the AppImage first and then running the program from the extracted directory.

Run the following commands in a terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

### Sintesi

`v1.4.8` e' focalizzata su rifiniture timing/operator FT2, correttezza QSO/signoff FT2, hardening del controllo remoto web, fix bounded sulle stringhe e maggiore robustezza della pipeline release.

### Modifiche Dettagliate (`v1.4.7 -> v1.4.8`)

- Timing/workflow operatore FT2:
- `NUM_FT2_SYMBOLS` allineato da `105` a `103`.
- margine timing Tx FT2 ridotto da `0.5 s` a `0.2 s` sopra la durata reale d'onda.
- aggiunto supporto FT2 per `Speedy`, `D-CW` e `TX NOW` per trasmissione immediata o pre-caricata sotto controllo operatore.
- la logica AutoSeq FT2 ora usa un path dedicato FT2 invece di dipendere dalla checkbox AutoSeq standard nascosta.
- Correttezza QSO/signoff FT2:
- FT2 non autologa piu' quando e' stato trasmesso solo il `RR73/73` locale senza ack finale reale del partner.
- FT2 attende ora l'ack finale reale del partner prima del log.
- se il budget retry di signoff FT2 si esaurisce senza ack partner, il QSO non viene loggato e la TX viene fermata o si torna a CQ secondo il modo.
- il dedupe async FT2 sopprime ora ipotesi ripetute con lo stesso payload su bin audio vicini.
- Sicurezza Remote Web / dashboard:
- RemoteCommandServer disabilitato su bind non-loopback senza token di accesso.
- i token non-loopback devono avere almeno `12` caratteri.
- rimossi gli header wildcard CORS dall'API HTTP remota.
- i client WebSocket devono presentare un `Origin` same-origin ammesso.
- Sicurezza stringhe/buffer:
- corretto il rischio concreto di overflow nella formattazione della porta COM in `lib/ptt.c`.
- migrata a `snprintf` bounded la formattazione PTT correlata in `lib/ft2` e `map65`.
- induriti i path `map65` per device-info, testo label e testo astronomico.
- Release/build:
- il packaging release macOS ora tollera mount DMG residui di CPack e continua se lo staging e' valido.
- default release, fallback note e stringhe workflow allineati a `v1.4.8`.

### Artifact Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/sperimentale): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Requisiti Minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi
- Runtime/software:
- Linux con `glibc >= 2.35`
- supporto `libfuse2` / FUSE2
- stack audio ALSA, PulseAudio o PipeWire

### Se macOS blocca l'avvio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Avvio consigliato AppImage su Linux

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Espanol

### Resumen

`v1.4.8` se centra en refinamientos de timing/operacion FT2, correccion QSO/signoff FT2, hardening del control remoto web, fixes bounded en cadenas y mas robustez de la pipeline release.

### Cambios Detallados (`v1.4.7 -> v1.4.8`)

- Timing/workflow de operador FT2:
- `NUM_FT2_SYMBOLS` alineado de `105` a `103`.
- margen de timing Tx FT2 reducido de `0.5 s` a `0.2 s` sobre la duracion real de onda.
- anadido soporte FT2 para `Speedy`, `D-CW` y `TX NOW` para transmision inmediata o precargada bajo control del operador.
- la logica AutoSeq FT2 usa ahora una ruta dedicada FT2 en lugar de depender de la checkbox AutoSeq estandar oculta.
- Correccion QSO/signoff FT2:
- FT2 ya no autolog cuando solo se ha transmitido el `RR73/73` local sin ack final real de la pareja.
- FT2 espera ahora el ack final real de la pareja antes del log.
- si el presupuesto de reintentos de signoff FT2 se agota sin ack de la pareja, el QSO no se registra y la TX se detiene o vuelve a CQ segun el modo.
- el dedupe async FT2 suprime ahora hipotesis repetidas con el mismo payload en bins de audio cercanos.
- Seguridad Remote Web / dashboard:
- RemoteCommandServer deshabilitado en bind no-loopback sin token de acceso.
- los tokens no-loopback deben tener al menos `12` caracteres.
- eliminados los headers wildcard CORS del API HTTP remoto.
- los clientes WebSocket deben presentar un `Origin` same-origin permitido.
- Seguridad de cadenas/buffer:
- corregido el riesgo concreto de overflow al formatear la puerta COM en `lib/ptt.c`.
- migrada a `snprintf` bounded la formateacion PTT relacionada en `lib/ft2` y `map65`.
- reforzados los paths `map65` para device-info, texto de labels y texto astronomico.
- Release/build:
- el packaging release macOS ahora tolera mounts DMG residuales de CPack y continua si el staging es valido.
- defaults de release, fallback de notas y strings de workflow alineados a `v1.4.8`.

### Artefactos de Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Requisitos Minimos Linux

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo (8 GB recomendado)
- Espacio: al menos 500 MB libres
- Runtime/software:
- Linux con `glibc >= 2.35`
- soporte `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

### Si macOS bloquea el inicio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Recomendacion AppImage Linux

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

Ejecutar los siguientes comandos en la terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
