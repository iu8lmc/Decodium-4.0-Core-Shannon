#include "DecodeUnresolved.h"
#include "DecodeTokenClassifier.h"
#include "Sequencer/MessageTokenRules.hpp"

using decodium::decode_tokens::isGridToken;
using decodium::decode_tokens::looksLikeCallsignToken;
using namespace decodium::seq;
namespace decodium::decode_unresolved {
bool messageIsUnresolvedGridOnlyDecode(const QString& message,
                                              QString* gridOut)
{
    QStringList const tokens = normalizedMessageTokens(message);
    bool hasPlaceholder = false;
    bool hasRealCall = false;
    QString grid;

    for (QString const& rawToken : tokens) {
        QString const token = normalizeCallToken(rawToken).trimmed().toUpper();
        if (token.isEmpty()
            || token == QStringLiteral("CQ")
            || token == QStringLiteral("QRZ")
            || token == QStringLiteral("DE")
            || isDirectedCqModifierToken(token)) {
            continue;
        }
        if (isPlaceholderCallToken(token)) {
            hasPlaceholder = true;
            continue;
        }
        if (isGridToken(token)) {
            if (grid.isEmpty()) {
                grid = token;
            }
            continue;
        }
        if (looksLikeCallsignToken(token)) {
            hasRealCall = true;
        }
    }

    if (gridOut) {
        *gridOut = grid;
    }
    return hasPlaceholder && !hasRealCall && !grid.isEmpty();
}

QString displayMessageWithCallsignsActorFirst(const QString& message,
                                                     bool isTx,
                                                     const QString& myCall)
{
    // 1.0.114: rimosso il riordino "actor-first" che invertiva i call solo
    // su alcuni RX (e non sui TX con grid). Risultato confuso: TX1 e RX2
    // sembravano avere lo stesso ordine ma significati opposti.
    // Ora tutti i messaggi sono mostrati nel formato wire WSJT-X canonico:
    // <recipient> <sender> <payload>. Coerente con jt9, MSHV, JTDX.
    Q_UNUSED(isTx);
    Q_UNUSED(myCall);
    return message.trimmed();
}

bool messageHasUnresolvedPeerPlaceholder(const QString& message,
                                                const QString& myCall)
{
    QStringList const words = message.simplified().split(QLatin1Char(' '),
                                                         Qt::SkipEmptyParts);
    if (words.size() < 2) {
        return false;
    }

    QString const myFull = normalizeCallToken(myCall).toUpper();
    QString const myBase = normalizedBaseCall(myFull);
    if (myFull.isEmpty() && myBase.isEmpty()) {
        return false;
    }

    return (tokenMatchesCall(words.at(0), myFull, myBase) && isPlaceholderCallToken(words.at(1)))
        || (tokenMatchesCall(words.at(1), myFull, myBase) && isPlaceholderCallToken(words.at(0)));
}

bool isFst4wUnresolvedHashDecode(QString const& mode, QString const& message)
{
    QString const normalizedMode = mode.trimmed().toUpper();
    if (!(normalizedMode == QStringLiteral("FST4W")
          || normalizedMode.startsWith(QStringLiteral("FST4W-")))) {
        return false;
    }

    QStringList const words = message.simplified().toUpper().split(QLatin1Char(' '),
                                                                   Qt::SkipEmptyParts);
    if (words.isEmpty()) {
        return false;
    }

    // FST4W compound/hash decodes can legitimately start with the unresolved
    // "<...>" sentinel. The following token is often a locator/power context,
    // not a callsign, so it must not feed DXCC, Live Map, PSK Reporter, or
    // worked-before state.
    return isPlaceholderCallToken(words.constFirst());
}
}
