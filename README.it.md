# Decodium 3 FT2 (Fork macOS) - v1.4.4

Fork mantenuto da **Salvatore Raccampo 9H1SR**.

Per la panoramica bilingue, vedere [README.md](README.md).

## Novita' v1.4.4 (`v1.4.3 -> v1.4.4`)

- Aggiunto sistema certificati DXpedition (`.dxcert`) con verifica firma HMAC-SHA256 su payload canonico.
- Aggiunti controlli validita' certificato (finestra temporale) e autorizzazione operatore in base al callsign locale.
- Aggiunte nuove azioni menu DXped:
- `Load DXped Certificate...`
- `DXped Certificate Manager...`
- Aggiunti tool DXped in `tools/` e installazione nei pacchetti release.
- DXped mode ora richiede certificato valido: attivazione bloccata se certificato assente/non valido/scaduto/non autorizzato.
- Migliorata la logica runtime DXped:
- bloccato il flusso standard `processMessage()` quando DXped mode e' attivo, per evitare collisioni con AutoSeq.
- `dxpedAutoSequence` ora richiamato direttamente nei percorsi decode.
- fallback CQ migliorato: se `tx5` e' vuoto viene specchiato da `tx6`.
- Async L2 in FT2 ora obbligatorio:
- ON forzato in FT2 e OFF forzato fuori FT2.
- tentativi locali/remoti di disabilitarlo in FT2 vengono ignorati.
- Migliorie ASYMX/progress bar:
- nuovi stati `GUARD`, `TX`, `RX`, `IDLE` con colori dedicati.
- guardia TX di 300 ms prima del primo auto-TX in FT2.
- reset esplicito stato buffer/counter/timer async nelle transizioni toggle.
- Migliorata correttezza visualizzazione decode:
- font monospazio forzato nelle pane decode per preservare allineamenti a colonne.
- marker AP/qualita' spostato in coda riga per evitare disallineamento colonna destra.
- normalizzazione marker FT2 (`~` in `+`) nei decode normali e async.
- doppio click su decode reso piu robusto: rimozione annotazioni a destra prima del parsing.
- Migliorata identificazione UDP multiistanza:
- client id derivato dal nome applicazione (distinzione migliore tra istanze parallele).
- Aggiunte traduzioni italiane per stati async e messaggi "Async L2 obbligatorio".
- Allineati metadati release/workflow/documentazione a `v1.4.4`.

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
- [RELEASE_NOTES_v1.4.4.md](RELEASE_NOTES_v1.4.4.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.4.md](doc/GITHUB_RELEASE_BODY_v1.4.4.md)
- [doc/README.it.md](doc/README.it.md)
