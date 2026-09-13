# Decodium 4 FT2 1.0.432

Release di manutenzione orientata a usabilita' operativa, filtri contest e coerenza visuale.

## Novita'

- Frequenzimetro principale aggiornato durante TX/Tune in CAT split o fake split: quando il ricetrasmettitore sposta temporaneamente il VFO di trasmissione, Decodium mostra la frequenza TX effettiva nel display principale. La frequenza RX interna resta invariata per decoder, waterfall, log e calcoli di banda.
- Nuovi filtri per contest/logbook:
  - nascondi stazioni gia' lavorate sulla banda corrente;
  - nascondi stazioni gia' lavorate oggi in UTC.
- Tracciamento worked migliorato: lo stato worked viene aggiornato anche dopo il log di un nuovo QSO e ricostruito dal log ADIF usando banda, modo e data QSO.
- Nuovo colore configurabile "Normal decodes" per scegliere il colore base delle righe decode non evidenziate.
- Le evidenziazioni CQ ora rispettano davvero il relativo toggle: se il colore CQ e' disattivato, i CQ non restano forzati in grassetto/colore CQ.
- Pannelli Full Spectrum e Signal RX allineati alla stessa logica colori di decode, incluse evidenziazioni 73/RR73, nominativi arancioni/blu, CQ e testo normale.
- Finestra CALL resa piu' proporzionata su monitor piccoli: dimensioni responsive e scorrimento verticale quando lo spazio non basta.

## Correzioni

- Evitato che il click sui digit del frequenzimetro possa fare QSY durante TX/Tune, quando il display puo' rappresentare la frequenza TX di split.
- I filtri worked ignorano le righe TX e rispettano lo stato globale di bypass filtri.
- Le impostazioni dei nuovi colori e dei filtri worked notificano subito i modelli Full Spectrum e Signal RX, senza dover riavviare l'app.
- Aggiornati i default versione per build locali e installer a 1.0.432.

## Build

- Codice sorgente allegato automaticamente da GitHub.
- Windows x64: installer generato dal runner Windows.
- macOS Apple Silicon: DMG/ZIP generati dal runner dedicato.
- macOS Intel: DMG/ZIP generati dal runner dedicato.
- Linux x86_64: AppImage generata dal runner dedicato.
- Linux aarch64: AppImage generata dal runner dedicato.
