# Remote sharing E2EE: implementation in progress

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
manifests; its encrypted opener is a fixture. Additional controls fail the
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
