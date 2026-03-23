# Note di Documentazione (Italiano) - 1.5.5

Questo indice raccoglie la documentazione di rilascio del ciclo corrente del fork.

- Release corrente: `1.5.5`
- Ciclo aggiornamento: `1.5.4 -> 1.5.5`
- Focus principale: affidabilita' di Preferenze su macOS in tutte le lingue UI, Settings scrollabile su macOS, diagnostica subprocess FT2, migrazione ADIF FT2, recupero audio all'avvio e ritiro della UI RTTY sperimentale dal percorso pubblico.

## Cambi Tecnici Principali (`1.5.4 -> 1.5.5`)

- corretto il routing dei ruoli menu nativi su macOS, cosi' solo le voci previste usano i ruoli speciali del sistema, indipendentemente dalla lingua tradotta.
- rese scrollabili anche su macOS le pagine di `Settings`, mantenendo raggiungibile `OK`.
- aggiunto `jt9_subprocess.log` con tracing persistente di avvio/errori/terminazione del decoder FT2 e popup diagnostici piu' ricchi.
- sostituiti i crash FT2 opachi con percorsi di reporting piu' chiari per shared memory e stdout/stderr.
- portato l'ADIF FT2 allo stile ADIF 3.17 `MODE=MFSK` + `SUBMODE=FT2`, con migrazione automatica dello storico e backup.
- migliorato il recupero audio in avvio e all'attivazione del monitor con reopen stile Settings piu' health check.
- rimossa dal percorso utente la UI RTTY incompleta per questa release pubblica.

## Artifact Release

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

- `x86_64`, dual-core 2.0 GHz+
- 4 GB RAM minima (8 GB consigliati)
- 500 MB liberi su disco
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

## Guida Avvio

Quarantena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Flusso Linux AppImage extract-run:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## File Collegati

- [README.md](../README.md)
- [README.en-GB.md](../README.en-GB.md)
- [README.it.md](../README.it.md)
- [README.es.md](../README.es.md)
- [RELEASE_NOTES_1.5.5.md](../RELEASE_NOTES_1.5.5.md)
- [doc/GITHUB_RELEASE_BODY_1.5.5.md](./GITHUB_RELEASE_BODY_1.5.5.md)
- [CHANGELOG.md](../CHANGELOG.md)
