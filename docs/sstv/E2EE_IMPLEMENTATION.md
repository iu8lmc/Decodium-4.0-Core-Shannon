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
This experimental protocol does not authenticate the sender and has not had
an independent security review.

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

This construction does not authenticate the sender: anyone who knows the
recipient public key can create an encrypted payload asserting an arbitrary
sender. A verified sender identity requires an additional authenticated
signature or equivalent protocol, and must not be inferred from the manifest.

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
