# FT8: MyCall AP request wiring and phantom calls

Base: local main, v1.0.618 (`211a90ccafaeb6084fea0822f40182dd35642a4c`).
Verified on macOS 26.6.2 arm64, Qt 6.11.0, 2026-09-08.

## Evidence and limits

The local ALL.TXT contains:

```
260908_182500    14.074 Rx FT8    -17  0.3 2435 9H1SR CR9KVV -42
```

The receiver measured -17 dB; -42 is the report carried in the message.
Other suspicious one-off messages to 9H1SR that day carried -11 and +06.
No TX record was found for that day before the reported event. Diagnostic
records identify the embedded MainWindow decode dispatch as the active path.
There is no recording of that exact slot, so the AP type and DSP cause of that
individual decode cannot be proved from ALL.TXT. A report cutoff at -28 would
not address other false messages carrying ordinary reports.

The MyCall AP gate already existed upstream, but only an alternative bridge
producer set it. MainWindow's requests did not transport it and the worker did
not apply it. The process-wide default therefore left the hypotheses enabled.
Two native live-pass builders could also re-enable AP despite AP being off.

## Change

- Transport MyCall AP eligibility with each immutable FT8 DecodeRequest.
- Set and restore the DSP gate inside the worker's runtime mutex, preventing
  queued UI requests from changing the gate of an in-flight decode.
- Wire both bridge producers and the active MainWindow producer.
- Allow MyCall hypotheses during FT8 message transmission and for 180 seconds
  from a recorded FT8 message TX start. Tuning, an armed Auto TX, RX resume and
  selected/stale QSO state do not establish recent transmission. A clock value
  earlier than the recorded TX does not qualify. Offline decoding retains AP.
- Honour AP disabled in native fast/deep request construction.
- Add `ap_mycall=0/1` to FT8 DECODEMETRIC logging.

This suppresses AP types 2–6 in idle reception, using the existing upstream
gate. It does not discard normally decoded calls addressed to the local
station. CQ and independent historical hypotheses keep their existing policy.
This is a mitigation of a verified wiring defect, not proof that every possible
false decode has been eliminated.

## Validation

Build succeeded:

```
cmake --build build --target test_ft8_mycall_ap decodium_qml -j 4
cmake --build build --target test_qt_helpers -j 4
```

New test `test_ft8_mycall_ap`: **9 passed, 0 failed, 0 skipped**. Covers recent
TX expiry and future timestamps, CQ preservation, applying/restoring the
request's gate, AP disabled, CPU pressure, and decoding a generated
`9H1SR K1ABC -11` waveform while MyCall AP is off. The waveform stays entirely
in memory; no radio transmission is performed.

Existing targeted tests with current defaults: **15 passed, 1 failed**.
`ft8_ap_pass_meta` assumes 13 passes but the current upstream defaults select
17 (CQ and predictive message AP). Neither the test nor its pass-count
implementation was changed here. The same selection with the historical
profile (`DECODIUM_FT8_AP_MSG=0 DECODIUM_FT8_AP_CQ=0
DECODIUM_FT8_AP_STORICO=0`) passes **16/16**. This environment was limited to the
test process; application preferences were not changed.

Selected existing tests: ft8_ap_pass_meta, ft8_prepare_decode_pass_meta,
ft8_decode_pass_policy_meta, ft8_validation_and_snr_meta,
ft8var_false_decode_bridges, ft8_message77_unpack_and_tones,
ftx_decode77_round_trip.

`git diff --check` passed. Existing SuperFox indicator and compact Next-message
QML edits remain present. No commit, push or release performed.

Build retains the existing Homebrew/macOS deployment-target linker warnings;
this validates the local build, not distribution to older macOS versions.

## Activation and live verification remaining

`build/decodium` was rebuilt at 20:46 local. The running process (PID 42532)
started at 20:23 and was deliberately left running during reception. Restart
Decodium to load the rebuilt executable. In idle FT8 reception the new
DECODEMETRIC entries should show `ap_mycall=0`. Controlled tests pass, but
post-restart on-air behaviour and the exact reported audio slot remain
unverified.

Local test logs: `/tmp/decodium-mycall-tests.txt`,
`/tmp/decodium-mycall-existing-tests.txt`,
`/tmp/decodium-mycall-legacy-profile-tests.txt`.
