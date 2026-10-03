# Decodium 4 FT2 v1.0.609

Version 1.0.609 takes upstream 1.0.608 in full and adds predictive decoding
for FT2: a station that repeats the same message verbatim can now be
confirmed at signals the normal decoder alone cannot close.

## English (UK)

### Predictive decoding for FT2, off by default

FT8 has long had a "type 8" a-priori pass: when a station has been heard two
slots ago and repeats the same 77-bit message, the receiver does not need to
guess it again, only confirm it. This release brings the same idea to FT2,
with a mechanism tuned to FT2's own decoder rather than a straight port.

Feeding the 77 known bits into the LDPC as a hard a-priori, the way FT8 does
it, was tried first and measured to add only about 1 dB: the OSD still has to
guess the 14 information bits that are not covered by the prior, and below
threshold it cannot. Because the message is fully known, the CRC and parity
determine a single compatible codeword, so the real question is not "which
message is this" but "is this signal present" — answered by comparing the
174 channel LLRs directly against that one codeword. That is the mechanism
that ships: a direct comparison, bypassing the LDPC search entirely for this
one hypothesis.

| | decode threshold |
|---|---|
| normal decoder | -16.6 dB |
| with type 8 | -19.6 dB |

**+3.0 dB**, on the bench that measures it (`ft2_make_test_wav`, fixed
frequency, ten seeds per point).

A second effect showed up once frequency drift entered the bench: below
threshold, `getcandidates2` sometimes never proposes a candidate near the
remembered frequency at all, so the hypothesis is never tried. The fix forces
one there directly, exempted from the usual sync gates, and the LDPC
comparison alone decides whether it is genuine — the same safety net as
above, not a relaxed one. Two bugs in that mechanism were caught and fixed
before publishing: forcing was skipped whenever an unrelated raw candidate
happened to land nearby (leaving that candidate subject to gates it could
still fail), and the verification's own frequency tolerance was tighter than
the estimation noise on the refined candidate, rejecting genuine matches by a
hair. After both fixes, drift up to ~20 Hz between the two slots is recovered
reliably; only past ~30 Hz — beyond the search's own capture range — does it
fall away, which is a physical limit, not a defect.

Safety: 541+ verifications against wrong hypotheses (unrelated messages,
correlated ones from the same station mid-QSO, pure noise) at the default
threshold, zero false accepts. Confirmed live on air over several hours
tonight: 649 genuine confirmations out of 87,982 attempts on real FT2
traffic, no false positives observed.

A second idea — biasing the LLRs toward the previous slot's message for a
QSO that has moved on, rather than requiring an exact repeat — was built,
bench-tested safe on isolated frequency pairs, and then found in the field to
misbehave badly on a very strong, long-running signal: the same message
appeared at well over a hundred different frequencies, some at signal levels
no real transmission could produce. It ships in the source, disabled, and is
explicitly documented as unproven rather than safe.

FT8's own type 8 gained an unrelated fix in this release: it shared its
message archive with FT2's, so a station heard on FT8 could be offered as a
hypothesis to FT2 and vice versa — never able to match, pure wasted
exposure. The two now keep separate archives.

Enable with `DECODIUM_FT2_AP_MSG=1`; behaviour with the flag unset is
bit-identical to 1.0.608.

## Italiano

### Decodifica predittiva per FT2, spenta di default

FT8 aveva gia' da tempo un passo a priori "tipo 8": una stazione sentita due
slot fa che ripete lo stesso messaggio di 77 bit non va piu' indovinata, solo
verificata. Questa versione porta la stessa idea a FT2, con un meccanismo
tarato sul decoder di FT2 e non un porting diretto.

Imporre i 77 bit noti come a priori duro nell'LDPC, come fa FT8, e' stato il
primo tentativo, e vale solo circa 1 dB: l'OSD deve comunque indovinare i 14
bit d'informazione non coperti dall'a priori, e sotto soglia non ci riesce.
Siccome il messaggio e' interamente noto, CRC e parita' determinano UNA sola
parola di codice compatibile, quindi la domanda giusta non e' "quale
messaggio e'" ma "questo segnale c'e'" — risposta che si ottiene confrontando
direttamente i 174 LLR di canale con quella parola. E' questo il meccanismo
pubblicato: un confronto diretto, che aggira del tutto la ricerca dell'LDPC
per questa singola ipotesi.

| | soglia di decodifica |
|---|---|
| decoder normale | -16,6 dB |
| con il tipo 8 | -19,6 dB |

**+3,0 dB**, sul banco che lo misura (`ft2_make_test_wav`, frequenza fissa,
dieci semi per punto).

Un secondo effetto e' emerso quando la deriva in frequenza e' entrata nel
banco: sotto soglia, `getcandidates2` a volte non propone mai un candidato
vicino alla frequenza ricordata, quindi l'ipotesi non viene mai provata. La
correzione ne forza uno li' direttamente, esente dai consueti cancelli del
sincronismo, e il solo confronto con l'LDPC decide se e' genuino — la stessa
rete di sicurezza di sopra, non una allentata. Due difetti in questo
meccanismo sono stati trovati e corretti prima della pubblicazione: la
forzatura veniva saltata ogni volta che un candidato grezzo non correlato
capitava vicino (lasciando quel candidato soggetto a cancelli che poteva
comunque fallire), e la tolleranza di frequenza della verifica era piu'
stretta del rumore di stima sul candidato affinato, respingendo per un pelo
corrispondenze genuine. Dopo entrambe le correzioni, la deriva fino a ~20 Hz
fra i due slot viene recuperata con affidabilita'; solo oltre i ~30 Hz — oltre
il raggio di cattura della ricerca stessa — il recupero cala, ed e' un limite
fisico, non un difetto.

Sicurezza: 541+ verifiche contro ipotesi sbagliate (messaggi scorrelati,
correlati della stessa stazione a QSO avanzato, rumore puro) alla soglia di
default, zero falsi accettati. Confermato in aria per diverse ore stanotte:
649 conferme vere su 87.982 tentativi su traffico FT2 reale, nessun falso
osservato.

Una seconda idea — spingere gli LLR verso il messaggio dello slot precedente
per un QSO che e' avanzato, invece di richiedere una ripetizione esatta — e'
stata scritta, misurata sicura al banco su una coppia isolata di frequenze, e
poi trovata in aria a comportarsi male su un segnale molto forte e
persistente: lo stesso messaggio compariva a oltre cento frequenze diverse,
alcune a livelli di segnale che nessuna trasmissione vera potrebbe produrre.
Resta nel sorgente, spenta, documentata esplicitamente come non dimostrata,
non come sicura.

Il tipo 8 di FT8 ha ricevuto in questa versione una correzione indipendente:
condivideva l'archivio dei messaggi con quello di FT2, quindi una stazione
sentita su FT8 poteva essere offerta come ipotesi a FT2 e viceversa — mai in
grado di corrispondere, puro spreco di esposizione. Ora i due tengono archivi
separati.

Si attiva con `DECODIUM_FT2_AP_MSG=1`; a flag spento il comportamento e'
identico bit per bit alla 1.0.608.
