# Guida Web App (Neofiti) - Italiano

Questa guida spiega, passo-passo, come attivare e usare la dashboard web remota di Decodium da telefono/tablet/PC nella stessa rete locale.

## 1) Requisiti

- Decodium `v1.4.3` in esecuzione sul computer di stazione.
- Computer di stazione e telefono/tablet collegati alla stessa LAN/Wi-Fi.
- App Decodium principale gia' funzionante (CAT/audio/decode).

## 2) Attivare la Dashboard Web Remota in Decodium

1. Apri Decodium.
2. Vai in `Impostazioni` -> `Generale`.
3. Cerca la sezione: `ATTENZIONE: SEZIONE SPERIMENTALE` / `Remote Web Dashboard (LAN)`.
4. Abilita: `Enable remote web dashboard`.
5. Imposta i campi:
   - `HTTP port`: default `19091` (consigliato).
   - `WS bind address`: usa `0.0.0.0` per accesso da LAN.
   - `Username`: lascia `admin` (o imposta il tuo).
   - `Access token (password)`: imposta una password robusta.
6. Premi `OK`.
7. Riavvia Decodium (consigliato quando cambi questi parametri).

## 3) Trovare l'IP del computer di stazione

Dal terminale del computer di stazione:

- macOS/Linux:
```bash
ipconfig getifaddr en0
```
(se vuoto, prova `en1`)

Esempio risultato: `192.168.1.4`

## 4) Aprire la dashboard da un altro dispositivo

Dal browser del telefono/tablet apri:

```text
http://<ip-stazione>:19091
```

Esempio:

```text
http://192.168.1.4:19091
```

Effettua login con:

- Username: quello impostato in Decodium (default `admin`)
- Password: il tuo `Access token`

## 5) Aggiunta alla schermata home (PWA)

- iPhone/iPad (Safari): Condividi -> `Aggiungi alla schermata Home`
- Android (Chrome): menu -> `Installa app` o `Aggiungi a schermata Home`

Dopo l'installazione puoi avviarla come un'app normale.

## 6) Cosa puoi fare dalla web app

- Vedere stato live stazione: modo, banda, dial, frequenze RX/TX, stato TX/monitor.
- Cambiare modalita' e banda.
- Impostare frequenza RX.
- Abilitare/disabilitare TX.
- Abilitare/disabilitare Auto-CQ.
- Inviare azione remota slot-aware verso chiamante.
- Visualizzare tabella attivita' RX/TX (con pulsanti pausa/pulisci).
- Usare waterfall remota opzionale (sperimentale).

## 7) Raccomandazioni sicurezza (importante)

- Mantieni il servizio solo in LAN.
- Usa un token/password forte.
- Non esporre la porta dashboard su Internet/router se non sai esattamente cosa stai facendo.
- Se possibile limita firewall ai soli dispositivi locali fidati.

## 8) Risoluzione problemi rapida

### La dashboard non si apre

- Verifica che Decodium sia in esecuzione.
- Verifica che `Enable remote web dashboard` sia attivo.
- Verifica IP/porta corretti.
- Verifica `WS bind address = 0.0.0.0` per uso LAN.
- Riavvia Decodium dopo modifiche remote.

### Login fallito

- Ricontrolla username/password nelle impostazioni Decodium.
- Evita spazi finali in username/token.
- Salva impostazioni e riavvia Decodium.

### Funziona sul PC ma non dal telefono

- I dispositivi potrebbero essere su VLAN/rete guest diverse.
- Firewall locale potrebbe bloccare la porta `19091`.
- Verifica che l'IP della stazione non sia cambiato (DHCP).

### Web app connessa ma i comandi non applicano

- Verifica che Decodium sia in monitor attivo.
- Verifica che il CAT sia operativo nell'app principale.
- Se la modalita' non e' FT2, i controlli FT2-only possono essere nascosti/limitati.

## 9) Endpoint API (opzionale avanzato)

- Health:
```text
GET /api/v1/health
```
- Comandi:
```text
POST /api/v1/commands
```

Gli endpoint usano la stessa autenticazione della dashboard web.
