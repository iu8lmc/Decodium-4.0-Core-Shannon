# Decodium Fork v1.3.5 (base: Decodium v3.0 SE Raptor)

## English

Release focused on network/security hardening, FT2 multi-period decoding improvements, and robust NTP/CTY startup behavior.

### Highlights

- Cloudlog/LotW transport hardening:
  - Cloudlog endpoints normalized to HTTPS with safer request handling.
  - LotW no longer downgrades TLS to HTTP.
- UDP control hardening:
  - trusted sender enforcement + destination-id filtering in MessageClient.
- ADIF/log integrity hardening:
  - ADIF field sanitization,
  - mutex-protected append paths,
  - dynamic program-id metadata.
- FT2 decoder upgrades:
  - multi-period averaging (EMA buffer),
  - averaged decode retry path when single-period decode fails.
- NTP reliability:
  - centralized UI enable/disable state flow,
  - robust bootstrap including single-server confirmation mode,
  - automatic fallback pin to `time.apple.com` after repeated UDP no-response.
- CTY startup behavior:
  - immediate auto-download when `cty.dat` is missing,
  - stale refresh policy retained,
  - CTY source migrated to HTTPS.
- `.pkg` installers remain disabled (not required after mmap shared-memory migration).

### Linux minimum requirements

- Architecture: `x86_64`
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Runtime: `glibc >= 2.35`, `libfuse2`/FUSE2, ALSA/PulseAudio/PipeWire

### macOS quarantine command

If Gatekeeper blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Artifacts

- `decodium3-ft2-v1.3.5-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.5-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.5-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.5-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.5-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.5-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.5-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.5-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.5-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.5-macos-monterey-x86_64.dmg` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.5-macos-monterey-x86_64.zip` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.5-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.5-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.5-linux-x86_64.AppImage.sha256.txt`

Note: `.pkg` installers are not produced.

---

## Italiano

Release focalizzata su hardening rete/sicurezza, miglioramenti decode FT2 multi-periodo e robustezza NTP/CTY in avvio.

### Highlight

- Hardening trasporti Cloudlog/LotW:
  - endpoint Cloudlog normalizzati a HTTPS con gestione richieste piu' sicura.
  - LotW non effettua piu' downgrade TLS->HTTP.
- Hardening controllo UDP:
  - enforcement sender trusted + filtro id destinazione in MessageClient.
- Hardening integrita' ADIF/log:
  - sanitizzazione campi ADIF,
  - append protetto da mutex,
  - metadati program-id dinamici.
- Upgrade decoder FT2:
  - averaging multi-periodo (buffer EMA),
  - retry decode su media quando il single-period fallisce.
- Affidabilita' NTP:
  - flusso stato enable/disable UI centralizzato,
  - bootstrap robusto con modalita' conferma single-server,
  - fallback auto pin su `time.apple.com` dopo no-response UDP ripetuti.
- Comportamento startup CTY:
  - auto-download immediato quando `cty.dat` manca,
  - policy refresh stale mantenuta,
  - sorgente CTY migrata a HTTPS.
- Gli installer `.pkg` restano disabilitati (non necessari dopo migrazione shared-memory a mmap).

### Requisiti minimi Linux

- Architettura: `x86_64`
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Runtime: `glibc >= 2.35`, `libfuse2`/FUSE2, ALSA/PulseAudio/PipeWire

### Comando quarantena macOS

Se Gatekeeper blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Nota: i pacchetti `.pkg` non vengono prodotti.
