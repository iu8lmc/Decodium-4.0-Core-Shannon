# Decodium 4 v1.0.689

This release adds DecoPort, a way to share one radio between several programs, and a few performance and low-end PC changes.

## DecoPort: share the radio

- **Radio detector.** Finds the serial ports of connected radios by their device identity, skips virtual ports, and reports which ones are actual radios.
- **CAT sharing.** Decodium can share its CAT connection with other programs using the radio's own protocol. Hamlib 4.7 clients are supported: `\get_lock_mode` and `q` now answer as the protocol expects, so `set_mode` no longer waits about 20 seconds.
- **Virtual COM ports.** Uses com0com 3.0.0.0 (GPL 2 or later), installed with Decodium under `com0com` in the application folder. Its license and provenance are in `third_party/com0com/NOTICE.md`.
- **Radio library.** Built from Hamlib 4.7.0 and shipped with Decodium.

## Performance and low-end PC

- **Low-end mode.** Turning on "PC lento" now switches the interface quality to Low, and turning it off restores the previous quality.
- **Waveform scope.** The tuning scope in RTTY stops redrawing while it is hidden.
- **GUI metrics (optional).** With `DECODIUM_GUI_METRICS=1`, the diagnostic log records the CPU use of the GUI thread and of the process every 10 seconds.

## Notes

- DecoPort has been compiled and its driver test passes, but it has not been tested with a real radio on a shared port.
- Use a dummy load for the first transmissions of the voice keyer, as in 1.0.686.

## Italiano

Questa release aggiunge DecoPort, un modo per condividere una sola radio tra più programmi, e alcune modifiche di prestazioni per PC modesti.

### DecoPort: condividere la radio

- **Rilevatore radio.** Trova le porte seriali delle radio collegate tramite l'identità del dispositivo, scarta le porte virtuali e indica quali sono radio vere.
- **Condivisione CAT.** Decodium può condividere la propria connessione CAT con altri programmi usando il protocollo della radio. Sono supportati i client Hamlib 4.7: `\get_lock_mode` e `q` ora rispondono come il protocollo prevede, quindi `set_mode` non attende più circa 20 secondi.
- **Porte COM virtuali.** Usa com0com 3.0.0.0 (GPL 2 o successiva), installato con Decodium nella cartella `com0com` dell'applicazione. Licenza e provenienza sono in `third_party/com0com/NOTICE.md`.
- **Libreria radio.** Generata da Hamlib 4.7.0 e distribuita con Decodium.

### Prestazioni e PC modesti

- **Modalità PC lento.** Attivandola la qualità dell'interfaccia passa a Low; disattivandola si ripristina la qualità precedente.
- **Scope di sintonia.** Lo scope RTTY smette di ridisegnarsi quando è nascosto.
- **Metriche GUI (facoltative).** Con `DECODIUM_GUI_METRICS=1` il log diagnostico registra ogni 10 secondi l'uso di CPU del thread GUI e del processo.

### Note

- DecoPort è compilato e il suo test del driver passa, ma non è ancora stato provato con una radio vera su una porta condivisa.
- Per le prime trasmissioni del voice keyer usa un carico fittizio, come nella 1.0.686.

## Downloads

The Windows x64 installer EXE is attached to this release.
