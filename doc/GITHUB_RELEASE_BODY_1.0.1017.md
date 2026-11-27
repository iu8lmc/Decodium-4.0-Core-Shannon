# Decodium 4 FT2 1.0.1017

## English (UK)

Release 1.0.1017 is the iu8lmc fork build of 1.0.1016 with the settings kept
open in memory instead of being reopened for every read.

- **Settings reads:** while decoding, Decodium reads many settings for every
  group of decodes and for every decoded line (filters, contest options,
  telemetry, map, UDP). Each read used to reopen the settings file: about
  0.14–0.29 ms and up to 22 file-system checks each, all on the user
  interface thread; reading the decode filters alone reopened it about fifty
  times per group. The settings objects are now kept open and reused: a read
  takes about 0.001 ms and no file-system checks.
- The values read are the same: everything Decodium writes is visible at once,
  and a change made from outside the program (a second instance or a manual
  edit of the INI file) is picked up within a quarter of a second.
- Everything in 1.0.1016 is included unchanged. The previously reported
  Hound/SuperFox RR73 issue remains open.

## Italiano

La release 1.0.1017 è la build del fork iu8lmc della 1.0.1016 con le
impostazioni tenute aperte in memoria invece di essere riaperte a ogni
lettura.

- **Lettura delle impostazioni:** durante la decodifica Decodium legge molte
  impostazioni per ogni gruppo di decodifiche e per ogni riga decodificata
  (filtri, opzioni contest, telemetria, mappa, UDP). Ogni lettura riapriva il
  file delle impostazioni: circa 0,14–0,29 ms e fino a 22 controlli sul file
  system ciascuna, tutto sul thread dell'interfaccia; la sola lettura dei
  filtri di decodifica lo riapriva una cinquantina di volte per gruppo. Ora
  gli oggetti delle impostazioni restano aperti e vengono riusati: una lettura
  richiede circa 0,001 ms e nessun controllo sul file system.
- I valori letti sono gli stessi: tutto ciò che Decodium scrive è visibile
  subito, e una modifica fatta da fuori del programma (una seconda istanza o
  il file INI modificato a mano) arriva entro un quarto di secondo.
- Tutto ciò che c'è nella 1.0.1016 è incluso senza modifiche. Il problema
  Hound/SuperFox RR73 precedentemente segnalato rimane aperto.
