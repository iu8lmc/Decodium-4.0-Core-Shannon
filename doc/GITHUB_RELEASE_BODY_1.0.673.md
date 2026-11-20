# Decodium 4 v1.0.673 — CW live typing and SSB radio controls

## English

- Optional **TX while typing** in the CW window. Newly typed text is queued and
  transmitted after a short pause; text entered while a CW chunk is sending is
  queued for the next chunk.
- SSB Hamlib controls now include **ATU Tune** and **Power off radio**. Power
  off asks for confirmation and closes the CAT session after success.
- Both radio actions are unavailable during transmission. Unsupported tuner
  and power commands are reported by Hamlib in the SSB status area.

Windows x64 installer and portable ZIP include runtime libraries and auxiliary
programs. The application compiled successfully and the complete dependency
scan found no unresolved DLLs. Hardware behavior depends on the radio and its
Hamlib driver; verify with the connected equipment.

## Italiano

- **TX mentre si digita** opzionale nella finestra CW. Il testo digitato viene
  accodato e trasmesso dopo una breve pausa; altro testo inserito durante
  l'invio passa al blocco successivo.
- I controlli SSB Hamlib includono **ATU Tune** e **Spegni radio**. Lo
  spegnimento richiede conferma e chiude la sessione CAT se riesce.
- I comandi radio sono disabilitati durante la trasmissione. Hamlib mostra
  nello stato SSB gli eventuali comandi tuner o power non supportati.

Installer e ZIP portabile Windows x64 includono librerie runtime e programmi
ausiliari. Compilazione completata e scansione delle dipendenze senza DLL
mancanti. Il comportamento hardware dipende dalla radio e dal driver Hamlib:
verificare con l'apparato collegato.
