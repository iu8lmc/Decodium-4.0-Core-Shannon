# Decodium 4 FT2 1.0.465

Release cumulativa dalla 1.0.449 alla 1.0.465, centrata su FT2-Link, stabilita RX/TX, workflow di messaggistica dati, UI integrata e pipeline release multipiattaforma.

## FT2-Link mode

- Aggiunta e stabilizzata la modalita FT2-Link come modo selezionabile dalla lista modi, senza finestra esterna dedicata.
- Integrazione della UI FT2-Link nell'area principale RF/RX, mantenendo panadapter/waterfall e Live Map utilizzabili.
- Accesso FT2-Link protetto per test privato: password non salvata in chiaro e fallback automatico a FT2 normale se accesso non provisioned, annullato o errato.
- Supporto profili waveform NARROW, W500 e W2300, con selezione rapida W2300/FAST e basi per ROBUST/FAST.
- Estesa tolleranza AFC NARROW da circa +/-20 Hz a +/-50 Hz per reggere meglio flutter e drift di propagazione.
- LBT/listen-before-transmit, busy channel guard, timer CQ slot e gestione stato RF/QUEUE piu leggibile.

## FT2-Link protocol and reliability

- Workflow completo beacon/CQ, station list, connect, sessione P2P, ping, QSO, QSL/ADIF, canned messages e info requests.
- Chat affidabile su sessione dati con ACK, ritrasmissione, stato Delivered/Failed e log conversazione.
- ACK ripetuti per aumentare la probabilita di consegna dello stato finale sui trasferimenti affidabili.
- Deduplica dei file gia ricevuti con finestra estesa, ri-ACK dei duplicati e prevenzione di record duplicati in RX.
- Migliorata la logica dei messaggi live in entrambe le direzioni, incluso handling dei casi in cui il peer risponde con ACK mentre il lato opposto sta ancora processando.

## FT2-Link data features

- Broadcast e CQ broadcast.
- Mail, relay queue e mailbox unread.
- Form templates e invio form.
- BBS/bulletin path iniziale.
- File transfer testuale fino al payload massimo supportato.
- Received Files queue dedicata.
- Path/relay hint, frequency/QSY helpers, cluster last heard e contact timeline.
- Pannelli LOG, DB, PRE, FREQ, BLK, CLST e statistiche operative.

## Received files / RXF

- Nuova schermata RXF dedicata, indipendente dallo StackLayout fragile precedente.
- Lista file ricevuti con mittente, nome file, data, dimensione, preview, SAVE e COPY.
- Stato unread/read persistente per i file ricevuti.
- `QUEUE RXF n` e `RXF*` ora contano solo i file non letti.
- Pulsanti `READ` per singolo file e `MARK ALL READ`.
- `SAVE` e `COPY` marcano automaticamente il file come letto.
- I file letti restano visibili e salvabili nella lista RXF.

## UI fixes

- Pulizia delle sovrapposizioni nella UI FT2-Link.
- Migliorati controlli in alto a destra e checkbox compatte.
- Sistemati pulsanti CQ/ARM/AUTO, label inglesi e stati Connecting/Connected.
- Rimossi blocchi/placeholder poco chiari dalla zona file transfer.
- Migliorata la tabella stazioni e sessioni, inclusi fix per required property nei delegati QML.
- Aggiunte scrollbar dove servono e ridotta altezza di aree vuote inutili.
- File transfer UI ora usa selezione file cross-platform invece di campo testo manuale.

## Audio, diagnostics and stability

- Aggiunti log dettagliati TXPLAN/TXSIGNAL e piano audio FT2-Link con centro, low/high, toni/carrier, profilo e durata.
- Migliorati log radio TX requested/completed, buffer RX, busy channel, drift stimato e metriche live.
- Gating del decode FT2-Link per evitare lavoro DSP inutile quando non c'e energia utile.
- Ridotti freeze e stall legati all'audio tap sul main thread.
- Migliorata gestione RX suspended durante TX e ripresa monitor.
- Aggiunti controlli per evitare seek su device audio sequenziali dove non applicabile.

## Tests and CI

- Test FT2-Link core ampliati.
- Test QML adapter ampliati per chat, file, ACK ripetuto, deduplica, RXF unread/read e persistenza.
- Gate CI per FT2-Link su push/PR e su build release Windows.
- Fix CI Hamlib per Windows usando include/library espliciti.
- Windows release non firmata pubblicabile finche SignPath non e attivo.
- Workflow release disponibili per Windows x64, macOS Apple Silicon, macOS Intel, Linux x86_64 AppImage e Linux aarch64 AppImage.

## Release/build

- Versione fork aggiornata a `1.0.465`.
- Installer Inno/NSIS default allineati a `1.0.465`.
- Tag release: `v1.0.465`.
- Gli asset binari vengono caricati dai runner GitHub:
  - Windows x64 installer `.exe`
  - macOS Apple Silicon `.dmg` e `.zip`
  - macOS Intel `.dmg` e `.zip`
  - Linux x86_64 AppImage
  - Linux aarch64 AppImage

## Notes

FT2-Link resta una funzione sperimentale/protetta per test controllati. Il protocollo e la UI sono ora abbastanza completi per test end-to-end, ma ulteriori prove over-the-air sono necessarie prima di considerarlo stabile per uso generale.
