# Remote sharing E2EE: implementation in progress

An installable experimental HTTPS server is now available in
`tools/sstv-share-server`. Authenticated local A/B tests transferred two
images between full Decodium processes, both with and without pinned sender
signatures; the test checked stored ciphertext manifests and reconstructed
images. Separate server tests cover account isolation, invalid credentials,
integrity failures, expiry, quotas, token rotation and upload resumption
after server restart. The installed CLI also passed a CA-verified local HTTPS
smoke test with two separate accounts. This is not an Internet deployment or
an independent security audit; the service README records current limits.
The installed Python package was additionally used by the signed and
unsigned two-Decodium A/B labs after the server's structural manifest checks
were added. Artifacts are `.../decodium-share-server-ab-7r3jpzjz` and
`.../decodium-share-server-ab-3bhjrpi7`. These runs use temporary local
credentials and Keychains; no user account or production provider was used.

After rebuilding the 1.0.736 source, which includes the FT4/FT8/SDR lifecycle
changes, the signed two-application transfer and both signed nonce-tamper
restart regressions were run again on macOS ARM64. All three passed: the
isolated HTTPS fixture confirmed that the altered signed manifest was rejected
before its content was requested, each restarted receiver retained its
recipient identity, and the original A/B images were subsequently decrypted
and compared with their sources. These tests exercise a local fixture and
temporary Keychain, not a production provider, a security audit or every
possible interruption.

The independent-signing-key impersonation A/B laboratory now records content
requests by inbox identifier and requires zero requests for the forged sender's
item. In B it also requires an actual content request for the preceding valid
A image; that image is compared with its source. The fixture's own A/B actor
tests check that it counts both a normal download and a tampered-ciphertext
download, guarding against a counter that always reports zero. After this
change both full-Decodium forgery cases and the fixture tests passed, as did
the normal signed transfer. This is a local HTTPS/Keychain attack simulation;
it does not prove sender identity without independently verified pins or
cover every provider and attack.

Two additional signed A/B application tests substitute the ciphertext of the
other completed image and update the target manifest's byte size and SHA-256,
while preserving its original sender signature and all other fields. The
fixture verifies that the ciphertexts, nonces and ephemeral keys differ and
that only the intended manifest fields changed. Both receivers rejected the
invalid signature before requesting the substituted content. In the B case,
the preceding untampered A content was requested and reconstructed. The
normal signed transfer, unsigned payload-substitution A/B controls and HTTPS
fixture test also passed. These are isolated local two-process tests, not
evidence for every provider or an independent security review. Run
`ctest --test-dir build -R '^test_sstv_share_app_signed_payload_swap_[ab]$' --output-on-failure`.

The signed payload-tampering A/B application laboratory now also checks the
exact attack construction: the fixture flips one ciphertext bit, recomputes
its SHA-256 in the signed manifest and retains the original signature. Both
receivers rejected the signature before requesting the altered content. In
the B case the preceding valid A image was requested and reconstructed. The
normal signed transfer, fixture test, and unsigned payload-tampering A/B
controls also passed. These are isolated local HTTPS tests and do not prove
behaviour against every provider or establish an independent security audit.

Two full Decodium application A/B tests now also exercise a provider that
changes one bit of the 12-byte AES-GCM nonce in the manifest while retaining
valid canonical Base64 and recalculating the manifest digest. The receiving
application rejects each modified encrypted transfer as an authenticated-open
failure; in the B case it first receives the unaffected A image. The local
HTTPS fixture checks that its alteration occurred. The existing AEAD,
ephemeral-key tampering and signed-transfer regressions also passed. Run
`ctest --test-dir build --output-on-failure -R '^test_sstv_share_app_nonce_tamper_[ab]$'`.
The provider and certificates are isolated test fixtures, and this checks
the unsigned encrypted envelope rather than independently proving every
provider behaviour or an external security audit.

Signed encrypted A/B transfers were also tested against the same validly
encoded nonce alteration. The receiver rejected the sender signature and the
fixture recorded no request for the altered item's content. The fixture now
checks this no-download condition for every signed manifest-tampering case;
both existing signature-alteration controls passed with the new check.
Separate signed A/B regressions confirmed invalid-then-valid handling and
replay rejection after restart. Additional signed A/B app tests now combine
the sequence: the local HTTPS fixture first supplies a copy whose only changed
manifest field is a one-bit nonce alteration, retains the original ciphertext,
and waits for the receiver to reject its signature. The laboratory then
terminates and restarts that receiver with the same isolated settings,
Keychain and provider, releases the original valid signed item, and compares
both received images with the originals. No content was requested for the
altered item. Each run also compares the recipient public key and fingerprint
before and after restart and confirms that both valid manifests name that
same fingerprint. The restart uses a process termination, not a simulated power
failure, and the test uses a loopback fixture rather than a public provider.
The existing invalid-then-valid and legacy-record A/B regressions also passed.

Verified sender signing pins can now be replaced when a sender obtains a new
P-256 key. The receiver requires the expected current fingerprint, the new
public key and an independently confirmed new fingerprint. The successor is
inserted atomically, the current chain is re-read, and stale, conflicting,
invalid, cyclic or corrupt entries fail closed. Initial pin creation also
rechecks the current chain tip after insertion. Replacement changes trust to
the new key: old signatures no longer authenticate under the current pin,
including messages still awaiting verification. Already imported images are
not removed. The sharing page states this consequence, requires confirmation
and clears it after success or edits.

In-memory A/B tests reject stale or incorrect fingerprints and injected write
failures, verify that old signed envelopes cease to authenticate, and decrypt
new signed envelopes after replacement. They also reject invalid and cyclic
successor records without overwriting them. A macOS temporary-Keychain QML
test rejects unconfirmed replacement and performs two verified consecutive
updates, checking confirmation is cleared. A full-application test first
transferred signed images, then restarted the receiver using its existing
profile and Keychain while a separate sender with a fresh Keychain published
a new signing key. After controller-driven verified replacement, two fresh
signed encrypted images were transferred over loopback HTTPS, decrypted and
compared with the originals; manifests named the new fingerprint. The
receiver replacement was driven by the lab controller, not a human QML click.
The cryptographic, QML, controller, signed-transfer, replacement and existing
recipient-rotation/restart CTests passed on macOS ARM64. This test uses an
isolated HTTPS fixture, not a public provider. Whole-store rollback or deletion
of a successor can still restore an older pin; the chain is not a monotonic
trust anchor.

The local P-256 signing identity can also be rotated through an explicit
confirmation dialog. Rotation requires the displayed current fingerprint and
atomically inserts a new private-key successor in the OS secure store. The
previous private keys remain stored as part of the chain; this is not secure
erasure of a compromised key. The dialog tells the operator to share and
independently verify the new fingerprint with recipients, who must replace
their sender pin before new signatures authenticate. Old messages may cease
to authenticate once a recipient changes that pin. Missing or corrupted
successors and whole-store rollback are not distinguishable from an older
valid chain; there is no external monotonic anchor.

In-memory A/B checks cover repeated rotations, stale fingerprints, injected
storage failure, corrupted and cyclic private-key chains, and signed-envelope
verification before and after the recipient updates its pin. Two concurrent
helper processes against a temporary macOS Keychain attempted the same
rotation; one succeeded and the other conflicted. The offscreen QML test
cancelled a rotation and performed two confirmed rotations, checking the
current key after each. In the full-application A/B test, the existing sender
process profile and Keychain were restarted, the sender rotated its signing
key, the receiver replaced its verified pin, and two fresh signed encrypted
images were transferred and checked through the loopback HTTPS fixture. A
further sender application restart loaded the new fingerprint. The lab drives
controllers rather than a human QML interaction; it is not a public-provider
test. The crypto, QML, controller, native Keychain and signed application
regressions passed on macOS ARM64.

The signing-key rotation test now also exercises old signatures in two full
Decodium A/B sessions. After the receiver independently updates its pin, the
isolated HTTPS fixture republishes the exact old-key A or B manifest and
ciphertext under a fresh inbox ID. The receiver attempts that specific ID,
rejects its sender signature before requesting content, and still downloads
and decrypts both new-key images. The fixture checks that the replayed bytes
are identical to the original and records zero content-download requests for
the replay; the sender's rotated fingerprint also survives restart. The two
`test_sstv_share_app_retired_signing_{a,b}` cases passed, as did the existing
signed high-S replay A/B, sender-pin replacement and signing-rotation
application regressions. This checks exact old-envelope replays through a
local fixture; it does not model every possible newly forged old-key message,
a public provider, secure deletion or rollback of the key store.

The A/B application check was extended beyond exact replay. While the
receiver was stopped and before the sender rotated its key, a restarted
sender uploaded two additional signed encrypted images that the receiver had
never seen. Their manifests and ciphertext differed from all initial
transfers; OpenSSL independently verified each signature against the old
public key. After the sender rotated and the receiver replaced its pin, the
fixture presented one of those previously unseen old-key items. In each A/B
run the receiver rejected the sender signature before any content download,
then received and decrypted both freshly signed images using the new key.
The two `test_sstv_share_app_unseen_old_signing_{a,b}` cases passed, as did
the exact-replay, rotation and ordinary signed-transfer regressions. This
uses an isolated loopback HTTPS provider and prepared pre-rotation messages;
it does not establish behaviour against every possible malicious provider or
repair secure-store rollback.

Opening a stored envelope can select a previously archived X25519 identity by
the recipient fingerprint in its manifest. The key's public fingerprint is
recomputed before use; a mismatched or missing archived identity is rejected,
and corrupt active identity storage is not bypassed. Signed A/B envelopes were
opened in both directions using an in-memory archive, with corruption and
missing-key controls. A separate macOS test created an isolated temporary
Keychain, replaced its active identity and opened two earlier envelopes using
the archived identity in separate receiver processes. The signed A/B transfer
between two complete Decodium processes was also repeated using active keys.
That application transfer did not exercise an archived identity. The key
store now also provides an immutable, insert-if-absent archive operation: it
checks the stored bytes and never overwrites an existing entry. A/B in-memory
tests cover idempotency, a conflicting entry and a storage failure; the
temporary macOS Keychain test uses the operation before its two receiver
processes open earlier envelopes. A new internal rotation operation uses a
fingerprint-confirmed, insert-only successor chain after archiving the prior
identity. A/B in-memory tests rotate both recipients, open old signed
envelopes, decrypt new envelopes and reject stale requests and cyclic chains.
The isolated macOS Keychain test invokes this rotation and opens two older
envelopes in receiver processes. This operation is not exposed in the UI:
full application rotation, broader interruption and concurrency coverage,
peer trust updates and rotation UI remain open.

Two separate helper processes now race to rotate the same fingerprint in an
isolated macOS Keychain: one succeeds, one reports a conflict, the winning
public identity reloads, and receiver processes still open both old payloads.
The A/B in-memory suite also injects a successor-write failure after archiving,
checks that the previous identity remains active, and retries successfully.
This models one interruption point; it is not a process-kill or power-loss
test, nor a concurrent full-Decodium rotation test.

The isolated macOS Keychain test now also starts two archiver processes
concurrently against the same identity; both return the same public key and
the archived key opens the two earlier encrypted payloads in receiver
processes. A conflicting pre-existing archive is rejected without changing
either the archive or the active item. This covers concurrent archive
creation, independently of the subsequent concurrent rotation check above.
The separate signed two-Decodium HTTPS test was repeated with active keys.

The worker/controller now exposes an internal fingerprint-confirmed rotation
call. In a signed loopback HTTPS laboratory run, the receiving Decodium
process published its initial identity, rotated it through that controller,
and then received and decrypted images A and B sent by a second Decodium
process using the initial public key. The lab checks that rotation occurred
before the sender is released. This proves old-key reception through the
application queue after rotation; it does not exercise a user-facing rotation
control or replacement of a peer's pinned key.

The key store now has an internal verified recipient-pin replacement: it
requires the expected current fingerprint and the independently verified new
fingerprint, validates the new X25519 point and uses an insert-only successor
entry. The A/B in-memory tests reject incorrect or stale fingerprints, then
replace both pins and exchange signed envelopes to the new recipient keys.
This primitive is not yet exposed by the controller or UI and has not yet
been exercised between two complete Decodium processes; the application lab
above still sends to the previous pin.

The worker/controller now also exposes verified pin replacement. In the
signed two-Decodium loopback HTTPS lab, the receiver rotates its identity,
the sender transmits image A to the previously confirmed pin, explicitly
replaces the pin through the controller, and transmits image B to the new
pin. Both images are decrypted and their manifest fingerprints are checked
against the old and new identities respectively. The test models independent
confirmation in the isolated lab; user-facing confirmation and controls
remain to be implemented and tested.

The sharing page now provides a confirmation dialog for rotating the local
recipient key and a verified pin-replacement control requiring the previous
fingerprint and independent confirmation of the new one. A macOS offscreen
QML/controller test using a temporary Keychain cancels one rotation, then
performs two rotations and confirmed pin updates; it checks the replacement
button is disabled before confirmation. The signed two-Decodium A/B old/new
pin transfer was repeated successfully. That application test drives the
controller laboratory, not the QML controls, and does not constitute a human
verification of the fingerprint exchange or a packaged GUI test.

The QML confirmation is now cleared after a successful pin or replacement, as
well as when the recipient, either public key/fingerprint field, previous
fingerprint or provider configuration changes. The isolated Keychain QML test
checks the four editable fields in both rotation iterations and requires a
fresh confirmation after each edit and successful replacement. The A/B
two-Decodium signed transfer was repeated; it still drives the laboratory
controller and does not validate human use of the interface.

Two separate processes now attempt to replace the same recipient pin with
different verified keys in an isolated macOS Keychain. Exactly one succeeds;
the other reports a conflict, and the stored current pin matches the winner.
The signed two-Decodium old/new-pin A/B transfer was repeated successfully.
The insert-only chain does not detect rollback of secure storage: deleting a
successor entry or restoring an older Keychain snapshot can make an earlier
pin appear current. The native test deliberately removes its isolated test
successor to restore its fixture, demonstrating that limitation. A monotonic
trust anchor or equivalent rollback detection remains unresolved.

Initial recipient pinning now reads the current successor-chain tip after
insertion. Previously, a concurrent verified replacement between insertion
and readback could leave a different key current while the initial pin call
reported success for the old key. Two in-memory A/B controls inject the
replacement at that point, require a conflict, and verify that the
successor remains current; the first control failed before the change. The
signed two-Decodium rotated-identity, pin-replacement and restart test passed
after rebuilding, transferring and decrypting its A/B images. The full-app
test does not inject the race, and the readback does not prevent a legitimate
later rotation. This does not address whole-store rollback.

The race is now also exercised A/B against a temporary macOS Keychain: a
test wrapper pauses the initial pin immediately after the native insertion,
starts a separate process that performs a verified replacement, then resumes
the initial call. Both key orders report conflict and retain the replacement.
Rebuilding with the previous root-only readback makes the native test fail;
the corrected readback passes. The signed two-Decodium rotation/restart A/B
transfer and in-memory cryptographic suite also passed after restoration.
The race itself is between helper processes, not two full Decodium windows.

Additional A/B in-memory controls corrupt a recipient successor with invalid
encoding and a low-order X25519 point, and separately insert a cyclic link.
Each corruption blocks signed envelope creation without replacing the stored
record; restoring the fixture permits sealing again. These checks do not
detect a missing successor or rollback of the whole secure store. The existing
signed two-Decodium old/new-key exchange was repeated with valid storage.

Following an identity successor now requires its predecessor's archived
private key to exist and match the predecessor. Missing or mismatched archives
fail closed rather than silently offering a current identity that cannot open
older messages. In-memory A/B tests remove and substitute that archive, then
restore it and successfully open the prior signed envelope. Two successive
rotations in each direction also preserve signed envelopes addressed to the
original and intermediate keys. The signed two-Decodium A/B transfer was
repeated with valid storage. This is not a fix for deletion of a successor
or rollback of the entire secure store, nor a full-application restart test.

The signed two-Decodium old/new-pin A/B transfer now also restarts the
recipient application with the same test profile and Keychain after both
processes exit. The restarted application reports the same rotated public
key and fingerprint. This checks persistence of the active recipient
identity; it does not yet resend or decrypt the images after restart, or
verify sender-pin persistence in a restarted application.

The same test now restarts the sender with its existing profile, Keychain and
provider configuration, explicitly skips pin creation/replacement, and uploads
two additional signed images. The HTTPS fixture verifies both completed
manifests target the rotated recipient fingerprint. The receiver identity is
reloaded separately after restart; these additional uploads are not downloaded
or decrypted by a restarted receiver in this test.

The restarted receiver now also downloads and decrypts those two additional
signed uploads using its existing profile, Keychain and sender-signing pin.
The lab compares both resulting images against their original RGB pixels.
The sender and receiver run sequentially after their initial restart, using
the loopback HTTPS fixture; no provider interoperability or human fingerprint
comparison is implied.

Stored X25519 recipient pins are now revalidated on read, as sender signing
pins already were. A correctly encoded but unusable low-order stored point is
reported as corrupt rather than returned as a usable recipient key. Two
low-order-point controls modify only an in-memory test backend, verify
rejection, then restore and reload the valid pin. The cryptographic component
suite passed, as did the signed A/B two-Decodium transfer test using isolated
Keychain identities and loopback HTTPS after rebuilding the application.
This does not implement key rotation or establish public-provider behaviour.

The component suite now also corrupts the stored recipient pin in both
directions independently. Unsigned and signed envelope creation both fail
without changing the test key store; after restoring each valid pin, a signed
envelope is created and opened by the other identity. These are in-memory
key-store controls. The preceding two-application test exercised valid pins,
not an operating-system keychain containing a corrupt pin.

Attempting to pin a valid recipient over either of the two unusable stored
points now reports corrupt storage rather than a normal key conflict, without
overwriting the existing record. The A/B in-memory controls check the error
and absence of writes. The signed two-process A/B transfer with valid pins
was repeated after rebuilding and passed; a corrupt OS-keychain application
test remains open.

The macOS Keychain component test now uses a newly created temporary file
keychain to insert each of two unusable X25519 public pins. In each case it
checks corrupt status, refusal to replace or seal with the pin, and byte-for-
byte preservation of the invalid test record. After restoring the valid pin,
two encrypted payloads are opened by separate receiver processes. The test
passed and deletes its temporary Keychain. These are native Keychain and
separate helper-process checks, not a corrupt-pin test through the complete
Decodium GUI or remote-sharing provider.

The controller now reports corrupt recipient and sender pins distinctly from
invalid confirmation or an ordinary key conflict; unavailable secure storage
also has its own error. Sender-pin insertion revalidates an existing stored
P256 key before classifying a conflict. An offscreen QML/controller test
uses a temporary macOS Keychain, clicks the pin controls with two corrupt
recipient fixtures and one corrupt sender fixture, checks the respective
operation messages and restores the original test pins. The QML, crypto and
signed two-Decodium A/B transfer tests passed together. The corrupt-pin QML
test uses an in-process local provider; the two-process transfer uses valid
pins, so it does not prove a corrupt-pin provider exchange.

The operation notice is now in a fixed footer outside the scrolling content,
so the pin result need not be sought at the end of a long page. Offscreen
QML checks place the sender error inside both 1040x900 and 800x500 page bounds
at two scroll offsets; recipient error text is also checked for two corrupt
pins. The signed two-Decodium A/B transfer passed after rebuilding. These
are geometric QML checks on this Mac, not a visual survey of all window sizes
or a full GUI corrupt-pin exchange with a remote provider.

The two corrupt-recipient cases were subsequently rendered with the QML
software backend at 1040x900. Both saved window images show the error notice
at the bottom and the sharing controls above it; they were visually inspected
on this Mac. The optional test-only screenshot directory is selected with
`DECODIUM_SHARE_NOTICE_SCREENSHOT_DIR`. This is an offscreen rendering using
the test style, not a visual inspection of the packaged application on every
desktop environment.

The offscreen QML regression also checks that the corrupt sender and recipient
errors reach the page's visible notice label with error state. This checks
the QML object properties, not whether a scrolled viewport places the label
onscreen or a human can see it on every window size. The signed A/B
two-Decodium transfer with valid pins was repeated and passed.

The REST OpenAPI `Encryption` schema now admits the three optional sender
authentication fields as an all-or-none group and limits their combination to
end-to-end AES-256-GCM. Previously `additionalProperties: false` rejected
actual signed manifests, and the description incorrectly said no envelope was
implemented. The schema remains a structural contract: canonical signature
encoding, key pinning and cryptographic verification are client checks.

Two Decodium processes transferred and decrypted images A and B over the
verified loopback HTTPS fixture, first with signed envelopes and then with
unsigned envelopes. In each run, Python jsonschema Draft 2020-12 validated the
two actual manifests against `ManifestV1`; for each signed manifest, removing
any one sender-authentication field or changing its mode or encryption
algorithm was rejected. Image A/B mean RGB error was 0.3333, maximum 1, in
both runs. The optional laboratory check uses `DECODIUM_LAB_VALIDATE_OPENAPI=1`
and requires separately installed Python `jsonschema` and PyYAML; ordinary
application tests have no such dependency. An initial CTest invocation used
the system Python with a module installed for a different Python version and
failed at import after successful image transfer; the successful runs used the
matching interpreter. These results do not establish interoperability with a
deployed provider or an independent cryptographic audit.

The OpenAPI schema also enforces the mode-dependent encryption fields and
the relationship between the mode and `providerCanReadContent`. For each of
the real signed and unsigned A/B manifests in repeated two-application HTTPS
transfers, the schema accepted the original and rejected altered nonce,
empty key ID or recipient fingerprint, missing authenticated-data binding,
and a claim that the provider can read E2EE content. Converted TLS-only
structural A/B controls were accepted, while inconsistent TLS-only nonce,
key ID, authenticated-data binding or provider visibility were rejected.
The TLS-only controls were derived from the received E2EE manifests; they
were not separate TLS-only application transfers. Schema validation still
does not prove encryption or key authenticity.

Replay lookups now use two additive SQLite indexes on direction, provider,
and respectively manifest identity or payload hash. Existing schema-v3
queues acquire them transactionally on open without changing the schema
version, stored manifests or idempotency keys. Later opens check for existing
indexes before taking a writer transaction. A/B queue tests start from v3
databases without these indexes, simulate a failed index-creation commit and
verify rollback plus preservation of the existing transfer, reopen to create
the indexes, check SQLite's query plans and reopen while a separate writer
transaction holds a lock. Both variants pass.

A synthetic in-memory SQLite comparison of 30,000 same-provider rows on this
Mac found about 2.8 ms per exact-key or payload-hash lookup with the former
provider index, versus about 0.004–0.005 ms with the composite indexes. This
microbenchmark is not a full-application latency or memory measurement. Two
Decodium A/B tests also removed the indexes from an isolated existing v3
queue after an accepted signed transfer, restarted the receiver, verified
both indexes were restored and the accepted image retained, and confirmed a
re-signed replay was rejected before content download. Tests
`test_sstv_share_app_signed_replay_index_upgrade_a` and `_b` passed using
temporary Keychains and loopback HTTPS; this does not establish production
provider performance or an independent security audit.

The receiver now verifies signed incoming manifests against the pinned sender
key before adding them to the download queue. An absent verifier, unpinned
key, wrong fingerprint or invalid signature rejects the operation without
requesting content. This prevents a newly presented invalid signature from
reserving a signed-envelope duplicate identity and blocking a subsequent
valid signature. The worker performs this check, and authenticated opening
still checks the signature again before decryption; pin changes can therefore
invalidate a queued item. Unsigned E2EE follows the existing sender policy.

Two-process A/B HTTPS tests present a signature-altered copy first and the
genuine signed envelope afterwards, with identical ciphertext and all other
manifest fields unchanged. The fake is rejected before any content request;
both genuine images are then decrypted and compared, with mean RGB error
0.3333 and maximum 1. The queue test also verifies A/B failure with no
verifier and with an independently signed forgery before admitting the
valid envelope. These tests use isolated identities and a local fixture.
At that stage, pre-existing invalid queued records from an older build had
not been separately verified.

The duplicate comparison now rechecks the signature of a matching older
signed record. A cryptographically invalid prior signature does not block a
valid incoming envelope; a valid prior signature still blocks the replay. If
the pinned-key store cannot be read reliably, the operation fails closed and
the SQLite transaction is rolled back. No previous record is deleted or
modified by this check. A/B queue tests simulate a pre-upgrade invalid queued
record, an unavailable verifier, the later valid arrival, and restart; the
crypto suite separately distinguishes valid, invalid and unavailable-key
results for both signing directions.

Two further application A/B tests use isolated Decodium processes, Keychain
and loopback HTTPS. After a fake signature is rejected, the harness inserts a
paused, invalid-signature legacy record into that test session's SQLite queue
and confirms it remains present. The same signed fields and ciphertext with
the legitimate signature are then admitted and both images are decrypted
and compared. The simulated record is inserted directly into a guarded
Qt-test storage directory; this is not a migration from an installed older
release. The key-store-unavailable case is covered by component tests, not by
the two-process fixture. These results do not establish public-provider or
RF interoperability or an independent security audit.

On 2026-11-03 signed-envelope replay detection was extended beyond exact
signature bytes. ECDSA can produce different valid low-S signatures for the
same signed manifest, so the queue now compares the canonical signed fields
with the signature omitted against active or accepted signed downloads from
the same provider with the same ciphertext hash. The comparison is inside
the existing insertion transaction, before content download and history
reclamation. Existing persisted idempotency keys and schema are unchanged.
Failed, cancelled, rejected and expired records remain retryable; a distinct
signed manifest remains eligible. This is a duplicate check, not signature
verification. At that stage an unverified queued entry with matching signed
fields could temporarily block another; the pre-queue signature check above
now addresses newly queued items, while opening retains its own check.

An A/B queue regression generated two different valid low-S P-256 signatures
over exactly the same signed bytes and independently verified both. It failed
before the change and now rejects the second entry both while the first is
queued and after acceptance plus queue restart. The test also signs a distinct
manifest and verifies that a cancelled entry can be retried with another
valid signature. Its queue uses a fake payload-opening fixture, so it is not
an end-to-end cryptographic transfer test.

Two Decodium applications then transferred signed encrypted A/B images through
the verified local HTTPS fixture. After acceptance and receiver restart, a
replayed envelope with the mathematically equivalent high-S signature was
rejected as a duplicate before content download; the accepted image and
Gallery inventory remained unchanged. Both A/B cases, exact-replay controls,
and separate high-S authentication-rejection A/B controls passed. At that
stage the latter had no previously queued equivalent and reached
cryptographic opening; the pre-queue check described above now rejects them
before the content request.
The high-S replay tests no longer exercise the earlier delayed-download busy
path described below because the duplicate is now rejected before download.
At that stage, valid-low-S re-signing had only been tested at queue level.
These checks do not establish a public-provider
deployment, an independent key-verification ceremony or a security audit.

Two subsequent full Decodium application tests now cover that valid re-signing
case for A and B. In isolated test mode the sender uses its temporary Keychain
identity to make a second low-S signature over the original canonical signed
bytes; it verifies both signatures and writes only the second public signature
to the laboratory directory. On macOS the harness also verifies both
signatures separately with the OpenSSL command-line tool and the sender's
published public key. The fixture re-presents the same ciphertext and signed
fields with the new signature and a fresh inbox ID after the receiver has
accepted the original and restarted. Both tests require rejection before any
replay content request and check that the accepted transfer and Gallery files
remain unchanged. The test hook rejects operation outside test mode and its
output path outside the isolated laboratory directory. The A/B tests are
`test_sstv_share_app_signed_replay_resigned_a` and `_b`; they use a simulated
trusted-key exchange and loopback HTTPS, not a public provider or independent
security audit. The isolated-Keychain helper experiment was denied access by
Keychain; no private key was exported to that helper or to the fixture.

The coordinated create-only backend now rejects an inconsistent lookup that
reports a missing item while returning a nonempty value, including the lookup
performed under its cross-process lock. Previously that response could trigger
a write over an existing item. A test-only backend reproduced the failure for
two existing values; after the fix both remain unchanged with no write.
The coordinated-backend, secure-settings and AEAD tests passed on macOS ARM64,
as did two-application HTTPS transfer and targeted ciphertext-tampering A/B
tests. The contradictory response was injected in a fixture; the full app
tests used macOS Keychain, not the Windows or Linux coordinated backends.

On 2026-11-01 the controller's cryptographic path was made available to
OpenSSL-enabled Windows and Linux builds. It uses a per-user cross-process
lock to coordinate create-only identity and pin writes over Windows DPAPI or
Linux `secret-tool`; those backends are explicitly selected because private
values are not put into command-line arguments. On Linux, an accessible
Secret Service and the host `secret-tool` executable are required. Missing or
failing storage must reject the operation; there is no plaintext fallback.
Two-application A/B transfer and tampering tests pass on macOS ARM64 and on a
Debian 13 ARM64 virtual machine using the real GNOME Secret Service. Windows,
Linux x86_64, release packaging and other desktop keyrings remain unverified.
Those initial tests used unsigned envelopes and did not authenticate the
sender. The later optional signed-envelope and strict sender-policy work is
described below; the protocol has not had an independent security review.

The Windows CI configuration now registers four full-application DPAPI lab
cases: A/B HTTPS transfer, ciphertext tampering, and manifest tampering for
each image. The test-only entry point requires an explicit opt-in, a sentinel,
and existing absolute isolated settings and lock roots; the harness places
both applications and their DPAPI-backed INI files in temporary roots. It
also checks that non-test launches, missing opt-in or roots, and invalid
sentinels are rejected. The Windows runner installs its Python and FFmpeg
dependencies and fails if the four cases are not registered. This is test
infrastructure, not a Windows pass: no Windows runner was available for this
worktree. The existing macOS two-application identity, A/B transfer and three
tampering tests were rerun after these changes and passed. Native SSTV CI now
also triggers for security backend, secure-settings test and CI script
changes; its Windows, Linux and Intel jobs still need execution.

The macOS release workflows build OpenSSL 3.5.8 from its SHA-256-pinned
official source for each architecture and declared minimum macOS version.
The release script requires E2EE, bundles the OpenSSL licence, and checks the
executable's `@rpath` link, library architecture and minimum OS version even
when the general Intel compatibility check is skipped. A local cross-build
produced x86_64 `libcrypto` with minimum macOS 13.0. A small runtime check
executed it under Rosetta and passed two AES-256-GCM round trips, tag-tamper
rejection and two X25519 exchanges; the same checks passed natively with an
arm64 build. This does not verify the complete Intel application or an Intel
Mac. The arm64 build was also used in two full Decodium
processes for HTTPS encrypted A/B image transfer and ciphertext-tampering
rejection. A fresh CMake configuration with `DECODIUM_REQUIRE_SSTV_E2EE=ON`
compiled and passed `test_sstv_share_aead`, with its Mach-O dependency pointing
to the newly built arm64 `libcrypto`. A second A/B run loaded the same library from an `@rpath`-linked
test `.app` bundle. That fixture used locally installed Qt and a symlink to
the development QML tree, and is not a packaged-DMG test. The actual GitHub
release workflows, their final bundles, and older macOS installations remain
unverified. The gate's local A/B and negative checks are reproducible with
`bash tests/sstv/tools/test_macos_sstv_crypto_gate.sh`.
Both macOS release workflows also run
`tests/sstv/tools/test_macos_openssl_runtime.sh` against their freshly built
library before building release assets.

The Linux secret-tool lookup now distinguishes the normal missing-item
result (exit 1 without output) from an unexpected exit, diagnostics, or
partial output. An ambiguous failure cannot be treated as permission to
create a replacement identity. This is based on the
[libsecret secret-tool lookup implementation](https://github.com/GNOME/libsecret/blob/main/tool/secret-tool.c).
The same process wrapper and coordinated create-only backend were exercised
on macOS with two synthetic identities, missing items, errors, a terminated
helper and partial output. The actual Linux backend was then compiled on an
isolated Debian 13 ARM64 VirtualBox guest and exercised with GNOME Keyring 48
and `secret-tool`: for two different identities, two competing processes
produced exactly one create and one already-exists result, and the winning
value was read back and removed from the real Secret Service. The reproducible
actor is `tests/sstv/tools/linux_secret_service_lab.cpp`. This test does not
by itself prove a full application transfer or x86_64 Linux behavior.
An additional Linux ARM64 application test built Decodium with Qt 6.8.2,
HAMDRM and required E2EE, then used two isolated application processes,
GNOME Keyring and a verified loopback HTTPS fixture. Both images were received
with a mean RGB error of 0.3333 and maximum 1; altered ciphertext and altered
manifests for images A and B were rejected. The four registered CTests
`test_sstv_share_linux_*` passed on Debian 13 ARM64. The wrapper
`tests/sstv/tools/run_share_linux_secret_service_lab.sh` uses a private D-Bus
session, temporary XDG data and an explicit test-mode opt-in; Linux CI installs
the required tools. The Linux lab runs without a QML window. Earlier attempts
that quit during asynchronous BootLoader loading crashed in Qt QML engine
teardown; this lab-specific change avoids that race but does not prove a fix
for graphical application shutdown. The actual GitHub jobs and AppImage
bundles were not run in this experiment.
Windows DPAPI removal now
syncs QSettings and reports storage errors. The shared QSettings removal helper
passed A/B removal and error checks on macOS; the Windows DPAPI runtime path
has not been executed here. Following these changes, the macOS full-application HTTPS
transfer A/B and altered-ciphertext rejection tests passed again.

The Windows DPAPI lookup also distinguishes an absent settings entry from a
present but empty protected blob. The latter now returns an error, so the
coordinated create-only identity path cannot silently replace it. A portable
QSettings-backed A/B test covers absent, present, empty and inaccessible
entries, and checks that an empty existing value is not overwritten. A
Windows-only DPAPI test adds A/B empty and malformed blobs to the native
create-only test; it has not been run on Windows in this worktree. On macOS,
the two-application encrypted A/B transfer and ciphertext/manifest tampering
tests passed after this change; those use Keychain and do not prove DPAPI.

`SstvShareAead` is an optional OpenSSL AES-256-GCM building block. It creates
fresh 96-bit random nonces and uses 128-bit tags. Decryption returns no
plaintext when authentication fails and clears its temporary plaintext buffer.
Input sizes are bounded. Its caller supplies an independently generated
32-byte secret key and associated data; the caller must ensure safe key
lifecycle and bound the number of encryptions per key.

`SstvShareEncryptedPayload` adds an experimental payload representation:
ciphertext followed by the 16-byte authentication tag, with the nonce in the
manifest. The authenticated data is a domain-separated canonical manifest
with the ciphertext SHA-256 and nonce fields omitted: the hash depends on the
tag and is verified separately, while GCM authenticates the nonce implicitly.
The payload size, recipient, key identifier, fingerprint, mode and other
manifest fields remain bound. An altered payload is rejected even if its
unkeyed hash is recomputed.

`SstvShareKeyAgreement` adds X25519 key generation and a role-ordered
X25519/HKDF-SHA256 transfer-key derivation. Both public keys and a bounded,
nonempty transfer context are bound to the derived key. It rejects a private
key that does not match its declared role and rejects invalid low-order peers.
The recipient public key must be authenticated independently; this primitive
does not authenticate the sender. The caller must handle private keys and
derived keys securely: their current QByteArray representation can make copies
and does not provide guaranteed erasure.

`SstvShareRecipientEncryption` combines these primitives: a fresh ephemeral
X25519 key is generated per encryption; its public part is encoded in the
manifest key ID as `x25519-v1-` followed by lowercase hex. The recipient key
fingerprint is SHA-256 of its 32 raw public-key bytes. The sender must supply
that fingerprint from an independently authenticated pin, not from the same
untrusted provider supplying the public key. The recipient checks its private
key against the fingerprint before decryption. The manifest and payload format
are experimental and have not received independent security review.

The unsigned construction does not authenticate the sender: anyone who knows the
recipient public key can create an encrypted payload asserting an arbitrary
sender. The later signed-envelope extension requires an independently verified
sender pin and a valid signature. Sender authentication must not be inferred
merely from an unsigned or unverified manifest.

Two further macOS ARM64 two-application HTTPS tests exercise cross-transfer
ciphertext substitution. The loopback provider waits for two completed
encrypted uploads, replaces A's payload with B's or B's with A's, and updates
the target manifest's public size and SHA-256. The fixture requires different
ciphertexts, nonces and ephemeral key identifiers. Both substituted payloads
were rejected by authenticated opening; in the B test the unchanged A image
was received and checked first. The clean two-image exchange and AEAD suite
passed in the same run. Tests are registered for Windows DPAPI and Linux
Secret Service but have not run on those platforms for this change. This
tests substitution between two transfers to one recipient, not sender
authentication, replay of a complete valid envelope, or a public provider.

```sh
ctest --test-dir build -R '^test_sstv_share_app_payload_swap_[ab]$' --output-on-failure
```

An exact encrypted envelope can also be replayed with a different
provider-controlled inbox ID. Download insertion now checks, in the same
SQLite transaction and before history reclamation, for the same canonical
manifest idempotency key already queued or accepted from that provider. It
rejects a duplicate before requesting its content. A different transfer is
still allowed; failed, cancelled, rejected or expired transfers do not block a
new attempt, and rejecting a duplicate inbox entry remains available. This
check applies while the relevant transfer history is retained. For an
acknowledged E2EE download, both transfer- and inbox-capacity reclamation now
preserve the paired record until the manifest expires, even after its staged
files are removed. If capacity contains no other reclaimable rows, insertion
fails rather than discarding that evidence. At expiry the row is reclaimable
again. Failed, cancelled and rejected transfers can still be retried. This
does not cover a different provider, changed envelopes, deleted local state,
an altered system clock or pre-existing records already reclaimed before this
change. It is not sender authentication or a complete replay-prevention
guarantee.

The queue A/B test covers encrypted and TLS-only fixtures, a duplicate while
queued and after an accepted transfer and restart, a distinct transfer, and a
retry after cancellation. Two full Decodium processes with real E2EE and a
loopback HTTPS fixture also received both valid images and rejected an exact
replay of A or B under a fresh inbox ID before the fixture saw a replay content
request. The queue suite and these application A/B cases passed on macOS
ARM64. Equivalent cases are registered for Windows and Linux but have not run
there; the queue fixtures do not provide real encryption, whereas the
application tests use the macOS Keychain and real encrypted envelopes.
An additional A/B queue-store test sets each capacity to one in turn,
confirms that an unexpired encrypted acknowledgement survives attempted
reclamation and restart, and confirms that it becomes reclaimable at expiry.
That capacity-pressure test uses a fixture and a controlled clock; the
two-application replay tests do not force a full queue.

```sh
ctest --test-dir build -R '^test_sstv_share_app_replay_[ab]$' --output-on-failure
```

`SstvShareKeyStore` adds a worker-oriented identity and recipient-pin layer.
It keeps profile namespaces separate, refuses malformed stored identities,
rejects low-order keys and mismatched fingerprints, and reports a conflict
instead of silently replacing a pinned key. Identity and pin creation require
an atomic insert-if-absent backend operation, so concurrent creators cannot
silently replace the winner.

The keystore also rejects an internally inconsistent backend response that
reports an item missing while returning a nonempty value. Such a response is
treated as a storage failure, not permission to create an identity or pin.
The A/B component check injects that response for an existing identity and
an existing recipient pin, verifies that no insertion is attempted, and then
checks that both original values still load. The normal two-Decodium-process
identity, HTTPS image A/B transfer, ciphertext-tampering and manifest-tampering
tests passed again after this change on macOS ARM64. The inconsistent response
itself was injected into the component test, not a platform keychain.

`MacKeychainBackend` provides that operation using native macOS Security
framework calls. Its integration test creates a temporary keychain and checks
identity persistence and concurrent access from two processes. It also seals
two different envelopes with a pinned recipient key and opens each in a separate
process using the temporary keychain. This is a component harness, not two
Decodium application sessions or a provider upload. On macOS the controller
now configures the queue to open envelopes using an existing identity in the
native keychain and its own profile-derived recipient ID. It does not create
an identity automatically. A controller-level test receives two encrypted images through the in-process
local test provider using a temporary native keychain, checks their pixels, and
rejects an altered third payload. This is not a two-application or remote
provider test. The existing platform credential backend
passes secrets as command-line arguments and does not opt in to private-key
storage. QString, QByteArray and Core Foundation data may make copies that
cannot be guaranteed erased. Keychain access policies and permissions in a
signed production application still need end-to-end validation.

The download queue now has an optional worker-thread authenticated-open hook.
When configured, encrypted data is authenticated before the in-memory image
validator runs; failure or an absent hook rejects the transfer. The normalized
PNG handoff retains the original ciphertext digest and size for restart and
provenance. The queue test exercises upload and download of two distinct images with
real X25519/AES-GCM encryption, pinned test keys and a fake provider; it also
rejects ciphertext modified with a recomputed SHA-256. The test uses an
in-memory key backend and does not exercise a real provider or application UI.

The controller also has an explicit encrypted-upload method that requires a
pinned key; its in-process local-provider test uploads and decrypts two images
and rejects an unpinned recipient. On macOS the experimental UI exposes identity
creation, independent fingerprint confirmation, pinning and encrypted upload;
the upload controls require a compatible provider and an available secure
backend. A QML integration test drives identity and pin controls, uploads an encrypted
image and a TLS-only image through the in-process test provider, and checks
the downloaded pixels. The upload privacy message follows the selected mode:
it identifies provider-readable TLS-only transfers and, for encrypted uploads,
states that metadata remains visible and sender identity is not authenticated.
The QML test checks both messages while performing the corresponding uploads.
A recipient edit now clears both the fingerprint comparison confirmation and
the separate recipient confirmation. The upload button remains disabled until
the new recipient is explicitly confirmed, for encrypted and TLS-only modes.
If encryption was selected and subsequently becomes unavailable, the choice
stays selected, upload remains disabled, and the privacy message states that
the upload is blocked. The QML integration test exercises both recipient
changes, the availability transition, and successful encrypted and TLS-only
uploads. This UI check does not authenticate the sender or a remote provider.
A public provider remains unverified; the two-application loopback test is
described below.

Incoming rows now provide an explicit “Download only if encrypted” action.
The worker checks the persisted incoming manifest and rejects a TLS-only item
before queuing or contacting the provider. For an E2EE item, the existing
authenticated-open and image-validation path still has to succeed before the
item can be accepted. The ordinary Download action remains available for
TLS-only items. The choice applies to this download, not to the whole inbox:
an untrusted provider can still omit messages or present a different TLS-only
item, and sender identity is not authenticated. The manifest's
`downgradeProtected` field is not by itself proof that a recipient required
encryption; the explicit action and authenticated-open path provide that
local enforcement.

Queue A/B tests show that the same TLS-only image is rejected without a
provider download when encryption is required and is accepted through the
ordinary path. A second A/B checks that an E2EE item is rejected without a
successful authenticated-open and accepted when it succeeds. The existing
two-full-application HTTPS transfer and tampering tests now use the strict
download API for both images; they pass on the loopback fixture.
For encrypted uploads the sanitized image is encoded and encrypted in memory;
only the ciphertext is written to the outgoing staging file. Memory copies
cannot be guaranteed erased. Later sections describe subsequent key-storage,
upload/download and two-process integration. Recipient keys still require
independent authentication, downgrade prevention must cover the entire UI,
and the experimental envelope requires independent interoperability and
security review. Metadata visible to a provider must be considered separately
from content confidentiality.

`test_sstv_share_aead` tests identity and pin isolation with a fake backend,
backend failures, corruption, changed pins and low-order key rejection, plus
an independent AES-256-GCM known-answer decryption,
an RFC 7748 X25519 vector with independently computed HKDF output, matching
sender/recipient transfer keys, random-nonce A/B round trips and rejection of
modified ciphertext, nonce, tag,
associated data and wrong keys. These are component tests, not a security audit;
the separate full-application evidence is described below. OpenSSL is optional at build
time; absence leaves existing TLS-only sharing unchanged.

Expiry metadata binding is also checked A/B. Two component envelopes are
modified by moving `expiresUtc` five minutes earlier; the resulting manifests
remain valid, but authenticated opening rejects both. In two separate
full-application HTTPS tests, the isolated provider modifies that field in
image A or B after upload while keeping the ciphertext unchanged. The receiver
rejects the altered encrypted item with an authenticated-open failure; in B,
the unaltered A image is received and compared first. The clean two-image
transfer and existing `sstvMode` manifest-tampering A/B controls also pass.
These tests exercise two Decodium processes, a loopback fixture and local secure
storage; they do not authenticate the sender, test a public provider, or
constitute an independent cryptographic review. Reproduce with
`ctest --test-dir build -R '^test_sstv_share_app_expiry_tamper_[ab]$' --output-on-failure`.

Two additional A/B component and full-application controls substitute the
sender's ephemeral X25519 public key in the manifest with the X25519 base
point, leaving ciphertext and its public digest unchanged. The component
test verifies that each modified manifest is structurally valid and that the
substituted public key can participate in key derivation, then confirms that
authenticated opening fails. In the separate two-Decodium HTTPS tests, the
fixture alters exactly one target item; both are rejected on authenticated
opening and, for the B case, the unchanged A image is received and compared
first. The ordinary transfer, existing manifest-tamper A/B, expiry-tamper A/B
and AEAD tests passed alongside these checks on the local test setup. They do
not provide sender authentication or an independent security review. Run
`ctest --test-dir build -R '^test_sstv_share_app_ephemeral_tamper_[ab]$' --output-on-failure`.

A separate REST transport A/B test uses two independent provider instances and
a loopback HTTP fixture to upload, list and download two encrypted payloads;
it repeats both cases with separate sender and receiver test processes, passing
test keys over standard input rather than command-line arguments;
recipient decryption succeeds and altered ciphertext with a recomputed digest
is rejected. Plain HTTP is available only in the specially compiled test
target, and the production-gate test still rejects it. This does not establish
HTTPS service interoperability or two running Decodium sessions.

The production-linked provider now also has a local HTTPS A/B integration
test. A test-only CA and key in `tests/sstv/fixtures/localhost-test-only.*`
serve a loopback REST fixture; they must never be used as production
credentials. Only the test processes add that CA to their Qt trust settings.
The test verifies rejection of an untrusted certificate and a hostname
mismatch before any HTTP request reaches the server. It then uploads, lists,
downloads and decrypts two distinct encrypted payloads, including separate
sender and recipient processes. It tests the production transport library,
with the insecure-localhost option disabled. It does not verify a public HTTPS
service, the Decodium application UI, or a two-application exchange.

An application-level identity A/B test now launches two full Decodium test
processes with separate profiles and explicitly created temporary macOS
keychains. It verifies distinct X25519 public keys, checks each SHA-256
fingerprint, restarts both processes, and confirms each identity is stable.
The lab keychain and public-identity output options require `--test-mode`,
absolute paths and an existing keychain. Invalid invocations are rejected
before startup workers begin; tests cover both the production-mode rejection
and a missing keychain. Only public keys and fingerprints are written to the
lab output files. The test keychains are deleted by the harness.

Lab keychain access suppresses OS interaction prompts so an old keychain
whose access policy requires approval fails explicitly instead of hanging a
test. This was observed after rebuilding the unsigned test application:
the prior test keychain produced a prompt before this change and the processes
had to be stopped; after the change the same access returned an error and the
process exited with status 2. Normal application keychain interaction is not
changed. On macOS, run:

```sh
ctest --test-dir build -R '^test_sstv_share_app_identities$' --output-on-failure
```

This verifies application identity isolation
and persistence; the full-application transfer is tested separately below.

`tests/sstv/tools/share_https_fixture.py` provides a separate in-memory,
loopback-only HTTPS REST fixture for the next full-application test. It uses
the explicitly public test certificate and has no authentication, persistence
or production deployment role. `test_sstv_share_https_fixture` runs the
production-linked provider in separate sender and receiver processes for two
different payloads, rejects an untrusted TLS certificate, and rejects modified
ciphertext even when its digest and the manifest digest are recomputed. This
is provider-process evidence. The full-application transfer is tested separately below.


Two full Decodium application processes now exchange two different images
through this loopback HTTPS fixture. Each process uses an isolated profile and
a temporary native macOS keychain; the harness transfers only the receiver's
public identity and simulates independent fingerprint confirmation. The real
application controllers and queues upload, download, authenticate and decode
the images. The test compares dimensions and RGB pixels with the originals,
checks the recipient fingerprint, and verifies distinct nonces and ephemeral
key IDs. A separate two-application test modifies the ciphertext and
recomputes the unkeyed payload and manifest digests; the receiver rejects the
download with an authenticated-open error and produces no accepted image.
The error also covers a missing key or an unreadable encrypted staging file; it
does not identify the cause by itself. Run on macOS with Python, ffmpeg and
ffprobe available:

```sh
ctest --test-dir build -R 'test_sstv_share_app_(transfer|tamper)$' --output-on-failure
```

The fixture is an unauthenticated local test server, not a public service; it
does not establish provider interoperability or sender authentication. Its
test CA is trusted only by the lab processes. The tests do not establish
behavior in a signed production app, on other platforms, over RF, or under an
independent security audit.

The inbox offers a persistent setting requiring end-to-end encryption for new
downloads. When enabled, the worker rejects a TLS-only incoming item even if
the ordinary download action is used; it does not retroactively cancel items
already queued. Disabling it permits TLS-only downloads again. The setting is
stored in Decodium's settings and is not protected against local modification.
It does not authenticate the sender or stop a provider hiding items. Encrypted
payloads still require successful authenticated decryption before acceptance.
The QML test exercises the policy in both states and checks persistence; the
two-application transfer and tampering tests use the ordinary download action
with the policy enabled.

Two further full-application tests run the ordinary download action with the
policy enabled while the loopback HTTPS provider rewrites an encrypted
manifest as a syntactically valid TLS-only manifest and recomputes its public
manifest digest. The receiver rejects the downgraded A or B before the
provider receives a request for that item's content; in the B case it first
receives and checks the legitimate image A. Both passed on macOS ARM64 with
two Decodium processes. Equivalent tests are registered for Windows DPAPI and
Linux Secret Service but have not yet run there. These tests cover the local
policy and this fixture, not sender authentication, provider omission or
modification of local settings.

Two more two-application cases target ciphertext for only image A or only
image B. The HTTPS fixture alters exactly one selected payload and recomputes
its public SHA-256; for B it waits until the receiver has saved A, whose
pixels are checked. In both cases the modified payload is rejected by
authenticated decryption and no modified image is accepted. The clean A/B
transfer and both targeted cases passed on macOS ARM64. Equivalent tests are
registered for Windows DPAPI and Linux Secret Service but have not been run
on those platforms. These local fixture tests do not establish sender
authentication or an independent cryptographic security review.

Two additional full-application cases alter the `sstvMode` manifest field
at the HTTPS fixture, then recompute the public manifest digest while leaving
the encrypted payload unchanged. Decodium rejects the modified authenticated
data: case A changes the first image; case B first receives a valid image A,
checks its pixels, and then rejects the modified image B. The fixture keeps
the inbox receipt timestamp stable across refreshes and delays visibility of
B until A is received in the second case. These are controlled local tests,
not proof of sender identity or a production-provider security audit. Run:

```sh
ctest --test-dir build -R '^test_sstv_share_app_manifest_tamper_[ab]$' --output-on-failure
```

The inbound queue now rechecks expiry when a provider's download response
arrives and before handing a decoded image to the inbox. A deterministic
clock-controlled A/B test holds a provider response across the expiry boundary
for both a single-chunk and a multi-chunk image: neither response is written or
made available for acceptance, and both the transfer and inbox become expired.
The single-chunk case reached awaiting-acceptance before the fix. Two encrypted
payload cases also advance the clock during authenticated opening; the transfer
becomes expired, the validated image is removed, and acceptance is refused.
The queue suite and the separate clean two-Decodium HTTPS A/B exchange passed on
macOS ARM64 after this change.

An image that reached awaiting acceptance could previously still be handed
off and accepted after its expiry. Awaiting downloads are now expired before
direct acceptance, after validation and during queue processing or restart;
expired staged data is cleaned up by the existing managed-path cleanup.
Handoff queries also refuse an expired awaiting image. Images accepted before
expiry retain their existing local handoff. Clock-controlled queue tests cover
encrypted and TLS-only inputs, direct acceptance, queue processing, restart,
and the already-accepted control. The encrypted queue opener in these focused
tests is a fixture. Two further macOS ARM64 application A/B tests use real
E2EE and a loopback HTTPS provider: each reaches a validated image, advances
the receiver's laboratory clock, and checks expiry and staged-file removal;
in B the first image is checked before the second expires. The application
lab copies the validated handoff for comparison and does not click the GUI's
accept button; direct acceptance is covered by the queue test. Windows and
Linux variants are registered but have not been executed for this change.

Two more full-application tests use the real encrypted upload, HTTPS provider,
receiver queue and platform keychains. When the fixture receives the selected
content request, it advances the receiver's laboratory clock by two days before
returning the original ciphertext. In case A the first image expires; in case B
the first image is received and checked before the second expires. Both verify
the expired transfer and absence of the selected output. The clock override is
only enabled for the explicitly configured test-mode receiver, and preflight
checks reject a marker outside the isolated test directory or on the sender.
The A/B tests passed on macOS ARM64 and are registered for Windows DPAPI and
Linux Secret Service, where they have not been run. They simulate time rather
than waiting 24 hours; they do not constitute an independent security review.

```sh
ctest --test-dir build -R '^test_sstv_share_app_expiry_[ab]$' --output-on-failure
```

Expiry also removes an already downloaded managed `.partial/<transfer-id>.part`
file after the expired transfer and inbox state have been persisted. Removal is
restricted to the expected regular file inside the partial directory; a failed
removal is reported in the transfer error. Expiry is processed for idle downloads
even when their provider is unavailable or other transfers use all capacity.
The database update and file removal are separate operations. At startup the
queue revisits expired downloads and retries cleanup of the exact managed partial
and validated paths. Fault-injection A/B checks interrupt immediately after the
database commit, then restart without a provider: both partial files and, in
separate authenticated-open simulations, staged decoded PNGs are removed. A
neighbouring unrelated file survives, and repeating startup is idempotent. These
are queue-process simulations; failed removal is reported and is retried on a
later startup. A separate macOS ARM64 A/B application test performs each
two-Decodium encrypted transfer, recreates the first ciphertext block at the
expired transfer's managed temporary path to model the post-commit crash state,
and starts a new receiver process with the same profile and keychain. The new
process removes the residual block while preserving an unrelated neighbouring
file. This exercises actual application startup from a simulated residual state;
it does not forcibly terminate the application at the precise commit boundary.
Equivalent tests are registered for Windows and Linux but have not run there.

```sh
ctest --test-dir build -R '^test_sstv_share_app_expiry_restart_[ab]$' --output-on-failure
```
The queue A/B test exercises a prior block for both TLS-only and encrypted
manifests; a separate A/B restart test recovers a durable partial checkpoint and
expires it without registering the provider. Two full-application HTTPS tests
limit responses to 64 bytes, verify that the selected encrypted image's first
chunk was received, advance the receiver clock on the second request and check
that the temporary file is gone. In B, image A is first received and checked.
These passed on macOS ARM64; equivalent Windows and Linux cases are registered
but unverified there. They do not test physical RF or a public provider.

```sh
ctest --test-dir build -R '^test_sstv_share_app_expiry_partial_[ab]$' --output-on-failure
```

On 2026-11-02 the queue test and both `test_sstv_share_app_expiry_partial_[ab]`
application cases were repeated successfully on the local macOS ARM64 build
based on 1.0.734, with the current uncommitted SSTV changes. The executable
was rebuilt before the application checks. This repeats the specific partial
expiry scenario described above; it is not evidence of the complete E2EE goal,
a public-provider deployment, or independent interoperability.

After an accepted download is persisted, its raw managed `.partial` file is
now removed through an exact-name, regular-file and canonical-directory check.
Startup retries that cleanup for accepted, acknowledging and acknowledged
downloads, preserving unrelated files and the validated image. If removal
fails, an otherwise empty transfer diagnostic reports it; later startup
retries it. Queue A/B tests simulate a residual after acceptance for TLS-only
and encrypted envelopes, verify two restarts, preserve a neighbouring file
and validated PNG, and test a directory at the partial-file name followed by
successful retry. The queue's encrypted opener in this focused test is a
fixture. The accepted-state restart test failed before this change.

The application A/B test exposed a separate pre-existing restart issue: a
successful Gallery import removes the validated staging PNG, so the queue
previously marked the accepted transfer `Failed` on restart. It now permits
that missing staging PNG only when an owner-provided Gallery verification
finds the same transfer UUID, remote provider and object, category and image
dimensions in the local Gallery database and verifies the bounded PNG path,
size and SHA-256. Missing, inaccessible or mismatched Gallery evidence fails
closed. A present but damaged staged PNG cannot use the Gallery fallback.
Two full Decodium sessions exchanged real encrypted images over the loopback
HTTPS provider; for A and B, the receiver accepted the selected image, waited
for its Gallery file, recreated a 64-byte ciphertext residual to simulate an
interruption after the acceptance commit, and restarted with the same profile
and keychain. Both preserved the Gallery bytes and accepted state and removed
only the managed residual. Two further application cases altered the Gallery
PNG before restarting and verified that the transfer became `Failed`; the
queue test also rejects a corrupt present staging PNG without consulting the
Gallery fallback. The simulated residue does not forcibly terminate the app
at the precise commit boundary. These local macOS ARM64 tests do not constitute
a production-provider deployment or an independent security audit.

```sh
ctest --test-dir build -R '^test_sstv_share_queue_manager$|^test_sstv_share_app_accepted_restart_[ab]$|^test_sstv_share_app_gallery_corrupt_[ab]$' --output-on-failure
```

Gallery recovery now distinguishes verified data, a definitive mismatch and
an unavailable check. An unavailable database query or transient read error
leaves the transfer in accepted-but-unverified recovery, records a pending
diagnostic, and blocks acceptance and acknowledgement until verification.
While sharing is enabled, the queue retries after five seconds, at most two
checks per pump; a verified result clears the pending diagnostic and retries
managed partial cleanup, while a definitive mismatch transitions to integrity
failure. The pending condition is re-established on restart rather than trusted
as proof. A/B queue tests simulate unavailable-to-valid and unavailable-to-
invalid outcomes, verify the retry delay and action blocking, and retain the
corrupt-present-staging negative control.

Two further macOS ARM64 application A/B tests run real encrypted transfers,
accept one image, recreate an isolated partial residue, and restart the
receiver with a test-mode, denial-only injection that makes the Gallery check
unavailable. They verify the pending state, then restart without injection and
verify the accepted state, cleared diagnostic, and unchanged Gallery image.
The unit tests cover retry within one running queue. An attempt to hold an
exclusive SQLite lock instead blocked the earlier Gallery initialization and
never reached this check; the application cases therefore simulate the
check's unavailability, not a real lock at that exact point. A permanently
unavailable or incompatible Gallery database can remain pending until it is
made readable. These checks do not prove external-provider security.

```sh
ctest --test-dir build -R '^test_sstv_share_queue_manager$|^test_sstv_share_app_gallery_unavailable_[ab]$' --output-on-failure
```

During pending incoming-image recovery, an inbox lookup failure was previously
indistinguishable from a missing inbox item and could permanently mark an
accepted image as an integrity failure. Inbox query errors now defer verification
for five seconds; a genuinely absent item or mismatched image remains a
failure. The same distinction applies during initialization: a query error
leaves the transfer pending and blocks acceptance and acknowledgement until
verification can run. SQL stepping errors are also surfaced rather than being
interpreted as an absent row.

The queue regression temporarily renamed the inbox table in an isolated SQLite
database while the Gallery check was pending, for TLS-only and encrypted
fixtures. Before the change the accepted state became Failed; afterwards both
remained accepted and blocked, then recovered after the table was restored and
the verification succeeded. This is a controlled database-query failure, not
a simulation of every SQLite I/O failure. The full queue CTest passed.

Two full Decodium application tests transferred real encrypted A/B images over
verified loopback HTTPS using the macOS Keychain. After acceptance, each receiver
restarted with a test-mode-only, denial-only inbox lookup fault; its accepted
state remained pending. A further restart without the fault verified the Gallery
image and cleared the pending diagnostic. The application fault is injected,
not a real SQLite lock; same-process retry after a real failed query is covered
by the queue test above. Run `ctest --test-dir build --output-on-failure -R
'^test_sstv_share_queue_manager$|^test_sstv_share_app_inbox_unavailable_[ab]$'`.
These local checks do not prove public-provider security or complete E2EE
interoperability.

A further A/B queue regression covered an unaccepted download whose deferred
inbox verification is scheduled five seconds later but whose manifest expires
one second after recovery. Previously the pending check prevented expiry at
the deadline. The queue now attempts the atomic expiry transaction before the
pending-verification delay, while retaining the pending state if that
transaction cannot be committed. The fixture checks TLS-only and encrypted
manifests; its original encrypted opener was a fixture. Additional controls fail the
inbox lookup or the database commit at expiry, confirm that the validated file
is preserved and acceptance remains blocked, then verify that the next pump
persists expiry and removes only the managed temporary files. The initially
failing deadline test and the complete queue suite pass after the fix.

Four separate full-application macOS ARM64 tests also passed: encrypted A/B
expiry while awaiting acceptance and encrypted A/B recovery from an injected
inbox lookup failure. They exchange images between two Decodium processes over
verified loopback HTTPS. These app cases exercise the two conditions separately;
their exact combination and injected commit failure were covered by the queue
fixture, not by a full application test.

The queue regression now repeats every expiry scenario for two distinct
images. When the cryptographic backend is enabled, the encrypted cases use
real X25519 recipient encryption and AES-GCM opening instead of the fake
opener; each case also verifies rejection of a modified ciphertext. The
negative control also recomputes the public ciphertext SHA-256 after the
modification, so rejection is checked beyond the unauthenticated digest.
The queue and AEAD tests passed locally with `DECODIUM_TEST_SHARE_CRYPTO=1`.
Builds without that optional backend retain the simulated opener. This
strengthens the queue-level combined failure test but does not turn it into
a full-application test of that exact combination.

The queue expiry cases additionally compare the decrypted staged image
byte-for-byte with each A/B input and keep a distinct sentinel file in the
same directory. They verify that the staged bytes and sentinel survive a
failed expiry transaction and that the sentinel survives successful expiry
and cleanup. All queue cases passed locally with real cryptography enabled.

The two separate full-application expiry-awaiting and inbox-unavailable
scenarios have now also been run with signed encrypted envelopes for both A/B
images. All four new tests passed using two Decodium processes, isolated
Keychains and a loopback HTTPS provider; the four existing unsigned encrypted
controls also passed. As before, the expiry and injected lookup failure are
separate application cases rather than one combined case. The macOS CTest
inventory gate now requires these four signed cases as well, bringing its
required signed A/B inventory to 18; its positive and negative controls and
the local CTest inventory check passed. No remote CI run is implied.

A later attempt to add the combined pending-lookup/expiry full-process test
did not yield a valid result. The initial A/B attempts did not observe the
expected pending state; subsequent attempts stalled before publishing the
laboratory identities. The previously passing signed expiry-awaiting A
control also timed out before identity publication after the experimental
changes were removed and the application rebuilt. The cause is not yet
established. Diagnostic directories include
`.../decodium-share-transfer-_frjni72`, `...-szlrek52`, `...-5ef4unnt`,
`...-xjttzefo` and control `...-7799njfb`. The unverified combined-test
prototype was removed; the timeout means the earlier passing application
results have not been reproduced in the current environment.

Further isolation found the test-mode receiver ready, enabled and configured
with secure storage available, but without the discovered provider inbox
capability. The standalone local HTTPS fixture test also timed out. A separate
minimal Python socket test, with no Decodium or HTTPS involved, timed out
connecting to both `127.0.0.1` and `::1`; a Unix socket pair succeeded.
`lo0` was up with both addresses and the IPv4 route pointed to `lo0`.
This establishes a local TCP-loopback failure at that time, but not its
cause. The macOS Keychain, AEAD and sharing QML tests passed separately.
A test-mode-only wait diagnostic was added to the application lab; the
network or system firewall configuration was not modified.

On recheck, IPv4 and IPv6 loopback TCP and ICMP still timed out. ExpressVPN
reported disconnected; an enabled LuLu network extension reported its
localhost-allow preference enabled. These observations do not establish the
cause, and packet-filter status was not readable without elevated access.
The sharing application harness now performs a verified, bounded HTTPS
capability preflight before launching Decodium. A/B offline mock controls
cover success, malformed and oversized responses, missing E2EE capability,
HTTP errors and timeout; they passed. The real control now fails at that
preflight in about five seconds with an explicit timeout rather than waiting
for both applications for 105 seconds. Successful real preflight and the
full-process tests remain to be rerun when loopback is available.

On a later local recheck, TCP loopback responded again without a network
configuration change made by this test. The standalone HTTPS fixture, the
verified HTTPS preflight, and all four signed expiry-awaiting / inbox-
unavailable A/B application cases then passed again (six CTests total).
The cause and duration of the earlier loopback outage remain unknown. These
application cases still exercise expiry and the injected lookup failure
separately; the combined full-process scenario remains unverified.
Both corrupt-recipient-pin and corrupt-sender-pin full-process A/B controls
also passed after connectivity returned, with the new HTTPS preflight active.

The combined pending-lookup/expiry scenario is now covered by signed A/B
full-process tests on macOS. Each test transfers encrypted images through
the isolated HTTPS fixture, stops the receiver while the target image awaits
acceptance, and restarts it with a test-mode-only lookup denial. The harness
reads the isolated queue to verify the pending diagnostic and unchanged staged
bytes before advancing the laboratory clock; the denial is then released.
Both targets reached `Expired`, their managed staged files were removed and a
separate sentinel in the same directory remained unchanged. For target B the
earlier A image is also checked. The denial is deliberately simulated and
the clock advance is test-only; this is not a real disk failure or public
provider test. Both combined tests and nine related queue, crypto, QML,
inventory and separate-case regressions passed. The macOS inventory gate now
requires 20 signed application A/B tests. An earlier attempt failed because
the lab read a stale inbox view after expiry; the lab now waits for the
history state instead. The temporary local TCP outage described above was
independent and had recovered before these successful runs.

The expiry-hold lab guard now also requires the receiver Keychain, identity
and expiry marker to be in the same canonical temporary directory with the
sharing laboratory prefix. For both A/B cases, preflight controls reject a
launch outside test mode, the sender role, missing or outside markers, the
sender Keychain and a same-named Keychain in another directory. The combined
tests passed again after the guard was tightened. These restrictions apply to
the laboratory fault injection, not to ordinary user Keychains.

### Sender-authentication design gate

The existing encrypted envelope remains anonymous with respect to the sender.
An initial Ed25519/OpenSSL signing prototype was removed after a negative test:
using the encoded identity point as the public key and `R = identity, S = 0`
as a signature, OpenSSL's generic verification accepted two distinct messages.
`EVP_PKEY_public_check` also returned success. The same diagnostic accepted
an RFC 8032 valid vector and rejected an altered message under its valid key.
This was observed on macOS ARM64 with the locally linked OpenSSL 3.6.4 and
with the locally built OpenSSL 3.5.8 used by the macOS release scripts.
It is a property of these tested generic Ed25519 APIs, not a break of the
existing X25519/AES-GCM encryption path, which does not use signatures.

`test_sstv_ed25519_backend_diagnostic` records the actual backend behavior;
a passing CTest means the diagnostic executed, not that an accepted weak key
is safe. The two-Decodium encrypted transfer and both ciphertext-tamper A/B
application tests still pass after removing the prototype. No sender-signature
feature or sender-verification claim is enabled by this diagnostic.

A future authenticated format must specify separately pinned signing keys,
bind the claimed sender and full encrypted envelope to the signature, reject
weak/noncanonical or wrong-subgroup public keys using an audited implementation,
and define explicit version and downgrade policy. In particular, a verifier
must not trust a public key supplied only by the provider. The
[libsodium point-validation documentation](https://doc.libsodium.org/advanced/point-arithmetic)
describes canonical, small-order and subgroup checks, as well as version-specific
caveats; adding such a dependency would also require a portable build and
packaging strategy. These requirements and the eventual two-application attack
and compatibility tests remain open; they are not satisfied by the primitive
probe or by existing anonymous E2EE transfers.

As a possible alternative to the weak-key Ed25519 behavior described above,
`test_sstv_p256_backend_diagnostic` evaluates ECDSA P-256/SHA-256 in an
isolated test executable. The experiment admits only exact uncompressed
P-256 SubjectPublicKeyInfo encoding, invokes OpenSSL's full
`EVP_PKEY_public_check`, and verifies signatures with EVP. Two independently
generated key pairs signed and verified distinct messages; changed messages,
signatures, wrong keys, an all-zero off-curve point, altered point encoding,
a P-384 key and trailing DER were rejected. Two SHA-256 signatures from
[RFC 6979 section A.2.5](https://www.rfc-editor.org/rfc/rfc6979#appendix-A.2.5)
verified against the published public key; cross-message verification failed.
The diagnostic passed against locally linked OpenSSL 3.6.4 and the separately
built OpenSSL 3.5.8 ARM64 release library. OpenSSL documents its EC full
public-key validation in its [EVP_PKEY-EC documentation](https://docs.openssl.org/3.0/man7/EVP_PKEY-EC/).

This is a feasibility diagnostic, not a production implementation or a
security audit. It does not define signature serialization or canonicality,
sender-key provisioning and pinning, envelope binding, version negotiation,
key rotation, anti-downgrade policy, or replay handling. At that diagnostic
stage no signing key was persisted and no sender signature was exchanged.
The later test-mode-only integration and its remaining limitations are
described below.

The candidate P-256 primitive and signing-key store are available to the
native sharing library and the test-mode laboratory, but are not connected
to the operator-facing UI or normal outgoing envelopes. They generate a separate P-256 signing identity, export only
canonical uncompressed P-256 SubjectPublicKeyInfo, validate its curve point
with OpenSSL full public-key checking, and bound keys, messages and signatures.
The key store keeps the private DER in the existing private secure backend,
uses a distinct account from X25519, and stores immutable sender pins scoped
by profile, provider and sender. It requires a supplied fingerprint matching
the validated key; the caller is responsible for independently confirming it.
A conflicting or corrupt stored value is never silently replaced.

A/B library tests cover separate identities, RFC 6979 signatures, private-key
reload, pin persistence and provider isolation, as well as wrong signatures,
keys, fingerprints, invalid points, trailing bytes and secure-backend errors.
They passed on macOS ARM64 using both the development OpenSSL and the locally
built release OpenSSL 3.5.8. The existing two-Decodium HTTPS transfer and A/B
ciphertext-tampering cases also passed with the rebuilt application, but those
application cases used the old unsigned envelope and did not exercise
P-256 signing. Later laboratory-only signed-envelope checks are described
below. Operator-facing key exchange and trust policy were added afterwards;
sender authentication still depends on independent fingerprint verification.

### Experimental signed envelopes and sender policy

After the test-mode `--lab-no-monitor` startup-timer correction, both the
unsigned-encrypted and signed-encrypted full-application transfer CTests were
rerun locally. Each uses two Decodium processes, two distinct A/B images,
isolated Keychains and the verified loopback HTTPS fixture; both passed. This
is a regression check for that lab startup change, not public-provider,
cross-platform or independent-security-audit evidence.

The sharing page can create and display a signing identity, sign an encrypted
upload explicitly, and pin a sender signing key after the operator confirms its
fingerprint through a separate trusted channel. The opt-in incoming sender
policy is persistent: newly queued downloads must be encrypted and signed;
pending downloads are checked again before staging and before acceptance
against the current policy. Already accepted downloads are not retroactively
removed.
Without the policy, unsigned encrypted downloads remain available; signed
ones always require the sender pin and a valid signature. Disabling the policy
explicitly permits unsigned downloads again. Signed uploads fail if a signing
identity or verified recipient key is absent. Existing recipients with a strict
older manifest parser reject the additional fields; compatibility has not been
negotiated.

The experimental format includes the signing algorithm, pinned public-key
fingerprint and P-256 signature in the encryption object. The signature is
computed over a domain-separated canonical manifest with the signature field
omitted; this binds sender, recipient, provider, nonce and ciphertext SHA-256
along with the other manifest fields. AES-GCM associated data binds the
signing algorithm and fingerprint while omitting the signature, which is not
available until after sealing. Opening a signed item requires a signing key
already pinned to that provider and claimed sender; the fingerprint and
signature are checked before decryption. A strict caller can also reject all
unsigned envelopes. DER signatures are required to be canonical and low-S,
preventing a third party from changing the manifest idempotency key through
the usual ECDSA high-S/low-S transformation. P-256 points are checked with
OpenSSL full public-key validation. These properties do not substitute for
independent key verification or an independent protocol security review.

Two full Decodium sessions in test mode exchanged two encrypted and signed
images through a verified loopback HTTPS provider. The sender published its
signing key to an isolated laboratory file, and the receiver pinned that key
before accepting either message; this simulates, rather than proves, an
independent trusted key exchange. A/B tests rejected each of the following:
a separately generated signing key claiming the pinned sender identity,
changed ciphertext with recomputed public hash, changed signature, changed
sender ID, and removal of all signing fields. The B cases checked that the
preceding unchanged A image was received. The harness verifies the uploaded
manifests identify the expected signing fingerprint. Eleven signed
application checks, the unsigned application regression and the crypto suite
passed on macOS ARM64. The crypto suite additionally covers reverse-direction
A/B signing, strict rejection of unsigned messages, malformed signing fields,
RFC vectors, low-S normalization and high-S rejection. The same crypto suite
passed with the locally built OpenSSL 3.5.8 library and under ASan/UBSan on this
Mac (leak detection disabled). Queue, HTTP provider and unsigned manifest
attack regressions also passed. Other platforms, a public provider, real
out-of-band identity confirmation, key rotation, broader replay cases and an
independent security audit remain unverified.

```sh
ctest --test-dir build --output-on-failure \\
  -R '^test_sstv_share_aead$|^test_sstv_share_app_signed_|^test_sstv_share_app_transfer$'
```

The application laboratory now uses the same explicit signed-upload API and
persistent policy as the operator interface. On macOS ARM64 the QML/controller
test verifies signing-key display and pinning, rejection of both plaintext and
unsigned encrypted inbox items under strict policy, and persistence across
controller restart. In two-session A/B tests, removing all signing fields is
rejected before requesting the payload; the HTTPS fixture checks that no
content request occurred. The trusted-channel confirmation in the laboratory
is simulated by a local file, not an independent human verification. These
checks do not establish compatibility with older clients, public providers or
other operating systems, and the protocol remains experimental and unaudited.

The page also displays the exact local sender ID to share with a verified
signing fingerprint. Strict sender policy is rechecked at staging and before
acceptance, including plaintext and unsigned encrypted items queued before the
policy was enabled. Already accepted items are not retroactively revoked.

On macOS ARM64, A/B queue tests changed the policy after queuing and before
staging for both TLS-only and unsigned encrypted images; neither acquired a
validated handoff. They also enabled the policy after each was awaiting
acceptance and verified rejection, then disabled it explicitly and confirmed
acceptance. Two full Decodium sessions repeated the awaiting-acceptance case
with unsigned encrypted image A and image B via loopback HTTPS. Each rejected
the pending acceptance under strict policy and completed the transfer after
explicitly disabling it. The normal unsigned and signed application transfers
and the QML/controller test also passed. These tests use simulated trusted
key exchange and are not independent security review or RF evidence.

The signing control and signing identity are in the encrypted-upload section,
independent of whether provider configuration is expanded. The inbox also
exposes the local recipient ID, identity creation, public key and fingerprint
without enabling outgoing encryption. On macOS ARM64, the QML/controller test
checks effective control visibility with provider configuration collapsed and
creates the recipient identity from the inbox. The signed and unsigned
two-process transfers and both sender-authentication tampering cases passed
after this interface change. The transfer harness exercises controller APIs;
it does not establish a manual visual workflow or independent key exchange.

The transfer privacy summary does not assert sender authentication solely
because a manifest claims a signature: it states that a verified signature
is required. The QML test checks both signed-claim and unsigned-claim inputs
and the TLS/provider-readable alternative. Signed and unsigned two-process
transfer regressions passed on macOS ARM64 after this presentation change;
those transfer tests do not exercise the displayed list text.

Transfer-list privacy labels now describe the manifest's audience as
"declared public" or "declared recipient-only", with an unknown fallback,
instead of always saying recipient-only. End-to-end encryption is also
identified as a declaration in that summary; the row does not assert that an
incoming manifest has already been authenticated. QML A/B and missing-field
checks cover these labels. The signed and unsigned two-process transfer tests
passed on macOS ARM64 after the change; they do not independently exercise
public-sharing enforcement or visual inspection of the transfer rows.

The QML/controller fixture was subsequently attached to a shown offscreen test
window, queued and listed a real local-provider encrypted red image and a
TLS-only blue image, and inspected the instantiated inbox delegate labels via
the visual tree. Both displayed the declared recipient audience; the former
displayed the declared encryption warning and the latter identified that the
provider can read content. The QML test and separate signed and unsigned
two-process HTTPS transfer regressions passed on macOS ARM64. This checks
rendered QML object text, not manual visual inspection, a public provider or
public-sharing enforcement.

Two additional macOS ARM64 application tests combine sender signatures with
the exact-envelope replay scenario. In each, two Decodium processes first
transferred and decrypted both images through the verified loopback HTTPS
fixture; the receiver pinned the sender key through the laboratory's simulated
trusted channel. The provider then presented the unchanged signed manifest and
ciphertext again under a fresh provider inbox ID, for image A or B. Both runs
verified that the receiver rejected the duplicate before requesting its
content. The fixture separately checks the original signature algorithm,
fingerprint and sender identity, as well as exact equality of the replayed
manifest and ciphertext. `test_sstv_share_app_signed_replay_a` and `_b` cover
these cases. This checks exact replays while the queue retains its evidence;
it is not a general proof against every replay, a public-provider test, an
independent key-verification ceremony or a security audit.

A further signed-envelope A/B application test now restarts the receiver
before replay. Two isolated Decodium processes first transfer both images over
the verified loopback HTTPS fixture; after both exit, the same receiver
settings, identity and provider are reopened, and the fixture presents the
unchanged signed manifest and ciphertext under a fresh inbox ID. For both A
and B, the restarted receiver rejects the duplicate without requesting its
content. The fixture checks equality of the manifest and ciphertext, and the
application must report the duplicate and complete its resumed session.
`test_sstv_share_app_signed_replay_restart_a` and `_b` passed on macOS ARM64;
the existing signed and unsigned transfer and replay application tests also
passed. The first laboratory attempt tried to reconfigure the provider with
active pending transfers and was correctly rejected; the revised test-only
session reuses the existing provider instead. The tests use a simulated local
trusted-key exchange and retained queue history: they do not establish
behaviour after history expiry, a public provider, RF, other platforms or an
independent security review.

The restart replay check was extended to accepted signed images. In each A/B
case, two Decodium processes transfer both images over the loopback HTTPS
fixture, and the receiver accepts the selected image. The harness checks the
persistent queue state is `Accepted` and that exactly one corresponding
Gallery image exists before restarting the receiver with the same settings and
identity. The fixture then re-presents the unchanged signed envelope and
ciphertext under a new inbox ID. Both cases reject it before requesting the
content; the other pending image remains available. The two new
`test_sstv_share_app_signed_replay_accepted_restart_{a,b}` cases passed on
macOS ARM64, as did the existing accepted-restart and signed replay tests.
The key exchange was simulated, the queue history was retained, and this does
not establish behaviour after history expiry, on a public provider or after
independent security review.

Two further full-application A/B tests exercise ECDSA signature malleability
over the loopback HTTPS fixture. The sender publishes a valid signed encrypted
image; the fixture changes only its canonical DER signature from low S to
`order - S` and updates the manifest representation and advertised digest.
This signature has a different manifest identity but is mathematically
equivalent under ECDSA. The receiver rejects the altered envelope during
authenticated opening; in the B case it also receives the preceding unchanged
A image. `test_sstv_share_app_signed_senderSignatureHighS_{a,b}` passed on
macOS ARM64, as did the signed-transfer control, HTTPS fixture and crypto
tests. The fixture conversion was checked against two known P-256 vectors;
OpenSSL independently verified both low- and high-S forms for each vector.
This is an authentication rejection test with a simulated trusted key
exchange, not an accepted-image replay test, public-provider interoperability
or independent security audit.

The high-S case was subsequently tested as an actual replay after acceptance
and receiver restart. For A and B, two Decodium processes first transferred
both signed images; the receiver accepted the selected image, and the harness
verified its persistent `Accepted` state and Gallery file. After restarting
the same receiver identity and settings, the fixture presented the same
ciphertext and manifest fields under a fresh provider inbox ID, changing only
the signature to its equivalent high-S representation. The altered manifest
has a different identity, so content was requested; authenticated opening
then failed. The harness confirmed that the original transfer stayed accepted
and the complete imported-PNG inventory and hashes were unchanged. Both
`test_sstv_share_app_signed_replay_high_s_{a,b}` passed on macOS ARM64, as
did the exact-replay-after-acceptance A/B and valid signed-transfer controls.

The first two attempts instead exposed a laboratory scheduling race: a new
inbox poll was started while the fixture's provider slot was occupied, and
the session stopped on the temporary busy response before testing the altered
envelope. The test-only session now retries that specific response within its
existing 90-second deadline; both successful A/B tests reached the required
authenticated-open failure. This is not proof against every replay strategy,
an independent key-verification ceremony, a public provider or a security
audit.

Inbox refresh now reports a missing or unsupported provider separately from
a provider whose operation slots are busy. Both retain the existing
`ProviderUnavailable` failure category; this change makes the diagnostic
distinct and does not add automatic retry to the production UI. Queue A/B
tests cover an existing inbox refresh and an active upload occupying the
single provider slot, check that an unknown provider reports unavailable in
both cases, and confirm that another refresh succeeds once the slot is free.
The test-only sharing session retries only the explicit busy diagnostic,
within its existing deadline, rather than also retrying missing providers.

For full-application A/B controls, the loopback fixture delayed each altered
high-S replay content response for 3.5 seconds, forcing a concurrent inbox
poll while the download slot was occupied. Both resumed laboratory sessions
reported the busy retry and subsequently rejected the altered envelope during
authenticated opening, while retaining the accepted Gallery image. The
signed transfer and exact-replay-after-acceptance A/B regressions also passed
on macOS ARM64. The retry belongs to the isolated laboratory session; this
does not establish public-provider behaviour or an independent security audit.

The Windows SSTV workflow's inventory gate previously required an exact
count of six sharing tests, although the current CMake declarations contain
20. It now checks the CTest JSON inventory for all 20 named requirements and
allows additional tests; missing or malformed inventories fail the gate.
Local A/B controls verified the declared names against the CMake block,
accepted extra tests and rejected missing A/B replay cases and malformed
input. The sharing queue test and two full-application high-S replay A/B
regressions also passed on macOS. No GitHub runner was started or monitored
for this change, and it is not evidence of a Windows build or runtime pass.

### Inbox refresh while a provider slot is occupied

The application worker now defers an inbox refresh when the queue manager
reports its specific local `inbox provider is busy` condition. It coalesces
pending retries, waits 250 ms between attempts and stops after 40 retries;
an unavailable provider and other failures are still reported immediately.
Disabling sharing or rebuilding the provider invalidates scheduled retries
using the provider generation and a retry token. This changes refresh
scheduling only; it does not weaken encryption, signature or acceptance
checks. The earlier statement that retries were laboratory-only describes
the implementation before this change.

Two-process signed A/B HTTPS fixture tests held the download slot for 3.5
seconds while polling the inbox. Both exercised the worker retry, decrypted
the expected A/B images and completed without the laboratory poller's own
busy-error retry. Further A/B tests held the slot for 12 seconds: the
worker's bounded attempts ended with the busy result, the laboratory poller
resumed later, and both transfers still completed. This longer test proves
the bounded worker result; its eventual additional poll belongs to the test
session, not an unbounded production retry. Controller tests also exercised
two concurrent refreshes and disabling/re-enabling sharing while a retry
was pending; manager tests retained the distinction between busy and missing
providers. The sharing QML, signed transfer and both signed high-S replay
regressions passed on this Mac. These are local HTTPS tests with simulated
key exchange, not a public-provider trial or an independent security audit.

The queue manager now marks its own capacity-busy result explicitly; the
worker no longer identifies it by comparing a provider diagnostic string.
The public failure category remains `ProviderUnavailable` for compatibility.
A/B manager checks confirm the local marker is present for occupied inbox
and upload slots, absent for a missing provider, and absent when a generic
provider failure merely repeats the same diagnostic text. Both signed
two-process busy-refresh and bounded-exhaustion A/B tests, plus the controller
and queue tests, passed after this change. The marker is internal process
state, not a new remote protocol field or proof about arbitrary providers.

Both Windows workflows that compile the pinned Hamlib 4.7.0 archive now
verify SHA-256 and apply `tests/sstv/fixtures/hamlib-4.7.0-windows-assert.patch`.
The original Windows timer branch calls `assert` without including
`<assert.h>`; the patch adds that standard header only to the Windows branch.
`test_hamlib_windows_assert_patch.py` applied the patch to the official
archive's extracted source and compiled the isolated branch against a minimal
Windows API stub: with assertions enabled and with `NDEBUG`, an undeclared
`assert` failed before the patch and the patched branch compiled. The test
also rejects a second patch application. The local inventory test and two
full Decodium signed-replay A/B regressions passed on macOS. This is a
targeted compiler simulation, not a Windows Hamlib build, application runtime
or evidence that all CI failures are resolved. No runner was started or
monitored.

Two additional full-application signed A/B tests change the `sstvMode`
field in an otherwise signed encrypted manifest at the local HTTPS fixture.
Both receivers rejected sender authentication before requesting the altered
content; in the B case, the preceding unmodified A image was also received.
The normal signed transfer and accepted-image replay after restart A/B
controls passed in the same local run. This tests that particular signed
metadata alteration with two local processes, not an external provider or
an independent security audit.

The same signed-manifest application checks were extended to `expiresUtc`
(changed by five minutes while retaining a valid timestamp) and the X25519
recipient `keyId` (replaced with a validly encoded different key ID). Both
alterations were rejected before content download for image A and image B;
the B controls also received the preceding unchanged A image. These tests
exercise the local HTTPS fixture and do not establish provider interoperability
or independently verify identities.

Two further signed application tests alter `recipientId` to a different
valid identifier at the local provider, retaining the original signature.
For both A and B, the receiver rejects the altered manifest before requesting
its content; the B control first receives the intact A image. The local HTTPS
fixture and ordinary signed transfer controls also pass. This tests rejection
by the intended receiver, not delivery to an independently operated recipient.

Two-process A/B controls now cover enabling the sender-authentication policy
after a valid signed image has already been staged for acceptance. Both
images are accepted and compared with their originals under the newly enabled
policy. Complementary unsigned A/B controls confirm acceptance is rejected
while the policy is enabled and succeeds only after it is disabled again.
These tests use an isolated HTTPS provider and simulated identity pinning.

After adding the policy-change scenario, a combined 12-test local regression
also passed: the AEAD suite, signed and unsigned pending-policy A/B controls,
the signed transfer, recipient and SSTV-mode tampering A/B, and signed replay
after acceptance and receiver restart A/B. The application cases use the
same local fixture and isolated identities described above.

The following existing full-application tests were rerun on macOS ARM64:
`test_sstv_share_app_signed_invalid_then_valid_{a,b}`,
`test_sstv_share_app_signed_nonce_restart_{a,b}`,
`test_sstv_share_app_signed_legacy_invalid_{a,b}`,
`test_sstv_share_app_signed_replay_resigned_{a,b}`, and
`test_sstv_share_app_signed_replay_index_upgrade_{a,b}`. All ten passed.
The first cases verify that a manifest with an altered signature or nonce is
rejected before content download, and that the original valid transfer can
subsequently complete; the nonce cases restart the receiver with its saved
identity. The legacy cases insert a paused invalid record before the valid
transfer. The replay cases re-sign an accepted envelope, verify the new
signature independently with OpenSSL, restart the receiver and check that
the duplicate is rejected without changing the accepted Gallery files; the
index-upgrade variant removes the test database replay indexes before the
restart and checks that they are recreated. These are isolated two-process
loopback HTTPS tests with simulated identity pinning, not public-provider or
RF interoperability, cross-platform runtime evidence, or a security audit.

The macOS native SSTV workflow now explicitly installs ffmpeg (which also
provides ffprobe) and checks CTest's JSON inventory for the ten signed
application A/B cases above, including their `sstv`, `sharing`, `security`
and `application` labels. This prevents the workflow's `sstv` label run
from silently omitting those cases when optional test dependencies are
absent. Local A/B positive controls accepted complete inventories, while
missing tests, missing labels, malformed entries and duplicate names were
rejected. The check also accepted the locally configured CTest inventory.
No GitHub runner was started or observed for this change.

After the inventory change, the local macOS build's `test_sstv_share_aead`,
`test_sstv_macos_keychain`, `test_sstv_share_qml` and
`test_sstv_share_app_signed_transfer` were rerun and passed. The Keychain
test exercises corrupt recipient pins in a separate temporary keychain; the
QML/controller test checks both corrupt recipient-pin notices and a corrupt
sender-pin notice, while the application transfer uses valid isolated pins
to exchange two signed images. Together these checks do not constitute a
full-application test with a corrupt OS Keychain record, a public-provider
test, independent identity verification or an independent security audit.

The missing full-process corrupt-recipient-pin scenario is now covered on
macOS by `test_sstv_share_app_signed_corrupt_recipient_pin_{a,b}`. Each test
starts two Decodium test-mode processes with separate temporary keychains
and a loopback HTTPS fixture. The sender laboratory inserts one of two
distinct low-order recipient keys into its isolated keychain immediately
before pinning. The actual sharing controller reports the stored key as
corrupt; the lab verifies that the bad record was not overwritten, removes
only that test record, and retries the normal verified pin. Both processes
then transfer and decrypt the A/B images. Preflight controls reject this
laboratory operation outside test mode and reject a marker path outside the
isolated root. The A/B cases, signed-transfer control, AEAD, QML sharing and
macOS inventory controls passed after rebuilding. The initial attempt used
an external process to insert the item, but the isolated Keychain denied the
application access; it was replaced with in-process injection and is not
counted as a successful corruption test. This exercises the complete app
process and real isolated Keychain via the controller, not a human-operated
GUI, personal Keychain, public provider or independent security audit.

The fault-injection guard was subsequently tightened at both command-line
validation and the injection site: the keychain must be the sender file in
the same canonical temporary laboratory directory as the identity file,
with the expected laboratory directory prefix. A/B preflights reject the
receiver keychain and a same-named file in a different directory, in
addition to rejecting non-test mode and an outside marker. The normal signed
transfer and both corrupt-pin A/B application scenarios passed after the
guard change. These checks constrain this laboratory fault injection; they
do not change production key-storage semantics.

`test_sstv_share_app_signed_corrupt_sender_pin_{a,b}` now exercises the
complementary receiver-side signing pin. In the isolated receiver keychain,
the laboratory inserts two distinct invalid 91-byte public-key encodings.
The real sharing controller reports the stored sender key as corrupt, the
laboratory verifies that the bad record is unchanged and clears only that
test record, and the receiver retries the verified signing pin. Both
Decodium processes then exchange and decrypt the A/B signed images. The
same preflight checks reject non-test mode, a marker outside the lab root,
the other actor's keychain and a same-named file in another directory.
Both new tests and both earlier corrupt-recipient tests passed after the
change, as did the AEAD, QML and macOS inventory controls. The inventory
now requires all 14 named signed A/B cases. These are full-process tests
using isolated Keychains and a local HTTPS fixture, not manual GUI operation,
public-provider interoperability or a security audit.

After subsequent SSTV receiver changes, the local AEAD and sharing QML
tests and both signed `replay_index_upgrade_{a,b}` full-process tests were
rerun successfully. The latter again exercise receiver restart and replay
index recovery against the isolated loopback HTTPS provider and temporary
identities; they do not establish independent identity verification or a
security audit.
