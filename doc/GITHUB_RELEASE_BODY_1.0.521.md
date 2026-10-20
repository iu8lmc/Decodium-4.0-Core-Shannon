# Decodium 4.0 v1.0.521

Version 1.0.521 combines the multilingual rotator and Live Map refinements
introduced in 1.0.520 with a new network CAT backend and a substantial
reliability and usability update for FT2-Link interactive traffic and file
transfers.

## Changes from v1.0.519 to v1.0.521

### Network CAT integration

- A fifth CAT backend is available for connecting Decodium to a CAT4OM service
  through its native JSON-over-WebSocket protocol.
- The backend discovers radio groups and control ports through the management
  endpoint, selects the configured radio and can fall back to a directly
  configured control endpoint when management discovery is unavailable.
- Master/slave ownership is represented explicitly. Operator commands can be
  retained while ownership is requested, coalesced to avoid an unbounded
  queue and dispatched when control becomes available.
- Frequency, transmit frequency, operating mode, PTT, split state, active and
  transmit VFO, supported modes, available commands, power, SWR and ALC are
  integrated with the existing Decodium radio-control surface.
- State is driven by server push messages instead of active polling. Connection
  handshakes, request deadlines, reconnect backoff and disconnect handling are
  all asynchronous.
- PTT shutdown is fail-safe: Decodium requests return to receive before closing
  a session when possible, while the backend avoids running a second embedded
  CAT controller against the same selected radio.
- Management endpoint, control endpoint, group, radio, automatic ownership,
  auto-connect and split preferences are stored with the active Decodium
  profile.

### CAT settings and operator feedback

- CAT4OM appears alongside the existing backends in both Radio Settings and
  the compact Rig Control dialog.
- Network-specific fields replace serial-only controls when this backend is
  selected, including separate management and control endpoints, group and
  radio selectors, ownership choice and live connection status.
- Capability and ownership feedback distinguishes connected master control,
  read-only slave operation, discovery, reconnect and protocol errors.
- English and Italian catalogues include the complete new settings, status and
  error vocabulary.

### Local CAT service simulator

- A Python CAT4OM simulator is included for development on systems where the
  native service is unavailable.
- It exposes independent management and control WebSocket ports, presents a
  simulated IC-7300, supports discovery, ownership transfer, state pushes,
  VFO selection, frequency, mode, split and PTT state, and never opens a real
  serial port.
- The simulator models master/slave arbitration, rejects unsafe state changes
  during simulated transmission, returns automatically to receive after a
  bounded watchdog interval and unkeys when the controlling client leaves.
- An asynchronous terminal console can change the simulated radio state and
  display protocol activity without blocking the service event loop.

### FT2-Link queued operator commands

- CONNECT, BCAST TX and SEND FILE now behave as persistent operator requests:
  one click is retained while carrier assessment or the current radio state
  delays transmission, then dispatched automatically when the channel is
  ready.
- Repeated clicks no longer create duplicate connection attempts, broadcasts
  or file transfers while the original request is pending.
- Pending, connecting, transmitting and failure states are shown directly in
  the relevant controls, with bounded retry and cancellation behaviour.
- File transmission checks the complete application and ARQ session state so
  a new transfer cannot replace a transfer still waiting for acknowledgement
  or collide with an inbound window.

### FT2-Link broadcast timeline

- Sent and received broadcasts are exposed with the same message-row metadata
  as connected traffic, including direction, delivery state, peer and BCAST
  classification.
- The broadcast page follows new traffic when already at the end while
  preserving the operator's reading position when older entries are being
  inspected.
- Connectionless traffic is visible in the message history immediately after
  it is accepted for transmission and when it is decoded on reception.

### FT2-Link received-file centre

- Received files have a dedicated view with unread count, type indication,
  metadata and per-entry actions.
- Operators can select the receive directory, open it in the system file
  manager, save to the configured directory, use Save As, copy textual
  content, mark entries read or unread and remove individual or already-read
  entries.
- Optional automatic saving persists new files without blocking the graphical
  event loop. Save and folder-open operations report completion asynchronously.
- File names received over radio are sanitised, platform-reserved names are
  avoided, path traversal is removed and duplicate names create unique files
  instead of overwriting existing data.
- Binary payloads are validated before atomic writes, and the default receive
  directory follows the platform's standard Downloads location when available.
- The complete received-file interface and status messages are available in
  English and Italian.

### FT2-Link ARQ and wide-profile reliability

- Partial receive windows now schedule selective acknowledgements
  asynchronously, allowing the sender to identify missing frames before the
  final end-of-message frame arrives.
- Acknowledgements that advance the transfer immediately refill the active
  window with selective retries and the next unsent frames instead of waiting
  for a coarse retry timeout.
- Duplicate acknowledgements are distinguished from progress and no longer
  disturb an active outbound transfer.
- Wide-profile candidate selection prioritises the earliest valid sample
  position and then signal quality, preserving consecutive frames contained
  in the same receive buffer.
- The added protocol scenarios cover queued connect, broadcast and file
  commands, selective acknowledgement, multi-window file delivery and the
  network CAT state model for future regression checks.

### Rotator and Live Map localisation

- Manual azimuth and elevation controls, transport and feedback-port labels,
  tracking status and the adjustable PSK spot time window introduced in
  1.0.519 are now represented consistently across all supported language
  catalogues.
- The translated controls retain the same placeholders and operational meaning
  across the complete desktop interface.

## Release contents

- Decodium 4.0 source code at tag `v1.0.521`.
- Windows x64 installer executable.
- macOS DMG and ZIP packages for Apple Silicon and Intel, including the
  available compatibility variants.
- Linux x86_64 and aarch64 AppImage packages with SHA-256 checksums.

No local or automated test suite was executed for this release, as requested.
