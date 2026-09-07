#ifndef DECODIUM_MESSAGE_TOKEN_RULES_HPP
#define DECODIUM_MESSAGE_TOKEN_RULES_HPP

// Fase 1 port mobile — step A3 strangler: famiglia PURA di parsing token e
// messaggi FT2/FT4/FT8 (call normalization, base-call, token match, payload
// 73/RR73, peer diretto) spostata VERBATIM da DecodiumBridge.cpp e condivisa
// tra desktop e decodium-core mobile. Nessuno stato, nessun side effect.

#include <QString>
#include <QStringList>

namespace decodium
{
namespace seq
{

bool isGridTokenStrict(QString const& token);

QString normalizeCallToken(QString token);

bool isPlaceholderCallToken(QString const& token);

bool isDirectedCqModifierToken(QString const& token);

bool isStrictAmateurCallsignToken(QString const& token);

bool isSpecialEventStyleCallsignToken(QString const& token);

bool isPlausibleDecodedCallsignToken(QString const& token);

QString normalizedUsableCallToken(QString const& token);

QString normalizedBaseCall(QString token);

bool tokenMatchesCall(QString const& token,

                             QString const& fullCall,

                             QString const& baseCall);

QStringList normalizedMessageTokens(QString const& message);

QString decodedDxCallToken(QString const& message);

bool messageContainsCallToken(QString const& message,

                                     QString const& fullCall,

                                     QString const& baseCall);

QString directedPeerTokenFromMessage(QString const& message,

                                            QString const& myFullCall,

                                            QString const& myBaseCall);

QString signalReportFromMessage(QString const& message);

bool messageCarries73Payload(QString const& message);

bool messageCarries73PayloadForCall(QString const& message,

                                           QString const& fullCall,

                                           QString const& baseCall);

}
}

#endif
