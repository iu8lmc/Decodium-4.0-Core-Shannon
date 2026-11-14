#include "DecodiumBridge.h"
#include "DecodiumBridgeTraceScope.h"
#include <QDateTime>
#include <QScopeGuard>
#include <QTimer>

void DecodiumBridge::emitDecodeListChangedThrottled()
{
    m_decodeUiBurstUntilMs = qMax(
        m_decodeUiBurstUntilMs,
        QDateTime::currentMSecsSinceEpoch() + 900);
    MainThreadTraceScope trace(QStringLiteral("emit_decode_list_changed_throttled"),
                               QStringLiteral("decode_rows=%1 rx_rows=%2 timer_active=%3")
                                   .arg(m_decodeList.size())
                                   .arg(m_rxDecodeList.size())
                                   .arg(m_decodeListEmitTimer && m_decodeListEmitTimer->isActive() ? 1 : 0));
    // Lazy-init del timer al primo uso (after construction OK perché
    // siamo nel thread principale).
    if (m_decodeListEmitTimer == nullptr) {
        m_decodeListEmitTimer = new QTimer(this);
        m_decodeListEmitTimer->setSingleShot(true);
        // 1.0.207b — coalesce throttle raddoppiato da 250→500ms (4→2 emit/sec).
        // rebuildBandActivityModel + rebuildRxDecodeModel sono O(N) con N≤500;
        // a 4 emit/sec saturavano CPU 146% in regime stabile RX FT8 + Live Map.
        // A 2 emit/sec la latenza percepita resta sotto-soglia (decode appare
        // sempre entro 500ms dall'arrivo) ma CPU media scende del 50%.
        m_decodeListEmitTimer->setInterval(500);
        connect(m_decodeListEmitTimer, &QTimer::timeout, this, [this]() {
            if (m_decodeListEmitPending) {
                MainThreadTraceScope timeoutTrace(QStringLiteral("emit_decode_list_changed_timer"),
                                                  QStringLiteral("decode_rows=%1 rx_rows=%2")
                                                      .arg(m_decodeList.size())
                                                      .arg(m_rxDecodeList.size()));
                m_decodeListEmitPending = false;
                noteDecodeModelEmitStart();
                auto modelEmitPhaseGuard = qScopeGuard([] {
                    DecodiumBridge::noteDecodeModelEmitEnd();
                });
                emit decodeListChanged();
                if (m_decodeListEmitPending)
                    m_decodeListEmitTimer->start();
            }
        });
    }

    m_decodeListEmitPending = true;
    if (!m_decodeListEmitTimer->isActive()) {
        // Burst in corso: marca pending, sarà emesso al timeout
        m_decodeListEmitTimer->start();
    }
}

void DecodiumBridge::emitRxDecodeListChangedThrottled(bool normalizeBeforeEmit)
{
    m_decodeUiBurstUntilMs = qMax(
        m_decodeUiBurstUntilMs,
        QDateTime::currentMSecsSinceEpoch() + 900);
    MainThreadTraceScope trace(QStringLiteral("emit_rx_decode_list_changed_throttled"),
                               QStringLiteral("rx_rows=%1 timer_active=%2 normalize=%3")
                                   .arg(m_rxDecodeList.size())
                                   .arg(m_rxDecodeListEmitTimer && m_rxDecodeListEmitTimer->isActive() ? 1 : 0)
                                   .arg(normalizeBeforeEmit ? 1 : 0),
                               30);
    if (m_rxDecodeListEmitTimer == nullptr) {
        m_rxDecodeListEmitTimer = new QTimer(this);
        m_rxDecodeListEmitTimer->setObjectName(QStringLiteral("decodiumRxDecodeListEmitTimer"));
        m_rxDecodeListEmitTimer->setSingleShot(true);
        // Signal RX must feel live, but the expensive part is a full-list
        // normalize + model rebuild. One update every 250ms is responsive
        // while collapsing decode bursts into a single UI pass.
        m_rxDecodeListEmitTimer->setInterval(250);
        connect(m_rxDecodeListEmitTimer, &QTimer::timeout, this, [this]() {
            if (!m_rxDecodeListEmitPending) {
                return;
            }

            MainThreadTraceScope timeoutTrace(QStringLiteral("emit_rx_decode_list_changed_timer"),
                                              QStringLiteral("rx_rows=%1 normalize=%2")
                                                  .arg(m_rxDecodeList.size())
                                                  .arg(m_rxDecodeListNormalizePending ? 1 : 0),
                                              30);
            m_rxDecodeListEmitPending = false;
            m_rxDecodeListNormalizePending = false;
            // Dedup, parsing and chronological sort are performed by the
            // decode UI snapshot worker. The source history remains append-
            // oriented here, keeping this timer callback O(1) on the GUI
            // thread even after several minutes of operation.
            emit rxDecodeListChanged();
            if (m_rxDecodeListEmitPending) {
                m_rxDecodeListEmitTimer->start();
            }
        });
    }

    m_rxDecodeListEmitPending = true;
    m_rxDecodeListNormalizePending = m_rxDecodeListNormalizePending || normalizeBeforeEmit;
    if (!m_rxDecodeListEmitTimer->isActive()) {
        m_rxDecodeListEmitTimer->start();
    }
}

