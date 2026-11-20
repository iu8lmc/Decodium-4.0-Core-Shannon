// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <optional>

namespace decodium::sstv::sharing {
struct SstvShareX25519KeyPair final { QByteArray privateKey; QByteArray publicKey; };
enum class SstvShareKeyRole { SenderEphemeral, Recipient };
std::optional<SstvShareX25519KeyPair> sstvShareGenerateX25519KeyPair();
std::optional<QByteArray> sstvShareX25519PublicKey(const QByteArray& privateKey);
// Derives one transfer key from X25519 and HKDF-SHA256. The context is a
// unique transfer identifier, and both public keys are bound in role order.
// The caller MUST independently authenticate the recipient's public key.
std::optional<QByteArray> sstvShareDeriveTransferKey(
    SstvShareKeyRole role, const QByteArray& privateKey,
    const QByteArray& senderEphemeralPublic, const QByteArray& recipientPublic,
    const QByteArray& context);
} // namespace decodium::sstv::sharing
