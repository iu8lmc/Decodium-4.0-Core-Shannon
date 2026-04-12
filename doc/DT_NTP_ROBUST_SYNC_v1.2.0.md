# DT/NTP Robust Sync Architecture / Architettura Sync DT/NTP - Fork v1.2.0

Date / Data: 2026-02-26

---

## English

## Context

Real FT2 operation is affected by:

- system clock drift,
- audio-device drift,
- CAT/PTT latency variability,
- sparse/inconsistent NTP server sets.

v1.2.0 introduces a lock-preserving strategy: avoid false timing jumps while still tracking real drift.

## Design rules

1. TX slot scheduling stays tied to raw system time.
2. NTP drives sync/correction logic, not direct hard phase injection in slot scheduler.
3. Sparse sets (`2-3` servers) cannot cause immediate medium/large jumps.
4. DT correction remains conservative when NTP health is good.

## Pipeline

### A) NTP sample handling

- Query randomized server subset.
- Validate packet and originate timestamp echo.
- Reject high RTT and absurd offsets.

### B) Coherence/cluster filtering

- Build densest offset cluster.
- Tighten around lock window when lock exists.
- Reject incoherent sets.

### C) Weak-sync gate

- Deadband,
- standard confirmation count,
- strong-step confirmation count,
- emergency threshold for unavoidable large corrections.

### D) Sparse jump guard

With lock active and low server count:

- jumps over threshold require repeated same-direction confirmations before apply.

### E) Hold mode

On temporary no-response cycles:

- keep lock state,
- avoid abrupt "No sync" collapse,
- delay coarse fallback when prior lock is still credible.

### F) DT correction behavior

- adaptive EMA,
- step clamp + total clamp,
- decay when sample flow is poor,
- reduced aggressiveness when NTP offset is healthy.

## Field-tested defaults promoted in v1.2.0

- `FT2_NTP_WEAK_SYNC=1`
- `FT2_NTP_WEAK_STRONG_MS=350`
- `FT2_NTP_WEAK_STRONG_CONFIRM=4`
- `FT2_NTP_SPARSE_JUMP_MS=50`
- `FT2_NTP_SPARSE_JUMP_CONFIRM=4`

These values are now default in code and no longer require manual startup env overrides.

## Difference vs original Raptor baseline

- Lock-aware hold state management.
- Sparse-jump confirmation layer.
- Stronger coherence filtering before applying offsets.
- Safer behavior on temporary NTP degradation.
- DT color semantics include correction saturation, not only instantaneous DT.

Operational tradeoff: slightly slower adaptation, much lower false-jump rate.

---

## Italiano

## Contesto

L'operativita FT2 reale e influenzata da:

- drift clock di sistema,
- drift scheda audio,
- variabilita latenza CAT/PTT,
- set NTP sparsi/incoerenti.

La v1.2.0 introduce una strategia che preserva il lock: evita salti temporali falsi ma segue comunque il drift reale.

## Regole di progetto

1. Lo scheduling slot TX resta agganciato al tempo grezzo di sistema.
2. NTP guida logica sync/correzione, non inietta fase hard nello scheduler slot.
3. Set sparsi (`2-3` server) non possono causare salti medi/grandi immediati.
4. La correzione DT resta conservativa quando la salute NTP e buona.

## Pipeline

### A) Gestione campioni NTP

- Query su subset random di server.
- Validazione pacchetto ed echo timestamp originate.
- Scarto RTT elevato e offset assurdi.

### B) Filtro coerenza/cluster

- Costruzione cluster offset piu denso.
- Restrizione attorno finestra lock quando il lock esiste.
- Scarto set incoerenti.

### C) Gate weak-sync

- Deadband,
- conferme standard,
- conferme per step forte,
- soglia emergenza per correzioni grandi inevitabili.

### D) Guardia sparse jump

Con lock attivo e pochi server:

- i salti sopra soglia richiedono conferme ripetute nella stessa direzione prima dell'applicazione.

### E) Hold mode

Su cicli senza risposta temporanei:

- mantiene stato lock,
- evita crollo brusco a "No sync",
- ritarda fallback grossolani se il lock precedente e ancora credibile.

### F) Correzione DT

- EMA adattiva,
- clamp per step + clamp totale,
- decay quando i campioni scarseggiano,
- aggressivita ridotta quando offset NTP e sano.

## Default validati sul campo promossi in v1.2.0

- `FT2_NTP_WEAK_SYNC=1`
- `FT2_NTP_WEAK_STRONG_MS=350`
- `FT2_NTP_WEAK_STRONG_CONFIRM=4`
- `FT2_NTP_SPARSE_JUMP_MS=50`
- `FT2_NTP_SPARSE_JUMP_CONFIRM=4`

Questi valori sono ora default nel codice e non richiedono piu override manuali all'avvio.

## Differenze rispetto alla baseline Raptor originale

- Gestione stato hold lock-aware.
- Livello conferme su sparse-jump.
- Filtro coerenza piu forte prima di applicare offset.
- Comportamento piu sicuro su degrado NTP temporaneo.
- Semantica colori DT include saturazione correzione, non solo DT istantaneo.

Tradeoff operativo: adattamento leggermente piu lento, ma forte riduzione dei salti falsi.
