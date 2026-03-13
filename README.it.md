# Decodium 3 FT2 (Fork macOS) - v1.4.6

Fork mantenuto da **Salvatore Raccampo 9H1SR**.

Per la panoramica bilingue, vedere [README.md](README.md).

## Novita' v1.4.6 (`v1.4.5 -> v1.4.6`)

- Hardening AutoCQ e macchina stati QSO:
- ripristinato comportamento FIFO stabile della coda caller (baseline logica v1.3.8).
- lock partner attivo piu rigoroso durante QSO in corso, evitando takeover involontari.
- gestione retry deferred RR73/73 estesa in modo coerente a FT2, FT8, FT4, FST4, Q65 e MSK144.
- migliorato matching partner su token payload normalizzati (anche con formati decode borderline).
- Correttezza signoff/log:
- recuperato contesto deferred autolog dopo finestre retry.
- ridotti casi di logging forzato senza conferma reale del partner.
- ridotte casistiche di log tardivo dovute a stati pending stale.
- Continuita' decode e pannello Frequenza Rx:
- estrazione payload decode resa piu robusta con marker variabile/assente.
- ridotti i casi in cui risposte valide restavano solo su Attivita' di Banda senza seguire il flusso centrale Rx.
- Correzioni UI/runtime desktop:
- corretto toggle `Vista -> World Map` su macOS (la mappa ora segue lo stato menu).
- migliorata gestione layout splitter/pannelli secondari su Linux/macOS.
- su Linux, tab impostazioni troppo lunghe ora scorrono in `QScrollArea` (pulsante OK sempre raggiungibile).
- su macOS aggiunto refresh automatico stream audio in avvio per evitare reload manuale periferiche.
- Aggiornamenti dashboard remota/web app:
- aggiunto supporto completo al comando remoto `set_tx_frequency`.
- webapp aggiornata con: set combinato Rx+Tx, set Rx/set Tx separati, preset frequenze per modo (save/apply).
- mantenuto hint password localizzato con minimo 12 caratteri (IT/EN).

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
- [RELEASE_NOTES_v1.4.6.md](RELEASE_NOTES_v1.4.6.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.6.md](doc/GITHUB_RELEASE_BODY_v1.4.6.md)
- [doc/README.it.md](doc/README.it.md)
