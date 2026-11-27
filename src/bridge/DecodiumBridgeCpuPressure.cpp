#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumProfileSettings.h"
#include "Detector/FT8DecodeWorker.hpp"

#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>
#include <QSettings>
#include <QThread>
#include <QTimer>

int decodiumBridgeMaxFtDecodeThreads();
int decodiumBridgeFtAutoUiReserveForCores(int);
int decodiumBridgeAutoFtThreadCountForCores(int);
int decodiumBridgeAdaptiveInteractiveFtThreadCount(int);
void decodiumBridgeApplyFtOpenMpThreadLimit(int);
void decodiumBridgeCpuPressureLog(const QString&);

bool DecodiumBridge::cpuPressureActive() const
{
    return QDateTime::currentMSecsSinceEpoch() < m_cpuPressureUntilMs;
}

bool DecodiumBridge::cpuPressureSevereActive() const
{
    return QDateTime::currentMSecsSinceEpoch() < m_cpuPressureSevereUntilMs;
}

int DecodiumBridge::effectiveFtThreadLimit() const
{
    int const cores = qMax(1, QThread::idealThreadCount());
    int const configured = qBound(1, m_ftThreads, decodiumBridgeMaxFtDecodeThreads());
    // P0 (1.0.409): riserva UI ANCHE col conteggio thread MANUALE. Senza, un decode
    // deep+AP con omp_set_num_threads(cores) + omp_set_dynamic(0) satura TUTTI i core
    // logici per ~12s e affama il main thread GUI -> freeze totale della UI (root-cause
    // agente-rx: CPU starvation OpenMP, non contesa mutex). Il ramo AUTO gia' riserva
    // via autoFtThreadCountForCores; allineiamo il manuale cappando a cores-riserva.
    int const uiReserveCap = qMax(1, cores - decodiumBridgeFtAutoUiReserveForCores(cores));
    int const normalLimit = m_ftThreadsAuto
        ? decodiumBridgeAutoFtThreadCountForCores(cores)
        : qMin(configured, uiReserveCap);
    int const pressureLimit = cores <= 2 ? 1
                            : qMin(normalLimit, qMax(2, cores / 2));
    int const severeLimit = cores <= 2 ? 1
                          : qMin(normalLimit, 2);
    if (cpuPressureSevereActive()) {
        return severeLimit;
    }
    if (m_lowCpuModeEnabled && cpuPressureActive()) {
        return severeLimit;
    }
    if (m_lowCpuModeEnabled) {
        return qMin(normalLimit, 2);
    }
    if (cpuPressureActive()) {
        return pressureLimit;
    }
    return normalLimit;
}

int DecodiumBridge::effectiveFtThreadLimitForDecode() const
{
    // Embedded legacy workers share the process with the Qt Quick GUI and its
    // render thread. Preserve additional interactive headroom on every CPU
    // topology while still honoring manual and pressure-reduced limits.
    return decodiumBridgeAdaptiveInteractiveFtThreadCount(effectiveFtThreadLimit());
}

void DecodiumBridge::setMainWindowRenderActive(bool active)
{
    // Increment even when the state did not change. The event-loop watchdog
    // can resume before or after QML receives a visibility transition, so the
    // epoch is the reliable request to discard one elapsed interval.
    m_mainWindowRenderActive.store(active, std::memory_order_release);
    quint64 const epoch = m_mainWindowRenderEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;

    if (m_uiStallClock.isValid())
        m_lastUiStallTickMs = m_uiStallClock.elapsed();
    else
        m_lastUiStallTickMs = 0;
    m_recentShortStalls.clear();

    qInfo().noquote()
        << "[MAINWATCH] main window render state"
        << (active ? "active" : "suspended")
        << "epoch=" << epoch;
}

void DecodiumBridge::noteMainThreadMicroStall(qint64 deltaMs)
{
    if (!mainWindowRenderActive()
        || deltaMs < decodium::decode::Ft8MicroStallGuard::StallThresholdMs
        || !m_monitoring) {
        return;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    // Short QML/model updates are common on Windows and do not implicate the
    // scene-graph FFT. Keep the decoder thread penalty responsive, but only
    // move the panadapter off the GPU after a genuinely severe UI freeze.
    static constexpr qint64 kGpuPanadapterPauseThresholdMs = 750;
#if defined(Q_OS_WIN)
    QByteArray const windowsRhi = qgetenv("QSG_RHI_BACKEND").trimmed().toLower();
    bool const hardwareD3d11 = windowsRhi == QByteArrayLiteral("d3d11")
        && qEnvironmentVariableIntValue("QSG_RHI_PREFER_SOFTWARE_RENDERER") == 0;
    if (hardwareD3d11
        && deltaMs >= kGpuPanadapterPauseThresholdMs
        && m_forceGpuPanadapterFft.exchange(false)) {
        // Intel's D3D11 user-mode driver can enter a Present/device-loss loop
        // after a long render stall.  Re-probing custom RHI compute in that
        // session repeatedly recreated the scene graph and eventually crashed
        // inside igd10um64gen11.DLL.  Preserve the Qt scene graph, but keep the
        // panadapter on asynchronous FFTW until restart.
        m_gpuPanadapterFftStallGuard.store(true);
        m_lastGpuPanadapterProbeMs = nowMs;
        m_gpuPanadapterStableSinceMs = 0;
        m_gpuPanadapterRetryNotBeforeMs = 0;
        setGpuPanadapterFftAvailable(
            false,
            QStringLiteral("Windows D3D11 severe UI stall (%1 ms); disabled for this session")
                .arg(deltaMs));
        decodiumBridgeCpuPressureLog(QStringLiteral(
            "Panadapter GPU FFT disabled for this Windows D3D11 session after a %1 ms UI stall; async CPU FFTW fallback remains active and GPU retry is suppressed")
                      .arg(deltaMs));
    }
#endif

    // The decoder-thread pressure guard is FT8-specific.  GPU protection
    // above is deliberately mode-independent because D3D11 Present() and the
    // panadapter scene graph are shared by FT8, FT4, FT2 and the other modes.
    if (m_mode != QStringLiteral("FT8"))
        return;

    bool activated = false;
    bool guardActive = false;
    int recentStallCount = 0;
    {
        QMutexLocker locker(&m_ft8MicroStallGuardMutex);
        activated = m_ft8MicroStallGuard.recordStall(nowMs, deltaMs);
        guardActive = m_ft8MicroStallGuard.active();
        recentStallCount = m_ft8MicroStallGuard.recentStallCount();
    }
    if (guardActive
        && deltaMs >= kGpuPanadapterPauseThresholdMs
        && !m_gpuPanadapterFftStallGuard.exchange(true)) {
        m_lastGpuPanadapterProbeMs = nowMs;
        m_gpuPanadapterStableSinceMs = 0;
        m_gpuPanadapterStallRetryCount = qMin(m_gpuPanadapterStallRetryCount + 1, 4);
        qint64 const retryDelayMs = qMin<qint64>(
            30000,
            5000LL << (m_gpuPanadapterStallRetryCount - 1));
        m_gpuPanadapterRetryNotBeforeMs = nowMs + retryDelayMs;
        setGpuPanadapterFftAvailable(false,
            QStringLiteral("severe main-thread stall (%1 ms)").arg(deltaMs));
        decodiumBridgeCpuPressureLog(QStringLiteral("Panadapter GPU FFT paused after severe main-thread stall (%1 ms); async CPU fallback active, GPU retry in %2 ms")
                      .arg(deltaMs)
                      .arg(retryDelayMs));
    }
    if (activated) {
        if (legacyBackendAvailable()) {
            m_legacyBackend->setFt8DeepThreadPenalty(true);
        }
        decodiumBridgeCpuPressureLog(QStringLiteral("FT8 micro-stall guard active: bridge=0x%1 stalls=%2 windowMs=%3 thresholdMs=%4 deepThreadsPenalty=1 restoreCleanPeriods=%5 gpuPauseThresholdMs=%6")
                      .arg(reinterpret_cast<quintptr>(this), 0, 16)
                      .arg(recentStallCount)
                      .arg(decodium::decode::Ft8MicroStallGuard::ObservationWindowMs)
                      .arg(decodium::decode::Ft8MicroStallGuard::StallThresholdMs)
                      .arg(decodium::decode::Ft8MicroStallGuard::CleanPeriodsToRestore)
                      .arg(kGpuPanadapterPauseThresholdMs));
    }
}

void DecodiumBridge::noteFt8AdaptivePeriod(qint64 periodId)
{
    if (periodId < 0) {
        return;
    }

    decodium::decode::Ft8MicroStallGuard::PeriodTransition transition;
    {
        QMutexLocker locker(&m_ft8MicroStallGuardMutex);
        transition = m_ft8MicroStallGuard.notePeriod(periodId);
    }
    if (transition != decodium::decode::Ft8MicroStallGuard::PeriodTransition::Restored) {
        return;
    }

    if (legacyBackendAvailable()) {
        m_legacyBackend->setFt8DeepThreadPenalty(false);
    }
    if (m_forceGpuPanadapterFft.load()
        && m_gpuPanadapterFftStallGuard.exchange(false)) {
        // Do not blindly re-enable the GPU path: make it eligible for the
        // normal probe after the FT8 guard has observed clean periods.
        m_lastGpuPanadapterProbeMs = 0;
        m_gpuPanadapterRetryNotBeforeMs = 0;
        decodiumBridgeCpuPressureLog(QStringLiteral("Panadapter GPU FFT retry eligible after FT8 micro-stall guard restored"));
    }
    decodiumBridgeCpuPressureLog(QStringLiteral("FT8 micro-stall guard restored: %1 clean periods, normal DEEP thread budget active")
                  .arg(decodium::decode::Ft8MicroStallGuard::CleanPeriodsToRestore));
}

bool DecodiumBridge::ft4AdaptiveCpuLimitActive(qint64 nowMs) const
{
    if (nowMs <= 0) {
        nowMs = QDateTime::currentMSecsSinceEpoch();
    }
    return nowMs < m_ft4AdaptiveLimitUntilMs;
}

void DecodiumBridge::activateFt4AdaptiveCpuLimit(qint64 nowMs, const QString& reason, qint64 durationMs)
{
    if (nowMs <= 0) {
        nowMs = QDateTime::currentMSecsSinceEpoch();
    }
    durationMs = qBound<qint64>(qint64 {1000}, durationMs, qint64 {60000});
    m_ft4AdaptiveLimitUntilMs = qMax(m_ft4AdaptiveLimitUntilMs, nowMs + durationMs);

    if (nowMs - m_lastFt4AdaptiveLogMs >= 2000) {
        m_lastFt4AdaptiveLogMs = nowMs;
        decodiumBridgeCpuPressureLog(QStringLiteral("FT4 adaptive CPU limit active: until +%1ms reason=%2 streak=%3")
                      .arg(qMax<qint64>(0, m_ft4AdaptiveLimitUntilMs - nowMs))
                      .arg(reason)
                      .arg(m_ft4LateCallbackStreak));
    }
}

bool DecodiumBridge::ft4LatencyGuardActive(qint64 nowMs) const
{
    if (nowMs <= 0) {
        nowMs = QDateTime::currentMSecsSinceEpoch();
    }
    return nowMs < m_ft4LatencyGuardUntilMs;
}

void DecodiumBridge::activateFt4LatencyGuard(qint64 nowMs, const QString& reason)
{
    if (nowMs <= 0) {
        nowMs = QDateTime::currentMSecsSinceEpoch();
    }
    m_ft4LatencyGuardUntilMs = qMax(m_ft4LatencyGuardUntilMs, nowMs + qint64 {120000});

    if (nowMs - m_lastFt4LatencyGuardLogMs >= 2000) {
        m_lastFt4LatencyGuardLogMs = nowMs;
        decodiumBridgeCpuPressureLog(QStringLiteral("FT4 latency guard active: until +%1ms reason=%2")
                      .arg(qMax<qint64>(0, m_ft4LatencyGuardUntilMs - nowMs))
                      .arg(reason));
    }
}

int DecodiumBridge::effectiveSpectrumTimerIntervalMs() const
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    int const configured = qBound(10, s.value(QStringLiteral("spectrumInterval"), 20).toInt(), 500);
    if (m_lowCpuModeEnabled) {
        return qMax(configured, 500);
    }
    if (useModernSpectrumFeedWithLegacy()) {
        return configured;
    }
    return configured;
}

extern "C" void ftx_ft2_set_sync_candidates_c(int mode);
extern "C" void ftx_ft4_set_sync_candidates_c(int mode);

void DecodiumBridge::applyLowCpuRuntimeProfile(const QString& reason)
{
    // FT2 and FT4 candidates from the sync words (mode 1: first, followed by a few spectral-peak
    // candidates). Recovers signals that a strong neighbour hides in the average spectrum,
    // and costs less decoder CPU than the peak-only list, so it is on for everybody.
    ftx_ft2_set_sync_candidates_c(1);
    ftx_ft4_set_sync_candidates_c(1);
    int const ftLimit = effectiveFtThreadLimit();
    decodiumBridgeApplyFtOpenMpThreadLimit(ftLimit);

    int spectrumMs = -1;
    if (m_spectrumTimer) {
        spectrumMs = effectiveSpectrumTimerIntervalMs();
        if (m_spectrumTimer->interval() != spectrumMs) {
            m_spectrumTimer->setInterval(spectrumMs);
        }
    }

    decodiumBridgeCpuPressureLog(QStringLiteral("runtime CPU profile: low=%1 ft_threads=%2 spectrum_ms=%3 reason=%4")
                  .arg(m_lowCpuModeEnabled ? 1 : 0)
                  .arg(ftLimit)
                  .arg(spectrumMs)
                  .arg(reason));
}

void DecodiumBridge::noteCpuPressure(const QString& reason, int durationMs, bool severe)
{
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    // 1.0.189 — Telemetria sessione: incrementa contatori distinti per
    // pressione normale vs severa, emette signal Q_PROPERTY per Settings UI.
    bool const wasPressureActive = (nowMs < m_cpuPressureUntilMs);
    bool const wasSevereActive   = (nowMs < m_cpuPressureSevereUntilMs);
    if (!wasPressureActive) {
        ++m_cpuPressureEventCount;
        emit cpuPressureEventCountChanged();
    }
    if (severe && !wasSevereActive) {
        ++m_cpuPressureSevereEventCount;
        emit cpuPressureSevereEventCountChanged();
    }
    m_cpuPressureUntilMs = qMax(m_cpuPressureUntilMs,
                                nowMs + static_cast<qint64>(qMax(1000, durationMs)));
    if (severe) {
        m_cpuPressureSevereUntilMs = qMax(m_cpuPressureSevereUntilMs,
                                          nowMs + static_cast<qint64>(qMax(1000, durationMs)));
    }
    if (m_mode == QStringLiteral("FT8") && (m_monitoring || m_transmitting || m_tuning)) {
        int const cooldownSlots = severe ? 8 : 4;
        m_ft8DeepFollowupCooldownSlots = qMax(m_ft8DeepFollowupCooldownSlots, cooldownSlots);
        m_ft8PendingDeepFollowups.clear();
        m_ft8PendingSubpassHarvest.clear();
        if (severe && m_ft8Worker) {
            m_ft8Worker->cancelCurrentDecode();
        }
    }
    decodiumBridgeApplyFtOpenMpThreadLimit(effectiveFtThreadLimit());

    if (nowMs - m_lastCpuPressureLogMs < 5000) {
        return;
    }
    m_lastCpuPressureLogMs = nowMs;
    decodiumBridgeCpuPressureLog(QStringLiteral("CPU pressure guard active: %1%2, until +%3ms")
                  .arg(reason)
                  .arg(cpuPressureSevereActive() ? QStringLiteral(" severe") : QString())
                  .arg(qMax<qint64>(0, m_cpuPressureUntilMs - nowMs)));
}

