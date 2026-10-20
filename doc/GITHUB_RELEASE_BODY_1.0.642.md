# Decodium 4 v1.0.642

## English (UK)

A translation correction on top of v1.0.641.

### "TX Delay (s)": the (s) means seconds

- The `(s)` in the English label is the unit of measurement, not a plural marker. Four catalogues had read it as a plural, producing labels that mean nothing: Italian showed `TX Ritardo/i:`, German `TX Verzögerung(en):`, Spanish `TX Retraso(s):` and Danish `TX Forsinkelse (r):`.
- They now read `Ritardo TX (s):`, `TX-Verzögerung (s):`, `Retardo TX (s):` and `TX-forsinkelse (s):`. The string appears twice in the interface — in the settings dialog and in the TX page — and both places are corrected.
- The compiled catalogues were regenerated: without that step the source files change and the interface stays as it was.
- The setting itself is unchanged: it is the delay, in seconds, between keying the transmitter and the start of the audio. The default is 0.2 s.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Una correzione di traduzione sopra la 1.0.641.

### «TX Delay (s)»: quella (s) sono i secondi

- La `(s)` dell'etichetta inglese è l'unità di misura, non il segno del plurale. Quattro cataloghi l'avevano letta come plurale, producendo etichette che non vogliono dire niente: in italiano si leggeva `TX Ritardo/i:`, in tedesco `TX Verzögerung(en):`, in spagnolo `TX Retraso(s):` e in danese `TX Forsinkelse (r):`.
- Ora si leggono `Ritardo TX (s):`, `TX-Verzögerung (s):`, `Retardo TX (s):` e `TX-forsinkelse (s):`. La stringa compare due volte nell'interfaccia — nella finestra delle impostazioni e nella pagina TX — ed è corretta in entrambe.
- I cataloghi compilati sono stati rigenerati: senza quel passaggio i file sorgente cambiano e l'interfaccia resta com'era.
- L'impostazione non cambia: è il ritardo, in secondi, fra la messa in trasmissione e l'inizio dell'audio. Il valore predefinito è 0,2 s.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
