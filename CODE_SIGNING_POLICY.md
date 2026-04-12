# Code Signing Policy

Decodium 3.0 FT2 "Raptor" Windows installers and executables are digitally signed.

## Certificate

Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).

## Signing Process

All signing requests are submitted automatically through GitHub Actions CI/CD pipelines. The build artifacts are compiled from source code in this public repository and submitted to SignPath for signing. The private key is stored on a Hardware Security Module (HSM) managed by SignPath Foundation.

- **Release signing** requires manual approval from an authorized Approver.
- **Test signing** is used for development builds and pull requests.

## Team Roles

| Role | Members | Description |
|------|---------|-------------|
| **Author** | [iu8lmc](https://github.com/iu8lmc) | Trusted to modify source code directly |
| **Reviewer** | [iu8lmc](https://github.com/iu8lmc) | Reviews all external contributions before merge |
| **Approver** | [iu8lmc](https://github.com/iu8lmc) | Authorizes release signing requests |

## Privacy Policy

This program will not transfer any information to other networked systems unless specifically requested by the user. Decodium is a ham radio digital communication client that transmits and receives RF signals via the user's radio equipment. Network connections are only made when:
- The user enables UDP message forwarding to a local or remote application
- The user manually triggers a web lookup (e.g., callsign database)
- The user enables automatic CTY.DAT updates from a public server

No telemetry, analytics, or user tracking data is collected or transmitted.

## Verification

To verify a signed binary:
1. Right-click the `.exe` file and select **Properties**
2. Go to the **Digital Signatures** tab
3. The signer should be **"SignPath Foundation"**
4. Click **Details** to verify the signature is valid

## Reporting Suspicious Binaries

If you find a suspicious file claiming to be Decodium, please open an issue on this repository with:
- The SHA256 hash of the file
- The download source URL
- Any other relevant details

## License

Decodium 3.0 FT2 is licensed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.txt).
