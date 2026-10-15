# Decodium 4 FT2 1.0.482

## Italiano

Punti principali (`1.0.481 -> 1.0.482`):

- **Impostazioni fuori dal registro di Windows**:
  - le impostazioni non vengono piu' salvate in `HKCU\Software\Decodium\...` ma in file INI sotto un'unica
    radice `Decodium` (`%APPDATA%\Decodium\`). Motivo: i valori nel registro sopravvivevano alla
    disinstallazione e potevano rendere l'app inutilizzabile — un `uiSpectrumHeight` fuori scala, salvato
    quando il pannello era molto alto, rendeva la barra dello spettro/cascata inservibile per sempre.
  - **migrazione automatica al primo avvio**: le impostazioni esistenti vengono trasferite dal registro ai
    file INI e le chiavi legacy rimosse. Non serve riconfigurare nulla.
  - i dati utente passano da `%APPDATA%\IU8LMC\...` a `%APPDATA%\Decodium\...` mantenendo database dei QSO,
    cache e configurazioni multiple (MultiSettings) esistenti.
  - Nota: la lettura delle porte COM continua a usare il registro di Windows (`HARDWARE\DEVICEMAP\SERIALCOMM`),
    perche' e' un dato di sistema e non un'impostazione dell'applicazione.

- **Disinstallazione pulita**:
  - alla disinstallazione le impostazioni vengono sempre rimosse; per i dati personali (database dei QSO,
    cache, log) viene chiesta conferma esplicita, con messaggio tradotto in tutte le lingue dell'app.
  - la richiesta NON compare durante gli aggiornamenti (che disinstallano la versione precedente in
    automatico): aggiornando, impostazioni e log dei QSO restano intatti.
  - l'installer non scrive piu' alcuna chiave propria nel registro.

- **Barra spettro/cascata bloccata (risolto)**:
  - il limite massimo dell'altezza dello spettro era un valore fisso, scollegato dall'altezza reale del
    pannello: l'altezza memorizzata poteva finire molto oltre il massimo mostrabile e da quel momento
    trascinare la barra non produceva alcun effetto visibile. Ora il limite segue l'altezza reale del
    pannello e il controllo si ripristina da solo al primo trascinamento.

- **Percorso del log stabile**:
  - il file diagnostico veniva risolto ad ogni scrittura mentre l'identita' dell'applicazione cambiava a
    runtime, finendo scritto in due percorsi diversi nella stessa sessione. Ora il percorso e' risolto una
    sola volta: un unico log sotto `%LOCALAPPDATA%\Decodium\`.

- **Ricerca QRZ.com dalla lista decodifiche**:
  - click destro su un nominativo decodificato apre la sua scheda su qrz.com nel browser. Nelle viste in cui
    il click destro imposta gia' la frequenza RX (Full Spectrum a colonne, Signal RX), si usa Shift + click
    destro. Viene usato il nominativo base, cosi' i portable (`.../P`) risolvono sulla scheda dell'operatore.

## English

Release highlights (`1.0.481 -> 1.0.482`):

- **Settings moved out of the Windows registry**: settings are now stored in INI files under a single
  `Decodium` root instead of `HKCU\Software\Decodium\...`. Registry values used to survive uninstallation and
  could permanently break the UI (an out-of-range stored spectrum height made the waterfall splitter
  unusable). Existing settings are **migrated automatically on first run** and the legacy keys removed; user
  data moves from `%APPDATA%\IU8LMC\...` to `%APPDATA%\Decodium\...` keeping the QSO database, caches and
  MultiSettings configurations. COM port enumeration still reads the Windows registry, as that is system
  data rather than an application setting.

- **Clean uninstall**: settings are always removed; personal data (QSO database, caches, logs) is only
  removed after explicit confirmation, localized in every supported language. The prompt does not appear
  during upgrades, so updating never touches your settings or QSO log. The installer no longer writes any
  registry key of its own.

- **Fixed a stuck spectrum/waterfall splitter**: the spectrum height cap was a fixed value unrelated to the
  actual panel height, so the stored value could exceed what is renderable and dragging the bar had no
  visible effect. The cap now tracks the real panel height and the control self-heals on the first drag.

- **Stable log path**: the diagnostic log path was resolved on every write while the application identity
  changed at runtime, producing two log files in one session. It is now resolved once, under
  `%LOCALAPPDATA%\Decodium\`.

- **QRZ.com lookup from the decode list**: right-click a decoded callsign to open its qrz.com page. In views
  where right-click already sets the RX frequency (Full Spectrum columns, Signal RX), use Shift + right-click.
