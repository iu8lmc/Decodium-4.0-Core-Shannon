# Decodium 4.0 v1.0.536

Version 1.0.536 is an interface localisation release. A general audit found
several hundred interface labels that were written directly into the QML
sources without `qsTr()`. Those strings never entered the translation
catalogues, so they stayed in English in every language while the catalogue
counters still reported zero unfinished entries. A smaller group had the
opposite problem: the text was written in Italian and stayed Italian even when
another language was selected.

## English (British)

### Hardcoded interface strings

- Main panels: Signal RX, caller queue, DX cluster, active stations, message
  averaging, time synchronisation, DecoSync, MAM, decode window, DX-Pedition
  workspace and the status bar.
- Windows and dialogues: Log, Astro, Macro, Rig Control, Info, colour
  highlighting and decode history.
- FT2-Link panel, the largest single group with 90 entries: BBS bulletins and
  server files, digipeater, alert centre, mailbox and SMTP settings, shared
  cluster, frequency presets and schedule windows, blocked callsigns and the
  satellite half-duplex section including its operating warnings.
- Developer overlay, Live Map, QSY dialogue, splash screen and boot loader.
- Column headers of Full Spectrum and of the RX-frequency panel, together with
  the menu that controls their visibility, the decode counter and the time
  synchronisation status text.

### Deliberate exceptions

- Format examples, host names and template lines are left unchanged, for
  instance `smtp.host`, `key=value; key=value`, `localhost:50001` and the
  frequency preset samples.
- Protocol abbreviations used as compact button captions are left unchanged:
  ACK, CF, SNR, LH, LC, VM, RX, TX. Translating them would widen the buttons of
  an already dense panel without making anything clearer.
- International column abbreviations are left unchanged: UTC, dB, DT, DXCC and
  Az. Those columns have a fixed pixel width.

### Catalogues

- 5007 to 5464 messages across all 14 languages.
- Zero unfinished and zero vanished entries in every catalogue.
- `lrelease` reports 5464 finished translations per language.

### Validation

- Local `decodium_qml` build completed successfully.
- `qmllint` reported no errors on every modified QML file.
- Runtime check of the built application: no QML warnings, no type errors, FT8
  decoding active and the waterfall rendering normally.

## Italiano

La versione 1.0.536 e' una release di localizzazione dell'interfaccia. Un
controllo generale ha trovato diverse centinaia di etichette scritte
direttamente nei sorgenti QML senza `qsTr()`. Quelle stringhe non entravano nei
cataloghi di traduzione e restavano quindi in inglese in ogni lingua, senza mai
comparire fra le voci non tradotte. Un gruppo piu' piccolo aveva il problema
opposto: il testo era scritto in italiano e restava in italiano anche
selezionando un'altra lingua.

### Stringhe fisse nell'interfaccia

- Pannelli principali: Signal RX, coda chiamate, cluster DX, stazioni attive,
  media messaggi, sincronia oraria, DecoSync, MAM, finestra decodifiche,
  spazio DX-Pedition e barra di stato.
- Finestre e dialoghi: Log, Astro, Macro, Rig Control, Info, evidenziazione
  colori e cronologia decodifiche.
- Pannello FT2-Link, il gruppo piu' numeroso con 90 voci: bollettini e file del
  server BBS, digipeater, centro avvisi, mailbox e impostazioni SMTP, cluster
  condiviso, preimpostati di frequenza e finestre orarie, nominativi bloccati e
  la sezione satellite half-duplex con le sue avvertenze operative.
- Overlay di sviluppo, Live Map, dialogo QSY, schermata iniziale e boot loader.
- Intestazioni di colonna di Full Spectrum e del pannello frequenza RX, insieme
  al menu che ne regola la visibilita', al contatore delle decodifiche e al
  testo di stato della sincronia oraria.

### Eccezioni volute

- Restano invariati gli esempi di formato, i nomi host e le righe modello, come
  `smtp.host`, `key=value; key=value`, `localhost:50001` e i campioni dei
  preimpostati di frequenza.
- Restano invariate le sigle di protocollo usate come etichette compatte di
  pulsante: ACK, CF, SNR, LH, LC, VM, RX, TX. Tradurle allargherebbe i pulsanti
  di un pannello gia' denso senza rendere nulla piu' chiaro.
- Restano invariate le sigle internazionali di colonna: UTC, dB, DT, DXCC e Az.
  Quelle colonne hanno larghezza fissa in pixel.

### Cataloghi

- Da 5007 a 5464 messaggi in tutte e 14 le lingue.
- Zero voci non tradotte e zero voci obsolete in ogni catalogo.
- `lrelease` riporta 5464 traduzioni complete per lingua.

### Verifica

- Build locale di `decodium_qml` completata correttamente.
- `qmllint` non ha segnalato errori su nessuno dei file QML modificati.
- Prova a runtime dell'applicazione compilata: nessun avviso QML, nessun errore
  di tipo, decodifica FT8 attiva e waterfall regolare.

## Release assets

The release workflows publish the Windows x64 executable, macOS Intel and
Apple Silicon DMG packages, and Linux x86_64 and aarch64 AppImages together
with their checksums where provided by the workflow.
