# Decodium 4 FT2 1.0.375

Release cumulativa dalla 1.0.374 alla 1.0.375.

## Correzioni principali

- Allineata la versione locale a `1.0.375` in `fork_release_version.txt` e negli script installer Windows Inno Setup/NSIS.
- Rafforzata la sicurezza dei workflow GitHub Actions: gli input manuali `version` vengono ora validati e passano da variabili d'ambiente, evitando interpolazioni shell dirette durante build e pubblicazione release.
- Hardened remote/web dashboard:
  - token di accesso persistente generato automaticamente;
  - accesso HTTP/WebSocket vincolato al token;
  - limite ai client WebSocket;
  - header `CSP`, `Referrer-Policy` e `nosniff`;
  - rimozione di risorse CDN nella pagina QR locale.
- Migliorata la robustezza delle importazioni:
  - limite dimensione/righe per import working frequencies;
  - limite dimensione file ADIF, numero record, numero campi e lunghezza campi;
  - gestione piu' sicura di file ADIF malformati o eccessivamente grandi.
- Rafforzato AutoSpot/DX Cluster:
  - buffer telnet/verifica limitati;
  - lettura a blocchi;
  - chiusura controllata su overflow invece di accumulare memoria senza limiti.
- Rafforzato parser TCI: valori telemetry non finiti, negativi o fuori range vengono scartati prima della conversione intera.

## Sequenza radio e sicurezza TX

- Wait & Pounce non puo' piu' armare TX partendo solo da un decode RF: parte solo se l'operatore ha gia' armato TX/CQ.
- I messaggi diretti R-report/RR73 non possono piu' sostituire il partner QSO attivo se non combaciano con il partner corrente o con il payload TX corrente/ultimo.
- FT2 async smart scheduler reso bounded/single-flight:
  - massimo numero retry;
  - timeout totale;
  - cancellazione quando cambiano modo, stato TX, monitor, tune, hold manuale o async.
- Correzioni alla gestione FT2/FT8/FT4 per evitare ritrasmissioni o stati non coerenti durante AutoCQ e signoff.
- Correzione della logica `Check SWR`: il blocco per SWR alto non viene applicato al Tune, ma resta valido per TX/AutoCQ.
- Aggiornati i controlli worked-before per rispettare modo e banda.
- Refresh periferiche audio anticipato all'avvio prima del monitoring, cosi' gli utenti con device USB lenti o duplicati non devono premere Refresh manualmente.

## Stabilita' e piattaforme

- Fix crash Linux/PipeWire dopo errori TX/Tune con `QSocketNotifier` da thread non corretto.
- Fix crash/errore iterator erase nel daemon UDP.
- Migliorata la gestione lifetime audio sink su macOS/CoreAudio.
- DecoSync self-calibration resa opt-in e vincolata a un offset trusted, evitando che decodes RF possano creare una calibrazione temporale iniziale non attendibile.
- Supporto migliorato per path seriali Linux `/dev/serial/by-id`.
- Ridotta la verbosita' di log per categorie rumorose come MAPGPU/PANMETRIC/DEPTHDBG.

## UI e usabilita'

- Setup: ampliati riquadri e layout per evitare testo tagliato in TX, Decode e Watchdog.
- Prompt/error dialog: migliorata dimensione e posizionamento dei pulsanti per evitare sovrapposizioni.
- Correzione rendering testo/emoji su macOS Tahoe con sanitizzazione dei valori non numerici/testuali nelle impostazioni.
- Popout Astro e Macro nuovamente trascinabili.
- Layout footer/header sistemato: pulsanti Layout/History/DX Cluster coerenti con la barra superiore.
- Tooltip inglesi rivisti dove l'interfaccia era in inglese ma comparivano testi italiani.
- Opzioni Display per colonne decode:
  - possibilita' di mostrare/nascondere `Dist` e `Az`;
  - aggiunta colonna `Freq` in Signal RX.
- Rimosse indicazioni gialle ridondanti 500/1000/1500/2000/2500/3000 dalla scala waterfall/panadapter.

## Asset previsti

- Source code GitHub per il tag `1.0.375`.
- Eseguibile/installer Microsoft Windows x64.
- DMG macOS Apple Silicon.
- DMG macOS Intel.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
