# Methods without call-shaped text in selected tests

This is a search aid, not a list or count of untested functions. It excludes other test files and misses property writes, indirect calls, overloaded resolution and calls with other syntax. Existing references and reports must be reviewed before drawing conclusions.

| Module | Method |
| --- | --- |
| `src/bridge/DecodiumBridgeAdifImport.cpp` | `importConfirmedAdifIntoLogbookAsync` |
| `src/bridge/DecodiumBridgeAdvancedDecode.cpp` | `setAdvAutoModeEnabled` |
| `src/bridge/DecodiumBridgeAlcCalibration.cpp` | `finishAlcCalibration` |
| `src/bridge/DecodiumBridgeAudioAdapters.cpp` | `initTxDevices` |
| `src/bridge/DecodiumBridgeAudioIdentity.cpp` | `syncAudioDeviceSettingsToLegacyIni` |
| `src/bridge/DecodiumBridgeAudioSelection.cpp` | `audioInputChannel` |
| `src/bridge/DecodiumBridgeAudioSelection.cpp` | `audioOutputChannel` |
| `src/bridge/DecodiumBridgeAudioSelection.cpp` | `audioOutputDevice` |
| `src/bridge/DecodiumBridgeAudioSelection.cpp` | `setAudioInputChannel` |
| `src/bridge/DecodiumBridgeAudioSelection.cpp` | `setAudioOutputChannel` |
| `src/bridge/DecodiumBridgeAudioSelection.cpp` | `setAudioOutputDevice` |
| `src/bridge/DecodiumBridgeAutoCall.cpp` | `autoCallModeSupported` |
| `src/bridge/DecodiumBridgeAutoCqCadence.cpp` | `resetAutoCqBurstCadence` |
| `src/bridge/DecodiumBridgeCatDispatch.cpp` | `activeCatReportsPttActive` |
| `src/bridge/DecodiumBridgeCatDispatch.cpp` | `activePttMethod` |
| `src/bridge/DecodiumBridgeCatDispatch.cpp` | `setSstvTxPtt` |
| `src/bridge/DecodiumBridgeCatDispatch.cpp` | `sstvTxCanControlPtt` |
| `src/bridge/DecodiumBridgeCatDispatch.cpp` | `sstvTxPttActive` |
| `src/bridge/DecodiumBridgeCatDispatch.cpp` | `sstvTxUsesVoxPtt` |
| `src/bridge/DecodiumBridgeCatQsy.cpp` | `hamlibCatFrequencySettleActive` |
| `src/bridge/DecodiumBridgeDecoPortPtt.cpp` | `keySharedAudioTransmitter` |
| `src/bridge/DecodiumBridgeDecoPortTxFrames.cpp` | `decoPortPumpTxFrames` |
| `src/bridge/DecodiumBridgeDecodeBacklog.cpp` | `resetEarlyDecodeSchedule` |
| `src/bridge/DecodiumBridgeDecodeHistory.cpp` | `openHistoryReadDb` |
| `src/bridge/DecodiumBridgeDecodeMetrics.cpp` | `msSinceLastDecodeModelEmitEnd` |
| `src/bridge/DecodiumBridgeDecodeMetrics.cpp` | `msSinceLastDecodeModelEmitStart` |
| `src/bridge/DecodiumBridgeDecodeMetrics.cpp` | `msSinceLastDecodeReadySlotEnd` |
| `src/bridge/DecodiumBridgeDecodeMetrics.cpp` | `msSinceLastDecodeReadySlotStart` |
| `src/bridge/DecodiumBridgeDecodeMetrics.cpp` | `noteDecodeModelEmitEnd` |
| `src/bridge/DecodiumBridgeDecodeMetrics.cpp` | `noteDecodeModelEmitStart` |
| `src/bridge/DecodiumBridgeDecodeMetrics.cpp` | `noteDecodeReadySlotEnd` |
| `src/bridge/DecodiumBridgeDecodeMetrics.cpp` | `noteDecodeReadySlotStart` |
| `src/bridge/DecodiumBridgeDecodeSecondary.cpp` | `drainLegacyDecodeSecondaryWork` |
| `src/bridge/DecodiumBridgeDecodeSecondary.cpp` | `drainNativeDecodeSecondaryWork` |
| `src/bridge/DecodiumBridgeDecodeSecondary.cpp` | `scheduleLegacyDecodeSecondaryDrain` |
| `src/bridge/DecodiumBridgeDecodeSecondary.cpp` | `scheduleNativeDecodeSecondaryDrain` |
| `src/bridge/DecodiumBridgeDestructor.cpp` | `~DecodiumBridge` |
| `src/bridge/DecodiumBridgeFontSettings.cpp` | `fontSettingFont` |
| `src/bridge/DecodiumBridgeFtxProfiles.cpp` | `setFt2Conservative` |
| `src/bridge/DecodiumBridgeLegacyBridgeAudio.cpp` | `stopBridgeAudioForLegacyDigitalTx` |
| `src/bridge/DecodiumBridgeLegacyDecodeDelta.cpp` | `applyLegacyFullSpectrumModelDelta` |
| `src/bridge/DecodiumBridgeLegacyOwnership.cpp` | `legacyBackendAvailable` |
| `src/bridge/DecodiumBridgeLegacyPreflight.cpp` | `abortLegacyBridgeTxRequest` |
| `src/bridge/DecodiumBridgeLegacyPttTransition.cpp` | `beginLegacyPttTransition` |
| `src/bridge/DecodiumBridgeLegacyPttTransition.cpp` | `clearLegacyPttTransition` |
| `src/bridge/DecodiumBridgeLegacyPttTransition.cpp` | `confirmLegacyPttTransition` |
| `src/bridge/DecodiumBridgeLegacyPttTransition.cpp` | `failLegacyPttTransition` |
| `src/bridge/DecodiumBridgeLegacyPttTransition.cpp` | `setPttConfirmedState` |
| `src/bridge/DecodiumBridgeLegacyPttTransition.cpp` | `setPttPendingState` |
| `src/bridge/DecodiumBridgeLegacyRxHandoff.cpp` | `recoverGpuPanadapterAfterSourceHandoff` |
| `src/bridge/DecodiumBridgeLocalDataState.cpp` | `call3TxtState` |
| `src/bridge/DecodiumBridgeLocalDataState.cpp` | `ctyDatSearchPaths` |
| `src/bridge/DecodiumBridgeLocalDataState.cpp` | `ctyDatState` |
| `src/bridge/DecodiumBridgeLocalUpdate.cpp` | `reloadDxccLookupAsync` |
| `src/bridge/DecodiumBridgeLogPromptDialog.cpp` | `showLogQsoPromptDialog` |
| `src/bridge/DecodiumBridgeLogbookProfiles.cpp` | `defaultAdifLogPath` |
| `src/bridge/DecodiumBridgeMamCaller.cpp` | `activeMamCallerBase` |
| `src/bridge/DecodiumBridgeMamCaller.cpp` | `maybeEnqueueMamCallerFromMessage` |
| `src/bridge/DecodiumBridgeMamCaller.cpp` | `removeActiveMamCallerFromQueue` |
| `src/bridge/DecodiumBridgeMamControls.cpp` | `setDigiPileupBudget` |
| `src/bridge/DecodiumBridgeMamControls.cpp` | `setDigiPileupElastic` |
| `src/bridge/DecodiumBridgeMamState.cpp` | `isMamMultiStreamMode` |
| `src/bridge/DecodiumBridgeMamState.cpp` | `setMamMultiStream` |
| `src/bridge/DecodiumBridgeMode.cpp` | `availableModes` |
| `src/bridge/DecodiumBridgeProcessUsage.cpp` | `updateProcessGpuUsage` |
| `src/bridge/DecodiumBridgePskReporter.cpp` | `pskReporterAntennaInfo` |
| `src/bridge/DecodiumBridgePskReporter.cpp` | `refreshPskReporterLocalStation` |
| `src/bridge/DecodiumBridgePskReporter.cpp` | `syncPskMapSpotWindowToMap` |
| `src/bridge/DecodiumBridgeQ65Split.cpp` | `refreshQ65Doppler` |
| `src/bridge/DecodiumBridgeQ65Split.cpp` | `syncCatSplitModeToLegacy` |
| `src/bridge/DecodiumBridgeReceivedFiles.cpp` | `openFileDialog` |
| `src/bridge/DecodiumBridgeReceivedFiles.cpp` | `saveFileDialog` |
| `src/bridge/DecodiumBridgeRtlSdrCapture.cpp` | `applyRtlSdrSettings` |
| `src/bridge/DecodiumBridgeRtlSdrCapture.cpp` | `scheduleRtlSdrRetune` |
| `src/bridge/DecodiumBridgeRtlSdrCapture.cpp` | `startRtlSdrCapture` |
| `src/bridge/DecodiumBridgeRtlSdrCapture.cpp` | `stopRtlSdrCapture` |
| `src/bridge/DecodiumBridgeRxControl.cpp` | `stopRx` |
| `src/bridge/DecodiumBridgeRxMonitoring.cpp` | `setMonitoring` |
| `src/bridge/DecodiumBridgeSatelliteHalfDuplex.cpp` | `finishFt2LinkSatelliteHalfDuplexTx` |
| `src/bridge/DecodiumBridgeSatelliteHalfDuplex.cpp` | `ft2LinkSatelliteHalfDuplexOperationActive` |
| `src/bridge/DecodiumBridgeSatelliteHalfDuplex.cpp` | `setFt2LinkSatelliteHalfDuplexState` |
| `src/bridge/DecodiumBridgeSaveSettings.cpp` | `saveSettingsInternal` |
| `src/bridge/DecodiumBridgeServices.cpp` | `amplifierObject` |
| `src/bridge/DecodiumBridgeServices.cpp` | `catBackendForPersistence` |
| `src/bridge/DecodiumBridgeServices.cpp` | `catShareObject` |
| `src/bridge/DecodiumBridgeServices.cpp` | `mapIntelligenceService` |
| `src/bridge/DecodiumBridgeServices.cpp` | `propagationManager` |
| `src/bridge/DecodiumBridgeServices.cpp` | `requestSatelliteTrackingWindow` |
| `src/bridge/DecodiumBridgeServices.cpp` | `satelliteTracking` |
| `src/bridge/DecodiumBridgeServices.cpp` | `spotShareObject` |
| `src/bridge/DecodiumBridgeSpecialOperation.cpp` | `legacyTxBackendRequested` |
| `src/bridge/DecodiumBridgeSpecialOperation.cpp` | `setFoxMode` |
| `src/bridge/DecodiumBridgeSpecialOperation.cpp` | `setHoundMode` |
| `src/bridge/DecodiumBridgeSpecialOperation.cpp` | `setNcontest` |
| `src/bridge/DecodiumBridgeSstvAudioSource.cpp` | `currentSstvAudioSourceKind` |
| `src/bridge/DecodiumBridgeSstvAudioSource.cpp` | `currentSstvAudioStreamId` |
| `src/bridge/DecodiumBridgeSstvAudioSource.cpp` | `nativeSstvRxForcesDedicatedAudioCapture` |
| `src/bridge/DecodiumBridgeSstvCalibrationTx.cpp` | `startSstvCalibrationTone` |
| `src/bridge/DecodiumBridgeSstvControls.cpp` | `sstvRxControls` |
| `src/bridge/DecodiumBridgeSstvControls.cpp` | `sstvRxModeChoices` |
| `src/bridge/DecodiumBridgeSstvDiagnosticsSnapshot.cpp` | `refreshSstvDiagnosticsSnapshot` |
| `src/bridge/DecodiumBridgeSstvFax480Tx.cpp` | `startSstvFax480Tx` |
| `src/bridge/DecodiumBridgeSstvHamDrmTx.cpp` | `cancelHamDrmPreparedAudio` |
| `src/bridge/DecodiumBridgeSstvHamDrmTx.cpp` | `startHamDrmPreparedAudio` |
| `src/bridge/DecodiumBridgeSstvImageSave.cpp` | `maybeAutoSaveSstvRxImage` |
| `src/bridge/DecodiumBridgeSstvImageSave.cpp` | `queueSstvRxImageSave` |
| `src/bridge/DecodiumBridgeSstvImageSave.cpp` | `saveSstvRxImage` |
| `src/bridge/DecodiumBridgeSstvImageSave.cpp` | `setSstvRxSaveStatus` |
| `src/bridge/DecodiumBridgeSstvProducerTaps.cpp` | `activateHamDrmRxTap` |
| `src/bridge/DecodiumBridgeSstvProducerTaps.cpp` | `deactivateHamDrmRxTap` |
| `src/bridge/DecodiumBridgeSstvProducerTaps.cpp` | `refreshSstvProducerTaps` |
| `src/bridge/DecodiumBridgeSstvQso.cpp` | `commitSstvNewQso` |
| `src/bridge/DecodiumBridgeSstvQsoAssociation.cpp` | `finishSstvQsoLog` |
| `src/bridge/DecodiumBridgeSstvQsoAssociation.cpp` | `queueSstvQsoAssociation` |
| `src/bridge/DecodiumBridgeSstvQsoChoices.cpp` | `isSstvExistingQsoChoiceCurrent` |
| `src/bridge/DecodiumBridgeSstvQsoChoices.cpp` | `sstvExistingQsoChoices` |
| `src/bridge/DecodiumBridgeSstvRadiofaxTx.cpp` | `startSstvRadiofaxTx` |
| `src/bridge/DecodiumBridgeSstvRadiofaxTx.cpp` | `startSstvRadiofaxTxAdvanced` |
| `src/bridge/DecodiumBridgeSstvRadiofaxTx.cpp` | `startSstvRadiofaxTxAligned` |
| `src/bridge/DecodiumBridgeSstvRadiofaxTx.cpp` | `startSstvRadiofaxTxProfile` |
| `src/bridge/DecodiumBridgeSstvRawAudio.cpp` | `cancelSstvRxAudioJob` |
| `src/bridge/DecodiumBridgeSstvRawAudio.cpp` | `saveSstvRecentRxAudio` |
| `src/bridge/DecodiumBridgeSstvRawAudio.cpp` | `saveSstvRxRawAudio` |
| `src/bridge/DecodiumBridgeSstvRawAudio.cpp` | `saveSstvRxRawAudioImpl` |
| `src/bridge/DecodiumBridgeSstvRedecode.cpp` | `redecodeRecentSstv` |
| `src/bridge/DecodiumBridgeSstvRuntimeShutdown.cpp` | `shutdownSstvRuntime` |
| `src/bridge/DecodiumBridgeSstvRxDiagnostics.cpp` | `sstvRxDiagnostics` |
| `src/bridge/DecodiumBridgeSstvRxReset.cpp` | `abortSstvRxFrame` |
| `src/bridge/DecodiumBridgeSstvRxReset.cpp` | `resetSstvRx` |
| `src/bridge/DecodiumBridgeSstvRxReset.cpp` | `resetSstvRxAfc` |
| `src/bridge/DecodiumBridgeSstvRxReset.cpp` | `resetSstvRxSlant` |
| `src/bridge/DecodiumBridgeSstvRxReset.cpp` | `updateSstvRxControls` |
| `src/bridge/DecodiumBridgeSstvRxSourceRoute.cpp` | `selectSstvRxSource` |
| `src/bridge/DecodiumBridgeSstvRxStart.cpp` | `startSstvRx` |
| `src/bridge/DecodiumBridgeSstvRxStop.cpp` | `stopSstvRx` |
| `src/bridge/DecodiumBridgeSstvRxStop.cpp` | `stopSstvRxForMonitorStop` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `setSstvThumbnailProvider` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvAvailable` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvDetectedMode` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvDiagnostics` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvDigital` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvGallery` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvRxActive` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvRxImageSnapshot` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvRxImageSource` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvRxRequested` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvRxSource` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvRxSourceDevice` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvRxState` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvShare` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvStudio` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvTxNowMs` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvTxPreparedImageSnapshot` |
| `src/bridge/DecodiumBridgeSstvStatus.cpp` | `sstvTxSourceImageSnapshot` |
| `src/bridge/DecodiumBridgeSstvStorageQueue.cpp` | `enqueueSstvStoredRecord` |
| `src/bridge/DecodiumBridgeSstvStorageQueue.cpp` | `queueSstvIncomingImport` |
| `src/bridge/DecodiumBridgeSstvStorageShutdown.cpp` | `shutdownSstvStorage` |
| `src/bridge/DecodiumBridgeSstvStudioArchive.cpp` | `handleSstvStudioPreparedChanged` |
| `src/bridge/DecodiumBridgeSstvStudioArchive.cpp` | `queueHamDrmTransmittedImage` |
| `src/bridge/DecodiumBridgeSstvStudioArchive.cpp` | `queueSstvStudioDraftImage` |
| `src/bridge/DecodiumBridgeSstvStudioArchive.cpp` | `queueSstvStudioTransmittedImage` |
| `src/bridge/DecodiumBridgeSstvTxInit.cpp` | `initialiseSstvTx` |
| `src/bridge/DecodiumBridgeSstvTxLifecycle.cpp` | `releaseSstvTxBridgeOwnership` |
| `src/bridge/DecodiumBridgeSstvTxLifecycle.cpp` | `shutdownSstvTx` |
| `src/bridge/DecodiumBridgeSstvTxLifecycle.cpp` | `tickSstvTx` |
| `src/bridge/DecodiumBridgeSstvWavReplay.cpp` | `finishSstvWavReplay` |
| `src/bridge/DecodiumBridgeSstvWavReplay.cpp` | `restoreSstvLiveAfterReplay` |
| `src/bridge/DecodiumBridgeStartTxPreflight.cpp` | `startTxPreflight` |
| `src/bridge/DecodiumBridgeStartTxSelection.cpp` | `prepareStartTxSelection` |
| `src/bridge/DecodiumBridgeState.cpp` | `activeStations` |
| `src/bridge/DecodiumBridgeState.cpp` | `audioInputDevice` |
| `src/bridge/DecodiumBridgeState.cpp` | `audioOutputDevices` |
| `src/bridge/DecodiumBridgeState.cpp` | `callsign` |
| `src/bridge/DecodiumBridgeState.cpp` | `decodeList` |
| `src/bridge/DecodiumBridgeState.cpp` | `decoding` |
| `src/bridge/DecodiumBridgeState.cpp` | `mode` |
| `src/bridge/DecodiumBridgeState.cpp` | `onUtcTimer` |
| `src/bridge/DecodiumBridgeState.cpp` | `openCatSettings` |
| `src/bridge/DecodiumBridgeState.cpp` | `periodProgress` |
| `src/bridge/DecodiumBridgeState.cpp` | `q65Doppler` |
| `src/bridge/DecodiumBridgeState.cpp` | `rxDecodeList` |
| `src/bridge/DecodiumBridgeState.cpp` | `setUiPaletteIndex` |
| `src/bridge/DecodiumBridgeState.cpp` | `transmitting` |
| `src/bridge/DecodiumBridgeState.cpp` | `utcTime` |
| `src/bridge/DecodiumBridgeTune.cpp` | `launchTuneAudio` |
| `src/bridge/DecodiumBridgeTxMessages.cpp` | `setTx1` |
| `src/bridge/DecodiumBridgeTxMessages.cpp` | `setTx2` |
| `src/bridge/DecodiumBridgeTxMessages.cpp` | `setTx3` |
| `src/bridge/DecodiumBridgeTxMessages.cpp` | `setTx4` |
| `src/bridge/DecodiumBridgeTxMessages.cpp` | `setTx5` |
| `src/bridge/DecodiumBridgeTxMessages.cpp` | `setTx6` |
| `src/bridge/DecodiumBridgeTxMessages.cpp` | `tx1` |
| `src/bridge/DecodiumBridgeTxMessages.cpp` | `tx3` |
| `src/bridge/DecodiumBridgeTxMessages.cpp` | `tx4` |
| `src/bridge/DecodiumBridgeTxMessages.cpp` | `tx5` |
| `src/bridge/DecodiumBridgeTxMessages.cpp` | `tx6` |
| `src/bridge/DecodiumBridgeTxPartnerGuard.cpp` | `repairOrRejectStalePartnerTxMessage` |
| `src/bridge/DecodiumBridgeUiPreferences.cpp` | `setAutoDetachFullSpectrum` |
| `src/bridge/DecodiumBridgeUiPreferences.cpp` | `setSpectrumFpsCap` |
| `src/bridge/DecodiumBridgeUiPreferences.cpp` | `setUiFramelessPopouts` |
| `src/bridge/DecodiumBridgeUiPreferences.cpp` | `setUiQuality` |
| `src/bridge/DecodiumBridgeUiPreferences.cpp` | `setUiStyle` |
| `src/bridge/DecodiumBridgeWorkedHistory.cpp` | `applyDecoLogRows` |
| `src/bridge/DecodiumBridgeWorkedHistory.cpp` | `configureDecoLogLink` |
| `src/bridge/DecodiumBridgeWorkedHistory.cpp` | `workedCount` |
| `src/bridge/DecodiumBridgeWorkingFrequencies.cpp` | `workingFrequencyForBandMode` |

## Confirmed indirect TX-message coverage

`bridge_tx_messages_runtime_lab.py` calls `setTxMessage` for slots 1–5 and
slot 6 (through a high-index clamp). The dispatch in
`DecodiumBridgeTxMessages.cpp` calls `setTx1` through `setTx6` for those slots.
The retained `tx-messages-legacy-v1.0.737-macos.txt` and
`tx-messages-legacy-refactor-macos.txt` record both app variants passing with
the legacy backend active and CAT disabled. The test does not read back the
embedded legacy widgets or transmit RF. These six names appear above only
because the search looks for direct textual calls. No test was rerun.

## QML properties and indirect setters

The missing-call search includes Q_PROPERTY setters: assignment to a QML
property invokes the setter without spelling its C++ name. Existing audio
selection/reload labs write both input and output channels; advanced decode
writes `advAutoModeEnabled`; UI preference labs write the auto-detach, FPS,
frameless and quality properties. Special-operation wrappers exercise Fox,
Hound and contest. MAM's `setDigiPileupEnabled` calls both the elastic and
budget setters, with matching app and harness results. Applying ready profiles
calls `setFt2Conservative`; the palette runtime probe invokes its setter and
checks two resulting indices. Existing report entries describe their A/B
placements and limitations. Prior macOS style tests affected a native setting;
its original value is unknown, so no style test was rerun. These mappings
explain search omissions rather than independently proving every property or
visual behavior.

## Audio adapter and device identity review

The search misses unqualified calls inside injected application probes:
`bridge_audio_adapters_runtime_ab.py` invokes `initTxDevices()` directly in
both app placements; the main report records that probe, but this review did
not locate a compact raw output file for it. The retained
`audioidentity/` before/after app results show the same BlackHole 2ch/16ch
input/output selections and IDs for two combinations. The associated A/B
harness covers identity changes and legacy-INI synchronization with a
substituted legacy sink. `audio_selection/` and reload-settings labs exercise
QML input/output channel properties. These results do not prove physical device
hot-plug or Linux audio behavior; no test was rerun for this review.

## DecoPort and RTL-SDR review

The existing DecoPort TX A/B harness exercises admission, PCM framing/pacing
and simulated PTT release (`decoporttxframes/ab.txt`); the corresponding app
probe checks disconnected admission with CAT/TX off (`runtime.txt`). The
report also records a logical PTT/watchdog A/B with a three-second real timer,
and an RTL-SDR A/B using an in-memory receiver for configuration, retune,
restart and stop. These are simulated collaborators, not evidence of an active
remote radio or physical RTL-SDR. No test was rerun in this review.

## Service accessor paths

The report documents `bridge_services_runtime_ab.py` checking propagation,
satellite, map and callsign services and the satellite-window signal in both
app placements. `bridge_sharing_accessors_runtime_ab.py` checks CAT sharing,
spot sharing and amplifier objects plus three CAT-backend persistence cases.
These methods are called without a receiver qualifier in the injected C++
probe and therefore were missed by the call-shape search. This review inspected
the probes and report but did not locate separate compact result files; it did
not rerun either test or establish live service connections.

Service accessor follow-up: no prior service-accessor output was found among
matching top-level `/tmp` files in this check. The saved pre-extraction
fixture remains contained in `DecodiumBridgeServices.cpp`, the test insertion
marker remains present, and both runtime probes assert concrete before/after
objects and states. Those source facts do not replace retained execution logs;
the main report's earlier pass remains the only recorded execution summary
located here. No tests were rerun.

Inventory update: removed 4 qualified-call entries not detected as method definitions. The parser is heuristic and counts remain discovery aids.

## TCI and receive-audio lifecycle

The retained full suite reports `TCI audio: A/B OK` for the controlled
transport/audio collaborators. The report describes separate signal-routing
and completion A/B, while explicitly excluding an actual TCI server and network
audio transport. `legacy-audiocapture-repeat-ab.txt` records matching RX
start/stop and non-silent 12 kHz mono recordings in both placements;
`audiostop-runtime.txt` records a monitor stop/restart with two local apps and
virtual audio. These do not establish physical RF or hardware capture. No tests
were rerun for this review.

## SSTV controls and satellite state

Retained SSTV helper results show matching mode choices, controls and Radiofax
profiles for both placements; status results show matching synthetic Replay
start/stop and image-state/reset checks. The main report documents additional
source-route and diagnostics A/B with simulated inputs. Satellite half-duplex
`before.json`/`after.json` match for disabled, invalid and valid synthetic
configuration; both options results report the same satellite count and mode
list. Q65 split/Doppler tests use disconnected or synthetic CAT. These checks
do not prove physical reception, orbital corrections or rendered pixels. No
tests were rerun for this review.

## ALC, output selection and watchdog

The existing ALC harness compares six method bodies using real calibration
math and simulated CAT/telemetry; the retained app results include matching
unavailable-CAT rejection. `tx_output_device/before.json` and `after.json`
record both placements selecting the virtual output and decoding the CQ in a
second app. `watchdog-runtime.txt` describes controlled real-timer A/B and a
separate virtual monitoring/mode-change probe; the app probe did not trigger
an actual watchdog fault. These are scoped software tests, not a physical ALC,
PTT, failing-device or RF measurement. No tests were rerun in this review.

## SSTV lifecycle and storage paths

The main report documents before/after isolated-app comparisons for existing
QSO association through the real temporary storage worker, storage queue and
shutdown, runtime shutdown, source routing, RX reset and TX lifecycle. Retained
`sstv-source-signals-ab.txt` and `sstv-image-reset-repeat-ab.txt` show matching
source notifications and image reset state. The TX lifecycle report retains
initial incomplete Robot 12 receptions and a later successful longer-observed
A/B; the different observation time does not identify their cause. These are
synthetic/local/virtual-audio scenarios, not physical RF. No tests were rerun.

## Main-window state accessors

`bridge_state_runtime_ab.py` invokes the real callsign/mode/decode-list/RX-list,
period, transmitting and decoding getters, fires the UTC timer and checks CAT
settings and time signals against explicit expected values in both source
placements. The main report records that isolated app A/B; separate UI/audio
accessor and virtual monitor tests are documented nearby. The calls occur
unqualified inside injected C++, so the selected call-shape search misses them.
No compact output for this specific state probe was located in the current
review; the report is the retained execution summary. No test was rerun.

Audio adapter follow-up: `audio-adapters-runtime-current-ab.txt` now retains
the before/after application output, including the same virtual device ID
and format and CAT/TX off. This does not establish physical hot-plug.

Audio selection follow-up: the four retained `audio_selection/` before/after
pairs were compared directly: two missing-device/channel-boundary cases and
two BlackHole input/output cases match with monitor and TX false. The QML
properties in `DecodiumBridge.h` explicitly bind these reads and writes to
`audioInputDevice`, `audioOutputDevice`, `audioInputChannel`,
`audioOutputChannel` and their corresponding setters. These results exercise
the accessor paths without requiring literal C++ call names in the test. They
do not cover every device or physical hot-plug. No test was rerun.

SSTV thumbnail provider follow-up: `bridge_sstv_studio_status_ab.py` calls
`setSstvThumbnailProvider` for bind, repeated bind and unbind while preparing
a synthetic image. The retained `sstv-studio-status-repeat-ab.txt` confirms
matching bound/unbound, image and clock results with CAT/TX off. The result
does not separately inspect gallery thumbnail rendering. No test was rerun.

SSTV status group follow-up: directly compared the retained before/after JSON
in `sstv-status-repeat-ab.txt`, `sstv-status-getters-ab.txt`,
`sstv-image-reset-repeat-ab.txt` and `sstv-source-signals-ab.txt`: six
scenarios agree with CAT/TX false. The probes explicitly call status getters
without a `bridge.` qualifier, so the selected call-shape search misses them.
They cover synthetic Replay/controller/image and notification states, not RF
or all decoder metrics. No test was rerun.

Legacy PTT follow-up: the two retained `legacy-ptt-transition-expanded-ab.txt`
and `legacyptttransition-coverage-recheck-ab.txt` records have identical
before/after pending, confirmed, mismatch, timeout, cancellation and audio
failure outcomes with CAT/TX false. The `legacy_bridge_audio/` and
`legacy_preflight/` pairs both decode the same message over virtual audio.
PTT confirmation was simulated; these are not physical relay measurements.
No test was rerun.

File-dialog and storage follow-up: retained `file-dialog-runtime-ab.txt`
records identical before/after cancel, open and save selection, and
`directory-dialog-unicode-ab.txt` records cancel and selection.
`received-files-runtime-ab.txt` records matching filenames and byte counts;
`adif-import-wrapper-ab.txt` and `adif-full-export-ab.txt` each document two
isolated app scenarios per placement. The dialogs are non-native/offscreen,
and the import/export use synthetic records. No test was rerun.

PSK station/map follow-up: `bridge_psk_ab.py` compares the release source
with the extracted code and exercises `setPskMapSpotWindowMinutes`, which
calls `syncPskMapSpotWindowToMap`, verifying the mock map receives 60. Enabling
the mock reporter calls `refreshPskReporterLocalStation` and the antenna
getter; the updated A/B harness verifies the trimmed antenna actually reaches
the mock. `psk-antenna-app-ab.txt` additionally records the same getter result
and queued spot in both isolated app placements with external network denied.
This does not demonstrate delivery to the external PSK Reporter service.

Auto Call/WSPR follow-up: the retained `autocall-{baseline,refactor}-{a,b}.txt`
files match for FT8 enable/disable and WSPR rejection; the real
`setAutoCallEnabled` calls `autoCallModeSupported`. The four
`wspr_callback/wspr-{before,after}-{a,b}.txt` files match for two synthetic
rows; the production callback calls `parseWsprRow`. These results do not cover
physical SWR/RF or all modes. No test was rerun.
