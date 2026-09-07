// Corpi spostati VERBATIM da DecodiumBridge.cpp (erano static a file scope,
// step A3 strangler). Qualsiasi modifica alla logica va fatta QUI.
#include "Sequencer/MessageTokenRules.hpp"

#include <QRegularExpression>

#include "Radio.hpp"

namespace decodium
{
namespace seq
{

bool isGridTokenStrict(QString const& token)

{

    static const QRegularExpression gridPattern {

        R"(\A(?![Rr]{2}73)[A-Ra-r]{2}[0-9]{2}(?:[A-Xa-x]{2})?\z)"

    };

    return gridPattern.match(token.trimmed()).hasMatch();

}


QString normalizeCallToken(QString token)

{

    token = token.trimmed();

    token.remove(QChar('<'));

    token.remove(QChar('>'));

    if (token.endsWith(QChar(';'))) {

        token.chop(1);

    }

    return token.trimmed();

}


bool isPlaceholderCallToken(QString const& token)

{

    QString const upper = normalizeCallToken(token).trimmed().toUpper();

    return upper == QStringLiteral("...")

        || upper == QStringLiteral("<...>");

}


bool isDirectedCqModifierToken(QString const& token)

{

    QString const upper = normalizeCallToken(token).trimmed().toUpper();

    if (upper.isEmpty()) {

        return false;

    }



    static const QSet<QString> knownModifiers {

        QStringLiteral("DX"), QStringLiteral("NA"), QStringLiteral("SA"),

        QStringLiteral("EU"), QStringLiteral("AF"), QStringLiteral("AS"),

        QStringLiteral("OC"), QStringLiteral("TEST"), QStringLiteral("FD"),

        QStringLiteral("WW"), QStringLiteral("POTA"), QStringLiteral("SOTA"),

        QStringLiteral("IOTA"), QStringLiteral("BOTA"), QStringLiteral("QRP")

    };

    if (knownModifiers.contains(upper)) {

        return true;

    }



    // FT software commonly allows short directed-CQ tags. Do not classify

    // digit-bearing tokens as modifiers, otherwise unusual calls can be hidden.

    if (upper.size() <= 3) {

        for (QChar const& ch : upper) {

            if (ch.isDigit()) {

                return false;

            }

        }

        return true;

    }

    return false;

}


bool isStrictAmateurCallsignToken(QString const& token)

{

    QString t = normalizeCallToken(token).trimmed().toUpper();

    if (t.isEmpty()) {

        return false;

    }

    int const slashIdx = t.indexOf(QLatin1Char('/'));

    if (slashIdx > 0) {

        QString const suffix = t.mid(slashIdx + 1);

        QString const prefix = t.left(slashIdx);

        static const QSet<QString> portableSuffixes {

            QStringLiteral("P"), QStringLiteral("M"), QStringLiteral("MM"),

            QStringLiteral("AM"), QStringLiteral("A"), QStringLiteral("R"),

            QStringLiteral("QRP"), QStringLiteral("PM"), QStringLiteral("MA"),

            QStringLiteral("LH"),

            QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"),

            QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5"),

            QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8"),

            QStringLiteral("9")

        };

        if (portableSuffixes.contains(suffix)) {

            t = prefix;

        } else if (portableSuffixes.contains(prefix)

                   || (prefix.size() <= 3

                       && !prefix.contains(QRegularExpression(QStringLiteral(R"([0-9])"))))) {

            t = suffix;

        }

    }

    if (t.size() < 3 || t.size() > 7) {

        return false;

    }

    static const QRegularExpression pattern {

        QStringLiteral(R"(^(?:[A-Z]{1,2}|[A-Z][0-9]|[2-9][A-Z])[0-9][A-Z]{1,4}$)")

    };

    return pattern.match(t).hasMatch();

}


bool isSpecialEventStyleCallsignToken(QString const& token)

{

    QString const t = normalizeCallToken(token).trimmed().toUpper();

    if (t.isEmpty() || isPlaceholderCallToken(t)) {

        return false;

    }

    QString const base = Radio::base_callsign(t).trimmed().toUpper();

    if (base.size() < 4 || base.size() > 10) {

        return false;

    }

    bool hasLetter = false;

    bool hasDigit = false;

    bool digitSeen = false;

    bool hasLetterBeforeDigit = false;

    bool hasLetterAfterDigit = false;

    bool const hasDigitLetterPrefix = base.size() >= 2

        && base.at(0).isDigit()

        && base.at(1).isLetter();

    bool allHex = true;

    int digitCount = 0;

    for (QChar const& ch : base) {

        if (ch.isLetter()) {

            hasLetter = true;

            hasLetterBeforeDigit = hasLetterBeforeDigit || !digitSeen;

            hasLetterAfterDigit = hasLetterAfterDigit || digitSeen;

            if (ch < QLatin1Char('A') || ch > QLatin1Char('F')) {

                allHex = false;

            }

        } else if (ch.isDigit()) {

            hasDigit = true;

            digitSeen = true;

            ++digitCount;

        } else {

            return false;

        }

    }

    if (!hasLetter || !hasDigit || digitCount < 2

        || (!hasLetterBeforeDigit && !hasDigitLetterPrefix)

        || !hasLetterAfterDigit) {

        return false;

    }

    // Seven-character all-hex strings are ambiguous with telemetry, but they

    // can also be valid allocated calls (for example Indonesian 8B/8D special

    // events). The strict amateur-call pattern remains authoritative here.

    if (allHex && base.size() >= 7 && !isStrictAmateurCallsignToken(t)) {

        return false;

    }

    return Radio::is_callsign(base);

}


bool isPlausibleDecodedCallsignToken(QString const& token)

{

    return isStrictAmateurCallsignToken(token)

        || isSpecialEventStyleCallsignToken(token);

}


QString normalizedUsableCallToken(QString const& token)

{

    QString const upper = normalizeCallToken(token).trimmed().toUpper();

    if (upper.isEmpty()

        || isPlaceholderCallToken(upper)) {

        return {};

    }

    // Scarta grid 4-char (es. JN54). Non scartare grid 6-char ambigui:

    // call special-event come RP81AS, RA82BX matchano il pattern Maidenhead

    // esteso ma sono callsign reali. I grid 6-char non compaiono mai in

    // posizione callsign nei messaggi FT/CW standard.

    if (upper.size() == 4 && isGridTokenStrict(upper)) {

        return {};

    }

    if (upper == QStringLiteral("CQ")

        || isDirectedCqModifierToken(upper)

        || upper == QStringLiteral("QRZ")

        || upper == QStringLiteral("DE")

        || upper == QStringLiteral("TU")

        || upper == QStringLiteral("73")

        || upper == QStringLiteral("RR73")

        || upper == QStringLiteral("RRR")) {

        return {};

    }

    if (upper.size() < 3 || upper.size() > 15) {

        return {};

    }



    bool hasLetter = false;

    bool hasDigit = false;

    for (QChar const& ch : upper) {

        if (ch.isLetter()) {

            hasLetter = true;

        } else if (ch.isDigit()) {

            hasDigit = true;

        } else if (ch != QLatin1Char('/') && ch != QLatin1Char('-')) {

            return {};

        }

    }

    if (!hasLetter || !hasDigit) {

        return {};

    }



    QString const base = Radio::base_callsign(upper).trimmed().toUpper();

    if (base.isEmpty() || !Radio::is_callsign(base)) {

        return {};

    }

    return upper;

}


QString normalizedBaseCall(QString token)

{

    token = normalizeCallToken(token).toUpper();

    if (token.isEmpty()) {

        return {};

    }

    return Radio::base_callsign(token).trimmed().toUpper();

}


bool tokenMatchesCall(QString const& token,

                             QString const& fullCall,

                             QString const& baseCall)

{

    QString const upper = normalizeCallToken(token).toUpper();

    if (upper.isEmpty()) {

        return false;

    }



    QString const fullUpper = normalizeCallToken(fullCall).toUpper();

    QString const baseUpper = normalizeCallToken(baseCall).toUpper();

    QString const tokenBase = normalizedBaseCall(upper);



    return (!fullUpper.isEmpty() && (upper == fullUpper || tokenBase == fullUpper))

        || (!baseUpper.isEmpty() && (upper == baseUpper || tokenBase == baseUpper));

}


QStringList normalizedMessageTokens(QString const& message)
{
    // This runs for every decoded row, often several times per row.  A temporary
    // QRegularExpression forces PCRE compilation/JIT work in the main thread at
    // every decode burst.  simplified() has the same whitespace semantics for
    // FTx payloads and the character split does not compile a regex.
    QStringList const rawTokens = message.toUpper().simplified().split(QLatin1Char(' '),
                                                                       Qt::SkipEmptyParts);
    QStringList normalized;

    normalized.reserve(rawTokens.size());

    for (QString const& token : rawTokens) {

        QString const cleaned = normalizeCallToken(token);

        if (!cleaned.isEmpty()) {

            normalized.push_back(cleaned);

        }

    }

    return normalized;

}


QString decodedDxCallToken(QString const& message)

{

    QStringList const tokens = normalizedMessageTokens(message);

    if (tokens.isEmpty()) {

        return {};

    }

    QString const firstToken = tokens.constFirst();

    if (firstToken == QStringLiteral("CQ")

        || firstToken == QStringLiteral("QRZ")

        || firstToken == QStringLiteral("DE")) {

        for (int i = 1; i < tokens.size(); ++i) {

            QString const token = tokens.at(i);

            if (isDirectedCqModifierToken(token) || isGridTokenStrict(token)) {

                continue;

            }

            QString const call = normalizedUsableCallToken(token);

            if (!call.isEmpty()) {

                return call;

            }

        }

        return {};

    }

    QString const firstCall = normalizedUsableCallToken(firstToken);

    if (tokens.size() == 1) {

        return firstCall;

    }

    QString const secondToken = tokens.at(1);

    QString const secondCall = normalizedUsableCallToken(secondToken);

    if (!secondCall.isEmpty()) {

        return secondCall;

    }

    // Single-station payloads such as WSPR use CALL GRID POWER. Preserve that

    // case, but never substitute the left-hand station when a directed row's

    // right-hand callsign is unrecognised.

    if (!firstCall.isEmpty() && isGridTokenStrict(secondToken)) {

        return firstCall;

    }

    return {};

}


bool messageContainsCallToken(QString const& message,

                                     QString const& fullCall,

                                     QString const& baseCall)

{

    if (normalizeCallToken(fullCall).isEmpty() && normalizeCallToken(baseCall).isEmpty()) {

        return false;

    }



    QStringList const tokens = normalizedMessageTokens(message);

    for (QString const& token : tokens) {

        if (tokenMatchesCall(token, fullCall, baseCall)) {

            return true;

        }

    }

    return false;

}


QString directedPeerTokenFromMessage(QString const& message,

                                            QString const& myFullCall,

                                            QString const& myBaseCall)

{

    QStringList const tokens = normalizedMessageTokens(message);

    if (tokens.size() < 2) {

        return {};

    }



    bool const firstIsMine = tokenMatchesCall(tokens.at(0), myFullCall, myBaseCall);

    bool const secondIsMine = tokenMatchesCall(tokens.at(1), myFullCall, myBaseCall);

    if (firstIsMine == secondIsMine) {

        return {};

    }

    return firstIsMine ? tokens.at(1) : tokens.at(0);

}

QString signalReportFromMessage(QString const& message)
{
    static const QRegularExpression reportPattern {
        QStringLiteral(R"(\A(?:R)?([+-]\d{2})\z)")
    };

    QStringList const tokens = message.toUpper().simplified().split(
        QLatin1Char(' '), Qt::SkipEmptyParts);
    for (auto it = tokens.crbegin(); it != tokens.crend(); ++it) {
        QRegularExpressionMatch const match = reportPattern.match(it->trimmed());
        if (match.hasMatch()) {
            return match.captured(1);
        }
    }
    return {};
}


bool messageCarries73Payload(QString const& message)

{

    QStringList const parts = message.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString const& token : parts) {

        QString const upper = token.trimmed().toUpper();

        if (upper == QStringLiteral("73")

            || upper == QStringLiteral("RR73")

            || upper == QStringLiteral("RRR")) {

            return true;

        }

    }

    return false;

}


bool messageCarries73PayloadForCall(QString const& message,

                                           QString const& fullCall,

                                           QString const& baseCall)

{

    if (!messageCarries73Payload(message)) {

        return false;

    }



    QString const normalizedFull = normalizeCallToken(fullCall);

    QString const normalizedBase = normalizeCallToken(baseCall);

    if (normalizedFull.isEmpty() && normalizedBase.isEmpty()) {

        return true;

    }



    return messageContainsCallToken(message, normalizedFull, normalizedBase);

}


}
}
