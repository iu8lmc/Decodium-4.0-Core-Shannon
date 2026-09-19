# Decodium 4 FT2 1.0.351

Release finale dalla 1.0.346 alla 1.0.351.

Questa release integra il lavoro locale delle serie 1.0.347/1.0.348, l'allineamento di Martino fino a `v1.0.350` e il merge locale finale sul ramo principale. La versione `1.0.351` e' il punto di rilascio unico per distribuire installer Windows, DMG macOS e AppImage Linux coerenti tra loro.

## Modifiche principali

- Integrato upstream/Martino `v1.0.350`:
  - merge della base fork con le correzioni fino alla 1.0.347;
  - localizzazione italiana completa post-merge;
  - preservate le funzioni fork-only: DX-Pedition Mode, ALC, miglioramenti FT8/FT2 e funzioni UI locali.
- Audio RX/TX/Tune piu' robusto:
  - identificazione piu' stabile delle periferiche audio;
  - refresh automatico della cache audio prima dell'auto-monitor all'avvio;
  - log piu' chiari su device salvato/scelto e stato audio;
  - ridotto il rischio di fallback silenzioso verso device omonimi o default non desiderati.
- Crash Linux TX/Tune corretto:
  - gli stop dovuti a errori audio rientrano sul thread del bridge;
  - i callback `QAudioSink` sono protetti da guardie su oggetto e seriale playback;
  - evitato il percorso `QSocketNotifier` da thread sbagliato seguito da `SIGNAL 11`.
- CAT/PTT Linux migliorato:
  - supporto ai path stabili `/dev/serial/by-id`;
  - confronto canonico dei symlink per evitare doppie porte quando puntano allo stesso device `/dev/tty*`.
- ALC Hamlib piu' affidabile:
  - valore e disponibilita' sono separati;
  - la UI mostra `ALC --` quando il backend non fornisce il dato;
  - sui backend Linux viene tentato un probe controllato anche se la capability mask non dichiara ALC.
- Q65 ZAP cablato:
  - il controllo ZAP arriva al decoder Q65 nativo e puo' abilitare/disabilitare il blanking nel percorso corretto.
- UI QML/macOS piu' stabile:
  - introdotti `DecoComboBox` e `DecoTextField` per evitare rendering errato di emoji/simboli nei campi Material;
  - corretto l'avvio QML su macOS dovuto a proprieta' non supportate in `TxPanel`;
  - prompt-to-log e MessageBox dimensionati in modo piu' robusto, senza sovrapposizione dei pulsanti.
- Cloudlog piu' diagnostico:
  - normalizzazione degli endpoint API;
  - gestione distinta di HTTP 401/403/407, proxy/autenticazione e risposte API key;
  - messaggi di errore piu' utili per gli utenti.
- Autosequenza e worked-before:
  - gestione migliore di nominativi lunghi/speciali e hash diretti al partner attivo;
  - margine maggiore per CPU lente nei passaggi TX1/TX2/TX3;
  - worked-before calcolato per chiamata, banda e modo, evitando falsi B4 tra 20m FT2, 20m FT8 e 40m FT2.
- Display decode:
  - opzioni in Setup > Display > Decodes per mostrare/nascondere `Dist` e `Az` in Full Spectrum e Signal RX;
  - aggiunta colonna `Freq` configurabile in Signal RX.
- Panadapter:
  - rimossi i marker gialli duplicati 500/1000/1500/2000/2500/3000 Hz, lasciando la scala inferiore come riferimento unico.
- Check SWR:
  - non blocca piu' Tune, utile per misurare e correggere SWR alto con strumenti esterni;
  - la protezione resta attiva su TX reale e AutoCQ.

## Dettaglio tecnico

- `fork_release_version.txt`, Inno Setup, NSIS e workflow macOS legacy sono allineati a `1.0.351`.
- `DecodiumBridge` integra refresh audio pre-monitor, gestione TX/Tune thread-safe, guardie `QAudioSink`, worked-before call/band/mode e autosequence piu' tollerante.
- `DecodiumCatManager` e `DecodiumTransceiverManager` enumerano e confrontano correttamente `/dev/serial/by-id`.
- `HamlibTransceiver` distingue disponibilita' ALC e valore ALC.
- `DecodiumCloudlogLite` limita e normalizza le risposte di rete e produce diagnostica piu' leggibile.
- `FtxQ65Core`, `FtxQ65Decoder` e `Q65DecodeWorker` portano il flag ZAP nel percorso Q65.
- QML integra i nuovi controlli `Deco*`, opzioni decode persistenti e testo status bar aggiornato per SWR/Tune.

## Asset previsti

- Sorgenti GitHub automatici della release/tag `1.0.351`.
- Installer Microsoft Windows x64: `Decodium_1.0.351_Setup_x64.exe`.
- macOS Apple Silicon: DMG/ZIP Tahoe arm64 e Sequoia arm64.
- macOS Intel: DMG/ZIP Ventura, Sonoma e Sequoia x86_64.
- Linux Intel 64 bit: `decodium4-ft2-1.0.351-linux-x86_64.AppImage`.
- Linux aarch64: `decodium4-ft2-1.0.351-linux-aarch64.AppImage`.

Gli asset binari vengono caricati dai workflow GitHub Actions appena ogni runner termina con successo.
