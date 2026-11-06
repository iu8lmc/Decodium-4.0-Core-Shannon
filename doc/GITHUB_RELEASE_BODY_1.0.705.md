# Decodium 4 FT2 1.0.705

Changes from `1.0.704` to `1.0.705`.

## English (British)

### FT2 on legacy CPUs

- Adds a deliberately narrow compatibility path for FT2 machines where the SuperLDPC SIMD implementation is not available.
- The legacy path keeps BP, AP and validation passes, but avoids the expensive deep scalar OSD search that could occupy the FT2 worker for tens or hundreds of seconds.
- It applies a bounded compatibility budget and checks cancellation between short scalar decode calls, so old CPUs remain responsive and can resume decoding after a cancelled attempt.
- A valid result already found before the compatibility budget expires is retained; an explicit user cancellation still discards the result as before.
- The capability check is independent of the LDPC user setting. AVX2/NEON SuperLDPC-capable machines keep the existing SIMD path, search depth and performance unchanged.

### CW waterfall signal selection

- In CW, right-clicking a signal in the waterfall now selects that audio tone for the CW decoder, matching the practical signal-selection workflow available in established digital-mode applications.
- The selected tone is shown with an orange marker and a visible frequency label.
- A compact control reports the locked tone and returns the decoder to automatic tone search when clicked.
- The operation changes the receive audio selection only: it does not retune the radio and does not transmit.

### CAT mode safety and CW/SSB operation

- Strengthens the delayed verification used when leaving CW after CAT startup, preventing a late radio state report from silently restoring CW over the operator's requested mode.
- The reassertion is guarded by CAT generation, current mode, transmit/tune state and connection state, and the mode setter continues to force PTT off.
- Improves SSB microphone stream recovery with one bounded reopen attempt; a real microphone failure releases PTT and reports the failure instead of leaving the transmit path in an indeterminate state.
- Keeps the CW CAT Stop warning and transmission-safety behaviour introduced in the 1.0.704 line.

### FT8/CQ and settings persistence

- Preserves a complete operator-edited CQ message, including a custom or removed grid, instead of regenerating it unexpectedly after a QSO or restart.
- Saves and restores the custom CQ state per profile while retaining compatibility with older profiles that only stored TX6.
- Restores the standard generated CQ when the operator resets the standard messages.

### Linux window stability and verification

- Makes Linux Liquid Glass surfaces opaque by default to avoid compositor recursion and GPU saturation on affected Mesa/X11/Wayland combinations; an explicit environment opt-in remains available for known-good systems.
- Adds focused regression coverage for legacy FT2 timing/cancellation, custom CQ persistence, CW waterfall selection, CAT TX dispatch, SuperLDPC dispatch and liquid-glass behaviour.
- Local validation completed for the affected targets and focused tests, including `test_ft2_legacy_cpu`, `test_custom_cq_persistence`, `test_waterfall_signal_selection`, `test_cw_controller`, `test_cw_window`, `test_ssb`, `test_ssb_window_focus`, `test_tx_dispatch_guard`, `test_superldpc_dispatch` and `test_liquid_glass_windows`.
- `git diff --check` and the local QML/build checks pass. Hardware CAT/PTT and receive tests remain dependent on the operator's connected radio and audio devices.

### Release assets

- GitHub source archives are available automatically for tag `v1.0.705`.
- Windows x64 installer: `Decodium_1.0.705_Setup_x64.exe`.
- macOS Apple Silicon DMGs for Tahoe and Sequoia.
- macOS Intel DMGs for Ventura, Sonoma and Sequoia.
- Linux Qt 6.11 AppImages for x86_64 and aarch64.
- The macOS and Linux workflows also publish SHA-256 checksum files.

## Italiano

### FT2 sui computer meno recenti

- Aggiunge un percorso di compatibilità volutamente limitato per FT2 quando l'implementazione SIMD di SuperLDPC non è disponibile.
- Il percorso legacy conserva i passaggi BP, AP e di validazione, ma evita la costosa ricerca OSD profonda scalare che poteva occupare il worker FT2 per decine o centinaia di secondi.
- Applica un budget temporale limitato e controlla la cancellazione tra brevi chiamate scalari al decoder, così i vecchi processori restano reattivi e possono riprendere dopo un tentativo annullato.
- Un risultato valido trovato prima della scadenza del budget viene conservato; la cancellazione esplicita dell'utente continua invece ad annullare il risultato come prima.
- Il controllo della capacità hardware è indipendente dall'impostazione utente LDPC. Sui computer con SuperLDPC AVX2/NEON il percorso SIMD, la profondità di ricerca e le prestazioni esistenti restano invariati.

### Selezione del segnale CW dal waterfall

- In CW, un clic destro su un segnale nel waterfall seleziona ora quella tonalità audio per il decoder CW, rendendo pratico scegliere la stazione da demodulare.
- Il tono selezionato viene mostrato con un marcatore arancione e la relativa frequenza.
- Un controllo compatto indica il tono bloccato e, con un clic, riporta il decoder alla ricerca automatica.
- L'operazione modifica soltanto la selezione audio in ricezione: non cambia la frequenza della radio e non trasmette.

### Sicurezza CAT e funzionamento CW/SSB

- Rafforza la verifica ritardata usata quando si esce dal CW dopo l'avvio CAT, impedendo che una risposta tardiva della radio ripristini silenziosamente il CW sopra la modalità richiesta dall'operatore.
- La riapplicazione è protetta da generazione CAT, modalità corrente, stato TX/tune e stato della connessione; il cambio modalità continua inoltre a forzare il PTT disattivato.
- Migliora il recupero del flusso microfonico SSB con un solo tentativo di riapertura controllato; se il microfono è realmente guasto, il PTT viene rilasciato e l'errore viene segnalato.
- Mantiene l'avviso CAT del pulsante Stop CW e le protezioni di trasmissione introdotte nella linea 1.0.704.

### FT8/CQ e persistenza delle impostazioni

- Conserva il messaggio CQ completo modificato dall'operatore, compreso un grid personalizzato o rimosso, evitando che venga rigenerato inaspettatamente dopo un QSO o un riavvio.
- Salva e ripristina lo stato del CQ personalizzato per profilo, mantenendo la compatibilità con i vecchi profili che memorizzavano soltanto TX6.
- Il ripristino dei messaggi standard torna a generare il CQ predefinito.

### Stabilità delle finestre Linux e verifiche

- Rende opache per impostazione predefinita le superfici Liquid Glass su Linux, evitando ricorsioni del compositor e saturazione della GPU sulle combinazioni Mesa/X11/Wayland interessate; resta disponibile un opt-in tramite variabile d'ambiente per i sistemi verificati.
- Aggiunge test di regressione mirati per tempi/cancellazione FT2 legacy, persistenza CQ personalizzata, selezione CW dal waterfall, dispatch TX CAT, dispatch SuperLDPC e comportamento Liquid Glass.
- Sono stati completati localmente i target e i test interessati, inclusi `test_ft2_legacy_cpu`, `test_custom_cq_persistence`, `test_waterfall_signal_selection`, `test_cw_controller`, `test_cw_window`, `test_ssb`, `test_ssb_window_focus`, `test_tx_dispatch_guard`, `test_superldpc_dispatch` e `test_liquid_glass_windows`.
- `git diff --check` e i controlli locali di build/QML risultano superati. I test CAT/PTT hardware e quelli audio reali restano dipendenti dalla radio e dai dispositivi collegati dall'operatore.

### Asset della release

- Gli archivi del codice sorgente GitHub sono disponibili automaticamente per il tag `v1.0.705`.
- Installer Windows x64: `Decodium_1.0.705_Setup_x64.exe`.
- DMG macOS Apple Silicon per Tahoe e Sequoia.
- DMG macOS Intel per Ventura, Sonoma e Sequoia.
- AppImage Linux Qt 6.11 per x86_64 e aarch64.
- I workflow macOS e Linux pubblicano anche i file checksum SHA-256.
