// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvShareSecurity.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace decodium::sstv::sharing {
namespace {

struct JsonShape final
{
    int depth {0};
    std::size_t nodes {0U};
    bool valid {true};
};

// QJsonDocument intentionally keeps the last value for a duplicate object
// key. That is unsafe for authenticated/versioned manifests because different
// implementations may bind or validate a different occurrence. Run this
// bounded lexical pass after Qt has validated syntax and shape, and compare
// decoded keys so `"key"` and `"\u006bey"` are duplicates too.
class JsonDuplicateKeyScanner final
{
public:
    explicit JsonDuplicateKeyScanner(const QByteArray& json) noexcept
        : m_json(json)
    {
    }

    bool hasDuplicate()
    {
        skipWhitespace();
        if (!scanValue()) {
            return true;
        }
        skipWhitespace();
        return m_duplicate || m_position != m_json.size();
    }

private:
    void skipWhitespace() noexcept
    {
        while (m_position < m_json.size()) {
            const char value = m_json.at(m_position);
            if (value != ' ' && value != '\t' && value != '\r'
                && value != '\n') {
                break;
            }
            ++m_position;
        }
    }

    bool scanString(QByteArray* encoded = nullptr)
    {
        if (m_position >= m_json.size() || m_json.at(m_position) != '"') {
            return false;
        }
        const qsizetype start = m_position++;
        bool escaped = false;
        while (m_position < m_json.size()) {
            const char value = m_json.at(m_position++);
            if (escaped) {
                escaped = false;
                continue;
            }
            if (value == '\\') {
                escaped = true;
                continue;
            }
            if (value == '"') {
                if (encoded) {
                    *encoded = m_json.mid(start, m_position - start);
                }
                return true;
            }
        }
        return false;
    }

    static bool decodeString(const QByteArray& encoded, QString& decoded)
    {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(
            QByteArray("[") + encoded + ']', &error);
        const QJsonArray array = document.array();
        if (error.error != QJsonParseError::NoError || !document.isArray()
            || array.size() != 1 || !array.at(0).isString()) {
            return false;
        }
        decoded = array.at(0).toString();
        return true;
    }

    bool scanObject()
    {
        ++m_position;
        skipWhitespace();
        if (m_position < m_json.size() && m_json.at(m_position) == '}') {
            ++m_position;
            return true;
        }
        QSet<QString> keys;
        for (;;) {
            QByteArray encodedKey;
            if (!scanString(&encodedKey)) {
                return false;
            }
            QString key;
            if (!decodeString(encodedKey, key)) {
                return false;
            }
            if (keys.contains(key)) {
                m_duplicate = true;
                return true;
            }
            keys.insert(key);
            skipWhitespace();
            if (m_position >= m_json.size()
                || m_json.at(m_position++) != ':') {
                return false;
            }
            skipWhitespace();
            if (!scanValue()) {
                return false;
            }
            if (m_duplicate) {
                return true;
            }
            skipWhitespace();
            if (m_position >= m_json.size()) {
                return false;
            }
            const char delimiter = m_json.at(m_position++);
            if (delimiter == '}') {
                return true;
            }
            if (delimiter != ',') {
                return false;
            }
            skipWhitespace();
        }
    }

    bool scanArray()
    {
        ++m_position;
        skipWhitespace();
        if (m_position < m_json.size() && m_json.at(m_position) == ']') {
            ++m_position;
            return true;
        }
        for (;;) {
            if (!scanValue()) {
                return false;
            }
            if (m_duplicate) {
                return true;
            }
            skipWhitespace();
            if (m_position >= m_json.size()) {
                return false;
            }
            const char delimiter = m_json.at(m_position++);
            if (delimiter == ']') {
                return true;
            }
            if (delimiter != ',') {
                return false;
            }
            skipWhitespace();
        }
    }

    bool scanValue()
    {
        skipWhitespace();
        if (m_position >= m_json.size()) {
            return false;
        }
        const char value = m_json.at(m_position);
        if (value == '{') {
            return scanObject();
        }
        if (value == '[') {
            return scanArray();
        }
        if (value == '"') {
            return scanString();
        }
        const qsizetype start = m_position;
        while (m_position < m_json.size()) {
            const char current = m_json.at(m_position);
            if (current == ',' || current == ']' || current == '}'
                || current == ' ' || current == '\t' || current == '\r'
                || current == '\n') {
                break;
            }
            ++m_position;
        }
        return m_position > start;
    }

    const QByteArray& m_json;
    qsizetype m_position {0};
    bool m_duplicate {false};
};

void inspectJsonShape(const QJsonValue& value,
                      int currentDepth,
                      int maximumDepth,
                      std::size_t maximumNodes,
                      JsonShape& shape)
{
    if (!shape.valid) {
        return;
    }
    ++shape.nodes;
    shape.depth = std::max(shape.depth, currentDepth);
    if (currentDepth > maximumDepth || shape.nodes > maximumNodes) {
        shape.valid = false;
        return;
    }
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            inspectJsonShape(it.value(), currentDepth + 1, maximumDepth,
                             maximumNodes, shape);
        }
    } else if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue& member : array) {
            inspectJsonShape(member, currentDepth + 1, maximumDepth,
                             maximumNodes, shape);
        }
    }
}

QByteArray encodedJsonString(const QString& value)
{
    const QByteArray array =
        QJsonDocument(QJsonArray {value}).toJson(QJsonDocument::Compact);
    return array.size() >= 2 ? array.mid(1, array.size() - 2) : QByteArray {};
}

bool appendCanonical(const QJsonValue& value,
                     QByteArray& output,
                     int depth,
                     int maximumDepth,
                     std::size_t maximumNodes,
                     std::size_t& nodes)
{
    ++nodes;
    if (depth > maximumDepth || nodes > maximumNodes) {
        return false;
    }
    switch (value.type()) {
    case QJsonValue::Null:
        output += "null";
        return true;
    case QJsonValue::Bool:
        output += value.toBool() ? "true" : "false";
        return true;
    case QJsonValue::Double: {
        const double number = value.toDouble();
        // JSON numbers larger than 2^53-1 are not exactly representable by
        // QJsonValue's double storage. Reject them before integer conversion.
        constexpr double maximumExactJsonInteger = 9'007'199'254'740'991.0;
        if (!std::isfinite(number) || std::trunc(number) != number
            || number < -maximumExactJsonInteger
            || number > maximumExactJsonInteger) {
            return false;
        }
        output += QByteArray::number(static_cast<qint64>(number));
        return true;
    }
    case QJsonValue::String:
        output += encodedJsonString(value.toString());
        return true;
    case QJsonValue::Array: {
        output += '[';
        const QJsonArray array = value.toArray();
        for (qsizetype i = 0; i < array.size(); ++i) {
            if (i != 0) {
                output += ',';
            }
            if (!appendCanonical(array.at(i), output, depth + 1,
                                 maximumDepth, maximumNodes, nodes)) {
                return false;
            }
        }
        output += ']';
        return true;
    }
    case QJsonValue::Object: {
        output += '{';
        const QJsonObject object = value.toObject();
        QStringList keys = object.keys();
        std::sort(keys.begin(), keys.end(), [](const QString& left,
                                               const QString& right) {
            return QString::compare(left, right, Qt::CaseSensitive) < 0;
        });
        for (qsizetype i = 0; i < keys.size(); ++i) {
            if (i != 0) {
                output += ',';
            }
            output += encodedJsonString(keys.at(i));
            output += ':';
            if (!appendCanonical(object.value(keys.at(i)), output, depth + 1,
                                 maximumDepth, maximumNodes, nodes)) {
                return false;
            }
        }
        output += '}';
        return true;
    }
    case QJsonValue::Undefined:
        return false;
    }
    return false;
}

bool forbiddenTextCategory(QChar::Category category)
{
    switch (category) {
    case QChar::Other_Control:
    case QChar::Other_Format:
    case QChar::Other_Surrogate:
    case QChar::Other_PrivateUse:
    case QChar::Other_NotAssigned:
        return true;
    default:
        return false;
    }
}

void appendCodePoint(QString& output, char32_t codePoint)
{
    output += QString::fromUcs4(&codePoint, 1);
}

QString redactEmbeddedUrls(const QString& text)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(https?://(?:[^\s<>"']|<redacted-url>)+)"),
        QRegularExpression::CaseInsensitiveOption);
    QString output;
    qsizetype copiedUntil = 0;
    QRegularExpressionMatchIterator matches = pattern.globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        output += text.mid(copiedUntil, match.capturedStart() - copiedUntil);
        output += match.captured().endsWith(
                      QStringLiteral("/<redacted-url>"))
            ? match.captured()
            : redactedShareUrl(QUrl(match.captured()));
        copiedUntil = match.capturedEnd();
    }
    output += text.mid(copiedUntil);
    return output;
}

} // namespace

SstvShareValidationError SstvShareValidationError::failure(
    SstvShareValidationCode code,
    QString field)
{
    return {code, std::move(field)};
}

QString sstvShareValidationCodeName(SstvShareValidationCode code)
{
    switch (code) {
    case SstvShareValidationCode::None: return QStringLiteral("none");
    case SstvShareValidationCode::JsonTooLarge: return QStringLiteral("json-too-large");
    case SstvShareValidationCode::MalformedJson: return QStringLiteral("malformed-json");
    case SstvShareValidationCode::JsonTooDeep: return QStringLiteral("json-too-deep");
    case SstvShareValidationCode::TooManyJsonNodes: return QStringLiteral("too-many-json-nodes");
    case SstvShareValidationCode::DuplicateJsonKey: return QStringLiteral("duplicate-json-key");
    case SstvShareValidationCode::RootNotObject: return QStringLiteral("root-not-object");
    case SstvShareValidationCode::UnknownProtocolVersion: return QStringLiteral("unknown-protocol-version");
    case SstvShareValidationCode::UnknownField: return QStringLiteral("unknown-field");
    case SstvShareValidationCode::MissingField: return QStringLiteral("missing-field");
    case SstvShareValidationCode::WrongType: return QStringLiteral("wrong-type");
    case SstvShareValidationCode::InvalidUuid: return QStringLiteral("invalid-uuid");
    case SstvShareValidationCode::InvalidIdentifier: return QStringLiteral("invalid-identifier");
    case SstvShareValidationCode::InvalidTimestamp: return QStringLiteral("invalid-timestamp");
    case SstvShareValidationCode::InvalidExpiry: return QStringLiteral("invalid-expiry");
    case SstvShareValidationCode::InvalidFilename: return QStringLiteral("invalid-filename");
    case SstvShareValidationCode::InvalidMimeType: return QStringLiteral("invalid-mime-type");
    case SstvShareValidationCode::InvalidHash: return QStringLiteral("invalid-hash");
    case SstvShareValidationCode::InvalidDimensions: return QStringLiteral("invalid-dimensions");
    case SstvShareValidationCode::InvalidByteSize: return QStringLiteral("invalid-byte-size");
    case SstvShareValidationCode::InvalidChunkCount: return QStringLiteral("invalid-chunk-count");
    case SstvShareValidationCode::InvalidText: return QStringLiteral("invalid-text");
    case SstvShareValidationCode::InvalidPrivacy: return QStringLiteral("invalid-privacy");
    case SstvShareValidationCode::InvalidTransportSecurity: return QStringLiteral("invalid-transport-security");
    case SstvShareValidationCode::InvalidEncryption: return QStringLiteral("invalid-encryption");
    case SstvShareValidationCode::Expired: return QStringLiteral("expired");
    case SstvShareValidationCode::UrlNotAllowed: return QStringLiteral("url-not-allowed");
    case SstvShareValidationCode::InvalidState: return QStringLiteral("invalid-state");
    case SstvShareValidationCode::InvalidTransition: return QStringLiteral("invalid-transition");
    case SstvShareValidationCode::InvalidRetryPolicy: return QStringLiteral("invalid-retry-policy");
    case SstvShareValidationCode::InvalidProgress: return QStringLiteral("invalid-progress");
    case SstvShareValidationCode::InvalidIdempotencyKey: return QStringLiteral("invalid-idempotency-key");
    case SstvShareValidationCode::InternalEncodingError: return QStringLiteral("internal-encoding-error");
    }
    return QStringLiteral("unknown");
}

SstvShareBoundedJsonResult parseBoundedJsonObject(
    const QByteArray& json,
    qsizetype maximumBytes,
    int maximumDepth,
    std::size_t maximumNodes)
{
    if (maximumBytes <= 0 || json.size() > maximumBytes) {
        return {{}, SstvShareValidationError::failure(
                        SstvShareValidationCode::JsonTooLarge)};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        return {{}, SstvShareValidationError::failure(
                        SstvShareValidationCode::MalformedJson)};
    }
    if (!document.isObject()) {
        return {{}, SstvShareValidationError::failure(
                        SstvShareValidationCode::RootNotObject)};
    }
    JsonShape shape;
    inspectJsonShape(document.object(), 1, maximumDepth, maximumNodes, shape);
    if (!shape.valid) {
        const SstvShareValidationCode code = shape.depth > maximumDepth
            ? SstvShareValidationCode::JsonTooDeep
            : SstvShareValidationCode::TooManyJsonNodes;
        return {{}, SstvShareValidationError::failure(code)};
    }
    JsonDuplicateKeyScanner duplicateScanner(json);
    if (duplicateScanner.hasDuplicate()) {
        return {{}, SstvShareValidationError::failure(
                        SstvShareValidationCode::DuplicateJsonKey)};
    }
    return {document.object(), {}};
}

QByteArray canonicalJson(const QJsonValue& value,
                         SstvShareValidationError* error,
                         int maximumDepth,
                         std::size_t maximumNodes)
{
    if (error) {
        *error = {};
    }
    QByteArray output;
    std::size_t nodes = 0U;
    if (!appendCanonical(value, output, 1, maximumDepth, maximumNodes, nodes)) {
        if (error) {
            *error = SstvShareValidationError::failure(
                SstvShareValidationCode::InternalEncodingError);
        }
        return {};
    }
    return output;
}

QString sanitizeShareDisplayText(const QString& input,
                                 qsizetype maximumCharacters,
                                 bool allowNewlines)
{
    if (maximumCharacters <= 0) {
        return {};
    }
    const QString normalized = input.normalized(QString::NormalizationForm_C);
    QString output;
    output.reserve(std::min(normalized.size(), maximumCharacters));
    bool previousSpace = false;
    for (qsizetype i = 0; i < normalized.size(); ++i) {
        char32_t codePoint = normalized.at(i).unicode();
        if (QChar::isHighSurrogate(static_cast<char16_t>(codePoint))
            && i + 1 < normalized.size()
            && QChar::isLowSurrogate(normalized.at(i + 1).unicode())) {
            const QChar high = normalized.at(i);
            const QChar low = normalized.at(i + 1);
            ++i;
            codePoint = QChar::surrogateToUcs4(high, low);
        }
        if (codePoint == '\r' || codePoint == '\n') {
            if (allowNewlines && !output.endsWith(QLatin1Char('\n'))) {
                output += QLatin1Char('\n');
            } else if (!allowNewlines && !previousSpace && !output.isEmpty()) {
                output += QLatin1Char(' ');
                previousSpace = true;
            }
            continue;
        }
        if (codePoint == '\t' || QChar::isSpace(codePoint)) {
            if (!previousSpace && !output.isEmpty()) {
                output += QLatin1Char(' ');
                previousSpace = true;
            }
            continue;
        }
        const QChar::Category category = QChar::category(codePoint);
        if (forbiddenTextCategory(category)) {
            continue;
        }
        const qsizetype requiredUnits = codePoint > 0xffffU ? 2 : 1;
        if (output.size() + requiredUnits > maximumCharacters) {
            break;
        }
        appendCodePoint(output, codePoint);
        previousSpace = false;
        if (output.size() == maximumCharacters) {
            break;
        }
    }
    output = output.trimmed();
    while (!output.isEmpty() && QChar::isHighSurrogate(output.back().unicode())) {
        output.chop(1);
    }
    return output;
}

QString sanitizeShareFilename(const QString& input, qsizetype maximumCharacters)
{
    if (maximumCharacters <= 0) {
        return {};
    }
    maximumCharacters = std::min<qsizetype>(maximumCharacters, 4'096);
    const QString baseText = sanitizeShareDisplayText(input, maximumCharacters * 2,
                                                      false);
    QString output;
    output.reserve(std::min(baseText.size(), maximumCharacters));
    for (QChar character : baseText) {
        const QChar::Category category = character.category();
        const bool alphaNumeric = character.isLetterOrNumber()
            || category == QChar::Mark_NonSpacing
            || category == QChar::Mark_SpacingCombining;
        const bool safePunctuation = QStringLiteral(" ._()-[]@+")
            .contains(character);
        output += alphaNumeric || safePunctuation ? character : QLatin1Char('_');
    }
    output.replace(QRegularExpression(QStringLiteral(R"(\s+)")),
                   QStringLiteral(" "));
    output.replace(QRegularExpression(QStringLiteral(R"(_{2,})")),
                   QStringLiteral("_"));
    output = output.trimmed();
    while (output.startsWith(QLatin1Char('.'))) {
        output.remove(0, 1);
    }
    if (output.size() > maximumCharacters) {
        output.truncate(maximumCharacters);
    }
    while (!output.isEmpty() && QChar::isHighSurrogate(output.back().unicode())) {
        output.chop(1);
    }
    if (output == QStringLiteral(".") || output == QStringLiteral("..")) {
        return {};
    }
    return output;
}

bool isSafeShareFilename(const QString& value, qsizetype maximumCharacters)
{
    return !value.isEmpty() && value.size() <= maximumCharacters
        && !containsNetworkUrl(value)
        && !value.contains(QLatin1Char('/'))
        && !value.contains(QLatin1Char('\\'))
        && !value.contains(QStringLiteral(".."))
        && sanitizeShareFilename(value, maximumCharacters) == value;
}

bool isSafeShareIdentifier(const QString& value, qsizetype maximumCharacters)
{
    if (value.isEmpty() || value.size() > maximumCharacters
        || value.contains(QStringLiteral("..")) || containsNetworkUrl(value)) {
        return false;
    }
    for (QChar character : value) {
        const ushort code = character.unicode();
        const bool allowed = (code >= 'a' && code <= 'z')
            || (code >= 'A' && code <= 'Z')
            || (code >= '0' && code <= '9')
            || character == QLatin1Char('.') || character == QLatin1Char('_')
            || character == QLatin1Char(':') || character == QLatin1Char('@')
            || character == QLatin1Char('+') || character == QLatin1Char('-');
        if (!allowed) {
            return false;
        }
    }
    return true;
}

bool isLowercaseSha256(const QString& value) noexcept
{
    if (value.size() != 64) {
        return false;
    }
    for (QChar character : value) {
        if (!((character >= QLatin1Char('0') && character <= QLatin1Char('9'))
              || (character >= QLatin1Char('a') && character <= QLatin1Char('f')))) {
            return false;
        }
    }
    return true;
}

bool containsNetworkUrl(const QString& value)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(\b(?:https?|wss?|ftp|s3|webdav|file):/{1,2})"),
        QRegularExpression::CaseInsensitiveOption);
    return value.contains(pattern);
}

QString redactedShareUrl(const QUrl& url)
{
    const QString scheme = url.scheme().toLower();
    const QString host = url.host(QUrl::FullyDecoded);
    if ((scheme != QStringLiteral("https") && scheme != QStringLiteral("http"))
        || host.isEmpty()) {
        return QStringLiteral("<redacted-url>");
    }
    QString result = scheme + QStringLiteral("://") + host;
    const int port = url.port(-1);
    if (port > 0) {
        result += QLatin1Char(':') + QString::number(port);
    }
    return result + QStringLiteral("/<redacted-url>");
}

QString redactShareSecrets(const QString& text)
{
    QString output = redactEmbeddedUrls(text);
    static const QRegularExpression authorization(
        QStringLiteral(R"((authorization\s*[:=]\s*)[^\r\n,;]+)"),
        QRegularExpression::CaseInsensitiveOption);
    output.replace(authorization, QStringLiteral("\\1<redacted>"));

    static const QRegularExpression bearer(
        QStringLiteral(R"(\bbearer\s+[A-Za-z0-9._~+/=-]+)"),
        QRegularExpression::CaseInsensitiveOption);
    output.replace(bearer, QStringLiteral("Bearer <redacted>"));

    static const QRegularExpression jsonSecret(
        QStringLiteral(R"regex(("(?:access[_-]?token|refresh[_-]?token|token|api[_-]?key|signature|x-amz-[^"]+)"\s*:\s*")[^"]*("))regex"),
        QRegularExpression::CaseInsensitiveOption);
    output.replace(jsonSecret, QStringLiteral("\\1<redacted>\\2"));

    static const QRegularExpression keyValueSecret(
        QStringLiteral(R"(\b((?:access[_-]?token|refresh[_-]?token|token|api[_-]?key|signature|x-amz-[a-z0-9-]+)\s*[:=]\s*)[^\s&,;"']+)"),
        QRegularExpression::CaseInsensitiveOption);
    output.replace(keyValueSecret, QStringLiteral("\\1<redacted>"));
    return output;
}

} // namespace decodium::sstv::sharing
