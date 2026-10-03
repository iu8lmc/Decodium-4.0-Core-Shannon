# Decodium 4 FT2 v1.0.612

Version 1.0.612 adds an opt-in way to send your station's radio, antenna,
power and current weather to whoever you just worked, packed into one extra
FT8/FT4 message right after the QSO is logged. Everything about it is off
by default.

## English (UK)

### Station + weather telemetry, off by default

FT8/FT4 already define a "telemetry" message type (i3=0, n3=5): 71 raw
bits, no callsign field, meant for exactly this kind of use. Decodium could
already decode one as a hex dump; this release adds a specific layout for
it — a signature byte, your locator, live temperature and wind from a free
weather API (Open-Meteo, no key needed), your transmit power, and a radio
and antenna picked from a short built-in list — and sends it as one extra
transmission immediately after a QSO is logged, if you have turned that on
in Settings > Station.

On the receiving end, if a correspondent's software sends the same layout
and you have the matching option enabled, Decodium shows a small popup with
the decoded fields instead of a plain hex string. This message type carries
no callsign, so the popup can only guess who it is from — a QSO logged with
you in the last minute on the same frequency — and says so plainly rather
than presenting a guess as a certainty.

Three independent switches control this, all off by default: sending it,
showing the popup on receive, and fetching weather automatically. The
existing free-text station fields (radio, antenna, QTH) are untouched and
still go into your log and ADIF exports as before — this is a separate,
compact encoding just for the over-the-air message, not a replacement for
them.

Verified: a clean full build, 11 unit tests covering the bit-packing and
its edge cases (including the one genuine trap — a signature byte whose
top bit is 1, so the hex text this produces is never truncated by the
existing leading-zero-stripping in the decoder), and a live one-shot
transmission that produced the exact expected payload. The one thing not
verified end-to-end in this release is the physical audio round trip on a
virtual-cable loopback test — an environment/routing issue on the test
machine, not in this code, and nothing in the existing FT2/FT8 modulator or
demodulator was touched to build this feature.

## Italiano

### Telemetria stazione+meteo, spenta di default

FT8/FT4 hanno gia' un tipo di messaggio "telemetria" (i3=0, n3=5): 71 bit
grezzi, nessun campo nominativo, pensato esattamente per questo uso.
Decodium sapeva gia' decodificarlo come dump esadecimale; questa versione
aggiunge un formato specifico — un byte di firma, il tuo locatore,
temperatura e vento in tempo reale da un'API meteo gratuita (Open-Meteo,
senza chiave), la tua potenza di trasmissione, e una radio e un'antenna
scelte da un breve elenco incorporato — e lo invia come trasmissione extra
subito dopo che un QSO e' stato loggato, se lo hai attivato in
Impostazioni > Stazione.

In ricezione, se il software del corrispondente invia lo stesso formato e
hai l'opzione corrispondente attiva, Decodium mostra un piccolo popup con i
campi decodificati invece del solo esadecimale. Questo tipo di messaggio
non porta il nominativo, quindi il popup puo' solo ipotizzare da chi
arriva — un QSO loggato con te nell'ultimo minuto sulla stessa frequenza —
e lo dichiara esplicitamente invece di presentare un'ipotesi come certezza.

Tre interruttori indipendenti governano tutto questo, spenti di default:
inviarla, mostrare il popup in ricezione, e recuperare il meteo in
automatico. I campi testo libero della stazione gia' esistenti (radio,
antenna, QTH) restano intatti e finiscono nel log e negli export ADIF come
prima — questa e' una codifica separata e compatta solo per il messaggio
via radio, non li sostituisce.

Verificato: build completa pulita, 11 test unitari che coprono
l'impacchettamento dei bit e i suoi casi limite (inclusa l'unica vera
trappola trovata — un byte di firma col bit piu' alto a 1, cosi' il testo
esadecimale prodotto non viene mai troncato dal taglio degli zeri iniziali
gia' presente nel decoder), e una trasmissione one-shot dal vivo che ha
prodotto esattamente il payload atteso. L'unica cosa non verificata fino in
fondo in questa versione e' il giro audio fisico completo su un test di
loopback con cavo virtuale — un problema di instradamento sulla macchina di
prova, non del codice, e nessuna riga del modulatore o demodulatore
FT2/FT8 esistente e' stata toccata per costruire questa funzione.
