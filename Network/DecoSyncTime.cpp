// -*- Mode: C++ -*-
#include "DecoSyncTime.hpp"
#include "NtpClient.hpp"
#include "HttpsTimeSource.hpp"

#include <QDateTime>

#include <cmath>

DecoSyncTime::DecoSyncTime(QObject *parent)
    : QObject(parent)
    , m_ntp(new NtpClient(this))
    , m_https(new HttpsTimeSource(this))
{
    // Fase 2: NTP UDP e HTTPS Date girano IN PARALLELO. La combinazione
    // privilegia NTP quando synced (offset piu' preciso, RTT misurabile),
    // altrimenti cade su HTTPS che funziona anche dietro firewall.
    connect(m_ntp, &NtpClient::offsetUpdated, this, [this](double) {
        recomputeCombinedOffset(QStringLiteral("ntp offset"));
    });
    connect(m_ntp, &NtpClient::syncStatusChanged, this,
        [this](bool synced, QString const& statusText) {
            Q_UNUSED(synced)
            recomputeCombinedOffset(QStringLiteral("ntp sync ") + statusText);
        });
    connect(m_ntp, &NtpClient::errorOccurred, this, &DecoSyncTime::errorOccurred);

    connect(m_https, &HttpsTimeSource::offsetUpdated, this,
        [this](double, double, int) {
            recomputeCombinedOffset(QStringLiteral("https offset"));
        });
    connect(m_https, &HttpsTimeSource::syncStatusChanged, this,
        [this](bool, QString const& statusText) {
            recomputeCombinedOffset(QStringLiteral("https sync ") + statusText);
        });
    connect(m_https, &HttpsTimeSource::errorOccurred, this,
        &DecoSyncTime::errorOccurred);

    // Default ENABLED; il consumer (DecodiumBridge) chiama setEnabled con
    // il valore della QSettings; default ON su nuova install.
    m_ntp->setEnabled(true);
    m_https->setEnabled(true);
}

DecoSyncTime::~DecoSyncTime() = default;

bool DecoSyncTime::synced() const
{
    if (m_ntp && m_ntp->isSynced()) return true;
    if (m_https && m_https->isSynced()) return true;
    return false;
}

double DecoSyncTime::offsetMs() const
{
    // Combinatore: preferisce NTP se synced (piu' preciso), altrimenti HTTPS.
    if (m_ntp && m_ntp->isSynced()) {
        return m_ntp->offsetMs();
    }
    if (m_https && m_https->isSynced()) {
        return m_https->offsetMs();
    }
    return 0.0;
}

int DecoSyncTime::lastServerCount() const
{
    int total = 0;
    if (m_ntp) total += m_ntp->lastServerCount();
    if (m_https) total += m_https->lastEndpointCount();
    return total;
}

bool   DecoSyncTime::httpsSynced()    const { return m_https && m_https->isSynced(); }
double DecoSyncTime::httpsOffsetMs()  const { return m_https ? m_https->offsetMs() : 0.0; }
int    DecoSyncTime::httpsLastCount() const { return m_https ? m_https->lastEndpointCount() : 0; }

void DecoSyncTime::setEnabled(bool on)
{
    if (m_enabled == on) return;
    m_enabled = on;
    if (m_ntp) m_ntp->setEnabled(on);
    if (m_https) m_https->setEnabled(on);
    emit enabledChanged();
    recomputeCombinedOffset(on ? QStringLiteral("enabled") : QStringLiteral("disabled"));
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
    if (m_https) m_https->syncNow();
}

void DecoSyncTime::recomputeCombinedOffset(QString const& reason)
{
    Q_UNUSED(reason)
    bool const nowSynced = synced();
    double const nowOffset = offsetMs();

    bool const syncedChanged = (nowSynced != m_lastEmittedSynced);
    bool const offsetChanged =
        std::abs(nowOffset - m_lastEmittedOffsetMs) >= 0.5
        || (syncedChanged && nowSynced);

    if (syncedChanged) {
        m_lastEmittedSynced = nowSynced;
        QString const status = nowSynced
            ? QStringLiteral("synced (combined ntp+https)")
            : QStringLiteral("not synced");
        emit this->syncedChanged(nowSynced, status);
    }
    if (offsetChanged) {
        m_lastEmittedOffsetMs = nowOffset;
        emit offsetUpdated(nowOffset);
    }
}
