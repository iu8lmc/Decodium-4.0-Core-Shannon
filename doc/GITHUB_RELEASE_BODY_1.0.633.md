# Decodium 4 v1.0.633

## English (UK)

This release adds two companion tools around Decodium: FT2 Log Bridge now starts together with the application, and Decodium RX brings the terminal FT8/FT4/FT2 receiver from DecodiumOS to Windows. It also includes the RTTY receive and CAT corrections published upstream in v1.0.630–v1.0.632.

### FT2 Log Bridge starts with Decodium

- FT2 Log Bridge is the FT2 Community client that receives every logged QSO over UDP (127.0.0.1:2237) and uploads it to the Online Log on community.ft2.it. Until now it had to be started by hand.
- At start-up Decodium looks for a configured copy — one with a `config.json` containing an API key, as downloaded from the site Dashboard — in Downloads, Documents and Desktop, remembers where it is and starts it. The search runs in the background and, when nothing is found, is repeated at most once a day.
- It is never started twice: a second copy could not open the UDP port and would show an error window. If the client is already running, Decodium leaves it alone.
- Decodium warns when its own UDP settings would not reach the bridge: a server other than the local machine, a different port from the one in `config.json`, or "QSO logged" not sent over UDP.
- A new **FT2 LOG BRIDGE** section in Settings → Reporting switches automatic start on or off (on by default), lets you choose the program by hand, repeat the search, start it immediately and see its status.

### Decodium RX: the terminal receiver, now on Windows

- `decodium-rx.exe` is a receive-only FT8, FT4 and FT2 decoder for the terminal, the same tool DecodiumOS ships as `decodium-rx`. It never transmits and never touches CAT or PTT. The installer adds a **Decodium RX (terminal)** shortcut to the Start menu.
- Started without options it asks for mode, dial frequency, callsign and audio input, and remembers the answers. Decodes appear line by line with CQ calls in green and calls to you in red; they are also written to an `ALL.TXT` in WSJT-X format. `--cq-only`, `--grep`, `--save-slots`, `--wav` and `--list-devices` work as on DecodiumOS.
- Unlike the DecodiumOS version, it is a single program: it does not need Python, and it runs the Decodium decoders inside the same process instead of starting a separate decoder for every slot, so the decoders keep their memory of stations already heard between slots, as in the main application.
- Audio is captured at 48 kHz and reduced to 12 kHz with the same filter as Decodium's own receiver. It can run alongside Decodium on the same sound card.

### Also included

- RTTY: receive audio recovers after leaving RTTY independently of decoder timing, and the radio's previous CAT mode is restored (upstream v1.0.630–v1.0.632).

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish. Decodium RX is included in the Windows installer; the Linux packages do not include it yet.

---

## Italiano

Questo rilascio aggiunge due strumenti di contorno a Decodium: FT2 Log Bridge ora parte insieme all'applicazione, e Decodium RX porta su Windows il ricevitore FT8/FT4/FT2 da terminale di DecodiumOS. Contiene anche le correzioni di ricezione RTTY e di CAT pubblicate a monte nelle v1.0.630–v1.0.632.

### FT2 Log Bridge parte con Decodium

- FT2 Log Bridge è il client di FT2 Community che riceve ogni QSO loggato via UDP (127.0.0.1:2237) e lo carica nel Log Online di community.ft2.it. Finora andava avviato a mano.
- All'avvio Decodium cerca una copia configurata — con un `config.json` che contiene la chiave API, come la scarica la Dashboard del sito — in Download, Documenti e Desktop, si ricorda dov'è e la avvia. La ricerca gira in background e, se non trova nulla, si ripete al massimo una volta al giorno.
- Non viene mai avviato due volte: una seconda copia non potrebbe aprire la porta UDP e mostrerebbe una finestra d'errore. Se il client è già in esecuzione, Decodium lo lascia stare.
- Decodium avvisa quando le proprie impostazioni UDP non arriverebbero al bridge: un server diverso dal computer locale, una porta diversa da quella di `config.json`, oppure «QSO logged» non inviato via UDP.
- Una nuova sezione **FT2 LOG BRIDGE** in Impostazioni → Reporting accende o spegne l'avvio automatico (acceso di default), permette di scegliere il programma a mano, ripetere la ricerca, avviarlo subito e vederne lo stato.

### Decodium RX: il ricevitore da terminale, ora su Windows

- `decodium-rx.exe` è un decodificatore FT8, FT4 e FT2 da terminale, in sola ricezione: lo stesso strumento che DecodiumOS distribuisce come `decodium-rx`. Non trasmette mai e non tocca CAT o PTT. L'installer aggiunge al menu Start il collegamento **Decodium RX (terminale)**.
- Avviato senza opzioni chiede modo, frequenza, nominativo e ingresso audio, e ricorda le risposte. Le decodifiche compaiono riga per riga, con i CQ in verde e le chiamate a te in rosso, e vengono scritte anche in un `ALL.TXT` in formato WSJT-X. `--cq-only`, `--grep`, `--save-slots`, `--wav` e `--list-devices` funzionano come su DecodiumOS.
- A differenza della versione di DecodiumOS è un solo programma: non serve Python, e usa i decoder di Decodium dentro lo stesso processo invece di avviare un decoder a parte per ogni slot, quindi i decoder conservano fra uno slot e l'altro la memoria delle stazioni già sentite, come nell'applicazione principale.
- L'audio viene catturato a 48 kHz e ridotto a 12 kHz con lo stesso filtro della ricezione di Decodium. Può girare insieme a Decodium sulla stessa scheda audio.

### Contiene anche

- RTTY: l'audio di ricezione riparte all'uscita da RTTY indipendentemente dai tempi del decoder, e viene ripristinato il modo CAT precedente della radio (v1.0.630–v1.0.632 a monte).

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questa release; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di controllo man mano che terminano. Decodium RX è incluso nell'installer Windows; i pacchetti Linux per ora non lo contengono.
