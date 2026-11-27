# Decodium 4.0 Core Shannon — v1.0.687

## English (British)

Release 1.0.687 contains the local integration work after Martino's 1.0.686 fast-forward, covering the changes accumulated from 1.0.685 through 1.0.687.

### User interface and SSB

- Resizable SSB window with a clearer two-column layout.
- Local voice synthesis and preview are asynchronous, so voice operations do not block the interface.
- Updated QML translation catalogues and active translation handling.
- Added an optional decode-provenance column in Full Spectrum and Signal RX. It is disabled by default for new installations and can be enabled from Setup > Decode.
- Decode rows now carry an explicit origin such as `Local decoder` or `Local TX` when no feed-specific source is available.

### CAT, audio and recovery

- Added a readable Audio/CAT diagnostic panel in Setup.
- Added non-destructive recovery actions to restore current system audio defaults and reconnect the selected CAT backend without deleting profiles.
- CAT profiles now preserve their audio input/output devices and channels.
- Audio-device telemetry is refreshed during recovery and sleep/wake transitions.
- CAT recovery after application resume is scheduled without interrupting an active transmission or tuning operation.
- The CW sidetone test is protected against overlapping requests and does not wait synchronously on the audio worker.
- Power-on controls are enabled only when a usable direct serial/USB CAT endpoint is configured; remote and placeholder ports are rejected clearly.

### Guided setup and graphics safety

- The first-run radio detector now distinguishes passive USB detection from an already connected CAT session.
- A connected radio is no longer reported as “no radio detected” merely because the passive detector cannot probe the already-owned port or a remote backend.
- The guided setup can be requested for the next launch and clears itself after use.
- “Reset graphics once on next launch” is explicitly one-shot; it does not force D3D11 or another backend on every subsequent launch.
- Detection messages now distinguish a missing serial device from a visible device whose model cannot be identified automatically.

### LiveMap and roster

- Added roster filters for `New DXCC`, `New on band`, and `New on band + mode`.
- Added provider/source information to roster rows and an action to ignore a provider persistently.
- Added a direct action to open the logbook filtered to the selected grid/VUCC entity.
- Kept provenance and roster/database operations compatible with the existing worker-thread model.

### Validation

- Local CMake build completed successfully.
- QML lint completed without errors; existing non-fatal layout warnings remain in legacy QML areas.
- Map-layer tests and the existing focused regression tests remain part of the validation set.

### Release assets

- Windows x64 installer executable.
- macOS Apple Silicon DMG builds for the supported macOS runners.
- macOS Intel DMG builds for the supported macOS runners.
- Linux x86_64 AppImage.
- Linux aarch64 AppImage.
- SHA-256 checksum files for the generated macOS and Linux artefacts where provided by the workflows.

## Italiano

La 1.0.687 integra il lavoro locale successivo al fast-forward di Martino alla 1.0.686 e comprende tutte le modifiche accumulate dalla 1.0.685 alla 1.0.687.

### Interfaccia e SSB

- Finestra SSB ridimensionabile con disposizione a due colonne piu' chiara.
- Sintesi vocale locale e anteprima eseguite in modo asincrono, senza bloccare l'interfaccia.
- Aggiornati i cataloghi QML e la gestione delle traduzioni attive.
- Aggiunta la colonna opzionale della provenienza dei decode in Full Spectrum e Signal RX. Per le nuove installazioni e' disattivata e si abilita da Setup > Decode.
- Ogni decode riceve ora un'origine esplicita, ad esempio `Local decoder` o `Local TX`, quando non e' disponibile una sorgente specifica del feed.

### CAT, audio e ripristino

- Aggiunto in Setup un pannello diagnostico Audio/CAT leggibile.
- Aggiunte azioni di recupero non distruttive per ripristinare i default audio correnti del sistema e riconnettere il backend CAT selezionato senza cancellare i profili.
- I profili CAT conservano ora periferiche e canali audio di ingresso/uscita.
- Il monitoraggio delle periferiche audio viene aggiornato durante il recupero e dopo sleep/wake.
- Dopo il resume il recupero CAT viene pianificato senza interrompere trasmissione o sintonia attive.
- Il test del sidetone CW rifiuta richieste sovrapposte e non attende sincronicamente il worker audio.
- I comandi Power On sono abilitati solo quando esiste un endpoint CAT seriale/USB diretto utilizzabile; porte remote o segnaposto vengono escluse chiaramente.

### Configurazione guidata e sicurezza grafica

- Il rilevamento radio al primo avvio distingue la scansione USB passiva da una sessione CAT gia' connessa.
- Una radio gia' collegata non viene piu' indicata come “radio non rilevata” solo perche' il detector passivo non puo' sondare una porta gia' posseduta o un backend remoto.
- La configurazione guidata puo' essere richiesta al prossimo avvio e si disattiva automaticamente dopo l'utilizzo.
- “Reset grafico una volta al prossimo avvio” e' esplicitamente one-shot: non forza D3D11 o un altro backend a ogni avvio successivo.
- I messaggi distinguono una porta seriale assente da una periferica visibile il cui modello non e' identificabile automaticamente.

### LiveMap e roster

- Aggiunti i filtri roster `New DXCC`, `New on band` e `New on band + mode`.
- Aggiunte la sorgente/provider degli spot e l'azione per ignorare in modo persistente un provider.
- Aggiunto il collegamento diretto al logbook filtrato per la griglia/entita' VUCC selezionata.
- Le operazioni di provenienza e roster/database restano compatibili con il modello esistente a worker separati.

### Verifiche

- Build CMake locale completata con successo.
- QML lint completato senza errori; restano solo warning di layout non bloccanti in aree QML preesistenti.
- I test del map layer e i test di regressione focalizzati esistenti restano inclusi nella validazione.

### Asset della release

- Eseguibile installer Windows x64.
- Build DMG macOS Apple Silicon per i runner macOS supportati.
- Build DMG macOS Intel per i runner macOS supportati.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
- File SHA-256 per gli artefatti macOS e Linux generati dai workflow, dove previsto.
