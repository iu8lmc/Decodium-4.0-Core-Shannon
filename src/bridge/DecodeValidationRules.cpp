#include "DecodeValidationRules.h"
#include "Sequencer/MessageTokenRules.hpp"
#include "Radio.hpp"
#include <QRegularExpression>
#include <QSet>

using namespace decodium::seq;
namespace decodium::decode_validation {
bool isPlainSignalReportToken(QString const& token)
{
    static const QRegularExpression plainReportPattern {
        R"(\A[-+]\d{2}\z)"
    };
    return plainReportPattern.match(normalizeCallToken(token).toUpper()).hasMatch();
}

bool isRogerSignalReportToken(QString const& token)
{
    static const QRegularExpression rogerReportPattern {
        R"(\A(?:R[-+]\d{2}|R\d{2})\z)"
    };
    return rogerReportPattern.match(normalizeCallToken(token).toUpper()).hasMatch();
}

bool isQsoSignoffToken(QString const& token)
{
    QString const upper = normalizeCallToken(token).toUpper();
    return upper == QStringLiteral("73")
        || upper == QStringLiteral("RR73")
        || upper == QStringLiteral("RRR");
}

bool isDirectedDecodeTerminalToken(QString const& token)
{
    QString const upper = normalizeCallToken(token).toUpper();
    return isQsoSignoffToken(upper)
        || isGridTokenStrict(upper)
        || isPlainSignalReportToken(upper)
        || isRogerSignalReportToken(upper);
}

bool looksLikeMalformedRGrid(QString const& msgText)
{
    QStringList const tokens = msgText.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < tokens.size(); ++i) {
        if (tokens[i].trimmed().toUpper() == QStringLiteral("R")
            && isGridTokenStrict(tokens[i + 1].trimmed().toUpper())) {
            return true;
        }
    }
    return false;
}

bool hasPortableOperatingDesignator(QString const& token)
{
    QString const upper = normalizeCallToken(token).trimmed().toUpper();
    int const slashIdx = upper.lastIndexOf(QLatin1Char('/'));
    if (slashIdx <= 0 || slashIdx >= upper.size() - 1) {
        return false;
    }

    QString const suffix = upper.mid(slashIdx + 1);
    static const QSet<QString> portableSuffixes {
        QStringLiteral("P"), QStringLiteral("M"), QStringLiteral("MM"),
        QStringLiteral("AM"), QStringLiteral("A"), QStringLiteral("R"),
        QStringLiteral("QRP"), QStringLiteral("PM"), QStringLiteral("MA"),
        QStringLiteral("LH")
    };
    return portableSuffixes.contains(suffix);
}

bool isPlausibleDecodedCallsign(QString const& token)
{
    return isPlausibleDecodedCallsignToken(token);
}

bool isSyntheticLabCallsign(QString const& token)
{
    QString const base = Radio::base_callsign(normalizeCallToken(token))
                             .trimmed()
                             .toUpper();
    static const QRegularExpression re(R"(\ATEST[A-Z0-9]\z)");
    return re.match(base).hasMatch();
}

int digitRunCount(QString const& token)
{
    int runs = 0;
    bool inDigitRun = false;
    for (QChar const& ch : token) {
        if (ch.isDigit()) {
            if (!inDigitRun) {
                ++runs;
                inDigitRun = true;
            }
        } else {
            inDigitRun = false;
        }
    }
    return runs;
}

bool isPlausibleFt2CqCallsign(QString const& token)
{
    if (isStrictAmateurCallsignToken(token)) {
        return true;
    }
    if (!isSpecialEventStyleCallsignToken(token)) {
        return false;
    }

    QString const base = Radio::base_callsign(normalizeCallToken(token))
                             .trimmed()
                             .toUpper();
    // FT2 CQ false positives often pass the broad special-event gate as
    // long letter/digit soup with multiple separated numeric islands
    // (for example "CQ KS58OQ94M"). Real event calls normally have one
    // contiguous event number, while normal calls are accepted above.
    return digitRunCount(base) <= 1;
}

bool isCqRoutingModifierToken(QString const& token)
{
    QString const upper = normalizeCallToken(token).trimmed().toUpper();
    if (isDirectedCqModifierToken(upper)) {
        return true;
    }
    static const QRegularExpression numericCqTag {
        R"(\A[0-9]{3}\z)"
    };
    return numericCqTag.match(upper).hasMatch();
}

bool isValidFt2CqTrailingPayload(QStringList const& tokens,
                                        int callIdx,
                                        QString* reason)
{
    for (int i = callIdx + 1; i < tokens.size(); ++i) {
        QString const token = normalizeCallToken(tokens.at(i)).trimmed().toUpper();
        if (token == QStringLiteral("?")) {
            continue;
        }
        if (i == callIdx + 1 && isGridTokenStrict(token)) {
            continue;
        }
        if (reason) {
            *reason = QStringLiteral("FT2 CQ trailing payload invalid (ghost)");
        }
        return false;
    }
    return true;
}

bool hasHighConfidenceGhostPrefix(QString const& callRaw)
{
    QString call = callRaw.trimmed().toUpper();
    int const slashIdx = call.indexOf('/');
    if (slashIdx > 0) call = call.left(slashIdx);
    if (call.size() < 2) return false;

    QString const prefix2 = call.left(2);
    if (prefix2.at(0) == QLatin1Char('0')) {
        return true;
    }
    if (prefix2.at(0) == QLatin1Char('1') && prefix2 != QStringLiteral("1A")) {
        return true;
    }

    static const QSet<QString> impossiblePrefixes {
        "H1", "ZH", "ZQ",
        // 1.0.224 — VZ non e' assegnato da ITU (V* assegnati: VA-VG, VK-VO, VP-VV, VX-VY).
        // HE non aggiunto: Switzerland storica (anche se ora HB9 standard).
        "VZ"
    };
    return impossiblePrefixes.contains(prefix2);
}

bool directedPeerLooksStructurallyGhost(QString const& peerToken)
{
    if (peerToken.trimmed().isEmpty() || isPlaceholderCallToken(peerToken)) {
        return false;
    }
    return !isPlausibleDecodedCallsign(peerToken)
        || hasHighConfidenceGhostPrefix(peerToken);
}

bool isDirectedDecodePayloadValid(QStringList const& payload,
                                         bool contestExchangeEnabled,
                                         QString* reason)
{
    if (payload.isEmpty()) {
        if (reason) *reason = QStringLiteral("missing directed payload");
        return false;
    }

    if (payload.size() == 1) {
        if (isDirectedDecodeTerminalToken(payload.constFirst())) {
            return true;
        }
        if (reason) *reason = QStringLiteral("unknown directed payload");
        return false;
    }

    if (payload.size() == 2) {
        QString const first = normalizeCallToken(payload.at(0)).toUpper();
        QString const second = normalizeCallToken(payload.at(1)).toUpper();
        if (second == QStringLiteral("TU")
            && (isPlainSignalReportToken(first) || isRogerSignalReportToken(first))) {
            return true;
        }
        if (first == QStringLiteral("R") && isGridTokenStrict(second)) {
            if (contestExchangeEnabled) {
                return true;
            }
            if (reason) *reason = QStringLiteral("contest R+grid payload while contest is disabled");
            return false;
        }
        if (reason) *reason = QStringLiteral("unknown two-token directed payload");
        return false;
    }

    if (!contestExchangeEnabled) {
        for (int i = 0; i + 1 < payload.size(); ++i) {
            QString const current = normalizeCallToken(payload.at(i)).toUpper();
            QString const next = normalizeCallToken(payload.at(i + 1)).toUpper();
            if (current == QStringLiteral("R") && isGridTokenStrict(next)) {
                if (reason) *reason = QStringLiteral("contest R+grid payload while contest is disabled");
                return false;
            }
        }
    }

    return true;
}

bool isResolvablePlaceholderDirectedPayload(QStringList const& payload,
                                                   bool contestExchangeEnabled)
{
    if (payload.size() == 1) {
        return isDirectedDecodeTerminalToken(payload.constFirst());
    }

    if (payload.size() == 2) {
        QString const first = normalizeCallToken(payload.at(0)).toUpper();
        QString const second = normalizeCallToken(payload.at(1)).toUpper();
        if (second == QStringLiteral("TU")
            && (isPlainSignalReportToken(first) || isRogerSignalReportToken(first))) {
            return true;
        }
        return first == QStringLiteral("R")
            && isGridTokenStrict(second)
            && contestExchangeEnabled;
    }

    return false;
}

bool decodeListModel_isTelemetryHexToken(QString const& token)
{
    QString const t = token.toUpper().remove(QRegularExpression("[<>;,]")).trimmed();
    if (t.size() < 7 || t.size() > 18) return false;
    bool hasLetter = false;
    bool hasDigit = false;
    for (QChar const ch : t) {
        char const c = ch.toLatin1();
        if (c >= '0' && c <= '9') hasDigit = true;
        else if (c >= 'A' && c <= 'F') hasLetter = true;
        else return false;  // non-hex char
    }
    return hasLetter && hasDigit;
}

bool decodeListModel_isTelemetryOnlyMessage(QString const& message)
{
    QStringList const parts = message.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() != 1) return false;
    QString const token = normalizeCallToken(parts.first());
    return decodeListModel_isTelemetryHexToken(token)
        && !isPlausibleDecodedCallsignToken(token);
}

bool isValidDecode(const QString& msg, int snr, const QString& aptype, float quality)
{
    Q_UNUSED(snr);
    Q_UNUSED(aptype);
    Q_UNUSED(quality);

    QString const trimmed = msg.trimmed();

    // Manteniamo solo guardrail minimi contro linee palesemente corrotte.
    // Il filtro aggressivo del bridge moderno non esiste in Decodium3 e
    // stava nascondendo decode validi e deboli nel confronto diretto.
    if (trimmed.length() < 2) return false;
    if (trimmed.contains(QStringLiteral("> <")) || trimmed.contains(QStringLiteral("< >"))) {
        return false;
    }

    return true;
}
}


bool decodiumBridgeDecodeTelemetryOnly(QString const& message)
{
    return decodium::decode_validation::decodeListModel_isTelemetryOnlyMessage(message);
}
