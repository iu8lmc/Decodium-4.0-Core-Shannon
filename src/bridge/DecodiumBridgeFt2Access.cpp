#include "DecodiumBridge.h"
#include "Ft2LinkAccessCrypto.h"

static constexpr int kFt2LinkAccessSaltBytes = 16;
static constexpr int kFt2LinkAccessHashBytes = 32;
extern void decodiumBridgeLogbookLog(const QString&);

bool DecodiumBridge::ft2LinkAccessUnlocked() const
{
    return m_ft2LinkAccessUnlocked;
}

bool DecodiumBridge::ft2LinkAccessPasswordConfigured() const
{
    QByteArray const salt = decodium::ft2_access::ft2LinkConfiguredSalt();
    QByteArray const hash = decodium::ft2_access::ft2LinkConfiguredHash();
    int const iterations = decodium::ft2_access::ft2LinkConfiguredIterations();
    return salt.size() >= kFt2LinkAccessSaltBytes
        && hash.size() == kFt2LinkAccessHashBytes
        && iterations >= 10000;
}

bool DecodiumBridge::verifyFt2LinkAccessPassword(const QString& password)
{
    QByteArray const salt = decodium::ft2_access::ft2LinkConfiguredSalt();
    QByteArray const expected = decodium::ft2_access::ft2LinkConfiguredHash();
    int const iterations = decodium::ft2_access::ft2LinkConfiguredIterations();

    if (salt.size() < kFt2LinkAccessSaltBytes
        || expected.size() != kFt2LinkAccessHashBytes
        || iterations < 10000) {
        decodiumBridgeLogbookLog(QStringLiteral("FT2-Link access denied: password not configured"));
        return false;
    }

    QByteArray const actual = decodium::ft2_access::ft2LinkPbkdf2Sha256(
        password, salt, iterations, expected.size());
    bool const ok = decodium::ft2_access::constantTimeEquals(actual, expected);
    if (ok && !m_ft2LinkAccessUnlocked) {
        m_ft2LinkAccessUnlocked = true;
        emit ft2LinkAccessChanged();
    }
    decodiumBridgeLogbookLog(ok
              ? QStringLiteral("FT2-Link access unlocked")
              : QStringLiteral("FT2-Link access denied: bad password"));
    return ok;
}

void DecodiumBridge::lockFt2LinkAccess()
{
    if (!m_ft2LinkAccessUnlocked) {
        return;
    }
    m_ft2LinkAccessUnlocked = false;
    emit ft2LinkAccessChanged();
}

