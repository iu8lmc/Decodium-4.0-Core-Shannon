# Decodium 4 v1.0.695

## English (British)

Decodium 4 v1.0.695 improves the closing of FT2 QSOs started with AutoCQ and enables FT2 drift search by default for satellite work such as QO-100.

### Changes since 1.0.694

- FT2: during a QSO the AP decoding and the search for the expected reply now follow the frequency of the active partner's last message to you, instead of your fixed receive frequency. With AutoCQ the partner often answers on a different frequency, so a weak 73 or RR73 could be missed and the final message repeated. The frequency is remembered for two minutes and forgotten when the DX call changes.
- FT2: new option "FT2 drift search (satellites, QO-100)" in the FT2 settings, on by default. It lets the decoder also recover signals whose frequency drifts during the slot. It takes effect the next time Decodium starts, and a DECODIUM_FT2_DRIFT_SEARCH environment variable set by hand still takes precedence.

### Notes

- The drift search uses more CPU in the decoder; on a slow PC you can switch it off in the FT2 settings.
- Not yet verified on a real QO-100 transmission.
- Carries everything of 1.0.694, including com0com, Hamlib, the serial-port registry and the DecoPort port hub.

## Italiano

Decodium 4 v1.0.695 migliora la chiusura dei QSO FT2 avviati con AutoCQ e attiva di default la ricerca della deriva FT2 per i satelliti come QO-100.

### Modifiche dalla 1.0.694

- FT2: durante un QSO la decodifica AP e la ricerca della risposta attesa seguono la frequenza dell'ultimo messaggio che il corrispondente attivo ti ha mandato, non più la tua frequenza di ricezione fissa. Con l'AutoCQ il corrispondente risponde spesso su un'altra frequenza, quindi un 73 o RR73 debole poteva non essere decodificato e il messaggio finale veniva ripetuto. La frequenza viene ricordata per due minuti e dimenticata al cambio del nominativo DX.
- FT2: nuova opzione "FT2 drift search (satellites, QO-100)" nelle impostazioni FT2, attiva di default. Permette al decoder di recuperare anche i segnali la cui frequenza deriva durante lo slot. Vale dal prossimo avvio di Decodium; una variabile d'ambiente DECODIUM_FT2_DRIFT_SEARCH impostata a mano ha ancora la precedenza.

### Note

- La ricerca della deriva usa più CPU nel decoder; su un PC lento si può disattivare nelle impostazioni FT2.
- Non ancora verificata con una trasmissione reale su QO-100.
- Contiene tutto ciò che c'è nella 1.0.694, inclusi com0com, Hamlib, il registro delle porte seriali e il hub porte DecoPort.

## Downloads

The source archive, Windows x64 installer EXE, macOS Intel and Apple Silicon DMGs, and Linux x86_64 and aarch64 AppImages are attached to this release by the corresponding GitHub Actions workflows.
