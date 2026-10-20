# Note Telegram — Decodium 4 FT2 v1.0.394

Release: https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.394

---

## 🇮🇹 Italiano

📡 **Decodium 4 FT2 — v1.0.394**
🎯 FT2: trovata e chiusa la causa vera delle collisioni — grazie ZL3DMH e ZL1BW!

🛠 **Cosa cambia**
• **FT2 — la risposta ora mira sempre al periodo giusto** — un bug di arrotondamento (l'orario dei decode è mostrato a secondi interi, ma metà degli slot FT2 inizia a .75) faceva trasmettere la risposta **nello stesso periodo del CQ in 12 casi su 16**: i due si coprivano a vicenda e nessuno sentiva l'altro. Scovato incrociando i log di due stazioni neozelandesi che non riuscivano ad agganciarsi se non spegnendo il TX a mano. Ora il doppio clic mira sempre al periodo opposto.
• **FT2 — il "phase-lock breaker" copre anche CQ e prima chiamata** — se una collisione nasce comunque (QSB, orologi), lo sfasamento automatico ora interviene anche su TX1 e TX6 (prima solo a QSO avviato).
• **FT2 — cede sempre il lato giusto** — in fase di aggancio è chi risponde a sfasarsi (il CQer, sordo durante la collisione, non può accorgersene): niente più stalli "nessuno dei due si sposta".

Se con le versioni precedenti vi capitava di chiamarvi a vicenda senza mai decodificarvi: era questo. Aggiornate entrambi e riprovate. 📻

⬇️ Download:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.394

73! 🌍

---

## 🇬🇧 English

📡 **Decodium 4 FT2 — v1.0.394**
🎯 FT2: the real cause of collisions found and fixed — thanks ZL3DMH and ZL1BW!

🛠 **What changed**
• **FT2 — replies now always aim at the right period** — a rounding bug (decode times are displayed as whole seconds, but half of FT2 slots start at .75) made the reply transmit **in the same period as the CQ in 12 cases out of 16**: both stations covered each other and neither could hear the other. Found by cross-matching the logs of two New Zealand stations who could only connect by manually toggling TX. Double-click now always aims at the opposite period.
• **FT2 — the phase-lock breaker now covers CQ and first call too** — if a collision still forms (QSB, clocks), the automatic phase shift now also kicks in on TX1 and TX6 (previously only mid-QSO).
• **FT2 — the right side always yields** — while linking up, it's the responder that shifts (the CQer is deaf during the collision and can't notice it): no more "neither station moves" standoffs.

If on previous versions you kept calling each other without ever decoding: this was it. Both update and try again. 📻

⬇️ Download:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.394

73! 🌍

---

## 🇫🇷 Français

📡 **Decodium 4 FT2 — v1.0.394**
🎯 FT2 : la vraie cause des collisions trouvée et corrigée — merci ZL3DMH et ZL1BW !

🛠 **Ce qui change**
• **FT2 — la réponse vise désormais toujours la bonne période** — un bug d'arrondi (l'heure des décodages est affichée en secondes entières, mais la moitié des slots FT2 commence à .75) faisait émettre la réponse **dans la même période que le CQ dans 12 cas sur 16** : les deux stations se couvraient mutuellement et aucune n'entendait l'autre. Découvert en croisant les logs de deux stations néo-zélandaises qui ne parvenaient à se contacter qu'en coupant le TX à la main. Le double-clic vise maintenant toujours la période opposée.
• **FT2 — le "phase-lock breaker" couvre aussi le CQ et le premier appel** — si une collision survient malgré tout (QSB, horloges), le décalage automatique intervient désormais aussi sur TX1 et TX6 (avant : seulement en cours de QSO).
• **FT2 — c'est toujours le bon côté qui cède** — pendant l'accrochage, c'est celui qui répond qui se décale (le CQer, sourd pendant la collision, ne peut pas s'en apercevoir) : fini les blocages « aucun des deux ne bouge ».

Si avec les versions précédentes vous vous appeliez mutuellement sans jamais vous décoder : c'était ça. Mettez à jour les deux côtés et réessayez. 📻

⬇️ Téléchargement :
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.394

73 ! 🌍

---

## 🇪🇸 Español

📡 **Decodium 4 FT2 — v1.0.394**
🎯 FT2: encontrada y cerrada la causa real de las colisiones — ¡gracias ZL3DMH y ZL1BW!

🛠 **Qué cambia**
• **FT2 — la respuesta ahora siempre apunta al período correcto** — un bug de redondeo (la hora de los decodes se muestra en segundos enteros, pero la mitad de los slots FT2 empieza en .75) hacía que la respuesta transmitiera **en el mismo período del CQ en 12 casos de 16**: las dos estaciones se tapaban mutuamente y ninguna oía a la otra. Descubierto cruzando los logs de dos estaciones neozelandesas que solo conseguían enlazar apagando el TX a mano. El doble clic ahora siempre apunta al período opuesto.
• **FT2 — el "phase-lock breaker" cubre también CQ y primera llamada** — si una colisión surge igualmente (QSB, relojes), el desfase automático ahora también actúa en TX1 y TX6 (antes solo con el QSO en marcha).
• **FT2 — siempre cede el lado correcto** — durante el enganche es quien responde el que se desfasa (el CQer, sordo durante la colisión, no puede darse cuenta): no más bloqueos de "ninguno de los dos se mueve".

Si con versiones anteriores os llamabais mutuamente sin decodificaros nunca: era esto. Actualizad ambos y probad de nuevo. 📻

⬇️ Descarga:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.394

¡73! 🌍
