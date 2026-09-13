# Decodium SSTV remote sharing protocol

Status: version 1 client protocol and implementation ledger, 2026-08-24.
Decodium4 contains an opt-in native sharing client, queue and Sharing page, but
this checkout does not contain, name or configure a production sharing service.
No compatible endpoint or account is bundled, deployed or independently
validated by this implementation.

The machine-readable reference profile is
[`remote-sharing-openapi.yaml`](remote-sharing-openapi.yaml). It deliberately
contains no `servers` entry and records an empty production-endpoint list.

## Current implementation boundary

| Capability | Current checkout | What must not be inferred |
| --- | --- | --- |
| Strict manifest/security/transfer core | Implemented in `src/sstv/sharing/SstvShareManifest.*`, `SstvShareSecurity.*` and `SstvShareTransfer.*`; focused native tests exist. | A parser and state machine are not a deployed service. |
| Provider abstraction | Implemented as the asynchronous, cancellable `SstvShareProvider` interface with explicit capabilities and redacted result categories. | Unsupported operations are not emulated. |
| Generic REST outbound client | Implemented for configured create, sequential chunk, status/resume, complete and cancel path templates. | There are no built-in provider URLs, accounts or server defaults. |
| WebDAV and pre-signed PUT clients | WebDAV collection validation/upload/status/delete plus bounded direct GET and a trusted-lease single PUT are implemented and exercised by deterministic HTTP tests. The UI exposes pre-signed PUT as unavailable until a trusted target broker is configured. | WebDAV does not invent a recipient directory or inbox-list/decision contract. A user-pasted or persisted signed URL is never accepted. |
| Durable upload/download/inbox queue | SQLite schema v3, transactional v1/v2-to-v3 migration, retry/restart recovery, public durable pause/resume for both directions, explicit accept/acknowledge/reject/cancel, validated Save As, local-copy deletion, bounded persistent sender blocks, provider incoming deletion, diagnostics and metered-network policy are implemented in `SstvShareQueueManager.*`. At configured record limits, the store deterministically reclaims only the oldest terminal transfers and closed inbox rows; active/retryable work and any managed local or staging file remain protected. A completed upload with a persisted remote-object ID can also be explicitly deleted or revoked only when the selected provider has a real executable capability. | Local tests, including migration/restart with more than 10,000 inbox cycles, do not prove Internet interoperability. Provider delete/block remains unavailable unless capability discovery verifies it. |
| Validated handoff to Gallery storage | `DecodiumBridge` connects `SstvShareController::incomingHandoffReady` to `SstvStorageWorker::importValidatedIncomingHandoff` on the storage worker thread. The importer strictly parses schema v1, reopens, re-hashes and fully decodes the private normalized PNG, preserves its exact bytes in the existing imported-image layout, commits SQLite and only then removes staging. UUID/hash replay is idempotent and focused tests cover the exact controller signal/slot contract. | This proves the in-process lifecycle and local persistence path, not any production sharing endpoint or Internet interoperability. |
| Generic HTTP recipient/inbox/download/decision mapping | Implemented fail-closed by `SstvGenericRestShareProvider`, including authenticated capability discovery, recipient lookup, bounded metadata list/range GET, idempotent acknowledge/reject/incoming-delete and sender block. | No production endpoint has been named, audited or tested. Delete/block are additive optional v1 capabilities and default false when absent. |
| Credentials | `SstvShareController` uses `secure_settings::Backend` directly for namespaced Bearer/Basic secrets, creates opaque credential leases and fails closed instead of persisting a secret through ordinary settings. Focused tests cover successful store/remove on the worker and scan the test `QSettings` file for the submitted secret. | Backend-unavailable and lookup/store/remove failure paths, platform stores and diagnostic/log surfaces still require complete release evidence. |
| Peer/relay, local integration and E2EE | `SstvLocalIntegrationShareProvider` is a deterministic bounded process-local contract adapter exercised through the queue/controller in developer tests. Production peer/relay and E2EE remain explicit unavailable states. | The local adapter opens no socket, persists no credential and is available only by explicit test injection. No relay, peer discovery, direct listener, key exchange, encrypted envelope or production endpoint is shipped. |

`qml/decodium/components/sstv/SstvSharePage.qml` is now connected through
`DecodiumBridge::sstvShare` to the worker-owned controller. Sharing defaults
off; the page configures only user-supplied HTTPS REST/WebDAV endpoints,
requires explicit recipient confirmation and exposes active/history/inbox
models. Each upload explicitly selects expiry, optional callsign/grid disclosure
and whether metered transfer is allowed; public/automatic sharing and EXIF stay
off. REST inbound controls are enabled only after a complete authenticated
capability response; WebDAV exposes direct bounded GET but no inbox listing.
Pre-signed PUT and peer/relay are visible, navigable unavailable states: the
former requires a maintained trusted target broker, and the latter requires an
authenticated relay backend. Neither state exposes a field for a signed URL or
starts peer discovery/listening.
Before queueing an outgoing item, the worker accepts only a bounded decodable
PNG/JPEG under the native SSTV storage root, reconstructs its pixels as a new
private owner-only PNG under `sharing/outgoing`, omits source text/EXIF metadata
and hashes that immutable staged payload. The focused controller test exercises
this sanitisation path.

## Scope and separation from radio

The protocol transfers an already received or prepared image over an IP
network. It is not analog SSTV, VIS/FSK ID, HAMDRM or any other RF mode.
Provider and queue code have no authority to assert PTT, start TX audio or
change CAT state. Accepting a download emits a precise validated local
handoff; its Gallery consumer is storage-only, and any later on-air
transmission remains a separate action through the existing Decodium TX
coordinator and interlocks.

All client code stays inside the Decodium4 process and repository. It reuses Qt
Network, Qt SQL, `QStandardPaths`, `QSaveFile`, native models and Decodium's
security backend. `Network/RemoteCommandServer.*` is a radio-control console,
not an image-sharing backend, and cannot be relabelled as one.

## Manifest v1

The native manifest is an exact-field, canonical JSON object with integer
`protocolVersion: 1`. Unknown or missing fields are rejected. UUIDs are
lower-case canonical text, hashes are lower-case hexadecimal SHA-256 and times
are UTC RFC 3339 strings ending in `Z`. The canonical UTF-8 form is immutable
after queueing and is bound to the transfer-derived idempotency key.

Synthetic valid-shape example:

```json
{
  "byteSize": 123456,
  "callsign": {
    "grid": "",
    "remoteCallsign": "",
    "senderCallsign": ""
  },
  "chunkCount": 1,
  "completion": "complete",
  "contentDisposition": "attachment",
  "createdUtc": "2026-08-24T10:00:00.000Z",
  "encryption": {
    "algorithm": "none",
    "downgradeProtected": true,
    "keyId": "",
    "manifestBoundAsAuthenticatedData": false,
    "mode": "transport-tls",
    "nonceBase64": "",
    "recipientKeyFingerprint": ""
  },
  "expiresUtc": "2026-08-31T10:00:00.000Z",
  "height": 256,
  "mediaSource": "analog-reception",
  "mediaUtc": "2026-08-24T09:58:00.000Z",
  "message": "Synthetic protocol example",
  "mimeType": "image/png",
  "originalFilename": "synthetic-test-card.png",
  "privacy": {
    "automaticIncomingDownloadAllowed": false,
    "automaticUploadAllowed": false,
    "callsignIncluded": false,
    "exifRetained": false,
    "explicitExpiry": true,
    "gridIncluded": false,
    "locationIncluded": false,
    "meteredNetworkAllowed": false,
    "publicShare": false,
    "recipientConfirmed": true
  },
  "protocolVersion": 1,
  "providerId": "local-test",
  "recipientId": "recipient-0002",
  "safeDisplayFilename": "synthetic-test-card.png",
  "senderId": "sender-0001",
  "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
  "sstvMode": "Martin M1",
  "transferId": "018f2f79-2a3d-7d91-8d42-111111111111",
  "transport": {
    "certificateValidationRequired": true,
    "providerCanReadContent": true,
    "sameOriginRedirectsOnly": true,
    "tlsRequired": true
  },
  "width": 320
}
```

Native hard limits are part of the protocol contract:

- manifest JSON: 65,536 bytes, depth 8 and 4,096 nodes;
- image payload: 1 through 67,108,864 bytes;
- dimensions: 1 through 8,192 per side and at most 16,777,216 pixels;
- chunks: 1 through 4,096 and no more chunks than payload bytes;
- lifetime: greater than zero and at most 30 days;
- message: at most 1,000 sanitized characters;
- original and display filenames: at most 128 sanitized characters, never a
  local path;
- MIME: `image/png` or `image/jpeg` only;
- media source: `analog-reception`, `analog-transmission`,
  `digital-reception` or `digital-transmission`;
- completion: `complete` or `partial`.

For an outgoing transfer, expiry must still be in the future and
`privacy.recipientConfirmed` must be true. Callsign/grid values are legal only
when their corresponding inclusion flag is true. Remote sharing itself is
never stored as an SSTV mode or media source.

Transport-TLS manifests require `algorithm: none`, empty key/nonce fields,
`manifestBoundAsAuthenticatedData: false` and
`providerCanReadContent: true`. The schema also validates the shape of future
`end-to-end` envelopes, but schema acceptance is not an E2EE implementation or
availability claim.

## Provider contract and errors

`SstvShareProvider` exposes authentication status and these capabilities:
recipient lookup, chunked/resumable upload, download, acknowledgement,
rejection, incoming deletion, sender blocking, revocation, remote delete,
incoming list, E2EE envelope, strict TLS, maximum chunk bytes and maximum
response bytes. UI and queue actions remain disabled when a capability is
absent. The two new abuse/lifecycle flags are additive: a legacy v1 capability
document that omits them is valid but means `false`; a wrong type or unknown
field rejects the complete document.

The concrete public interface is asynchronous and cancellable:

- recipient lookup;
- create, chunk upload, resume/status, complete and cancel;
- bounded download;
- acknowledge, reject, provider-side incoming delete and sender block;
- revoke/delete, refresh credentials and bounded inbox listing.

Provider handles are opaque safe identifiers. They cannot contain URLs,
filesystem paths, bearer tokens or cookies. Results expose only bounded payload
bytes and a redacted category. Retryable categories are
`transient-network`, `provider-unavailable`, `offline` and `rate-limited`;
authentication, authorization, validation, conflict, integrity, TLS and other
permanent failures do not enter an automatic retry loop.

## HTTPS REST profile

All production endpoints are configured by the user/provider; none is embedded
in this repository. The reference paths below are interoperability names. The
generic client accepts equivalent validated path templates containing exactly
one `{uploadId}` placeholder where required.

### Implemented generic REST outbound mapping

| Operation | Reference method/path | Required binding |
| --- | --- | --- |
| Create | `POST /api/v1/transfers` | Exact canonical manifest plus the transfer-derived lower-case SHA-256 `Idempotency-Key`. Response is exactly `uploadId` and optional `committedBytes`. |
| Status/resume | `GET /api/v1/transfers/{uploadId}` | Response is exactly `committedBytes`; values beyond declared size fail. |
| Chunk | `PUT /api/v1/transfers/{uploadId}/chunks` | Sequential `Upload-Offset`, body SHA-256 in `Digest` and `X-Content-SHA256`, and operation-derived idempotency key. |
| Complete | `POST /api/v1/transfers/{uploadId}/complete` | Body contains final `byteSize` and `sha256`; default client policy requires the server to echo both with a safe `remoteObjectId`. |
| Cancel/revoke | `DELETE /api/v1/transfers/{uploadId}` | Stable idempotency key; 200/202/204 and authenticated already-missing 404 are terminal. |

The v1 REST profile has no endpoint that deletes by `remoteObjectId`.
Consequently the generic client clamps discovered `remoteDelete` to false even
if a server advertises it, and exposes only `revocation` through the configured
cancel/revoke upload path. Before a history revocation after restart, the queue
rehydrates the opaque upload session with the documented idempotent create
operation and requires the exact persisted upload identity. It never derives
or invents a remote-object URL.

The current implementation allows only sequential chunks and a single durable
committed offset; the earlier indexed/out-of-order chunk design is not a current
capability. The REST client keeps its provider-session map in memory, while the
queue persists its opaque session handle and committed byte offset and
rehydrates it with idempotent create before status/resume reconciliation on
restart.

WebDAV validates the configured collection, uploads a single bounded object
with overwrite policy, checks status/integrity, can delete it and exposes a
bounded direct `GET` for a caller that already has an opaque object name. It
does not invent recipient lookup, inbox listing or acknowledgement/rejection
semantics that WebDAV itself does not define. The pre-signed provider accepts
exactly one full-object PUT target obtained through an opaque trusted lease;
the signed URL never enters queue persistence or a public result.

For WebDAV completion, the persisted remote-object ID is exactly Decodium's
locally derived lower-case transfer UUID plus `.png` or `.jpg`. History delete
accepts no other identifier shape and appends that single name to the already
configured collection URL. The Decodium WebDAV profile authorizes an
authenticated same-origin 404 as an idempotent success only after an explicit
delete request for such a persisted completed object. This makes a retry safe
across a crash between remote success and the local SQLite commit; unrelated
or caller-supplied object names are rejected before network I/O.

### Native generic REST inbound mapping

| Operation | Reference method/path | Queue behavior |
| --- | --- | --- |
| Capabilities | `GET /api/v1/capabilities` | Exact versioned response; inbound operations remain disabled after any missing, unknown or unsafe field. Remote limits are clamped to local bounds. |
| Recipient lookup | `GET /api/v1/recipients/{recipientId}` | Exact stable provider/recipient identity, bounded display data and optional verified key metadata. |
| Inbox | `GET /api/v1/inbox?limit=...` | Exact metadata-only response, bounded to at most 1,000; duplicate identities and manifest-binding mismatches fail the entire response. The current interface has no continuation cursor. |
| Download | `GET /api/v1/inbox/{incomingId}/content` with one `Range` | Sequential bounded chunks to private staging; final size/hash verified. |
| Acknowledge | `POST /api/v1/inbox/{incomingId}/acknowledge` | Stable action-specific idempotency key; legal only after explicit native-validated local acceptance. |
| Reject | `POST /api/v1/inbox/{incomingId}/reject` | Stable distinct idempotency key; separate from accept, acknowledgement and remote deletion. |
| Delete incoming provider copy | `DELETE /api/v1/inbox/{incomingId}` | Enabled only by verified `incomingDelete`; stable action key; 404 is idempotent only for an authenticated configured provider. Success persists `ProviderDeleted` without deleting local or Gallery files. |
| Block sender | `POST /api/v1/senders/{senderId}/block` | Enabled only by verified `senderBlocking` and only for a sender obtained from the bounded authenticated inbox. Success is also persisted locally. |

Inbound metadata carries provider ID, sender ID, incoming opaque ID, exact
canonical manifest JSON, its SHA-256, byte size, receipt time and expiry. The
queue validates that all duplicated fields bind to the manifest before writing
SQLite. Automatic content download is off.

Local sender blocking is a separate explicit action. It never contacts the
provider and is labelled local-only in the UI. Provider blocking never silently
falls back to local-only when its capability/request fails. Persistent block
records are keyed by configured provider and opaque sender ID, capped by the
queue limit, and apply to later listings. `Save As` copies only a revalidated
normalized PNG to a new non-symlink destination via `QSaveFile`; the chosen
destination is not persisted or reported in diagnostics. Deleting the private
sharing copy is idempotent and leaves any already imported Gallery record,
independent Save As file and provider object unchanged.

The transport uses manual redirects and cache/cookie/auth-reuse controls,
validates TLS peers, rejects cross-origin redirect credential forwarding, caps
redirects at five and timeouts at five minutes, caps HTTP response bodies at 1
MiB, response headers at 128/32 KiB and transport URLs at 8 KiB. Default
response and redirect limits are smaller. Plain HTTP requires both a dedicated
compile definition and an explicit loopback-only test option; the production
library is compiled without that definition.

## Durable queue, files and SQLite

`SstvShareQueueManager` is timer-agnostic and QObject-thread-affine.
`SstvShareController` now owns it on a dedicated `SstvShareWorker` thread and
drives `processDue()` from a bounded timer and explicit events. Embedders of the
core manager retain the same thread-affinity requirement. Queue defaults are:

- 10,000 transfer rows and 10,000 inbox rows;
- 4,096 provider/sender block rows;
- 200 rows per model query;
- two concurrent transfers, one per provider;
- 1 MiB upload and download chunks;
- five retries with deterministic jitter and bounded provider `Retry-After`.

Absolute configurable ceilings are 100,000 transfer/inbox rows, 100,000 sender
blocks, 1,000 query rows and 16 concurrent transfers. Queue state, attempts,
next retry, byte offset, manifest, provider opaque session and redacted error
are committed transactionally. Credentials and full image BLOBs are not stored
in SQLite.

Uploads whose manifest does not explicitly allow metered transfer are eligible
only when the platform probe positively reports an unmetered route. A metered
or unknown result leaves the row durably queued; the UI explains this before
queueing. The controller publishes a versioned diagnostics map containing only
bounded uploaded/downloaded byte totals, bounded average bytes/second, active/
upload/download queue depths and UTC reset time. Totals saturate at `2^53-1`
for exact QML integer representation and reset explicitly. Provider IDs, URLs,
credentials, tokens, manifest text and local/export paths are not diagnostics
fields.

Remote-copy removal is a separate asynchronous history action, never a normal
transfer cancellation. It accepts only upload rows whose managed and embedded
core states are both `Completed`, whose provider upload handle and
`remoteObjectId` survived strict persistence validation, and whose current
provider is authenticated and advertises an executable capability. Real
remote delete takes precedence over revocation. A failure remains `Completed`
with a redacted durable diagnostic and requires an explicit retry; there is no
automatic destructive retry. Success retains the immutable completed transfer
snapshot and changes only the managed terminal projection to `RemoteDeleted`
or `RemoteRevoked`, so restart and history queries remain valid. Operation
claims make late callbacks after cancellation, provider replacement or
shutdown inert.

The active-queue model exposes capability-derived `Pause` and `Resume`
actions. Pausing first invalidates the current provider-operation claim, asks
the provider to cancel only that in-flight request, then persists `Paused`.
Upload core state retains its exact resume target; download state retains the
private staging path and byte checkpoint and resumes as `DownloadQueued`.
Paused rows survive manager/process restart and `processDue()` cannot start a
provider request until explicit resume. Cancellation remains available while
paused and is a separate terminal request. Downloads already awaiting user
acceptance cannot be paused because their network transfer is complete.
The History model exposes `Delete provider copy` or `Revoke provider upload`
only for the exact action selected above. Its modal confirmation requires a
second acknowledgement and states that Decodium's local Gallery is unchanged;
the controller/queue/provider path has no CAT, PTT, audio or TX dependency.

Downloads remain under a configured canonical root. Symlink/path escape,
hostile display filenames, unexpected MIME/magic, pre-existing destination and
oversize inputs fail. Raw bytes are written to a private UUID-named `.partial`
file, checkpointed and fully re-hashed. Before `AwaitingAcceptance`, the native
validator enforces the manifest byte/hash/dimension/pixel contract, an
allowlisted PNG/JPEG magic and MIME match, single-frame `QImageReader` decoding
under a checked allocation cap, then reconstructs the pixels as a metadata-free
owner-only PNG using `QSaveFile` with direct-write fallback disabled. A separate
user accept re-inspects that private object, persists `Accepted`, removes the
raw remote bytes and emits a versioned validated handoff. The storage consumer
then repeats path, permission, byte, SHA-256, PNG structure, dimension, pixel,
allocation and full-decode checks, atomically publishes the exact normalized
PNG plus sidecar in the existing imported layout, commits the Gallery row and
only afterwards unlinks staging. Acknowledgement is a later provider
operation. Reject removes raw
and normalized staging but does not imply remote deletion; cancellation never
deletes the source upload or an already accepted handoff.

The live controller derives `queue.sqlite`, `downloads` and `outgoing` below
the native SSTV `sharing` directory. Generated outgoing copies are separate
from gallery/source files and are removed only after their transfer reaches a
terminal state; cancelling never removes the operator's original image.

### Validated incoming handoff and storage ownership

The native `schemaVersion: 1` map is exact-field and contains `transferId`,
`providerId`, `incomingId`, `senderId`, `safeDisplayFilename`, `sstvMode`,
`sourceMimeType`, `sourceSha256`, `sourceByteSize`, `stagedCanonicalPath`,
`stagedMimeType`, `stagedSha256`, `stagedByteSize`, `width`, `height`,
`receivedUtc` and `expiresUtc`. Unknown, missing or type-coerced fields are
rejected. `DecodiumBridge` connects
`SstvShareController::incomingHandoffReady(QVariantMap)` with a queued
connection directly to
`SstvStorageWorker::importValidatedIncomingHandoff(QVariantMap)`; native callers
may instead use `importValidatedIncomingHandoffTyped(...)`.

Completion is reported as one typed
`incomingImportFinished(SstvIncomingImportResult)`. The result carries
`transferId`, `ok`, `retryable`, `idempotent`, a bounded error category/message
and the committed `SstvImageRecord` when one exists. On `ok`, Gallery PNG,
sidecar and SQLite row are verified and staging is absent. On every pre-commit
or retryable failure staging remains owned by the sharing side; a post-commit
cleanup interruption returns `CleanupPending`, and replay finishes cleanup by
matching transfer UUID plus normalized PNG SHA-256. A restart can also adopt a
fully verified exact PNG/sidecar pair left between file publication and the
database commit. A conflicting UUID/hash is never overwritten.

For process-restart recovery, the controller emits each still-valid durable
`Accepted`, `Acknowledging` or `Acknowledged` handoff once after rebuilding its
queue manager. The Bridge makes two bounded delayed retries after a retryable
Gallery result, always with the same immutable handoff; after that an accepted
item remains explicitly retryable while its private staging file exists and
can be imported again from the UI or on restart. Provider acknowledgement is a
separate operator action and is never inferred from Gallery import success.

The Gallery record stores only metadata with a matching native meaning:
validated mode, receive/expiry times, provider ID, provider incoming-object ID,
remote/imported status, dimensions and the exact normalized PNG hash. The
opaque `senderId` is deliberately not guessed to be a callsign, and remote
filename/text/embedded image metadata is not copied into record note, callsign
or QSO fields.

## Credentials and privacy

Bearer/refresh tokens, passwords, API keys, persistent signed URLs and private
keys must use `secure_settings::Backend` directly through an SSTV-specific,
fail-closed credential source. The controller implements that rule for its
current Bearer/Basic provider secret and does not use the convenience
`load_or_import()` or `value_for_write()` paths, which can return plaintext for
ordinary `QSettings` persistence when the platform backend is unavailable or a
store fails.

Only an opaque provider/account handle and non-secret policy may enter settings
or SQLite. A credential lease is the only object permitted to modify a network
request; it cannot change the endpoint or relax redirect, cookie, cache or TLS
policy. Signed target URLs are leases too and are never returned in public
results. Backend unavailable, lookup failure or refresh-needed status disables
authenticated transfer with an actionable redacted error.

Remote sharing, public sharing, automatic incoming download, EXIF/location,
metered-network background work and callsign/grid disclosure default false.
The native page explains the opt-in and secure-store boundary, exposes expiry
and every optional disclosure before upload, and states explicitly that the
configured TLS-only provider can read content. This is not an E2EE claim.

The compiled pre-signed provider can receive a target only through a trusted
`SstvSharePresignedTargetSource` lease. A signed URL is a bearer secret: it is
not a provider ID, manifest field, controller configuration value, model role,
queue/telemetry field or ordinary setting. This repository contains no broker
that authenticates a Decodium user and mints that lease, so the native selector
shows the provider as unavailable. Enabling it requires a maintained broker,
audited authentication/authorization and expiry/replay policy, plus tests that
the signed target never crosses the lease boundary. Manual signed-URL entry is
intentionally unsupported.

## Local integration adapter and peer/relay boundary

When no external backend is available, deterministic integration tests inject
`SstvLocalIntegrationShareProvider` behind the normal `SstvShareProvider`
contract. It implements bounded create/chunk/status/complete/cancel, inbox,
download, acknowledge/reject, revoke/delete and sender-block semantics with
stable local participant IDs, expiry, hash verification and idempotency. One
resident-payload budget covers both active sessions and completed objects;
pending operation count and captured chunk bytes are jointly bounded. Cancel,
reject, revoke, delete and expiry reclaim their payload budget. This adapter is
not registered in production settings/UI, opens no listener or client socket,
stores no secret and is not an Internet or LAN provider.

Production peer/relay is a future provider design, not implemented code. If pursued, it
must remain an adapter behind `SstvShareProvider`; no relay logic, second GUI or
helper process belongs in Decodium. A separately deployed relay would need an
audited stable-identity directory, authenticated rendezvous, bounded object and
inbox APIs, expiry/retention enforcement, abuse controls, monitoring and an
independent privacy policy. Direct inbound listeners and automatic NAT/port
mapping remain off by default.

Relay metadata and content visibility must be stated explicitly. A claim that
the relay cannot read content requires a separately audited E2EE envelope,
recipient-key verification, nonce uniqueness, manifest-bound authenticated
data, rotation/revocation behavior and maintained-platform packaging. Relay or
peer messages can never carry an instruction that invokes TX/PTT.

DecoPort, the existing remote-command WebSocket channel and any other Decodium
control transport remain out of scope unless a separate audit establishes the
sharing protocol, authentication, origin, storage, expiry and abuse boundaries.

## Build and packaging requirements

The native targets require Qt 6 Core; the queue additionally requires Qt 6 Sql
and the runtime `QSQLITE` driver; HTTP providers additionally require Qt 6
Network. Queue initialization fails closed when `QSQLITE` is absent. Linux
build containers already install `libqt6sql6-sqlite`, and the AppImage script
has explicit `sqldrivers/libqsqlite` handling, but every produced macOS,
Windows and Linux artifact must still be inspected before a release claim.

No additional server, Python runtime or external SSTV application is required
for local analog SSTV. A provider service is optional and independently
configured.

REST and WebDAV upload sessions and pre-signed target leases have explicit
hard-bounded active-session and terminal-idempotency tables. Concurrent create
reservations prevent response races from exceeding the active cap. Successful
complete/cancel/revoke/delete transitions remove the full session; a minimal
bounded tombstone preserves an immediate idempotent retry until its manifest
expiry. Expired records are purged on the next provider operation. Pre-signed
target leases are released on completion, cancellation, expiry and provider
destruction. HTTP request concurrency remains capped at 16 with a shared
128 MiB request-body-plus-response reservation budget.

## Local verification snapshot

On 2026-08-24 the sharing UI and `decodium_qml` application target built and
linked locally. A timed, isolated, software-rendered launch loaded the native
`BootLoader`/`Main.qml` application and exited normally. The focused CTest
targets cover sharing core, queue manager, incoming-media validation, the
Gallery importer and crash-window recovery, controller, QML, HTTP providers
and the production plaintext gate; the queue test includes upload/download
pause, restart-without-network-work, checkpointed resume, fail-closed metered
policy, monotonic/reset diagnostics, bounded persistent sender blocks, remote
incoming deletion, Save As and local-copy deletion. HTTP tests cover verified
delete/block capabilities and stable action keys, while the controller test
exercises explicit privacy manifests and secret-free diagnostics. The Sharing page
also has a 1040x700 offscreen render test. This proves only the tested native paths in that build
environment, not a live HTTPS service, certificate/hostname matrix, real
platform credential store, packaged QSQLITE plugin, peer/relay deployment or
maintained-platform interoperability.

## Remaining M6 release gates

The current code and deterministic tests implement a native bidirectional
generic REST client tranche, not a completed or deployed M6 service. Release
readiness still requires:

1. complete fail-closed credential tests for backend-unavailable and
   lookup/store/remove failures, plus scans of SQLite, QML, logs and diagnostic
   exports and maintained-platform secure-store validation;
2. full lifecycle/thread-affinity validation of the Bridge/worker/QML
   integration, beyond the passing focused controller and offscreen-render
   tests;
3. broader shutdown-race and expiry-during-operation coverage around provider
   deletion/blocking, the accepted-handoff importer and pause/resume controls;
4. additional expiry-during-transfer, pagination/quota and
   concurrent-shutdown tests beyond the deterministic inbound success, auth,
   redirect, cancel, integrity, decision and restart coverage;
5. OpenAPI conformance tests against any server offered for use;
6. maintained-platform build/package verification including QSQLITE;
7. explicit architecture evidence that no provider/manifest/inbox transition
   can call CAT/PTT/TX;
8. an independent audit of any named production endpoint, authentication,
   limits, privacy, retention, operations and monitoring.

Until those gates pass, the production-provider list remains empty. The Sharing
page remains explicit opt-in, and every operation not advertised by its
configured provider remains disabled; no bundled service makes it turnkey.
