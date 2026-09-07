# Decodium 4 FT2 1.0.372

Release cumulativa dalla 1.0.369 alla 1.0.372.

## Novita principali

- Integrate le modifiche upstream di Martino fino alla 1.0.371:
  - regolazione della potenza TX dal pulsante Enable TX tramite tasto destro e rotellina;
  - correzione della persistenza dell'orologio floating;
  - salvataggio/ripristino della posizione dell'orologio floating;
  - opzione FT2 per loggare RR73/TX4 anche quando il partner sparisce.
- Ridotta la verbosita dei log di decode metric:
  - `DECODEMETRIC` non viene piu emesso a ogni decode normale;
  - rimane sempre visibile per decode lenti o code di lavoro lente;
  - log completo riattivabile con `DECODIUM_DECODEMETRIC_VERBOSE=1` oppure `DECODIUM_DECODEMETRIC=verbose`;
  - intervallo periodico configurabile con `DECODIUM_DECODEMETRIC_INTERVAL_MS`.
- Migliorato il TX watchdog:
  - il watchdog time-based usa ora wall-clock reale, non solo tick UI;
  - reset piu coerente quando cambia modalita/tempo/conteggio;
  - sincronizzazione del watchdog anche con il backend legacy;
  - correzione dello stato TX quando il bridge audio legacy e ancora attivo.
- Decode colors:
  - aggiunti toggle per abilitare/disabilitare singole categorie di colore;
  - quando una categoria colore e disattivata usa un colore fallback uniforme;
  - applicazione coerente in Main, Full Spectrum, Signal RX e finestre decode.
- Setup/UI:
  - Settings piu larga e con margini corretti per evitare testo e controlli tagliati;
  - layout Auto Sequence/FT2 utility reso piu stabile;
  - normalizzazione dei filtri territorio, cosi i campi Europe/Africa/Oceania/Asia/North America/South America non ereditano testo corrotto;
  - finestra Astro e Macro rese piu comode da trascinare/ridimensionare;
  - migliorati combobox e controlli della Waterfall;
  - filtro logbook aggiornato con modo FT2;
  - indicatore thread decoder nel footer mostra chiaramente AUTO.

## Dettaglio tecnico

- `v1.0.370`: regolazione potenza TX con rotellina e fix persistenza orologio floating.
- `v1.0.371`: fix finale posizione orologio floating e opzione FT2 per log RR73 quando il partner sparisce.
- `1.0.372`: integrazione delle modifiche locali:
  - `Detector/DecodeMetricLogging.hpp` centralizza la policy di logging decode metric;
  - `FT2DecodeWorker`, `FT4DecodeWorker` e `FT8DecodeWorker` usano la nuova policy anti-spam;
  - `DecodiumBridge` salva e applica i toggle colore decode e usa watchdog time-based a wall-clock;
  - `SettingsDialog`, `Main`, `DecodeWindow` e Waterfall sono stati allineati alle nuove opzioni UI.

## File principali modificati

- `Configuration.cpp`
- `Configuration.hpp`
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`
- `DecodiumLegacyBackend.cpp`
- `DecodiumLegacyBackend.h`
- `Detector/DecodeMetricLogging.hpp`
- `Detector/FT2DecodeWorker.cpp`
- `Detector/FT4DecodeWorker.cpp`
- `Detector/FT8DecodeWorker.cpp`
- `qml/decodium/Main.qml`
- `qml/decodium/components/SettingsDialog.qml`
- `qml/decodium/components/Waterfall.qml`
- `qml/decodium/components/DecodeWindow.qml`
- `qml/decodium/components/AstroWindow.qml`
- `qml/decodium/components/MacroDialog.qml`
- `widgets/mainwindow.cpp`
- `widgets/mainwindow.h`
- `fork_release_version.txt`
- `installer/decodium_setup.iss`
- `installer/decodium_setup_nsis.nsi`

## Asset previsti nella release

- Source code GitHub per il tag `1.0.372`.
- Installer Windows x64 `.exe`.
- Pacchetti macOS Apple Silicon tramite runner GitHub.
- Pacchetti macOS Intel tramite runner GitHub.
- AppImage Linux x86_64 Qt 6.11.
- AppImage Linux aarch64 Qt 6.11.
