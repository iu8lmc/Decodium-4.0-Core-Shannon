# SSTV performance and diagnostic counters

Status: native bounded instrumentation implemented on the 2026-08-24
development snapshot. This is local functional evidence, not a maintained-
platform latency benchmark.

## Contract

`SstvStorageWorker::performanceSnapshot()` returns one coherent, thread-safe
`SstvStoragePerformanceSnapshot`. The snapshot contains numbers and one
lifecycle boolean only. It never contains database names, local paths,
filenames, record/request identifiers, callsigns, metadata, image bytes or
sharing credentials.

Database work submitted through
`SstvStorageWorker::enqueueDatabaseOperation()` is measured from acceptance
until owner-thread dispatch. The in-memory queue is capped at 1,024 pending
operations. New work is rejected when the bound is full or after shutdown has
started. Shutdown stops acceptance, accounts for pending work as cancelled and
sets current depth to zero. The Qt receiver context owns each queued callable,
so destruction cannot run it against a dead worker. Each accepted callable
also carries a lifecycle-generation ticket: work cancelled by one shutdown is
rejected if it is delivered after a later reinitialization.

The queue snapshot reports:

- accepting/not-accepting lifecycle state;
- current and lifetime peak depth;
- accepted, dispatched and completed callable counts;
- rejected and shutdown-cancelled counts;
- the combined queue-failure count.

“Completed” means that the owner-thread callable returned; the existing typed
storage signals continue to report SQL or record-level success. Queue failures
cover backpressure/rejection, cancellation, dispatch failure or an unexpected
callable exception, and do not duplicate those typed operation outcomes.

Image timing surrounds the actual `SstvImageStore::save()` or
`savePreservingPng()` filesystem publication call on the storage thread. Every
attempt records success/failure and last, integer-average and maximum duration
in nanoseconds using `std::chrono::steady_clock`. SQLite insertion time is not
silently folded into the image encoder/publication duration. A save success
means that the filesystem publication call succeeded; a later SQLite insertion
failure does not retroactively rewrite that timing sample.

All additions saturate at the unsigned 64-bit maximum instead of wrapping.
The counter object has fixed resident state and a short mutex critical section;
it allocates no per-operation history, histogram, path or diagnostic string.

## Producer rule

Storage producers must use `enqueueDatabaseOperation()` instead of posting an
unmeasured `QMetaObject` lambda directly. `SstvGalleryModel` follows this rule.
The native Bridge insert, incoming-import, RX-save, Gallery-preflight and QSO-
association paths follow it as well. A producer must treat a `false` return as
backpressure or shutdown and retain its existing user-visible failure/
cancellation behaviour.

Direct owner-thread calls remain legal inside one already-dispatched storage
operation, for example retention applying its validated deletion plan. They do
not create a second queue entry.

## Deterministic evidence

`test_sstv_storage` covers the exact 1,024-entry ceiling, rejection,
dispatch/completion, shutdown cancellation, arithmetic saturation, exact
last/average/maximum durations, concurrent updates, the scalar-only diagnostic
map, successful/failed real PNG saves and worker lifecycle. Gallery regression
tests exercise the queue-aware producer path.

Actual duration values are machine/filesystem dependent. Tests assert exact
synthetic arithmetic and structural relationships for real saves; they do not
set an unsupported universal millisecond threshold. Cross-platform profiling,
packaged-database behaviour and forced process-kill evidence remain separate
release gates.

## Transmit signal counters

`SstvTxAudioDevice` updates a maximum absolute PCM16 magnitude and a saturating
count of full-scale frames while it already walks each bounded mono pull to
route channels. No history or waveform copy is retained. The coordinator
snapshot exposes normalised peak, the source plan's nominal headroom and the
clipped-frame count, and preserves the last values after the SoundOutput lease
is detached. A new session resets them. The Bridge diagnostic projection is
scalar-only and omits device IDs, paths and radio configuration. These counters
indicate digital PCM headroom; they are not an RF power, ALC or sound-card
calibration measurement.

TX/calibration and Studio loopback diagnostics are transition-only events.
They are emitted on start acceptance/rejection, cancellation and terminal
completion/failure, never from `pullPcm16()`, the audio callback, PCM chunk
ingress or the 100 ms UI progress timer. Their scalar maps enter the existing
512-event bounded ring after central allowlisting and retain no waveform,
image, path or per-sample history.

## Sharing counters and provider bounds

`SstvShareController::diagnosticsSnapshot()` publishes scalar-only upload and
download byte totals, bounded average bytes/second, active/pending queue depths,
inbox depth and reset sequence/time. Additions saturate rather than wrap;
reset starts a new interval without making lifetime operation sequencing go
backwards. The snapshot never includes a provider URL, signed target, token,
credential handle, manifest, transfer ID or local path.

The production HTTP transport permits at most 16 simultaneous operations and
reserves request bodies plus each operation's maximum response against one
128 MiB aggregate budget. REST, WebDAV and pre-signed providers separately cap
active sessions and minimal terminal idempotency records at validated values no
greater than 256. Terminal and expired sessions reclaim full state; pre-signed
target leases are released. The developer-only local integration provider uses
one resident byte budget for active and completed payloads and validates that
pending chunk count multiplied by maximum chunk size cannot exceed its total
budget.

## Structured diagnostic ring and export

The exact logging categories are `sstv.core`, `sstv.rx`, `sstv.tx`, `sstv.vis`,
`sstv.sync`, `sstv.storage`, `sstv.share`, `sstv.hamdrm` and `sstv.security`.
Significant lifecycle/error events pass a strict event-name and scalar-key
allowlist before entering a mutex-protected 512-event ring. The ring sequence is
monotonic across clear/reset; an export selects at most 256 events. Parser/audio
hot loops do not emit per-frame or per-sample events.

`SstvDiagnosticsController` accepts only allowlisted RX/TX/storage/share/HAMDRM
scalar maps and SSTV settings. It adds app/build, platform/ABI, capability flags,
the canonical mode-registry hash/version, calibration/test-tone results and the
bounded event selection. The version-1 JSON is capped at 1 MiB and published
atomically on a worker through `QSaveFile`. Unknown keys, path-like text,
credentials, URLs, person metadata, images and audio fail closed rather than
being redacted heuristically into an apparently safe report.
