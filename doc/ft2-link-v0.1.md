# FT2-Link v0.1 Draft

Stato: bozza tecnica interna. Questo documento definisce un protocollo aperto
per chat e messaggi affidabili in Decodium. Non e' una implementazione o
ricostruzione del protocollo VarAC.

## Obiettivo

FT2-Link deve fornire un workflow "VarAC-like" sopra una famiglia di profili
Decodium:

- discovery e handshake su FT2-Link narrow;
- trasferimento dati testuale su waveform wide;
- ARQ pubblico, documentato e verificabile;
- limiti RF conservativi per evitare beacon o ritrasmissioni aggressive.

La scelta chiave e' separare protocollo e modem. Il protocollo ARQ resta lo
stesso, mentre il profilo fisico puo' essere FT2 narrow, 500 Hz o 2300 Hz.

## Profili

| Profilo | Nome utente | Scopo | Stato v0.1 |
| --- | --- | --- | --- |
| `NARROW` | FT2-Link Narrow | Discovery, CQ, handshake, fallback weak-signal | BEACON/HELLO/HELLO_ACK RF implementati |
| `W500` | FT2-Link 500 | Chat e form brevi in circa 500 Hz | primo target wide |
| `W2300` | FT2-Link 2300 | Chat piu' veloce e piccoli trasferimenti dati | DSP wide live implementato |

`NARROW` usa una waveform di controllo FT2-Link dedicata e stretta. `W500` e
`W2300` sono waveform Decodium wide: nessuno di questi profili deve essere
presentato come FT2 standard.

## Handshake

La sequenza prevista e':

1. una stazione trasmette beacon/CQ `NARROW` indicando i profili supportati;
2. la seconda risponde con `HELLO`;
3. le due stazioni scelgono il profilo migliore comune;
4. la sessione passa a `W500` o `W2300`;
5. i dati applicativi viaggiano in frame `DATA` confermati da `ACK`.

Se il profilo wide fallisce, la sessione torna a `NARROW` o termina.

Payload handshake v0.1:

| Byte | Campo | Note |
| ---: | --- | --- |
| 0 | magic | `H` |
| 1 | handshake_version | `1` |
| 2 | capability_flags | bit 0 `W500`, bit 1 `W2300`, bit 2 `W2300 FAST`, bit 3 `W2300 ROBUST` |
| 3 | profile | profilo preferito in `HELLO`, profilo negoziato in `HELLO_ACK` |
| 4 | w2300_rate_mode | `FAST` o `ROBUST` |
| 5 | status | `0` offer, `1` accepted, `2` rejected |
| 6 | call_len | opzionale, presente nei frame identitari |
| 7.. | call | nominativo ASCII maiuscolo, massimo 12 byte |
| next | locator_len | opzionale |
| next.. | locator | locator ASCII maiuscolo, massimo 8 byte |

`HELLO` e `HELLO_ACK` viaggiano in frame `NARROW`. I primi 6 byte restano
compatibili con i frame v0.1 iniziali; i byte successivi permettono al ricevitore
RF di ricavare il nominativo anche quando il frame arriva solo dal demodulatore
audio. La scelta v0.1 e' conservativa:

- se entrambe le stazioni supportano `W2300`, si sceglie `W2300`;
- altrimenti, se entrambe supportano `W500`, si sceglie `W500`;
- per `W2300`, `ROBUST` viene scelto se richiesto o se `FAST` non e' comune;
- se non esiste profilo wide comune, `HELLO_ACK` risponde `rejected`.

`BEACON` usa lo stesso payload `NARROW` con status `offer`, session id zero e
non apre sessione. Il bit `FlagEndOfMessage` nel frame comune viene riusato come
flag CQ per il beacon: impostato = stazione disponibile a chiamate, non impostato
= presenza/ascolto. Per i beacon CQ, `ack_bitmap` codifica anche il tipo CQ
speciale: `0=CQ`, `1=CHAT`, `2=NET`, `3=EMCOMM`, `4=TEST`, `5=QSY`. Il locator
CQ viene portato nel campo locator del payload identitario del beacon, quindi
rimane copiabile anche da decoder narrow che non conoscono ancora il tipo
speciale.

La pipeline audio specifica `W2300` puo' eseguire questo handshake come
preflight: salva i frame `HELLO`/`HELLO_ACK`, verifica la scelta lato iniziatore
e avvia i frame `DATA` solo se il profilo negoziato e' `W2300`. La pipeline
audio wide invece usa lo stesso handshake come selettore: se l'accordo cade su
`W2300` delega al modem 2300 Hz, se cade su `W500` passa al modem audio 500 Hz.

## Modello applicativo

`FT2LinkAppModel` e' il primo livello pensato per UI e integrazione app. Non
modula audio e non fa I/O radio: mantiene stato coerente per dashboard CQ,
sessioni e chat.

Responsabilita' v0.1:

- identita' locale e capability locali;
- advertisement locali per CQ/beacon;
- lista stazioni ascoltate con `heardAtMs`, filtro CQ e scadenza per eta';
- avvio sessione verso una stazione nota con generazione `HELLO`;
- risposta a `HELLO` remoto con `HELLO_ACK`;
- applicazione `HELLO_ACK` lato chiamante e passaggio a `Connected` o
  `Rejected`;
- log chat per sessione con messaggi outgoing pending/delivered e incoming
  received;
- chiusura sessione.

Questo livello include nominativo e locator nel payload `BEACON`,
`HELLO`/`HELLO_ACK` per discovery e sessioni RF. Nome e QTH esteso restano da
definire con messaggi di discovery ripetuti. Il modello stabilizza il contratto
operativo: CQ dashboard -> handshake -> sessione -> chat/log.

`FT2LinkQmlAdapter` espone questo modello a Qt/QML senza introdurre I/O radio.
Lo stato UI viene esportato come liste/mappe Qt per stazioni, sessioni e
messaggi; `HELLO` e `HELLO_ACK` passano come `QByteArray` serializzati con il
frame binario comune. Questo mantiene separati tre livelli: protocollo C++ puro,
adapter UI, e futuro trasporto audio/radio.

La UI `FT2LinkPanel.qml` e' integrata come modo applicativo `FT2-Link` nella
lista dei modi di emissione. Quando il modo e' attivo, il pannello FT2-Link
occupa inline l'area centrale normalmente usata da `RF Spectrum` e `Signal RX`;
questi due pannelli vengono nascosti o tenuti chiusi se erano staccati. Live Map
e waterfall/panadapter restano indipendenti e continuano a funzionare. La toolbar
non espone piu' un pulsante separato `D4 Link` e non esiste piu' il toggle
dashboard dedicato: l'ingresso operativo e' la selezione del modo.

FT2-Link e' protetto da un gate temporaneo finche' il modo non e' pronto per
uso pubblico. La UI non permette di creare o cambiare password: se la build non
contiene un salt e un hash PBKDF2-HMAC-SHA256 validi, la selezione di
`FT2-Link` viene rifiutata e il modo torna a `FT2`. Se il gate e' provisionato,
la UI chiede la password; `Cancel` o password errata chiudono il dialogo e
ripristinano `FT2`. La password non viene salvata in chiaro nelle impostazioni
utente. I parametri si generano localmente con `tools/ft2link_gate_hash.py` e si
passano a CMake come `DECODIUM_FT2LINK_ACCESS_SALT_B64`,
`DECODIUM_FT2LINK_ACCESS_HASH_B64` e `DECODIUM_FT2LINK_ACCESS_ITERATIONS`.
Questo gate serve a bloccare l'accesso nelle build distribuite; non va trattato
come DRM contro chi modifica o ricompila il sorgente.

Il pannello espone CQ/stazioni, profilo `W500`/`W2300`, `FAST`/`ROBUST`,
sessioni e chat log usando il context property `ft2Link`. In questa fase e' un
banco operativo locale e radio: puo' creare stazioni osservate/manuali, generare
`HELLO`, applicare un `HELLO_ACK` di loopback, trasmettere un testo dentro la
pipeline audio wide locale e preparare/inviare audio RF reale tramite il bridge.
Il risultato aggiorna il messaggio a `Delivered`/`Failed` e pubblica metriche
`profile`, burst, retry, drop, throughput e utilizzo canale. Le preferenze
operative del pannello, inclusi `W500`/`W2300`, `FAST`/`ROBUST` e intervallo
beacon, vengono salvate nelle impostazioni Decodium. Le sessioni possono essere
chiuse localmente dalla UI; una sessione `Closed` resta visibile nel log ma non
accetta nuovi messaggi o piani TX.

Le tab operative restano inline nel pannello principale. Lo stack strumenti usa
un'altezza responsive, liste con scrollbar e la tab `INFO` ha scroll verticale
dedicato per non tagliare toggle/privacy e campi profilo su layout bassi.

Il composer include macro/canned messages locali per ridurre digitazione in RF.
Il primo set fisso (`INFO`, `NAME`, `QTH`, `LINK`, `73`) e' stato esteso con
un catalogo tag stile VarAC: dati locali (`MYDATA`, `RIG`, `EMAIL`, `ICE`,
`GPS`, `UTC`), richieste operative (`SR`, `INFO?`, `LOC?`, `LHR`, `LHC`, `FSR`,
`VM?`, `LC?`, `GPS?`, `BBS?`, `GET`, `VER`), gestures (`TU`, `LIKE`, `DING`),
stato (`AWAY`), is-typing (`TYP`, `TYP0`), inviti QSY (`QSY+`, `QSY-`),
verbose SNR/test link (`VSNR`, `TL`), risposte legacy (`SFRD`, `SMR`,
`NOPLAY`) e chiusura sessione (`DISC`).
L'espansione
avviene nell'adapter prima dell'invio e usa tag pubblici come `<MYCALL>`,
`<MYGRID>`, `<MYLOC>`, `<NAME>`, `<QTH>`, `<EMAIL>`, `<ICE>`, `<RIG>`, `<ANT>`,
`<PWR>`, `<GPSLOC>`, `<CALL>`, `<HCALL>`, `<HLOC>`, `<HNAME>`, `<PROFILE>`,
`<RATE>`, `<UTC>`, `<UTCDT>`, `<UTCD>`, `<UTCT>` e `<MYDT>`. La macro inserisce
solo testo nel campo messaggio: non trasmette automaticamente.

La tab `PRE` aggiunge preset/canned messages custom persistenti. Ogni preset ha
label, template e descrizione, viene salvato nello store FT2-Link e viene
mostrato insieme al catalogo fisso della tab `CHAT`; doppio uso pratico:
messaggi rapidi ricorrenti e testi EmComm/check-in. La stessa tab include un
helper "Wednesday check-in" che genera il corpo standard e puo' preparare la
mail con destinatario `varacwednesday@gmail.com` e subject `VarAC Wednesday
Check-In`, oppure inserire solo il body nel composer chat.

La tab `INFO` della UI contiene il profilo operatore locale salvato nelle
impostazioni Decodium: name, QTH, email, rig, antenna, power, ice breaker e GPS.
Questi campi alimentano i tag VarAC-like `<NAME:...>`, `<QTH:...>`, `<LOC:...>`,
`<EM:...>`, `<RIG:...>`, `<ANT:...>`, `<PWR:...>`, `<ICE:...>` e `<GPS:...>`.
Restano dati opzionali e locali; nessun valore viene inviato se l'operatore non
inserisce una macro o una risposta.

La stessa tab `INFO` contiene lo stato presenza locale. `AWAY` accoda un
messaggio con `<AWAY>` oppure `<AWQ>` se l'operatore accetta inviti QSY mentre
e' assente; `WELCOME` accoda un saluto quando un `HELLO` entrante viene
accettato. Il messaggio viene inserito come chat `Outgoing/Pending` nella
sessione: non parte alcuna trasmissione RF autonoma e resta necessario il normale
flusso `ARM`/`RF TX`. Testi e flag sono salvati nello store FT2-Link locale.
`AUTO AWAY` puo' attivare automaticamente lo stato `AWAY` dopo inattivita'
dell'applicazione. L'attivazione automatica e' runtime: nello store vengono
salvati solo abilita/intervallo e l'eventuale `AWAY` manuale, non il fatto che
il timer abbia messo temporaneamente assente la stazione. Un input operatore
toglie solo l'AWAY automatico, non quello impostato manualmente.
La stessa area contiene due timer QSO locali: `CALL ID` accoda
`DE <MYCALL> <ID>` ogni N minuti dentro le sessioni connesse, ma non trasmette
RF automaticamente e non duplica il messaggio se un Call ID automatico e' gia'
`Pending`; `AUTO DISC` chiude localmente una sessione dopo N minuti senza
attivita' reale, ignorando i Call ID automatici per evitare che tengano viva una
sessione abbandonata. `0` disabilita ciascun timer. I valori sono salvati nello
store FT2-Link.
L'opzione `AUTO REPLY` accoda risposte sicure per richieste Inquire/BBS
copiabili dallo stato locale (`INFO`, `LOCR`, `LHR/LHC`, `FSR`, `VRP`, `LCR`,
`GPSR`, `VER`, `BLR` e reject `BGJ` quando il file BBS non esiste). Anche in
questo caso si tratta solo di un messaggio `Outgoing/Pending`: nessun CAT/PTT o
invio RF viene eseguito senza azione operatore.

La tab `BLK` mantiene una block list locale di nominativi. I nominativi bloccati
non compaiono nella dashboard CQ/last-heard, non aggiornano la callsign history
e vengono ignorati per beacon, CQ, broadcast e ping. Un `HELLO` da nominativo
bloccato riceve un `HELLO_ACK` con stato `rejected`, cosi' il chiamante chiude la
sessione senza passare ai profili wide. La lista e' salvata nello store
FT2-Link; non e' trasmessa in RF e non modifica messaggi gia' presenti nei log.

Il parser RX v0.1 interpreta ora i tag ricevuti in chat e li trasforma in righe
`System` nel log sessione. Gestisce dati partner (`<NAME:...>`, `<LOC:...>`),
report (`<R+NN>`), stato (`<AWAY>`, `<AWQ>`), richieste (`<SR>`, `<INFO>`,
`<LOCR>`, `<LHR>`, `<VER>`), profilo remoto (`<QTH:...>`, `<EM:...>`,
`<RIG:...>`, `<ANT:...>`, `<PWR:...>`, `<ICE:...>`, `<GPS:...>`), inviti QSY
(`<Q>`, `<QSYU>`, `<QSYD>`, `<Q:...>`, `<QF:...>`) e chiusura (`<DISC>`). Le
richieste producono una risposta suggerita ma non trasmettono da sole; `<DISC>`
chiude la sessione localmente dopo la consegna/ACK.

`<TYP>` e `<TYP0>` implementano il primo indicatore "is typing": `<TYP>` marca
il corrispondente come in digitazione per circa 12 secondi e `<TYP0>` cancella
subito l'indicatore. Il composer include entrambi i tag come canned messages,
ma FT2-Link non li trasmette automaticamente ad ogni pressione di tasto: devono
seguire il normale flusso chat/ARM/RF TX scelto dall'operatore.

Le richieste Inquire v0.1 coperte sono `<LHC:CALL>`, `<FSR>`, `<VRP>`, `<LCR>` e
`<GPSR>`. L'adapter risponde solo con righe `System`: last-heard specifico usa la
callsign history locale, frequency scheduler usa il primo preset locale e torna
`<FS:...>` oppure `<FSO>` se non esistono preset, parked VMail produce `<VW>`
solo se la mailbox locale ha messaggi parcheggiati per il partner,
last-connections deriva dal QSO log locale e GPS usa il profilo operatore. Sono
riconosciute anche risposte/rifiuti
`<LHE>`, `<LHJ>`, `<LHCE>`, `<FSO>`, `<VRPJ>`, `<VW>`, `<LC:...>` e `<LCJ>`.
Il toggle persistente `LH PEEK` disabilita la pubblicazione della lista
last-heard locale: in quel caso `<LHR>` e `<LHC:CALL>` producono `<LHJ>` anche
quando la callsign history contiene dati. Il toggle persistente `LC PEEK`
disabilita la pubblicazione delle ultime connessioni: `<LCR>` produce `<LCJ>`
anche quando il QSO log contiene contatti recenti. Il toggle persistente
`VM PEEK` disabilita l'interrogazione dei messaggi parcheggiati: `<VRP>` produce
`<VRPJ>` anche quando esistono VMail in attesa per il richiedente. Il toggle
persistente `SNR TX` controlla solo il suggerimento locale per `<SR>`; se e'
spento non viene proposta la risposta `<R+00>`. Il toggle persistente `INFO REQ`
disabilita suggerimenti e
auto-reply per `<INFO>`, `<LOCR>` e `<GPSR>`, evitando di esporre profilo,
locator e GPS locali; `<VER>` resta una risposta tecnica non personale. Il
toggle persistente `VSNR OK` accetta automaticamente un invito `<VSI>` accodando
`<VSIR>` come messaggio pending quando l'auto-reply queue e' attiva; non aziona
PTT e non invia RF da solo.
Per evitare una griglia ingestibile di toggle, il tab `INFO` espone anche tre
preset privacy sopra ai controlli avanzati: `OPEN` mantiene il comportamento
aperto classico, `CONTROL` lascia attivi ping, last-heard, SNR, info e relay
parking ma nasconde last-connections e parked-VMail peeking, `QUIET` spegne
disclosure automatiche, ping in ingresso e relay parking. Dopo un preset
l'operatore puo' ancora modificare manualmente ogni singolo toggle; lo stato
diventa `CUSTOM` se la combinazione non corrisponde piu' a un preset.
QSY response (`<QSYR>`, `<QSYJ>`, `<QJO>`), verbose SNR (`<VSI>`, `<VSIR>`,
`<VSIJ>`, `<VSS>`), idle warning (`<IE>`, `<AE>`) e test-link `<TL>` sono
registrati come eventi manuali, senza cambio frequenza o disconnessione
automatica.

Il blocco AI/gateway v0.1 e' solo parser/composer passivo: `AI:`, `<DISAI>`,
`<A>`, `<EA>`, `<ACIE>`, `<AIJ>`, `<AIE>` e `<AIL>` vengono riconosciuti nel log
sessione. FT2-Link non esegue chiamate AI, non interroga Internet e non genera
risposte automatiche; un eventuale gateway dovra' essere progettato come modulo
separato, esplicito e limitato dall'operatore.

Il parser riconosce anche gesture/suoni testuali (`HIHI!`, `TU!`, `LIKE!`,
`BYE!`, `COOL!`, `FB!`, `DING`, `RING`, `HIHIW!`, `HIHIM!`) e li registra come
eventi; non riproduce suoni automaticamente. I tag file/VMail legacy VarAC
(`<SF:...>`, `<SFRD>`, `<SFOK>`, `<SFFA>`, `<SFAB>`, `<SFB...>`, `<SM>`,
`<TO:...>`, `<FRM:...>`, `<TME:...>`, `<SBJ:...>`, `<MSG:...>`, `<EG>`, `<EJ>`,
`<U>`, `<SMR>`, `<SMF>`, `<SMFP>`) sono letti come stato interoperativo ma non
vengono convertiti in traffico nativo: FT2-Link continua a usare `FT2M1` e
`FT2FILE1`. Anche i tag HamPlay (`<P:...>`, `<PM:...>`, `<PA>`, `<PE>`, `<PJ>`,
`<PR>`) restano passivi; non esiste un motore gioco integrato.

La UI CHAT include anche un primo helper QSY/slot manuale. Lo slot size e'
VarAC-like a 750 Hz, con cinque slot sopra e cinque sotto la calling frequency.
Il pulsante slot cicla gli offset e `QSY` inserisce il tag relativo: `+750`
produce `<QSYU>`, `-750` produce `<QSYD>`, gli altri offset usano `<Q:+/-NNN>`
in deca-Hz come da convenzione VarAC. Non viene eseguito nessun cambio CAT
automatico: per W500/W2300 l'operatore deve ancora verificare waterfall, band
plan e occupazione dello slot.

Il pannello include anche un primo broadcast `BCAST` one-to-many su `NARROW`.
Non apre sessione, non usa ARQ e non promette consegna: serve per messaggi brevi
di rete, check-in e allerta. I broadcast ricevuti vengono salvati in un log
locale dell'adapter e scansionati con alert tag predefiniti (`SOS`, `MAYDAY`,
`EMERGENCY`, `URGENT`, `MEDICAL`, `EVAC`, `QSY`) piu' tag custom configurabili
dalla tab `BCAST` e persistenti nello store FT2-Link. Un match crea un evento
alert locale e aggiorna lo stato UI.

Path Finder v0.1 usa broadcast `NARROW` brevi e non garantiti per cercare un
relay VMail. La richiesta compatta e':

```text
P? TARGET REQUESTOR
```

La risposta compatta e':

```text
P! TARGET VIA [LOCATOR] [AGEM]
```

Entrambe devono restare entro la capacita' `NARROW` di 32 byte. L'adapter
accetta anche le forme leggibili `PATH? TARGET FROM CALL` e `PATH! TARGET VIA
CALL LOC GRID AGE Nm` in RX, ma la UI trasmette il formato compatto. Una
stazione puo' rispondere solo se `TARGET` e' nella callsign history locale ed e'
stato sentito nelle ultime 24 ore. La ricezione di una richiesta o risposta Path
Finder crea un alert locale `PATH`; non parte nessuna trasmissione automatica.
L'operatore deve armare e premere `PATH!` per rispondere.

Le risposte `P!` vengono anche salvate come hint strutturati `target -> relay`
per collegare meglio Path Finder e VMail. Se esiste una mail parcheggiata verso
`TARGET`, la station list mostra un workflow relay sulla stazione `VIA`:
badge breve `RLY>TARGET`, riga operativa `Relay VIA -> TARGET / mail N` con
priorita' `URGENT`/`EMCOMM`, eta' dell'hint e soggetto piu' recente. Il bottone
`FWD` sulla riga stazione prepara la VMail parcheggiata e apre/avvia la sessione
con il relay. Il pannello `BCAST/PATH` mostra la stessa informazione con tre
azioni locali: `MAIL` prepara il tab mailbox sul destinatario finale, `CALL`
apre/avvia una sessione con il relay suggerito, `FWD` fa entrambe le cose quando
esiste gia' una mail parcheggiata pronta. La mail resta comunque indirizzata al
destinatario finale: il relay ricevente la salvera' come direzione `Relay` e la
consegnera' quando sentira' `TARGET`.

`PING`/`PING_ACK` v0.1 e' un controllo RF `NARROW` one-shot per misurare se una
stazione risponde senza aprire una sessione wide. `PING` usa `session_id = 0`,
`sequence` come token locale e payload con il nominativo del mittente. Se
`autoAck` e' attivo, il ricevitore risponde con `PING_ACK` mantenendo lo stesso
token e payload con il proprio nominativo. La stazione chiamante misura l'RTT
localmente dalla differenza tra invio `PING` e ricezione `PING_ACK`; non e' una
misura di propagazione assoluta e non deve essere usata per trasmissioni
automatiche aggressive. La risposta ai ping in ingresso puo' essere disabilitata
localmente: in quel caso il ping viene registrato come `Rejected` e non parte
alcun `PING_ACK`.

La mailbox v0.1 fornisce il primo blocco "VMail-like" senza introdurre un
secondo ARQ. Il contenuto viene incapsulato come envelope applicativo `FT2M1`
e trasportato dentro normali frame `DATA` della sessione `W500`/`W2300`; quindi
eredita ACK, retry e adattamento `W2300` gia' presenti. La UI mantiene inbox e
outbox locali in memoria, con stati `Pending`, `Delivered`, `Failed` e
`Received`. In chat non viene mostrato l'envelope tecnico: il log espone una
riga leggibile `MAIL to CALL: subject` o `MAIL from CALL: subject`.

Envelope mailbox:

```text
FT2M1|TO|FROM|SUBJECT_HEX_UTF8|BODY_HEX_UTF8
FT2M2|TO|FROM|FLAGS|SUBJECT_HEX_UTF8|BODY_HEX_UTF8
FT2RLY1|TO|VIA|FROM|FLAGS|HOPS|SUBJECT_HEX_UTF8|BODY_HEX_UTF8
```

`TO` e `FROM` sono nominativi normalizzati maiuscoli. `SUBJECT` e `BODY` sono
UTF-8 codificati in hex per evitare collisioni con separatori e mantenere il
payload trasparente e documentato. `FT2M1` resta la forma base compatibile.
`FT2M2` aggiunge `FLAGS`: `U` indica VMail urgente, `E` indica VMail EmComm,
`UE` indica entrambe, `N` indica priorita' normale. La UI usa `FT2M2` solo
quando serve almeno un flag speciale; altrimenti continua a generare `FT2M1`.
`FT2RLY1` e' il wrapper relay RF strutturato: `TO` resta il destinatario finale,
`VIA` e' il relay/session peer scelto per l'hop corrente, `FROM` e' l'origine
del messaggio, `HOPS` e' un contatore 1-9. Subject e body restano identici al
formato mailbox, quindi l'envelope e' pubblico, ispezionabile e non offusca il
contenuto della comunicazione. Il limite a 9 hop e' hard: il destinatario finale
puo' ancora ricevere un envelope a hop massimo, ma un relay intermedio non lo
parcheggia e non lo reinoltra. In quel caso viene registrato un evento locale
`RELAY MAIL hop limit`; se l'operatore prova a reinoltrare una mail gia' al
limite, l'azione `RELAY` viene rifiutata e la riga mailbox passa a `Failed`.

La mailbox include anche un primo parking/relay locale stile VarAC. `PARK`
salva una mail senza sessione attiva con stato `Parked`; quando il destinatario
viene sentito tramite osservazione stazione o beacon RF, l'adapter porta il
messaggio a `Relay ready`, registra `relayNotifiedAtMs` e crea un alert locale
`MAIL`. Se una stazione riceve un envelope `FT2M1` destinato a un terzo, non lo
tratta come inbox personale: lo salva come direzione `Relay` e stato `Parked`.
La station list espone lo stato operativo: `MAIL N` indica mail parcheggiata per
quella stazione, mentre `RLY>TARGET` indica che quella stazione e' un candidato
relay trovato da Path Finder per una mail gia' parcheggiata verso `TARGET`.
Il toggle persistente `VM PARK` permette di rifiutare questi envelope relay:
la chat mostra il rifiuto operativo, ma la mailbox locale non salva il messaggio.
Quando l'operatore apre una sessione con il destinatario o con un relay suggerito
da Path Finder, la UI abilita `RELAY`; l'invio usa `FT2RLY1`, aggiorna
`relayProtocol`, `relayViaCall` e `relayHopCount`, e dopo ACK marca la mail
`Delivered` se era il destinatario finale o `Forwarded` se era un intermediario.
La notifica viene emessa una sola volta per messaggio parcheggiato e viene
conservata nel local store. Le stazioni nuove riconoscono `FT2RLY1` e mostrano
la mail come relay strutturato; gli envelope legacy `FT2M1/FT2M2` destinati a
terzi restano supportati e vengono parcheggiati come prima.

La tab `MAIL` espone anche gestione locale: lista mailbox con stato, priorita',
peer, soggetto/corpo, marcatura `READ`/`NEW` per mail incoming e cancellazione
per singolo elemento. Il contatore unread e' visibile nella barra superiore e
nel tab `MAIL` (`MAIL*` quando esiste posta incoming non letta); la barra mostra
anche `rly` con gli elementi relay ancora attivi. Le righe relay mostrano `RLY`
o `RLYN` quando arrivano da `FT2RLY1`, il peer visualizzato diventa il `VIA`
quando disponibile, e `RLYQ` copia una coda relay stampabile con protocollo,
via e hop count. `COPY` esporta una versione testuale stampabile della mailbox
con flag `URGENT`, `EMCOMM` e `UNREAD`; il bundle log completo include mailbox
e relay queue. `EMAIL` genera un draft `mailto:` per il client email di sistema
quando trova un indirizzo nel messaggio o nel profilo locale, altrimenti copia
comunque il payload `.eml`. `EML` salva il singolo messaggio come file RFC 5322
con header `X-Decodium-FT2Link-*`, body UTF-8 e priorita' VMail.

La tab `MAIL` contiene ora anche il primo gateway SMTP reale. Il toggle `GW`
abilita l'azione locale, i campi `host`, `port`, `STARTTLS/TLS/NONE`,
`username` e `from` configurano il server, mentre la password viene scritta solo
nel secure store della piattaforma tramite `SAVE` e rimossa con `CLR`: macOS usa
Keychain, Windows DPAPI e Linux `secret-tool`. Se il backend sicuro non e'
disponibile, Decodium rifiuta il salvataggio invece di cadere su QSettings in
chiaro. `TEST` apre la sessione SMTP, negozia TLS se richiesto, autentica con
`AUTH LOGIN` quando e' presente uno username e chiude con `QUIT` senza inviare
`DATA`, quindi serve a verificare host/porta/TLS/password prima di usare il
gateway. Il pulsante `SMTP` su ogni VMail invia invece il payload `.eml`
direttamente al server configurato; lo stato asincrono
`Queued/Connected/TLS/Sending/Sent/Failed/Ready` torna in UI tramite
`ft2LinkEmailGatewayStatus`. Per le VMail reali lo stato viene anche salvato
nella mailbox (`emailGatewayState`, `emailGatewayDetail`,
`emailGatewayAtMs`), incluso in `COPY`/bundle e ripristinato dallo store locale.
Gli indirizzi Internet non vengono mai inseriti in RF automaticamente: `SMTP`,
`TEST`, `EMAIL` ed `EML` sono azioni locali esplicite dell'operatore e non
modificano messaggi gia' in trasmissione.

La UI principale contiene una status strip globale sotto l'header FT2-Link:
`RF` mostra l'ultimo piano TX/stato radio, `QUEUE` sintetizza relay queue,
unread mailbox, outbox logbook `queued/submitted/failed` e alert, mentre `ERR`
mostra l'ultimo errore operativo. La strip resta visibile anche passando tra
CHAT, MAIL, BCAST/PATH, LOG e INFO, cosi' le code pendenti e gli errori non
restano nascosti dentro un singolo tab.

Forms EmComm v0.1 usa lo stesso trasporto affidabile. I template iniziali sono
`ICS213`, `SITREP` e `CHECKIN`; i campi restano un JSON UTF-8 compatto
codificato in hex, cosi' ogni stazione puo' ispezionare il contenuto senza
reverse engineering.

```text
FT2FORM1|TO|FROM|FORM_TYPE|FIELDS_JSON_HEX_UTF8
```

Il file transfer v0.1 e' deliberatamente piccolo: massimo 4096 byte di testo
UTF-8 per oggetto nella UI iniziale. Il payload include dimensione dichiarata e
SHA-256 per evitare di salvare contenuti corrotti anche se il layer ARQ dovrebbe
gia' bloccare errori residui.

```text
FT2FILE1|TO|FROM|FILENAME_HEX_UTF8|SIZE|SHA256_HEX|CONTENT_BASE64
```

Il BBS/bulletin v0.1 e' una bacheca testuale affidabile dentro una sessione
wide, non un broadcast narrow. Serve per annunci di rete o note operative che
devono essere confermate dal destinatario sessione.

```text
FT2BBS1|FROM|GROUP|TITLE_HEX_UTF8|BODY_HEX_UTF8
```

Il sottoinsieme BBS file v0.1 usa tag chat/control manuali, non un download
automatico:

```text
<BLR>                         richiesta lista file BBS
<BL:name|yyyy-mm-dd|bytes>    elemento lista file
<BLJ>                         lista rifiutata o non disponibile
<BG:name>                     richiesta download file
<BGJ>                         download rifiutato
```

La lista locale viene generata dalla cache dei piccoli file FT2-Link gia'
ricevuti/inviati e limita la risposta suggerita ai cinque elementi piu' recenti.
Se un partner chiede `<BG:name>` e il file e' presente, la UI suggerisce di usare
`FILE TX`; se manca, suggerisce `<BGJ>`. Nessun file viene trasmesso
automaticamente.

L'adapter mantiene anche QSO log e callsign history locali. Il QSO log deriva
dalle sessioni FT2-Link e conserva session id, nominativo remoto, profilo,
rate `W2300`, stato, evento recente, orari e numero messaggi. La callsign
history aggrega beacon, CQ, sessioni, messaggi applicativi, broadcast e alert
per nominativo. I callsign tag v0.1 sono etichette locali persistenti, massimo
16 caratteri maiuscoli, mostrate in last-heard/station list e nello storico
contatti; si impostano dal campo `TAG` della stazione manuale o dal tab `CALL`
e non vengono trasmesse in RF. Il tab `CALL` permette anche di modificare
locator, nome e un commento locale del contatto, sempre dentro lo store JSON.
`contactTimeline(call)` produce lo storico filtrato del nominativo combinando
QSO, chat live della sessione attiva, broadcast/alert, mailbox, forms, file,
bulletin e ping; la UI lo mostra nel tab `CALL` accanto alla lista contatti.
`qslCard(sessionId)` produce una riga QSL testuale pronta da inserire in chat o
in mailbox; non e' ancora una cartolina grafica.
`adifRecord(sessionId)` produce un record ADIF copiabile dalla UI con il
pulsante `ADIF`; `adifLog()` restituisce un documento ADIF completo per i QSO
FT2-Link locali. Per compatibilita' con il logging Decodium/ADIF, il record usa
`MODE=MFSK` e `SUBMODE=FT2`, aggiungendo `APP_DECODIUM_MODE=FT2-LINK`,
`APP_DECODIUM_PROFILE`, `APP_DECODIUM_RATE`, session id e message count.
`writeAdifLogFile()` scrive pero' un file ADIF atomico osservabile da logger
esterni, predefinito accanto allo store locale come `ft2link_qso_log.adi`. Il
tab `LOG` espone `WRITE` per forzare la scrittura e `PATH` per copiare il path
da configurare in Log4OM/GridTracker o programmi simili.
La stessa area espone anche una outbox logbook FT2-Link: `QSO` mette in coda il
QSO selezionato, `QALL` mette in coda tutti i QSO FT2-Link, `SEND` invia i
record ADIF ai logger esterni gia' configurati in Decodium (`UDP`, `TCP ADIF`,
`N1MM`, `QRZ`, `Cloudlog`) e `CLR` svuota solo la coda. La outbox e'
persistente nello store JSON, deduplica sessione/target/ADIF e mantiene stato
`Queued`, `Submitted`, `Sent`, `Failed` o `Skipped`. `Submitted` indica che il
record e' stato consegnato al bridge; per QRZ Logbook e Cloudlog la riga viene
poi aggiornata in modo asincrono a `Sent` o `Failed` quando il server risponde,
con dettaglio backend per backend. Cloudlog usa lo stesso endpoint
`/index.php/api/qso` del logger principale ma riceve il record ADIF raw prodotto
da FT2-Link.
Il tab `LOG` espone export copiabili per ADIF, chat history, operational log,
outbox logbook, cluster JSON, store JSON e bundle completo. La chat history
mostra le sessioni live quando presenti e, dopo un riavvio, ricade sui QSO
summary persistenti per mantenere tracciabile il nominativo anche senza
transcript completo.
`receivedFiles()` restituisce la galleria dei soli file incoming con nominativo
mittente, data UTC, dimensione, preview e flag immagine; il tab `RXF` la mostra
con scrollbar e copia contenuto. `statistics()` e `statisticsText()` aggregano
QSO, contatti, CQ/beacon, chat live/loggata, ping, path report, broadcast,
mailbox, forms, file, bulletin e alert; il tab `STAT` mostra i contatori e copia
il report testuale. I report SNR espliciti `<R+NN>`/`<R-NN>` vengono salvati come
`PathReport` incoming/outgoing e partecipano a min/avg/max; le metriche live
`W2300` vengono salvate come report di qualita' decoder, separati dagli SNR.
`pathReports()` espone lo storico e `pathAnalysis(call, locator)` calcola
riepilogo, media SNR, best hour UTC, aggregati per call/grid e ultimi report; il
tab `PATH` fornisce filtro `CALL`/`GRID`, riepilogo e lista scrollabile. Banda e
frequenza non sono ancora tracciate nel path analyzer finche' il layer radio non
fornisce dial frequency/band affidabili.

Il primo layer multi-band/cluster e' separato dal path analyzer e dalla rubrica:
`clusterLastHeard()` mantiene record per chiave `node|band|dial|call`, cosi' lo
stesso nominativo puo' apparire su piu' bande o da piu' istanze senza
sovrascriversi. La UI aggiorna `configureCluster()` con la dial CAT corrente
quando disponibile; l'adapter ricava la banda da `FREQ` oppure da una tabella
HF/VHF semplice. Ogni beacon/CQ/osservazione valida aggiorna il cluster locale
con call, locator, nome, profilo preferito, evento, sorgente, node id, banda,
dial, flag CQ, tipo CQ, primo/ultimo ascolto e contatore. I nominativi bloccati
e il nominativo locale non vengono inseriti.

La tab `CLST` mostra il last-heard cluster con scrollbar, node/band/dial
correnti e comandi `EXPORT`, `IMPORT`, `COPY`, `CLR`. `clusterExportJson()`
produce un JSON portabile `ft2link-cluster-last-heard`; `importClusterLastHeard()`
fonde un export incollato senza duplicare lo stesso record se importato piu'
volte. Per lo scambio tra istanze reali, `PUSH` scrive lo stesso JSON in un file
share atomico (`writeClusterShareFile()`), `PULL` legge e fonde quel file
(`mergeClusterShareFile()`), mentre `SYNC` esegue pull/merge e poi riscrive il
file con lo stato locale aggiornato (`syncClusterShareFile()`). Il toggle
`AUTO` nella tab `CLST` ripete `SYNC` a intervalli configurabili, cosi' due o
piu' istanze possono condividere last-heard via cartella locale, SMB, cloud
drive o altro file sync senza introdurre un server FT2-Link. Il path vuoto usa
un default accanto allo store locale; un path esplicito puo' puntare a una
cartella condivisa o sincronizzata tra macOS/Windows/Linux. Questo e' ancora un
cluster file-based: non trasmette su Internet, non modifica la rubrica locale e
lascia all'operatore la scelta del mezzo di sincronizzazione.
`clusterLastHeardText()` entra nel bundle log come `CLUSTER LAST HEARD` e in
`STAT` compare il contatore `CLST`.

La persistenza locale v0.1 salva in JSON atomico lo storico operativo: broadcast,
alert, mailbox, forms, piccoli file, bulletin, QSO log, callsign history, ping
history, path report, cluster last-heard multi-banda e contatori CQ/beacon
TX/RX. Il path predefinito usa
`QStandardPaths::AppDataLocation` con file
`ft2link/state-v1.json`, quindi resta portabile tra macOS, Windows e Linux senza
path hard-coded. Ogni lista operativa mantiene il limite iniziale di 100 record,
mentre path report e cluster last-heard usano limiti piu' ampi per analisi
operative.
`localStoreAudit()` controlla versione, path, dimensione, hash SHA-256 e contatori
persistenti; `backupLocalStore()` scrive una copia JSON atomica nella directory
`backups`; `fixLocalStore(true)` crea backup e riscrive lo store in forma
canonica. Il tab `DB` espone questi comandi come equivalente iniziale del
database fixer.
Lo stato ARQ live, la coda TX, i ping pending e le sessioni aperte non vengono
ripristinati dopo riavvio: sono stati di canale, non log affidabili.

La UI puo' anche preparare un piano TX radio disarmato: `buildWideTxAudioPlan`
genera campioni `float32 mono` a 48 kHz, frame `DATA` e burst schedulati per
`W500` o `W2300`. Questo piano e' intenzionalmente portabile e non dipende da
CoreAudio, WASAPI o ALSA. Con `ARM` + `RF TX`, l'adapter emette i campioni verso
`DecodiumBridge::transmitFt2LinkAudio`, che riusa il percorso TX comune
`startTx()` per device audio, CAT/PTT, TCI, watchdog e cleanup. La richiesta si
auto-disarma dopo l'invio per evitare ripetizioni accidentali.

Con `ARM` attivo, il click su una stazione nella lista avvia
`startSessionRadioHandshake()`: viene trasmesso un `HELLO` `NARROW` a 48 kHz
verso lo stesso percorso RF. Il ricevitore decodifica il controllo narrow prima
di cercare frame wide, crea la sessione dal nominativo contenuto nel payload e
risponde automaticamente con `HELLO_ACK` `NARROW`. Una volta negoziato il profilo,
i messaggi e gli ACK dati passano a `W500` o `W2300`.

Con `ARM` + `CQ TX`, la UI trasmette un `BEACON` `NARROW` one-shot. L'adapter
applica un intervallo minimo di 60 secondi tra beacon locali per evitare CQ
ripetuti accidentalmente.

La UI espone anche `AUTO` per beacon CQ periodici opzionali. L'abilitazione
richiede `ARM`, poi il timer resta attivo con intervalli selezionabili 3/5/10
minuti. Se un beacon locale e' appena stato inviato, l'automatismo non forza una
seconda trasmissione: resta in `AUTO CQ WAIT` e riparte dal prossimo slot valido.

Ogni CQ/beacon TX, beacon RF RX e osservazione stazione viene salvato anche in
uno storico locale strutturato con direzione `TX/RX`, nominativo, locator, nome,
profilo preferito, tipo CQ, locator CQ, slot CQ, sorgente (`RF`, `OBS`, `MANUAL`,
`AUTO`) e timestamp. Il pulsante `HIST` nella colonna stazioni alterna la lista
live con lo storico CQ/beacon; `beaconHistoryText()` esporta lo stesso contenuto
in forma testuale e il bundle log lo include nella sezione `CQ/BEACON HISTORY`.
Lo storico e' persistito nello store locale e limitato agli ultimi 100 eventi.

L'adapter mantiene anche una coda TX stimata per i burst FT2-Link live. Gli ACK
hanno priorita' e non vengono scartati se un burst precedente e' ancora in corso;
i messaggi DATA live vengono reinviati in modo conservativo fino a 3 tentativi se
non arriva un ACK. I retry gia' accodati vengono scartati se nel frattempo arriva
l'ACK della sessione. La coda usa priorita' FIFO per ACK/control e una selezione
fair per DATA/RETRY normali, evitando di servire due volte la stessa sessione se
un'altra sessione e' gia' pronta. Dal secondo tentativo, i retry live `W2300`
rigenerano il piano audio in modalita' `ROBUST`.

Il local store viene salvato automaticamente quando cambiano gli storici
operativi e ancora una volta su chiusura applicazione. Un JSON non valido o con
versione non supportata non sovrascrive lo stato gia' in memoria; l'errore viene
esposto tramite `lastLocalStoreError`.

La main UI mantiene la chat agganciata all'ultimo messaggio finche' l'operatore
non scorre indietro. Se arrivano nuovi messaggi mentre la vista non e' in fondo,
compare `DOWN` in basso a destra per tornare subito al punto live. Le colonne
`STATIONS` e `SESSIONS` hanno maniglie a tre punti trascinabili; le larghezze
vengono salvate nei setting Decodium e restano portabili su macOS, Windows e
Linux.

Il CQ slot workflow usa la griglia relativa `+/-750 Hz` attorno alla calling
frequency. La UI consente di abilitare/disabilitare lo slot selector, scegliere
`S+1`, `S-1` ecc., impostare il wait locale e vedere `FREE/BUSY/WAIT`. Un CQ
slot one-shot invia lo Slot-ID nel frame `BEACON NARROW` usando campi finora
non usati dal beacon (`sequence` per Slot-ID firmato, `ack_base` per slot size);
chi riceve mostra il badge slot nella station list. Non viene ancora comandato
un auto-QSY CAT: l'operatore resta responsabile di frequenza, band plan e canale
libero.

Il CQ workflow ora include anche un tipo speciale e locator CQ. Il pannello
cicla `CQ`, `CHAT`, `NET`, `EMCOMM`, `TEST` e `QSY`; il locator usa il campo
locale se vuoto oppure un valore massimo 8 caratteri inserito dall'operatore.
La station list mostra tipo, locator e slot ricevuti, cosi' un CQ EMCOMM o NET
non appare come un CQ generico.

Il QSY assistant legge gli ultimi tag QSY della chat (`<QSYU>`, `<QSYD>`,
`<Q:...>`, `<QF:...>` e il formato obsoleto `<QSYF>...</QSYF>`), calcola offset
e target dial se la frequenza CAT e' nota, verifica il target contro i range
dial consentiti e propone le risposte operative `<QSYR>`, `<QSYJ>` e `<QJO>`.
La UI permette anche di salvare la dial corrente come calling frequency locale e
inserire un tag `<QF:...>` per invitare il ritorno alla CF a fine QSO. Anche qui
non viene eseguito auto-QSY: e' una guida operativa conservativa.

La tab `FREQ` mantiene preset calling frequency, finestre schedule UTC e range
QSY consentiti. I default preset/range ricalcano la lista VarAC del manuale;
l'operatore puo' modificarli con righe del tipo `14105000|20m|Main` e
`14101250-14108750|20m`. La schedule usa righe:

```text
HHMM-HHMM|ACTION|DIAL_HZ|LABEL|CQTYPE
0000-2359|CALLING|14105000|20m main|CQ
1300-1359|DATA|14105750|20m data|CQ
```

`ACTION` puo' essere `CALLING`, `CQ`, `BEACON`, `EMCOMM`, `QUIET` o `DATA`.
Le impostazioni sono salvate nello store locale FT2-Link e usate da
`qsyPlanForText()`, dalla risposta `<FSR>` e dalla calling-frequency guard.

La UI applica anche una prima guardia calling-frequency: se la dial CAT corrente
e' entro 250 Hz dalla calling frequency locale salvata o dalla finestra schedule
attiva, le azioni wide data manuali (`RF TX`, `MAIL`, `RELAY`, `FORM`, `FILE`,
`BBS`) vengono rifiutate con messaggio `CF GUARD`. I controlli `NARROW` (`CQ`,
`BEACON`, `BCAST`, `PATH`, `PING`) restano consentiti perche' servono a discovery
e coordinamento. Una finestra `DATA` marca invece una frequenza come ammessa per
traffico wide in quell'orario.

La chiusura QSO distingue `DISC` e `ABORT`: `DISC` espande e trasmette
`73 <CALL> DE <MYCALL> <DISC>` prima di chiudere la sessione locale, mentre
`ABORT` chiude subito senza messaggio di cortesia. La ricezione di `<DISC>`
continua a chiudere automaticamente la sessione dopo l'ACK.

## Frame binario comune

Il frame binario comune e' indipendente dalla waveform. Ogni adattatore fisico
puo' aggiungere FEC, interleaving e mapping simboli, ma il payload logico resta
questo:

| Campo | Byte | Note |
| --- | ---: | --- |
| magic | 2 | `D4` |
| version | 1 | `1` per questa bozza |
| type | 1 | `BEACON`, `HELLO`, `HELLO_ACK`, `DATA`, `ACK`, `END`, `REJECT`, `BROADCAST`, `PING`, `PING_ACK` |
| profile | 1 | `NARROW`, `W500`, `W2300` |
| flags | 1 | bit 0 = ultimo frame del messaggio |
| session_id | 2 | identificatore sessione |
| sequence | 2 | numero frame dati |
| ack_base | 2 | prima sequenza rappresentata dalla bitmap ACK |
| ack_bitmap | 2 | bit 0 conferma `ack_base`, bit 15 conferma `ack_base + 15` |
| payload_len | 1 | byte payload |
| payload | N | dati applicativi |
| crc16 | 2 | CRC-16/CCITT del frame senza CRC |

Le dimensioni payload iniziali sono conservative:

- `NARROW`: 32 byte logici;
- `W500`: 48 byte logici;
- `W2300`: 224 byte logici.

Questi valori sono limiti di protocollo iniziali, non promesse di throughput RF.
La waveform potra' ridurli se il livello FEC richiede piu' ridondanza.

`BROADCAST` v0.1 usa `NARROW`, `session_id = 0`, `FlagEndOfMessage` e payload
UTF-8 breve. Non genera `ACK` e non partecipa allo stato sessione.

`PING` e `PING_ACK` v0.1 usano `NARROW`, `session_id = 0`,
`FlagEndOfMessage`, `sequence` come token di correlazione e payload UTF-8 con il
nominativo della stazione che trasmette il frame. Non generano messaggi chat e
non aprono sessioni.

## Pacchetto fisico wide

`W500` e `W2300` usano un pacchetto fisico binario comune sopra il frame logico.
Il pacchetto fisico iniziale e' ancora offline/test-only: serve a stabilizzare
byte ordering, scrambling, FEC/interleaving e controlli di integrita' prima del
DSP reale.

Struttura plain prima del coding:

| Campo | Byte | Note |
| --- | ---: | --- |
| magic | 4 | `F2LP` |
| physical_version | 1 | `1` |
| profile | 1 | `W500` o `W2300` |
| frame_len | 2 | lunghezza del frame logico serializzato |
| frame | N | frame FT2-Link completo di CRC logico |
| physical_crc16 | 2 | CRC-16/CCITT sul pacchetto fisico senza CRC |

Il pacchetto viene poi:

1. whitened con LFSR deterministico dipendente dal profilo;
2. convertito in bit;
3. protetto con ripetizione semplice;
4. interlacciato con stride coprimo alla lunghezza del blocco;
5. repackato in byte per il futuro mapper simboli.

Parametri iniziali:

| Profilo | Banda nominale | Symbol rate | Bits/symbol | FEC v0.1 |
| --- | ---: | ---: | ---: | --- |
| `W500` | 500 Hz | 125 | 2 | ripetizione 3x + majority decode |
| `W2300 FAST` | 2300 Hz | 600 | 8 | nessuna ripetizione, CRC/logical retry |
| `W2300 ROBUST` | 2300 Hz | 600 | 8 | ripetizione simboli 3x + majority decode |

Questi parametri sono intenzionalmente provvisori. `W500` privilegia robustezza,
`W2300` privilegia throughput e si affida maggiormente ad ARQ/CRC.

## Waveform W500 v0.1

Il primo waveform wide implementato offline e' `W500`. Non e' ancora collegato
alla TX/RX reale dell'applicazione; serve a verificare mapping simboli e audio
prima di integrare il modem nella pipeline radio.

Parametri iniziali:

- sample rate: 12000 Hz;
- centro audio nominale: 1500 Hz;
- 4-FSK ortogonale;
- symbol rate: 125 baud;
- spacing toni: 125 Hz;
- toni nominali: 1312.5, 1437.5, 1562.5, 1687.5 Hz;
- mapping dibit Gray: `00 -> tone0`, `01 -> tone1`, `11 -> tone2`, `10 -> tone3`;
- envelope globale con mezzo simbolo di ramp in/out.

Burst `W500`:

| Campo | Simboli | Note |
| --- | ---: | --- |
| preambolo | 16 | alternanza tone0/tone3 |
| sync | 8 | sequenza fissa `0,1,3,2,3,1,0,2` |
| length | 8 | lunghezza pacchetto fisico, 16 bit |
| packet | `N * 4` | pacchetto fisico, 4 simboli per byte |

Il demodulatore offline usa correlazione non coerente sui quattro toni per ogni
simbolo. Il decoder prova tutte le possibili fasi di simbolo dentro uno stream
audio, cerca `preamble+sync`, legge `length` e ritaglia il burst. Questo copre
silenzio prima/dopo il burst e offset iniziale non allineato al simbolo.

Metriche esposte dal decoder `W500`:

- `sampleOffset`: primo campione del burst dentro lo stream audio;
- `symbolOffset`: offset simbolico rispetto alla fase provata;
- `symbolCount`: simboli totali del burst ritagliato;
- `packetBytes`: byte del pacchetto fisico decodificato;
- `quality`: separazione media tra tono migliore e secondo tono;
- `estimatedFrequencyOffsetHz`: stima coarse dello scarto dal centro atteso;
- `estimatedCenterFrequencyHz`: centro audio stimato.

La stima di frequenza e' volutamente semplice: prova finestre coarse intorno al
centro nominale e rivaluta il burst gia' decodificato. Serve per diagnostica e
per il futuro AFC, non e' ancora un loop di tracking continuo.

Restano fuori da questa fase:

- AFC continuo;
- tracking di clock durante burst lunghi;
- decode continuo dal waterfall;
- stima SNR calibrata per decidere retry e backoff.

## Waveform NARROW v0.1

`NARROW` e' il profilo di controllo RF per `BEACON`, `HELLO` e `HELLO_ACK`. Non
e' pensato per chat ad alto throughput: porta frame piccoli, robusti e
identificabili prima di passare ai profili wide.

Parametri iniziali:

- sample rate modem: 12000 Hz in RX, 48000 Hz sul piano TX verso device radio;
- centro audio nominale: 1500 Hz;
- BFSK a 2 toni;
- symbol rate: 125 baud;
- spacing toni: 125 Hz;
- ripetizione bit: 3x con majority decode;
- envelope globale con mezzo simbolo di ramp in/out.

Il burst narrow contiene:

| Campo | Byte | Note |
| --- | ---: | --- |
| preambolo | 6 | sequenza `55 55 55 55 33 CC` |
| sync | 3 | sequenza `D4 7A B2` |
| packet_magic | 2 | `N2` |
| packet_version | 1 | `1` |
| frame_len | 1 | lunghezza frame FT2-Link serializzato |
| frame | N | frame comune completo di CRC logico |
| packet_crc16 | 2 | CRC-16/CCITT sul pacchetto narrow senza CRC |

Il decoder narrow e' sempre attivo sul flusso RX live, anche quando non esiste
ancora una sessione connessa. Un `BEACON` aggiorna la dashboard CQ; un `HELLO`
apre la sessione on-air: `HELLO` ricevuto -> creazione sessione -> `HELLO_ACK`
automatico -> passaggio a `W500`/`W2300`.

## Waveform W2300 v0.1

`W2300` e' il profilo wide veloce. Non e' un FT2 allargato: usa quattro
sottoportanti parallele e modulazione differenziale DQPSK per sfruttare meglio
la larghezza di banda disponibile.

Parametri iniziali:

- sample rate: 12000 Hz;
- centro audio nominale: 1500 Hz;
- 4 sottoportanti;
- spacing sottoportanti: 600 Hz;
- sottoportanti nominali: 600, 1200, 1800, 2400 Hz;
- symbol rate: 600 baud;
- DQPSK differenziale per sottoportante;
- 2 bit per sottoportante, 8 bit per simbolo complessivo;
- `FAST`: 4800 bit/s raw prima di overhead, CRC, ARQ e pause TX/RX;
- `ROBUST`: 4800 bit/s raw, 1600 bit/s sui simboli payload dopo ripetizione 3x;
- envelope globale con mezzo simbolo di ramp in/out.

Il burst porta esplicitamente il rate mode. Il ricevitore non deve sapere prima
se il trasmettitore sta usando `FAST` o `ROBUST`: decodifica il modo dopo sync e
applica il relativo fattore di ripetizione.

Burst `W2300`:

| Campo | Simboli | Note |
| --- | ---: | --- |
| reference | 1 | simbolo non informativo per fase differenziale |
| preambolo | 16 | byte-symbol pattern ad alta transizione |
| sync | 8 | sequenza byte fissa |
| mode | 3 | simbolo modo ripetuto 3x, `FAST` o `ROBUST` |
| length | `2 * R` | lunghezza pacchetto fisico, 16 bit, ripetuta `R` volte |
| packet | `N * R` | pacchetto fisico, 1 byte per simbolo prima della ripetizione |

`R` e' il repetition factor del modo:

| Modo | `R` | Bitrate payload-symbol |
| --- | ---: | ---: |
| `FAST` | 1 | 4800 bit/s |
| `ROBUST` | 3 | 1600 bit/s |

Il decoder offline correla I/Q ogni sottoportante, ricava la rotazione
differenziale tra simboli consecutivi e ricostruisce un byte per simbolo. Come
per `W500`, prova tutte le fasi di simbolo dentro uno stream audio e cerca
`preamble+sync`. In `ROBUST`, ogni byte ripetuto viene recuperato a majority
decode bit-per-bit.

Metriche esposte dal decoder `W2300`:

- `sampleOffset`: primo campione del burst, incluso il reference symbol;
- `symbolOffset`: offset simbolico rispetto alla fase provata;
- `symbolCount`: simboli totali del burst, incluso il reference symbol;
- `packetBytes`: byte del pacchetto fisico decodificato;
- `quality`: confidenza media delle decisioni DQPSK;
- `estimatedFrequencyOffsetHz`: stima dello scarto dal centro atteso;
- `estimatedCenterFrequencyHz`: centro audio stimato;
- `rateMode`: modo rilevato nel burst;
- `repetitionFactor`: fattore `R`;
- `rawBitRate`: bitrate lordo DQPSK;
- `payloadBitRate`: bitrate payload-symbol dopo ripetizione.

La stima frequenza `W2300` non usa solo una ricerca per qualita': misura il
residuo medio di fase rispetto ai simboli gia' decodificati. Questo e' piu'
adatto a DQPSK, dove il differenziale tollera bene piccoli offset ma nasconde
parte dell'informazione a una semplice metrica di potenza.

Regola adattiva iniziale:

- se ci sono retry, qualita' bassa o offset frequenza elevato, usare `ROBUST`;
- se qualita' e offset tornano buoni, usare `FAST`;
- l'ARQ resta comunque l'autorita' finale: CRC/ACK decidono se il frame e'
  realmente passato.

Il controller TX `W2300` usa due input:

- metriche RX recenti, per aggiornare il modo corrente dei nuovi frame;
- tentativi ARQ per sequenza, per forzare `ROBUST` sulle ritrasmissioni.

Questa separazione evita di mettere il rate mode nel frame logico: lo stesso
frame `DATA` puo' essere ritrasmesso con una waveform piu' robusta senza
cambiare sequenza, payload o semantica ACK.

Direzioni ulteriori per spremere meglio il canale:

- FEC piu' efficiente della ripetizione brutale;
- interleaver piu' profondo su burst lunghi;
- modalita' futura `TURBO`;
- equalizzazione per portante e stima SNR per decidere rate e retry;
- futuro profilo coerente QPSK/8PSK o 16-QAM solo quando AFC e timing saranno
  solidi.

## ACK e ritrasmissioni

Il ricevitore mantiene l'insieme delle sequenze ricevute. Invia `ACK` con:

- `ack_base` = prima sequenza non ancora confermata cumulativamente;
- `ack_bitmap` = bitmap delle 16 sequenze da `ack_base` a `ack_base + 15`.

Il trasmettitore considera confermate tutte le sequenze `< ack_base` e quelle
presenti nella bitmap.

Parametri iniziali:

- finestra TX: 4 frame;
- retry: 8000 ms;
- massimo tentativi: 6;
- payload zero consentito solo per frame di controllo o messaggio vuoto.

## Pipeline offline W2300

Il modulo offline collega i pezzi gia' implementati:

1. `OutboundTransfer` produce frame `DATA`;
2. `W2300RateController` sceglie `FAST` o `ROBUST`;
3. il frame viene trasformato in waveform `W2300`;
4. il decoder RX ricostruisce il frame e produce metriche;
5. `InboundTransfer` accetta il frame e genera `ACK`;
6. il lato TX applica ACK, retry e cambio rate.

Il test offline copre sia lo scambio pulito sia la perdita forzata di un frame:
il primo invio parte in `FAST`, la ritrasmissione della stessa sequenza passa a
`ROBUST`, e il messaggio finale viene ricostruito lato RX.

Non e' ancora radio reale: manca audio device, waterfall continuo, PTT,
listen-before-transmit reale e schedulazione con tempi di canale. Pero' e' gia'
un end-to-end deterministico utile per non rompere il contratto tra protocollo,
waveform e ARQ mentre integriamo l'app.

## Adapter audio W2300

Gli adapter audio aggiungono un modello piu' vicino alla pipeline radio:

- `W2300TxAudioBuffer` schedula burst audio in un buffer TX;
- `W500TxAudioBuffer` fa lo stesso per il profilo 500 Hz;
- ogni burst ha guard interval prima/dopo e gap tra burst;
- `W2300RxAudioBuffer` e `W500RxAudioBuffer` ricevono campioni a chunk e tentano
  il decode quando il buffer contiene abbastanza audio;
- dopo un decode valido, il buffer RX consuma il burst e conserva eventuale
  audio successivo;
- prima dei `DATA`, puo' eseguire `HELLO -> HELLO_ACK` e inizializzare il
  controller TX con il `W2300` rate mode negoziato;
- `runWideAudioPipeline` sceglie il ramo `W2300` o `W500` in base al profilo
  negoziato;
- opzionalmente, l'ACK viene trasmesso come burst audio di ritorno invece di
  essere applicato in modo logico immediato;
- il modello half-duplex aggiunge `dataToAckTurnaroundMs` e
  `ackToDataTurnaroundMs` per rappresentare cambio RX/TX e ritorno alla
  trasmissione dati;
- i test possono perdere il primo ACK di una sequenza con
  `dropFirstAckForSequences`, separatamente dalla perdita DATA;
- la pipeline applica listen-before-transmit simulato con finestre busy
  esterne, RX busy, TX busy locale e backoff;
- la pipeline audio usa gli stessi `OutboundTransfer`, `InboundTransfer`,
  `W2300RateController`, waveform e ACK della pipeline offline.
- `buildWideTxAudioPlan` costruisce un buffer TX-only a 48 kHz con frame
  `DATA`, burst schedulati, metriche throughput attivo e campioni pronti per il
  futuro bridge radio; non aziona PTT e non assume un backend audio specifico.
- `FT2LinkQmlAdapter::radioTxAudioRequested` passa la wave preparata al bridge;
  `DecodiumBridge::transmitFt2LinkAudio` arma una TX custom `FT2LINK`, salta le
  logiche QSO FT standard e mantiene PTT/device/TCI sul percorso comune.
- `DecodiumBridge::ft2LinkRxSamplesReady` inoltra blocchi PCM RX a 12 kHz
  all'adapter; `FT2LinkQmlAdapter::ingestRxSamples` alimenta il decoder narrow
  per `HELLO`/`HELLO_ACK` anche senza sessione e i decoder live `W500`/`W2300`
  per `DATA`/`ACK` dopo la negoziazione.
- L'adapter converte i frame decodificati in eventi ARQ, genera `HELLO_ACK` e
  ACK dati automatici con la stessa richiesta `radioTxAudioRequested`, e accoda
  le risposte se la durata stimata del burst precedente non e' ancora scaduta.

Il test audio copre handshake prima dei dati, fallback negoziato su `W500`,
ACK audio half-duplex, stream chunked, rumore deterministico leggero, gap tra
burst, perdita forzata di una sequenza DATA, perdita forzata di un ACK, retry in
`ROBUST`, rinvio TX quando il canale e' occupato e piano TX-only decodificabile
a 48 kHz su `W500` e `W2300`. Il test QML adapter copre anche ACK remoto che
marca un messaggio `Delivered`, DATA remoto che genera ACK radio, decode da
campioni W2300 live e round-trip audio `HELLO`/`HELLO_ACK` narrow.

La perdita ACK e' deliberatamente distinta dalla perdita DATA. Se il DATA arriva
ma l'ACK di ritorno non viene decodificato dal mittente, il mittente ritrasmette
la stessa sequenza. `InboundTransfer` accetta il duplicato senza corrompere il
messaggio e genera un nuovo ACK cumulativo.

Con finestra ARQ maggiore di 1, gli ACK usano davvero `ack_base` e
`ack_bitmap`: se manca la sequenza 1 ma le sequenze 2 e 3 sono gia' arrivate,
il ricevitore puo' trasmettere `ack_base = 1` con i bit relativi a 2 e 3
impostati. Il mittente mantiene aperta solo la sequenza mancante e non
ritrasmette frame gia' confermati dalla bitmap. I test coprono questa condizione
sia su `W2300` sia su `W500`, insieme a una perdita ACK sul frame finale.

Il backoff non consuma tentativi ARQ: il frame viene richiesto a
`OutboundTransfer` solo quando il canale risulta libero. Questo evita di
trasformare un canale occupato in un falso retry.

## Metriche throughput audio

Le pipeline audio producono `AudioThroughputMetrics` per misurare il rendimento
reale del profilo usato:

- `payloadBytes`: byte applicativi consegnati;
- `burstCount`: burst DATA trasmessi, inclusi retry e drop simulati;
- `ackBurstCount`: burst ACK audio trasmessi nel modello half-duplex;
- `decodedBurstCount`: burst decodificati dal ricevitore;
- `decodedAckBurstCount`: ACK audio decodificati dal lato mittente DATA;
- `droppedBurstCount`: burst persi per test di canale;
- `droppedAckBurstCount`: ACK audio trasmessi ma non decodificati dal mittente;
- `retryBurstCount`: burst ARQ trasmessi con tentativo maggiore di 1;
- `sessionDurationMs`: durata della sessione TX modellata, inclusi backoff,
  silenzi, gap schedulati, ACK audio e turnaround;
- `dataTransmitMs`: airtime dei burst DATA;
- `ackTransmitMs`: airtime dei burst ACK;
- `activeTransmitMs`: somma dell'airtime DATA e ACK;
- `effectivePayloadBytesPerSecond`: throughput applicativo sulla durata totale
  di sessione;
- `activePayloadBytesPerSecond`: throughput applicativo considerando solo
  airtime TX attivo;
- `channelUtilization`: rapporto tra airtime attivo e durata sessione.

Nel percorso radio live, l'adapter pubblica anche metriche RX `W2300` per
sessione: modo decodificato, qualita' decisionale, offset di frequenza stimato,
bitrate lordo/applicativo e prossimo modo consigliato. Queste metriche alimentano
un rate controller per sessione: il piano TX successivo usa `FAST` quando la
ricezione e' pulita e passa a `ROBUST` quando qualita' o offset indicano margine
ridotto.

Queste metriche sono intenzionalmente applicative: misurano quanti byte utili
arrivano a destinazione dopo frammentazione, overhead fisico, FEC semplice,
ritrasmissioni, ACK audio e backoff. Non sono il bitrate simbolico lordo. Il
tempo RF dell'ACK e il turnaround RX/TX sono modellati quando `modelAckAudio` e'
attivo. Il percorso app ora include anche audio device, CAT/PTT reale, decoder
RX live narrow/wide, handshake RF `HELLO/HELLO_ACK` e retry RF automatico per i
messaggi live, beacon CQ one-shot on-air e beacon CQ automatico opzionale con
guard interval. Il retry live `W2300` passa a `ROBUST` dal secondo tentativo.
Il rate-control live usa le metriche RX decodificate per scegliere il prossimo
piano `W2300`. Il listen-before-transmit live usa energia RMS/peak sui campioni
RX per applicare un breve hold-off software prima di svuotare la coda RF.

## Portabilita' build

Il core protocollo/audio FT2-Link e' isolato nel target statico `ft2link_core`.
Il target e' C++17 puro, non dipende da Qt/QML e disattiva AutoMOC/AutoUIC/AutoRCC.
Questo permette ai test protocollo di linkare direttamente il core, evitando di
trascinare l'intera app quando si validano frame, ARQ, waveform e pipeline audio.

La configurazione CMake evita inoltre di applicare flag GCC/Clang a MSVC:
`-fdata-sections`, `-ffunction-sections`, `-pthread`, `-Wl,--gc-sections` e le
opzioni stack/heap MinGW vengono abilitate solo sui toolchain compatibili. Questo
mantiene lo stesso codice sorgente adatto a macOS legacy, Linux, MinGW e build
Windows native, fermo restando che il packaging legacy macOS richiede librerie
Qt/Boost/FFTW/Hamlib compilate con un deployment target coerente.

Il workflow manuale `.github/workflows/test-runner.yml` esegue smoke test
`ft2link_core`, `test_ft2link` e `test_ft2link_qml_adapter` su macOS, Linux e
Windows/MinGW. Non sostituisce le pipeline release complete, ma intercetta
rapidamente regressioni del protocollo, dell'ARQ e dell'adapter Qt.

## Regole RF

FT2-Link deve essere conservativo:

- beacon con intervallo minimo configurabile e default lungo;
- listen-before-transmit live con hold-off se l'audio RX indica canale occupato;
- niente retry infinito;
- retry `W2300` conservativo in `ROBUST` dopo il primo tentativo;
- stop automatico se il canale e' occupato o se manca ACK dopo il limite;
- `W2300` solo su frequenze e segmenti compatibili con emissioni wide data.

## Roadmap implementativa

1. Specifica e modulo C++ puro per frame/ARQ.
2. Test offline con perdita, duplicati e ritrasmissioni.
3. Adattatore `NARROW` per discovery su FT2 esistente.
4. Prototipo waveform `W500`.
5. Scanner burst e metriche `W500` su stream audio.
6. Prototipo waveform `W2300` multicarrier DQPSK.
7. Scanner burst e metriche `W2300` su stream audio.
8. Rate adaptation `W2300` con `FAST`/`ROBUST`.
9. Pipeline offline `W2300` end-to-end con ACK/retry.
10. Adapter audio `W2300` con buffer TX/RX e chunking.
11. Listen-before-transmit e backoff nel modello audio.
12. Handshake `HELLO/HELLO_ACK` e negoziazione `W500/W2300`.
13. Handshake come gate della pipeline audio `W2300`.
14. Pipeline audio wide con fallback negoziato `W500`.
15. Metriche throughput effettivo per `W500` e `W2300`.
16. ACK audio half-duplex con turnaround RX/TX.
17. Perdita ACK separata da perdita DATA e retry su duplicati.
18. Finestra ARQ > 1 con ACK bitmap e perdite miste DATA/ACK.
19. Catalogo gesture/tag chat stile VarAC nel composer FT2-Link.
20. Parser RX per gesture/tag con eventi `System`, QSY manuale e `DISC`.
21. Helper QSY/slot 750 Hz nel composer con tag `<QSYU>`, `<QSYD>` e `<Q:...>`.
22. Export ADIF per QSO FT2-Link e pulsante clipboard `ADIF`.
23. Profilo operatore `INFO` per tag name/QTH/email/rig/antenna/power/ICE/GPS.
24. Parking mailbox locale con stato `Relay ready` quando il destinatario viene
    sentito via stazione/beacon.
25. Inoltro `RELAY` delle mailbox parcheggiate dentro una sessione wide, con
    stati `Pending relay`, `Delivered` e `Forwarded`.
25a. Envelope relay RF strutturato `FT2RLY1` con `TO/VIA/FROM/FLAGS/HOPS`,
     coda relay esportabile `RLYQ`, persistenza di protocollo/via/hop e parsing
     RX separato dagli envelope mailbox legacy.
26. Path Finder broadcast `P?`/`P!` basato su callsign history locale entro 24h.
19. Modello applicativo per CQ dashboard, sessioni e chat log.
20. Adapter Qt/QML per stazioni, handshake, sessioni e chat log.
21. Prima UI FT2-Link con CQ, sessioni, chat e profilo wide.
22. Collegamento UI -> pipeline audio locale con stato ARQ e metriche live.
23. Accesso come modo di emissione `FT2-Link`, UI inline centrale e piano TX
    radio disarmato a 48 kHz.
24. Collegamento piano TX -> bridge audio/radio comune con CAT/PTT reale.
25. RX radio live wide, ACK reale e stato messaggio `Delivered` da ACK remoto.
26. Handshake audio narrow on-air `HELLO/HELLO_ACK` senza loopback UI.
27. Scheduler RF automatico per retry DATA e coda ACK prioritaria.
28. Discovery/beacon CQ one-shot on-air.
29. Beacon automatici opzionali con intervallo configurabile.
30. Scheduling fair tra piu' sessioni concorrenti e retry `W2300` robust.
31. Rate control live basato su metriche RX `W2300`.
32. Listen-before-transmit live basato su misura energia audio RX.
33. Target `ft2link_core` C++17 puro e CMake compatibile con toolchain macOS,
    Linux, MinGW/MSVC.
34. Workflow manuale smoke-test FT2-Link su macOS, Linux e Windows/MinGW.
35. Tag Inquire manuali (`LHC`, `FSR`, `VRP`, `LCR`, `GPSR`) con risposte
    suggerite da stato locale.
36. Tag BBS file (`BLR`, `BL`, `BG`, `BLJ`, `BGJ`) con lista dalla cache file e
    invio manuale via `FILE TX`.
37. Tag AI/gateway (`AI:`, `DISAI`, `A`, `EA`, `ACIE`, `AIJ`, `AIE`, `AIL`)
    registrati come eventi passivi senza gateway automatico.
38. Gesture/suoni, file/VMail legacy e HamPlay riconosciuti come eventi passivi,
    mantenendo `FT2M1`/`FT2FILE1` come formati nativi.
39. Callsign tag locali persistenti con badge in station list e callsign
    history, senza trasmissione RF automatica.
40. Editor `CALL` per dettagli e commenti callsign history, persistente nello
    store FT2-Link locale.
41. Timeline `CALL` per QSO, chat live, broadcast/alert, mail, forms, file,
    bulletin e ping filtrati per nominativo.
42. Dashboard `STAT` e galleria `RXF` per statistiche operative e file ricevuti,
    con export testuale e contatori CQ/beacon persistenti.
43. Path Analyzer locale con report SNR `<R...>`, qualita' live `W2300`, filtro
    call/grid, best hour UTC e persistenza nello store FT2-Link.
44. Tab `LOG` e `DB` per export ADIF/chat/operational/store, audit store, backup
    atomico e rewrite/fix canonico dello store locale.
45. Usabilita' main screen: indicatore `DOWN` per chat scrollback e colonne
    `STATIONS`/`SESSIONS` ridimensionabili con persistenza cross-platform.
46. CQ slot workflow iniziale: selector `S+/-N`, wait locale, indicatore
    `FREE/BUSY/WAIT`, Slot-ID nel beacon CQ e badge slot in station list.
47. QSY assistant: parsing inviti QSY, target dial calcolato, risposte
    `QSYR/QSYJ/QJO` e tag ritorno calling frequency senza auto-CAT.
48. QSO ending UI: `DISC` con 73 e tag `<DISC>`, piu' `ABORT` locale immediato.
49. Writer ADIF atomico `ft2link_qso_log.adi` per logger esterni, con pulsanti
    `WRITE`/`PATH` nel tab `LOG`.
50. Tab `PRE` per preset/canned custom persistenti e generatore rapido
    VarAC Wednesday/check-in verso mail o chat.
51. Alert tag custom persistenti con editor nella tab `BCAST`, usati dalla
    scansione locale di broadcast/chat per creare eventi alert.
52. Tab `FREQ` con calling-frequency preset, allowed QSY ranges persistenti,
    schedule UTC opzionale, validazione QSY locale e risposta `<FS:...>` per
    richieste `<FSR>`.
53. Stato presenza `AWAY`/`AWQ` e welcome message persistenti, accodati alla
    sessione entrante senza TX RF automatico.
54. Auto-reply queue opzionale per richieste Inquire/BBS sicure, sempre
    `Outgoing/Pending` e senza TX RF automatico.
55. Block list callsign locale persistente con filtro CQ/last-heard,
    ignoramento beacon/broadcast/ping e rifiuto `HELLO` da nominativi bloccati.
56. Auto-away locale persistente per abilita/intervallo, con stato automatico
    runtime che non viene salvato come `AWAY` manuale.
57. Timer QSO locali per Call ID periodico pending e auto-disconnect inattivo,
    senza RF automatico e senza tenere viva la sessione con Call ID automatici.
58. Toggle `PING RX` persistente per accettare o rifiutare ping in ingresso
    senza generare `PING_ACK`.
59. Toggle persistenti `LH PEEK` e `SNR TX` per rifiutare richieste last-heard
    con `<LHJ>` e sopprimere suggerimenti locali di report SNR.
60. Toggle persistente `INFO REQ` per sopprimere risposte Inquire con profilo,
    locator e GPS locali.
61. Toggle persistente `LC PEEK` per rifiutare richieste last-connections con
    `<LCJ>`.
62. Toggle persistente `VM PEEK` per rifiutare peeking sulle VMail parcheggiate
    con `<VRPJ>`.
63. Toggle persistente `VSNR OK` per accettare inviti verbose SNR `<VSI>`
    accodando `<VSIR>` senza TX RF automatico.
64. Toggle persistente `VM PARK` per rifiutare envelope VMail relay destinati
    a terzi senza salvarli nello store locale.
65. Is-typing chat con tag pubblici `<TYP>`/`<TYP0>`, scadenza locale e
    indicatore compatto nella testata chat.
66. CQ speciale con tipo `CQ/CHAT/NET/EMCOMM/TEST/QSY`, locator CQ, campo
    `ack_bitmap` documentato e badge tipo/locator nella station list.
67. VMail `FT2M2` con flag Urgent/EmComm, contatore unread sempre visibile,
    export testuale stampabile e persistenza dei flag mailbox/relay.
68. Hint relay Path Finder strutturati da `P!`, badge `MAIL N`/`RLY>TARGET`
    nella station list, riga `relayWorkflow` con priorita' e freschezza hint, e
    azioni `MAIL`/`CALL`/`FWD` per il workflow trova relay -> parcheggia ->
    inoltra.
69. Tool stack FT2-Link ad altezza responsive e tab `INFO` scrollabile per
    evitare controlli tagliati nei layout bassi.
70. Storico CQ/beacon persistente con eventi `TX/RX`, tipo CQ, locator, slot,
    sorgente, export testuale e modalita' `HIST` nella station list.
71. Prima guardia calling-frequency: blocco UI per wide data manuale su CF e
    controlli `NARROW` ancora permessi per discovery/coordinamento.
72. Cluster last-heard multi-banda locale con chiave `node|band|dial|call`,
    export/import JSON cross-platform, file share `PUSH`/`PULL`/`SYNC`, auto
    sync periodico opzionale, tab `CLST`, contatore `STAT`, persistenza nello
    store e merge senza duplicati per scambio manuale o cartella condivisa tra
    istanze.
73. Outbox logbook FT2-Link persistente con deduplica ADIF, stato invio,
    export testuale e submit verso backend Decodium gia' configurati:
    UDP raw ADIF, ADIF TCP, N1MM/EasyLog, QRZ Logbook e Cloudlog raw ADIF.
74. Email gateway locale per VMail: generazione `mailto:` e `.eml` RFC 5322,
    invio SMTP diretto `STARTTLS/TLS/NONE` con `AUTH LOGIN`, password salvata
    solo nel secure store della piattaforma, stato asincrono
    `Queued/Connected/TLS/Sending/Sent/Failed/Ready`, test connessione/auth
    senza `DATA`, persistenza dello stato gateway nella mailbox e azioni UI
    `EMAIL`/`EML`/`SMTP`/`TEST`.
75. Relay/digipeater RF piu' strutturato: `FT2RLY1` durante forwarding,
    contatore `rly` in header, export `relayQueueText`, salvataggio nello store
    test di parsing, forwarding, queue e restore, limite hard a 9 hop con
    consegna finale permessa ma ulteriore inoltro bloccato.
76. Scheduler frequency/beacon piu' VarAC-like: finestre UTC
    `HHMM-HHMM|ACTION|FREQ|LABEL|CQTYPE`, tab `FREQ` aggiornata, `<FSR>` basato
    sulla finestra attiva e guard CF che distingue `CALLING/CQ/BEACON/EMCOMM`
    da `DATA`.
77. Feedback asincrono outbox logbook: `SEND` passa un upload id al bridge,
    QRZ Logbook e Cloudlog rientrano con conferma/rifiuto per quella riga e la
    UI aggiorna `Submitted` in `Sent` o `Failed` senza perdere gli invii
    immediati UDP/TCP/N1MM.
78. Status strip globale per chiarezza operativa: `RF`, `QUEUE` e `ERR`
    restano visibili in tutti i tab e riassumono stato radio, relay queue,
    unread, outbox logbook pendente/fallita e ultimo errore.
79. Preset privacy `OPEN`/`CONTROL`/`QUIET` sopra ai toggle avanzati INFO:
    riducono il rumore UI, aggiornano i flag persistenti e mostrano summary
    `privacyPreset`/`privacySummary` in `qsoAutomation`.
