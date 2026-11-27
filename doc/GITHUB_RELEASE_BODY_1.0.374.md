# Decodium 4 FT2 1.0.374

## Correzioni
- **FT2 / cambio banda**: cambiando banda dalla band-bar in modo FT2, la frequenza impostata e' ora quella FT2 della banda (es. 20m -> 14.084, 40m -> 7.062, 17m -> 18.104), non piu' quella FT8. Il bug si manifestava avviando l'applicazione gia' in modo FT2: il modo del gestore bande restava sul default "FT8" e il cambio banda calcolava la frequenza sbagliata. Corretto sincronizzando il modo del gestore bande con il modo applicativo all'avvio e a ogni cambio banda.

## Asset
- Installer Windows x64 `.exe` (allegato).
- AppImage Linux / pacchetti macOS generati dai runner GitHub.
