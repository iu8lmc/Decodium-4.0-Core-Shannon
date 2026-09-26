# Decodium 4 FT2 v1.0.611

Version 1.0.611 takes upstream 1.0.610 in full (a cross-platform fix for the
Live Map greyline) and lands the learned gate for FT2's LDPC decoder, off by
default and explicitly not yet trustworthy — a research step, not a claim.

## English (UK)

### A learned gate for the FT2 decoder — ported, not yet retrained

The FT2 decoder's anti-false-decode gate has always worked on one number: the
normalised soft distance of a candidate that already passed its CRC-14
check. FASTLDPC-AI-SPEC-001 §2 proposed replacing that single threshold with
a small learned classifier over ten features of the candidate — how many
bits it flipped, how many min-sum iterations it took, how many checks stayed
unsatisfied, and so on — deciding accept or reject instead of a distance
cutoff alone. That classifier existed only in the research package until
this release; it now lives in `Detector/fastldpc/`, wired behind
`DECODIUM_LDPC_GATE=1`.

Porting it was not a copy of the research files: the production decoder had
moved on since the classifier was built (a different min-sum normalisation
constant, a different candidate-pairing search, wider candidate spans), and
importing the old decoder header wholesale would have quietly reverted all
of that, already-measured work. The gate logic was re-applied onto the
current production decoder instead, field by field, leaving everything that
came after it untouched.

With the flag off — the default — behaviour is bit-identical to 1.0.610,
confirmed both by inspection (every new line sits behind a single
conditional) and by a direct on/off comparison on the same recording.

One thing needs to be said plainly: the classifier's weights are unchanged
from when the research package built them, against a decoder configuration
that no longer exists. Turning the flag on today is opening a test bench,
not enabling a validated feature — the same caution that applies to FT2's
soft prior from the previous release. Retraining on real exported LLRs, and
measuring false-accept rates against hundreds of thousands of trials rather
than a handful, is the work that has to happen before this is anything more
than a flag to experiment with.

## Italiano

### Un gate appreso per il decoder FT2 — portato, non ancora riaddestrato

Il gate anti-false-decode del decoder FT2 ha sempre lavorato su un solo
numero: la distanza soft normalizzata di un candidato che ha gia' superato
la CRC-14. FASTLDPC-AI-SPEC-001 §2 proponeva di sostituire quella soglia
unica con un piccolo classificatore appreso su dieci caratteristiche del
candidato — quanti bit ha dovuto correggere, quante iterazioni di min-sum
sono servite, quanti controlli sono rimasti insoddisfatti, e cosi' via —
che decide accettare o rifiutare invece di limitarsi a un taglio sulla
distanza. Quel classificatore esisteva solo nel pacchetto di ricerca fino a
questa versione; ora vive in `Detector/fastldpc/`, dietro
`DECODIUM_LDPC_GATE=1`.

Portarlo non e' stato un copia-incolla dei file di ricerca: il decoder di
produzione era andato avanti da quando il classificatore era stato scritto
(una costante di normalizzazione del min-sum diversa, una ricerca delle
coppie di candidati diversa, intervalli di ricerca piu' ampi), e importare
il vecchio header del decoder avrebbe fatto regredire silenziosamente tutto
quel lavoro, gia' misurato. Il meccanismo del gate e' stato invece
riapplicato sul decoder di produzione attuale, campo per campo, lasciando
intatto tutto cio' che era venuto dopo.

Con il flag spento — il default — il comportamento resta identico bit per
bit alla 1.0.610, verificato sia per ispezione (ogni riga nuova sta dietro
un solo controllo condizionale) sia con un confronto diretto acceso/spento
sulla stessa registrazione.

Una cosa va detta senza giri di parole: i pesi del classificatore sono
rimasti quelli di quando il pacchetto di ricerca li ha costruiti, contro una
configurazione del decoder che non esiste piu'. Accendere il flag oggi
significa aprire un banco di prova, non attivare una funzione validata — la
stessa cautela che vale per l'a priori morbido di FT2 della versione
precedente. Riaddestrare sui LLR reali esportati da Decodium, e misurare il
tasso di false accettazioni su centinaia di migliaia di prove invece che su
una manciata, e' il lavoro che deve ancora succedere prima che questo sia
qualcosa di piu' di un flag su cui sperimentare.
