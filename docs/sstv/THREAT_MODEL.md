# Native SSTV threat model

Status: living security model and implementation ledger, 2026-08-24. Rows below
remain release requirements unless the current-evidence section explicitly says
that the native control exists and has focused test coverage. No production
sharing backend, peer/relay or E2EE implementation is claimed.

## Scope and repository evidence

The scope is the native Decodium4 SSTV subsystem: analog and HAMDRM input, imported images and WAV files, local storage/gallery, remote image sharing, and the hand-off of a prepared image to the existing Decodium TX/CAT/PTT path. Remote IP sharing is not RF SSTV and must never be represented as an on-air mode.

The current checkout now contains native SSTV and sharing client code, while the
production-backend status remains **none**:

- `src/sstv/sharing/SstvShareManifest.*`, `SstvShareSecurity.*` and
  `SstvShareTransfer.*` implement exact-field bounded manifests, redaction,
  idempotency and a durable transfer state machine.
- `SstvHttpShareProviders.*` implements a bounded asynchronous Qt Network
  transport plus generic REST outbound and authenticated fail-closed inbound
  operations, WebDAV upload/status/delete/direct bounded GET, and pre-signed
  PUT. The production target cannot enable plaintext transport at runtime.
  At most 16 HTTP operations reserve a combined 128 MiB for request bodies and
  maximum responses. Provider session tables and terminal retry records have
  independently validated hard caps; complete/cancel/delete/revoke/expiry
  reclaim full sessions, and pre-signed bearer leases are then destroyed.
- `SstvLocalIntegrationShareProvider.*` is a deterministic process-local
  developer/test adapter using the same capability contract. A single resident
  payload budget spans sessions and objects, pending chunks are jointly bounded
  by count and bytes, and terminal operations reclaim the budget. It opens no
  socket, stores no credential and cannot be selected by production UI.
- `SstvShareQueueManager.*` implements SQLite schema v3 with transactional
  v1/v2-to-v3 migration, bounded persistent upload/download/inbox state,
  restart/retry/resume, raw quarantine, final hash,
  MIME/magic/dimension/pixel/allocation validation and atomic metadata-free PNG
  staging before explicit acceptance. At configured limits it deterministically
  reclaims only the oldest terminal/closed rows; active and retryable work plus
  rows owning managed local or staging files are retained fail-closed.
  Acceptance exposes a separately versioned local handoff.
- `SstvStorageWorker.*` consumes that exact schema-v1 handoff on its owner
  thread, repeats canonical/private path, SHA-256, size, PNG, dimension, pixel,
  allocation and full-decode validation, atomically publishes the exact bytes
  through the existing image layout, commits SQLite, and only then removes the
  staged object. `DecodiumBridgeSstv.cpp` connects the accepted handoff to that
  slot with a queued connection and reports the typed completion result.
- `src/security/SecureSettings.*` exposes macOS Keychain, Linux Secret Service
  via `secret-tool`, and Windows DPAPI backends. `SstvShareController` bypasses
  the convenience plaintext fallback and calls `secure_settings::Backend`
  directly for namespaced Bearer/Basic secrets. A focused test covers worker-
  thread store/remove and scans its `QSettings` file for the submitted secret.
- The queue target links Qt 6 Sql and explicitly refuses initialization without
  `QSQLITE`. `SstvShareController` owns the manager on a dedicated worker,
  `DecodiumBridge::sstvShare` exposes it, and the native Sharing page renders
  active/history/inbox models. Artifact-level plugin and lifecycle verification
  remain pending.
- `Network/RemoteCommandServer.*` is an HTTP/WebSocket radio-control console, including non-TLS transports. It has no recipient directory, resumable object store, inbox, expiry or E2EE contract. It is not an SSTV sharing relay and must not be treated as one.

## Current control evidence and open gaps

| Invariant area | Native evidence in this checkout | Remaining release gap |
| --- | --- | --- |
| Bounds, strict fields and idempotency | Manifest/persistence byte, depth, node, string, dimension, pixel, chunk and lifetime limits; pre-conversion rejection of literal and escaped-equivalent duplicate JSON keys; transfer-derived idempotency; bounded active/terminal provider sessions; deterministic local-provider budget/reclaim and hostile-response/restart tests. The real private incoming-media staging boundary has a 259-case deterministic fuzz-smoke test, and the complete ordinary CTest set passed locally under address/undefined sanitizers. | Ordinary deterministic smoke and address/undefined sanitizer coverage do not replace coverage-guided libFuzzer corpus runs. Apple Clang lacks that runtime locally and the configured Linux fuzz job was not executed; local adapters are not production-service evidence. |
| HTTP/TLS policy and redaction | HTTPS-only production target, peer verification, manual same-origin redirects, bounded response/header/URL/timeout policy, 16-operation/128 MiB aggregate pending budget, opaque credential leases and deterministic TLS-failure/redaction/redirect/plaintext-gate tests. | Broader certificate/hostname matrix, DNS rebinding/private-address SSRF policy and an actual production provider audit are missing. |
| Persistent queue/inbox | QSQLITE schema v3, WAL, prepared statements, validated rows, bounded concurrency/retries/offsets/sender blocks, transactional v1/v2 migration and crash-boundary tests, deterministic oldest-first terminal/closed-row reclamation, hash/native-image-gated explicit accept/ack/reject, provider delete/block, Save As/local-copy deletion, a dedicated controller worker and bounded live QML models. Migration/restart coverage seeds more than 10,000 closed inbox cycles, verifies safe reclamation, and retains active rows. | Corrupt-row/concurrency shutdown coverage and maintained-platform lifecycle validation are missing. |
| Quarantine and promotion | Root/symlink checks, private UUID staging, bounded byte count, SHA-256, MIME/magic/dimension/pixel/allocation/single-frame validation and `QSaveFile` normalization with direct fallback disabled are implemented. Both outgoing and validated incoming pixels are reconstructed as private metadata-free PNGs; the Bridge queues the accepted handoff to storage, which revalidates and transactionally imports it with UUID/hash idempotency. | Maintained-platform permission and full process-kill crash testing remain. |
| Secret fail-closed | Provider APIs never expose tokens/URLs and require a lease when credentials are configured; the controller uses direct `Backend` availability/lookup/store/remove, keeps only non-secret configuration in `QSettings` and creates the credential source in its worker. | Backend-unavailable and lookup/store/remove failure tests, platform-store validation, plus SQLite/QML/log/diagnostic scans are incomplete. |
| Network/RF isolation | Sharing targets have no CAT/PTT/audio dependency, acceptance is a local file action, and the controller is exposed through a data/action-only Bridge property. The storage importer rejects a forged `transmit` field, and Gallery-to-Studio remains a load-only action. | A final linked-artifact architecture audit and real-radio interlock trial remain required. |
| Diagnostics and test tone | Central allowlists retain only scalar state. The Bridge emits stable ASCII source tokens, caps active-TX refresh at 4 Hz plus one terminal refresh, omits unavailable HAMDRM counters rather than presenting false zeros, and preserves the last terminal test-tone result. The Diagnostics button and confirmation identify the tone as a real PTT/RF transmission and the Bridge repeats the normal TX preflight/SWR/output guard. | The rebuilt local Diagnostics/QML coverage is included in the final 83/83 SSTV-labelled portion of the 120/120 normal CTest run. A produced-package diagnostic archive and real PTT observation remain external gates. |
| E2EE | Manifest policy has downgrade and envelope validation fields. | No crypto library, envelope production/consumption, key lifecycle, packaging or vectors exist; E2EE is unavailable. |

## Assets and security objectives

| Asset | Required property |
| --- | --- |
| CAT/PTT and TX audio ownership | Only an explicit local TX action can key the radio; PTT is released on every path. |
| Received/prepared images and retained audio | Integrity, bounded processing, predictable retention and no unintended disclosure. |
| Gallery metadata and QSO links | Integrity, privacy, transactional persistence and correct ownership. |
| Provider credentials and E2EE private keys | Confidentiality, least lifetime, redacted diagnostics and secure deletion. |
| Sender/recipient identity | Stable provider ID; a callsign or display name alone is not authentication. |
| Transfer state | Durable, idempotent and resistant to replay, duplication and rollback. |
| Decodium availability | Bounded CPU, memory, disk, queue and network use; no work on GUI/audio callbacks. |
| Existing Decodium modes and audio | Isolation from SSTV failure or overload; no competing capture or PTT stack. |

## Trust boundaries and data flow

```text
untrusted RF/audio/WAV --> bounded native RX/DSP --> candidate pixels
                                                    |
untrusted HTTPS provider --> bounded raw staging --> size + SHA-256
                                                        |
                                   MIME/magic/dim/pixel/allocation decode gate
                                                        |
                                     metadata-free private PNG --> explicit accept
                                                        |
                                         sharing SQLite worker
                                                        |
                                  bounded Bridge models --> Sharing QML
                                                        |
                                  explicit accept --> independently revalidated
                                                     Gallery publication

gallery/preparer -- explicit local confirmation --> existing TX coordinator
                                                   --> existing audio/CAT/PTT
```

The following boundaries are security-significant:

1. RF, sound-card, RTL-SDR, DecoPort and imported WAV samples entering native DSP.
2. User-selected or remotely supplied image files entering Qt image codecs.
3. JSON, bytes, redirects and authentication challenges crossing Qt Network.
4. Quarantine files becoming gallery objects.
5. C++ models exposing bounded, non-secret data to QML.
6. `SecureSettings` platform storage versus ordinary settings/SQLite/logs.
7. Worker queues versus GUI/audio threads.
8. A prepared image becoming a request to the existing TX/PTT coordinator.

## Actors and attacker capabilities

- A legitimate local operator who can make mistakes, choose unsafe files or misconfigure a provider.
- A malicious RF station or crafted WAV author controlling the complete audio waveform.
- A malicious remote sender controlling metadata, JSON, filenames and image bytes.
- A compromised, curious or faulty provider/relay that can inspect, reorder, replay, truncate or delete traffic.
- A network attacker able to redirect, intercept or delay traffic but not defeat correctly validated TLS.
- Another process or local user able to modify writable files or exhaust local resources; compromise of the logged-in OS account is not fully mitigated.
- A compromised image codec, Qt/network dependency, credential backend, radio or CAT endpoint.

## Mandatory security invariants

- **TM-01 — Network/RF separation:** sharing code has no API that asserts PTT, starts TX audio or selects an on-air mode. Accepting an inbox item emits only a validated local-file handoff. The Bridge/storage worker independently revalidates that handoff before an explicit Gallery import; it cannot transmit. Any later transmit requires a separate, visible local action and existing Decodium TX interlocks.
- **TM-02 — No implicit trust:** every image, WAV, manifest, server response, database row and filename is untrusted even after TLS or E2EE.
- **TM-03 — Bounded work:** parsers and queues enforce byte, dimension, pixel, duration, depth, string, chunk, redirect and retry limits before allocation.
- **TM-04 — Quarantine first:** remote bytes are streamed to a private temporary-transfer location, hashed and validated before decoding or gallery import.
- **TM-05 — Atomic promotion:** final files are written with `QSaveFile` under explicit `QStandardPaths`-derived roots; database publication occurs only after commit succeeds.
- **TM-06 — Secret fail-closed:** if the platform secure backend is unavailable, SSTV authenticated sharing/E2EE is disabled with an actionable error. Secrets never fall back to `QSettings` or SQLite.
- **TM-07 — TLS fail-closed:** production providers use HTTPS, never ignore certificate errors and never silently fall back to HTTP. Credentials are not forwarded across origins.
- **TM-08 — E2EE policy is sticky:** a transfer created as `required` cannot be retried, resumed or completed as cleartext. Missing keys/capability is a hard failure, not a downgrade.
- **TM-09 — Thread ownership:** networking, hashing, file encoding/decoding and SQLite writes do not run on the GUI or audio callback thread. Each Qt SQL connection remains on its owning worker thread.
- **TM-10 — TX fail-safe:** SSTV uses the existing Decodium audio/CAT/PTT coordinator, exclusive TX ownership, lead/tail policy and watchdog; cancellation, exception, disconnect and shutdown all release PTT.
- **TM-11 — Privacy defaults:** upload, public sharing, automatic content download, EXIF retention and metered-network background transfer default off. Recipient and expiry require confirmation.
- **TM-12 — Redaction:** logs and diagnostic exports omit authorization headers, passwords, tokens, signed URLs, private keys, complete envelopes, image bytes and unnecessary personal metadata.

## Abuse cases and required controls

| Threat / abuse | Required controls | Verification gate |
| --- | --- | --- |
| PNG/JPEG decompression bomb or huge animation | Check compressed bytes; probe magic, dimensions and frame count before full decode; checked pixel/memory arithmetic; hard codec and thumbnail budgets; no external references. Both the incoming validator and local Studio reader enforce compressed-byte, single-frame, dimensions/pixels, Qt allocation, post-decode byte and metadata bounds; Studio also rejects symlink sources. | Broader hostile codec corpus at and beyond each limit; measured peak allocation remains bounded. |
| MIME spoofing, corrupt or polyglot image | Compare declared MIME, magic and decoder result; allowlist formats; decode in worker; strip EXIF by default; never execute embedded content. | Mismatch, truncation, animation and malformed-metadata tests. |
| Malformed RIFF/WAV and adversarial RF audio | Checked RIFF/chunk arithmetic, format/channel/rate/duration limits, streaming decode and bounded rings; deterministic abort. | Truncated, overlapping, oversized and integer-overflow corpus plus fuzzing. |
| Deep/large/ambiguous JSON | Manifest maximum 64 KiB, depth 8, 4,096 nodes, bounded strings, integer range checks, exact known fields and supported version 1 only. A bounded lexical pass rejects duplicate keys, including escaped spellings of the same key, before Qt object conversion. | Literal, escaped-equivalent and nested duplicate-key tests pass. Full libFuzzer corpus execution remains a release gate. |
| Path traversal or overwrite | Treat remote names as display text; generate UUID storage names; reject absolute paths, separators, `..`, Windows reserved names, controls and invalid Unicode; verify destination remains under its root and does not escape through a symlink; explicit overwrite only. | Cross-platform traversal/symlink/collision tests. |
| Partial/crash-corrupt save | Stream to quarantine; `QSaveFile::commit()` for final object; fsync/transaction ordering; recover or remove orphan temporary data after restart. | Fault injection before/after file and DB commits. |
| MITM, bad certificate or HTTP downgrade | HTTPS-only URL policy, strict Qt TLS verification, abort on any TLS error, compile-time-only loopback exception for tests. | Expired, self-signed, wrong-host and downgrade tests. |
| Redirect credential theft / SSRF | Maximum redirects; permit HTTPS; reject userinfo; never follow a cross-origin redirect with credentials. Add provider-specific public/private-address and DNS-rebinding policy before accepting arbitrary Internet endpoints. | Cross-origin, DNS/port/scheme/private-address and redirect-loop tests. |
| Oversized/chunked network response | Preflight `Content-Length` when present, count actual streamed bytes, abort above manifest/provider/application limit; validate ranges and chunk hashes. | Missing/false length, endless stream and range-overlap tests. |
| Replay, duplicate completion or corrupt resume | Transfer UUID plus idempotency key, immutable manifest hash, server status reconciliation, per-chunk range/hash, final SHA-256, bounded retry. | Restart/resume, repeated request and changed-body conflict tests. |
| Expired or revoked object accepted | Compare UTC using a sane-clock policy; provider enforces expiry; client refuses new download/import after expiry and requests deletion/revocation. | Boundary clock, stale listing and in-flight expiry tests. |
| Inbox spam and disk exhaustion | Metadata-only polling by default, bounded provider list/rows/concurrency/persistent sender blocks, explicit download/accept/reject, capability-gated provider block and an explicit local-only block with no silent fallback. | Flood, pagination, storage-quota reservation and wider restart tests. |
| Sender spoofing by callsign | Authenticate stable provider recipient/sender ID; callsign is display metadata; surface verification/fingerprint/trust state. | Same-callsign/different-ID and changed-key tests. |
| Credential disclosure through current fallback | Use `secure_settings::Backend` availability/store/lookup directly or add a fail-closed SSTV wrapper; persist only opaque credential handles in SQLite; never expose values to QML. | Backend-unavailable/store-failure tests and settings/log scans. |
| Signed URL leakage | Treat full pre-signed URLs as bearer secrets. The adapter receives a short-lived target only through an opaque trusted broker lease; the UI has no manual URL field and the URL never enters manifest, queue, model or diagnostics. | No trusted broker is shipped, so the provider remains unavailable; broker authentication/expiry/replay and maintained-platform redaction tests are release gates. |
| Compromised relay reads content | E2EE with an audited packaged library, authenticated encryption, unique nonce, recipient key, manifest-bound AAD and integrity before decode. UI must say “provider can read” when TLS-only. | Ciphertext tamper, wrong key/AAD and downgrade tests. |
| Nonce/key reuse or rotation confusion | Library-generated nonce, per-recipient envelope, key ID/fingerprint, algorithm allowlist, rotation history and no private-key export in diagnostics. | Duplicate nonce detection and rotated/revoked-key tests. |
| SQLite race/tampering | Dedicated worker connection, prepared statements, constraints, transactional versioned migrations, WAL/busy timeout, bounded queue and validation when reading rows. No secrets/BLOB images. | Migration interruption, concurrency and corrupt-row tests. |
| QML injection or secret lifetime | Expose typed roles, not executable markup/URLs; escape display strings; keep credentials in C++; bound model text and collection sizes. | Malicious filenames/messages and object-lifetime tests. |
| Network item triggers RF transmission | No inbox/provider-to-TX signal; remote input can become only a validated private handoff and an imported Gallery record. The storage consumer has no TX dependency and rejects unknown handoff fields such as `transmit`. Gallery “Open in Transmit Studio” only loads pixels and changes workspace page, while “Prepare remote sharing” only preselects a local file; radio TX and network queueing each remain separate explicit actions under their own checks. | Final linked-artifact architecture audit plus real TX-interlock testing. |
| PTT remains asserted | RAII/fail-safe TX ownership, watchdog, idempotent release and shutdown ordering through the existing coordinator. Local tests cover failure/timeout/underrun/disconnect/destruction and cancellation during header, image and FSK ID with exactly-once release. The Diagnostics test tone uses the same guarded coordinator and is explicitly labelled as PTT/RF rather than a harmless local UI probe. | Physical CAT/PTT feedback, sleep/resume and shutdown on a real station remain required. |
| Diagnostic/privacy leakage | Centralized allowlisting plus a versioned sharing map containing only `2^53-1`-bounded transfer byte totals/rates, bounded queue depths and UTC reset time. Studio image rejections use fixed reason codes under `sstv.security`; TX, calibration and loopback record only low-rate scalar lifecycle events under `sstv.tx`. Active-TX snapshot refresh is capped at 4 Hz with a terminal refresh; source kinds are stable ASCII tokens, unavailable HAMDRM values are not forged as zero, and the final tone result is retained without paths or device identity. | Rebuilt focused tests inject path-like failure details and inspect the bounded event ring/controller/QML path; complete packaged diagnostic-archive inspection remains. |

## Storage and permissions requirements

Production roots must derive from `QStandardPaths` after Decodium application
identity is established. The sharing queue accepts explicit canonical upload
roots and a download root; the Bridge now supplies the native SSTV storage root
resolved by the storage worker, rather than an arbitrary QML path. Temporary
and final roots use separate subdirectories and user-only permissions where the
platform permits. Missing canonical roots, symlinks and permission failures
stop the operation. Maintained-platform path/permission behavior remains a
release validation item.

The current queue stores paths and metadata, not image BLOBs. Promotion order
is: finish bounded raw staging; verify byte count and SHA-256; enforce
MIME/magic/dimension/pixel/allocation/single-frame limits; reconstruct pixels;
atomically commit an owner-only metadata-free PNG with `QSaveFile`; explicitly
accept and transactionally update queue/inbox state; emit the exact versioned
handoff. The Gallery storage operation strictly parses schema v1, repeats the
private canonical path, no-symlink, byte/hash, PNG and bounded full-decode
checks, preserves the exact PNG hash in the existing imported-image layout,
commits its row and only then removes staging. A matching transfer UUID/hash is
an idempotent success; conflicts fail closed and retryable failures retain the
staged object. Deletion remains separate from reject/revoke. Gallery retention
protects favourites, QSO-linked rows, shared rows under the persisted policy,
and every missing/symlink/unowned path. Automatic retention is persisted
opt-in and off by default; manual retention requires a fresh preview token and
exact phrase. The current local Gallery delete action is a
separate strongly confirmed operation: it re-verifies indexed records, rejects
unsafe or shared mandatory paths, privately renames owned files before the row
transaction, restores them on in-process failure, and only removes optional raw
audio or thumbnails when every database reference is selected. Its bounded,
strict private journal is reconciled on worker startup: existing rows restore
staged files, while an already committed row deletion finishes cleanup. Real
process-kill and maintained-platform filesystem evidence remain hardening gates.

## Credentials and E2EE decisions

SSTV provider passwords, bearer/refresh tokens, API keys, persistent pre-signed
URLs and private encryption keys must use namespaced accounts in Decodium
`SecureSettings`. Only an opaque provider/account identifier may be stored in
the sharing SQLite tables. The controller's concrete current source calls
`secure_settings::Backend` availability/lookup/store/remove directly and does
not use `load_or_import()` or `value_for_write()`, whose fallback can return
plaintext for ordinary settings. This currently covers configured Bearer/Basic
provider secrets; future refresh tokens, signed-target leases and E2EE keys must
obey the same boundary.

E2EE is a capability and policy, not a checked box. Until an audited cryptographic library is selected, packaged on all release platforms and tested, E2EE remains **not implemented**. TLS-only sharing may be implemented, but the UI must state that the provider can read content. A user selecting “E2EE required” cannot send through a TLS-only provider.

## Required security test evidence

Focused native tests currently exercise manifest/state security, REST/WebDAV/
pre-signed transport, authenticated capability/recipient/inbox/range download,
stable acknowledge/reject idempotency, plaintext gating, cross-origin redirect
rejection, redaction, response bounds, cancellation, queue restart/resume,
hash/MIME/magic/dimension-bomb failure, inbox accept/ack/reject and database/file
fault boundaries. Controller and QML tests additionally cover privacy-off
startup, dedicated credential-backend calls, successful secret store/remove
without test-settings plaintext, capability-disabled inbox polling and
offscreen page rendering. They are useful evidence for individual controls,
not a complete M6 release result.

The 2026-08-24 local M6 work built the sharing UI and linked `decodium_qml`;
focused sharing CTest targets and the 1040x700 offscreen Sharing-page render
passed in the local build. It did not contact a live provider, exercise a real
platform credential store or validate release packages.

Before release, the remaining evidence must include:

1. Execute the existing VIS/N-VIS/FSK-ID, WAV, QSO, sharing and HAMDRM
   libFuzzer targets with Clang, ASan and UBSan against their committed corpora;
   deterministic ordinary-test fuzz smoke is not a substitute for that run.
2. Extend the deterministic local provider/server beyond its current
   upload/download/authentication/cancellation/idempotency/wrong-hash/inbox
   coverage to expiry during transfer, revocation, pagination and quota/flood
   behavior.
3. Complete the certificate/hostname/DNS/private-address matrix and retain a
   repository scan proving production code never ignores TLS errors.
4. Secure-backend unavailable/read/store/remove failures proving no SSTV secret enters `QSettings`, SQLite, QML, logs or diagnostic exports.
5. Maintained-platform filesystem/permission/plugin tests for the existing
   traversal, Unicode/reserved-name, symlink, collision, atomic, quota and
   restart controls, including a real forced process kill.
6. Broader image/WAV hostile corpora with measured peak memory/CPU and no GUI/audio-thread work.
7. Longer SQLite/thread-affinity/concurrency/shutdown stress under sanitizers;
   the ordinary current-tree sanitizer suite passed, but it is not that stress
   evidence.
8. E2EE tests, if enabled, for tamper, wrong recipient/key/AAD, key rotation, nonce uniqueness and silent-downgrade prevention.
9. A final linked-artifact isolation audit plus real-radio tests. Local forged-
   handoff and coordinator tests already prove that the sharing schema cannot
   request TX and that tested software paths release ownership; they do not
   prove physical CAT/PTT behavior.

## Residual risks and release blockers

- No production SSTV sharing backend has been identified or audited. A protocol, abstraction and local test provider do not create one.
- The controller implements the TM-06 direct-backend path, but backend-
  unavailable and lookup/store/remove failure injection plus maintained-platform
  secure-store, SQLite, QML, log and diagnostic scans remain release gates.
- The generic REST inbound operations and deterministic loopback tests are not
  an Internet interoperability or production-provider claim.
- WebDAV direct bounded GET exists, but WebDAV supplies no standard recipient,
  inbox-list or acknowledge/reject contract. Generic REST provider delete and
  sender block now remain fail-closed behind verified capabilities; local-only
  blocking is separately labelled and bounded. Save As and private sharing-copy
  deletion do not change Gallery/provider state. Public durable pause/resume is
  implemented for both directions, including restart preservation and
  stale-callback rejection.
- Hash-correct remote bytes now pass native MIME/magic/dimension/pixel/
  allocation/single-frame decoding and metadata-free staging before
  `AwaitingAcceptance`; the accepted handoff is revalidated by the storage
  importer before a Gallery row can be committed.
- The worker/Bridge/QML path is implemented and focused tests exist, but full
  shutdown/concurrency/platform validation and hostile model-text coverage are
  still incomplete. Generic REST inbound capability remains fail-closed until
  an exact authenticated capability document succeeds.
- The TLS-only Sharing page states that its configured provider can read
  content; this disclosure mitigates confusion but does not provide E2EE.
- `QSQLITE` is a mandatory runtime plugin. Linux packaging has explicit support,
  but produced Windows, macOS and Linux bundles have not all been inspected for
  this tranche.
- TLS-only providers can inspect content and visible manifest metadata. This must remain explicit until E2EE is packaged and independently reviewed.
- A compromised logged-in OS account, credential store, Qt image plugin, crypto library, radio firmware or recipient device can defeat application controls.
- Revoke/delete cannot retract copies already downloaded, backed up or captured by the recipient/provider.
- Callsigns are not cryptographic identity; initial key verification remains a social/operational trust decision.
- Metadata exposed outside an E2EE payload can reveal callsigns, timestamps, mode and object size; minimisation remains necessary.
- Existing Decodium remote-control transports are outside this protocol and must not be reused as an Internet image relay without a separate audit.
- Live RF/CAT/audio behavior and platform permission semantics require hardware and maintained-platform verification; static review is insufficient.
