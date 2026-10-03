# Decodium 4 FT2 1.0.445

Release di manutenzione rispetto alla `1.0.443`, con integrazione degli aggiornamenti `1.0.444` di Martino e fix locali per CAT/Hamlib e layout impostazioni.

## Novita' dalla 1.0.443 alla 1.0.445

- Integrata la `1.0.444` upstream di Martino.
- Aggiunto **Ft2.it Award 2026** anche nel dropdown **Contest** dell'editor **Macro TX**.
- Mantenuta la voce **Ft2.it Award 2026** nella finestra **Contest Mode**, gia' introdotta nella `1.0.443`.
- Aggiornata la versione locale e i metadati installer a `1.0.445`.

## CAT / Hamlib

- Aggiunta l'opzione **CAT keep-alive** nella schermata CAT/Radio e nel dialog rapido CAT.
- Il keep-alive e' disattivato di default, cosi' i setup Icom CI-V sensibili ai timeout, come IC-7300 su adattatori seriali lenti o instabili, restano protetti.
- Quando attivato su Hamlib/Icom seriale, il keep-alive usa solo una lettura leggera e rate-limited di frequenza, senza ripristinare il polling aggressivo di PTT/VFO/split/mode.
- Il keep-alive si sospende in TX/PTT e si disattiva automaticamente per la connessione corrente dopo errori ripetuti.
- Aggiunto logging `catKeepAlive` nei parametri CAT per rendere leggibile dai log se l'opzione era attiva.
- La scelta viene salvata nelle impostazioni CAT e resta compatibile anche con il backend CAT nativo.

## Layout impostazioni

- Aggiunto margine scrollabile in fondo alla tab **Radio/CAT** delle impostazioni.
- La sezione **ALC AUTO CALIBRATION** e il relativo pulsante restano visibili anche su risoluzioni dove il footer della finestra copriva l'ultima parte del contenuto.

## Compatibilita' e note operative

- Per utenti con interfacce tipo RigExpert TI-5000 che vogliono vedere attivita' sul LED CAT anche in RX, attivare **CAT keep-alive**.
- Per setup gia' stabili dopo la rimozione del polling aggressivo, lasciare **CAT keep-alive** disattivato.
- Il decoder non cambia rispetto alla `1.0.443`/`1.0.444`.

## Build e release

- Predisposta la release `1.0.445` per:
  - sorgente GitHub;
  - Windows x64 installer/exe tramite GitHub Actions;
  - macOS Apple Silicon DMG;
  - macOS Intel DMG;
  - Linux AppImage x86_64;
  - Linux AppImage aarch64.

## Validazione locale

- `git diff --check`
- `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`
- `ctest --test-dir "/Users/salvo/Desktop/Decodium4-build" -R "test_qt_helpers|test_streaming_list_model" --output-on-failure`
