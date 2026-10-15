# Decodium 4 v1.0.652

## English (UK)

FT2: weak signals next to a strong one are no longer swallowed by the subtraction. Includes elisir80's 1.0.651.

### FT2: weak stations beside a strong one

- When the decoder finds a strong FT2 signal it subtracts it from the audio and searches again for the weaker ones underneath. The filter that estimates the amplitude and phase of the signal to subtract was 700 samples long (58 ms, about 17 Hz wide): it also removed much of a weak station within a few tens of hertz, because an FT2 signal is about 167 Hz wide.
- The filter is now **2,000 samples** long, with the frame-edge correction used by FT8. Subtracting a signal 10 dB stronger, the damage to a nearby weak station falls from -8.7 to -12.9 dB of its energy.
- On a new test bench (149 weak signals, each 5-50 Hz from a strong one) the asynchronous decoder now takes **95** of them instead of **78** (111 when the strong signals are absent); the 50% threshold goes from -12.9 to -13.8 dB. Scenes with scattered signals, simulated QSOs with AP decoding and pure noise are unchanged, with no false decodes. These are bench measurements, not yet confirmed on air.
- The price is tolerance to a strong signal drifting in frequency: above about 2 Hz/s the subtraction leaves a little more residue than before. `DECODIUM_FT2_SUB_NFILT=700` restores the previous filter.
- Two experimental options, both **off**: removing already-decoded signals that the window cuts before each decode (`DECODIUM_FT2_ASYNC_AVANTI=1`, +2 on the bench), and a candidate for the partner's reply at the expected time and frequency during a QSO (`DECODIUM_FT2_ASYNC_ATTESO=1`, +0.6 dB on the bench, no false decodes in an hour of empty waits).

### From elisir80 1.0.651

- More stable audio capture at start-up and during mode changes; no transient `monitoring off` while the audio backend is being armed.
- Cleaner JTTY to FT8/FT4 hand-off: capture restart, watchdog suppressed during the transition, longer recovery window.
- More readable JTTY keyboard transmit field in the dark theme.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

FT2: i segnali deboli accanto a uno forte non vengono più cancellati dalla sottrazione. Comprende la 1.0.651 di elisir80.

### FT2: stazioni deboli accanto a una forte

- Quando il decodificatore trova un segnale FT2 forte lo sottrae dall'audio e cerca di nuovo quelli più deboli sotto. Il filtro che stima ampiezza e fase del segnale da sottrarre era lungo 700 campioni (58 ms, circa 17 Hz di banda): si portava via anche buona parte di una stazione debole a poche decine di hertz, perché un segnale FT2 è largo circa 167 Hz.
- Ora il filtro è lungo **2.000 campioni**, con la correzione ai bordi del frame usata da FT8. Sottraendo un segnale 10 dB più forte, il danno a una stazione debole vicina scende da -8,7 a -12,9 dB della sua energia.
- Su un nuovo banco di prova (149 segnali deboli, ciascuno a 5-50 Hz da uno forte) il decodificatore asincrono ne prende **95** invece di **78** (111 quando i forti non ci sono); la soglia del 50% passa da -12,9 a -13,8 dB. Scene con segnali sparsi, QSO simulati con la decodifica AP e rumore puro invariati, nessuna decodifica falsa. Sono misure al banco, non ancora confermate in aria.
- Il prezzo è la tolleranza a un segnale forte che deriva in frequenza: oltre circa 2 Hz/s la sottrazione lascia un po' più di residuo di prima. `DECODIUM_FT2_SUB_NFILT=700` torna al filtro precedente.
- Due opzioni sperimentali, entrambe **spente**: togliere prima di ogni decodifica i segnali già decodificati che la finestra taglia (`DECODIUM_FT2_ASYNC_AVANTI=1`, +2 al banco), e un candidato per la risposta del corrispondente nel tempo e sulla frequenza attesi durante un QSO (`DECODIUM_FT2_ASYNC_ATTESO=1`, +0,6 dB al banco, nessun falso in un'ora di attese vuote).

### Dalla 1.0.651 di elisir80

- Cattura audio più stabile all'avvio e nei cambi di modo; niente `monitoring off` passeggero mentre il backend audio si arma.
- Passaggio da JTTY a FT8/FT4 più pulito: riavvio della cattura, watchdog sospeso durante la transizione, finestra di recupero più lunga.
- Campo di trasmissione della tastiera JTTY più leggibile nel tema scuro.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
