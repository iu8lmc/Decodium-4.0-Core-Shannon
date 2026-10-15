// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvQsoLog.h"

#include <QCryptographicHash>
#include <QDate>
#include <QRegularExpression>
#include <QSet>
#include <QTime>
#include <QTimeZone>
#include <QUuid>

#include <cmath>
#include <limits>
#include <utility>

namespace decodium::sstv {
namespace {

constexpr qsizetype kMaximumRecordIdCharacters = 64;
constexpr qsizetype kMaximumQsoIdCharacters = 256;
constexpr qsizetype kMaximumCallsignCharacters = 64;
constexpr qsizetype kMaximumGridCharacters = 16;
constexpr qsizetype kMaximumReportCharacters = 32;
constexpr qsizetype kMaximumCommentCharacters = 2'048;
constexpr qsizetype kMaximumModeCharacters = 128;
constexpr qint64 kMaximumFrequencyHz = 10'000'000'000'000LL;

bool parseExplicitUtc(const QVariant& value,
                      bool required,
                      QDateTime* result,
                      QString* error)
{
    if (!result) {
        if (error) {
            *error = QStringLiteral("internal UTC result is unavailable");
        }
        return false;
    }
    *result = {};
    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        if (!required) {
            return true;
        }
        if (error) {
            *error = QStringLiteral("QSO start UTC is required");
        }
        return false;
    }
    if (!text.endsWith(QLatin1Char('Z'), Qt::CaseInsensitive)) {
        if (error) {
            *error = QStringLiteral(
                "QSO time must be an explicit ISO-8601 UTC value ending in Z");
        }
        return false;
    }
    QDateTime parsed = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(text, Qt::ISODate);
    }
    if (!parsed.isValid()) {
        if (error) {
            *error = QStringLiteral("QSO UTC value is invalid");
        }
        return false;
    }
    *result = parsed.toUTC();
    return result->isValid() && result->timeSpec() == Qt::UTC;
}

bool printableBounded(QStringView value,
                      qsizetype maximumCharacters,
                      bool allowEmpty,
                      QStringView field,
                      QString* error)
{
    const QString text = value.toString().trimmed();
    if ((!allowEmpty && text.isEmpty())
        || text.size() > maximumCharacters) {
        if (error) {
            *error = QStringLiteral("%1 must contain %2 to %3 characters")
                         .arg(field.toString())
                         .arg(allowEmpty ? 0 : 1)
                         .arg(maximumCharacters);
        }
        return false;
    }
    for (const QChar character : text) {
        if (character.isNull()
            || (character.category() == QChar::Other_Control
                && character != QLatin1Char('\t'))) {
            if (error) {
                *error = QStringLiteral("%1 contains a control character")
                             .arg(field.toString());
            }
            return false;
        }
    }
    return true;
}

bool validCallsign(QStringView value, QString* error)
{
    const QString call = value.toString().trimmed().toUpper();
    if (!printableBounded(call, kMaximumCallsignCharacters, false,
                          u"remote callsign", error)) {
        return false;
    }
    static const QRegularExpression syntax(
        QStringLiteral("^[A-Z0-9]+(?:[/-][A-Z0-9]+)*$"));
    if (!syntax.match(call).hasMatch()
        || !call.contains(QRegularExpression(QStringLiteral("[A-Z]")))
        || !call.contains(QRegularExpression(QStringLiteral("[0-9]")))) {
        if (error) {
            *error = QStringLiteral("remote callsign has invalid syntax");
        }
        return false;
    }
    return true;
}

bool validGrid(QStringView value, QString* error)
{
    const QString grid = value.toString().trimmed().toUpper();
    if (!printableBounded(grid, kMaximumGridCharacters, true,
                          u"remote grid", error)) {
        return false;
    }
    if (grid.isEmpty()) {
        return true;
    }
    static const QRegularExpression maidenhead(
        QStringLiteral("^[A-R]{2}[0-9]{2}(?:[A-X]{2}(?:[0-9]{2})?)?$"));
    if (!maidenhead.match(grid).hasMatch()) {
        if (error) {
            *error = QStringLiteral("remote grid is not a 4, 6 or 8 character Maidenhead locator");
        }
        return false;
    }
    return true;
}

QString normalizedAdifTime(QStringView value)
{
    QString digits;
    const QString source = value.toString().trimmed();
    digits.reserve(source.size());
    for (const QChar character : source) {
        if (!character.isDigit()) {
            return {};
        }
        digits.append(character);
    }
    if (digits.size() != 4 && digits.size() != 6) {
        return {};
    }
    if (digits.size() == 4) {
        digits.append(QStringLiteral("00"));
    }
    return QTime::fromString(digits, QStringLiteral("HHmmss")).isValid()
        ? digits : QString {};
}

bool parsePositiveFrequency(QStringView value)
{
    QString text = value.toString().trimmed();
    text.replace(QLatin1Char(','), QLatin1Char('.'));
    bool converted = false;
    const double frequencyMhz = text.toDouble(&converted);
    return converted && std::isfinite(frequencyMhz)
        && frequencyMhz > 0.0 && frequencyMhz <= 10'000'000.0;
}

QString associationId(QStringView call,
                      QStringView date,
                      QStringView time,
                      QStringView mode,
                      QStringView band,
                      QString* error)
{
    const QString cleanCall = call.toString().trimmed().toUpper();
    const QString cleanDate = date.toString().trimmed();
    const QString cleanTime = normalizedAdifTime(time);
    const QString cleanMode = mode.toString().trimmed().toUpper();
    const QString cleanBand = band.toString().trimmed().toUpper();
    const QDate parsedDate = QDate::fromString(cleanDate,
                                               QStringLiteral("yyyyMMdd"));
    if (!validCallsign(cleanCall, error)
        || !parsedDate.isValid()
        || cleanTime.isEmpty()
        || cleanMode.isEmpty()
        || cleanBand.isEmpty()
        || cleanBand == QStringLiteral("OOB")) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral(
                "QSO identity requires valid CALL, QSO_DATE, TIME_ON, MODE and BAND");
        }
        return {};
    }
    const QByteArray canonical = QStringLiteral(
        "CALL=%1\nQSO_DATE=%2\nTIME_ON=%3\nMODE=%4\nBAND=%5")
                                     .arg(cleanCall, cleanDate, cleanTime,
                                          cleanMode, cleanBand)
                                     .toUtf8();
    const QByteArray digest = QCryptographicHash::hash(
        canonical, QCryptographicHash::Sha256).toHex();
    return QStringLiteral("adif-sha256:%1")
        .arg(QString::fromLatin1(digest));
}

bool forbiddenTag(QStringView tag)
{
    const QString upper = tag.toString().toUpper();
    return upper.contains(QStringLiteral("PATH"))
        || upper.contains(QStringLiteral("ATTACH"))
        || upper.contains(QStringLiteral("FILENAME"))
        || upper.contains(QStringLiteral("FILE_URI"));
}

} // namespace

bool SstvQsoLogRequest::validate(QString* error) const
{
    if (error) {
        error->clear();
    }
    const QString cleanRecordId = imageRecordId.trimmed();
    if (!printableBounded(cleanRecordId, kMaximumRecordIdCharacters, false,
                          u"image record ID", error)
        || QUuid::fromString(cleanRecordId).isNull()) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("image record ID must be a UUID");
        }
        return false;
    }
    if (!printableBounded(imageMode, kMaximumModeCharacters, false,
                          u"SSTV image mode", error)
        || SstvQsoLog::containsPathLikeValue(imageMode)) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("SSTV image mode contains a path-like value");
        }
        return false;
    }

    if (!createNewQso) {
        if (!printableBounded(existingQsoId, kMaximumQsoIdCharacters, false,
                              u"existing QSO ID", error)) {
            return false;
        }
        if (!existingQsoId.trimmed().startsWith(
                QStringLiteral("adif-sha256:"), Qt::CaseSensitive)
            || existingQsoId.trimmed().size() != 76) {
            if (error) {
                *error = QStringLiteral("existing QSO ID has invalid format");
            }
            return false;
        }
        static const QRegularExpression qsoIdSyntax(
            QStringLiteral("^adif-sha256:[0-9a-f]{64}$"));
        if (!qsoIdSyntax.match(existingQsoId.trimmed()).hasMatch()) {
            if (error) {
                *error = QStringLiteral("existing QSO ID digest is invalid");
            }
            return false;
        }
        return true;
    }

    if (!existingQsoId.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("a new QSO request cannot also name an existing QSO");
        }
        return false;
    }
    if (!validCallsign(remoteCallsign, error)
        || !validGrid(remoteGrid, error)) {
        return false;
    }
    if (frequencyHz <= 0 || frequencyHz > kMaximumFrequencyHz) {
        if (error) {
            *error = QStringLiteral("RF frequency is outside the supported range");
        }
        return false;
    }
    if (!timeOnUtc.isValid()
        || timeOnUtc.timeSpec() != Qt::UTC
        || (timeOffUtc.isValid() && timeOffUtc.timeSpec() != Qt::UTC)
        || (timeOffUtc.isValid() && timeOffUtc < timeOnUtc)) {
        if (error) {
            *error = QStringLiteral("QSO times must be ordered explicit UTC values");
        }
        return false;
    }
    if (!printableBounded(reportSent, kMaximumReportCharacters, true,
                          u"sent report", error)
        || !printableBounded(reportReceived, kMaximumReportCharacters, true,
                             u"received report", error)
        || !printableBounded(comments, kMaximumCommentCharacters, true,
                             u"comments", error)) {
        return false;
    }
    if (SstvQsoLog::containsPathLikeValue(comments)) {
        if (error) {
            *error = QStringLiteral(
                "comments contain a local path or file URI; image attachments stay in Gallery");
        }
        return false;
    }
    return true;
}

bool SstvQsoLog::requestFromVariantMap(
    const QVariantMap& values,
    SstvQsoLogRequest* request,
    QString* error)
{
    if (error) {
        error->clear();
    }
    if (!request) {
        if (error) {
            *error = QStringLiteral("internal SSTV QSO request is unavailable");
        }
        return false;
    }
    static const QSet<QString> allowedKeys {
        QStringLiteral("imageRecordId"),
        QStringLiteral("createNewQso"),
        QStringLiteral("existingQsoId"),
        QStringLiteral("remoteCallsign"),
        QStringLiteral("remoteGrid"),
        QStringLiteral("frequencyHz"),
        QStringLiteral("timeOnUtc"),
        QStringLiteral("timeOffUtc"),
        QStringLiteral("reportSent"),
        QStringLiteral("reportReceived"),
        QStringLiteral("comments"),
        QStringLiteral("imageMode")
    };
    static const QSet<QString> stringKeys {
        QStringLiteral("imageRecordId"),
        QStringLiteral("existingQsoId"),
        QStringLiteral("remoteCallsign"),
        QStringLiteral("remoteGrid"),
        QStringLiteral("timeOnUtc"),
        QStringLiteral("timeOffUtc"),
        QStringLiteral("reportSent"),
        QStringLiteral("reportReceived"),
        QStringLiteral("comments"),
        QStringLiteral("imageMode")
    };
    if (values.size() > allowedKeys.size()) {
        if (error) {
            *error = QStringLiteral("SSTV QSO request contains unknown fields");
        }
        return false;
    }
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (!allowedKeys.contains(it.key())) {
            if (error) {
                *error = QStringLiteral("SSTV QSO field is not permitted: %1")
                             .arg(it.key().left(64));
            }
            return false;
        }
        if (stringKeys.contains(it.key())
            && it.value().metaType().id() != QMetaType::QString) {
            if (error) {
                *error = QStringLiteral("SSTV QSO field must be a string: %1")
                             .arg(it.key());
            }
            return false;
        }
    }

    SstvQsoLogRequest parsed;
    const QVariant createNewValue = values.value(
        QStringLiteral("createNewQso"));
    if (!createNewValue.isValid()
        || createNewValue.metaType().id() != QMetaType::Bool) {
        if (error) {
            *error = QStringLiteral("createNewQso must be an explicit boolean");
        }
        return false;
    }
    parsed.imageRecordId = values.value(
        QStringLiteral("imageRecordId")).toString();
    parsed.createNewQso = createNewValue.toBool();
    parsed.existingQsoId = values.value(
        QStringLiteral("existingQsoId")).toString();
    parsed.remoteCallsign = values.value(
        QStringLiteral("remoteCallsign")).toString();
    parsed.remoteGrid = values.value(
        QStringLiteral("remoteGrid")).toString();
    parsed.reportSent = values.value(
        QStringLiteral("reportSent")).toString();
    parsed.reportReceived = values.value(
        QStringLiteral("reportReceived")).toString();
    parsed.comments = values.value(QStringLiteral("comments")).toString();
    parsed.imageMode = values.value(QStringLiteral("imageMode")).toString();

    if (parsed.createNewQso) {
        const QVariant frequencyValue = values.value(
            QStringLiteral("frequencyHz"));
        bool frequencyOk = false;
        const int frequencyType = frequencyValue.metaType().id();
        if (frequencyType == QMetaType::Double
            || frequencyType == QMetaType::Float) {
            const double numeric = frequencyValue.toDouble(&frequencyOk);
            if (!frequencyOk || !std::isfinite(numeric)
                || std::floor(numeric) != numeric
                || numeric < static_cast<double>(
                       std::numeric_limits<qint64>::min())
                || numeric > static_cast<double>(
                       std::numeric_limits<qint64>::max())) {
                frequencyOk = false;
            } else {
                parsed.frequencyHz = static_cast<qint64>(numeric);
            }
        } else if (frequencyType == QMetaType::Int
                   || frequencyType == QMetaType::LongLong) {
            parsed.frequencyHz = frequencyValue.toLongLong(&frequencyOk);
        } else if (frequencyType == QMetaType::UInt
                   || frequencyType == QMetaType::ULongLong) {
            const qulonglong numeric = frequencyValue.toULongLong(&frequencyOk);
            if (!frequencyOk
                || numeric > static_cast<qulonglong>(
                       std::numeric_limits<qint64>::max())) {
                frequencyOk = false;
            } else {
                parsed.frequencyHz = static_cast<qint64>(numeric);
            }
        } else {
            frequencyOk = false;
        }
        if (!frequencyOk) {
            if (error) {
                *error = QStringLiteral(
                    "RF frequency must be an integer number of hertz");
            }
            return false;
        }
        if (!parseExplicitUtc(
                values.value(QStringLiteral("timeOnUtc")), true,
                &parsed.timeOnUtc, error)
            || !parseExplicitUtc(
                values.value(QStringLiteral("timeOffUtc")), false,
                &parsed.timeOffUtc, error)) {
            return false;
        }
    }

    QString validationError;
    if (!parsed.validate(&validationError)) {
        if (error) {
            *error = validationError;
        }
        return false;
    }
    *request = std::move(parsed);
    return true;
}

SstvAdifValidationResult SstvQsoLog::validateGeneratedAdif(
    QStringView record,
    const QStringList& forbiddenLocalTokens)
{
    SstvAdifValidationResult result;
    const QByteArray bytes = record.toString().toUtf8();
    if (bytes.isEmpty() || bytes.size() > kMaximumAdifBytes) {
        result.error = QStringLiteral("ADIF record is empty or exceeds the size limit");
        return result;
    }

    qsizetype cursor = 0;
    int fieldCount = 0;
    bool endOfRecord = false;
    while (cursor < bytes.size()) {
        const qsizetype open = bytes.indexOf('<', cursor);
        if (open < 0) {
            break;
        }
        if (!bytes.mid(cursor, open - cursor).trimmed().isEmpty()) {
            result.error = QStringLiteral(
                "ADIF record contains data outside a declared field");
            return result;
        }
        if (endOfRecord) {
            result.error = QStringLiteral("ADIF field appears after EOR");
            return result;
        }
        const qsizetype close = bytes.indexOf('>', open + 1);
        if (close < 0) {
            result.error = QStringLiteral("ADIF field header is unterminated");
            return result;
        }
        const QByteArray headerBytes = bytes.mid(open + 1, close - open - 1)
                                           .trimmed();
        const QString header = QString::fromLatin1(headerBytes);
        if (header.compare(QStringLiteral("EOR"), Qt::CaseInsensitive) == 0) {
            endOfRecord = true;
            cursor = close + 1;
            continue;
        }
        if (header.compare(QStringLiteral("EOH"), Qt::CaseInsensitive) == 0) {
            result.error = QStringLiteral("ADIF header marker is not valid inside a QSO record");
            return result;
        }
        const QStringList parts = header.split(QLatin1Char(':'));
        if (parts.size() < 2 || parts.size() > 3) {
            result.error = QStringLiteral("ADIF field header has invalid syntax");
            return result;
        }
        const QString tag = parts.at(0).trimmed().toUpper();
        static const QRegularExpression tagSyntax(
            QStringLiteral("^[A-Z][A-Z0-9_]{0,63}$"));
        bool lengthConverted = false;
        const qlonglong declaredLength = parts.at(1).trimmed().toLongLong(
            &lengthConverted);
        if (!tagSyntax.match(tag).hasMatch()
            || !lengthConverted
            || declaredLength < 0
            || declaredLength > kMaximumFieldBytes) {
            result.error = QStringLiteral("ADIF field name or byte length is invalid");
            return result;
        }
        const qsizetype valueStart = close + 1;
        const qsizetype valueLength = static_cast<qsizetype>(declaredLength);
        if (valueStart > bytes.size()
            || valueLength > bytes.size() - valueStart) {
            result.error = QStringLiteral("ADIF field byte length exceeds the record");
            return result;
        }
        const QByteArray valueBytes = bytes.mid(valueStart, valueLength);
        const QString value = QString::fromUtf8(valueBytes);
        if (value.toUtf8() != valueBytes) {
            result.error = QStringLiteral("ADIF field is not valid UTF-8");
            return result;
        }
        if (result.fields.contains(tag)) {
            result.error = QStringLiteral("ADIF record contains duplicate field %1")
                               .arg(tag);
            return result;
        }
        if (forbiddenTag(tag) || containsPathLikeValue(value)) {
            result.error = QStringLiteral(
                "ADIF record contains a local attachment or path field");
            return result;
        }
        for (const QString& token : forbiddenLocalTokens) {
            const QString cleanToken = token.trimmed();
            if (!cleanToken.isEmpty()
                && value.contains(cleanToken, Qt::CaseSensitive)) {
                result.error = QStringLiteral(
                    "ADIF record contains a forbidden local attachment identifier");
                return result;
            }
        }
        result.fields.insert(tag, value);
        ++fieldCount;
        if (fieldCount > kMaximumFields) {
            result.error = QStringLiteral("ADIF record contains too many fields");
            return result;
        }
        cursor = valueStart + valueLength;
    }
    if (!bytes.mid(cursor).trimmed().isEmpty()) {
        result.error = QStringLiteral(
            "ADIF record contains trailing data outside a declared field");
        return result;
    }

    const QString mode = result.fields.value(QStringLiteral("MODE"))
                             .trimmed().toUpper();
    if (mode != QStringLiteral("SSTV")) {
        result.error = QStringLiteral("SSTV ADIF must contain MODE=SSTV");
        return result;
    }
    if (result.fields.contains(QStringLiteral("SUBMODE"))) {
        result.error = QStringLiteral(
            "ADIF 3.1.6 defines no SSTV SUBMODE; keep the image mode in Gallery");
        return result;
    }
    if (!validCallsign(result.fields.value(QStringLiteral("CALL")),
                       &result.error)) {
        return result;
    }
    if (!validCallsign(
            result.fields.value(QStringLiteral("STATION_CALLSIGN")),
            &result.error)) {
        result.error = QStringLiteral("ADIF STATION_CALLSIGN is missing or invalid");
        return result;
    }
    const QString date = result.fields.value(QStringLiteral("QSO_DATE"))
                             .trimmed();
    if (!QDate::fromString(date, QStringLiteral("yyyyMMdd")).isValid()
        || normalizedAdifTime(
               result.fields.value(QStringLiteral("TIME_ON"))).isEmpty()) {
        result.error = QStringLiteral("ADIF QSO_DATE or TIME_ON is invalid");
        return result;
    }
    const QString band = result.fields.value(QStringLiteral("BAND"))
                             .trimmed().toUpper();
    if (band.isEmpty() || band == QStringLiteral("OOB")) {
        result.error = QStringLiteral("ADIF BAND is missing or out of band");
        return result;
    }
    if (!parsePositiveFrequency(
            result.fields.value(QStringLiteral("FREQ")))) {
        result.error = QStringLiteral("ADIF FREQ is missing or invalid");
        return result;
    }

    result.associationId = associationIdForFields(result.fields,
                                                   &result.error);
    result.ok = !result.associationId.isEmpty();
    return result;
}

QString SstvQsoLog::associationIdForFields(
    const QMap<QString, QString>& fields,
    QString* error)
{
    if (error) {
        error->clear();
    }
    const QString submode = fields.value(QStringLiteral("SUBMODE"))
                                .trimmed().toUpper();
    const QString mode = submode.isEmpty()
        ? fields.value(QStringLiteral("MODE")).trimmed().toUpper()
        : submode;
    return associationId(fields.value(QStringLiteral("CALL")),
                         fields.value(QStringLiteral("QSO_DATE")),
                         fields.value(QStringLiteral("TIME_ON")),
                         mode,
                         fields.value(QStringLiteral("BAND")),
                         error);
}

QString SstvQsoLog::associationIdForExistingQso(
    QStringView callsign,
    QStringView dateTimeUtc,
    QStringView mode,
    QStringView band,
    QString* error)
{
    if (error) {
        error->clear();
    }
    QDateTime dateTime = QDateTime::fromString(
        dateTimeUtc.toString().trimmed(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    dateTime.setTimeZone(QTimeZone::UTC);
    if (!dateTime.isValid()) {
        if (error) {
            *error = QStringLiteral("existing QSO date/time is invalid");
        }
        return {};
    }
    return associationId(callsign,
                         dateTime.date().toString(QStringLiteral("yyyyMMdd")),
                         dateTime.time().toString(QStringLiteral("HHmmss")),
                         mode, band, error);
}

QString SstvQsoLog::mergedComment(QStringView operatorComment,
                                  QStringView imageMode,
                                  QString* error)
{
    if (error) {
        error->clear();
    }
    const QString cleanComment = operatorComment.toString().trimmed();
    const QString cleanMode = imageMode.toString().trimmed();
    if (!printableBounded(cleanComment, kMaximumCommentCharacters, true,
                          u"comments", error)
        || !printableBounded(cleanMode, kMaximumModeCharacters, false,
                             u"SSTV image mode", error)
        || containsPathLikeValue(cleanComment)
        || containsPathLikeValue(cleanMode)) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral(
                "SSTV log comment cannot contain a local path or file URI");
        }
        return {};
    }
    QString merged = QStringLiteral("SSTV image mode: %1").arg(cleanMode);
    if (!cleanComment.isEmpty()) {
        merged += QStringLiteral(" | ") + cleanComment;
    }
    if (merged.size() > kMaximumCommentCharacters) {
        if (error) {
            *error = QStringLiteral("merged SSTV log comment exceeds the size limit");
        }
        return {};
    }
    return merged;
}

bool SstvQsoLog::containsPathLikeValue(QStringView value) noexcept
{
    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        return false;
    }
    const QString lower = text.toLower();
    if (lower.contains(QStringLiteral("file://"))
        || text.startsWith(QStringLiteral("\\\\"))) {
        return true;
    }
    static const QRegularExpression posixAbsolute(
        QStringLiteral("(?:^|[\\s\\\"'=])(?:~/|/(?:users|home|var|tmp)/)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression windowsAbsolute(
        QStringLiteral("(?:^|[\\s\\\"'=])[A-Za-z]:[\\\\/].+"));
    return posixAbsolute.match(text).hasMatch()
        || windowsAbsolute.match(text).hasMatch();
}

} // namespace decodium::sstv
