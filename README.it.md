# Decodium (Fork macOS/Linux) - 1.5.5

Questo repository contiene il fork Decodium mantenuto per macOS e Linux AppImage.

- Release stabile corrente: `1.5.5`
- Ciclo aggiornamento: `1.5.4 -> 1.5.5`

## Novita' 1.5.5 (`1.5.4 -> 1.5.5`)

- corretta la gestione nativa macOS di `Preferenze...`, cosi' le euristiche sui menu non possono piu' aprire azioni sbagliate nelle UI tradotte.
- reso il dialog `Settings` scrollabile anche su macOS, mantenendo raggiungibili i pulsanti finali su display piu' piccoli e con layout localizzati piu' lunghi.
- aggiunto il log persistente `jt9_subprocess.log` e una diagnostica piu' ricca di stdout/stderr per analizzare i crash del subprocess FT2.
- aggiornato l'ADIF FT2 a `MODE=MFSK` + `SUBMODE=FT2` con migrazione automatica dei vecchi record `MODE=FT2` e backup del file originale.
- migliorato il recupero dell'audio in avvio/monitor, cosi' non serve piu' riaprire `Settings > Audio` per risvegliare lo stream.
- ritirata dalla release pubblica la UI RTTY sperimentale e incompleta, lasciandola fuori dal percorso utente.

## Target Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / sperimentale)
- Linux x86_64 AppImage

## Asset Release

- `decodium3-ft2-1.5.5-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage.sha256.txt`

## Requisiti Minimi Linux

Hardware:

- CPU `x86_64`
- dual-core 2.0 GHz o meglio
- minimo 4 GB RAM (8 GB consigliati)
- almeno 500 MB liberi su disco
- display 1280x800 o superiore consigliato

Software:

- Linux `x86_64` con `glibc >= 2.35`
- `libfuse2` / supporto FUSE2
- ALSA, PulseAudio o PipeWire
- sessione desktop capace di eseguire AppImage Qt5

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

- [RELEASE_NOTES_1.5.5.md](RELEASE_NOTES_1.5.5.md)
- [doc/GITHUB_RELEASE_BODY_1.5.5.md](doc/GITHUB_RELEASE_BODY_1.5.5.md)
- [CHANGELOG.md](CHANGELOG.md)
