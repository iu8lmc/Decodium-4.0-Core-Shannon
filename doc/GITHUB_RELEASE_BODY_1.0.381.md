# Decodium 4 FT2 1.0.381

## Annullata la correzione retry FT8/FT4 della 1.0.380 (timing errato)

La correzione introdotta nella 1.0.380 (ritrasmissione in FT8/FT4 quando il partner non risponde) aveva un **difetto di timing**: la ritrasmissione partiva troppo tardi nello slot (verso l'11°-12° secondo) e risultava **troncata** (~2 secondi), quindi inutile e potenzialmente di disturbo sulla banda.

Per evitare trasmissioni incomplete, in questa versione la modifica viene **annullata** e il comportamento torna a quello della **1.0.379**: in FT8/FT4 con QuickQSO, se chiami una stazione che non risponde, Decodium **non** ritrasmette automaticamente.

La correzione corretta del retry (far ripartire la chiamata all'inizio del periodo, a tempo) verrà ripubblicata dopo validazione approfondita in loopback. Il problema FT2 non è interessato.

## Asset

- Installer Windows x64 `.exe` (allegato).
- AppImage Linux / pacchetti macOS generati dai runner GitHub.
