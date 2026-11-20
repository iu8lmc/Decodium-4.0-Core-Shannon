#include "DecodiumBridge.h"
#include "Radio.hpp"
#include "models/Bands.hpp"
#include "models/FrequencyList.hpp"
#include "models/IARURegions.hpp"
#include "models/Modes.hpp"
#include <QCoreApplication>
#include <QBuffer>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

extern void decodiumBridgeLogbookLog(const QString&);
extern QString decodiumBridgeCanonicalApplicationDecodeMode(QString);
extern bool decodiumBridgeIsRadioOnlyModeLabel(QString);

namespace
{
using WorkingFrequencyItem = FrequencyList_v2_101::Item;
using WorkingFrequencyItems = FrequencyList_v2_101::FrequencyItems;
constexpr quint32 kQrgMagic = 0xadbccbdb;
constexpr quint32 kQrgVersion = 101;
constexpr quint32 kQrgVersion100 = 100;
constexpr qint64 kMaxWorkingFrequencyImportBytes = 4 * 1024 * 1024;
constexpr quint64 kMaxWorkingFrequencyImportRows = 4096;

WorkingFrequencyItems workingFrequencyItemsFromSetting(const QVariant& value)
{
    if (!value.isValid()) {
        return {};
    }
    return value.value<WorkingFrequencyItems>();
}

void sortWorkingFrequencyItems(WorkingFrequencyItems& items)
{
    std::stable_sort(items.begin(), items.end(), [](WorkingFrequencyItem const& lhs,
                                                    WorkingFrequencyItem const& rhs) {
        if (lhs.frequency_ != rhs.frequency_) return lhs.frequency_ < rhs.frequency_;
        if (lhs.region_ != rhs.region_) return lhs.region_ < rhs.region_;
        return lhs.mode_ < rhs.mode_;
    });
}

void ensureDefaultWorkingFrequencies(WorkingFrequencyItems& items)
{
    Bands bands;
    FrequencyList_v2_101 defaults {&bands};
    defaults.reset_to_defaults();
    if (items.isEmpty()) {
        items = defaults.frequency_list();
    } else {
        auto hasEquivalentRow = [&bands, &items](WorkingFrequencyItem const& candidate) {
            QString const candidateBand = bands.find(candidate.frequency_);
            for (WorkingFrequencyItem const& item : std::as_const(items)) {
                if (item.mode_ != candidate.mode_ || item.region_ != candidate.region_) {
                    continue;
                }
                QString const itemBand = bands.find(item.frequency_);
                if (!candidateBand.isEmpty() && !itemBand.isEmpty()) {
                    if (itemBand == candidateBand) {
                        return true;
                    }
                    continue;
                }
                if (item.frequency_ == candidate.frequency_) {
                    return true;
                }
            }
            return false;
        };
        for (WorkingFrequencyItem const& item : defaults.frequency_list()) {
            if (!hasEquivalentRow(item)) {
                items << item;
            }
        }
    }
    sortWorkingFrequencyItems(items);
}

QDateTime parseFrequencyDateTime(QString value, bool* ok)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        if (ok) *ok = true;
        return {};
    }
    QDateTime dt = QDateTime::fromString(value, Qt::ISODate);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm"));
    }
    if (!dt.isValid()) {
        dt = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd hh:mm"));
    }
    if (ok) *ok = dt.isValid();
    return dt;
}

Radio::Frequency parseFrequencyHzValue(const QVariant& value, bool* ok)
{
    QString text = value.toString().trimmed();
    text.replace(QLatin1Char(','), QLatin1Char('.'));
    bool explicitHz = text.contains(QStringLiteral("hz"), Qt::CaseInsensitive)
                      && !text.contains(QStringLiteral("mhz"), Qt::CaseInsensitive);
    bool explicitMHz = text.contains(QStringLiteral("mhz"), Qt::CaseInsensitive);
    static QRegularExpression const numberRe(QStringLiteral("[-+]?\\d+(?:\\.\\d+)?"));
    QRegularExpressionMatch const match = numberRe.match(text);
    if (!match.hasMatch()) {
        if (ok) *ok = false;
        return 0;
    }
    bool numberOk = false;
    double numeric = match.captured(0).toDouble(&numberOk);
    if (!numberOk || numeric <= 0.0) {
        if (ok) *ok = false;
        return 0;
    }
    double hz = 0.0;
    if (explicitMHz || (!explicitHz && numeric < 1000000.0)) {
        hz = numeric * 1000000.0;
    } else {
        hz = numeric;
    }
    if (hz <= 0.0 || hz > static_cast<double>(std::numeric_limits<Radio::Frequency>::max())) {
        if (ok) *ok = false;
        return 0;
    }
    if (ok) *ok = true;
    return static_cast<Radio::Frequency>(std::llround(hz));
}

Radio::FrequencyDelta parseFrequencyOffsetHzValue(const QVariant& value, bool* ok)
{
    QString text = value.toString().trimmed();
    text.replace(QLatin1Char(','), QLatin1Char('.'));
    // Copy/paste from formatted documents and some mobile keyboards produces
    // a typographic minus rather than ASCII '-'.  Treat the common variants
    // as a sign instead of accidentally parsing the following digits as a
    // positive offset.
    static QString const unicodeMinusCharacters =
        QStringLiteral("\u2212\u2012\u2013\u2014\uFE63\uFF0D");
    for (QChar const minus : unicodeMinusCharacters) {
        text.replace(minus, QLatin1Char('-'));
    }
    if (text.isEmpty()) {
        if (ok) *ok = true;
        return 0;
    }
    static QRegularExpression const numberRe {
        QStringLiteral("^\\s*([-+]?(?:\\d+(?:\\.\\d*)?|\\.\\d+))\\s*(MHz|Hz)?\\s*$"),
        QRegularExpression::CaseInsensitiveOption
    };
    QRegularExpressionMatch const match = numberRe.match(text);
    if (!match.hasMatch()) {
        if (ok) *ok = false;
        return 0;
    }
    bool numberOk = false;
    double numeric = match.captured(1).toDouble(&numberOk);
    if (!numberOk) {
        if (ok) *ok = false;
        return 0;
    }
    QString const unit = match.captured(2);
    bool const explicitMHz = unit.compare(QStringLiteral("MHz"), Qt::CaseInsensitive) == 0;
    bool const explicitHz = unit.compare(QStringLiteral("Hz"), Qt::CaseInsensitive) == 0;
    double hz = 0.0;
    if (explicitMHz || (!explicitHz && std::abs(numeric) < 1000000.0)) {
        hz = numeric * 1000000.0;
    } else {
        hz = numeric;
    }
    if (!std::isfinite(hz)
        || hz < static_cast<double>(std::numeric_limits<Radio::FrequencyDelta>::min())
        || hz > static_cast<double>(std::numeric_limits<Radio::FrequencyDelta>::max())) {
        if (ok) *ok = false;
        return 0;
    }
    if (ok) *ok = true;
    return static_cast<Radio::FrequencyDelta>(std::llround(hz));
}

void enforcePreferredWorkingFrequency(WorkingFrequencyItems& items, int preferredIndex)
{
    if (preferredIndex < 0 || preferredIndex >= items.size() || !items[preferredIndex].preferred_) {
        return;
    }
    Bands bands;
    QString const band = bands.find(items[preferredIndex].frequency_);
    if (band.isEmpty()) {
        return;
    }
    for (int i = 0; i < items.size(); ++i) {
        if (i == preferredIndex) {
            continue;
        }
        if (items[i].mode_ == items[preferredIndex].mode_
            && items[i].region_ == items[preferredIndex].region_
            && bands.find(items[i].frequency_) == band) {
            items[i].preferred_ = false;
        }
    }
}

WorkingFrequencyItem workingFrequencyItemFromFields(const QString& region,
                                                    const QString& mode,
                                                    const QVariant& frequency,
                                                    const QString& description,
                                                    const QString& startTime,
                                                    const QString& endTime,
                                                    bool preferred,
                                                    bool* ok,
                                                    QString* error)
{
    bool frequencyOk = false;
    Radio::Frequency const frequencyHz = parseFrequencyHzValue(frequency, &frequencyOk);
    bool startOk = false;
    bool endOk = false;
    QDateTime const start = parseFrequencyDateTime(startTime, &startOk);
    QDateTime const end = parseFrequencyDateTime(endTime, &endOk);
    WorkingFrequencyItem item {
        frequencyHz,
        Modes::value(mode.trimmed()),
        IARURegions::value(region.trimmed()),
        description.trimmed(),
        QString(),
        start,
        end,
        preferred
    };
    bool const sane = frequencyOk && startOk && endOk && item.isSane();
    if (ok) {
        *ok = sane;
    }
    if (!sane && error) {
        if (!frequencyOk) {
            *error = QObject::tr("Invalid frequency");
        } else if (!startOk || !endOk) {
            *error = QObject::tr("Invalid date/time. Use ISO format or yyyy-MM-dd HH:mm.");
        } else {
            *error = QObject::tr("Invalid frequency row");
        }
    }
    return item;
}

QString frequencyDisplayString(Radio::Frequency frequencyHz)
{
    Bands bands;
    QString const band = bands.find(frequencyHz);
    return Radio::pretty_frequency_MHz_string(frequencyHz)
        + QStringLiteral(" MHz (")
        + (band.isEmpty() ? QStringLiteral("OOB") : band)
        + QLatin1Char(')');
}

bool readQtSerializedListCount(QIODevice& device, QDataStream const& stream, quint64* count)
{
    if (!count) {
        return false;
    }
    QByteArray prefix = device.peek(12);
    if (prefix.size() < 4) {
        return false;
    }
    QBuffer buffer(&prefix);
    buffer.open(QIODevice::ReadOnly);
    QDataStream probe(&buffer);
    probe.setVersion(stream.version());
    quint32 count32 = 0;
    probe >> count32;
    if (probe.status() != QDataStream::Ok) {
        return false;
    }
    if (count32 == 0xfffffffeu) {
        if (prefix.size() < 12) {
            return false;
        }
        quint64 count64 = 0;
        probe >> count64;
        if (probe.status() != QDataStream::Ok) {
            return false;
        }
        *count = count64;
        return true;
    }
    if (count32 == 0xffffffffu) {
        return false;
    }
    *count = count32;
    return true;
}

WorkingFrequencyItems readWorkingFrequenciesJsonFile(QFile& file, QString* error)
{
    QJsonParseError parseError{};
    QJsonDocument const doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QObject::tr("Failed to parse JSON frequencies file: %1")
                         .arg(parseError.errorString());
        }
        return {};
    }
    QJsonArray const arr = doc.object().value(QStringLiteral("frequencies")).toArray();
    if (arr.isEmpty()) {
        if (error) {
            *error = QObject::tr("No frequencies were found");
        }
        return {};
    }
    if (static_cast<quint64>(arr.size()) > kMaxWorkingFrequencyImportRows) {
        if (error) {
            *error = QObject::tr("Frequency file contains too many rows (%1, limit %2)")
                         .arg(arr.size())
                         .arg(kMaxWorkingFrequencyImportRows);
        }
        return {};
    }

    WorkingFrequencyItems list;
    list.reserve(arr.size());
    for (QJsonValue const& value : arr) {
        QJsonObject const obj = value.toObject();
        QString const region = obj.value(QStringLiteral("region")).toString();
        QString const mode = obj.value(QStringLiteral("mode")).toString();
        WorkingFrequencyItem item;
        item.frequency_ = static_cast<Radio::Frequency>(
            obj.value(QStringLiteral("frequency")).toString().toDouble() * 1000000.0);
        item.region_ = IARURegions::value(region);
        item.mode_ = Modes::value(mode);
        item.description_ = obj.value(QStringLiteral("description")).toString();
        item.source_ = obj.value(QStringLiteral("source")).toString();
        item.start_time_ = QDateTime::fromString(obj.value(QStringLiteral("start_time")).toString(),
                                                 Qt::ISODate);
        item.end_time_ = QDateTime::fromString(obj.value(QStringLiteral("end_time")).toString(),
                                               Qt::ISODate);
        item.preferred_ = obj.value(QStringLiteral("preferred")).toBool();
        if ((item.mode_ != Modes::ALL || QString::compare(QStringLiteral("ALL"), mode))
            && (item.region_ != IARURegions::ALL
                || QString::compare(QStringLiteral("ALL"), region, Qt::CaseInsensitive))
            && item.isSane()) {
            list.push_back(item);
        }
    }
    sortWorkingFrequencyItems(list);
    return list;
}

WorkingFrequencyItems readWorkingFrequenciesFile(const QString& path, QString* error)
{
    QFileInfo const info(path);
    if (!info.exists() || !info.isFile()) {
        if (error) {
            *error = QObject::tr("Frequency file does not exist");
        }
        return {};
    }
    if (info.size() > kMaxWorkingFrequencyImportBytes) {
        if (error) {
            *error = QObject::tr("Frequency file is too large (%1 bytes, limit %2 bytes)")
                         .arg(info.size())
                         .arg(kMaxWorkingFrequencyImportBytes);
        }
        return {};
    }

    QFile file {path};
    if (!file.open(QFile::ReadOnly)) {
        if (error) {
            *error = QObject::tr("Cannot open frequency file: %1").arg(file.errorString());
        }
        return {};
    }

    if (path.endsWith(QStringLiteral(".qrg.json"), Qt::CaseInsensitive)) {
        WorkingFrequencyItems items = readWorkingFrequenciesJsonFile(file, error);
        if (items.isEmpty()) {
            if (error) {
                if (error->isEmpty()) {
                    *error = QObject::tr("No valid working frequencies found");
                }
            }
            return {};
        }
        return items;
    }

    QDataStream in {&file};
    quint32 magic = 0;
    quint32 version = 0;
    in >> magic;
    if (magic != kQrgMagic) {
        if (error) {
            *error = QObject::tr("Not a valid frequencies file: incorrect file magic");
        }
        return {};
    }
    in >> version;
    if (version > kQrgVersion) {
        if (error) {
            *error = QObject::tr("Not a valid frequencies file: version is too new");
        }
        return {};
    }

    quint64 serializedRows = 0;
    if (!readQtSerializedListCount(file, in, &serializedRows)) {
        if (error) {
            *error = QObject::tr("Not a valid frequencies file: cannot read row count");
        }
        return {};
    }
    if (serializedRows > kMaxWorkingFrequencyImportRows) {
        if (error) {
            *error = QObject::tr("Frequency file contains too many rows (%1, limit %2)")
                         .arg(serializedRows)
                         .arg(kMaxWorkingFrequencyImportRows);
        }
        return {};
    }

    WorkingFrequencyItems items;
    if (version == kQrgVersion100) {
        FrequencyList_v2::FrequencyItems oldItems;
        in >> oldItems;
        for (auto const& item : std::as_const(oldItems)) {
            items << WorkingFrequencyItem {item.frequency_, item.mode_, item.region_,
                                           QString(), QString(), QDateTime(), QDateTime(), false};
        }
    } else {
        in >> items;
    }

    if (in.status() != QDataStream::Ok || !in.atEnd()) {
        if (error) {
            *error = QObject::tr("Not a valid frequencies file: contents corrupt");
        }
        return {};
    }
    if (static_cast<quint64>(items.size()) > kMaxWorkingFrequencyImportRows) {
        if (error) {
            *error = QObject::tr("Frequency file contains too many rows (%1, limit %2)")
                         .arg(items.size())
                         .arg(kMaxWorkingFrequencyImportRows);
        }
        return {};
    }

    WorkingFrequencyItems saneItems;
    for (WorkingFrequencyItem const& item : std::as_const(items)) {
        if (item.isSane()) {
            saneItems << item;
        }
    }
    sortWorkingFrequencyItems(saneItems);
    return saneItems;
}

}

Radio::FrequencyDelta decodiumBridgeParseFrequencyOffsetHzValue(const QVariant& value, bool* ok)
{
    return parseFrequencyOffsetHzValue(value, ok);
}

Radio::Frequency decodiumBridgeParseFrequencyHzValue(const QVariant& value, bool* ok)
{
    return parseFrequencyHzValue(value, ok);
}

QVariantList DecodiumBridge::workingFrequencyRows() const
{
    WorkingFrequencyItems items =
        workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2")));
    if (items.isEmpty()) {
        items = workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes")));
    }
    ensureDefaultWorkingFrequencies(items);

    QVariantList rows;
    rows.reserve(items.size());
    for (int i = 0; i < items.size(); ++i) {
        WorkingFrequencyItem const& item = items.at(i);
        Bands bands;
        QString const band = bands.find(item.frequency_);
        QVariantMap row;
        row.insert(QStringLiteral("index"), i);
        row.insert(QStringLiteral("region"), IARURegions::name(item.region_));
        row.insert(QStringLiteral("mode"), Modes::name(item.mode_));
        row.insert(QStringLiteral("frequencyHz"), QVariant::fromValue<qlonglong>(item.frequency_));
        row.insert(QStringLiteral("frequencyMHz"),
                   QString::number(static_cast<double>(item.frequency_) / 1000000.0, 'f', 6));
        row.insert(QStringLiteral("frequency"), frequencyDisplayString(item.frequency_));
        row.insert(QStringLiteral("band"), band.isEmpty() ? QStringLiteral("OOB") : band);
        row.insert(QStringLiteral("preferred"), item.preferred_);
        row.insert(QStringLiteral("description"), item.description_);
        row.insert(QStringLiteral("startTime"), item.start_time_.isValid() && !item.start_time_.isNull()
                   ? item.start_time_.toString(Qt::ISODate) : QString());
        row.insert(QStringLiteral("endTime"), item.end_time_.isValid() && !item.end_time_.isNull()
                   ? item.end_time_.toString(Qt::ISODate) : QString());
        rows << row;
    }
    return rows;
}

QStringList DecodiumBridge::frequencyRegionOptions() const
{
    QStringList regions;
    for (int i = 0; i < IARURegions::SENTINAL; ++i) {
        regions << QString::fromLatin1(IARURegions::name(static_cast<IARURegions::Region>(i)));
    }
    return regions;
}

QStringList DecodiumBridge::frequencyModeOptions() const
{
    QStringList modes;
    for (int i = 0; i < Modes::MODES_END_SENTINAL_AND_COUNT; ++i) {
        modes << QString::fromLatin1(Modes::name(static_cast<Modes::Mode>(i)));
    }
    return modes;
}

QStringList DecodiumBridge::frequencyBandOptions() const
{
    Bands bands;
    QStringList names;
    for (QString const& band : bands) {
        names << band;
    }
    return names;
}

double DecodiumBridge::workingFrequencyForBandMode(const QString& bandLambda, const QString& mode) const
{
    QString const targetBand = bandLambda.trimmed();
    if (targetBand.isEmpty()) {
        return 0.0;
    }

    QString targetMode = decodiumBridgeCanonicalApplicationDecodeMode(mode).trimmed().toUpper();
    if (targetMode.isEmpty()) {
        targetMode = mode.trimmed().toUpper();
    }
    if (targetMode.isEmpty()) {
        return 0.0;
    }
    if (targetMode.startsWith(QStringLiteral("Q65-"))) {
        targetMode = QStringLiteral("Q65");
    }

    WorkingFrequencyItems items =
        workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2")));
    if (items.isEmpty()) {
        items = workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes")));
    }
    ensureDefaultWorkingFrequencies(items);

    Bands bands;
    auto itemBandAndMode = [&](WorkingFrequencyItem const& item,
                               QString* itemModeOut = nullptr) {
        QString const itemBand = bands.find(item.frequency_);
        QString const itemMode = QString::fromLatin1(Modes::name(item.mode_)).trimmed().toUpper();
        if (itemModeOut) {
            *itemModeOut = itemMode;
        }
        return item.frequency_ > 0
            && itemBand.compare(targetBand, Qt::CaseInsensitive) == 0;
    };

    Radio::Frequency preferredAllMode = 0;
    Radio::Frequency firstExactAllRegion = 0;
    Radio::Frequency firstExactAnyRegion = 0;
    for (WorkingFrequencyItem const& item : std::as_const(items)) {
        QString itemMode;
        if (!itemBandAndMode(item, &itemMode)) {
            continue;
        }
        if (itemMode == targetMode && item.preferred_) {
            return static_cast<double>(item.frequency_);
        }
        // Satellite/cross-band entries are intentionally stored as mode ALL.
        // When the operator marks one preferred it becomes the quick band QSY
        // for the currently selected digital mode too.
        if (itemMode == QStringLiteral("ALL") && item.preferred_ && preferredAllMode <= 0) {
            preferredAllMode = item.frequency_;
        }
        if (itemMode == targetMode) {
            if (item.region_ == IARURegions::ALL && firstExactAllRegion <= 0) {
                firstExactAllRegion = item.frequency_;
            }
            if (firstExactAnyRegion <= 0) {
                firstExactAnyRegion = item.frequency_;
            }
        }
    }

    Radio::Frequency const fallback = preferredAllMode > 0
        ? preferredAllMode
        : (firstExactAllRegion > 0 ? firstExactAllRegion : firstExactAnyRegion);
    return fallback > 0 ? static_cast<double>(fallback) : 0.0;
}

bool DecodiumBridge::addWorkingFrequencyRow(const QString& region,
                                            const QString& mode,
                                            const QVariant& frequency,
                                            const QString& description,
                                            const QString& startTime,
                                            const QString& endTime,
                                            bool preferred)
{
    WorkingFrequencyItems items =
        workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2")));
    if (items.isEmpty()) {
        items = workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes")));
    }
    ensureDefaultWorkingFrequencies(items);
    bool ok = false;
    QString error;
    WorkingFrequencyItem item = workingFrequencyItemFromFields(region, mode, frequency, description,
                                                               startTime, endTime, preferred, &ok, &error);
    if (!ok) {
        emit errorMessage(error);
        return false;
    }
    items << item;
    if (item.preferred_) {
        enforcePreferredWorkingFrequency(items, items.size() - 1);
    }
    sortWorkingFrequencyItems(items);
    syncSettingToLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    emit settingValueChanged(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    emit statusMessage(QStringLiteral("Working frequency added: %1 %2").arg(Modes::name(item.mode_), frequencyDisplayString(item.frequency_)));
    return true;
}

bool DecodiumBridge::updateWorkingFrequencyRow(int index,
                                               const QString& region,
                                               const QString& mode,
                                               const QVariant& frequency,
                                               const QString& description,
                                               const QString& startTime,
                                               const QString& endTime,
                                               bool preferred)
{
    WorkingFrequencyItems items =
        workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2")));
    if (items.isEmpty()) {
        items = workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes")));
    }
    ensureDefaultWorkingFrequencies(items);
    if (index < 0 || index >= items.size()) {
        emit errorMessage(QStringLiteral("Invalid working frequency selection"));
        return false;
    }
    bool ok = false;
    QString error;
    WorkingFrequencyItem item = workingFrequencyItemFromFields(region, mode, frequency, description,
                                                               startTime, endTime, preferred, &ok, &error);
    if (!ok) {
        emit errorMessage(error);
        return false;
    }
    items[index] = item;
    if (item.preferred_) {
        enforcePreferredWorkingFrequency(items, index);
    }
    sortWorkingFrequencyItems(items);
    syncSettingToLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    emit settingValueChanged(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    emit statusMessage(QStringLiteral("Working frequency updated: %1 %2").arg(Modes::name(item.mode_), frequencyDisplayString(item.frequency_)));
    return true;
}

bool DecodiumBridge::deleteWorkingFrequencyRow(int index)
{
    WorkingFrequencyItems items =
        workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2")));
    if (items.isEmpty()) {
        items = workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes")));
    }
    ensureDefaultWorkingFrequencies(items);
    if (index < 0 || index >= items.size()) {
        emit errorMessage(QStringLiteral("Invalid working frequency selection"));
        return false;
    }
    WorkingFrequencyItem const removed = items.takeAt(index);
    syncSettingToLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    emit settingValueChanged(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    emit statusMessage(QStringLiteral("Working frequency deleted: %1 %2").arg(Modes::name(removed.mode_), frequencyDisplayString(removed.frequency_)));
    return true;
}

bool DecodiumBridge::setWorkingFrequencyPreferred(int index, bool preferred)
{
    WorkingFrequencyItems items =
        workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2")));
    if (items.isEmpty()) {
        items = workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes")));
    }
    ensureDefaultWorkingFrequencies(items);
    if (index < 0 || index >= items.size()) {
        emit errorMessage(QStringLiteral("Invalid working frequency selection"));
        return false;
    }
    items[index].preferred_ = preferred;
    if (preferred) {
        enforcePreferredWorkingFrequency(items, index);
    }
    sortWorkingFrequencyItems(items);
    syncSettingToLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    emit settingValueChanged(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    return true;
}

void DecodiumBridge::resetWorkingFrequenciesToDefaults()
{
    Bands bands;
    FrequencyList_v2_101 defaults {&bands};
    defaults.reset_to_defaults();
    WorkingFrequencyItems items = defaults.frequency_list();
    sortWorkingFrequencyItems(items);
    syncSettingToLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    emit settingValueChanged(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    emit statusMessage(QStringLiteral("Working frequencies reset to defaults"));
}

bool DecodiumBridge::loadWorkingFrequenciesFile(const QString& path, bool merge)
{
    QString error;
    WorkingFrequencyItems loaded = readWorkingFrequenciesFile(path, &error);
    if (loaded.isEmpty()) {
        emit errorMessage(error.isEmpty() ? QStringLiteral("No valid working frequencies found") : error);
        return false;
    }

    WorkingFrequencyItems items;
    if (merge) {
        items = workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2")));
        if (items.isEmpty()) {
            items = workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes")));
        }
        ensureDefaultWorkingFrequencies(items);
        Bands bands;
        FrequencyList_v2_101 model {&bands};
        model.frequency_list(items);
        model.frequency_list_merge(loaded);
        items = model.frequency_list();
    } else {
        items = loaded;
    }

    if (static_cast<quint64>(items.size()) > kMaxWorkingFrequencyImportRows) {
        emit errorMessage(QStringLiteral("Working frequency list too large after import (%1 rows, limit %2)")
                              .arg(items.size())
                              .arg(kMaxWorkingFrequencyImportRows));
        return false;
    }

    sortWorkingFrequencyItems(items);
    syncSettingToLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    emit settingValueChanged(QStringLiteral("FrequenciesForRegionModes_v2"), QVariant::fromValue(items));
    emit statusMessage(merge
                       ? QStringLiteral("Working frequencies merged from %1").arg(QFileInfo(path).fileName())
                       : QStringLiteral("Working frequencies loaded from %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool DecodiumBridge::saveWorkingFrequenciesFile(const QString& path)
{
    WorkingFrequencyItems items =
        workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes_v2")));
    if (items.isEmpty()) {
        items = workingFrequencyItemsFromSetting(readSettingFromLegacyIni(QStringLiteral("FrequenciesForRegionModes")));
    }
    ensureDefaultWorkingFrequencies(items);

    QFile file {path};
    if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
        emit errorMessage(QStringLiteral("Cannot save frequency file: %1").arg(file.errorString()));
        return false;
    }

    if (path.endsWith(QStringLiteral(".qrg.json"), Qt::CaseInsensitive)) {
        Bands bands;
        FrequencyList_v2_101 model {&bands};
        model.to_json_file(&file,
                           QStringLiteral("0x%1").arg(QString::number(kQrgMagic, 16).toUpper()),
                           QStringLiteral("0x%1").arg(QString::number(kQrgVersion, 16).toUpper()),
                           items);
    } else {
        QDataStream out {&file};
        out << kQrgMagic << kQrgVersion << items;
        if (out.status() != QDataStream::Ok) {
            emit errorMessage(QStringLiteral("Cannot save frequency file: write failed"));
            return false;
        }
    }

    emit statusMessage(QStringLiteral("Working frequencies saved to %1").arg(QFileInfo(path).fileName()));
    return true;
}

// 1.0.195 — QSY rapido a un preset Working Frequencies. Risolve l'index nella
// lista corrente (rispetta filtri/sorting di workingFrequencyRows), legge
// frequency_ + mode_ e comanda il normale percorso QSY bridge/CAT. Log audit.
void DecodiumBridge::qsyToWorkingFrequency(int index)
{
    QVariantList const rows = workingFrequencyRows();
    if (index < 0 || index >= rows.size()) {
        decodiumBridgeLogbookLog(QStringLiteral("[QSY-Preset] invalid index=%1 (rows=%2)")
                      .arg(index).arg(rows.size()));
        return;
    }
    QVariantMap const entry = rows.at(index).toMap();
    qlonglong const freqHz = entry.value(QStringLiteral("frequencyHz")).toLongLong();
    QString const requestedMode = entry.value(QStringLiteral("mode")).toString().trimmed();
    QString mode = decodiumBridgeCanonicalApplicationDecodeMode(requestedMode);
    QString const band = entry.value(QStringLiteral("band")).toString();
    QString const desc = entry.value(QStringLiteral("description")).toString();
    if (freqHz <= 0) {
        decodiumBridgeLogbookLog(QStringLiteral("[QSY-Preset] skip: invalid frequency in entry idx=%1").arg(index));
        return;
    }
    if (!requestedMode.isEmpty() && mode.isEmpty()) {
        QString const detectedMode = startupModeForFrequency(static_cast<double>(freqHz));
        if (!detectedMode.isEmpty()) {
            mode = detectedMode;
            decodiumBridgeLogbookLog(QStringLiteral("[QSY-Preset] remapped non-decode mode %1 to %2 from %3 Hz")
                          .arg(requestedMode, mode, QString::number(freqHz)));
        } else if (decodiumBridgeIsRadioOnlyModeLabel(requestedMode)) {
            decodiumBridgeLogbookLog(QStringLiteral("[QSY-Preset] preserving current mode %1 for radio/CAT preset mode %2 at %3 Hz")
                          .arg(m_mode, requestedMode, QString::number(freqHz)));
        } else {
            decodiumBridgeLogbookLog(QStringLiteral("[QSY-Preset] preserving current mode %1 for unsupported preset mode %2 at %3 Hz")
                          .arg(m_mode, requestedMode, QString::number(freqHz)));
        }
    }
    decodiumBridgeLogbookLog(QStringLiteral("[QSY-Preset] QSY idx=%1 → %2 Hz mode=%3 band=%4 desc=%5")
                  .arg(index)
                  .arg(freqHz)
                  .arg(mode.isEmpty() ? requestedMode : mode)
                  .arg(band)
                  .arg(desc.isEmpty() ? QStringLiteral("-") : desc));
    requestRigFrequencyFromBridge(static_cast<double>(freqHz), QStringLiteral("working-frequency"));
    if (!mode.isEmpty() && mode != m_mode) {
        setMode(mode);
    }
}

