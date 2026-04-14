# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork 1.5.7

Scope: update cycle from `1.5.6` to `1.5.7`.

## English

`1.5.7` focuses on FT2 decode quality, FT2 operator workflow from Band Activity, and keeping the Linux release pipeline healthy while preserving the macOS layout already validated by the previous successful deploy.

### Detailed Changes (`1.5.6 -> 1.5.7`)

- added a plausibility filter in `Detector/FtxFt2Stage7.cpp` for FT2 type-4 decodes so clearly implausible callsign-like payloads are rejected before they are accepted into the active decode path.
- this removes nonsense outputs such as `CAYOBTYZCV0`, `7SVPAYTXTIK`, `3K6TLT33XGQ`, `6PWE9JEL80E`, and similar noise-derived payloads that looked valid only because of base38 decoding.
- exported `ftx_ft2_message_is_plausible_c` and added regression coverage in `tests/test_qt_helpers.cpp` for valid special-event/slash cases and invalid garbage examples.
- fixed FT2 Band Activity double-click selection in `widgets/mainwindow.cpp` for standard `CQ` / `QRZ` messages and for direct clicked callsign tokens inside FT2 rows, so valid callers such as `D2UY`, `K1RZ`, `KL7J`, and `N7XR` behave consistently whether selected from Band Activity or the map.
- extended the FT2 plausibility filter to reject ghost free-text decodes that masquerade as callsign pairs, such as `M9B ZNWF6WH7V`, while preserving legitimate free-text traffic like `TNX BOB 73 GL`.
- restored the Linux `wsprd` target/build path so Linux release and AppImage workflows publish the complete expected runtime set again.
- hardened the macOS release packaging script with isolated DMG staging and `hdiutil create` retries so transient `Resource busy` failures on the Monterey Intel release path do not abort the asset build.
- aligned local version metadata, workflow defaults, readmes, docs, changelog, release notes, and GitHub release body to `1.5.7`.

### Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage

### Linux Minimum Requirements

Hardware:

- `x86_64`
- dual-core 2.0 GHz or better
- 4 GB RAM minimum
- 500 MB free disk space

Software:

- `glibc >= 2.35`
- `libfuse2`
- ALSA, PulseAudio, or PipeWire

### Startup Guidance

macOS quarantine workaround:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Linux AppImage extract-run flow:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

`1.5.7` e' focalizzata sulla qualita' del decode FT2, sul workflow operatore FT2 dalla Band Activity, e sul mantenimento sano della pipeline release Linux, preservando il layout macOS gia' validato dal precedente deploy riuscito.

### Modifiche Dettagliate (`1.5.6 -> 1.5.7`)

- aggiunto in `Detector/FtxFt2Stage7.cpp` un filtro di plausibilita' per i decode FT2 type-4, cosi' payload chiaramente implausibili simili a nominativi vengono rigettati prima di entrare nel path decode attivo.
- questo elimina output senza senso come `CAYOBTYZCV0`, `7SVPAYTXTIK`, `3K6TLT33XGQ`, `6PWE9JEL80E` e payload simili derivati dal rumore che apparivano validi solo per via della codifica base38.
- esportato `ftx_ft2_message_is_plausible_c` e aggiunta copertura di regressione in `tests/test_qt_helpers.cpp` per casi validi special-event/slash e per esempi garbage non validi.
- corretto in `widgets/mainwindow.cpp` il doppio click FT2 nella Band Activity per i messaggi standard `CQ` / `QRZ` e per il nominativo cliccato direttamente dentro una riga FT2, armando direttamente il caller selezionato, cosi' nominativi validi come `D2UY`, `K1RZ`, `KL7J` e `N7XR` si comportano in modo coerente sia dalla Band Activity sia dalla mappa.
- esteso il filtro di plausibilita' FT2 per rigettare anche ghost free-text che si mascherano da coppie di nominativi, come `M9B ZNWF6WH7V`, preservando pero' traffico free-text legittimo come `TNX BOB 73 GL`.
- ripristinato il target/percorso Linux `wsprd`, cosi' i workflow Linux release e AppImage tornano a pubblicare il set runtime completo atteso.
- reso piu' robusto lo script di packaging release macOS con staging DMG isolato e retry di `hdiutil create`, cosi' fallimenti transitori `Resource busy` sul percorso Monterey Intel non bloccano la build degli asset.
- allineati a `1.5.7` metadati versione locali, default workflow, readme, documentazione, changelog, note release e body GitHub.

### Target Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / sperimentale)*
- Linux x86_64 AppImage

### Requisiti Minimi Linux

Hardware:

- `x86_64`
- dual-core 2.0 GHz o meglio
- minimo 4 GB RAM
- 500 MB liberi su disco

Software:

- `glibc >= 2.35`
- `libfuse2`
- ALSA, PulseAudio o PipeWire

### Guida Avvio

Workaround quarantena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Flusso Linux AppImage extract-run:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Espanol

`1.5.7` se centra en la calidad del decode FT2, en el workflow operativa FT2 desde Band Activity, y en mantener sana la pipeline release Linux, preservando el layout macOS ya validado por el deploy correcto anterior.

### Cambios Detallados (`1.5.6 -> 1.5.7`)

- anadido en `Detector/FtxFt2Stage7.cpp` un filtro de plausibilidad para los decodes FT2 type-4, de forma que payloads claramente implausibles con aspecto de nominativo se rechacen antes de entrar en el camino activo de decode.
- esto elimina salidas sin sentido como `CAYOBTYZCV0`, `7SVPAYTXTIK`, `3K6TLT33XGQ`, `6PWE9JEL80E` y payloads similares derivados del ruido que solo parecian validos por la codificacion base38.
- exportado `ftx_ft2_message_is_plausible_c` y anadida cobertura de regresion en `tests/test_qt_helpers.cpp` para casos validos special-event/slash y para ejemplos garbage no validos.
- corregido en `widgets/mainwindow.cpp` el doble click FT2 en Band Activity para mensajes estandar `CQ` / `QRZ` y para el indicativo clicado directamente dentro de una linea FT2, armando directamente el caller seleccionado, de modo que llamadas validas como `D2UY`, `K1RZ`, `KL7J` y `N7XR` se comporten de forma consistente tanto desde Band Activity como desde el mapa.
- ampliado el filtro de plausibilidad FT2 para rechazar tambien ghost free-text que se disfrazan de pares de indicativos, como `M9B ZNWF6WH7V`, preservando al mismo tiempo trafico free-text legitimo como `TNX BOB 73 GL`.
- restaurado el target/ruta Linux `wsprd`, de manera que los workflows Linux release y AppImage vuelvan a publicar el conjunto runtime completo esperado.
- reforzado el script de packaging release macOS con staging DMG aislado y reintentos de `hdiutil create`, de forma que fallos transitorios `Resource busy` en la ruta Monterey Intel no aborten la build de artefactos.
- alineados con `1.5.7` los metadatos locales de version, defaults de workflow, readmes, documentacion, changelog, notas release y body GitHub.

### Targets Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage

### Requisitos Minimos Linux

Hardware:

- `x86_64`
- dual-core 2.0 GHz o superior
- minimo 4 GB RAM
- 500 MB libres en disco

Software:

- `glibc >= 2.35`
- `libfuse2`
- ALSA, PulseAudio o PipeWire

### Guia de Arranque

Workaround cuarentena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Flujo Linux AppImage extract-run:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
