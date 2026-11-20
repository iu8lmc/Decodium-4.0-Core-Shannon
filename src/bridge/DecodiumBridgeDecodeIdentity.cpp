#include "DecodiumBridge.h"
#include "Sequencer/MessageTokenRules.hpp"
#include "DecodiumLegacyBackend.h"

using decodium::seq::normalizedBaseCall;
using decodium::seq::normalizedUsableCallToken;
using decodium::seq::normalizedMessageTokens;
using decodium::seq::isDirectedCqModifierToken;
using decodium::seq::tokenMatchesCall;
bool decodiumBridgeIsGridToken(const QString&);
bool decodiumBridgeLooksLikeCallsignToken(const QString&);

QString DecodiumBridge::extractDecodedCallsign(const QString& msg, bool isCQ) const
{
    QStringList parts = msg.toUpper().split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return {};

    if (isCQ) {
        for (int i = 1; i < parts.size(); ++i) {
            if (decodiumBridgeIsGridToken(parts[i])) {
                continue;
            }
            if (decodiumBridgeLooksLikeCallsignToken(parts[i]))
                return parts[i];
        }
        return {};
    }

    QStringList const localCalls = localDecodeCallCandidates();
    for (QString const& localCall : localCalls) {
        QString const localBase = normalizedBaseCall(localCall);
        if (localCall.isEmpty() && localBase.isEmpty()) {
            continue;
        }
        for (int i = 0; i < parts.size(); ++i) {
            if (!tokenMatchesCall(parts[i], localCall, localBase)) {
                continue;
            }
            for (int j = i + 1; j < parts.size(); ++j) {
                if (decodiumBridgeIsGridToken(parts[j])) {
                    continue;
                }
                if (decodiumBridgeLooksLikeCallsignToken(parts[j]))
                    return parts[j];
            }
            for (int j = i - 1; j >= 0; --j) {
                if (decodiumBridgeIsGridToken(parts[j])) {
                    continue;
                }
                if (decodiumBridgeLooksLikeCallsignToken(parts[j]))
                    return parts[j];
            }
            return {};
        }
    }

    for (const QString& part : parts) {
        if (decodiumBridgeIsGridToken(part)) {
            continue;
        }
        if (decodiumBridgeLooksLikeCallsignToken(part))
            return part;
    }
    return {};
}

QString DecodiumBridge::extractDecodedGrid(const QString& msg) const
{
    QStringList parts = msg.split(' ', Qt::SkipEmptyParts);
    for (int pi = parts.size() - 1; pi >= 0; --pi) {
        if (decodiumBridgeIsGridToken(parts[pi]))
            return parts[pi].toUpper();
    }
    return {};
}


QStringList DecodiumBridge::localDecodeCallCandidates() const
{
    QStringList calls;

    auto addCall = [&calls](QString const& rawCall) {
        QString const normalized = normalizedUsableCallToken(rawCall);
        if (normalized.isEmpty() || calls.contains(normalized)) {
            return;
        }
        calls.append(normalized);
    };

    auto addOwnCallFromTxPayload = [&addCall](QString const& payload) {
        QStringList const tokens = normalizedMessageTokens(payload);
        if (tokens.isEmpty()) {
            return;
        }

        int ownIndex = -1;
        QString const first = tokens.constFirst();
        if (first == QStringLiteral("CQ") || first == QStringLiteral("QRZ")) {
            ownIndex = 1;
            if (tokens.size() > 2 && isDirectedCqModifierToken(tokens.value(1))) {
                ownIndex = 2;
            }
        } else if (tokens.size() >= 2) {
            // Standard FTx QSO TX payloads are sent as "<DX> <MYCALL> ...".
            ownIndex = 1;
        }

        if (ownIndex >= 0 && ownIndex < tokens.size()) {
            addCall(tokens.value(ownIndex));
        }
    };

    addCall(m_callsign);
    if (m_legacyBackend) {
        addCall(m_legacyBackend->callsign());
    }
    addOwnCallFromTxPayload(m_lastTransmittedMessage);
    addOwnCallFromTxPayload(m_pendingAutoSeqMessage);
    addOwnCallFromTxPayload(buildCurrentTxMessage());

    return calls;
}
