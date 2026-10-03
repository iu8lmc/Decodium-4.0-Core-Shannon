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

// Un nominativo hashato CON contenuto ("<IU8LMC>"), da distinguere dal
// segnaposto "<...>" di un hash non ancora risolvibile.
bool isHashedCallToken(QString const& token);

// Il messaggio e' nella forma canonica del tipo 4, quella dei nominativi non
// standard: "<IU8LMC> II8IHBC", due soli elementi, uno hashato e uno per
// esteso, senza coda. Significa "il secondo chiama il primo" ed e' COMPLETO.
//
// Va interrogata sul messaggio GREZZO: normalizeCallToken() toglie le
// parentesi angolari, quindi sui token normalizzati un hash e' ormai
// indistinguibile da un nominativo qualsiasi. E' esattamente l'errore che ha
// reso inerte la prima correzione dell'11/9/2026.
bool isNonStandardDirectedForm(QString const& message);

// Come sopra, ma restituisce anche i due nominativi: in *hashedOut quello che
// sta fra le parentesi angolari (il DESTINATARIO, gia' risolto dal decoder) e
// in *plainOut quello per esteso (il MITTENTE, non standard). Entrambi senza
// parentesi e in maiuscolo. Torna false se il messaggio non e' di tipo 4.
//
// Serve al sequencer: quando il destinatario e' esplicito non c'e' bisogno di
// indovinare dal QSO in corso, basta confrontarlo col proprio nominativo.
bool splitNonStandardDirected(QString const& message,
                              QString* hashedOut,
                              QString* plainOut);

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
