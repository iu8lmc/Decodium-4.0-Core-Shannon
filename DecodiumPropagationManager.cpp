#include "DecodiumPropagationManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QVariantMap>
#include <QXmlStreamReader>

namespace
{
QUrl const kSolarXmlUrl {"https://www.hamqsl.com/solarxml.php"};
QUrl const kSolarHtmlUrl {"https://www.hamqsl.com/solar.html"};
int const kRefreshMs = 15 * 60 * 1000;
qint64 const kMaxSolarXmlReplyBytes = 256 * 1024;
}

DecodiumPropagationManager::DecodiumPropagationManager(QObject * parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    connect(m_network, &QNetworkAccessManager::finished,
            this, &DecodiumPropagationManager::onNetworkFinished);
    connect(&m_refreshTimer, &QTimer::timeout,
            this, &DecodiumPropagationManager::refresh);
    m_refreshTimer.setInterval(kRefreshMs);
    m_refreshTimer.start();

    refresh();
}

QString DecodiumPropagationManager::sourceUrl() const
{
    return kSolarXmlUrl.toString();
}

QString DecodiumPropagationManager::sourcePageUrl() const
{
    return kSolarHtmlUrl.toString();
}

void DecodiumPropagationManager::refresh()
{
    if (m_requestInFlight) {
        return;
    }

    m_requestInFlight = true;
    if (!m_updating) {
        m_updating = true;
        emit updatingChanged();
    }
    setStatusText(tr("Updating propagation data from %1 ...").arg(kSolarXmlUrl.toString()));

    QNetworkRequest request {kSolarXmlUrl};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("%1/%2")
                          .arg(QCoreApplication::applicationName(),
                               QCoreApplication::applicationVersion().isEmpty()
                                   ? QStringLiteral("dev")
                                   : QCoreApplication::applicationVersion()));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
    m_network->get(request);
}

void DecodiumPropagationManager::onNetworkFinished(QNetworkReply * reply)
{
    if (!reply) {
        m_requestInFlight = false;
        if (m_updating) {
            m_updating = false;
            emit updatingChanged();
        }
        setStatusText(tr("Propagation update failed: empty network reply"));
        return;
    }

    auto const err = reply->error();
    auto const errText = reply->errorString();
    QString payloadError;
    QByteArray const payload = (err == QNetworkReply::NoError)
        ? readReplyPayloadLimited(reply, kMaxSolarXmlReplyBytes, &payloadError)
        : QByteArray {};
    reply->deleteLater();

    m_requestInFlight = false;
    if (m_updating) {
        m_updating = false;
        emit updatingChanged();
    }

    if (err != QNetworkReply::NoError) {
        setStatusText(tr("Propagation update failed: %1").arg(errText));
        return;
    }
    if (!payloadError.isEmpty()) {
        setStatusText(tr("Propagation update failed: %1").arg(payloadError));
        return;
    }

    SolarSnapshot snapshot;
    QString parseError;
    if (!parseSolarXml(payload, &snapshot, &parseError)) {
        setStatusText(tr("Propagation update failed: %1").arg(parseError));
        return;
    }

    applySnapshot(snapshot);
    QString status = tr("Propagation updated: %1 UTC")
                         .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!snapshot.updated.isEmpty()) {
        status += tr(" | Feed time: %1").arg(snapshot.updated);
    }
    setStatusText(status);
}

QString DecodiumPropagationManager::normalizeText(QString const& text)
{
    return text.simplified();
}

QString DecodiumPropagationManager::conditionColor(QString const& text)
{
    QString const t = text.trimmed().toLower();
    if (t.contains(QStringLiteral("good")) || t.contains(QStringLiteral("open")) || t.contains(QStringLiteral("quiet"))) {
        return QStringLiteral("#43d787");
    }
    if (t.contains(QStringLiteral("fair")) || t.contains(QStringLiteral("minor"))) {
        return QStringLiteral("#f0c65a");
    }
    if (t.contains(QStringLiteral("poor")) || t.contains(QStringLiteral("closed"))
        || t.contains(QStringLiteral("storm")) || t.contains(QStringLiteral("major"))) {
        return QStringLiteral("#ff6b6b");
    }
    return QStringLiteral("#d5dde6");
}

QString DecodiumPropagationManager::displayBandName(QString const& band)
{
    QString const trimmed = band.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("-") : trimmed;
}

QString DecodiumPropagationManager::displayVhfName(QString const& name, QString const& location)
{
    QString n = name.trimmed();
    QString l = location.trimmed();

    if (l == QStringLiteral("northern_hemi")) l = QStringLiteral("Northern Hemisphere");
    if (l == QStringLiteral("north_america")) l = QStringLiteral("North America");
    if (l == QStringLiteral("europe_6m")) l = QStringLiteral("Europe 6m");
    if (l == QStringLiteral("europe_4m")) l = QStringLiteral("Europe 4m");
    if (l == QStringLiteral("europe")) l = QStringLiteral("Europe");

    if (n == QStringLiteral("vhf-aurora")) n = QStringLiteral("VHF Aurora");
    if (n == QStringLiteral("E-Skip")) n = QStringLiteral("E-Skip");

    if (!l.isEmpty()) {
        return QStringLiteral("%1 (%2)").arg(n, l);
    }
    return n;
}

QByteArray DecodiumPropagationManager::readReplyPayloadLimited(QNetworkReply * reply, qint64 maxBytes, QString * error)
{
    if (!reply) {
        if (error) *error = QObject::tr("empty reply");
        return {};
    }

    QVariant const lengthHeader = reply->header(QNetworkRequest::ContentLengthHeader);
    if (lengthHeader.isValid() && lengthHeader.toLongLong() > maxBytes) {
        if (error) *error = QObject::tr("reply too large");
        return {};
    }

    QByteArray const payload = reply->read(maxBytes + 1);
    if (payload.size() > maxBytes || !reply->atEnd()) {
        if (error) *error = QObject::tr("reply exceeds limit");
        return {};
    }

    return payload;
}

QVariantList DecodiumPropagationManager::buildHfConditions(SolarSnapshot const& snapshot)
{
    QMap<QString, QString> dayByBand;
    QMap<QString, QString> nightByBand;
    QStringList order;

    for (BandCondition const& condition : snapshot.bands) {
        QString const band = displayBandName(condition.name);
        if (!order.contains(band)) {
            order << band;
        }
        QString const time = condition.time.trimmed().toLower();
        if (time == QStringLiteral("night")) {
            nightByBand.insert(band, condition.value);
        } else {
            dayByBand.insert(band, condition.value);
        }
    }

    QVariantList rows;
    for (QString const& band : order) {
        QVariantMap row;
        row.insert(QStringLiteral("band"), band);
        row.insert(QStringLiteral("day"), dayByBand.value(band, QStringLiteral("N/A")));
        row.insert(QStringLiteral("night"), nightByBand.value(band, QStringLiteral("N/A")));
        row.insert(QStringLiteral("dayColor"), conditionColor(row.value(QStringLiteral("day")).toString()));
        row.insert(QStringLiteral("nightColor"), conditionColor(row.value(QStringLiteral("night")).toString()));
        rows << row;
    }
    return rows;
}

QVariantList DecodiumPropagationManager::buildVhfConditions(SolarSnapshot const& snapshot)
{
    QVariantList rows;
    for (VhfCondition const& condition : snapshot.vhf) {
        QVariantMap row;
        row.insert(QStringLiteral("title"), displayVhfName(condition.name, condition.location));
        row.insert(QStringLiteral("status"), condition.value);
        row.insert(QStringLiteral("color"), conditionColor(condition.value));
        rows << row;
    }
    return rows;
}

bool DecodiumPropagationManager::parseSolarXml(QByteArray const& xml, SolarSnapshot * out, QString * error) const
{
    if (!out) {
        if (error) *error = QStringLiteral("Invalid output buffer");
        return false;
    }

    SolarSnapshot snapshot;
    QXmlStreamReader reader {xml};
    bool foundData = false;

    while (!reader.atEnd()) {
        QXmlStreamReader::TokenType const token = reader.readNext();
        if (token != QXmlStreamReader::StartElement) {
            continue;
        }
        if (reader.name() != QLatin1String("solardata")) {
            continue;
        }

        foundData = true;
        while (reader.readNextStartElement()) {
            QString const name = reader.name().toString();
            if (name == QStringLiteral("updated")) snapshot.updated = normalizeText(reader.readElementText());
            else if (name == QStringLiteral("solarflux")) snapshot.solarflux = normalizeText(reader.readElementText());
            else if (name == QStringLiteral("aindex")) snapshot.aindex = normalizeText(reader.readElementText());
            else if (name == QStringLiteral("kindex")) snapshot.kindex = normalizeText(reader.readElementText());
            else if (name == QStringLiteral("xray")) snapshot.xray = normalizeText(reader.readElementText());
            else if (name == QStringLiteral("sunspots")) snapshot.sunspots = normalizeText(reader.readElementText());
            else if (name == QStringLiteral("solarwind")) snapshot.solarwind = normalizeText(reader.readElementText());
            else if (name == QStringLiteral("muf")) snapshot.muf = normalizeText(reader.readElementText());
            else if (name == QStringLiteral("geomagfield")) snapshot.geomagfield = normalizeText(reader.readElementText());
            else if (name == QStringLiteral("signalnoise")) snapshot.signalnoise = normalizeText(reader.readElementText());
            else if (name == QStringLiteral("calculatedconditions")) {
                while (reader.readNextStartElement()) {
                    if (reader.name() == QLatin1String("band")) {
                        QXmlStreamAttributes const attrs = reader.attributes();
                        BandCondition band;
                        band.name = normalizeText(attrs.value(QStringLiteral("name")).toString());
                        band.time = normalizeText(attrs.value(QStringLiteral("time")).toString());
                        band.value = normalizeText(reader.readElementText());
                        snapshot.bands.push_back(band);
                    } else {
                        reader.skipCurrentElement();
                    }
                }
            } else if (name == QStringLiteral("calculatedvhfconditions")) {
                while (reader.readNextStartElement()) {
                    if (reader.name() == QLatin1String("phenomenon")) {
                        QXmlStreamAttributes const attrs = reader.attributes();
                        VhfCondition vhf;
                        vhf.name = normalizeText(attrs.value(QStringLiteral("name")).toString());
                        vhf.location = normalizeText(attrs.value(QStringLiteral("location")).toString());
                        vhf.value = normalizeText(reader.readElementText());
                        snapshot.vhf.push_back(vhf);
                    } else {
                        reader.skipCurrentElement();
                    }
                }
            } else {
                reader.skipCurrentElement();
            }
        }
        break;
    }

    if (reader.hasError()) {
        if (error) *error = reader.errorString();
        return false;
    }
    if (!foundData) {
        if (error) *error = QStringLiteral("No <solardata> node found in feed");
        return false;
    }

    *out = snapshot;
    return true;
}

void DecodiumPropagationManager::applySnapshot(SolarSnapshot const& snapshot)
{
    m_available = true;
    m_updated = snapshot.updated;
    m_solarFlux = snapshot.solarflux;
    m_aIndex = snapshot.aindex;
    m_kIndex = snapshot.kindex;
    m_xRay = snapshot.xray;
    m_sunspots = snapshot.sunspots;
    m_solarWind = snapshot.solarwind;
    m_muf = snapshot.muf;
    m_geomagneticField = snapshot.geomagfield;
    m_signalNoise = snapshot.signalnoise;
    m_hfConditions = buildHfConditions(snapshot);
    m_vhfConditions = buildVhfConditions(snapshot);
    emit propagationChanged();
}

void DecodiumPropagationManager::setStatusText(QString const& text)
{
    if (m_statusText == text) {
        return;
    }
    m_statusText = text;
    emit statusTextChanged();
}
