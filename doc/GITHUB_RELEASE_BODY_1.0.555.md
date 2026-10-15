# Decodium 4 FT2 v1.0.555

This release makes the on-air state truthful and fail-safe, extends logbook and map intelligence for multi-locator QSOs, improves Linux AppImage integration with desktop services, and adds a complete mouse context menu to the QSO comment field.


> **Why 1.0.555 and not 1.0.554.** This fork and elisir80 had both used the
> number 1.0.554 for different builds, and an operator had already installed
> one believing it was the other. From here on this fork numbers ahead, so a
> version number identifies one build and one only. The contents are those of
> our 1.0.554, unchanged.

> **Perche' 1.0.555 e non 1.0.554.** Questo fork ed elisir80 avevano usato
> entrambi il numero 1.0.554 per build diverse, e un operatore ne aveva gia'
> installata una credendola l'altra. Da qui in avanti questo fork numera piu'
> avanti, cosi' un numero di versione identifica una build e una sola. Il
> contenuto e' quello della nostra 1.0.554, invariato.

## English (British)

### TX/PTT state and safety

- Separated **TX requested**, **PTT pending** and **PTT confirmed** into distinct states.
- Added an amber pending indication while Decodium waits for PTT acknowledgement. The interface, panadapter and status bar turn red and display **TX ON AIR** only after PTT has actually been confirmed.
- On the macOS bridge-managed digital-audio path, audio playback, TX timing and the transmitted-message row now start only after confirmed PTT, rather than treating the legacy engine's requested state as proof that the radio is transmitting.
- Added a non-blocking 650 ms confirmation timeout for Hamlib-controlled PTT. If confirmation does not arrive, the pending transmission is cancelled safely, audio is stopped, the fault is reported and the message is rescheduled for the next valid slot without losing the QSO sequence.
- Eliminated duplicate PTT-OFF dispatches and made transition clean-up idempotent, reducing unnecessary CAT traffic and relay activity.
- Kept VOX and direct DTR/RTS operation on separate policies because those methods do not always provide queryable PTT feedback.
- The transition logic uses asynchronous signals and timers only; no blocking wait has been introduced into the user interface, waterfall or panadapter paths.
- Existing Windows and Linux transmission execution paths remain unchanged unless they use the new confirmation-capable transition path.

### Multi-locator logbook and map intelligence

- Added complete import and validation of every locator contained in the ADIF `VUCC_GRIDS` field.
- Added a normalised secondary-locator table, with invalid values rejected and duplicates removed against both the primary `GRIDSQUARE` and other secondary locators.
- When `GRIDSQUARE` is absent, the first valid locator from `VUCC_GRIDS` becomes the primary locator.
- Worked, confirmed and VUCC award calculations now consider all valid locators associated with a QSO.
- Logbook search and cell history now expose the complete list of additional locators.
- Distance, azimuth and the geographical QSO path continue to use only the primary locator, preventing artificial multi-path lines on the map.
- Live data and call-roster details now show multiple observation sources, source count and corroboration level when the same station is supported by more than one source.

### Linux AppImage and desktop integration

- Centralised the clean host environment used by external secure-storage operations on Linux.
- Removed AppImage-provided library preloads, library paths and GLib/GIO plug-in paths before invoking the system `secret-tool`, while preserving the D-Bus session, runtime directory and X11/Wayland display variables required to reach the user's keyring.
- Applied the same environment handling to secure-value read, save and clear operations.
- Added automated coverage with a simulated external `secret-tool` to verify the exact environment it receives.
- Improved AppImage host helper packaging and launching so desktop services are reached through the host system rather than through incompatible bundled libraries.
- External links are now opened asynchronously; Decodium checks the host opener's result and falls back to Qt desktop services when required.

### QSO logging usability

- Added a themed right-click menu to the QSO **Comment** field with **Cut**, **Copy**, **Paste** and **Select All**.
- Menu text, enabled states and colours remain readable with Decodium's dark theme on macOS, Windows and Linux.

### Packaging and validation

- Retained the Intel macOS packaging-only compatibility flag needed for Boost 1.85, without weakening warning policy in the application source build.
- Built the complete `decodium_qml` target successfully on macOS Apple Silicon.
- Passed the focused TX pipeline, PTT transition, multi-locator map and Linux secure-settings tests.
- Checked the release diff for whitespace errors and validated the repository packaging contract.
- No decoder DSP, audio processing, normal waterfall rendering or normal panadapter rendering algorithm is changed by this release.

---

## Italiano

Questa release rende veritiero e sicuro lo stato di trasmissione, estende logbook e intelligenza della mappa per i QSO con locator multipli, migliora l'integrazione delle AppImage Linux con i servizi del desktop e aggiunge un menu mouse completo al campo Comment del QSO.

### Stato e sicurezza TX/PTT

- Separati gli stati **TX richiesto**, **PTT in attesa** e **PTT confermato**.
- Aggiunta un'indicazione ambra durante l'attesa della conferma PTT. Interfaccia, panadapter e barra di stato diventano rossi e mostrano **TX ON AIR** soltanto dopo la reale conferma del PTT.
- Nel percorso macOS con audio digitale gestito dal bridge, riproduzione audio, temporizzazione TX e riga del messaggio trasmesso iniziano soltanto dopo il PTT confermato; lo stato richiesto dal motore legacy non viene più considerato prova di una trasmissione reale.
- Aggiunto per il PTT controllato da Hamlib un timeout asincrono di 650 ms. Se la conferma non arriva, la trasmissione pendente viene annullata in sicurezza, l'audio viene fermato, l'errore viene segnalato e il messaggio viene riprogrammato nel successivo slot valido senza perdere la sequenza del QSO.
- Eliminati gli invii duplicati di PTT-OFF e resa idempotente la pulizia della transizione, riducendo traffico CAT e attività non necessaria dei relè.
- VOX e controllo diretto DTR/RTS mantengono politiche separate, perché questi metodi non forniscono sempre un feedback PTT interrogabile.
- La transizione usa esclusivamente segnali e timer asincroni: non è stata introdotta alcuna attesa bloccante nell'interfaccia, nel waterfall o nel panadapter.
- I percorsi di trasmissione esistenti per Windows e Linux restano invariati, salvo quando utilizzano il nuovo percorso capace di confermare il PTT.

### Logbook multi-locator e intelligenza della mappa

- Aggiunta l'importazione e la validazione completa di tutti i locator presenti nel campo ADIF `VUCC_GRIDS`.
- Aggiunta una tabella normalizzata per i locator secondari: i valori invalidi vengono scartati e i duplicati vengono eliminati rispetto sia al `GRIDSQUARE` principale sia agli altri locator secondari.
- Quando `GRIDSQUARE` è assente, il primo locator valido di `VUCC_GRIDS` diventa il locator principale.
- I conteggi worked, confirmed e gli award VUCC considerano ora tutti i locator validi associati al QSO.
- La ricerca nel logbook e la cronologia delle celle mostrano l'elenco completo dei locator aggiuntivi.
- Distanza, azimut e percorso geografico del QSO continuano a usare soltanto il locator principale, evitando linee geografiche artificiali sulla mappa.
- I dettagli dei dati live e del roster mostrano fonti multiple, numero delle fonti e livello di corroborazione quando la stessa stazione è supportata da più osservazioni.

### AppImage Linux e integrazione con il desktop

- Centralizzato l'ambiente host pulito utilizzato dalle operazioni di archiviazione sicura su Linux.
- Prima di eseguire il `secret-tool` di sistema vengono rimossi preload, percorsi delle librerie e percorsi dei plug-in GLib/GIO provenienti dall'AppImage, conservando però sessione D-Bus, directory runtime e variabili display X11/Wayland necessarie per raggiungere il portachiavi dell'utente.
- La stessa gestione dell'ambiente viene applicata a lettura, salvataggio e cancellazione dei valori protetti.
- Aggiunto un test automatico con un `secret-tool` esterno simulato per verificare esattamente l'ambiente ricevuto.
- Migliorati il packaging e l'avvio degli helper host dell'AppImage, così i servizi desktop vengono raggiunti tramite il sistema host e non tramite librerie incluse incompatibili.
- I collegamenti esterni vengono ora aperti in modo asincrono; Decodium verifica l'esito dell'apertura host e, se necessario, usa i servizi desktop Qt come fallback.

### Usabilità del log QSO

- Aggiunto al campo **Comment** del QSO un menu contestuale coerente con il tema, accessibile con il pulsante destro e dotato di **Taglia**, **Copia**, **Incolla** e **Seleziona tutto**.
- Testi, stati abilitati e colori del menu restano leggibili con il tema scuro di Decodium su macOS, Windows e Linux.

### Packaging e verifiche

- Mantenuta esclusivamente nel packaging macOS Intel l'opzione di compatibilità necessaria per Boost 1.85, senza indebolire la gestione dei warning nella compilazione del codice applicativo.
- Compilato con successo il target completo `decodium_qml` su macOS Apple Silicon.
- Superati i test mirati della pipeline TX, della transizione PTT, della mappa multi-locator e delle impostazioni sicure Linux.
- Controllato il diff della release per errori di spaziatura e validato il contratto di packaging del repository.
- Questa release non modifica DSP di decodifica, elaborazione audio, rendering normale del waterfall o rendering normale del panadapter.

---

## In this fork (iu8lmc)

### The automatic noise threshold, and what it does to the 3D spectrum

Reported by an operator: with the automatic threshold on, the 3D waterfall
loses its traces; with it off, the spectrum is there. Both halves were real.

**The threshold had been quietly made harsher.** Since 1.0.495 the noise
estimate had moved from the 10th to the 25th percentile — a quarter of the
spectrum declared noise and cut away instead of a tenth — and the smoothing
had gone from 0.03 to 0.08, so the floor chases the signal four times faster
and the cut moves while you watch. Both are back to the 1.0.495 values.

**And it is now yours to set.** A slider next to the Auto box, 5 to 40,
default 10. Raise it to clean up an empty band, lower it when weak signals
vanish. It appears only while the threshold is on, and the value is
remembered. The right setting is not the same for everyone: it depends on the
band, the receiver, and what you are looking for.

**The 3D ridges were measured on the wrong scale.** With the automatic
threshold the dB window anchors at the noise floor and runs 80 dB upwards,
where nothing lives; the 3D normalised the ridge height over all of it. A
signal 10 dB above the noise raised its trace by 0.8% of the available
height — invisible. Now the height is measured over the range the signals
actually occupy: that same signal reaches 11%, one 15 dB up reaches 46%, and
the strongest in the band reaches full scale. The scale never drops below
18 dB, so an empty band stays flat instead of being inflated into a
landscape. All three renderers — the two CPU paths and the shader — were
corrected to agree; the shader lost two texture reads per vertex it no
longer needs.

### Translations

Ten new strings in all fifteen languages, with zero unfinished messages: the
threshold slider, the three strings of the new map work, the logbook line,
and the four entries of the TX panel context menu — copy, cut, paste, select
all — which had been in English since they were written.

