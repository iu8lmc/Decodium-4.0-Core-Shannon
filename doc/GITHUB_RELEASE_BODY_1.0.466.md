# Decodium 4 FT2 1.0.466

Release incrementale dalla 1.0.465 alla 1.0.466, centrata su FT2-Link, stabilita del cambio modo, diagnostica waveform/RX e rifiniture di chiusura applicazione.

## FT2-Link RX and waveform

- Migliorata l'acquisizione NARROW con stima del centro burst e retry attorno al centro stimato.
- Aggiunto test dedicato per ricezione NARROW con offset ampio, utile per verificare tolleranza a drift e disallineamento audio.
- Rafforzati i log RX FT2-Link con eventi `RXBUSY`, `RXFAIL` e `RXDECODE`, inclusi buffer attivi W500/W2300/NARROW e motivi di mancata acquisizione.
- Estesi i log TX con piano segnale, profilo, centro, low/high, toni, carrier, durata, frame e burst.

## Mode switching and legacy decoder isolation

- Corretto il problema per cui, passando da FT2 a FT2-Link, il backend legacy continuava a eseguire decode FT2 in background.
- Il decoder FT2 legacy ora viene abilitato solo quando il modo applicativo e realmente `FT2`.
- FT2-Link puo continuare a usare la base radio/audio FT2 per panadapter, waterfall e backend legacy, ma senza far partire decode FT2 legacy.
- Le code Qt del worker FT2 vengono svuotate quando il decode viene disabilitato, evitando decode gia accodati dopo il cambio modo.
- Il cambio da FT2 verso qualunque altro modo ora ferma timer async, pending audio e decode FT2 residuo.

## FT2-Link UI

- Il selettore modo inferiore mantiene `FT2-Link` visibile dopo lo sblocco e non torna graficamente a `FT2`.
- Migliorata la sincronizzazione del mode selector rispetto al modo applicativo reale.
- Preservata la separazione tra modo mostrato all'utente e modo base usato internamente dal backend legacy.

## Shutdown and macOS behavior

- Ridotte le attese in chiusura quando non c'e una trasmissione reale da fermare.
- Evitato il rilascio PTT Hamlib quando lo stato e gia idle e non esiste audio/TX attivo.
- Aggiunta chiusura esplicita dei loader QML asincroni prima del quit per ridurre warning e ritardi a distruzione engine.
- Migliorata la gestione di stop TX/tune in presenza di backend legacy e bridge audio.

## Diagnostics and validation

- Aggiunto gate runtime nel worker FT2 per scartare risultati quando il decode viene disabilitato durante una decodifica gia in corso.
- Aggiunto controllo bridge/backend legacy per evitare che polling legacy riporti il modo applicativo da FT2-Link a FT2.
- Validati i test FT2-Link core e QML adapter dopo le modifiche.
- Build locale `decodium_qml` verificata su macOS.

## Release/build

- Versione fork aggiornata a `1.0.466`.
- Installer Inno/NSIS default allineati a `1.0.466`.
- Tag release: `v1.0.466`.
- Asset previsti dai runner GitHub:
  - Windows x64 installer `.exe`
  - macOS Apple Silicon `.dmg` e `.zip`
  - macOS Intel `.dmg` e `.zip`
  - Linux x86_64 AppImage
  - Linux aarch64 AppImage

## Notes

FT2-Link resta una funzione sperimentale/protetta per test controllati. Questa release migliora soprattutto affidabilita del cambio modo, isolamento dal decoder FT2 legacy e strumenti diagnostici per capire perche un segnale visibile nel panadapter non viene ancora decodificato.
