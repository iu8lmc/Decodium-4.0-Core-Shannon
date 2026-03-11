# Note di Documentazione (Italiano) - v1.4.4

## Scopo

Note tecniche del fork macOS Decodium con pacchettizzazione Linux AppImage.

## Contesto Release

- Release corrente: `v1.4.4`
- Ciclo aggiornamento: `v1.4.3 -> v1.4.4`
- Target: Apple Silicon Tahoe, Apple Silicon Sequoia, Apple Intel Sequoia, Apple Intel Monterey (sperimentale), Linux x86_64 AppImage

## Cambi Tecnici Principali (`v1.4.3 -> v1.4.4`)

- Introdotto sottosistema certificati DXped (`DXpedCertificate.hpp`) con verifica firma HMAC-SHA256 su payload JSON canonico.
- Introdotti controlli runtime certificato DXped:
- validita' temporale (start/end).
- autorizzazione operatore rispetto al callsign locale.
- Aggiunte azioni menu per caricamento certificato e avvio manager certificati DXped.
- Aggiunti tool Python in `tools/` e regole installazione in `CMakeLists.txt`.
- Attivazione DXped ora subordinata a certificato valido.
- Integrata auto-sequenza DXped nel flusso decode; bloccato `processMessage()` standard quando FSM DXped e' attiva.
- Async L2 in FT2 reso obbligatorio (anche lato comando remoto).
- Nuovi stati barra progressi ASYMX (`GUARD/TX/RX/IDLE`) con timer di guardia.
- Migliorie consistenza decode/UI:
- font monospazio forzato sulle pane decode.
- allineamento marker AP/qualita' corretto in coda riga.
- normalizzazione marker FT2 (`~` in `+`) nelle righe decode.
- parsing doppio click reso piu robusto rimuovendo annotazioni in coda.
- Client UDP identificato in base al nome applicazione (migliore separazione multiistanza).
- Traduzioni italiane aggiornate per stati async e messaggi Async L2 obbligatorio.

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
- [RELEASE_NOTES_v1.4.4.md](../RELEASE_NOTES_v1.4.4.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.4.md](./GITHUB_RELEASE_BODY_v1.4.4.md)
- [doc/WEBAPP_SETUP_GUIDE.it.md](./WEBAPP_SETUP_GUIDE.it.md)
- [doc/WEBAPP_SETUP_GUIDE.en-GB.md](./WEBAPP_SETUP_GUIDE.en-GB.md)
- [doc/WEBAPP_SETUP_GUIDE.es.md](./WEBAPP_SETUP_GUIDE.es.md)
