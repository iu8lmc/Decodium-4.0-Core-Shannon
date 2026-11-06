# Decodium 1.0.714

## English (UK)

This release consolidates the changes made since 1.0.713.

### CW and CAT

- Clarified the CW window control previously labelled “Center tone”. It is now labelled “Center target (RX)”.
- Added an always-visible explanation that this value is the received-audio target used by “Center CW”; it does not read or change the physical radio’s CW pitch or sidetone through CAT.
- Added regression coverage for radios using 600 Hz, 650 Hz and 700 Hz targets. Changing the target is local and does not retune or key the radio; only “Center CW” performs the explicit VFO adjustment.
- Preserved the CW CAT/keyer, shared-CAT, macro, timing and stop-path improvements already present in the 1.0.713 worktree.

### Linux graphics recovery

- Extended Linux graphics-loss detection to recognise Qt Quick/QRhi device-loss messages as well as the driver’s `VK_ERROR_DEVICE_LOST` diagnostic.
- Persisted safe software-rendering recovery remains available after an NVIDIA/NVK device loss, while explicit user graphics choices are respected.
- Added regression coverage for repeated Qt-only device-loss sequences and for failed graphics-cache promotion.

### Validation

- macOS arm64 local build completed successfully.
- Focused CW, QML, Linux graphics recovery, TX-dispatch and FT8 auto-TX regression suites passed.
- No real-radio transmission was performed during automated validation.

## Italiano

Questa release consolida le modifiche effettuate dalla versione 1.0.713.

### CW e CAT

- Chiarito il controllo della finestra CW precedentemente chiamato “Center tone”. Ora è indicato come “Tono centratura (RX)”.
- Aggiunta una spiegazione sempre visibile: il valore è il riferimento della frequenza audio ricevuta usato da “Centra CW”; non legge e non modifica tramite CAT il pitch o il sidetone CW fisico della radio.
- Aggiunti test di regressione per riferimenti a 600 Hz, 650 Hz e 700 Hz. Cambiare il riferimento è un’operazione locale e non risintonizza né mette in trasmissione la radio; solo “Centra CW” esegue lo spostamento VFO esplicito.
- Conservati i miglioramenti CW già presenti nel worktree 1.0.713 relativi a CAT/keyer, CAT condivisa, macro, temporizzazione e arresto della trasmissione.

### Recupero grafico Linux

- Esteso il riconoscimento della perdita della GPU Linux ai messaggi di Qt Quick/QRhi, oltre alla diagnostica del driver `VK_ERROR_DEVICE_LOST`.
- Il recupero persistente con rendering software resta disponibile dopo una perdita del dispositivo NVIDIA/NVK, rispettando comunque le scelte grafiche esplicite dell’utente.
- Aggiunti test di regressione per sequenze ripetute di perdita GPU segnalate solo da Qt e per impedire la promozione di cache grafiche non valide.

### Verifiche

- Build locale macOS arm64 completata con successo.
- Superate le suite mirate CW, QML, recupero grafico Linux, protezione TX e FT8 auto-TX.
- Durante le verifiche automatiche non è stata effettuata alcuna trasmissione reale sulla radio.
