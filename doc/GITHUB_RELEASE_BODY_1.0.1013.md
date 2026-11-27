# Decodium 4 FT2 1.0.1013

## English (UK)

Release 1.0.1013 is the iu8lmc fork build of 1.0.1012 with two fixes in the
decode lists.

- **Row background colours now work:** a decode can fit several colour
  categories at once (for example CQ, new callsign and LoTW user). The
  background used to come only from the first of them, even when that one had
  no background switched on, so a background set for a lower category such as
  "LoTW marker" almost never appeared. The first matching category with its
  background switched on is now used.
- **LoTW tag:** stations in the LoTW user activity list show a small "LoTW"
  tag instead of the previous 6-pixel dot, in Full Spectrum (embedded,
  floating and DX-Pedition), Signal RX (embedded and floating) and the decode
  windows. It uses the "LoTW marker" colour.
- **Settings > Colours:** new checkbox to mark LoTW users, off by default.
  It is the same switch as "LotW Enabled" in the LoTW section; the tag, the
  LoTW colour and the LoTW background all need the LoTW user list. A note
  explains how the background is chosen. New strings are translated in all
  15 interface languages.
- **Tests:** the source check of the window release calls now expects the
  SSTV workspace too (it failed on the Linux SSTV CI since 1.0.1010).
- Validation: an isolated laboratory run with injected decodes showed the
  LoTW tag and the LoTW background on a CQ from a LoTW user and nothing on a
  non-LoTW station; the decode-colour harness covers the new priority. Not
  yet used over the air.
- The previously reported Hound/SuperFox RR73 issue remains open.

## Italiano

La release 1.0.1013 è la build del fork iu8lmc della 1.0.1012 con due
correzioni nelle liste decode.

- **I colori di sfondo delle righe ora funzionano:** una decodifica può
  rientrare in più categorie di colore insieme (per esempio CQ, nominativo
  nuovo e utente LoTW). Lo sfondo veniva preso solo dalla prima, anche se per
  quella lo sfondo era spento, quindi uno sfondo impostato per una categoria
  più in basso come "Marker LoTW" non compariva quasi mai. Ora si usa la prima
  categoria corrispondente che ha lo sfondo acceso.
- **Etichetta LoTW:** le stazioni presenti nell'elenco di attività degli
  utenti LoTW mostrano una piccola etichetta "LoTW" al posto del puntino di 6
  pixel, in Full Spectrum (integrata, staccata e DX-Pedition), Signal RX
  (integrata e staccata) e nelle finestre decode. Usa il colore "Marker LoTW".
- **Impostazioni > Colori:** nuova casella per segnalare gli utenti LoTW,
  spenta di default. È lo stesso interruttore di "LotW Enabled" nella sezione
  LoTW: etichetta, colore e sfondo LoTW richiedono l'elenco utenti LoTW. Una
  nota spiega come viene scelto lo sfondo. Le stringhe nuove sono tradotte in
  tutte le 15 lingue dell'interfaccia.
- **Test:** il controllo sorgente delle chiamate di rilascio delle finestre
  ora comprende anche l'area SSTV (falliva nella CI SSTV Linux dalla 1.0.1010).
- Verifica: una prova di laboratorio isolata con decode iniettati ha mostrato
  etichetta e sfondo LoTW su un CQ di un utente LoTW e niente su una stazione
  non LoTW; il banco dei colori copre la nuova priorità. Non ancora usata in
  aria.
- Il problema Hound/SuperFox RR73 precedentemente segnalato rimane aperto.
