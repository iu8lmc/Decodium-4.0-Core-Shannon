// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "SstvShareManifest.h"
#include <QByteArray>
#include <optional>

namespace decodium::sstv::sharing {
// Experimental authenticated payload; it does not provide recipient key
// authentication or a complete sharing protocol.
struct SstvShareEncryptedPayload final {
    SstvShareManifestV1 manifest;
    QByteArray bytes; // ciphertext followed by the 16-byte GCM tag
};
std::optional<SstvShareEncryptedPayload> sstvShareEncryptPayload(
    SstvShareManifestV1 manifest, const QByteArray& key, const QByteArray& plaintext);
std::optional<QByteArray> sstvShareDecryptPayload(
    const SstvShareManifestV1& manifest, const QByteArray& key, const QByteArray& payload);
} // namespace decodium::sstv::sharing
