# Decodium 4 FT2 1.0.273

This release consolidates the fixes developed after 1.0.272. It focuses on CAT data-mode stability for Ham Radio Deluxe and OmniRig, special-event callsign handling, FT2 async decode visibility, SHF band access, frequency-control ergonomics, and several UI regressions found during field testing.

## CAT, HRD, And Data Mode

- Improves Ham Radio Deluxe handling for Icom radios configured as Data/Packet / USB-D.
- Avoids writing the plain carrier-mode fallback back into the HRD Mode dropdown when the radio is already on the correct USB/LSB carrier for a data mode.
- Reasserts the HRD Data control before a carrier change only when the carrier really needs to move, reducing the visible USB -> USB-D bounce during TX, RX return, mode changes, and band changes.
- Applies the safer HRD mode-write helper to RX/TX VFO mode changes, split/emulated split handling, and direct mode changes.
- Adds HRD diagnostics for skipped carrier-equivalent writes so future tap logs show when Decodium intentionally leaves the dropdown untouched.

## OmniRig And Kenwood Data/Packet

- Fixes OmniRig mode interpretation so `PM_DIG_U` and `PM_DIG_L` take priority over `PM_SSB_U` and `PM_SSB_L` when both flags are exposed by a rig definition.
- Prevents the modern OmniRig backend from being shadowed by the embedded legacy rig-control path.
- Disables legacy embedded mode overrides while OmniRig owns CAT, avoiding a second `PM_DIG_U`/mode assertion that can map Kenwood rigs such as TS-890/TS-590 into FSK or plain USB.
- Keeps application mode and radio transport mode more clearly separated when OmniRig is selected.

## Special-Event Callsigns

- Adds standard-call detection and hashed-call formatting for FT8/FT4/FT2 message generation.
- Protects special or non-standard callsigns such as `II9MESC` from being truncated to a six-character standard-call form.
- Regenerates TX1-TX6 with the safe hashed-call form when either operator callsign cannot be represented as a normal 77-bit standard call.
- Adds regression coverage proving that the unsafe standard form truncates while the new hashed-call messages round-trip correctly.

## FT2 Async Decode UI

- Keeps exact FT2 async UI de-duplication inside the same FT2 slot, using time/frequency bucket/message keys.
- Removes the previous visible-list burst de-duplication that could suppress valid repeated decodes across later FT2 slots.
- Leaves the more aggressive de-duplication in the auto-sequence path where it is still needed for anti-loop protection.
- Makes FT2 async logging clearer: empty polling attempts are logged as empty attempts, while only a completed slot with no rows is logged as a zero-decode slot.

## SHF And Higher Bands

- Extends the default band table beyond HF/VHF/UHF to include 1.25 m, 33 cm, 23 cm, 13 cm, 9 cm, 6 cm, 3 cm, and 1.25 cm.
- Adds mode-specific nominal frequencies for FT8, FT2, FT4, WSPR/FST4W, JT65, JT9, JT4, Q65, Echo, and MSK144 where available.
- Uses the current application mode when selecting a band and falls back to the first available frequency only when that mode has no nominal QRG on the selected band.
- Exposes SHF band buttons in the band selector and expands toolbar band buttons so longer labels fit.
- Extends Decode History band filters to include the new microwave/SHF bands.

## Frequency Control

- Replaces the main frequency readout with clickable digit cells inspired by SDR++ style tuning.
- Clicking the upper half of a digit increments that digit's frequency step.
- Clicking the lower half decrements that digit's frequency step.
- Adds hover feedback on each digit so the active increment/decrement area is visible before clicking.
- Routes digit changes through `qsyTo()` so the existing band/CAT synchronisation path remains in control.

## UI Fixes

- Enlarges and restyles the footer `Layout` and `History` buttons so icons and text no longer overflow their borders.
- Fixes the footer Reset Layout confirmation dialog binding loop by giving it an explicit width.
- Makes the Prompt To Log accept/reject buttons visible and styled consistently on Windows even when the selected Qt Quick Controls theme is not Material.
- Keeps the prompt buttons at explicit dimensions with custom text/background rendering instead of relying on theme defaults that may hide the text.

## Test Coverage

- Adds a Qt helper regression test for special-event/non-standard callsign message generation.
- Verifies safe encoded/decoded message sequences for hashed special calls across TX1, TX2, TX3, TX4, TX5, and TX6-style messages.

## Italiano

Questa release raccoglie le modifiche successive alla 1.0.272. I punti principali sono stabilita' del modo dati con Ham Radio Deluxe e OmniRig, nominativi speciali, FT2 async, bande SHF, controllo frequenza e fix UI.

- HRD: ridotto il passaggio visibile da USB-D a USB normale e ritorno durante TX, cambio banda, cambio modo e ritorno in RX.
- HRD: Decodium evita di riscrivere il fallback USB/LSB quando la portante e' gia' corretta per il modo dati.
- OmniRig: priorita' corretta ai flag DIG_U/DIG_L rispetto a SSB_U/SSB_L.
- OmniRig/Kenwood: disattivata la strada legacy parallela che poteva riportare TS-890/TS-590 in FSK o USB normale.
- Nominativi speciali: messaggi FT8/FT4/FT2 generati con forma hashed quando serve, evitando troncamenti come `II9MESC` -> `II9MES`.
- FT2 async: dedupe visibile limitata allo stesso slot; le ripetizioni valide negli slot successivi tornano visibili.
- FT2 async: log piu' chiari tra tentativo vuoto e slot completato senza decode.
- Bande: aggiunte 1.25 m, 33 cm, 23 cm, 13 cm, 9 cm, 6 cm, 3 cm e 1.25 cm con frequenze nominali per i modi disponibili.
- UI: aggiunti pulsanti SHF e filtri Decode History per le nuove bande.
- Frequenza: display principale cliccabile per cifra, con incremento sulla meta' superiore e decremento sulla meta' inferiore.
- UI: pulsanti `Layout` e `History` nel footer ingranditi.
- UI: corretti i pulsanti del Prompt To Log sui temi Windows non Material.
- UI: eliminato il binding loop del dialog Reset Layout nel footer.

## Artifacts

- Windows x64 installer
- macOS Apple Silicon DMG/ZIP
- Linux x86_64 AppImage built with Qt 6.11
