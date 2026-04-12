#include "DecodiumCloudlogLite.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QUrl>
#include <QRegularExpression>
#include <QDebug>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

// Formats an ADIF field: <TAG:LEN>VALUE
QString adifField(const QString& tag, const QString& value)
{
    return QStringLiteral("<%1:%2>%3 ").arg(tag).arg(value.size()).arg(value);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

DecodiumCloudlogLite::DecodiumCloudlogLite(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

DecodiumCloudlogLite::~DecodiumCloudlogLite() = default;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void DecodiumCloudlogLite::setApiUrl(const QString& url)
{
    // Normalise: strip trailing slash so we can always append /api/...
    m_apiUrl = url;
    if (m_apiUrl.endsWith(QLatin1Char('/')))
        m_apiUrl.chop(1);
}

void DecodiumCloudlogLite::setApiKey(const QString& key)
{
    m_apiKey = key;
}

void DecodiumCloudlogLite::setStationId(int id)
{
    m_stationId = id;
}

// ---------------------------------------------------------------------------
// bandFromHz
// ---------------------------------------------------------------------------

QString DecodiumCloudlogLite::bandFromHz(double freqHz)
{
    // Frequency boundaries taken from the ITU/ARRL band plan.
    // The comparisons are ordered from lowest to highest.
    if (freqHz <   2'000'000.0) return QStringLiteral("160M");
    if (freqHz <   4'000'000.0) return QStringLiteral("80M");
    if (freqHz <   8'000'000.0) return QStringLiteral("40M");
    if (freqHz <  11'000'000.0) return QStringLiteral("30M");
    if (freqHz <  15'000'000.0) return QStringLiteral("20M");
    if (freqHz <  19'000'000.0) return QStringLiteral("17M");
    if (freqHz <  22'000'000.0) return QStringLiteral("15M");
    if (freqHz <  25'000'000.0) return QStringLiteral("12M");
    if (freqHz <  30'000'000.0) return QStringLiteral("10M");
    if (freqHz <  52'000'000.0) return QStringLiteral("6M");
    if (freqHz < 150'000'000.0) return QStringLiteral("2M");
    if (freqHz < 440'000'000.0) return QStringLiteral("70CM");
    return QStringLiteral("UNK");
}

// ---------------------------------------------------------------------------
// buildAdifRecord
// ---------------------------------------------------------------------------

QString DecodiumCloudlogLite::buildAdifRecord(const QString& dxCall,
                                               const QString& dxGrid,
                                               double         freqHz,
                                               const QString& mode,
                                               const QDateTime& utcTime,
                                               int            snr,
                                               const QString& reportSent,
                                               const QString& reportRcvd,
                                               const QString& myCall,
                                               const QString& myGrid) const
{
    QString adif;

    // CALL – DX station callsign
    adif += adifField(QStringLiteral("CALL"), dxCall);

    // BAND – derived from frequency
    adif += adifField(QStringLiteral("BAND"), bandFromHz(freqHz));

    // FREQ – in MHz, 6 decimal places
    const double freqMHz = freqHz / 1'000'000.0;
    adif += adifField(QStringLiteral("FREQ"),
                      QString::number(freqMHz, 'f', 6));

    // MODE
    adif += adifField(QStringLiteral("MODE"), mode);

    // QSO_DATE – YYYYMMDD (UTC)
    adif += adifField(QStringLiteral("QSO_DATE"),
                      utcTime.toUTC().toString(QStringLiteral("yyyyMMdd")));

    // TIME_ON – HHmmss (UTC)
    adif += adifField(QStringLiteral("TIME_ON"),
                      utcTime.toUTC().toString(QStringLiteral("HHmmss")));

    // RST_SENT / RST_RCVD
    // Use explicit report fields if provided; fall back to SNR string.
    const QString rstSent = reportSent.isEmpty()
                                ? QString::number(snr)
                                : reportSent;
    const QString rstRcvd = reportRcvd.isEmpty()
                                ? QString::number(snr)
                                : reportRcvd;
    adif += adifField(QStringLiteral("RST_SENT"), rstSent);
    adif += adifField(QStringLiteral("RST_RCVD"), rstRcvd);

    // GRIDSQUARE – DX grid
    if (!dxGrid.isEmpty())
        adif += adifField(QStringLiteral("GRIDSQUARE"), dxGrid);

    // MY_GRIDSQUARE – own grid (Cloudlog extension, widely supported)
    if (!myGrid.isEmpty())
        adif += adifField(QStringLiteral("MY_GRIDSQUARE"), myGrid);

    // OPERATOR / STATION_CALLSIGN – own callsign
    if (!myCall.isEmpty())
        adif += adifField(QStringLiteral("STATION_CALLSIGN"), myCall);

    // End-of-record marker
    adif += QStringLiteral("<EOR>");

    return adif;
}

// ---------------------------------------------------------------------------
// logQso
// ---------------------------------------------------------------------------

void DecodiumCloudlogLite::logQso(const QString& dxCall,
                                   const QString& dxGrid,
                                   double         freqHz,
                                   const QString& mode,
                                   const QDateTime& utcTime,
                                   int            snr,
                                   const QString& reportSent,
                                   const QString& reportRcvd,
                                   const QString& myCall,
                                   const QString& myGrid)
{
    if (!m_enabled) {
        qDebug() << "[CloudlogLite] logQso called but disabled – skipping.";
        return;
    }

    if (m_apiUrl.isEmpty() || m_apiKey.isEmpty()) {
        emit errorOccurred(tr("Cloudlog API URL or API key not configured."));
        return;
    }

    const QString adif = buildAdifRecord(dxCall, dxGrid, freqHz, mode,
                                          utcTime, snr,
                                          reportSent, reportRcvd,
                                          myCall, myGrid);

    // Build JSON body
    QJsonObject body;
    body[QStringLiteral("key")]                = m_apiKey;
    body[QStringLiteral("station_profile_id")] = QString::number(m_stationId);
    body[QStringLiteral("type")]               = QStringLiteral("adif");
    body[QStringLiteral("string")]             = adif;

    const QByteArray jsonBytes = QJsonDocument(body).toJson(QJsonDocument::Compact);

    // Build request
    QNetworkRequest request(QUrl(m_apiUrl + QStringLiteral("/api/qso")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    QNetworkReply* reply = m_nam->post(request, jsonBytes);

    // Capture dxCall by value for use in the lambda
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, dxCall]() {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError) {
                    emit errorOccurred(
                        tr("Cloudlog network error: %1").arg(reply->errorString()));
                    return;
                }

                const int httpStatus =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

                if (httpStatus == 200) {
                    qDebug() << "[CloudlogLite] QSO logged for" << dxCall;
                    emit qsoLogged(dxCall);
                } else {
                    emit errorOccurred(
                        tr("Cloudlog returned HTTP %1 for QSO logging.").arg(httpStatus));
                }
            });
}

// ---------------------------------------------------------------------------
// testApi
// ---------------------------------------------------------------------------

void DecodiumCloudlogLite::testApi()
{
    if (m_apiUrl.isEmpty() || m_apiKey.isEmpty()) {
        emit errorOccurred(tr("Cloudlog API URL or API key not configured."));
        return;
    }

    const QString urlStr =
        m_apiUrl + QStringLiteral("/api/auth?key=") + m_apiKey;

    QNetworkRequest request{QUrl(urlStr)};
    QNetworkReply* reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError) {
                    emit errorOccurred(
                        tr("Cloudlog test error: %1").arg(reply->errorString()));
                    return;
                }

                // Parse the response body looking for "status":"auth"
                // We intentionally avoid QJsonDocument here and use a regex
                // to keep the parsing self-contained and dependency-light.
                const QString body = QString::fromUtf8(reply->readAll());

                // Match: "status" : "auth"  (spaces optional around colon)
                static const QRegularExpression reAuth(
                    QStringLiteral(R"("status"\s*:\s*"auth")"),
                    QRegularExpression::CaseInsensitiveOption);

                if (reAuth.match(body).hasMatch()) {
                    qDebug() << "[CloudlogLite] API key OK.";
                    emit apiKeyOk();
                } else {
                    qDebug() << "[CloudlogLite] API key invalid. Response:" << body;
                    emit apiKeyInvalid();
                }
            });
}
