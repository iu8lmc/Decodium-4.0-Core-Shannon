# Decodium 4 FT2 1.0.442

Release di manutenzione rispetto alla 1.0.441, focalizzata sulla stabilita' FT8 quando il sistema e' sotto pressione CPU/audio e sulla tracciabilita' dei tempi di decode.

## Correzioni principali

- Aggiunto un profilo FT8 adattivo quando Decodium rileva pressione CPU, callback FT8 lenti o modalita' low CPU.
  - Il limite thread non viene piu' tagliato sempre a 1 nei casi di pressione: sui sistemi con piu' core viene mantenuto un profilo piu' equilibrato, fino a meta' dei core in pressione normale e fino a 2 thread in pressione severa.
  - In pressione FT8 vengono ridotti profondita', finestra RX e tempi massimi di decode per proteggere lo slot successivo e contenere i ritardi.
  - I follow-up FT8 piu' costosi vengono messi in cooldown quando il sistema e' gia' in difficolta', evitando code di lavoro che si trascinano nello slot successivo.
  - In caso di pressione severa, il decode FT8 corrente puo' essere cancellato per liberare il percorso radio piu' rapidamente.
- Migliorata la gestione dei decode FT8 time-limited.
  - Le richieste con deadline stretta saltano la preparazione/replay hash piu' pesante, riducendo overhead nei momenti critici.
  - Il profilo constrained evita cicli extra e sensibilita' RX piu' costose quando il limite thread o il tempo massimo indicano una condizione di pressione.
  - Le metriche `[DECODEMETRIC]` includono ora `max_ms`, profilo constrained, tempo di preparazione hash, replay hash e candidate thinning.
- Aggiunto prefiltraggio adattivo dei risultati FT8 duplicati in fase di callback.
  - Quando ci sono molte righe e il callback e' lento o il sistema e' in pressione, Decodium scarta duplicati gia' visti prima di alimentarli alla UI.
  - Questo riduce carico sulla lista decode senza cambiare il contenuto utile dei nuovi decode.
- Rinviato il refresh automatico dei dispositivi audio mentre il percorso radio e' attivo.
  - Gli eventi di cambio device non verbose vengono differiti quando Decodium e' in monitor, TX o tune.
  - La cache audio resta marcata dirty e viene rivalutata piu' avanti, evitando refresh invasivi durante RX/TX.
- Aggiornata la versione locale e i metadati installer a `1.0.442`.

## Verifiche locali

- Build locale `decodium_qml`: OK.
- Test mirati `test_tx_pipeline` e `test_ft2_qso_sim`: OK, 2/2 passed.
- `git diff --check`: OK.

## Packaging

- La release `1.0.442` e' predisposta per:
  - installer Windows x64 tramite workflow GitHub Actions;
  - DMG macOS Apple Silicon tramite runner GitHub;
  - DMG macOS Intel tramite runner GitHub;
  - AppImage Linux x86_64 Qt 6.11 tramite runner GitHub;
  - AppImage Linux aarch64 Qt 6.11 tramite runner GitHub.
