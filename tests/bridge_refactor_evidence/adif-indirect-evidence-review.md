# ADIF module evidence review

These four modules have no literal module-name reference in the runtime-script
index; the following existing results identify their evidence. No test rerun.

- `DecodiumBridgeAdifRecord.cpp`: the release/refactor serializer harness
  outputs `adif-record-real-baseline.txt` and `adif-record-real-refactor.txt`
  match with real band lookup/sanitizer. Synthetic QSO app workflows are
  recorded in `sstvqso/runtime-ab.txt` and `adif-full-export-ab.txt`.
- `DecodiumBridgeAdifAppend.cpp`: `adifappend-ab.txt` is the before/after
  harness and `adifappend-runtime.txt` records an isolated synthetic log;
  `sstvqso/runtime-ab.txt` confirms app-level QSO creation in both QSO-code
  placements. Folder opening uses a controlled substitute, not a native
  file-manager observation.
- `DecodiumBridgeAdifTcp.cpp`: `adiftcp-ab.txt` comes from a before/after
  harness with a temporary loopback server; `adiftcp-runtime.txt` records
  synthetic application delivery.
- `DecodiumBridgeAdifUdp.cpp`: `adifudp-ab.txt` is a before/after harness;
  `adifudp-runtime.txt` records synthetic application delivery on loopback.

Loopback/simulated paths do not establish external loggers, and the app-level
TCP/UDP records do not themselves label both extraction placements.
