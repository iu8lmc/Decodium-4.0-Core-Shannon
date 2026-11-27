# Decodium 4 v1.0.699

## English (British)

This release consolidates the radio-startup safety work and the latest CW,
JTTY, SSB, CAT and receive-meter fixes developed since v1.0.698.

### Radio startup and CAT/PTT safety

- PTT is released immediately after CAT opens, before startup queries,
  frequency/mode restoration and power probing, with additional guarded
  release checks during startup synchronisation.
- Added RX-only startup guards in the bridge and transceiver manager. Automatic,
  periodic, CW, frequency, split and CAT-mode paths can no longer inherit a
  stale PTT-on state during startup.
- Explicit operator transmission remains available once the startup guard is
  released.
- Avoided an unnecessary intermediate DATA-U/USB transition when the radio was
  already in CW, reducing mode flicker during CAT connection.

### CW, JTTY, SSB and interface

- Added persistent `SHOW CW`, `SHOW JTTY` and `SHOW SSB` controls to reopen the
  relevant external window without changing the active mode.
- Widened the LDPC status control so its label remains readable beside STOP.
- Fixed the SSB external-window focus race that prevented the Radio AGC menu
  from opening. Combo boxes retain focus during mouse interaction, including
  dynamically created menus.
- Radio AGC selections now reliably reach the CAT path for OFF, FAST, MID and
  SLOW, using the correct numeric Hamlib values.
- The space-bar PTT remains blocked while a menu is active, but continues to
  work normally elsewhere. Text-entry fields still accept spaces without
  starting transmission.

### Receive levels and regression coverage

- Fixed frozen receive-level indicators after changing between CW/JTTY/SSB and
  FT modes by rearming legacy state refresh and selecting the active RX-level
  owner instead of checking whether a retained audio-input object exists.
- Added focused SSB UI regression tests for menu opening, AGC values, keyboard
  and mouse focus, text entry and PTT safety.
- Validated the SSB focus tests with both Qt Material and Fusion styles.
- Local validation completed with the Decodium QML build and focused CAT,
  audio, TX-safety, window and SSB regression tests.

## Italiano

Questa release consolida il lavoro di sicurezza all'avvio della radio e le
correzioni per CW, JTTY, SSB, CAT e indicatori di livello RX sviluppate dalla
v1.0.698.

### Sicurezza radio all'avvio e CAT/PTT

- Il PTT viene rilasciato subito dopo l'apertura CAT, prima delle interrogazioni
  iniziali, del ripristino di frequenza/modo e della lettura della potenza, con
  ulteriori controlli durante la sincronizzazione di avvio.
- Aggiunte protezioni RX-only all'avvio nel bridge e nel transceiver manager:
  i percorsi automatici, periodici, CW, frequenza, split e modo CAT non possono
  più ereditare uno stato PTT attivo durante l'avvio.
- La trasmissione esplicita dell'operatore resta disponibile dopo la protezione
  iniziale.
- Evitato il passaggio intermedio non necessario a DATA-U/USB quando la radio
  era già in CW, riducendo il cambio di modo durante la connessione CAT.

### CW, JTTY, SSB e interfaccia

- Aggiunti i pulsanti persistenti `SHOW CW`, `SHOW JTTY` e `SHOW SSB` per
  riaprire le finestre esterne senza cambiare modo.
- Allargato il controllo LDPC per rendere leggibile l'etichetta accanto a STOP.
- Corretto il focus della finestra SSB esterna che impediva l'apertura del menu
  Radio AGC. I ComboBox mantengono il focus durante il clic, inclusi i menu
  creati dinamicamente.
- Le selezioni Radio AGC raggiungono correttamente il CAT per OFF, FAST, MID e
  SLOW con i rispettivi valori numerici Hamlib.
- La barra spaziatrice non attiva il PTT mentre un menu è aperto e continua a
  funzionare normalmente negli altri contesti; i campi testo accettano gli
  spazi senza iniziare la trasmissione.

### Livelli RX e test

- Corretto il congelamento degli indicatori RX dopo il passaggio tra CW/JTTY/SSB
  e i modi FT, riattivando il refresh legacy e usando il proprietario effettivo
  del livello RX.
- Aggiunti test mirati per apertura menu, valori AGC, focus da mouse e tastiera,
  campi testo e sicurezza PTT.
- I test del focus SSB sono validati con gli stili Qt Material e Fusion.
- Completata la validazione locale con build QML e test CAT, audio, sicurezza TX,
  finestre e SSB.

La release pubblica gli asset Windows x64, macOS Apple Silicon e Intel DMG,
Linux x86_64 e Linux aarch64 AppImage, insieme ai relativi SHA-256.
