# Decodium 4 v1.0.685

This release consolidates the upstream work from v1.0.681 to v1.0.684 and the local CAT, CW, audio, graphics and interface fixes developed on the fork after v1.0.680.

## English (British)

### Highlights since v1.0.680

- CW startup and remote operation were corrected. The CW module is now started on first use, remote radios remain in CW mode, and the serial keyer refuses CAT/PTT ports or TCP rigctld endpoints that cannot be opened as a local serial keyer.
- SSB received the local voice keyer, optional Piper voice pack, optional speech and assistant packs, hold-to-listen support, space-bar PTT, CAT receive controls and software listening AGC. Optional packs are independently installed and removed, with checksum verification.
- Failed remote audio monitor sinks are handled asynchronously: the user interface remains responsive, the error state is reported, and a failed sink can be reopened when the device becomes available again. TCI/local audio fallback is also recovered after CAT loss.
- Hamlib cold-start Power On now keeps a usable CAT session open while a radio is powered off, sends the model-specific wake-up only for an explicit Power On request, waits for the radio to settle, and retries serial enumeration after a delayed USB or mains restore.
- Icom serial startup no longer switches temporarily between VFO A and VFO B when split control is disabled. Decodium reads the selected VFO without causing a visible or audible frequency jump.
- Added separate ATU ON and ATU OFF commands, with capability checks, safe refusal while transmitting and readable ATU state through Hamlib.
- The CW sidetone output is now opened asynchronously, handles `QAudio::ErrorState`, recreates a failed audio sink and reports an actionable device error instead of freezing the interface during a tone test.
- Added a one-shot, cross-platform “reset safe graphics on next launch” option, equivalent to `--reset-safe-graphics`, for recovering persistent graphics fallback settings.
- Improved decoded-text layout so messages remain vertically inside their result rectangles, and reduced normal map-intelligence snapshot logging while keeping diagnostic data collection available.
- CMake developer warnings were reduced by adopting the relevant policy settings and replacing deprecated `exec_program` usage in the Qt path.

### Upstream v1.0.684 features included

- Optional local Voice, Speech Recognition and Assistant packs for the SSB window.
- Eight labelled local SSB voice-keyer messages with recording and playback support.
- Local assistant chat using the optional model pack; sending an answer by voice is explicitly labelled as an on-air action.
- CW is no longer started or reopened automatically merely because Decodium starts in CW mode.
- SSB CAT controls and DECODIUM SDR software listening AGC with Off, Auto, Fast, Medium, Slow and Long choices.
- Linux build correction for the SSB AGC header include.

### Verification

- The local `decodium` target builds successfully on macOS arm64.
- The focused `test_transceiver_base_split` test passes.
- The release source tree is versioned through `fork_release_version.txt` at `1.0.685`.

## Italiano

Questa release consolida il lavoro upstream dalla v1.0.681 alla v1.0.684 e tutte le correzioni locali CAT, CW, audio, grafica e interfaccia sviluppate nel fork dopo la v1.0.680.

### Novità principali dalla v1.0.680

- Corretto l'avvio e il funzionamento remoto del CW. Il modulo CW parte al primo utilizzo, le radio remote restano in modo CW e il keyer seriale rifiuta le porte CAT/PTT o gli endpoint TCP rigctld che non possono essere aperti come keyer seriali locali.
- La modalità SSB include il voice keyer locale, i pacchetti opzionali Piper, riconoscimento vocale e assistente, la funzione hold-to-listen, il PTT con barra spaziatrice, i controlli CAT di ricezione e l'AGC software per l'ascolto. I pacchetti opzionali sono installabili e rimovibili separatamente e vengono verificati tramite checksum.
- I sink audio remoti guasti sono gestiti in modo asincrono: l'interfaccia non si blocca, l'errore viene mostrato e il sink può essere riaperto quando la periferica torna disponibile. È stato migliorato anche il ritorno all'audio locale dopo la perdita del CAT/TCI.
- Il Power On a freddo tramite Hamlib mantiene aperta una sessione CAT utilizzabile mentre la radio è spenta, invia il wake-up specifico del modello solo dopo un'esplicita richiesta Power On, attende la stabilizzazione e ritenta l'enumerazione seriale dopo il ripristino ritardato di USB o alimentazione.
- All'avvio CAT seriale Icom, quando il controllo split è disattivato, Decodium non passa più temporaneamente da VFO A a VFO B. La VFO selezionata viene letta senza provocare cambi di frequenza visibili o udibili.
- Aggiunti comandi separati ATU ON e ATU OFF, con controllo del supporto del modello, rifiuto sicuro durante la trasmissione e lettura dello stato ATU tramite Hamlib.
- L'uscita sidetone CW viene aperta in modo asincrono, gestisce `QAudio::ErrorState`, ricrea il sink audio guasto e mostra un errore utile senza congelare l'interfaccia durante il tone test.
- Aggiunta un'opzione multipiattaforma “reset grafico al prossimo avvio”, equivalente a `--reset-safe-graphics`, per ripristinare le impostazioni grafiche persistenti in caso di problemi.
- Migliorato il layout del testo decodificato, che ora resta verticalmente dentro il proprio rettangolo, e ridotti i messaggi normali degli snapshot della map intelligence, mantenendo disponibile la raccolta diagnostica.
- Ridotti i warning developer di CMake adottando le policy pertinenti e sostituendo l'uso deprecato di `exec_program` nel percorso Qt.

### Funzioni upstream della v1.0.684 incluse

- Pacchetti locali opzionali Voice, Speech Recognition e Assistant per la finestra SSB.
- Otto messaggi SSB locali etichettati, con registrazione e riproduzione.
- Chat dell'assistente locale con pacchetto modello opzionale; l'invio vocale della risposta è indicato esplicitamente come azione in trasmissione.
- Il CW non viene più avviato o riaperto automaticamente solo perché Decodium parte in modo CW.
- Controlli CAT SSB e AGC software di ascolto da DECODIUM SDR con scelte Off, Auto, Fast, Medium, Slow e Long.
- Correzione della compilazione Linux per l'include dell'header AGC SSB.

### Verifica

- Il target locale `decodium` compila correttamente su macOS arm64.
- Il test mirato `test_transceiver_base_split` è superato.
- Il codice sorgente della release è versionato tramite `fork_release_version.txt` a `1.0.685`.

## Downloads

The source archive, Windows x64 executable, macOS Apple Silicon and Intel DMGs, and Linux x86_64 and aarch64 AppImages are published by the corresponding GitHub Actions workflows.
