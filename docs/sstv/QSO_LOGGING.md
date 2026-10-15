# Native SSTV QSO logging

## Scope and ADIF decision

SSTV QSO logging is part of the Decodium process and active Decodium
logbook. It does not create a second log database, helper process, or SSTV-only
ADIF file.

The protocol decision was checked against the released
[ADIF 3.1.6 specification](https://www.adif.org.uk/316/ADIF_316.htm) on
2026-08-24. `SSTV` is a value of the ADIF `MODE` enumeration and ADIF 3.1.6
does not define an SSTV `SUBMODE`. Related fax operation uses the separate
`FAX` mode and must not be logged as SSTV.

Decodium therefore exports an SSTV contact as:

```text
<MODE:4>SSTV
```

It does not emit a made-up `SUBMODE` for Martin, Scottie, Robot, PD, AVT, or
another image waveform. The precise native image mode remains first-class
Gallery metadata and is also recorded as ordinary human-readable `COMMENT`
text. No image path, raw-audio path, file URI, attachment name, or Gallery UUID
is written to ADIF.

## Local attachment identity

The active ADIF logbook has no stable, standard attachment identifier. Native
Gallery records therefore keep their relationship in the SQLite
`related_qso_id` column and JSON sidecar.

For a QSO already present in the active Decodium logbook, the local identifier
is deterministic:

```text
adif-sha256(SHA-256(CALL, QSO_DATE, TIME_ON, effective MODE, BAND))
```

Fields are upper-cased and `TIME_ON` is normalized to six digits before the
digest is calculated. This is a local reference only; it is never appended to
the ADIF record. If an operator edits one of the identity fields in the ADIF
logbook, the Gallery association must be deliberately refreshed to the new
identity.

## Required operator flow

1. The operator explicitly chooses **Log SSTV QSO** on a received,
   transmitted, imported, or draft Gallery image.
2. Decodium shows the image preview and editable callsign, grid, RF frequency,
   UTC, sent/received reports, and comment. The operator can instead select an
   existing QSO from the active logbook.
3. An explicit confirmation validates the request. Image detection, VIS
   detection, automatic saving, WAV replay, and remote import never invoke the
   log action.
4. For a new contact, Decodium serializes `MODE=SSTV`, validates the final
   byte-counted ADIF record, writes the active native logbook, and then queues
   the local Gallery association on the storage thread.
5. If the ADIF write fails, no association is reported. If the ADIF write
   succeeds but the later local association fails, Decodium reports the split
   outcome and leaves the QSO available for the explicit **associate existing
   QSO** recovery path.

The existing-QSO chooser and new-QSO duplicate guard read only Decodium's
already prepared native logbook cache. Gallery initialization starts the
existing `warmLogCacheAsync()` path. A cold or externally changed logbook
temporarily returns no choices or asks the operator to retry; neither path
parses ADIF on the QML thread. A choice is bound to the cache generation and
active logbook file metadata, then checked again after the Gallery-record
preflight. Changing or replacing the logbook therefore makes an old choice
fail closed instead of associating the image with a stale identity.

The final serialized-record guard rejects:

- a mode other than `SSTV`;
- any `SUBMODE`;
- malformed byte lengths, duplicate fields, invalid date/time/frequency, or
  out-of-band records;
- path, filename, attachment, or file-URI fields;
- path-like values and the local Gallery record identifier.

## Verification

`tests/sstv/test_sstv_qso_log.cpp` covers the exact, typed, path-free
QML/Bridge request boundary, request bounds, new/existing QSO identity, UTF-8
ADIF byte lengths, deterministic identifiers, `MODE=SSTV`, the absence of
`SUBMODE`, and fail-closed path/attachment cases.

`tests/sstv/test_sstv_qso_qml.cpp` loads the real dialog offscreen and verifies
the explicit-confirmation gate, new/existing workflows, UUID-only image
provider preview, a request token above JavaScript's exact integer range, and
the absence of image/audio/metadata paths in the request. The storage and
Gallery-model suites separately exercise atomic SQLite-sidecar association,
rollback after a forced database failure, restart persistence, disassociation,
and retention protection. These automated checks do not claim a human visual
review of the rendered dialog or a live external logbook-service upload.
