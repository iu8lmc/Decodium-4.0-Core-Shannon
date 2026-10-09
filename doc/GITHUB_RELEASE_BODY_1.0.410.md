# Decodium 4 FT2 1.0.410

Release 1.0.410 porta sul ramo principale gli aggiornamenti locali successivi alla 1.0.409 e prepara una distribuzione completa per Windows, macOS e Linux.

## Modifiche dalla 1.0.409

- Decoder FT4:
  - aggiunta semina hash locale per migliorare il riconoscimento dei nominativi compressi e composti;
  - introdotte ricerche FT4 aggiuntive controllate da guardie CPU, in modo che i PC veloci possano sfruttare il recupero piu profondo senza bloccare le macchine lente;
  - aggiunto fallback LDPC selettivo per candidati deboli, limitato ai profili in cui il costo rimane sostenibile;
  - spostata la sottrazione FT4 dopo l'accettazione effettiva del decode, evitando sottrazioni generate da risultati scartati;
  - aggiunti trace diagnostici mirati per seguire target FT4 attesi durante l'analisi di WAV registrati.

- Strumenti di analisi:
  - nuovo tool `tools/compare_ft4_wav_slots.py` per confrontare slot WAV FT4 con file `ALL.TXT` e produrre riepiloghi JSON/testuali;
  - ampliato `tests/ft4_stage_compare.cpp` per supportare diagnostica, seed hash e confronto per slot;
  - affinato `tools/compare_alltxt.py` per l'uso nei confronti locali.

- Log QSO e ADIF:
  - aggiunti orario inizio e fine QSO al prompt di conferma log;
  - gli orari sono precompilati dal QSO osservato e restano modificabili manualmente;
  - l'autolog conserva `QSO_DATE`, `TIME_ON` e `TIME_OFF` coerenti anche nel backend legacy e nel bridge moderno.

- Logbook:
  - aggiunta cancellazione dei database logbook esistenti;
  - popup di conferma con scelta tra rimuovere solo l'associazione o eliminare anche il file ADIF dal disco;
  - se viene rimosso l'ultimo logbook, Decodium ricrea un logbook vuoto utilizzabile.

- Distanze e mappa:
  - corretta la preferenza miglia/km nelle viste principali, nel logbook e nella live map;
  - la live map ora rispetta correttamente il valore booleano della preferenza `Miles`.

- UI Linux/QML:
  - ritoccato l'allineamento delle toolbar e dei pulsanti operativi per ridurre gli scostamenti osservati su Linux;
  - uniformate le righe inferiori dei controlli banda/TX e alcuni pannelli QML.

## Compatibilita e timing

- Le funzioni FT4 piu pesanti sono protette da guardie sul numero di core disponibili.
- Su CPU single core Decodium evita i percorsi FT4 piu costosi.
- Il profilo FT4 corrente e' stato mantenuto entro i tempi di rilascio osservati nei test locali.

## Validazione locale

- `python3 -m py_compile tools/compare_ft4_wav_slots.py`
- `git diff --check`
- build locale Decodium/QML e tool FT4 prima della pubblicazione del tag.

## Asset attesi

- Source code archive generato da GitHub per `v1.0.410`.
- Windows x64:
  - `Decodium_1.0.410_Setup_x64.exe`
- macOS Apple Silicon:
  - `decodium4-ft2-1.0.410-macos-tahoe-arm64.dmg`
  - `decodium4-ft2-1.0.410-macos-sequoia-arm64.dmg`
- macOS Intel:
  - `decodium4-ft2-1.0.410-macos-ventura-x86_64.dmg`
  - `decodium4-ft2-1.0.410-macos-sonoma-x86_64.dmg`
  - `decodium4-ft2-1.0.410-macos-sequoia-x86_64.dmg`
- Linux:
  - `decodium4-ft2-1.0.410-linux-x86_64.AppImage`
  - `decodium4-ft2-1.0.410-linux-aarch64.AppImage`
- File SHA256 generati dai runner macOS e Linux.
