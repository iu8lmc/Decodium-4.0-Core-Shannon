# Note di Documentazione (Italiano) - v1.4.6

## Scopo

Note tecniche del fork macOS Decodium con pacchettizzazione Linux AppImage.

## Contesto Release

- Release corrente: `v1.4.6`
- Ciclo aggiornamento: `v1.4.5 -> v1.4.6`
- Target: Apple Silicon Tahoe, Apple Silicon Sequoia, Apple Intel Sequoia, Apple Intel Monterey (sperimentale), Linux x86_64 AppImage

## Cambi Tecnici Principali (`v1.4.5 -> v1.4.6`)

- Hardening FT2/AutoCQ/sequenza QSO:
- ripristinato comportamento FIFO coda caller della baseline v1.3.8.
- lock partner attivo reso piu robusto durante QSO in corso.
- flusso deferred retry RR73/73 esteso in modo coerente su FT2/FT8/FT4/FST4/Q65/MSK144.
- matching partner su token payload normalizzati migliorato su formati decode edge.
- Stabilita' signoff/log:
- recovery contesto deferred autolog migliorato dopo finestre retry.
- ridotti i casi di logging prematuro/senza conferma partner.
- ridotte le attese log tardive causate da stati pending stale.
- Continuita' decode/pannello Frequenza Rx:
- estrazione payload decode resa robusta con marker variabile/assente.
- ridotti casi in cui messaggi validi partner restavano solo in Attivita' di Banda.
- Fix UI/runtime:
- corretto hide/show `Vista -> World Map` su macOS.
- migliorata logica dimensionamento splitter per pannelli decode/map secondari.
- su Linux le tab impostazioni ora sono wrappate in scroll area (azioni sempre raggiungibili).
- aggiunto refresh startup stream audio su macOS per periferiche gia' selezionate.
- Dashboard remota/web:
- aggiunto endpoint comando remoto `set_tx_frequency`.
- web app con set Rx+Tx, set Rx/set Tx separati, preset frequenze per modo.
- mantenuto hint password localizzato con minimo 12 caratteri (IT/EN).

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
- [RELEASE_NOTES_v1.4.6.md](../RELEASE_NOTES_v1.4.6.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.6.md](./GITHUB_RELEASE_BODY_v1.4.6.md)
- [doc/WEBAPP_SETUP_GUIDE.it.md](./WEBAPP_SETUP_GUIDE.it.md)
- [doc/WEBAPP_SETUP_GUIDE.en-GB.md](./WEBAPP_SETUP_GUIDE.en-GB.md)
- [doc/WEBAPP_SETUP_GUIDE.es.md](./WEBAPP_SETUP_GUIDE.es.md)
