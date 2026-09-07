Fork iu8lmc — 1.0.314 fork-only (non in elisir80). Toggle opt-in `ftxImmediateClickTx` per ripristinare il TX immediato al click stile 1.0.283 su tutti e 3 i modi.

## Decodium 1.0.314 — TX immediato al click (opt-in)

### Nuova funzionalita (fork-only, NON in elisir80)

- **TX immediato al click** (`ftxImmediateClickTx`, default OFF): toggle opt-in che ripristina il comportamento pre-1.0.300 "TX parte subito al click" su FT2, FT8 e FT4.
  - **FT2**: rilassa il gate `inQsoResponse` da `m_currentTx >= 2` a `>= 1` (TX1 da double-click bypassa il period-gate)
  - **FT8/FT4**: alza il cap `latestCleanStartMs` da ~650 ms a 2000 ms (finestra cliccabile da ~4% a ~13-27% dello slot, dentro la tolleranza partner D3/JTDX)
  - CheckBox in Settings > Auto Sequence > FT2 UTILITY, persistito nello store Decodium3
  - Default OFF = comportamento upstream Salvatore conservato (safe per tutti gli utenti)

### Note di build

- Cross-compilato con MSYS2/MinGW64, Qt 6.11.0
- Target Windows x64 (Windows 10/11)
- Cambio C++ in DecodiumBridge.cpp: rebuild esteso (2 obj cancellati prima di make)
