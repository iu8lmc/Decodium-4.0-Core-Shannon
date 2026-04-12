# Release Notes / Note di Rilascio - Fork v1.3.5

Date: 2026-03-01  
Scope: network/security hardening, FT2 averaging path, NTP reliability, CTY startup flow, release alignment.

## English

### Summary

- Network/security hardening pass completed across Cloudlog, LotW, UDP controller, ADIF/logging, and downloader paths.
- FT2 decoder path extended with multi-period averaging and averaged retry flow.
- NTP bootstrap/recovery behavior significantly improved for real-world constrained networks.
- Startup CTY handling now downloads immediately when missing and uses HTTPS endpoint.

### Detailed Changes

- Cloudlog:
  - HTTPS endpoint normalization for auth/upload.
  - JSON payload assembly hardened.
  - Blocking modal dialog in network callback replaced with non-blocking `open()`.
- LotW:
  - TLS downgrade to HTTP removed.
  - Cross-thread progress/load-finished emissions queued safely.
- MessageClient UDP control:
  - trusted sender enforcement added,
  - destination id filtering added,
  - untrusted packet warning path added.
- ADIF/log hardening:
  - ADIF value sanitization before serialization (`LogBook`),
  - mutex-protected ADIF append path (`WorkedBefore`),
  - dynamic program id/version fields.
- FileDownload:
  - stale-reply guard and safer reply cleanup handling.
- PSKReporter:
  - timer arithmetic bug fixed (`(MIN_SEND_INTERVAL + 1) * 1000`),
  - global spot cache synchronized with mutex.
- Main settings trace:
  - sensitive keys redacted (password/token/api key/cloudlog/lotw).
- Windows utility:
  - `killbyname` now uses dynamic process list buffer sizing.
- FT2 decoder:
  - module-level averaging buffer,
  - averaged decode attempt when single period fails,
  - averaging reset hook (`ft2_clravg`) integrated into decode clear path.
- NTP:
  - centralized enable/disable state path in UI,
  - server selection prioritizes reliable bootstrap servers,
  - single-server bootstrap confirmations,
  - IPv4/IPv6-mapped normalization for query/reply matching,
  - bootstrap RTT tolerance + timeout tuning,
  - auto fallback pin to `time.apple.com` after repeated UDP no-response,
  - improved status bar rendering during weak-sync hold.
- CTY:
  - startup auto-download immediate when `cty.dat` is missing,
  - stale-only delayed refresh retained,
  - CTY URL moved to HTTPS.

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Note: `.pkg` installers are intentionally not produced.

### Linux minimum requirements

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free
- Runtime/software:
  - `glibc >= 2.35`
  - `libfuse2` / FUSE2 support
  - ALSA, PulseAudio, or PipeWire audio stack

### If macOS blocks startup

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Italiano

### Sintesi

- Completato hardening rete/sicurezza su Cloudlog, LotW, controllo UDP, ADIF/log e downloader.
- Esteso il percorso decoder FT2 con media multi-periodo e retry su media.
- Comportamento bootstrap/recovery NTP migliorato in modo significativo per reti reali con vincoli.
- Gestione CTY in startup aggiornata con download immediato quando mancante e endpoint HTTPS.

### Modifiche Dettagliate

- Cloudlog:
  - normalizzazione endpoint HTTPS per auth/upload.
  - composizione payload JSON irrobustita.
  - dialog modale bloccante in callback rete sostituito con `open()` non bloccante.
- LotW:
  - rimosso downgrade TLS->HTTP.
  - emissioni progress/load-finished cross-thread accodate in sicurezza.
- Controllo UDP MessageClient:
  - enforcement sender trusted,
  - filtro id destinazione,
  - warning su pacchetti non trusted.
- Hardening ADIF/log:
  - sanitizzazione valori ADIF prima della serializzazione (`LogBook`),
  - append ADIF protetto da mutex (`WorkedBefore`),
  - campi program id/versione dinamici.
- FileDownload:
  - guardia reply stale e cleanup reply piu' sicuro.
- PSKReporter:
  - corretto bug aritmetico timer (`(MIN_SEND_INTERVAL + 1) * 1000`),
  - cache spot globale sincronizzata via mutex.
- Trace impostazioni:
  - redazione chiavi sensibili (password/token/api key/cloudlog/lotw).
- Utility Windows:
  - `killbyname` ora usa buffer dinamico per lista processi.
- Decoder FT2:
  - buffer media a livello modulo,
  - tentativo decode su media quando il single-period fallisce,
  - hook reset media (`ft2_clravg`) integrato nel clear decode.
- NTP:
  - percorso stato enable/disable UI centralizzato,
  - selezione server con priorita' bootstrap affidabile,
  - conferme bootstrap a singolo server,
  - normalizzazione IPv4/IPv6-mapped per matching query/reply,
  - tuning timeout + tolleranza RTT in bootstrap,
  - fallback auto pin su `time.apple.com` dopo no-response UDP ripetuti,
  - rendering status bar migliorato durante weak-sync hold.
- CTY:
  - auto-download startup immediato se `cty.dat` manca,
  - refresh ritardato mantenuto solo se stale,
  - URL CTY migrato a HTTPS.

### Artifact release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/sperimentale): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: i pacchetti `.pkg` non vengono prodotti intenzionalmente.

### Requisiti minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi
- Runtime/software:
  - `glibc >= 2.35`
  - supporto `libfuse2` / FUSE2
  - stack audio ALSA, PulseAudio o PipeWire

### Se macOS blocca l'avvio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```
