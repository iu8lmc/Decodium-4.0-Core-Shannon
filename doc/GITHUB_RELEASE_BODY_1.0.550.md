# Decodium 4 FT2 v1.0.550

Two bodies of work in one release: the graphics and Linux reliability line from
elisir80, and — from this fork — a second Decodium you can open with a button,
once a defect that made that dangerous was out of the way.

## English (British)

### Open a second Decodium

A serial port belongs to one program at a time, and that holds between two
copies of Decodium too. The second one does not take the port: it connects to
the first, which shares it. **Settings → CAT → Open a second Decodium** now
takes a name and a button.

Before that could be offered, a defect had to go. Starting with `--rig-name`
wrote that name into `CurrentMultiSettingsConfiguration` at the root of the
settings: from then on the **first** instance also came up inside the second
one's profile. Anyone who tried to run two found their settings swapped at the
next restart, with no way of guessing why. `--rig-name` picks the
configuration for the instance being started, not for all of them; it no
longer writes to the root. Verified by actually starting a second instance
with the station on the air: the root key stayed on the operator's own
profile.

A new profile is born by copying the root, so it inherits this instance's
serial port and its shared-CAT server — both of which are already taken, and
the second copy would meet two errors at startup without having done anything
wrong. The button clears them: no serial port, sharing off, and on request the
network rig already pointed at our shared CAT (which it switches on if needed,
without touching permission to transmit — that keeps its own switch).

That is what makes the second instance worth having: a second radio or a
receiver of its own, its own audio device, and two bands listened to at once.
The message after startup says so, because the audio device and the radio have
to be chosen in there and nobody guesses it.

### From elisir80: GPU panadapter and Linux reliability

- **GPU-native 3D panadapter**: dedicated Qt RHI shaders let the graphics
  processor keep and draw the spectral history instead of copying every trace
  through the CPU. Ordered per-trace draws keep the inner ridges visible on
  Metal as well as OpenGL. The ordinary 2D route is untouched when 3D is off,
  and the asynchronous CPU implementation stays as an automatic fallback.
- **Linux graphics selection and diagnostics**: an optional, Linux-only
  **OpenGL GPU FFT** control; an eligibility probe for Vulkan devices on
  hybrid-GPU systems; better DRM accounting for Intel i915 and Xe; better
  primary-device choice on multi-GPU machines. Unsupported, failed or stalled
  accelerated paths fall back on their own.
- **AppImage keyring reliability**: one Linux environment for every external
  `secret-tool` call, and bundled linker, GLib and GIO paths no longer leak
  into it. The AppRun wrapper records the original host values before adding
  the bundled ones, and the aarch64 packaging keeps that wrapper.
- **Worked-before and RTL-SDR**: B4 strikethrough now behaves the same in Full
  Spectrum and Signal RX, logging a contact refreshes the worked-before cache
  immediately, and the RTL-SDR checkboxes are easier to see.

### Translations

The three strings that came with the GPU FFT control were in **no catalogue at
all** — not even the English one, so the interface would have shown them exactly
as they are written in the source. They are now in all fifteen languages, with
zero unfinished messages, along with the twelve new strings of the second
instance.

One thing had to survive translation in every language: that control moves the
panadapter's **visual** FFT to the graphics processor — not FT decoding. Anyone
who reads "GPU" on a decoding program understands the other thing.

### Validation

- Amplifier frame parser 10/10, updater 9/9, secure settings 10/10,
  worked-before 5/5, Linux DRM accounting 10/10.
- Four test executables still fail to build here, all pre-existing and none of
  them touched by this release: one wants a Fortran symbol that is not linked
  to it, the other three trip over a GCC 15.2 false positive inside Qt's own
  headers and two Windows symbols. The application itself links and runs.

---

## Italiano

### Apri un secondo Decodium

Una porta seriale la tiene un solo programma alla volta, e vale anche fra due
Decodium. La seconda non prende la porta: si collega alla prima, che gliela
condivide. **Impostazioni → CAT → Apri un secondo Decodium** ora ha un nome e
un bottone.

Prima però andava tolto un difetto. Avviare con `--rig-name` scriveva quel nome
in `CurrentMultiSettingsConfiguration`, nella radice delle impostazioni: da quel
momento anche la **prima** istanza ripartiva dentro il profilo della seconda.
Chi provava ad aprirne due si ritrovava le impostazioni scambiate al riavvio
successivo, senza poter capire perché. `--rig-name` sceglie la configurazione
per l'istanza che si sta avviando, non per tutte: ora non scrive più nella
radice. Verificato avviando davvero una seconda istanza con la stazione in
aria: la chiave di radice è rimasta sul profilo dell'operatore.

Un profilo nuovo nasce copiando la radice, quindi eredita la porta seriale e il
server della CAT condivisa di questa istanza — che però sono già occupati, e la
seconda copia si troverebbe due errori all'avvio senza aver fatto nulla di
sbagliato. Il bottone glieli toglie: seriale vuota, condivisione spenta, e su
richiesta il rig di rete già puntato sulla nostra CAT condivisa (che accende,
se serve, senza toccare il permesso di trasmettere: quello ha un interruttore
suo).

È questo che rende utile la seconda istanza: una seconda radio o un ricevitore
a parte, una scheda audio sua, e due bande ascoltate insieme. Il messaggio dopo
l'avvio lo dice, perché scheda audio e radio vanno scelte lì dentro e nessuno
lo indovina.

### Da elisir80: panadapter su GPU e affidabilità su Linux

- **Panadapter 3D nativo su GPU**: shader Qt RHI dedicati permettono al
  processore grafico di tenere e disegnare la storia dello spettro, invece di
  farne passare ogni traccia dalla CPU. I disegni ordinati traccia per traccia
  tengono visibili le creste interne sia su Metal sia su OpenGL. La via 2D
  normale resta intatta a 3D spento, e l'implementazione asincrona su CPU
  rimane come ripiego automatico.
- **Scelta della grafica e diagnostica su Linux**: un comando **FFT su GPU
  OpenGL**, opzionale e solo per Linux; una sonda di idoneità per i dispositivi
  Vulkan sui sistemi a GPU ibrida; conteggio DRM migliore per Intel i915 e Xe;
  scelta migliore del dispositivo primario sulle macchine multi-GPU. Le vie
  accelerate non supportate, fallite o bloccate ripiegano da sole.
- **Portachiavi delle AppImage**: un solo ambiente Linux per ogni chiamata
  esterna a `secret-tool`, e i percorsi di linker, GLib e GIO impacchettati non
  ci finiscono più dentro. Il lanciatore AppRun registra i valori originali
  dell'ospite prima di aggiungere i propri, e l'impacchettamento aarch64 quel
  lanciatore lo conserva.
- **Già lavorato e RTL-SDR**: la barratura B4 si comporta allo stesso modo in
  Full Spectrum e in Signal RX, registrare un contatto aggiorna subito la cache
  dei già lavorati, e le caselle dell'RTL-SDR si vedono meglio.

### Traduzioni

Le tre stringhe arrivate con il comando della FFT su GPU non erano in **nessun
catalogo**, nemmeno in quello inglese: l'interfaccia le avrebbe mostrate
esattamente come stanno scritte nel sorgente. Ora sono in tutte e quindici le
lingue, con zero messaggi non finiti, insieme alle dodici stringhe nuove della
seconda istanza.

Una cosa doveva sopravvivere alla traduzione in ogni lingua: quel comando
sposta sul processore grafico la FFT **visiva** del panadapter, non la
decodifica FT. Chi legge «GPU» su un programma di decodifica capisce l'altra
cosa.

### Verifica

- Analizzatore di trame dell'amplificatore 10/10, aggiornatore 9/9,
  impostazioni sicure 10/10, già lavorati 5/5, conteggio DRM su Linux 10/10.
- Quattro eseguibili di prova continuano a non costruirsi qui, tutti
  preesistenti e nessuno toccato da questa release: uno cerca un simbolo
  Fortran che non gli viene collegato, gli altri tre inciampano in un falso
  positivo di GCC 15.2 dentro le intestazioni di Qt e in due simboli Windows.
  L'applicazione si collega e parte.
