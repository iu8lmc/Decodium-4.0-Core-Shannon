#pragma once
#include <QByteArray>
#include <QString>

namespace decodium::ft2_access {
QByteArray ft2LinkConfiguredSalt();
QByteArray ft2LinkConfiguredHash();
int ft2LinkConfiguredIterations();
QByteArray ft2LinkPbkdf2Sha256(QString const& password, QByteArray const& salt,
                              int iterations, int outputBytes);
bool constantTimeEquals(QByteArray const& lhs, QByteArray const& rhs);
} // namespace decodium::ft2_access
