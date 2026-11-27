# Decodium 4 FT2 1.0.380

## Correzione FT8/FT4: nessuna ritrasmissione se il partner non risponde

- **Retry ripristinato in FT8/FT4**: se chiami una stazione (rispondi a un CQ) e il partner **non risponde**, ora Decodium **ritrasmette** la chiamata al periodo successivo (fino al limite di tentativi), invece di fare una sola trasmissione e poi fermarsi. Quando il partner risponde, il QSO procede regolarmente come prima.

Era una regressione introdotta dall'irrobustimento del sequencer nella 1.0.375: in un QSO attivo, il sistema aspettava il decode del partner ma, se non arrivava nessuna risposta, **saltava** la ritrasmissione senza riprovare. Ora aspetta comunque il decode (per non sovrascrivere una risposta in arrivo), ma se il partner tace ritrasmette la chiamata. In FT2 il problema non si presentava.

## Asset

- Installer Windows x64 `.exe` (allegato).
- AppImage Linux / pacchetti macOS generati dai runner GitHub.
