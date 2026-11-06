# Decodium SSTV share server — work in progress

This is an experimental implementation under development, not an audited
production service. **Do not expose it to the Internet or use it for real
accounts yet.** Authentication, data retention and resource limits require
further review and hardening. The certificate used by the tests is public and
must never be used for a real deployment.

The server currently requires Python 3.10 or newer and has no runtime
dependencies outside the standard library. Its SQLite database persists
encrypted payloads and only hashes randomly generated account tokens. The
provider sees transfer metadata, and encrypting content does not replace
independent verification of recipient and sender keys.

## Install for local evaluation

From the repository root, create an isolated Python environment and install:

```sh
python3 -m venv .venv-share-server
.venv-share-server/bin/python -m pip install ./tools/sstv-share-server
```

On Windows use `py -3 -m venv .venv-share-server` followed by
`.venv-share-server\Scripts\python -m pip install .\tools\sstv-share-server`.
Replace the executable path below with
`.venv-share-server\Scripts\decodium-sstv-share-server.exe` on Windows.

Choose an existing private directory for the SQLite database. Create one
account per Decodium user; the command prints a randomly generated bearer
token once. Keep each token private and save it in that user's Decodium
provider configuration; tokens are not recoverable from the database.
If a token is lost or compromised, run `rotate-account alice` with the same
database and provider options; the old token immediately stops working and
the new one is shown once.

```sh
.venv-share-server/bin/decodium-sstv-share-server --database /private/path/share.sqlite --provider-id myprovider add-account alice
.venv-share-server/bin/decodium-sstv-share-server --database /private/path/share.sqlite --provider-id myprovider add-account bob
```

Start the server with a certificate and private key, using a hostname and CA
trusted by the Decodium clients. The default listener is `127.0.0.1:8443`:

```sh
.venv-share-server/bin/decodium-sstv-share-server --database /private/path/share.sqlite --provider-id myprovider serve --certificate /private/path/server.crt --key /private/path/server.key
```

In each Decodium Sharing page, configure REST, the same provider ID, the
`https://` endpoint, Bearer authentication and the corresponding token.
Configure the five upload paths as `/api/v1/transfers`,
`/api/v1/transfers/{uploadId}/chunks`, `/api/v1/transfers/{uploadId}`,
`/api/v1/transfers/{uploadId}/complete`, and
`/api/v1/transfers/{uploadId}` for create, chunk, status, complete and
cancel, respectively. Account IDs must match the sending station's normalized
sender ID and the recipient ID entered in Decodium. Verify recipient fingerprints
through a separate trusted channel; do the same for sender signing keys when
using signatures.

The current service accepts only encrypted payloads, limits each object to
8 MiB, reserves at most 64 MiB per account and 512 MiB across the server,
and permits at most 128 accounts and 128 live transfers per account.
Transfers may expire at most 31 days after creation; expired records are
removed periodically and before new uploads. Back up the database and TLS
key according to your own retention policy; losing the Decodium recipients'
private keys prevents recovery of their encrypted images.

## Verification

Run the local development tests from this directory:

```sh
python3 -m unittest -v test_store test_server
```

On macOS the repository helper
`python3 tests/sstv/tools/run_share_server_two_app_lab.py` runs two full
Decodium test-mode processes against this authenticated HTTPS service with
isolated temporary Keychains and two distinct images. Add `--unsigned` to
test encrypted messages without sender signatures. These local results are
not an independent security audit or proof of Internet deployment.
