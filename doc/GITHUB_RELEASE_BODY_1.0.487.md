# Decodium 4 FT2 1.0.487

## Italiano

Punti principali (`1.0.486 -> 1.0.487`):

- **Aggiornamento automatico con avviso e conferma**:
  - all'avvio Decodium controlla (una volta al giorno) se c'e' una versione piu' recente. Se la trova,
    apre un avviso con le note della nuova versione e tre scelte: **Aggiorna ora**, **Piu' tardi**,
    **Salta questa versione**. Nulla parte da solo: nessun download senza la tua conferma.
  - "Aggiorna ora" scarica l'installer con barra di avanzamento, chiude Decodium e avvia
    l'installazione; le impostazioni e il log dei QSO vengono mantenuti.
  - il controllo puo' essere lanciato anche a mano dal menu ("Controlla aggiornamenti...").
  - Nota storica: il controllo aggiornamenti era disattivato dalla 1.0.62 e non aveva mai avvisato
    nessuno — questa e' la ragione per cui molte stazioni restavano su versioni molto vecchie.

- **Piattaforma di assistenza integrata (autodiagnosi + segnalazione alla community)**:
  - alla voce **"Segnala un problema"** l'app esegue prima un'**autodiagnosi** che riconosce le cause
    piu' comuni che NON sono bug: versione non aggiornata, orologio del PC fuori sincronia (giudicato in
    proporzione allo slot del modo), MAM multi-stream acceso in FT2, troppi thread di decodifica, banda
    chiusa / audio non collegato, nominativo mancante. Cosi' molti problemi si risolvono subito, senza
    aprire una segnalazione.
  - se serve segnalare, il report viene pubblicato sul forum della community (**groups.ft2.it**), nella
    **lingua dell'utente** e nella **categoria** giusta (suggerita dall'autodiagnosi e modificabile), con
    allegato il contesto diagnostico. Un pulsante dedicato **carica l'ultimo log diagnostico** intitolato
    col tuo nominativo nel gruppo Generale. Protezione anti-doppione integrata.

- Include gli aggiornamenti di elisir80 dalla 1.0.483 alla 1.0.486: audio legacy e validazione JT9/JT65,
  consegna dei decode piu' fluida, resa stabile in warm-run, e hardening del packaging Windows/Qt.

## English

Release highlights (`1.0.486 -> 1.0.487`):

- **Automatic updates with notice and confirmation**: on startup Decodium checks once a day for a newer
  release. If one is found it shows a notice with the release notes and three choices — **Update now**,
  **Later**, **Skip this version**. Nothing happens on its own: no download without your confirmation.
  "Update now" downloads the installer with a progress bar, closes Decodium and starts the installation;
  settings and QSO log are kept. A manual check is available from the menu. (The update check had been
  disabled since 1.0.62 and had never notified anyone — the reason many stations stayed on very old
  versions.)

- **Built-in support platform (self-check + community reporting)**: "Report a problem" first runs a
  **self-check** that recognises the most common non-bug causes — outdated version, PC clock out of sync
  (judged against the mode's slot length), MAM multi-stream enabled in FT2, too many decoder threads,
  closed band / audio not connected, missing callsign — so many issues are solved on the spot. When a
  report is needed it is posted to the community forum (**groups.ft2.it**) in the user's language and the
  right category (suggested by the self-check, editable), with the diagnostic context attached. A dedicated
  button uploads the latest diagnostic log, titled with your callsign, to the General group. Duplicate
  protection included.

- Includes elisir80's updates from 1.0.483 to 1.0.486: legacy audio and JT9/JT65 validation, smoother
  decode delivery, stable warm-run rendering, and hardened Windows/Qt packaging.
