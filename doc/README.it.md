# Note di Documentazione (Italiano) - v1.4.5

## Scopo

Note tecniche del fork macOS Decodium con pacchettizzazione Linux AppImage.

## Contesto Release

- Release corrente: `v1.4.5`
- Ciclo aggiornamento: `v1.4.4 -> v1.4.5`
- Target: Apple Silicon Tahoe, Apple Silicon Sequoia, Apple Intel Sequoia, Apple Intel Monterey (sperimentale), Linux x86_64 AppImage

## Cambi Tecnici Principali (`v1.4.4 -> v1.4.5`)

- Hardening FT2 AutoCQ/sequenza QSO:
- lock partner attivo piu affidabile durante QSO.
- risposte dirette durante CQ agganciate con matching base-call.
- `73` di stazioni non partner ignorati in auto-sequenza.
- timer autolog stale/deferred filtrati in sicurezza.
- coda caller filtrata contro stazioni appena lavorate.
- Stabilita' signoff/log FT2:
- percorso deferred signoff migliorato con handoff snapshot log.
- gestione RR73/73 resa piu deterministica prima del ritorno a CQ.
- retry aggiornati (`MAX_TX_RETRIES=5`).
- Aggiornamenti decode/runtime FT2:
- colonna FT2 aggiornata con `TΔ` (tempo da ultimo TX).
- fix allineamento colonne per marker FT2 e segni meno Unicode.
- conferme async deboli preservate prima della deduplica.
- soglia decoder `nharderror` aggiornata (`35 -> 48`).
- Fix timing TX:
- sistemati ramp-up/ramp-down e latenza FT2 su TCI e soundcard.
- Fix web/UI:
- corretto avvio dashboard remota e fetch init.
- corretta apertura `Vista -> Display a cascata` (waterfall).
- ripristinato hint password web localizzato (minimo 12 caratteri).
- Compatibilita' logger UDP:
- corretto slicing payload FT2 via UDP, evitando perdita primo carattere callsign su RumLog.

## Build e Runtime

- Binario locale: `build/ft2.app/Contents/MacOS/ft2`
- Eseguibili supporto bundle: `jt9`, `wsprd`
- Backend shared memory su macOS resta `mmap` file-backed (nessun bootstrap `.pkg` richiesto).

## Requisiti Minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi
- Runtime/software:
- Linux con `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

## Avvio Consigliato AppImage

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Comando Quarantena Gatekeeper

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Riferimenti

- [CHANGELOG.md](../CHANGELOG.md)
- [RELEASE_NOTES_v1.4.5.md](../RELEASE_NOTES_v1.4.5.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.5.md](./GITHUB_RELEASE_BODY_v1.4.5.md)
- [doc/WEBAPP_SETUP_GUIDE.it.md](./WEBAPP_SETUP_GUIDE.it.md)
- [doc/WEBAPP_SETUP_GUIDE.en-GB.md](./WEBAPP_SETUP_GUIDE.en-GB.md)
- [doc/WEBAPP_SETUP_GUIDE.es.md](./WEBAPP_SETUP_GUIDE.es.md)
