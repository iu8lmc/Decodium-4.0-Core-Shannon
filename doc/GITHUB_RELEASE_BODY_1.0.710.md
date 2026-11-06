# Decodium 4 v1.0.710

## English (British)

This release is built on v1.0.709 and improves how FT2 and FT4 find signals on a
busy band.

### Finding weak FT2 and FT4 signals next to strong ones

- FT2 and FT4 used to look for signals only at the peaks of the average spectrum of
  the slot. A weak station close to a strong one was hidden by it, and the decoder
  never tried the right frequency.
- The decoder now also looks for the known synchronisation symbols of the protocol,
  the way FT8 does, and tries those frequencies first. The strongest peaks of the
  average spectrum are still tried as well.
- Measured on synthetic transmissions with known content added to real band
  recordings (three recordings of different days): FT2 decoded 105 instead of 81 of
  200 on the tuning recording and 170 instead of 96 of 400 on the two confirmation
  recordings. FT4 decoded 125 instead of 96, 90 instead of 55 and 118 instead of 90
  of 200. On white noise the results are unchanged (FT2 140 and 91, FT4 156), and no
  extra false lines appeared in the tests.
- The FT2 decoder also uses less CPU: about 67 ms per decode cycle instead of 226 ms
  in the test workload, because fewer useless candidates are tried. The delay before
  a decode appears is unchanged.
- The SNR shown for the added candidates is computed from the spectrum at their
  frequency.

### Limits

- The measurements use synthetic transmissions added to real recordings. The
  recordings used contain no real FT2 or FT4 traffic, so the gain on the air has not
  been measured yet. Please compare the number of decodes in a busy FT2 or FT4
  session with v1.0.709 and report what you see.
- The FT4 decoder already produced a few meaningless lines on plain background
  before (3 in 960 test slots); the new search does not add to them.

### Release contents

- Based on Decodium 4 v1.0.709 (elisir80/Decodium-4.0-Core-Shannon).
- Test tools: a scoreboard for the receiver in `tools/autoimprove`, and a generator
  of FT4 test signals in `tests`.
- The Windows installer is built from this repository with the fork packaging.

## Italiano

Questa release è costruita sulla v1.0.709 e migliora il modo in cui FT2 e FT4
trovano i segnali in una banda affollata.

### Trovare segnali FT2 e FT4 deboli vicino a quelli forti

- FT2 e FT4 cercavano i segnali solo nei picchi dello spettro medio dello slot. Una
  stazione debole vicina a una forte restava nascosta da quella, e il decoder non
  provava mai la frequenza giusta.
- Ora il decoder cerca anche i simboli di sincronismo noti del protocollo, come fa
  FT8, e prova prima quelle frequenze. I picchi più forti dello spettro medio
  vengono provati lo stesso.
- Misurato su trasmissioni sintetiche di contenuto noto sommate a registrazioni
  vere della banda (tre registrazioni di giorni diversi): FT2 ha decodificato 105
  invece di 81 su 200 nella registrazione di taratura e 170 invece di 96 su 400 nelle
  due di conferma. FT4 ha decodificato 125 invece di 96, 90 invece di 55 e 118
  invece di 90 su 200. Su rumore bianco i risultati sono invariati (FT2 140 e 91,
  FT4 156) e nei test non sono comparse righe false in più.
- Anche la CPU del decoder FT2 cala: circa 67 ms per ciclo di decodifica invece di
  226 ms nel carico di prova, perché si provano meno candidati inutili. Il ritardo
  prima che compaia una decodifica non cambia.
- L'SNR mostrato per i candidati aggiunti è calcolato dallo spettro alla loro
  frequenza.

### Limiti

- Le misure usano trasmissioni sintetiche sommate a registrazioni vere. Le
  registrazioni usate non contengono traffico FT2 o FT4 reale, quindi il guadagno in
  aria non è ancora stato misurato. Confronta il numero di decodifiche in una
  sessione FT2 o FT4 affollata con la v1.0.709 e segnala cosa vedi.
- Il decoder FT4 produceva già prima qualche riga senza senso sul solo fondo (3 in
  960 slot di prova); la nuova ricerca non ne aggiunge.

### Contenuto della release

- Basata su Decodium 4 v1.0.709 (elisir80/Decodium-4.0-Core-Shannon).
- Strumenti di prova: un banco di misura del ricevitore in `tools/autoimprove` e un
  generatore di segnali di prova FT4 in `tests`.
- L'installer Windows è costruito da questo repository con il pacchetto del fork.
