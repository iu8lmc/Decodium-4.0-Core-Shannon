#include "AdifDocumentInternal.h"
#include "AdifExportSanitizer.h"
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace decodium::adif_document {
constexpr int kMaxAdifRecords = 200000;
constexpr int kMaxAdifFieldBytes = 65536;
constexpr int kMaxAdifFieldsPerRecord = 128;

QList<ParsedAdifRecord> parseAdifRecordsBytes(QByteArray const& bodyBytes)
{
    QList<ParsedAdifRecord> records;
    ParsedAdifRecord record;
    int fieldsInRecord = 0;
    int cursor = 0;
    while (cursor < bodyBytes.size()) {
        if (records.size() >= kMaxAdifRecords) {
            qWarning().noquote() << "[ADIF] record cap reached while parsing log:" << kMaxAdifRecords;
            break;
        }

        int const tagStart = bodyBytes.indexOf('<', cursor);
        if (tagStart < 0) {
            break;
        }
        int const tagEnd = bodyBytes.indexOf('>', tagStart + 1);
        if (tagEnd < 0) {
            break;
        }

        QByteArray const spec = bodyBytes.mid(tagStart + 1, tagEnd - tagStart - 1).trimmed();
        QByteArray const specLower = spec.toLower();
        if (specLower == "eor") {
            if (!record.fields.isEmpty()) {
                records.append(record);
                record = ParsedAdifRecord {};
                fieldsInRecord = 0;
            }
            cursor = tagEnd + 1;
            continue;
        }
        if (specLower == "eoh") {
            record = ParsedAdifRecord {};
            fieldsInRecord = 0;
            cursor = tagEnd + 1;
            continue;
        }

        int const lengthSeparator = spec.indexOf(':');
        if (lengthSeparator <= 0) {
            cursor = tagEnd + 1;
            continue;
        }

        QByteArray const fieldNameBytes = spec.left(lengthSeparator).trimmed().toUpper();
        QByteArray lengthBytes = spec.mid(lengthSeparator + 1);
        int const typeSeparator = lengthBytes.indexOf(':');
        if (typeSeparator >= 0) {
            lengthBytes = lengthBytes.left(typeSeparator);
        }
        lengthBytes = lengthBytes.trimmed();

        bool ok = false;
        int const valueLength = lengthBytes.toInt(&ok);
        if (!ok || valueLength < 0) {
            cursor = tagEnd + 1;
            continue;
        }

        int const valueStart = tagEnd + 1;
        if (valueStart > bodyBytes.size()) {
            break;
        }

        int const available = bodyBytes.size() - valueStart;
        if (valueLength > available) {
            qWarning().noquote() << "[ADIF] truncated field discarded at byte" << valueStart;
            break;
        }
        if (valueLength > kMaxAdifFieldBytes) {
            qWarning().noquote() << "[ADIF] oversized field discarded at byte" << valueStart
                                 << "length" << valueLength;
            cursor = valueStart + valueLength;
            continue;
        }
        if (fieldsInRecord >= kMaxAdifFieldsPerRecord) {
            cursor = valueStart + valueLength;
            continue;
        }

        QByteArray const valueBytes = bodyBytes.mid(valueStart, valueLength);
        QString const fieldName = QString::fromLatin1(fieldNameBytes).trimmed().toUpper();
        QString const value = QString::fromUtf8(valueBytes).trimmed();
        if (!fieldName.isEmpty()) {
            record.fields.insert(fieldName, value);
            ++fieldsInRecord;
        }
        cursor = valueStart + valueLength;
    }

    if (!record.fields.isEmpty() && records.size() < kMaxAdifRecords) {
        records.append(record);
    }
    return records;
}

ParsedAdifDocument loadAdifDocument(QString const& path)
{
    ParsedAdifDocument doc;
    QFile file(path);
    if (!file.exists()) {
        doc.header = QStringLiteral("Decodium4 ADIF Log\n<EOH>\n");
        return doc;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return doc;
    }

    QByteArray raw = file.readAll();
    file.close();
    if (raw.startsWith("\xEF\xBB\xBF")) {
        raw.remove(0, 3);
    }

    QByteArray const rawLower = raw.toLower();
    int const eohStart = rawLower.indexOf("<eoh>");
    QByteArray body = raw;
    if (eohStart >= 0) {
        int const eohEnd = eohStart + 5;
        doc.header = QString::fromUtf8(raw.left(eohEnd)).trimmed() + QLatin1Char('\n');
        body = raw.mid(eohEnd);
    } else {
        doc.header = QStringLiteral("Decodium4 ADIF Log\n<EOH>\n");
    }

    doc.records = parseAdifRecordsBytes(body);
    doc.loaded = true;
    return doc;
}

bool writeAdifDocument(QString const& path, ParsedAdifDocument const& doc)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream ts(&file);
    ts << (doc.header.isEmpty() ? QStringLiteral("Decodium4 ADIF Log\n<EOH>\n") : doc.header);
    if (!doc.header.endsWith(QLatin1Char('\n'))) {
        ts << '\n';
    }

    auto writeField = [&ts](QString const& tag, QString const& value) {
        if (!tag.isEmpty() && !value.isNull() && !value.isEmpty()
            && !decodium::adif::isInvalidUnsetValue(tag, value)) {
            ts << '<' << tag << ':' << value.toUtf8().size() << '>' << value << ' ';
        }
    };

    for (ParsedAdifRecord const& record : doc.records) {
        for (auto it = record.fields.cbegin(); it != record.fields.cend(); ++it) {
            writeField(it.key(), it.value());
        }
        ts << "<EOR>\n";
    }
    ts.flush();
    if (ts.status() != QTextStream::Ok) {
        return false;
    }
    return file.commit();
}


QString adifDigitsOnly(QString const& value)
{
    QString digits;
    digits.reserve(value.size());
    for (QChar const& ch : value.trimmed()) {
        if (ch.isDigit()) {
            digits.append(ch);
        }
    }
    return digits;
}

QString normalizedAdifDateToken(QString const& value)
{
    QString const digits = adifDigitsOnly(value);
    return digits.size() >= 8 ? digits.left(8) : QString {};
}

QString normalizedAdifTimeToken(QString const& value)
{
    QString const digits = adifDigitsOnly(value);
    if (digits.isEmpty()) {
        return {};
    }
    return digits.leftJustified(6, QLatin1Char('0')).left(6);
}

QString normalizeAdifModeForDisplay(QMap<QString, QString> const& fields)
{
    QString const submode = fields.value(QStringLiteral("SUBMODE")).trimmed().toUpper();
    if (!submode.isEmpty()) {
        return submode;
    }
    return fields.value(QStringLiteral("MODE")).trimmed().toUpper();
}

QString normalizedAdifDateTime(QMap<QString, QString> const& fields)
{
    QString const date = normalizedAdifDateToken(fields.value(QStringLiteral("QSO_DATE")));
    if (date.size() != 8) {
        return {};
    }
    QString time = normalizedAdifTimeToken(fields.value(QStringLiteral("TIME_ON")));
    if (time.isEmpty()) {
        time = normalizedAdifTimeToken(fields.value(QStringLiteral("TIME_OFF")));
    }
    QDateTime utc = QDateTime::fromString(date + time, QStringLiteral("yyyyMMddHHmmss"));
    utc.setTimeSpec(Qt::UTC);
    return utc.isValid() ? utc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString {};
}

quint64 adifFrequencyHzFromFields(QMap<QString, QString> const& fields)
{
    QString freqMhzText = fields.value(QStringLiteral("FREQ")).trimmed();
    if (freqMhzText.isEmpty()) {
        freqMhzText = fields.value(QStringLiteral("FREQ_RX")).trimmed();
    }
    if (freqMhzText.isEmpty()) {
        return 0;
    }

    freqMhzText.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    double const freqMhz = freqMhzText.toDouble(&ok);
    if (!ok || freqMhz <= 0.0) {
        return 0;
    }
    return static_cast<quint64>(freqMhz * 1'000'000.0 + 0.5);
}

QString adifBandFromFields(QMap<QString, QString> const& fields)
{
    QString const band = fields.value(QStringLiteral("BAND")).trimmed().toUpper();
    if (!band.isEmpty()) {
        return band;
    }

    quint64 const freqHz = adifFrequencyHzFromFields(fields);
    return freqHz > 0 ? freqHzToBandToken(freqHz).toUpper() : QString {};
}

ParsedAdifRecord normalizeImportedAdifRecord(ParsedAdifRecord record)
{
    auto normalizeUpperField = [&record](QString const& key) mutable {
        QString const value = record.fields.value(key).trimmed().toUpper();
        if (value.isEmpty()) {
            record.fields.remove(key);
        } else {
            record.fields.insert(key, value);
        }
    };

    normalizeUpperField(QStringLiteral("CALL"));
    normalizeUpperField(QStringLiteral("GRIDSQUARE"));
    normalizeUpperField(QStringLiteral("BAND"));
    normalizeUpperField(QStringLiteral("MODE"));
    normalizeUpperField(QStringLiteral("SUBMODE"));
    normalizeUpperField(QStringLiteral("RST_SENT"));
    normalizeUpperField(QStringLiteral("RST_RCVD"));

    QString const date = normalizedAdifDateToken(record.fields.value(QStringLiteral("QSO_DATE")));
    if (!date.isEmpty()) {
        record.fields.insert(QStringLiteral("QSO_DATE"), date);
    }

    QString time = normalizedAdifTimeToken(record.fields.value(QStringLiteral("TIME_ON")));
    if (time.isEmpty()) {
        time = normalizedAdifTimeToken(record.fields.value(QStringLiteral("TIME_OFF")));
    }
    if (!time.isEmpty()) {
        record.fields.insert(QStringLiteral("TIME_ON"), time);
    }

    QString const band = adifBandFromFields(record.fields);
    if (!band.isEmpty()) {
        record.fields.insert(QStringLiteral("BAND"), band);
    }

    return record;
}

QString adifRecordDedupeKey(QMap<QString, QString> const& fields)
{
    QString const call = fields.value(QStringLiteral("CALL")).trimmed().toUpper();
    if (call.isEmpty()) {
        return {};
    }

    QString const dateTime = normalizedAdifDateTime(fields);
    QString const mode = normalizeAdifModeForDisplay(fields);
    QString const band = adifBandFromFields(fields);
    if (!dateTime.isEmpty()) {
        return QStringList { call, dateTime, mode, band }.join(QLatin1Char('|'));
    }

    QString time = normalizedAdifTimeToken(fields.value(QStringLiteral("TIME_ON")));
    if (time.isEmpty()) {
        time = normalizedAdifTimeToken(fields.value(QStringLiteral("TIME_OFF")));
    }

    return QStringList {
        call,
        QStringLiteral("RAW"),
        normalizedAdifDateToken(fields.value(QStringLiteral("QSO_DATE"))),
        time,
        mode,
        band,
        fields.value(QStringLiteral("FREQ")).trimmed(),
        fields.value(QStringLiteral("RST_SENT")).trimmed().toUpper(),
        fields.value(QStringLiteral("RST_RCVD")).trimmed().toUpper(),
        fields.value(QStringLiteral("GRIDSQUARE")).trimmed().toUpper()
    }.join(QLatin1Char('|'));
}

// Map a frequency in Hz to the standard band token used by ADIF / WSJT-X
// (e.g. "20m"). Returns empty string for invalid/out-of-range frequencies.
QString freqHzToBandToken(quint64 freqHz)
{
    const double mhz = freqHz / 1'000'000.0;
    if      (mhz <  1.8)   return QString();
    else if (mhz <  2.0)   return QStringLiteral("160m");
    else if (mhz <  4.0)   return QStringLiteral("80m");
    else if (mhz <  6.0)   return QStringLiteral("60m");
    else if (mhz <  8.0)   return QStringLiteral("40m");
    else if (mhz < 11.0)   return QStringLiteral("30m");
    else if (mhz < 15.0)   return QStringLiteral("20m");
    else if (mhz < 19.0)   return QStringLiteral("17m");
    else if (mhz < 22.0)   return QStringLiteral("15m");
    else if (mhz < 25.0)   return QStringLiteral("12m");
    else if (mhz < 30.0)   return QStringLiteral("10m");
    else if (mhz < 52.0)   return QStringLiteral("6m");
    else if (mhz < 150.0)  return QStringLiteral("2m");
    else if (mhz < 440.0)  return QStringLiteral("70cm");
    else                    return QString();
}

} // namespace decodium::adif_document
