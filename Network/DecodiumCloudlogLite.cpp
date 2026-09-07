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

constexpr qint64 kMaxCloudlogReplyBytes = 64 * 1024;

// Formats an ADIF field: <TAG:LEN>VALUE
QString adifField(const QString& tag, const QString& value)
{
    return QStringLiteral("<%1:%2>%3 ").arg(tag).arg(value.size()).arg(value);
}

QUrl cloudlogEndpoint(QString base, const QString& endpointPath, QString* error = nullptr)
{
    base = base.trimmed();
    if (base.isEmpty()) {
        if (error)
            *error = QObject::tr("Cloudlog URL is empty.");
        return {};
    }

    if (!base.contains(QStringLiteral("://")))
        base.prepend(QStringLiteral("https://"));

    QUrl url(base);
    if (!url.isValid() || url.host().isEmpty()) {
        if (error)
            *error = QObject::tr("Cloudlog URL is invalid: %1").arg(base);
        return {};
    }

    const QString scheme = url.scheme().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
        if (error)
            *error = QObject::tr("Cloudlog URL scheme must be http or https.");
        return {};
    }

    QString path = url.path();
    path.replace(QRegularExpression(QStringLiteral("/+$")), QString());
    path.replace(QRegularExpression(QStringLiteral("(?i)/(?:index\\.php/)?api(?:/.*)?$")), QString());
    path.replace(QRegularExpression(QStringLiteral("(?i)/index\\.php$")), QString());
    path.replace(QRegularExpression(QStringLiteral("/+$")), QString());

    url.setPath(path + QStringLiteral("/index.php/api/") + endpointPath);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

QNetworkRequest makeCloudlogRequest(const QUrl& endpoint)
{
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("User-Agent", "Decodium Cloudlog API");
    return request;
}

QString readLimitedReply(QNetworkReply* reply)
{
    if (!reply)
        return {};

    const QVariant lengthHeader = reply->header(QNetworkRequest::ContentLengthHeader);
    if (lengthHeader.isValid() && lengthHeader.toLongLong() > kMaxCloudlogReplyBytes)
        return QObject::tr("[reply too large]");

    const QByteArray bytes = reply->read(kMaxCloudlogReplyBytes + 1);
    if (bytes.size() > kMaxCloudlogReplyBytes || !reply->atEnd())
        return QObject::tr("[reply exceeds limit]");

    return QString::fromUtf8(bytes);
}

QString replyPreview(QString body)
{
    body = body.trimmed();
    body.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    if (body.size() > 300)
        body = body.left(300) + QStringLiteral("...");
    return body;
}

QString cloudlogReplyReason(const QString& body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body.toUtf8());
    if (document.isObject()) {
        const QJsonObject object = document.object();
        const QStringList keys {
            QStringLiteral("reason"),
            QStringLiteral("message"),
            QStringLiteral("error"),
            QStringLiteral("status")
        };
        for (const QString& key : keys) {
            const QString value = object.value(key).toString().trimmed();
            if (!value.isEmpty())
                return value;
        }
    }

    if (body.contains(QStringLiteral("<status>Valid</status>"), Qt::CaseInsensitive))
        return QStringLiteral("Valid");
    if (body.contains(QStringLiteral("<status>Invalid</status>"), Qt::CaseInsensitive))
        return QStringLiteral("Invalid");

    return replyPreview(body);
}

bool cloudlogReplyLooksRejected(const QString& body)
{
    const QString lower = body.toLower();
    if (lower.contains(QStringLiteral("missing api key"))
        || lower.contains(QStringLiteral("invalid api key"))
        || lower.contains(QStringLiteral("not authorized"))
        || lower.contains(QStringLiteral("unauthorized"))
        || lower.contains(QStringLiteral("<status>invalid</status>"))) {
        return true;
    }

    const QJsonDocument document = QJsonDocument::fromJson(body.toUtf8());
    if (!document.isObject())
        return false;

    const QJsonObject object = document.object();
    const QString status = object.value(QStringLiteral("status")).toString().trimmed().toLower();
    const QString result = object.value(QStringLiteral("result")).toString().trimmed().toLower();
    const QString reason = object.value(QStringLiteral("reason")).toString().trimmed().toLower();
    return status == QStringLiteral("error")
        || status == QStringLiteral("failed")
        || status == QStringLiteral("invalid")
        || result == QStringLiteral("error")
        || result == QStringLiteral("failed")
        || reason.contains(QStringLiteral("missing api key"))
        || reason.contains(QStringLiteral("invalid api key"));
}

QString cloudlogHttpErrorMessage(const QString& context,
                                 QNetworkReply::NetworkError networkError,
                                 const QString& networkErrorText,
                                 int httpStatus,
                                 const QString& body)
{
    const QString detail = cloudlogReplyReason(body);
    const QString detailSuffix = detail.isEmpty()
        ? QString()
        : QObject::tr(" Risposta: %1").arg(detail);

    if (networkError == QNetworkReply::AuthenticationRequiredError || httpStatus == 401) {
        return QObject::tr("%1: HTTP 401. Il server o proxy richiede autenticazione prima dell'API Cloudlog. Controlla URL, Basic/Auth/Cloudflare o protezioni su /index.php/api; l'API key Cloudlog non basta.%2")
            .arg(context, detailSuffix);
    }

    if (networkError == QNetworkReply::ProxyAuthenticationRequiredError || httpStatus == 407) {
        return QObject::tr("%1: HTTP 407. Il proxy richiede autenticazione prima di raggiungere Cloudlog.%2")
            .arg(context, detailSuffix);
    }

    if (httpStatus == 403) {
        return QObject::tr("%1: HTTP 403. Accesso negato dal server Cloudlog; verifica URL, permessi API key e protezioni web.%2")
            .arg(context, detailSuffix);
    }

    if (httpStatus > 0) {
        return QObject::tr("%1: HTTP %2.%3")
            .arg(context)
            .arg(httpStatus)
            .arg(detailSuffix);
    }

    return QObject::tr("%1: network error: %2.%3")
        .arg(context, networkErrorText, detailSuffix);
}

enum class CloudlogApiKeyState
{
    invalid,
    readOnly,
    writable,
};

CloudlogApiKeyState parseCloudlogApiTestReply(const QString& body)
{
    if (body.contains(QStringLiteral("<status>Valid</status>"), Qt::CaseInsensitive)) {
        return body.contains(QStringLiteral("<rights>rw</rights>"), Qt::CaseInsensitive)
            ? CloudlogApiKeyState::writable
            : CloudlogApiKeyState::readOnly;
    }

    const QJsonDocument document = QJsonDocument::fromJson(body.toUtf8());
    if (document.isObject()) {
        const QJsonObject object = document.object();
        const QString status = object.value(QStringLiteral("status")).toString().trimmed().toLower();
        const QString reason = object.value(QStringLiteral("reason")).toString().trimmed().toLower();

        if (status == QStringLiteral("ok")
            || status == QStringLiteral("success")
            || status == QStringLiteral("valid")) {
            return CloudlogApiKeyState::writable;
        }
        if (reason.contains(QStringLiteral("station profile id"))) {
            return CloudlogApiKeyState::writable;
        }
        if (reason.contains(QStringLiteral("missing api key"))
            || reason.contains(QStringLiteral("invalid api key"))) {
            return CloudlogApiKeyState::invalid;
        }
    }

    if (body.contains(QStringLiteral("station profile id"), Qt::CaseInsensitive)
        || body.contains(QStringLiteral("missing fields"), Qt::CaseInsensitive)) {
        return CloudlogApiKeyState::writable;
    }

    return CloudlogApiKeyState::invalid;
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
    m_apiUrl = url.trimmed();
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

    QString urlError;
    const QUrl endpoint = cloudlogEndpoint(m_apiUrl, QStringLiteral("qso"), &urlError);
    if (!endpoint.isValid()) {
        emit errorOccurred(urlError);
        return;
    }

    QNetworkRequest request = makeCloudlogRequest(endpoint);
    qDebug().noquote() << "[CloudlogLite] POST" << endpoint.toString(QUrl::RemoveUserInfo)
                       << "station_profile_id=" << m_stationId;

    QNetworkReply* reply = m_nam->post(request, jsonBytes);

    // Capture dxCall by value for use in the lambda
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, dxCall]() {
                const QNetworkReply::NetworkError networkError = reply->error();
                const QString networkErrorText = reply->errorString();
                const int httpStatus =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QString body = readLimitedReply(reply);

                reply->deleteLater();

                if (networkError != QNetworkReply::NoError) {
                    emit errorOccurred(cloudlogHttpErrorMessage(
                        tr("Cloudlog QSO upload"),
                        networkError,
                        networkErrorText,
                        httpStatus,
                        body));
                    return;
                }

                if (httpStatus >= 200 && httpStatus < 300
                    && !cloudlogReplyLooksRejected(body)) {
                    qDebug() << "[CloudlogLite] QSO logged for" << dxCall;
                    emit qsoLogged(dxCall);
                } else {
                    emit errorOccurred(cloudlogHttpErrorMessage(
                        tr("Cloudlog QSO upload"),
                        QNetworkReply::NoError,
                        QString(),
                        httpStatus,
                        body));
                }
            });
}

void DecodiumCloudlogLite::uploadAdif(const QString& dxCall,
                                      const QByteArray& adifRecord,
                                      quint32 requestId)
{
    if (!m_enabled) {
        qDebug() << "[CloudlogLite] uploadAdif called but disabled - skipping.";
        if (requestId != 0) {
            emit adifUploadFinished(requestId, dxCall, false, tr("Cloudlog disabled"));
        }
        return;
    }

    if (m_apiUrl.isEmpty() || m_apiKey.isEmpty()) {
        const QString detail = tr("Cloudlog API URL or API key not configured.");
        emit errorOccurred(detail);
        if (requestId != 0) {
            emit adifUploadFinished(requestId, dxCall, false, detail);
        }
        return;
    }

    QString adif = QString::fromUtf8(adifRecord).trimmed();
    if (adif.isEmpty()) {
        const QString detail = tr("Cloudlog ADIF record is empty.");
        emit errorOccurred(detail);
        if (requestId != 0) {
            emit adifUploadFinished(requestId, dxCall, false, detail);
        }
        return;
    }
    if (!adif.contains(QStringLiteral("<EOR>"), Qt::CaseInsensitive)) {
        adif += QStringLiteral(" <EOR>");
    }

    QJsonObject body;
    body[QStringLiteral("key")] = m_apiKey;
    body[QStringLiteral("station_profile_id")] = QString::number(m_stationId);
    body[QStringLiteral("type")] = QStringLiteral("adif");
    body[QStringLiteral("string")] = adif;

    QString urlError;
    const QUrl endpoint = cloudlogEndpoint(m_apiUrl, QStringLiteral("qso"), &urlError);
    if (!endpoint.isValid()) {
        emit errorOccurred(urlError);
        if (requestId != 0) {
            emit adifUploadFinished(requestId, dxCall, false, urlError);
        }
        return;
    }

    QNetworkRequest request = makeCloudlogRequest(endpoint);
    qDebug().noquote() << "[CloudlogLite] raw ADIF POST"
                       << endpoint.toString(QUrl::RemoveUserInfo)
                       << "station_profile_id=" << m_stationId;

    QNetworkReply* reply = m_nam->post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, dxCall, requestId]() {
                const QNetworkReply::NetworkError networkError = reply->error();
                const QString networkErrorText = reply->errorString();
                const int httpStatus =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QString body = readLimitedReply(reply);

                reply->deleteLater();

                if (networkError != QNetworkReply::NoError) {
                    const QString detail = cloudlogHttpErrorMessage(
                        tr("Cloudlog raw ADIF upload"),
                        networkError,
                        networkErrorText,
                        httpStatus,
                        body);
                    emit errorOccurred(detail);
                    if (requestId != 0) {
                        emit adifUploadFinished(requestId, dxCall, false, detail);
                    }
                    return;
                }

                if (httpStatus >= 200 && httpStatus < 300
                    && !cloudlogReplyLooksRejected(body)) {
                    qDebug() << "[CloudlogLite] raw ADIF logged for" << dxCall;
                    emit qsoLogged(dxCall);
                    if (requestId != 0) {
                        emit adifUploadFinished(requestId,
                                                dxCall,
                                                true,
                                                tr("Cloudlog accepted raw ADIF"));
                    }
                } else {
                    const QString detail = cloudlogHttpErrorMessage(
                        tr("Cloudlog raw ADIF upload"),
                        QNetworkReply::NoError,
                        QString(),
                        httpStatus,
                        body);
                    emit errorOccurred(detail);
                    if (requestId != 0) {
                        emit adifUploadFinished(requestId, dxCall, false, detail);
                    }
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

    QString urlError;
    const QUrl endpoint = cloudlogEndpoint(m_apiUrl, QStringLiteral("qso"), &urlError);
    if (!endpoint.isValid()) {
        emit errorOccurred(urlError);
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("key"), m_apiKey);
    payload.insert(QStringLiteral("station_profile_id"), QStringLiteral("0"));
    payload.insert(QStringLiteral("type"), QStringLiteral("adif"));
    payload.insert(QStringLiteral("string"), QStringLiteral("<eor>"));

    QNetworkRequest request = makeCloudlogRequest(endpoint);
    qDebug().noquote() << "[CloudlogLite] test POST"
                       << endpoint.toString(QUrl::RemoveUserInfo)
                       << "station_profile_id=0 configured_station_profile_id=" << m_stationId;

    QNetworkReply* reply = m_nam->post(
        request, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() {
                const QNetworkReply::NetworkError networkError = reply->error();
                const QString networkErrorText = reply->errorString();
                const int httpStatus =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QString body = readLimitedReply(reply);

                reply->deleteLater();

                if (networkError != QNetworkReply::NoError) {
                    emit errorOccurred(cloudlogHttpErrorMessage(
                        tr("Cloudlog API test"),
                        networkError,
                        networkErrorText,
                        httpStatus,
                        body));
                    return;
                }

                if (httpStatus < 200 || httpStatus >= 300) {
                    emit errorOccurred(cloudlogHttpErrorMessage(
                        tr("Cloudlog API test"),
                        QNetworkReply::NoError,
                        QString(),
                        httpStatus,
                        body));
                    return;
                }

                const CloudlogApiKeyState state = parseCloudlogApiTestReply(body);
                if (state == CloudlogApiKeyState::writable) {
                    qDebug() << "[CloudlogLite] API key OK.";
                    emit apiKeyOk();
                } else if (state == CloudlogApiKeyState::readOnly) {
                    qDebug() << "[CloudlogLite] API key read-only.";
                    emit errorOccurred(tr("Cloudlog API key valida ma senza permessi di scrittura."));
                } else {
                    qDebug() << "[CloudlogLite] API key invalid. Response:" << replyPreview(body);
                    emit apiKeyInvalid();
                }
            });
}
