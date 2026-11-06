# Telemetria di un amplificatore verso il DECØMETER, via TCI

Richiesta di PA3GYQ: mostrare nel DECØMETER la potenza all'uscita
dell'amplificatore (SPE Expert 1.5K Taurus) invece di quella dell'eccitatrice.

**La strada esiste già e non richiede modifiche a Decodium.** Il percorso TCI
accetta potenza e ROS, li converte e li porta fino allo strumento. Quello che
manca è soltanto qualcosa che legga l'amplificatore e parli TCI.

Verificato leggendo `Transceiver/TCITransceiver.cpp` e
`src/radio/DecodiumTransceiverManager.cpp`, non dedotto dalla documentazione.

## Cosa Decodium accetta

| Comando TCI | Come viene letto |
|---|---|
| `tx_power:<valore>;` | potenza, da `arg(0)` |
| `tx_swr:<valore>;` | ROS |
| `tx_sensors:<trx>,<*>,<*>,<potenza>,<ros>;` | potenza da `arg(3)`, ROS da `arg(4)` |

I valori sono **decimali semplici**, con il punto come separatore: `407.0` per i
watt, `1.20` per il ROS. Non serve alcuna scala particolare — le conversioni
interne (×10 nel parser, poi ×100 per la potenza e ×10 per il ROS, e infine
÷1000 e ÷100 nel bridge) si compensano esattamente e producono watt e rapporto.

## Le tre condizioni perché il dato compaia

1. **L'impostazione «PWR and SWR» dev'essere accesa** (Impostazioni → CAT).
   Senza, il ramo `do_pwr_` è spento e la telemetria viene scartata anche se
   arriva. È la stessa impostazione che serve alla lettura via Hamlib.
2. **Solo in trasmissione**: la propagazione avviene sotto
   `if (do_pwr_ && PTT_)`. In ricezione i valori restano a zero, per scelta.
3. **Il server deve inviare i dati.** Decodium chiede attivamente lo stream con
   `tx_sensors_enable:true,500;` — ogni 500 ms — ma **solo se il server si
   identifica come ESDR3 o HPSDR**. Un ponte che si identifichi altrimenti deve
   inviare `tx_power` e `tx_swr` di propria iniziativa: quelli vengono accolti
   comunque.

## Cosa resta da costruire, e dove

Il pezzo mancante sta **fuori** da Decodium: un ponte che apra la seriale
dell'amplificatore, ne legga la telemetria e la pubblichi come server TCI.

È una buona notizia per due ragioni. Non tocca il percorso radio, che è la
parte delicata del programma. E chiunque può scriverlo per il proprio
amplificatore senza toccare Decodium: il contratto è quello qui sopra.

Resta il vincolo pratico di sempre: **una porta seriale la apre un solo
programma alla volta**. Se il software del costruttore tiene la USB
dell'amplificatore, il ponte non può accedervi. Serve una seconda via — un
secondo connettore, una porta Ethernet, o un'esportazione dati offerta dal
software stesso.

## L'alternativa via Hamlib

Hamlib ha un'interfaccia per amplificatori e conosce gli SPE Expert (modello
401, famiglia 1.3K-FA/1.5K-FA/2K-FA), ma quel backend dichiara
`has_get_level = 0x0` pur avendo la funzione di lettura implementata: non si
può stabilire dall'esterno se risponda. La sonda in `tools/amp-probe/` lo
chiede all'apparato.

Se rispondesse, sarebbe la strada più diretta perché non richiede alcun ponte —
solo un pannello in Decodium per aprire l'amplificatore. Le due strade non si
escludono.
