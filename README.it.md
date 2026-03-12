# Decodium 3 FT2 (Fork macOS) - v1.4.5

Fork mantenuto da **Salvatore Raccampo 9H1SR**.

Per la panoramica bilingue, vedere [README.md](README.md).

## Novita' v1.4.5 (`v1.4.4 -> v1.4.5`)

- Correzioni FT2 AutoCQ e macchina stati QSO:
- lock partner attivo piu rigoroso durante QSO (override manuale solo con Ctrl/Shift).
- risposta diretta durante CQ agganciata in modo piu affidabile (match su base callsign).
- i `73` non del partner non forzano piu chiusure errate.
- timer di autolog differiti/stale ora ignorati in sicurezza.
- coda caller protetta da duplicati appena lavorati dopo logging.
- Affidabilita' signoff/log FT2:
- migliorato percorso deferred signoff FT2 con snapshot autolog piu robusto.
- completamento RR73/73 gestito meglio prima del ritorno a CQ.
- retry AutoCQ aggiornati (`MAX_TX_RETRIES=5`) con reset pulito al cambio DX.
- Migliorie decode/runtime FT2:
- colonna FT2 ora mostra `TΔ` (tempo da ultimo TX) al posto di DT.
- corretto allineamento colonne FT2 (`~`, segni meno Unicode, prefisso a larghezza fissa).
- conferme async deboli preservate prima della soppressione near-duplicate.
- hardening decoder con soglia `nharderror` rilassata (`35 -> 48`).
- Correzioni timing TX:
- sistemati ramp-up/ramp-down e latenza TX FT2 su percorsi TCI e soundcard.
- Correzioni web/UI:
- fix startup dashboard remota e inizializzazione fetch.
- fix apertura `Vista -> Display a cascata` (waterfall).
- ripristinato hint password web localizzato (minimo 12 caratteri).
- Fix compatibilita' logger UDP FT2:
- corretto slicing payload decode FT2 via UDP: logger esterni (RumLog) non perdono piu la prima lettera del callsign.

## Target release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (sperimentale / best effort)
- Linux x86_64 AppImage

## Requisiti minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi (AppImage + log + impostazioni)
- Runtime/software:
- Linux con `glibc >= 2.35`
- supporto `libfuse2` / FUSE2
- stack audio ALSA, PulseAudio o PipeWire
- Integrazione stazione: hardware CAT/audio secondo setup radio

## Avvio consigliato AppImage su Linux

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Comando macOS (quarantena)

Se Gatekeeper blocca l'avvio, eseguire:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Build locale

```bash
cmake --build build -j6
./build/ft2.app/Contents/MacOS/ft2
```

## Documentazione

- [README.en-GB.md](README.en-GB.md)
- [README.es.md](README.es.md)
- [RELEASE_NOTES_v1.4.5.md](RELEASE_NOTES_v1.4.5.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.5.md](doc/GITHUB_RELEASE_BODY_v1.4.5.md)
- [doc/README.it.md](doc/README.it.md)
