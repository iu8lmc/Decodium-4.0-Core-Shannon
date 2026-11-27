# QSO, MAM and sequencing evidence review

Existing results only; no test rerun. Scenarios use synthetic state and/or virtual audio and do not establish complete on-air QSOs.

| Module | Retained files | Scope |
| --- | --- | --- |
| `DecodiumBridgeMamIngest.cpp` | `mamingest/direct-before.json`, `mamingest/direct-after.json`, `mamingest/queued-before.json`, `mamingest/queued-after.json` | synthetic direct and queued decodes |
| `DecodiumBridgeMamLog.cpp` | `mamlog/before.json`, `mamlog/after.json` | synthetic slot and isolated logbook |
| `DecodiumBridgeMamPromote.cpp` | `mampromote/before.json`, `mampromote/after.json` | simulated queue promotion |
| `DecodiumBridgeMamRecovery.cpp` | `mamrecovery/before.json`, `mamrecovery/after.json` | synthetic late report and virtual audio |
| `DecodiumBridgeCallerScoring.cpp` | `caller_scoring/callerscore-before-a.txt`, `caller_scoring/callerscore-after-a.txt` | opposite synthetic caller order |
| `DecodiumBridgeCqReplyWindow.cpp` | `cq_window/cqwindow-before-a.txt`, `cq_window/cqwindow-after-a.txt` | AutoCQ arm/clear; CAT disabled |
| `DecodiumBridgeDeferredAutoSeq.cpp` | `deferred_autoseq/deferred-before-a.txt`, `deferred_autoseq/deferred-after-a.txt` | synthetic FT message state |
| `DecodiumBridgeDirectedReportGuard.cpp` | `directed_report_guard/report-before-a.txt`, `directed_report_guard/report-after-a.txt` | directed report scenario |
| `DecodiumBridgePromptSnapshot.cpp` | `promptsnapshot-ab.txt`, `promptsnapshot-runtime.txt` | snapshot harness and synthetic prompt without QSO commit |
| `DecodiumBridgeQsoCompletion.cpp` | `qsocompletion-ab.txt`, `qsocompletion-runtime.txt` | controlled cleanup and synthetic isolated QSO |
| `DecodiumBridgeQsoLimit.cpp` | `qsolimit-ab.txt`, `qsolimit-runtime.txt` | controlled limit and isolated settings |
| `DecodiumBridgeResumeReply.cpp` | `resume_reply/resume-positive-before-a.txt`, `resume_reply/resume-positive-after-a.txt` | synthetic reply and immediate halt over virtual audio |
| `DecodiumBridgeStartupQso.cpp` | `startupqso-ab.txt`, `startupqso-runtime.txt` | synthetic QSO fields with TX off |
