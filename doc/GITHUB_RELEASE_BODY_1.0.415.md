# Decodium 4 FT2 1.0.415

Release 1.0.415 allinea il ramo principale alla base 1.0.414 e aggiunge gli aggiornamenti locali preparati per FT4, packaging Windows e pulizia uninstall.

## Modifiche dalla 1.0.414

- Decoder FT4:
  - aggiunti percorsi di recupero selettivi per candidati FT4 deboli o marginali;
  - introdotto fallback LDPC aggiuntivo con `norder=4`, attivo solo quando il profilo hardware e il budget temporale lo consentono;
  - aggiunto retry mirato per candidati a bassa frequenza audio, utile sui segnali validi vicini alla parte bassa della finestra;
  - ampliata in modo controllato la finestra DT tardiva per candidati FT4 selezionati;
  - mantenuti budget e deadline interni per evitare che i recuperi extra ritardino il rilascio dei decode;
  - protetti i percorsi piu costosi con guardie CPU: i PC lenti restano sul profilo leggero, mentre i PC veloci possono usare il recupero profondo;
  - aggiunti parametri ambiente diagnostici per disabilitare, forzare o misurare i nuovi percorsi FT4 durante i test.

- Messaggi e deduplica FT4:
  - migliorata la gestione dei messaggi composti separati da `;`;
  - separazione automatica di due decode FT4 validi quando arrivano fusi nella stessa riga;
  - normalizzazione piu robusta dei token hash risolti nei messaggi FT4.

- Packaging Windows:
  - aggiornati i riferimenti versione a `1.0.415`;
  - aggiunta pulizia completa della directory di installazione durante la disinstallazione Inno Setup;
  - in installazione per-utente, la rimozione pulisce anche `C:\Users\<utente>\AppData\Local\Programs\Decodium`;
  - lo script NSIS legacy resta allineato alla stessa versione.

## Compatibilita e timing

- I percorsi FT4 piu pesanti sono condizionati dal numero di thread hardware e da budget/deadline interni.
- Su CPU lente o single-core Decodium evita automaticamente i recuperi piu costosi.
- Il profilo di rilascio dei decode e' stato mantenuto entro la finestra temporale prevista dai test locali.

## Validazione locale

- `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`
- `git diff --check`

## Asset attesi

- Source code archive generato da GitHub per `v1.0.415`.
- Windows x64:
  - `Decodium_1.0.415_Setup_x64.exe`
- macOS Apple Silicon:
  - `decodium4-ft2-1.0.415-macos-tahoe-arm64.dmg`
  - `decodium4-ft2-1.0.415-macos-sequoia-arm64.dmg`
- macOS Intel:
  - `decodium4-ft2-1.0.415-macos-ventura-x86_64.dmg`
  - `decodium4-ft2-1.0.415-macos-sonoma-x86_64.dmg`
  - `decodium4-ft2-1.0.415-macos-sequoia-x86_64.dmg`
- Linux:
  - `decodium4-ft2-1.0.415-linux-x86_64.AppImage`
  - `decodium4-ft2-1.0.415-linux-aarch64.AppImage`
- File SHA256 generati dai runner macOS e Linux.
