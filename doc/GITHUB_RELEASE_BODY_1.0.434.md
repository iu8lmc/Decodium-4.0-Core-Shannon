# Decodium 4 FT2 1.0.434

Release di manutenzione che integra gli avanzamenti della 1.0.433 e completa il lavoro sui colori decode, sulle evidenziazioni e sulla coerenza visuale fra le finestre di ricezione.

## Novita'

- Aggiunta la scelta del grassetto per ogni regola colore dei decode: CQ, 73/RR73, proprio nominativo, messaggi TX, entita' DX, nuove entita'/zone/grid/nominativi e decode normali possono ora essere configurati separatamente come testo normale o bold.
- Il colore "Normal decodes" ora viene applicato davvero alle righe standard che non rientrano in una categoria speciale.
- Le viste Full Spectrum, Signal RX e le finestre decode usano la stessa priorita' colore/grassetto, evitando differenze fra pannelli diversi.
- La colonna frequenza e' stata resa piu' coerente: frequenze decode normali in colore standard, righe TX in colore TX, senza variazioni spurie legate alla vicinanza con la frequenza RX.
- La voce "Decode Boost" e' stata chiarita come opzione di contrasto visuale, non come aumento della sensibilita' del decoder.
- Aggiornate le traduzioni della nuova etichetta descrittiva in tutte le lingue presenti nel progetto.

## Correzioni

- Integrato il fix 1.0.433: il limite "Caller retries" non interrompe piu' prematuramente un QSO gia' entrato nella fase finale dopo report/RR73/73.
- Quando una categoria colore e' disattivata, Decodium ora rimuove davvero quella evidenziazione invece di ricadere sul colore decode generico.
- I CQ non restano piu' forzati in bianco/bold quando il relativo filtro colore e' disabilitato o configurato diversamente.
- Le righe con entita' DX nota non vengono piu' trattate automaticamente come evidenziazione "DX Entity": quella regola resta riservata alle condizioni realmente speciali.
- Le impostazioni colore e grassetto notificano subito le viste decode, senza richiedere il riavvio dell'app.
- Aggiornati i default versione per build locali e installer a 1.0.434.

## Build

- Codice sorgente allegato automaticamente da GitHub.
- Windows x64: installer generato dal runner Windows.
- macOS Apple Silicon: DMG/ZIP generati dal runner dedicato.
- macOS Intel: DMG/ZIP generati dal runner dedicato.
- Linux x86_64: AppImage generata dal runner dedicato.
- Linux aarch64: AppImage generata dal runner dedicato.
