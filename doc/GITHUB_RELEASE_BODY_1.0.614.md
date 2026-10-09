# Decodium 4 FT2 v1.0.614

## English (UK)

This maintenance release builds on v1.0.613 with Live Map rendering controls,
clearer RTTY operation, explicit DXpedition mode indicators and a waterfall
layout correction.

### Live Map: selectable GPU or CPU rendering

- Added **Map GPU** under **Setup → Display → Map and Distance**. GPU rendering
  remains enabled by default. Disable it to use the CPU renderer when station
  markers are missing or the map displays graphical artefacts.
- The setting is also synchronised with the legacy INI file, so operators who
  previously set `LiveMapUseGpu=false` manually can switch it back from Setup.
- This provides a workaround for the reported Windows rendering problem; it
  does not claim to repair the underlying GPU/driver interaction.

### RTTY: distinguish audio AFSK from native FSK

- The mode buttons now identify **DIGU/DIGL as AFSK** and native RTTY modes as
  **FSK** for conventional rigs. The DIGU tooltip explains DATA-U/USB-D and the
  need to configure the radio's computer-audio input.
- Audio transmission is blocked in native RTTY/FSK modes on conventional rigs,
  before PTT is asserted. The explanation directs the operator to **Set radio**
  in the DECODER panel, which requests DATA-U/DIGU.
- The existing QMX USB-audio FSK exception, mode translation and full-scale
  audio policy are preserved. Reception in native RTTY modes remains available.
- Corrected the earlier blanket statement that DATA-U was unsuitable for RTTY
  and added bilingual AFSK/FSK guidance in `doc/RTTY_AFSK_FSK.md`.

### SuperFox / SuperHound: clearer operating state

- In FT8, the window title and mode indicator now distinguish **SuperHound,
  SuperFox, Hound and Fox**, according to the selected operating role.
- The Setup tooltip explains that working a SuperFox DXpedition requires
  **FT8 + Hound + SuperFox**; the SuperFox checkbox alone does not select Hound.
- The SuperFox checkbox cannot be changed during transmission or tuning.
- A delayed FT8 UI update is now tied to the lifetime of its window and ignored
  after switching to another mode. This hardens a path also used by SuperFox
  changes. The reported shutdown/decode-stop case has not been reproduced, so
  this release does not claim a confirmed fix for that incident.

### Waterfall layout

- Reserved a separate header row for the embedded waterfall's title, drag
  handle and Pop control, preventing overlap with the Calls/Font controls.

### Verification and packages

The Intel macOS workflow now reuses the runner's installed Fortran compiler,
with GCC 14 as a fallback, and avoids upgrading already installed build tools.
This addresses the dependency-installation failure seen in the previous release.

The RTTY regression test covers native-FSK PTT blocking, DATA-U audio output
and the existing QMX behaviour. QML syntax and whitespace checks were also
performed. Live IC-7100 operation and the reported Windows SuperFox incident
still require hardware/runtime confirmation. No new fix for the intermittent
log-dialog language report is claimed in this release.

Release targets: Windows x64 installer; macOS Apple Silicon DMGs for Sequoia
and Tahoe; macOS Intel DMGs for Ventura, Sonoma and Sequoia; Linux x86_64 and
aarch64 AppImages. Binary packages are built and attached by GitHub Actions.
Source ZIP and tar.gz archives are available through GitHub's source downloads.

## Italiano

Questa versione di manutenzione parte dalla v1.0.613 e aggiunge il controllo
del rendering della mappa, chiarimenti sulla RTTY, indicatori espliciti per i
modi DXpedition e una correzione del layout del waterfall.

### Mappa live: rendering GPU o CPU selezionabile

- Aggiunta la voce **Map GPU** in **Setup → Display → Map and Distance**. La GPU
  resta attiva di default. Disattivarla permette di usare il renderer CPU quando
  mancano i pallini delle stazioni o compaiono anomalie grafiche.
- La scelta viene sincronizzata anche con il file INI legacy: chi aveva inserito
  manualmente `LiveMapUseGpu=false` può riattivare la GPU dal Setup.
- È una soluzione alternativa al problema grafico segnalato su Windows; non
  viene dichiarata risolta la causa nell'interazione tra GPU e driver.

### RTTY: distinzione tra AFSK audio e FSK nativa

- I pulsanti identificano **DIGU/DIGL come AFSK** e i modi RTTY nativi come
  **FSK** per gli apparati convenzionali. Il tooltip DIGU chiarisce DATA-U/USB-D
  e la configurazione dell'ingresso audio del computer nella radio.
- Nei modi RTTY/FSK nativi degli apparati convenzionali la trasmissione audio
  viene bloccata prima di attivare il PTT. Il messaggio invita a usare
  **Set radio** nel pannello DECODER per richiedere DATA-U/DIGU.
- Restano l'eccezione QMX per FSK tramite audio USB, la traduzione dei modi e
  la gestione del livello audio a fondo scala. La ricezione RTTY nativa resta
  disponibile.
- Corrette le vecchie indicazioni generiche contro DATA-U e aggiunta la guida
  bilingue `doc/RTTY_AFSK_FSK.md`.

### SuperFox / SuperHound: stato operativo più chiaro

- In FT8, titolo della finestra e indicatore del modo distinguono ora
  **SuperHound, SuperFox, Hound e Fox** in base al ruolo selezionato.
- Il tooltip nel Setup chiarisce che per lavorare una spedizione SuperFox serve
  **FT8 + Hound + SuperFox**: la sola spunta SuperFox non seleziona Hound.
- La spunta SuperFox non può essere cambiata durante trasmissione o Tune.
- Un aggiornamento differito dell'interfaccia FT8 viene annullato alla
  distruzione della finestra e ignorato dopo il passaggio a un altro modo.
  La protezione interessa anche i cambi SuperFox. Il caso segnalato di chiusura
  e arresto della decodifica non è stato riprodotto: non viene dichiarato risolto.

### Layout waterfall

- Titolo, maniglia e comando Pop del waterfall integrato hanno una riga
  dedicata, evitando la sovrapposizione ai controlli Calls/Font.

### Verifiche e pacchetti

Il workflow macOS Intel riutilizza il compilatore Fortran presente sul runner,
con GCC 14 come alternativa, ed evita l'aggiornamento degli strumenti già
installati. La modifica affronta il fallimento nell'installazione delle
dipendenze osservato nella release precedente.

Il test di regressione RTTY copre il blocco PTT in FSK nativa, l'uscita audio
DATA-U e il comportamento QMX esistente. Eseguiti anche controlli sintattici
QML e di formattazione delle modifiche. Restano da confermare sul campo il
funzionamento IC-7100 e il caso SuperFox segnalato su Windows. Questa versione
non dichiara una nuova correzione per la lingua intermittente del dialogo di log.

Pacchetti previsti: installer Windows x64; DMG Apple Silicon per Sequoia e
Tahoe; DMG Intel per Ventura, Sonoma e Sequoia; AppImage Linux x86_64 e aarch64.
Gli eseguibili vengono compilati e allegati tramite GitHub Actions. I sorgenti
ZIP e tar.gz sono disponibili tramite i download del codice sorgente di GitHub.

**Full comparison / Confronto completo:**
https://github.com/elisir80/Decodium-4.0-Core-Shannon/compare/v1.0.613...v1.0.614
