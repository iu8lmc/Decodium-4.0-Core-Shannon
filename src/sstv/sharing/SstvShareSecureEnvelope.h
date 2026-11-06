// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SstvShareKeyStore.h"
#include "SstvShareRecipientEncryption.h"
#include <functional>
#include <memory>

namespace decodium::sstv::sharing {
// Verify a signed manifest against the already pinned sender key. Does not
// authenticate or decrypt the payload; opening must still be performed.
bool sstvShareVerifyStoredSender(const SstvShareKeyStore& keys,
                                 const SstvShareManifestV1& manifest);
// nullopt means the pinned-key store could not be read reliably.
std::optional<bool> sstvShareCheckStoredSender(const SstvShareKeyStore& keys,
                                              const SstvShareManifestV1& manifest);
// The returned hook retains its backend and only loads an existing identity.
std::function<std::optional<QByteArray>(const SstvShareManifestV1&, const QByteArray&)>
sstvShareMakeStoredEnvelopeOpener(std::shared_ptr<const secure_settings::Backend> backend,
                                 QString profile, QString expectedProvider,
                                 QString expectedRecipient, bool requireSenderAuthentication = false);
// Worker-only adapter. A recipient key must have been independently verified
// and pinned before sending. Neither operation falls back to plaintext.
std::optional<SstvShareEncryptedPayload> sstvShareSealPinnedEnvelope(
    const SstvShareKeyStore& keys, SstvShareManifestV1 manifest,
    const QByteArray& plaintext);
// Experimental signed variant. The receiver must independently pin the
// sender's signing key and explicitly require authentication when needed.
std::optional<SstvShareEncryptedPayload> sstvShareSealSignedEnvelope(
    const SstvShareKeyStore& keys, SstvShareManifestV1 manifest,
    const QByteArray& plaintext, const SstvShareKeyStore* signingKeys = nullptr);
std::optional<QByteArray> sstvShareOpenStoredEnvelope(
    const SstvShareKeyStore& keys, const QString& expectedProvider,
    const QString& expectedRecipient, const SstvShareManifestV1& manifest,
    const QByteArray& payload, bool requireSenderAuthentication = false);
} // namespace decodium::sstv::sharing
