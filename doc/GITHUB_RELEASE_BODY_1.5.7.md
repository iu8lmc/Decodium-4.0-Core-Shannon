# Decodium 1.5.7 (Fork 9H1SR)

## English

Release highlights (`1.5.6 -> 1.5.7`):

- added an FT2 type-4 plausibility filter so bogus callsign-like payloads no longer appear as accepted traffic.
- fixed FT2 Band Activity double-click handling for standard `CQ` / `QRZ` lines and for directly clicked callsign tokens inside FT2 rows, so valid callers such as `D2UY`, `K1RZ`, `KL7J`, and `N7XR` arm reliably from Band Activity as well as from the map.
- extended the FT2 plausibility filter to reject ghost free-text decodes that masquerade as callsign pairs, such as `M9B ZNWF6WH7V`, while preserving legitimate free-text traffic.
- added targeted `test_qt_helpers` regression coverage for valid slash/special-event FT2 forms and invalid garbage examples.
- restored the Linux `wsprd` target so Linux/AppImage release jobs publish the expected full binary set again.
- hardened the macOS DMG packaging script with isolated staging and `hdiutil create` retries to avoid transient `Resource busy` failures on the Monterey Intel path.
- aligned local version metadata, workflow defaults, readmes, docs, changelog, and release notes to semantic version `1.5.7`.
- kept the macOS folder/layout changes already proven by the previous successful deploy.

Release assets:

- `decodium3-ft2-1.5.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64`, dual-core 2.0 GHz+, 4 GB RAM minimum, 500 MB free disk
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire

If macOS blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

To avoid issues caused by the AppImage read-only filesystem, it is recommended to start Decodium by extracting the AppImage first and then running the program from the extracted directory.

Run the following commands in a terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No `.pkg` installers are produced.

## Italiano

Punti principali (`1.5.6 -> 1.5.7`):

- aggiunto un filtro di plausibilita' FT2 type-4 cosi' payload simili a nominativi ma fasulli non appaiono piu' come traffico accettato.
- corretto il doppio click FT2 nella Band Activity per le righe standard `CQ` / `QRZ` e per il nominativo cliccato direttamente dentro una riga FT2, cosi' nominativi validi come `D2UY`, `K1RZ`, `KL7J` e `N7XR` si armano in modo affidabile sia dalla Band Activity sia dalla mappa.
- esteso il filtro di plausibilita' FT2 per rigettare ghost free-text che si mascherano da coppie di nominativi, come `M9B ZNWF6WH7V`, preservando il traffico free-text legittimo.
- aggiunta copertura di regressione mirata in `test_qt_helpers` per forme FT2 valide con slash/special-event e per esempi garbage non validi.
- ripristinato il target Linux `wsprd`, cosi' i job Linux/AppImage tornano a pubblicare il set binario completo atteso.
- reso piu' robusto lo script di packaging DMG macOS con staging isolato e retry di `hdiutil create` per evitare fallimenti transitori `Resource busy` sul percorso Monterey Intel.
- allineati alla semver `1.5.7` metadati versione locali, default workflow, readme, documentazione, changelog e note release.
- mantenuto il layout/cartelle macOS gia' dimostrato dal precedente deploy riuscito.

Asset release:

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

Requisiti minimi Linux:

- `x86_64`, dual-core 2.0 GHz+, minimo 4 GB RAM, 500 MB liberi
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

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

Non vengono prodotti installer `.pkg`.

## Espanol

Resumen (`1.5.6 -> 1.5.7`):

- anadido un filtro de plausibilidad FT2 type-4 para que payloads con aspecto de nominativo pero falsos ya no aparezcan como trafico aceptado.
- corregido el doble click FT2 en Band Activity para lineas estandar `CQ` / `QRZ` y para el indicativo clicado directamente dentro de una linea FT2, de modo que llamadas validas como `D2UY`, `K1RZ`, `KL7J` y `N7XR` se armen de manera fiable tanto desde Band Activity como desde el mapa.
- ampliado el filtro de plausibilidad FT2 para rechazar ghost free-text disfrazados de pares de indicativos, como `M9B ZNWF6WH7V`, preservando trafico free-text legitimo.
- anadida cobertura de regresion dirigida en `test_qt_helpers` para formas FT2 validas con slash/special-event y para ejemplos garbage no validos.
- restaurado el target Linux `wsprd`, por lo que los jobs Linux/AppImage vuelven a publicar el conjunto binario completo esperado.
- reforzado el script de packaging DMG macOS con staging aislado y reintentos de `hdiutil create` para evitar fallos transitorios `Resource busy` en la ruta Monterey Intel.
- alineados con la semver `1.5.7` los metadatos locales de version, defaults de workflow, readmes, documentacion, changelog y notas release.
- mantenido el layout/carpetas macOS ya demostrado por el deploy correcto anterior.

Artefactos release:

- `decodium3-ft2-1.5.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage.sha256.txt`

Requisitos minimos Linux:

- `x86_64`, dual-core 2.0 GHz+, minimo 4 GB RAM, 500 MB libres
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Si macOS bloquea el inicio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

Ejecutar los siguientes comandos en la terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No se generan instaladores `.pkg`.
