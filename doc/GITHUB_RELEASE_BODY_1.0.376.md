# Decodium 4 FT2 1.0.376

## Correzioni di sicurezza e stabilità

Da un audit di sicurezza del codice (22 segnalazioni analizzate; diverse già risolte nella 1.0.375). In questa release, 5 correzioni mirate. Nessuna vulnerabilità critica/RCE: l'impatto per un uso desktop singolo è basso, ma queste correzioni eliminano **due crash raggiungibili dall'operatore** e rafforzano la sicurezza in trasmissione.

- **Fix crash in TX su FST4 / FST4W / Fox / RTTY** (`foxcom`): a processo appena avviato, il buffer della forma d'onda poteva non essere allocato prima della scrittura, causando un crash deterministico al primo TX. Ora il buffer è sempre allocato prima dell'uso (corretto in 5 punti del percorso TX).
- **Fix crash su file WAV malformato**: la scansione dei chunk WAV usava aritmetica a 32 bit che, con un campo dimensione corrotto, poteva portare a una lettura fuori dai limiti. Scansione portata a 64 bit con limiti sul buffer.
- **TX-safety FT2 / Digital Morse**: selezionando un CQ dalla Band Activity con "Digital Morse" attivo, il TX poteva armarsi da solo scavalcando la conferma manuale. Ora rispetta il gate (pre-arma e attende il pulsante TX).
- **TX-safety CAT**: in caso di errore CAT durante la trasmissione, lo stop del trasmettitore è ora immediato — niente attesa dei tentativi di ripristino mentre si è in TX.
- **Limite risposta PSK Reporter**: la ricerca PSK Reporter limita ora la dimensione della risposta (1 MiB), come già fa la funzione "heard by", evitando un eccesso di memoria da risposte anomale.

## Asset

- Installer Windows x64 `.exe` (allegato).
- AppImage Linux / pacchetti macOS generati dai runner GitHub.
