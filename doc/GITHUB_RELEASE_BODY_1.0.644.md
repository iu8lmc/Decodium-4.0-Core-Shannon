# Decodium 4 v1.0.644

## English (UK)

This release incorporates upstream v1.0.643 and repairs the update window, which was drawing outside its own frame.

### The update window stays inside its frame

- Reported on 19 September 2026 with a screenshot: the contents of the update notice spilled out of the panel and were painted over the decode list, and the release notes were shown as raw Markdown (`## English (UK)`, `**v1.0.639**`).
- The cause was an anchored column: an anchored layout has no implicit size, so the dialog stayed as tall as its title while the children drew further down, outside the background. The content now lives in the dialog's own content item, with its own header and footer, and its height is decided by the layout and bounded by the window that hosts it.
- The release notes are rendered instead of shown as Markdown source, inside a framed, scrollable panel.
- Colours now come from the application theme rather than being hard-coded, as they already did in the Astro window: the notice follows the theme in use, with the new version highlighted in the header and buttons shaped like the rest of the program.

### Language

- The status line "Update available: v…" was not translatable and appeared in Italian in every language. It is now an English source string translated into all 14 supplied languages, with the compiled catalogues regenerated.
- Known and not yet addressed: other status messages are still written in Italian at source and therefore appear in Italian regardless of the selected language. They will be dealt with in a dedicated pass.

### Testing the update window without waiting for a release

- `DECODIUM_UPDATE_FAKE_CURRENT=1.0.600` makes Decodium report an older version of itself and bypasses the once-a-day check limit, so the update notice can be opened on demand. Diagnostics only: without the variable nothing changes.
- This is not a convenience. The window was visible once per release and only on users' machines, which is why the defect reached us as a report. The first run under the variable immediately exposed a second defect that `qmllint` does not catch: `availableWidth` and `availableHeight` are FINAL properties of `Control`, and redefining them prevented the window from opening at all.

### Absorbed from upstream v1.0.643

- Settings and profile persistence corrections (issue #84), with a per-page source inventory and a cross-process persistence test. Values previously written outside a profile are not reassigned automatically: please review radio, audio, logging and transmission settings after upgrading.
- Station weather localised in all supplied languages (issue #85).
- Plain-background SSTV receive-page controls, addressing reported rendering artefacts (issue #86).
- The cross-process settings test also passes on Windows now: INI keys are case-insensitive there, and five intentional pairs differing only in case were making the test compare a value against the other spelling.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Questo rilascio assorbe la versione upstream 1.0.643 e ripara la finestra dell'aggiornamento, che disegnava fuori dal proprio riquadro.

### La finestra dell'aggiornamento resta dentro il suo riquadro

- Segnalata il 19 settembre 2026 con immagine: il contenuto dell'avviso usciva dal pannello e finiva sopra la lista delle decodifiche, e le note di rilascio si leggevano in Markdown grezzo (`## English (UK)`, `**v1.0.639**`).
- La causa era una colonna ancorata: un layout ancorato non ha dimensione implicita, quindi la finestra restava alta quanto il solo titolo mentre i figli disegnavano più in basso, fuori dallo sfondo. Ora il contenuto sta nel contenitore proprio della finestra, con intestazione e piede suoi, e l'altezza la decide il layout entro i limiti della finestra che lo ospita.
- Le note di rilascio sono rese leggibili invece che mostrate come sorgente Markdown, dentro un riquadro con barra di scorrimento.
- I colori arrivano dal tema dell'applicazione invece di essere fissi, come già avveniva nella finestra Astro: l'avviso segue il tema in uso, con la versione nuova in evidenza nell'intestazione e i pulsanti della stessa forma del resto del programma.

### Lingua

- La riga di stato «Aggiornamento disponibile: v…» non era traducibile e compariva in italiano in tutte le lingue. Ora è una stringa inglese all'origine, tradotta in tutte e 14 le lingue fornite, con i cataloghi compilati rigenerati.
- Noto e non ancora sistemato: altri messaggi di stato sono scritti in italiano all'origine e compaiono quindi in italiano qualunque sia la lingua scelta. Saranno affrontati in un giro dedicato.

### Provare la finestra senza aspettare un rilascio

- `DECODIUM_UPDATE_FAKE_CURRENT=1.0.600` fa dichiarare a Decodium una versione più vecchia e deroga al limite di un controllo al giorno, così l'avviso si può aprire quando serve. Solo diagnostica: senza la variabile non cambia nulla.
- Non è una comodità. Quella finestra si vedeva una volta per rilascio e solo sui computer degli utenti, ed è il motivo per cui il difetto è arrivato come segnalazione. La prima prova con la variabile ha fatto emergere subito un secondo difetto che `qmllint` non rileva: `availableWidth` e `availableHeight` sono proprietà FINAL di `Control`, e ridefinirle impediva del tutto l'apertura della finestra.

### Assorbito dalla versione upstream 1.0.643

- Correzioni alla persistenza di impostazioni e profili (issue #84), con inventario per pagina e prova di persistenza fra processi. I valori scritti in precedenza fuori da un profilo non vengono riassegnati da soli: dopo l'aggiornamento conviene ricontrollare radio, audio, log e trasmissione.
- Meteo della stazione localizzato in tutte le lingue fornite (issue #85).
- Pulsanti della pagina di ricezione SSTV a sfondo pieno, per gli artefatti segnalati (issue #86).
- La prova delle impostazioni fra processi ora passa anche su Windows: lì le chiavi INI non distinguono maiuscole e minuscole, e cinque coppie volute che differiscono solo per quelle facevano confrontare un valore con l'altra grafia.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
