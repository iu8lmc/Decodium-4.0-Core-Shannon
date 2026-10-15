# Decodium 4.0 v1.0.527

## English (UK)

Version 1.0.527 combines the improvements delivered in 1.0.524, 1.0.525 and
1.0.526 with a major receive-only RTL-SDR integration, a rebuilt local callsign
database centre, and a number of important reliability and layout corrections.

### RTL-SDR receiver — new receive-only input

- Decodium can now receive directly from compatible RTL-SDR devices, including
  the RTL-SDR Blog V4. No virtual audio cable is needed.
- The new **Settings → Audio → RTL-SDR Receiver** section detects devices,
  keeps the connection state visible, and performs opening, retuning, USB IQ
  capture, FFT processing and recovery away from the user-interface thread.
- The RF IQ path is now separate from receiver audio and weak-signal decoder
  audio. This gives the panadapter and waterfall their correct RF span, while
  preserving a dedicated 12 kHz compatibility path for FT8/FT4 decoding.
- General receiver audio supports Wide FM, Narrow FM, AM, USB, LSB and CW. The
  selected RF channel is demodulated asynchronously to 48 kHz mono and played
  through the configured receiver audio output; it is not accidentally sent to
  the FT8 decoder.
- The receiver controls cover sample rate, PPM correction, tuner AGC or manual
  gain, bias tee, follow-dial operation and safely debounced retuning.
- **SDR Radio** and **Direct Sampling** are exposed as distinct modes. Direct
  sampling is limited to supported HF hardware and frequencies; unsupported
  profiles automatically and safely fall back to SDR Radio. RTL-SDR Blog V4 is
  recognised and uses its proper tuner/upconverter path.
- Fixed-IF receivers are supported with configurable IF frequency, USB and LSB
  shifts, sideband selection and optional spectrum inversion. This keeps the
  displayed dial frequency, logging metadata and decoder frequency correct even
  when the RTL-SDR is connected to a radio IF output.
- RTL-SDR is deliberately RX-only: Decodium blocks Tune, manual TX,
  auto-sequence TX and PTT requests whenever this input is selected.
- Release builds compile and bundle the pinned RTL-SDR Blog `librtlsdr` 2.0.2
  runtime on Windows, macOS and Linux; package checks fail if the runtime is
  missing. The installed documentation includes operating notes and the
  third-party licence notice.

### Local callsign databases and confirmations

- The **Callsign → Local databases** page has been redesigned as a clear set
  of provider cards. It reports the real local record count and the last update
  state instead of treating external files such as `cty.dat` as empty SQL data.
- FCC ULS, LoTW user activity, eQSL AG and Club Log OQRS updates run in the
  background. Their status remains visible and the application stays usable
  throughout downloads, parsing and database writes.
- `cty.dat` now has its own refresh action and true record count. `CALL3.TXT`
  is also managed from the same page, with its own refresh action and status.
  The former duplicate download controls have been removed from Colours.
- Local file selection and ADI import are available per provider, with
  non-blocking progress and error feedback.
- The confirmation sources are now separated from activity lists: eQSL InBox,
  QRZ.com confirmations (API or ADI import), and LoTW received confirmations
  are imported into the active logbook without modal progress windows. QRZ
  downloads are paged and all providers keep their update status visible.
- Club Log OQRS credentials no longer contain a duplicate update control. The
  global lookup-cache action is named accurately, asks for confirmation, and
  does not delete the downloaded provider databases.
- The local-database interface is translated consistently rather than falling
  back to Italian while Decodium is being used in another language.

### UI, display and usability

- Settings pages were made substantially more resilient on compact and 4:3
  displays. Controls no longer extend needlessly beyond the screen, and the
  database cards, update actions and performance selectors have improved
  minimum sizes and wrapping.
- The Full Spectrum / Signal RX layout now adapts its visible columns to the
  available width. It preserves readable values, avoids overlapping distance
  and message text, and drops only the least essential columns when a panel is
  made very narrow. Resizing restores the columns when space becomes available.
- The panadapter/waterfall monitor path now follows the selected input correctly
  and has stronger start/stop and asynchronous refresh handling for RTL-SDR
  operation.
- Experimental RTL-SDR controls are explicitly marked as under development in
  every shipped translation, so the current RX-only scope is clear.

### Other included improvements since 1.0.523

- **1.0.524 and 1.0.525:** repaired the log-confirmation path so a completed
  QSO no longer loses its confirmation prompt. The safety guard prevents a
  duplicate prompt, keeps it visible in a reduced window and supports the
  themed dialog.
- **1.0.526:** added passive automatic radio detection. It identifies likely
  radio, CAT port, baud rate and audio devices from existing operating-system
  information without opening a CAT port or transmitting a command.
- **1.0.526:** reworked the light theme and added a light/dark switch next to
  the font-size controls, preserving the chosen dark theme when switching back.
- Added FT2-Link satellite half-duplex support and corresponding test coverage.
- Added a Linux DRM GPU-usage helper and tests to improve diagnostic accuracy on
  Linux graphics systems.
- Updated the project, RTL-SDR and UI translation catalogues, and added focused
  tests for RTL-SDR DSP, tuning plans, input behaviour, capabilities and RF
  spectrum processing.

### Package matrix

This release provides source archives from the `v1.0.527` tag plus an x64
Windows installer, macOS DMGs for Apple Silicon and Intel, and Linux AppImages
for x86_64 and aarch64. Each package is built by its matching GitHub Actions
runner and checked for its required RTL-SDR runtime.

- The Windows packaging helper builds and stages only the upstream shared
  RTL-SDR DLL and its import library. Its optional legacy command-line tools
  are not part of Decodium and are intentionally excluded, preserving
  compatibility with the current MSYS2 GCC toolchain.

---

## Italiano

La versione 1.0.527 riunisce i miglioramenti delle 1.0.524, 1.0.525 e 1.0.526
con una grande integrazione RTL-SDR solo ricezione, il nuovo centro dei database
locali dei nominativi e varie correzioni importanti di affidabilità e layout.

### Ricevitore RTL-SDR — nuovo ingresso solo RX

- Decodium può ora ricevere direttamente da dispositivi RTL-SDR compatibili,
  incluso RTL-SDR Blog V4. Non serve alcun cavo audio virtuale.
- La nuova sezione **Impostazioni → Audio → Ricevitore RTL-SDR** rileva i
  dispositivi, mostra sempre lo stato della connessione ed esegue apertura,
  retune, acquisizione USB IQ, FFT e recupero fuori dal thread dell'interfaccia.
- Il percorso RF IQ è ora separato dall'audio del ricevitore e dall'audio per i
  decoder a segnali deboli. Panadapter e waterfall mostrano quindi la vera
  porzione RF, mentre FT8/FT4 mantengono il proprio percorso compatibile a
  12 kHz.
- L'audio radio supporta Wide FM, Narrow FM, AM, USB, LSB e CW. Il canale RF
  scelto viene demodulato in background a 48 kHz mono e riprodotto sull'uscita
  audio configurata; non viene inviato per errore al decoder FT8.
- I controlli comprendono sample rate, correzione PPM, AGC tuner o guadagno
  manuale, bias tee, inseguimento del dial e retune con debounce sicuro.
- **SDR Radio** e **Direct Sampling** sono due modalità distinte. Il direct
  sampling è limitato all'hardware e alle frequenze HF supportati; i profili
  non compatibili tornano automaticamente e in sicurezza a SDR Radio. RTL-SDR
  Blog V4 viene riconosciuto e usa il proprio percorso tuner/upconverter.
- Sono supportati anche ricevitori con IF fissa: frequenza IF, shift USB e LSB,
  scelta della banda laterale e inversione opzionale dello spettro sono
  configurabili. Frequenza di dial, dati di log e frequenza del decoder restano
  corretti anche con RTL-SDR collegato all'uscita IF della radio.
- RTL-SDR è intenzionalmente solo RX: con questo ingresso Decodium blocca Tune,
  TX manuale, TX in auto-sequenza e richieste PTT.
- Le build di rilascio compilano e includono il runtime RTL-SDR Blog
  `librtlsdr` 2.0.2 su Windows, macOS e Linux; il packaging fallisce se manca.
  La documentazione installata include istruzioni operative e avviso di licenza
  di terze parti.

### Database locali dei nominativi e conferme

- La pagina **Callsign → Database locali** è stata ridisegnata in riquadri
  chiari per provider. Mostra il numero reale di record e l'ultimo stato di
  aggiornamento, senza scambiare file esterni come `cty.dat` per tabelle SQL
  vuote.
- Gli aggiornamenti FCC ULS, attività utenti LoTW, eQSL AG e Club Log OQRS
  lavorano in background. Stato e avanzamento restano visibili e il programma
  rimane utilizzabile durante download, parsing e scrittura nel database.
- `cty.dat` dispone ora del proprio pulsante di aggiornamento e del conteggio
  reale. Anche `CALL3.TXT` è gestito nella stessa pagina, con aggiornamento e
  stato dedicati. I vecchi pulsanti duplicati sono stati rimossi da Colori.
- Per ogni provider sono disponibili la scelta di un file locale e l'import
  ADI, con avanzamento ed errori non bloccanti.
- Le sorgenti di conferme sono separate dalle liste di attività: eQSL InBox,
  conferme QRZ.com (API oppure import ADI) e conferme ricevute LoTW vengono
  importate nel logbook attivo senza finestre modali. QRZ scarica a pagine e
  tutti i provider mantengono visibile lo stato di aggiornamento.
- Le credenziali Club Log OQRS non mostrano più un pulsante di aggiornamento
  duplicato. L'azione per la cache globale di lookup ha un nome corretto,
  richiede conferma e non cancella i database scaricati dai provider.
- L'interfaccia dei database locali viene tradotta in modo coerente, senza
  ricadere in italiano quando Decodium è in uso in un'altra lingua.

### Interfaccia, display e usabilità

- Le pagine delle impostazioni sono molto più robuste su schermi compatti e
  4:3. I controlli non escono inutilmente dai bordi e riquadri database,
  azioni di aggiornamento e selettori performance hanno minimi e a-capo
  migliorati.
- Il layout Full Spectrum / Signal RX adatta le colonne alla larghezza
  disponibile. Mantiene leggibili i valori, evita sovrapposizioni fra distanza
  e messaggio e rimuove solo le colonne meno essenziali quando il pannello è
  molto stretto; allargando, le colonne tornano disponibili.
- Il percorso panadapter/waterfall segue ora correttamente l'ingresso scelto e
  ha una gestione più solida di avvio, arresto e aggiornamento asincrono per
  RTL-SDR.
- I controlli RTL-SDR sperimentali sono marcati chiaramente come funzione in
  sviluppo in tutte le traduzioni distribuite, così è esplicito l'attuale
  perimetro solo RX.

### Altri miglioramenti inclusi dalla 1.0.523

- **1.0.524 e 1.0.525:** riparato il percorso di conferma del log: dopo un QSO
  completato il prompt non si perde più. La protezione evita duplicati, lo
  mantiene visibile con finestra ridotta e supporta il dialog a tema.
- **1.0.526:** aggiunto il rilevamento automatico passivo della radio. Propone
  modello, porta CAT, baud rate e dispositivi audio dalle informazioni già note
  al sistema, senza aprire la porta CAT né inviare comandi.
- **1.0.526:** tema chiaro ridisegnato e nuovo interruttore chiaro/scuro vicino
  ai controlli della dimensione font, mantenendo il tema scuro scelto al ritorno.
- Aggiunto il supporto satellite half-duplex per FT2-Link e i relativi test.
- Aggiunti helper e test Linux DRM per rendere più accurata la diagnostica
  dell'utilizzo GPU sui sistemi grafici Linux.
- Aggiornati cataloghi di traduzione del progetto, RTL-SDR e interfaccia;
  aggiunti test mirati per DSP RTL-SDR, piano di sintonia, comportamento
  dell'ingresso, capacità e spettro RF.

### Matrice dei pacchetti

Questa release fornisce gli archivi sorgente dal tag `v1.0.527`, il programma
di installazione Windows x64, DMG macOS per Apple Silicon e Intel e AppImage
Linux per x86_64 e aarch64. Ogni pacchetto viene creato dal relativo runner
GitHub Actions e verificato con il runtime RTL-SDR richiesto.

- L'helper di packaging Windows compila e prepara soltanto la DLL RTL-SDR
  condivisa a monte e la relativa import library. Le utility a riga di comando
  legacy opzionali non fanno parte di Decodium e sono intenzionalmente escluse,
  così il packaging resta compatibile con l'attuale toolchain MSYS2 GCC.
