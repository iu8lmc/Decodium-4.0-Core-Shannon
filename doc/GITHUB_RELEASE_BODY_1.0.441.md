# Decodium 4 FT2 1.0.441

Release di manutenzione rispetto alla 1.0.440, focalizzata su robustezza CAT, pulizia dei log `ALL.TXT`, usabilita' del footer su schermi piccoli e preparazione packaging multipiattaforma.

## Correzioni principali

- Aggiunto fallback automatico per la porta CAT seriale Hamlib quando la porta salvata non e' piu' esposta dal sistema operativo.
  - Se la porta configurata e' disponibile, Decodium continua a usarla senza modifiche.
  - Se la porta configurata e' scomparsa, Decodium cerca una candidata seriale/USB coerente, preferendo la stessa famiglia (`usbserial`, `usbmodem`, stesso prefisso) e salvando la nuova scelta solo quando il match e' affidabile.
  - Sono escluse porte non CAT come Bluetooth, debug console e incoming port.
  - Questo protegge dai casi macOS/driver USB in cui la seriale viene temporaneamente rinumerata o riappare con un path diverso dopo sleep, unplug o reboot.
- Migliorato il confronto dei log D4/JTDX ripulendo l'output `ALL.TXT` di Decodium.
  - I suffissi interni del decoder (`aN`, `qNN`) e marker finali diagnostici (`?`, `*`, `#`, `^`) non vengono piu' scritti sulle righe `Rx`/`Ck` del file `ALL.TXT`.
  - La modifica riguarda solo il formato del log esportato; non cambia il testo decodificato mostrato internamente ne' la logica di decode.
- Reso sempre accessibile il controllo FT decoder threads nel footer QML.
  - Il badge Threads non viene piu' nascosto sotto i 1500 px di larghezza.
  - Su layout stretti diventa compatto (`FT A` per AUTO, `FT N` per numero thread), mantenendo click sinistro per ciclare i thread e click destro per tornare ad AUTO.
  - Il monitor GPU viene nascosto sui footer molto stretti per lasciare spazio ai controlli operativi piu' importanti.
- Aggiornata la versione locale e i metadati installer a `1.0.441`.

## Verifiche operative

- Build locale `decodium_qml`: OK.
- Test reale D4/JTDX di 10 minuti prima del cleanup `ALL.TXT`:
  - D4: 2022 decode
  - JTDX: 1885 decode
  - D4 +137 decode
  - Hashati `<...>`: D4 14, JTDX 21
- Test reale D4/JTDX di 10 minuti dopo il cleanup `ALL.TXT`:
  - D4: 2158 decode
  - JTDX: 2101 decode
  - D4 +57 decode
  - Hashati `<...>`: D4 7, JTDX 11
  - Righe D4 con suffissi interni nel nuovo `ALL.TXT`: 0
- Verifica CAT seriale: Decodium apre correttamente la porta CAT disponibile e non forza fallback quando la porta salvata e' tornata presente.

## Packaging

- La release `1.0.441` e' predisposta per:
  - installer Windows x64 tramite workflow GitHub Actions;
  - DMG macOS Apple Silicon tramite runner GitHub;
  - DMG macOS Intel tramite runner GitHub;
  - AppImage Linux x86_64 Qt 6.11 tramite runner GitHub;
  - AppImage Linux aarch64 Qt 6.11 tramite runner GitHub.
