# Decodium 4 FT2 v1.0.618

## English (UK)

### Changes since v1.0.617

- FT8: the a-priori hypothesis toggle now has a checkbox. The backend
  property that turns AP on or off (a-priori decoding: known CQ history,
  the predictive "whole message" check, and the hypotheses on your own
  callsign) already existed and was already wired into every decode
  request, but no screen let a user actually switch it off. Added next to
  "Deep Search" and "Avg Decode" in the Decoder Options menu, same style,
  same persistence. No behaviour change for anyone who leaves it on
  (the default, unchanged).
- FT8 gate: an attempt to retrain the anti-phantom classifier on a
  synthetic collision (pileup) dataset was measured and **not adopted**.
  It improved on two held-out synthetic evaluation sets (same-signal and
  collision) but made things measurably worse on the same 510-slot real
  recording used to validate this morning's echo fix (-2.7% true decodes,
  +3 phantoms versus the current weights). The learned gate weights are
  unchanged from the 5 September retrain. The tooling that produced the
  collision dataset (`tests/ft8_gate_dump --pileup`) stays, in case someone
  wants to try a more varied dataset later; the negative result and the
  reasoning behind it are logged in the lab notes for anyone who revisits
  this.

This release is published with the source code and platform packages built by
the GitHub Actions runners: Windows x64 executable, macOS Apple Silicon and
Intel DMGs, and Linux x86_64 and aarch64 AppImages.

## Italiano

### Modifiche dalla v1.0.617

- FT8: l'interruttore dell'ipotesi a priori (AP) ha ora una spunta.
  La proprietà sul ponte che accende o spegne l'AP (decodifica a priori:
  storico dei CQ noti, la verifica predittiva del messaggio intero, e le
  ipotesi sul proprio nominativo) esisteva già ed era già collegata a ogni
  richiesta di decodifica, ma nessuna schermata permetteva di spegnerla
  davvero. Aggiunta accanto a "Deep Search" e "Avg Decode" nel menu Decoder
  Options, stesso stile, stessa persistenza. Nessun cambiamento per chi la
  lascia accesa (il default, invariato).
- Gate FT8: un tentativo di riaddestrare il classificatore anti-fantasmi su
  un dataset sintetico di collisioni (pileup) è stato misurato e **non
  adottato**. Migliorava su due insiemi di verifica sintetici tenuti da
  parte (segnale singolo e collisione) ma peggiorava misurabilmente sulla
  stessa registrazione reale di 510 slot usata questa mattina per validare
  la correzione dell'eco (-2,7% di decodifiche vere, +3 fantasmi rispetto
  ai pesi attuali). I pesi del gate restano quelli del riaddestramento del
  5 settembre, invariati. Lo strumento che ha prodotto il dataset di
  collisione (`tests/ft8_gate_dump --pileup`) resta, per chi vorrà provare
  un dataset più vario in futuro; il risultato negativo e il suo perché
  sono annotati nel diario di laboratorio per chi ci ritornerà sopra.

Questa release viene pubblicata con il codice sorgente e i pacchetti prodotti
dai runner GitHub Actions: eseguibile Windows x64, DMG macOS Apple Silicon e
Intel, AppImage Linux x86_64 e aarch64.
