# Decodium 4 FT2 1.0.274

This release focuses on FT8 weak-signal recovery after the 1.0.273 cycle. The investigation used synthetic FT8 frames at -24, -25, and -26 dB to separate candidate acquisition from LDPC/OSD recovery.

## FT8 Weak-Signal Decode

- Confirms that FT8 synthetic signals at -25/-26 dB already reached the sync candidate list at the correct frequency and DT.
- Moves the main FT8 fix away from `sync8var/getcandidates` and into the later bitmetrics, LDPC, OSD, and repeated-message recovery path.
- Adds a deep FT8 bitmetrics path inspired by WSJT-X/JTDX `ft8bvar` weak-signal handling.
- Applies a weak-signal transform when the Costas-derived `srr` is below the low-SNR threshold.
- Uses RMS normalisation for the deep weak-signal FT8 metrics, matching the behaviour expected by the weak transform.
- Keeps the existing standard FT8 bitmetrics path unchanged for normal decode depth and non-supplemental passes.

## LDPC / OSD

- Adds exact belief-propagation check-node updates for the FT8 deep profile.
- Uses the `tanh/platanh` update when the decoder is running with `maxosd=3` and `norder=4`.
- Keeps the existing normalised min-sum LDPC path for the lighter normal decode modes.
- This brings the deepest FT8 path closer to the original WSJT-X style decoder without making every normal decode more expensive.

## Repeated Message Recovery

- Strengthens the FT8 repeated-hint recovery path for very weak repeated messages already seen in the previous slot.
- Falls back to a conservative soft check when strict `ft8sdvar` does not accept the repeated message.
- Requires the hint to match the previous message at the same frequency and DT.
- Requires valid sync plus bounded hard-error and LLR-distance thresholds before accepting the hinted message.
- Prevents the new path from becoming a broad ghost-call source by refusing candidates with no measurable tone energy or excessive soft errors.

## Test Coverage

- Extends the FT8 weak decode regression to cover a synthetic -26 dB FT8 frame.
- Keeps the generic weak decode regression for the existing lower-SNR smoke path.
- Manual verification was also run at -24, -25, and -26 dB.
- -27 dB and -30 dB synthetic FT8 frames were checked and did not decode, confirming that the new threshold is not unbounded or overly permissive.

## Italiano

Questa release lavora sulla sensibilita' FT8 dopo la 1.0.273. L'analisi e' stata fatta con segnali FT8 sintetici a -24, -25 e -26 dB per capire se il problema fosse nella ricerca sync o nella fase successiva.

- Confermato che a -25/-26 dB il candidato FT8 entra gia' nella lista sync con frequenza e DT corretti.
- Il collo di bottiglia non era quindi `sync8var/getcandidates`, ma bitmetrics, LDPC/OSD e recupero del messaggio ripetuto.
- Aggiunto un percorso bitmetrics FT8 deep ispirato alla logica weak-signal di `ft8bvar`.
- Applicata una trasformazione weak-signal quando `srr` e' sotto la soglia dei segnali molto deboli.
- Aggiunta normalizzazione RMS per i metrici deep FT8.
- Aggiunto belief propagation esatto `tanh/platanh` per il profilo FT8 piu' profondo `maxosd=3/norder=4`.
- Il percorso LDPC leggero precedente resta attivo per le decodifiche normali.
- Rafforzato il recupero di messaggi FT8 gia' visti nello slot precedente quando vengono ripetuti molto deboli.
- Il recupero soft richiede stessa frequenza/DT, sync valida, energia misurabile e soglie strette su hard-error e distanza LLR.
- Il test automatico FT8 deep ora verifica un frame sintetico a -26 dB.
- Verificati manualmente -24, -25 e -26 dB; -27 e -30 dB non vengono accettati, quindi il nuovo percorso non e' troppo permissivo.

## Artifacts

- Windows x64 installer
- macOS Apple Silicon DMG/ZIP
- Linux x86_64 AppImage built with Qt 6.11
