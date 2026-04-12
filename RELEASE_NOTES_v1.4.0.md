# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.4.0

Date: 2026-03-08  
Scope: Consolidated fixes/features from `v1.3.8` to `v1.4.0`, release/documentation alignment, multi-platform artifacts.

## English

### Summary

`v1.4.0` consolidates the full stabilization pass completed after `v1.3.8`:

- FT2 decode stream reliability improvements (single-flow rendering, near-duplicate suppression window, safer row split handling).
- Async-L2 UX correction (control now visible only in FT2 mode).
- Remote web dashboard maturity improvements (LAN settings, username/password auth, mobile UX, clearer control surface).
- CAT/UDP/TCI hardening continuity from previous releases retained.
- Map/runtime fixes retained (optional greyline, active path distance badge, compact-screen layout behavior).
- Release/CI metadata aligned to `v1.4.0` for macOS + Linux AppImage.

### Detailed Changes (v1.3.8 -> v1.4.0)

- Decode pipeline and FT2 operational stability:
  - Added split handling for packed decode lines to avoid malformed merged rows in activity views.
  - Added near-duplicate suppression cache (5-second window) to reduce repeated close-in duplicate lines while preserving best-SNR entries.
  - Applied dedupe/splitting consistently across normal decode path and async decode completion path.
  - Reset dedupe cache on activity clear to avoid stale suppression state.
- Async/L2 behavior:
  - `Async L2` control visibility is now tied to FT2 mode only.
  - Leaving FT2 mode now auto-disables Async L2 to avoid mode-inconsistent behavior.
- Remote Web Dashboard (LAN) improvements:
  - Continued rollout of username + password auth model (`Remote user` + `Remote token`).
  - Improved mobile-first dashboard behavior and PWA install ergonomics.
  - Clarified and streamlined operator-facing UI (reduced debug-only noise in normal flow).
  - Added/kept direct controls for mode, band, RX frequency, TX enable/disable, Auto-CQ, and slot-aware caller action.
  - Waterfall remote view remains experimental and optional.
- Security/runtime continuity (already integrated before this tag and kept in this baseline):
  - CAT/Configure hardening against generic external packets forcing FT2.
  - UDP control target-id restrictions to reduce cross-app interference.
  - TCI parser/runtime guard rails (bounds/timer/null checks) retained.
  - LotW/Cloudlog HTTPS/TLS and safer URL/query handling retained.
  - Sensitive setting redaction in traces retained.
- Map/UI continuity (already integrated before this tag and kept in this baseline):
  - Optional greyline toggle in `Settings -> General`.
  - Active world-map path distance badge in km/mi according to configured units.
  - Compact top-controls refinements for smaller displays.
  - Multi-monitor geometry resilience improvements retained.

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

To avoid issues caused by AppImage read-only filesystem mode:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

### Web App Setup Guide

Detailed novice setup is available in:

- `doc/WEBAPP_SETUP_GUIDE.en-GB.md`
- `doc/WEBAPP_SETUP_GUIDE.it.md`
- `doc/WEBAPP_SETUP_GUIDE.es.md`

## Italiano

### Sintesi

`v1.4.0` consolida l'intero ciclo di stabilizzazione completato dopo `v1.3.8`:

- Affidabilita' stream decode FT2 (rendering a flusso singolo, soppressione duplicati ravvicinati, gestione robusta righe packed).
- Correzione UX Async-L2 (controllo visibile solo in modalita' FT2).
- Maturazione dashboard web remota (impostazioni LAN, autenticazione username/password, UX mobile piu' chiara).
- Continuita' hardening CAT/UDP/TCI delle release precedenti.
- Continuita' fix mappa/runtime (greyline opzionale, distanza su path attivo, layout schermi compatti).
- Metadati release/CI allineati a `v1.4.0` per macOS + Linux AppImage.

### Modifiche Dettagliate (v1.3.8 -> v1.4.0)

- Pipeline decode e stabilita' operativa FT2:
  - Aggiunta gestione split delle righe decode packed per evitare righe fuse/malformate nelle viste attivita'.
  - Aggiunta cache di soppressione near-duplicate (finestra 5 secondi) per ridurre righe duplicate ravvicinate mantenendo l'entry migliore per SNR.
  - Dedupe/split applicati in modo uniforme sia al decode normale sia al percorso di completamento async decode.
  - Reset cache dedupe su clear attivita' per evitare stato stale.
- Comportamento Async/L2:
  - Visibilita' `Async L2` ora vincolata alla sola modalita' FT2.
  - Uscendo da FT2, Async L2 viene disabilitato automaticamente per evitare comportamenti incoerenti tra modalita'.
- Migliorie Dashboard Web Remota (LAN):
  - Proseguito roll-out modello auth con username + password (`Remote user` + `Remote token`).
  - Migliorato comportamento dashboard mobile-first e installazione PWA.
  - Interfaccia operatore resa piu' pulita (riduzione indicatori debug non utili in uso normale).
  - Confermati controlli diretti per modalita', banda, frequenza RX, TX on/off, Auto-CQ e azione caller slot-aware.
  - Vista waterfall remota resta opzionale/sperimentale.
- Continuita' sicurezza/runtime (gia' integrate prima di questo tag e mantenute in baseline):
  - Hardening CAT/Configure contro forzatura FT2 da pacchetti esterni generici.
  - Restrizioni target-id sui comandi UDP controllo per ridurre interferenze cross-app.
  - Guardie TCI parser/runtime (bounds/timer/null).
  - Gestione LotW/Cloudlog HTTPS/TLS e URL/query sicure.
  - Redazione dati sensibili nei trace.
- Continuita' mappa/UI (gia' integrate prima di questo tag e mantenute in baseline):
  - Toggle greyline opzionale in `Settings -> General`.
  - Badge distanza sul path mappa attivo in km/mi secondo unita' configurate.
  - Rifiniture top-controls compatti su display piccoli.
  - Miglioramenti resilienza multi-monitor mantenuti.

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

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

### Guida Web App

Guida dettagliata per neofiti disponibile in:

- `doc/WEBAPP_SETUP_GUIDE.it.md`
- `doc/WEBAPP_SETUP_GUIDE.en-GB.md`
- `doc/WEBAPP_SETUP_GUIDE.es.md`

## Espanol

### Resumen

`v1.4.0` consolida todo el ciclo de estabilizacion completado tras `v1.3.8`:

- Mejora de fiabilidad del flujo de decodificacion FT2 (render en flujo unico, supresion de duplicados cercanos, gestion robusta de lineas packed).
- Correccion UX de Async-L2 (visible solo en modo FT2).
- Maduracion del dashboard web remoto (ajustes LAN, autenticacion usuario/password, UX movil mas clara).
- Continuidad del hardening CAT/UDP/TCI ya integrado.
- Continuidad de fixes mapa/runtime (greyline opcional, distancia en ruta activa, layout compacto).
- Metadatos release/CI alineados a `v1.4.0` para macOS + Linux AppImage.

### Cambios Detallados (v1.3.8 -> v1.4.0)

- Pipeline de decode y estabilidad FT2:
  - Se anade split de lineas decode packed para evitar filas fusionadas/malformadas en las vistas de actividad.
  - Se anade cache de supresion near-duplicate (ventana 5 segundos) para reducir repetidos cercanos manteniendo la entrada con mejor SNR.
  - Dedupe/split aplicados de forma consistente en decode normal y en ruta async decode completion.
  - Reset de cache dedupe al limpiar actividad para evitar estado stale.
- Comportamiento Async/L2:
  - `Async L2` ahora solo es visible en modo FT2.
  - Al salir de FT2, Async L2 se desactiva automaticamente para evitar comportamiento inconsistente entre modos.
- Mejoras del Dashboard Web Remoto (LAN):
  - Continuacion del modelo de autenticacion con usuario + password (`Remote user` + `Remote token`).
  - Mejor comportamiento mobile-first e instalacion PWA.
  - Interfaz mas limpia para operador (menos ruido de debug en uso normal).
  - Se mantienen controles directos para modo, banda, frecuencia RX, TX on/off, Auto-CQ y accion slot-aware sobre caller.
  - Vista waterfall remota sigue siendo opcional/experimental.
- Continuidad seguridad/runtime (ya integrado antes de este tag y mantenido):
  - Hardening CAT/Configure para evitar forzado FT2 por paquetes externos genericos.
  - Restricciones target-id en comandos UDP de control.
  - Guardas parser/runtime TCI (bounds/timer/null).
  - Manejo seguro HTTPS/TLS para LotW/Cloudlog.
  - Redaccion de datos sensibles en trazas.
- Continuidad mapa/UI (ya integrado antes de este tag y mantenido):
  - Toggle greyline opcional en `Settings -> General`.
  - Badge de distancia en ruta activa del mapa (km/mi segun unidad).
  - Ajustes de controles superiores compactos en pantallas pequenas.
  - Mejoras de resiliencia multi-monitor mantenidas.

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

Para evitar problemas por el sistema de archivos de solo lectura de AppImage:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

### Guia Web App

Guia detallada para usuarios principiantes disponible en:

- `doc/WEBAPP_SETUP_GUIDE.es.md`
- `doc/WEBAPP_SETUP_GUIDE.en-GB.md`
- `doc/WEBAPP_SETUP_GUIDE.it.md`
