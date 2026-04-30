#include "DecodiumPskReporterLite.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QDateTime>
#include <QByteArray>
#include <QUrl>
#include <QDebug>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

DecodiumPskReporterLite::DecodiumPskReporterLite(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_flushTimer(new QTimer(this))
{
    connect(m_flushTimer, &QTimer::timeout,
            this, &DecodiumPskReporterLite::onFlushTimer);

    connect(m_nam, &QNetworkAccessManager::finished,
            this, &DecodiumPskReporterLite::onReplyFinished);

    m_flushTimer->setInterval(FLUSH_INTERVAL_MS);
    m_flushTimer->start();
}

DecodiumPskReporterLite::~DecodiumPskReporterLite()
{
    // Nothing extra – Qt parent ownership cleans up m_nam and m_flushTimer.
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void DecodiumPskReporterLite::setEnabled(bool v)
{
    if (m_enabled == v) return;
    m_enabled = v;
    if (v) {
        // Reset failure state when re-enabling so the LED does not stay
        // orange indefinitely after a transient outage.
        m_autoDisabled = false;
        m_consecutiveFailures = 0;
    }
    emit connectedChanged();
}

void DecodiumPskReporterLite::setLocalStation(const QString& callsign,
                                               const QString& grid,
                                               const QString& programInfo)
{
    m_myCall      = callsign.trimmed().toUpper();
    m_myGrid      = grid.trimmed().toUpper();
    m_programInfo = programInfo.isEmpty()
                        ? QStringLiteral("Decodium4")
                        : programInfo;
}

void DecodiumPskReporterLite::addSpot(const QString& call,
                                       const QString& grid,
                                       quint64 freqHz,
                                       const QString& mode,
                                       int snr)
{
    if (!m_enabled) return;

    Spot s;
    s.call   = call.trimmed().toUpper();
    s.grid   = grid.trimmed().toUpper();
    s.freqHz = freqHz;
    s.mode   = mode.trimmed().toUpper();
    s.snr    = snr;
    s.time   = QDateTime::currentDateTimeUtc();

    m_spots.append(s);
}

void DecodiumPskReporterLite::sendReport(bool last)
{
    if (last) {
        m_flushTimer->stop();
    }

    if (m_spots.isEmpty()) return;
    if (!m_enabled)        return;
    if (m_myCall.isEmpty()) {
        qWarning() << "[PskReporterLite] sendReport: local callsign not set, skipping.";
        return;
    }

    const QByteArray payload = buildXmlPayload();

    // Keep track of how many spots are being submitted in this POST.
    m_pendingCount = m_spots.size();

    // Clear the buffer *before* sending so that spots added while the network
    // round-trip is in progress are queued for the next batch.
    m_spots.clear();

    QNetworkRequest request{QUrl{QString::fromLatin1(PSK_REPORTER_URL)}};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QByteArrayLiteral("application/xml"));
    request.setRawHeader("User-Agent", m_programInfo.toUtf8());

    m_nam->post(request, payload);
    // Reply is handled asynchronously in onReplyFinished().
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void DecodiumPskReporterLite::onFlushTimer()
{
    sendReport(false);
}

void DecodiumPskReporterLite::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    auto handleFailure = [this](const QString& msg) {
        const bool wasOk = m_lastSendOk;
        m_lastSendOk = false;
        if (wasOk) emit connectedChanged();

        ++m_consecutiveFailures;
        if (m_consecutiveFailures >= MAX_CONSECUTIVE_FAILURES && !m_autoDisabled) {
            m_autoDisabled = true;
            m_enabled = false;
            const QString note = QStringLiteral(
                "PSK Reporter Lite auto-disabled after %1 consecutive failures. "
                "The main IPFIX uploader still works.").arg(m_consecutiveFailures);
            qWarning() << "[PskReporterLite]" << note;
            emit errorOccurred(note);
        } else if (!m_autoDisabled) {
            emit errorOccurred(msg);
        }
    };

    if (reply->error() != QNetworkReply::NoError) {
        const QString msg = reply->errorString();
        qWarning() << "[PskReporterLite] HTTP error:" << msg;
        handleFailure(msg);
        return;
    }

    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (statusCode == 200) {
        const bool wasOk = m_lastSendOk;
        m_lastSendOk = true;
        m_consecutiveFailures = 0;
        if (!wasOk) emit connectedChanged();

        emit spotsUploaded(m_pendingCount);
        m_pendingCount = 0;

        qDebug() << "[PskReporterLite] Upload OK, spots sent:" << m_pendingCount;
    } else {
        const QString msg =
            QStringLiteral("PSK Reporter returned HTTP %1").arg(statusCode);
        qWarning() << "[PskReporterLite]" << msg;
        handleFailure(msg);
    }
}

// ---------------------------------------------------------------------------
// XML payload builder
// ---------------------------------------------------------------------------

QByteArray DecodiumPskReporterLite::buildXmlPayload() const
{
    // flowStartSeconds = Unix timestamp of the earliest spot in the batch.
    // If the buffer is empty this function should not be called, but guard
    // defensively.
    if (m_spots.isEmpty()) return {};

    const qint64 flowStart = m_spots.first().time.toSecsSinceEpoch();

    QString xml;
    xml.reserve(512 + m_spots.size() * 200);

    // XML declaration + root element opening tag
    xml += QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<receptionReport xmlns=\"http://www.pskreporter.info/schema\""
        " senderCallsign=\"%1\""
        " senderLocator=\"%2\""
        " flowStartSeconds=\"%3\">\n")
        .arg(m_myCall.toHtmlEscaped(),
             m_myGrid.toHtmlEscaped(),
             QString::number(flowStart));

    // Sender information element (required by the schema)
    xml += QStringLiteral(
        "  <senderInformation name=\"%1\" noActive=\"1\"/>\n")
        .arg(m_programInfo.toHtmlEscaped());

    // One <receptionReport> child element per spot
    for (const Spot& s : m_spots) {
        const qint64 spotTs = s.time.toSecsSinceEpoch();

        xml += QStringLiteral(
            "  <receptionReport"
            " callsign=\"%1\""
            " locator=\"%2\""
            " frequency=\"%3\""
            " mode=\"%4\""
            " sNR=\"%5\""
            " timeSeconds=\"%6\""
            " flowStartSeconds=\"%7\"/>\n")
            .arg(s.call.toHtmlEscaped(),
                 s.grid.toHtmlEscaped(),
                 QString::number(s.freqHz),
                 s.mode.toHtmlEscaped(),
                 QString::number(s.snr),
                 QString::number(spotTs),
                 QString::number(flowStart));
    }

    xml += QStringLiteral("</receptionReport>\n");

    return xml.toUtf8();
}
