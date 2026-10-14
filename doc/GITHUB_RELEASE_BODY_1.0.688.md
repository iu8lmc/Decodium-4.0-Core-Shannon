# Decodium 4.0 Core Shannon — v1.0.688

## English (British)

Release 1.0.688 is the Windows x64 build of the current `main` branch, which contains all the changes of 1.0.687 and a CI fix for the macOS release builds. It has no new user-facing features.

### Changes since 1.0.687

- CI: the macOS release workflows tolerate the Qt 6.11 deprecation warnings instead of failing on them.

### Included from 1.0.687

- Resizable SSB window with a two-column layout, asynchronous local voice synthesis and preview.
- Optional decode-provenance column (Full Spectrum and Signal RX), off by default, enabled from Setup > Decode.
- Audio/CAT diagnostic panel, non-destructive CAT and audio recovery actions, CAT profiles keep their audio devices.
- Guided setup with a clearer first-run radio detector; one-shot graphics reset.
- LiveMap roster filters (`New DXCC`, `New on band`, `New on band + mode`) and provider management.

### Notes

- The SSB voice keyer has not yet been verified on the air with a real radio: use a dummy load for the first transmissions.

## Italiano

La 1.0.688 è la build Windows x64 del ramo `main` corrente, che contiene tutte le modifiche della 1.0.687 e una correzione della CI per le build macOS. Non introduce nuove funzioni per l'utente.

### Modifiche rispetto alla 1.0.687

- CI: i workflow di release macOS tollerano gli avvisi di deprecazione di Qt 6.11 invece di fallire.

### Incluso dalla 1.0.687

- Finestra SSB ridimensionabile a due colonne, sintesi vocale locale e anteprima asincrone.
- Colonna opzionale della provenienza dei decode (Full Spectrum e Signal RX), disattivata di default, da attivare in Setup > Decode.
- Pannello diagnostico Audio/CAT, azioni di ripristino CAT e audio non distruttive; i profili CAT conservano le periferiche audio.
- Configurazione guidata con rilevamento radio più chiaro al primo avvio; reset grafico one-shot.
- Filtri roster LiveMap (`New DXCC`, `New on band`, `New on band + mode`) e gestione dei provider.

### Note

- Il voice keyer SSB non è ancora stato verificato in aria con una radio vera: per le prime trasmissioni usa un carico fittizio.

## Downloads

The Windows x64 installer EXE is attached to this release.
