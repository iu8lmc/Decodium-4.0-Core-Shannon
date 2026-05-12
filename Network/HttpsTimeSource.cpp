// -*- Mode: C++ -*-
#include "HttpsTimeSource.hpp"

#include <QDateTime>
#include <QLocale>
#include <QTimeZone>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace
{
    double medianOf(QVector<double> values)
    {
        if (values.isEmpty()) return 0.0;
        std::sort(values.begin(), values.end());
        int const n = values.size();
        if ((n % 2) == 0) {
            return (values[n / 2 - 1] + values[n / 2]) / 2.0;
        }
        return values[n / 2];
    }

    // Parse "Tue, 13 May 2026 00:30:45 GMT" → ms since epoch.
    qint64 parseHttpDateMs(QString const& header)
    {
        if (header.trimmed().isEmpty()) return 0;
        // RFC 7231 IMF-fixdate: "Sun, 06 Nov 1994 08:49:37 GMT"
        QLocale const locale(QLocale::C);
        QDateTime dt = locale.toDateTime(
            header.trimmed(),
            QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
        if (dt.isValid()) {
            dt.setTimeZone(QTimeZone::UTC);
            return dt.toMSecsSinceEpoch();
        }
        // Fallback Qt parser tollerante
        dt = QDateTime::fromString(header.trimmed(), Qt::RFC2822Date);
        if (dt.isValid()) {
            return dt.toMSecsSinceEpoch();
        }
        return 0;
    }
}

HttpsTimeSource::HttpsTimeSource(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_endpoints({
        // Endpoint HTTPS che restituiscono `Date:` header valido.
        // L'ordine NON conta: tutte le richieste partono in parallelo.
        QStringLiteral("https://www.google.com/generate_204"),
        QStringLiteral("https://www.cloudflare.com/cdn-cgi/trace"),
        QStringLiteral("https://www.microsoft.com/"),
        QStringLiteral("https://1.1.1.1/"),
        QStringLiteral("https://www.amazon.com/")
    })
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &HttpsTimeSource::onReply);

    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout,
            this, &HttpsTimeSource::onRefreshTimeout);
}

HttpsTimeSource::~HttpsTimeSource() = default;

void HttpsTimeSource::setEnabled(bool on)
{
    if (m_enabled == on) return;
    m_enabled = on;
    if (m_enabled) {
        // Trigger immediato + schedula refresh successivo.
        QTimer::singleShot(0, this, [this]() { launchOneQueryCycle(); });
    } else {
        m_refreshTimer.stop();
        // Abort eventuali pending requests
        for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
            QNetworkReply* r = it.key();
            if (r) r->abort();
        }
        m_pending.clear();
        m_collected.clear();
        m_inFlight = 0;
        if (m_synced) {
            m_synced = false;
            emit syncStatusChanged(false, QStringLiteral("disabled"));
        }
    }
}

void HttpsTimeSource::syncNow()
{
    if (!m_enabled) return;
    launchOneQueryCycle();
}

void HttpsTimeSource::onRefreshTimeout()
{
    if (m_enabled) launchOneQueryCycle();
}

void HttpsTimeSource::launchOneQueryCycle()
{
    if (!m_nam) return;
    if (m_inFlight > 0) {
        // Cycle gia' in corso: non lanciarne un altro sopra.
        return;
    }
    m_collected.clear();

    for (QString const& url : m_endpoints) {
        QNetworkRequest req{QUrl(url)};
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium-DecoSyncTime/1.0"));
        // HEAD: ci serve solo il Date header, niente body.
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        // TLS default config (gli endpoint sono tutti HTTPS pubblici).
        qint64 const t1 = QDateTime::currentMSecsSinceEpoch();
        QNetworkReply* reply = m_nam->head(req);
        if (!reply) continue;

        m_pending.insert(reply, PendingRequest{t1});
        ++m_inFlight;

        // Watchdog per timeout — se non risponde entro kRequestTimeoutMs
        // lo abortiamo cosi' non resta appeso.
        QTimer::singleShot(kRequestTimeoutMs, reply, [reply]() {
            if (reply && reply->isRunning()) reply->abort();
        });
    }
}

void HttpsTimeSource::onReply(QNetworkReply* reply)
{
    if (!reply) return;
    auto pendingIt = m_pending.find(reply);
    if (pendingIt == m_pending.end()) {
        reply->deleteLater();
        return;
    }
    PendingRequest const pending = pendingIt.value();
    m_pending.erase(pendingIt);
    if (m_inFlight > 0) --m_inFlight;

    qint64 const t2 = QDateTime::currentMSecsSinceEpoch();
    double const rttMs = static_cast<double>(t2 - pending.t1Ms);

    bool sampleAccepted = false;
    QString rejectReason;
    if (reply->error() != QNetworkReply::NoError) {
        rejectReason = reply->errorString();
    } else if (rttMs > kMaxRttMs) {
        rejectReason = QStringLiteral("rtt>%1ms").arg(kMaxRttMs);
    } else {
        QVariant const headerVar = reply->header(QNetworkRequest::LastModifiedHeader);
        // QNetworkRequest::LastModifiedHeader e' "Last-Modified" non "Date";
        // Date header e' raw → leggiamolo dal rawHeader.
        QByteArray const dateHeader = reply->rawHeader(QByteArrayLiteral("Date"));
        qint64 const serverMs = parseHttpDateMs(QString::fromLatin1(dateHeader));
        if (serverMs > 0) {
            // Stima del server time al tempo t2:
            //   server_at_t2 = server_at_response_sent + half_rtt_back
            //   = serverMs + RTT/2
            // offset = server_at_t2 - t2
            double const offsetMs =
                static_cast<double>(serverMs) + rttMs / 2.0 - static_cast<double>(t2);
            if (std::isfinite(offsetMs) && std::abs(offsetMs) < kMaxOffsetMs) {
                m_collected.append({offsetMs, rttMs});
                sampleAccepted = true;
            } else {
                rejectReason = QStringLiteral("offset out of range");
            }
        } else {
            rejectReason = QStringLiteral("no Date header");
        }
    }

    if (!sampleAccepted && !rejectReason.isEmpty()) {
        emit errorOccurred(
            QStringLiteral("HttpsTime %1: %2")
                .arg(reply->url().host(), rejectReason));
    }

    reply->deleteLater();

    // Se questo era l'ultimo reply del cycle, processiamo i risultati.
    if (m_inFlight == 0 && m_pending.isEmpty()) {
        if (m_collected.isEmpty()) {
            if (m_synced) {
                m_synced = false;
                emit syncStatusChanged(false, QStringLiteral("no valid replies"));
            }
            m_refreshTimer.start(kRetryIntervalMs);
            return;
        }
        QVector<double> offs;
        QVector<double> rtts;
        offs.reserve(m_collected.size());
        rtts.reserve(m_collected.size());
        for (auto const& s : m_collected) {
            offs.append(s.offsetMs);
            rtts.append(s.rttMs);
        }
        double const medOffset = medianOf(offs);
        double const medRtt    = medianOf(rtts);

        m_offsetMs = medOffset;
        m_lastRttMs = medRtt;
        m_lastEndpointCount = m_collected.size();
        m_collected.clear();

        if (!m_synced) {
            m_synced = true;
            emit syncStatusChanged(true,
                QStringLiteral("synced offset=%1ms rtt=%2ms n=%3")
                    .arg(QString::number(m_offsetMs, 'f', 1),
                         QString::number(m_lastRttMs, 'f', 0),
                         QString::number(m_lastEndpointCount)));
        }
        emit offsetUpdated(m_offsetMs, m_lastRttMs, m_lastEndpointCount);
        m_refreshTimer.start(kRefreshIntervalMs);
    }
}
