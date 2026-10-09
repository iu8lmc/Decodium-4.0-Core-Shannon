# Decodium 4 v1.0.637

## English (UK)

This release absorbs upstream v1.0.635 and v1.0.636 and fixes a weakness in the FT2 candidate search that made the program miss signals near the edges of the receiver passband, even strong ones, and adds the Decodium RX terminal receiver to the ☰ menu.

### FT2 now finds signals at the edges of the audio passband

- A new bench measured the program against known truth: synthetic FT2 signals of known message, frequency, DT and level mixed into a real 20 m recording, 800 slots in all. It showed that the frames Decodium loses are not lost by the decoder but by the **signal search**: the proportion of decoded frames follows the proportion of frames for which a candidate exists at the right frequency, at every level.
- That search was not finding every signal even at +6 dB, where it stopped at 77 %. By frequency the picture was clear: 85–91 % between 1000 and 2000 Hz, but 46 % below 500 Hz and 44 % above 2500 Hz.
- The cause is the way the noise floor is estimated. A quartic curve is fitted to the spectrum, and on the shoulder of the receiver filter — 40 dB of slope between 500 and 1500 Hz on the test recording — the parabolic interpolation of the peak lands 38 to 55 Hz away from the real signal, outside the ±12–16 Hz that FT2 synchronisation can recover. The candidate exists but starts from the wrong place.
- From this release the peak and its interpolation are searched on the **difference** between the smoothed spectrum and the estimated floor, instead of their ratio, where the slope of the floor weighs far less. On the same 800 slots the search rises from 56.1 % to 67.0 % and decodes increase by **18.3 %**, with no unexpected lines and none at all on 500 slots of noise only. The cost is negligible: 42 candidates per slot instead of 38, and the same decode time.
- The correction is enabled in FT2, where it was measured. FT4 uses the same search but has not been measured, so there it remains optional. `DECODIUM_FONDO=0` restores the previous behaviour exactly; `DECODIUM_FONDO=mediana` replaces the quartic with a sliding median, which decodes about as much but doubles the number of candidates and the decode time, which is why it is not the default.

### Decodium RX in the ☰ menu

- "Decodium RX — terminal receiver" opens the standalone FT8/FT4/FT2 receiver in a console window of its own, without the graphical interface. On Windows it gets a real new console; on Linux it uses the system terminal emulator.
- The terminal now accepts the dial frequency in kHz as well as MHz, and no longer prints Qt Multimedia notices over its opening questions.

### Measurement tooling

- With `DECODIUM_LLR_DUMP=<file>` set, each FT2 candidate writes a binary record with its 174 LLRs, estimated frequency and DT, sync score and decode outcome. This separates the demodulator from the decoder, so different decoders can be compared offline on exactly the same candidates. Without the variable nothing is written and behaviour is unchanged. Format and conventions: `doc/fastldpc/llr_dump.md`.

### Absorbed from upstream v1.0.635 and v1.0.636

- AutoCQ burst counting recognises personalised TX6 calls such as `TEST VY2XT FN86`.
- ADIF band names are normalised in every logging destination, including UDP datagrams.
- The dashboard SuperFox control enables the mode and selects Hound when needed, and is blocked during TX or Tune.
- Dashboard panel dividers remember their height across restarts.
- SuperFox transitions stabilised and wanted-call alerts restored.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Questo rilascio assorbe le versioni upstream 1.0.635 e 1.0.636 e corregge un difetto nella ricerca dei candidati FT2 che faceva perdere al programma i segnali vicini ai bordi del passabanda del ricevitore, anche quelli forti, e porta il ricevitore da terminale Decodium RX nel menu ☰.

### FT2 trova i segnali ai bordi della banda audio

- Un banco nuovo ha misurato il programma con la verità nota: segnali FT2 sintetici, di cui si sa messaggio, frequenza, DT e livello, mescolati dentro una registrazione vera dei 20 metri, 800 slot in tutto. Ne è uscito che i frame che Decodium perde non li perde il decodificatore ma **l'aggancio del segnale**: la percentuale di frame decodificati segue quella dei frame per cui esiste un candidato alla frequenza giusta, a ogni livello.
- Quella ricerca non trovava tutti i segnali nemmeno a +6 dB, dove si fermava al 77 %. Guardando per frequenza il quadro era chiaro: 85-91 % fra 1000 e 2000 Hz, ma 46 % sotto i 500 Hz e 44 % sopra i 2500 Hz.
- La causa è il modo in cui si stima il rumore di fondo. Al fondo viene adattata una quartica, e sulla spalla del filtro del ricevitore — 40 dB di dislivello fra 500 e 1500 Hz nella registrazione di prova — l'interpolazione parabolica del picco finisce da 38 a 55 Hz lontano dal segnale vero, fuori dai ±12-16 Hz che il sincronismo FT2 riesce a recuperare. Il candidato esiste ma parte dal posto sbagliato.
- Da questa versione il massimo e la sua interpolazione si cercano sulla **differenza** fra lo spettro lisciato e il fondo stimato, invece che sul loro rapporto, dove la pendenza del fondo pesa molto meno. Sugli stessi 800 slot l'aggancio sale dal 56,1 % al 67,0 % e le decodifiche aumentano del **18,3 %**, senza righe impreviste e con zero righe su 500 slot di solo rumore. Il costo è trascurabile: 42 candidati per slot invece di 38, e lo stesso tempo di decodifica.
- La correzione è accesa in FT2, dove è stata misurata. FT4 usa la stessa ricerca ma non è stato misurato, quindi lì resta opzionale. `DECODIUM_FONDO=0` riporta esattamente al comportamento precedente; `DECODIUM_FONDO=mediana` sostituisce la quartica con una mediana scorrevole, che decodifica quasi altrettanto ma raddoppia i candidati e il tempo di decodifica, ed è il motivo per cui non è il default.

### Decodium RX nel menu ☰

- "Decodium RX — terminal receiver" apre il ricevitore FT8/FT4/FT2 da terminale in una console propria, senza interfaccia grafica. Su Windows ottiene una console vera; su Linux usa l'emulatore di terminale di sistema.
- Il terminale accetta ora la frequenza anche in kHz oltre che in MHz, e non stampa più gli avvisi di Qt Multimedia sopra le domande iniziali.

### Strumenti di misura

- Con `DECODIUM_LLR_DUMP=<file>` ogni candidato FT2 scrive un record binario con i suoi 174 LLR, la frequenza e il DT stimati, il punteggio di sincronismo e l'esito della decodifica. Serve a separare il demodulatore dal decodificatore, così decodificatori diversi si confrontano fuori dal programma sugli stessi identici candidati. Senza la variabile non viene scritto nulla e il comportamento non cambia. Formato e convenzioni: `doc/fastldpc/llr_dump.md`.

### Assorbito dalle versioni upstream 1.0.635 e 1.0.636

- Il conteggio delle raffiche AutoCQ riconosce le chiamate TX6 personalizzate come `TEST VY2XT FN86`.
- I nomi di banda ADIF sono normalizzati in tutte le destinazioni di log, datagrammi UDP compresi.
- Il tasto SuperFox del cruscotto attiva il modo e seleziona Hound quando serve, ed è bloccato durante TX o Tune.
- I divisori dei pannelli del cruscotto ricordano l'altezza scelta dopo il riavvio.
- Transizioni SuperFox stabilizzate e avvisi sui nominativi cercati ripristinati.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 è allegato a questo rilascio; i workflow GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
