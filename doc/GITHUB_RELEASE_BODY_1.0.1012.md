# Decodium 4 FT2 1.0.1012

## English (UK)

Release 1.0.1012 is the iu8lmc fork build that replaces the withdrawn
1.0.1011. It contains everything in 1.0.1010 plus the changes below.

- **Interface scaling is now an explicit option:** Settings has a new
  "Enlarge the interface (accessibility)" checkbox, off by default. While it
  is off the interface is shown at its normal size and the scale selector is
  disabled. When it is on, the scale can be chosen among 110%, 125%, 150%,
  175% and 200% and is applied after a restart. Previously any stored scale
  was applied at the next start, with no on/off switch.
- A scale other than 100% already stored in the profile is kept: the option
  shows as enabled with that value. Untick it to return to the normal size.
- The new option is translated in all 15 interface languages.
- **Per-control scale hooks** (from the withdrawn 1.0.1011): text fields,
  combo boxes, the glass buttons, the SSTV button and the TX message buttons
  carry a `scaleFactor` property. It is not connected to a setting yet and is
  always 1, so the layout is unchanged.
- Validation: the new settings section was exercised in a QML bench with a
  stand-in bridge (off, 125% and 150% starting states) and the build was
  checked with an isolated start-up. It was not checked by hand inside the
  running application.
- 1.0.1011 was online for about an hour. If you installed it and the
  interface looks too large, untick the new option, or set the scale back to
  100% there, and restart.
- The previously reported Hound/SuperFox RR73 issue remains open.

## Italiano

La release 1.0.1012 è la build del fork iu8lmc che sostituisce la 1.0.1011
ritirata. Contiene tutto ciò che c'è nella 1.0.1010 più le modifiche seguenti.

- **La scala dell'interfaccia ora è un'opzione esplicita:** nelle Impostazioni
  c'è la nuova casella "Ingrandisci l'interfaccia (accessibilità)", spenta di
  default. Finché è spenta l'interfaccia resta a dimensione normale e il
  selettore della scala è disattivato. Quando è accesa si può scegliere fra
  110%, 125%, 150%, 175% e 200%, e la scala si applica al riavvio. Prima
  qualunque scala salvata veniva applicata all'avvio successivo, senza un
  interruttore.
- Una scala diversa dal 100% già salvata nel profilo viene mantenuta:
  l'opzione risulta accesa con quel valore. Basta spegnerla per tornare alla
  dimensione normale.
- La nuova opzione è tradotta in tutte le 15 lingue dell'interfaccia.
- **Agganci di scala per i controlli** (dalla 1.0.1011 ritirata): campi di
  testo, caselle combinate, pulsanti "glass", pulsante SSTV e pulsanti dei
  messaggi TX hanno una proprietà `scaleFactor`. Non è ancora collegata a
  un'impostazione e vale sempre 1, quindi l'impaginazione non cambia.
- Verifica: la nuova sezione delle impostazioni è stata provata in un banco
  QML con un bridge sostitutivo (partenza spenta, a 125% e a 150%) e la build
  con un avvio isolato. Non è stata controllata a mano dentro l'applicazione
  in funzione.
- La 1.0.1011 è rimasta online circa un'ora. Se l'hai installata e
  l'interfaccia appare troppo grande, spegni la nuova opzione, oppure riporta
  lì la scala al 100%, e riavvia.
- Il problema Hound/SuperFox RR73 precedentemente segnalato rimane aperto.
