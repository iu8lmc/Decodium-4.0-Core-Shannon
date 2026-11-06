# Decodium 4 v1.0.708

## English (British)

This release is built on v1.0.707. It reduces background work during busy bands and
completes the translations of the 1.0.707 additions.

### Less background work in Map Intelligence

- New decodes and PSK spots no longer trigger a full database refresh of the map
  every time. These automatic refreshes are now spaced at least 2.5 seconds apart.
  Changes you make yourself (filters, layers, import) are still applied at once.
- The map and its panels show the same data as before; only the number of database
  queries during continuous FT2 or FT8 traffic goes down.

### Translations

- The first-start Slow PC question no longer mentions OpenGL (Decodium uses
  Direct3D 11): it now says that the mode uses conservative graphics settings.
- The **Open diagnostic panel** button of the bug report window is translated into
  all 15 languages of the interface.

### Smaller package

- Four QML files that no part of the program used any more were removed
  (AstroWindowContent, TxRow, BandScale, MessageAvgPanel).

### Limits

- The new refresh spacing and the translated texts have not been compared on very old
  hardware or checked by native speakers of every language yet.

### Release contents

- Based on Decodium 4 v1.0.707 (itself built on v1.0.706 and
  elisir80/Decodium-4.0-Core-Shannon v1.0.705).
- The Windows installer is built from this repository with the fork packaging.

## Italiano

Questa release è costruita sulla v1.0.707. Riduce il lavoro in background quando le
bande sono affollate e completa le traduzioni delle novità della 1.0.707.

### Meno lavoro in background in Map Intelligence

- Le nuove decodifiche e i nuovi spot PSK non fanno più rileggere ogni volta tutto il
  database alla mappa. Questi aggiornamenti automatici sono ora distanziati di almeno
  2,5 secondi. Le modifiche che fai tu (filtri, livelli, importazione) vengono
  applicate subito, come prima.
- La mappa e i suoi pannelli mostrano gli stessi dati di prima; cala solo il numero di
  interrogazioni al database durante il traffico continuo FT2 o FT8.

### Traduzioni

- La domanda sulla modalità PC lento al primo avvio non nomina più OpenGL (Decodium
  usa Direct3D 11): ora dice che la modalità usa impostazioni grafiche prudenti.
- Il pulsante **Open diagnostic panel** della finestra di segnalazione problemi è
  tradotto in tutte le 15 lingue dell'interfaccia.

### Pacchetto più piccolo

- Sono stati tolti quattro file QML che nessuna parte del programma usava più
  (AstroWindowContent, TxRow, BandScale, MessageAvgPanel).

### Limiti

- La nuova distanza tra gli aggiornamenti e i testi tradotti non sono ancora stati
  provati su hardware molto vecchio né controllati da madrelingua di ogni lingua.

### Contenuto della release

- Basata su Decodium 4 v1.0.707 (a sua volta costruita sulla v1.0.706 e su
  elisir80/Decodium-4.0-Core-Shannon v1.0.705).
- L'installer Windows è costruito da questo repository con il pacchetto del fork.
