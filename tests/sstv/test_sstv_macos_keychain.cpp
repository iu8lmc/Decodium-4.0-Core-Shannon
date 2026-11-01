// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/security/MacKeychainBackend.h"
#include "../../src/sstv/sharing/SstvShareKeyStore.h"
#include "../../src/sstv/sharing/SstvShareRecipientEncryption.h"
#include "../../src/sstv/sharing/SstvShareSecureEnvelope.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <Security/Security.h>
#include <cstdio>

using namespace decodium::sstv::sharing;
namespace {
constexpr char kTestPassword[] = "temporary-test-keychain";
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
OSStatus unlock(const QByteArray& path) {
    SecKeychainRef keychain = nullptr;
    const auto opened = SecKeychainOpen(path.constData(), &keychain);
    if (opened != errSecSuccess) return opened;
    const auto status = SecKeychainUnlock(keychain, sizeof(kTestPassword)-1, kTestPassword, true);
    CFRelease(keychain);
    return status;
}
#pragma clang diagnostic pop
int child(const QString& path) {
    if (unlock(path.toUtf8()) != errSecSuccess) return 10;
    secure_settings::MacKeychainBackend backend(path);
    SstvShareKeyStore store(backend, QStringLiteral("shared-profile"));
    const auto key = store.createOrLoadPublicKey();
    if (!key.ok()) return 11;
    const QByteArray output = key.key->toHex();
    std::fwrite(output.constData(), 1, size_t(output.size()), stdout);
    return 0;
}
int receive(const QString& path) {
    if (unlock(path.toUtf8()) != errSecSuccess) return 20;
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) return 21;
    const auto json = QJsonDocument::fromJson(input.readAll()).object();
    const auto parsed = parseSstvShareManifestV1(QByteArray::fromBase64(json.value("manifest").toString().toLatin1()));
    if (!parsed.ok()) return 22;
    secure_settings::MacKeychainBackend backend(path);
    SstvShareKeyStore store(backend, QStringLiteral("local-test"));
    const auto plain = sstvShareOpenStoredEnvelope(store, "test-provider", "peer", *parsed.manifest,
        QByteArray::fromBase64(json.value("payload").toString().toLatin1()));
    if (!plain) return 23;
    const auto digest = QCryptographicHash::hash(*plain, QCryptographicHash::Sha256).toHex();
    std::fwrite(digest.constData(), 1, size_t(digest.size()), stdout);
    return 0;
}
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().size() == 3 && app.arguments().at(1) == "--child")
        return child(app.arguments().at(2));
    if (app.arguments().size() == 3 && app.arguments().at(1) == "--receive")
        return receive(app.arguments().at(2));
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    const QString path = directory.filePath(QStringLiteral("ephemeral.keychain-db"));
    const QByteArray nativePath = path.toUtf8();
    SecKeychainRef keychain = nullptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    SecKeychainSetUserInteractionAllowed(false);
    const OSStatus created = SecKeychainCreate(nativePath.constData(), sizeof(kTestPassword)-1,
                                                kTestPassword, false, nullptr, &keychain);
#pragma clang diagnostic pop
    if (created != errSecSuccess || !keychain) {
        std::fprintf(stderr, "temporary keychain creation failed (%d)\n", int(created)); return 2;
    }
    int result = 0;
    {
        secure_settings::MacKeychainBackend backend(path);
        if (!backend.available() || !backend.supports_private_key_storage()) result = 3;
        SstvShareKeyStore store(backend, QStringLiteral("local-test"));
        const auto first = store.createOrLoadPublicKey();
        const auto second = store.createOrLoadPublicKey();
        if (!first.ok() || !second.ok() || first.key != second.key) result = 4;
        if (first.ok()) {
            const auto fingerprint = sstvShareX25519Fingerprint(*first.key);
            if (store.pinRecipient("test-provider", "peer", *first.key, fingerprint) != SstvShareKeyStoreError::None
                || store.pinnedRecipient("test-provider", "peer").key != first.key) result = 5;
        }
        if (first.ok()) {
            SstvShareKeyStore sender(backend, QStringLiteral("sender-test"));
            const auto fingerprint = sstvShareX25519Fingerprint(*first.key);
            if (sender.pinRecipient("test-provider", "peer", *first.key, fingerprint) != SstvShareKeyStoreError::None) result = 8;
            SstvShareManifestV1 manifest;
            manifest.transferId = QUuid::createUuid();
            manifest.providerId = QStringLiteral("test-provider");
            manifest.senderId = QStringLiteral("sender");
            manifest.recipientId = QStringLiteral("peer");
            manifest.createdUtc = QDateTime::currentDateTimeUtc();
            manifest.expiresUtc = manifest.createdUtc.addDays(1);
            manifest.mediaUtc = manifest.createdUtc;
            manifest.originalFilename = manifest.safeDisplayFilename = QStringLiteral("image.png");
            manifest.mimeType = QStringLiteral("image/png");
            manifest.width = manifest.height = 2;
            manifest.sstvMode = QStringLiteral("Martin M1");
            manifest.privacy.recipientConfirmed = true;
            const QByteArray plaintext("two process encrypted payload fixture");
            const auto encryptedA = sstvShareSealPinnedEnvelope(sender, manifest, plaintext);
            const auto encryptedB = sstvShareSealPinnedEnvelope(sender, manifest, plaintext);
            if (!encryptedA || !encryptedB || encryptedA->bytes == encryptedB->bytes) result = 9;
            else {
                const auto expected = QCryptographicHash::hash(plaintext, QCryptographicHash::Sha256).toHex();
                for (const auto* encrypted : {&*encryptedA, &*encryptedB}) {
                    QProcess receiver;
                    receiver.start(app.applicationFilePath(), {QStringLiteral("--receive"), path});
                    const QJsonObject obj {{"manifest", QString::fromLatin1(encrypted->manifest.toCanonicalJson().toBase64())},
                                           {"payload", QString::fromLatin1(encrypted->bytes.toBase64())}};
                    receiver.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
                    receiver.closeWriteChannel();
                    if (!receiver.waitForFinished(20'000) || receiver.exitStatus() != QProcess::NormalExit
                        || receiver.exitCode() != 0 || receiver.readAllStandardOutput() != expected) result = 12;
                }
            }
        }
        QProcess a, b;
        const QStringList args {QStringLiteral("--child"), path};
        a.start(app.applicationFilePath(), args);
        b.start(app.applicationFilePath(), args);
        const bool doneA = a.waitForFinished(20'000);
        const bool doneB = b.waitForFinished(20'000);
        if (!doneA || !doneB || a.exitStatus() != QProcess::NormalExit || b.exitStatus() != QProcess::NormalExit
            || a.exitCode() != 0 || b.exitCode() != 0 || a.readAllStandardOutput() != b.readAllStandardOutput()) result = 6;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    const auto removed = SecKeychainDelete(keychain);
#pragma clang diagnostic pop
    CFRelease(keychain);
    if (removed != errSecSuccess) { std::fprintf(stderr, "temporary keychain cleanup failed (%d)\n", int(removed)); return 7; }
    if (result) std::fprintf(stderr, "native keychain test failed at stage %d\n", result);
    else std::puts("Isolated native keychain, pins and concurrent process identity passed");
    return result;
}
