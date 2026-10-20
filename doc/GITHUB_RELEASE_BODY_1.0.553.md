# Decodium 4 FT2 v1.0.553

This corrective release withdraws the CAT power/SWR settings transaction introduced in v1.0.552 and restores the proven v1.0.551 behaviour while that area is redesigned. It also fixes callsign recognition and DXCC attribution for Indonesian stations, Indian stations and portable callsigns with numeric suffixes.

## English (British)

### CAT power and SWR rollback

- Reverted the CAT power/SWR settings propagation introduced in v1.0.552.
- Restored the v1.0.551 handling of **PWR and SWR** and **Check SWR**, including the previous settings and Hamlib construction paths.
- Removed the v1.0.552 atomic telemetry helper, its automatic coupling of meter polling and SWR protection, and the associated CAT rebuild logic.
- Restored the previous DECOMETER telemetry-enabling request and the previous Settings controls.
- Removed the v1.0.552-only telemetry-state test and diagnostic fields.
- This rollback is intentional: the v1.0.552 implementation will not be applied while the power/SWR behaviour is reviewed and redesigned.

### Callsign recognition

- Added shared, stricter callsign recognition for decoded messages so all relevant views use the same rules.
- Fixed valid Indonesian special-event calls beginning with `8A`, `8B` and `8D`, including calls such as `8A3B`, `8B8FTDM` and `8D8DADA`.
- Valid calls which happen to contain only hexadecimal characters are no longer rejected as telemetry payloads merely because of their spelling.
- Added support for portable numeric suffixes `/0` through `/9`; for example, `IZ1ABC/0` and `IZ1ABC/1` remain valid callsigns while their DXCC entity is resolved from the base call.
- Retained rejection of genuine unresolved hexadecimal telemetry payloads.

### Correct DXCC attribution

- Directed messages now use the semantic station on the right-hand side for DXCC, worked-before and LoTW status.
- If the right-hand station is invalid or unresolved, the DXCC field is left empty instead of incorrectly using the station written on the left.
- Fixed the reported case `IZ1JIZ VU33IN RR73`: `VU33IN` is now selected and shown as **India**, rather than attributing the row to Italy.
- Confirmed `8A3B`, `8B8FTDM` and `8D8DADA` as **Indonesia** using the actual `cty.dat` distributed with Decodium.

### Validation

- The complete `decodium_qml` application target builds successfully on macOS Apple Silicon.
- QSO parser tests cover Indonesian calls, Indian calls, numeric portable suffixes, unknown right-hand stations and the no-left-fallback rule.
- DXCC tests use both controlled fixtures and the bundled production `cty.dat`.
- QMX telemetry protection tests continue to pass after the v1.0.552 rollback.
- No panadapter, waterfall, decoder DSP or audio processing path is changed by this release.

---

## Italiano

Questa release correttiva ritira la gestione delle impostazioni CAT di potenza/ROS introdotta nella v1.0.552 e ripristina il comportamento collaudato della v1.0.551, in attesa di una sua riprogettazione. Corregge inoltre il riconoscimento dei nominativi e l'attribuzione DXCC per le stazioni indonesiane, indiane e per i nominativi portatili con suffisso numerico.

### Ripristino della gestione CAT di potenza e ROS

- Annullata la propagazione delle impostazioni CAT di potenza/ROS introdotta nella v1.0.552.
- Ripristinato il comportamento della v1.0.551 per **PWR and SWR** e **Check SWR**, compresi i precedenti percorsi delle impostazioni e di costruzione Hamlib.
- Rimossi l'helper atomico della telemetria della v1.0.552, l'accoppiamento automatico fra lettura dei meter e protezione ROS e la relativa logica di ricostruzione CAT.
- Ripristinate la precedente richiesta di attivazione telemetrica del DECOMETER e i precedenti controlli nelle Impostazioni.
- Rimossi il test dello stato telemetrico e i campi diagnostici presenti esclusivamente nella v1.0.552.
- Il rollback è intenzionale: l'implementazione della v1.0.552 non verrà applicata mentre il comportamento di potenza/ROS viene riesaminato e riprogettato.

### Riconoscimento dei nominativi

- Aggiunta una validazione condivisa e più rigorosa dei nominativi decodificati, usata in modo coerente dalle diverse viste.
- Corretti i nominativi speciali indonesiani con prefisso `8A`, `8B` e `8D`, compresi esempi quali `8A3B`, `8B8FTDM` e `8D8DADA`.
- I nominativi validi composti casualmente soltanto da caratteri esadecimali non vengono più scartati come payload telemetrici per il solo modo in cui sono scritti.
- Aggiunto il supporto ai suffissi portatili numerici da `/0` a `/9`: ad esempio `IZ1ABC/0` e `IZ1ABC/1` restano nominativi validi, mentre l'entità DXCC viene risolta dal nominativo base.
- I veri payload telemetrici esadecimali non risolti continuano ad essere scartati.

### Attribuzione DXCC corretta

- Nei messaggi diretti vengono ora usati semanticamente il nominativo a destra e la sua entità per DXCC, stato worked-before e LoTW.
- Se la stazione a destra è invalida o non risolta, il campo DXCC resta vuoto invece di usare erroneamente la stazione scritta a sinistra.
- Corretto il caso segnalato `IZ1JIZ VU33IN RR73`: viene selezionato `VU33IN` e visualizzata **India**, senza attribuire la riga all'Italia.
- Confermata l'assegnazione di `8A3B`, `8B8FTDM` e `8D8DADA` all'**Indonesia** usando il vero `cty.dat` distribuito con Decodium.

### Verifica

- Il target completo dell'applicazione `decodium_qml` viene compilato correttamente su macOS Apple Silicon.
- I test del parser QSO coprono nominativi indonesiani e indiani, suffissi portatili numerici, stazioni destre sconosciute e il divieto di ripiegare sul nominativo a sinistra.
- I test DXCC usano sia dati controllati sia il `cty.dat` reale incluso nel programma.
- I test della protezione telemetrica QMX continuano ad essere superati dopo il rollback della v1.0.552.
- Questa release non modifica panadapter, waterfall, DSP di decodifica o catena audio.
