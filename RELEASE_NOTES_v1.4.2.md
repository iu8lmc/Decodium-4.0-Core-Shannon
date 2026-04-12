# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.4.2

Date: 2026-03-10  
Scope: update cycle from `v1.4.1` to `v1.4.2`, focused on security hardening, Linux stability, FT2 control behavior, and release alignment.

## English

### Summary

`v1.4.2` delivers the requested hardening pass and runtime fixes for current field usage:

- LotW credentials are no longer sent in URL query strings.
- Remote Web LAN/WAN binding now enforces stronger token requirements.
- Linux runtime and UI stability improved (settings window bounds + async decode threading safeguards).
- FT2 control behavior finalized (Async L2 default ON in FT2, FT2-only control visibility logic).
- View windows (Ionospheric Forecast / DX Cluster) restored with stable open/close handling.

### Detailed Changes (`v1.4.1 -> v1.4.2`)

- Security/network hardening:
  - LotW downloads moved from credentialed GET URL to HTTPS POST body (`application/x-www-form-urlencoded`).
  - LotW logs now redact credential-bearing request details.
  - Credentialed LotW redirects are now strict: HTTPS + same expected host only.
  - Remote WS startup rejects LAN/WAN bind if token length is below 12 characters.
  - Remote startup now emits explicit warning that HTTP/WS traffic is plaintext (use trusted LAN/VPN or TLS proxy).
- FT2/runtime behavior:
  - Async L2 defaults to ON when entering FT2 (user can disable), and is auto-disabled when leaving FT2.
  - `Lock Tx Freq` and `Tx even/1st` controls are auto-hidden (and forced off) in FT2.
  - Band button frequency selection logic consolidated to prevent re-click deselection/re-tune anomalies.
- Linux stability fixes:
  - Settings dialog now auto-clamps to available screen geometry to keep action buttons reachable on low-height desktops.
  - Async decode now runs on dedicated thread pool with controlled stack size (Linux: 16 MB) and overlap guards.
  - Async decode parameters are now bounded before Fortran calls; shutdown waits for async completion.
- UI/tool windows:
  - Restored `View -> Ionospheric Forecast` and `View -> DX Cluster` behavior, including persistent toggled state sync.
- Web dashboard behavior:
  - TX events are preserved across state refreshes and first-state TX is reported correctly in RX/TX activity feed.
- C/C++ safety and platform cleanup:
  - Replaced unsafe string operations in WSPR helpers with bounded variants (`snprintf`/bounded append).
  - Removed Windows-specific helper inclusion paths from this fork build configuration.

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Note: `.pkg` installers are intentionally not produced.

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

To avoid issues caused by AppImage read-only filesystem mode, it is recommended to extract first and run from the extracted folder.

Run in terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

### Sintesi

`v1.4.2` completa il passaggio di hardening richiesto e i fix runtime per l'uso reale:

- credenziali LotW non piu' inviate nella query URL;
- bind remoto LAN/WAN con requisito token piu' robusto;
- stabilita' Linux migliorata (finestra impostazioni + thread async decode);
- comportamento controlli FT2 finalizzato (Async L2 ON di default in FT2, visibilita' controlli coerente);
- finestre `Ionospheric Forecast` e `DX Cluster` ripristinate.

### Modifiche Dettagliate (`v1.4.1 -> v1.4.2`)

- Hardening sicurezza/rete:
  - download LotW migrato da GET con credenziali in URL a POST HTTPS (`application/x-www-form-urlencoded`).
  - log LotW con redazione delle informazioni sensibili.
  - redirect LotW su richieste con credenziali ora strettamente limitato a HTTPS + host atteso.
  - avvio WS remoto rifiutato su bind LAN/WAN con token sotto 12 caratteri.
  - messaggio esplicito che traffico HTTP/WS e' in chiaro (consigliato LAN fidata/VPN/proxy TLS).
- Comportamento FT2/runtime:
  - Async L2 ON di default in ingresso FT2 (disattivabile utente), auto-OFF in uscita da FT2.
  - `Blocca la Freq Tx` e `Tx pari/1°` nascosti in FT2 (e forzati OFF se attivi).
  - logica pulsanti banda unificata per evitare deselezioni/re-tune anomali al re-click.
- Stabilita' Linux:
  - finestra impostazioni ridimensionata/ricentrata entro la geometria disponibile per mantenere i pulsanti sempre cliccabili.
  - decode async su thread pool dedicato con stack controllato (Linux: 16 MB) e guardie anti-overlap.
  - parametri async clampati prima delle chiamate Fortran; shutdown con attesa completamento async.
- Finestre strumenti/UI:
  - ripristinato funzionamento `View -> Ionospheric Forecast` e `View -> DX Cluster`, con sync corretto stato menu/finestra.
- Dashboard web:
  - eventi TX mantenuti durante refresh stato e prima transizione TX riportata correttamente nel feed RX/TX.
- Sicurezza codice C/C++ e pulizia piattaforma:
  - sostituite operazioni stringa non sicure nei moduli WSPR con varianti bounded (`snprintf`/append protetta).
  - esclusi dal fork i path helper specifici Windows nella configurazione build.

### Artifact Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/sperimentale): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: i pacchetti `.pkg` non vengono prodotti intenzionalmente.

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

`v1.4.2` completa la pasada de hardening y fixes runtime solicitados para uso operativo real:

- credenciales LotW fuera de la URL;
- bind remoto LAN/WAN con requisito de token mas fuerte;
- estabilidad Linux mejorada (ventana de ajustes + hilo async decode);
- comportamiento FT2 finalizado (Async L2 ON por defecto en FT2, visibilidad de controles coherente);
- ventanas `Ionospheric Forecast` y `DX Cluster` restauradas.

### Cambios Detallados (`v1.4.1 -> v1.4.2`)

- Hardening seguridad/red:
  - descarga LotW migrada de GET con credenciales en URL a POST HTTPS (`application/x-www-form-urlencoded`).
  - logs LotW con redaccion de datos sensibles.
  - redirect LotW en peticiones con credenciales ahora limitado a HTTPS + host esperado.
  - inicio WS remoto rechazado en bind LAN/WAN con token menor de 12 caracteres.
  - aviso explicito de que HTTP/WS viaja en claro (usar LAN confiable/VPN/proxy TLS).
- Comportamiento FT2/runtime:
  - Async L2 ON por defecto al entrar en FT2 (usuario puede desactivar), auto-OFF al salir de FT2.
  - `Bloquear Freq Tx` y `Tx par/1°` ocultos en FT2 (y forzados OFF si estaban activos).
  - logica de botones de banda unificada para evitar deseleccion/re-tune anomalo al reclick.
- Estabilidad Linux:
  - ventana de ajustes ajustada a la geometria disponible para mantener botones accesibles.
  - decode async en thread-pool dedicado con stack controlado (Linux: 16 MB) y guardas anti-solapamiento.
  - parametros async acotados antes de llamadas Fortran; cierre esperando fin de async.
- Ventanas UI/herramientas:
  - restaurado `View -> Ionospheric Forecast` y `View -> DX Cluster`, con sync correcto de estado menu/ventana.
- Dashboard web:
  - eventos TX preservados durante refresh de estado y primera transicion TX mostrada correctamente en el feed RX/TX.
- Seguridad de codigo C/C++ y limpieza de plataforma:
  - reemplazadas operaciones de string inseguras en modulos WSPR por variantes acotadas (`snprintf`/append protegido).
  - excluidas rutas helper especificas de Windows en esta configuracion de build del fork.

### Artefactos de Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: no se generan instaladores `.pkg`.

### Requisitos Minimos Linux

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo (8 GB recomendado)
- Espacio en disco: al menos 500 MB libres
- Runtime/software:
  - Linux con `glibc >= 2.35`
  - soporte `libfuse2` / FUSE2
  - audio ALSA, PulseAudio o PipeWire

### Si macOS bloquea el inicio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Recomendacion AppImage en Linux

Para evitar problemas por el sistema de archivos de solo lectura de AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando luego el programa desde la carpeta extraida.

Ejecutar en terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
