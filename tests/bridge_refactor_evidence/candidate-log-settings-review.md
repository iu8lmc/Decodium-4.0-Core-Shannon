# Candidate indirect-call review: log, profiles and settings

Existing results only, no rerun. Synthetic/local tests do not demonstrate external services, hardware or every branch.

| Module | Evidence | Scope and limitation |
| --- | --- | --- |
| `DecodiumBridgeExternalAdif.cpp` | `externaladif-ab.txt`, `externaladif-runtime.txt` | simulated services and private UDP; no real upload |
| `DecodiumBridgeExternalUrl.cpp` | `external-url-baseline.txt`, `external-url-refactor.txt` | URL validation; browser launching not established |
| `DecodiumBridgeFontSettings.cpp` | `fontscale-ab.txt`, `fontscale-runtime.txt` | bounds/settings; visual appearance not compared |
| `DecodiumBridgeFt2LogBridge.cpp` | `ft2logbridge-ab.txt`, `ft2logbridge-runtime.txt` | synthetic local QSO; not RF |
| `DecodiumBridgeFt2PartnerGuards.cpp` | `ft2_partner_guards/ft2partner-before-a.txt`, `ft2_partner_guards/ft2partner-after-a.txt` | synthetic resume; not every guard in app |
| `DecodiumBridgeGenericSettings.cpp` | `generic_settings/settings-before-a.txt`, `generic_settings/settings-after-a.txt` | isolated profile; not real credential store |
| `DecodiumBridgeKeyboardLog.cpp` | `keyboardlog-ab.txt`, `keyboardlog-runtime.txt` | synthetic log action |
| `DecodiumBridgeLogPrompt.cpp` | `logprompt-baseline-a.txt`, `logprompt-refactor-a.txt` | isolated prompt state; no on-air QSO |
| `DecodiumBridgeLogPromptLifecycle.cpp` | `promptlifecycle-ab.txt`, `promptlifecycle-runtime.txt` | harness acceptance/rejection and synthetic app prompt |
| `DecodiumBridgeLogbookProfiles.cpp` | `logbook-profiles-v1.0.737-macos.txt`, `logbook-profiles-refactor-macos.txt` | isolated profile operations |
| `DecodiumBridgeQsoFields.cpp` | `qsofields-ab.txt`, `qsofields-runtime.txt` | synthetic fields, no physical radio |
| `DecodiumBridgeWorkedHistory.cpp` | `workedhistory-harness-ab.txt`, `workedhistory-runtime-ab.txt` | temporary ADIF and worked index |
