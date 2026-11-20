# Decodium 4 FT2 1.0.1010

## English (UK)

Release 1.0.1010 is the iu8lmc fork build of elisir80 `main` at `e4f48690`
(1.0.1009) with a few local additions. Everything listed in the 1.0.1009
notes is included unchanged.

- **SSTV workspace:** closing the SSTV window now releases its lazily loaded
  workspace, and the page that was open is remembered for the next opening.
- **Detached windows:** if a window is reopened before its deferred clean-up
  runs, the freshly shown instance is kept instead of being unloaded.
- **Start-up profiling:** setting `DECODIUM_STARTUP_PROFILE=1` logs
  `[STARTUP-PROFILE]` checkpoints (storage, application, command line, SQLite,
  QML cache, bridge) and the cost of the QML content fingerprint. Off by
  default; no behaviour change when unset.
- **Tests:** the SSTV QML test checks the new release request; the FFT and
  downsample thread-lifetime probes report private memory on Windows;
  `test_ftx_weak_decode` links with a larger stack on MinGW.
- The fork installer is built locally with the fork's Qt runtime. Validation
  of these additions is limited to the build, the SSTV QML test and a start-up
  check; they have not been exercised over the air.
- The previously reported Hound/SuperFox RR73 issue remains open.

## Italiano

La release 1.0.1010 è la build del fork iu8lmc del `main` di elisir80 a
`e4f48690` (1.0.1009) con alcune aggiunte locali. Tutto ciò che è elencato
nelle note della 1.0.1009 è incluso senza modifiche.

- **Area di lavoro SSTV:** chiudendo la finestra SSTV la sua area di lavoro,
  caricata su richiesta, viene rilasciata, e la pagina aperta viene ricordata
  per l'apertura successiva.
- **Finestre staccate:** se una finestra viene riaperta prima che parta la
  pulizia differita, l'istanza appena mostrata resta in vita invece di essere
  scaricata.
- **Profilo di avvio:** con `DECODIUM_STARTUP_PROFILE=1` il log riporta i punti
  `[STARTUP-PROFILE]` (archivio, applicazione, riga di comando, SQLite, cache
  QML, bridge) e il costo dell'impronta dei contenuti QML. Spento di default;
  senza la variabile il comportamento non cambia.
- **Test:** il test QML SSTV verifica la nuova richiesta di rilascio; le sonde
  sulla vita dei thread FFT e downsample riportano la memoria privata su
  Windows; `test_ftx_weak_decode` è collegato con uno stack più grande su MinGW.
- L'installer del fork è costruito in locale con il runtime Qt del fork. La
  verifica di queste aggiunte si limita alla build, al test QML SSTV e a un
  controllo di avvio; non sono state provate in aria.
- Il problema Hound/SuperFox RR73 precedentemente segnalato rimane aperto.
