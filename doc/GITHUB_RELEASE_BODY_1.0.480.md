# Decodium 4 FT2 1.0.480

## English

Release highlights (`1.0.479 -> 1.0.480`):

- CAT failure dialog behavior:
- changed repeated CAT failure popups into a one-shot operator warning per application session.
- kept the diagnostic log behavior intact: repeated CAT/Hamlib failures still go to the diagnostic log for troubleshooting.
- improved the CAT failure dialog layout on macOS by widening the dialog and moving the OK action into the content layout, avoiding text/button overlap.
- kept native CAT false-positive filtering in place when the legacy backend reports Hamlib/serial errors while another CAT backend owns the rig.

- Toolbar / Wait & Pounce UI:
- removed the previously reintroduced `W&P` toolbar button from the TX toolbar.
- removed `waitpounce` from the persisted toolbar button model so saved layouts that still contain the old ID silently drop it.
- kept the Wait & Pounce backend/settings support available; only the toolbar button was removed.

- Build and validation:
- updated the fork release version to `1.0.480`.
- verified the QML build after the UI changes.

Validation performed locally:

- `git diff --check -- qml/decodium/Main.qml qml/decodium/components/TxPanel.qml`
- `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`

Release assets expected from GitHub Actions:

- `Decodium_1.0.480_Setup_x64.exe`
- `decodium4-ft2-1.0.480-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.480-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.480-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.480-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.480-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.480-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.480-linux-aarch64.AppImage`
- matching `.zip` and checksum files where produced by the platform workflow.

## Italiano

Punti principali (`1.0.479 -> 1.0.480`):

- Finestra CAT failure:
- trasformati i popup CAT failure ripetuti in un solo avviso per sessione applicativa.
- mantenuto invariato il diagnostic log: gli errori CAT/Hamlib ricorrenti continuano a essere registrati per debug.
- migliorato il layout della finestra CAT failure su macOS, allargando il dialog e spostando il pulsante OK nel layout del contenuto per evitare sovrapposizioni testo/pulsante.
- mantenuto il filtro dei falsi positivi CAT nativi quando il backend legacy segnala errori Hamlib/serial mentre un altro backend CAT gestisce il rig.

- Toolbar / Wait & Pounce:
- rimosso dalla toolbar TX il pulsante `W&P` precedentemente reintrodotto.
- rimosso `waitpounce` dal modello persistente dei pulsanti toolbar, cosi' eventuali layout salvati con il vecchio ID lo scartano automaticamente.
- mantenuto disponibile il supporto Wait & Pounce nel backend e nelle impostazioni; e' stato rimosso solo il pulsante toolbar.

- Build e validazione:
- aggiornata la versione fork a `1.0.480`.
- verificata la build QML dopo le modifiche UI.

Validazione locale eseguita:

- `git diff --check -- qml/decodium/Main.qml qml/decodium/components/TxPanel.qml`
- `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`

Asset attesi dai runner GitHub Actions:

- `Decodium_1.0.480_Setup_x64.exe`
- `decodium4-ft2-1.0.480-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.480-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.480-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.480-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.480-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.480-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.480-linux-aarch64.AppImage`
- relativi `.zip` e checksum dove prodotti dal workflow della piattaforma.
