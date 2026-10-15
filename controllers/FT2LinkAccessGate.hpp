// controllers/FT2LinkAccessGate.hpp
//
// 2026-07-02 iu8lmc — logica PURA del gate d'accesso FT2-Link
// (PBKDF2-HMAC-SHA256 + confronto constant-time), estratta da DecodiumBridge.cpp
// per essere coperta da test automatici (fail-closed) nel CI-gate.
//
// Header-only. Usa Qt (QByteArray/QCryptographicHash) quindi NON puo' stare in
// ft2link_core (C++17 puro). NON contiene segreti: salt/hash arrivano dai
// #define compile-time provisionati via CMake (DECODIUM_FT2LINK_ACCESS_*),
// presenti solo nel bridge. Qui si valida solo l'ALGORITMO e il fail-closed.
//
// GARANZIA fail-closed: se la build non e' provisionata (salt/hash vuoti),
// isConfigured() torna false e verifyPassword() torna false a prescindere dalla
// password -> la UI rifiuta il modo FT2-Link e ripristina FT2. Un refactor non
// deve poter aprire il modo in build pubbliche: questo header e' la SINGLE SOURCE
// usata sia dal bridge sia dai test.
#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QString>

namespace decodium {
namespace ft2linkgate {

inline constexpr int kSaltBytes = 16;
inline constexpr int kHashBytes = 32;
inline constexpr int kMinIterations = 10000;

inline QByteArray hmacSha256 (QByteArray const& key, QByteArray const& message)
{
    return QMessageAuthenticationCode::hash (
        message, key, QCryptographicHash::Sha256);
}

// PBKDF2-HMAC-SHA256 standard (RFC 8018). Deterministico: stesso
// (password, salt, iterations, outputBytes) -> stesso output su ogni piattaforma.
inline QByteArray pbkdf2Sha256 (QString const& password, QByteArray const& salt,
                                int iterations, int outputBytes)
{
    QByteArray const key = password.toUtf8 ();
    QByteArray output;
    quint32 blockIndex = 1;
    while (output.size () < outputBytes) {
        QByteArray blockInput = salt;
        blockInput.append (static_cast<char> ((blockIndex >> 24) & 0xffu));
        blockInput.append (static_cast<char> ((blockIndex >> 16) & 0xffu));
        blockInput.append (static_cast<char> ((blockIndex >> 8) & 0xffu));
        blockInput.append (static_cast<char> (blockIndex & 0xffu));

        QByteArray u = hmacSha256 (key, blockInput);
        QByteArray t = u;
        for (int round = 1; round < iterations; ++round) {
            u = hmacSha256 (key, u);
            for (int i = 0; i < t.size () && i < u.size (); ++i) {
                t[i] = static_cast<char> (
                    static_cast<unsigned char> (t[i])
                    ^ static_cast<unsigned char> (u[i]));
            }
        }
        output.append (t);
        ++blockIndex;
    }
    return output.left (outputBytes);
}

// Confronto a tempo costante: nessun early-return sui byte, per non esporre
// informazione temporale sull'hash atteso.
inline bool constantTimeEquals (QByteArray const& lhs, QByteArray const& rhs)
{
    if (lhs.size () != rhs.size ()) {
        return false;
    }
    unsigned char diff = 0;
    for (int i = 0; i < lhs.size (); ++i) {
        diff |= static_cast<unsigned char> (lhs[i])
              ^ static_cast<unsigned char> (rhs[i]);
    }
    return diff == 0;
}

// True solo se la build e' provisionata con parametri validi. Fail-closed.
inline bool isConfigured (QByteArray const& salt, QByteArray const& hash,
                          int iterations)
{
    return salt.size () >= kSaltBytes
        && hash.size () == kHashBytes
        && iterations >= kMinIterations;
}

// True solo se il gate e' configurato E la password deriva l'hash atteso.
inline bool verifyPassword (QString const& password, QByteArray const& salt,
                            QByteArray const& expectedHash, int iterations)
{
    if (!isConfigured (salt, expectedHash, iterations)) {
        return false;  // fail-closed
    }
    QByteArray const actual =
        pbkdf2Sha256 (password, salt, iterations, expectedHash.size ());
    return constantTimeEquals (actual, expectedHash);
}

}  // namespace ft2linkgate
}  // namespace decodium
