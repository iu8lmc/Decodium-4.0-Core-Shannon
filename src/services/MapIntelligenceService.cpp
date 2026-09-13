#include "MapIntelligenceService.h"

#include "DxccLookup.h"
#include "MapBaseMapService.h"
#include "MapExternalOverlayService.h"
#include "MapLayerModel.h"
#include "MapOperationsService.h"
#include "MapPskFeedService.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QRunnable>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QThread>
#include <QTime>
#include <QTimeZone>
#include <QTimer>
#include <QUuid>
#include <QVector>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace {

constexpr qint64 kMaxAdifBytes = 64 * 1024 * 1024;
constexpr int kMaxAdifRecords = 500000;
// Changing this invalidates the derived map_qso cache once.  The source ADI
// itself remains untouched; only metadata derived from it is rebuilt.
constexpr int kAdifImportFormatVersion = 4;
constexpr int kMaxPendingLiveSpots = 512;
constexpr int kRosterLimit = 100;
constexpr int kRosterCandidateLimit = 500;
constexpr qint64 kLiveRetentionMs = 30LL * 24LL * 60LL * 60LL * 1000LL;
// The live table is deliberately short-lived.  The event table is a separate
// historical stream used by the heatmap, timeline and path overlays.
constexpr qint64 kSpotEventRetentionMs = 30LL * 24LL * 60LL * 60LL * 1000LL;

QStringList mapCtyDatSearchPaths()
{
    QString const appDir = QCoreApplication::applicationDirPath();
    QStringList paths {
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/cty.dat"),
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            + QStringLiteral("/cty.dat"),
        appDir + QStringLiteral("/cty.dat"),
        QDir(appDir).absoluteFilePath(QStringLiteral("../cty.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/cty.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../share/Decodium/cty.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../share/wsjtx/cty.dat")),
        QDir::current().absoluteFilePath(QStringLiteral("cty.dat")),
        QDir::current().absoluteFilePath(QStringLiteral("resources/runtime/cty.dat"))
    };

    QStringList uniquePaths;
    QSet<QString> seen;
    for (QString const& path : paths) {
        QString const cleanPath = QDir::cleanPath(path);
        if (!cleanPath.isEmpty() && !seen.contains(cleanPath)) {
            seen.insert(cleanPath);
            uniquePaths.append(cleanPath);
        }
    }
    return uniquePaths;
}

std::shared_ptr<const DxccLookup> adifDxccLookup()
{
    static QMutex mutex;
    static QString cachedPath;
    static QDateTime cachedModified;
    static std::shared_ptr<DxccLookup> cachedLookup;

    QString path;
    QFileInfo info;
    for (QString const& candidate : mapCtyDatSearchPaths()) {
        QFileInfo const candidateInfo(candidate);
        if (candidateInfo.isFile()) {
            path = candidateInfo.absoluteFilePath();
            info = candidateInfo;
            break;
        }
    }

    QMutexLocker locker(&mutex);
    if (path == cachedPath && info.lastModified() == cachedModified) {
        return cachedLookup;
    }

    cachedPath = path;
    cachedModified = info.lastModified();
    cachedLookup.reset();
    if (path.isEmpty()) {
        return {};
    }

    auto lookup = std::make_shared<DxccLookup>();
    if (!lookup->loadCtyDat(path)) {
        return {};
    }
    cachedLookup = std::move(lookup);
    return cachedLookup;
}

QString decodedAdifValue(const QByteArray& bytes)
{
    QString value = QString::fromUtf8(bytes);
    if (value.contains(QChar::ReplacementCharacter)) {
        value = QString::fromLocal8Bit(bytes);
    }
    return value.trimmed();
}

QString normalizedGrid(QString value)
{
    value = value.trimmed().toUpper();
    if (value.size() < 4) {
        return {};
    }
    QString const square = value.left(4);
    if (square.at(0) < QLatin1Char('A') || square.at(0) > QLatin1Char('R')
        || square.at(1) < QLatin1Char('A') || square.at(1) > QLatin1Char('R')
        || !square.at(2).isDigit() || !square.at(3).isDigit()) {
        return {};
    }
    if (value.size() == 4) {
        return square;
    }
    if (value.size() < 6
        || value.at(4) < QLatin1Char('A') || value.at(4) > QLatin1Char('X')
        || value.at(5) < QLatin1Char('A') || value.at(5) > QLatin1Char('X')) {
        return {};
    }
    return value.left(6);
}

QStringList normalizedVuccGrids(const QString& value)
{
    QStringList grids;
    QSet<QString> seen;
    for (QString const& token : value.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        QString const grid = normalizedGrid(token);
        if (!grid.isEmpty() && !seen.contains(grid)) {
            seen.insert(grid);
            grids.append(grid);
        }
    }
    return grids;
}

QString normalizedMode(QString mode, QString submode = {})
{
    mode = mode.trimmed().toUpper();
    submode = submode.trimmed().toUpper();
    if (!submode.isEmpty()
        && (mode == QStringLiteral("MFSK")
            || mode == QStringLiteral("JT9")
            || mode.isEmpty())) {
        return submode;
    }
    return mode;
}

struct PropagationDefinition {
    const char* code;
    const char* label;
};

const QVector<PropagationDefinition>& propagationDefinitions()
{
    static const QVector<PropagationDefinition> definitions {
        {"MIXED", "Mixed"},
        {"UNKNOWN", "Unknown"},
        {"AS", "Aircraft Scatter"},
        {"AUE", "Aurora-E"},
        {"AUR", "Aurora"},
        {"BS", "Back Scatter"},
        {"ECH", "EchoLink"},
        {"EME", "EME"},
        {"ES", "Sporadic E"},
        {"F2", "F2 Reflection"},
        {"FAI", "Field Aligned I"},
        {"INTERNET", "Inet-assist"},
        {"ION", "Ionoscatter"},
        {"IRL", "IRLP"},
        {"MS", "Meteor Scatter"},
        {"RPT", "Repeater"},
        {"RS", "Rain Scatter"},
        {"SAT", "Satellite"},
        {"TEP", "Trans-equatorial"},
        {"TR", "Tropo-ducting"}
    };
    return definitions;
}

QString normalizePropagationMode(QString value)
{
    value = value.trimmed().toUpper();
    if (value.isEmpty()) {
        return QStringLiteral("UNKNOWN");
    }

    value.replace(QLatin1Char('_'), QLatin1Char(' '));
    value.replace(QLatin1Char('-'), QLatin1Char(' '));
    value = value.simplified();
    static const QHash<QString, QString> aliases {
        {QStringLiteral("MIXED"), QStringLiteral("MIXED")},
        {QStringLiteral("UNKNOWN"), QStringLiteral("UNKNOWN")},
        {QStringLiteral("AIRCRAFT"), QStringLiteral("AS")},
        {QStringLiteral("AIRCRAFT SCATTER"), QStringLiteral("AS")},
        {QStringLiteral("AURORA E"), QStringLiteral("AUE")},
        {QStringLiteral("AURORA-E"), QStringLiteral("AUE")},
        {QStringLiteral("AURORA"), QStringLiteral("AUR")},
        {QStringLiteral("BACKSCATTER"), QStringLiteral("BS")},
        {QStringLiteral("BACK SCATTER"), QStringLiteral("BS")},
        {QStringLiteral("ECHOLINK"), QStringLiteral("ECH")},
        {QStringLiteral("MOONBOUNCE"), QStringLiteral("EME")},
        {QStringLiteral("EME"), QStringLiteral("EME")},
        {QStringLiteral("SPORADIC E"), QStringLiteral("ES")},
        {QStringLiteral("F2 REFLECTION"), QStringLiteral("F2")},
        {QStringLiteral("FIELD ALIGNED I"), QStringLiteral("FAI")},
        {QStringLiteral("INTERNET ASSIST"), QStringLiteral("INTERNET")},
        {QStringLiteral("INET ASSIST"), QStringLiteral("INTERNET")},
        {QStringLiteral("IONOSCATTER"), QStringLiteral("ION")},
        {QStringLiteral("IRLP"), QStringLiteral("IRL")},
        {QStringLiteral("METEOR"), QStringLiteral("MS")},
        {QStringLiteral("METEOR SCATTER"), QStringLiteral("MS")},
        {QStringLiteral("REPEATER"), QStringLiteral("RPT")},
        {QStringLiteral("RAIN SCATTER"), QStringLiteral("RS")},
        {QStringLiteral("SATELLITE"), QStringLiteral("SAT")},
        {QStringLiteral("TRANS EQUATORIAL"), QStringLiteral("TEP")},
        {QStringLiteral("TRANSEQUATORIAL"), QStringLiteral("TEP")},
        {QStringLiteral("TROPO"), QStringLiteral("TR")},
        {QStringLiteral("TROPO DUCTING"), QStringLiteral("TR")}
    };
    QString const canonical = aliases.value(value, value);
    for (PropagationDefinition const& definition : propagationDefinitions()) {
        if (canonical == QLatin1String(definition.code)) {
            return canonical;
        }
    }
    return QStringLiteral("UNKNOWN");
}

QString normalizedIota(QString value)
{
    value = value.trimmed().toUpper();
    static const QRegularExpression expression(
        QStringLiteral("^[A-Z]{2}-\\d{3}$"));
    return expression.match(value).hasMatch() ? value : QString();
}

QString normalizedPota(QString value)
{
    value = value.trimmed().toUpper();
    static const QRegularExpression expression(
        QStringLiteral("^[A-Z0-9]{1,4}-\\d{1,6}$"));
    return expression.match(value).hasMatch() ? value : QString();
}

QString wpxPrefix(QString call)
{
    call = call.trimmed().toUpper();
    QStringList const parts = call.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (QString const& part : parts) {
        if (part.size() >= 3
            && part.contains(QRegularExpression(QStringLiteral("[A-Z]")))
            && part.contains(QRegularExpression(QStringLiteral("\\d")))) {
            call = part;
            break;
        }
    }
    call.remove(QRegularExpression(QStringLiteral("[^A-Z0-9]")));
    int lastDigit = -1;
    for (int index = 0; index < call.size(); ++index) {
        if (call.at(index).isDigit()) {
            lastDigit = index;
            break;
        }
    }
    if (lastDigit < 0) {
        QString letters = call.left(2);
        return letters.isEmpty() ? QString() : letters + QLatin1Char('0');
    }
    return call.left(lastDigit + 1);
}

int bandOrder(const QString& band)
{
    static const QStringList ordered {
        QStringLiteral("2190m"), QStringLiteral("630m"), QStringLiteral("560m"),
        QStringLiteral("160m"), QStringLiteral("80m"), QStringLiteral("60m"),
        QStringLiteral("40m"), QStringLiteral("30m"), QStringLiteral("20m"),
        QStringLiteral("17m"), QStringLiteral("15m"), QStringLiteral("12m"),
        QStringLiteral("10m"), QStringLiteral("8m"), QStringLiteral("6m"),
        QStringLiteral("5m"), QStringLiteral("4m"), QStringLiteral("2m"),
        QStringLiteral("1.25m"), QStringLiteral("70cm"), QStringLiteral("33cm"),
        QStringLiteral("23cm"), QStringLiteral("13cm"), QStringLiteral("9cm"),
        QStringLiteral("6cm"), QStringLiteral("3cm"), QStringLiteral("1.25cm")
    };
    int const index = ordered.indexOf(band.toLower());
    return index >= 0 ? index : 1000;
}

QString bandFromFrequencyMhz(double mhz)
{
    struct Range { double low; double high; const char* name; };
    static const Range ranges[] = {
        {0.135, 0.138, "2190m"}, {0.472, 0.480, "630m"},
        {1.8, 2.0, "160m"}, {3.5, 4.1, "80m"}, {5.0, 5.6, "60m"},
        {7.0, 7.4, "40m"}, {10.0, 10.2, "30m"}, {14.0, 14.4, "20m"},
        {18.0, 18.2, "17m"}, {21.0, 21.5, "15m"}, {24.8, 25.0, "12m"},
        {28.0, 30.0, "10m"}, {40.0, 45.0, "8m"}, {50.0, 54.5, "6m"},
        {70.0, 71.0, "4m"}, {144.0, 148.0, "2m"}, {219.0, 225.0, "1.25m"},
        {420.0, 450.0, "70cm"}, {902.0, 928.0, "33cm"},
        {1240.0, 1300.0, "23cm"}, {2300.0, 2450.0, "13cm"},
        {3300.0, 3500.0, "9cm"}, {5650.0, 5925.0, "6cm"},
        {10000.0, 10500.0, "3cm"}, {24000.0, 24250.0, "1.25cm"}
    };
    for (Range const& range : ranges) {
        if (mhz >= range.low && mhz <= range.high) {
            return QString::fromLatin1(range.name);
        }
    }
    return {};
}

QString normalizedBand(QString band, double frequencyMhz)
{
    band = band.trimmed().toLower();
    return band.isEmpty() ? bandFromFrequencyMhz(frequencyMhz) : band;
}

bool isConfirmed(const QHash<QString, QString>& fields)
{
    static const QStringList confirmationFields {
        QStringLiteral("QSL_RCVD"),
        QStringLiteral("LOTW_QSL_RCVD"),
        QStringLiteral("EQSL_QSL_RCVD")
    };
    for (QString const& key : confirmationFields) {
        if (fields.value(key).trimmed().compare(QStringLiteral("Y"), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool adifYes(const QString& value)
{
    QString const normalized = value.trimmed().toUpper();
    return normalized == QStringLiteral("Y")
        || normalized == QStringLiteral("YES")
        || normalized == QStringLiteral("1");
}

struct ExternalAwardDefinition {
    QString id;
    QString label;
    QString sponsor;
    QString type;
    QString tooltip;
    QStringList bands;
    QStringList modes;
    QStringList endorsements;
    QStringList dxccNumbers;
    QStringList continents;
    QStringList calls;
    QStringList grids;
    QStringList iotaReferences;
    QVector<int> zones;
    QStringList prefixes;
    // Some catalog rules collapse multiple prefixes or suffixes into one
    // award entity.  Preserve those equivalences instead of treating every
    // raw callsign fragment as a separate award credit.
    QVector<QStringList> aliases;
    QStringList prefixWhitelist;
    QVector<int> unique;
    QVector<int> count;
    int requiredBandCount {0};
    int target {0};
};

QStringList jsonStrings(const QJsonValue& value)
{
    QStringList values;
    if (value.isString()) {
        QString const text = value.toString().trimmed().toUpper();
        if (!text.isEmpty()) values.append(text);
    } else if (value.isArray()) {
        for (QJsonValue const& child : value.toArray()) {
            values.append(jsonStrings(child));
        }
    }
    values.removeDuplicates();
    return values;
}

QVector<QStringList> jsonAliasGroups(const QJsonValue& value)
{
    QVector<QStringList> groups;
    if (!value.isArray()) return groups;
    for (QJsonValue const& child : value.toArray()) {
        QStringList const group = jsonStrings(child);
        if (!group.isEmpty()) groups.append(group);
    }
    return groups;
}

QVector<int> jsonInts(const QJsonValue& value)
{
    QVector<int> values;
    if (value.isDouble()) {
        values.append(value.toInt());
    } else if (value.isString()) {
        bool ok = false;
        int const parsed = value.toString().trimmed().toInt(&ok);
        if (ok) values.append(parsed);
    } else if (value.isArray()) {
        for (QJsonValue const& child : value.toArray()) {
            QVector<int> const nested = jsonInts(child);
            for (int const value : nested) values.append(value);
        }
    }
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

QStringList jsonNumberStrings(const QJsonValue& value)
{
    QStringList values;
    QVector<int> const numbers = jsonInts(value);
    for (int const number : numbers) values.append(QString::number(number));
    return values;
}

QVector<ExternalAwardDefinition> const& externalAwardDefinitions()
{
    static const QVector<ExternalAwardDefinition> definitions = [] {
        QVector<ExternalAwardDefinition> parsed;
        QFile file(QStringLiteral(":/decodium-awards-catalog.json"));
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning().noquote() << "[MAPINT] award catalog unavailable:" << file.errorString();
            return parsed;
        }
        QJsonParseError error;
        QJsonDocument const document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning().noquote() << "[MAPINT] award catalog parse failed:" << error.errorString();
            return parsed;
        }
        QJsonObject const sponsors = document.object();
        for (auto sponsorIt = sponsors.constBegin(); sponsorIt != sponsors.constEnd(); ++sponsorIt) {
            QJsonObject const awards = sponsorIt.value().toObject()
                                          .value(QStringLiteral("awards")).toObject();
            for (auto awardIt = awards.constBegin(); awardIt != awards.constEnd(); ++awardIt) {
                QJsonObject const award = awardIt.value().toObject();
                QJsonObject const rule = award.value(QStringLiteral("rule")).toObject();
                if (rule.isEmpty()) continue;
                ExternalAwardDefinition definition;
                definition.sponsor = award.value(QStringLiteral("sponsor"))
                                         .toString(sponsorIt.key()).trimmed();
                definition.id = definition.sponsor + QStringLiteral(":") + awardIt.key();
                QString const name = award.value(QStringLiteral("name"))
                                         .toString(awardIt.key()).trimmed();
                definition.label = definition.sponsor + QStringLiteral(": ") + name;
                definition.type = rule.value(QStringLiteral("type")).toString().trimmed().toLower();
                definition.tooltip = award.value(QStringLiteral("tooltip")).toString().trimmed();
                definition.bands = jsonStrings(rule.value(QStringLiteral("band")));
                definition.modes = jsonStrings(
                    rule.contains(QStringLiteral("mode"))
                        ? rule.value(QStringLiteral("mode"))
                        : rule.value(QStringLiteral("mdoe")));
                definition.endorsements = jsonStrings(rule.value(QStringLiteral("endorse")));
                definition.dxccNumbers = jsonNumberStrings(rule.value(QStringLiteral("dxcc")));
                definition.continents = jsonStrings(rule.value(QStringLiteral("cont")));
                definition.calls = jsonStrings(rule.value(QStringLiteral("call")));
                definition.grids = jsonStrings(rule.value(QStringLiteral("grid")));
                definition.iotaReferences = jsonStrings(rule.value(QStringLiteral("IOTA")));
                definition.zones = jsonInts(rule.value(QStringLiteral("zone")));
                definition.prefixes = jsonStrings(rule.value(QStringLiteral("px")));
                definition.unique = jsonInts(rule.value(QStringLiteral("unique")));
                definition.count = jsonInts(rule.value(QStringLiteral("count")));
                for (int const value : definition.count) {
                    definition.target = qMax(definition.target, value);
                }
                for (int const value : definition.unique) {
                    definition.target = qMax(definition.target, value);
                }
                definition.requiredBandCount = definition.count.isEmpty()
                    ? definition.bands.size() : *std::max_element(
                          definition.count.constBegin(), definition.count.constEnd());
                if (definition.type == QStringLiteral("pxa")) {
                    definition.aliases = jsonAliasGroups(rule.value(QStringLiteral("pxa")));
                } else if (definition.type == QStringLiteral("numsfx")) {
                    definition.aliases = jsonAliasGroups(rule.value(QStringLiteral("numsfx")));
                } else if (definition.type == QStringLiteral("sfx")) {
                    definition.aliases = jsonAliasGroups(rule.value(QStringLiteral("sfx")));
                } else if (definition.type == QStringLiteral("pxplus")) {
                    definition.prefixWhitelist = jsonStrings(rule.value(QStringLiteral("pxplus")));
                }
                if (definition.target <= 0) definition.target = 1;
                if (!definition.type.isEmpty()) parsed.append(std::move(definition));
            }
        }
        std::sort(parsed.begin(), parsed.end(), [](ExternalAwardDefinition const& left,
                                                    ExternalAwardDefinition const& right) {
            return left.label.localeAwareCompare(right.label) < 0;
        });
        return parsed;
    }();
    return definitions;
}

ExternalAwardDefinition const* externalAwardForLabel(const QString& label)
{
    for (ExternalAwardDefinition const& definition : externalAwardDefinitions()) {
        if (definition.label.compare(label, Qt::CaseInsensitive) == 0) {
            return &definition;
        }
    }
    return nullptr;
}

QString externalAwardEntityExpression(const ExternalAwardDefinition& definition)
{
    QString const type = definition.type;
    if (type == QStringLiteral("grids")) return QStringLiteral("upper(grid4)");
    if (type == QStringLiteral("dxcc")) return QStringLiteral("lower(dxcc)");
    if (type == QStringLiteral("dxcc2band")) {
        return QStringLiteral("lower(dxcc) || '@' || lower(band)");
    }
    if (type == QStringLiteral("cqz")) return QStringLiteral("CAST(cq_zone AS TEXT)");
    if (type == QStringLiteral("states")) {
        return QStringLiteral("upper(state)");
    }
    if (type == QStringLiteral("states2band")) {
        return QStringLiteral("upper(state) || '@' || lower(band)");
    }
    if (type == QStringLiteral("cnty")) return QStringLiteral("upper(county)");
    if (type == QStringLiteral("iota")) return QStringLiteral("upper(iota)");
    if (type == QStringLiteral("cont") || type == QStringLiteral("cont5")) {
        return QStringLiteral("upper(continent)");
    }
    if (type == QStringLiteral("cont2band") || type == QStringLiteral("cont52band")) {
        return QStringLiteral("upper(continent) || '@' || lower(band)");
    }
    if (type == QStringLiteral("call") || type == QStringLiteral("calls2dxcc")) {
        return QStringLiteral("upper(call)");
    }
    if (type == QStringLiteral("calls2band")) {
        return QStringLiteral("upper(call) || '@' || lower(band)");
    }
    // Prefix and call-area rules use the normalized WPX prefix stored during
    // ADIF import. Suffix variants retain the full call in historic queries;
    // live candidates below derive the exact suffix token for selection.
    if (type == QStringLiteral("callarea") || type == QStringLiteral("px")
        || type == QStringLiteral("pxa") || type == QStringLiteral("pxplus")) {
        return QStringLiteral("upper(wpx)");
    }
    if (type == QStringLiteral("numsfx") || type == QStringLiteral("sfx")) {
        return QStringLiteral("upper(call)");
    }
    return {};
}

QString sqlQuotedList(const QStringList& values)
{
    QStringList quoted;
    for (QString value : values) {
        value = value.trimmed();
        if (!value.isEmpty()) {
            quoted.append(QStringLiteral("'%1'").arg(value.replace(QLatin1Char('\''), QStringLiteral("''"))));
        }
    }
    return quoted.join(QStringLiteral(","));
}

QStringList externalAwardBands(const ExternalAwardDefinition& definition,
                               const QString& endorsement)
{
    QStringList bands;
    QString const selected = endorsement.trimmed();
    if (!selected.isEmpty()
        && selected.compare(QStringLiteral("Mixed"), Qt::CaseInsensitive) != 0
        && selected.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0
        && selected.compare(QStringLiteral("Any"), Qt::CaseInsensitive) != 0) {
        bands.append(selected.toLower());
        return bands;
    }
    for (QString const& band : definition.bands) {
        if (band.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0
            && band.compare(QStringLiteral("Mixed"), Qt::CaseInsensitive) != 0
            && band.compare(QStringLiteral("Any"), Qt::CaseInsensitive) != 0) {
            bands.append(band.toLower());
        }
    }
    bands.removeDuplicates();
    return bands;
}

bool isDigitalAwardMode(const QString& mode)
{
    static const QStringList digitalModes {
        QStringLiteral("FT2"), QStringLiteral("FT4"), QStringLiteral("FT8"),
        QStringLiteral("FST4"), QStringLiteral("FST4W"), QStringLiteral("JT4"),
        QStringLiteral("JT9"), QStringLiteral("JT65"), QStringLiteral("JT6M"),
        QStringLiteral("MSK144"), QStringLiteral("Q65"), QStringLiteral("WSPR"),
        QStringLiteral("FSK441"), QStringLiteral("ISCAT"), QStringLiteral("OLIVIA"),
        QStringLiteral("DOMINO"), QStringLiteral("PSK"), QStringLiteral("JS8"),
        QStringLiteral("MFSK"), QStringLiteral("PKT")
    };
    return digitalModes.contains(mode.trimmed().toUpper());
}

bool isPhoneAwardMode(const QString& mode)
{
    static const QStringList phoneModes {
        QStringLiteral("AM"), QStringLiteral("FM"), QStringLiteral("SSB"),
        QStringLiteral("USB"), QStringLiteral("LSB"), QStringLiteral("DSB"),
        QStringLiteral("PHONE"), QStringLiteral("DV"), QStringLiteral("DSTAR")
    };
    return phoneModes.contains(mode.trimmed().toUpper());
}

bool matchesAwardMode(const QStringList& modes, const QString& mode)
{
    if (modes.isEmpty()) return true;
    bool familyMatch = false;
    for (QString const& candidate : modes) {
        if (candidate.compare(QStringLiteral("Mixed"), Qt::CaseInsensitive) == 0
            || candidate.compare(QStringLiteral("Any"), Qt::CaseInsensitive) == 0
            || candidate.compare(mode, Qt::CaseInsensitive) == 0) {
            return true;
        }
        if (candidate.compare(QStringLiteral("Digital"), Qt::CaseInsensitive) == 0) {
            familyMatch = familyMatch || isDigitalAwardMode(mode);
        } else if (candidate.compare(QStringLiteral("Phone"), Qt::CaseInsensitive) == 0) {
            familyMatch = familyMatch || isPhoneAwardMode(mode);
        } else if (candidate.compare(QStringLiteral("CW"), Qt::CaseInsensitive) == 0) {
            familyMatch = familyMatch || mode.compare(QStringLiteral("CW"), Qt::CaseInsensitive) == 0;
        }
    }
    return familyMatch;
}

QString externalAwardScopeFilter(const ExternalAwardDefinition& definition,
                                 const QString& endorsement = {})
{
    QString filter;
    QStringList const bands = externalAwardBands(definition, endorsement);
    if (!bands.isEmpty()) {
        filter += QStringLiteral(" AND lower(band) IN (%1)").arg(sqlQuotedList(bands));
    }

    QStringList modes;
    for (QString const& mode : definition.modes) {
        if (mode.compare(QStringLiteral("Digital"), Qt::CaseInsensitive) != 0
            && mode.compare(QStringLiteral("Phone"), Qt::CaseInsensitive) != 0
            && mode.compare(QStringLiteral("CW"), Qt::CaseInsensitive) != 0
            && mode.compare(QStringLiteral("Mixed"), Qt::CaseInsensitive) != 0
            && mode.compare(QStringLiteral("Any"), Qt::CaseInsensitive) != 0) {
            modes.append(mode.toUpper());
        }
    }
    bool const hasDigital = definition.modes.contains(QStringLiteral("Digital"), Qt::CaseInsensitive);
    bool const hasPhone = definition.modes.contains(QStringLiteral("Phone"), Qt::CaseInsensitive);
    bool const hasCw = definition.modes.contains(QStringLiteral("CW"), Qt::CaseInsensitive);
    if (!modes.isEmpty()) {
        filter += QStringLiteral(" AND upper(mode) IN (%1)").arg(sqlQuotedList(modes));
    } else if (hasDigital || hasPhone || hasCw) {
        QStringList families;
        if (hasDigital) families << QStringLiteral("'FT2'") << QStringLiteral("'FT4'")
                                 << QStringLiteral("'FT8'") << QStringLiteral("'FST4'")
                                 << QStringLiteral("'FST4W'") << QStringLiteral("'JT4'")
                                 << QStringLiteral("'JT9'") << QStringLiteral("'JT65'")
                                 << QStringLiteral("'Q65'") << QStringLiteral("'MSK144'")
                                 << QStringLiteral("'WSPR'") << QStringLiteral("'FSK441'")
                                 << QStringLiteral("'ISCAT'") << QStringLiteral("'OLIVIA'")
                                 << QStringLiteral("'DOMINO'") << QStringLiteral("'PSK'")
                                 << QStringLiteral("'JS8'") << QStringLiteral("'MFSK'")
                                 << QStringLiteral("'PKT'");
        if (hasPhone) families << QStringLiteral("'AM'") << QStringLiteral("'FM'")
                               << QStringLiteral("'SSB'") << QStringLiteral("'USB'")
                               << QStringLiteral("'LSB'") << QStringLiteral("'DSB'")
                               << QStringLiteral("'PHONE'") << QStringLiteral("'DV'")
                               << QStringLiteral("'DSTAR'");
        if (hasCw) families << QStringLiteral("'CW'");
        filter += QStringLiteral(" AND upper(mode) IN (%1)").arg(families.join(','));
    }
    if (!definition.dxccNumbers.isEmpty()) {
        filter += QStringLiteral(" AND CAST(dxcc_number AS TEXT) IN (%1)")
                      .arg(sqlQuotedList(definition.dxccNumbers));
    }
    if (!definition.continents.isEmpty()) {
        filter += QStringLiteral(" AND upper(continent) IN (%1)")
                      .arg(sqlQuotedList(definition.continents));
    }
    if (!definition.calls.isEmpty()) {
        filter += QStringLiteral(" AND upper(call) IN (%1)")
                      .arg(sqlQuotedList(definition.calls));
    }
    if (!definition.grids.isEmpty()) {
        filter += QStringLiteral(" AND upper(grid4) IN (%1)")
                      .arg(sqlQuotedList(definition.grids));
    }
    if (!definition.iotaReferences.isEmpty()) {
        filter += QStringLiteral(" AND upper(iota) IN (%1)")
                      .arg(sqlQuotedList(definition.iotaReferences));
    }
    return filter;
}

bool externalAwardMatchesSpot(const ExternalAwardDefinition& definition,
                              const QString& band, const QString& mode,
                              const QString& endorsement = {})
{
    QStringList const bands = externalAwardBands(definition, endorsement);
    return (bands.isEmpty() || bands.contains(band.trimmed().toLower()))
        && matchesAwardMode(definition.modes, mode);
}

bool externalAwardMatchesFields(const ExternalAwardDefinition& definition,
                                const QString& call, const QString& band,
                                const QString& mode, const QString& grid,
                                int dxccNumber, int cqZone,
                                const QString& continent, const QString& iota,
                                const QString& endorsement = {})
{
    if (!externalAwardMatchesSpot(definition, band, mode, endorsement)) return false;
    QString const normalizedCall = call.trimmed().toUpper();
    QString const normalizedGrid = grid.left(4).trimmed().toUpper();
    QString const normalizedContinent = continent.trimmed().toUpper();
    QString const normalizedIota = iota.trimmed().toUpper();
    if (!definition.dxccNumbers.isEmpty()
        && !definition.dxccNumbers.contains(QString::number(dxccNumber))) {
        return false;
    }
    if (!definition.continents.isEmpty()
        && !definition.continents.contains(normalizedContinent, Qt::CaseInsensitive)) {
        return false;
    }
    if (!definition.calls.isEmpty()
        && !definition.calls.contains(normalizedCall, Qt::CaseInsensitive)) {
        return false;
    }
    if (!definition.grids.isEmpty()
        && !definition.grids.contains(normalizedGrid, Qt::CaseInsensitive)) {
        return false;
    }
    if (!definition.iotaReferences.isEmpty()
        && !definition.iotaReferences.contains(normalizedIota, Qt::CaseInsensitive)) {
        return false;
    }
    QString const prefix = wpxPrefix(normalizedCall);
    if (!definition.prefixes.isEmpty()) {
        bool found = false;
        for (QString const& allowed : definition.prefixes) {
            if (prefix.startsWith(allowed, Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    if (!definition.zones.isEmpty()) {
        int zone = cqZone;
        if (definition.type == QStringLiteral("callarea")) {
            QRegularExpressionMatch const match = QRegularExpression(
                QStringLiteral("[0-9]")).match(prefix);
            zone = match.hasMatch() ? match.captured(0).toInt() : -1;
        }
        if (!definition.zones.contains(zone)) return false;
    }
    return true;
}

QString awardAliasEntity(const ExternalAwardDefinition& definition,
                         QString candidate, bool prefixMatch)
{
    candidate = candidate.trimmed().toUpper();
    if (candidate.isEmpty()) return {};
    if (!definition.prefixWhitelist.isEmpty()) {
        bool accepted = false;
        for (QString const& prefix : definition.prefixWhitelist) {
            if (candidate.startsWith(prefix)) {
                accepted = true;
                break;
            }
        }
        if (!accepted) return {};
    }
    if (definition.aliases.isEmpty()) return candidate;
    for (QStringList const& group : definition.aliases) {
        for (QString const& alias : group) {
            if ((prefixMatch && candidate.startsWith(alias))
                || (!prefixMatch && candidate == alias)) {
                return group.constFirst();
            }
        }
    }
    return {};
}

QString externalAwardSpotEntity(const ExternalAwardDefinition& definition,
                                const QString& call, const QString& band,
                                const QString& grid,
                                const QString& dxcc, int cqZone,
                                const QString& state, const QString& continent,
                                const QString& county = {},
                                const QString& iota = {})
{
    QString const type = definition.type;
    if (type == QStringLiteral("grids")) return grid.left(4).toUpper();
    if (type == QStringLiteral("dxcc")) return dxcc.trimmed().toLower();
    if (type == QStringLiteral("dxcc2band")) {
        QString const entity = dxcc.trimmed().toLower();
        return entity.isEmpty() ? QString() : entity + QStringLiteral("@") + band.toLower();
    }
    if (type == QStringLiteral("cqz")) return cqZone > 0 ? QString::number(cqZone) : QString();
    if (type == QStringLiteral("states")) {
        return state.trimmed().toUpper();
    }
    if (type == QStringLiteral("states2band")) {
        QString const entity = state.trimmed().toUpper();
        return entity.isEmpty() ? QString() : entity + QStringLiteral("@") + band.toLower();
    }
    if (type == QStringLiteral("cnty")) return county.trimmed().toUpper();
    if (type == QStringLiteral("iota")) return iota.trimmed().toUpper();
    if (type == QStringLiteral("cont") || type == QStringLiteral("cont5")) {
        return continent.trimmed().toUpper();
    }
    if (type == QStringLiteral("cont2band") || type == QStringLiteral("cont52band")) {
        QString const entity = continent.trimmed().toUpper();
        return entity.isEmpty() ? QString() : entity + QStringLiteral("@") + band.toLower();
    }
    if (type == QStringLiteral("call") || type == QStringLiteral("calls2dxcc")) {
        return call.trimmed().toUpper();
    }
    if (type == QStringLiteral("calls2band")) {
        QString const entity = call.trimmed().toUpper();
        return entity.isEmpty() ? QString() : entity + QStringLiteral("@") + band.toLower();
    }
    if (type == QStringLiteral("callarea") || type == QStringLiteral("px")
        || type == QStringLiteral("pxa") || type == QStringLiteral("pxplus")) {
        return awardAliasEntity(definition, wpxPrefix(call), true);
    }
    if (type == QStringLiteral("numsfx")) {
        int const lastDigit = call.lastIndexOf(QRegularExpression(QStringLiteral("[0-9]")));
        return lastDigit >= 0 && lastDigit + 1 < call.size()
            ? awardAliasEntity(definition, call.mid(lastDigit, 2), false) : QString();
    }
    if (type == QStringLiteral("sfx")) {
        int const lastDigit = call.lastIndexOf(QRegularExpression(QStringLiteral("[0-9]")));
        return lastDigit >= 0 && lastDigit + 1 < call.size()
            ? awardAliasEntity(definition, call.mid(lastDigit + 1), true) : QString();
    }
    return {};
}

QString normalizedSpotAgeFilter(const QString& value)
{
    QString const normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("all retained")) {
        return QStringLiteral("All retained");
    }
    if (normalized == QStringLiteral("1 hour")) {
        // Keep the persisted value used by earlier releases meaningful after
        // moving to the finer 5-minute selector.
        return QStringLiteral("60 min");
    }
    if (normalized == QStringLiteral("6 hours")) return QStringLiteral("6 hours");
    if (normalized == QStringLiteral("24 hours")) return QStringLiteral("24 hours");
    if (normalized == QStringLiteral("7 days")) return QStringLiteral("7 days");

    QRegularExpression const minutesExpression(
        QStringLiteral("^([0-9]{1,3})\\s*min(?:ute)?s?$"));
    QRegularExpressionMatch const match = minutesExpression.match(normalized);
    if (match.hasMatch()) {
        bool ok = false;
        int const minutes = match.captured(1).toInt(&ok);
        if (ok && minutes >= 5 && minutes <= 60 && minutes % 5 == 0) {
            return QStringLiteral("%1 min").arg(minutes);
        }
    }
    return {};
}

qint64 spotAgeCutoff(const QString& filter, qint64 nowMs)
{
    QString const normalized = normalizedSpotAgeFilter(filter);
    qint64 minutes = 0;
    QRegularExpression const minutesExpression(QStringLiteral("^([0-9]+) min$"));
    QRegularExpressionMatch const match = minutesExpression.match(normalized);
    if (match.hasMatch()) {
        minutes = match.captured(1).toLongLong();
    } else if (normalized == QStringLiteral("6 hours")) {
        minutes = 360;
    } else if (normalized == QStringLiteral("24 hours")) {
        minutes = 1440;
    } else if (normalized == QStringLiteral("7 days")) {
        minutes = 10080;
    }
    return minutes > 0 ? nowMs - minutes * 60LL * 1000LL : 0;
}

QString normalizedGridOrigin(const QString& value)
{
    QString const normalized = value.trimmed().toLower();
    if (normalized.isEmpty()) return QStringLiteral("UNKNOWN");
    if (normalized.contains(QStringLiteral("decode"))
        || normalized.contains(QStringLiteral("on-air"))
        || normalized.contains(QStringLiteral("over the air"))) {
        return QStringLiteral("DECODED");
    }
    if (normalized.contains(QStringLiteral("psk"))) return QStringLiteral("PSK");
    if (normalized.contains(QStringLiteral("oams"))) return QStringLiteral("OAMS");
    if (normalized.contains(QStringLiteral("rtsn"))) return QStringLiteral("RTSN");
    if (normalized.contains(QStringLiteral("lookup"))
        || normalized.contains(QStringLiteral("qrz"))
        || normalized.contains(QStringLiteral("hamqth"))
        || normalized.contains(QStringLiteral("callbook"))) {
        return QStringLiteral("LOOKUP");
    }
    return QStringLiteral("UNKNOWN");
}

QString gridOriginForSource(const QString& source, const QString& provider = {})
{
    QString const explicitOrigin = normalizedGridOrigin(provider);
    if (explicitOrigin != QStringLiteral("UNKNOWN")) return explicitOrigin;
    QString const normalizedSource = source.trimmed().toLower();
    if (normalizedSource == QStringLiteral("decoder")) return QStringLiteral("DECODED");
    if (normalizedSource == QStringLiteral("psk")) return QStringLiteral("PSK");
    if (normalizedSource == QStringLiteral("oams")) return QStringLiteral("OAMS");
    if (normalizedSource == QStringLiteral("rtsn")) return QStringLiteral("RTSN");
    if (normalizedSource == QStringLiteral("lookup")) return QStringLiteral("LOOKUP");
    return QStringLiteral("UNKNOWN");
}

QString gridOriginLabel(const QString& origin)
{
    QString const normalized = normalizedGridOrigin(origin);
    if (normalized == QStringLiteral("DECODED")) return QStringLiteral("Decoded on-air");
    if (normalized == QStringLiteral("PSK")) return QStringLiteral("PSK Reporter");
    if (normalized == QStringLiteral("OAMS")) return QStringLiteral("OAMS");
    if (normalized == QStringLiteral("RTSN")) return QStringLiteral("RTSN");
    if (normalized == QStringLiteral("LOOKUP")) return QStringLiteral("Lookup estimate");
    return QStringLiteral("Unspecified");
}

QString gridReliabilityLabel(const QString& origin)
{
    QString const normalized = normalizedGridOrigin(origin);
    if (normalized == QStringLiteral("DECODED")) return QStringLiteral("Verified");
    if (normalized == QStringLiteral("PSK")
        || normalized == QStringLiteral("OAMS")
        || normalized == QStringLiteral("RTSN")) {
        return QStringLiteral("Corroborated");
    }
    if (normalized == QStringLiteral("LOOKUP")) return QStringLiteral("Estimated");
    return QStringLiteral("Unspecified");
}

QString gridReliabilityMarker(const QString& origin)
{
    QString const normalized = normalizedGridOrigin(origin);
    if (normalized == QStringLiteral("DECODED")) return QStringLiteral("✓");
    if (normalized == QStringLiteral("PSK")
        || normalized == QStringLiteral("OAMS")
        || normalized == QStringLiteral("RTSN")) {
        return QStringLiteral("◇");
    }
    if (normalized == QStringLiteral("LOOKUP")) return QStringLiteral("?");
    return QStringLiteral("·");
}

QString activityTypeForMessage(const QString& message,
                               const QString& mode,
                               const QString& source,
                               bool isCq,
                               const QString& targetCall)
{
    QString const normalized = message.simplified().toUpper();
    if (source.compare(QStringLiteral("psk"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("PSK");
    }
    if (mode.compare(QStringLiteral("WSPR"), Qt::CaseInsensitive) == 0
        || mode.compare(QStringLiteral("FST4W"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("WSPR");
    }
    if (normalized == QStringLiteral("QRZ")
        || normalized.startsWith(QStringLiteral("QRZ "))) {
        return QStringLiteral("QRZ");
    }
    if (normalized.startsWith(QStringLiteral("CQ DX "))
        || normalized == QStringLiteral("CQ DX")) {
        return QStringLiteral("CQDX");
    }
    if (isCq) {
        return QStringLiteral("CQ");
    }
    if (!targetCall.trimmed().isEmpty()) {
        return QStringLiteral("QSX");
    }
    return QStringLiteral("LIVE");
}

qreal liveOpacityForAge(qint64 ageMs, int decayMinutes)
{
    qreal const lifetimeMs =
        static_cast<qreal>(qMax(1, decayMinutes)) * 60.0 * 1000.0;
    qreal const normalized =
        qBound<qreal>(0.0, static_cast<qreal>(ageMs) / lifetimeMs, 1.0);
    return 1.0 - 0.72 * normalized;
}

QString digestKey(const QStringList& parts)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        parts.join(QChar(0x1f)).toUtf8(), QCryptographicHash::Sha256).toHex());
}

QStringList sortedBands(QStringList values)
{
    values.removeDuplicates();
    values.removeAll(QString());
    std::sort(values.begin(), values.end(), [](QString const& left, QString const& right) {
        int const leftOrder = bandOrder(left);
        int const rightOrder = bandOrder(right);
        return leftOrder == rightOrder ? left < right : leftOrder < rightOrder;
    });
    values.prepend(QStringLiteral("All"));
    return values;
}

QStringList sortedModes(QStringList values)
{
    values.removeDuplicates();
    values.removeAll(QString());
    std::sort(values.begin(), values.end(), [](QString const& left, QString const& right) {
        return left.localeAwareCompare(right) < 0;
    });
    values.prepend(QStringLiteral("All"));
    return values;
}

class ScopedSqliteConnection
{
public:
    explicit ScopedSqliteConnection(const QString& path)
        : m_name(QStringLiteral("map_intelligence_%1")
                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
    {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        m_database.setDatabaseName(path);
    }

    ~ScopedSqliteConnection()
    {
        if (m_database.isValid()) {
            m_database.close();
        }
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_name);
    }

    QSqlDatabase& database() { return m_database; }

private:
    QString m_name;
    QSqlDatabase m_database;
};

bool execSql(QSqlDatabase& db, const QString& sql, QString* error)
{
    QSqlQuery query(db);
    if (query.exec(sql)) {
        return true;
    }
    if (error) {
        *error = query.lastError().text();
    }
    return false;
}

bool ensureColumn(QSqlDatabase& db,
                  const QString& table,
                  const QString& column,
                  const QString& definition,
                  QString* error)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        if (error) {
            *error = query.lastError().text();
        }
        return false;
    }
    while (query.next()) {
        if (query.value(1).toString().compare(column, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return execSql(db,
                   QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                       .arg(table, column, definition),
                   error);
}

QString transmittingCallFromMessage(const QString& message);
QString targetCallFromMessage(const QString& message);
bool repairDecoderSpotAttribution(QSqlDatabase& db, QString* error);

bool openMapDatabase(const QString& path,
                     std::unique_ptr<ScopedSqliteConnection>* connection,
                     QString* error)
{
    QFileInfo const info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) {
            *error = QStringLiteral("Cannot create database directory: %1").arg(info.absolutePath());
        }
        return false;
    }

    auto candidate = std::make_unique<ScopedSqliteConnection>(path);
    QSqlDatabase& db = candidate->database();
    if (!db.open()) {
        if (error) {
            *error = db.lastError().text();
        }
        return false;
    }

    execSql(db, QStringLiteral("PRAGMA journal_mode=WAL"), nullptr);
    execSql(db, QStringLiteral("PRAGMA synchronous=NORMAL"), nullptr);
    execSql(db, QStringLiteral("PRAGMA busy_timeout=5000"), nullptr);
    execSql(db, QStringLiteral("PRAGMA temp_store=MEMORY"), nullptr);
    execSql(db, QStringLiteral("PRAGMA foreign_keys=ON"), nullptr);

    static const QStringList schema {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_meta ("
            " key TEXT PRIMARY KEY,"
            " value TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_qso ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " source_key TEXT NOT NULL UNIQUE,"
            " call TEXT,"
            " grid TEXT,"
            " grid4 TEXT,"
            " band TEXT,"
            " mode TEXT,"
            " qso_date TEXT,"
            " time_on TEXT,"
            " frequency_mhz REAL,"
            " satellite TEXT,"
            " sat_mode TEXT,"
            " freq_rx_mhz REAL,"
            " confirmed INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_grid4 ON map_qso(grid4)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_band_mode ON map_qso(band, mode)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_confirmed ON map_qso(confirmed)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_qso_grid ("
            " qso_id INTEGER NOT NULL,"
            " grid TEXT NOT NULL,"
            " grid4 TEXT NOT NULL,"
            " grid6 TEXT,"
            " is_primary INTEGER NOT NULL DEFAULT 0,"
            " PRIMARY KEY(qso_id, grid),"
            " FOREIGN KEY(qso_id) REFERENCES map_qso(id) ON DELETE CASCADE)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_spot ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " unique_key TEXT NOT NULL UNIQUE,"
            " call TEXT NOT NULL,"
            " grid TEXT,"
            " grid4 TEXT,"
            " grid_origin TEXT NOT NULL DEFAULT 'UNKNOWN',"
            " band TEXT,"
            " mode TEXT,"
            " message TEXT,"
            " observed_utc TEXT NOT NULL,"
            " observed_ms INTEGER NOT NULL,"
            " frequency_hz INTEGER,"
            " snr INTEGER,"
            " source TEXT,"
            " hits INTEGER NOT NULL DEFAULT 1)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_observed ON map_spot(observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_band_mode ON map_spot(band, mode)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_grid4 ON map_spot(grid4)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_alert ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " alert_key TEXT NOT NULL UNIQUE,"
            " alert_type TEXT NOT NULL,"
            " call TEXT,"
            " grid TEXT,"
            " dxcc TEXT,"
            " message TEXT NOT NULL,"
            " created_ms INTEGER NOT NULL,"
            " is_read INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_alert_created ON map_alert(created_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_alert_unread ON map_alert(is_read, created_ms DESC)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_roster_preference ("
            " call TEXT PRIMARY KEY COLLATE NOCASE,"
            " watched INTEGER NOT NULL DEFAULT 0,"
            " ignored INTEGER NOT NULL DEFAULT 0,"
            " updated_ms INTEGER NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_roster_ignore ("
            " ignore_type TEXT NOT NULL COLLATE NOCASE,"
            " ignore_value TEXT NOT NULL COLLATE NOCASE,"
            " updated_ms INTEGER NOT NULL,"
            " PRIMARY KEY(ignore_type, ignore_value))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_roster_rule ("
            " rule_type TEXT NOT NULL COLLATE NOCASE,"
            " rule_value TEXT NOT NULL COLLATE NOCASE,"
            " rule_action TEXT NOT NULL,"
            " band TEXT NOT NULL DEFAULT '',"
            " mode TEXT NOT NULL DEFAULT '',"
            " enabled INTEGER NOT NULL DEFAULT 1,"
            " updated_ms INTEGER NOT NULL,"
            " PRIMARY KEY(rule_type, rule_value, band, mode))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_spot_event ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " spot_key TEXT NOT NULL,"
            " call TEXT, grid TEXT, receiver_call TEXT, receiver_grid TEXT,"
            " band TEXT, mode TEXT, source TEXT, provider TEXT,"
            " observed_ms INTEGER NOT NULL, frequency_hz INTEGER, snr INTEGER,"
            " correlation INTEGER NOT NULL DEFAULT 0, activity_type TEXT)")
    };
    for (QString const& ddl : schema) {
        if (!execSql(db, ddl, error)) {
            return false;
        }
    }

    struct ColumnMigration {
        const char* table;
        const char* column;
        const char* definition;
    };
    static const ColumnMigration migrations[] {
        {"map_qso", "qso_epoch", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "source", "TEXT NOT NULL DEFAULT 'ADIF'"},
        {"map_qso", "propagation_mode", "TEXT NOT NULL DEFAULT 'UNKNOWN'"},
        {"map_qso", "operator_call", "TEXT"},
        {"map_qso", "grid6", "TEXT"},
        {"map_qso", "dxcc", "TEXT"},
        {"map_qso", "dxcc_number", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "continent", "TEXT"},
        {"map_qso", "cq_zone", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "itu_zone", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "state", "TEXT"},
        {"map_qso", "county", "TEXT"},
        {"map_qso", "lotw_confirmed", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "eqsl_confirmed", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "oqrs", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "pota_ref", "TEXT"},
        {"map_qso", "iota", "TEXT"},
        {"map_qso", "wpx", "TEXT"},
        {"map_qso", "satellite", "TEXT"},
        {"map_qso", "sat_mode", "TEXT"},
        {"map_qso", "freq_rx_mhz", "REAL NOT NULL DEFAULT 0"},
        {"map_spot", "dxcc", "TEXT"},
        {"map_spot", "dxcc_number", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "continent", "TEXT"},
        {"map_spot", "cq_zone", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "itu_zone", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "state", "TEXT"},
        {"map_spot", "is_cq", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "target_call", "TEXT"},
        {"map_spot", "distance_km", "REAL NOT NULL DEFAULT -1"},
        {"map_spot", "grid6", "TEXT"},
        {"map_spot", "grid_origin", "TEXT NOT NULL DEFAULT 'UNKNOWN'"},
        {"map_spot", "dt", "REAL NOT NULL DEFAULT 0"},
        {"map_spot", "county", "TEXT"},
        {"map_spot", "pota_ref", "TEXT"},
        {"map_spot", "iota", "TEXT"},
        {"map_spot", "wpx", "TEXT"},
        {"map_spot", "activity_type", "TEXT NOT NULL DEFAULT 'LIVE'"},
        {"map_spot", "receiver_call", "TEXT"},
        {"map_spot", "receiver_grid", "TEXT"},
        {"map_spot", "provider", "TEXT"},
        {"map_spot", "first_observed_ms", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "last_observed_ms", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "correlation_count", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "direction", "TEXT NOT NULL DEFAULT 'RX'"},
        {"map_spot", "propagation_mode", "TEXT NOT NULL DEFAULT 'UNKNOWN'"},
        {"map_spot_event", "propagation_mode", "TEXT NOT NULL DEFAULT 'UNKNOWN'"},
        {"map_spot_event", "direction", "TEXT NOT NULL DEFAULT 'RX'"}
    };
    for (ColumnMigration const& migration : migrations) {
        if (!ensureColumn(db,
                          QString::fromLatin1(migration.table),
                          QString::fromLatin1(migration.column),
                          QString::fromLatin1(migration.definition),
                          error)) {
            return false;
        }
    }
    // Existing databases predate grid provenance.  Infer only the origin of
    // their already stored locator; a decoded value is still never replaced by
    // data of lower confidence.
    if (!execSql(db, QStringLiteral(
            "UPDATE map_spot SET grid_origin=CASE"
            " WHEN lower(source)='decoder' AND grid<>'' THEN 'DECODED'"
            " WHEN lower(source)='psk' AND grid<>'' THEN 'PSK'"
            " WHEN lower(source)='oams' AND grid<>'' THEN 'OAMS'"
            " ELSE 'UNKNOWN' END"
            " WHERE COALESCE(grid_origin, 'UNKNOWN')='UNKNOWN'"), error)) {
        return false;
    }
    static const QStringList extendedIndexes {
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_period ON map_qso(qso_epoch DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_geo ON map_qso(continent, dxcc, cq_zone)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_source ON map_qso(source)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_propagation ON map_qso(propagation_mode, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_call_status ON map_qso(call, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_grid_status ON map_qso(grid4, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_grid6_status ON map_qso(grid6, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_dxcc_status ON map_qso(dxcc, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_dxcc_number ON map_qso(dxcc_number, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_pota ON map_qso(pota_ref, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_iota ON map_qso(iota, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_wpx ON map_qso(wpx, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_county ON map_qso(county, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_satellite ON map_qso(satellite, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_grid_qso ON map_qso_grid(qso_id, is_primary)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_grid_grid4 ON map_qso_grid(grid4, qso_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_grid_grid6 ON map_qso_grid(grid6, qso_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_geo ON map_spot(continent, dxcc, cq_zone)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_source_cq ON map_spot(source, is_cq)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_call_time ON map_spot(call, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_grid6 ON map_spot(grid6)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_activity_time ON map_spot(activity_type, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_receiver_time ON map_spot(receiver_call, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_correlation ON map_spot(correlation_count, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_propagation ON map_spot(propagation_mode, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_event_time ON map_spot_event(observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_event_propagation ON map_spot_event(propagation_mode, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_event_grid ON map_spot_event(grid, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_event_band_window ON map_spot_event(band, observed_ms DESC, source, direction)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_roster_ignore_type ON map_roster_ignore(ignore_type, ignore_value)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_roster_rule_type ON map_roster_rule(rule_type, rule_value)")
    };
    for (QString const& ddl : extendedIndexes) {
        if (!execSql(db, ddl, error)) {
            return false;
        }
    }
    if (!execSql(db,
                 QStringLiteral(
                     "UPDATE map_qso SET grid6=upper(substr(grid,1,6))"
                     " WHERE (grid6 IS NULL OR grid6='') AND length(grid)>=6"),
                 error)
        || !execSql(db,
                    QStringLiteral(
                        "UPDATE map_spot SET grid6=upper(substr(grid,1,6))"
                        " WHERE (grid6 IS NULL OR grid6='') AND length(grid)>=6"),
                    error)
        || !execSql(db,
                    QStringLiteral(
                        "INSERT OR IGNORE INTO map_qso_grid(qso_id, grid, grid4, grid6, is_primary)"
                        " SELECT id, upper(grid), upper(grid4), nullif(upper(grid6), ''), 1"
                        " FROM map_qso WHERE COALESCE(grid4, '')<>''"),
                    error)
        || !execSql(db,
                    QStringLiteral(
                        "UPDATE map_spot SET first_observed_ms=observed_ms, last_observed_ms=observed_ms"
                        " WHERE first_observed_ms=0 OR last_observed_ms=0"),
                    error)
        || !execSql(db,
                    QStringLiteral(
                        "UPDATE map_qso SET propagation_mode='UNKNOWN'"
                        " WHERE propagation_mode IS NULL OR trim(propagation_mode)=''"),
                    error)
        || !execSql(db,
                    QStringLiteral(
                        "UPDATE map_spot SET propagation_mode='UNKNOWN'"
                        " WHERE propagation_mode IS NULL OR trim(propagation_mode)=''"),
                    error)
        || !execSql(db,
                    QStringLiteral(
                        "UPDATE map_spot_event SET propagation_mode='UNKNOWN'"
                        " WHERE propagation_mode IS NULL OR trim(propagation_mode)=''"),
                    error)) {
        return false;
    }
    if (!repairDecoderSpotAttribution(db, error)) {
        return false;
    }

    *connection = std::move(candidate);
    return true;
}

QString adifFingerprint(const QString& path)
{
    QFileInfo const info(path);
    return digestKey({
        QStringLiteral("map-adif-import-v%1").arg(kAdifImportFormatVersion),
        info.absoluteFilePath(),
        QString::number(info.size()),
        QString::number(info.lastModified().toMSecsSinceEpoch())
    });
}

QStringList callsFromMessage(const QString& message)
{
    static const QRegularExpression callPattern(
        QStringLiteral(R"(\b(?:[A-Z0-9]{1,4}/)?[A-Z0-9]{1,3}[0-9][A-Z0-9]{1,4}(?:/[A-Z0-9]{1,4})?\b)"),
        QRegularExpression::CaseInsensitiveOption);
    QStringList calls;
    QRegularExpressionMatchIterator matches = callPattern.globalMatch(message.toUpper());
    while (matches.hasNext()) {
        QString const token = matches.next().captured(0);
        if (!token.startsWith(QStringLiteral("RR"))
            && token != QStringLiteral("73")
            // A four-character Maidenhead locator such as JN70 also matches
            // the loose callsign expression.  It is never a station call.
            && normalizedGrid(token).isEmpty()) {
            calls.append(token);
        }
    }
    return calls;
}

bool isGeneralCallMessage(const QString& message)
{
    QString const firstToken =
        message.trimmed().toUpper().section(QLatin1Char(' '), 0, 0);
    return firstToken == QStringLiteral("CQ")
        || firstToken == QStringLiteral("QRZ")
        || firstToken == QStringLiteral("DE")
        || firstToken == QStringLiteral("BEACON");
}

QString transmittingCallFromMessage(const QString& message)
{
    QStringList const calls = callsFromMessage(message);
    if (calls.isEmpty()) {
        return {};
    }
    if (isGeneralCallMessage(message) || calls.size() == 1) {
        return calls.first();
    }
    // In a directed standard message the addressee is first and the station
    // transmitting the message is second: "TO_CALL FROM_CALL payload".
    return calls.at(1);
}

QString targetCallFromMessage(const QString& message)
{
    if (isGeneralCallMessage(message)) {
        return {};
    }
    QStringList const calls = callsFromMessage(message);
    return calls.size() >= 2 ? calls.first() : QString();
}

bool repairDecoderSpotAttribution(QSqlDatabase& db, QString* error)
{
    constexpr int kAttributionVersion = 1;
    constexpr auto kMetaKey = "decoder_sender_attribution_version";

    QSqlQuery versionQuery(db);
    versionQuery.prepare(QStringLiteral("SELECT value FROM map_meta WHERE key=:key"));
    versionQuery.bindValue(QStringLiteral(":key"), QString::fromLatin1(kMetaKey));
    if (!versionQuery.exec()) {
        if (error) *error = versionQuery.lastError().text();
        return false;
    }
    if (versionQuery.next() && versionQuery.value(0).toInt() >= kAttributionVersion) {
        return true;
    }

    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    QSqlQuery select(db);
    if (!select.exec(QStringLiteral(
            "SELECT id, unique_key, call, message, target_call, dxcc, continent,"
            " cq_zone, itu_zone, state"
            " FROM map_spot WHERE lower(coalesce(source, 'decoder'))='decoder'"))) {
        if (error) *error = select.lastError().text();
        db.rollback();
        return false;
    }

    QSqlQuery updateSpot(db);
    if (!updateSpot.prepare(QStringLiteral(
            "UPDATE map_spot SET call=:call, target_call=:target_call,"
            " dxcc=:dxcc, continent=:continent, cq_zone=:cq_zone,"
            " itu_zone=:itu_zone, state=:state WHERE id=:id"))) {
        if (error) *error = updateSpot.lastError().text();
        db.rollback();
        return false;
    }
    QSqlQuery updateEvents(db);
    if (!updateEvents.prepare(QStringLiteral(
            "UPDATE map_spot_event SET call=:call WHERE spot_key=:spot_key"))) {
        if (error) *error = updateEvents.lastError().text();
        db.rollback();
        return false;
    }

    std::shared_ptr<const DxccLookup> const lookup = adifDxccLookup();
    int repaired = 0;
    while (select.next()) {
        qint64 const id = select.value(0).toLongLong();
        QString const spotKey = select.value(1).toString();
        QString const storedCall = select.value(2).toString().trimmed().toUpper();
        QString const message = select.value(3).toString();
        QString const storedTarget = select.value(4).toString().trimmed().toUpper();
        QString const correctedCall = transmittingCallFromMessage(message);
        QString const correctedTarget = targetCallFromMessage(message);
        if (correctedCall.isEmpty()
            || (correctedCall == storedCall && correctedTarget == storedTarget)) {
            continue;
        }

        QString dxcc = select.value(5).toString();
        QString continent = select.value(6).toString();
        int cqZone = select.value(7).toInt();
        int ituZone = select.value(8).toInt();
        QString state = select.value(9).toString();
        if (correctedCall != storedCall) {
            dxcc.clear();
            continent.clear();
            cqZone = 0;
            ituZone = 0;
            state.clear();
            if (lookup) {
                DxccEntity const entity = lookup->lookup(correctedCall);
                if (entity.isValid()) {
                    dxcc = entity.name;
                    continent = entity.continent.toUpper();
                    cqZone = entity.cqZone;
                    ituZone = entity.ituZone;
                }
            }
        }

        updateSpot.bindValue(QStringLiteral(":call"), correctedCall);
        updateSpot.bindValue(QStringLiteral(":target_call"), correctedTarget);
        updateSpot.bindValue(QStringLiteral(":dxcc"), dxcc);
        updateSpot.bindValue(QStringLiteral(":continent"), continent);
        updateSpot.bindValue(QStringLiteral(":cq_zone"), cqZone);
        updateSpot.bindValue(QStringLiteral(":itu_zone"), ituZone);
        updateSpot.bindValue(QStringLiteral(":state"), state);
        updateSpot.bindValue(QStringLiteral(":id"), id);
        if (!updateSpot.exec()) {
            if (error) *error = updateSpot.lastError().text();
            db.rollback();
            return false;
        }

        updateEvents.bindValue(QStringLiteral(":call"), correctedCall);
        updateEvents.bindValue(QStringLiteral(":spot_key"), spotKey);
        if (!updateEvents.exec()) {
            if (error) *error = updateEvents.lastError().text();
            db.rollback();
            return false;
        }
        ++repaired;
    }

    QSqlQuery setVersion(db);
    setVersion.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO map_meta(key, value) VALUES(:key, :value)"));
    setVersion.bindValue(QStringLiteral(":key"), QString::fromLatin1(kMetaKey));
    setVersion.bindValue(QStringLiteral(":value"), kAttributionVersion);
    if (!setVersion.exec()) {
        if (error) *error = setVersion.lastError().text();
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    if (repaired > 0) {
        qInfo().noquote()
            << "[MAPINT] repaired decoder sender attribution rows=" << repaired;
    }
    return true;
}

QString gridFromMessage(const QString& message)
{
    static const QRegularExpression gridPattern(
        QStringLiteral(R"(\b([A-R]{2}[0-9]{2}(?:[A-X]{2})?)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    return normalizedGrid(gridPattern.match(message).captured(1));
}

bool messageAssociatesGridWithCall(const QString& message, const QString& call)
{
    QString const normalizedCall = call.trimmed().toUpper();
    QString const grid = gridFromMessage(message);
    if (normalizedCall.isEmpty() || grid.isEmpty()) {
        return false;
    }

    QStringList const tokens = message.toUpper().simplified().split(QLatin1Char(' '),
                                                                       Qt::SkipEmptyParts);
    int callIndex = -1;
    int gridIndex = -1;
    for (int index = 0; index < tokens.size(); ++index) {
        QString const token = tokens.at(index).trimmed();
        if (gridIndex < 0 && normalizedGrid(token) == grid) {
            gridIndex = index;
        }
        if (callIndex < 0 && token == normalizedCall) {
            callIndex = index;
        }
    }
    return callIndex >= 0 && gridIndex >= 0 && callIndex < gridIndex;
}

qint64 adifEpoch(const QString& date, const QString& time)
{
    QString const dateDigits = date.trimmed();
    if (!QRegularExpression(QStringLiteral("^\\d{8}$")).match(dateDigits).hasMatch()) {
        return 0;
    }

    QString digits = time.trimmed();
    while (digits.size() < 6) {
        digits.append(QLatin1Char('0'));
    }
    if (!QRegularExpression(QStringLiteral("^\\d{6}$")).match(digits.left(6)).hasMatch()) {
        return 0;
    }

    QDate const qsoDate = QDate::fromString(dateDigits, QStringLiteral("yyyyMMdd"));
    QTime const qsoTime = QTime::fromString(digits.left(6), QStringLiteral("hhmmss"));
    if (!qsoDate.isValid() || !qsoTime.isValid()) {
        return 0;
    }
    return QDateTime(qsoDate, qsoTime, QTimeZone::UTC).toMSecsSinceEpoch();
}

qint64 periodCutoffMs(const QString& period)
{
    qint64 const now = QDateTime::currentMSecsSinceEpoch();
    QString const value = period.trimmed().toLower();
    if (value == QStringLiteral("1 hour")) return now - 60LL * 60LL * 1000LL;
    if (value == QStringLiteral("24 hours")) return now - 24LL * 60LL * 60LL * 1000LL;
    if (value == QStringLiteral("7 days")) return now - 7LL * 24LL * 60LL * 60LL * 1000LL;
    if (value == QStringLiteral("30 days")) return now - 30LL * 24LL * 60LL * 60LL * 1000LL;
    if (value == QStringLiteral("1 year")) return now - 365LL * 24LL * 60LL * 60LL * 1000LL;
    return 0;
}

QString normalizedFilter(const QString& value)
{
    return value.trimmed().isEmpty() ? QStringLiteral("All") : value.trimmed();
}

QStringList defaultRosterWantedTypes()
{
    return {QStringLiteral("CALL"), QStringLiteral("GRID"),
            QStringLiteral("DXCC"), QStringLiteral("WPX"),
            QStringLiteral("POTA"), QStringLiteral("CQ"),
            QStringLiteral("ITU"), QStringLiteral("STATE"),
            QStringLiteral("COUNTY"), QStringLiteral("CONTINENT")};
}

QString potaFromMessage(const QString& message)
{
    static const QRegularExpression expression(
        QStringLiteral("\\b([A-Z0-9]{1,4}-[0-9]{1,6})\\b"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator iterator = expression.globalMatch(message);
    while (iterator.hasNext()) {
        QString const candidate = normalizedPota(iterator.next().captured(1));
        if (!candidate.isEmpty()) {
            return candidate;
        }
    }
    return {};
}

QString rosterOrderColumn(const QString& sort)
{
    QString const value = sort.trimmed().toLower();
    if (value == QStringLiteral("call")) return QStringLiteral("s.call");
    if (value == QStringLiteral("snr")) return QStringLiteral("s.snr");
    if (value == QStringLiteral("distance")) return QStringLiteral("s.distance_km");
    if (value == QStringLiteral("dxcc")) return QStringLiteral("s.dxcc");
    if (value == QStringLiteral("grid")) return QStringLiteral("s.grid");
    return QStringLiteral("s.observed_ms");
}

bool isWasState(QString state)
{
    static const QSet<QString> states {
        QStringLiteral("AL"), QStringLiteral("AK"), QStringLiteral("AZ"),
        QStringLiteral("AR"), QStringLiteral("CA"), QStringLiteral("CO"),
        QStringLiteral("CT"), QStringLiteral("DE"), QStringLiteral("FL"),
        QStringLiteral("GA"), QStringLiteral("HI"), QStringLiteral("ID"),
        QStringLiteral("IL"), QStringLiteral("IN"), QStringLiteral("IA"),
        QStringLiteral("KS"), QStringLiteral("KY"), QStringLiteral("LA"),
        QStringLiteral("ME"), QStringLiteral("MD"), QStringLiteral("MA"),
        QStringLiteral("MI"), QStringLiteral("MN"), QStringLiteral("MS"),
        QStringLiteral("MO"), QStringLiteral("MT"), QStringLiteral("NE"),
        QStringLiteral("NV"), QStringLiteral("NH"), QStringLiteral("NJ"),
        QStringLiteral("NM"), QStringLiteral("NY"), QStringLiteral("NC"),
        QStringLiteral("ND"), QStringLiteral("OH"), QStringLiteral("OK"),
        QStringLiteral("OR"), QStringLiteral("PA"), QStringLiteral("RI"),
        QStringLiteral("SC"), QStringLiteral("SD"), QStringLiteral("TN"),
        QStringLiteral("TX"), QStringLiteral("UT"), QStringLiteral("VT"),
        QStringLiteral("VA"), QStringLiteral("WA"), QStringLiteral("WV"),
        QStringLiteral("WI"), QStringLiteral("WY")
    };
    return states.contains(state.trimmed().toUpper());
}

bool isLower48State(QString state)
{
    state = state.trimmed().toUpper();
    return isWasState(state)
        && state != QStringLiteral("AK")
        && state != QStringLiteral("HI");
}

} // namespace

MapIntelligenceService::MapIntelligenceService(QObject* parent,
                                               const QString& databasePath)
    : QObject(parent)
    , m_layerModel(new MapLayerModel(this))
    , m_pskFeedService(new MapPskFeedService(this))
    , m_databasePath(databasePath.trimmed().isEmpty()
          ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                .absoluteFilePath(QStringLiteral("map_intelligence.sqlite"))
          : QFileInfo(databasePath).absoluteFilePath())
{
    m_workerPool.setMaxThreadCount(1);
    m_workerPool.setExpiryTimeout(30000);
    m_workerPool.setThreadPriority(QThread::LowPriority);

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("LiveMapLayers"));
    constexpr int kLayerPreferencesVersion = 2;
    int const savedLayerPreferencesVersion =
        settings.value(QStringLiteral("PreferencesVersion"), 0).toInt();
    // Older builds allowed the data-view selector to silently overwrite Live.
    // Restore the intended default once, then preserve every explicit choice.
    bool const migrateLiveDefault =
        savedLayerPreferencesVersion < kLayerPreferencesVersion;
    // Prior versions called this purely visual checkbox "OFFLINE BASE MAP".
    // It did not control network activity, therefore it must not silently
    // become the new operational Offline mode after an upgrade.
    bool const migrateOfflineMode = savedLayerPreferencesVersion < 2;
    bool const hasLivePreference = settings.contains(QStringLiteral("Live"));
    m_bandFilter = settings.value(QStringLiteral("Band"), QStringLiteral("All")).toString();
    m_modeFilter = settings.value(QStringLiteral("Mode"), QStringLiteral("All")).toString();
    m_periodFilter = settings.value(QStringLiteral("Period"), QStringLiteral("All time")).toString();
    m_continentFilter = settings.value(QStringLiteral("Continent"), QStringLiteral("All")).toString();
    m_dxccFilter = settings.value(QStringLiteral("Dxcc"), QStringLiteral("All")).toString();
    m_sourceFilter = settings.value(QStringLiteral("Source"), QStringLiteral("All")).toString();
    m_propagationFilter = settings.value(QStringLiteral("PropagationFilter"), QStringLiteral("MIXED"))
                              .toString().trimmed().toUpper();
    if (!availablePropagationModes().contains(m_propagationFilter, Qt::CaseInsensitive)) {
        m_propagationFilter = QStringLiteral("MIXED");
    }
    m_cqOnly = settings.value(QStringLiteral("CqOnly"), false).toBool();
    m_rosterSort = settings.value(QStringLiteral("RosterSort"), QStringLiteral("Time")).toString();
    m_rosterSortDescending =
        settings.value(QStringLiteral("RosterSortDescending"), true).toBool();
    m_rosterStatusFilter =
        settings.value(QStringLiteral("RosterStatus"), QStringLiteral("All")).toString();
    m_rosterHuntScope =
        settings.value(QStringLiteral("RosterHuntScope"), QStringLiteral("All time")).toString();
    m_rosterScope = settings.value(QStringLiteral("RosterScope"), m_rosterScope).toString();
    if (!QStringList {QStringLiteral("Current band"), QStringLiteral("Current mode"),
                      QStringLiteral("Digital modes"), QStringLiteral("All bands"),
                      QStringLiteral("Award selected")}
            .contains(m_rosterScope, Qt::CaseInsensitive)) {
        m_rosterScope = QStringLiteral("All bands");
    }
    m_rosterDxccScope =
        settings.value(QStringLiteral("RosterDxccScope"), m_rosterDxccScope).toString();
    if (!QStringList {QStringLiteral("All"), QStringLiteral("Same DXCC"),
                      QStringLiteral("Other DXCC")}
            .contains(m_rosterDxccScope, Qt::CaseInsensitive)) {
        m_rosterDxccScope = QStringLiteral("All");
    }
    m_rosterUsesLoTW = settings.value(QStringLiteral("RosterUsesLoTW"), false).toBool();
    m_rosterMaxLoTWDays = qBound(1,
        settings.value(QStringLiteral("RosterMaxLoTWDays"), 810).toInt(), 3650);
    m_rosterUsesEQSL = settings.value(QStringLiteral("RosterUsesEQSL"), false).toBool();
    m_rosterUsesOQRS = settings.value(QStringLiteral("RosterUsesOQRS"), false).toBool();
    m_rosterSpottedMeOnly =
        settings.value(QStringLiteral("RosterSpottedMeOnly"), false).toBool();
    m_rosterMinSnrEnabled =
        settings.value(QStringLiteral("RosterMinSnrEnabled"), false).toBool();
    m_rosterMinSnr = qBound(-60, settings.value(QStringLiteral("RosterMinSnr"), -25).toInt(), 30);
    m_rosterMaxDtEnabled =
        settings.value(QStringLiteral("RosterMaxDtEnabled"), false).toBool();
    m_rosterMaxDt = qBound(0.01,
                           settings.value(QStringLiteral("RosterMaxDt"), 0.5).toDouble(), 10.0);
    m_rosterTreatRr73AsCq =
        settings.value(QStringLiteral("RosterTreatRr73AsCq"), false).toBool();
    QStringList const savedWantedTypes =
        settings.value(QStringLiteral("RosterWantedTypes"), m_rosterWantedTypes).toStringList();
    m_rosterWantedTypes.clear();
    for (QString const& type : savedWantedTypes) {
        int const index = defaultRosterWantedTypes().indexOf(type.trimmed().toUpper());
        if (index >= 0 && !m_rosterWantedTypes.contains(defaultRosterWantedTypes().at(index))) {
            m_rosterWantedTypes.append(defaultRosterWantedTypes().at(index));
        }
    }
    m_rosterRetentionMinutes =
        qBound(1, settings.value(QStringLiteral("RosterRetentionMinutes"), 5).toInt(), 60);
    m_rosterCqOnly = settings.value(QStringLiteral("RosterCqOnly"), false).toBool();
    m_rosterTextFilter =
        settings.value(QStringLiteral("RosterTextFilter"), QString()).toString();
    m_rosterTextMode =
        settings.value(QStringLiteral("RosterTextMode"),
                       QStringLiteral("No filter")).toString();
    static const QStringList rosterTextModes {
        QStringLiteral("No filter"), QStringLiteral("Only"),
        QStringLiteral("Exclude"), QStringLiteral("Regex")
    };
    if (!rosterTextModes.contains(m_rosterTextMode, Qt::CaseInsensitive)) {
        m_rosterTextMode = QStringLiteral("No filter");
    }
    m_activeAwardProgram =
        settings.value(QStringLiteral("ActiveAwardProgram"), QStringLiteral("None")).toString();
    if (!availableAwardPrograms().contains(m_activeAwardProgram, Qt::CaseInsensitive)) {
        m_activeAwardProgram = QStringLiteral("None");
    }
    m_awardGoal =
        settings.value(QStringLiteral("AwardGoal"), QStringLiteral("Confirmed")).toString();
    if (!availableAwardGoals().contains(m_awardGoal, Qt::CaseInsensitive)) {
        m_awardGoal = QStringLiteral("Confirmed");
    }
    m_awardEndorsement = settings.value(QStringLiteral("AwardEndorsement")).toString();
    m_awardConfirmation = settings.value(QStringLiteral("AwardConfirmation"),
                                         QStringLiteral("Any")).toString();
    if (!availableAwardConfirmations().contains(m_awardConfirmation, Qt::CaseInsensitive)) {
        m_awardConfirmation = QStringLiteral("Any");
    }
    m_awardCallsign = settings.value(QStringLiteral("AwardCallsign")).toString().trimmed().toUpper();
    m_awardFromDate = settings.value(QStringLiteral("AwardFromDate")).toString().trimmed();
    m_awardToDate = settings.value(QStringLiteral("AwardToDate")).toString().trimmed();
    m_gridPrecision =
        settings.value(QStringLiteral("GridPrecision"), 4).toInt() == 6 ? 6 : 4;
    m_liveDecayMinutes =
        qBound(1, settings.value(QStringLiteral("LiveDecayMinutes"), 15).toInt(), 120);
    m_sourceDecayMinutes.insert(
        QStringLiteral("decoder"), m_liveDecayMinutes);
    m_sourceDecayMinutes.insert(
        QStringLiteral("psk"), qBound(1,
            settings.value(QStringLiteral("Decay/psk"), 60).toInt(), 240));
    m_sourceDecayMinutes.insert(
        QStringLiteral("oams"), qBound(1,
            settings.value(QStringLiteral("Decay/oams"), 30).toInt(), 240));
    m_splitGridEnabled =
        settings.value(QStringLiteral("SplitGridEnabled"), true).toBool();
    m_coveragePushPinsEnabled =
        settings.value(QStringLiteral("CoveragePushPinsEnabled"), false).toBool();
    m_timeZoneOverlayEnabled =
        settings.value(QStringLiteral("TimeZoneOverlayEnabled"), false).toBool();
    m_pskDisplayMode =
        settings.value(QStringLiteral("PskDisplayMode"), QStringLiteral("Overlay")).toString();
    if (!availablePskDisplayModes().contains(m_pskDisplayMode, Qt::CaseInsensitive)) {
        m_pskDisplayMode = QStringLiteral("Overlay");
    }
    m_pskOpacityPercent =
        qBound(20, settings.value(QStringLiteral("PskOpacityPercent"), 65).toInt(), 100);
    m_spotAgeFilter = normalizedSpotAgeFilter(
        settings.value(QStringLiteral("SpotAgeFilter"),
                       QStringLiteral("15 min")).toString());
    if (m_spotAgeFilter.isEmpty()) {
        m_spotAgeFilter = QStringLiteral("15 min");
    }
    m_spotCorrelationFilter = settings.value(QStringLiteral("SpotCorrelationFilter"),
                                             QStringLiteral("All")).toString();
    if (!availableCorrelationFilters().contains(m_spotCorrelationFilter,
                                                Qt::CaseInsensitive)) {
        m_spotCorrelationFilter = QStringLiteral("All");
    }
    m_bandActivityWindowHours =
        settings.value(QStringLiteral("BandActivityWindowHours"), 6).toInt();
    if (m_bandActivityWindowHours != 1 && m_bandActivityWindowHours != 6
        && m_bandActivityWindowHours != 12 && m_bandActivityWindowHours != 24) {
        m_bandActivityWindowHours = 6;
    }
    m_rosterVisibleColumns = settings.value(QStringLiteral("RosterVisibleColumns"),
                                             m_rosterVisibleColumns).toStringList();
    if (m_rosterVisibleColumns.isEmpty()) {
        m_rosterVisibleColumns = availableRosterColumns();
    }
    m_pskFeedService->setEndpoint(settings.value(QStringLiteral("PskMqttEndpoint"),
                                                  QStringLiteral("mqtt://mqtt.pskreporter.info:1883"))
                                      .toString());
    m_callLookupProvider =
        settings.value(QStringLiteral("CallLookupProvider"), QStringLiteral("QRZ")).toString();
    if (!availableCallLookupProviders().contains(m_callLookupProvider, Qt::CaseInsensitive)) {
        m_callLookupProvider = QStringLiteral("QRZ");
    }
    m_alertNewGridEnabled =
        settings.value(QStringLiteral("AlertNewGrid"), true).toBool();
    m_alertNewDxccEnabled =
        settings.value(QStringLiteral("AlertNewDxcc"), true).toBool();
    m_alertCqEnabled =
        settings.value(QStringLiteral("AlertCq"), true).toBool();
    m_alertCallPattern =
        settings.value(QStringLiteral("AlertCallPattern"), QString()).toString().trimmed().left(64);
    m_layerModel->setLayerEnabled(QStringLiteral("worked"),
                                  settings.value(QStringLiteral("Worked"), true).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("confirmed"),
                                  settings.value(QStringLiteral("Confirmed"), true).toBool());
    m_layerModel->setLayerEnabled(
        QStringLiteral("live"),
        migrateLiveDefault || settings.value(QStringLiteral("Live"), true).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("active"),
                                  settings.value(QStringLiteral("Active"), true).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("missing"),
                                  settings.value(QStringLiteral("Missing"), true).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("psk"),
                                  settings.value(QStringLiteral("Psk"), true).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("pota"),
                                  settings.value(QStringLiteral("Pota"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("states"),
                                  settings.value(QStringLiteral("States"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("counties"),
                                  settings.value(QStringLiteral("Counties"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("iota"),
                                  settings.value(QStringLiteral("Iota"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("wpx"),
                                  settings.value(QStringLiteral("Wpx"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("moon"),
                                  settings.value(QStringLiteral("Moon"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("propagation"),
                                  settings.value(QStringLiteral("Propagation"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("radar"),
                                  settings.value(QStringLiteral("Radar"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("lightning"),
                                  settings.value(QStringLiteral("Lightning"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("muf"),
                                  settings.value(QStringLiteral("Muf"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("fof2"),
                                  settings.value(QStringLiteral("Fof2"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("es"),
                                  settings.value(QStringLiteral("Es"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("aurora"),
                                  settings.value(QStringLiteral("Aurora"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("tropo"),
                                  settings.value(QStringLiteral("Tropo"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("earthquakes"),
                                  settings.value(QStringLiteral("Earthquakes"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("wildfires"),
                                  settings.value(QStringLiteral("Wildfires"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("offline"),
                                  migrateOfflineMode
                                      ? false
                                      : settings.value(QStringLiteral("Offline"), false).toBool());

    // Layer appearance is part of the map preference, not a transient QML
    // choice.  Keep it beside the legacy enabled flags so old profiles remain
    // fully compatible while every layer gains independent style controls.
    for (int row = 0; row < m_layerModel->rowCount(); ++row) {
        QModelIndex const index = m_layerModel->index(row, 0);
        QString const id = m_layerModel->data(
            index, MapLayerModel::LayerIdRole).toString();
        QVariantMap const defaults = m_layerModel->layerStyle(id);
        m_layerModel->setLayerStyle(
            id,
            settings.value(QStringLiteral("Style/%1/Color").arg(id),
                           defaults.value(QStringLiteral("color"))).toString(),
            settings.value(QStringLiteral("Style/%1/Opacity").arg(id),
                           defaults.value(QStringLiteral("opacity"))).toDouble(),
            settings.value(QStringLiteral("Style/%1/Thickness").arg(id),
                           defaults.value(QStringLiteral("thickness"))).toDouble(),
            settings.value(QStringLiteral("Style/%1/LabelDensity").arg(id),
                           defaults.value(QStringLiteral("labelDensity"))).toInt());
    }

    if (migrateLiveDefault || !hasLivePreference) {
        settings.setValue(QStringLiteral("Live"), true);
    }
    if (migrateOfflineMode) {
        settings.setValue(QStringLiteral("Offline"), false);
    }
    settings.setValue(QStringLiteral("PreferencesVersion"),
                      kLayerPreferencesVersion);
    settings.sync();

    if (m_layerModel->layerEnabled(QStringLiteral("propagation"))) {
        static const QStringList propagationLayers {
            QStringLiteral("muf"), QStringLiteral("fof2"),
            QStringLiteral("es"), QStringLiteral("aurora")
        };
        for (QString const& layer : propagationLayers) {
            m_layerModel->setLayerEnabled(layer, true);
        }
    }

    connect(m_layerModel, &MapLayerModel::layerToggled, this,
            [this](QString const& id, bool enabled) {
        saveSetting(id.left(1).toUpper() + id.mid(1), enabled);
        if (id == QStringLiteral("worked")) {
            emit workedLayerEnabledChanged();
            rebuildVisibleCoverage();
        } else if (id == QStringLiteral("confirmed")) {
            emit confirmedLayerEnabledChanged();
            rebuildVisibleCoverage();
        } else if (id == QStringLiteral("live")) {
            emit liveLayerEnabledChanged();
            rebuildVisibleCoverage();
        } else if (id == QStringLiteral("active")) {
            emit activeLayerEnabledChanged();
            rebuildVisibleCoverage();
        } else if (id == QStringLiteral("missing")) {
            emit missingLayerEnabledChanged();
            rebuildVisibleCoverage();
        } else if (id == QStringLiteral("psk")) {
            emit pskLayerEnabledChanged();
            scheduleQuery();
        } else if (id == QStringLiteral("offline")) {
            setOfflineMode(enabled);
        } else if (id == QStringLiteral("propagation")) {
            static const QStringList propagationLayers {
                QStringLiteral("muf"), QStringLiteral("fof2"),
                QStringLiteral("es"), QStringLiteral("aurora")
            };
            for (QString const& layer : propagationLayers) {
                m_layerModel->setLayerEnabled(layer, enabled);
            }
        }
    });

    connect(m_layerModel, &MapLayerModel::layerStyleChanged, this,
            [this](const QString& id) {
        QVariantMap const style = m_layerModel->layerStyle(id);
        saveSetting(QStringLiteral("Style/%1/Color").arg(id),
                    style.value(QStringLiteral("color")));
        saveSetting(QStringLiteral("Style/%1/Opacity").arg(id),
                    style.value(QStringLiteral("opacity")));
        saveSetting(QStringLiteral("Style/%1/Thickness").arg(id),
                    style.value(QStringLiteral("thickness")));
        saveSetting(QStringLiteral("Style/%1/LabelDensity").arg(id),
                    style.value(QStringLiteral("labelDensity")));
    });

    m_baseMapService = new MapBaseMapService(this);
    m_externalOverlayService =
        new MapExternalOverlayService(m_layerModel, this);
    {
        std::unique_ptr<ScopedSqliteConnection> connection;
        QString error;
        if (!openMapDatabase(m_databasePath, &connection, &error)) {
            qWarning().noquote()
                << "[MAPINT] database initialization failed:" << error;
        }
    }
    m_operationsService =
        new MapOperationsService(m_databasePath, m_layerModel, this);
    connect(m_pskFeedService, &MapPskFeedService::spotsReceived,
            this, &MapIntelligenceService::ingestPskSpots);
    connect(m_pskFeedService, &MapPskFeedService::enabledChanged, this, [this] {
        saveSetting(QStringLiteral("PskMqttEnabled"), m_pskFeedService->enabled());
    });
    connect(m_pskFeedService, &MapPskFeedService::endpointChanged, this, [this] {
        saveSetting(QStringLiteral("PskMqttEndpoint"), m_pskFeedService->endpoint());
    });
    m_pskFeedService->setEnabled(
        settings.value(QStringLiteral("PskMqttEnabled"), false).toBool());
    setOfflineMode(m_layerModel->layerEnabled(QStringLiteral("offline")));

    m_queryTimer = new QTimer(this);
    m_queryTimer->setSingleShot(true);
    m_queryTimer->setInterval(60);
    connect(m_queryTimer, &QTimer::timeout, this, [this] {
        queueSnapshotQuery(m_queryGeneration.load());
    });

    m_liveFlushTimer = new QTimer(this);
    m_liveFlushTimer->setSingleShot(true);
    m_liveFlushTimer->setInterval(250);
    connect(m_liveFlushTimer, &QTimer::timeout,
            this, &MapIntelligenceService::flushPendingLiveSpots);

    // Keep only the newest map result and publish it at a bounded cadence.
    // A decode/PSK burst must not fan out into repeated heavy QML updates.
    m_snapshotFlushTimer = new QTimer(this);
    m_snapshotFlushTimer->setSingleShot(true);
    // SQLite results may update multiple map domains at once. A half-second
    // coalescing window keeps the Live Map current without repeatedly waking
    // the QML scene graph during an RX burst.
    m_snapshotFlushTimer->setInterval(500);
    connect(m_snapshotFlushTimer, &QTimer::timeout,
            this, &MapIntelligenceService::flushPendingSnapshot);

    // Publishing one SQLite snapshot can touch several independent QML
    // surfaces. Deliver the current state in short groups so a decode burst
    // cannot turn into one long main-thread MetaCall on Windows.
    m_snapshotNotificationTimer = new QTimer(this);
    m_snapshotNotificationTimer->setSingleShot(true);
    connect(m_snapshotNotificationTimer, &QTimer::timeout,
            this, &MapIntelligenceService::flushSnapshotNotifications);

    scheduleQuery();
}

MapIntelligenceService::~MapIntelligenceService()
{
    ++m_queryGeneration;
    ++m_importGeneration;
    ++m_gridDetailsGeneration;
    m_workerPool.clear();
    m_workerPool.waitForDone(5000);
}

QObject* MapIntelligenceService::layerModel() const
{
    return m_layerModel;
}

QObject* MapIntelligenceService::baseMapService() const
{
    return m_baseMapService;
}

QObject* MapIntelligenceService::externalOverlayService() const
{
    return m_externalOverlayService;
}

QObject* MapIntelligenceService::operationsService() const
{
    return m_operationsService;
}

QObject* MapIntelligenceService::pskFeedService() const
{
    return m_pskFeedService;
}

void MapIntelligenceService::setOfflineMode(bool offline)
{
    const bool changed = m_offlineMode != offline;
    m_offlineMode = offline;
    if (m_baseMapService) {
        m_baseMapService->setOfflineMode(offline);
    }
    if (m_externalOverlayService) {
        m_externalOverlayService->setOfflineMode(offline);
    }
    if (m_pskFeedService) {
        m_pskFeedService->setOfflineMode(offline);
    }
    if (m_operationsService) {
        m_operationsService->setOfflineMode(offline);
    }
    if (changed) {
        emit offlineModeChanged();
    }
}

bool MapIntelligenceService::workedLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("worked"));
}

bool MapIntelligenceService::confirmedLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("confirmed"));
}

bool MapIntelligenceService::liveLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("live"));
}

QStringList MapIntelligenceService::availablePeriods() const
{
    return {
        QStringLiteral("All time"), QStringLiteral("1 hour"),
        QStringLiteral("24 hours"), QStringLiteral("7 days"),
        QStringLiteral("30 days"), QStringLiteral("1 year")
    };
}

QStringList MapIntelligenceService::availablePropagationModes() const
{
    QStringList result;
    for (PropagationDefinition const& definition : propagationDefinitions()) {
        result.append(QString::fromLatin1(definition.code));
    }
    return result;
}

QVariantList MapIntelligenceService::availablePropagationTypes() const
{
    QVariantList result;
    for (PropagationDefinition const& definition : propagationDefinitions()) {
        result.append(QVariantMap {
            {QStringLiteral("code"), QString::fromLatin1(definition.code)},
            {QStringLiteral("label"), QString::fromLatin1(definition.label)}
        });
    }
    return result;
}

QStringList MapIntelligenceService::availableRosterStatuses() const
{
    return {
        QStringLiteral("All"), QStringLiteral("New"),
        QStringLiteral("Unconfirmed"), QStringLiteral("Wanted"),
        QStringLiteral("Award"), QStringLiteral("Watched")
    };
}

QStringList MapIntelligenceService::availableRosterHuntScopes() const
{
    return {
        QStringLiteral("All time"), QStringLiteral("Band"),
        QStringLiteral("Band + mode")
    };
}

QStringList MapIntelligenceService::availableAwardPrograms() const
{
    QStringList programs {
        QStringLiteral("None"), QStringLiteral("DXCC"),
        QStringLiteral("Maidenhead"), QStringLiteral("WAZ"),
        QStringLiteral("WAS"), QStringLiteral("US48"),
        QStringLiteral("WAC"), QStringLiteral("ITU Zones"),
        QStringLiteral("POTA"), QStringLiteral("IOTA"),
        QStringLiteral("WPX")
    };
    for (ExternalAwardDefinition const& definition : externalAwardDefinitions()) {
        programs.append(definition.label);
    }
    return programs;
}

QStringList MapIntelligenceService::availableAwardGoals() const
{
    return {
        QStringLiteral("Worked"), QStringLiteral("Confirmed")
    };
}

QStringList MapIntelligenceService::availableAwardConfirmations() const
{
    return {QStringLiteral("Any"), QStringLiteral("QSL"), QStringLiteral("LoTW"),
            QStringLiteral("eQSL"), QStringLiteral("OQRS")};
}

QStringList MapIntelligenceService::availableAwardEndorsements() const
{
    QStringList result {QStringLiteral("Mixed")};
    ExternalAwardDefinition const* definition =
        externalAwardForLabel(m_activeAwardProgram);
    if (definition) {
        for (QString const& endorsement : definition->endorsements) {
            QString const display = endorsement.compare(QStringLiteral("Mixed"),
                                                        Qt::CaseInsensitive) == 0
                ? QStringLiteral("Mixed") : endorsement.toLower();
            if (!display.isEmpty() && !result.contains(display, Qt::CaseInsensitive)) {
                result.append(display);
            }
        }
    }
    return result;
}

QStringList MapIntelligenceService::availablePskDisplayModes() const
{
    return {QStringLiteral("Overlay"), QStringLiteral("Replace")};
}

QStringList MapIntelligenceService::availableSpotAgeFilters() const
{
    QStringList values;
    for (int minutes = 5; minutes <= 60; minutes += 5) {
        values.append(QStringLiteral("%1 min").arg(minutes));
    }
    values.append({QStringLiteral("6 hours"), QStringLiteral("24 hours"),
                   QStringLiteral("7 days"), QStringLiteral("All retained")});
    return values;
}

QStringList MapIntelligenceService::availableCorrelationFilters() const
{
    return {QStringLiteral("All"), QStringLiteral("Correlated"),
            QStringLiteral("Local decode"), QStringLiteral("PSK Reporter"),
            QStringLiteral("OAMS")};
}

QStringList MapIntelligenceService::availableRosterColumns() const
{
    return {QStringLiteral("Grid"), QStringLiteral("Grid source"),
            QStringLiteral("Band"),
            QStringLiteral("Mode"), QStringLiteral("SNR"), QStringLiteral("DT"),
            QStringLiteral("DXCC"), QStringLiteral("Continent"),
            QStringLiteral("CQ zone"), QStringLiteral("ITU zone"),
            QStringLiteral("State"), QStringLiteral("County"),
            QStringLiteral("POTA"), QStringLiteral("IOTA"),
            QStringLiteral("WPX"), QStringLiteral("LoTW age"),
            QStringLiteral("eQSL age"), QStringLiteral("OQRS"),
            QStringLiteral("Age"), QStringLiteral("Source")};
}

QStringList MapIntelligenceService::availableRosterScopes() const
{
    return {QStringLiteral("Current band"), QStringLiteral("Current mode"),
            QStringLiteral("Digital modes"), QStringLiteral("All bands"),
            QStringLiteral("Award selected")};
}

QStringList MapIntelligenceService::availableRosterDxccScopes() const
{
    return {QStringLiteral("All"), QStringLiteral("Same DXCC"),
            QStringLiteral("Other DXCC")};
}

QStringList MapIntelligenceService::availableRosterRuleTypes() const
{
    return defaultRosterWantedTypes() + QStringList {
        QStringLiteral("IOTA"), QStringLiteral("OQRS"),
        QStringLiteral("BAND"), QStringLiteral("MODE")};
}

QStringList MapIntelligenceService::availableRosterWantedTypes() const
{
    return defaultRosterWantedTypes();
}

QStringList MapIntelligenceService::availableCallLookupProviders() const
{
    return {
        QStringLiteral("QRZ"), QStringLiteral("HamQTH"),
        QStringLiteral("QRZCQ")
    };
}

bool MapIntelligenceService::activeLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("active"));
}

bool MapIntelligenceService::missingLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("missing"));
}

bool MapIntelligenceService::pskLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("psk"));
}

bool MapIntelligenceService::liveEntryMatchesCurrentFilters(
    const QVariantMap& entry,
    qint64 dialFrequencyHz,
    const QString& band) const
{
    LiveSpot const spot = liveSpotFromEntry(entry, dialFrequencyHz, band);
    if (spot.message.isEmpty() || spot.direction == QStringLiteral("TX")) {
        return false;
    }

    auto matches = [](QString const& filter, QString const& value) {
        return filter.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0
            || filter.compare(value, Qt::CaseInsensitive) == 0;
    };

    if (!matches(m_bandFilter, spot.band)
        || !matches(m_modeFilter, spot.mode)
        || !matches(m_continentFilter, spot.continent)
        || !matches(m_dxccFilter, spot.dxcc)
        || !matches(m_sourceFilter, spot.source)
        || (m_propagationFilter.compare(QStringLiteral("MIXED"), Qt::CaseInsensitive) != 0
            && m_propagationFilter.compare(spot.propagationMode, Qt::CaseInsensitive) != 0)) {
        return false;
    }

    qint64 const cutoff = periodCutoffMs(m_periodFilter);
    if (cutoff > 0 && spot.observedMs < cutoff) {
        return false;
    }
    return !m_cqOnly || spot.isCq;
}

void MapIntelligenceService::setBandFilter(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty() ? QStringLiteral("All") : value.trimmed();
    if (m_bandFilter.compare(normalized, Qt::CaseInsensitive) == 0) {
        return;
    }
    m_bandFilter = normalized;
    saveSetting(QStringLiteral("Band"), m_bandFilter);
    emit bandFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setModeFilter(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty()
        ? QStringLiteral("All") : value.trimmed().toUpper();
    if (m_modeFilter.compare(normalized, Qt::CaseInsensitive) == 0) {
        return;
    }
    m_modeFilter = normalized;
    saveSetting(QStringLiteral("Mode"), m_modeFilter);
    emit modeFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setPeriodFilter(const QString& value)
{
    QString const normalized = normalizedFilter(value);
    if (m_periodFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_periodFilter = normalized;
    saveSetting(QStringLiteral("Period"), normalized);
    emit periodFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setContinentFilter(const QString& value)
{
    QString const normalized = normalizedFilter(value).toUpper();
    if (m_continentFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_continentFilter = normalized;
    saveSetting(QStringLiteral("Continent"), normalized);
    emit continentFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setDxccFilter(const QString& value)
{
    QString const normalized = normalizedFilter(value);
    if (m_dxccFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_dxccFilter = normalized;
    saveSetting(QStringLiteral("Dxcc"), normalized);
    emit dxccFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setSourceFilter(const QString& value)
{
    QString const normalized = normalizedFilter(value);
    if (m_sourceFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_sourceFilter = normalized;
    saveSetting(QStringLiteral("Source"), normalized);
    emit sourceFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setPropagationFilter(const QString& value)
{
    QString normalized = value.trimmed().toUpper();
    if (normalized.isEmpty()) {
        normalized = QStringLiteral("MIXED");
    }
    int const index = availablePropagationModes().indexOf(normalized, 0, Qt::CaseInsensitive);
    if (index < 0) {
        normalized = QStringLiteral("MIXED");
    } else {
        normalized = availablePropagationModes().at(index);
    }
    if (m_propagationFilter == normalized) {
        return;
    }
    m_propagationFilter = normalized;
    saveSetting(QStringLiteral("PropagationFilter"), normalized);
    emit propagationFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setCqOnly(bool enabled)
{
    if (m_cqOnly == enabled) return;
    m_cqOnly = enabled;
    saveSetting(QStringLiteral("CqOnly"), enabled);
    emit cqOnlyChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterSort(const QString& value)
{
    QString const normalized = normalizedFilter(value);
    if (m_rosterSort.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_rosterSort = normalized;
    saveSetting(QStringLiteral("RosterSort"), normalized);
    emit rosterSortChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterSortDescending(bool descending)
{
    if (m_rosterSortDescending == descending) return;
    m_rosterSortDescending = descending;
    saveSetting(QStringLiteral("RosterSortDescending"), descending);
    emit rosterSortDescendingChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterStatusFilter(const QString& value)
{
    QString const normalized = availableRosterStatuses().contains(value, Qt::CaseInsensitive)
        ? availableRosterStatuses().at(
              availableRosterStatuses().indexOf(value, 0, Qt::CaseInsensitive))
        : QStringLiteral("All");
    if (m_rosterStatusFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_rosterStatusFilter = normalized;
    saveSetting(QStringLiteral("RosterStatus"), normalized);
    emit rosterStatusFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterHuntScope(const QString& value)
{
    QString const normalized = availableRosterHuntScopes().contains(value, Qt::CaseInsensitive)
        ? availableRosterHuntScopes().at(
              availableRosterHuntScopes().indexOf(value, 0, Qt::CaseInsensitive))
        : QStringLiteral("All time");
    if (m_rosterHuntScope.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_rosterHuntScope = normalized;
    saveSetting(QStringLiteral("RosterHuntScope"), normalized);
    emit rosterHuntScopeChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterScope(const QString& value)
{
    int const index = availableRosterScopes().indexOf(value.trimmed(), 0,
                                                      Qt::CaseInsensitive);
    QString const normalized = index >= 0 ? availableRosterScopes().at(index)
                                          : QStringLiteral("All bands");
    if (m_rosterScope == normalized) return;
    m_rosterScope = normalized;
    saveSetting(QStringLiteral("RosterScope"), normalized);
    emit rosterScopeChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterDxccScope(const QString& value)
{
    int const index = availableRosterDxccScopes().indexOf(value.trimmed(), 0,
                                                           Qt::CaseInsensitive);
    QString const normalized = index >= 0 ? availableRosterDxccScopes().at(index)
                                          : QStringLiteral("All");
    if (m_rosterDxccScope == normalized) return;
    m_rosterDxccScope = normalized;
    saveSetting(QStringLiteral("RosterDxccScope"), normalized);
    emit rosterDxccScopeChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterRetentionMinutes(int minutes)
{
    int const bounded = qBound(1, minutes, 60);
    if (m_rosterRetentionMinutes == bounded) return;
    m_rosterRetentionMinutes = bounded;
    saveSetting(QStringLiteral("RosterRetentionMinutes"), bounded);
    emit rosterRetentionMinutesChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterCqOnly(bool enabled)
{
    if (m_rosterCqOnly == enabled) return;
    m_rosterCqOnly = enabled;
    saveSetting(QStringLiteral("RosterCqOnly"), enabled);
    emit rosterCqOnlyChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterTextFilter(const QString& value)
{
    QString const normalized = value.trimmed().left(128);
    if (m_rosterTextFilter == normalized) {
        return;
    }
    m_rosterTextFilter = normalized;
    saveSetting(QStringLiteral("RosterTextFilter"), normalized);
    emit rosterTextFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterTextMode(const QString& value)
{
    static const QStringList modes {
        QStringLiteral("No filter"), QStringLiteral("Only"),
        QStringLiteral("Exclude"), QStringLiteral("Regex")
    };
    int const index = modes.indexOf(value.trimmed(), 0, Qt::CaseInsensitive);
    QString const normalized = index >= 0 ? modes.at(index) : QStringLiteral("No filter");
    if (m_rosterTextMode == normalized) {
        return;
    }
    m_rosterTextMode = normalized;
    saveSetting(QStringLiteral("RosterTextMode"), normalized);
    emit rosterTextModeChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterUsesLoTW(bool enabled)
{
    if (m_rosterUsesLoTW == enabled) return;
    m_rosterUsesLoTW = enabled;
    saveSetting(QStringLiteral("RosterUsesLoTW"), enabled);
    emit rosterFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterMaxLoTWDays(int days)
{
    int const bounded = qBound(1, days, 3650);
    if (m_rosterMaxLoTWDays == bounded) return;
    m_rosterMaxLoTWDays = bounded;
    saveSetting(QStringLiteral("RosterMaxLoTWDays"), bounded);
    emit rosterFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterUsesEQSL(bool enabled)
{
    if (m_rosterUsesEQSL == enabled) return;
    m_rosterUsesEQSL = enabled;
    saveSetting(QStringLiteral("RosterUsesEQSL"), enabled);
    emit rosterFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterUsesOQRS(bool enabled)
{
    if (m_rosterUsesOQRS == enabled) return;
    m_rosterUsesOQRS = enabled;
    saveSetting(QStringLiteral("RosterUsesOQRS"), enabled);
    emit rosterFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterSpottedMeOnly(bool enabled)
{
    if (m_rosterSpottedMeOnly == enabled) return;
    m_rosterSpottedMeOnly = enabled;
    saveSetting(QStringLiteral("RosterSpottedMeOnly"), enabled);
    emit rosterFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterMinSnrEnabled(bool enabled)
{
    if (m_rosterMinSnrEnabled == enabled) return;
    m_rosterMinSnrEnabled = enabled;
    saveSetting(QStringLiteral("RosterMinSnrEnabled"), enabled);
    emit rosterFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterMinSnr(int snr)
{
    int const bounded = qBound(-60, snr, 30);
    if (m_rosterMinSnr == bounded) return;
    m_rosterMinSnr = bounded;
    saveSetting(QStringLiteral("RosterMinSnr"), bounded);
    emit rosterFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterMaxDtEnabled(bool enabled)
{
    if (m_rosterMaxDtEnabled == enabled) return;
    m_rosterMaxDtEnabled = enabled;
    saveSetting(QStringLiteral("RosterMaxDtEnabled"), enabled);
    emit rosterFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterMaxDt(double dt)
{
    double const bounded = qBound(0.01, dt, 10.0);
    if (qFuzzyCompare(m_rosterMaxDt, bounded)) return;
    m_rosterMaxDt = bounded;
    saveSetting(QStringLiteral("RosterMaxDt"), bounded);
    emit rosterFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterTreatRr73AsCq(bool enabled)
{
    if (m_rosterTreatRr73AsCq == enabled) return;
    m_rosterTreatRr73AsCq = enabled;
    saveSetting(QStringLiteral("RosterTreatRr73AsCq"), enabled);
    emit rosterTreatRr73AsCqChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterWantedTypes(const QStringList& types)
{
    QStringList normalized;
    QStringList const known = availableRosterWantedTypes();
    for (QString const& type : types) {
        int const index = known.indexOf(type.trimmed(), 0, Qt::CaseInsensitive);
        if (index >= 0 && !normalized.contains(known.at(index))) {
            normalized.append(known.at(index));
        }
    }
    if (m_rosterWantedTypes == normalized) return;
    m_rosterWantedTypes = normalized;
    saveSetting(QStringLiteral("RosterWantedTypes"), normalized);
    emit rosterWantedTypesChanged();
    scheduleQuery();
}

void MapIntelligenceService::setActiveAwardProgram(const QString& value)
{
    QString const normalized = availableAwardPrograms().contains(value, Qt::CaseInsensitive)
        ? availableAwardPrograms().at(
              availableAwardPrograms().indexOf(value, 0, Qt::CaseInsensitive))
        : QStringLiteral("None");
    if (m_activeAwardProgram.compare(normalized, Qt::CaseInsensitive) == 0) {
        return;
    }
    m_activeAwardProgram = normalized;
    saveSetting(QStringLiteral("ActiveAwardProgram"), normalized);
    if (!availableAwardEndorsements().contains(m_awardEndorsement, Qt::CaseInsensitive)) {
        m_awardEndorsement.clear();
        saveSetting(QStringLiteral("AwardEndorsement"), m_awardEndorsement);
    }
    emit activeAwardProgramChanged();
    emit awardFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setAwardGoal(const QString& value)
{
    QString const normalized = availableAwardGoals().contains(value, Qt::CaseInsensitive)
        ? availableAwardGoals().at(
              availableAwardGoals().indexOf(value, 0, Qt::CaseInsensitive))
        : QStringLiteral("Confirmed");
    if (m_awardGoal.compare(normalized, Qt::CaseInsensitive) == 0) {
        return;
    }
    m_awardGoal = normalized;
    saveSetting(QStringLiteral("AwardGoal"), normalized);
    emit awardGoalChanged();
    scheduleQuery();
}

void MapIntelligenceService::setAwardEndorsement(const QString& value)
{
    QString const normalized = availableAwardEndorsements().contains(
        value.trimmed(), Qt::CaseInsensitive)
        ? availableAwardEndorsements().at(
              availableAwardEndorsements().indexOf(value.trimmed(), 0, Qt::CaseInsensitive))
        : QStringLiteral("Mixed");
    if (m_awardEndorsement.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_awardEndorsement = normalized == QStringLiteral("Mixed") ? QString() : normalized;
    saveSetting(QStringLiteral("AwardEndorsement"), m_awardEndorsement);
    emit awardFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setAwardConfirmation(const QString& value)
{
    QString const normalized = availableAwardConfirmations().contains(
        value.trimmed(), Qt::CaseInsensitive)
        ? availableAwardConfirmations().at(
              availableAwardConfirmations().indexOf(value.trimmed(), 0, Qt::CaseInsensitive))
        : QStringLiteral("Any");
    if (m_awardConfirmation.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_awardConfirmation = normalized;
    saveSetting(QStringLiteral("AwardConfirmation"), normalized);
    emit awardFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setAwardCallsign(const QString& value)
{
    QString const normalized = value.trimmed().toUpper().left(32);
    if (m_awardCallsign == normalized) return;
    m_awardCallsign = normalized;
    saveSetting(QStringLiteral("AwardCallsign"), normalized);
    emit awardFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setAwardFromDate(const QString& value)
{
    QString const normalized = value.trimmed().left(10);
    if (!normalized.isEmpty() && !QDate::fromString(normalized, Qt::ISODate).isValid()) return;
    if (m_awardFromDate == normalized) return;
    m_awardFromDate = normalized;
    saveSetting(QStringLiteral("AwardFromDate"), normalized);
    emit awardFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setAwardToDate(const QString& value)
{
    QString const normalized = value.trimmed().left(10);
    if (!normalized.isEmpty() && !QDate::fromString(normalized, Qt::ISODate).isValid()) return;
    if (m_awardToDate == normalized) return;
    m_awardToDate = normalized;
    saveSetting(QStringLiteral("AwardToDate"), normalized);
    emit awardFiltersChanged();
    scheduleQuery();
}

void MapIntelligenceService::setGridPrecision(int precision)
{
    int const normalized = precision == 6 ? 6 : 4;
    if (m_gridPrecision == normalized) {
        return;
    }
    m_gridPrecision = normalized;
    saveSetting(QStringLiteral("GridPrecision"), normalized);
    emit gridPrecisionChanged();
    scheduleQuery();
}

void MapIntelligenceService::setLiveDecayMinutes(int minutes)
{
    int const bounded = qBound(1, minutes, 120);
    if (m_liveDecayMinutes == bounded) {
        return;
    }
    m_liveDecayMinutes = bounded;
    m_sourceDecayMinutes.insert(QStringLiteral("decoder"), bounded);
    saveSetting(QStringLiteral("LiveDecayMinutes"), bounded);
    emit liveDecayMinutesChanged();
    emit mapTemporalSettingsChanged();
    scheduleQuery();
}

void MapIntelligenceService::setSplitGridEnabled(bool enabled)
{
    if (m_splitGridEnabled == enabled) {
        return;
    }
    m_splitGridEnabled = enabled;
    saveSetting(QStringLiteral("SplitGridEnabled"), enabled);
    emit splitGridEnabledChanged();
    scheduleQuery();
}

void MapIntelligenceService::setCoveragePushPinsEnabled(bool enabled)
{
    if (m_coveragePushPinsEnabled == enabled) {
        return;
    }
    m_coveragePushPinsEnabled = enabled;
    saveSetting(QStringLiteral("CoveragePushPinsEnabled"), enabled);
    emit coveragePushPinsEnabledChanged();
}

void MapIntelligenceService::setTimeZoneOverlayEnabled(bool enabled)
{
    if (m_timeZoneOverlayEnabled == enabled) {
        return;
    }
    m_timeZoneOverlayEnabled = enabled;
    saveSetting(QStringLiteral("TimeZoneOverlayEnabled"), enabled);
    emit timeZoneOverlayEnabledChanged();
}

void MapIntelligenceService::setPskDisplayMode(const QString& mode)
{
    int const index = availablePskDisplayModes().indexOf(
        mode.trimmed(), 0, Qt::CaseInsensitive);
    QString const normalized = index >= 0
        ? availablePskDisplayModes().at(index) : QStringLiteral("Overlay");
    if (m_pskDisplayMode == normalized) {
        return;
    }
    m_pskDisplayMode = normalized;
    saveSetting(QStringLiteral("PskDisplayMode"), normalized);
    emit pskDisplayModeChanged();
    scheduleQuery();
}

void MapIntelligenceService::setPskOpacityPercent(int percent)
{
    int const bounded = qBound(20, percent, 100);
    if (m_pskOpacityPercent == bounded) {
        return;
    }
    m_pskOpacityPercent = bounded;
    saveSetting(QStringLiteral("PskOpacityPercent"), bounded);
    emit pskOpacityPercentChanged();
    scheduleQuery();
}

void MapIntelligenceService::setSpotAgeFilter(const QString& value)
{
    QString const requested = normalizedSpotAgeFilter(value);
    QString const normalized = requested.isEmpty()
        ? QStringLiteral("15 min") : requested;
    if (m_spotAgeFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_spotAgeFilter = normalized;
    saveSetting(QStringLiteral("SpotAgeFilter"), normalized);
    emit spotAgeFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setSpotCorrelationFilter(const QString& value)
{
    int const index = availableCorrelationFilters().indexOf(value.trimmed(), 0,
                                                            Qt::CaseInsensitive);
    QString const normalized = index >= 0 ? availableCorrelationFilters().at(index)
                                          : QStringLiteral("All");
    if (m_spotCorrelationFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_spotCorrelationFilter = normalized;
    saveSetting(QStringLiteral("SpotCorrelationFilter"), normalized);
    emit spotCorrelationFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setBandActivityWindowHours(int hours)
{
    if (hours != 1 && hours != 6 && hours != 12 && hours != 24) {
        return;
    }
    if (m_bandActivityWindowHours == hours) {
        return;
    }
    m_bandActivityWindowHours = hours;
    saveSetting(QStringLiteral("BandActivityWindowHours"), hours);
    emit bandActivityWindowHoursChanged();
    scheduleQuery();
}

QVariantList MapIntelligenceService::temporalLegend() const
{
    auto sourceRow = [this](const QString& source, const QString& label,
                            const QString& color) {
        int const minutes = m_sourceDecayMinutes.value(source, 15).toInt();
        return QVariantMap {
            {QStringLiteral("source"), source},
            {QStringLiteral("label"), label},
            {QStringLiteral("color"), color},
            {QStringLiteral("decayMinutes"), minutes},
            {QStringLiteral("freshLabel"), QStringLiteral("0-%1 min").arg(qMax(1, minutes / 3))},
            {QStringLiteral("fadingLabel"), QStringLiteral("%1-%2 min")
                                                  .arg(qMax(1, minutes / 3))
                                                  .arg(qMax(2, (minutes * 2) / 3))},
            {QStringLiteral("staleLabel"), QStringLiteral(">%1 min").arg(qMax(2, (minutes * 2) / 3))}
        };
    };
    return {
        sourceRow(QStringLiteral("decoder"), QStringLiteral("Decoder / local"),
                  QStringLiteral("#00d8ff")),
        sourceRow(QStringLiteral("psk"), QStringLiteral("PSK Reporter"),
                  QStringLiteral("#ba7cff")),
        sourceRow(QStringLiteral("oams"), QStringLiteral("OAMS / external"),
                  QStringLiteral("#ff9f43"))
    };
}

void MapIntelligenceService::setSourceDecayMinutes(const QVariantMap& values)
{
    QVariantMap next = m_sourceDecayMinutes;
    next.insert(QStringLiteral("decoder"), qBound(1,
        values.value(QStringLiteral("decoder"), m_liveDecayMinutes).toInt(), 120));
    next.insert(QStringLiteral("psk"), qBound(1,
        values.value(QStringLiteral("psk"), 60).toInt(), 240));
    next.insert(QStringLiteral("oams"), qBound(1,
        values.value(QStringLiteral("oams"), 30).toInt(), 240));
    if (next == m_sourceDecayMinutes) {
        return;
    }
    m_sourceDecayMinutes = next;
    m_liveDecayMinutes = next.value(QStringLiteral("decoder")).toInt();
    saveSetting(QStringLiteral("LiveDecayMinutes"), m_liveDecayMinutes);
    saveSetting(QStringLiteral("Decay/psk"), next.value(QStringLiteral("psk")));
    saveSetting(QStringLiteral("Decay/oams"), next.value(QStringLiteral("oams")));
    emit liveDecayMinutesChanged();
    emit mapTemporalSettingsChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterVisibleColumns(const QStringList& columns)
{
    QStringList normalized;
    QStringList const known = availableRosterColumns();
    for (QString const& column : columns) {
        int const index = known.indexOf(column.trimmed(), 0, Qt::CaseInsensitive);
        if (index >= 0 && !normalized.contains(known.at(index))) {
            normalized.append(known.at(index));
        }
    }
    if (normalized.isEmpty()) {
        normalized = {QStringLiteral("Grid"), QStringLiteral("Band"),
                      QStringLiteral("Mode"), QStringLiteral("SNR"),
                      QStringLiteral("DXCC"), QStringLiteral("Age")};
    }
    if (m_rosterVisibleColumns == normalized) return;
    m_rosterVisibleColumns = normalized;
    saveSetting(QStringLiteral("RosterVisibleColumns"), normalized);
    emit rosterVisibleColumnsChanged();
}

void MapIntelligenceService::setCallLookupProvider(const QString& provider)
{
    int const index = availableCallLookupProviders().indexOf(
        provider.trimmed(), 0, Qt::CaseInsensitive);
    QString const normalized = index >= 0
        ? availableCallLookupProviders().at(index) : QStringLiteral("QRZ");
    if (m_callLookupProvider == normalized) {
        return;
    }
    m_callLookupProvider = normalized;
    saveSetting(QStringLiteral("CallLookupProvider"), normalized);
    emit callLookupProviderChanged();
}

void MapIntelligenceService::setAlertNewGridEnabled(bool enabled)
{
    if (m_alertNewGridEnabled == enabled) return;
    m_alertNewGridEnabled = enabled;
    saveSetting(QStringLiteral("AlertNewGrid"), enabled);
    emit alertRulesChanged();
}

void MapIntelligenceService::setAlertNewDxccEnabled(bool enabled)
{
    if (m_alertNewDxccEnabled == enabled) return;
    m_alertNewDxccEnabled = enabled;
    saveSetting(QStringLiteral("AlertNewDxcc"), enabled);
    emit alertRulesChanged();
}

void MapIntelligenceService::setAlertCqEnabled(bool enabled)
{
    if (m_alertCqEnabled == enabled) return;
    m_alertCqEnabled = enabled;
    saveSetting(QStringLiteral("AlertCq"), enabled);
    emit alertRulesChanged();
}

void MapIntelligenceService::setAlertCallPattern(const QString& pattern)
{
    QString const normalized = pattern.trimmed().left(64);
    if (m_alertCallPattern == normalized) return;
    m_alertCallPattern = normalized;
    saveSetting(QStringLiteral("AlertCallPattern"), normalized);
    emit alertRulesChanged();
}

void MapIntelligenceService::setWorkedLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("worked"), enabled);
}

void MapIntelligenceService::setConfirmedLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("confirmed"), enabled);
}

void MapIntelligenceService::setLiveLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("live"), enabled);
}

void MapIntelligenceService::setActiveLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("active"), enabled);
}

void MapIntelligenceService::setMissingLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("missing"), enabled);
}

void MapIntelligenceService::setPskLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("psk"), enabled);
}

QString MapIntelligenceService::reserveMapConfigurationPath() const
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    QDir directory(root);
    directory.mkpath(QStringLiteral("Decodium"));
    return directory.absoluteFilePath(QStringLiteral("Decodium-Map-Config-%1.json")
        .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
}

bool MapIntelligenceService::exportMapConfiguration(const QString& path,
                                                     const QVariantMap& viewport)
{
    QString localPath = path.trimmed();
    QUrl const url(localPath);
    if (url.isLocalFile()) {
        localPath = url.toLocalFile();
    }
    if (localPath.isEmpty()) {
        return false;
    }

    QJsonObject root {
        {QStringLiteral("type"), QStringLiteral("decodium-map-configuration")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("exportedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("viewport"), QJsonObject::fromVariantMap(viewport)},
        {QStringLiteral("layers"), QJsonArray()},
        {QStringLiteral("settings"), QJsonObject()},
        {QStringLiteral("mapOperations"), QJsonObject()},
        {QStringLiteral("presetNames"), QJsonArray()},
        {QStringLiteral("presets"), QJsonObject()},
        {QStringLiteral("roster"), QJsonObject()}
    };

    QJsonArray layers;
    for (int row = 0; row < m_layerModel->rowCount(); ++row) {
        QModelIndex const index = m_layerModel->index(row, 0);
        QString const id = m_layerModel->data(index, MapLayerModel::LayerIdRole).toString();
        QJsonObject layer = QJsonObject::fromVariantMap(m_layerModel->layerStyle(id));
        layer.insert(QStringLiteral("id"), id);
        layer.insert(QStringLiteral("enabled"),
                     m_layerModel->data(index, MapLayerModel::LayerEnabledRole).toBool());
        layers.append(layer);
    }
    root.insert(QStringLiteral("layers"), layers);

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    auto exportGroup = [&settings](const QString& group) {
        QJsonObject result;
        settings.beginGroup(group);
        for (QString const& key : settings.allKeys()) {
            result.insert(key, QJsonValue::fromVariant(settings.value(key)));
        }
        settings.endGroup();
        return result;
    };
    root.insert(QStringLiteral("settings"), exportGroup(QStringLiteral("LiveMapLayers")));
    QJsonObject const mapOperations = exportGroup(QStringLiteral("MapOperations"));
    root.insert(QStringLiteral("mapOperations"), mapOperations);

    QJsonArray presetNames;
    if (m_operationsService) {
        for (const QString& name : m_operationsService->mapPresets()) {
            presetNames.append(name);
        }
    }
    root.insert(QStringLiteral("presetNames"), presetNames);

    QJsonObject presets;
    settings.beginGroup(QStringLiteral("MapPresets"));
    for (QString const& name : settings.childGroups()) {
        settings.beginGroup(name);
        QJsonObject preset;
        for (QString const& key : settings.allKeys()) {
            preset.insert(key, QJsonValue::fromVariant(settings.value(key)));
        }
        settings.endGroup();
        presets.insert(name, preset);
    }
    settings.endGroup();
    root.insert(QStringLiteral("presets"), presets);

    QJsonObject roster;
    ScopedSqliteConnection connection(m_databasePath);
    if (connection.database().open()) {
        QSqlDatabase& db = connection.database();
        auto queryRows = [&db](const QString& sql, int columns) {
            QJsonArray result;
            QSqlQuery query(db);
            if (!query.exec(sql)) {
                return result;
            }
            while (query.next()) {
                QJsonObject row;
                for (int column = 0; column < columns; ++column) {
                    row.insert(QString::number(column),
                               QJsonValue::fromVariant(query.value(column)));
                }
                result.append(row);
            }
            return result;
        };
        roster.insert(QStringLiteral("preferences"), queryRows(
            QStringLiteral("SELECT call,watched,ignored,updated_ms FROM map_roster_preference"), 4));
        roster.insert(QStringLiteral("ignores"), queryRows(
            QStringLiteral("SELECT ignore_type,ignore_value,updated_ms FROM map_roster_ignore"), 3));
        roster.insert(QStringLiteral("rules"), queryRows(
            QStringLiteral("SELECT rule_type,rule_value,rule_action,band,mode,enabled,updated_ms FROM map_roster_rule"), 7));
    }
    root.insert(QStringLiteral("roster"), roster);

    QFileInfo const info(localPath);
    if (!QDir().mkpath(info.absolutePath())) {
        return false;
    }
    QSaveFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

QVariantMap MapIntelligenceService::importMapConfiguration(const QString& path)
{
    QString localPath = path.trimmed();
    QUrl const url(localPath);
    if (url.isLocalFile()) {
        localPath = url.toLocalFile();
    }
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    QJsonObject const root = document.object();
    if (root.value(QStringLiteral("type")).toString()
            != QStringLiteral("decodium-map-configuration")
        || root.value(QStringLiteral("version")).toInt() != 1) {
        return {};
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("LiveMapLayers"));
    settings.remove(QString());
    QJsonObject const settingObject = root.value(QStringLiteral("settings")).toObject();
    for (auto it = settingObject.constBegin(); it != settingObject.constEnd(); ++it) {
        settings.setValue(it.key(), it.value().toVariant());
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("MapOperations"));
    QJsonObject const operationObject = root.value(QStringLiteral("mapOperations")).toObject();
    for (auto it = operationObject.constBegin(); it != operationObject.constEnd(); ++it) {
        settings.setValue(it.key(), it.value().toVariant());
    }
    settings.endGroup();
    settings.beginGroup(QStringLiteral("MapPresets"));
    settings.remove(QString());
    QJsonObject const presets = root.value(QStringLiteral("presets")).toObject();
    for (auto presetIt = presets.constBegin(); presetIt != presets.constEnd(); ++presetIt) {
        settings.beginGroup(presetIt.key());
        QJsonObject const preset = presetIt.value().toObject();
        for (auto valueIt = preset.constBegin(); valueIt != preset.constEnd(); ++valueIt) {
            settings.setValue(valueIt.key(), valueIt.value().toVariant());
        }
        settings.endGroup();
    }
    settings.endGroup();
    settings.sync();

    for (QJsonValue const& value : root.value(QStringLiteral("layers")).toArray()) {
        QJsonObject const layer = value.toObject();
        QString const id = layer.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }
        m_layerModel->setLayerStyle(
            id, layer.value(QStringLiteral("color")).toString(),
            layer.value(QStringLiteral("opacity")).toDouble(1.0),
            layer.value(QStringLiteral("thickness")).toDouble(1.0),
            layer.value(QStringLiteral("labelDensity")).toInt(100));
        m_layerModel->setLayerEnabled(
            id, layer.contains(QStringLiteral("enabled"))
                    ? layer.value(QStringLiteral("enabled")).toBool() : true);
    }

    QJsonObject const roster = root.value(QStringLiteral("roster")).toObject();
    ScopedSqliteConnection connection(m_databasePath);
    if (connection.database().open()) {
        QSqlDatabase& db = connection.database();
        if (db.transaction()) {
            QSqlQuery clear(db);
            clear.exec(QStringLiteral("DELETE FROM map_roster_preference"));
            clear.exec(QStringLiteral("DELETE FROM map_roster_ignore"));
            clear.exec(QStringLiteral("DELETE FROM map_roster_rule"));
            QSqlQuery insertPreference(db);
            insertPreference.prepare(QStringLiteral(
                "INSERT INTO map_roster_preference(call,watched,ignored,updated_ms) VALUES(?,?,?,?)"));
            for (QJsonValue const& value : roster.value(QStringLiteral("preferences")).toArray()) {
                QJsonObject const row = value.toObject();
                for (int column = 0; column < 4; ++column) {
                    insertPreference.bindValue(column, row.value(QString::number(column)).toVariant());
                }
                insertPreference.exec();
                insertPreference.finish();
            }
            QSqlQuery insertIgnore(db);
            insertIgnore.prepare(QStringLiteral(
                "INSERT INTO map_roster_ignore(ignore_type,ignore_value,updated_ms) VALUES(?,?,?)"));
            for (QJsonValue const& value : roster.value(QStringLiteral("ignores")).toArray()) {
                QJsonObject const row = value.toObject();
                for (int column = 0; column < 3; ++column) {
                    insertIgnore.bindValue(column, row.value(QString::number(column)).toVariant());
                }
                insertIgnore.exec();
                insertIgnore.finish();
            }
            QSqlQuery insertRule(db);
            insertRule.prepare(QStringLiteral(
                "INSERT INTO map_roster_rule(rule_type,rule_value,rule_action,band,mode,enabled,updated_ms) VALUES(?,?,?,?,?,?,?)"));
            for (QJsonValue const& value : roster.value(QStringLiteral("rules")).toArray()) {
                QJsonObject const row = value.toObject();
                for (int column = 0; column < 7; ++column) {
                    insertRule.bindValue(column, row.value(QString::number(column)).toVariant());
                }
                insertRule.exec();
                insertRule.finish();
            }
            db.commit();
        }
    }

    auto value = [&settingObject](const QString& key, const QVariant& fallback = {}) {
        return settingObject.contains(key) ? settingObject.value(key).toVariant() : fallback;
    };
    setBandFilter(value(QStringLiteral("Band"), m_bandFilter).toString());
    setModeFilter(value(QStringLiteral("Mode"), m_modeFilter).toString());
    setPeriodFilter(value(QStringLiteral("Period"), m_periodFilter).toString());
    setContinentFilter(value(QStringLiteral("Continent"), m_continentFilter).toString());
    setDxccFilter(value(QStringLiteral("Dxcc"), m_dxccFilter).toString());
    setSourceFilter(value(QStringLiteral("Source"), m_sourceFilter).toString());
    setPropagationFilter(value(QStringLiteral("PropagationFilter"), m_propagationFilter).toString());
    setCqOnly(value(QStringLiteral("CqOnly"), m_cqOnly).toBool());
    setRosterSort(value(QStringLiteral("RosterSort"), m_rosterSort).toString());
    setRosterSortDescending(value(QStringLiteral("RosterSortDescending"), m_rosterSortDescending).toBool());
    setRosterStatusFilter(value(QStringLiteral("RosterStatus"), m_rosterStatusFilter).toString());
    setRosterHuntScope(value(QStringLiteral("RosterHuntScope"), m_rosterHuntScope).toString());
    setRosterScope(value(QStringLiteral("RosterScope"), m_rosterScope).toString());
    setRosterDxccScope(value(QStringLiteral("RosterDxccScope"), m_rosterDxccScope).toString());
    setRosterRetentionMinutes(value(QStringLiteral("RosterRetentionMinutes"), m_rosterRetentionMinutes).toInt());
    setRosterCqOnly(value(QStringLiteral("RosterCqOnly"), m_rosterCqOnly).toBool());
    setRosterTextFilter(value(QStringLiteral("RosterTextFilter"), m_rosterTextFilter).toString());
    setRosterTextMode(value(QStringLiteral("RosterTextMode"), m_rosterTextMode).toString());
    setActiveAwardProgram(value(QStringLiteral("ActiveAwardProgram"), m_activeAwardProgram).toString());
    setAwardGoal(value(QStringLiteral("AwardGoal"), m_awardGoal).toString());
    setAwardEndorsement(value(QStringLiteral("AwardEndorsement"), m_awardEndorsement).toString());
    setAwardConfirmation(value(QStringLiteral("AwardConfirmation"), m_awardConfirmation).toString());
    setAwardCallsign(value(QStringLiteral("AwardCallsign"), m_awardCallsign).toString());
    setAwardFromDate(value(QStringLiteral("AwardFromDate"), m_awardFromDate).toString());
    setAwardToDate(value(QStringLiteral("AwardToDate"), m_awardToDate).toString());
    setRosterUsesLoTW(value(QStringLiteral("RosterUsesLoTW"), m_rosterUsesLoTW).toBool());
    setRosterMaxLoTWDays(value(QStringLiteral("RosterMaxLoTWDays"), m_rosterMaxLoTWDays).toInt());
    setRosterUsesEQSL(value(QStringLiteral("RosterUsesEQSL"), m_rosterUsesEQSL).toBool());
    setRosterUsesOQRS(value(QStringLiteral("RosterUsesOQRS"), m_rosterUsesOQRS).toBool());
    setRosterSpottedMeOnly(value(QStringLiteral("RosterSpottedMeOnly"), m_rosterSpottedMeOnly).toBool());
    setRosterMinSnrEnabled(value(QStringLiteral("RosterMinSnrEnabled"), m_rosterMinSnrEnabled).toBool());
    setRosterMinSnr(value(QStringLiteral("RosterMinSnr"), m_rosterMinSnr).toInt());
    setRosterMaxDtEnabled(value(QStringLiteral("RosterMaxDtEnabled"), m_rosterMaxDtEnabled).toBool());
    setRosterMaxDt(value(QStringLiteral("RosterMaxDt"), m_rosterMaxDt).toDouble());
    setRosterTreatRr73AsCq(value(QStringLiteral("RosterTreatRr73AsCq"), m_rosterTreatRr73AsCq).toBool());
    setGridPrecision(value(QStringLiteral("GridPrecision"), m_gridPrecision).toInt());
    setLiveDecayMinutes(value(QStringLiteral("LiveDecayMinutes"), m_liveDecayMinutes).toInt());
    setSplitGridEnabled(value(QStringLiteral("SplitGridEnabled"), m_splitGridEnabled).toBool());
    setCoveragePushPinsEnabled(value(QStringLiteral("CoveragePushPinsEnabled"), m_coveragePushPinsEnabled).toBool());
    setTimeZoneOverlayEnabled(value(QStringLiteral("TimeZoneOverlayEnabled"), m_timeZoneOverlayEnabled).toBool());
    setPskDisplayMode(value(QStringLiteral("PskDisplayMode"), m_pskDisplayMode).toString());
    setPskOpacityPercent(value(QStringLiteral("PskOpacityPercent"), m_pskOpacityPercent).toInt());
    setSpotAgeFilter(value(QStringLiteral("SpotAgeFilter"), m_spotAgeFilter).toString());
    setSpotCorrelationFilter(value(QStringLiteral("SpotCorrelationFilter"), m_spotCorrelationFilter).toString());
    setBandActivityWindowHours(value(QStringLiteral("BandActivityWindowHours"),
                                     m_bandActivityWindowHours).toInt());
    setRosterWantedTypes(value(QStringLiteral("RosterWantedTypes"),
                               m_rosterWantedTypes).toStringList());
    setRosterVisibleColumns(value(QStringLiteral("RosterVisibleColumns"),
                                  m_rosterVisibleColumns).toStringList());
    setCallLookupProvider(value(QStringLiteral("CallLookupProvider"),
                                m_callLookupProvider).toString());
    setAlertNewGridEnabled(value(QStringLiteral("AlertNewGrid"),
                                 m_alertNewGridEnabled).toBool());
    setAlertNewDxccEnabled(value(QStringLiteral("AlertNewDxcc"),
                                 m_alertNewDxccEnabled).toBool());
    setAlertCqEnabled(value(QStringLiteral("AlertCq"), m_alertCqEnabled).toBool());
    setAlertCallPattern(value(QStringLiteral("AlertCallPattern"),
                               m_alertCallPattern).toString());
    if (m_pskFeedService) {
        m_pskFeedService->setEndpoint(value(QStringLiteral("PskMqttEndpoint"),
                                             m_pskFeedService->endpoint()).toString());
        m_pskFeedService->setEnabled(value(QStringLiteral("PskMqttEnabled"),
                                           m_pskFeedService->enabled()).toBool());
    }
    QVariantMap decay {
        {QStringLiteral("decoder"), m_liveDecayMinutes},
        {QStringLiteral("psk"), value(QStringLiteral("Decay/psk"), 60)},
        {QStringLiteral("oams"), value(QStringLiteral("Decay/oams"), 30)}
    };
    setSourceDecayMinutes(decay);

    if (m_operationsService) {
        QVariantMap const operations = operationObject.toVariantMap();
        m_operationsService->setMapProjection(
            operations.value(QStringLiteral("Projection"), QStringLiteral("Equirectangular")).toString());
        m_operationsService->setDataViewMode(
            operations.value(QStringLiteral("DataView"), QStringLiteral("Live + Logbook")).toString());
        QString const active = operations.value(QStringLiteral("ActivePreset"),
                                                  QStringLiteral("Operational")).toString();
        m_operationsService->applyMapPreset(active);
    }
    scheduleQuery();
    return root.value(QStringLiteral("viewport")).toObject().toVariantMap();
}

void MapIntelligenceService::reloadFromAdif(const QString& path)
{
    QString const cleanPath = QFileInfo(path).absoluteFilePath();
    quint64 const importGeneration = ++m_importGeneration;
    quint64 const queryGeneration = ++m_queryGeneration;
    if (m_sourcePath != cleanPath) {
        m_sourcePath = cleanPath;
        emit sourcePathChanged();
    }
    setLoading(true);

    QString const database = m_databasePath;
    QueryOptions const options {
        m_bandFilter, m_modeFilter, m_periodFilter, m_continentFilter,
        m_dxccFilter, m_sourceFilter, m_propagationFilter, m_rosterSort, m_rosterStatusFilter,
        m_rosterHuntScope, m_rosterScope, m_rosterDxccScope,
        m_rosterMyCall, m_rosterMyDxcc, m_activeAwardProgram, m_awardGoal,
        m_rosterRetentionMinutes, m_gridPrecision, m_liveDecayMinutes,
        m_sourceDecayMinutes,
        m_cqOnly, m_rosterSortDescending, m_rosterCqOnly,
        m_splitGridEnabled, m_rosterTextFilter, m_rosterTextMode,
        pskLayerEnabled(), m_pskDisplayMode, m_pskOpacityPercent / 100.0,
        m_spotAgeFilter, m_spotCorrelationFilter, m_bandActivityWindowHours,
        m_rosterUsesLoTW, m_rosterMaxLoTWDays, m_rosterUsesEQSL,
        m_rosterUsesOQRS, m_rosterSpottedMeOnly, m_rosterMinSnrEnabled,
        m_rosterMinSnr, m_rosterMaxDtEnabled, m_rosterMaxDt,
        m_rosterTreatRr73AsCq, m_rosterWantedTypes,
        m_awardEndorsement, m_awardConfirmation, m_awardCallsign,
        m_awardFromDate, m_awardToDate
    };
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, cleanPath, database, options, importGeneration, queryGeneration] {
            QByteArray data;
            QString error;
            QFile file(cleanPath);
            bool const sourceReadable = file.open(QIODevice::ReadOnly);
            if (sourceReadable) {
                data = file.read(kMaxAdifBytes + 1);
                if (data.size() > kMaxAdifBytes) {
                    data.truncate(kMaxAdifBytes);
                }
            } else {
                error = QStringLiteral("Cannot read ADIF log %1: %2")
                            .arg(cleanPath, file.errorString());
            }

            // Do not replace a valid local map cache with an empty import when
            // a log is temporarily unavailable (network drive, permissions or
            // a log rotation in progress).
            bool const imported = sourceReadable && importAdifIntoDatabase(
                database, cleanPath, data, adifFingerprint(cleanPath),
                options.awardCallsign, &error);
            Snapshot snapshot = queryDatabase(database, options);
            if ((!sourceReadable || !imported) && snapshot.error.isEmpty()) {
                snapshot.error = error;
            }

            if (!guard) {
                return;
            }
            QMetaObject::invokeMethod(guard.data(),
                [guard, importGeneration, queryGeneration, snapshot = std::move(snapshot)]() mutable {
                    if (!guard) {
                        return;
                    }
                    if (importGeneration == guard->m_importGeneration.load()) {
                        guard->setLoading(false);
                    }
                    guard->applySnapshot(queryGeneration, std::move(snapshot));
                    if (guard->m_operationsService) {
                        guard->m_operationsService->refreshLogbook();
                    }
                }, Qt::QueuedConnection);
        }));
}

void MapIntelligenceService::appendAdifRecord(const QByteArray& record)
{
    QString const database = m_databasePath;
    QString const defaultOperatorCall = m_awardCallsign;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, record, defaultOperatorCall] {
        QList<QsoRecord> records = parseAdif(record);
        for (QsoRecord& qso : records) {
            if (qso.operatorCall.isEmpty()) {
                qso.operatorCall = defaultOperatorCall;
            }
        }
        QString error;
        if (!records.isEmpty()) {
            appendQsoRecords(database, records, &error);
        }
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) {
                return;
            }
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] incremental ADIF insert failed:" << error;
            }
            if (guard->m_operationsService) {
                guard->m_operationsService->refreshLogbook();
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::refresh()
{
    scheduleQuery();
    if (!m_selectedGrid.isEmpty()) {
        selectGrid(m_selectedGrid);
    }
}

void MapIntelligenceService::selectGrid(const QString& grid)
{
    QString const fullGrid = normalizedGrid(grid);
    QString const normalized =
        fullGrid.left(m_gridPrecision == 6 && fullGrid.size() >= 6 ? 6 : 4);
    if (normalized.isEmpty()) {
        clearGridSelection();
        return;
    }

    quint64 const generation = ++m_gridDetailsGeneration;
    m_selectedGrid = normalized;
    m_selectedGridLive.clear();
    m_selectedGridQsos.clear();
    m_selectedGridSummary = {
        {QStringLiteral("grid"), normalized},
        {QStringLiteral("workedCount"), 0},
        {QStringLiteral("confirmedCount"), 0},
        {QStringLiteral("activeCount"), 0},
        {QStringLiteral("pskCount"), 0}
    };
    for (QVariant const& value : std::as_const(m_rawCoverage)) {
        QVariantMap const row = value.toMap();
        if (row.value(QStringLiteral("grid")).toString()
                .compare(normalized, Qt::CaseInsensitive) == 0) {
            m_selectedGridSummary = row;
            break;
        }
    }
    emit gridDetailsChanged();
    setGridDetailsLoading(true);

    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, normalized, generation] {
        GridDetails details = queryGridDetails(database, normalized);
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(),
            [guard, generation, normalized, details = std::move(details)]() mutable {
                if (guard) {
                    guard->applyGridDetails(generation, normalized,
                                            std::move(details));
                }
            }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::clearGridSelection()
{
    ++m_gridDetailsGeneration;
    bool const hadSelection = !m_selectedGrid.isEmpty()
        || !m_selectedGridSummary.isEmpty()
        || !m_selectedGridLive.isEmpty()
        || !m_selectedGridQsos.isEmpty();
    m_selectedGrid.clear();
    m_selectedGridSummary.clear();
    m_selectedGridLive.clear();
    m_selectedGridQsos.clear();
    setGridDetailsLoading(false);
    if (hadSelection) {
        emit gridDetailsChanged();
    }
}

void MapIntelligenceService::clearLiveSpots()
{
    m_pendingDecodes.clear();
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database] {
        QString error;
        clearLiveSpotRows(database, &error);
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) {
                return;
            }
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] clear live spots failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::clearAlerts()
{
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database] {
        QString error;
        clearAlertRows(database, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] clear alerts failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::markAlertsRead()
{
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database] {
        QString error;
        markAlertRowsRead(database, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] mark alerts read failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::setRosterCallWatched(const QString& call, bool watched)
{
    QString const normalizedCall = call.trimmed().toUpper();
    if (normalizedCall.isEmpty()) return;
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, normalizedCall, watched] {
        QString error;
        updateRosterPreference(database, normalizedCall, watched, false, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] update watched call failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::setRosterCallIgnored(const QString& call, bool ignored)
{
    QString const normalizedCall = call.trimmed().toUpper();
    if (normalizedCall.isEmpty()) return;
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, normalizedCall, ignored] {
        QString error;
        updateRosterPreference(database, normalizedCall, false, ignored, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] update ignored call failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::setRosterDxccIgnored(const QString& dxcc, bool ignored)
{
    QString const normalizedDxcc = dxcc.trimmed();
    if (normalizedDxcc.isEmpty()) return;
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, database, normalizedDxcc, ignored] {
            QString error;
            updateRosterIgnore(database, QStringLiteral("DXCC"),
                               normalizedDxcc, ignored, &error);
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(), [guard, error] {
                if (!guard) return;
                if (!error.isEmpty()) {
                    qWarning().noquote()
                        << "[MAPINT] update ignored DXCC failed:" << error;
                }
                guard->scheduleQuery();
            }, Qt::QueuedConnection);
        }));
}

void MapIntelligenceService::removeRosterPreference(const QString& type,
                                                    const QString& value)
{
    QString const normalizedType = type.trimmed().toUpper();
    QString const normalizedValue = value.trimmed();
    if (normalizedValue.isEmpty()
        || (normalizedType != QStringLiteral("WATCH")
            && normalizedType != QStringLiteral("CALL")
            && normalizedType != QStringLiteral("DXCC"))) {
        return;
    }
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, database, normalizedType, normalizedValue] {
            QString error;
            removeRosterPreferenceRow(database, normalizedType,
                                      normalizedValue, &error);
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(), [guard, error] {
                if (!guard) return;
                if (!error.isEmpty()) {
                    qWarning().noquote()
                        << "[MAPINT] remove roster preference failed:" << error;
                }
                guard->scheduleQuery();
            }, Qt::QueuedConnection);
        }));
}

void MapIntelligenceService::clearRosterPreferences()
{
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database] {
        QString error;
        clearRosterPreferenceRows(database, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] clear roster preferences failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::configurePskFeed(const QString& callsign,
                                              const QString& grid)
{
    if (!m_pskFeedService) return;
    m_pskFeedService->configureStation(callsign, grid);
    if (!m_pskFeedService->enabled()) {
        m_pskFeedService->setEnabled(true);
        saveSetting(QStringLiteral("PskMqttEnabled"), true);
    }
}

void MapIntelligenceService::setRosterRule(const QString& type,
                                           const QString& value,
                                           const QString& action,
                                           const QString& band,
                                           const QString& mode)
{
    QString const normalizedType = type.trimmed().toUpper().left(24);
    QString const normalizedValue = value.trimmed().toUpper().left(128);
    QString const normalizedAction = action.trimmed().toUpper().left(24);
    if (normalizedType.isEmpty() || normalizedValue.isEmpty()
        || !QStringList {QStringLiteral("WANTED"), QStringLiteral("IGNORE"),
                         QStringLiteral("WATCH")}
                .contains(normalizedAction)) {
        return;
    }
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, normalizedType,
                                         normalizedValue, normalizedAction,
                                         band, mode] {
        QString error;
        updateRosterRuleRow(database, normalizedType, normalizedValue,
                            normalizedAction, band, mode, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] update roster rule failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::removeRosterRule(const QString& type,
                                              const QString& value,
                                              const QString& band,
                                              const QString& mode)
{
    QString const normalizedType = type.trimmed().toUpper().left(24);
    QString const normalizedValue = value.trimmed().toUpper().left(128);
    if (normalizedType.isEmpty() || normalizedValue.isEmpty()) return;
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, normalizedType,
                                         normalizedValue, band, mode] {
        QString error;
        removeRosterRuleRow(database, normalizedType, normalizedValue,
                            band, mode, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] remove roster rule failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::setRosterStationCall(const QString& call)
{
    QString const normalized = call.trimmed().toUpper();
    bool const rosterChanged = m_rosterMyCall != normalized;
    bool const awardChanged = m_awardCallsign.isEmpty() && !normalized.isEmpty();
    if (!rosterChanged && !awardChanged) return;
    m_rosterMyCall = normalized;
    if (awardChanged) {
        m_awardCallsign = normalized;
        saveSetting(QStringLiteral("AwardCallsign"), normalized);
        emit awardFiltersChanged();
    }
    m_rosterMyDxcc.clear();
    if (!m_rosterMyCall.isEmpty()) {
        std::shared_ptr<const DxccLookup> const lookup = adifDxccLookup();
        if (lookup) {
            DxccEntity const entity = lookup->lookup(m_rosterMyCall);
            if (entity.isValid()) {
                m_rosterMyDxcc = entity.name;
            }
        }
    }
    scheduleQuery();
}

void MapIntelligenceService::ingestPskSpots(const QVariantList& rows,
                                            const QString& senderCall,
                                            const QString& senderGrid)
{
    queuePskSpots(rows, senderCall, senderGrid, false);
}

void MapIntelligenceService::replacePskHeardBySpots(const QVariantList& rows,
                                                    const QString& senderCall,
                                                    const QString& senderGrid)
{
    queuePskSpots(rows, senderCall, senderGrid, true);
}

void MapIntelligenceService::queuePskSpots(const QVariantList& rows,
                                           const QString& senderCall,
                                           const QString& senderGrid,
                                           bool replaceHeardBySnapshot)
{
    if (rows.isEmpty() && !replaceHeardBySnapshot) return;
    QString const database = m_databasePath;
    AlertRules const rules {
        m_alertNewGridEnabled, m_alertNewDxccEnabled,
        m_alertCqEnabled, m_alertCallPattern
    };
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, rows, senderCall, senderGrid,
                                          rules, replaceHeardBySnapshot] {
        QList<LiveSpot> spots;
        spots.reserve(rows.size());
        qint64 const now = QDateTime::currentMSecsSinceEpoch();
        for (QVariant const& value : rows) {
            QVariantMap const row = value.toMap();
            LiveSpot spot;
            spot.call = row.value(QStringLiteral("call")).toString().trimmed().toUpper();
            spot.grid = normalizedGrid(row.value(QStringLiteral("grid")).toString());
            spot.grid4 = spot.grid.left(4);
            spot.grid6 = spot.grid.size() >= 6 ? spot.grid.left(6) : QString();
            spot.frequencyHz = row.value(QStringLiteral("freq"),
                                         row.value(QStringLiteral("frequency"))).toLongLong();
            spot.band = normalizedBand(row.value(QStringLiteral("band")).toString(),
                                       spot.frequencyHz / 1.0e6);
            spot.mode = normalizedMode(row.value(QStringLiteral("mode")).toString());
            spot.propagationMode = normalizePropagationMode(
                row.value(QStringLiteral("propMode"),
                          row.value(QStringLiteral("propagationMode"),
                                    row.value(QStringLiteral("propagation"),
                                              row.value(QStringLiteral("prop_mode")))))
                    .toString());
            spot.snr = row.value(QStringLiteral("snr")).toInt();
            spot.distanceKm = row.value(QStringLiteral("distKm"), -1.0).toDouble();
            spot.dxcc = row.value(QStringLiteral("dxcc")).toString().trimmed();
            spot.continent = row.value(QStringLiteral("continent")).toString().trimmed().toUpper();
            spot.cqZone = row.value(QStringLiteral("cqZone")).toInt();
            spot.ituZone = row.value(QStringLiteral("ituZone")).toInt();
            spot.state = row.value(QStringLiteral("state")).toString().trimmed().toUpper();
            spot.county = row.value(QStringLiteral("county"),
                                    row.value(QStringLiteral("cnty"))).toString()
                              .trimmed().toUpper();
            spot.potaReference = normalizedPota(
                row.value(QStringLiteral("pota"), row.value(QStringLiteral("potaReference")))
                    .toString());
            spot.iotaReference = normalizedIota(
                row.value(QStringLiteral("iota"), row.value(QStringLiteral("iotaReference")))
                    .toString());
            spot.wpxPrefix = wpxPrefix(spot.call);
            bool dtOk = false;
            spot.dt = row.value(QStringLiteral("dt")).toDouble(&dtOk);
            if (!dtOk || !std::isfinite(spot.dt)) spot.dt = 0.0;
            spot.source = row.value(QStringLiteral("source"),
                                    QStringLiteral("psk")).toString().trimmed().toLower();
            if (spot.source.isEmpty()) spot.source = QStringLiteral("psk");
            spot.targetCall = senderCall.trimmed().toUpper();
            spot.receiverCall = row.value(QStringLiteral("receiverCall"), senderCall)
                                    .toString().trimmed().toUpper();
            spot.receiverGrid = normalizedGrid(
                row.value(QStringLiteral("receiverGrid"), senderGrid).toString());
            spot.provider = row.value(QStringLiteral("provider"),
                                      QStringLiteral("PSK Reporter")).toString().trimmed();
            if (!spot.grid.isEmpty()) {
                QString const explicitGridOrigin = normalizedGridOrigin(
                    row.value(QStringLiteral("gridOrigin"),
                              row.value(QStringLiteral("gridSource"))).toString());
                spot.gridOrigin = explicitGridOrigin != QStringLiteral("UNKNOWN")
                    ? explicitGridOrigin
                    : gridOriginForSource(spot.source, spot.provider);
            }
            spot.direction = row.value(QStringLiteral("direction"),
                                       QStringLiteral("TX")).toString()
                                 .trimmed().toUpper();
            if (spot.direction != QStringLiteral("RX")
                && spot.direction != QStringLiteral("TX")) {
                spot.direction = QStringLiteral("TX");
            }
            spot.message = QStringLiteral("%1 heard %2 from %3")
                               .arg(spot.call, senderCall.trimmed().toUpper(),
                                    senderGrid.trimmed().toUpper());
            spot.observedMs = row.value(QStringLiteral("timestamp")).toLongLong();
            if (spot.observedMs > 0 && spot.observedMs < 100000000000LL) {
                spot.observedMs *= 1000;
            }
            if (spot.observedMs <= 0 || spot.observedMs > now + 5 * 60 * 1000LL) {
                spot.observedMs = now;
            }
            spot.observedUtc = QDateTime::fromMSecsSinceEpoch(now, QTimeZone::UTC)
                                   .toString(Qt::ISODate);
            spot.observedUtc = QDateTime::fromMSecsSinceEpoch(spot.observedMs, QTimeZone::UTC)
                                   .toString(Qt::ISODate);
            spot.activityType = spot.source.compare(QStringLiteral("oams"), Qt::CaseInsensitive) == 0
                ? QStringLiteral("OAMS") : QStringLiteral("PSK");
            spot.uniqueKey = digestKey({
                spot.source, spot.direction, spot.call, spot.grid, spot.receiverCall,
                QString::number(spot.frequencyHz), QString::number(spot.observedMs / 60000)
            });
            if (!spot.call.isEmpty()) spots.append(std::move(spot));
        }
        QString error;
        if (replaceHeardBySnapshot) {
            clearPskHeardByRows(database, &error);
        }
        if (error.isEmpty() && !spots.isEmpty()) {
            appendLiveSpots(database, spots, rules, &error);
        }
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] PSK spot batch failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::ingestDecodeEntry(const QVariantMap& entry,
                                               qint64 dialFrequencyHz,
                                               const QString& band)
{
    if (entry.value(QStringLiteral("partialDecode")).toBool()
        || entry.value(QStringLiteral("unresolvedHash")).toBool()
        || entry.value(QStringLiteral("message")).toString().trimmed().isEmpty()) {
        return;
    }
    if (m_pendingDecodes.size() >= kMaxPendingLiveSpots) {
        m_pendingDecodes.removeFirst();
    }
    m_pendingDecodes.append(PendingDecode {entry, dialFrequencyHz, band});
    if (!m_liveFlushTimer->isActive()) {
        m_liveFlushTimer->start();
    }
}

void MapIntelligenceService::scheduleQuery()
{
    ++m_queryGeneration;
    m_queryTimer->start();
}

void MapIntelligenceService::flushPendingLiveSpots()
{
    if (m_pendingDecodes.isEmpty()) {
        return;
    }
    QList<PendingDecode> pending;
    pending.swap(m_pendingDecodes);
    QString const database = m_databasePath;
    AlertRules const rules {
        m_alertNewGridEnabled, m_alertNewDxccEnabled,
        m_alertCqEnabled, m_alertCallPattern
    };
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, rules,
                                         pending = std::move(pending)] {
        QList<LiveSpot> spots;
        spots.reserve(pending.size());
        for (PendingDecode const& decode : pending) {
            LiveSpot spot = liveSpotFromEntry(
                decode.entry, decode.dialFrequencyHz, decode.band);
            if (!spot.call.isEmpty()) {
                spots.append(std::move(spot));
            }
        }
        QString error;
        if (!spots.isEmpty()) {
            appendLiveSpots(database, spots, rules, &error);
        }
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) {
                return;
            }
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] live spot batch failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::flushPendingSnapshot()
{
    if (!m_snapshotPending) {
        return;
    }

    m_snapshotPending = false;
    Snapshot snapshot = std::move(m_pendingSnapshot);
    m_pendingSnapshot = Snapshot {};
    applySnapshotNow(std::move(snapshot));
}

void MapIntelligenceService::queueSnapshotNotifications(int flags)
{
    if (flags == SnapshotNotifyNone) {
        return;
    }
    m_pendingSnapshotNotificationFlags |= flags;
    if (!m_snapshotNotificationTimer->isActive()) {
        // Queue after the snapshot handoff. This leaves the current event
        // loop turn available to audio, CAT and the scene graph.
        m_snapshotNotificationTimer->start(0);
    }
}

void MapIntelligenceService::flushSnapshotNotifications()
{
    int const pending = m_pendingSnapshotNotificationFlags;
    if (pending == SnapshotNotifyNone) {
        return;
    }

    // Coverage is the only map-visible data that needs the first turn. The
    // following groups are independent dashboards and can safely yield one
    // or two frames to the renderer between their notifications.
    int flags = SnapshotNotifyNone;
    int nextDelayMs = 45;
    int const coverageGroup = SnapshotNotifyFilters | SnapshotNotifyCoverage;
    int const rosterGroup = SnapshotNotifyRoster | SnapshotNotifyPreferences;
    int const activityGroup = SnapshotNotifySpotAnalytics
        | SnapshotNotifyBandActivity | SnapshotNotifyPropagation;
    int const auxiliaryGroup = SnapshotNotifyAwards | SnapshotNotifyAlerts
        | SnapshotNotifyRules | SnapshotNotifyMatrices | SnapshotNotifyStatistics;
    if (pending & coverageGroup) {
        flags = pending & coverageGroup;
    } else if (pending & rosterGroup) {
        flags = pending & rosterGroup;
    } else if (pending & activityGroup) {
        flags = pending & activityGroup;
    } else {
        flags = pending & auxiliaryGroup;
        nextDelayMs = 0;
    }
    m_pendingSnapshotNotificationFlags &= ~flags;

    QElapsedTimer timer;
    timer.start();
    if (flags & SnapshotNotifyFilters) {
        emit filtersChanged();
    }
    if (flags & SnapshotNotifyCoverage) {
        rebuildVisibleCoverage();
    }
    if (flags & SnapshotNotifyRoster) {
        emit rosterChanged();
    }
    if (flags & SnapshotNotifyPreferences) {
        emit rosterPreferencesChanged();
    }
    if (flags & SnapshotNotifyBandActivity) {
        emit bandActivityChanged();
    }
    if (flags & SnapshotNotifyPropagation) {
        emit propagationStatisticsChanged();
    }
    if (flags & SnapshotNotifySpotAnalytics) {
        emit spotAnalyticsChanged();
    }
    if (flags & SnapshotNotifyAwards) {
        emit awardsChanged();
    }
    if (flags & SnapshotNotifyAlerts) {
        emit alertsChanged();
    }
    if (flags & SnapshotNotifyRules) {
        emit rosterRulesChanged();
    }
    if (flags & SnapshotNotifyMatrices) {
        emit rosterMatricesChanged();
    }
    if (flags & SnapshotNotifyStatistics) {
        emit statisticsChanged();
    }

    qint64 const elapsedMs = timer.elapsed();
    if (elapsedMs >= 50) {
        qWarning().noquote()
            << "[MAPINT] notification slice"
            << "flags=" << flags
            << "elapsed_ms=" << elapsedMs
            << "remaining=" << m_pendingSnapshotNotificationFlags;
    }
    if (m_pendingSnapshotNotificationFlags != SnapshotNotifyNone) {
        m_snapshotNotificationTimer->start(nextDelayMs);
    }
}

QList<MapIntelligenceService::QsoRecord>
MapIntelligenceService::parseAdif(const QByteArray& data)
{
    QList<QsoRecord> records;
    std::shared_ptr<const DxccLookup> const dxccLookup = adifDxccLookup();
    auto enrichRecord = [dxccLookup](QsoRecord& record) {
        if (!dxccLookup || record.call.isEmpty()) {
            return;
        }

        DxccEntity const entity = dxccLookup->lookup(record.call);
        if (!entity.isValid()) {
            return;
        }
        if (record.dxcc.isEmpty()) {
            record.dxcc = entity.name;
        }
        if (record.continent.isEmpty()) {
            record.continent = entity.continent;
        }
        if (record.cqZone <= 0) {
            record.cqZone = entity.cqZone;
        }
        if (record.ituZone <= 0) {
            record.ituZone = entity.ituZone;
        }
    };
    QHash<QString, QString> fields;
    int position = 0;
    while (position < data.size() && records.size() < kMaxAdifRecords) {
        int const open = data.indexOf('<', position);
        if (open < 0) {
            break;
        }
        int const close = data.indexOf('>', open + 1);
        if (close < 0) {
            break;
        }

        QList<QByteArray> const parts = data.mid(open + 1, close - open - 1).trimmed().split(':');
        QString const name = QString::fromLatin1(parts.value(0)).trimmed().toUpper();
        position = close + 1;

        auto appendRecord = [&records, &enrichRecord](QHash<QString, QString> const& recordFields) {
            if (recordFields.isEmpty()) {
                return;
            }
            QsoRecord record;
            record.call = recordFields.value(QStringLiteral("CALL")).trimmed().toUpper();
            record.grid = normalizedGrid(recordFields.value(QStringLiteral("GRIDSQUARE")));
            record.vuccGrids = normalizedVuccGrids(
                recordFields.value(QStringLiteral("VUCC_GRIDS")));
            if (record.grid.isEmpty() && !record.vuccGrids.isEmpty()) {
                record.grid = record.vuccGrids.takeFirst();
            } else if (!record.grid.isEmpty()) {
                record.vuccGrids.removeAll(record.grid);
                record.vuccGrids.removeAll(record.grid.left(4));
            }
            record.grid4 = record.grid.left(4);
            record.grid6 = record.grid.size() >= 6 ? record.grid.left(6) : QString();
            bool frequencyOk = false;
            record.frequencyMhz = recordFields.value(QStringLiteral("FREQ")).toDouble(&frequencyOk);
            if (!frequencyOk) {
                record.frequencyMhz = 0.0;
            }
            bool receiveFrequencyOk = false;
            record.receiveFrequencyMhz = recordFields.value(QStringLiteral("FREQ_RX"))
                                             .toDouble(&receiveFrequencyOk);
            if (!receiveFrequencyOk) record.receiveFrequencyMhz = 0.0;
            record.band = normalizedBand(recordFields.value(QStringLiteral("BAND")),
                                         record.frequencyMhz);
            record.mode = normalizedMode(recordFields.value(QStringLiteral("MODE")),
                                         recordFields.value(QStringLiteral("SUBMODE")));
            record.propagationMode = normalizePropagationMode(
                recordFields.value(QStringLiteral("PROP_MODE")));
            record.satelliteName = recordFields.value(QStringLiteral("SAT_NAME")).trimmed();
            record.satelliteMode = recordFields.value(QStringLiteral("SAT_MODE")).trimmed();
            record.qsoDate = recordFields.value(QStringLiteral("QSO_DATE")).trimmed();
            record.timeOn = recordFields.value(QStringLiteral("TIME_ON")).trimmed();
            record.qsoEpoch = adifEpoch(record.qsoDate, record.timeOn);
            record.operatorCall = recordFields.value(QStringLiteral("STATION_CALLSIGN"))
                                      .trimmed().toUpper();
            if (record.operatorCall.isEmpty()) {
                record.operatorCall = recordFields.value(QStringLiteral("OPERATOR"))
                                          .trimmed().toUpper();
            }
            if (record.operatorCall.isEmpty()) {
                record.operatorCall = recordFields.value(QStringLiteral("OWNER_CALLSIGN"))
                                          .trimmed().toUpper();
            }
            record.source = normalizedFilter(
                recordFields.value(QStringLiteral("APP_DECODIUM_SOURCE")));
            if (record.source.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0) {
                record.source = QStringLiteral("ADIF");
            }
            record.dxcc = recordFields.value(QStringLiteral("COUNTRY")).trimmed();
            record.dxccNumber = recordFields.value(QStringLiteral("DXCC")).toInt();
            record.continent = recordFields.value(QStringLiteral("CONT")).trimmed().toUpper();
            record.cqZone = recordFields.value(QStringLiteral("CQZ")).toInt();
            record.ituZone = recordFields.value(QStringLiteral("ITUZ")).toInt();
            record.state = recordFields.value(QStringLiteral("STATE")).trimmed().toUpper();
            record.county = recordFields.value(QStringLiteral("CNTY")).trimmed().toUpper();
            if (record.county.isEmpty()) {
                record.county = recordFields.value(QStringLiteral("COUNTY")).trimmed().toUpper();
            }
            record.lotwConfirmed = adifYes(recordFields.value(QStringLiteral("LOTW_QSL_RCVD")));
            record.eqslConfirmed = adifYes(recordFields.value(QStringLiteral("EQSL_QSL_RCVD")));
            record.oqrs = adifYes(recordFields.value(QStringLiteral("OQRS")));
            record.potaReference =
                normalizedPota(recordFields.value(QStringLiteral("POTA_REF")));
            if (record.potaReference.isEmpty()
                && recordFields.value(QStringLiteral("SIG"))
                       .compare(QStringLiteral("POTA"), Qt::CaseInsensitive) == 0) {
                record.potaReference =
                    normalizedPota(recordFields.value(QStringLiteral("SIG_INFO")));
            }
            record.iotaReference =
                normalizedIota(recordFields.value(QStringLiteral("IOTA")));
            record.wpxPrefix = wpxPrefix(record.call);
            record.confirmed = isConfirmed(recordFields);
            enrichRecord(record);
            record.sourceKey = digestKey({
                record.operatorCall, record.call, record.grid, record.band, record.mode,
                record.qsoDate, record.timeOn,
                QString::number(record.frequencyMhz, 'f', 6)
            });
            records.append(std::move(record));
        };

        if (name == QStringLiteral("EOH")) {
            fields.clear();
            continue;
        }
        if (name == QStringLiteral("EOR")) {
            appendRecord(fields);
            fields.clear();
            continue;
        }
        if (parts.size() < 2) {
            continue;
        }

        bool lengthOk = false;
        int const length = parts.at(1).trimmed().toInt(&lengthOk);
        if (!lengthOk || length < 0 || position + length > data.size()) {
            continue;
        }
        static const QSet<QString> wanted {
            QStringLiteral("CALL"), QStringLiteral("GRIDSQUARE"),
            QStringLiteral("VUCC_GRIDS"),
            QStringLiteral("BAND"), QStringLiteral("FREQ"),
            QStringLiteral("MODE"), QStringLiteral("SUBMODE"),
            QStringLiteral("PROP_MODE"),
            QStringLiteral("SAT_NAME"), QStringLiteral("SAT_MODE"),
            QStringLiteral("FREQ_RX"),
            QStringLiteral("QSO_DATE"), QStringLiteral("TIME_ON"),
            QStringLiteral("STATION_CALLSIGN"), QStringLiteral("OPERATOR"),
            QStringLiteral("OWNER_CALLSIGN"), QStringLiteral("DXCC"),
            QStringLiteral("QSL_RCVD"), QStringLiteral("LOTW_QSL_RCVD"),
            QStringLiteral("EQSL_QSL_RCVD"), QStringLiteral("COUNTRY"),
            QStringLiteral("CONT"), QStringLiteral("CQZ"),
            QStringLiteral("ITUZ"), QStringLiteral("STATE"),
            QStringLiteral("CNTY"), QStringLiteral("COUNTY"),
            QStringLiteral("OQRS"),
            QStringLiteral("POTA_REF"), QStringLiteral("IOTA"),
            QStringLiteral("SIG"), QStringLiteral("SIG_INFO"),
            QStringLiteral("APP_DECODIUM_SOURCE")
        };
        if (wanted.contains(name)) {
            fields.insert(name, decodedAdifValue(data.mid(position, length)));
        }
        position += length;
    }
    if (!fields.isEmpty() && records.size() < kMaxAdifRecords) {
        QsoRecord record;
        record.call = fields.value(QStringLiteral("CALL")).trimmed().toUpper();
        record.grid = normalizedGrid(fields.value(QStringLiteral("GRIDSQUARE")));
        record.vuccGrids = normalizedVuccGrids(
            fields.value(QStringLiteral("VUCC_GRIDS")));
        if (record.grid.isEmpty() && !record.vuccGrids.isEmpty()) {
            record.grid = record.vuccGrids.takeFirst();
        } else if (!record.grid.isEmpty()) {
            record.vuccGrids.removeAll(record.grid);
            record.vuccGrids.removeAll(record.grid.left(4));
        }
        record.grid4 = record.grid.left(4);
        record.grid6 = record.grid.size() >= 6 ? record.grid.left(6) : QString();
        bool ok = false;
        record.frequencyMhz = fields.value(QStringLiteral("FREQ")).toDouble(&ok);
        if (!ok) record.frequencyMhz = 0.0;
        bool receiveFrequencyOk = false;
        record.receiveFrequencyMhz = fields.value(QStringLiteral("FREQ_RX"))
                                         .toDouble(&receiveFrequencyOk);
        if (!receiveFrequencyOk) record.receiveFrequencyMhz = 0.0;
        record.band = normalizedBand(fields.value(QStringLiteral("BAND")), record.frequencyMhz);
        record.mode = normalizedMode(fields.value(QStringLiteral("MODE")),
                                     fields.value(QStringLiteral("SUBMODE")));
        record.propagationMode = normalizePropagationMode(
            fields.value(QStringLiteral("PROP_MODE")));
        record.satelliteName = fields.value(QStringLiteral("SAT_NAME")).trimmed();
        record.satelliteMode = fields.value(QStringLiteral("SAT_MODE")).trimmed();
        record.qsoDate = fields.value(QStringLiteral("QSO_DATE")).trimmed();
        record.timeOn = fields.value(QStringLiteral("TIME_ON")).trimmed();
        record.qsoEpoch = adifEpoch(record.qsoDate, record.timeOn);
        record.operatorCall = fields.value(QStringLiteral("STATION_CALLSIGN"))
                                  .trimmed().toUpper();
        if (record.operatorCall.isEmpty()) {
            record.operatorCall = fields.value(QStringLiteral("OPERATOR"))
                                      .trimmed().toUpper();
        }
        if (record.operatorCall.isEmpty()) {
            record.operatorCall = fields.value(QStringLiteral("OWNER_CALLSIGN"))
                                      .trimmed().toUpper();
        }
        record.source = normalizedFilter(fields.value(QStringLiteral("APP_DECODIUM_SOURCE")));
        if (record.source.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0) {
            record.source = QStringLiteral("ADIF");
        }
        record.dxcc = fields.value(QStringLiteral("COUNTRY")).trimmed();
        record.dxccNumber = fields.value(QStringLiteral("DXCC")).toInt();
        record.continent = fields.value(QStringLiteral("CONT")).trimmed().toUpper();
        record.cqZone = fields.value(QStringLiteral("CQZ")).toInt();
        record.ituZone = fields.value(QStringLiteral("ITUZ")).toInt();
        record.state = fields.value(QStringLiteral("STATE")).trimmed().toUpper();
        record.county = fields.value(QStringLiteral("CNTY")).trimmed().toUpper();
        if (record.county.isEmpty()) {
            record.county = fields.value(QStringLiteral("COUNTY")).trimmed().toUpper();
        }
        record.lotwConfirmed = adifYes(fields.value(QStringLiteral("LOTW_QSL_RCVD")));
        record.eqslConfirmed = adifYes(fields.value(QStringLiteral("EQSL_QSL_RCVD")));
        record.oqrs = adifYes(fields.value(QStringLiteral("OQRS")));
        record.potaReference =
            normalizedPota(fields.value(QStringLiteral("POTA_REF")));
        if (record.potaReference.isEmpty()
            && fields.value(QStringLiteral("SIG"))
                   .compare(QStringLiteral("POTA"), Qt::CaseInsensitive) == 0) {
            record.potaReference =
                normalizedPota(fields.value(QStringLiteral("SIG_INFO")));
        }
        record.iotaReference =
            normalizedIota(fields.value(QStringLiteral("IOTA")));
        record.wpxPrefix = wpxPrefix(record.call);
        record.confirmed = isConfirmed(fields);
        enrichRecord(record);
        record.sourceKey = digestKey({
            record.operatorCall, record.call, record.grid, record.band, record.mode,
            record.qsoDate, record.timeOn,
            QString::number(record.frequencyMhz, 'f', 6)
        });
        records.append(std::move(record));
    }
    return records;
}

MapIntelligenceService::LiveSpot
MapIntelligenceService::liveSpotFromEntry(const QVariantMap& entry,
                                          qint64 dialFrequencyHz,
                                          const QString& band)
{
    LiveSpot spot;
    if (entry.value(QStringLiteral("partialDecode")).toBool()
        || entry.value(QStringLiteral("unresolvedHash")).toBool()) {
        return spot;
    }

    spot.direction = entry.value(QStringLiteral("isTx")).toBool()
        ? QStringLiteral("TX") : QStringLiteral("RX");
    spot.message = entry.value(QStringLiteral("message")).toString().trimmed();
    if (spot.message.isEmpty()) {
        return spot;
    }
    spot.source = entry.value(QStringLiteral("source"), QStringLiteral("decoder"))
                      .toString().trimmed().toLower();
    QString const entryCall =
        entry.value(QStringLiteral("fromCall")).toString().trimmed().toUpper();
    QString const fallbackCall = !entryCall.isEmpty()
        ? entryCall
        : entry.value(QStringLiteral("dxCallsign")).toString().trimmed().toUpper();
    // Directed weak-signal messages are "TO_CALL FROM_CALL payload".  The
    // final locator therefore belongs to the second callsign, not the first.
    QString const decodedTransmitter = spot.source == QStringLiteral("decoder")
        ? transmittingCallFromMessage(spot.message)
        : QString();
    spot.call = decodedTransmitter;
    if (spot.call.isEmpty()) {
        spot.call = fallbackCall;
    }

    QString const transmittedGrid = gridFromMessage(spot.message);
    QString const explicitGridOrigin = normalizedGridOrigin(
        entry.value(QStringLiteral("gridOrigin"),
                    entry.value(QStringLiteral("gridSource"))).toString());
    if (!transmittedGrid.isEmpty()
        && ((!decodedTransmitter.isEmpty())
            || messageAssociatesGridWithCall(spot.message, spot.call))) {
        // CQ, standard exchange and beacon messages place the sender's grid
        // in the decoded payload.  This is the only decoder-side location we
        // use for map coverage and grid detail popups.
        spot.grid = transmittedGrid;
        spot.gridOrigin = QStringLiteral("DECODED");
    } else if (spot.source == QStringLiteral("psk")
               || spot.source == QStringLiteral("oams")) {
        // External spot feeds provide a station location independently of a
        // decoded over-the-air payload, so their explicit grid remains valid.
        spot.grid = normalizedGrid(entry.value(QStringLiteral("dxGrid")).toString());
        if (!spot.grid.isEmpty()) {
            spot.gridOrigin = explicitGridOrigin != QStringLiteral("UNKNOWN")
                ? explicitGridOrigin : gridOriginForSource(spot.source);
        }
    } else if (explicitGridOrigin != QStringLiteral("UNKNOWN")) {
        // A future callbook/RTSN adapter may supply a locator even if the
        // received text did not contain one.  Keep it visible, but distinctly
        // marked as lower-confidence rather than pretending it was decoded.
        spot.grid = normalizedGrid(entry.value(QStringLiteral("dxGrid")).toString());
        if (!spot.grid.isEmpty()) {
            spot.gridOrigin = explicitGridOrigin;
        }
    }
    spot.grid4 = spot.grid.left(4);
    spot.grid6 = spot.grid.size() >= 6 ? spot.grid.left(6) : QString();
    spot.mode = normalizedMode(entry.value(QStringLiteral("mode")).toString());
    QVariant propagationValue = entry.value(QStringLiteral("propMode"));
    if (!propagationValue.isValid()) {
        propagationValue = entry.value(QStringLiteral("propagationMode"));
    }
    if (!propagationValue.isValid()) {
        propagationValue = entry.value(QStringLiteral("propagation"));
    }
    if (!propagationValue.isValid()) {
        propagationValue = entry.value(QStringLiteral("prop_mode"));
    }
    if (!propagationValue.isValid()) {
        propagationValue = entry.value(QStringLiteral("PROP_MODE"));
    }
    spot.propagationMode = normalizePropagationMode(propagationValue.toString());
    spot.band = normalizedBand(band, dialFrequencyHz / 1.0e6);
    spot.snr = entry.value(QStringLiteral("db")).toString().toInt();
    bool dtOk = false;
    spot.dt = entry.value(QStringLiteral("dt")).toString().toDouble(&dtOk);
    if (!dtOk) {
        spot.dt = entry.value(QStringLiteral("deltaTime")).toDouble(&dtOk);
    }
    if (!dtOk || !std::isfinite(spot.dt)) {
        spot.dt = 0.0;
    }
    spot.frequencyHz = dialFrequencyHz + entry.value(QStringLiteral("freq")).toLongLong();
    spot.distanceKm = entry.value(QStringLiteral("distanceKm"), -1.0).toDouble();
    spot.dxcc = entry.value(QStringLiteral("dxcc")).toString().trimmed();
    spot.dxccNumber = entry.value(QStringLiteral("dxccNumber"),
                                  entry.value(QStringLiteral("dxccNum"))).toInt();
    spot.continent = entry.value(QStringLiteral("continent")).toString().trimmed().toUpper();
    spot.cqZone = entry.value(QStringLiteral("cqZone")).toInt();
    spot.ituZone = entry.value(QStringLiteral("ituZone")).toInt();
    spot.state = entry.value(QStringLiteral("state")).toString().trimmed().toUpper();
    spot.county = entry.value(QStringLiteral("county"),
                              entry.value(QStringLiteral("cnty"))).toString()
                      .trimmed().toUpper();
    spot.potaReference = normalizedPota(
        entry.value(QStringLiteral("pota"), entry.value(QStringLiteral("potaReference")))
            .toString());
    if (spot.potaReference.isEmpty()) {
        spot.potaReference = potaFromMessage(spot.message);
    }
    spot.iotaReference = normalizedIota(
        entry.value(QStringLiteral("iota"), entry.value(QStringLiteral("iotaReference")))
            .toString());
    spot.wpxPrefix = wpxPrefix(spot.call);
    QString const decodedTarget = spot.source == QStringLiteral("decoder")
        ? targetCallFromMessage(spot.message)
        : QString();
    if (spot.source == QStringLiteral("decoder") && isGeneralCallMessage(spot.message)) {
        spot.targetCall.clear();
    } else {
        spot.targetCall = !decodedTarget.isEmpty()
            ? decodedTarget
            : entry.value(QStringLiteral("toCall")).toString().trimmed().toUpper();
    }
    if (spot.source == QStringLiteral("decoder") && !spot.call.isEmpty()) {
        bool const attributionChanged =
            !fallbackCall.isEmpty() && fallbackCall != spot.call;
        std::shared_ptr<const DxccLookup> const lookup = adifDxccLookup();
        DxccEntity const entity = lookup ? lookup->lookup(spot.call) : DxccEntity();
        if (entity.isValid()) {
            if (attributionChanged || spot.dxcc.isEmpty()) spot.dxcc = entity.name;
            if (attributionChanged || spot.continent.isEmpty()) {
                spot.continent = entity.continent.toUpper();
            }
            if (attributionChanged || spot.cqZone <= 0) spot.cqZone = entity.cqZone;
            if (attributionChanged || spot.ituZone <= 0) spot.ituZone = entity.ituZone;
        } else if (attributionChanged) {
            spot.dxcc.clear();
            spot.continent.clear();
            spot.cqZone = 0;
            spot.ituZone = 0;
        }
        if (attributionChanged) {
            spot.state.clear();
        }
    }
    spot.isCq = entry.value(QStringLiteral("isCQ")).toBool()
        || spot.message.compare(QStringLiteral("CQ"), Qt::CaseInsensitive) == 0
        || spot.message.startsWith(QStringLiteral("CQ "), Qt::CaseInsensitive);
    spot.observedMs = entry.value(QStringLiteral("timestamp")).toLongLong();
    if (spot.observedMs <= 0) {
        spot.observedMs = QDateTime::currentMSecsSinceEpoch();
    }
    spot.observedUtc = QDateTime::fromMSecsSinceEpoch(spot.observedMs, QTimeZone::UTC)
                           .toString(Qt::ISODate);
    spot.activityType = activityTypeForMessage(
        spot.message, spot.mode, spot.source, spot.isCq, spot.targetCall);
    spot.uniqueKey = digestKey({
        spot.direction, entry.value(QStringLiteral("time")).toString(),
        spot.call, spot.grid, spot.band, spot.mode,
        QString::number(spot.frequencyHz), spot.message
    });
    return spot;
}

MapIntelligenceService::Snapshot
MapIntelligenceService::queryDatabase(const QString& databasePath,
                                      const QueryOptions& options)
{
    Snapshot snapshot;
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, &snapshot.error)) {
        return snapshot;
    }
    QSqlDatabase& db = connection->database();

    auto scalar = [&db](QString const& sql) {
        QSqlQuery query(db);
        return query.exec(sql) && query.next() ? query.value(0).toInt() : 0;
    };
    snapshot.qsoCount = scalar(QStringLiteral("SELECT COUNT(*) FROM map_qso"));
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const periodCutoff = periodCutoffMs(options.period);
    qint64 const retainedCutoff = nowMs - kLiveRetentionMs;
    qint64 const spotCutoff = qMax(periodCutoff,
                                   qMax(retainedCutoff,
                                        spotAgeCutoff(options.spotAgeFilter, nowMs)));
    // Event history intentionally has a wider retention window than the live
    // spot table.  Do not let the live-table retention silently truncate a
    // user-selected 24 hour or 7 day analytics view.
    qint64 const eventCutoff = qMax(periodCutoff,
                                    spotAgeCutoff(options.spotAgeFilter, nowMs));
    qint64 const coverageCutoff = qMax(
        spotCutoff,
        nowMs - qBound(1, options.liveDecayMinutes, 120) * 60LL * 1000LL);
    qint64 const pskCoverageCutoff = qMax(
        spotCutoff,
        nowMs - qBound(1,
            options.sourceDecayMinutes.value(QStringLiteral("psk"), 60).toInt(),
            240) * 60LL * 1000LL);
    qint64 const oamsCoverageCutoff = qMax(
        spotCutoff,
        nowMs - qBound(1,
            options.sourceDecayMinutes.value(QStringLiteral("oams"), 30).toInt(),
            240) * 60LL * 1000LL);
    QString const qsoGridExpression = options.gridPrecision == 6
        ? QStringLiteral(
              "CASE WHEN length(g.grid6)=6 THEN upper(g.grid6) ELSE upper(g.grid4) END")
        : QStringLiteral("upper(g.grid4)");
    QString const spotGridExpression = options.gridPrecision == 6
        ? QStringLiteral(
              "CASE WHEN length(grid6)=6 THEN upper(grid6) ELSE upper(grid4) END")
        : QStringLiteral("upper(grid4)");
    bool const allBand =
        options.band.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0;
    bool const allMode =
        options.mode.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0;
    bool const allContinent =
        options.continent.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0;
    bool const allDxcc =
        options.dxcc.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0;
    bool const allSource =
        options.source.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0;
    bool const allPropagation =
        options.propagation.compare(QStringLiteral("MIXED"), Qt::CaseInsensitive) == 0;

    auto bindCommon = [&](QSqlQuery& query, bool spot) {
        query.bindValue(QStringLiteral(":all_band"), allBand);
        query.bindValue(QStringLiteral(":band"), options.band);
        query.bindValue(QStringLiteral(":all_mode"), allMode);
        query.bindValue(QStringLiteral(":mode"), options.mode);
        query.bindValue(QStringLiteral(":all_continent"), allContinent);
        query.bindValue(QStringLiteral(":continent"), options.continent);
        query.bindValue(QStringLiteral(":all_dxcc"), allDxcc);
        query.bindValue(QStringLiteral(":dxcc"), options.dxcc);
        query.bindValue(QStringLiteral(":all_source"), allSource);
        query.bindValue(QStringLiteral(":source"), options.source);
        query.bindValue(QStringLiteral(":all_propagation"), allPropagation);
        query.bindValue(QStringLiteral(":propagation"), options.propagation);
        query.bindValue(QStringLiteral(":cutoff"), spot ? spotCutoff : periodCutoff);
        if (spot) {
            query.bindValue(QStringLiteral(":cq_only"), options.cqOnly);
        }
    };

    QString const commonFilter = QStringLiteral(
        " AND (:all_band = 1 OR lower(band) = lower(:band))"
        " AND (:all_mode = 1 OR upper(mode) = upper(:mode))"
        " AND (:all_continent = 1 OR upper(continent) = upper(:continent))"
        " AND (:all_dxcc = 1 OR lower(dxcc) = lower(:dxcc))"
        " AND (:all_source = 1 OR lower(source) = lower(:source))"
        " AND (:all_propagation = 1 OR upper(propagation_mode) = upper(:propagation))");
    QString const qsoFilter =
        commonFilter + QStringLiteral(" AND (:cutoff = 0 OR qso_epoch >= :cutoff)");
    QString awardConfirmedColumn = QStringLiteral("confirmed");
    if (options.awardConfirmation.compare(QStringLiteral("LoTW"), Qt::CaseInsensitive) == 0) {
        awardConfirmedColumn = QStringLiteral("lotw_confirmed");
    } else if (options.awardConfirmation.compare(QStringLiteral("eQSL"), Qt::CaseInsensitive) == 0) {
        awardConfirmedColumn = QStringLiteral("eqsl_confirmed");
    } else if (options.awardConfirmation.compare(QStringLiteral("OQRS"), Qt::CaseInsensitive) == 0) {
        awardConfirmedColumn = QStringLiteral("oqrs");
    }
    qint64 const awardFrom = options.awardFromDate.isEmpty()
        ? 0 : adifEpoch(options.awardFromDate, QStringLiteral("000000"));
    qint64 const awardTo = options.awardToDate.isEmpty()
        ? 0 : adifEpoch(QDate::fromString(options.awardToDate, Qt::ISODate)
                            .addDays(1).toString(Qt::ISODate), QStringLiteral("000000"));
    // Award progress is durable by default, but an explicit callsign/date
    // window is part of the award query itself. This keeps roster, statistics
    // and award calculations on the same imported map_qso source.
    QString const awardQsoFilter = QStringLiteral(
        " AND (:all_band = 1 OR lower(band) = lower(:band))"
        " AND (:all_mode = 1 OR upper(mode) = upper(:mode))"
        " AND (:all_propagation = 1 OR upper(propagation_mode) = upper(:propagation))"
        " AND (:award_call = '' OR upper(operator_call) = upper(:award_call))"
        " AND (:award_from = 0 OR qso_epoch >= :award_from)"
        " AND (:award_to = 0 OR qso_epoch < :award_to)");
    auto bindAward = [awardFrom, awardTo, &options](QSqlQuery& query) {
        query.bindValue(QStringLiteral(":award_call"), options.awardCallsign.isEmpty()
                                                            ? QStringLiteral("")
                                                            : options.awardCallsign.toUpper());
        query.bindValue(QStringLiteral(":award_from"), awardFrom);
        query.bindValue(QStringLiteral(":award_to"), awardTo);
    };
    QString const correlation = options.spotCorrelationFilter.trimmed().toLower();
    QString correlationFilter;
    if (correlation == QStringLiteral("correlated")) {
        correlationFilter = QStringLiteral(" AND correlation_count > 0");
    } else if (correlation == QStringLiteral("local")) {
        correlationFilter = QStringLiteral(" AND lower(source) NOT IN ('psk','oams')");
    } else if (correlation == QStringLiteral("psk reporter")) {
        correlationFilter = QStringLiteral(" AND lower(source)='psk'");
    } else if (correlation == QStringLiteral("oams")) {
        correlationFilter = QStringLiteral(" AND lower(source)='oams'");
    }
    QString const spotFilter =
        commonFilter
        + QStringLiteral(
            " AND ((lower(source)='psk' AND observed_ms >= :psk_cutoff)"
            " OR (lower(source)='oams' AND observed_ms >= :oams_cutoff)"
            " OR (lower(source) NOT IN ('psk','oams')"
            "     AND observed_ms >= :cutoff))"
            " AND (:cq_only = 0 OR is_cq = 1)")
        + (!options.pskLayerEnabled
               ? QStringLiteral(" AND lower(source)<>'psk'")
               : (options.pskDisplayMode.compare(
                      QStringLiteral("Replace"), Qt::CaseInsensitive) == 0
                      ? QStringLiteral(" AND lower(source)='psk'")
                      : QString()))
        + correlationFilter;

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT COUNT(DISTINCT upper(call)) FROM map_spot"
            " WHERE lower(source)='psk' AND observed_ms >= :cutoff"));
        query.bindValue(QStringLiteral(":cutoff"), nowMs - 60LL * 60LL * 1000LL);
        if (query.exec() && query.next()) {
            snapshot.pskListenerCount = query.value(0).toInt();
        }
    }

    QVariantList activeRosterRules;
    {
        QSqlQuery rulesQuery(db);
        if (rulesQuery.exec(QStringLiteral(
                "SELECT upper(rule_type), upper(rule_value), upper(rule_action),"
                " band, mode FROM map_roster_rule WHERE enabled=1"))) {
            while (rulesQuery.next()) {
                QVariantMap rule;
                rule.insert(QStringLiteral("type"), rulesQuery.value(0).toString());
                rule.insert(QStringLiteral("value"), rulesQuery.value(1).toString());
                rule.insert(QStringLiteral("action"), rulesQuery.value(2).toString());
                rule.insert(QStringLiteral("band"), rulesQuery.value(3).toString());
                rule.insert(QStringLiteral("mode"), rulesQuery.value(4).toString());
                activeRosterRules.append(rule);
                if (rulesQuery.value(2).toString() == QStringLiteral("WANTED")) {
                    snapshot.rosterWantedMatrix.append(rule);
                } else if (rulesQuery.value(2).toString() == QStringLiteral("IGNORE")) {
                    snapshot.rosterExceptionMatrix.append(rule);
                }
            }
        }
    }

    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT DISTINCT band FROM map_qso WHERE band <> ''"
                " UNION SELECT DISTINCT band FROM map_spot WHERE band <> ''"))) {
            QStringList values;
            while (query.next()) values.append(query.value(0).toString());
            snapshot.bands = sortedBands(values);
        }
    }
    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT DISTINCT mode FROM map_qso WHERE mode <> ''"
                " UNION SELECT DISTINCT mode FROM map_spot WHERE mode <> ''"))) {
            QStringList values;
            while (query.next()) values.append(query.value(0).toString());
            snapshot.modes = sortedModes(values);
        }
    }
    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT DISTINCT continent FROM map_qso WHERE continent <> ''"
                " UNION SELECT DISTINCT continent FROM map_spot WHERE continent <> ''"))) {
            QStringList values;
            while (query.next()) values.append(query.value(0).toString().toUpper());
            values.removeDuplicates();
            std::sort(values.begin(), values.end());
            values.prepend(QStringLiteral("All"));
            snapshot.continents = values;
        }
    }
    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT DISTINCT dxcc FROM map_qso WHERE dxcc <> ''"
                " UNION SELECT DISTINCT dxcc FROM map_spot WHERE dxcc <> ''"))) {
            QStringList values;
            while (query.next()) values.append(query.value(0).toString());
            values.removeDuplicates();
            std::sort(values.begin(), values.end(), [](QString const& a, QString const& b) {
                return a.localeAwareCompare(b) < 0;
            });
            values.prepend(QStringLiteral("All"));
            snapshot.dxcc = values;
        }
    }
    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT DISTINCT source FROM map_qso WHERE source <> ''"
                " UNION SELECT DISTINCT source FROM map_spot WHERE source <> ''"))) {
            QStringList values;
            while (query.next()) values.append(query.value(0).toString());
            values.removeDuplicates();
            std::sort(values.begin(), values.end());
            values.prepend(QStringLiteral("All"));
            snapshot.sources = values;
        }
    }

    QHash<QString, QVariantMap> coverageByGrid;
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT %1 AS coverage_grid, COUNT(DISTINCT q.id),"
            " COUNT(DISTINCT CASE WHEN q.confirmed<>0 THEN q.id END)"
            " FROM map_qso_grid g JOIN map_qso q ON q.id=g.qso_id"
            " WHERE g.grid4 <> ''").arg(qsoGridExpression) + qsoFilter
            + QStringLiteral(" GROUP BY coverage_grid ORDER BY coverage_grid"));
        bindCommon(query, false);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("grid"), query.value(0).toString());
                row.insert(QStringLiteral("workedCount"), query.value(1).toInt());
                row.insert(QStringLiteral("confirmedCount"), query.value(2).toInt());
                row.insert(QStringLiteral("confirmed"), query.value(2).toInt() > 0);
                row.insert(QStringLiteral("activeCount"), 0);
                row.insert(QStringLiteral("pskCount"), 0);
                row.insert(QStringLiteral("active"), false);
                row.insert(QStringLiteral("missing"), false);
                row.insert(QStringLiteral("psk"), false);
                row.insert(QStringLiteral("historicalStatus"),
                           query.value(2).toInt() > 0
                               ? QStringLiteral("QSL")
                               : QStringLiteral("QSO"));
                row.insert(QStringLiteral("liveStatus"), QString());
                row.insert(QStringLiteral("liveOpacity"), 0.0);
                row.insert(QStringLiteral("split"), false);
                coverageByGrid.insert(query.value(0).toString(), row);
                ++snapshot.workedGridCount;
                if (query.value(2).toInt() > 0) {
                    ++snapshot.confirmedGridCount;
                }
            }
        } else {
            snapshot.error = query.lastError().text();
        }
    }
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT %1 AS coverage_grid, COUNT(*),"
            " SUM(CASE WHEN lower(source)='psk' THEN 1 ELSE 0 END),"
            " MAX(observed_ms),"
            " SUM(CASE WHEN upper(activity_type)='CQ' THEN 1 ELSE 0 END),"
            " SUM(CASE WHEN upper(activity_type)='CQDX' THEN 1 ELSE 0 END),"
            " SUM(CASE WHEN upper(activity_type)='QRZ' THEN 1 ELSE 0 END),"
            " SUM(CASE WHEN upper(activity_type)='WSPR' THEN 1 ELSE 0 END),"
            " SUM(CASE WHEN upper(activity_type)='QSX' THEN 1 ELSE 0 END)"
            " FROM map_spot WHERE grid4 <> ''").arg(spotGridExpression)
            + spotFilter
            + QStringLiteral(" GROUP BY coverage_grid ORDER BY coverage_grid"));
        bindCommon(query, true);
            query.bindValue(QStringLiteral(":cutoff"), coverageCutoff);
            query.bindValue(QStringLiteral(":psk_cutoff"), pskCoverageCutoff);
            query.bindValue(QStringLiteral(":oams_cutoff"), oamsCoverageCutoff);
        if (query.exec()) {
            while (query.next()) {
                QString const grid = query.value(0).toString();
                QVariantMap row = coverageByGrid.value(grid);
                int const activeCount = query.value(1).toInt();
                int const pskCount = query.value(2).toInt();
                qint64 const newestMs = query.value(3).toLongLong();
                QString liveStatus = QStringLiteral("LIVE");
                if (query.value(5).toInt() > 0) {
                    liveStatus = QStringLiteral("CQDX");
                } else if (query.value(4).toInt() > 0) {
                    liveStatus = QStringLiteral("CQ");
                } else if (query.value(6).toInt() > 0) {
                    liveStatus = QStringLiteral("QRZ");
                } else if (query.value(7).toInt() > 0) {
                    liveStatus = QStringLiteral("WSPR");
                } else if (query.value(8).toInt() > 0) {
                    liveStatus = QStringLiteral("QSX");
                } else if (pskCount > 0) {
                    liveStatus = QStringLiteral("PSK");
                }
                bool const missing = row.isEmpty()
                    || row.value(QStringLiteral("workedCount")).toInt() == 0;
                if (row.isEmpty()) {
                    row.insert(QStringLiteral("grid"), grid);
                    row.insert(QStringLiteral("workedCount"), 0);
                    row.insert(QStringLiteral("confirmedCount"), 0);
                    row.insert(QStringLiteral("confirmed"), false);
                    row.insert(QStringLiteral("historicalStatus"), QString());
                }
                row.insert(QStringLiteral("activeCount"), activeCount);
                row.insert(QStringLiteral("pskCount"), pskCount);
                row.insert(QStringLiteral("active"), activeCount > 0);
                row.insert(QStringLiteral("missing"), missing);
                row.insert(QStringLiteral("psk"), pskCount > 0);
                row.insert(QStringLiteral("lastSeenMs"), newestMs);
                row.insert(QStringLiteral("ageSeconds"),
                           qMax<qint64>(0, nowMs - newestMs) / 1000);
                row.insert(QStringLiteral("liveStatus"), liveStatus);
                QString const source = pskCount > 0
                    ? QStringLiteral("psk")
                    : (liveStatus == QStringLiteral("WSPR")
                           ? QStringLiteral("oams")
                           : QStringLiteral("decoder"));
                int const sourceDecay = qBound(1,
                    options.sourceDecayMinutes.value(source,
                                                     options.liveDecayMinutes).toInt(),
                    source == QStringLiteral("decoder") ? 120 : 240);
                qreal opacity = liveOpacityForAge(
                    qMax<qint64>(0, nowMs - newestMs), sourceDecay);
                if (liveStatus == QStringLiteral("PSK")) {
                    opacity *= qBound(0.2, options.pskOpacity, 1.0);
                }
                row.insert(QStringLiteral("liveOpacity"), opacity);
                row.insert(QStringLiteral("split"),
                           options.splitGridEnabled
                               && row.value(QStringLiteral("workedCount")).toInt() > 0);
                coverageByGrid.insert(grid, row);
                ++snapshot.activeGridCount;
                if (missing) ++snapshot.missingGridCount;
            }
        }
    }
    QStringList coverageKeys = coverageByGrid.keys();
    std::sort(coverageKeys.begin(), coverageKeys.end());
    for (QString const& grid : std::as_const(coverageKeys)) {
        snapshot.coverage.append(coverageByGrid.value(grid));
    }

    {
        QSqlQuery query(db);
        QString const orderColumn = rosterOrderColumn(options.rosterSort);
        QString const orderDirection =
            options.rosterSortDescending ? QStringLiteral("DESC") : QStringLiteral("ASC");
        QString const scope = options.rosterHuntScope.trimmed().toLower();
        bool const scopeBand = scope != QStringLiteral("all time");
        bool const scopeMode = scope == QStringLiteral("band + mode");
        QString const rosterScope = options.rosterScope.trimmed().toLower();
        bool const rosterCurrentBand = rosterScope == QStringLiteral("current band")
            && options.band.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0;
        bool const rosterCurrentMode = rosterScope == QStringLiteral("current mode")
            && options.mode.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0;
        bool const rosterDigitalModes = rosterScope == QStringLiteral("digital modes");
        bool const rosterAwardScope = rosterScope == QStringLiteral("award selected");
        QString const rosterDxccScope = options.rosterDxccScope.trimmed().toLower();
        QString const digitalModeSql =
            "('FT2','FT4','FT8','FST4','FST4W','JT4','JT9','JT65','JT6M',"
            "'MSK144','Q65','WSPR','FSK441','ISCAT','OLIVIA','DOMINO','PSK','JS8')";
        qint64 const rosterCutoff =
            QDateTime::currentMSecsSinceEpoch()
            - qBound(1, options.rosterRetentionMinutes, 60) * 60LL * 1000LL;
        QString const historyScope = QStringLiteral(
            " AND (:scope_band = 0 OR lower(q.band) = lower(s.band))"
            " AND (:scope_mode = 0 OR upper(q.mode) = upper(s.mode))"
            " AND (:roster_all_propagation = 1 OR upper(q.propagation_mode) = upper(:roster_propagation))");
        query.prepare(QStringLiteral(
            "SELECT s.call, s.grid, s.band, s.mode, s.snr, s.frequency_hz,"
            " s.observed_utc, s.source, s.message, s.dxcc, s.continent, s.cq_zone,"
            " s.itu_zone, s.state, s.is_cq, s.target_call, s.distance_km,"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE upper(q.call)=upper(s.call)") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE upper(q.call)=upper(s.call) AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   JOIN map_qso_grid qg ON qg.qso_id=q.id"
            "   WHERE s.grid4<>'' AND qg.grid4=s.grid4") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   JOIN map_qso_grid qg ON qg.qso_id=q.id"
            "   WHERE s.grid4<>'' AND qg.grid4=s.grid4 AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.dxcc<>'' AND lower(q.dxcc)=lower(s.dxcc)") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.dxcc<>'' AND lower(q.dxcc)=lower(s.dxcc)"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(
            " LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.cq_zone>0 AND q.cq_zone=s.cq_zone") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.cq_zone>0 AND q.cq_zone=s.cq_zone"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.itu_zone>0 AND q.itu_zone=s.itu_zone") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.itu_zone>0 AND q.itu_zone=s.itu_zone"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.state<>'' AND upper(q.state)=upper(s.state)") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.state<>'' AND upper(q.state)=upper(s.state)"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(
            " LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.continent<>'' AND upper(q.continent)=upper(s.continent)") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.continent<>'' AND upper(q.continent)=upper(s.continent)"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " COALESCE(p.watched, 0),"
            " COALESCE(p.ignored, 0),"
            " COALESCE(s.county, ''), COALESCE(s.pota_ref, ''),"
            " COALESCE(s.iota, ''), COALESCE(s.wpx, ''), COALESCE(s.dt, 0),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.county<>'' AND upper(q.county)=upper(s.county)") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.county<>'' AND upper(q.county)=upper(s.county)"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.pota_ref<>'' AND upper(q.pota_ref)=upper(s.pota_ref)") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.pota_ref<>'' AND upper(q.pota_ref)=upper(s.pota_ref)"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.wpx<>'' AND upper(q.wpx)=upper(s.wpx)") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.wpx<>'' AND upper(q.wpx)=upper(s.wpx)"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1), s.dxcc_number, s.propagation_mode,"
                             " COALESCE(s.grid_origin, 'UNKNOWN') AS grid_origin,"
            " COALESCE((SELECT GROUP_CONCAT(DISTINCT CASE"
            "   WHEN lower(e.source)='decoder' THEN 'Local decode'"
            "   WHEN COALESCE(trim(e.provider),'')<>'' THEN trim(e.provider)"
            "   WHEN lower(e.source)='psk' THEN 'PSK Reporter'"
            "   WHEN lower(e.source)='oams' THEN 'OAMS'"
            "   ELSE upper(e.source) END) FROM map_spot_event e"
            "   WHERE upper(e.call)=upper(s.call)"
            "     AND (s.grid4='' OR e.grid='' OR upper(substr(e.grid,1,4))=upper(s.grid4))"
            "     AND (s.band='' OR lower(e.band)=lower(s.band))"
            "     AND (s.mode='' OR upper(e.mode)=upper(s.mode))"
            "     AND abs(e.observed_ms-s.observed_ms)<=300000), s.source) AS source_summary,"
            " MAX(1, COALESCE((SELECT COUNT(DISTINCT lower(e.source) || '|' ||"
            "   lower(COALESCE(e.provider,''))) FROM map_spot_event e"
            "   WHERE upper(e.call)=upper(s.call)"
            "     AND (s.grid4='' OR e.grid='' OR upper(substr(e.grid,1,4))=upper(s.grid4))"
            "     AND (s.band='' OR lower(e.band)=lower(s.band))"
            "     AND (s.mode='' OR upper(e.mode)=upper(s.mode))"
            "     AND abs(e.observed_ms-s.observed_ms)<=300000), 0)) AS source_count")
            + QStringLiteral(
            " FROM map_spot s"
            " LEFT JOIN map_roster_preference p ON upper(p.call)=upper(s.call)"
            " WHERE COALESCE(p.ignored, 0)=0"
            " AND NOT EXISTS("
            "   SELECT 1 FROM map_roster_ignore i"
            "   WHERE upper(i.ignore_type)='DXCC'"
            "     AND upper(i.ignore_value)=upper(s.dxcc))"
            " AND (:roster_all_band = 1 OR lower(s.band)=lower(:roster_band))"
            " AND (:roster_all_mode = 1 OR upper(s.mode)=upper(:roster_mode))"
            " AND (:roster_all_propagation = 1 OR upper(s.propagation_mode)=upper(:roster_propagation))"
            " AND (:roster_digital = 0 OR upper(s.mode) IN ") + digitalModeSql
            + QStringLiteral(")"
            " AND (:roster_min_snr_enabled = 0 OR s.snr >= :roster_min_snr)"
            " AND (:roster_max_dt_enabled = 0 OR ABS(COALESCE(s.dt, 0)) <= :roster_max_dt)"
            " AND (:roster_dxcc_scope = 'all'"
            "      OR (:roster_dxcc_scope = 'same dxcc'"
            "          AND :roster_my_dxcc <> '' AND upper(s.dxcc)=upper(:roster_my_dxcc))"
            "      OR (:roster_dxcc_scope = 'other dxcc'"
            "          AND (:roster_my_dxcc = '' OR upper(s.dxcc)<>upper(:roster_my_dxcc)))"
            " )"
            " AND ((:roster_spotted = 1 AND lower(s.source)='psk'"
            "       AND upper(s.target_call)=upper(:roster_my_call))"
            "      OR (:roster_spotted = 0 AND lower(s.source)<>'psk'))"
            " AND (:roster_cq_only = 0 OR s.is_cq = 1"
            "      OR (:roster_rr73 = 1 AND upper(s.message) LIKE '%RR73%'))"
            " AND s.id IN ("
            "   SELECT MAX(recent.id) FROM map_spot recent"
            "   WHERE recent.observed_ms >= :roster_cutoff"
            "     AND recent.call <> ''"
            "     AND (:roster_all_band = 1 OR lower(recent.band)=lower(:roster_band))"
            "     AND (:roster_all_mode = 1 OR upper(recent.mode)=upper(:roster_mode))"
            "     AND (:roster_all_propagation = 1 OR upper(recent.propagation_mode)=upper(:roster_propagation))"
            "     AND (:roster_digital = 0 OR upper(recent.mode) IN ") + digitalModeSql
            + QStringLiteral(")"
            "     AND (:roster_min_snr_enabled = 0 OR recent.snr >= :roster_min_snr)"
            "     AND (:roster_max_dt_enabled = 0 OR ABS(COALESCE(recent.dt, 0)) <= :roster_max_dt)"
            "     AND (:roster_dxcc_scope = 'all'"
            "          OR (:roster_dxcc_scope = 'same dxcc'"
            "              AND :roster_my_dxcc <> ''"
            "              AND upper(recent.dxcc)=upper(:roster_my_dxcc))"
            "          OR (:roster_dxcc_scope = 'other dxcc'"
            "              AND (:roster_my_dxcc = ''"
            "                   OR upper(recent.dxcc)<>upper(:roster_my_dxcc))))"
            "     AND ((:roster_spotted = 1 AND lower(recent.source)='psk'"
            "           AND upper(recent.target_call)=upper(:roster_my_call))"
            "          OR (:roster_spotted = 0 AND lower(recent.source)<>'psk'))"
            "     AND (:roster_cq_only = 0 OR recent.is_cq = 1"
            "          OR (:roster_rr73 = 1 AND upper(recent.message) LIKE '%RR73%'))"
            "   GROUP BY upper(recent.call))"
            " ORDER BY COALESCE(p.watched, 0) DESC,"
            " %1 %2, s.observed_ms DESC LIMIT :limit")
                  .arg(orderColumn, orderDirection));
        query.bindValue(QStringLiteral(":scope_band"), scopeBand);
        query.bindValue(QStringLiteral(":scope_mode"), scopeMode);
        query.bindValue(QStringLiteral(":roster_all_band"), !rosterCurrentBand);
        query.bindValue(QStringLiteral(":roster_band"), options.band);
        query.bindValue(QStringLiteral(":roster_all_mode"), !rosterCurrentMode);
        query.bindValue(QStringLiteral(":roster_mode"), options.mode);
        query.bindValue(QStringLiteral(":roster_all_propagation"), allPropagation);
        query.bindValue(QStringLiteral(":roster_propagation"), options.propagation);
        query.bindValue(QStringLiteral(":roster_digital"), rosterDigitalModes);
        query.bindValue(QStringLiteral(":roster_min_snr_enabled"), options.rosterMinSnrEnabled);
        query.bindValue(QStringLiteral(":roster_min_snr"), options.rosterMinSnr);
        query.bindValue(QStringLiteral(":roster_max_dt_enabled"), options.rosterMaxDtEnabled);
        query.bindValue(QStringLiteral(":roster_max_dt"), options.rosterMaxDt);
        query.bindValue(QStringLiteral(":roster_dxcc_scope"), rosterDxccScope);
        query.bindValue(QStringLiteral(":roster_my_dxcc"), options.rosterMyDxcc.toUpper());
        query.bindValue(QStringLiteral(":roster_spotted"), options.rosterSpottedMeOnly);
        query.bindValue(QStringLiteral(":roster_my_call"), options.rosterMyCall.toUpper());
        query.bindValue(QStringLiteral(":roster_cutoff"), rosterCutoff);
        query.bindValue(QStringLiteral(":roster_cq_only"), options.rosterCqOnly);
        query.bindValue(QStringLiteral(":roster_rr73"), options.rosterTreatRr73AsCq);
        query.bindValue(QStringLiteral(":limit"), kRosterCandidateLimit);
        if (query.exec()) {
            int const rosterDxccNumberIndex = query.record().indexOf(QStringLiteral("dxcc_number"));
            int const rosterGridOriginIndex = query.record().indexOf(QStringLiteral("grid_origin"));
            int const rosterSourceSummaryIndex =
                query.record().indexOf(QStringLiteral("source_summary"));
            int const rosterSourceCountIndex =
                query.record().indexOf(QStringLiteral("source_count"));
            QSqlQuery profileQuery(db);
            profileQuery.prepare(QStringLiteral(
                "SELECT COALESCE(MAX(county), ''), COALESCE(MAX(pota_ref), ''),"
                " COALESCE(MAX(iota), ''), COALESCE(MAX(wpx), ''),"
                " COALESCE(MAX(qso_epoch), 0),"
                " COALESCE(MAX(CASE WHEN lotw_confirmed=1 THEN qso_epoch ELSE 0 END), 0),"
                " COALESCE(MAX(CASE WHEN eqsl_confirmed=1 THEN qso_epoch ELSE 0 END), 0),"
                " COALESCE(MAX(oqrs), 0)"
                " FROM map_qso WHERE upper(call)=upper(:call)"));
            while (query.next()) {
                bool const callWorked = query.value(17).toBool();
                bool const callConfirmed = query.value(18).toBool();
                bool const gridWorked = query.value(19).toBool();
                bool const gridConfirmed = query.value(20).toBool();
                bool const dxccWorked = query.value(21).toBool();
                bool const dxccConfirmed = query.value(22).toBool();
                bool const cqWorked = query.value(23).toBool();
                bool const cqConfirmed = query.value(24).toBool();
                bool const ituWorked = query.value(25).toBool();
                bool const ituConfirmed = query.value(26).toBool();
                bool const stateWorked = query.value(27).toBool();
                bool const stateConfirmed = query.value(28).toBool();
                bool const continentWorked = query.value(29).toBool();
                bool const continentConfirmed = query.value(30).toBool();
                bool watched = query.value(31).toBool();
                bool const ignored = query.value(32).toBool();
                bool const hasGrid = !query.value(1).toString().trimmed().isEmpty();
                bool const hasDxcc = !query.value(9).toString().trimmed().isEmpty();
                bool const countyWorked = query.value(38).toBool();
                bool const countyConfirmed = query.value(39).toBool();
                bool const potaWorked = query.value(40).toBool();
                bool const potaConfirmed = query.value(41).toBool();
                bool const wpxWorked = query.value(42).toBool();
                bool const wpxConfirmed = query.value(43).toBool();
                bool const hasCounty = !query.value(33).toString().trimmed().isEmpty();
                bool const hasPota = !query.value(34).toString().trimmed().isEmpty();
                bool const hasWpx = !query.value(36).toString().trimmed().isEmpty();
                QString county = query.value(33).toString().trimmed().toUpper();
                QString pota = query.value(34).toString().trimmed().toUpper();
                QString const iota = query.value(35).toString().trimmed().toUpper();
                QString wpx = query.value(36).toString().trimmed().toUpper();
                double const dt = query.value(37).toDouble();
                qint64 lastQsoEpoch = 0;
                qint64 lotwEpoch = 0;
                qint64 eqslEpoch = 0;
                bool oqrs = false;
                profileQuery.bindValue(QStringLiteral(":call"), query.value(0).toString());
                if (profileQuery.exec() && profileQuery.next()) {
                    if (county.isEmpty()) county = profileQuery.value(0).toString().trimmed().toUpper();
                    if (pota.isEmpty()) pota = profileQuery.value(1).toString().trimmed().toUpper();
                    if (wpx.isEmpty()) wpx = profileQuery.value(3).toString().trimmed().toUpper();
                    lastQsoEpoch = profileQuery.value(4).toLongLong();
                    lotwEpoch = profileQuery.value(5).toLongLong();
                    eqslEpoch = profileQuery.value(6).toLongLong();
                    oqrs = profileQuery.value(7).toBool();
                }
                auto ageDays = [nowMs](qint64 epoch) {
                    return epoch > 0 ? static_cast<int>(qMax<qint64>(0, nowMs - epoch)
                                                         / (24LL * 60LL * 60LL * 1000LL))
                                     : -1;
                };
                int const lotwAgeDays = ageDays(lotwEpoch);
                int const eqslAgeDays = ageDays(eqslEpoch);
                int const lastQsoAgeDays = ageDays(lastQsoEpoch);
                QString sourceSummary = rosterSourceSummaryIndex >= 0
                    ? query.value(rosterSourceSummaryIndex).toString()
                    : query.value(7).toString();
                sourceSummary.replace(QLatin1Char(','), QStringLiteral(" · "));
                int const sourceCount = rosterSourceCountIndex >= 0
                    ? qMax(1, query.value(rosterSourceCountIndex).toInt()) : 1;
                int const providerFilters = (options.rosterUsesLoTW ? 1 : 0)
                    + (options.rosterUsesEQSL ? 1 : 0)
                    + (options.rosterUsesOQRS ? 1 : 0);
                if (providerFilters > 0) {
                    bool providerMatch = false;
                    if (options.rosterUsesLoTW
                        && lotwAgeDays >= 0
                        && lotwAgeDays <= options.rosterMaxLoTWDays) {
                        providerMatch = true;
                    }
                    if (options.rosterUsesEQSL && eqslAgeDays >= 0) {
                        providerMatch = true;
                    }
                    if (options.rosterUsesOQRS && oqrs) {
                        providerMatch = true;
                    }
                    if (!providerMatch) continue;
                }
                QString const rosterNeedle = options.rosterText.trimmed();
                QString const rosterMode = options.rosterTextMode.trimmed().toLower();
                if (!rosterNeedle.isEmpty()
                    && rosterMode != QStringLiteral("no filter")) {
                    QString const searchable =
                        QStringLiteral("%1 %2 %3 %4 %5 %6 %7 %8")
                            .arg(query.value(0).toString(),
                                 query.value(1).toString(),
                                 query.value(8).toString(),
                                 query.value(9).toString(),
                                 query.value(10).toString(),
                                 query.value(13).toString(),
                                 query.value(15).toString(),
                                 sourceSummary);
                    bool matches = false;
                    if (rosterMode == QStringLiteral("regex")) {
                        QRegularExpression const expression(
                            rosterNeedle,
                            QRegularExpression::CaseInsensitiveOption);
                        matches = expression.isValid()
                            && expression.match(searchable).hasMatch();
                    } else {
                        matches = searchable.contains(rosterNeedle,
                                                      Qt::CaseInsensitive);
                    }
                    if ((rosterMode == QStringLiteral("exclude") && matches)
                        || (rosterMode != QStringLiteral("exclude") && !matches)) {
                        continue;
                    }
                }
                struct HuntEntity {
                    QString type;
                    QString label;
                    QString value;
                    bool eligible {false};
                    bool worked {false};
                    bool confirmed {false};
                };
                QVector<HuntEntity> huntEntities {
                    {QStringLiteral("CALL"), QStringLiteral("call"),
                     query.value(0).toString(), !query.value(0).toString().isEmpty(),
                     callWorked, callConfirmed},
                    {QStringLiteral("GRID"), QStringLiteral("grid"),
                     query.value(1).toString().left(4), hasGrid,
                     gridWorked, gridConfirmed},
                    {QStringLiteral("DXCC"), QStringLiteral("DXCC"),
                     query.value(9).toString(), hasDxcc, dxccWorked, dxccConfirmed},
                    {QStringLiteral("WPX"), QStringLiteral("WPX"),
                     wpx, hasWpx, wpxWorked, wpxConfirmed},
                    {QStringLiteral("POTA"), QStringLiteral("POTA"),
                     pota, hasPota, potaWorked, potaConfirmed},
                    {QStringLiteral("CQ"), QStringLiteral("CQ zone"),
                     QString::number(query.value(11).toInt()),
                     query.value(11).toInt() > 0, cqWorked, cqConfirmed},
                    {QStringLiteral("ITU"), QStringLiteral("ITU zone"),
                     QString::number(query.value(12).toInt()),
                     query.value(12).toInt() > 0, ituWorked, ituConfirmed},
                    {QStringLiteral("STATE"), QStringLiteral("state"),
                     query.value(13).toString().trimmed().toUpper(),
                     !query.value(13).toString().trimmed().isEmpty(),
                     stateWorked, stateConfirmed},
                    {QStringLiteral("COUNTY"), QStringLiteral("county"),
                     county, hasCounty, countyWorked, countyConfirmed},
                    {QStringLiteral("CONTINENT"), QStringLiteral("continent"),
                     query.value(10).toString().trimmed().toUpper(),
                     !query.value(10).toString().trimmed().isEmpty(),
                     continentWorked, continentConfirmed}
                };
                auto wantedTypeEnabled = [&options](const QString& type) {
                    return options.rosterWantedTypes.contains(type, Qt::CaseInsensitive);
                };
                bool anyNew = false;
                bool hasUnconfirmedEntity = false;
                QStringList newReasons;
                QStringList unconfirmedReasons;
                for (HuntEntity const& entity : std::as_const(huntEntities)) {
                    if (!entity.eligible || !wantedTypeEnabled(entity.type)) continue;
                    if (!entity.worked) {
                        anyNew = true;
                        newReasons.append(QStringLiteral("New %1: %2")
                                               .arg(entity.label, entity.value));
                    } else if (!entity.confirmed) {
                        hasUnconfirmedEntity = true;
                        unconfirmedReasons.append(QStringLiteral("Unconfirmed %1: %2")
                                                       .arg(entity.label, entity.value));
                    }
                }
                bool const anyUnconfirmed = !anyNew && hasUnconfirmedEntity;
                QString const activeAward =
                    options.activeAwardProgram.trimmed().toLower();
                bool const confirmedGoal =
                    options.awardGoal.compare(QStringLiteral("Confirmed"),
                                              Qt::CaseInsensitive) == 0;
                bool awardEligible = false;
                bool awardWorked = false;
                bool awardConfirmed = false;
                QString awardEntity;
                if (activeAward == QStringLiteral("dxcc")) {
                    awardEligible = hasDxcc;
                    awardWorked = dxccWorked;
                    awardConfirmed = dxccConfirmed;
                    awardEntity = query.value(9).toString();
                } else if (activeAward == QStringLiteral("maidenhead")) {
                    awardEligible = hasGrid;
                    awardWorked = gridWorked;
                    awardConfirmed = gridConfirmed;
                    awardEntity = query.value(1).toString().left(4);
                } else if (activeAward == QStringLiteral("waz")) {
                    int const zone = query.value(11).toInt();
                    awardEligible = zone >= 1 && zone <= 40;
                    awardWorked = cqWorked;
                    awardConfirmed = cqConfirmed;
                    awardEntity = QString::number(zone);
                } else if (activeAward == QStringLiteral("was")) {
                    awardEntity = query.value(13).toString().trimmed().toUpper();
                    awardEligible = isWasState(awardEntity);
                    awardWorked = stateWorked;
                    awardConfirmed = stateConfirmed;
                } else if (activeAward == QStringLiteral("us48")) {
                    awardEntity = query.value(13).toString().trimmed().toUpper();
                    awardEligible = isLower48State(awardEntity);
                    awardWorked = stateWorked;
                    awardConfirmed = stateConfirmed;
                } else if (activeAward == QStringLiteral("wac")) {
                    awardEntity = query.value(10).toString().trimmed().toUpper();
                    awardEligible = QStringList {
                        QStringLiteral("AF"), QStringLiteral("AS"),
                        QStringLiteral("EU"), QStringLiteral("NA"),
                        QStringLiteral("OC"), QStringLiteral("SA")
                    }.contains(awardEntity);
                    awardWorked = continentWorked;
                    awardConfirmed = continentConfirmed;
                } else if (activeAward == QStringLiteral("itu zones")) {
                    int const zone = query.value(12).toInt();
                    awardEligible = zone >= 1 && zone <= 90;
                    awardWorked = ituWorked;
                    awardConfirmed = ituConfirmed;
                    awardEntity = QString::number(zone);
                } else if (ExternalAwardDefinition const* external =
                               externalAwardForLabel(options.activeAwardProgram)) {
                    awardEntity = externalAwardSpotEntity(
                        *external,
                        query.value(0).toString(), query.value(2).toString(),
                        query.value(1).toString(),
                        query.value(9).toString(), query.value(11).toInt(),
                        query.value(13).toString(), query.value(10).toString(),
                        county, iota);
                    awardEligible = !awardEntity.isEmpty()
                        && externalAwardMatchesFields(
                            *external, query.value(0).toString(), query.value(2).toString(),
                            query.value(3).toString(), query.value(1).toString(),
                            rosterDxccNumberIndex >= 0
                                ? query.value(rosterDxccNumberIndex).toInt() : 0,
                            query.value(11).toInt(), query.value(10).toString(),
                            query.value(35).toString(), options.awardEndorsement);
                    if (external->type == QStringLiteral("grids")) {
                        awardWorked = gridWorked;
                        awardConfirmed = gridConfirmed;
                    } else if (external->type == QStringLiteral("dxcc")
                               || external->type == QStringLiteral("dxcc2band")
                               || external->type == QStringLiteral("calls2dxcc")) {
                        awardWorked = dxccWorked;
                        awardConfirmed = dxccConfirmed;
                    } else if (external->type == QStringLiteral("cqz")) {
                        awardWorked = cqWorked;
                        awardConfirmed = cqConfirmed;
                    } else if (external->type == QStringLiteral("states")
                               || external->type == QStringLiteral("states2band")) {
                        awardWorked = stateWorked;
                        awardConfirmed = stateConfirmed;
                    } else if (external->type == QStringLiteral("cont")
                               || external->type == QStringLiteral("cont2band")
                               || external->type == QStringLiteral("cont5")
                               || external->type == QStringLiteral("cont52band")) {
                        awardWorked = continentWorked;
                        awardConfirmed = continentConfirmed;
                    } else {
                        awardWorked = callWorked;
                        awardConfirmed = callConfirmed;
                    }
                }
                bool const awardMode = activeAward != QStringLiteral("none")
                    && !activeAward.isEmpty();
                bool const awardWanted = awardMode && awardEligible
                    && (confirmedGoal ? !awardConfirmed : !awardWorked);
                bool forceWanted = false;
                bool ignoredByRule = false;
                QStringList ruleReasons;
                for (QVariant const& variant : activeRosterRules) {
                    QVariantMap const rule = variant.toMap();
                    QString const ruleBand = rule.value(QStringLiteral("band")).toString();
                    QString const ruleMode = rule.value(QStringLiteral("mode")).toString();
                    if (!ruleBand.isEmpty()
                        && ruleBand.compare(query.value(2).toString(), Qt::CaseInsensitive) != 0) {
                        continue;
                    }
                    if (!ruleMode.isEmpty()
                        && ruleMode.compare(query.value(3).toString(), Qt::CaseInsensitive) != 0) {
                        continue;
                    }
                    QString const type = rule.value(QStringLiteral("type")).toString();
                    QString candidate;
                    if (type == QStringLiteral("CALL")) candidate = query.value(0).toString();
                    else if (type == QStringLiteral("GRID")) candidate = query.value(1).toString().left(4);
                    else if (type == QStringLiteral("DXCC")) candidate = query.value(9).toString();
                    else if (type == QStringLiteral("WPX")) candidate = wpx;
                    else if (type == QStringLiteral("CQ")) candidate = QString::number(query.value(11).toInt());
                    else if (type == QStringLiteral("ITU")) candidate = QString::number(query.value(12).toInt());
                    else if (type == QStringLiteral("STATE")) candidate = query.value(13).toString();
                    else if (type == QStringLiteral("CONTINENT")) candidate = query.value(10).toString();
                    else if (type == QStringLiteral("COUNTY")) candidate = county;
                    else if (type == QStringLiteral("POTA")) candidate = pota;
                    else if (type == QStringLiteral("IOTA")) candidate = iota;
                    else if (type == QStringLiteral("OQRS")) candidate = oqrs ? QStringLiteral("YES") : QStringLiteral("NO");
                    else if (type == QStringLiteral("BAND")) candidate = query.value(2).toString();
                    else if (type == QStringLiteral("MODE")) candidate = query.value(3).toString();
                    if (candidate.isEmpty()
                        || candidate.compare(rule.value(QStringLiteral("value")).toString(),
                                             Qt::CaseInsensitive) != 0) {
                        continue;
                    }
                    QString const action = rule.value(QStringLiteral("action")).toString();
                    if (action == QStringLiteral("IGNORE")) {
                        ignoredByRule = true;
                        break;
                    }
                    if (action == QStringLiteral("WATCH")) {
                        watched = true;
                        ruleReasons.append(QStringLiteral("Watch rule"));
                    } else if (action == QStringLiteral("WANTED")) {
                        forceWanted = true;
                        ruleReasons.append(QStringLiteral("Wanted rule"));
                    }
                }
                if (ignoredByRule) {
                    continue;
                }
                if (rosterAwardScope && !awardWanted && !forceWanted) {
                    continue;
                }
                bool const wanted = forceWanted || (awardMode
                    ? awardWanted
                    : (anyNew || hasUnconfirmedEntity));

                if (anyNew) ++snapshot.rosterNewCount;
                if (anyUnconfirmed) ++snapshot.rosterUnconfirmedCount;
                if (wanted) ++snapshot.rosterWantedCount;

                QString const statusFilter = options.rosterStatus.trimmed().toLower();
                if ((statusFilter == QStringLiteral("new") && !anyNew)
                    || (statusFilter == QStringLiteral("unconfirmed") && !anyUnconfirmed)
                    || (statusFilter == QStringLiteral("wanted") && !wanted)
                    || (statusFilter == QStringLiteral("award") && !awardWanted)
                    || (statusFilter == QStringLiteral("watched") && !watched)) {
                    continue;
                }

                QStringList reasons = newReasons;
                reasons.append(unconfirmedReasons);
                if (awardWanted) {
                    reasons.append(QStringLiteral("%1 %2 %3")
                                        .arg(options.activeAwardProgram,
                                             awardEntity,
                                             confirmedGoal
                                                 ? QStringLiteral("unconfirmed")
                                                 : QStringLiteral("new")));
                }
                reasons.append(ruleReasons);

                QVariantMap row;
                row.insert(QStringLiteral("call"), query.value(0).toString());
                row.insert(QStringLiteral("grid"), query.value(1).toString());
                QString const gridOrigin = rosterGridOriginIndex >= 0
                    ? query.value(rosterGridOriginIndex).toString()
                    : QStringLiteral("UNKNOWN");
                row.insert(QStringLiteral("gridOrigin"), gridOriginLabel(gridOrigin));
                row.insert(QStringLiteral("gridReliability"),
                           gridReliabilityLabel(gridOrigin));
                row.insert(QStringLiteral("gridMarker"),
                           gridReliabilityMarker(gridOrigin));
                row.insert(QStringLiteral("band"), query.value(2).toString());
                row.insert(QStringLiteral("mode"), query.value(3).toString());
                row.insert(QStringLiteral("propagation"), query.value(45).toString());
                row.insert(QStringLiteral("snr"), query.value(4).toInt());
                row.insert(QStringLiteral("dt"), dt);
                row.insert(QStringLiteral("frequencyHz"), query.value(5).toLongLong());
                row.insert(QStringLiteral("observedUtc"), query.value(6).toString());
                row.insert(QStringLiteral("source"), query.value(7).toString());
                row.insert(QStringLiteral("sourceSummary"), sourceSummary);
                row.insert(QStringLiteral("sourceCount"), sourceCount);
                row.insert(QStringLiteral("corroborationLevel"),
                           sourceCount >= 3 ? QStringLiteral("Strongly corroborated")
                                            : (sourceCount == 2
                                                   ? QStringLiteral("Corroborated")
                                                   : QStringLiteral("Single source")));
                row.insert(QStringLiteral("message"), query.value(8).toString());
                row.insert(QStringLiteral("dxcc"), query.value(9).toString());
                row.insert(QStringLiteral("continent"), query.value(10).toString());
                row.insert(QStringLiteral("cqZone"), query.value(11).toInt());
                row.insert(QStringLiteral("ituZone"), query.value(12).toInt());
                row.insert(QStringLiteral("state"), query.value(13).toString());
                row.insert(QStringLiteral("county"), county);
                row.insert(QStringLiteral("pota"), pota);
                row.insert(QStringLiteral("iota"), iota);
                row.insert(QStringLiteral("wpx"), wpx);
                row.insert(QStringLiteral("lotwAgeDays"), lotwAgeDays);
                row.insert(QStringLiteral("eqslAgeDays"), eqslAgeDays);
                row.insert(QStringLiteral("oqrs"), oqrs);
                row.insert(QStringLiteral("ageDays"), lastQsoAgeDays);
                row.insert(QStringLiteral("isCQ"), query.value(14).toBool()
                           || (options.rosterTreatRr73AsCq
                               && query.value(8).toString().contains(QStringLiteral("RR73"),
                                                                       Qt::CaseInsensitive)));
                row.insert(QStringLiteral("targetCall"), query.value(15).toString());
                row.insert(QStringLiteral("distanceKm"), query.value(16).toDouble());
                row.insert(QStringLiteral("callWorked"), callWorked);
                row.insert(QStringLiteral("callConfirmed"), callConfirmed);
                row.insert(QStringLiteral("gridWorked"), gridWorked);
                row.insert(QStringLiteral("gridConfirmed"), gridConfirmed);
                row.insert(QStringLiteral("dxccWorked"), dxccWorked);
                row.insert(QStringLiteral("dxccConfirmed"), dxccConfirmed);
                row.insert(QStringLiteral("countyWorked"), countyWorked);
                row.insert(QStringLiteral("countyConfirmed"), countyConfirmed);
                row.insert(QStringLiteral("potaWorked"), potaWorked);
                row.insert(QStringLiteral("potaConfirmed"), potaConfirmed);
                row.insert(QStringLiteral("wpxWorked"), wpxWorked);
                row.insert(QStringLiteral("wpxConfirmed"), wpxConfirmed);
                row.insert(QStringLiteral("new"), anyNew);
                row.insert(QStringLiteral("unconfirmed"), anyUnconfirmed);
                row.insert(QStringLiteral("wanted"), wanted);
                row.insert(QStringLiteral("awardWanted"), awardWanted);
                row.insert(QStringLiteral("awardProgram"),
                           options.activeAwardProgram);
                row.insert(QStringLiteral("awardEntity"), awardEntity);
                row.insert(QStringLiteral("awardWorked"), awardWorked);
                row.insert(QStringLiteral("awardConfirmed"), awardConfirmed);
                row.insert(QStringLiteral("watched"), watched);
                row.insert(QStringLiteral("ignored"), ignored);
                row.insert(QStringLiteral("status"),
                           anyNew ? QStringLiteral("NEW")
                                  : (anyUnconfirmed ? QStringLiteral("UNCONFIRMED")
                                                    : QStringLiteral("CONFIRMED")));
                row.insert(QStringLiteral("newReasons"), newReasons);
                row.insert(QStringLiteral("unconfirmedReasons"), unconfirmedReasons);
                row.insert(QStringLiteral("huntReason"), reasons.join(QStringLiteral(" · ")));
                snapshot.roster.append(row);
                if (snapshot.roster.size() >= kRosterLimit) break;
            }
        } else if (snapshot.error.isEmpty()) {
            qWarning().noquote() << "[MAPINT] roster query failed:" << query.lastError().text();
            snapshot.error = query.lastError().text();
        }
    }
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT COUNT(*) FROM map_spot WHERE 1=1") + spotFilter);
        bindCommon(query, true);
        query.bindValue(QStringLiteral(":psk_cutoff"), pskCoverageCutoff);
        query.bindValue(QStringLiteral(":oams_cutoff"), oamsCoverageCutoff);
        if (query.exec() && query.next()) {
            snapshot.liveSpotCount = query.value(0).toInt();
        }
    }

    // Keep the event log separate from current spot state. It permits a
    // temporal heatmap and directional paths without growing the live roster.
    QString eventFilter = QStringLiteral(
        " WHERE observed_ms >= :event_cutoff"
        " AND (upper(COALESCE(direction, 'RX'))='RX'"
        "      OR lower(source)='psk'"
        "      OR lower(COALESCE(provider, '')) LIKE '%psk reporter%')"
        " AND (:event_all_band = 1 OR lower(band)=lower(:event_band))"
        " AND (:event_all_mode = 1 OR upper(mode)=upper(:event_mode))"
        " AND (:event_all_source = 1 OR lower(source)=lower(:event_source))"
        " AND (:event_all_propagation = 1 OR upper(propagation_mode)=upper(:event_propagation))"
        " AND (:event_cq_only = 0 OR upper(activity_type) IN ('CQ','CQDX','QRZ'))");
    QString const eventCorrelation = options.spotCorrelationFilter.trimmed().toLower();
    if (eventCorrelation == QStringLiteral("correlated")) {
        eventFilter += QStringLiteral(" AND correlation > 0");
    } else if (eventCorrelation == QStringLiteral("local")) {
        eventFilter += QStringLiteral(" AND lower(source) NOT IN ('psk','oams')");
    } else if (eventCorrelation == QStringLiteral("psk reporter")) {
        eventFilter += QStringLiteral(" AND lower(source)='psk'");
    } else if (eventCorrelation == QStringLiteral("oams")) {
        eventFilter += QStringLiteral(" AND lower(source)='oams'");
    }
    auto bindEvent = [&](QSqlQuery& query) {
        query.bindValue(QStringLiteral(":event_cutoff"), eventCutoff);
        query.bindValue(QStringLiteral(":event_all_band"), allBand);
        query.bindValue(QStringLiteral(":event_band"), options.band);
        query.bindValue(QStringLiteral(":event_all_mode"), allMode);
        query.bindValue(QStringLiteral(":event_mode"), options.mode);
        query.bindValue(QStringLiteral(":event_all_source"), allSource);
        query.bindValue(QStringLiteral(":event_source"), options.source);
        query.bindValue(QStringLiteral(":event_all_propagation"), allPropagation);
        query.bindValue(QStringLiteral(":event_propagation"), options.propagation);
        query.bindValue(QStringLiteral(":event_cq_only"), options.cqOnly);
    };
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT upper(substr(grid,1,4)), COUNT(*), MAX(observed_ms),"
            " ROUND(AVG(snr), 1), COUNT(DISTINCT NULLIF(receiver_call, '')) ,"
            " COALESCE(SUM(CASE WHEN correlation>0 THEN 1 ELSE 0 END), 0)"
            " FROM map_spot_event")
                      + eventFilter
                      + QStringLiteral(
                          " AND grid<>'' GROUP BY upper(substr(grid,1,4))"
                          " ORDER BY COUNT(*) DESC, MAX(observed_ms) DESC LIMIT 2000"));
        bindEvent(query);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("grid"), query.value(0).toString());
                row.insert(QStringLiteral("count"), query.value(1).toInt());
                row.insert(QStringLiteral("lastObservedMs"), query.value(2).toLongLong());
                row.insert(QStringLiteral("averageSnr"), query.value(3).toDouble());
                row.insert(QStringLiteral("receivers"), query.value(4).toInt());
                row.insert(QStringLiteral("correlated"), query.value(5).toInt());
                snapshot.spotHeatmap.append(row);
            }
        }
    }
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT (observed_ms / 300000) * 300000, lower(source), COUNT(*),"
            " COUNT(DISTINCT upper(call)), COALESCE(SUM(CASE WHEN correlation>0 THEN 1 ELSE 0 END), 0)"
            " FROM map_spot_event")
                      + eventFilter
                      + QStringLiteral(
                          " GROUP BY 1, 2 ORDER BY 1 DESC, 2 LIMIT 576"));
        bindEvent(query);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("bucketMs"), query.value(0).toLongLong());
                row.insert(QStringLiteral("source"), query.value(1).toString());
                row.insert(QStringLiteral("count"), query.value(2).toInt());
                row.insert(QStringLiteral("calls"), query.value(3).toInt());
                row.insert(QStringLiteral("correlated"), query.value(4).toInt());
                snapshot.spotTimeline.append(row);
            }
        }
    }
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT receiver_grid, grid, receiver_call, call, COUNT(*),"
            " MAX(observed_ms), ROUND(AVG(correlation), 2), lower(source)"
            " FROM map_spot_event")
                      + eventFilter
                      + QStringLiteral(
                          " AND receiver_grid<>'' AND grid<>''"
                          " GROUP BY receiver_grid, grid, receiver_call, call, lower(source)"
                          " ORDER BY MAX(observed_ms) DESC LIMIT 250"));
        bindEvent(query);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("fromGrid"), query.value(0).toString());
                row.insert(QStringLiteral("toGrid"), query.value(1).toString());
                row.insert(QStringLiteral("fromCall"), query.value(2).toString());
                row.insert(QStringLiteral("toCall"), query.value(3).toString());
                row.insert(QStringLiteral("count"), query.value(4).toInt());
                row.insert(QStringLiteral("lastObservedMs"), query.value(5).toLongLong());
                row.insert(QStringLiteral("correlation"), query.value(6).toDouble());
                row.insert(QStringLiteral("source"), query.value(7).toString());
                snapshot.spotPaths.append(row);
            }
        }
    }

    {
        struct BandMetric {
            QVariantMap row;
            QString band;
            int total {0};
            int uniqueCalls {0};
            int rx {0};
            int tx {0};
            int local {0};
            int psk {0};
            double averageSnr {-30.0};
            qint64 latestMs {0};
        };

        int const windowHours =
            (options.bandActivityWindowHours == 1
             || options.bandActivityWindowHours == 6
             || options.bandActivityWindowHours == 12
             || options.bandActivityWindowHours == 24)
                ? options.bandActivityWindowHours : 6;
        qint64 const windowMs = windowHours * 60LL * 60LL * 1000LL;
        qint64 const activityCutoff = nowMs - windowMs;
        QString const pskPredicate =
            QStringLiteral("(lower(source)='psk'"
                           " OR lower(COALESCE(provider,'')) LIKE '%psk reporter%')");
        QString const directionExpression =
            QStringLiteral("upper(COALESCE(direction,'RX'))");
        QVector<BandMetric> metrics;

        QSqlQuery aggregate(db);
        aggregate.prepare(
            QStringLiteral(
                "SELECT band,"
                " SUM(CASE WHEN NOT %1 AND %2='RX' THEN 1 ELSE 0 END),"
                " SUM(CASE WHEN NOT %1 AND %2='TX' THEN 1 ELSE 0 END),"
                " SUM(CASE WHEN %1 AND %2='RX' THEN 1 ELSE 0 END),"
                " SUM(CASE WHEN %1 AND %2='TX' THEN 1 ELSE 0 END),"
                " COUNT(DISTINCT CASE WHEN %2='RX'"
                "   THEN upper(NULLIF(call,'')) END),"
                " ROUND(AVG(CASE WHEN %2='RX' THEN snr END), 1),"
                " MAX(observed_ms)"
                " FROM map_spot_event"
                " WHERE observed_ms>=:activity_cutoff"
                " AND trim(band)<>'' AND lower(band)<>'unknown'"
                " AND (:activity_all_mode=1 OR upper(mode)=upper(:activity_mode))"
                " AND (:activity_all_propagation=1 OR upper(propagation_mode)=upper(:activity_propagation))"
                " GROUP BY band")
                .arg(pskPredicate, directionExpression));
        aggregate.bindValue(QStringLiteral(":activity_cutoff"), activityCutoff);
        aggregate.bindValue(QStringLiteral(":activity_all_mode"), allMode);
        aggregate.bindValue(QStringLiteral(":activity_mode"), options.mode);
        aggregate.bindValue(QStringLiteral(":activity_all_propagation"), allPropagation);
        aggregate.bindValue(QStringLiteral(":activity_propagation"), options.propagation);
        int maxTotal = 0;
        int maxUniqueCalls = 0;
        if (aggregate.exec()) {
            while (aggregate.next()) {
                BandMetric metric;
                metric.band = aggregate.value(0).toString();
                int const localRx = aggregate.value(1).toInt();
                int const localTx = aggregate.value(2).toInt();
                int const pskRx = aggregate.value(3).toInt();
                int const pskTx = aggregate.value(4).toInt();
                metric.uniqueCalls = aggregate.value(5).toInt();
                if (!aggregate.value(6).isNull()) {
                    metric.averageSnr = aggregate.value(6).toDouble();
                }
                metric.latestMs = aggregate.value(7).toLongLong();
                metric.rx = localRx + pskRx;
                metric.tx = localTx + pskTx;
                metric.local = localRx + localTx;
                metric.psk = pskRx + pskTx;
                metric.total = metric.local + metric.psk;
                metric.row.insert(QStringLiteral("band"), metric.band);
                metric.row.insert(QStringLiteral("localRx"), localRx);
                metric.row.insert(QStringLiteral("localTx"), localTx);
                metric.row.insert(QStringLiteral("pskRx"), pskRx);
                metric.row.insert(QStringLiteral("pskTx"), pskTx);
                metric.row.insert(QStringLiteral("rx"), metric.rx);
                metric.row.insert(QStringLiteral("tx"), metric.tx);
                metric.row.insert(QStringLiteral("local"), metric.local);
                metric.row.insert(QStringLiteral("psk"), metric.psk);
                metric.row.insert(QStringLiteral("total"), metric.total);
                metric.row.insert(QStringLiteral("uniqueCalls"), metric.uniqueCalls);
                metric.row.insert(QStringLiteral("averageSnr"), metric.averageSnr);
                metric.row.insert(QStringLiteral("latestMs"), metric.latestMs);
                maxTotal = qMax(maxTotal, metric.total);
                maxUniqueCalls = qMax(maxUniqueCalls, metric.uniqueCalls);
                metrics.append(std::move(metric));
            }
        } else if (snapshot.error.isEmpty()) {
            snapshot.error = aggregate.lastError().text();
        }

        for (BandMetric& metric : metrics) {
            double const volumeQuality = maxTotal > 0
                ? std::log1p(static_cast<double>(metric.total))
                    / std::log1p(static_cast<double>(maxTotal))
                : 0.0;
            double const uniqueQuality = maxUniqueCalls > 0
                ? static_cast<double>(metric.uniqueCalls) / maxUniqueCalls
                : 0.0;
            double const snrQuality =
                qBound(0.0, (metric.averageSnr + 30.0) / 40.0, 1.0);
            double const recencyQuality = metric.latestMs > 0
                ? qBound(0.0,
                         1.0 - static_cast<double>(nowMs - metric.latestMs)
                                   / static_cast<double>(windowMs),
                         1.0)
                : 0.0;
            double const balanceQuality = metric.total > 0
                ? 1.0 - static_cast<double>(qAbs(metric.rx - metric.tx))
                            / metric.total
                : 0.0;
            double const agreementQuality =
                qMax(metric.local, metric.psk) > 0
                    ? static_cast<double>(qMin(metric.local, metric.psk))
                        / qMax(metric.local, metric.psk)
                    : 0.0;
            int const score = qBound(
                0,
                qRound(100.0 * (0.35 * volumeQuality
                                + 0.25 * uniqueQuality
                                + 0.15 * snrQuality
                                + 0.10 * recencyQuality
                                + 0.05 * balanceQuality
                                + 0.10 * agreementQuality)),
                100);
            metric.row.insert(QStringLiteral("score"), score);
            metric.row.insert(QStringLiteral("volumeQuality"), volumeQuality);
            metric.row.insert(QStringLiteral("uniqueQuality"), uniqueQuality);
            metric.row.insert(QStringLiteral("snrQuality"), snrQuality);
            metric.row.insert(QStringLiteral("recencyQuality"), recencyQuality);
            metric.row.insert(QStringLiteral("balanceQuality"), balanceQuality);
            metric.row.insert(QStringLiteral("agreementQuality"), agreementQuality);
        }
        std::sort(metrics.begin(), metrics.end(),
                  [](BandMetric const& left, BandMetric const& right) {
                      int const leftScore =
                          left.row.value(QStringLiteral("score")).toInt();
                      int const rightScore =
                          right.row.value(QStringLiteral("score")).toInt();
                      return leftScore != rightScore
                          ? leftScore > rightScore
                          : bandOrder(left.band) < bandOrder(right.band);
                  });

        int totalLocalRx = 0;
        int totalLocalTx = 0;
        int totalPskRx = 0;
        int totalPskTx = 0;
        for (int index = 0; index < metrics.size(); ++index) {
            metrics[index].row.insert(QStringLiteral("rank"), index + 1);
            metrics[index].row.insert(QStringLiteral("best"), index == 0);
            snapshot.bandActivity.append(metrics[index].row);
            totalLocalRx +=
                metrics[index].row.value(QStringLiteral("localRx")).toInt();
            totalLocalTx +=
                metrics[index].row.value(QStringLiteral("localTx")).toInt();
            totalPskRx +=
                metrics[index].row.value(QStringLiteral("pskRx")).toInt();
            totalPskTx +=
                metrics[index].row.value(QStringLiteral("pskTx")).toInt();
        }

        QSqlQuery timeline(db);
        timeline.prepare(
            QStringLiteral(
                "SELECT (observed_ms / 900000) * 900000, band,"
                " SUM(CASE WHEN NOT %1 AND %2='RX' THEN 1 ELSE 0 END),"
                " SUM(CASE WHEN NOT %1 AND %2='TX' THEN 1 ELSE 0 END),"
                " SUM(CASE WHEN %1 AND %2='RX' THEN 1 ELSE 0 END),"
                " SUM(CASE WHEN %1 AND %2='TX' THEN 1 ELSE 0 END)"
                " FROM map_spot_event"
                " WHERE observed_ms>=:activity_cutoff"
                " AND trim(band)<>'' AND lower(band)<>'unknown'"
                " AND (:activity_all_mode=1 OR upper(mode)=upper(:activity_mode))"
                " AND (:activity_all_propagation=1 OR upper(propagation_mode)=upper(:activity_propagation))"
                " GROUP BY 1, band ORDER BY 1, band")
                .arg(pskPredicate, directionExpression));
        timeline.bindValue(QStringLiteral(":activity_cutoff"), activityCutoff);
        timeline.bindValue(QStringLiteral(":activity_all_mode"), allMode);
        timeline.bindValue(QStringLiteral(":activity_mode"), options.mode);
        timeline.bindValue(QStringLiteral(":activity_all_propagation"), allPropagation);
        timeline.bindValue(QStringLiteral(":activity_propagation"), options.propagation);
        if (timeline.exec()) {
            while (timeline.next()) {
                QVariantMap row;
                int const localRx = timeline.value(2).toInt();
                int const localTx = timeline.value(3).toInt();
                int const pskRx = timeline.value(4).toInt();
                int const pskTx = timeline.value(5).toInt();
                row.insert(QStringLiteral("bucketMs"),
                           timeline.value(0).toLongLong());
                row.insert(QStringLiteral("band"), timeline.value(1).toString());
                row.insert(QStringLiteral("localRx"), localRx);
                row.insert(QStringLiteral("localTx"), localTx);
                row.insert(QStringLiteral("pskRx"), pskRx);
                row.insert(QStringLiteral("pskTx"), pskTx);
                row.insert(QStringLiteral("total"),
                           localRx + localTx + pskRx + pskTx);
                snapshot.bandActivityTimeline.append(row);
            }
        } else if (snapshot.error.isEmpty()) {
            snapshot.error = timeline.lastError().text();
        }

        snapshot.bandActivitySummary.insert(
            QStringLiteral("windowHours"), windowHours);
        snapshot.bandActivitySummary.insert(
            QStringLiteral("bandCount"), metrics.size());
        snapshot.bandActivitySummary.insert(
            QStringLiteral("localRx"), totalLocalRx);
        snapshot.bandActivitySummary.insert(
            QStringLiteral("localTx"), totalLocalTx);
        snapshot.bandActivitySummary.insert(
            QStringLiteral("pskRx"), totalPskRx);
        snapshot.bandActivitySummary.insert(
            QStringLiteral("pskTx"), totalPskTx);
        snapshot.bandActivitySummary.insert(
            QStringLiteral("generatedMs"), nowMs);
        if (!metrics.isEmpty()) {
            snapshot.bandActivitySummary.insert(
                QStringLiteral("bestBand"), metrics.first().band);
            snapshot.bandActivitySummary.insert(
                QStringLiteral("bestScore"),
                metrics.first().row.value(QStringLiteral("score")));
        } else {
            snapshot.bandActivitySummary.insert(
                QStringLiteral("bestBand"), QString());
            snapshot.bandActivitySummary.insert(
                QStringLiteral("bestScore"), 0);
        }
    }

    // Blend retained spot history into the GPU coverage model.  This keeps the
    // heatmap a first-class map layer instead of a sidebar-only statistic.
    // Existing grid state wins for QSO/QSL data; the PSK fields add intensity
    // and recency without overwriting that history.
    if (!snapshot.spotHeatmap.isEmpty()) {
        QHash<QString, int> coverageByGrid;
        coverageByGrid.reserve(snapshot.coverage.size());
        for (int index = 0; index < snapshot.coverage.size(); ++index) {
            QString const grid = snapshot.coverage.at(index).toMap()
                                     .value(QStringLiteral("grid")).toString()
                                     .trimmed().toUpper();
            if (!grid.isEmpty()) coverageByGrid.insert(grid, index);
        }
        for (QVariant const& value : std::as_const(snapshot.spotHeatmap)) {
            QVariantMap const heat = value.toMap();
            QString const grid = heat.value(QStringLiteral("grid")).toString()
                                     .trimmed().toUpper();
            if (grid.size() < 4) continue;
            int const count = heat.value(QStringLiteral("count")).toInt();
            qint64 const lastObservedMs = heat.value(QStringLiteral("lastObservedMs")).toLongLong();
            double const intensity = qBound(0.18,
                0.20 + qLn(1.0 + qMax(0, count)) / 4.0, 1.0);
            auto applyHeat = [&](QVariantMap& coverage) {
                coverage.insert(QStringLiteral("psk"), true);
                coverage.insert(QStringLiteral("pskCount"), count);
                coverage.insert(QStringLiteral("heatCount"), count);
                coverage.insert(QStringLiteral("heatLastObservedMs"), lastObservedMs);
                coverage.insert(QStringLiteral("heatAverageSnr"),
                                heat.value(QStringLiteral("averageSnr")));
                coverage.insert(QStringLiteral("heatReceivers"),
                                heat.value(QStringLiteral("receivers")));
                coverage.insert(QStringLiteral("liveOpacity"), qMax(
                                    coverage.value(QStringLiteral("liveOpacity"), 0.0).toDouble(),
                                    intensity));
            };
            auto const existing = coverageByGrid.constFind(grid);
            if (existing != coverageByGrid.constEnd()) {
                QVariantMap coverage = snapshot.coverage.at(existing.value()).toMap();
                applyHeat(coverage);
                snapshot.coverage[existing.value()] = coverage;
            } else {
                QVariantMap coverage;
                coverage.insert(QStringLiteral("grid"), grid);
                coverage.insert(QStringLiteral("worked"), false);
                coverage.insert(QStringLiteral("confirmed"), false);
                coverage.insert(QStringLiteral("active"), false);
                coverage.insert(QStringLiteral("missing"), false);
                coverage.insert(QStringLiteral("split"), false);
                coverage.insert(QStringLiteral("liveCount"), 0);
                coverage.insert(QStringLiteral("liveStatus"), QStringLiteral("PSK"));
                applyHeat(coverage);
                coverageByGrid.insert(grid, snapshot.coverage.size());
                snapshot.coverage.append(coverage);
            }
        }
    }

    {
        QVariantMap statistics;
        {
            QSqlQuery total(db);
            if (total.exec(QStringLiteral(
                    "SELECT COUNT(*), COALESCE(SUM(confirmed), 0),"
                    " COUNT(DISTINCT CASE WHEN call<>'' THEN upper(call) END),"
                    " COUNT(DISTINCT CASE WHEN dxcc<>'' THEN lower(dxcc) END),"
                    " COUNT(DISTINCT CASE WHEN grid4<>'' THEN upper(grid4) END),"
                    " MIN(qso_epoch), MAX(qso_epoch)"
                    " FROM map_qso"))) {
                if (total.next()) {
                    statistics.insert(QStringLiteral("totalQso"), total.value(0).toInt());
                    statistics.insert(QStringLiteral("totalConfirmed"),
                                      total.value(1).toInt());
                    statistics.insert(QStringLiteral("totalCalls"), total.value(2).toInt());
                    statistics.insert(QStringLiteral("totalDxcc"), total.value(3).toInt());
                    statistics.insert(QStringLiteral("totalGrids"), total.value(4).toInt());
                    statistics.insert(QStringLiteral("totalFirstEpoch"),
                                      total.value(5).toLongLong());
                    statistics.insert(QStringLiteral("totalLastEpoch"),
                                      total.value(6).toLongLong());
                }
            }
            QSqlQuery totalGridQuery(db);
            if (totalGridQuery.exec(QStringLiteral(
                    "SELECT COUNT(DISTINCT upper(grid4))"
                    " FROM map_qso_grid WHERE grid4<>''"))
                && totalGridQuery.next()) {
                statistics.insert(QStringLiteral("totalGrids"),
                                  totalGridQuery.value(0).toInt());
            }
        }
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT COUNT(*), COALESCE(SUM(confirmed), 0),"
            " COUNT(DISTINCT CASE WHEN call<>'' THEN upper(call) END),"
            " COUNT(DISTINCT CASE WHEN dxcc<>'' THEN lower(dxcc) END),"
            " COUNT(DISTINCT CASE WHEN grid4<>'' THEN upper(grid4) END),"
            " MIN(qso_epoch), MAX(qso_epoch)"
            " FROM map_qso WHERE 1=1") + qsoFilter);
        bindCommon(query, false);
        if (query.exec() && query.next()) {
            statistics.insert(QStringLiteral("qso"), query.value(0).toInt());
            statistics.insert(QStringLiteral("confirmed"), query.value(1).toInt());
            statistics.insert(QStringLiteral("calls"), query.value(2).toInt());
            statistics.insert(QStringLiteral("dxcc"), query.value(3).toInt());
            statistics.insert(QStringLiteral("grids"), query.value(4).toInt());
            statistics.insert(QStringLiteral("firstEpoch"), query.value(5).toLongLong());
            statistics.insert(QStringLiteral("lastEpoch"), query.value(6).toLongLong());
        }
        QSqlQuery filteredGridQuery(db);
        filteredGridQuery.prepare(QStringLiteral(
            "SELECT COUNT(DISTINCT upper(g.grid4))"
            " FROM map_qso_grid g JOIN map_qso q ON q.id=g.qso_id"
            " WHERE g.grid4<>''") + qsoFilter);
        bindCommon(filteredGridQuery, false);
        if (filteredGridQuery.exec() && filteredGridQuery.next()) {
            statistics.insert(QStringLiteral("grids"),
                              filteredGridQuery.value(0).toInt());
        }

        auto groupedRows = [&db, &bindCommon, &qsoFilter](QString const& column) {
            QVariantList rows;
            QSqlQuery grouped(db);
            grouped.prepare(QStringLiteral(
                "SELECT COALESCE(NULLIF(%1, ''), 'Unknown'), COUNT(*),"
                " COALESCE(SUM(confirmed), 0)"
                " FROM map_qso WHERE 1=1")
                                .arg(column)
                            + qsoFilter
                            + QStringLiteral(
                                " GROUP BY 1 ORDER BY COUNT(*) DESC, 1 LIMIT 12"));
            bindCommon(grouped, false);
            if (grouped.exec()) {
                while (grouped.next()) {
                    QVariantMap row;
                    row.insert(QStringLiteral("label"), grouped.value(0).toString());
                    row.insert(QStringLiteral("qso"), grouped.value(1).toInt());
                    row.insert(QStringLiteral("confirmed"), grouped.value(2).toInt());
                    rows.append(row);
                }
            }
            return rows;
        };
        statistics.insert(QStringLiteral("bands"), groupedRows(QStringLiteral("band")));
        statistics.insert(QStringLiteral("modes"), groupedRows(QStringLiteral("mode")));
        statistics.insert(QStringLiteral("continents"),
                          groupedRows(QStringLiteral("continent")));
        statistics.insert(QStringLiteral("satellites"),
                          groupedRows(QStringLiteral("satellite")));

        QHash<QString, QVariantMap> propagationRowsByCode;
        for (PropagationDefinition const& definition : propagationDefinitions()) {
            QString const code = QString::fromLatin1(definition.code);
            propagationRowsByCode.insert(code, QVariantMap {
                {QStringLiteral("code"), code},
                {QStringLiteral("label"), QString::fromLatin1(definition.label)},
                {QStringLiteral("qso"), 0},
                {QStringLiteral("confirmed"), 0},
                {QStringLiteral("calls"), 0},
                {QStringLiteral("percent"), 0.0}
            });
        }
        QSqlQuery propagationQuery(db);
        propagationQuery.prepare(QStringLiteral(
            "SELECT COALESCE(NULLIF(propagation_mode, ''), 'UNKNOWN'), COUNT(*),"
            " COALESCE(SUM(confirmed), 0),"
            " COUNT(DISTINCT CASE WHEN call<>'' THEN upper(call) END)"
            " FROM map_qso WHERE 1=1") + qsoFilter
            + QStringLiteral(" GROUP BY 1"));
        bindCommon(propagationQuery, false);
        int propagationQso = 0;
        int propagationConfirmed = 0;
        int propagationUnknown = 0;
        if (propagationQuery.exec()) {
            while (propagationQuery.next()) {
                QString code = normalizePropagationMode(propagationQuery.value(0).toString());
                QVariantMap row = propagationRowsByCode.value(code);
                if (row.isEmpty()) {
                    code = QStringLiteral("UNKNOWN");
                    row = propagationRowsByCode.value(code);
                }
                int const qso = propagationQuery.value(1).toInt();
                int const confirmed = propagationQuery.value(2).toInt();
                propagationQso += qso;
                propagationConfirmed += confirmed;
                if (code == QStringLiteral("UNKNOWN")) {
                    propagationUnknown += qso;
                }
                row.insert(QStringLiteral("qso"), qso);
                row.insert(QStringLiteral("confirmed"), confirmed);
                row.insert(QStringLiteral("calls"), propagationQuery.value(3).toInt());
                propagationRowsByCode.insert(code, row);
            }
        } else if (snapshot.error.isEmpty()) {
            snapshot.error = propagationQuery.lastError().text();
        }
        for (PropagationDefinition const& definition : propagationDefinitions()) {
            QString const code = QString::fromLatin1(definition.code);
            QVariantMap row = propagationRowsByCode.value(code);
            int const qso = row.value(QStringLiteral("qso")).toInt();
            row.insert(QStringLiteral("percent"), propagationQso > 0
                           ? 100.0 * static_cast<double>(qso) / propagationQso : 0.0);
            snapshot.propagationStatistics.append(row);
        }
        snapshot.propagationSummary.insert(QStringLiteral("qso"), propagationQso);
        snapshot.propagationSummary.insert(QStringLiteral("confirmed"), propagationConfirmed);
        snapshot.propagationSummary.insert(QStringLiteral("classified"),
                                           propagationQso - propagationUnknown);
        snapshot.propagationSummary.insert(QStringLiteral("unknown"), propagationUnknown);
        snapshot.propagationSummary.insert(QStringLiteral("filter"), options.propagation);
        statistics.insert(QStringLiteral("propagation"), snapshot.propagationStatistics);
        statistics.insert(QStringLiteral("propagationSummary"), snapshot.propagationSummary);
        statistics.insert(QStringLiteral("live"), snapshot.liveSpotCount);
        statistics.insert(QStringLiteral("roster"), snapshot.roster.size());
        statistics.insert(QStringLiteral("spotHeatmap"), snapshot.spotHeatmap.size());
        statistics.insert(QStringLiteral("spotTimeline"), snapshot.spotTimeline.size());
        statistics.insert(QStringLiteral("spotPaths"), snapshot.spotPaths.size());
        statistics.insert(QStringLiteral("awardCatalogCount"), externalAwardDefinitions().size());
        statistics.insert(QStringLiteral("period"), options.period);
        statistics.insert(QStringLiteral("band"), options.band);
        statistics.insert(QStringLiteral("mode"), options.mode);
        snapshot.statistics = statistics;
    }

    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT call, watched, ignored, updated_ms"
                " FROM map_roster_preference"
                " WHERE watched=1 OR ignored=1"
                " ORDER BY updated_ms DESC"))) {
            while (query.next()) {
                QString const call = query.value(0).toString();
                qint64 const updatedMs = query.value(3).toLongLong();
                if (query.value(1).toBool()) {
                    QVariantMap row;
                    row.insert(QStringLiteral("type"), QStringLiteral("WATCH"));
                    row.insert(QStringLiteral("value"), call);
                    row.insert(QStringLiteral("updatedMs"), updatedMs);
                    snapshot.rosterPreferences.append(row);
                }
                if (query.value(2).toBool()) {
                    QVariantMap row;
                    row.insert(QStringLiteral("type"), QStringLiteral("CALL"));
                    row.insert(QStringLiteral("value"), call);
                    row.insert(QStringLiteral("updatedMs"), updatedMs);
                    snapshot.rosterPreferences.append(row);
                }
            }
        }
        if (query.exec(QStringLiteral(
                "SELECT upper(ignore_type), ignore_value, updated_ms"
                " FROM map_roster_ignore ORDER BY updated_ms DESC"))) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("type"), query.value(0).toString());
                row.insert(QStringLiteral("value"), query.value(1).toString());
                row.insert(QStringLiteral("updatedMs"), query.value(2).toLongLong());
                snapshot.rosterPreferences.append(row);
            }
        }
    }

    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT rule_type, rule_value, rule_action, band, mode, enabled, updated_ms"
                " FROM map_roster_rule ORDER BY rule_type, rule_value, band, mode"))) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("type"), query.value(0).toString());
                row.insert(QStringLiteral("value"), query.value(1).toString());
                row.insert(QStringLiteral("action"), query.value(2).toString());
                row.insert(QStringLiteral("band"), query.value(3).toString());
                row.insert(QStringLiteral("mode"), query.value(4).toString());
                row.insert(QStringLiteral("enabled"), query.value(5).toBool());
                row.insert(QStringLiteral("updatedMs"), query.value(6).toLongLong());
                snapshot.rosterRules.append(row);
            }
        }
    }

    {
        QSqlQuery query(db);
        QString awardAggregate = QStringLiteral(
            "SELECT"
            " COUNT(DISTINCT CASE WHEN dxcc<>'' THEN dxcc END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND dxcc<>'' THEN dxcc END),"
            " COUNT(DISTINCT CASE WHEN grid4<>'' THEN grid4 END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND grid4<>'' THEN grid4 END),"
            " COUNT(DISTINCT CASE WHEN cq_zone>0 THEN cq_zone END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND cq_zone>0 THEN cq_zone END),"
            " COUNT(DISTINCT CASE WHEN upper(state) IN ("
            "'AL','AK','AZ','AR','CA','CO','CT','DE','FL','GA','HI','ID',"
            "'IL','IN','IA','KS','KY','LA','ME','MD','MA','MI','MN','MS',"
            "'MO','MT','NE','NV','NH','NJ','NM','NY','NC','ND','OH','OK',"
            "'OR','PA','RI','SC','SD','TN','TX','UT','VT','VA','WA','WV',"
            "'WI','WY') THEN upper(state) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND upper(state) IN ("
            "'AL','AK','AZ','AR','CA','CO','CT','DE','FL','GA','HI','ID',"
            "'IL','IN','IA','KS','KY','LA','ME','MD','MA','MI','MN','MS',"
            "'MO','MT','NE','NV','NH','NJ','NM','NY','NC','ND','OH','OK',"
            "'OR','PA','RI','SC','SD','TN','TX','UT','VT','VA','WA','WV',"
            "'WI','WY') THEN upper(state) END),"
            " COUNT(DISTINCT CASE WHEN itu_zone>0 THEN itu_zone END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND itu_zone>0 THEN itu_zone END),"
            " COUNT(DISTINCT CASE WHEN upper(state) IN ("
            "'AL','AZ','AR','CA','CO','CT','DE','FL','GA','ID','IL','IN','IA',"
            "'KS','KY','LA','ME','MD','MA','MI','MN','MS','MO','MT','NE','NV',"
            "'NH','NJ','NM','NY','NC','ND','OH','OK','OR','PA','RI','SC','SD',"
            "'TN','TX','UT','VT','VA','WA','WV','WI','WY') THEN upper(state) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND upper(state) IN ("
            "'AL','AZ','AR','CA','CO','CT','DE','FL','GA','ID','IL','IN','IA',"
            "'KS','KY','LA','ME','MD','MA','MI','MN','MS','MO','MT','NE','NV',"
            "'NH','NJ','NM','NY','NC','ND','OH','OK','OR','PA','RI','SC','SD',"
            "'TN','TX','UT','VT','VA','WA','WV','WI','WY') THEN upper(state) END),"
            " COUNT(DISTINCT CASE WHEN upper(continent) IN "
            "('AF','AS','EU','NA','OC','SA') THEN upper(continent) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND upper(continent) IN "
            "('AF','AS','EU','NA','OC','SA') THEN upper(continent) END),"
            " COUNT(DISTINCT CASE WHEN pota_ref<>'' THEN upper(pota_ref) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND pota_ref<>''"
            " THEN upper(pota_ref) END),"
            " COUNT(DISTINCT CASE WHEN iota<>'' THEN upper(iota) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND iota<>''"
            " THEN upper(iota) END),"
            " COUNT(DISTINCT CASE WHEN wpx<>'' THEN upper(wpx) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND wpx<>''"
            " THEN upper(wpx) END)"
            " FROM map_qso WHERE 1=1");
        awardAggregate.replace(QStringLiteral("confirmed=1"),
                               awardConfirmedColumn + QStringLiteral("=1"));
        query.prepare(awardAggregate + awardQsoFilter);
        bindCommon(query, false);
        bindAward(query);
        bool const awardQueryOk = query.exec();
        if (!awardQueryOk) {
            qWarning().noquote() << "[MAPINT] award aggregate failed:" << query.lastError().text();
        }
        if (awardQueryOk && query.next()) {
            int maidenheadWorked = query.value(2).toInt();
            int maidenheadConfirmed = query.value(3).toInt();
            QSqlQuery gridAwardQuery(db);
            QString gridAwardSql = QStringLiteral(
                "SELECT COUNT(DISTINCT g.grid4),"
                " COUNT(DISTINCT CASE WHEN q.confirmed=1 THEN g.grid4 END)"
                " FROM map_qso_grid g JOIN map_qso q ON q.id=g.qso_id"
                " WHERE g.grid4<>''");
            gridAwardSql.replace(QStringLiteral("q.confirmed=1"),
                                 QStringLiteral("q.%1=1").arg(awardConfirmedColumn));
            gridAwardQuery.prepare(gridAwardSql + awardQsoFilter);
            bindCommon(gridAwardQuery, false);
            bindAward(gridAwardQuery);
            if (gridAwardQuery.exec() && gridAwardQuery.next()) {
                maidenheadWorked = gridAwardQuery.value(0).toInt();
                maidenheadConfirmed = gridAwardQuery.value(1).toInt();
            }
            bool const confirmedGoal =
                options.awardGoal.compare(QStringLiteral("Confirmed"),
                                          Qt::CaseInsensitive) == 0;
            auto addAward = [&snapshot, &options, confirmedGoal](
                                QString const& id, QString const& label,
                                int worked, int confirmed, int target,
                                QString const& rule) {
                int const achieved = confirmedGoal ? confirmed : worked;
                QVariantMap award;
                award.insert(QStringLiteral("id"), id);
                award.insert(QStringLiteral("label"), label);
                award.insert(QStringLiteral("worked"), worked);
                award.insert(QStringLiteral("confirmed"), confirmed);
                award.insert(QStringLiteral("target"), target);
                award.insert(QStringLiteral("achieved"), achieved);
                award.insert(QStringLiteral("remaining"), qMax(0, target - achieved));
                award.insert(QStringLiteral("complete"), achieved >= target);
                award.insert(QStringLiteral("selected"),
                             options.activeAwardProgram.compare(
                                 label, Qt::CaseInsensitive) == 0);
                award.insert(QStringLiteral("goal"), options.awardGoal);
                QStringList scopeParts {
                    options.band, options.mode,
                    options.awardConfirmation,
                    options.awardEndorsement.isEmpty()
                        ? QStringLiteral("Mixed") : options.awardEndorsement,
                    options.awardCallsign.isEmpty()
                        ? QStringLiteral("All callsigns") : options.awardCallsign,
                    (options.awardFromDate.isEmpty() && options.awardToDate.isEmpty())
                        ? QStringLiteral("All time")
                        : QStringLiteral("%1..%2").arg(options.awardFromDate,
                                                        options.awardToDate)
                };
                award.insert(QStringLiteral("scope"), scopeParts.join(QStringLiteral(" / ")));
                award.insert(QStringLiteral("rule"), rule);
                award.insert(QStringLiteral("progress"),
                             target > 0
                                 ? qMin(1.0, achieved / static_cast<double>(target))
                                 : 0.0);
                snapshot.awards.append(award);
            };
            addAward(QStringLiteral("dxcc"), QStringLiteral("DXCC"),
                     query.value(0).toInt(), query.value(1).toInt(), 100,
                     QStringLiteral("Base DXCC: 100 distinct entities"));
            addAward(QStringLiteral("grid"), QStringLiteral("Maidenhead"),
                     maidenheadWorked, maidenheadConfirmed, 100,
                     QStringLiteral("Base grid target: 100 distinct 4-character grids"));
            addAward(QStringLiteral("waz"), QStringLiteral("WAZ"),
                     query.value(4).toInt(), query.value(5).toInt(), 40,
                     QStringLiteral("All 40 CQ zones"));
            addAward(QStringLiteral("was"), QStringLiteral("WAS"),
                     query.value(6).toInt(), query.value(7).toInt(), 50,
                     QStringLiteral("The 50 US states; territories are excluded"));
            addAward(QStringLiteral("itu"), QStringLiteral("ITU Zones"),
                     query.value(8).toInt(), query.value(9).toInt(), 90,
                     QStringLiteral("All 90 ITU zones"));
            addAward(QStringLiteral("us48"), QStringLiteral("US48"),
                     query.value(10).toInt(), query.value(11).toInt(), 48,
                     QStringLiteral("The contiguous 48 US states; Alaska and Hawaii are excluded"));
            addAward(QStringLiteral("wac"), QStringLiteral("WAC"),
                     query.value(12).toInt(), query.value(13).toInt(), 6,
                     QStringLiteral("Six populated continents; Antarctica is excluded"));
            addAward(QStringLiteral("pota"), QStringLiteral("POTA"),
                     query.value(14).toInt(), query.value(15).toInt(), 100,
                     QStringLiteral("Base POTA scorecard: 100 distinct park references"));
            addAward(QStringLiteral("iota"), QStringLiteral("IOTA"),
                     query.value(16).toInt(), query.value(17).toInt(), 100,
                     QStringLiteral("Base IOTA scorecard: 100 distinct island groups"));
            addAward(QStringLiteral("wpx"), QStringLiteral("WPX"),
                     query.value(18).toInt(), query.value(19).toInt(), 100,
                     QStringLiteral("Base WPX scorecard: 100 distinct prefixes"));

            // The external catalog is intentionally queried only for the
            // selected award. This still keeps refreshes bounded, while the
            // rows below let prefix/suffix equivalence rules be applied before
            // an entity is counted.
            if (ExternalAwardDefinition const* external =
                    externalAwardForLabel(options.activeAwardProgram)) {
                if (!externalAwardEntityExpression(*external).isEmpty()) {
                    QSqlQuery externalQuery(db);
                    bool const usesGridTable = external->type == QStringLiteral("grids");
                    QString externalScope = externalAwardScopeFilter(
                        *external, options.awardEndorsement);
                    if (usesGridTable) {
                        externalScope.replace(QStringLiteral("grid4"),
                                              QStringLiteral("g.grid4"));
                    }
                    QString const externalSql = usesGridTable
                        ? QStringLiteral(
                              "SELECT call, g.grid4, band, mode, dxcc, cq_zone, state, continent,"
                              " county, iota, wpx, dxcc_number, confirmed, lotw_confirmed,"
                              " eqsl_confirmed, oqrs FROM map_qso q"
                              " JOIN map_qso_grid g ON g.qso_id=q.id WHERE 1=1")
                        : QStringLiteral(
                              "SELECT call, grid4, band, mode, dxcc, cq_zone, state, continent,"
                              " county, iota, wpx, dxcc_number, confirmed, lotw_confirmed,"
                              " eqsl_confirmed, oqrs FROM map_qso WHERE 1=1");
                    externalQuery.prepare(externalSql + awardQsoFilter + externalScope);
                    bindCommon(externalQuery, false);
                    bindAward(externalQuery);
                    if (externalQuery.exec()) {
                        QSet<QString> workedEntities;
                        QSet<QString> confirmedEntities;
                        QHash<QString, QSet<QString> > workedGroups;
                        QHash<QString, QSet<QString> > confirmedGroups;
                        while (externalQuery.next()) {
                            QString const entity = externalAwardSpotEntity(
                                *external,
                                externalQuery.value(0).toString(),
                                externalQuery.value(2).toString(),
                                externalQuery.value(1).toString(),
                                externalQuery.value(4).toString(),
                                externalQuery.value(5).toInt(),
                                externalQuery.value(6).toString(),
                                externalQuery.value(7).toString(),
                                externalQuery.value(8).toString(),
                                externalQuery.value(9).toString());
                            if (entity.isEmpty()
                                || !externalAwardMatchesFields(
                                    *external, externalQuery.value(0).toString(),
                                    externalQuery.value(2).toString(),
                                    externalQuery.value(3).toString(),
                                    externalQuery.value(1).toString(),
                                    externalQuery.value(11).toInt(),
                                    externalQuery.value(5).toInt(),
                                    externalQuery.value(7).toString(),
                                    externalQuery.value(9).toString(),
                                    options.awardEndorsement)) {
                                continue;
                            }
                            bool confirmed = externalQuery.value(12).toBool();
                            if (options.awardConfirmation.compare(
                                    QStringLiteral("LoTW"), Qt::CaseInsensitive) == 0) {
                                confirmed = externalQuery.value(13).toBool();
                            } else if (options.awardConfirmation.compare(
                                           QStringLiteral("eQSL"), Qt::CaseInsensitive) == 0) {
                                confirmed = externalQuery.value(14).toBool();
                            } else if (options.awardConfirmation.compare(
                                           QStringLiteral("OQRS"), Qt::CaseInsensitive) == 0) {
                                confirmed = externalQuery.value(15).toBool();
                            }
                            workedEntities.insert(entity);
                            if (confirmed) {
                                confirmedEntities.insert(entity);
                            }
                            if (external->type.endsWith(QStringLiteral("2band"))) {
                                QString const base = entity.section(QLatin1Char('@'), 0, 0);
                                workedGroups[base].insert(externalQuery.value(2).toString().toLower());
                                if (confirmed) {
                                    confirmedGroups[base].insert(externalQuery.value(2).toString().toLower());
                                }
                            } else if (external->type == QStringLiteral("calls2dxcc")) {
                                QString const group = externalQuery.value(11).toInt() > 0
                                    ? QString::number(externalQuery.value(11).toInt())
                                    : externalQuery.value(4).toString().toUpper();
                                workedGroups[group].insert(externalQuery.value(0).toString().toUpper());
                                if (confirmed) {
                                    confirmedGroups[group].insert(externalQuery.value(0).toString().toUpper());
                                }
                            }
                        }
                        int worked = workedEntities.size();
                        int confirmed = confirmedEntities.size();
                        int target = external->target;
                        if (external->type.endsWith(QStringLiteral("2band"))
                            && !external->unique.isEmpty()) {
                            target = *std::max_element(external->unique.constBegin(),
                                                       external->unique.constEnd());
                            worked = 0;
                            confirmed = 0;
                            for (QHash<QString, QSet<QString> >::const_iterator it = workedGroups.constBegin();
                                 it != workedGroups.constEnd(); ++it) {
                                if (it.value().size() >= external->requiredBandCount) ++worked;
                                if (confirmedGroups.value(it.key()).size() >= external->requiredBandCount) {
                                    ++confirmed;
                                }
                            }
                        } else if (external->type == QStringLiteral("calls2dxcc")
                                   && !external->unique.isEmpty()) {
                            target = *std::max_element(external->unique.constBegin(),
                                                       external->unique.constEnd());
                            int const requiredEntities = external->count.isEmpty()
                                ? external->dxccNumbers.size()
                                : *std::max_element(external->count.constBegin(),
                                                    external->count.constEnd());
                            worked = requiredEntities > 0 ? requiredEntities : workedGroups.size();
                            confirmed = worked;
                            for (QString const& group : external->dxccNumbers) {
                                worked = qMin(worked, workedGroups.value(group).size());
                                confirmed = qMin(confirmed, confirmedGroups.value(group).size());
                            }
                        }
                        addAward(external->id, external->label,
                                 worked, confirmed, target,
                                 external->tooltip.isEmpty()
                                     ? QStringLiteral("%1 rule: %2")
                                           .arg(external->sponsor, external->type)
                                     : external->tooltip);
                    }
                }
            }

            if (options.activeAwardProgram.compare(QStringLiteral("None"),
                                                   Qt::CaseInsensitive) != 0) {
                ExternalAwardDefinition const* selectedExternal =
                    externalAwardForLabel(options.activeAwardProgram);
                QSet<QString> detailWorked;
                QSet<QString> detailConfirmed;
                QString const active = options.activeAwardProgram.trimmed().toLower();
                auto baseEntity = [&active](QString const& call, QString const& grid,
                                             QString const& dxcc, int cqZone, int ituZone,
                                             QString const& state, QString const& continent,
                                             QString const& pota, QString const& iota,
                                             QString const& wpx) {
                    if (active == QStringLiteral("dxcc")) return dxcc.trimmed().toLower();
                    if (active == QStringLiteral("maidenhead")) return grid.left(4).toUpper();
                    if (active == QStringLiteral("waz")) {
                        return cqZone > 0 && cqZone <= 40 ? QString::number(cqZone) : QString();
                    }
                    if (active == QStringLiteral("was") || active == QStringLiteral("us48")) {
                        QString const candidate = state.trimmed().toUpper();
                        return (active == QStringLiteral("was") ? isWasState(candidate)
                                                                  : isLower48State(candidate))
                            ? candidate : QString();
                    }
                    if (active == QStringLiteral("itu zones")) {
                        return ituZone > 0 && ituZone <= 90 ? QString::number(ituZone) : QString();
                    }
                    if (active == QStringLiteral("wac")) {
                        QString const candidate = continent.trimmed().toUpper();
                        return QStringList {QStringLiteral("AF"), QStringLiteral("AS"),
                                            QStringLiteral("EU"), QStringLiteral("NA"),
                                            QStringLiteral("OC"), QStringLiteral("SA")}
                            .contains(candidate) ? candidate : QString();
                    }
                    if (active == QStringLiteral("pota")) return pota.trimmed().toUpper();
                    if (active == QStringLiteral("iota")) return iota.trimmed().toUpper();
                    if (active == QStringLiteral("wpx")) return wpx.trimmed().toUpper();
                    return call.trimmed().toUpper();
                };
                bool const detailUsesGridTable =
                    active == QStringLiteral("maidenhead")
                    || (selectedExternal
                        && selectedExternal->type == QStringLiteral("grids"));
                QString detailSql = detailUsesGridTable
                    ? QStringLiteral(
                          "SELECT call, g.grid4, band, mode, dxcc, dxcc_number, cq_zone, itu_zone, state,"
                          " continent, county, iota, wpx, pota_ref, confirmed, lotw_confirmed,"
                          " eqsl_confirmed, oqrs FROM map_qso q"
                          " JOIN map_qso_grid g ON g.qso_id=q.id WHERE 1=1")
                    : QStringLiteral(
                          "SELECT call, grid4, band, mode, dxcc, dxcc_number, cq_zone, itu_zone, state,"
                          " continent, county, iota, wpx, pota_ref, confirmed, lotw_confirmed,"
                          " eqsl_confirmed, oqrs FROM map_qso WHERE 1=1");
                detailSql += awardQsoFilter;
                if (selectedExternal) {
                    QString detailScope = externalAwardScopeFilter(
                        *selectedExternal, options.awardEndorsement);
                    if (detailUsesGridTable) {
                        detailScope.replace(QStringLiteral("grid4"),
                                            QStringLiteral("g.grid4"));
                    }
                    detailSql += detailScope;
                }
                QSqlQuery detailQuery(db);
                detailQuery.prepare(detailSql);
                bindCommon(detailQuery, false);
                bindAward(detailQuery);
                if (detailQuery.exec()) {
                    while (detailQuery.next()) {
                        QString entity;
                        if (selectedExternal) {
                            entity = externalAwardSpotEntity(
                                *selectedExternal, detailQuery.value(0).toString(),
                                detailQuery.value(2).toString(), detailQuery.value(1).toString(),
                                detailQuery.value(4).toString(), detailQuery.value(6).toInt(),
                                detailQuery.value(8).toString(), detailQuery.value(9).toString(),
                                detailQuery.value(10).toString(), detailQuery.value(11).toString());
                            if (entity.isEmpty()
                                || !externalAwardMatchesFields(
                                    *selectedExternal, detailQuery.value(0).toString(),
                                    detailQuery.value(2).toString(), detailQuery.value(3).toString(),
                                    detailQuery.value(1).toString(), detailQuery.value(5).toInt(),
                                    detailQuery.value(6).toInt(), detailQuery.value(9).toString(),
                                    detailQuery.value(11).toString(), options.awardEndorsement)) {
                                continue;
                            }
                        } else {
                            entity = baseEntity(
                                detailQuery.value(0).toString(), detailQuery.value(1).toString(),
                                detailQuery.value(4).toString(), detailQuery.value(6).toInt(),
                                detailQuery.value(7).toInt(), detailQuery.value(8).toString(),
                                detailQuery.value(9).toString(), detailQuery.value(13).toString(),
                                detailQuery.value(11).toString(), detailQuery.value(12).toString());
                        }
                        if (entity.isEmpty()) continue;
                        detailWorked.insert(entity);
                        bool confirmed = detailQuery.value(14).toBool();
                        if (options.awardConfirmation.compare(QStringLiteral("LoTW"), Qt::CaseInsensitive) == 0) {
                            confirmed = detailQuery.value(15).toBool();
                        } else if (options.awardConfirmation.compare(QStringLiteral("eQSL"), Qt::CaseInsensitive) == 0) {
                            confirmed = detailQuery.value(16).toBool();
                        } else if (options.awardConfirmation.compare(QStringLiteral("OQRS"), Qt::CaseInsensitive) == 0) {
                            confirmed = detailQuery.value(17).toBool();
                        }
                        if (confirmed) detailConfirmed.insert(entity);
                    }
                }
                QVariantList missing;
                QSet<QString> missingKeys;
                QSqlQuery liveQuery(db);
                liveQuery.prepare(QStringLiteral(
                    "SELECT call, grid, band, mode, dxcc, dxcc_number, cq_zone, itu_zone, state,"
                    " continent, county, pota_ref, iota, wpx FROM map_spot"
                    " WHERE observed_ms >= :live_cutoff"
                    " AND (:all_band = 1 OR lower(band) = lower(:band))"
                    " AND (:all_mode = 1 OR upper(mode) = upper(:mode))"));
                bindCommon(liveQuery, false);
                liveQuery.bindValue(QStringLiteral(":live_cutoff"), spotCutoff);
                if (liveQuery.exec()) {
                    while (liveQuery.next()) {
                        QString entity;
                        if (selectedExternal) {
                            entity = externalAwardSpotEntity(
                                *selectedExternal, liveQuery.value(0).toString(),
                                liveQuery.value(2).toString(), liveQuery.value(1).toString(),
                                liveQuery.value(4).toString(), liveQuery.value(6).toInt(),
                                liveQuery.value(8).toString(), liveQuery.value(9).toString(),
                                liveQuery.value(10).toString(), liveQuery.value(12).toString());
                            if (entity.isEmpty()
                                || !externalAwardMatchesFields(
                                    *selectedExternal, liveQuery.value(0).toString(),
                                    liveQuery.value(2).toString(), liveQuery.value(3).toString(),
                                    liveQuery.value(1).toString(), liveQuery.value(5).toInt(),
                                    liveQuery.value(6).toInt(), liveQuery.value(9).toString(),
                                    liveQuery.value(12).toString(), options.awardEndorsement)) {
                                continue;
                            }
                        } else {
                            entity = baseEntity(
                                liveQuery.value(0).toString(), liveQuery.value(1).toString(),
                                liveQuery.value(4).toString(), liveQuery.value(6).toInt(),
                                liveQuery.value(7).toInt(), liveQuery.value(8).toString(),
                                liveQuery.value(9).toString(), liveQuery.value(11).toString(),
                                liveQuery.value(12).toString(), liveQuery.value(13).toString());
                        }
                        QString const call = liveQuery.value(0).toString().toUpper();
                        QString const key = call + QLatin1Char('\n') + entity;
                        if (entity.isEmpty() || detailWorked.contains(entity)
                            || missingKeys.contains(key)) continue;
                        missingKeys.insert(key);
                        QVariantMap row;
                        row.insert(QStringLiteral("entity"), entity);
                        row.insert(QStringLiteral("call"), call);
                        row.insert(QStringLiteral("grid"), liveQuery.value(1).toString());
                        row.insert(QStringLiteral("band"), liveQuery.value(2).toString());
                        row.insert(QStringLiteral("mode"), liveQuery.value(3).toString());
                        row.insert(QStringLiteral("dxcc"), liveQuery.value(4).toString());
                        row.insert(QStringLiteral("reason"),
                                   QStringLiteral("Missing %1: %2")
                                       .arg(options.activeAwardProgram, entity));
                        missing.append(row);
                    }
                }
                std::sort(missing.begin(), missing.end(), [](QVariant const& left,
                                                             QVariant const& right) {
                    QVariantMap const a = left.toMap();
                    QVariantMap const b = right.toMap();
                    return a.value(QStringLiteral("entity")).toString()
                        + a.value(QStringLiteral("call")).toString()
                        < b.value(QStringLiteral("entity")).toString()
                        + b.value(QStringLiteral("call")).toString();
                });
                QStringList workedEntities = detailWorked.values();
                QStringList confirmedEntities = detailConfirmed.values();
                std::sort(workedEntities.begin(), workedEntities.end());
                std::sort(confirmedEntities.begin(), confirmedEntities.end());
                for (int index = 0; index < snapshot.awards.size(); ++index) {
                    QVariantMap award = snapshot.awards.at(index).toMap();
                    if (award.value(QStringLiteral("label")).toString().compare(
                            options.activeAwardProgram, Qt::CaseInsensitive) == 0) {
                        award.insert(QStringLiteral("workedEntities"), workedEntities);
                        award.insert(QStringLiteral("confirmedEntities"), confirmedEntities);
                        award.insert(QStringLiteral("missing"), missing);
                        award.insert(QStringLiteral("missingCount"), missing.size());
                        snapshot.awards[index] = award;
                        break;
                    }
                }
                snapshot.awardMissing = missing;
            }
        }
    }

    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT alert_type, call, grid, dxcc, message, created_ms, is_read"
                " FROM map_alert ORDER BY created_ms DESC LIMIT 50"))) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("type"), query.value(0).toString());
                row.insert(QStringLiteral("call"), query.value(1).toString());
                row.insert(QStringLiteral("grid"), query.value(2).toString());
                row.insert(QStringLiteral("dxcc"), query.value(3).toString());
                row.insert(QStringLiteral("message"), query.value(4).toString());
                row.insert(QStringLiteral("createdMs"), query.value(5).toLongLong());
                row.insert(QStringLiteral("read"), query.value(6).toBool());
                snapshot.alerts.append(row);
                if (!query.value(6).toBool()) ++snapshot.unreadAlertCount;
            }
        }
    }
    return snapshot;
}

MapIntelligenceService::GridDetails
MapIntelligenceService::queryGridDetails(const QString& databasePath,
                                         const QString& grid)
{
    GridDetails details;
    QString const fullGrid = normalizedGrid(grid);
    QString const normalized = fullGrid.left(fullGrid.size() >= 6 ? 6 : 4);
    QString const gridColumn =
        normalized.size() == 6 ? QStringLiteral("grid6") : QStringLiteral("grid4");
    details.summary.insert(QStringLiteral("grid"), normalized);
    if (normalized.isEmpty()) {
        details.error = QStringLiteral("Invalid Maidenhead grid");
        return details;
    }

    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, &details.error)) {
        return details;
    }
    QSqlDatabase& db = connection->database();
    int workedCount = 0;
    int confirmedCount = 0;
    int historicalCallCount = 0;
    int activeCount = 0;
    int pskCount = 0;
    int liveCallCount = 0;

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT COUNT(DISTINCT q.id),"
            " COUNT(DISTINCT CASE WHEN q.confirmed<>0 THEN q.id END),"
            " COUNT(DISTINCT upper(q.call))"
            " FROM map_qso q JOIN map_qso_grid g ON g.qso_id=q.id"
            " WHERE upper(g.%1)=upper(:grid)").arg(gridColumn));
        query.bindValue(QStringLiteral(":grid"), normalized);
        if (query.exec() && query.next()) {
            workedCount = query.value(0).toInt();
            confirmedCount = query.value(1).toInt();
            historicalCallCount = query.value(2).toInt();
        } else {
            details.error = query.lastError().text();
        }
    }

    qint64 const liveCutoff =
        QDateTime::currentMSecsSinceEpoch() - kLiveRetentionMs;
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT COUNT(DISTINCT upper(call)),"
            " COUNT(DISTINCT CASE WHEN lower(source)='psk' THEN upper(call) END),"
            " COUNT(DISTINCT upper(call))"
            " FROM map_spot"
            " WHERE upper(%1)=upper(:grid) AND observed_ms>=:cutoff")
                          .arg(gridColumn));
        query.bindValue(QStringLiteral(":grid"), normalized);
        query.bindValue(QStringLiteral(":cutoff"), liveCutoff);
        if (query.exec() && query.next()) {
            activeCount = query.value(0).toInt();
            pskCount = query.value(1).toInt();
            liveCallCount = query.value(2).toInt();
        } else if (details.error.isEmpty()) {
            details.error = query.lastError().text();
        }
    }

    details.summary.insert(QStringLiteral("workedCount"), workedCount);
    details.summary.insert(QStringLiteral("confirmedCount"), confirmedCount);
    details.summary.insert(QStringLiteral("historicalCallCount"), historicalCallCount);
    details.summary.insert(QStringLiteral("activeCount"), activeCount);
    details.summary.insert(QStringLiteral("pskCount"), pskCount);
    details.summary.insert(QStringLiteral("liveCallCount"), liveCallCount);
    details.summary.insert(QStringLiteral("worked"), workedCount > 0);
    details.summary.insert(QStringLiteral("confirmed"), confirmedCount > 0);
    details.summary.insert(QStringLiteral("active"), activeCount > 0);
    details.summary.insert(QStringLiteral("missing"), workedCount == 0 && activeCount > 0);
    details.summary.insert(QStringLiteral("psk"), pskCount > 0);

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT s.call, s.grid, s.band, s.mode, s.message, s.observed_utc, s.observed_ms,"
            " s.frequency_hz, s.snr, s.source, s.hits, s.dxcc, s.continent, s.state, s.is_cq,"
            " s.target_call, s.distance_km, s.activity_type,"
            " COALESCE((SELECT GROUP_CONCAT(DISTINCT CASE"
            "   WHEN lower(e.source)='decoder' THEN 'Local decode'"
            "   WHEN COALESCE(trim(e.provider),'')<>'' THEN trim(e.provider)"
            "   WHEN lower(e.source)='psk' THEN 'PSK Reporter'"
            "   WHEN lower(e.source)='oams' THEN 'OAMS'"
            "   ELSE upper(e.source) END) FROM map_spot_event e"
            "   WHERE upper(e.call)=upper(s.call)"
            "     AND (s.grid4='' OR e.grid='' OR upper(substr(e.grid,1,4))=upper(s.grid4))"
            "     AND (s.band='' OR lower(e.band)=lower(s.band))"
            "     AND (s.mode='' OR upper(e.mode)=upper(s.mode))"
            "     AND abs(e.observed_ms-s.observed_ms)<=300000), s.source),"
            " MAX(1, COALESCE((SELECT COUNT(DISTINCT lower(e.source) || '|' ||"
            "   lower(COALESCE(e.provider,''))) FROM map_spot_event e"
            "   WHERE upper(e.call)=upper(s.call)"
            "     AND (s.grid4='' OR e.grid='' OR upper(substr(e.grid,1,4))=upper(s.grid4))"
            "     AND (s.band='' OR lower(e.band)=lower(s.band))"
            "     AND (s.mode='' OR upper(e.mode)=upper(s.mode))"
            "     AND abs(e.observed_ms-s.observed_ms)<=300000), 0))"
            " FROM map_spot s"
            " WHERE s.id IN ("
            "   SELECT MAX(recent.id) FROM map_spot recent"
            "   WHERE upper(recent.%1)=upper(:grid) AND recent.observed_ms>=:cutoff"
            "   GROUP BY upper(recent.call)"
            " )"
            " ORDER BY s.observed_ms DESC LIMIT 100").arg(gridColumn));
        query.bindValue(QStringLiteral(":grid"), normalized);
        query.bindValue(QStringLiteral(":cutoff"), liveCutoff);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("call"), query.value(0).toString());
                row.insert(QStringLiteral("grid"), query.value(1).toString());
                row.insert(QStringLiteral("band"), query.value(2).toString());
                row.insert(QStringLiteral("mode"), query.value(3).toString());
                row.insert(QStringLiteral("message"), query.value(4).toString());
                row.insert(QStringLiteral("observedUtc"), query.value(5).toString());
                row.insert(QStringLiteral("observedMs"), query.value(6).toLongLong());
                row.insert(QStringLiteral("frequencyHz"), query.value(7).toLongLong());
                row.insert(QStringLiteral("snr"), query.value(8).toInt());
                row.insert(QStringLiteral("source"), query.value(9).toString());
                row.insert(QStringLiteral("hits"), query.value(10).toInt());
                row.insert(QStringLiteral("dxcc"), query.value(11).toString());
                row.insert(QStringLiteral("continent"), query.value(12).toString());
                row.insert(QStringLiteral("state"), query.value(13).toString());
                row.insert(QStringLiteral("isCq"), query.value(14).toBool());
                row.insert(QStringLiteral("targetCall"), query.value(15).toString());
                row.insert(QStringLiteral("distanceKm"), query.value(16).toDouble());
                row.insert(QStringLiteral("activityType"), query.value(17).toString());
                QString sourceSummary = query.value(18).toString();
                sourceSummary.replace(QLatin1Char(','), QStringLiteral(" · "));
                int const sourceCount = qMax(1, query.value(19).toInt());
                row.insert(QStringLiteral("sourceSummary"), sourceSummary);
                row.insert(QStringLiteral("sourceCount"), sourceCount);
                row.insert(QStringLiteral("corroborationLevel"),
                           sourceCount >= 3 ? QStringLiteral("Strongly corroborated")
                                            : (sourceCount == 2
                                                   ? QStringLiteral("Corroborated")
                                                   : QStringLiteral("Single source")));
                QString const source = query.value(9).toString().trimmed().toLower();
                row.insert(QStringLiteral("gridEvidence"),
                           (source == QStringLiteral("psk") || source == QStringLiteral("oams"))
                               ? QStringLiteral("External spot locator")
                               : QStringLiteral("TX locator in decoded message"));
                details.live.append(row);
            }
        } else if (details.error.isEmpty()) {
            details.error = query.lastError().text();
        }
    }

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT call, grid, band, mode, qso_date, time_on, frequency_mhz,"
            " satellite, sat_mode, freq_rx_mhz, confirmed, source, dxcc, continent, state, qso_epoch,"
            " (SELECT matched.grid FROM map_qso_grid matched"
            "   WHERE matched.qso_id=q.id AND upper(matched.%1)=upper(:grid)"
            "   ORDER BY matched.is_primary DESC, matched.grid LIMIT 1),"
            " (SELECT matched.is_primary FROM map_qso_grid matched"
            "   WHERE matched.qso_id=q.id AND upper(matched.%1)=upper(:grid)"
            "   ORDER BY matched.is_primary DESC, matched.grid LIMIT 1),"
            " COALESCE((SELECT GROUP_CONCAT(extra.grid, ', ') FROM map_qso_grid extra"
            "   WHERE extra.qso_id=q.id AND extra.is_primary=0), '')"
            " FROM map_qso q WHERE EXISTS(SELECT 1 FROM map_qso_grid matched"
            "   WHERE matched.qso_id=q.id AND upper(matched.%1)=upper(:grid))"
            " ORDER BY qso_epoch DESC, qso_date DESC, time_on DESC LIMIT 100")
                          .arg(gridColumn));
        query.bindValue(QStringLiteral(":grid"), normalized);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("call"), query.value(0).toString());
                row.insert(QStringLiteral("grid"), query.value(1).toString());
                row.insert(QStringLiteral("band"), query.value(2).toString());
                row.insert(QStringLiteral("mode"), query.value(3).toString());
                row.insert(QStringLiteral("qsoDate"), query.value(4).toString());
                row.insert(QStringLiteral("timeOn"), query.value(5).toString());
                row.insert(QStringLiteral("frequencyMhz"), query.value(6).toDouble());
                row.insert(QStringLiteral("satellite"), query.value(7).toString());
                row.insert(QStringLiteral("satMode"), query.value(8).toString());
                row.insert(QStringLiteral("frequencyRxMhz"), query.value(9).toDouble());
                row.insert(QStringLiteral("confirmed"), query.value(10).toBool());
                row.insert(QStringLiteral("source"), query.value(11).toString());
                row.insert(QStringLiteral("dxcc"), query.value(12).toString());
                row.insert(QStringLiteral("continent"), query.value(13).toString());
                row.insert(QStringLiteral("state"), query.value(14).toString());
                row.insert(QStringLiteral("qsoEpoch"), query.value(15).toLongLong());
                row.insert(QStringLiteral("matchedGrid"), query.value(16).toString());
                row.insert(QStringLiteral("matchedGridIsPrimary"), query.value(17).toBool());
                QStringList const vuccGrids = query.value(18).toString()
                                                  .split(QStringLiteral(", "),
                                                         Qt::SkipEmptyParts);
                row.insert(QStringLiteral("vuccGrids"), vuccGrids);
                QStringList allGrids;
                if (!query.value(1).toString().isEmpty()) {
                    allGrids.append(query.value(1).toString());
                }
                allGrids.append(vuccGrids);
                allGrids.removeDuplicates();
                row.insert(QStringLiteral("allGrids"), allGrids);
                details.qsos.append(row);
            }
        } else if (details.error.isEmpty()) {
            details.error = query.lastError().text();
        }
    }
    return details;
}

bool MapIntelligenceService::importAdifIntoDatabase(const QString& databasePath,
                                                    const QString& sourcePath,
                                                    const QByteArray& data,
                                                    const QString& fingerprint,
                                                    const QString& defaultOperatorCall,
                                                    QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) {
        return false;
    }
    QSqlDatabase& db = connection->database();

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT value FROM map_meta WHERE key = 'adif_fingerprint'"));
        if (query.exec() && query.next() && query.value(0).toString() == fingerprint) {
            if (!defaultOperatorCall.isEmpty()) {
                QSqlQuery update(db);
                update.prepare(QStringLiteral(
                    "UPDATE map_qso SET operator_call = :operator_call "
                    "WHERE COALESCE(operator_call, '') = ''"));
                update.bindValue(QStringLiteral(":operator_call"),
                                 defaultOperatorCall.toUpper());
                if (!update.exec()) {
                    if (error) *error = update.lastError().text();
                    return false;
                }
            }
            return true;
        }
    }

    QList<QsoRecord> const records = parseAdif(data);
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    if (!execSql(db, QStringLiteral("DELETE FROM map_qso_grid"), error)
        || !execSql(db, QStringLiteral("DELETE FROM map_qso"), error)) {
        db.rollback();
        return false;
    }

    QSqlQuery insert(db);
    if (!insert.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO map_qso"
            " (source_key, call, grid, grid4, grid6, band, mode, propagation_mode, qso_date, time_on,"
            " frequency_mhz, satellite, sat_mode, freq_rx_mhz, confirmed, qso_epoch, source, operator_call, dxcc, dxcc_number, continent,"
            " cq_zone, itu_zone, state, county, lotw_confirmed, eqsl_confirmed, oqrs,"
            " pota_ref, iota, wpx)"
            " VALUES (:key, :call, :grid, :grid4, :grid6, :band, :mode, :propagation_mode, :date, :time,"
            " :freq, :satellite, :sat_mode, :freq_rx, :confirmed, :epoch, :source, :operator_call, :dxcc, :dxcc_number, :continent,"
            " :cq_zone, :itu_zone, :state, :county, :lotw_confirmed, :eqsl_confirmed, :oqrs,"
            " :pota_ref, :iota, :wpx)"))) {
        if (error) *error = insert.lastError().text();
        db.rollback();
        return false;
    }
    QSqlQuery insertGrid(db);
    if (!insertGrid.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO map_qso_grid"
            " (qso_id, grid, grid4, grid6, is_primary)"
            " VALUES (:qso_id, :grid, :grid4, :grid6, :is_primary)"))) {
        if (error) *error = insertGrid.lastError().text();
        db.rollback();
        return false;
    }
    auto storeGrids = [&insertGrid, error](qint64 qsoId, QsoRecord const& record) {
        QStringList grids;
        if (!record.grid.isEmpty()) grids.append(record.grid);
        grids.append(record.vuccGrids);
        QSet<QString> stored;
        for (QString const& rawGrid : std::as_const(grids)) {
            QString const grid = normalizedGrid(rawGrid);
            if (grid.isEmpty() || stored.contains(grid)) continue;
            stored.insert(grid);
            insertGrid.bindValue(QStringLiteral(":qso_id"), qsoId);
            insertGrid.bindValue(QStringLiteral(":grid"), grid);
            insertGrid.bindValue(QStringLiteral(":grid4"), grid.left(4));
            insertGrid.bindValue(QStringLiteral(":grid6"),
                                 grid.size() >= 6 ? grid.left(6) : QString());
            insertGrid.bindValue(QStringLiteral(":is_primary"),
                                 grid == record.grid ? 1 : 0);
            if (!insertGrid.exec()) {
                if (error) *error = insertGrid.lastError().text();
                return false;
            }
        }
        return true;
    };
    for (QsoRecord const& record : records) {
        insert.bindValue(QStringLiteral(":key"), record.sourceKey);
        insert.bindValue(QStringLiteral(":call"), record.call);
        insert.bindValue(QStringLiteral(":grid"), record.grid);
        insert.bindValue(QStringLiteral(":grid4"), record.grid4);
        insert.bindValue(QStringLiteral(":grid6"), record.grid6);
        insert.bindValue(QStringLiteral(":band"), record.band);
        insert.bindValue(QStringLiteral(":mode"), record.mode);
        insert.bindValue(QStringLiteral(":propagation_mode"), record.propagationMode);
        insert.bindValue(QStringLiteral(":date"), record.qsoDate);
        insert.bindValue(QStringLiteral(":time"), record.timeOn);
        insert.bindValue(QStringLiteral(":freq"), record.frequencyMhz);
        insert.bindValue(QStringLiteral(":satellite"), record.satelliteName);
        insert.bindValue(QStringLiteral(":sat_mode"), record.satelliteMode);
        insert.bindValue(QStringLiteral(":freq_rx"), record.receiveFrequencyMhz);
        insert.bindValue(QStringLiteral(":confirmed"), record.confirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":epoch"), record.qsoEpoch);
        insert.bindValue(QStringLiteral(":source"), record.source);
        insert.bindValue(QStringLiteral(":operator_call"),
                         record.operatorCall.isEmpty()
                             ? defaultOperatorCall.toUpper() : record.operatorCall);
        insert.bindValue(QStringLiteral(":dxcc"), record.dxcc);
        insert.bindValue(QStringLiteral(":dxcc_number"), record.dxccNumber);
        insert.bindValue(QStringLiteral(":continent"), record.continent);
        insert.bindValue(QStringLiteral(":cq_zone"), record.cqZone);
        insert.bindValue(QStringLiteral(":itu_zone"), record.ituZone);
        insert.bindValue(QStringLiteral(":state"), record.state);
        insert.bindValue(QStringLiteral(":county"), record.county);
        insert.bindValue(QStringLiteral(":lotw_confirmed"), record.lotwConfirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":eqsl_confirmed"), record.eqslConfirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":oqrs"), record.oqrs ? 1 : 0);
        insert.bindValue(QStringLiteral(":pota_ref"), record.potaReference);
        insert.bindValue(QStringLiteral(":iota"), record.iotaReference);
        insert.bindValue(QStringLiteral(":wpx"), record.wpxPrefix);
        if (!insert.exec()) {
            if (error) *error = insert.lastError().text();
            db.rollback();
            return false;
        }
        qint64 const qsoId = insert.lastInsertId().toLongLong();
        if (qsoId <= 0 || !storeGrids(qsoId, record)) {
            if (error && error->isEmpty()) *error = QStringLiteral("Cannot store QSO grids");
            db.rollback();
            return false;
        }
    }

    QSqlQuery meta(db);
    meta.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO map_meta(key, value) VALUES(:key, :value)"));
    auto writeMeta = [&meta](QString const& key, QString const& value) {
        meta.bindValue(QStringLiteral(":key"), key);
        meta.bindValue(QStringLiteral(":value"), value);
        return meta.exec();
    };
    if (!writeMeta(QStringLiteral("adif_fingerprint"), fingerprint)
        || !writeMeta(QStringLiteral("adif_source_path"), sourcePath)) {
        if (error) *error = meta.lastError().text();
        db.rollback();
        return false;
    }
    return db.commit();
}

bool MapIntelligenceService::appendQsoRecords(const QString& databasePath,
                                              const QList<QsoRecord>& records,
                                              QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) {
        return false;
    }
    QSqlDatabase& db = connection->database();
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    QSqlQuery insert(db);
    if (!insert.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO map_qso"
            " (source_key, call, grid, grid4, grid6, band, mode, propagation_mode, qso_date, time_on,"
            " frequency_mhz, satellite, sat_mode, freq_rx_mhz, confirmed, qso_epoch, source, operator_call, dxcc, dxcc_number, continent,"
            " cq_zone, itu_zone, state, county, lotw_confirmed, eqsl_confirmed, oqrs,"
            " pota_ref, iota, wpx)"
            " VALUES (:key, :call, :grid, :grid4, :grid6, :band, :mode, :propagation_mode, :date, :time,"
            " :freq, :satellite, :sat_mode, :freq_rx, :confirmed, :epoch, :source, :operator_call, :dxcc, :dxcc_number, :continent,"
            " :cq_zone, :itu_zone, :state, :county, :lotw_confirmed, :eqsl_confirmed, :oqrs,"
            " :pota_ref, :iota, :wpx)"))) {
        if (error) *error = insert.lastError().text();
        db.rollback();
        return false;
    }
    QSqlQuery deleteGrids(db);
    deleteGrids.prepare(QStringLiteral("DELETE FROM map_qso_grid WHERE qso_id=:qso_id"));
    QSqlQuery insertGrid(db);
    if (!insertGrid.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO map_qso_grid"
            " (qso_id, grid, grid4, grid6, is_primary)"
            " VALUES (:qso_id, :grid, :grid4, :grid6, :is_primary)"))) {
        if (error) *error = insertGrid.lastError().text();
        db.rollback();
        return false;
    }
    for (QsoRecord const& record : records) {
        insert.bindValue(QStringLiteral(":key"), record.sourceKey);
        insert.bindValue(QStringLiteral(":call"), record.call);
        insert.bindValue(QStringLiteral(":grid"), record.grid);
        insert.bindValue(QStringLiteral(":grid4"), record.grid4);
        insert.bindValue(QStringLiteral(":grid6"), record.grid6);
        insert.bindValue(QStringLiteral(":band"), record.band);
        insert.bindValue(QStringLiteral(":mode"), record.mode);
        insert.bindValue(QStringLiteral(":propagation_mode"), record.propagationMode);
        insert.bindValue(QStringLiteral(":date"), record.qsoDate);
        insert.bindValue(QStringLiteral(":time"), record.timeOn);
        insert.bindValue(QStringLiteral(":freq"), record.frequencyMhz);
        insert.bindValue(QStringLiteral(":satellite"), record.satelliteName);
        insert.bindValue(QStringLiteral(":sat_mode"), record.satelliteMode);
        insert.bindValue(QStringLiteral(":freq_rx"), record.receiveFrequencyMhz);
        insert.bindValue(QStringLiteral(":confirmed"), record.confirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":epoch"), record.qsoEpoch);
        insert.bindValue(QStringLiteral(":source"), record.source);
        insert.bindValue(QStringLiteral(":operator_call"), record.operatorCall);
        insert.bindValue(QStringLiteral(":dxcc"), record.dxcc);
        insert.bindValue(QStringLiteral(":dxcc_number"), record.dxccNumber);
        insert.bindValue(QStringLiteral(":continent"), record.continent);
        insert.bindValue(QStringLiteral(":cq_zone"), record.cqZone);
        insert.bindValue(QStringLiteral(":itu_zone"), record.ituZone);
        insert.bindValue(QStringLiteral(":state"), record.state);
        insert.bindValue(QStringLiteral(":county"), record.county);
        insert.bindValue(QStringLiteral(":lotw_confirmed"), record.lotwConfirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":eqsl_confirmed"), record.eqslConfirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":oqrs"), record.oqrs ? 1 : 0);
        insert.bindValue(QStringLiteral(":pota_ref"), record.potaReference);
        insert.bindValue(QStringLiteral(":iota"), record.iotaReference);
        insert.bindValue(QStringLiteral(":wpx"), record.wpxPrefix);
        if (!insert.exec()) {
            if (error) *error = insert.lastError().text();
            db.rollback();
            return false;
        }
        qint64 const qsoId = insert.lastInsertId().toLongLong();
        if (qsoId <= 0) {
            if (error) *error = QStringLiteral("Cannot resolve stored QSO id");
            db.rollback();
            return false;
        }
        deleteGrids.bindValue(QStringLiteral(":qso_id"), qsoId);
        if (!deleteGrids.exec()) {
            if (error) *error = deleteGrids.lastError().text();
            db.rollback();
            return false;
        }
        QStringList grids;
        if (!record.grid.isEmpty()) grids.append(record.grid);
        grids.append(record.vuccGrids);
        QSet<QString> stored;
        for (QString const& rawGrid : std::as_const(grids)) {
            QString const grid = normalizedGrid(rawGrid);
            if (grid.isEmpty() || stored.contains(grid)) continue;
            stored.insert(grid);
            insertGrid.bindValue(QStringLiteral(":qso_id"), qsoId);
            insertGrid.bindValue(QStringLiteral(":grid"), grid);
            insertGrid.bindValue(QStringLiteral(":grid4"), grid.left(4));
            insertGrid.bindValue(QStringLiteral(":grid6"),
                                 grid.size() >= 6 ? grid.left(6) : QString());
            insertGrid.bindValue(QStringLiteral(":is_primary"),
                                 grid == record.grid ? 1 : 0);
            if (!insertGrid.exec()) {
                if (error) *error = insertGrid.lastError().text();
                db.rollback();
                return false;
            }
        }
    }
    return db.commit();
}

bool MapIntelligenceService::appendLiveSpots(const QString& databasePath,
                                             const QList<LiveSpot>& spots,
                                             const AlertRules& rules,
                                             QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) {
        return false;
    }
    QSqlDatabase& db = connection->database();
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    QSqlQuery insert(db);
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO map_spot"
            " (unique_key, call, grid, grid4, grid6, grid_origin, band, mode, propagation_mode, message, observed_utc,"
            " observed_ms, frequency_hz, snr, dt, source, dxcc, continent, cq_zone,"
            " itu_zone, state, county, pota_ref, iota, wpx, dxcc_number, is_cq, target_call, distance_km, activity_type,"
            " receiver_call, receiver_grid, provider, first_observed_ms, last_observed_ms,"
            " correlation_count, direction)"
            " VALUES (:key, :call, :grid, :grid4, :grid6, :grid_origin, :band, :mode, :propagation_mode, :message, :utc,"
            " :ms, :freq, :snr, :dt, :source, :dxcc, :continent, :cq_zone,"
            " :itu_zone, :state, :county, :pota_ref, :iota, :wpx, :dxcc_number, :is_cq, :target_call, :distance_km, :activity_type,"
            " :receiver_call, :receiver_grid, :provider, :first_observed_ms, :last_observed_ms,"
            " :correlation_count, :direction)"
            " ON CONFLICT(unique_key) DO UPDATE SET"
            " observed_utc=excluded.observed_utc,"
            " observed_ms=excluded.observed_ms,"
            " last_observed_ms=excluded.last_observed_ms,"
            " grid_origin=CASE WHEN excluded.grid_origin<>'UNKNOWN'"
            " THEN excluded.grid_origin ELSE map_spot.grid_origin END,"
            " snr=excluded.snr,"
            " dt=excluded.dt,"
            " county=CASE WHEN excluded.county<>'' THEN excluded.county ELSE map_spot.county END,"
            " pota_ref=CASE WHEN excluded.pota_ref<>'' THEN excluded.pota_ref ELSE map_spot.pota_ref END,"
            " iota=CASE WHEN excluded.iota<>'' THEN excluded.iota ELSE map_spot.iota END,"
            " wpx=CASE WHEN excluded.wpx<>'' THEN excluded.wpx ELSE map_spot.wpx END,"
            " receiver_call=excluded.receiver_call,"
            " receiver_grid=excluded.receiver_grid,"
            " provider=excluded.provider,"
            " direction=excluded.direction,"
            " propagation_mode=CASE WHEN excluded.propagation_mode<>'UNKNOWN'"
            " THEN excluded.propagation_mode ELSE map_spot.propagation_mode END,"
            " correlation_count=MAX(map_spot.correlation_count, excluded.correlation_count),"
            " activity_type=excluded.activity_type,"
            " hits=map_spot.hits+1"))) {
        if (error) *error = insert.lastError().text();
        db.rollback();
        return false;
    }
    QSqlQuery correlated(db);
    correlated.prepare(QStringLiteral(
        "SELECT COUNT(DISTINCT lower(source) || '|' || lower(COALESCE(provider,'')))"
        " FROM map_spot"
        " WHERE upper(call)=upper(:call)"
        " AND (:grid='' OR upper(grid4)=upper(:grid))"
        " AND (:band='' OR lower(band)=lower(:band))"
        " AND (:mode='' OR upper(mode)=upper(:mode))"
        " AND NOT (lower(source)=lower(:source)"
        "          AND lower(COALESCE(provider,''))=lower(:provider))"
        " AND last_observed_ms>=:cutoff"));
    QSqlQuery event(db);
    if (!event.prepare(QStringLiteral(
            "INSERT INTO map_spot_event"
            " (spot_key, call, grid, receiver_call, receiver_grid, band, mode, propagation_mode, source, provider,"
            " observed_ms, frequency_hz, snr, correlation, activity_type, direction)"
            " VALUES (:spot_key, :call, :grid, :receiver_call, :receiver_grid, :band, :mode, :propagation_mode,"
            " :source, :provider, :observed_ms, :frequency_hz, :snr, :correlation,"
            " :activity_type, :direction)"))) {
        if (error) *error = event.lastError().text();
        db.rollback();
        return false;
    }
    for (LiveSpot const& spot : spots) {
        QString const propagationMode = normalizePropagationMode(spot.propagationMode);
        bool const pskReporterSpot =
            spot.source.compare(QStringLiteral("psk"), Qt::CaseInsensitive) == 0
            || spot.provider.contains(QStringLiteral("PSK Reporter"),
                                      Qt::CaseInsensitive);
        bool const visibleSpot =
            spot.direction != QStringLiteral("TX") || pskReporterSpot;
        int correlationCount = 0;
        if (visibleSpot) {
            correlated.bindValue(QStringLiteral(":call"), spot.call);
            correlated.bindValue(QStringLiteral(":grid"), spot.grid4);
            correlated.bindValue(QStringLiteral(":band"), spot.band);
            correlated.bindValue(QStringLiteral(":mode"), spot.mode);
            correlated.bindValue(QStringLiteral(":source"), spot.source);
            correlated.bindValue(QStringLiteral(":provider"), spot.provider);
            correlated.bindValue(QStringLiteral(":cutoff"),
                                 spot.observedMs - 5 * 60 * 1000LL);
            if (correlated.exec() && correlated.next()) {
                correlationCount = correlated.value(0).toInt();
            }
            insert.bindValue(QStringLiteral(":key"), spot.uniqueKey);
            insert.bindValue(QStringLiteral(":call"), spot.call);
            insert.bindValue(QStringLiteral(":grid"), spot.grid);
            insert.bindValue(QStringLiteral(":grid4"), spot.grid4);
            insert.bindValue(QStringLiteral(":grid6"), spot.grid6);
            insert.bindValue(QStringLiteral(":grid_origin"), spot.gridOrigin);
            insert.bindValue(QStringLiteral(":band"), spot.band);
            insert.bindValue(QStringLiteral(":mode"), spot.mode);
            insert.bindValue(QStringLiteral(":propagation_mode"), propagationMode);
            insert.bindValue(QStringLiteral(":message"), spot.message);
            insert.bindValue(QStringLiteral(":utc"), spot.observedUtc);
            insert.bindValue(QStringLiteral(":ms"), spot.observedMs);
            insert.bindValue(QStringLiteral(":freq"), spot.frequencyHz);
            insert.bindValue(QStringLiteral(":snr"), spot.snr);
            insert.bindValue(QStringLiteral(":dt"), spot.dt);
            insert.bindValue(QStringLiteral(":source"), spot.source);
            insert.bindValue(QStringLiteral(":dxcc"), spot.dxcc);
            insert.bindValue(QStringLiteral(":continent"), spot.continent);
            insert.bindValue(QStringLiteral(":cq_zone"), spot.cqZone);
            insert.bindValue(QStringLiteral(":itu_zone"), spot.ituZone);
            insert.bindValue(QStringLiteral(":state"), spot.state);
            insert.bindValue(QStringLiteral(":county"), spot.county);
            insert.bindValue(QStringLiteral(":pota_ref"), spot.potaReference);
            insert.bindValue(QStringLiteral(":iota"), spot.iotaReference);
            insert.bindValue(QStringLiteral(":wpx"), spot.wpxPrefix);
            insert.bindValue(QStringLiteral(":dxcc_number"), spot.dxccNumber);
            insert.bindValue(QStringLiteral(":is_cq"), spot.isCq ? 1 : 0);
            insert.bindValue(QStringLiteral(":target_call"), spot.targetCall);
            insert.bindValue(QStringLiteral(":distance_km"), spot.distanceKm);
            insert.bindValue(QStringLiteral(":activity_type"), spot.activityType);
            insert.bindValue(QStringLiteral(":receiver_call"), spot.receiverCall);
            insert.bindValue(QStringLiteral(":receiver_grid"), spot.receiverGrid);
            insert.bindValue(QStringLiteral(":provider"), spot.provider);
            insert.bindValue(QStringLiteral(":first_observed_ms"), spot.observedMs);
            insert.bindValue(QStringLiteral(":last_observed_ms"), spot.observedMs);
            insert.bindValue(QStringLiteral(":correlation_count"), correlationCount);
            insert.bindValue(QStringLiteral(":direction"), spot.direction);
            if (!insert.exec()) {
                if (error) *error = insert.lastError().text();
                db.rollback();
                return false;
            }
        }
        event.bindValue(QStringLiteral(":spot_key"), spot.uniqueKey);
        event.bindValue(QStringLiteral(":call"), spot.call);
        event.bindValue(QStringLiteral(":grid"), spot.grid);
        event.bindValue(QStringLiteral(":receiver_call"), spot.receiverCall);
        event.bindValue(QStringLiteral(":receiver_grid"), spot.receiverGrid);
        event.bindValue(QStringLiteral(":band"), spot.band);
        event.bindValue(QStringLiteral(":mode"), spot.mode);
        event.bindValue(QStringLiteral(":propagation_mode"), propagationMode);
        event.bindValue(QStringLiteral(":source"), spot.source);
        event.bindValue(QStringLiteral(":provider"), spot.provider);
        event.bindValue(QStringLiteral(":observed_ms"), spot.observedMs);
        event.bindValue(QStringLiteral(":frequency_hz"), spot.frequencyHz);
        event.bindValue(QStringLiteral(":snr"), spot.snr);
        event.bindValue(QStringLiteral(":correlation"), correlationCount);
        event.bindValue(QStringLiteral(":activity_type"), spot.activityType);
        event.bindValue(QStringLiteral(":direction"), spot.direction);
        if (!event.exec()) {
            if (error) *error = event.lastError().text();
            db.rollback();
            return false;
        }
        if (!visibleSpot) {
            continue;
        }

        auto addAlert = [&db, &spot](QString const& type, QString const& text) {
            QString const day = QDateTime::fromMSecsSinceEpoch(
                spot.observedMs, QTimeZone::UTC).date().toString(Qt::ISODate);
            QSqlQuery alert(db);
            alert.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO map_alert"
                " (alert_key, alert_type, call, grid, dxcc, message, created_ms, is_read)"
                " VALUES (:key, :type, :call, :grid, :dxcc, :message, :created, 0)"));
            alert.bindValue(QStringLiteral(":key"),
                            digestKey({type, spot.call, spot.grid4, spot.dxcc, day}));
            alert.bindValue(QStringLiteral(":type"), type);
            alert.bindValue(QStringLiteral(":call"), spot.call);
            alert.bindValue(QStringLiteral(":grid"), spot.grid);
            alert.bindValue(QStringLiteral(":dxcc"), spot.dxcc);
            alert.bindValue(QStringLiteral(":message"), text);
            alert.bindValue(QStringLiteral(":created"), spot.observedMs);
            alert.exec();
        };

        if (rules.newGridEnabled && !spot.grid4.isEmpty()) {
            QSqlQuery knownGrid(db);
            knownGrid.prepare(QStringLiteral(
                "SELECT 1 FROM map_qso_grid WHERE grid4=:grid LIMIT 1"));
            knownGrid.bindValue(QStringLiteral(":grid"), spot.grid4);
            if (knownGrid.exec() && !knownGrid.next()) {
                addAlert(QStringLiteral("new_grid"),
                         QStringLiteral("%1 active from new grid %2")
                             .arg(spot.call, spot.grid4));
            }
        }
        if (rules.newDxccEnabled && !spot.dxcc.isEmpty()) {
            QSqlQuery knownDxcc(db);
            knownDxcc.prepare(QStringLiteral(
                "SELECT 1 FROM map_qso WHERE lower(dxcc)=lower(:dxcc) LIMIT 1"));
            knownDxcc.bindValue(QStringLiteral(":dxcc"), spot.dxcc);
            if (knownDxcc.exec() && !knownDxcc.next()) {
                addAlert(QStringLiteral("new_dxcc"),
                         QStringLiteral("%1 active from new DXCC %2")
                             .arg(spot.call, spot.dxcc));
            }
        }
        if (rules.cqEnabled && spot.isCq) {
            addAlert(QStringLiteral("cq"),
                     QStringLiteral("CQ from %1 %2")
                         .arg(spot.call, spot.grid4));
        }
        if (!rules.callPattern.isEmpty()) {
            QRegularExpression const expression(
                QRegularExpression::wildcardToRegularExpression(
                    rules.callPattern,
                    QRegularExpression::UnanchoredWildcardConversion),
                QRegularExpression::CaseInsensitiveOption);
            if (expression.isValid()
                && (expression.match(spot.call).hasMatch()
                    || expression.match(spot.message).hasMatch())) {
                addAlert(QStringLiteral("call_watch"),
                         QStringLiteral("%1 matched alert pattern %2")
                             .arg(spot.call, rules.callPattern));
            }
        }
    }
    QSqlQuery prune(db);
    prune.prepare(QStringLiteral("DELETE FROM map_spot WHERE observed_ms < :cutoff"));
    prune.bindValue(QStringLiteral(":cutoff"),
                    QDateTime::currentMSecsSinceEpoch() - kLiveRetentionMs);
    prune.exec();
    QSqlQuery pruneEvents(db);
    pruneEvents.prepare(QStringLiteral("DELETE FROM map_spot_event WHERE observed_ms < :cutoff"));
    pruneEvents.bindValue(QStringLiteral(":cutoff"),
                          QDateTime::currentMSecsSinceEpoch() - kSpotEventRetentionMs);
    pruneEvents.exec();
    execSql(db, QStringLiteral(
        "DELETE FROM map_spot WHERE id NOT IN"
        " (SELECT id FROM map_spot ORDER BY observed_ms DESC LIMIT 5000)"), nullptr);
    execSql(db, QStringLiteral(
        "DELETE FROM map_spot_event WHERE id NOT IN"
        " (SELECT id FROM map_spot_event ORDER BY observed_ms DESC LIMIT 100000)"), nullptr);
    execSql(db, QStringLiteral(
        "DELETE FROM map_alert WHERE id NOT IN"
        " (SELECT id FROM map_alert ORDER BY created_ms DESC LIMIT 500)"), nullptr);
    return db.commit();
}

bool MapIntelligenceService::clearLiveSpotRows(const QString& databasePath,
                                               QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) {
        return false;
    }
    return execSql(connection->database(), QStringLiteral("DELETE FROM map_spot"), error);
}

bool MapIntelligenceService::clearPskHeardByRows(const QString& databasePath,
                                                 QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) {
        return false;
    }
    QSqlDatabase& db = connection->database();
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    // The HTTP endpoint returns a replacement snapshot for this station. Keep
    // MQTT events, which are an independent continuous feed, untouched.
    QString const predicate = QStringLiteral(
        "lower(source)='psk'"
        " AND lower(COALESCE(provider, '')) <> 'psk reporter mqtt'");
    if (!execSql(db, QStringLiteral("DELETE FROM map_spot WHERE ") + predicate, error)
        || !execSql(db, QStringLiteral("DELETE FROM map_spot_event WHERE ") + predicate,
                    error)) {
        db.rollback();
        return false;
    }
    return db.commit();
}

bool MapIntelligenceService::clearAlertRows(const QString& databasePath,
                                            QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    return execSql(connection->database(), QStringLiteral("DELETE FROM map_alert"), error);
}

bool MapIntelligenceService::markAlertRowsRead(const QString& databasePath,
                                               QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    return execSql(connection->database(),
                   QStringLiteral("UPDATE map_alert SET is_read=1 WHERE is_read=0"),
                   error);
}

bool MapIntelligenceService::updateRosterPreference(const QString& databasePath,
                                                    const QString& call,
                                                    bool watched,
                                                    bool ignored,
                                                    QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlDatabase& db = connection->database();
    if (!watched && !ignored) {
        QSqlQuery remove(db);
        remove.prepare(QStringLiteral(
            "DELETE FROM map_roster_preference WHERE call=:call"));
        remove.bindValue(QStringLiteral(":call"), call);
        if (!remove.exec()) {
            if (error) *error = remove.lastError().text();
            return false;
        }
        return true;
    }

    QSqlQuery upsert(db);
    upsert.prepare(QStringLiteral(
        "INSERT INTO map_roster_preference(call, watched, ignored, updated_ms)"
        " VALUES(:call, :watched, :ignored, :updated_ms)"
        " ON CONFLICT(call) DO UPDATE SET"
        " watched=excluded.watched,"
        " ignored=excluded.ignored,"
        " updated_ms=excluded.updated_ms"));
    upsert.bindValue(QStringLiteral(":call"), call);
    upsert.bindValue(QStringLiteral(":watched"), watched ? 1 : 0);
    upsert.bindValue(QStringLiteral(":ignored"), ignored ? 1 : 0);
    upsert.bindValue(QStringLiteral(":updated_ms"), QDateTime::currentMSecsSinceEpoch());
    if (!upsert.exec()) {
        if (error) *error = upsert.lastError().text();
        return false;
    }
    return true;
}

bool MapIntelligenceService::updateRosterIgnore(const QString& databasePath,
                                                const QString& type,
                                                const QString& value,
                                                bool ignored,
                                                QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlDatabase& db = connection->database();
    if (!ignored) {
        QSqlQuery remove(db);
        remove.prepare(QStringLiteral(
            "DELETE FROM map_roster_ignore"
            " WHERE upper(ignore_type)=upper(:type)"
            " AND upper(ignore_value)=upper(:value)"));
        remove.bindValue(QStringLiteral(":type"), type);
        remove.bindValue(QStringLiteral(":value"), value);
        if (!remove.exec()) {
            if (error) *error = remove.lastError().text();
            return false;
        }
        return true;
    }

    QSqlQuery upsert(db);
    upsert.prepare(QStringLiteral(
        "INSERT INTO map_roster_ignore(ignore_type, ignore_value, updated_ms)"
        " VALUES(upper(:type), :value, :updated_ms)"
        " ON CONFLICT(ignore_type, ignore_value) DO UPDATE SET"
        " updated_ms=excluded.updated_ms"));
    upsert.bindValue(QStringLiteral(":type"), type);
    upsert.bindValue(QStringLiteral(":value"), value);
    upsert.bindValue(QStringLiteral(":updated_ms"),
                     QDateTime::currentMSecsSinceEpoch());
    if (!upsert.exec()) {
        if (error) *error = upsert.lastError().text();
        return false;
    }
    return true;
}

bool MapIntelligenceService::removeRosterPreferenceRow(
    const QString& databasePath,
    const QString& type,
    const QString& value,
    QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlDatabase& db = connection->database();
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    bool success = true;
    QSqlQuery query(db);
    if (type == QStringLiteral("WATCH")
        || type == QStringLiteral("CALL")) {
        QString const column = type == QStringLiteral("WATCH")
            ? QStringLiteral("watched") : QStringLiteral("ignored");
        query.prepare(QStringLiteral(
            "UPDATE map_roster_preference SET %1=0, updated_ms=:updated_ms"
            " WHERE upper(call)=upper(:value)").arg(column));
        query.bindValue(QStringLiteral(":updated_ms"),
                        QDateTime::currentMSecsSinceEpoch());
        query.bindValue(QStringLiteral(":value"), value);
        success = query.exec();
        if (success) {
            success = execSql(
                db,
                QStringLiteral(
                    "DELETE FROM map_roster_preference"
                    " WHERE watched=0 AND ignored=0"),
                error);
        }
    } else {
        query.prepare(QStringLiteral(
            "DELETE FROM map_roster_ignore"
            " WHERE upper(ignore_type)=upper(:type)"
            " AND upper(ignore_value)=upper(:value)"));
        query.bindValue(QStringLiteral(":type"), type);
        query.bindValue(QStringLiteral(":value"), value);
        success = query.exec();
    }

    if (!success) {
        if (error && error->isEmpty()) *error = query.lastError().text();
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    return true;
}

bool MapIntelligenceService::clearRosterPreferenceRows(const QString& databasePath,
                                                       QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlDatabase& db = connection->database();
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    if (!execSql(db, QStringLiteral("DELETE FROM map_roster_preference"), error)
        || !execSql(db, QStringLiteral("DELETE FROM map_roster_ignore"), error)) {
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    return true;
}

bool MapIntelligenceService::updateRosterRuleRow(const QString& databasePath,
                                                 const QString& type,
                                                 const QString& value,
                                                 const QString& action,
                                                 const QString& band,
                                                 const QString& mode,
                                                 QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlQuery query(connection->database());
    query.prepare(QStringLiteral(
        "INSERT INTO map_roster_rule(rule_type, rule_value, rule_action, band, mode, enabled, updated_ms)"
        " VALUES(upper(:type), upper(:value), upper(:action), lower(:band), upper(:mode), 1, :updated)"
        " ON CONFLICT(rule_type, rule_value, band, mode) DO UPDATE SET"
        " rule_action=excluded.rule_action, enabled=1, updated_ms=excluded.updated_ms"));
    query.bindValue(QStringLiteral(":type"), type);
    query.bindValue(QStringLiteral(":value"), value);
    query.bindValue(QStringLiteral(":action"), action);
    QString const normalizedBand = band.trimmed().isNull()
        ? QStringLiteral("") : band.trimmed();
    QString const normalizedMode = mode.trimmed().isNull()
        ? QStringLiteral("") : mode.trimmed();
    query.bindValue(QStringLiteral(":band"), normalizedBand);
    query.bindValue(QStringLiteral(":mode"), normalizedMode);
    query.bindValue(QStringLiteral(":updated"), QDateTime::currentMSecsSinceEpoch());
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool MapIntelligenceService::removeRosterRuleRow(const QString& databasePath,
                                                 const QString& type,
                                                 const QString& value,
                                                 const QString& band,
                                                 const QString& mode,
                                                 QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlQuery query(connection->database());
    query.prepare(QStringLiteral(
        "DELETE FROM map_roster_rule WHERE upper(rule_type)=upper(:type)"
        " AND upper(rule_value)=upper(:value) AND lower(band)=lower(:band)"
        " AND upper(mode)=upper(:mode)"));
    query.bindValue(QStringLiteral(":type"), type);
    query.bindValue(QStringLiteral(":value"), value);
    QString const normalizedBand = band.trimmed().isNull()
        ? QStringLiteral("") : band.trimmed();
    QString const normalizedMode = mode.trimmed().isNull()
        ? QStringLiteral("") : mode.trimmed();
    query.bindValue(QStringLiteral(":band"), normalizedBand);
    query.bindValue(QStringLiteral(":mode"), normalizedMode);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

void MapIntelligenceService::queueSnapshotQuery(quint64 generation)
{
    QString const database = m_databasePath;
    QueryOptions const options {
        m_bandFilter, m_modeFilter, m_periodFilter, m_continentFilter,
        m_dxccFilter, m_sourceFilter, m_propagationFilter, m_rosterSort, m_rosterStatusFilter,
        m_rosterHuntScope, m_rosterScope, m_rosterDxccScope,
        m_rosterMyCall, m_rosterMyDxcc, m_activeAwardProgram, m_awardGoal,
        m_rosterRetentionMinutes, m_gridPrecision, m_liveDecayMinutes,
        m_sourceDecayMinutes,
        m_cqOnly, m_rosterSortDescending, m_rosterCqOnly,
        m_splitGridEnabled, m_rosterTextFilter, m_rosterTextMode,
        pskLayerEnabled(), m_pskDisplayMode, m_pskOpacityPercent / 100.0,
        m_spotAgeFilter, m_spotCorrelationFilter, m_bandActivityWindowHours,
        m_rosterUsesLoTW, m_rosterMaxLoTWDays, m_rosterUsesEQSL,
        m_rosterUsesOQRS, m_rosterSpottedMeOnly, m_rosterMinSnrEnabled,
        m_rosterMinSnr, m_rosterMaxDtEnabled, m_rosterMaxDt,
        m_rosterTreatRr73AsCq, m_rosterWantedTypes,
        m_awardEndorsement, m_awardConfirmation, m_awardCallsign,
        m_awardFromDate, m_awardToDate
    };
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, options, generation] {
        Snapshot snapshot = queryDatabase(database, options);
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(),
            [guard, generation, snapshot = std::move(snapshot)]() mutable {
                if (guard) {
                    guard->applySnapshot(generation, std::move(snapshot));
                }
            }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::applySnapshot(quint64 generation, Snapshot snapshot)
{
    if (generation != m_queryGeneration.load()) {
        return;
    }

    // The result is current when it reaches the GUI thread. Replace an older
    // result and publish one coalesced batch later. Do not re-check the query
    // generation in the timer callback: a newer query may be scheduled but
    // not completed yet, and dropping this result would leave the map stale.
    m_pendingSnapshotGeneration = generation;
    m_pendingSnapshot = std::move(snapshot);
    m_snapshotPending = true;
    if (!m_snapshotFlushTimer->isActive()) {
        m_snapshotFlushTimer->start();
    }
}

void MapIntelligenceService::applySnapshotNow(Snapshot snapshot)
{
    if (!snapshot.error.isEmpty()) {
        qWarning().noquote() << "[MAPINT] SQLite query failed:" << snapshot.error;
    }

    // Snapshot queries intentionally return all map intelligence domains. On
    // Windows, notifying every QML view after every decode made unchanged
    // rosters, awards and charts rebuild together and stall the scene graph.
    // Compare before moving the snapshot and only notify domains that changed.
    bool const coverageDataChanged = snapshot.coverage != m_rawCoverage
        || snapshot.qsoCount != m_qsoCount
        || snapshot.workedGridCount != m_workedGridCount
        || snapshot.confirmedGridCount != m_confirmedGridCount
        || snapshot.activeGridCount != m_activeGridCount
        || snapshot.missingGridCount != m_missingGridCount;
    bool const rosterDataChanged = snapshot.roster != m_roster
        || snapshot.liveSpotCount != m_liveSpotCount
        || snapshot.rosterWantedCount != m_rosterWantedCount
        || snapshot.rosterNewCount != m_rosterNewCount
        || snapshot.rosterUnconfirmedCount != m_rosterUnconfirmedCount;
    bool const rosterPreferencesDataChanged =
        snapshot.rosterPreferences != m_rosterPreferences;
    bool const awardsDataChanged = snapshot.awards != m_awards
        || snapshot.awardMissing != m_awardMissing;
    bool const alertsDataChanged = snapshot.alerts != m_alerts
        || snapshot.unreadAlertCount != m_unreadAlertCount;
    bool const spotAnalyticsDataChanged = snapshot.spotHeatmap != m_spotHeatmap
        || snapshot.spotTimeline != m_spotTimeline
        || snapshot.spotPaths != m_spotPaths;
    bool const bandActivityDataChanged = snapshot.bandActivity != m_bandActivity
        || snapshot.bandActivityTimeline != m_bandActivityTimeline
        || snapshot.bandActivitySummary != m_bandActivitySummary;
    bool const propagationDataChanged =
        snapshot.propagationStatistics != m_propagationStatistics
        || snapshot.propagationSummary != m_propagationSummary;
    bool const rosterRulesDataChanged = snapshot.rosterRules != m_rosterRules;
    bool const rosterMatricesDataChanged =
        snapshot.rosterWantedMatrix != m_rosterWantedMatrix
        || snapshot.rosterExceptionMatrix != m_rosterExceptionMatrix;
    bool const statisticsDataChanged = snapshot.statistics != m_statistics;
    bool const filtersDataChanged = snapshot.bands != m_availableBands
        || snapshot.modes != m_availableModes
        || snapshot.continents != m_availableContinents
        || snapshot.dxcc != m_availableDxcc
        || snapshot.sources != m_availableSources;

    m_rawCoverage = std::move(snapshot.coverage);
    m_roster = std::move(snapshot.roster);
    m_rosterPreferences = std::move(snapshot.rosterPreferences);
    m_awards = std::move(snapshot.awards);
    m_awardMissing = std::move(snapshot.awardMissing);
    m_alerts = std::move(snapshot.alerts);
    m_spotHeatmap = std::move(snapshot.spotHeatmap);
    m_spotTimeline = std::move(snapshot.spotTimeline);
    m_spotPaths = std::move(snapshot.spotPaths);
    m_bandActivity = std::move(snapshot.bandActivity);
    m_bandActivityTimeline = std::move(snapshot.bandActivityTimeline);
    m_bandActivitySummary = std::move(snapshot.bandActivitySummary);
    m_propagationStatistics = std::move(snapshot.propagationStatistics);
    m_propagationSummary = std::move(snapshot.propagationSummary);
    m_rosterRules = std::move(snapshot.rosterRules);
    m_rosterWantedMatrix = std::move(snapshot.rosterWantedMatrix);
    m_rosterExceptionMatrix = std::move(snapshot.rosterExceptionMatrix);
    m_statistics = std::move(snapshot.statistics);
    m_availableBands = std::move(snapshot.bands);
    m_availableModes = std::move(snapshot.modes);
    m_availableContinents = std::move(snapshot.continents);
    m_availableDxcc = std::move(snapshot.dxcc);
    m_availableSources = std::move(snapshot.sources);
    m_qsoCount = snapshot.qsoCount;
    m_workedGridCount = snapshot.workedGridCount;
    m_confirmedGridCount = snapshot.confirmedGridCount;
    m_activeGridCount = snapshot.activeGridCount;
    m_missingGridCount = snapshot.missingGridCount;
    m_liveSpotCount = snapshot.liveSpotCount;
    m_rosterWantedCount = snapshot.rosterWantedCount;
    m_rosterNewCount = snapshot.rosterNewCount;
    m_rosterUnconfirmedCount = snapshot.rosterUnconfirmedCount;
    m_unreadAlertCount = snapshot.unreadAlertCount;

    if (!m_availableBands.contains(m_bandFilter, Qt::CaseInsensitive)) {
        m_bandFilter = QStringLiteral("All");
        emit bandFilterChanged();
    }
    if (!m_availableModes.contains(m_modeFilter, Qt::CaseInsensitive)) {
        m_modeFilter = QStringLiteral("All");
        emit modeFilterChanged();
    }
    if (!m_availableContinents.contains(m_continentFilter, Qt::CaseInsensitive)) {
        m_continentFilter = QStringLiteral("All");
        emit continentFilterChanged();
    }
    if (!m_availableDxcc.contains(m_dxccFilter, Qt::CaseInsensitive)) {
        m_dxccFilter = QStringLiteral("All");
        emit dxccFilterChanged();
    }
    if (!m_availableSources.contains(m_sourceFilter, Qt::CaseInsensitive)) {
        m_sourceFilter = QStringLiteral("All");
        emit sourceFilterChanged();
    }
    if (!availablePropagationModes().contains(m_propagationFilter, Qt::CaseInsensitive)) {
        m_propagationFilter = QStringLiteral("MIXED");
        emit propagationFilterChanged();
    }

    m_layerModel->setCount(QStringLiteral("live"), m_liveSpotCount);
    m_layerModel->setCount(QStringLiteral("worked"), m_workedGridCount);
    m_layerModel->setCount(QStringLiteral("confirmed"), m_confirmedGridCount);
    m_layerModel->setCount(QStringLiteral("active"), m_activeGridCount);
    m_layerModel->setCount(QStringLiteral("missing"), m_missingGridCount);
    m_layerModel->setCount(QStringLiteral("psk"), snapshot.pskListenerCount);
    int notificationFlags = SnapshotNotifyNone;
    if (filtersDataChanged) notificationFlags |= SnapshotNotifyFilters;
    if (coverageDataChanged) notificationFlags |= SnapshotNotifyCoverage;
    if (rosterDataChanged) notificationFlags |= SnapshotNotifyRoster;
    if (rosterPreferencesDataChanged) notificationFlags |= SnapshotNotifyPreferences;
    if (awardsDataChanged) notificationFlags |= SnapshotNotifyAwards;
    if (alertsDataChanged) notificationFlags |= SnapshotNotifyAlerts;
    if (spotAnalyticsDataChanged) notificationFlags |= SnapshotNotifySpotAnalytics;
    if (bandActivityDataChanged) notificationFlags |= SnapshotNotifyBandActivity;
    if (propagationDataChanged) notificationFlags |= SnapshotNotifyPropagation;
    if (rosterRulesDataChanged) notificationFlags |= SnapshotNotifyRules;
    if (rosterMatricesDataChanged) notificationFlags |= SnapshotNotifyMatrices;
    if (statisticsDataChanged) notificationFlags |= SnapshotNotifyStatistics;
    queueSnapshotNotifications(notificationFlags);

    qInfo().noquote()
        << QStringLiteral("[MAPINT] snapshot qso=%1 worked=%2 confirmed=%3 active=%4 missing=%5 live=%6 roster=%7 wanted=%8 alerts=%9 db=%10")
               .arg(m_qsoCount)
               .arg(m_workedGridCount)
               .arg(m_confirmedGridCount)
               .arg(m_activeGridCount)
               .arg(m_missingGridCount)
               .arg(m_liveSpotCount)
               .arg(m_roster.size())
               .arg(m_rosterWantedCount)
               .arg(m_unreadAlertCount)
               .arg(m_databasePath);
}

void MapIntelligenceService::applyGridDetails(quint64 generation,
                                              const QString& grid,
                                              GridDetails details)
{
    if (generation != m_gridDetailsGeneration.load()
        || m_selectedGrid.compare(grid, Qt::CaseInsensitive) != 0) {
        return;
    }
    if (!details.error.isEmpty()) {
        qWarning().noquote()
            << "[MAPINT] grid details query failed grid=" << grid
            << "error=" << details.error;
    }
    m_selectedGridSummary = std::move(details.summary);
    m_selectedGridLive = std::move(details.live);
    m_selectedGridQsos = std::move(details.qsos);
    setGridDetailsLoading(false);
    emit gridDetailsChanged();
}

void MapIntelligenceService::rebuildVisibleCoverage()
{
    QVariantList visible;
    bool const showWorked = workedLayerEnabled();
    bool const showConfirmed = confirmedLayerEnabled();
    // Coverage overlays are independent map layers. Requiring the generic
    // live-station layer here made Active/Missing/PSK counters non-zero while
    // rendering no cells when LIVE was toggled off.
    bool const showActive = activeLayerEnabled();
    bool const showMissing = missingLayerEnabled();
    bool const showPsk = pskLayerEnabled();
    visible.reserve(m_rawCoverage.size());
    for (QVariant const& value : std::as_const(m_rawCoverage)) {
        QVariantMap row = value.toMap();
        bool const confirmed = row.value(QStringLiteral("confirmed")).toBool();
        bool const worked = row.value(QStringLiteral("workedCount")).toInt() > 0;
        bool const active = row.value(QStringLiteral("active")).toBool();
        bool const missing = row.value(QStringLiteral("missing")).toBool();
        bool const psk = row.value(QStringLiteral("psk")).toBool();
        if ((!worked || !showWorked)
            && (!confirmed || !showConfirmed)
            && (!active || !showActive)
            && (!missing || !showMissing)
            && (!psk || !showPsk)) {
            continue;
        }
        row.insert(QStringLiteral("confirmed"), confirmed && showConfirmed);
        row.insert(QStringLiteral("worked"), worked && showWorked);
        row.insert(QStringLiteral("active"), active && showActive);
        row.insert(QStringLiteral("missing"), missing && showMissing);
        row.insert(QStringLiteral("psk"), psk && showPsk);
        visible.append(row);
    }
    m_coverageCells = std::move(visible);
    emit coverageChanged();
}

void MapIntelligenceService::setGridDetailsLoading(bool loading)
{
    if (m_gridDetailsLoading == loading) {
        return;
    }
    m_gridDetailsLoading = loading;
    emit gridDetailsLoadingChanged();
}

void MapIntelligenceService::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}

void MapIntelligenceService::saveSetting(const QString& key, const QVariant& value) const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("LiveMapLayers"));
    settings.setValue(key, value);
    settings.endGroup();
    settings.sync();
}
