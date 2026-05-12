// -*- Mode: C++ -*-
#include "DecoSyncTime.hpp"
#include "NtpClient.hpp"

#include <QDateTime>

DecoSyncTime::DecoSyncTime(QObject *parent)
    : QObject(parent)
    , m_ntp(new NtpClient(this))
{
    // Inoltra signals NtpClient verso i nostri consumer.
    connect(m_ntp, &NtpClient::offsetUpdated, this, &DecoSyncTime::offsetUpdated);
    connect(m_ntp, &NtpClient::syncStatusChanged, this, &DecoSyncTime::syncedChanged);
    connect(m_ntp, &NtpClient::errorOccurred, this, &DecoSyncTime::errorOccurred);

    // Fase 1: parte ENABLED. Il consumer (DecodiumBridge) chiama setEnabled
    // con il valore della QSettings; se quel valore e' false, lo spegne.
    // Ma il default su nuova install e' true.
    m_ntp->setEnabled(true);
}

DecoSyncTime::~DecoSyncTime() = default;

bool DecoSyncTime::synced() const
{
    return m_ntp && m_ntp->isSynced();
}

double DecoSyncTime::offsetMs() const
{
    return m_ntp ? m_ntp->offsetMs() : 0.0;
}

int DecoSyncTime::lastServerCount() const
{
    return m_ntp ? m_ntp->lastServerCount() : 0;
}

void DecoSyncTime::setEnabled(bool on)
{
    if (m_enabled == on) return;
    m_enabled = on;
    if (m_ntp) m_ntp->setEnabled(on);
    emit enabledChanged();
}

void DecoSyncTime::setCustomServer(QString const& server)
{
    if (m_ntp) m_ntp->setCustomServer(server);
}

void DecoSyncTime::setRefreshInterval(int ms)
{
    if (m_ntp) m_ntp->setRefreshInterval(ms);
}

void DecoSyncTime::setMaxRtt(double ms)
{
    if (m_ntp) m_ntp->setMaxRtt(ms);
}

void DecoSyncTime::setInitialOffset(double offsetMs)
{
    if (m_ntp) m_ntp->setInitialOffset(offsetMs);
}

qint64 DecoSyncTime::wallclockMs() const
{
    qint64 const systemMs = QDateTime::currentMSecsSinceEpoch();
    // Quando enabled+synced, applichiamo l'offset misurato. Altrimenti,
    // ritorniamo il system clock raw (degrado graceful).
    if (m_enabled && synced()) {
        double const off = offsetMs();
        if (std::isfinite(off)) {
            return systemMs + static_cast<qint64>(off);
        }
    }
    return systemMs;
}

void DecoSyncTime::syncNow()
{
    if (m_ntp) m_ntp->syncNow();
}
