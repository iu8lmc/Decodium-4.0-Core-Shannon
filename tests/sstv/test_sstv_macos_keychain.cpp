// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/security/MacKeychainBackend.h"
#include "../../src/sstv/sharing/SstvShareKeyStore.h"
#include "../../src/sstv/sharing/SstvShareKeyAgreement.h"
#include "../../src/sstv/sharing/SstvShareP256.h"
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
#include <utility>

using namespace decodium::sstv::sharing;
namespace {
constexpr char kTestPassword[] = "temporary-test-keychain";
class InterleavedPinBackend final : public secure_settings::Backend {
public:
    InterleavedPinBackend(const secure_settings::Backend& wrapped, QString executable,
                          QString keychainPath, QByteArray next)
        : wrapped_(wrapped), executable_(std::move(executable)), path_(std::move(keychainPath)), next_(std::move(next)) {}
    bool available() const override { return wrapped_.available(); }
    bool supports_private_key_storage() const override { return wrapped_.supports_private_key_storage(); }
    secure_settings::LookupResult lookup(const QString& service, const QString& account) const override {
        return wrapped_.lookup(service, account);
    }
    secure_settings::InsertResult store_if_absent(const QString& service, const QString& account,
                                                 const QString& value) const override {
        const auto result = wrapped_.store_if_absent(service, account, value);
        if (result == secure_settings::InsertResult::Created && account.startsWith(QStringLiteral("peer-"))
            && !account.contains(QStringLiteral("-next-"))) {
            QProcess process;
            const auto original = QByteArray::fromBase64(value.toLatin1());
            process.start(executable_, {QStringLiteral("--replace-pin"), path_,
                                        sstvShareX25519Fingerprint(original), QString::fromLatin1(next_.toHex())});
            replaced = process.waitForFinished(20'000) && process.exitStatus() == QProcess::NormalExit
                && process.exitCode() == 0;
        }
        return result;
    }
    bool store(const QString& service, const QString& account, const QString& value, QString* error) const override {
        return wrapped_.store(service, account, value, error);
    }
    bool remove(const QString& service, const QString& account, QString* error) const override {
        return wrapped_.remove(service, account, error);
    }
    mutable bool replaced {false};
private:
    const secure_settings::Backend& wrapped_;
    QString executable_, path_;
    QByteArray next_;
};
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
int archive(const QString& path) {
    if (unlock(path.toUtf8()) != errSecSuccess) return 30;
    secure_settings::MacKeychainBackend backend(path);
    SstvShareKeyStore store(backend, QStringLiteral("local-test"));
    const auto key = store.archiveCurrentIdentity();
    if (!key.ok()) return 31;
    const auto output = key.key->toHex();
    std::fwrite(output.constData(), 1, size_t(output.size()), stdout);
    return 0;
}
int rotate(const QString& path, const QString& fingerprint) {
    if (unlock(path.toUtf8()) != errSecSuccess) return 30;
    secure_settings::MacKeychainBackend backend(path);
    SstvShareKeyStore store(backend, QStringLiteral("local-test"));
    const auto key = store.rotateIdentity(fingerprint);
    if (key.error == SstvShareKeyStoreError::Conflict) return 32;
    if (!key.ok()) return 33;
    const auto output = key.key->toHex();
    std::fwrite(output.constData(), 1, size_t(output.size()), stdout);
    return 0;
}
int rotateSigning(const QString& path, const QString& fingerprint) {
    if (unlock(path.toUtf8()) != errSecSuccess) return 30;
    secure_settings::MacKeychainBackend backend(path);
    SstvShareKeyStore store(backend, QStringLiteral("local-signing-test"));
    const auto key = store.rotateSigningIdentity(fingerprint);
    if (key.error == SstvShareKeyStoreError::Conflict) return 32;
    if (!key.ok()) return 33;
    const auto output = key.key->toHex();
    std::fwrite(output.constData(), 1, size_t(output.size()), stdout);
    return 0;
}
int replacePin(const QString& path, const QString& oldFingerprint, const QString& publicHex) {
    if (unlock(path.toUtf8()) != errSecSuccess) return 40;
    secure_settings::MacKeychainBackend backend(path);
    SstvShareKeyStore store(backend, QStringLiteral("sender-test"));
    const auto key = QByteArray::fromHex(publicHex.toLatin1());
    const auto result = store.replacePinnedRecipient("test-provider", "peer", oldFingerprint,
        key, sstvShareX25519Fingerprint(key));
    if (result == SstvShareKeyStoreError::Conflict) return 42;
    if (result != SstvShareKeyStoreError::None) return 43;
    std::fwrite(publicHex.toLatin1().constData(), 1, size_t(publicHex.size()), stdout);
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
    if (app.arguments().size() == 3 && app.arguments().at(1) == "--archive")
        return archive(app.arguments().at(2));
    if (app.arguments().size() == 4 && app.arguments().at(1) == "--rotate")
        return rotate(app.arguments().at(2), app.arguments().at(3));
    if (app.arguments().size() == 4 && app.arguments().at(1) == "--rotate-signing")
        return rotateSigning(app.arguments().at(2), app.arguments().at(3));
    if (app.arguments().size() == 5 && app.arguments().at(1) == "--replace-pin")
        return replacePin(app.arguments().at(2), app.arguments().at(3), app.arguments().at(4));
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
        {
            SstvShareKeyStore signing(backend, QStringLiteral("local-signing-test"));
            const auto initial = signing.createOrLoadSigningPublicKey();
            if (!initial.ok()) result = 26;
            else {
                const auto fingerprint = sstvShareP256Fingerprint(*initial.key);
                QProcess a, b;
                const QStringList arguments {QStringLiteral("--rotate-signing"), path, fingerprint};
                a.start(app.applicationFilePath(), arguments);
                b.start(app.applicationFilePath(), arguments);
                const bool doneA = a.waitForFinished(20'000), doneB = b.waitForFinished(20'000);
                const bool aWon = a.exitCode() == 0 && b.exitCode() == 32;
                const bool bWon = b.exitCode() == 0 && a.exitCode() == 32;
                const auto current = signing.createOrLoadSigningPublicKey();
                if (!doneA || !doneB || a.exitStatus() != QProcess::NormalExit || b.exitStatus() != QProcess::NormalExit
                    || (!aWon && !bWon) || !current.ok() || current.key == initial.key
                    || (aWon ? a.readAllStandardOutput() : b.readAllStandardOutput()) != current.key->toHex()) result = 27;
            }
        }
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
            const auto raceA = sstvShareGenerateX25519KeyPair();
            const auto raceB = sstvShareGenerateX25519KeyPair();
            const auto raceService = QStringLiteral("Decodium.SSTV.E2EE.v1.")
                + QString::fromLatin1(QCryptographicHash::hash(QByteArrayLiteral("sender-test"), QCryptographicHash::Sha256).toHex());
            const auto raceAccount = QStringLiteral("peer-")
                + QString::fromLatin1(QCryptographicHash::hash(QByteArray("test-provider\0peer", 18), QCryptographicHash::Sha256).toHex());
            if (!raceA || !raceB) result = 23;
            else for (int direction = 0; direction < 2; ++direction) {
                const auto& original = direction == 0 ? raceA->publicKey : raceB->publicKey;
                const auto& next = direction == 0 ? raceB->publicKey : raceA->publicKey;
                InterleavedPinBackend interleaved(backend, app.applicationFilePath(), path, next);
                SstvShareKeyStore racing(interleaved, QStringLiteral("sender-test"));
                const auto current = racing.pinRecipient("test-provider", "peer", original,
                                                          sstvShareX25519Fingerprint(original));
                if (!interleaved.replaced || current != SstvShareKeyStoreError::Conflict
                    || sender.pinnedRecipient("test-provider", "peer").key != next) result = 24;
                if (!backend.remove(raceService, raceAccount + QStringLiteral("-next-") + sstvShareX25519Fingerprint(original))
                    || !backend.remove(raceService, raceAccount)) result = 25;
            }
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
            const auto service = QStringLiteral("Decodium.SSTV.E2EE.v1.")
                + QString::fromLatin1(QCryptographicHash::hash(QByteArrayLiteral("sender-test"), QCryptographicHash::Sha256).toHex());
            const auto account = QStringLiteral("peer-")
                + QString::fromLatin1(QCryptographicHash::hash(QByteArray("test-provider\0peer", 18), QCryptographicHash::Sha256).toHex());
            const auto validPin = QString::fromLatin1(first.key->toBase64());
            for (const auto& bad : {QByteArray(32, '\0'), QByteArray::fromHex("0100000000000000000000000000000000000000000000000000000000000000")}) {
                const auto encoded = QString::fromLatin1(bad.toBase64());
                if (!backend.remove(service, account) || !backend.store(service, account, encoded)
                    || sender.pinnedRecipient("test-provider", "peer").error != SstvShareKeyStoreError::Corrupt
                    || sender.pinRecipient("test-provider", "peer", *first.key, fingerprint) != SstvShareKeyStoreError::Corrupt
                    || sstvShareSealPinnedEnvelope(sender, manifest, plaintext)
                    || backend.lookup(service, account).value != encoded) result = 13;
                if (!backend.remove(service, account) || !backend.store(service, account, validPin)) result = 14;
            }
            const auto encryptedA = sstvShareSealPinnedEnvelope(sender, manifest, plaintext);
            const auto encryptedB = sstvShareSealPinnedEnvelope(sender, manifest, plaintext);
            if (!encryptedA || !encryptedB || encryptedA->bytes == encryptedB->bytes) result = 9;
            else {
                const auto candidateA = sstvShareGenerateX25519KeyPair();
                const auto candidateB = sstvShareGenerateX25519KeyPair();
                if (!candidateA || !candidateB) result = 20;
                else {
                    QProcess pinA, pinB;
                    pinA.start(app.applicationFilePath(), {QStringLiteral("--replace-pin"), path, fingerprint,
                                                          QString::fromLatin1(candidateA->publicKey.toHex())});
                    pinB.start(app.applicationFilePath(), {QStringLiteral("--replace-pin"), path, fingerprint,
                                                          QString::fromLatin1(candidateB->publicKey.toHex())});
                    const bool doneA = pinA.waitForFinished(20'000), doneB = pinB.waitForFinished(20'000);
                    const bool aWon = pinA.exitCode() == 0 && pinB.exitCode() == 42;
                    const bool bWon = pinB.exitCode() == 0 && pinA.exitCode() == 42;
                    const auto pinned = sender.pinnedRecipient("test-provider", "peer");
                    if (!doneA || !doneB || pinA.exitStatus() != QProcess::NormalExit || pinB.exitStatus() != QProcess::NormalExit
                        || (!aWon && !bWon) || !pinned.ok()
                        || *pinned.key != (aWon ? candidateA->publicKey : candidateB->publicKey)) result = 21;
                    if (!backend.remove(service, account + QStringLiteral("-next-") + fingerprint)
                        || sender.pinnedRecipient("test-provider", "peer").key != first.key) result = 22;
                }
                const auto identityService = QStringLiteral("Decodium.SSTV.E2EE.v1.")
                    + QString::fromLatin1(QCryptographicHash::hash(QByteArrayLiteral("local-test"), QCryptographicHash::Sha256).toHex());
                const auto archiveAccount = QStringLiteral("identity-x25519-v1-archive-") + fingerprint;
                const auto originalIdentity = backend.lookup(identityService, QStringLiteral("identity-x25519-v1"));
                const auto conflicting = QString::fromLatin1(QByteArray(32, 'x').toBase64());
                if (!backend.store(identityService, archiveAccount, conflicting)
                    || store.archiveCurrentIdentity().error != SstvShareKeyStoreError::Corrupt
                    || backend.lookup(identityService, archiveAccount).value != conflicting
                    || backend.lookup(identityService, QStringLiteral("identity-x25519-v1")).value != originalIdentity.value
                    || !backend.remove(identityService, archiveAccount)) result = 19;
                QProcess archiverA, archiverB;
                const QStringList archiveArgs {QStringLiteral("--archive"), path};
                archiverA.start(app.applicationFilePath(), archiveArgs);
                archiverB.start(app.applicationFilePath(), archiveArgs);
                const bool archivedA = archiverA.waitForFinished(20'000);
                const bool archivedB = archiverB.waitForFinished(20'000);
                if (!archivedA || !archivedB || archiverA.exitStatus() != QProcess::NormalExit
                    || archiverB.exitStatus() != QProcess::NormalExit || archiverA.exitCode() != 0
                    || archiverB.exitCode() != 0 || archiverA.readAllStandardOutput() != first.key->toHex()
                    || archiverB.readAllStandardOutput() != first.key->toHex()) result = 18;
                if (!originalIdentity.found
                    || !store.archiveCurrentIdentity().ok()) result = 15;
                QProcess rotateA, rotateB;
                const QStringList rotateArgs {QStringLiteral("--rotate"), path, fingerprint};
                rotateA.start(app.applicationFilePath(), rotateArgs);
                rotateB.start(app.applicationFilePath(), rotateArgs);
                const bool rotatedA = rotateA.waitForFinished(20'000);
                const bool rotatedB = rotateB.waitForFinished(20'000);
                const auto replacement = store.createOrLoadPublicKey();
                const bool aWon = rotateA.exitCode() == 0 && rotateB.exitCode() == 32;
                const bool bWon = rotateB.exitCode() == 0 && rotateA.exitCode() == 32;
                if (!rotatedA || !rotatedB || rotateA.exitStatus() != QProcess::NormalExit
                    || rotateB.exitStatus() != QProcess::NormalExit || (!aWon && !bWon)
                    || !replacement.ok() || replacement.key == first.key
                    || (aWon ? rotateA.readAllStandardOutput() : rotateB.readAllStandardOutput()) != replacement.key->toHex()) result = 16;
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
                if (!backend.remove(identityService, QStringLiteral("identity-x25519-v1-next-") + fingerprint)
                    || !backend.remove(identityService, archiveAccount)
                    || backend.lookup(identityService, QStringLiteral("identity-x25519-v1")).value != originalIdentity.value) result = 17;
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
