# Native SSTV documentation

This directory is the evidence and operator-documentation index for the SSTV
subsystem built into Decodium4. Capability claims are limited to the current
source tree and the named tests; deterministic loopback is not evidence of a
live radio, another application or a maintained-platform release bundle.

## Operators

- [User guide](USER_GUIDE.md): receive, Studio/TX, Gallery, sharing, HAMDRM,
  diagnostics, privacy and troubleshooting.
- [Release notes](RELEASE_NOTES.md): included capabilities, packaging needs and
  deliberate non-claims for this development snapshot.
- [Analog mode matrix](MODE_MATRIX.md): canonical registry geometry, duration,
  VIS, RX/TX and evidence status for every registered analog mode.
- [HAMDRM compatibility matrix](HAMDRM_COMPATIBILITY_MATRIX.md): exact native
  digital subset, connected waveform adapters and external-interoperability
  gates.
- [Gallery retention policy](GALLERY_RETENTION_POLICY.md) and
  [QSO logging](QSO_LOGGING.md): ownership, cleanup and ADIF boundaries.

## Developers and reviewers

- [Developer guide](DEVELOPER_GUIDE.md) and
  [architecture audit](ARCHITECTURE_AUDIT.md): target/thread boundaries and
  extension rules.
- [Mode catalogue](MODE_CATALOG.md): sources and unresolved catalogue entries;
  the executable status remains in `MODE_MATRIX.md`.
- [RX correction](RX_CORRECTION_ARCHITECTURE.md),
  [Studio/TX](STUDIO_TX.md), [AVT protocol](AVT_PROTOCOL.md) and
  [HAMDRM architecture](HAMDRM_ARCHITECTURE.md): subsystem designs.
- [Remote sharing protocol](REMOTE_SHARING_PROTOCOL.md) and its
  [OpenAPI profile](remote-sharing-openapi.yaml): provider-neutral client
  contract, trusted-broker boundary and absence of a bundled production relay.
- [Threat model](THREAT_MODEL.md),
  [performance counters](PERFORMANCE_COUNTERS.md),
  [test strategy](TEST_STRATEGY.md) and
  [definition of done](DEFINITION_OF_DONE.md): bounds, privacy and evidence.
- [Upstream provenance](UPSTREAM_PROVENANCE.md): audited references and
  clean-room/license constraints.

The `generate_sstv_mode_matrix` developer target and `test_sstv_mode_docs`
keep the canonical columns of `MODE_MATRIX.md` synchronized with the native
registry. Hand-written evidence and interoperability notes must remain honest
when the generated registry fields change.
