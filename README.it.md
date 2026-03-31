# Decodium (Fork macOS/Linux) - 1.5.9

Questo repository contiene il fork Decodium mantenuto per macOS e Linux AppImage.

- Release stabile corrente: `1.5.9`
- Ciclo aggiornamento: `1.5.8 -> 1.5.9`

## Novita' 1.5.9 (`1.5.8 -> 1.5.9`)

- corretto il comportamento intermittente Linux FT2/FT4 in cui la radio entrava in TX ma il payload partiva molto dopo, a volte quasi alla fine della finestra di trasmissione.
- rimosso dal path TX standard FT2/FT4 Linux il contenzioso inutile con il mutex globale del runtime Fortran, cosi' un decode lento non ritarda piu' l'uscita del payload.
- aggiunti start immediato post-waveform e fallback CAT/PTT su Linux FT2/FT4 quando il feedback rig arriva in ritardo.
- ridotta la latenza della coda audio TX Linux, selezionata una categoria audio a bassa latenza, e portato FT2 Linux a zero ritardo/zero lead-in aggiuntivo sul path waveform standard.
- cambiato lo stop Linux FT2/FT4 per seguire la fine reale di audio/modulator e non solo la scadenza teorica dello slot.
- limitata la scrittura dei dump waveform FT2/FT4 ai soli casi in cui il debug log e' attivo, riducendo I/O su disco durante i TX normali.
- corretto un crash macOS in chiusura causato dall'ordine di ownership/distruzione dei widget Qt in `MainWindow`.
- corretto il colore della UI `Band Hopping`: `QSOs to upload` non diventa piu' rosso insieme al pulsante `Band Hopping`.
- aggiornata la nuova icona FT2 per gli artifact macOS e Linux.
- allineati metadati versione locali, default workflow, documentazione release e note GitHub alla semver `1.5.9`.

## Target Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / sperimentale)
- Linux x86_64 AppImage

## Asset Release

- `decodium3-ft2-1.5.9-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage.sha256.txt`

## Requisiti Minimi Linux

Hardware:

- CPU `x86_64` con supporto SSE2
- dual-core 2.0 GHz o meglio
- minimo 4 GB RAM (8 GB consigliati)
- almeno 500 MB liberi su disco
- display 1280x800 o superiore consigliato
- hardware audio/CAT/seriale/USB adatto all'operativita' weak-signal radioamatoriale

Software:

- Linux `x86_64` con `glibc >= 2.35`
- `libfuse2` / supporto FUSE2 se si vuole montare direttamente l'AppImage
- ALSA, PulseAudio o PipeWire
- sessione desktop X11 o Wayland capace di eseguire AppImage Qt5
- permessi di accesso ai device seriali/USB (`dialout`, `uucp` o equivalente distro) se si usa CAT o radio esterna
- accesso rete consigliato per NTP, PSK Reporter, DX Cluster, updater e workflow online

## Avvio

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Documentazione Collegata

- [RELEASE_NOTES_1.5.9.md](RELEASE_NOTES_1.5.9.md)
- [doc/GITHUB_RELEASE_BODY_1.5.9.md](doc/GITHUB_RELEASE_BODY_1.5.9.md)
- [CHANGELOG.md](CHANGELOG.md)
