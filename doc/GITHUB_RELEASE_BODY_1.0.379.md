# Decodium 4 FT2 1.0.379

## Correzione: chiamata bloccata con "Tx1" disabilitato

- **Risposta a un CQ con Tx1 disabilitato**: se tieni il **Tx1** disabilitato (toggle in stile WSJT-X) e fai doppio-click su una stazione che chiama CQ, il QSO non partiva più — il sequencer tentava di selezionare Tx1, lo trovava disabilitato, **rifiutava la selezione e restava bloccato** (sintomo: "fa una chiamata poi si blocca"). Ora, esattamente come in WSJT-X, quando Tx1 è disabilitato la risposta parte automaticamente da **Tx2**. Puoi quindi tenere Tx1 spento e chiamare normalmente.

Era una regressione introdotta dall'hardening del sequencer nella 1.0.375 (`advanceQsoState` reso capace di rifiutare una selezione di TX disabilitato, ma senza il fallback per il caso "rispondi a un CQ con Tx1 off"). Vale per FT8/FT4/FT2.

## Asset

- Installer Windows x64 `.exe` (allegato).
- AppImage Linux / pacchetti macOS generati dai runner GitHub.
