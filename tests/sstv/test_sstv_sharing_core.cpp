// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/sharing/SstvShareManifest.h"
#include "src/sstv/sharing/SstvShareProvider.h"
#include "src/sstv/sharing/SstvShareSecurity.h"
#include "src/sstv/sharing/SstvShareTransfer.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace decodium::sstv::sharing;

namespace {

int g_checks = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        ++g_checks;                                                             \
        if (!(condition)) {                                                     \
            std::cerr << __func__ << ':' << __LINE__                            \
                      << ": check failed: " #condition << '\n';                \
            return false;                                                       \
        }                                                                       \
    } while (false)

QDateTime fixedUtc(const QString& text)
{
    return QDateTime::fromString(text, Qt::ISODateWithMs).toUTC();
}

QString sha256Hex(const QByteArray& bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QByteArray deterministicPayload(qsizetype size = 2'048)
{
    QByteArray payload(size, Qt::Uninitialized);
    std::uint32_t value = 0x91e10da5U;
    for (qsizetype i = 0; i < payload.size(); ++i) {
        value ^= value << 13U;
        value ^= value >> 17U;
        value ^= value << 5U;
        payload[i] = static_cast<char>(value & 0xffU);
    }
    return payload;
}

SstvShareManifestV1 validManifest(const QByteArray& payload =
                                      deterministicPayload())
{
    SstvShareManifestV1 manifest;
    manifest.transferId = QUuid::fromString(
        QStringLiteral("2e6492b8-f383-4aa4-b953-173ba74dfdea"));
    manifest.providerId = QStringLiteral("local-test");
    manifest.senderId = QStringLiteral("station:9h1abc");
    manifest.recipientId = QStringLiteral("recipient:9h1xyz");
    manifest.createdUtc = fixedUtc(QStringLiteral("2026-08-24T10:00:00.000Z"));
    manifest.expiresUtc = fixedUtc(QStringLiteral("2026-08-25T10:00:00.000Z"));
    manifest.mediaUtc = fixedUtc(QStringLiteral("2026-08-24T09:59:00.000Z"));
    manifest.originalFilename = QStringLiteral("martin-m1-20260824.png");
    manifest.safeDisplayFilename = QStringLiteral("Martin M1 2026-08-24.png");
    manifest.mimeType = QStringLiteral("image/png");
    manifest.byteSize = static_cast<quint64>(payload.size());
    manifest.sha256 = sha256Hex(payload);
    manifest.width = 320U;
    manifest.height = 256U;
    manifest.sstvMode = QStringLiteral("Martin M1");
    manifest.source = SstvShareMediaSource::AnalogReception;
    manifest.completion = SstvShareContentCompletion::Complete;
    manifest.message = QStringLiteral("Synthetic local integration image");
    manifest.chunkCount = 2U;
    manifest.disposition = SstvShareContentDisposition::Attachment;
    manifest.privacy.recipientConfirmed = true;
    return manifest;
}

QByteArray mutateObject(const QByteArray& json,
                        const std::function<void(QJsonObject&)>& mutation)
{
    QJsonObject object = QJsonDocument::fromJson(json).object();
    mutation(object);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool expectManifestError(const QByteArray& json,
                         SstvShareValidationCode expected)
{
    const auto parsed = parseSstvShareManifestV1(json);
    CHECK(!parsed.ok());
    CHECK(parsed.error.code == expected);
    return true;
}

bool privacyDefaultsArePrivateAndOptIn()
{
    const SstvSharePrivacyFlags privacy;
    CHECK(!privacy.automaticUploadAllowed);
    CHECK(!privacy.automaticIncomingDownloadAllowed);
    CHECK(!privacy.locationIncluded);
    CHECK(!privacy.exifRetained);
    CHECK(!privacy.callsignIncluded);
    CHECK(!privacy.gridIncluded);
    CHECK(!privacy.publicShare);
    CHECK(!privacy.recipientConfirmed);
    CHECK(!privacy.meteredNetworkAllowed);
    CHECK(privacy.explicitExpiry);

    const SstvShareTransportSecurity transport;
    CHECK(transport.tlsRequired);
    CHECK(transport.certificateValidationRequired);
    CHECK(transport.sameOriginRedirectsOnly);
    CHECK(transport.providerCanReadContent);
    return true;
}

bool canonicalManifestRoundTripIsStable()
{
    const SstvShareManifestV1 manifest = validManifest();
    CHECK(manifest.validate().ok());
    CHECK(manifest.validate(true,
                            fixedUtc(QStringLiteral("2026-08-24T10:00:01.000Z")))
              .ok());
    SstvShareValidationError error;
    const QByteArray canonical = manifest.toCanonicalJson(&error);
    CHECK(error.ok());
    CHECK(canonical.startsWith("{\"byteSize\":"));
    CHECK(canonical.endsWith("\"width\":320}"));
    CHECK(!canonical.contains("http"));
    CHECK(!canonical.contains("token"));

    const auto parsed = parseSstvShareManifestV1(canonical);
    CHECK(parsed.ok());
    CHECK(parsed.manifest->transferId == manifest.transferId);
    CHECK(parsed.manifest->sha256 == manifest.sha256);
    CHECK(parsed.manifest->recipientId == manifest.recipientId);
    CHECK(parsed.manifest->toCanonicalJson() == canonical);

    const QJsonObject unordered {
        {QStringLiteral("z"), 2},
        {QStringLiteral("a"), 1},
    };
    CHECK(canonicalJson(unordered) == QByteArray("{\"a\":1,\"z\":2}"));
    return true;
}

bool parserRejectsSizeDepthShapeAndUnknownFields()
{
    const QByteArray canonical = validManifest().toCanonicalJson();
    CHECK(expectManifestError(QByteArray(kMaximumManifestJsonBytes + 1, 'x'),
                              SstvShareValidationCode::JsonTooLarge));
    CHECK(expectManifestError(QByteArray("{not-json"),
                              SstvShareValidationCode::MalformedJson));
    CHECK(expectManifestError(QByteArray("[]"),
                              SstvShareValidationCode::RootNotObject));

    QByteArray deep;
    for (int i = 0; i < 10; ++i) {
        deep += "{\"a\":";
    }
    deep += '0';
    for (int i = 0; i < 10; ++i) {
        deep += '}';
    }
    const auto deepResult = parseBoundedJsonObject(deep);
    CHECK(!deepResult.ok());
    CHECK(deepResult.error.code == SstvShareValidationCode::JsonTooDeep);

    QByteArray nodes("{");
    for (int i = 0; i < 4'100; ++i) {
        if (i != 0) {
            nodes += ',';
        }
        nodes += "\"k" + QByteArray::number(i) + "\":0";
    }
    nodes += '}';
    CHECK(nodes.size() < kMaximumManifestJsonBytes);
    const auto nodeResult = parseBoundedJsonObject(nodes);
    CHECK(!nodeResult.ok());
    CHECK(nodeResult.error.code
          == SstvShareValidationCode::TooManyJsonNodes);

    const auto duplicateRoot = parseBoundedJsonObject(
        QByteArray(R"({"protocolVersion":1,"protocolVersion":2})"));
    CHECK(!duplicateRoot.ok());
    CHECK(duplicateRoot.error.code
          == SstvShareValidationCode::DuplicateJsonKey);
    const auto duplicateEscaped = parseBoundedJsonObject(
        QByteArray(R"({"key":1,"\u006bey":2})"));
    CHECK(!duplicateEscaped.ok());
    CHECK(duplicateEscaped.error.code
          == SstvShareValidationCode::DuplicateJsonKey);
    const auto duplicateNested = parseBoundedJsonObject(
        QByteArray(R"({"outer":[{"safe":1,"safe":2}]})"));
    CHECK(!duplicateNested.ok());
    CHECK(duplicateNested.error.code
          == SstvShareValidationCode::DuplicateJsonKey);
    CHECK(sstvShareValidationCodeName(
              SstvShareValidationCode::DuplicateJsonKey)
          == QStringLiteral("duplicate-json-key"));

    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("futureField"), true);
        }),
        SstvShareValidationCode::UnknownField));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            QJsonObject encryption = object.value(QStringLiteral("encryption")).toObject();
            encryption.insert(QStringLiteral("secretUrl"),
                              QStringLiteral("https://example.invalid/signed"));
            object.insert(QStringLiteral("encryption"), encryption);
        }),
        SstvShareValidationCode::UnknownField));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("protocolVersion"), 2);
        }),
        SstvShareValidationCode::UnknownProtocolVersion));
    return true;
}

bool parserRejectsPathUrlAndInvalidContentMetadata()
{
    const QByteArray canonical = validManifest().toCanonicalJson();
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("originalFilename"),
                          QStringLiteral("../private/image.png"));
        }),
        SstvShareValidationCode::InvalidFilename));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("safeDisplayFilename"),
                          QStringLiteral("https://example.invalid/signed?token=secret"));
        }),
        SstvShareValidationCode::UrlNotAllowed));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("message"),
                          QStringLiteral("fetch https://example.invalid/object"));
        }),
        SstvShareValidationCode::UrlNotAllowed));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("providerId"),
                          QStringLiteral("https://provider.invalid"));
        }),
        SstvShareValidationCode::UrlNotAllowed));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("mimeType"), QStringLiteral("image/svg+xml"));
        }),
        SstvShareValidationCode::InvalidMimeType));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("sha256"), QString(64, QLatin1Char('A')));
        }),
        SstvShareValidationCode::InvalidHash));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("width"), 9'000);
        }),
        SstvShareValidationCode::InvalidDimensions));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("byteSize"), 0);
        }),
        SstvShareValidationCode::InvalidByteSize));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("chunkCount"), 0);
        }),
        SstvShareValidationCode::InvalidChunkCount));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("byteSize"), 1.0e19);
        }),
        SstvShareValidationCode::WrongType));
    return true;
}

bool expiryTimestampPrivacyAndTransportAreEnforced()
{
    const QByteArray canonical = validManifest().toCanonicalJson();
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("expiresUtc"),
                          QStringLiteral("2026-08-24T10:00:00.000Z"));
        }),
        SstvShareValidationCode::InvalidExpiry));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("expiresUtc"),
                          QStringLiteral("2026-10-24T10:00:00.000Z"));
        }),
        SstvShareValidationCode::InvalidExpiry));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            object.insert(QStringLiteral("createdUtc"),
                          QStringLiteral("2026-08-24T10:00:00Z"));
        }),
        SstvShareValidationCode::InvalidTimestamp));

    const auto expiredForTransmission = parseSstvShareManifestV1(
        canonical, true,
        fixedUtc(QStringLiteral("2026-08-26T00:00:00.000Z")));
    CHECK(!expiredForTransmission.ok());
    CHECK(expiredForTransmission.error.code == SstvShareValidationCode::Expired);

    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            QJsonObject transport = object.value(QStringLiteral("transport")).toObject();
            transport.insert(QStringLiteral("tlsRequired"), false);
            object.insert(QStringLiteral("transport"), transport);
        }),
        SstvShareValidationCode::InvalidTransportSecurity));
    CHECK(expectManifestError(
        mutateObject(canonical, [](QJsonObject& object) {
            QJsonObject privacy = object.value(QStringLiteral("privacy")).toObject();
            privacy.insert(QStringLiteral("callsignIncluded"), false);
            object.insert(QStringLiteral("privacy"), privacy);
            QJsonObject callsign = object.value(QStringLiteral("callsign")).toObject();
            callsign.insert(QStringLiteral("senderCallsign"), QStringLiteral("9H1ABC"));
            object.insert(QStringLiteral("callsign"), callsign);
        }),
        SstvShareValidationCode::InvalidPrivacy));

    SstvShareManifestV1 unconfirmed = validManifest();
    unconfirmed.privacy.recipientConfirmed = false;
    CHECK(unconfirmed.validate().ok());
    CHECK(unconfirmed.validate(
              true, fixedUtc(QStringLiteral("2026-08-24T10:00:01.000Z")))
              .code == SstvShareValidationCode::InvalidPrivacy);
    return true;
}

bool encryptionMetadataPreventsSilentDowngrade()
{
    SstvShareManifestV1 manifest = validManifest();
    manifest.encryption.mode = SstvShareEncryptionMode::EndToEnd;
    manifest.encryption.algorithm = QStringLiteral("xchacha20-poly1305");
    manifest.encryption.keyId = QStringLiteral("recipient-key:2026-08");
    manifest.encryption.recipientKeyFingerprint =
        sha256Hex(QByteArray("synthetic-public-key"));
    manifest.encryption.nonceBase64 = QByteArray(24, 'n').toBase64();
    manifest.encryption.manifestBoundAsAuthenticatedData = true;
    manifest.encryption.downgradeProtected = true;
    manifest.transport.providerCanReadContent = false;
    CHECK(manifest.validate().ok());
    const QByteArray encoded = manifest.toCanonicalJson();
    CHECK(!encoded.isEmpty());
    CHECK(parseSstvShareManifestV1(encoded).ok());

    manifest.encryption.manifestBoundAsAuthenticatedData = false;
    CHECK(manifest.validate().code == SstvShareValidationCode::InvalidEncryption);
    manifest.encryption.manifestBoundAsAuthenticatedData = true;
    manifest.encryption.downgradeProtected = false;
    CHECK(manifest.validate().code == SstvShareValidationCode::InvalidEncryption);
    manifest.encryption.downgradeProtected = true;
    manifest.transport.providerCanReadContent = true;
    CHECK(manifest.validate().code == SstvShareValidationCode::InvalidEncryption);
    manifest.transport.providerCanReadContent = false;
    manifest.encryption.algorithm = QStringLiteral("home-made-cipher");
    CHECK(manifest.validate().code == SstvShareValidationCode::InvalidEncryption);
    return true;
}

bool sanitizationAndRedactionDoNotExposeSecrets()
{
    CHECK(sanitizeShareFilename(QStringLiteral("../evil\\name?.png"))
          == QStringLiteral("_evil_name_.png"));
    CHECK(!isSafeShareFilename(QStringLiteral("../evil.png")));
    CHECK(isSafeShareFilename(QStringLiteral("Safe image 01.png")));
    CHECK(sanitizeShareDisplayText(QStringLiteral("A\u202eB\u0001 C"), 32)
          == QStringLiteral("AB C"));
    CHECK(sanitizeShareDisplayText(QStringLiteral("hello\r\nworld"), 32, true)
          == QStringLiteral("hello\nworld"));
    const QString emoji = QString::fromUtf8("\xf0\x9f\x93\xbb");
    CHECK(sanitizeShareDisplayText(emoji, 1).isEmpty());
    CHECK(sanitizeShareDisplayText(emoji, 2) == emoji);

    const QString secretText = QStringLiteral(
        "Authorization: Bearer AUTHSECRET\n"
        "PUT https://user:pass@example.com/upload/object.png?X-Amz-Signature=URLSECRET&token=QUERYSECRET "
        "token=PLAINSECRET {\"access_token\":\"JSONSECRET\"}");
    const QString redacted = redactShareSecrets(secretText);
    for (const QString& secret : {QStringLiteral("AUTHSECRET"),
                                  QStringLiteral("URLSECRET"),
                                  QStringLiteral("QUERYSECRET"),
                                  QStringLiteral("PLAINSECRET"),
                                  QStringLiteral("JSONSECRET"),
                                  QStringLiteral("user:pass"),
                                  QStringLiteral("upload/object.png")}) {
        CHECK(!redacted.contains(secret));
    }
    CHECK(redacted.contains(QStringLiteral("https://example.com/<redacted-url>")));
    CHECK(redactShareSecrets(redacted) == redacted);

    const auto providerError = SstvShareProviderResult::failure(
        SstvShareProviderFailure::Authentication, secretText);
    CHECK(!providerError.ok());
    CHECK(!providerError.redactedDiagnostic().contains(QStringLiteral("AUTHSECRET")));
    CHECK(!providerError.redactedDiagnostic().contains(QStringLiteral("URLSECRET")));
    return true;
}

QDateTime stateNow()
{
    return fixedUtc(QStringLiteral("2026-08-24T10:00:01.000Z"));
}

bool transitionTableRejectsSkippedAndTerminalMoves()
{
    CHECK(SstvShareTransfer::canTransition(SstvShareTransferState::Draft,
                                           SstvShareTransferState::Queued));
    CHECK(!SstvShareTransfer::canTransition(SstvShareTransferState::Draft,
                                            SstvShareTransferState::Uploading));
    CHECK(SstvShareTransfer::canTransition(SstvShareTransferState::Preparing,
                                           SstvShareTransferState::Uploading));
    CHECK(!SstvShareTransfer::canTransition(SstvShareTransferState::Preparing,
                                            SstvShareTransferState::Completed));
    CHECK(SstvShareTransfer::canTransition(
        SstvShareTransferState::Uploading,
        SstvShareTransferState::WaitingForAcknowledgement));
    CHECK(!SstvShareTransfer::canTransition(SstvShareTransferState::Uploading,
                                            SstvShareTransferState::Completed));
    CHECK(SstvShareTransfer::canTransition(
        SstvShareTransferState::WaitingForAcknowledgement,
        SstvShareTransferState::Completed));
    for (SstvShareTransferState terminal : {
             SstvShareTransferState::Completed,
             SstvShareTransferState::Cancelled,
             SstvShareTransferState::Rejected,
             SstvShareTransferState::Expired,
             SstvShareTransferState::Failed}) {
        CHECK(isTerminalShareTransferState(terminal));
        CHECK(!SstvShareTransfer::canTransition(
            terminal, SstvShareTransferState::Queued));
        CHECK(SstvShareTransfer::canTransition(terminal, terminal));
    }
    return true;
}

bool idempotencyKeyBindsManifestRecipientAndPayload()
{
    const SstvShareManifestV1 original = validManifest();
    const QString originalKey = SstvShareTransfer::deriveIdempotencyKey(original);
    CHECK(isLowercaseSha256(originalKey));

    SstvShareManifestV1 recipientChanged = original;
    recipientChanged.recipientId = QStringLiteral("recipient:another");
    CHECK(SstvShareTransfer::deriveIdempotencyKey(recipientChanged)
          != originalKey);

    SstvShareManifestV1 payloadChanged = original;
    payloadChanged.sha256 = sha256Hex(QByteArray("different payload"));
    CHECK(SstvShareTransfer::deriveIdempotencyKey(payloadChanged)
          != originalKey);

    SstvShareManifestV1 transferChanged = original;
    transferChanged.transferId = QUuid::fromString(
        QStringLiteral("04187bf4-7eab-4fdd-b597-f75ea4b67fa4"));
    CHECK(SstvShareTransfer::deriveIdempotencyKey(transferChanged)
          != originalKey);
    return true;
}

bool transferHappyPathAndIdempotentCompletion()
{
    SstvShareTransfer transfer(validManifest());
    CHECK(transfer.isValid());
    CHECK(transfer.snapshot().state == SstvShareTransferState::Draft);
    CHECK(isLowercaseSha256(transfer.snapshot().idempotencyKey));
    CHECK(!transfer.beginPreparing(stateNow()));
    CHECK(transfer.enqueue(stateNow()));
    CHECK(transfer.beginPreparing(stateNow()));
    CHECK(!transfer.beginEncrypting(stateNow()));
    CHECK(!transfer.beginUploading(stateNow()));
    CHECK(!transfer.bindProviderUpload(
        QStringLiteral("wrong"), QStringLiteral("upload:1")));
    CHECK(!transfer.bindProviderUpload(
        transfer.snapshot().idempotencyKey,
        QStringLiteral("https://signed.invalid/upload")));
    CHECK(transfer.bindProviderUpload(
        transfer.snapshot().idempotencyKey, QStringLiteral("upload:1")));
    CHECK(transfer.bindProviderUpload(
        transfer.snapshot().idempotencyKey, QStringLiteral("upload:1")));
    CHECK(!transfer.bindProviderUpload(
        transfer.snapshot().idempotencyKey, QStringLiteral("upload:2")));
    CHECK(transfer.beginUploading(stateNow()));
    CHECK(!transfer.recordProgress(1U, {}));
    CHECK(transfer.recordProgress(100U, stateNow()));
    CHECK(!transfer.recordProgress(99U, stateNow()));
    CHECK(!transfer.recordProgress(transfer.manifest().byteSize + 1U,
                                   stateNow()));
    CHECK(!transfer.waitForAcknowledgement(stateNow()));
    CHECK(transfer.recordProgress(transfer.manifest().byteSize, stateNow()));
    CHECK(transfer.waitForAcknowledgement(stateNow()));
    CHECK(!transfer.markCompleted(QStringLiteral("wrong"),
                                  QStringLiteral("remote:1"), stateNow()));
    CHECK(!transfer.markCompleted(transfer.snapshot().idempotencyKey,
                                  QStringLiteral("https://signed.invalid/x"),
                                  stateNow()));
    const QString key = transfer.snapshot().idempotencyKey;
    CHECK(transfer.markCompleted(key, QStringLiteral("remote:1"), stateNow()));
    CHECK(transfer.snapshot().state == SstvShareTransferState::Completed);
    CHECK(transfer.markCompleted(key, QStringLiteral("remote:1"), stateNow()));
    CHECK(!transfer.markCompleted(key, QStringLiteral("remote:2"), stateNow()));
    CHECK(!transfer.cancel());
    return true;
}

bool progressCannotAdvanceAfterExpiry()
{
    SstvShareTransfer transfer(validManifest());
    CHECK(transfer.enqueue(stateNow()));
    CHECK(transfer.beginPreparing(stateNow()));
    CHECK(transfer.bindProviderUpload(
        transfer.snapshot().idempotencyKey, QStringLiteral("upload:expiry")));
    CHECK(transfer.beginUploading(stateNow()));
    CHECK(!transfer.recordProgress(
        1U, fixedUtc(QStringLiteral("2026-08-26T00:00:00.000Z"))));
    CHECK(transfer.snapshot().state == SstvShareTransferState::Expired);
    CHECK(transfer.snapshot().bytesTransferred == 0U);
    return true;
}

bool encryptionPauseCancelAndExpiryTransitions()
{
    SstvShareManifestV1 encryptedManifest = validManifest();
    encryptedManifest.encryption.mode = SstvShareEncryptionMode::EndToEnd;
    encryptedManifest.encryption.algorithm = QStringLiteral("aes-256-gcm");
    encryptedManifest.encryption.keyId = QStringLiteral("key:1");
    encryptedManifest.encryption.recipientKeyFingerprint =
        sha256Hex(QByteArray("recipient-key"));
    encryptedManifest.encryption.nonceBase64 = QByteArray(12, 'a').toBase64();
    encryptedManifest.encryption.manifestBoundAsAuthenticatedData = true;
    encryptedManifest.transport.providerCanReadContent = false;
    SstvShareTransfer transfer(encryptedManifest);
    CHECK(transfer.isValid());
    CHECK(transfer.enqueue(stateNow()));
    CHECK(transfer.beginPreparing(stateNow()));
    CHECK(!transfer.beginUploading(stateNow()));
    CHECK(transfer.beginEncrypting(stateNow()));
    CHECK(transfer.bindProviderUpload(
        transfer.snapshot().idempotencyKey, QStringLiteral("upload:e2ee")));
    CHECK(transfer.beginUploading(stateNow()));
    CHECK(transfer.pause(stateNow()));
    CHECK(transfer.snapshot().state == SstvShareTransferState::Paused);
    CHECK(transfer.resume(stateNow()));
    CHECK(transfer.snapshot().state == SstvShareTransferState::Uploading);
    CHECK(transfer.cancel());
    CHECK(transfer.snapshot().state == SstvShareTransferState::Cancelled);

    SstvShareTransfer expired(validManifest());
    CHECK(expired.expireIfNeeded(
        fixedUtc(QStringLiteral("2026-08-26T00:00:00.000Z"))));
    CHECK(expired.snapshot().state == SstvShareTransferState::Expired);
    CHECK(!expired.enqueue(stateNow()));
    return true;
}

bool retryClassificationBackoffAndLimitsAreDeterministic()
{
    SstvShareRetryPolicy policy;
    policy.baseDelayMs = 1'000;
    policy.maximumDelayMs = 8'000;
    policy.maximumProviderRetryAfterMs = 10'000;
    policy.maximumRetries = 2U;
    policy.jitterPermille = 200U;

    SstvShareTransfer transfer(validManifest(), policy);
    CHECK(transfer.enqueue(stateNow()));
    CHECK(transfer.beginPreparing(stateNow()));
    CHECK(transfer.bindProviderUpload(
        transfer.snapshot().idempotencyKey, QStringLiteral("upload:retry")));
    CHECK(transfer.beginUploading(stateNow()));
    CHECK(transfer.recordProgress(200U, stateNow()));
    const qint64 firstDelay = transfer.deterministicRetryDelayMs(1U);
    CHECK(firstDelay >= 800 && firstDelay <= 1'200);
    CHECK(firstDelay == transfer.deterministicRetryDelayMs(1U));
    CHECK(transfer.handleFailure(SstvShareProviderFailure::Offline, stateNow()));
    CHECK(transfer.snapshot().state == SstvShareTransferState::RetryScheduled);
    CHECK(transfer.snapshot().retryCount == 1U);
    CHECK(!transfer.activateScheduledRetry(
        transfer.snapshot().retryAtUtc.addMSecs(-1)));
    CHECK(transfer.activateScheduledRetry(transfer.snapshot().retryAtUtc));
    CHECK(transfer.snapshot().state == SstvShareTransferState::Uploading);
    CHECK(transfer.snapshot().bytesTransferred == 200U);

    CHECK(transfer.handleFailure(SstvShareProviderFailure::RateLimited,
                                 stateNow(), 7'000));
    CHECK(stateNow().msecsTo(transfer.snapshot().retryAtUtc) >= 7'000);
    CHECK(transfer.activateScheduledRetry(transfer.snapshot().retryAtUtc));
    CHECK(transfer.handleFailure(SstvShareProviderFailure::TransientNetwork,
                                 stateNow()));
    CHECK(transfer.snapshot().state == SstvShareTransferState::Failed);

    for (SstvShareProviderFailure permanent : {
             SstvShareProviderFailure::Authentication,
             SstvShareProviderFailure::Authorization,
             SstvShareProviderFailure::Validation,
             SstvShareProviderFailure::Integrity,
             SstvShareProviderFailure::TlsValidation,
             SstvShareProviderFailure::PermanentProviderFailure}) {
        SstvShareTransfer failed(validManifest(), policy);
        CHECK(failed.enqueue(stateNow()));
        CHECK(failed.handleFailure(permanent, stateNow()));
        CHECK(failed.snapshot().state == SstvShareTransferState::Failed);
        CHECK(failed.snapshot().retryCount == 0U);
    }
    SstvShareTransfer rejected(validManifest(), policy);
    CHECK(rejected.enqueue(stateNow()));
    CHECK(rejected.handleFailure(SstvShareProviderFailure::RejectedRecipient,
                                 stateNow()));
    CHECK(rejected.snapshot().state == SstvShareTransferState::Rejected);
    return true;
}

bool persistenceRoundTripAndRestartResumeAreBounded()
{
    SstvShareTransfer original(validManifest());
    CHECK(original.enqueue(stateNow()));
    CHECK(original.beginPreparing(stateNow()));
    CHECK(original.bindProviderUpload(
        original.snapshot().idempotencyKey, QStringLiteral("upload:restart")));
    CHECK(original.beginUploading(stateNow()));
    CHECK(original.recordProgress(777U, stateNow()));
    SstvShareValidationError error;
    const QByteArray persisted = original.toPersistenceJson(&error);
    CHECK(error.ok());
    CHECK(!persisted.isEmpty());
    CHECK(persisted.size() < kMaximumPersistenceJsonBytes);
    CHECK(!persisted.contains("http"));

    const auto exact = restoreSstvShareTransfer(persisted, stateNow(), false);
    CHECK(exact.ok());
    CHECK(exact.transfer->snapshot().state == SstvShareTransferState::Uploading);
    CHECK(exact.transfer->snapshot().bytesTransferred == 777U);
    CHECK(exact.transfer->snapshot().providerUploadId
          == QStringLiteral("upload:restart"));
    CHECK(exact.transfer->snapshot().idempotencyKey
          == original.snapshot().idempotencyKey);

    const auto recovered = restoreSstvShareTransfer(persisted, stateNow(), true);
    CHECK(recovered.ok());
    CHECK(recovered.transfer->snapshot().state
          == SstvShareTransferState::RetryScheduled);
    CHECK(recovered.transfer->snapshot().retryResumeState
          == SstvShareTransferState::Uploading);
    CHECK(recovered.transfer->snapshot().restartRecoveries == 1U);
    CHECK(recovered.transfer->snapshot().bytesTransferred == 777U);
    CHECK(recovered.transfer->snapshot().providerUploadId
          == QStringLiteral("upload:restart"));
    SstvShareTransfer resumed = *recovered.transfer;
    CHECK(resumed.activateScheduledRetry(stateNow()));
    CHECK(resumed.snapshot().state == SstvShareTransferState::Uploading);

    const QByteArray tamperedKey = mutateObject(
        persisted, [](QJsonObject& object) {
            object.insert(QStringLiteral("idempotencyKey"), QString(64, QLatin1Char('0')));
        });
    const auto keyResult = restoreSstvShareTransfer(tamperedKey, stateNow());
    CHECK(!keyResult.ok());
    CHECK(keyResult.error.code
          == SstvShareValidationCode::InvalidIdempotencyKey);

    const QByteArray unknownField = mutateObject(
        persisted, [](QJsonObject& object) {
            object.insert(QStringLiteral("signedUrl"),
                          QStringLiteral("https://invalid.example/secret"));
        });
    const auto unknownResult = restoreSstvShareTransfer(unknownField, stateNow());
    CHECK(!unknownResult.ok());
    CHECK(unknownResult.error.code == SstvShareValidationCode::UnknownField);

    const QByteArray unknownState = mutateObject(
        persisted, [](QJsonObject& object) {
            object.insert(QStringLiteral("state"), QStringLiteral("Teleporting"));
        });
    const auto stateResult = restoreSstvShareTransfer(unknownState, stateNow());
    CHECK(!stateResult.ok());
    CHECK(stateResult.error.code == SstvShareValidationCode::InvalidState);

    const QByteArray invalidWaitingProgress = mutateObject(
        persisted, [](QJsonObject& object) {
            object.insert(QStringLiteral("state"),
                          QStringLiteral("WaitingForAcknowledgement"));
        });
    const auto waitingResult = restoreSstvShareTransfer(
        invalidWaitingProgress, stateNow(), false);
    CHECK(!waitingResult.ok());
    CHECK(waitingResult.error.code == SstvShareValidationCode::InvalidState);

    const QByteArray unexpectedRetryTimestamp = mutateObject(
        persisted, [](QJsonObject& object) {
            object.insert(QStringLiteral("retryAtUtc"),
                          QStringLiteral("2026-08-24T11:00:00.000Z"));
        });
    const auto retryTimestampResult = restoreSstvShareTransfer(
        unexpectedRetryTimestamp, stateNow(), false);
    CHECK(!retryTimestampResult.ok());
    CHECK(retryTimestampResult.error.code
          == SstvShareValidationCode::InvalidState);

    const QByteArray unknownPersistenceVersion = mutateObject(
        persisted, [](QJsonObject& object) {
            object.insert(QStringLiteral("persistenceVersion"), 2);
        });
    const auto versionResult = restoreSstvShareTransfer(
        unknownPersistenceVersion, stateNow());
    CHECK(!versionResult.ok());
    CHECK(versionResult.error.code
          == SstvShareValidationCode::UnknownProtocolVersion);

    const QByteArray extremePersistenceNumber = mutateObject(
        persisted, [](QJsonObject& object) {
            QJsonObject manifest = object.value(QStringLiteral("manifest")).toObject();
            manifest.insert(QStringLiteral("byteSize"), 1.0e19);
            object.insert(QStringLiteral("manifest"), manifest);
        });
    const auto extremeResult = restoreSstvShareTransfer(
        extremePersistenceNumber, stateNow());
    CHECK(!extremeResult.ok());

    const auto oversizeResult = restoreSstvShareTransfer(
        QByteArray(kMaximumPersistenceJsonBytes + 1, 'x'), stateNow());
    CHECK(!oversizeResult.ok());
    CHECK(oversizeResult.error.code == SstvShareValidationCode::JsonTooLarge);
    return true;
}

bool providerCompatibilityRequiresTlsAndDeclaredCapabilities()
{
    const SstvShareManifestV1 manifest = validManifest();
    SstvShareProviderCapabilities capabilities;
    capabilities.strictTlsRequired = true;
    capabilities.chunkedUpload = true;
    capabilities.maximumChunkBytes = 1'024U;
    CHECK(validateShareProviderCompatibility(
              manifest, QStringLiteral("local-test"), capabilities).ok());

    capabilities.strictTlsRequired = false;
    CHECK(validateShareProviderCompatibility(
              manifest, QStringLiteral("local-test"), capabilities).code
          == SstvShareValidationCode::InvalidTransportSecurity);
    capabilities.strictTlsRequired = true;
    capabilities.chunkedUpload = false;
    CHECK(validateShareProviderCompatibility(
              manifest, QStringLiteral("local-test"), capabilities).code
          == SstvShareValidationCode::InvalidChunkCount);

    SstvShareManifestV1 encrypted = manifest;
    encrypted.encryption.mode = SstvShareEncryptionMode::EndToEnd;
    encrypted.encryption.algorithm = QStringLiteral("aes-256-gcm");
    encrypted.encryption.keyId = QStringLiteral("key:provider-test");
    encrypted.encryption.recipientKeyFingerprint =
        sha256Hex(QByteArray("provider-test-key"));
    encrypted.encryption.nonceBase64 = QByteArray(12, 'p').toBase64();
    encrypted.encryption.manifestBoundAsAuthenticatedData = true;
    encrypted.transport.providerCanReadContent = false;
    capabilities.chunkedUpload = true;
    CHECK(encrypted.validate().ok());
    CHECK(validateShareProviderCompatibility(
              encrypted, QStringLiteral("local-test"), capabilities).code
          == SstvShareValidationCode::InvalidEncryption);
    capabilities.endToEndEncryptionEnvelope = true;
    CHECK(validateShareProviderCompatibility(
              encrypted, QStringLiteral("local-test"), capabilities).ok());
    return true;
}

bool recipientAndIncomingMetadataAreBounded()
{
    SstvShareRecipientRecord recipient;
    recipient.providerId = QStringLiteral("provider:one");
    recipient.stableRecipientId = QStringLiteral("recipient:one");
    recipient.displayCallsign = QStringLiteral("9H1XYZ");
    recipient.displayName = QStringLiteral("Synthetic Recipient");
    recipient.publicEncryptionKey = QStringLiteral("synthetic-public-key");
    recipient.publicKeyFingerprint =
        sha256Hex(recipient.publicEncryptionKey.toUtf8());
    recipient.lastUsedUtc = stateNow();
    CHECK(validateShareRecipientRecord(recipient).ok());
    recipient.displayName = QStringLiteral("https://untrusted.invalid/name");
    CHECK(validateShareRecipientRecord(recipient).code
          == SstvShareValidationCode::InvalidText);
    recipient.displayName = QStringLiteral("Synthetic Recipient");
    recipient.publicKeyFingerprint.clear();
    CHECK(validateShareRecipientRecord(recipient).code
          == SstvShareValidationCode::InvalidEncryption);

    SstvShareIncomingItem incoming;
    incoming.opaqueId = QStringLiteral("incoming:1");
    SstvShareManifestV1 incomingManifest = validManifest(
        deterministicPayload(1'024));
    incomingManifest.providerId = QStringLiteral("provider:one");
    incomingManifest.senderId = QStringLiteral("sender:one");
    incoming.providerId = incomingManifest.providerId;
    incoming.senderId = incomingManifest.senderId;
    incoming.canonicalManifestJson = incomingManifest.toCanonicalJson();
    incoming.manifestSha256 = sha256Hex(incoming.canonicalManifestJson);
    incoming.byteSize = incomingManifest.byteSize;
    incoming.receivedUtc = stateNow();
    incoming.expiresUtc = incomingManifest.expiresUtc;
    CHECK(validateShareIncomingItem(incoming).ok());
    incoming.opaqueId = QStringLiteral("https://signed.invalid/object");
    CHECK(validateShareIncomingItem(incoming).code
          == SstvShareValidationCode::InvalidIdentifier);
    incoming.opaqueId = QStringLiteral("incoming:1");
    incoming.byteSize = kMaximumSharedImageBytes + 1U;
    CHECK(validateShareIncomingItem(incoming).code
          == SstvShareValidationCode::InvalidByteSize);
    return true;
}

bool retryPausePersistencePreservesSchedule()
{
    SstvShareTransfer transfer(validManifest());
    CHECK(transfer.enqueue(stateNow()));
    CHECK(transfer.handleFailure(SstvShareProviderFailure::Offline, stateNow()));
    const QDateTime retryAt = transfer.snapshot().retryAtUtc;
    CHECK(transfer.pause(stateNow()));
    CHECK(transfer.snapshot().pausedResumeState
          == SstvShareTransferState::RetryScheduled);
    const QByteArray persisted = transfer.toPersistenceJson();
    const auto restored = restoreSstvShareTransfer(persisted, stateNow(), true);
    CHECK(restored.ok());
    SstvShareTransfer resumed = *restored.transfer;
    CHECK(resumed.snapshot().state == SstvShareTransferState::Paused);
    CHECK(resumed.resume(stateNow()));
    CHECK(resumed.snapshot().state == SstvShareTransferState::RetryScheduled);
    CHECK(resumed.snapshot().retryAtUtc == retryAt);
    return true;
}

class DeterministicLocalProvider final : public SstvShareProvider
{
public:
    QString providerId() const override { return QStringLiteral("local-test"); }
    SstvShareProviderCapabilities capabilities() const override
    {
        SstvShareProviderCapabilities value;
        value.recipientLookup = true;
        value.chunkedUpload = true;
        value.resumableUpload = true;
        value.acknowledgement = true;
        value.strictTlsRequired = false; // test process has no transport.
        value.maximumChunkBytes = 1'024U;
        return value;
    }
    SstvShareAuthenticationStatus authenticationStatus() const override
    {
        return SstvShareAuthenticationStatus::NotRequired;
    }
    SstvShareOperationId lookupRecipientAsync(
        const QString& stableRecipientId,
        SstvShareRecipientCompletion completion) override
    {
        SstvShareRecipientRecord recipient;
        if (stableRecipientId == QStringLiteral("recipient:blocked")) {
            return finishRecipient(std::move(completion),
                SstvShareProviderResult::failure(
                SstvShareProviderFailure::RejectedRecipient,
                QStringLiteral("recipient rejected")), {});
        }
        if (!isSafeShareIdentifier(stableRecipientId)) {
            return finishRecipient(std::move(completion),
                SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("invalid recipient")), {});
        }
        recipient.providerId = providerId();
        recipient.stableRecipientId = stableRecipientId;
        recipient.verification = SstvShareRecipientVerification::ProviderVerified;
        return finishRecipient(std::move(completion),
                               SstvShareProviderResult::success(),
                               std::move(recipient));
    }
    SstvShareOperationId createUploadAsync(
        const SstvShareManifestV1& manifest,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override
    {
        if (manifest.providerId != providerId()
            || !manifest.validate(true, stateNow()).ok()
            || idempotencyKey != SstvShareTransfer::deriveIdempotencyKey(manifest)) {
            return finish(std::move(completion), SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("invalid create request")));
        }
        if (m_created) {
            if (idempotencyKey != m_idempotencyKey) {
                return finish(std::move(completion), SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Conflict,
                    QStringLiteral("idempotency conflict")));
            }
            return finish(std::move(completion),
                          SstvShareProviderResult::success(handle()));
        }
        m_created = true;
        m_manifest = manifest;
        m_idempotencyKey = idempotencyKey;
        return finish(std::move(completion),
                      SstvShareProviderResult::success(handle()));
    }
    SstvShareOperationId uploadChunkAsync(
        const SstvShareUploadHandle& supplied,
        quint64 offset,
        const QByteArray& chunk,
        const QString& chunkSha256,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) override
    {
        if (!validHandle(supplied) || offset != static_cast<quint64>(m_bytes.size())
            || chunk.isEmpty() || chunk.size() > 1'024
            || chunkSha256 != sha256Hex(chunk)
            || static_cast<quint64>(chunk.size())
                   > m_manifest.byteSize - static_cast<quint64>(m_bytes.size())) {
            return finish(std::move(completion), SstvShareProviderResult::failure(
                chunkSha256 != sha256Hex(chunk)
                    ? SstvShareProviderFailure::Integrity
                    : SstvShareProviderFailure::Validation,
                QStringLiteral("invalid local chunk")));
        }
        m_bytes += chunk;
        if (progress) {
            progress(static_cast<quint64>(chunk.size()),
                     static_cast<quint64>(chunk.size()));
        }
        return finish(std::move(completion),
                      SstvShareProviderResult::success(handle()));
    }
    SstvShareOperationId resumeUploadAsync(
        const SstvShareUploadHandle& supplied,
        SstvShareProviderCompletion completion) override
    {
        return finish(std::move(completion), validHandle(supplied)
            ? SstvShareProviderResult::success(handle())
            : SstvShareProviderResult::failure(
                  SstvShareProviderFailure::NotFound,
                  QStringLiteral("unknown local handle")));
    }
    SstvShareOperationId completeUploadAsync(
        const SstvShareUploadHandle& supplied,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override
    {
        if (m_completed && validHandle(supplied)
            && idempotencyKey == m_idempotencyKey) {
            return finish(std::move(completion),
                          SstvShareProviderResult::success(handle()));
        }
        if (!validHandle(supplied) || idempotencyKey != m_idempotencyKey
            || static_cast<quint64>(m_bytes.size()) != m_manifest.byteSize
            || sha256Hex(m_bytes) != m_manifest.sha256) {
            return finish(std::move(completion), SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("local completion hash mismatch")));
        }
        m_completed = true;
        return finish(std::move(completion),
                      SstvShareProviderResult::success(handle()));
    }
    SstvShareOperationId cancelUploadAsync(
        const SstvShareUploadHandle& supplied,
        SstvShareProviderCompletion completion) override
    {
        if (!validHandle(supplied)) {
            return finish(std::move(completion), SstvShareProviderResult::failure(
                SstvShareProviderFailure::NotFound, QStringLiteral("not found")));
        }
        m_cancelled = true;
        return finish(std::move(completion),
                      SstvShareProviderResult::success(handle()));
    }
    SstvShareOperationId queryStatusAsync(
        const SstvShareUploadHandle& supplied,
        SstvShareProviderCompletion completion) override
    {
        return finish(std::move(completion), validHandle(supplied)
            ? SstvShareProviderResult::success(handle())
            : SstvShareProviderResult::failure(
                  SstvShareProviderFailure::NotFound, QStringLiteral("not found")));
    }
    SstvShareOperationId downloadAsync(
        const QString&, quint64, quint64, SstvShareProgressCallback,
        SstvShareProviderCompletion completion) override
    {
        return finish(std::move(completion), SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("test provider has no incoming items")));
    }
    SstvShareOperationId acknowledgeAsync(
        const QString&, SstvShareProviderCompletion completion) override
    {
        return finish(std::move(completion), SstvShareProviderResult::success());
    }
    SstvShareOperationId rejectAsync(
        const QString&, SstvShareProviderCompletion completion) override
    {
        return finish(std::move(completion), SstvShareProviderResult::success());
    }
    SstvShareOperationId deleteIncomingAsync(
        const QString&, SstvShareProviderCompletion completion) override
    {
        return finish(std::move(completion), SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("test provider has no incoming deletion")));
    }
    SstvShareOperationId blockSenderAsync(
        const QString&, SstvShareProviderCompletion completion) override
    {
        return finish(std::move(completion), SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("test provider has no sender blocking")));
    }
    SstvShareOperationId revokeAsync(
        const SstvShareUploadHandle& supplied,
        SstvShareProviderCompletion completion) override
    {
        return cancelUploadAsync(supplied, std::move(completion));
    }
    SstvShareOperationId deleteRemoteObjectAsync(
        const QString& opaqueId,
        SstvShareProviderCompletion completion) override
    {
        return finish(std::move(completion),
            opaqueId == QStringLiteral("local-upload:1")
            ? SstvShareProviderResult::success()
            : SstvShareProviderResult::failure(
                  SstvShareProviderFailure::NotFound, QStringLiteral("not found")));
    }
    SstvShareOperationId refreshCredentialsAsync(
        SstvShareProviderCompletion completion) override
    {
        return finish(std::move(completion), SstvShareProviderResult::success());
    }
    SstvShareOperationId listIncomingAsync(
        qsizetype maximumItems,
        SstvShareIncomingCompletion completion) override
    {
        if (maximumItems < 0 || maximumItems > 1'000) {
            return finishIncoming(std::move(completion),
                SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("invalid list bound")), {});
        }
        return finishIncoming(std::move(completion),
                              SstvShareProviderResult::success(), {});
    }
    bool cancelOperation(SstvShareOperationId) override
    {
        return false;
    }

private:
    SstvShareOperationId finish(SstvShareProviderCompletion completion,
                                SstvShareProviderResult result)
    {
        const SstvShareOperationId id = m_nextOperationId++;
        if (completion) {
            completion(std::move(result));
        }
        return id;
    }
    SstvShareOperationId finishRecipient(
        SstvShareRecipientCompletion completion,
        SstvShareProviderResult result,
        SstvShareRecipientRecord recipient)
    {
        const SstvShareOperationId id = m_nextOperationId++;
        if (completion) {
            completion(std::move(result), std::move(recipient));
        }
        return id;
    }
    SstvShareOperationId finishIncoming(
        SstvShareIncomingCompletion completion,
        SstvShareProviderResult result,
        QVector<SstvShareIncomingItem> items)
    {
        const SstvShareOperationId id = m_nextOperationId++;
        if (completion) {
            completion(std::move(result), std::move(items));
        }
        return id;
    }
    SstvShareUploadHandle handle() const
    {
        return {QStringLiteral("local-upload:1"),
                static_cast<quint64>(m_bytes.size())};
    }
    bool validHandle(const SstvShareUploadHandle& supplied) const
    {
        return m_created && !m_cancelled
            && supplied.opaqueId == QStringLiteral("local-upload:1")
            && !containsNetworkUrl(supplied.opaqueId);
    }

    bool m_created {false};
    bool m_completed {false};
    bool m_cancelled {false};
    SstvShareManifestV1 m_manifest;
    QString m_idempotencyKey;
    QByteArray m_bytes;
    SstvShareOperationId m_nextOperationId {1U};
};

SstvShareProviderResult captureResult(
    const std::function<SstvShareOperationId(SstvShareProviderCompletion)>& start)
{
    SstvShareProviderResult result;
    bool completed = false;
    const SstvShareOperationId operationId = start(
        [&](SstvShareProviderResult value) {
            result = std::move(value);
            completed = true;
        });
    if (operationId == 0U || !completed) {
        return SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("test provider did not complete synchronously"));
    }
    return result;
}

bool abstractProviderContractWorksWithTestScopedLocalProvider()
{
    const QByteArray payload = deterministicPayload();
    const SstvShareManifestV1 manifest = validManifest(payload);
    const QString key = SstvShareTransfer::deriveIdempotencyKey(manifest);
    DeterministicLocalProvider provider;
    CHECK(provider.providerId() == QStringLiteral("local-test"));
    CHECK(provider.capabilities().resumableUpload);
    SstvShareRecipientRecord recipient;
    SstvShareProviderResult recipientResult;
    provider.lookupRecipientAsync(
        manifest.recipientId,
        [&](SstvShareProviderResult value, SstvShareRecipientRecord record) {
            recipientResult = std::move(value);
            recipient = std::move(record);
        });
    CHECK(recipientResult.ok());
    CHECK(recipient.stableRecipientId == manifest.recipientId);
    provider.lookupRecipientAsync(
        QStringLiteral("recipient:blocked"),
        [&](SstvShareProviderResult value, SstvShareRecipientRecord) {
            recipientResult = std::move(value);
        });
    CHECK(!recipientResult.ok());

    const auto created = captureResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(manifest, key, std::move(done));
    });
    CHECK(created.ok());
    CHECK(isSafeShareIdentifier(created.handle().opaqueId));
    CHECK(!containsNetworkUrl(created.handle().opaqueId));
    CHECK(captureResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(manifest, key, std::move(done));
    }).ok());
    CHECK(!captureResult([&](SstvShareProviderCompletion done) {
        return provider.uploadChunkAsync(created.handle(), 1U,
            payload.left(1'024), sha256Hex(payload.left(1'024)), {},
            std::move(done));
    }).ok());
    CHECK(!captureResult([&](SstvShareProviderCompletion done) {
        return provider.uploadChunkAsync(created.handle(), 0U,
            payload.left(1'024), QString(64, QLatin1Char('0')), {},
            std::move(done));
    }).ok());

    const QByteArray first = payload.left(1'024);
    const QByteArray second = payload.mid(1'024);
    auto chunkResult = captureResult([&](SstvShareProviderCompletion done) {
        return provider.uploadChunkAsync(created.handle(), 0U, first,
                                         sha256Hex(first), {}, std::move(done));
    });
    CHECK(chunkResult.ok());
    CHECK(chunkResult.handle().committedBytes == 1'024U);
    CHECK(captureResult([&](SstvShareProviderCompletion done) {
        return provider.resumeUploadAsync(chunkResult.handle(), std::move(done));
    }).handle().committedBytes
          == 1'024U);
    chunkResult = captureResult([&](SstvShareProviderCompletion done) {
        return provider.uploadChunkAsync(chunkResult.handle(), 1'024U, second,
                                         sha256Hex(second), {}, std::move(done));
    });
    CHECK(chunkResult.ok());
    CHECK(chunkResult.handle().committedBytes
          == static_cast<quint64>(payload.size()));
    CHECK(captureResult([&](SstvShareProviderCompletion done) {
        return provider.completeUploadAsync(chunkResult.handle(), key,
                                            std::move(done));
    }).ok());
    CHECK(captureResult([&](SstvShareProviderCompletion done) {
        return provider.completeUploadAsync(chunkResult.handle(), key,
                                            std::move(done));
    }).ok());
    return true;
}

bool hostileByteInputsStayBoundedAndNeverCrash()
{
    std::uint32_t state = 0xc001d00dU;
    for (int caseIndex = 0; caseIndex < 2'000; ++caseIndex) {
        const qsizetype size = caseIndex % 2 == 0
            ? caseIndex % 257
            : caseIndex % 4'097;
        QByteArray bytes(size, Qt::Uninitialized);
        for (qsizetype i = 0; i < size; ++i) {
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            bytes[i] = static_cast<char>(state & 0xffU);
        }
        const auto parsed = parseSstvShareManifestV1(bytes);
        if (parsed.ok()) {
            CHECK(parsed.manifest->toCanonicalJson().size()
                  <= kMaximumManifestJsonBytes);
        }
    }
    return true;
}

} // namespace

int main()
{
    const std::vector<std::pair<const char*, bool (*)()>> tests {
        {"privacyDefaultsArePrivateAndOptIn", privacyDefaultsArePrivateAndOptIn},
        {"canonicalManifestRoundTripIsStable", canonicalManifestRoundTripIsStable},
        {"parserRejectsSizeDepthShapeAndUnknownFields", parserRejectsSizeDepthShapeAndUnknownFields},
        {"parserRejectsPathUrlAndInvalidContentMetadata", parserRejectsPathUrlAndInvalidContentMetadata},
        {"expiryTimestampPrivacyAndTransportAreEnforced", expiryTimestampPrivacyAndTransportAreEnforced},
        {"encryptionMetadataPreventsSilentDowngrade", encryptionMetadataPreventsSilentDowngrade},
        {"sanitizationAndRedactionDoNotExposeSecrets", sanitizationAndRedactionDoNotExposeSecrets},
        {"transitionTableRejectsSkippedAndTerminalMoves", transitionTableRejectsSkippedAndTerminalMoves},
        {"idempotencyKeyBindsManifestRecipientAndPayload", idempotencyKeyBindsManifestRecipientAndPayload},
        {"transferHappyPathAndIdempotentCompletion", transferHappyPathAndIdempotentCompletion},
        {"progressCannotAdvanceAfterExpiry", progressCannotAdvanceAfterExpiry},
        {"encryptionPauseCancelAndExpiryTransitions", encryptionPauseCancelAndExpiryTransitions},
        {"retryClassificationBackoffAndLimitsAreDeterministic", retryClassificationBackoffAndLimitsAreDeterministic},
        {"persistenceRoundTripAndRestartResumeAreBounded", persistenceRoundTripAndRestartResumeAreBounded},
        {"retryPausePersistencePreservesSchedule", retryPausePersistencePreservesSchedule},
        {"providerCompatibilityRequiresTlsAndDeclaredCapabilities", providerCompatibilityRequiresTlsAndDeclaredCapabilities},
        {"recipientAndIncomingMetadataAreBounded", recipientAndIncomingMetadataAreBounded},
        {"abstractProviderContractWorksWithTestScopedLocalProvider", abstractProviderContractWorksWithTestScopedLocalProvider},
        {"hostileByteInputsStayBoundedAndNeverCrash", hostileByteInputsStayBoundedAndNeverCrash},
    };

    int failed = 0;
    for (const auto& test : tests) {
        const bool passed = test.second();
        std::cout << (passed ? "PASS " : "FAIL ") << test.first << '\n';
        failed += passed ? 0 : 1;
    }
    std::cout << "checks=" << g_checks << " tests=" << tests.size()
              << " failed=" << failed << '\n';
    return failed == 0 ? 0 : 1;
}
