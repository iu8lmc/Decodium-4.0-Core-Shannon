# Decodium 4 v1.0.696

## English (British)

Decodium 4 v1.0.696 is a stability and usability release following v1.0.695. It improves Q65 operation, CW mode transitions, remote DecoPort reception, light-theme readability, the selectable Apple Liquid Glass presentation and TX audio/ALC calibration safety.

### Changes since 1.0.695

- **ALC auto-calibration:** corrected the attenuation feedback direction. When measured ALC is above the target, Decodium now increases attenuation and reduces audio drive instead of increasing it. The correction is clamped to the actual `0–450` attenuation range, and `max-drive`/`min-drive` limit reporting now follows the physical meaning of that range.
- **ALC zero readings:** a valid ALC reading close to zero is no longer treated as a stalled calibration state. Hamlib telemetry marked as valid can therefore continue the calibration normally.
- **ALC regression coverage:** added focused tests for feedback direction, zero ALC, bounds and both drive limits. The implementation is shared by the production calibration path and the testable calibration arithmetic.
- **Q65:** added explicit Q65 period/submode handling for the `Q65-30A`, `Q65-60A`, `Q65-60B` and related labels. The selected period and A–E submode now reach the native decoder and TX waveform instead of silently falling back to the generic Q65 path.
- **Remote DecoPort reception:** remote PCM is now routed through the bridge-owned decoder path instead of the local legacy audio capture path. This prevents an audible remote stream from being paired with an empty local decoder or waterfall.
- **DecoPort CAT frequency control:** remote frequency changes now reuse the normal CAT request path when a direct DecoPort rig driver is not available, keeping Hamlib, TCI, OmniRig and fallback backends consistent.
- **CW and CAT transitions:** the physical CAT mode is captured on CW entry and restored when leaving the CW workspace, including a delayed reassertion after CAT/QSY settling where required by the radio.
- **Audio recovery:** mode-change audio reopening is queued after the old Qt audio source has been torn down. Superseded reopen requests are discarded, reducing race conditions and crash risk when switching between CW and digital modes.
- **Light-theme readability:** improved accessible decode colours and contrast for Stellar Light, including message, SNR, DXCC and LoTW-related text. Panadapter/waterfall and top-dashboard typography were also made more legible at compact sizes.
- **Apple Liquid Glass theme:** added the selectable Apple-style Liquid Glass presentation with rounded translucent surfaces and a controlled opaque fallback on systems without native material support; the classic themes remain available.
- **CW interface clarity:** labelled the CW receive-sideband selector and clarified that `CW-U/CW-L` controls decoder polarity and centring, not the CAT mode sent to the radio.
- **Settings recovery:** added owner-permission repair for user settings files that remain readable but cannot be saved after being copied or created with unsuitable permissions.
- **LiveMap/roster consistency:** kept digital-mode filtering aligned with the supported mode vocabulary used by the map intelligence and roster queries.

### Verification

- macOS build completed successfully with the local Qt/Hamlib toolchain.
- ALC calibration regression test passed.
- TX pipeline, PTT transition, audio-sink gate and ALC tests passed: 4/4.
- Decodium diagnostic startup reached the QML event loop without QML errors.
- Physical RF ALC calibration on an FT-991 was not performed locally; use a suitable dummy load and a safe power level for the first real calibration.

## Italiano

Decodium 4 v1.0.696 è una release di stabilità e usabilità successiva alla v1.0.695. Migliora il funzionamento Q65, le transizioni tra CW e CAT, la ricezione remota DecoPort, la leggibilità del tema chiaro, la nuova presentazione Apple Liquid Glass e la sicurezza della calibrazione TX audio/ALC.

### Modifiche dalla 1.0.695

- **Calibrazione automatica ALC:** corretta la direzione del feedback sull’attenuazione. Quando l’ALC misurato supera il target, Decodium ora aumenta l’attenuazione e riduce il drive audio invece di aumentarlo. La correzione è limitata all’intervallo reale `0–450` e la classificazione dei limiti `max-drive`/`min-drive` ora riflette il significato fisico dell’intervallo.
- **Letture ALC a zero:** una lettura ALC vicina a zero ma valida non viene più interpretata come calibrazione bloccata. La telemetria Hamlib dichiarata valida può quindi continuare normalmente la calibrazione.
- **Test ALC:** aggiunti test mirati per direzione del feedback, ALC zero, limiti numerici e condizioni di massimo/minimo drive. La matematica usata dalla calibrazione di produzione è condivisa con il codice testabile.
- **Q65:** aggiunta la gestione esplicita di periodo e submode Q65 per etichette come `Q65-30A`, `Q65-60A`, `Q65-60B` e varianti correlate. Il periodo e il submode A–E selezionati arrivano ora al decoder nativo e alla forma d’onda TX, senza ricadere silenziosamente nel percorso Q65 generico.
- **Ricezione remota DecoPort:** il PCM remoto viene ora inviato al decoder gestito dal bridge invece di essere trattato dalla cattura audio legacy locale. Si evita così di avere audio remoto udibile ma decoder o waterfall locali vuoti.
- **Controllo frequenza CAT DecoPort:** quando il driver radio diretto DecoPort non è disponibile, il cambio frequenza remoto usa il normale percorso CAT, mantenendo coerenti Hamlib, TCI, OmniRig e i fallback.
- **Transizioni CW e CAT:** il modo fisico CAT viene salvato entrando nell’ambiente CW e ripristinato uscendo dal CW, con una nuova riaffermazione ritardata dopo l’assestamento CAT/QSY quando richiesta dalla radio.
- **Recupero audio:** la riapertura dell’audio dopo un cambio modo viene accodata dopo la chiusura della vecchia sorgente Qt. Le richieste superate vengono scartate, riducendo race condition e rischio di crash passando da CW ai modi digitali.
- **Leggibilità tema chiaro:** migliorati colori accessibili e contrasto dei decode in Stellar Light, inclusi testo messaggi, SNR, DXCC e LoTW. Migliorata anche la leggibilità di panadapter/waterfall e della dashboard superiore alle dimensioni compatte.
- **Tema Apple Liquid Glass:** aggiunta la presentazione selezionabile in stile Apple Liquid Glass, con superfici arrotondate e traslucenza controllata e un fallback opaco sui sistemi senza supporto nativo ai materiali; i temi classici restano disponibili.
- **Chiarezza interfaccia CW:** il selettore della sideband di ricezione CW ora è etichettato e chiarisce che `CW-U/CW-L` regola polarità e centratura del decoder, non il modo CAT inviato alla radio.
- **Recupero impostazioni:** aggiunta la riparazione dei permessi del proprietario per i file di configurazione leggibili ma non più salvabili dopo copia o creazione con permessi inadatti.
- **Coerenza LiveMap/roster:** mantenuti coerenti i filtri dei modi digitali con il vocabolario supportato dalle query di map intelligence e roster.

### Verifica

- Build macOS completata con la toolchain Qt/Hamlib locale.
- Test di regressione della calibrazione ALC superato.
- Test pipeline TX, transizione PTT, audio-sink e ALC superati: 4/4.
- Avvio diagnostico di Decodium arrivato all’event loop QML senza errori QML.
- La calibrazione RF ALC reale su FT-991 non è stata eseguita localmente; per il primo test reale usare un carico fittizio e una potenza sicura.

## Downloads

The GitHub release source archive, Windows x64 installer EXE, macOS Intel and Apple Silicon DMGs, and Linux x86_64 and aarch64 AppImages are published by the corresponding release workflows, with checksums where provided.
