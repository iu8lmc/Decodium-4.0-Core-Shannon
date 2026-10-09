# Decodium 4 FT2 v1.0.540

## English (UK)

### UDP destination traffic filters

Added independent traffic filters for the primary, secondary and tertiary UDP destinations. Each destination can now selectively forward:

- Decode packets
- Status packets
- Logged QSO packets
- WSPR packets

All four filters are enabled by default, preserving the existing behaviour for JTAlert, GridTracker and other WSJT-X-compatible consumers. The controls are available under **Setup → Reporting → Network Services**.

The legacy backend now applies the same policy as the modern QML backend. A destination configured for **Logged QSO only** no longer creates a persistent real-time MessageClient, so it does not emit unwanted heartbeats, status packets or decoded traffic. When a QSO is committed, exactly one raw ADIF datagram is sent to that destination. Existing ADIF settings remain migration fallbacks for installations that have not yet saved the new controls.

WSPR reporting now follows the dedicated WSPR packet path instead of being mixed with ordinary decode traffic. Per-destination client identifiers are retained for the primary, secondary and tertiary paths.

### Windows graphics startup resilience

Improved the Windows Qt Quick graphics fallback chain and startup supervision. The application can now recover progressively from graphics initialisation failures using D3D12, D3D11, D3D11 WARP and the Qt software renderer, with persistent fallback state and clearer diagnostics. Low-end mode uses the conservative D3D11 hardware backend on Windows, while explicit software-renderer options remain available for troubleshooting.

### UI and validation

- Corrected the CAT poll-interval layout in the settings panel.
- Updated the slow-PC tooltip to describe the Windows D3D11 behaviour.
- Built the Qt/QML target successfully.
- Passed `test_tx_pipeline` and `test_udp_client_id`.
- Intercepted live UDP traffic to verify that QSO-only destinations remain silent until a QSO is logged, then receive one ADIF record.

## Italiano

### Filtri del traffico per destinazione UDP

Sono stati aggiunti filtri indipendenti per le destinazioni UDP primaria, secondaria e terziaria. Per ogni destinazione è ora possibile scegliere separatamente se inviare:

- pacchetti Decode;
- pacchetti Status;
- QSO registrati;
- pacchetti WSPR.

I quattro filtri sono attivi per impostazione predefinita, quindi il comportamento resta compatibile con JTAlert, GridTracker e gli altri client compatibili con WSJT-X. I controlli si trovano in **Setup → Reporting → Network Services**.

Anche il backend legacy applica ora la stessa politica del backend moderno QML. Una destinazione configurata per ricevere **solo i QSO registrati** non crea più un MessageClient persistente per il traffico realtime e quindi non genera heartbeat, status o decodifiche indesiderate. Quando un QSO viene registrato, quella destinazione riceve esattamente un datagramma ADIF grezzo. Le vecchie impostazioni ADIF restano utilizzate come fallback di migrazione per le installazioni che non hanno ancora salvato i nuovi controlli.

L'invio WSPR utilizza ora il percorso dedicato ai pacchetti WSPR, invece di essere mescolato al normale traffico Decode. Sono mantenuti identificativi UDP distinti per i percorsi primario, secondario e terziario.

### Maggiore affidabilità dell'avvio grafico su Windows

È stata migliorata la catena di fallback grafico Qt Quick su Windows, con supervisione dell'avvio. In caso di errore di inizializzazione, l'applicazione può passare progressivamente da D3D12 a D3D11, D3D11 WARP e infine al renderer software Qt, conservando lo stato del fallback e producendo diagnostica più chiara. La modalità PC lento usa il backend hardware D3D11 più conservativo; restano disponibili le opzioni software esplicite per la diagnosi.

### Interfaccia e verifiche

- Corretto l'allineamento del campo CAT Poll Interval nelle impostazioni.
- Aggiornato il suggerimento della modalità PC lento con il comportamento D3D11 su Windows.
- Compilato con successo il target Qt/QML.
- Superati `test_tx_pipeline` e `test_udp_client_id`.
- Intercettato traffico UDP reale per verificare che le destinazioni “solo QSO” restino silenziose fino alla registrazione del QSO e ricevano poi un solo record ADIF.
