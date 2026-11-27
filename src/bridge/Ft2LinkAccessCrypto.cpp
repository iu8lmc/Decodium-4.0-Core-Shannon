#include "Ft2LinkAccessCrypto.h"
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

namespace decodium::ft2_access {
#ifndef DECODIUM_FT2LINK_ACCESS_SALT_B64
#define DECODIUM_FT2LINK_ACCESS_SALT_B64 ""
#endif

#ifndef DECODIUM_FT2LINK_ACCESS_HASH_B64
#define DECODIUM_FT2LINK_ACCESS_HASH_B64 ""
#endif

#ifndef DECODIUM_FT2LINK_ACCESS_ITERATIONS
#define DECODIUM_FT2LINK_ACCESS_ITERATIONS 160000
#endif

QByteArray ft2LinkConfiguredSalt()
{
    return QByteArray::fromBase64(QByteArray(DECODIUM_FT2LINK_ACCESS_SALT_B64));
}

QByteArray ft2LinkConfiguredHash()
{
    return QByteArray::fromBase64(QByteArray(DECODIUM_FT2LINK_ACCESS_HASH_B64));
}

int ft2LinkConfiguredIterations()
{
    return DECODIUM_FT2LINK_ACCESS_ITERATIONS;
}

static QByteArray ft2LinkHmacSha256(QByteArray const& key,
                                    QByteArray const& message)
{
    return QMessageAuthenticationCode::hash(
        message, key, QCryptographicHash::Sha256);
}

QByteArray ft2LinkPbkdf2Sha256(QString const& password,
                                      QByteArray const& salt,
                                      int iterations,
                                      int outputBytes)
{
    QByteArray const key = password.toUtf8();
    QByteArray output;
    quint32 blockIndex = 1;
    while (output.size() < outputBytes) {
        QByteArray blockInput = salt;
        blockInput.append(static_cast<char>((blockIndex >> 24) & 0xffu));
        blockInput.append(static_cast<char>((blockIndex >> 16) & 0xffu));
        blockInput.append(static_cast<char>((blockIndex >> 8) & 0xffu));
        blockInput.append(static_cast<char>(blockIndex & 0xffu));

        QByteArray u = ft2LinkHmacSha256(key, blockInput);
        QByteArray t = u;
        for (int round = 1; round < iterations; ++round) {
            u = ft2LinkHmacSha256(key, u);
            for (int i = 0; i < t.size() && i < u.size(); ++i) {
                t[i] = static_cast<char>(
                    static_cast<unsigned char>(t[i])
                    ^ static_cast<unsigned char>(u[i]));
            }
        }
        output.append(t);
        ++blockIndex;
    }
    return output.left(outputBytes);
}

bool constantTimeEquals(QByteArray const& lhs, QByteArray const& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (int i = 0; i < lhs.size(); ++i) {
        diff |= static_cast<unsigned char>(lhs[i])
              ^ static_cast<unsigned char>(rhs[i]);
    }
    return diff == 0;
}

} // namespace decodium::ft2_access
