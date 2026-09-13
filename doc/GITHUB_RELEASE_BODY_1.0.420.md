## Decodium 4 FT2 1.0.420

Questa release include gli aggiornamenti integrati dalla 1.0.417 alla 1.0.420.

### Decoder e stabilita

- Aggiornata la base locale con le modifiche 1.0.418 e 1.0.419.
- Migliorata la gestione del ciclo di vita del worker FT4 durante chiusura, cambio modo e reset decode.
- Aggiunta protezione contro callback FT4 tardive: i risultati fuori finestra vengono ignorati e loggati invece di aggiornare lista decode, dashboard o stato TX.
- Pulite le metriche di sessione decode anche durante reset time-sync, riducendo il rischio di risultati obsoleti dopo cambio modo o riavvio del ciclo.
- Esteso il logging FT4 con seriale decode nelle metriche, utile per diagnosi dei casi di callback stale.

### LoTW e dati nominativi

- Corretto il caricamento LoTW quando l'opzione e attiva: il database viene caricato anche da impostazioni persistenti e non solo dopo toggle manuale.
- Corretto il parser della cache LoTW: viene usata la prima colonna CSV del file ufficiale, evitando confronti errati sull'intera riga.
- Dopo caricamento o aggiornamento LoTW, le righe decode vengono aggiornate subito senza richiedere riavvio.
- Aggiunto contatore utenti LoTW esposto alla UI.
- Aggiunto comando di aggiornamento forzato LoTW in Setup -> Advanced.

### US States

- Aggiunto comando di aggiornamento forzato per i dati US States in Setup -> Advanced.
- Migliorato l'allineamento della voce US State nelle impostazioni Display: checkbox, contatore e pulsante Update stanno sulla stessa riga.
- Il conteggio dei dati US State ora resta vicino alla relativa opzione e non interferisce con Show DXCC.

### Interfaccia e traduzioni

- I profili pronti sono stati normalizzati con testi sorgente in inglese.
- Tradotti i testi dei profili pronti, descrizioni e stato attivo per tutte le lingue presenti nel progetto.
- Aggiornate le stringhe toolbar dei profili pronti: Balanced, Profiles..., tooltip e indicatore active.
- Mantenute le traduzioni per catalano, danese, tedesco, inglese, spagnolo, francese, ungherese, italiano, giapponese, lettone, russo, cinese semplificato e cinese tradizionale.

### Aggiornamenti upstream inclusi

- 1.0.418: collegamento audio CW al bridge e soglia SWR configurabile.
- 1.0.419: completamento traduzioni UI per tedesco, spagnolo, francese, italiano e lettone.

### Asset previsti

- Codice sorgente GitHub per `v1.0.420`.
- Installer Windows x64.
- Pacchetti macOS Apple Silicon.
- Pacchetti macOS Intel.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
