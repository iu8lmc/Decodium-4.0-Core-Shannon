#include "DecodiumBridge.h"
#include "DecodiumDxCluster.h"
#include <QTimer>
#include <QDebug>

void decodiumBridgeDxClusterLog(const QString&);
#define bridgeLog decodiumBridgeDxClusterLog

void DecodiumBridge::initialiseDxClusterNotifications()
{
    m_dxClusterSpotsNotifyTimer = new QTimer(this);
    m_dxClusterSpotsNotifyTimer->setSingleShot(true);
    connect(m_dxClusterSpotsNotifyTimer, &QTimer::timeout, this, [this]() {
        flushDxClusterSpotsChanged();
    });
    connect(m_dxCluster, &DecodiumDxCluster::statusUpdate, this, [this](const QString& msg) {
        if (visualFeedsDeferredForTx()
            && (msg.startsWith(QStringLiteral("DX de "), Qt::CaseInsensitive)
                || msg.contains(QStringLiteral("SHOW/DX"), Qt::CaseInsensitive))) {
            return;
        }
        qDebug() << "[DxCluster]" << msg;
    });
    connect(m_dxCluster, &DecodiumDxCluster::errorOccurred, this, [this](const QString& msg) {
        qDebug() << "[DxCluster ERROR]" << msg;
        QString const lower = msg.toLower();
        if (lower.contains(QStringLiteral("autospot"))
            || lower.contains(QStringLiteral("cannot send spot"))
            || lower.contains(QStringLiteral("spot non inviato"))
            || lower.contains(QStringLiteral("spot skipped"))
            || lower.contains(QStringLiteral("spot rejected"))
            || lower.contains(QStringLiteral("show/dx did not echo"))
            || lower.contains(QStringLiteral("node accepted the command"))) {
            bridgeLog(QStringLiteral("DX Cluster spot issue suppressed from error dialog: %1").arg(msg));
            emit statusMessage(QStringLiteral("DX Cluster: %1").arg(msg));
            return;
        }
        if (lower.contains(QStringLiteral("socket error"))
            || lower.contains(QStringLiteral("timeout"))
            || lower.contains(QStringLiteral("not reachable"))
            || lower.contains(QStringLiteral("connection refused"))
            || lower.contains(QStringLiteral("host not found"))
            || lower.contains(QStringLiteral("network error"))
            || lower.contains(QStringLiteral("disconnected"))) {
            bridgeLog(QStringLiteral("DX Cluster connection issue: %1").arg(msg));
            emit statusMessage(QStringLiteral("DX Cluster: %1").arg(msg));
            return;
        }
        emit errorMessage("DX Cluster: " + msg);
    });
}
