# SSTV Gallery favourites, quota and retention policy

Status: native Decodium4 implementation. All SQLite, filesystem and planning
work runs on the existing SSTV storage worker in the Decodium process. This
feature does not transmit RF, invoke PTT, contact a provider or remove a remote
copy.

## Persisted metadata and migration

SQLite schema version 5 adds `sstv_images.favorite` and the singleton
`sstv_retention_settings` table. Migration from versions 1 through 4 is one
SQLite transaction. Its safe defaults are automatic retention disabled, every
age/quota limit disabled, shared records protected and a 100-record batch.

Sidecar schema version 4 stores `favorite` as a required boolean. Sidecar
versions 1 through 3 remain readable and deterministically map to
`favorite=false`; the next metadata write uses version 4. A favourite toggle is
executed on the storage worker, atomically replaces the sidecar, then commits
the guarded SQLite projection. On a database failure Decodium attempts to
restore the previous sidecar and reports any restoration failure.

## Quota inventory

Quota calculation is non-destructive and reports separate byte totals for:

- lossless Gallery image PNG files;
- generated thumbnail PNG files;
- retained raw-audio files;
- metadata sidecars (diagnostic total, not mixed into the three quotas).

Each canonical path is counted once. Raw audio shared by multiple rows is
counted once. Checked 64-bit addition, a 100,000-row scan ceiling and explicit
missing/unsafe counters make overflow or an unbounded inventory fail closed.
Missing optional thumbnails or raw audio count as zero; missing image/sidecar
files and symlink/unowned paths make the inventory incomplete.

## Retention protections and ordering

The planner sorts candidates by capture time ascending and UUID ascending.
It selects at most the persisted per-run bound (1 through 500). A row is never
a retention candidate when any of these conditions applies:

- `favorite=true`;
- `relatedQsoId` is non-empty;
- upload/provider state marks it as shared, under the default Protect policy;
- a required file is missing, or any indexed existing path is a symlink,
  non-regular file or outside the owned SSTV storage root.

The optional `AllowUploaded` policy applies only to terminal `Uploaded` rows
that have both provider and remote-object identifiers. Pending, uploading,
failed or inconsistent sharing state remains protected.

Quota and age value zero means disabled. A preview never mutates files or
SQLite. It includes projected reclaimed bytes, protected counts, whether every
target can be met and a one-use token. Manual apply requires the exact displayed
`DELETE N GALLERY ITEMS` phrase, expires after ten minutes, and revalidates each
protection/path before entering deletion.

## Deletion and automatic policy

All retention deletion reuses `SstvStorageWorker::deleteRecordsWithFiles`.
Owned files are renamed into private `.delete-staging`, a bounded recovery
journal is committed, SQLite rows are deleted transactionally, and staged files
are then removed. Startup recovery restores files when rows still exist or
finishes cleanup after a committed row deletion. There is no retention-specific
`remove()` path.

Automatic retention is destructive and remains disabled by default. Enabling
it is an explicit persisted Settings action. Once enabled, Decodium runs the
same bounded planner and journalled deletion path at Gallery startup and after
record changes; disabling it stops future automatic runs. Manual preview/apply
remains available independently of that switch.

## User interface and operational limits

Gallery cards expose a star toggle. Gallery shows the three quota buckets,
supports recalculation and opens the strong-confirmation retention preview.
SSTV Settings exposes age, individual quotas, batch size, shared policy and the
explicit automatic-retention switch.

The implementation protects only paths inside Decodium's configured SSTV
storage root. It does not reclaim unrelated/orphan files, follow symlinks,
delete Gallery-index-only records automatically, change provider retention, or
promise recovery from storage hardware failure. Tests delete only data created
inside test-owned temporary directories.
