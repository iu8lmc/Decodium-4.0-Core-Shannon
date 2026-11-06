# Decodium 4 v1.0.703

## English (British)

This release is built on v1.0.702 (CW and CAT transmission-stop hardening) and
refreshes the Stellar Light theme, which many users found hard to read, and adds
brightness and contrast controls to the main header.

### Stellar Light theme

- The palette now follows the light theme of the iq8do.aricaserta.it and
  hamhub.cloud sites: cool blue-grey page, softer off-white cards and darker,
  higher-contrast accents. Text keeps at least a 4.5:1 contrast ratio on the page,
  the cards and the panel headers.
- The window background is a single flat colour. The light-to-dark gradient that
  made the top of the window look washed out has been removed in the light theme.
- The cards are no longer near-white, to reduce glare and eye strain; thin
  borders separate the panel layers instead of large lightness differences.
- Text fields in the QSO log window and in the shared text-field component no
  longer use white text on a pale field, which made them unreadable.
- The rotor panel uses the same softer light surface.

### Brightness and contrast sliders

- The main header has two compact sliders next to the font-size buttons and the
  light/dark switch: brightness (sun icon) and contrast (half-moon icon), from
  -100 to +100, where 0 is the original palette.
- Brightness lightens or darkens backgrounds, panels and borders. Contrast scales
  how far text and accent colours sit from the background, and how much the page,
  bands and cards stand apart.
- Click an icon to reset that slider; hover a slider to read its value.
- The values are stored per theme in the active profile, so tuning Stellar Light
  does not alter the dark themes. The setting is written once when you stop
  dragging.
- In the light theme a legibility guard still keeps the text at a minimum 4.5:1
  contrast with the background, even with contrast at -100. Decode tables and the
  waterfall use their own palettes and do not follow the sliders.

### Release contents

- Based on elisir80/Decodium-4.0-Core-Shannon v1.0.702.
- The Windows installer is built from this repository with the fork packaging.

## Italiano

Questa release è costruita sulla v1.0.702 (CW e arresto della trasmissione via
CAT) e rinnova il tema Stellar Light, che a molti risultava poco leggibile.
Aggiunge inoltre i controlli di luminosità e contrasto nell'intestazione
principale.

### Tema Stellar Light

- La palette segue il tema chiaro dei siti iq8do.aricaserta.it e hamhub.cloud:
  pagina grigio-azzurra fredda, schede bianco spento più morbide e colori
  d'accento più scuri e contrastati. Il testo mantiene almeno 4,5:1 di contrasto
  su pagina, schede e intestazioni dei pannelli.
- Lo sfondo della finestra è un colore unico e piatto. Nel tema chiaro è stato
  tolto il gradiente chiaro-scuro che faceva sembrare slavata la parte alta.
- Le schede non sono più quasi bianche, per ridurre l'abbagliamento e
  l'affaticamento degli occhi; i livelli dei pannelli sono separati da filetti
  sottili e non da grandi differenze di chiarezza.
- I campi di testo della finestra del log QSO e il componente comune dei campi di
  testo non usano più testo bianco su campo chiaro, che risultava illeggibile.
- Il pannello rotore usa la stessa superficie chiara più morbida.

### Cursori di luminosità e contrasto

- Nell'intestazione principale ci sono due cursori compatti accanto ai tasti
  dei caratteri e allo switch chiaro/scuro: luminosità (icona sole) e contrasto
  (icona mezza luna), da -100 a +100, dove 0 è la palette originale.
- La luminosità schiarisce o scurisce sfondi, pannelli e filetti. Il contrasto
  regola quanto testi e colori d'accento si staccano dallo sfondo e quanto si
  distinguono pagina, fasce e schede.
- Un clic sull'icona azzera il cursore; passando il mouse si legge il valore.
- I valori sono salvati per ogni tema nel profilo attivo, quindi regolare Stellar
  Light non altera i temi scuri. Il salvataggio avviene una sola volta, a
  trascinamento finito.
- Nel tema chiaro una protezione di leggibilità mantiene comunque il testo ad
  almeno 4,5:1 sullo sfondo, anche con il contrasto a -100. Le tabelle di
  decodifica e il waterfall hanno palette proprie e non seguono i cursori.

### Contenuto della release

- Basata su elisir80/Decodium-4.0-Core-Shannon v1.0.702.
- L'installer Windows è costruito da questo repository con il pacchetto del fork.
