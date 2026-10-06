# Decodium 4 v1.0.668

## English (UK)

This release brings Decolink to Decodium: use a radio that sits anywhere behind a Decolink server, with several operators sharing it safely. It also stops FT2 from logging QSOs with stations that never answered.

### Decolink: a remote radio through the server

- New window (main menu, next to DecoPort: "Decolink - remote radio through the server"): sign in with your Decolink account, pick a station, and see its role (owner, operator or listener), frequency, mode, S-meter and round-trip time. The password is kept in the system secure store, never in the settings file.
- "Use the remote radio" replaces the local sound card and CAT: Decodium decodes the station's audio and drives its frequency, mode and PTT, and transmits through it. If the link drops, the local sound card comes back by itself. A listener account can only receive; transmit is refused before the PTT is raised.
- Audio profile selector: follow the station (default), PCM 48 kHz, or "Digital, lossless (12 kHz)", the Decolink v3 profile for digital modes: bit-exact audio at about a third of the PCM bandwidth, lost blocks are requested again and recovered, and the transmit audio travels the same way.
- Several operators on the same radio, MultiFLEX style: one transmitter at a time, first come first served. While someone transmits, the others can watch (frequency, mode, S-meter) but cannot key the PTT, release it or retune. Decodium shows who is transmitting and refuses to transmit with a clear message. This needs the Decolink relay updated on the server; with an older relay everything works as before.
- The installer asks whether to download and install the Decolink gateway (pinned release, SHA-256 checked) for the PC that sits next to the radio.
- DecoPort on the local network and Decolink now sit behind the same remote-radio interface. Transmitting through a remote radio no longer needs a local sound card.
- Not yet tested on air. The Opus voice and CW profiles and the CW key of Decolink v3 are not included.

### FT2: QSOs logged without the other station's reply

- With the radio hearing its own transmission, the decoder reads our own messages at +20 to +28 dB (for example `IW8AOF IQ8DO -06`). Multi-Answer Mode accepted any decode that contained our call in any position, so it took that echo for the other station's reply and the QSO advanced to RR73 and was logged after the station had sent only its grid. In a measured period, 34 of 168 FT2 QSOs (20%) had no confirmation at all from the other station. FT8 was not affected.
- A message whose second element is our call and whose first is not is now recognised as ours and ignored. Messages addressed to us, including non-standard calls and portable suffixes, still pass. The same check now applies to the single-QSO auto-sequencer in FT2, FT4 and FT8.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Questa versione porta Decolink in Decodium: si può usare una radio che sta dietro un server Decolink, da qualunque posto, con più operatori che la dividono in sicurezza. Impedisce inoltre a FT2 di registrare QSO con stazioni che non hanno mai risposto.

### Decolink: una radio remota tramite il server

- Nuova finestra (menu principale, accanto a DecoPort: "Decolink - radio remota tramite il server"): accedi con il tuo account Decolink, scegli una stazione e vedi ruolo (titolare, operatore o ascoltatore), frequenza, modo, S-meter e tempo di andata e ritorno. La password sta nel deposito sicuro del sistema, mai nel file delle impostazioni.
- "Usa la radio remota" sostituisce scheda audio e CAT locali: Decodium decodifica l'audio della stazione, ne comanda frequenza, modo e PTT e trasmette attraverso di lei. Se il collegamento cade, la scheda audio locale torna da sola. Un account da ascoltatore può solo ricevere; la trasmissione viene rifiutata prima di alzare il PTT.
- Selettore del profilo audio: segui la stazione (predefinito), PCM 48 kHz, oppure "Digitale, senza perdite (12 kHz)", il profilo v3 di Decolink per i modi digitali: audio identico campione per campione con circa un terzo della banda del PCM, blocchi persi richiesti di nuovo e recuperati, e anche l'audio da trasmettere viaggia così.
- Più operatori sulla stessa radio, alla MultiFLEX: un solo trasmettitore alla volta, primo arrivato primo servito. Mentre qualcuno trasmette gli altri possono guardare (frequenza, modo, S-meter) ma non possono alzare il PTT, abbassarlo o cambiare frequenza. Decodium mostra chi trasmette e rifiuta di trasmettere con un messaggio chiaro. Serve il relay Decolink aggiornato sul server; con un relay vecchio tutto funziona come prima.
- L'installer chiede se scaricare e installare il gateway Decolink (versione fissata, con verifica SHA-256) per il PC accanto alla radio.
- DecoPort in rete locale e Decolink stanno ora dietro un'unica interfaccia di radio remota. Per trasmettere attraverso una radio remota non serve più una scheda audio locale.
- Non ancora provato in aria. I profili Opus per voce e CW e il tasto CW di Decolink v3 non sono inclusi.

### FT2: QSO registrati senza la risposta dell'altra stazione

- Quando la radio risente la propria trasmissione, il decoder legge i nostri messaggi a +20/+28 dB (per esempio `IW8AOF IQ8DO -06`). Il Multi-Answer Mode accettava qualunque decodifica contenente il nostro nominativo in una posizione qualsiasi, e scambiava quell'eco per la risposta dell'altra stazione: il QSO avanzava fino all'RR73 e veniva registrato dopo che la stazione aveva mandato solo il locatore. In un periodo misurato, 34 QSO FT2 su 168 (20%) non avevano alcuna conferma dell'altra stazione. L'FT8 non era interessato.
- Un messaggio il cui secondo elemento è il nostro nominativo e il primo no viene ora riconosciuto come nostro e ignorato. I messaggi diretti a noi, compresi i nominativi non standard e i suffissi portatili, passano ancora. Lo stesso controllo vale ora anche per l'auto-sequencer del singolo QSO in FT2, FT4 e FT8.

### Download

Gli archivi ZIP e tar.gz del sorgente sono disponibili per questo tag. L'installer EXE per Windows x64 è allegato a questa release; i workflow di GitHub aggiungono gli AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
