#include "DecodiumBridgeAudioDeviceIdentity.h"
#include "BridgeConfigConstants.h"
#include "Ft2LinkPcmStats.h"
#include "DecodeValidationRules.h"
using namespace decodium::decode_validation;
#include "DecodeUnresolved.h"
using decodium::decode_unresolved::messageIsUnresolvedGridOnlyDecode;
using decodium::decode_unresolved::displayMessageWithCallsignsActorFirst;
using decodium::decode_unresolved::messageHasUnresolvedPeerPlaceholder;
using decodium::decode_unresolved::isFst4wUnresolvedHashDecode;
#include "DecodeTokenClassifier.h"
#include "UdpTargetHelpers.h"
#include "SpecialOperationBounds.h"
#include "FtxMessageHelpers.h"
#include "DecodiumBridgeMamWave.h"
#include "DecodiumBridgeWavIO.h"
#include "DecodiumBridgeTxWave.h"
#include "DecodiumBridgeTxTimingMath.h"
#include "DecodiumBridgeTxPolicy.h"
#include "DecodiumBridgeTxPcm.h"
#include "DecodiumBridgeTxFormat.h"
#include "DecodeSignalReport.h"
#include "DecodeChronology.h"
#include "DecodeUtcTime.h"
#include "DecodeUserFilterConfig.h"
#include "LegacyHashResolve.h"
#include "LegacyDecodeText.h"
#include "DecodiumBridgeDecodeUiFlags.h"
#include "DecodiumBridgeTraceScope.h"
#include "DecodiumBridgeWorkedHistory.h"
#include "DecodiumBridgeQsoCacheInternal.h"
#include "DecodiumBridge.h"

quint64 decodiumCurrentProcessCpuUsec();
bool decodiumCurrentProcessGpuTimeNs(quint64*);
#include "AdifDocumentInternal.h"
using namespace decodium::adif_document;
#include "DecodeColorHelpers.h"

using namespace decodium::decode_colors;
#include "DecodeMessageNormalization.h"
#include "Ft2LinkAccessCrypto.h"
#if defined(Q_OS_MACOS)
#include <execinfo.h>
#include <cstdlib>
#endif
#include "SettingsAsyncHandoff.h"
#include "AlcCalibrationMath.h"
#include "AdifExportSanitizer.h"
#include "Network/AdifUdpPayload.hpp"
#include "Sequencer/AutoCqCallPolicy.hpp"
#include "Sequencer/CallerScoring.hpp"

#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvQsoLog.h"
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/integration/SstvTxCoordinator.h"
#endif

#include "DecoPortLink.h"
#include "DecolinkLink.h"
#include "DecoPortRigDriver.h"
#include "DecodiumDecoPortGateway.h"
#include "DecodiumNativeCatShare.h"
#include "DecodiumSerialPortRegistry.h"
#include "AudioModeChangeCapturePolicy.h"
#include "DecodeUiFilterPolicy.h"
#include "MamTxPayloadPolicy.h"
#include "RttyRxRecovery.h"
#include "src/core/HostProcessEnvironment.h"
#include "Ft2LinkSatelliteHalfDuplex.h"
#include "LinuxDrmGpuUsage.h"
#include "DecodeListModel.h"
#include "FtDecodeThreadBudget.hpp"
#include "LegacyDecodeWindow.hpp"
#include "PanadapterItem.hpp"
#include "GraphicsPipelineCache.hpp"
#include "lib/persistence/DecodeHistoryWorker.h"  // 1.0.238 Phase 5.2 perf roadmap
#include "DecodiumLogging.hpp"
#include "DecodiumAlertManager.h"
#include "DecodiumRigDetector.h"
#include "DecodiumDiagnostics.h"
#include "DecodiumPropagationManager.h"
#include "SatelliteTrackingService.h"
#include "Q65DopplerTracker.h"
#include "MapExternalOverlayService.h"
#include "MapIntelligenceService.h"
#include "CallsignIntelligenceService.h"
#include "MapLayerModel.h"
#include "DecodiumProfileSettings.h"
#include "CatNativeMigration.h"
#include "Sequencer/QsoSequencerRules.hpp"
#include "Sequencer/MessageTokenRules.hpp"
#include "DecodiumDxCluster.h"
#include "DecodiumSelfCheck.hpp"
#include "Network/MessageClient.hpp"
#include "Network/UdpClientId.hpp"
#include "DxccLookup.h"
#include "DecodiumLegacyBackend.h"
#include "Network/DecodiumPskReporterLite.h"
#include "Network/DecodiumCloudlogLite.h"
#include "Network/DecodiumDecoLogLink.h"
#include "Network/DecodiumQrzLogbookLite.h"
#include "Network/DecodiumWsprUploader.h"
#include "Network/NtpClient.hpp"
#include "Network/DecoSyncTime.hpp"
#include "Web/DecodiumWebServer.hpp"
#include "Network/RemoteCommandServer.hpp"
#include "UsStateDataManager.h"
#include "PrecisionTime.hpp"
#include "SecureSettings.hpp"
#include <QHostAddress>
#include <QProcessEnvironment>
#include "controllers/ActiveStationsModel.hpp"
#include "helper_functions.h"
#include "Detector/FT8DecodeWorker.hpp"
#include "Detector/FftCompat.hpp"
#include "Detector/FT2DecodeWorker.hpp"
#include "Detector/CallsignHash28.h"  // 1.0.293 — hash28 condiviso per AP cache
#include "Detector/FT4DecodeWorker.hpp"
#include "Detector/Q65DecodeWorker.hpp"
#include "Detector/MSK144DecodeWorker.hpp"
#include "Detector/WSPRDecodeWorker.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "Detector/FST4DecodeWorker.hpp"
#include "Audio/AudioDevice.hpp"
#include "Audio/soundin.h"
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "Transceiver/TransceiverFactory.hpp"
#include "commons.h"
#include "widgets/FoxWaveGuard.hpp"
#include "widgets/itoneAndicw.h"
#include "Decoder/decodedtext.h"
#include "DecodiumAudioSink.h"
#include "src/rtl/RtlSdrAudioOutput.h"
#include "src/rtl/RtlSdrCapabilities.h"
#include "src/rtl/RtlSdrDsp.h"
#include "src/rtl/RtlSdrInput.h"
#include "src/rtl/RtlSdrRfSpectrum.h"
#include "src/rtl/RtlSdrTuningPlan.h"
#include "Radio.hpp"
Radio::Frequency decodiumBridgeParseFrequencyHzValue(const QVariant&, bool*);
#include "models/Bands.hpp"
#include "models/FrequencyList.hpp"
#include "models/StationList.hpp"
#include "revision_utils.hpp"

#include <QClipboard>
#include <QCoreApplication>
#include <QByteArray>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QScreen>
#include <QQuickGraphicsConfiguration>
#include <QQuickWindow>
#include <QWindow>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QFormLayout>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QMessageAuthenticationCode>
#include <QNetworkInterface>
#include <QHostInfo>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QSerialPortInfo>
#include "DecoPortHubOwners.h"
#include <QtConcurrent/QtConcurrent>
#include <QPointer>
#include <QSslSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QSqlDatabase>      // 1.0.238 Phase 5.2: session row in `sessions` table
#include <QSqlError>
#include <QSqlQuery>
#include <QMutex>
#include <QMutexLocker>
#include <QList>
#include <QPointer>
#include <QProcess>
#include <QRunnable>
#include <QTimeZone>
#include <QtEndian>
#include <QTimer>
#include <QScopeGuard>
#include <QThread>
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QOperatingSystemVersion>
#include "Network/FoxVerifier.hpp"
#include "wsjtx_config.h"
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>
#include <QSysInfo>
#include <QTextStream>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include "CwSidetone.h"
#include "CwTiming.h"
#include <QBuffer>
#include <algorithm>
#include <QMediaDevices>
#include <QRandomGenerator>
#include <QDebug>
#include <QDesktopServices>
#include <QDataStream>
#include <QUrl>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QScopedValueRollback>
#include <functional>
#include <memory>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <complex>
#include <chrono>
#include <cwchar>
#include <limits>
#include <thread>
#include <utility>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#if defined(_MSC_VER)
#pragma comment(lib, "pdh.lib")
#endif
#else
#include <sys/resource.h>
#include <sys/time.h>
#endif
#include "Ft2LogBridgeLauncher.h"
#ifdef Q_OS_MAC
#include <mach/mach.h>
#include <mach/task_info.h>
#endif
#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef FFTW3_SINGLE_FOUND
#include <fftw3.h>
#endif
#include <vector>
#if DECODIUM_HAS_RTTY
// Il piano di banda RTTY: una tabella sola per il menu, la finestra RTTY e
// il QSY che accompagna il cambio di modo. Riscriverla qui vorrebbe dire
// avere due elenchi che prima o poi discordano.
#include "app/BandPlan.h"
#endif

// La porta dell'amplificatore non deve mai essere quella del CAT.
bool decodiumBridgeAmplifierPortClashes(DecodiumTransceiverManager* cat,
                                         const QString& port);

// Il backend "native" (QSerialPort) e' stato rimosso: vedi CatNativeMigration.h.
// Si fa una volta sola, prima che i manager leggano le proprie impostazioni.
void decodiumBridgeMigrateRemovedNativeCatBackend();

// Registro dei QSO: le decisioni del sequencer (avvio, passaggi, tentativi,
// log o abbandono) in un file al giorno che NON ruota, tenuto 30 giorni. Il
// log diagnostico ruota a 5 MB x 3 e copre poche ore: i QSO persi del
// 16-22/9/2026 non si sono potuti spiegare perche' il log di quei giorni non
// c'era piu'. DECODIUM_QSO_JOURNAL=0 lo spegne.
bool decodiumBridgeQsoJournalWanted(const QString&);
void decodiumBridgeQsoJournal(const QString&);

void decodiumBridgeUiActionLog(const QString&);
#define bridgeLog decodiumBridgeUiActionLog

void decodiumBridgeStartLogUiStallProbe(QObject*, const QString&);

void decodiumBridgeRetireAudioSink(QAudioSink*, QBuffer*, const QString&);

int decodiumBridgeFtAutoUiReserveForCores(int);
void decodiumBridgeApplyFtOpenMpSchedulerProfile();
#ifdef Q_OS_WIN
void decodiumBridgeApplyWindowsInteractiveSchedulingProfile();
#endif

// BRIDGE_SHARED_RUNTIME_INSERTION_POINT

bool decodiumBridgeResolveAudioDeviceByIdentity(const QList<QAudioDevice>&, const QString&, const QString&, QAudioDevice*, QString*);

using decodium::decode_message::stripLegacyDecodeAppendage;
using decodium::decode_message::stripDecodeApAnnotation;
using decodium::decode_message::canonicalDecodeMessage;

bool shouldAcceptDecodeEntryByUserFilters(const QVariantMap& entry,
                                                 const DecodeUserFilterConfig& filters,
                                                 bool waitPounceScope,
                                                 QString* reason = nullptr);

// 1.0.538 iu8lmc - la frequenza entra nella chiave in forma canonica
// (intero). Prima due formattazioni diverse dello stesso hertz davano due
// chiavi distinte, e soprattutto non si potevano costruire le chiavi vicine
// per il confronto a tolleranza usato piu' avanti.

// 1.0.495 — step A3 strangler: famiglia parsing token/messaggi spostata
// VERBATIM in Sequencer/MessageTokenRules.cpp (condivisa desktop/mobile).
using decodium::seq::isGridTokenStrict;
using decodium::seq::normalizeCallToken;
using decodium::seq::isPlaceholderCallToken;
using decodium::seq::isDirectedCqModifierToken;
using decodium::seq::isStrictAmateurCallsignToken;
using decodium::seq::isSpecialEventStyleCallsignToken;
using decodium::seq::isPlausibleDecodedCallsignToken;
using decodium::seq::decodedDxCallToken;
using decodium::seq::normalizedUsableCallToken;
using decodium::seq::normalizedBaseCall;
using decodium::seq::tokenMatchesCall;
using decodium::seq::normalizedMessageTokens;
using decodium::seq::messageContainsCallToken;
using decodium::seq::messageIsSentByCall;
using decodium::seq::directedPeerTokenFromMessage;
using decodium::seq::messageCarries73Payload;
using decodium::seq::messageCarries73PayloadForCall;

// 1.0.373 - ghost strutturale FT2/FT8: token "R" NUDO (singola lettera) seguito da un
// grid valido (es. "CALL1 CALL2 R MK86", "CQ CALL/P R LD05"). Nessun tipo-messaggio
// 77-bit standard produce "R <GRID>" (l'exchange con R e' "CALL CALL R-NN", mai "R GRID";
// un CQ valido e' "CQ CALL GRID" senza R). Tipico falso-positivo LDPC a SNR basso con
// callsign compound spazzatura. Sicuro: NON confonde R-12/R+05/Rdd (token interi != "R")
// ne' RR73 col token "R" nudo. Default-ON: un TX conforme non lo genera mai.

// Sanity gate per callsign — rifiuta i ghost del decoder LDPC.
// Pattern ITU-R 19.68: prefisso (1-2 lettere | lettera+cifra | cifra 2-9 + lettera)
// + cifra (0-9) + suffisso (1-4 lettere). Esempi rifiutati: 0Z4SYH, 0L0MYK,
// L74PVK, 6H9IV5Y0. Esempi accettati: IU8LMC, K1JT, 9A3A, EA5JMN, A29TTX/P.
// Strippa suffissi portatili comuni (/P /M /MM /AM /A /R /QRP) prima del check.

QString decodiumBridgeNormalizedVisualTxTimeToken(const QString&, const QString&);

// BRIDGE_AUDIO_ADAPTER_INSERTION_POINT
// BRIDGE_TX_FORMAT_ADAPTER_INSERTION_POINT

// 1.0.364+ — MAM multi-stream nativo (FASE 1). Genera N stream FTx (uno per
// messaggio/frequenza), li somma campione-per-campione e peak-normalizza il
// risultato (stessa logica di normalize_wave/foxcom). Usato solo quando
// multiStreamActive() e' true; abort su qualunque errore di codifica.
// 1.0.365+ — parametrico per modo: FT8/FT4/FT2 condividono questo path
// (encoder + generatore d'onda + nsym/nsps presi da buildTxWaveformForMessage).
// I parametri d'onda DEVONO restare allineati a buildTxWaveformForMessage.
// 1.0.495 — corpo spostato VERBATIM in Sequencer/QsoSequencerRules.cpp
// (Fase 1 port mobile, step A strangler): regole pure condivise desktop/mobile.
using decodium::seq::deferredSignoffRetryCapForMode;

bool decodiumBridgeLegacyRigControlFallback(DecodiumLegacyBackend*, const QString&);

bool decodiumBridgeIsHamlibFamilyBackend(QString const& backend);

// Quando la radio remota fa da CAT, i punti di comando qui sotto devono mandare
// i comandi a lei invece che a un manager locale che non e' connesso. Sono
// funzioni libere di file, chiamate da decine di posti: passare il collegamento
// a ognuna vorrebbe dire toccarle tutte. Il bridge e' uno solo, e questo
// puntatore vive quanto la modalita' che lo accende.

// =====================================================================
// 1.0.143 fase 2 — replica C++ dei filter QML JS che alimentavano
// bandActivityModel / rxDecodeModel. Vedi DecodeWindow.qml:
//   isTelemetryHexToken (linea 328)
//   isTelemetryOnlyDecodeMessage (linea 337)
//   shouldDisplayDecodeEntry (linea 383)
//   filteredDecodeEntries (linea 391)
//   rxEntryBelongsToCurrentQso (linea 450)
//   isAtRxFrequency (linea 439)
//   sortedRxDecodes + rxSortKey (linea 506)
// =====================================================================

// BRIDGE_TX_POLICY_ADAPTER_INSERTION_POINT

// 1.0.584: single place where the worked-station switches are read, so the
// four display pipelines cannot drift apart.  "Bypass Filters" clears them all.
// 1.0.145: detection ghost decode = decoder false positives a SNR molto bassi.
// 1.0.270+: non filtrare piu' sul solo valore SNR: FT8/FT4/FT2 legittimi
// possono arrivare a -24/-26 dB e devono restare visibili. Manteniamo solo
// segnali strutturali di decode corrotto: callsign/token impossibili,
// mismatch call/grid e AP sospetta fuori QSO attivo.
// Le entry TX, MyCall, e quelle marked dxIsWorked/isB4 sono ESCLUSE dal
// filter (TX/MyCall sono certi, B4 sono stazioni note di cui abbiamo già
// log → se SNR è weak è plausibile sia davvero loro).

// 1.0.257 — Auto-clear a soglia per evitare che le finestre decode crescano
// all'infinito. Le due liste restano indipendenti. Signal RX contiene anche
// righe TX locali: quelle non sono decode RX e non devono far scattare il
// reset delle ricezioni precedenti.

// BRIDGE_CONSTRUCTOR_INSERTION_POINT

// BRIDGE_SERVICES_INSERTION_POINT

// === STATION ===

// ---------------------------------------------------------------------------
// Worked-before tracking (per band / DXCC / zone / grid)
// ---------------------------------------------------------------------------

// BRIDGE_Q65_ACCESSOR_INSERTION_POINT

// BRIDGE_RX_MONITORING_INSERTION_POINT

// BRIDGE_CAT_PTT_STATE_INSERTION_POINT

// BRIDGE_CAT_DISPATCH_SSTV_INSERTION_POINT

// === AUDIO ===

QString decodiumBridgeLegacyPaletteNameForUiIndex(int);

// BRIDGE_PALETTE_INSERTION_POINT

// === DECODE ===

// === TX MESSAGES ===
// =====================================================================
// === TARGET CALL feature (fork-only 1.0.262, iu8lmc)               ===
// Chiamata diretta verso uno specifico callsign con missed-target/timeout,
// coesiste con autoCqRepeat (CALL ha priorita' se attivo).
// =====================================================================
// === CAT ===

// -- DecoPort ---------------------------------------------------------------
// I tre oggetti nascono alla prima richiesta dal QML. Il gateway riceve dei
// ganci verso la radio invece di un backend: cosi' non sa, e non deve sapere,
// se sotto ci sia il nativo, Hamlib, OmniRig o TCI.
// BRIDGE_CAT_PERSISTENCE_INSERTION_POINT

// Aprire la radio da soli e' quello che rende DecoPort indipendente dal CAT
// dell'applicazione. Il modello non lo si chiede a nessuno: l'identita' USB dice
// che radio e', e il catalogo che Hamlib espone a runtime dice come si parla.
//
// L'unico caso in cui non si apre e' quando la porta e' gia' in mano al CAT di
// questa applicazione: due programmi sulla stessa seriale non sono due
// programmi sulla stessa seriale, sono nessuno dei due. Li' si continua a
// passare dai ganci, che e' come funzionava prima.
// Avvia il calcolo del hub in background; il risultato arriva via decoPortHubChanged.
// La chiave vive nello store come esadecimale. Chi legge il file non ottiene la
// password: per risalirci dovrebbe rifare 200000 giri di PBKDF2 per ogni
// tentativo. Non e' un caveau, e' una serratura ragionevole per una rete locale.
// L'audio della radio remota entra nello stesso buffer che riempirebbe la
// scheda locale. Il decoder, la cascata e l'S-meter non sanno da dove venga, ed
// e' esattamente il punto: da qui in poi e' una radio come le altre.
// L'ascolto usa la stessa meccanica di riproduzione della ricezione RTL-SDR —
// e' una coda verso un QAudioSink su un thread suo — ma con la propria istanza:
// mescolare i due ascolti vorrebbe dire che spegnerne uno spegne l'altro.
// La forma d'onda esce da qui verso la radio remota. Non si spara tutta in una
// volta: dodici secondi di FT8 sono trecento e passa datagrammi, e rovesciarli
// insieme sulla rete e' il modo di perderne. Se ne mandano avanti un secondo e
// mezzo alla volta, ognuno con l'ora esatta in cui deve suonare: il gateway li
// trattiene fino a quell'istante, cosi' il jitter della rete finisce in un
// buffer invece che sul confine di slot.
// Il PTT chiesto da un client. Il pericolo qui non e' alzarlo: e' non riabbassarlo.
// Se il client muore, se cade la rete, se il suo audio finisce a meta', la radio
// resta in aria a portante vuota finche' qualcuno non se ne accorge. Percio' il
// PTT alzato da remoto porta con se' una scadenza, che l'audio riprodotto
// rinnova: quando l'audio smette, entro tre secondi il PTT scende da solo.
// L'audio che il client vuole mandare in aria, gia' arrivato all'istante in cui
// va suonato: il gateway l'ha trattenuto fino a qui apposta. Va nel codec USB
// della radio, cioe' nello stesso dispositivo che userebbe la trasmissione
// locale.
// Il verso opposto: la radio remota si e' mossa (l'ha girata qualcuno, o un
// altro client) e l'applicazione deve seguirla. Il modo NON si specchia: quello
// della radio e' DIGU/USB, quello dell'applicazione e' FT8 o FT4, e sono due
// cose diverse che si somigliano solo nel nome.
// Passare alla radio remota vuol dire fermare la scheda locale: due sorgenti
// nello stesso buffer si mescolerebbero in rumore.
// BRIDGE_SPOT_SHARE_INSERTION_POINT

// Lettura dell'amplificatore. Come per la CAT condivisa, le impostazioni
// passano dallo stesso archivio di tutte le altre.
// Rifiuta la porta gia' usata dal CAT. Non e' pignoleria: in modo
// interrogante finirebbero sei byte al ricetrasmettitore, che potrebbe
// leggerli come un comando; e anche in sola lettura aprire quella porta la
// toglierebbe al controllo della radio. Meglio un rifiuto che una diagnosi
// difficile con la radio in aria.
// CAT condivisa. Le impostazioni passano dallo stesso archivio di tutte le
// altre: un secondo percorso di lettura e scrittura e' gia' costato caro in
// passato, con interruttori che risultavano accesi e non avevano effetto.
// 1.0.179 — Ritorna true se negli ultimi windowMs c'e' stato uno stall
// (gap) >= thresholdMs. Riusa il pressure tracker esistente: se
// m_cpuPressureUntilMs e' nel futuro entro windowMs, c'era pressure
// recente. Conservative: meglio fallback eccessivo che decode block.
// === SETTINGS ===
// BRIDGE_LEGACY_PREFLIGHT_INSERTION_POINT
// BRIDGE_LEGACY_AUDIO_INSERTION_POINT

// === QML INVOKABLE ACTIONS ===

// BRIDGE_TX_OUTPUT_DEVICE_INSERTION_POINT

// BRIDGE_MODULATOR_FINISH_INSERTION_POINT

// ===========================================================================

// Restituisce QAudioDevice corrispondente a m_audioOutputDevice (o il default)
// 1.0.364+ — MAM multi-stream nativo (FASE 1).
// Test puro: genera il multi-stream wave dagli argomenti diretti (NON usa
// m_mamMessages) e lo scrive come WAV mono 16-bit. Per analisi offline.
// ============================================================================
// 1.0.364+ - MAM multi-stream nativo (FASE 2): sequencer multi-QSO paralleli.
// Modello MSHV "rispondi sulla freq del chiamante". TUTTO gated da
// mamMultiStreamSequencerActive(); con il toggle OFF nessuna di queste funzioni
// viene mai invocata e il sequencer single-QSO esistente resta byte-identico.
// Le funzioni riusano SOLO primitivi puri (parsing/inferenza/codifica messaggi)
// e NON chiamano setDxCall/setReportSent/setCurrentTx/advanceQsoState/
// regenerateTxMessages: lo stato per-slot vive in m_mamSlots, isolato dai
// membri single-QSO (m_dxCall/m_qsoProgress/m_currentTx/...).
// ============================================================================

// FASE 3 - cap stream simultanei MAM (clamp 1..10 da 1.0.569; era 2..5).
// 1 = risposta a un chiamante per volta ma sulla SUA frequenza (modello MSHV),
// 10 = pileup pieno da DX-pedition. Persistito nello store canonico Decodium3;
// riletto da loadSettings al prossimo avvio.
// Costruisce il messaggio TX dello slot dal suo stato. PURA: usa
// standardFtxQsoMessage (file-static) come il single-QSO genStdMsgs.
// Parsa un decode FT8 (campi [time,snr,dt,df,message,aptype,quality,freq]) e,
// se diretto al mio call, lo instrada allo slot del partner (creandone uno se
// arriva una grid e ci sono slot liberi). NON emette segnali ne' tocca lo stato
// single-QSO.
// Rimuove gli slot conclusi o scaduti. Timeout: troppi retry sullo stesso step
// oppure nessun ascolto da >6 periodi.
// Promuove i caller in coda a slot attivi finche' c'e' capacita'. Usa la coda
// m_callerQueue (entry "BASECALL freq snr") gia' alimentata da
// maybeEnqueueMamCallerFromMessage in caso di overflow.
// Logga il QSO di uno slot riusando logQsoNow() via snapshot pending, SENZA
// prompt UI. Popola i campi m_pendingAutoLog* dai dati dello slot (come fa
// capturePendingAutoLogSnapshot per il single-QSO) e li ripulisce dopo.
// ---------------------------------------------------------------------------
// Ciclo elastico della Fox FT2 (opt-in: Ft2ElasticFox / DECODIUM_FT2_ELASTIC_FOX).
// Invece di trasmettere ogni secondo slot (7,5 s), la Fox riparte appena tutte
// le stazioni a cui ha scritto hanno risposto, o appena e' chiaro che non
// risponderanno (tempi appresi per stazione, come il timeout di TCP). La logica
// di decisione e' in FoxElasticCycle.h (testata); qui c'e' solo il collegamento.
// Il confine di periodo resta come ripiego: se l'elastico non scatta, dopo 6,5 s
// dalla fine del TX la griglia riprende il controllo.
// ---------------------------------------------------------------------------
// BRIDGE_COMPLETE_TX_INSERTION_POINT

// BRIDGE_START_TX_INSERTION_POINT
// BRIDGE_STOP_TX_INSERTION_POINT

// 1.0.623 — watchdog del Tune. Il Tune del bridge rigenera il tono ogni 9,8 s
// e non finisce mai da solo: chi lo lasciava acceso teneva la radio in
// trasmissione a tempo indeterminato. Il front-end legacy aveva gia' questa
// protezione (widgets/mainwindow.cpp, tuneATU_Timer) e le sue impostazioni,
// TuneWatchdog (acceso di default) e TuneWatchdogTime (secondi, 90), qui non
// venivano lette da nessuno.
// BRIDGE_TUNE_INSERTION_POINT

// ─────────────────────────────────────────────────────────────────────────────
// 1.0.324 — ALC AUTO-CALIBRATION (Fase 2)
// ─────────────────────────────────────────────────────────────────────────────

// ── catBackend switch ─────────────────────────────────────────────────────────
// ---------------------------------------------------------------------------
// Legacy INI sync helpers
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Standalone UDP MessageClient for WSJT-X protocol
// ---------------------------------------------------------------------------

// BRIDGE_SHUTDOWN_LAB_INSERTION_POINT

// Riordina m_callerQueue per punteggio decrescente (stabile: a parita' resta l'ordine di arrivo).
// Spento di default; l'ordine scelto a mano dall'utente non viene mai toccato.
// === advanceQsoState — GitHub TxController clone ===
// Mappa TX button number → QSOProgress enum, resetta watchdog
// 0=IDLE, 1=CALLING_CQ, 2=REPLYING, 3=REPORT, 4=ROGER_REPORT, 5=SIGNOFF, 6=IDLE_QSO
// 1.0.447 - Fase 2: matrice "best knowledge" delle transizioni di stato FT2 legittime
// (osservato dal census loopback+on-air U noto-dai-log/verifica avversariale). Opera sul txNum
// EFFETTIVO (post-remap QuickQSO/Tx1-disabled): il census mostro' che il grezzo 2->1 con QuickQSO
// e' in realta' 2->2. Include i 3 falsi-reject scoperti (6->{3,4,5} deferred-after-CQ 1.0.359,
// 2->4 fast-signoff anche q0, 2->1 risolto dal post-remap) + coda 5->2 e recovery 4/5->2/3 (note
// legittime dalla verifica). NON e' enforcement: oggi solo would-reject (log-only) per scoprire le
// celle mancanti prima di promuovere a guard attivo.
// 1.0.446 - P2-8: reset stato QSO per la sequenza successiva. Estratto byte-identico dal
// blocco duplicato nelle 2 lambda finishAutoSequenceQso (checkAndStartPeriodicTx +
// autoSequenceStep). preserveAutoTx=false => spegne il TX. Reset SNR partner = P2-9.
// BRIDGE_LAB_INSERTION_POINT

// Il CW come audio parte dallo stesso percorso di ogni altra trasmissione:
// con una radio locale esce dalla scheda audio, con una radio remota
// (DecoPort o Decolink) passa dal collegamento. Per questo qui si guarda la
// strada che c'e' adesso, non soltanto la scheda locale.
// SSB voice workspace.

QString bridgeAdifRecordText(const QString& dxCall, const QString& dxGrid,
                                    double freqHz, const QString& mode,
                                    const QDateTime& timeOnUtc,
                                    const QDateTime& timeOffUtc,
                                    const QString& rstSent, const QString& rstRcvd,
                                    const QString& myCall, const QString& myGrid,
                                    const QString& comments = QString(),
                                    const QString& propMode = QString(),
                                    const QString& satellite = QString(),
                                    const QString& satMode = QString(),
                                    const QString& freqRx = QString(),
                                    const QString& name = QString(),
                                    const QString& qth = QString(),
                                    int cqZone = 0,
                                    int ituZone = 0);

// BRIDGE_LOG_QSO_INSERTION_POINT

// === Shannon FDR (False Decode Reduction) — 4-level filter ===

namespace {
using decodium::decode_tokens::isGridToken;
using decodium::decode_tokens::looksLikeCallsignToken;

}

// Estrae il secondo callsign dal messaggio (quello a destra).
// "CQ IU8LMC JN70" → IU8LMC (primo call dopo CQ)
// "IU8LMC 9H1SR -05" → 9H1SR (secondo call)
// "9H1SR IU8LMC R-05" → IU8LMC (secondo call)
// BRIDGE_DECODE_ADAPTER_INSERTION_POINT

// BRIDGE_UTC_TIMER_INSERTION_POINT

// === PRIVATE METHODS ===

// ============================================================
// A4 — Fox/Hound caller queue management
// ============================================================

// mainwindow: processNextInQueue() — prende il prossimo caller dalla coda
// Chiamato dopo QSO completato in multi-answer mode
// ============================================================
// B9 — Active Stations model accessor
// ============================================================

// ============================================================
// C15 — QSY: cambio frequenza + modo in un'unica chiamata QML
// ============================================================

// ============================================================
// B6 — cty.dat: scarica se mancante o vecchio di >30 giorni
// ============================================================

// ============================================================
// B8 — Alert sounds: playback tramite DecodiumAlertManager
// ============================================================

// ============================================================
// WAV decode: decodifica un singolo file WAV o un'intera cartella
// ============================================================

// ============================================================
// B11 — Cabrillo export dal log ADIF salvato
// ============================================================

// ============================================================
// C16 — UpdateChecker: controlla releases su GitHub
// ============================================================

// ============================================================
// C17 — OTP Fox verification: valida codice Fox via 9dx.cc
// ============================================================

// ============================================================
// ADIF import/export + workedCalls B4 check
// ============================================================

// ============================================================
// US state data + LotW lite
// ============================================================
