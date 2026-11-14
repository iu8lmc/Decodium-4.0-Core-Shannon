#include "DecodiumBridge.h"

#include <QTimer>
#include <algorithm>
#include <cmath>

void decodiumBridgeOverlayLog(const QString& message);

#include <atomic>
#include <chrono>

static qint64 bridgeMonotonicMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

namespace {
std::atomic<qint64> g_lastDecodeReadySlotStartMs {0};
std::atomic<qint64> g_lastDecodeReadySlotEndMs {0};
std::atomic<qint64> g_lastDecodeModelEmitStartMs {0};
std::atomic<qint64> g_lastDecodeModelEmitEndMs {0};

qint64 msSinceAtomicMark(std::atomic<qint64> const& mark)
{
    qint64 const value = mark.load(std::memory_order_relaxed);
    if (value <= 0)
        return -1;
    return bridgeMonotonicMs() - value;
}
}

void DecodiumBridge::noteDecodeReadySlotStart()
{
    g_lastDecodeReadySlotStartMs.store(bridgeMonotonicMs(), std::memory_order_relaxed);
}

void DecodiumBridge::noteDecodeReadySlotEnd()
{
    g_lastDecodeReadySlotEndMs.store(bridgeMonotonicMs(), std::memory_order_relaxed);
}

void DecodiumBridge::noteDecodeModelEmitStart()
{
    g_lastDecodeModelEmitStartMs.store(bridgeMonotonicMs(), std::memory_order_relaxed);
}

void DecodiumBridge::noteDecodeModelEmitEnd()
{
    g_lastDecodeModelEmitEndMs.store(bridgeMonotonicMs(), std::memory_order_relaxed);
}

qint64 DecodiumBridge::msSinceLastDecodeReadySlotStart()
{
    return msSinceAtomicMark(g_lastDecodeReadySlotStartMs);
}

qint64 DecodiumBridge::msSinceLastDecodeReadySlotEnd()
{
    return msSinceAtomicMark(g_lastDecodeReadySlotEndMs);
}

qint64 DecodiumBridge::msSinceLastDecodeModelEmitStart()
{
    return msSinceAtomicMark(g_lastDecodeModelEmitStartMs);
}

qint64 DecodiumBridge::msSinceLastDecodeModelEmitEnd()
{
    return msSinceAtomicMark(g_lastDecodeModelEmitEndMs);
}


void DecodiumBridge::recordFrameTimestamp()
{
    m_totalFrameSamples.fetch_add(1, std::memory_order_relaxed);
    // 1.0.252: Qt 6.11 RHI on-demand stoppa frameSwapped+afterRendering
    // dopo ~4s startup quando UI dominata da custom QQuickPaintedItem
    // (Waterfall Decodium). Frame ms timing inaffidabile - DevOverlay
    // mostra "n/a" via QML quando ringFilled==0.
    if (!m_perfFrameElapsedStarted) {
        m_perfFrameElapsed.start();
        m_perfFrameElapsedStarted = true;
        return;
    }
    qint64 const ns = m_perfFrameElapsed.nsecsElapsed();
    m_perfFrameElapsed.restart();
    double const ms = double(ns) / 1.0e6;
    m_lastFrameTimeMs = ms;
    m_frameTimeRing[m_frameTimeRingPos] = ms;
    m_frameTimeRingPos = (m_frameTimeRingPos + 1) % kPerfFrameRingSize;
    if (m_frameTimeRingFilled < kPerfFrameRingSize) ++m_frameTimeRingFilled;
}

void DecodiumBridge::noteDecodeReceived() const
{
    m_totalDecodesReceived.fetch_add(1, std::memory_order_relaxed);
    // 1.0.248: gate atomic<bool> RIMOSSO (vedi recordFrameTimestamp).
    m_decodeRateReceivedCounter.fetch_add(1, std::memory_order_relaxed);
}

void DecodiumBridge::noteDecodeCommitted()
{
    m_totalDecodesCommitted.fetch_add(1, std::memory_order_relaxed);
    // 1.0.248: gate atomic<bool> RIMOSSO.
    m_decodeRateCommittedCounter.fetch_add(1, std::memory_order_relaxed);
}

QVariantList DecodiumBridge::frameTimeSamples() const
{
    QVariantList out;
    out.reserve(m_frameTimeRingFilled);
    if (m_frameTimeRingFilled < kPerfFrameRingSize) {
        for (int i = 0; i < m_frameTimeRingFilled; ++i) {
            out.append(m_frameTimeRing[i]);
        }
    } else {
        for (int i = 0; i < kPerfFrameRingSize; ++i) {
            int const idx = (m_frameTimeRingPos + i) % kPerfFrameRingSize;
            out.append(m_frameTimeRing[idx]);
        }
    }
    return out;
}

void DecodiumBridge::setActiveRhiBackend(QString const& backend)
{
    m_activeRhiBackend = backend.isEmpty()
        ? QStringLiteral("unknown") : backend;
}

void DecodiumBridge::setDevOverlayActive(bool v)
{
    if (m_devOverlayActive == v) return;
    decodiumBridgeOverlayLog(QStringLiteral("[DevOverlay] setDevOverlayActive(%1) "
                             "totalFrames=%2 totalDecRecv=%3 totalDecComm=%4")
              .arg(v ? "true" : "false")
              .arg(m_totalFrameSamples.load())
              .arg(m_totalDecodesReceived.load())
              .arg(m_totalDecodesCommitted.load()));
    m_devOverlayActive = v;
    if (v) {
        // Reset state quando l'overlay viene mostrato: counter atomic +
        // ring buffer ripartono puliti.
        m_frameTimeRingPos = 0;
        m_frameTimeRingFilled = 0;
        m_lastFrameTimeMs = 0.0;
        m_meanFrameTimeMs = 0.0;
        m_p99FrameTimeMs = 0.0;
        m_decodeRateReceivedHz = 0.0;
        m_decodeRateCommittedHz = 0.0;
        m_perfFrameElapsedStarted = false;
        m_decodeRateReceivedCounter.store(0, std::memory_order_relaxed);
        m_decodeRateCommittedCounter.store(0, std::memory_order_relaxed);
        if (m_perfMetricsTimer == nullptr) {
            m_perfMetricsTimer = new QTimer(this);
            m_perfMetricsTimer->setObjectName(QStringLiteral("decodiumPerfMetricsTimer"));
            m_perfMetricsTimer->setInterval(250);
            connect(m_perfMetricsTimer, &QTimer::timeout, this, [this]() {
                // Calcola mean + p99 dal ring buffer.
                int const n = m_frameTimeRingFilled;
                if (n > 0) {
                    double sum = 0.0;
                    QVector<double> sorted;
                    sorted.reserve(n);
                    for (int i = 0; i < n; ++i) {
                        sum += m_frameTimeRing[i];
                        sorted.push_back(m_frameTimeRing[i]);
                    }
                    m_meanFrameTimeMs = sum / double(n);
                    std::sort(sorted.begin(), sorted.end());
                    int const idx = qMin(n - 1, int(std::ceil(0.99 * n)) - 1);
                    m_p99FrameTimeMs = sorted.at(qMax(0, idx));
                }
                // Decode rate: contatori sample-and-clear ogni 250ms (×4 → Hz).
                int const recv = m_decodeRateReceivedCounter.exchange(0, std::memory_order_relaxed);
                int const comm = m_decodeRateCommittedCounter.exchange(0, std::memory_order_relaxed);
                m_decodeRateReceivedHz  = double(recv) * 4.0;
                m_decodeRateCommittedHz = double(comm) * 4.0;
                emit perfMetricsChanged();
            });
        }
        m_perfMetricsTimer->start();
        // 1.0.243 fix: forza un primo tick a 100ms invece di aspettare i
        // 250ms standard. Con questo, dopo aver premuto Ctrl+Shift+F l'utente
        // vede subito numeri (anche se mean su 1 sample). Migliora UX
        // "Copy diagnostics" appena aperto.
        QTimer::singleShot(100, this, [this]() {
            if (m_devOverlayActive) emit perfMetricsChanged();
        });
    } else {
        if (m_perfMetricsTimer) m_perfMetricsTimer->stop();
    }
    emit devOverlayActiveChanged();
    emit perfMetricsChanged();
}

