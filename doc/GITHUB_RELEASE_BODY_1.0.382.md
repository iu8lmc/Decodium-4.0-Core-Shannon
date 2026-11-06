# Decodium 4 FT2 1.0.382

## Ripristinato il sequencer auto-TX FT8/FT4 della 1.0.374

Il meccanismo di ritrasmissione automatica (auto-TX dopo il time-sync del decode) è stato **riportato alla versione della 1.0.374**, che funzionava correttamente.

La 1.0.375 aveva introdotto un'attesa del decode che, quando il partner non rispondeva, **saltava la ritrasmissione**; il tentativo di correzione della 1.0.380 invece la faceva partire troppo tardi nello slot (trasmissione troncata). Entrambi i comportamenti sono ora superati.

Con questo ripristino, come nella 1.0.374: in FT8/FT4 con QuickQSO, dopo la breve attesa per il decode, Decodium **ritrasmette la chiamata al momento giusto** (a inizio slot) se il partner non ha ancora risposto, e procede regolarmente quando risponde. FT2 non è interessato.

## Asset

- Installer Windows x64 `.exe` (allegato).
- AppImage Linux / pacchetti macOS generati dai runner GitHub.
