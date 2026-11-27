# Decodium 4.0 Core Shannon

<img width="3438" height="1378" alt="decodium4" src="https://github.com/user-attachments/assets/fabdbb3e-3652-42a3-b3e9-49fc10d8e03d" />


[![Release](https://img.shields.io/github/v/release/elisir80/Decodium-4.0-Core-Shannon?include_prereleases&label=release)](https://github.com/elisir80/Decodium-4.0-Core-Shannon/releases)
[![Licence: GPL-3.0](https://img.shields.io/badge/licence-GPL--3.0-blue.svg)](COPYING)
[![Qt](https://img.shields.io/badge/UI-Qt%206.11-41cd52.svg)](https://www.qt.io/)

Decodium 4.0 Core Shannon e' un programma per radioamatori dedicato ai modi digitali weak-signal. Nasce dalla linea WSJT-X / Decodium e porta una interfaccia moderna Qt/QML, decoder e modulatori progressivamente migrati in C++, gestione CAT integrata, waterfall/panadapter evoluti, Live Map, DX Cluster, PSK Reporter, log QSO e strumenti per operare FT8, FT4, FT2 e altri modi a bassa intensita di segnale.

Questa README e' bilingue:

- [Italiano](#italiano)
- [British English](#british-english)

---

## Italiano

### Cos'e Decodium 4

Decodium 4.0 Core Shannon e' una stazione digitale completa per modi weak-signal. L'obiettivo e' offrire un ambiente operativo unico per:

- ricevere e trasmettere in `FT8`, `FT4`, `FT2` e altri modi digitali supportati;
- controllare la radio tramite CAT;
- visualizzare waterfall, panadapter, segnali decodificati, mappa live e storico;
- gestire QSO manuali, Auto Sequence, Auto CQ e Multi Answer Mode;
- inviare spot e log verso servizi esterni;
- ridurre blocchi UI/audio su macchine moderne e su PC piu datati.

Il nome "Core Shannon" richiama Claude Shannon e l'idea centrale del progetto: trattare la comunicazione digitale debole come un problema di informazione, sincronizzazione, robustezza e affidabilita operativa.

### Funzioni principali

#### Modi digitali

Decodium include supporto operativo per:

- `FT8`
- `FT4`
- `FT2`
- `Q65`
- `MSK144`
- `JT65`
- `JT9`
- `JT4`
- `FST4`
- `FST4W`
- `WSPR`

La disponibilita effettiva di alcune funzioni puo dipendere dal backend di decode usato, dalla piattaforma e dalla configurazione di build.

#### SSTV nativo

Le build con `DECODIUM_ENABLE_SSTV=ON` includono un unico workspace SSTV
integrato per ricezione e trasmissione analogica, Studio immagini, Gallery,
logging QSO, condivisione HTTPS opt-in e diagnostica.  Usa gli stessi percorsi
audio e lo stesso coordinamento CAT/PTT di Decodium; non avvia un decoder
esterno e non apre una seconda sorgente di cattura.  HAMDRM e' un sottosistema
digitale separato, disponibile solo quando `DECODIUM_ENABLE_HAMDRM=ON` e quando
il backend dichiara le capacita effettive.

La [guida utente SSTV](docs/sstv/USER_GUIDE.md) descrive il flusso operativo e
i limiti di sicurezza.  La [matrice dei modi](docs/sstv/MODE_MATRIX.md) indica
esattamente quali righe hanno prove native o indipendenti: una funzione
implementata non implica automaticamente una prova on-air, con radio reale o
con un'altra applicazione.

#### FT2 e weak signal

FT2 e' uno dei focus principali del progetto. Decodium 4 include:

- decode FT2 asincrono;
- scheduler TX rapido con guard per evitare TX troppo tardive nello slot;
- gestione QSO FT2 con stati TX1/TX2/TX3/TX4/TX5/TX6;
- opzioni conservative per segnali deboli e QSB;
- protezioni contro ghost call e payload non risolti;
- retry e pulizia dello stato QSO dopo halt o limite retry;
- supporto a QSO manuali e Auto CQ.

#### Interfaccia operativa

L'interfaccia QML e' pensata per uso reale in stazione:

- header con frequenza, modo, stato CAT, RX/TX e controlli rapidi;
- waterfall e panadapter ad alta leggibilita;
- Full Spectrum per vedere tutta l'attivita decodificata;
- Signal RX per seguire le stazioni rilevanti per il QSO;
- pannello TX con messaggi, stato QSO, Auto, Call, Hold, Tune e Halt;
- finestre pop-out/dock per Full Spectrum, Signal RX, Live Map, Log, Cluster e strumenti;
- persistenza layout tra chiusura e riapertura;
- reset layout;
- modalita compatta per vedere piu righe decode;
- supporto tema scuro e stili Qt.

#### Waterfall, panadapter e GPU

Decodium usa Qt Quick/RHI e, quando disponibile, accelerazione GPU per:

- waterfall;
- panadapter;
- Live Map;
- texture e shader;
- rendering fluido dei pannelli.

Sono presenti fallback per sistemi problematici:

- fallback persistente D3D11 su Windows quando D3D12 crea instabilita;
- opzioni safe graphics;
- percorsi CPU/fallback quando GPU o shader non sono disponibili;
- modalita PC lento per ridurre carico grafico e UI.

Su Linux con backend Qt Quick OpenGL, la FFT visuale del panadapter puo essere
spostata sulla GPU da **Setup > Advanced > OpenGL GPU FFT**. L'opzione e'
volutamente disattiva di serie per compatibilita con driver OpenGL meno stabili;
richiede il riavvio di Decodium;
se il compute shader non e' supportato, fallisce o provoca uno stallo severo,
Decodium torna automaticamente alla FFTW asincrona sulla CPU. Questa opzione
accelera la sola FFT grafica del panadapter: il decoder FT rimane sulla CPU.
Non usare `QT_QUICK_BACKEND=software` o
`DECODIUM_DISABLE_GPU_PANADAPTER_FFT=1` quando si desidera questo offload.

#### CAT e controllo radio

Decodium supporta piu strade per il controllo radio:

- Hamlib;
- backend nativi per alcune radio;
- Ham Radio Deluxe;
- OmniRig su Windows;
- TCI;
- seriale locale;
- controllo di frequenza, modo, split e PTT.

Il programma e' stato adattato per preservare correttamente le modalita data/packet dove possibile, in particolare con radio Icom, Yaesu e Kenwood usate via Hamlib, Ham Radio Deluxe o OmniRig.

#### Audio RX/TX

Decodium gestisce audio RX e TX con attenzione a latenze, buffer e clipping:

- selezione device audio input/output;
- canale audio RX/TX;
- livello RX e TX;
- livellamento automatico RX opzionale e attivo di default;
- guard contro clipping RX;
- restart audio post-TX piu aggressivo su Windows;
- watchdog audio TX per evitare portanti bloccate o buffer non drenati;
- supporto ai classici codec USB delle radio.

#### Live Map

La Live Map mostra stazioni, percorsi e attivita rilevante:

- contatti in banda;
- percorsi tra la propria stazione e le stazioni decodificate;
- marker colore per direzione e stato;
- reset automatico su cambio banda, cambio modo e clear decode;
- pulizia quando Full Spectrum o Signal RX vengono svuotati;
- throttling durante TX per evitare stalli del main thread.

#### DX Cluster, PSK Reporter e servizi online

Il programma integra:

- DX Cluster;
- ricerca PSK Reporter;
- invio spot PSK Reporter;
- QRZ Logbook;
- Cloudlog;
- supporto ADIF;
- storico decode SQLite;
- esportazione ADIF dallo storico.

#### Decode History

Decodium mantiene uno storico dei decode in SQLite:

- ricerca per callsign;
- filtro banda;
- filtro modo;
- intervallo date UTC;
- limite risultati;
- esportazione ADIF;
- persistenza multi-sessione.

Lo storico e' pensato per analisi post-operativa, verifica propagazione e controllo attivita in banda.

#### Time Sync

Per modi come FT8/FT4, la sincronizzazione tempo e' critica. Decodium include:

- pannello DecoSyncTime;
- NTP interno;
- monitor DT;
- gestione offset;
- integrazione con i decode per stimare lo stato della sincronizzazione.

### Piattaforme

Il repository contiene codice e workflow per:

- Windows 64 bit;
- macOS Apple Silicon;
- macOS Intel dove supportato dalla build;
- Linux x86_64 AppImage;
- build Linux Qt 6.11;
- ambienti di sviluppo locali con CMake.

Gli artifact pubblici vengono distribuiti nella sezione [Releases](https://github.com/elisir80/Decodium-4.0-Core-Shannon/releases) quando disponibili.

### Download e installazione

1. Aprire la pagina [Releases](https://github.com/elisir80/Decodium-4.0-Core-Shannon/releases).
2. Scaricare il pacchetto adatto al proprio sistema:
   - Windows: installer o eseguibile `x64`;
   - macOS Apple Silicon: `dmg`;
   - Linux: `AppImage`.
3. Avviare Decodium.
4. Configurare callsign, locator, radio, audio e modo operativo.

#### macOS

Se macOS blocca l'app per quarantena, e' possibile rimuovere l'attributo dalla app installata:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/Decodium.app
```

Il nome esatto della app puo variare in base al pacchetto generato.

#### Linux AppImage

Rendere eseguibile l'AppImage:

```bash
chmod +x Decodium*.AppImage
./Decodium*.AppImage
```

Su alcune distribuzioni puo essere necessario installare `libfuse2` o usare l'estrazione manuale:

```bash
./Decodium*.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

#### Linux Wayland + NVIDIA: stalli FT8/Qt Quick

Su sistemi Linux con sessione **Wayland** e GPU/driver **NVIDIA** — in particolare
CachyOS — alcuni stack Qt 6 possono presentare stalli di circa un secondo nella
sincronizzazione Qt Quick. Il sintomo e' un waterfall apparentemente attivo ma
decode FT8 intermittente o bloccato.

Se si verifica questo problema, avviare l'AppImage con VSync disabilitato:

```bash
QSG_NO_VSYNC=1 ./Decodium*.AppImage
```

Il workaround e' stato verificato con Decodium 1.0.517 su CachyOS, NVIDIA e
Wayland: mantiene attivi il waterfall GPU, la Live Map e il decode FT8 evitando
gli stalli QSG. Non e' necessario su sistemi che funzionano normalmente e non
si applica a Windows o macOS. La disabilitazione del VSync puo causare tearing
o aumentare l'uso della GPU.

### Configurazione iniziale consigliata

#### 1. Stazione

Impostare:

- callsign;
- grid locator;
- regione/banda;
- modo di emissione iniziale.

#### 2. Audio

Selezionare:

- input audio dalla radio o interfaccia USB;
- output audio verso la radio;
- canale corretto se il dispositivo e' stereo;
- livello RX e TX.

Il livello RX automatico puo ridurre il rischio di clipping senza costringere l'operatore a correggere continuamente lo slider.

#### 3. CAT

Selezionare il backend piu adatto:

- Hamlib per controllo diretto;
- Ham Radio Deluxe se la radio e' gia gestita da HRD;
- OmniRig per configurazioni Windows con software esterni;
- TCI se la radio o il software SDR espone TCI.

Verificare:

- frequenza letta correttamente;
- cambio banda;
- cambio modo;
- PTT;
- modalita data/packet quando richiesta.

#### 4. Modo digitale

Scegliere il modo dal selettore principale. Per FT8/FT4 controllare che l'orologio sia sincronizzato. Per FT2 valutare le opzioni conservative solo quando servono per segnali molto deboli o QSB.

#### 5. Log e servizi

Configurare, se necessari:

- ADIF locale;
- PSK Reporter;
- QRZ Logbook;
- Cloudlog;
- DX Cluster;
- QSO forwarding esterno.

### Uso operativo

#### Chiamare una stazione

1. Fare doppio click su una riga decode valida.
2. Controllare TX/RX audio frequency.
3. Verificare i messaggi TX generati.
4. Avviare TX.
5. Lasciare Auto Sequence attivo se si vuole completare il QSO automaticamente.

#### Fare CQ

1. Impostare TX6/CQ.
2. Attivare TX o Auto CQ.
3. Monitorare Signal RX per le risposte dirette.
4. Lasciare Decodium proseguire nel QSO o intervenire manualmente.

#### Multi Answer Mode

Multi Answer Mode consente di gestire piu chiamanti. La coda puo essere usata per organizzare le priorita operative, ma l'operatore mantiene il controllo manuale.

### File e dati utente

Decodium usa cartelle di sistema standard Qt per:

- impostazioni;
- cache QML;
- database SQLite;
- CTY.DAT;
- log;
- storico decode.

Le posizioni esatte dipendono dalla piattaforma. Nei log di avvio Decodium stampa i percorsi principali, per esempio `AppDataLocation`, `CacheLocation` e il database `db.sqlite`.

### Build da sorgente

#### Dipendenze principali

Sono richiesti, a seconda della piattaforma:

- CMake;
- compilatore C/C++;
- Qt 6 con moduli `Quick`, `Qml`, `QuickControls2`, `Widgets`, `Multimedia`, `Network`, `SerialPort`, `Sql`, `WebSockets`;
- Hamlib;
- FFTW3 single precision e threads;
- Boost log;
- libusb;
- OpenMP dove disponibile;
- Qt ShaderTools per il percorso shader GPU, se disponibile.

#### Build locale tipica

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Il target QML principale e':

```bash
cmake --build build --target decodium_qml --parallel
```

Su macOS con Homebrew e Qt recente potrebbe essere necessario specificare `CMAKE_PREFIX_PATH`:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target decodium_qml --parallel
```

#### Workflow GitHub

Il repository include workflow per:

- build Windows x64;
- release macOS Apple Silicon;
- release macOS Intel;
- release Linux AppImage Qt 6.11 x86_64;
- release Linux AppImage Qt 6.11 aarch64;
- test runner.

I workflow si trovano in `.github/workflows`.

La versione viene letta esclusivamente da `fork_release_version.txt`. Tag,
input manuale del runner e release notes devono corrispondere; il controllo
locale e CI si esegue con:

```bash
scripts/ci/validate-repository-layout.sh
```

### Troubleshooting rapido

#### Non decodifica

Controllare:

- input audio corretto;
- livello RX non clippato;
- modo corretto;
- frequenza radio corretta;
- DT vicino allo zero per FT8/FT4;
- filtri CQ/MyCall non troppo restrittivi;
- radio in modalita data/packet.

#### La radio non cambia frequenza o modo

Controllare:

- backend CAT selezionato;
- porta seriale o endpoint rete;
- baud rate;
- permessi porta seriale;
- HRD/OmniRig gia aperti e configurati;
- conflitti con altri software che controllano la radio.

#### Su Windows la GPU crea problemi

Decodium puo usare D3D12, D3D11 o safe graphics. Se l'avvio grafico fallisce, il programma puo mantenere un fallback persistente a D3D11 per evitare cicli di crash o GPU instabile.

#### Audio TX non parte o cade

Controllare:

- device output;
- livello TX;
- PTT;
- buffer audio;
- modalita data/packet della radio;
- eventuali stalli CPU durante TX.

#### PC lento

Ridurre il carico:

- abbassare FPS waterfall;
- disattivare Live Map o usare fallback piu leggero;
- disattivare Cluster durante TX;
- evitare effetti UI non necessari;
- usare impostazioni conservative per macchine vecchie.

### Stato del progetto

Decodium 4 e' un progetto attivo. Il codice include componenti storici della famiglia WSJT-X/Decodium e componenti nuovi in C++/Qt/QML. Alcune parti sono in migrazione o in consolidamento, soprattutto nel percorso decoder/modulatore nativo e nelle integrazioni multipiattaforma.

### Contribuire

Contributi utili:

- log diagnostici riproducibili;
- bug report con modo, radio, backend CAT, OS e versione Decodium;
- test su Windows, macOS e Linux;
- patch C++/QML mirate;
- miglioramenti documentazione;
- traduzioni.

Quando si segnala un problema, includere:

- versione Decodium;
- sistema operativo;
- radio e interfaccia;
- backend CAT;
- modo digitale;
- estratto log;
- passi per riprodurre il problema.

### Licenza

Il progetto e' distribuito sotto GNU General Public License v3. Vedere [COPYING](COPYING).

### Ringraziamenti

Decodium deriva da una lunga storia di software weak-signal per radioamatori, inclusa la linea WSJT-X e il lavoro di molti sviluppatori, tester e operatori. Questo fork raccoglie contributi e test sul campo orientati all'uso quotidiano in stazione.

---

## British English

### What Decodium 4 Is

Decodium 4.0 Core Shannon is a weak-signal digital-mode application for amateur radio. It builds on the WSJT-X / Decodium lineage and combines a modern Qt/QML interface, progressively migrated native C++ decode and transmit paths, integrated CAT control, advanced waterfall and panadapter views, Live Map, DX Cluster, PSK Reporter, QSO logging and operating tools for FT8, FT4, FT2 and other weak-signal modes.

The "Core Shannon" name refers to Claude Shannon and to the core idea behind the project: weak-signal digital radio is a practical information, synchronisation and reliability problem.

### Main Features

#### Digital Modes

Decodium provides operating support for:

- `FT8`
- `FT4`
- `FT2`
- `Q65`
- `MSK144`
- `JT65`
- `JT9`
- `JT4`
- `FST4`
- `FST4W`
- `WSPR`

The exact runtime availability of some features can depend on platform, decoder backend and build configuration.

#### Native SSTV

Builds with `DECODIUM_ENABLE_SSTV=ON` include one integrated SSTV workspace for
analogue receive/transmit, image Studio, Gallery, QSO logging, opt-in HTTPS
sharing and diagnostics.  It reuses Decodium's audio routes and CAT/PTT
coordination; it neither launches an external decoder nor opens a second
capture source.  HAMDRM is a separate digital subsystem, present only with
`DECODIUM_ENABLE_HAMDRM=ON` and enabled according to the backend's reported
capabilities.

See the [SSTV user guide](docs/sstv/USER_GUIDE.md) for operation and safety
limits.  The [mode matrix](docs/sstv/MODE_MATRIX.md) states exactly which rows
have native or independent evidence: implemented functionality is not by
itself an on-air, real-radio or cross-application interoperability result.

#### FT2 and Weak-Signal Operation

FT2 is one of the main focuses of this fork. Decodium 4 includes:

- asynchronous FT2 decoding;
- fast TX scheduling with guards against starting too late in the slot;
- TX1/TX2/TX3/TX4/TX5/TX6 QSO state handling;
- conservative options for weak signals and QSB;
- protection against ghost calls and unresolved payloads;
- retry handling and QSO state clean-up after halt or retry limits;
- support for manual QSOs and Auto CQ.

#### Operator Interface

The QML interface is designed for real station use:

- main header with frequency, mode, CAT state, RX/TX state and quick controls;
- readable waterfall and panadapter;
- Full Spectrum pane for full-band decode activity;
- Signal RX pane for QSO-relevant decodes;
- TX panel with generated messages and QSO state;
- pop-out/dock windows for Full Spectrum, Signal RX, Live Map, Log, Cluster and tools;
- persistent layout state;
- layout reset;
- compact decode rows;
- dark UI and Qt style support.

#### Waterfall, Panadapter and GPU

Decodium uses Qt Quick/RHI and, when available, GPU acceleration for:

- waterfall;
- panadapter;
- Live Map;
- textures and shaders;
- smooth rendering.

Fallbacks are available for difficult systems:

- persistent D3D11 fallback on Windows when D3D12 is unstable;
- safe graphics options;
- CPU fallback paths where GPU or shaders are unavailable;
- low-performance PC settings to reduce UI and graphics load.

On Linux with the Qt Quick OpenGL backend, the visual panadapter FFT can be
offloaded to the GPU through **Setup > Advanced > OpenGL GPU FFT**. The option
is deliberately off by default for compatibility with less stable OpenGL
drivers and requires a Decodium restart. If compute shaders are unsupported,
fail, or cause a severe stall,
Decodium automatically returns to the asynchronous CPU FFTW path. This option
accelerates only the visual panadapter FFT; FT decoding remains on the CPU. Do
not use `QT_QUICK_BACKEND=software` or
`DECODIUM_DISABLE_GPU_PANADAPTER_FFT=1` when this offload is wanted.

#### CAT and Radio Control

Decodium supports several radio-control paths:

- Hamlib;
- native backends for selected radios;
- Ham Radio Deluxe;
- OmniRig on Windows;
- TCI;
- local serial control;
- frequency, mode, split and PTT handling.

The application includes work to preserve data/packet modes where possible, especially with Icom, Yaesu and Kenwood radios used through Hamlib, Ham Radio Deluxe or OmniRig.

#### RX/TX Audio

Decodium manages audio with attention to latency, buffering and clipping:

- audio input and output device selection;
- RX/TX channel selection;
- RX and TX level controls;
- optional automatic RX level control, enabled by default;
- RX clipping guards;
- more aggressive post-TX audio restart on Windows;
- TX audio watchdogs;
- support for common USB audio codec interfaces.

#### Live Map

Live Map shows stations, paths and relevant activity:

- band contacts;
- paths between the operator and decoded stations;
- colour-coded markers;
- automatic reset on band change, mode change and clear actions;
- throttling during TX to avoid blocking the main UI thread.

#### DX Cluster, PSK Reporter and Online Services

Integrated services include:

- DX Cluster;
- PSK Reporter search;
- PSK Reporter spot upload;
- QRZ Logbook;
- Cloudlog;
- ADIF support;
- SQLite decode history;
- ADIF export from decode history.

#### Decode History

Decodium stores decode history in SQLite:

- callsign search;
- band filter;
- mode filter;
- UTC date range;
- result limit;
- ADIF export;
- multi-session persistence.

This is intended for post-operation analysis, propagation checks and review of band activity.

#### Time Sync

Accurate time matters for FT8 and FT4. Decodium includes:

- DecoSyncTime panel;
- internal NTP client;
- DT monitor;
- offset handling;
- decode-based feedback for time alignment.

### Platforms

This repository contains code and workflows for:

- Windows 64-bit;
- macOS Apple Silicon;
- macOS Intel where supported by the build;
- Linux x86_64 AppImage;
- Linux Qt 6.11 builds;
- local CMake development builds.

Public artefacts are published on the [Releases](https://github.com/elisir80/Decodium-4.0-Core-Shannon/releases) page when available.

### Download and Installation

1. Open [Releases](https://github.com/elisir80/Decodium-4.0-Core-Shannon/releases).
2. Download the package for your platform:
   - Windows: `x64` installer or executable;
   - macOS Apple Silicon: `dmg`;
   - Linux: `AppImage`.
3. Start Decodium.
4. Configure callsign, locator, radio, audio and operating mode.

#### macOS

If macOS quarantines the app, remove the quarantine attribute from the installed application:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/Decodium.app
```

The exact application name can vary depending on the generated package.

#### Linux AppImage

Make the AppImage executable:

```bash
chmod +x Decodium*.AppImage
./Decodium*.AppImage
```

Some distributions require `libfuse2`, or manual extraction:

```bash
./Decodium*.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

#### Linux Wayland + NVIDIA: FT8/Qt Quick stalls

On Linux systems running a **Wayland** session with an **NVIDIA** GPU/driver —
especially CachyOS — some Qt 6 graphics stacks can exhibit approximately
one-second Qt Quick synchronisation stalls. The waterfall may appear active
while FT8 decoding becomes intermittent or stops.

If this occurs, start the AppImage with VSync disabled:

```bash
QSG_NO_VSYNC=1 ./Decodium*.AppImage
```

This workaround was verified with Decodium 1.0.517 on CachyOS, NVIDIA and
Wayland: it keeps the GPU waterfall, Live Map and FT8 decoding active while
avoiding the QSG stalls. It is not needed on systems that work normally and
does not apply to Windows or macOS. Disabling VSync can cause screen tearing or
increase GPU usage.

### Recommended First-Time Configuration

#### 1. Station

Set:

- callsign;
- grid locator;
- region/band;
- initial digital mode.

#### 2. Audio

Select:

- audio input from the radio or USB interface;
- audio output to the radio;
- correct channel if the device is stereo;
- RX and TX levels.

Automatic RX level control can reduce clipping without requiring the operator to keep adjusting the input slider.

#### 3. CAT

Choose the most suitable backend:

- Hamlib for direct radio control;
- Ham Radio Deluxe if HRD already controls the radio;
- OmniRig for Windows stations using external radio software;
- TCI where the SDR or radio exposes a TCI endpoint.

Check:

- frequency readback;
- band changes;
- mode changes;
- PTT;
- data/packet mode where required.

#### 4. Digital Mode

Choose the mode from the main selector. For FT8/FT4, verify that the clock is synchronised. For FT2, use conservative options only when they are useful for very weak signals or QSB.

#### 5. Log and Services

Configure, as needed:

- local ADIF;
- PSK Reporter;
- QRZ Logbook;
- Cloudlog;
- DX Cluster;
- external QSO forwarding.

### Operating Basics

#### Calling a Station

1. Double-click a valid decode row.
2. Check TX/RX audio frequency.
3. Verify the generated TX messages.
4. Start TX.
5. Leave Auto Sequence enabled if you want Decodium to complete the QSO automatically.

#### Calling CQ

1. Set TX6/CQ.
2. Enable TX or Auto CQ.
3. Watch Signal RX for direct replies.
4. Let Decodium continue the QSO or intervene manually.

#### Multi Answer Mode

Multi Answer Mode helps manage several callers. The queue can be used to organise operating priority while the operator keeps manual control.

### User Data

Decodium uses standard Qt system locations for:

- settings;
- QML cache;
- SQLite database;
- CTY.DAT;
- logs;
- decode history.

The exact paths depend on the platform. Decodium prints the main locations during startup, including `AppDataLocation`, `CacheLocation` and the `db.sqlite` path.

### Building From Source

#### Main Dependencies

Depending on the platform, the build requires:

- CMake;
- C/C++ compiler;
- Qt 6 modules: `Quick`, `Qml`, `QuickControls2`, `Widgets`, `Multimedia`, `Network`, `SerialPort`, `Sql`, `WebSockets`;
- Hamlib;
- FFTW3 single precision and threads;
- Boost log;
- libusb;
- OpenMP where available;
- Qt ShaderTools for the GPU shader path, where available.

#### Typical Local Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The main QML target is:

```bash
cmake --build build --target decodium_qml --parallel
```

On macOS with Homebrew and a recent Qt, `CMAKE_PREFIX_PATH` may be needed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target decodium_qml --parallel
```

#### GitHub Workflows

The repository includes workflows for:

- Windows x64 builds;
- macOS Apple Silicon releases;
- macOS Intel releases;
- Linux AppImage Qt 6.11 x86_64 releases;
- Linux AppImage Qt 6.11 aarch64 releases;
- test runner.

Workflow files are in `.github/workflows`.

The version is read exclusively from `fork_release_version.txt`. Tags, manual
runner input, and release notes must match. Run the local and CI contract check
with:

```bash
scripts/ci/validate-repository-layout.sh
```

### Quick Troubleshooting

#### No Decodes

Check:

- correct audio input;
- RX level not clipping;
- correct mode;
- correct radio frequency;
- DT close to zero for FT8/FT4;
- CQ/MyCall filters not too restrictive;
- radio in data/packet mode.

#### Radio Does Not Change Frequency or Mode

Check:

- selected CAT backend;
- serial port or network endpoint;
- baud rate;
- serial-port permissions;
- HRD/OmniRig already open and configured;
- conflicts with other software controlling the radio.

#### Windows GPU Problems

Decodium can use D3D12, D3D11 or safe graphics. If graphics startup fails, it can keep a persistent D3D11 fallback to avoid repeated crashes or unstable GPU start-up loops.

#### TX Audio Does Not Start or Drops

Check:

- output device;
- TX level;
- PTT;
- audio buffer behaviour;
- radio data/packet mode;
- CPU stalls during TX.

#### Older or Slower PCs

Reduce load by:

- lowering waterfall FPS;
- disabling Live Map or using a lighter fallback;
- disabling Cluster updates during TX;
- avoiding unnecessary UI effects;
- using conservative settings on old hardware.

### Project Status

Decodium 4 is an active project. The codebase contains historical WSJT-X/Decodium components and newer C++/Qt/QML components. Some areas are still being consolidated, especially the native decoder/modulator path and cross-platform integrations.

### Contributing

Useful contributions include:

- reproducible diagnostic logs;
- bug reports with mode, radio, CAT backend, OS and Decodium version;
- testing on Windows, macOS and Linux;
- focused C++/QML patches;
- documentation improvements;
- translations.

When reporting an issue, include:

- Decodium version;
- operating system;
- radio and interface;
- CAT backend;
- digital mode;
- log excerpt;
- reproduction steps.

### Licence

This project is distributed under the GNU General Public License v3. See [COPYING](COPYING).

### Acknowledgements

Decodium comes from a long history of weak-signal amateur-radio software, including the WSJT-X line and the work of many developers, testers and operators. This fork collects practical field testing and implementation work aimed at daily station operation.
