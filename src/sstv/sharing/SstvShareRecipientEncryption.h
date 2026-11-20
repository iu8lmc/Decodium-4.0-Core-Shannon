// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "SstvShareEncryptedPayload.h"

namespace decodium::sstv::sharing {
// The fingerprint must come from a separately authenticated, user-pinned
// source; a fingerprint supplied by the same provider as the key is not trust.
QString sstvShareX25519Fingerprint(const QByteArray& publicKey);
std::optional<SstvShareEncryptedPayload> sstvShareEncryptForRecipient(
    SstvShareManifestV1 manifest, const QByteArray& recipientPublicKey,
    const QString& verifiedFingerprint, const QByteArray& plaintext);
std::optional<QByteArray> sstvShareDecryptForRecipient(
    const SstvShareManifestV1& manifest, const QByteArray& recipientPrivateKey,
    const QByteArray& payload);
} // namespace decodium::sstv::sharing
