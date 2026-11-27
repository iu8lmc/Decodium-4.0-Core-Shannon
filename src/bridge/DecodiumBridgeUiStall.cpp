#include "DecodiumBridge.h"
#include "DecodiumLogging.hpp"
#include <QDateTime>

bool DecodiumBridge::isUiStallActive(int thresholdMs, int windowMs) const
{
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_cpuPressureUntilMs > nowMs - windowMs && m_cpuPressureUntilMs > 0) {
        Q_UNUSED(thresholdMs);  // thresholdMs gia' implicito nella detection
        return true;
    }
    return false;
}

void DecodiumBridge::updateUiStallDiagnostics()
{
    if (!m_uiStallClock.isValid()) {
        m_uiStallClock.start();
        m_lastUiStallTickMs = 0;
        return;
    }

    qint64 const nowMonoMs = m_uiStallClock.elapsed();
    if (!mainWindowRenderActive()) {
        // A hidden/minimised QQuickWindow may suspend its event loop. Reset
        // the baseline so that intentional non-presentation is never fed to
        // the CPU/GPU pressure guards as a UI freeze.
        m_lastUiStallTickMs = nowMonoMs;
        m_recentShortStalls.clear();
        return;
    }
    qint64 const gapMs = m_lastUiStallTickMs > 0 ? nowMonoMs - m_lastUiStallTickMs : 0;
    m_lastUiStallTickMs = nowMonoMs;
    // 1.0.178 — Relax UI stall thresholds: il valore 360/700/10000 era
    // troppo aggressivo su PC modesti, attivava cpuPressureActive
    // cronicamente strozzando FT8 early predecode e FT2 async dispatch.
    // Nuove soglie 600ms (was 360) / 1100ms severe (was 700) / 4s window
    // (was 10000) lasciano respirare il pipeline decode su short stall.
    if (gapMs < 360) {
        return;
    }

    qint64 const nowWallMs = QDateTime::currentMSecsSinceEpoch();

    // 1.0.186 — Adaptive short-stall tracker. Stall corti (360-599ms) singoli
    // sono benigni, ma 4+ in 5s indicano pressione cumulativa sul main thread
    // (es. drag handle SplitView + drain ListView + texture upload). Scatta
    // cpuPressure mild anche senza superare la soglia singola 600ms.
    if (m_monitoring || m_transmitting || m_tuning) {
        if (gapMs >= 360 && gapMs < 600) {
            m_recentShortStalls.append(nowWallMs);
            qint64 const windowMs = 5000;
            while (!m_recentShortStalls.isEmpty()
                   && m_recentShortStalls.first() < nowWallMs - windowMs) {
                m_recentShortStalls.removeFirst();
            }
            if (m_recentShortStalls.size() >= 4) {
                noteCpuPressure(QStringLiteral("UI short-stall burst %1 in 5s")
                                    .arg(m_recentShortStalls.size()),
                                4000,
                                false);
                m_recentShortStalls.clear();  // evita re-trigger immediato
            }
        }
    }

    if ((m_monitoring || m_transmitting || m_tuning) && gapMs >= 600) {
        noteCpuPressure(QStringLiteral("UI stall %1ms").arg(gapMs),
                        4000,
                        gapMs >= 1100);
    }

    if (nowWallMs - m_lastUiStallLogMs < 500) {
        return;
    }
    m_lastUiStallLogMs = nowWallMs;

    int const periodMs = periodMsForMode(m_mode);
    int const slotMs = periodMs > 0
        ? static_cast<int>(nowWallMs % static_cast<qint64>(periodMs))
        : -1;

    DIAG_WARN(QStringLiteral("[UIDBG] main-thread stall gap_ms=%1 mode=%2 slot_ms=%3 direct_visual=%4 legacy_pcm=%5 pan_busy=%6 monitoring=%7 tx=%8 tune=%9 decoding=%10")
                  .arg(gapMs)
                  .arg(m_mode)
                  .arg(slotMs)
                  .arg(useModernSpectrumFeedWithLegacy() ? 1 : 0)
                  .arg(m_legacyPcmSpectrumFeed ? 1 : 0)
                  .arg(m_panadapterComputeBusy.load() ? 1 : 0)
                  .arg(m_monitoring ? 1 : 0)
                  .arg(m_transmitting ? 1 : 0)
                  .arg(m_tuning ? 1 : 0)
                  .arg(m_decoding ? 1 : 0));
}

