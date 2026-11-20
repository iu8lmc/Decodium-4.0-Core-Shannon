// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <optional>

namespace decodium::sstv::sharing {

// Internal building block, not a complete E2EE protocol. Each call generates
// a fresh 96-bit nonce using OpenSSL's CSPRNG. Keys must be uniformly random,
// secret 32-byte values, never reused across independent security contexts.
struct SstvShareAeadEnvelope final {
    QByteArray nonce;
    QByteArray ciphertext;
    QByteArray tag;
};

std::optional<SstvShareAeadEnvelope> sstvShareAeadSeal(
    const QByteArray& key, const QByteArray& plaintext, const QByteArray& aad);
std::optional<QByteArray> sstvShareAeadOpen(
    const QByteArray& key, const SstvShareAeadEnvelope& envelope,
    const QByteArray& aad);

} // namespace decodium::sstv::sharing
