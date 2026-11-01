// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SstvShareKeyStore.h"
#include "SstvShareRecipientEncryption.h"
#include <functional>
#include <memory>

namespace decodium::sstv::sharing {
// The returned hook retains its backend and only loads an existing identity.
std::function<std::optional<QByteArray>(const SstvShareManifestV1&, const QByteArray&)>
sstvShareMakeStoredEnvelopeOpener(std::shared_ptr<const secure_settings::Backend> backend,
                                 QString profile, QString expectedProvider,
                                 QString expectedRecipient);
// Worker-only adapter. A recipient key must have been independently verified
// and pinned before sending. Neither operation falls back to plaintext.
std::optional<SstvShareEncryptedPayload> sstvShareSealPinnedEnvelope(
    const SstvShareKeyStore& keys, SstvShareManifestV1 manifest,
    const QByteArray& plaintext);
std::optional<QByteArray> sstvShareOpenStoredEnvelope(
    const SstvShareKeyStore& keys, const QString& expectedProvider,
    const QString& expectedRecipient, const SstvShareManifestV1& manifest,
    const QByteArray& payload);
} // namespace decodium::sstv::sharing
