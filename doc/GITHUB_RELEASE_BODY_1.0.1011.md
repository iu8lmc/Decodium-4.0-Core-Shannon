# Decodium 4 FT2 1.0.1011

## English (UK)

Release 1.0.1011 is the iu8lmc fork build of 1.0.1010 plus one QML change.
It behaves like 1.0.1010; there is nothing new to see in this release.

- **Per-control scale hooks:** text fields, combo boxes, the glass buttons,
  the SSTV button and the TX message buttons now multiply their sizes,
  paddings, corner radii and font sizes by a `scaleFactor` property. The
  factor reads a bridge setting that is not exposed yet, so it is always 1
  and the layout is unchanged. Interface scaling still works through the
  existing UI scale option in the settings.
- The SSTV button keeps its plain template-based drawing (issue #86).
- Validation is limited to an isolated start-up check and the SSTV digital
  QML test; the windows that use these controls were not checked by hand.
- Everything listed in the 1.0.1010 and 1.0.1009 notes is included unchanged.
- The previously reported Hound/SuperFox RR73 issue remains open.

## Italiano

La release 1.0.1011 è la build del fork iu8lmc della 1.0.1010 con una
modifica QML in più. Si comporta come la 1.0.1010: in questa release non c'è
nulla di nuovo da vedere.

- **Agganci di scala per i controlli:** campi di testo, caselle combinate,
  pulsanti "glass", pulsante SSTV e pulsanti dei messaggi TX moltiplicano
  dimensioni, margini, raggi degli angoli e dimensioni dei caratteri per una
  proprietà `scaleFactor`. Il fattore legge un'impostazione del bridge non
  ancora esposta, quindi vale sempre 1 e l'impaginazione non cambia. La scala
  dell'interfaccia continua a funzionare con l'opzione esistente nelle
  impostazioni.
- Il pulsante SSTV mantiene il disegno semplice basato sui template
  (issue #86).
- La verifica si limita a un controllo di avvio isolato e al test QML SSTV
  digitale; le finestre che usano questi controlli non sono state controllate
  a mano.
- Tutto ciò che è elencato nelle note della 1.0.1010 e della 1.0.1009 è
  incluso senza modifiche.
- Il problema Hound/SuperFox RR73 precedentemente segnalato rimane aperto.
