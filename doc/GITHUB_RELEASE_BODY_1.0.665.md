# Decodium 4 v1.0.665

## English (UK)

Test build: Decodium can now use a radio that sits behind a Decolink server, from anywhere.

### Decolink: remote radio through the relay

- New window (main menu, next to DecoPort: "Decolink - remote radio through the server"): sign in with your Decolink account, pick a station, and see its role (owner, operator or listener), frequency, mode, S-meter and round-trip time.
- "Use the remote radio" replaces the local sound card and CAT: Decodium decodes the station's audio and drives its frequency, mode and PTT. If the link drops, the local sound card comes back by itself.
- An account with the listener role can only receive; transmit is refused before the PTT is raised.
- The password is kept in the system secure store, never in the settings file.
- The installer asks whether to download and install the Decolink gateway (pinned release with SHA-256 check) for the PC that sits next to the radio.
- Same code path as DecoPort on the local network: both now sit behind one remote-radio interface.
- Fixes the transmit audio not reaching the remote radio (1.0.663 sent only the last chunk).
- New audio profile "Digital, lossless (12 kHz)": the Decolink v3 profile for digital modes. Bit-exact audio at about a third of the PCM bandwidth, lost blocks are requested again and recovered, and the transmit audio is sent the same way. The selector in the Decolink window offers: follow the station (default), PCM 48 kHz, digital lossless. Set the PCM profile if another app on the station needs it.
- Not yet tested on air; the Opus voice and CW profiles and the CW key are not included.

### Downloads

This is a test installer for the Windows x64 platform.

---

## Italiano

Versione di prova: Decodium può usare una radio che sta dietro un server Decolink, da qualunque posto.

### Decolink: radio remota tramite il relay

- Nuova finestra (menu principale, accanto a DecoPort: "Decolink - radio remota tramite il server"): accedi con il tuo account Decolink, scegli una stazione e vedi ruolo (titolare, operatore o ascoltatore), frequenza, modo, S-meter e tempo di andata e ritorno.
- "Usa la radio remota" sostituisce scheda audio e CAT locali: Decodium decodifica l'audio della stazione e ne comanda frequenza, modo e PTT. Se il collegamento cade, la scheda audio locale torna da sola.
- Un account con ruolo di ascoltatore può solo ricevere; la trasmissione viene rifiutata prima di alzare il PTT.
- La password sta nel deposito sicuro del sistema, mai nel file delle impostazioni.
- L'installer chiede se scaricare e installare il gateway Decolink (versione fissata, con verifica SHA-256) per il PC accanto alla radio.
- Stesso percorso di DecoPort in rete locale: ora stanno entrambi dietro un'unica interfaccia di radio remota.
- Corregge l'audio di trasmissione che non arrivava alla radio remota (la 1.0.663 mandava solo l'ultimo pezzo).
- Nuovo profilo audio "Digitale, senza perdite (12 kHz)": il profilo v3 di Decolink per i modi digitali. Audio identico campione per campione con circa un terzo della banda del PCM; i blocchi persi vengono richiesti di nuovo e recuperati, e anche l'audio da trasmettere viaggia così. Il selettore nella finestra Decolink offre: segui la stazione (predefinito), PCM 48 kHz, digitale senza perdite. Imposta il profilo PCM se un'altra app della stazione lo richiede.
- Non ancora provato in aria; i profili Opus per voce e CW e il tasto CW non sono inclusi.

### Download

Questo è un installer di prova per la piattaforma Windows x64.
