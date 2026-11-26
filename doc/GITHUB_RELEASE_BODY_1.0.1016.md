# Decodium 4 FT2 1.0.1016

## English (UK)

Release 1.0.1016 is the iu8lmc fork build of 1.0.1015 with the ALL.TXT logs
kept in memory instead of being read again and again.

- **FT8 known-call seeds:** every 10 seconds the FT8 decoder refreshes the
  calls, CQ callers and messages it has seen in the ALL.TXT logs of
  Decodium, WSJT-X and JTDX. It used to reopen up to ten logs and parse their
  last 2 MB four times, even when nothing had changed: about 400 ms per log
  on a fast PC, so only one log fitted in the time allowed. Now each log is
  parsed once and kept in memory; on each refresh only the lines added since
  the previous one are read. All ten logs are now included, usually in about
  80 ms (up to 300 ms with two busy logs), instead of one log in about 465 ms,
  for about 30 MB of memory with ten large logs. The result is the same as
  reading the files: checked in 1,279 comparisons while the logs were
  growing, being erased and being rewritten.
- **Hashed calls `<...>`:** Decodium can fill in a hashed call from the line
  that another decoder wrote for the same slot in its ALL.TXT. Until now it
  read the last 8 MB of up to 20 logs for each decode batch, and skipped the
  work whenever more than 512 KB had to be read, so with logs a few days old
  it never ran. Now only the lines of the last 15 minutes are kept in memory
  and only the new ones are read: the first batch takes about 26 ms, the
  following ones less than 1 ms. Lines of other days at the same time of day
  no longer take part.
- **Windows: Decodium's own logs as a source.** The FT8 and FT4 decoders
  looked for Decodium's live log in the folder used on Linux and macOS, so on
  Windows the ALL.TXT being written during the session was not used, and
  Decodium's other ALL.TXT ranked below every JTDX and WSJT-X log. Both are
  now found and get their intended priority.
- Two pattern checks used for every CQ and message line are now prepared once
  instead of on every line.
- The decoder itself is unchanged; what changes is which logs feed it, so
  decoding of hashed calls and a priori hints may differ slightly. Reports are
  welcome.
- Everything in 1.0.1015 is included unchanged. The previously reported
  Hound/SuperFox RR73 issue remains open.

## Italiano

La release 1.0.1016 è la build del fork iu8lmc della 1.0.1015 con i log
ALL.TXT tenuti in memoria invece di essere riletti di continuo.

- **Nominativi già visti per FT8:** ogni 10 secondi il decoder FT8 aggiorna
  i nominativi, le stazioni in CQ e i messaggi visti nei log ALL.TXT di
  Decodium, WSJT-X e JTDX. Prima riapriva fino a dieci log e ne rianalizzava
  gli ultimi 2 MB quattro volte, anche se non erano cambiati: circa 400 ms per
  log su un PC veloce, così nel tempo concesso ne entrava uno solo. Ora ogni
  log viene analizzato una volta e tenuto in memoria; a ogni aggiornamento si
  leggono solo le righe aggiunte dalla volta precedente. Entrano tutti e dieci
  i log, di solito in circa 80 ms (fino a 300 ms con due log molto attivi),
  invece di un log in circa 465 ms, con circa 30 MB di memoria per dieci log
  grandi. Il risultato è lo stesso della lettura dei file: verificato in
  1.279 confronti mentre i log crescevano, venivano azzerati e riscritti.
- **Nominativi in hash `<...>`:** Decodium può completare un nominativo in
  hash dalla riga che un altro decoder ha scritto per lo stesso slot nel suo
  ALL.TXT. Finora leggeva gli ultimi 8 MB di fino a 20 log a ogni gruppo di
  decodifiche, e rinunciava se c'era da leggere più di 512 KB: con log di
  qualche giorno non partiva mai. Ora in memoria restano solo le righe degli
  ultimi 15 minuti e si leggono solo quelle nuove: il primo gruppo richiede
  circa 26 ms, i successivi meno di 1 ms. Le righe di altri giorni alla stessa
  ora non entrano più nel confronto.
- **Windows: i log di Decodium come fonte.** I decoder FT8 e FT4 cercavano il
  log in corso di Decodium nella cartella usata su Linux e macOS, così su
  Windows l'ALL.TXT scritto durante la sessione non veniva usato, e l'altro
  ALL.TXT di Decodium aveva meno priorità di tutti i log di JTDX e WSJT-X.
  Ora vengono trovati entrambi, con la priorità prevista.
- Due controlli usati per ogni riga di CQ e di messaggio ora vengono preparati
  una volta sola invece che a ogni riga.
- Il decoder in sé non cambia; cambia quali log lo alimentano, quindi la
  decodifica dei nominativi in hash e i suggerimenti a priori possono
  differire leggermente. Le segnalazioni sono benvenute.
- Tutto ciò che c'è nella 1.0.1015 è incluso senza modifiche. Il problema
  Hound/SuperFox RR73 precedentemente segnalato rimane aperto.
