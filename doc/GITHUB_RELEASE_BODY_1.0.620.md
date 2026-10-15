# Decodium 4 FT2 v1.0.620

## English (UK)

### Changes since v1.0.618

This release absorbs elisir80's upstream v1.0.619 and adds one local fix.

#### From elisir80 v1.0.619: FT8 own-callsign AP wiring fix

- Fixed missing wiring between the live embedded decoder and the existing
  protection against speculative own-callsign AP hypotheses (types 2-6).
  Previously an alternative bridge path set the protection, but the
  MainWindow live decode requests did not carry it into the worker, so the
  process-wide default left the hypotheses enabled regardless of the
  setting.
- Each FT8 decode request now carries its own AP-eligibility snapshot,
  applied and restored inside the worker's runtime mutex so a queued
  request can no longer change the gate of an in-flight decode.
- Own-callsign AP hypotheses are allowed during an FT8 transmission and for
  180 seconds from a recorded FT8 message TX start; tuning, an armed Auto
  TX, RX resume, and stale QSO state do not count as recent transmission.
  File decoding keeps its explicit AP behaviour. CQ and independent
  historical hypotheses are unaffected.
- Added `ap_mycall=0/1` to FT8 DECODEMETRIC diagnostics.
- Bolder band/frequency/mode labels and operating buttons (including
  inactive ones), a new SuperFox status indicator between the received
  report and the next-message display, and a compact width for the Next/TX
  message field with a tooltip showing the full text when shortened.

This is a confirmed wiring fix, not a claim that every possible false
decode addressed to the local station has been eliminated.

#### Amplifier: Speed field no longer resets after Search

The SPE amplifier telemetry reader (Settings, read-only - it never keys or
band-switches the amplifier) had a "Speed" field hard-coded to display
9600, even after "Search" found the amplifier answering at a different
baud rate. Reopening Settings always showed 9600 regardless of the port's
actual configuration, which could lead to typing the wrong baud rate by
hand. The field now reflects the amplifier's actual configured baud.

This was found while investigating a report that an SPE Expert amplifier
would not connect over USB in Decodium while working normally with SPE's
own software; the underlying cause (most likely USB port contention with
that software running at the same time) is separate and still open.

This release is published with the source code and platform packages built by
the GitHub Actions runners: Windows x64 executable, macOS Apple Silicon and
Intel DMGs, and Linux x86_64 and aarch64 AppImages.

## Italiano

### Modifiche dalla v1.0.618

Questa release assorbe la v1.0.619 di elisir80 e aggiunge un fix locale.

#### Da elisir80 v1.0.619: collegamento mancante dell'AP sul proprio nominativo FT8

- Corretto il collegamento mancante fra il decoder embedded live e la
  protezione già esistente contro le ipotesi AP speculative sul proprio
  nominativo (tipi 2-6). Prima la protezione veniva impostata solo da un
  percorso alternativo del bridge, ma le richieste live di MainWindow non
  la trasportavano al worker, lasciando le ipotesi attive indipendentemente
  dall'impostazione.
- Ogni richiesta di decodifica FT8 ora porta il proprio stato di
  ammissibilità AP, applicato e ripristinato dentro il mutex del runtime
  del worker: una richiesta in coda non può più cambiare il gate di una
  decodifica in corso.
- Le ipotesi AP sul proprio nominativo sono ammesse durante una
  trasmissione FT8 e per 180 secondi dall'inizio registrato di un messaggio
  FT8 trasmesso; tuning, Auto TX armato, ripresa RX e stato QSO non
  aggiornato non contano come trasmissione recente. La decodifica da file
  conserva il comportamento AP esplicito. CQ e ipotesi indipendenti dallo
  storico non sono toccati.
- Aggiunto `ap_mycall=0/1` alla diagnostica FT8 DECODEMETRIC.
- Etichette di banda/frequenza/modo e pulsanti operativi ora in grassetto
  (anche da inattivi), nuovo indicatore di stato SuperFox fra il rapporto
  ricevuto e il messaggio successivo, e larghezza compatta per il campo
  messaggio Next/TX con tooltip che mostra il testo completo se abbreviato.

È la correzione di un difetto di collegamento verificato, non la garanzia
che ogni possibile falsa decodifica verso la propria stazione sia stata
eliminata.

#### Amplificatore: il campo Speed non si azzerava più dopo Search

Il lettore di telemetria dell'amplificatore SPE (in Impostazioni, sola
lettura — non comanda mai né commuta banda sull'ampli) aveva il campo
"Speed" fisso a 9600, anche dopo che "Search" aveva trovato l'ampli
rispondere a un baud diverso. Riaprendo Impostazioni il campo tornava
sempre a 9600 indipendentemente dalla configurazione reale della porta,
inducendo a digitare a mano un baud sbagliato. Ora il campo riflette il
baud realmente configurato.

Trovato indagando una segnalazione secondo cui un amplificatore SPE Expert
non si collegava via USB in Decodium pur funzionando regolarmente con il
software proprio di SPE; la causa di fondo (probabilmente contesa della
porta USB con quel software aperto in parallelo) è distinta e resta
aperta.

Questa release viene pubblicata con il codice sorgente e i pacchetti prodotti
dai runner GitHub Actions: eseguibile Windows x64, DMG macOS Apple Silicon e
Intel, AppImage Linux x86_64 e aarch64.
