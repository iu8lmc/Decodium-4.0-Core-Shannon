# Decodium 4 FT2 v1.0.713

This release contains the changes accumulated after v1.0.711, including the v1.0.712 DigiPileup/MAM work and the stability, CAT, CW, SSB and Linux graphics corrections prepared for v1.0.713.

## English (British)

### Highlights since v1.0.711

- **DigiPileup and FT2/MAM operation**
  - Added the DigiPileup control and configuration dialog.
  - Added the elastic FT2 Fox transmit cycle, opt-in and disabled by default.
  - Added the multi-stream power budget, with conservative stream spacing for FT2 and FT4.
  - Improved late-start and late-RR73 handling, QSO logging after RR73, partner-memory behaviour and low-confidence decode handling.
  - Corrected the power-budget treatment of stations without link data and improved the related user-facing explanations.

- **CW and CAT keyer reliability**
  - Added Icom-specific CAT Morse chunking at the CI-V 30-character limit.
  - Queued subsequent chunks only after the previous Hamlib acknowledgement, so long messages do not overrun the radio keyer buffer.
  - Preserved the final character and single-character messages, while ensuring STOP cancels the unsent tail and late acknowledgements cannot restart transmission.
  - Kept the existing FT-991 limit and left other manufacturers, including network rigctl, on their previous path.
  - Added regression coverage using a real Hamlib dummy backend and the Icom-sized buffered-keyer failure condition; no serial radio or RF transmission is used by the test.

- **SSB transmission safety and window lifecycle**
  - Added explicit SSB stop handling for window deactivation, closing, destruction, Escape, CAT disconnection, mode changes and cancelled mouse presses.
  - Prevented focus restoration from cancelling a held PTT button before the mouse release on Linux and Windows.
  - Added audio-device, PTT-session and failure diagnostics.
  - Resynchronised an audio backlog by retaining recent complete frames instead of unnecessarily dropping the SSB transmission; the no-audio watchdog remains active.
  - Added focused tests for SSB audio capture, backlog recovery, PTT release and window focus behaviour.

- **Linux hybrid-GPU and Vulkan recovery**
  - On Linux automatic GPU selection, an eligible integrated Intel GPU is preferred over the affected NVIDIA/NVK path reported in issue #101.
  - If no eligible Intel alternative is available, the application uses Qt software rendering instead of implicitly falling back through an OpenGL/Zink path that could still reach NVK.
  - Explicit user graphics selections remain respected.
  - `VK_ERROR_DEVICE_LOST` is recorded before GUI recovery work is queued. Decodium then requests an orderly shutdown without automatically restarting or touching CAT from the render-thread callback.
  - The next automatic launch uses software rendering; `--reset-safe-graphics` clears the Linux recovery marker and cached graphics state.
  - Vulkan pipeline caches are separated by GPU, device, driver and pipeline-cache UUID, preventing a failed NVIDIA cache from being reused by Intel.
  - The recovery shutdown stops native CW, SSB and JTTY activity and releases radio control before CAT shutdown.

### Validation

- macOS local build completed successfully with the new Linux policy code syntax-checked against the installed Qt/Vulkan headers.
- Focused automated tests passed, including Linux graphics policy and recovery, graphics pipeline cache handling, CW, SSB window focus and TX-dispatch safety tests.
- The Icom/Hamlib CW buffer test uses only the Hamlib dummy backend and performs no real radio transmission.
- The Linux NVK behaviour still requires confirmation on the reporter's Fedora/NVK laptop; this release does not claim that a physical radio or that hardware has been verified here.

## Italiano

### Punti principali dalla v1.0.711

- **DigiPileup e funzionamento FT2/MAM**
  - Aggiunti il controllo DigiPileup e il relativo pannello di configurazione.
  - Aggiunto il ciclo elastico di trasmissione FT2 Fox, opzionale e disattivato per impostazione predefinita.
  - Aggiunto il budget di potenza per le trasmissioni multi-stream, con spaziatura prudente per FT2 e FT4.
  - Migliorata la gestione degli avvii ritardati, degli RR73 tardivi, della registrazione del QSO dopo RR73, della memoria dei partner e dei decode a bassa affidabilità.
  - Corretta la gestione del budget per le stazioni senza dati di collegamento e migliorate le spiegazioni mostrate all'utente.

- **Affidabilità CW e keyer CAT**
  - Aggiunto il frazionamento specifico Icom entro il limite CI-V di 30 caratteri per comando Morse CAT.
  - I blocchi successivi vengono accodati solo dopo l'acknowledgement Hamlib del blocco precedente, evitando di saturare il buffer del keyer della radio.
  - Preservati l'ultimo carattere e i messaggi di un solo carattere; STOP annulla la coda non ancora inviata e gli acknowledgement tardivi non possono riavviare la trasmissione.
  - Conservato il limite già esistente per FT-991; gli altri produttori, incluso rigctl di rete, mantengono il percorso precedente.
  - Aggiunti test di regressione con il backend dummy reale di Hamlib e una condizione di buffer simulata delle dimensioni Icom; il test non usa porte seriali né trasmette RF.

- **Sicurezza della trasmissione SSB e ciclo di vita della finestra**
  - Aggiunta la gestione esplicita dello stop SSB per disattivazione, chiusura e distruzione della finestra, Escape, disconnessione CAT, cambio modo e annullamento del click del mouse.
  - Impedito che il ripristino del focus annulli il pulsante PTT tenuto premuto prima del rilascio del mouse su Linux e Windows.
  - Aggiunti diagnostica dei dispositivi audio, delle sessioni PTT e degli errori.
  - Il backlog audio viene riallineato conservando i frame completi più recenti, senza interrompere inutilmente la trasmissione SSB; il watchdog di assenza audio resta attivo.
  - Aggiunti test mirati per cattura audio SSB, recupero dal backlog, rilascio PTT e gestione del focus della finestra.

- **GPU ibride Linux e recupero Vulkan**
  - Nella selezione automatica della GPU Linux, una GPU Intel integrata compatibile viene preferita al percorso NVIDIA/NVK coinvolto nel problema dell'issue #101.
  - Se non è disponibile un'alternativa Intel compatibile, viene usato il rendering software Qt, evitando un fallback OpenGL/Zink implicito che potrebbe passare ancora da NVK.
  - Le scelte grafiche esplicite dell'utente restano rispettate.
  - `VK_ERROR_DEVICE_LOST` viene registrato prima di accodare il recupero grafico sulla GUI. Decodium richiede quindi una chiusura ordinata, senza riavvio automatico e senza comandare la CAT dal callback del render thread.
  - All'avvio automatico successivo viene usato il rendering software; `--reset-safe-graphics` cancella il marker Linux e lo stato cache grafico.
  - Le cache Vulkan vengono separate per GPU, dispositivo, driver e UUID della pipeline, impedendo il riuso di una cache NVIDIA danneggiata con Intel.
  - Durante la chiusura di recupero vengono fermati CW, SSB e JTTY nativi e il controllo della radio viene rilasciato prima della chiusura CAT.

### Verifiche

- La build locale macOS è terminata correttamente; il nuovo codice della policy Linux è stato anche verificato sintatticamente contro gli header Qt/Vulkan disponibili.
- I test automatici mirati sono passati, inclusi policy e recupero grafico Linux, cache della pipeline grafica, CW, focus della finestra SSB e sicurezza del dispatch TX.
- Il test del buffer CW Icom/Hamlib usa esclusivamente il backend dummy di Hamlib e non esegue trasmissioni reali.
- Il comportamento Linux con NVK deve ancora essere confermato sul portatile Fedora/NVK dell'utente segnalante; questa release non dichiara verifiche effettuate su radio fisica o su quell'hardware.

### Artefatti

- Windows x86_64 installer (EXE)
- macOS Intel x86_64 DMG (Ventura, Sonoma and Sequoia variants)
- macOS Apple Silicon ARM64 DMG (Tahoe and Sequoia variants)
- Linux x86_64 AppImage
- Linux aarch64 AppImage
- Source code archive and SHA-256 files where produced by the runners
