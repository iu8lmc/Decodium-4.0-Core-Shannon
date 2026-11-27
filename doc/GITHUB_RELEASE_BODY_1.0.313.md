Fork iu8lmc mirror della 1.0.313 di elisir80 — include i fix fork 1.0.310/1.0.311/1.0.312 già assorbiti upstream + le FT2/TX audio hardenings di Salvatore.

## Decodium 1.0.313 — FT2 Sequencer & TX Audio Hardening

### Modifiche principali (Salvatore / elisir80)

- **FT2 guard legacy TX / Fake-It**: protezioni aggiuntive sul percorso TX legacy per prevenire trasmissioni spurie in modalità Fake-It
- **Monotonic signoff**: la sequenza di signoff FT2 ora avanza in modo monotono, eliminando possibili loop di signoff ripetuto
- **Async TX timing**: il timing del modulo TX è ora gestito in modo asincrono, riducendo la latenza inter-TX e il rischio di stutter audio
- **DXCC false positive**: corretti falsi positivi nel filtro DXCC durante la decodifica FT2

### Fix precedenti inclusi (fork iu8lmc, già in upstream 1.0.313)

- **1.0.312** — Fix logger congelato a 5 MB: guard ri-entranza per-thread (g_inDiag) blocca deadlock QMutex non-ricorsivo da qWarning interna durante rotazione; fallback TRUNCATE se rename .log fallisce
- **1.0.311** — FT2 signoff retry cap configurabile (1-8, default 4), Q_PROPERTY ft2SignoffRetryCap, SpinBox in Settings > AutoSeq
- **1.0.310** — Splash Screen 10s + pulsanti "Offrimi un caffe" / "Avvia"; rimosso click-anywhere

### Note di build

- Cross-compilato con MSYS2/MinGW64, Qt 6.11.0
- Target Windows x64 (Windows 10/11)
