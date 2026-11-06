# Decodium 4.0 v1.0.538

Version 1.0.538 gives every UDP destination its own client id, removes a source
of duplicate decode broadcasts, and stops marking every build as a modified
working tree. All three came out of an operator's spot-collector log, which
rejected every packet Decodium sent with `OLD software version. Break`.

## English (British)

### One client id per UDP destination

- Decodium announced itself with the client id `WSJTX` and its own release
  number as the version. A WSJT-X aware collector read that as WSJT-X 1.0.x,
  compared it with the real 2.7.x and discarded every packet as an old program.
  Nothing was wrong with the data; the label was.
- Companion programs on the local machine, such as JTAlert and GridTracker,
  need the classic name, so a single global identifier could not satisfy both.
  Each destination now carries its own:
  - primary destination: `WSJTX` by default, for local companions;
  - secondary and tertiary destinations: `Decodium` by default, for remote
    collectors.
- Every field is editable, with a preset list offering both names, so any
  combination is possible. The packet rate is unchanged: one message per decode
  per destination, never sent twice with two different names.
- Incoming traffic addressed to `WSJTX` or `WSJT-X` is still accepted, for both
  control messages and ordinary messages, whatever the configured identifier.

### Duplicate decode broadcasts

- The decode de-duplication key used the exact frequency. The same signal
  re-decoded by the deep pass comes back a few hertz away from the first
  estimate, which produced two distinct keys: two rows in the list and two UDP
  packets for one message. External collectors counted that as double traffic.
- The frequency now enters the key in canonical integer form, and duplicates
  are matched with a tolerance of a few hertz. The FT2 asynchronous path
  already quantised the frequency for exactly this reason.

### Every build was reported as dirty

- In `CMake/getsvn.cmake` the sanity flag was hard-coded to `DIRTY`, so the
  check below it was always true and every build, from a clean tree or not, was
  stamped `-dirty` in its revision string. A collector reads that string and
  treats the sender as an unofficial build.
- The tree state is measured again, with `--untracked-files=no`: enumerating
  untracked files was the slow step on Windows that led to the check being
  disabled, and those files never reach the binary anyway.

### Note on decode volume

- Broadcasting one Decode message per decode is the WSJT-X UDP protocol working
  as intended; that is how GridTracker and JTAlert receive their data, and JTDX
  behaves the same way. What changes here is that the packets are no longer
  rejected as coming from an obsolete program, and that the same decode is no
  longer sent twice.

### Validation

- Local `decodium_qml` build completed successfully; `test_udp_client_id`
  passed; `qmllint` reported no errors on the modified QML file.
- Verified on the wire with two simultaneous UDP captures: 33 packets on the
  primary port announcing `WSJTX` and 33 on the secondary announcing
  `Decodium`, with no duplication.
- Verified in the binary: the version resource now reads `1.0.538 f06978`,
  without the `-dirty` suffix.

## Italiano

La versione 1.0.538 assegna a ogni destinazione UDP il proprio identificativo,
elimina una sorgente di decodifiche inviate due volte e smette di marcare ogni
build come albero modificato. I tre punti nascono dal registro di un operatore
che gestisce un collettore di spot: rifiutava ogni pacchetto di Decodium con
`OLD software version. Break`.

### Un identificativo per ogni destinazione UDP

- Decodium si annunciava con identificativo `WSJTX` e con il proprio numero di
  versione. Un collettore che conosce WSJT-X lo leggeva come WSJT-X 1.0.x, lo
  confrontava con il 2.7.x reale e scartava ogni pacchetto come programma
  vecchio. I dati non avevano nulla di sbagliato: era sbagliata l'etichetta.
- I programmi che girano sulla stessa macchina, come JTAlert e GridTracker,
  pretendono il nome classico, quindi un identificativo unico non poteva
  accontentare entrambi. Ora ogni destinazione porta il proprio:
  - destinazione primaria: `WSJTX` di serie, per i programmi locali;
  - destinazioni secondaria e terziaria: `Decodium` di serie, per i collettori
    remoti.
- Tutti i campi sono modificabili, con un menu che offre entrambi i nomi: ogni
  combinazione e' possibile. Il numero di pacchetti non cambia: un messaggio per
  decodifica per destinazione, mai inviato due volte con due nomi diversi.
- Il traffico in arrivo indirizzato a `WSJTX` o `WSJT-X` continua a essere
  accettato, sia per i messaggi di controllo sia per quelli ordinari, qualunque
  identificativo sia configurato.

### Decodifiche inviate due volte

- La chiave di deduplica usava la frequenza esatta. Lo stesso segnale
  ridecodificato dalla passata profonda torna con qualche hertz di scarto
  rispetto alla prima stima, e questo produceva due chiavi distinte: due righe
  in lista e due pacchetti UDP per un solo messaggio. I collettori esterni lo
  contavano come traffico doppio.
- Ora la frequenza entra nella chiave in forma canonica (intero) e i duplicati
  si riconoscono con una tolleranza di pochi hertz. Il percorso FT2 asincrono
  quantizzava gia' la frequenza proprio per questo motivo.

### Ogni build risultava "dirty"

- In `CMake/getsvn.cmake` la spia di controllo era cablata a `DIRTY`, quindi la
  verifica subito sotto risultava sempre vera e ogni build, pulita o meno,
  usciva marcata `-dirty` nella stringa di revisione. Un collettore legge quella
  stringa e considera il mittente una build non ufficiale.
- Lo stato dell'albero torna a essere misurato davvero, con
  `--untracked-files=no`: enumerare i file non tracciati era il passaggio lento
  su Windows che aveva portato a disattivare il controllo, e quei file nel
  binario non entrano comunque.

### Nota sul volume delle decodifiche

- Trasmettere un messaggio Decode per ogni decodifica e' il funzionamento
  previsto dal protocollo UDP di WSJT-X: e' cosi' che GridTracker e JTAlert
  ricevono i dati, e JTDX si comporta allo stesso modo. Qui cambia che i
  pacchetti non vengono piu' rifiutati come provenienti da un programma
  obsoleto e che la stessa decodifica non parte piu' due volte.

### Verifica

- Build locale di `decodium_qml` completata correttamente; `test_udp_client_id`
  superato; `qmllint` non ha segnalato errori sul file QML modificato.
- Verificato sul filo con due catture UDP simultanee: 33 pacchetti sulla porta
  primaria con identificativo `WSJTX` e 33 sulla secondaria con `Decodium`,
  senza duplicazioni.
- Verificato nel binario: la risorsa di versione riporta ora `1.0.538 f06978`,
  senza il suffisso `-dirty`.

## Release assets

The release workflows publish the Windows x64 executable, macOS Intel and
Apple Silicon DMG packages, and Linux x86_64 and aarch64 AppImages together
with their checksums where provided by the workflow.
