# Decodium (Fork macOS/Linux) - 1.5.7

Questo repository contiene il fork Decodium mantenuto per macOS e Linux AppImage.

- Release stabile corrente: `1.5.7`
- Ciclo aggiornamento: `1.5.6 -> 1.5.7`

## Novita' 1.5.7 (`1.5.6 -> 1.5.7`)

- aggiunto un filtro di plausibilita' FT2 type-4 che evita la comparsa in Band Activity di payload rumorosi trasformati in nominativi senza senso.
- corretto il doppio click FT2 nella Band Activity per i messaggi standard `CQ` / `QRZ`, cosi' nominativi validi come `D2UY`, `K1RZ` e `KL7J` si armano in modo affidabile.
- aggiunta copertura mirata in `test_qt_helpers` per forme valide special-event/slash e per output FT2 garbage da rigettare.
- ripristinato il target Linux `wsprd`, cosi' i job Linux/AppImage tornano a pubblicare il set binario atteso.
- allineati metadati versione locali, default workflow, documentazione release e note GitHub alla semver `1.5.7`.
- mantenuto il layout/cartelle macOS gia' validato nell'ultimo deploy riuscito.

## Target Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / sperimentale)
- Linux x86_64 AppImage

## Asset Release

- `decodium3-ft2-1.5.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage.sha256.txt`

## Requisiti Minimi Linux

Hardware:

- CPU `x86_64`
- dual-core 2.0 GHz o meglio
- minimo 4 GB RAM (8 GB consigliati)
- almeno 500 MB liberi su disco
- display 1280x800 o superiore consigliato

Software:

- Linux `x86_64` con `glibc >= 2.35`
- `libfuse2` / supporto FUSE2 se si vuole montare direttamente l'AppImage
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

- [RELEASE_NOTES_1.5.7.md](RELEASE_NOTES_1.5.7.md)
- [doc/GITHUB_RELEASE_BODY_1.5.7.md](doc/GITHUB_RELEASE_BODY_1.5.7.md)
- [CHANGELOG.md](CHANGELOG.md)
