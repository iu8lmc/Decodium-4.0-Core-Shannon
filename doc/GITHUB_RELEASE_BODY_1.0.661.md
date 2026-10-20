# Decodium 4 v1.0.661

## English (UK)

This release removes the native CAT backend, fixes a split command that was never sent, and stops the click heard after every logged QSO in FT2.

### The native CAT backend is gone

- The "Native (QSerialPort, 15 radios)" backend has been removed. Hamlib, OmniRig, TCI and Cat4OM remain.
- If you used it, nothing needs to be redone: at the first start your radio, serial port, speed, PTT and the other CAT fields are copied from the native settings to the Hamlib settings and the backend becomes Hamlib. This happens once.
- A saved CAT profile that used the native backend is not converted: it opens with the Hamlib settings stored in that profile.
- Rig and CAT errors that used to be hidden while the native backend was active are now shown, as they are with Hamlib.

### CAT: TX/split command with an unchanged RX frequency

- With Split set to Rig, the TX frequency command was skipped whenever the RX frequency was already on target, while the PTT request that follows it was still sent. The radio could transmit on the previous TX frequency. The command is now sent before the PTT. A new test drives the real transceiver base class with a recording rig.

### FT2: the click after every logged QSO

- With "Send station + weather info" on, the telemetry started right after the log, in the middle of the slot. In FT2 with asynchronous TX the audio was already past its end, so the PTT closed for about 0.4 s with nothing played: the tick heard between the log and the next TX. It now waits for the start of the next slot.
- When the log fell on a slot boundary, the just-completed MAM slot (RR73) could be transmitted instead of the telemetry. That no longer happens.
- Note: in FT2 the telemetry is now really transmitted after each QSO, about 2.5 s of TX. Switch the option off in the settings if you do not want it.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Questa versione toglie il backend CAT nativo, corregge un comando di split che non veniva mai inviato e fa sparire il ticchettio che si sentiva dopo ogni QSO registrato in FT2.

### Il backend CAT nativo non c'è più

- Il backend "Native (QSerialPort, 15 radio)" è stato rimosso. Restano Hamlib, OmniRig, TCI e Cat4OM.
- Se lo usavi non devi rifare nulla: al primo avvio radio, porta seriale, velocità, PTT e gli altri campi CAT vengono copiati dalle impostazioni native a quelle di Hamlib e il backend diventa Hamlib. Succede una volta sola.
- Un profilo CAT salvato con il backend nativo non viene convertito: si apre con le impostazioni Hamlib memorizzate in quel profilo.
- Gli errori di rig e CAT che prima venivano nascosti con il backend nativo ora si vedono, come con Hamlib.

### CAT: comando TX/split con frequenza RX invariata

- Con Split impostato su Rig, il comando della frequenza TX veniva saltato quando la frequenza RX era già quella giusta, mentre la richiesta di PTT che lo segue partiva comunque. La radio poteva trasmettere sulla frequenza TX precedente. Ora il comando parte prima del PTT. Un nuovo test usa la vera classe base del transceiver con una radio che registra i comandi.

### FT2: il ticchettio dopo ogni QSO registrato

- Con "Send station + weather info" attivo, la telemetria partiva subito dopo il log, a metà slot. In FT2 con TX asincrono l'audio era già oltre la sua fine e il PTT si chiudeva per circa 0,4 s senza suonare nulla: il ticchettio fra il log e il TX successivo. Ora aspetta l'inizio dello slot successivo.
- Quando il log cadeva sul confine di slot, al posto della telemetria poteva uscire lo slot MAM appena chiuso (RR73). Non succede più.
- Nota: in FT2 la telemetria ora viene davvero trasmessa dopo ogni QSO, circa 2,5 s di TX. Se non la vuoi, disattiva l'opzione nelle impostazioni.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
