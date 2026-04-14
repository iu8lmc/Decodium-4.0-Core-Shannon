# FT2 Remote Tap Scheduler (Low-Latency Design)

## 1) Objective
Enable remote control (web/mobile dashboard) for FT2 without breaking timing.
The key action is: operator double-taps a caller remotely, Decodium schedules TX on the next valid FT2 slot.

## 2) Design Principles
- Never TX immediately from remote command arrival.
- Always convert remote intent into a local scheduled action.
- Slot decision is made only by local Decodium clock/scheduler.
- If command arrives too close to slot boundary, defer to next slot.
- Fast feedback to client: accepted/deferred/rejected, with exact scheduled slot.

## 3) Scope (MVP)
- Remote action: `select caller` (double tap).
- Optional actions: `cancel`, `enable tx`, `disable tx`, `set tx message profile`.
- Live telemetry over WebSocket (current mode/band/frequency, next slot UTC, queue status, decode summary, TX state).
- No remote waterfall streaming in MVP (add later).

## 4) Timing Model
- Slot period for FT2: use runtime mode profile (`slot_period_ms`, typical ~3750 ms).
- `guard_pre_ms`: minimum safe margin before slot start (recommended 300 ms).
- `guard_post_ms`: minimum hold after slot tick before accepting same-slot command (recommended 120 ms).
- `max_cmd_age_ms`: stale command limit (recommended 2 * slot period).

### Why this works
Internet RTT variation affects only "which slot" is selected, not TX integrity.
Worst case: command is deferred by one slot, but never sent late/corrupt.

## 5) Architecture
Components:
- `RemoteApiServer` (REST for config/read, WS for live control/events)
- `CommandInbox` (thread-safe queue, idempotent by `command_id`)
- `SlotScheduler` (single owner of slot decisions, local monotonic time)
- `TxExecutor` (existing TX pipeline, unchanged timing-critical behavior)

Threading:
- API thread never touches audio/modulator directly.
- API thread enqueues intent -> scheduler thread decides -> UI/TX thread executes via queued signal.

## 6) Command Contract
Client -> Server (`WS` or `POST /api/v1/commands`):

```json
{
  "command_id": "4eec8d47-8a5b-4ad5-8f72-9d0eb1f3f14f",
  "type": "select_caller",
  "target_call": "K1ABC",
  "target_grid": "FN31",
  "client_sent_ms": 1772395212345,
  "client_seq": 182
}
```

Server ACK:

```json
{
  "command_id": "4eec8d47-8a5b-4ad5-8f72-9d0eb1f3f14f",
  "status": "accepted_next_slot",
  "scheduled_slot_utc_ms": 1772395215000,
  "slot_index": 472998,
  "server_now_ms": 1772395212452
}
```

Possible statuses:
- `accepted_current_slot`
- `accepted_next_slot`
- `deferred_next_slot`
- `rejected_stale`
- `rejected_invalid_state`
- `rejected_not_authorized`

## 7) Slot Selection Algorithm
Given:
- `now_ms` from local monotonic/precise clock
- `slot_start_ms` = next slot boundary from mode profile

Rules:
1. Reject if duplicate `command_id` (idempotent replay).
2. Reject if `now_ms - command_receive_ms > max_cmd_age_ms`.
3. If `now_ms <= slot_start_ms - guard_pre_ms`: schedule current next slot.
4. Else: schedule `slot_start_ms + slot_period_ms` (defer one slot).
5. Emit ACK immediately with scheduled slot.
6. At slot open, execute only if preconditions still valid (mode=FT2, TX enabled, rig ready).

Pseudo:

```text
slot = next_slot(now)
if now > slot - guard_pre_ms:
  slot = slot + period
enqueue(slot, command)
ack(slot)
```

## 8) Latency Profile (practical)
- RTT 40-120 ms: usually next slot accepted.
- RTT 120-280 ms: mostly next slot, sometimes +1 slot near boundary.
- RTT > 300 ms or jitter spikes: more frequent deferral by one slot.

Operationally acceptable because action reliability is preserved.

## 9) UX Rules for Remote Dashboard
- Show countdown to next FT2 slot (ms precision not needed; 0.1 s is enough).
- Show command outcome instantly: "Queued for 22:14:07.500 UTC".
- Disable double-tap spam for same caller for 1 slot (client-side debounce).
- If deferred, show explicit reason: "too close to slot boundary".

## 10) Safety and Security
- Default mode: remote read-only.
- Separate permission for TX commands (`remote_tx_enable`).
- Strong auth (token + optional IP allowlist; TLS if exposed outside LAN).
- Rate limit for command endpoint.
- Full audit log: who sent what, at what time, scheduled slot, outcome.
- Emergency stop command always high priority.

## 11) Failure Handling
- WS disconnected: local Decodium keeps running, no TX interruption.
- Command queue overflow: reject with explicit error, do not block decoder/audio.
- Rig not ready at scheduled slot: skip TX and notify client event.
- Mode changed before slot (FT2 -> FT8): cancel scheduled command safely.

## 12) Observability
Expose metrics:
- command RTT estimate (client timestamp optional)
- accepted current slot vs deferred next slot ratio
- stale/invalid rejects
- slot miss count
- TX start drift from scheduled slot

These metrics quickly show if internet latency is acceptable for operator workflow.

## 13) Rollout Plan
Phase 1:
- WS telemetry + command inbox + slot scheduler for `select_caller`.

Phase 2:
- add `cancel`, `halt_tx`, queue viewer, dashboard countdown.

Phase 3:
- optional remote waterfall tiles/FFT stream with capped bandwidth.

## 14) Acceptance Criteria
- No direct TX trigger from network thread.
- Under RTT up to 250 ms, command success remains deterministic (current or next slot).
- No UI/audio thread stalls from remote API activity.
- Command replay does not create duplicate TX actions.
- In boundary conditions, system defers safely (never late-transmits into wrong slot).

## 15) Phase 1 Endpoints
- WebSocket:
- `ws://127.0.0.1:<FT2_REMOTE_WS_PORT>`
- Event stream: `hello`, `telemetry`, `command_ack`, `command_execute`
- Command: `select_caller`

- REST:
- `POST http://127.0.0.1:<FT2_REMOTE_HTTP_PORT>/api/v1/commands`
- `GET  http://127.0.0.1:<FT2_REMOTE_HTTP_PORT>/api/v1/health`
- `OPTIONS /api/v1/commands` for CORS preflight

- Env enablement:
- `FT2_REMOTE_WS_PORT=<port>` (required to enable remote control)
- `FT2_REMOTE_HTTP_PORT=<port>` (optional, default `WS+1`)
- `FT2_REMOTE_WS_BIND=127.0.0.1` (default localhost)
- `FT2_REMOTE_WS_TOKEN=<secret>` (optional shared token)
