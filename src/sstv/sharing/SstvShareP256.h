// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

namespace decodium::sstv::sharing {

// Signing primitive only. A signature does not establish sender identity until
// a caller authenticates and pins the public key and binds the full protocol
// message. Signed sharing envelopes authenticate that binding separately.
struct SstvShareP256KeyPair final {
    QByteArray privateDer;
    QByteArray publicDer;
};
inline constexpr qsizetype kSstvShareP256MaximumMessageBytes = 131'072;
std::optional<SstvShareP256KeyPair> sstvShareGenerateP256KeyPair();
std::optional<QByteArray> sstvShareP256PublicKey(const QByteArray& privateDer);
bool sstvShareP256PublicKeyValid(const QByteArray& publicDer);
QString sstvShareP256Fingerprint(const QByteArray& publicDer);
std::optional<QByteArray> sstvShareP256Sign(const QByteArray& privateDer, const QByteArray& message);
bool sstvShareP256Verify(const QByteArray& publicDer, const QByteArray& message, const QByteArray& signature);

} // namespace decodium::sstv::sharing
