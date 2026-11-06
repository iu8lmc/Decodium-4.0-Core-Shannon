# Decodium 4 FT2 1.0.347

Release di stabilizzazione dalla 1.0.346 alla 1.0.347. Questa build raccoglie le correzioni locali fatte dopo l'allineamento con la 1.0.346 e prepara gli asset Windows, macOS e Linux tramite GitHub Actions.

## Modifiche principali

- Audio TX/Tune piu' robusto: il device di uscita viene risolto con identita' stabile e cache aggiornata prima dell'avvio, riducendo i casi di fallback silenzioso verso un device omonimo o default.
- Fix crash Linux su `Tune`: quando Qt audio segnala `IOError`, lo stop del tune viene riportato sul thread del bridge con connessione queued, evitando `QSocketNotifier` da thread sbagliato e il successivo `SIGSEGV`.
- Supporto Linux a `/dev/serial/by-id`: le liste CAT/PTT includono i path stabili e confrontano i symlink con il target reale, evitando doppie porte quando `/dev/serial/by-id/...` punta allo stesso `/dev/tty*`.
- ALC Hamlib piu' affidabile: il valore ALC ha ora un flag di validita' separato; sui backend Linux viene tentato un probe controllato anche se la capability mask non dichiara `RIG_LEVEL_ALC`.
- Q65 ZAP cablato: il controllo ZAP ora arriva al decoder Q65 nativo e puo' abilitare/disabilitare il blanking nel percorso corretto.
- UI QML piu' stabile su macOS: introdotti `DecoComboBox` e `DecoTextField` per evitare il rendering errato di emoji/simboli in campi e menu Material.
- Popup e MessageBox corretti: migliorata la dimensione delle finestre e rimossa la modalita' dim dal prompt-to-log, cosi' il popup resta leggibile e i pulsanti non si sovrappongono al testo.
- Cloudlog piu' chiaro: normalizzazione endpoint API, gestione distinta di HTTP 401/403/407, parsing delle risposte API key e messaggi di errore piu' utili per autenticazione/proxy/protezioni web.
- Fix avvio QML su macOS: rimosso l'uso di proprieta' non supportate in `TxPanel` che causava `Type TxPanel unavailable`.
- Versione locale e packaging allineati a `1.0.347` in `fork_release_version.txt`, Inno Setup, NSIS e workflow macOS legacy.

## Dettaglio tecnico

- `DecodiumBridge` aggiorna i percorsi di risoluzione audio TX/RX, invalida la cache quando necessario e gestisce lo stop Tune in modo thread-safe.
- `DecodiumCatManager` e `DecodiumTransceiverManager` enumerano `/dev/serial/by-id`, canonicalizzano i path Linux e confrontano correttamente CAT/PTT.
- `HamlibTransceiver` separa disponibilita' e valore ALC, disabilitando il probe solo quando Hamlib restituisce errori non supportati.
- `StatusBar` mostra `ALC --` quando il dato non e' realmente disponibile, invece di mascherare uno zero valido.
- `DecodiumCloudlogLite` limita le risposte lette dalla rete, normalizza `/index.php/api/...`, riconosce risposte JSON/XML e produce messaggi diagnostici piu' leggibili.
- `FtxQ65Core`, `FtxQ65Decoder` e `Q65DecodeWorker` portano il flag ZAP nel decoder Q65.
- I controlli QML migrati ai componenti `Deco*` usano rendering testo piu' stabile e dimensioni controllate.

## Asset previsti

- Sorgenti GitHub automatici della release/tag `1.0.347`.
- Installer Microsoft Windows x64: `Decodium_1.0.347_Setup_x64.exe`.
- macOS Apple Silicon: DMG/ZIP Tahoe arm64 e Sequoia arm64.
- macOS Intel: DMG/ZIP Ventura, Sonoma e Sequoia x86_64.
- Linux Intel 64 bit: `decodium4-ft2-1.0.347-linux-x86_64.AppImage`.
- Linux aarch64: `decodium4-ft2-1.0.347-linux-aarch64.AppImage`.

Gli asset binari vengono caricati dai workflow GitHub Actions appena ogni runner termina con successo.
