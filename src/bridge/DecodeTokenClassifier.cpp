#include "DecodeTokenClassifier.h"
#include "Sequencer/MessageTokenRules.hpp"

using decodium::seq::normalizedUsableCallToken;
using decodium::seq::isPlausibleDecodedCallsignToken;
namespace decodium::decode_tokens {
bool isGridToken(const QString& token)
{
    QString t = token.trimmed().toUpper();
    if (t == "RR73")
        return false;
    if (t.size() != 4 && t.size() != 6)
        return false;
    if (!t[0].isLetter() || !t[1].isLetter() || !t[2].isDigit() || !t[3].isDigit())
        return false;
    if (t.size() == 6 && (!t[4].isLetter() || !t[5].isLetter()))
        return false;
    return true;
}

bool isTelemetryHexToken(const QString& token)
{
    QString const t = token.trimmed().toUpper();
    if (t.size() < 7 || t.size() > 18) {
        return false;
    }

    bool hasLetter = false;
    bool hasDigit = false;
    for (QChar const& ch : t) {
        bool const isHexDigit =
            (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
            || (ch >= QLatin1Char('A') && ch <= QLatin1Char('F'));
        if (!isHexDigit) {
            return false;
        }
        hasLetter = hasLetter || ch.isLetter();
        hasDigit = hasDigit || ch.isDigit();
    }
    return hasLetter && hasDigit;
}

bool looksLikeCallsignToken(const QString& token)
{
    QString const normalized = normalizedUsableCallToken(token);
    return !normalized.isEmpty()
        && (!isTelemetryHexToken(normalized)
            || isPlausibleDecodedCallsignToken(normalized));
}
}

bool decodiumBridgeLooksLikeCallsignToken(QString const& token) { return decodium::decode_tokens::looksLikeCallsignToken(token); }
bool decodiumBridgeIsGridToken(QString const& token) { return decodium::decode_tokens::isGridToken(token); }
