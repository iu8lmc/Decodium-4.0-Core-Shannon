# CAT condivisa — contratto del protocollo

Decodium possiede la porta seriale della radio; nessun altro programma può
aprirla. Per condividerla, Decodium espone su TCP il protocollo **rigctld** di
Hamlib: è quello che ogni programma della comunità sa già parlare sotto il nome
di *Hamlib NET rigctl*, quindi la compatibilità non richiede che nessuno
adatti nulla.

`rigctld` non è distribuito né da MSYS2 né da Decodium — di Hamlib arriva solo
la libreria — perciò il server va implementato dentro Decodium.

## Formato verificato sul campo

Non dedotto dalla documentazione: stabilito facendo collegare il vero client
Hamlib (modello 2) a un prototipo, finché ogni operazione non è tornata `0`.
Prototipo: `tools/cat-share/catshare_proto.py`; client: `netclient.c`.

Sequenza di apertura, nell'ordine in cui il client la esegue:

| Richiesta | Risposta attesa |
|---|---|
| `\get_powerstat` | `1` |
| `\chk_vfo` | `0` |
| `\dump_state` | il blocco qui sotto |

⚠️ A `\chk_vfo` **si risponde `0`, non `CHKVFO 0`.** Con la forma lunga Hamlib
rifiuta l'apertura con *«unknown value returned from netrigctl_transaction=9»*
— e nove è proprio la lunghezza di `"CHKVFO 0\n"`. Un carattere di troppo e
non si collega nessuno.

### Risposta a `\dump_state`

```
1                                                    versione del protocollo
1                                                    modello del rig
2                                                    regione ITU
30000.000000 56000000.000000 0x2ffffff -1 -1 0x3 0x3 gamma in ricezione
0 0 0 0 0 0 0                                        terminatore
1800000.000000 54000000.000000 0x2ffffff 5000 100000 0x3 0x3   gamma in TX
0 0 0 0 0 0 0                                        terminatore
0x2ffffff 1                                          passi di sintonia
0 0                                                  terminatore
0x82 500                                             filtri
0x221 3000
0 0                                                  terminatore
0                                                    max_rit
0                                                    max_xit
0                                                    max_ifshift
0                                                    announces
0                                                    preamplificatori
0                                                    attenuatori
0x0                                                  has_get_func
0x0                                                  has_set_func
0x0                                                  has_get_level
0x0                                                  has_set_level
0x0                                                  has_get_parm
0x0                                                  has_set_parm
vfo_ops=0x0
ptt_type=0x1
targetable_vfo=0x0
done
```

Si annuncia il modello **1** (dummy) e non quello reale: dichiarare il modello
vero obbligherebbe a riprodurne fedelmente tutte le capacità, e ogni
discrepanza diventerebbe un difetto del client. Il modello neutro descrive
quello che il server sa davvero fare.

## Porta

Il valore predefinito e' **4533**, non 4532. La porta canonica di `rigctld` e'
la 4532, ma e' anche quella su cui Decodium si aspetta di trovare un rigctld
*esterno* quando lo usa come client, ed e' risultata gia' occupata da un altro
programma sulla macchina di prova (Decodium SDR). Partire da una porta libera
evita che la condivisione fallisca al primo avvio con un messaggio che nessuno
va a leggere. Resta configurabile: chi vuole la porta canonica la imposta.

Quando l'apertura fallisce, il pannello riporta il motivo invece di limitarsi a
dire che la condivisione non e' attiva: il caso tipico e' proprio la porta gia'
in uso, e senza quel dettaglio si cerca il guasto altrove.

## Comandi

| Comando | Significato | Risposta |
|---|---|---|
| `f` | frequenza | `14074000` |
| `F <hz>` | imposta frequenza | `RPRT 0` |
| `m` | modo | `PKTUSB` poi `3000` |
| `M <modo> <larghezza>` | imposta modo | `RPRT 0` |
| `t` | stato PTT | `0` o `1` |
| `T <0\|1>` | comanda PTT | `RPRT 0` |
| `v` | VFO corrente | `VFOA` |
| `V <vfo>` | imposta VFO | `RPRT 0` |
| `s` | split | `0` poi `VFOB` |
| `S <0\|1> <vfo>` | imposta split | `RPRT 0` |
| `i` | frequenza di trasmissione | `14074000` |
| `I <hz>` | imposta frequenza TX | `RPRT 0` |
| `q` | chiude | — |

Un comando non gestito si rifiuta con `RPRT -11`.

## Politica decisa

- **Lettura sempre, scrittura su richiesta.** Di serie i programmi collegati
  leggono soltanto; i comandi che cambiano lo stato della radio rispondono
  `RPRT -1` finché non si abilita la scrittura. Un programma dimenticato non
  deve poter mandare in trasmissione la radio.
- **Solo questo computer.** Ascolto su `127.0.0.1`. L'apertura alla rete
  locale richiede una lista di indirizzi ammessi, e quella a Internet
  autenticazione e cifratura: nessuna delle due è un primo passo.
- **Nessun traffico aggiuntivo sulla seriale.** Le letture rispondono dallo
  stato che Decodium tiene già in memoria. Interrogare la radio a ogni domanda
  di ogni client saturerebbe il bus — su CI-V è già successo, con il PTT
  rimasto incollato.
- **Retrocompatibilità.** A interruttore spento non si apre alcuna porta e il
  comportamento è identico a oggi.

## Due istanze di Decodium sulla stessa radio

La seriale la apre un solo programma alla volta, e questo vale anche fra due
Decodium. La seconda istanza non prende la porta: si collega alla prima, che
gliela rivende.

1. **Prima istanza** — Impostazioni, CAT, *CAT condivisa*: interruttore
   acceso, porta 4533. Se la seconda deve anche trasmettere serve *Consenti
   trasmissione*, che ha un interruttore suo apposta.
2. **Seconda istanza** — si avvia con `decodium.exe --rig-name seconda`. Il
   nome cambia il file di blocco, quindi la protezione a istanza singola non
   la ferma, e le impostazioni finiscono in un profilo separato.
3. Nella seconda: Impostazioni, CAT, *Usa una CAT condivisa* →
   `127.0.0.1:4533` → **Collegati**. Il bottone sceglie da solo il backend
   Hamlib e il rig `Hamlib NET rigctl`.

Il passo 3 si può fare anche a mano, ed è quello che facevano finora i
programmi esterni: backend Hamlib, rig `Hamlib NET rigctl`, campo Host:Port.
Verificato con `rigctl -m 2 -r 127.0.0.1:4533`, che legge frequenza, modo e
VFO dalla radio della prima istanza.

Due avvertenze che vale la pena dire prima che le scopra l'utente:

- **La seconda istanza non condivide a sua volta.** Se il suo interruttore di
  condivisione è acceso e la porta è la stessa, non riesce ad aprirla e lo
  scrive: la porta ce l'ha già la prima. Basta spegnerlo, o dargliene
  un'altra.
- **Chi comanda la radio.** Con la scrittura abilitata entrambe le istanze
  possono cambiare frequenza. Non c'è arbitraggio: l'ultima che scrive vince.
  Per il secondo posto di ascolto conviene lasciare la scrittura spenta.

