#include "DecodiumQrzLogbookLite.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace {

constexpr auto kQrzLogbookEndpoint = "https://logbook.qrz.com/api";

QString qrzUserAgent()
{
    return QStringLiteral("Decodium/4.0 QRZLogbook");
}

QString valueFromQrzResponse(const QString& body, const QString& key)
{
    const QString wanted = key.trimmed().toUpper();
    const QStringList parts = body.split(QLatin1Char('&'), Qt::SkipEmptyParts);
    for (QString part : parts) {
        const int eq = part.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        QString name = part.left(eq).trimmed().toUpper();
        QString value = part.mid(eq + 1);
        value.replace(QLatin1Char('+'), QLatin1Char(' '));
        if (name == wanted) {
            return QUrl::fromPercentEncoding(value.toUtf8()).trimmed();
        }
    }
    return QString();
}

QString ensureAdifRecordTerminator(QString adif)
{
    if (!adif.contains(QStringLiteral("<EOR"), Qt::CaseInsensitive)) {
        adif += QStringLiteral("<EOR>");
    }
    return adif;
}

QString qrzFailureReason(const QString& body)
{
    QString reason = valueFromQrzResponse(body, QStringLiteral("REASON"));
    if (reason.isEmpty()) {
        reason = valueFromQrzResponse(body, QStringLiteral("DATA"));
    }
    if (reason.isEmpty()) {
        reason = body.trimmed();
    }
    return reason;
}

} // namespace

DecodiumQrzLogbookLite::DecodiumQrzLogbookLite(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

DecodiumQrzLogbookLite::~DecodiumQrzLogbookLite() = default;

void DecodiumQrzLogbookLite::setApiKey(const QString& apiKey)
{
    m_apiKey = apiKey.trimmed();
}

QByteArray DecodiumQrzLogbookLite::formBody(const QList<QPair<QString, QString>>& fields) const
{
    QUrlQuery query;
    for (const auto& field : fields) {
        query.addQueryItem(field.first, field.second);
    }
    return query.toString(QUrl::FullyEncoded).toUtf8();
}

void DecodiumQrzLogbookLite::testApi()
{
    if (m_apiKey.trimmed().isEmpty()) {
        emit errorOccurred(tr("API key mancante."));
        return;
    }

    QNetworkRequest request{QUrl(QString::fromLatin1(kQrzLogbookEndpoint))};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setHeader(QNetworkRequest::UserAgentHeader, qrzUserAgent());
    request.setTransferTimeout(15000);

    const QByteArray body = formBody({
        {QStringLiteral("KEY"), m_apiKey},
        {QStringLiteral("ACTION"), QStringLiteral("STATUS")}
    });

    QNetworkReply* reply = m_nam->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray raw = reply->readAll();
        const QString bodyText = QString::fromUtf8(raw);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError && raw.isEmpty()) {
            emit errorOccurred(tr("test fallito: %1").arg(reply->errorString()));
            return;
        }

        const QString result = valueFromQrzResponse(bodyText, QStringLiteral("RESULT")).toUpper();
        if (result == QStringLiteral("OK")) {
            emit apiKeyOk();
            return;
        }

        emit errorOccurred(tr("API key non valida: %1").arg(qrzFailureReason(bodyText)));
    });
}

void DecodiumQrzLogbookLite::uploadAdif(const QString& dxCall, const QByteArray& adifRecord)
{
    if (!m_enabled) {
        return;
    }
    if (m_apiKey.trimmed().isEmpty()) {
        emit errorOccurred(tr("API key mancante."));
        return;
    }

    QString adif = QString::fromUtf8(adifRecord).trimmed();
    if (adif.isEmpty()) {
        emit errorOccurred(tr("record ADIF vuoto."));
        return;
    }
    adif = ensureAdifRecordTerminator(adif);

    QList<QPair<QString, QString>> fields {
        {QStringLiteral("KEY"), m_apiKey},
        {QStringLiteral("ACTION"), QStringLiteral("INSERT")},
        {QStringLiteral("ADIF"), adif}
    };
    if (m_replaceDuplicates) {
        fields.append({QStringLiteral("OPTION"), QStringLiteral("REPLACE")});
    }

    QNetworkRequest request{QUrl(QString::fromLatin1(kQrzLogbookEndpoint))};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setHeader(QNetworkRequest::UserAgentHeader, qrzUserAgent());
    request.setTransferTimeout(15000);

    QNetworkReply* reply = m_nam->post(request, formBody(fields));
    connect(reply, &QNetworkReply::finished, this, [this, reply, dxCall]() {
        const QByteArray raw = reply->readAll();
        const QString bodyText = QString::fromUtf8(raw);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError && raw.isEmpty()) {
            emit errorOccurred(tr("upload fallito: %1").arg(reply->errorString()));
            return;
        }

        const QString result = valueFromQrzResponse(bodyText, QStringLiteral("RESULT")).toUpper();
        if (result == QStringLiteral("OK") || result == QStringLiteral("REPLACE")) {
            emit qsoLogged(dxCall);
            return;
        }

        emit errorOccurred(tr("upload rifiutato per %1: %2")
                               .arg(dxCall, qrzFailureReason(bodyText)));
    });
}
