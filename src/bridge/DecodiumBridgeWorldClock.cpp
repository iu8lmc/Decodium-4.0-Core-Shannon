#include "DecodiumBridge.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
QTimeZone decodiumUtcTimeZone()
{
    return QTimeZone(QByteArrayLiteral("UTC"));
}
}

QVariantMap DecodiumBridge::worldClockSnapshot(const QString& timeZoneId) const
{
    QByteArray const zoneId = timeZoneId.trimmed().toUtf8();
    QTimeZone zone = (zoneId.isEmpty() || zoneId == "UTC")
        ? decodiumUtcTimeZone()
        : QTimeZone(zoneId);

    if (!zone.isValid()) {
        zone = decodiumUtcTimeZone();
    }

    QDateTime const zonedNow = QDateTime::currentDateTimeUtc().toTimeZone(zone);

    QVariantMap snapshot;
    snapshot.insert(QStringLiteral("hours"), zonedNow.time().hour());
    snapshot.insert(QStringLiteral("minutes"), zonedNow.time().minute());
    snapshot.insert(QStringLiteral("seconds"), zonedNow.time().second());
    snapshot.insert(QStringLiteral("date"), zonedNow.date().toString(QStringLiteral("dd/MM/yyyy")));
    snapshot.insert(QStringLiteral("abbreviation"), zone.abbreviation(zonedNow));
    snapshot.insert(QStringLiteral("offsetSeconds"), zone.offsetFromUtc(zonedNow));
    snapshot.insert(QStringLiteral("timeZoneId"), QString::fromUtf8(zone.id()));
    return snapshot;
}

QVariantList DecodiumBridge::worldClockCityOptions(const QString& query, int limit) const
{
    struct CityOption {
        QString name;
        QString zoneId;
        QString subtitle;
        QString searchText;
        int score {1000};
    };

	auto normalize = [](QString text) {
	    QString const decomposed = text.normalized(QString::NormalizationForm_D);
	    QString stripped;
	    stripped.reserve(decomposed.size());
	    for (QChar const ch : decomposed) {
	        QChar::Category const category = ch.category();
	        if (category == QChar::Mark_NonSpacing
	            || category == QChar::Mark_SpacingCombining
	            || category == QChar::Mark_Enclosing) {
	            continue;
	        }
	        stripped.append(ch);
	    }

	    static QRegularExpression const nonAlnum(QStringLiteral("[^a-z0-9]+"));
	    return stripped.toLower()
	        .replace(nonAlnum, QStringLiteral(" "))
	        .simplified();
	};

    auto cityNameForZone = [](QString const& zoneId) {
        if (zoneId == QStringLiteral("UTC")) {
            return QStringLiteral("UTC");
        }
        QString city = zoneId.section(QLatin1Char('/'), -1).replace(QLatin1Char('_'), QLatin1Char(' '));
        static const QHash<QString, QString> displayAliases {
            {QStringLiteral("Europe/Rome"), QStringLiteral("Roma")},
        };
        return displayAliases.value(zoneId, city);
    };

    auto aliasesForZone = [](QString const& zoneId) {
        static const QHash<QString, QStringList> aliases {
            {QStringLiteral("Europe/Rome"), {QStringLiteral("Roma"), QStringLiteral("Rome")}},
            {QStringLiteral("Europe/London"), {QStringLiteral("Londra"), QStringLiteral("London")}},
            {QStringLiteral("Europe/Paris"), {QStringLiteral("Parigi"), QStringLiteral("Paris")}},
            {QStringLiteral("Europe/Berlin"), {QStringLiteral("Berlino"), QStringLiteral("Berlin")}},
            {QStringLiteral("Europe/Moscow"), {QStringLiteral("Mosca"), QStringLiteral("Moscow")}},
            {QStringLiteral("Europe/Athens"), {QStringLiteral("Atene"), QStringLiteral("Athens")}},
            {QStringLiteral("Europe/Lisbon"), {QStringLiteral("Lisbona"), QStringLiteral("Lisbon")}},
            {QStringLiteral("Asia/Tokyo"), {QStringLiteral("Tokyo"), QStringLiteral("Tokio")}},
            {QStringLiteral("Africa/Cairo"), {QStringLiteral("Cairo"), QStringLiteral("Il Cairo")}},
        };
        return aliases.value(zoneId);
    };

    auto offsetLabel = [](QTimeZone const& zone) {
        if (zone == decodiumUtcTimeZone()) {
            return QStringLiteral("UTC+00:00");
        }
        int const offset = zone.offsetFromUtc(QDateTime::currentDateTimeUtc());
        int const absOffset = std::abs(offset);
        return QStringLiteral("UTC%1%2:%3")
            .arg(offset >= 0 ? QLatin1Char('+') : QLatin1Char('-'))
            .arg(absOffset / 3600, 2, 10, QLatin1Char('0'))
            .arg((absOffset % 3600) / 60, 2, 10, QLatin1Char('0'));
    };

    QString const q = normalize(query);
    int const maxResults = qBound(1, limit, 200);
    static const QStringList defaultZones {
        QStringLiteral("UTC"),
        QStringLiteral("Europe/London"),
        QStringLiteral("Europe/Rome"),
        QStringLiteral("Europe/Moscow"),
        QStringLiteral("Asia/Dubai"),
        QStringLiteral("Asia/Tokyo"),
        QStringLiteral("Australia/Sydney"),
        QStringLiteral("America/New_York"),
        QStringLiteral("America/Los_Angeles"),
    };

    QVector<CityOption> matches;
    auto appendOption = [&](QString const& zoneId, int defaultScore) {
        QString const name = cityNameForZone(zoneId);
        QStringList terms {name, zoneId};
        terms.append(aliasesForZone(zoneId));
        QString const searchText = normalize(terms.join(QLatin1Char(' ')));

        int score = defaultScore;
        if (!q.isEmpty()) {
            if (normalize(name).startsWith(q)) {
                score = 0;
            } else if (searchText.startsWith(q)) {
                score = 5;
            } else if (searchText.contains(QLatin1Char(' ') + q)) {
                score = 10;
            } else if (searchText.contains(q)) {
                score = 30;
            } else {
                return;
            }
        }

        // Most zone IDs do not match a typed query. Avoid loading their
        // timezone data until after the inexpensive text filter has passed.
        QTimeZone zone = zoneId == QStringLiteral("UTC")
            ? decodiumUtcTimeZone()
            : QTimeZone(zoneId.toUtf8());
        if (!zone.isValid()) {
            return;
        }

        CityOption option;
        option.name = name;
        option.zoneId = zoneId;
        option.subtitle = QStringLiteral("%1 · %2").arg(offsetLabel(zone), zoneId);
        option.searchText = searchText;
        option.score = score;
        matches.append(option);
    };

    if (q.isEmpty()) {
        int score = 0;
        for (QString const& zoneId : defaultZones) {
            appendOption(zoneId, score++);
        }
    } else {
        appendOption(QStringLiteral("UTC"), 0);
        for (QByteArray const& id : QTimeZone::availableTimeZoneIds()) {
            QString const zoneId = QString::fromUtf8(id);
            if (zoneId == QStringLiteral("UTC")) {
                continue;
            }
            appendOption(zoneId, 100);
        }
    }

    std::sort(matches.begin(), matches.end(), [](CityOption const& a, CityOption const& b) {
        if (a.score != b.score) {
            return a.score < b.score;
        }
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    QVariantList out;
    QSet<QString> seen;
    for (CityOption const& option : std::as_const(matches)) {
        if (seen.contains(option.zoneId)) {
            continue;
        }
        seen.insert(option.zoneId);
        QVariantMap item;
        item.insert(QStringLiteral("name"), option.name);
        item.insert(QStringLiteral("zoneId"), option.zoneId);
        item.insert(QStringLiteral("subtitle"), option.subtitle);
        out.append(item);
        if (out.size() >= maxResults) {
            break;
        }
    }
    return out;
}

