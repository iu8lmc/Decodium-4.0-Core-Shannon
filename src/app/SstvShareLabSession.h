// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// This helper is invoked only by --test-mode with an isolated keychain and a
// loopback HTTPS fixture. It exercises the same controller and queue as QML.
#include "src/sstv/models/SstvShareController.h"
#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#if defined(Q_OS_MACOS)
#include "src/security/MacKeychainBackend.h"
#endif

class SstvShareLabSession final : public QObject
{
public:
    SstvShareLabSession(decodium::sstv::SstvShareController* share,
                        QJsonObject config, QObject* parent)
        : QObject(parent), share_(share), config_(std::move(config))
    {
        timer_.setInterval(200);
        connect(&timer_, &QTimer::timeout, this, [this] { tick(); });
        connect(share_, &decodium::sstv::SstvShareController::operationFinished,
                this, [this](const QString& op, bool ok, const QString& detail) {
            if (!ok && op == QStringLiteral("sharing-pin") && phase_ == Pinning
                && !path("corruptPinMarker").isEmpty()
                && detail == QStringLiteral("Stored recipient key is corrupt")) {
                QFile marker(path("corruptPinMarker"));
                if (!marker.open(QIODevice::WriteOnly | QIODevice::NewOnly)
                    || marker.write("rejected\n") != 9) {
                    fail(QStringLiteral("corrupt-pin marker failed")); return;
                }
                if (!labCorruptPin(false, false)) { fail(QStringLiteral("corrupt-pin preservation check failed")); return; }
                qInfo() << "[LAB][SHARE SESSION] corrupt recipient pin rejected";
                phase_ = WaitPeer;
                return;
            }
            if (!ok && op == QStringLiteral("sender-signing-pin") && phase_ == PinningSender
                && !path("corruptPinMarker").isEmpty()
                && detail == QStringLiteral("Stored sender key is corrupt")) {
                QFile marker(path("corruptPinMarker"));
                if (!marker.open(QIODevice::WriteOnly | QIODevice::NewOnly)
                    || marker.write("rejected\n") != 9) {
                    fail(QStringLiteral("corrupt sender-pin marker failed")); return;
                }
                if (!labCorruptPin(false, true)) { fail(QStringLiteral("corrupt sender-pin preservation check failed")); return; }
                qInfo() << "[LAB][SHARE SESSION] corrupt sender signing pin rejected";
                phase_ = WaitSigningPeer;
                return;
            }
            if (!ok && op == QStringLiteral("accept") && policyStage_ == 2) {
                if (!detail.contains(QStringLiteral("validation"))) {
                    fail(QStringLiteral("unexpected policy rejection: ") + detail); return;
                }
                qInfo() << "[LAB][SHARE SESSION] pending unsigned acceptance rejected by sender policy";
                policyStage_ = 3;
                if (!share_->setRequireIncomingSenderAuthentication(false)) fail(QStringLiteral("policy disable queue failed"));
                return;
            }
            if (!ok && op == QStringLiteral("download") && !path("replayTarget").isEmpty()
                && detail == QStringLiteral("incoming envelope already queued or accepted")) {
                replayRejected_ = true;
                qInfo() << "[LAB][SHARE SESSION] duplicate encrypted envelope rejected";
                return;
            }
            if (!ok && op == QStringLiteral("download") && !path("replayTarget").isEmpty()
                && config_.value(QStringLiteral("replayExpectInvalidSignature")).toBool()
                && detail == QStringLiteral("incoming sender signature could not be authenticated")) {
                replayRejected_ = true;
                qInfo() << "[LAB][SHARE SESSION] replay sender signature rejected";
                return;
            }
            if (!ok && op == QStringLiteral("download") && !path("invalidSignatureMarker").isEmpty()
                && detail == QStringLiteral("incoming sender signature could not be authenticated")) {
                QFile marker(path("invalidSignatureMarker"));
                const QByteArray markerData = QJsonDocument(QJsonObject {
                    {QStringLiteral("storageFolder"), share_->storageFolder()}}).toJson(QJsonDocument::Compact);
                if (marker.exists() || !marker.open(QIODevice::WriteOnly | QIODevice::NewOnly)
                    || marker.write(markerData) != markerData.size()) { fail(QStringLiteral("invalid-signature marker failed")); return; }
                qInfo() << "[LAB][SHARE SESSION] invalid sender signature rejected before download";
                return;
            }
            if (!ok && op == QStringLiteral("refresh-inbox")
                && detail == QStringLiteral("inbox provider is busy")) {
                // A download can temporarily occupy the fixture's only slot.
                // Retry the laboratory poll instead of mistaking that for an
                // integrity result; the session deadline still bounds it.
                refreshing_ = false;
                lastRefresh_ = elapsed_.elapsed();
                qInfo() << "[LAB][SHARE SESSION] inbox busy; polling will retry";
                return;
            }
            if (!ok) { fail(op + QStringLiteral(": ") + detail); return; }
            if (op == QStringLiteral("configure") && phase_ == Configuring) phase_ = Enable;
            else if (op == QStringLiteral("enable") && phase_ == Enabling) phase_ = WaitCapability;
            else if (op == QStringLiteral("sharing-pin") && phase_ == Pinning) phase_ = UploadFirst;
            else if ((op == QStringLiteral("sender-signing-pin") || op == QStringLiteral("sender-signing-replace")) && phase_ == PinningSender) {
                if (op == QStringLiteral("sender-signing-replace"))
                    qInfo() << "[LAB][SHARE SESSION] verified sender signing pin replaced";
                QSaveFile ready(path("signingReady"));
                if (!ready.open(QIODevice::WriteOnly) || ready.write("ready\n") != 6 || !ready.commit()) {
                    fail(QStringLiteral("cannot publish signing readiness")); return;
                }
                phase_ = Receive;
            }
            else if (op == QStringLiteral("upload") && phase_ == UploadingFirst)
                phase_ = config_.value(QStringLiteral("replaceRecipient")).toBool() ? ReplacePeer : UploadSecond;
            else if (op == QStringLiteral("sharing-replace-pin") && phase_ == ReplacingPeer) {
                qInfo() << "[LAB][SHARE SESSION] recipient pin replaced between images";
                phase_ = UploadSecond;
            }
            else if (op == QStringLiteral("upload") && phase_ == UploadingSecond) phase_ = WaitUploads;
            else if (op == QStringLiteral("refresh-inbox")) refreshing_ = false;
            else if (op == QStringLiteral("accept") && acceptedQueued_) acceptedDone_ = true;
        });
        connect(share_, &decodium::sstv::SstvShareController::sharingIdentityReady,
                this, [this](const QString& key, const QString& fingerprint) {
            if (phase_ == RotationPending) {
                if (fingerprint == originalFingerprint_) { fail(QStringLiteral("rotation did not change identity")); return; }
                QSaveFile rotated(path("identity") + QStringLiteral(".rotated"));
                const auto data = QJsonDocument(QJsonObject {{"publicKey", key}, {"fingerprint", fingerprint}}).toJson(QJsonDocument::Compact);
                if (!rotated.open(QIODevice::WriteOnly) || rotated.write(data) != data.size() || !rotated.commit()) {
                    fail(QStringLiteral("cannot publish rotated identity")); return;
                }
                qInfo() << "[LAB][SHARE SESSION] receiver identity rotated before transfer";
                phase_ = WaitSigningPeer;
                return;
            }
            if (phase_ != IdentityPending) return;
            QSaveFile file(path("identity"));
            const QByteArray data = QJsonDocument(QJsonObject {{"publicKey", key},
                {"fingerprint", fingerprint}}).toJson(QJsonDocument::Compact);
            if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
                fail(QStringLiteral("cannot publish identity")); return;
            }
            if (config_.value(QStringLiteral("rotateRecipient")).toBool()) {
                originalFingerprint_ = fingerprint;
                phase_ = RotationPending;
                if (!signedLab() || !share_->rotateSharingIdentity(fingerprint))
                    fail(QStringLiteral("rotation queue failed"));
            } else phase_ = signedLab() ? WaitSigningPeer : Receive;
        });
        connect(share_, &decodium::sstv::SstvShareController::signingIdentityReady,
                this, [this](const QString& key, const QString& fingerprint) {
            if (phase_ == SigningIdentityPending && config_.value(QStringLiteral("rotateSenderSigning")).toBool()) {
                if (fingerprint != path("oldSigningFingerprint")) {
                    fail(QStringLiteral("sender signing fingerprint changed before rotation")); return;
                }
                phase_ = SigningRotationPending;
                if (!share_->rotateSigningIdentity(fingerprint)) fail(QStringLiteral("signing rotation queue failed"));
                return;
            }
            if (phase_ != SigningIdentityPending && phase_ != SigningRotationPending) return;
            if (phase_ == SigningRotationPending) {
                if (fingerprint == path("oldSigningFingerprint")) { fail(QStringLiteral("signing rotation did not change identity")); return; }
                qInfo() << "[LAB][SHARE SESSION] sender signing identity rotated";
            }
            QSaveFile file(path("signingIdentity"));
            const QByteArray data = QJsonDocument(QJsonObject {{"publicKey", key},
                {"fingerprint", fingerprint}}).toJson(QJsonDocument::Compact);
            if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
                fail(QStringLiteral("cannot publish signing identity")); return;
            }
            if (config_.value(QStringLiteral("signingIdentityOnly")).toBool()) { success(); return; }
            phase_ = WaitPeer;
        });
        elapsed_.start();
        timer_.start();
    }

private:
    enum Phase { WaitReady, Configure, Configuring, Enable, Enabling,
                 WaitCapability, IdentityPending, RotationPending, SigningIdentityPending, SigningRotationPending, WaitSigningPeer, PinningSender, WaitPeer, Pinning,
                 UploadFirst, UploadingFirst, ReplacePeer, ReplacingPeer, UploadSecond, UploadingSecond,
                 WaitUploads, Receive, Finished };
    QString path(const char* name) const { return config_.value(QLatin1String(name)).toString(); }
    bool signedLab() const {
        return QStandardPaths::isTestModeEnabled() && qEnvironmentVariableIntValue("DECODIUM_LAB_SIGNED_ENVELOPE") == 1;
    }
    bool labCorruptPin(bool inject, bool signingPin) {
#if defined(Q_OS_MACOS)
        if (!signedLab() || path("corruptPinMarker").isEmpty()
            || (path("corruptPinCase") != QStringLiteral("zero") && path("corruptPinCase") != QStringLiteral("one"))) return false;
        const QFileInfo keychain(QCoreApplication::instance()->property("decodiumLabShareKeychain").toString());
        const QString temp = QFileInfo(QDir::tempPath()).canonicalFilePath();
        const QString directory = keychain.dir().canonicalPath();
        const QString expectedFile = signingPin ? QStringLiteral("receiver.keychain-db") : QStringLiteral("sender.keychain-db");
        if (!keychain.isAbsolute() || !keychain.isFile() || keychain.isSymLink() || keychain.canonicalFilePath().isEmpty()
            || keychain.fileName() != expectedFile || temp.isEmpty() || directory.isEmpty()
            || !directory.startsWith(temp + QLatin1Char('/'))
            || !QFileInfo(directory).fileName().startsWith(QStringLiteral("decodium-share-transfer-"))
            || directory != QFileInfo(path("identity")).dir().canonicalPath()) return false;
        secure_settings::MacKeychainBackend backend(keychain.canonicalFilePath());
        const QByteArray profile = signingPin ? QByteArrayLiteral("LABRX") : QByteArrayLiteral("LABTX");
        const auto service = QStringLiteral("Decodium.SSTV.E2EE.v1.")
            + QString::fromLatin1(QCryptographicHash::hash(profile, QCryptographicHash::Sha256).toHex());
        const QByteArray peer = QByteArrayLiteral("rest-encrypted") + QByteArray(1, '\0')
            + (signingPin ? QByteArrayLiteral("labtx") : QByteArrayLiteral("labrx"));
        const auto account = (signingPin ? QStringLiteral("sender-p256-") : QStringLiteral("peer-"))
            + QString::fromLatin1(QCryptographicHash::hash(peer, QCryptographicHash::Sha256).toHex());
        QByteArray bytes(signingPin ? 91 : 32, '\0');
        if (path("corruptPinCase") == QStringLiteral("one")) bytes[0] = '\1';
        const auto encoded = QString::fromLatin1(bytes.toBase64());
        const auto current = backend.lookup(service, account);
        if (inject) return !current.found && backend.store(service, account, encoded)
            && backend.lookup(service, account).value == encoded;
        return current.found && current.value == encoded && backend.remove(service, account)
            && !backend.lookup(service, account).found;
#else
        Q_UNUSED(inject)
        Q_UNUSED(signingPin)
        return false;
#endif
    }
    void fail(const QString& error) {
        if (phase_ == Finished) return;
        phase_ = Finished; timer_.stop();
        qCritical().noquote() << "[LAB][SHARE SESSION]" << error;
        QCoreApplication::exit(2);
    }
    void success() {
        if (phase_ == Finished) return;
        QSaveFile file(path("result"));
        const QString resultId = expiredId_.isEmpty() ? acceptedId_ : expiredId_;
        const QByteArray data = resultId.isEmpty() ? QByteArrayLiteral("ok\n")
            : QJsonDocument(QJsonObject {{"transferId", resultId},
                                         {"storageFolder", share_->storageFolder()}})
                  .toJson(QJsonDocument::Compact);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            fail(QStringLiteral("cannot write session result")); return;
        }
        phase_ = Finished; timer_.stop();
        qInfo() << "[LAB][SHARE SESSION] completed";
        QCoreApplication::exit(0);
    }
    static QVariant field(QAbstractItemModel* model, int row, const QByteArray& name) {
        const auto roles = model->roleNames();
        const int role = roles.key(name, -1);
        return role < 0 ? QVariant {} : model->data(model->index(row, 0), role);
    }
    bool queueUpload(const QString& image) {
        const QString inputRoot = QDir(share_->storageFolder()).filePath(QStringLiteral("lab-input"));
        if (!QDir().mkpath(inputRoot)) return false;
        const QString localImage = QDir(inputRoot).filePath(QFileInfo(image).fileName());
        if (QFileInfo::exists(localImage) || !QFile::copy(image, localImage)) return false;
        const QVariantMap options {{"expiryHours", 24}, {"includeCallsign", false},
            {"callsign", QString {}}, {"includeGrid", false}, {"grid", QString {}},
            {"meteredNetworkAllowed", true}};
        return signedLab()
            ? share_->uploadSignedEncryptedWithOptions(QUrl::fromLocalFile(localImage),
                path("recipient"), QStringLiteral("lab"), QString {}, true, options)
            : share_->uploadEncryptedWithOptions(QUrl::fromLocalFile(localImage),
                path("recipient"), QStringLiteral("lab"), QString {}, true, options);
    }
    void tick() {
        if (elapsed_.elapsed() > 90'000) { fail(QStringLiteral("session timeout")); return; }
        if (!share_ || phase_ == Finished) return;
        if (phase_ == WaitCapability && elapsed_.elapsed() > 8'000 && !capabilityReported_) {
            capabilityReported_ = true;
            qWarning() << "[LAB][SHARE SESSION] waiting for encrypted capability"
                       << "ready" << share_->ready() << "enabled" << share_->enabled()
                       << "configured" << share_->configured()
                       << "secureStorage" << share_->secureStorageAvailable()
                       << "inbox" << share_->providerSupportsInbox()
                       << "busy" << share_->busy() << "error" << share_->errorString();
        }
        if (phase_ == WaitReady && share_->ready()) {
            phase_ = config_.value(QStringLiteral("resumeExistingProvider")).toBool()
                ? WaitCapability : Configure;
        }
        if (phase_ == Configure) {
            // Synthetic credentials are used only by this isolated test helper.
            const QString labToken = qEnvironmentVariable("DECODIUM_LAB_SHARE_BEARER");
            const bool authenticatedLab = !labToken.isEmpty();
            const QVariantMap cfg {{"type", "rest"}, {"providerId", "rest-encrypted"},
                {"endpoint", path("endpoint")}, {"createPath", authenticatedLab ? "/api/v1/transfers" : "/api/uploads"},
                {"chunkPath", authenticatedLab ? "/api/v1/transfers/{uploadId}/chunks" : "/api/uploads/{uploadId}/content"},
                {"statusPath", authenticatedLab ? "/api/v1/transfers/{uploadId}" : "/api/uploads/{uploadId}/status"},
                {"completePath", authenticatedLab ? "/api/v1/transfers/{uploadId}/complete" : "/api/uploads/{uploadId}/complete"},
                {"cancelPath", authenticatedLab ? "/api/v1/transfers/{uploadId}" : "/api/uploads/{uploadId}"},
                {"authType", "bearer"}, {"credentialsRequired", authenticatedLab}, {"secret", labToken}};
            phase_ = Configuring;
            if (!share_->configureProvider(cfg)) fail(QStringLiteral("configure queue failed"));
        } else if (phase_ == Enable) {
            phase_ = Enabling;
            if (!share_->setEnabled(true)) fail(QStringLiteral("enable queue failed"));
        } else if (phase_ == WaitCapability && share_->encryptedSharingAvailable()) {
            if (path("role") == QStringLiteral("receiver")) {
                if (!share_->setRequireIncomingEncryption(true)) {
                    fail(QStringLiteral("encryption policy queue failed")); return;
                }
                if (signedLab() && path("policyTarget").isEmpty()
                    && !share_->setRequireIncomingSenderAuthentication(true)) {
                    fail(QStringLiteral("sender authentication policy queue failed")); return;
                }
                phase_ = IdentityPending;
                if (!share_->createSharingIdentity()) fail(QStringLiteral("identity queue failed"));
            } else if (signedLab()) {
                phase_ = SigningIdentityPending;
                if (!share_->createSigningIdentity()) fail(QStringLiteral("signing identity queue failed"));
            } else phase_ = WaitPeer;
        } else if (phase_ == WaitSigningPeer) {
            QFile file(path("signingIdentity"));
            if (!file.exists()) return;
            if (!file.open(QIODevice::ReadOnly) || file.size() > 4096) { fail(QStringLiteral("invalid signing identity")); return; }
            const QJsonObject identity = QJsonDocument::fromJson(file.readAll()).object();
            if (!path("corruptPinMarker").isEmpty() && !corruptPinInjected_) {
                if (!labCorruptPin(true, true)) { fail(QStringLiteral("corrupt sender-pin injection failed")); return; }
                corruptPinInjected_ = true;
            }
            phase_ = PinningSender;
            const auto oldFingerprint = path("oldSigningFingerprint");
            const bool queued = oldFingerprint.isEmpty()
                ? share_->pinSenderSigningIdentity(QStringLiteral("labtx"), identity.value("publicKey").toString(),
                                                   identity.value("fingerprint").toString(), true)
                : share_->replaceSenderSigningIdentity(QStringLiteral("labtx"), oldFingerprint,
                    identity.value("publicKey").toString(), identity.value("fingerprint").toString(), true);
            if (!queued)
                fail(QStringLiteral("sender pin queue failed"));
        } else if (phase_ == WaitPeer) {
            if (config_.value(QStringLiteral("reuseExistingPin")).toBool()) {
                qInfo() << "[LAB][SHARE SESSION] reusing stored recipient pin";
                phase_ = UploadFirst;
                return;
            }
            if (signedLab() && !QFileInfo::exists(path("signingReady"))) return;
            QFile file(path("identity"));
            if (!file.exists()) return;
            if (!file.open(QIODevice::ReadOnly) || file.size() > 4096) { fail(QStringLiteral("invalid peer identity")); return; }
            const QJsonObject identity = QJsonDocument::fromJson(file.readAll()).object();
            if (!path("corruptPinMarker").isEmpty() && !corruptPinInjected_) {
                if (!labCorruptPin(true, false)) { fail(QStringLiteral("corrupt-pin injection failed")); return; }
                corruptPinInjected_ = true;
            }
            phase_ = Pinning;
            if (!share_->pinSharingRecipient(path("recipient"), identity.value("publicKey").toString(),
                                             identity.value("fingerprint").toString(), true))
                fail(QStringLiteral("pin queue failed"));
        } else if (phase_ == ReplacePeer) {
            QFile oldFile(path("identity")), newFile(path("identity") + QStringLiteral(".rotated"));
            if (!oldFile.open(QIODevice::ReadOnly) || !newFile.open(QIODevice::ReadOnly)
                || oldFile.size() > 4096 || newFile.size() > 4096) { fail(QStringLiteral("missing replacement identity")); return; }
            const auto oldIdentity = QJsonDocument::fromJson(oldFile.readAll()).object();
            const auto newIdentity = QJsonDocument::fromJson(newFile.readAll()).object();
            phase_ = ReplacingPeer;
            if (!share_->replaceSharingRecipient(path("recipient"), oldIdentity.value("fingerprint").toString(),
                    newIdentity.value("publicKey").toString(), newIdentity.value("fingerprint").toString(), true))
                fail(QStringLiteral("replacement pin queue failed"));
        } else if (phase_ == UploadFirst || phase_ == UploadSecond) {
            const bool first = phase_ == UploadFirst;
            phase_ = first ? UploadingFirst : UploadingSecond;
            if (!queueUpload(path(first ? "imageA" : "imageB"))) fail(QStringLiteral("upload queue failed"));
        } else if (phase_ == WaitUploads) {
            auto* history = share_->transferHistory();
            int completed = 0;
            for (int i = 0; i < history->rowCount(); ++i) {
                if (field(history, i, "direction").toString() == QStringLiteral("Upload")
                    && field(history, i, "transferState").toString() == QStringLiteral("Completed")) ++completed;
                if (field(history, i, "transferState").toString() == QStringLiteral("Failed"))
                    { fail(QStringLiteral("upload failed: ") + field(history, i, "error").toString()); return; }
            }
            if (completed >= config_.value(QStringLiteral("expectedUploads")).toInt(2)) success();
        } else if (phase_ == Receive) {
            auto* history = share_->transferHistory();
            for (int i = 0; i < history->rowCount(); ++i) {
                if (!path("expiryTarget").isEmpty()
                    && field(history, i, "fileName").toString()
                        == QStringLiteral("image-") + path("expiryTarget") + QStringLiteral(".png")
                    && field(history, i, "transferState").toString() == QStringLiteral("Expired")) {
                    if (path("expiryTarget") == QStringLiteral("b") && copied_.size() != 1
                        && !(qEnvironmentVariableIntValue("DECODIUM_LAB_EXPIRY_HOLD") == 1
                             && QFileInfo::exists(QDir(path("outputDir")).filePath(QStringLiteral("image-a.png"))))) {
                        fail(QStringLiteral("image A was not received before B expired")); return;
                    }
                    if (config_.value(QStringLiteral("expiryWhileAwaiting")).toBool()) {
                        const QString id = field(history, i, "transferId").toString();
                        const QDir downloads(QDir(share_->storageFolder()).filePath(QStringLiteral("sharing/downloads")));
                        if (id.isEmpty() || QFileInfo::exists(downloads.filePath(QStringLiteral("validated/") + id + QStringLiteral(".png")))
                            || QFileInfo::exists(downloads.filePath(QStringLiteral(".partial/") + id + QStringLiteral(".part")))) {
                            fail(QStringLiteral("expired awaiting files were not removed")); return;
                        }
                    }
                    if (config_.value(QStringLiteral("expiryAfterFirstChunk")).toBool()) {
                        const QString id = field(history, i, "transferId").toString();
                        const QString partial = QDir(share_->storageFolder()).filePath(
                            QStringLiteral("sharing/downloads/.partial/") + id + QStringLiteral(".part"));
                        if (id.isEmpty() || field(history, i, "byteOffset").toULongLong() == 0U
                            || QFileInfo::exists(partial)) {
                            fail(QStringLiteral("expired partial download was not removed")); return;
                        }
                    }
                    qInfo() << "[LAB][SHARE SESSION] download expired" << path("expiryTarget");
                    expiredId_ = field(history, i, "transferId").toString();
                    success(); return;
                }
                if (field(history, i, "transferState").toString() == QStringLiteral("Failed")) {
                    fail(QStringLiteral("download failed: ") + field(history, i, "error").toString());
                    return;
                }
            }
            if (qEnvironmentVariableIntValue("DECODIUM_LAB_EXPIRY_HOLD") == 1
                && !path("expiryMarker").isEmpty() && QFileInfo::exists(path("expiryMarker")))
                return;
            auto* inbox = share_->inbox();
            for (int i = 0; i < inbox->rowCount(); ++i) {
                const QString id = field(inbox, i, "incomingId").toString();
                const QString filename = field(inbox, i, "fileName").toString();
                if (config_.value(QStringLiteral("restartImagesOnly")).toBool()
                    && filename != QStringLiteral("restart-a.png") && filename != QStringLiteral("restart-b.png")) {
                    const QString retiredPrefix = config_.value(QStringLiteral("retiredUnseen")).toBool()
                        ? QStringLiteral("retired-") : QStringLiteral("image-");
                    if (path("replayIncomingIdFile").isEmpty() || replayRejected_
                        || filename != retiredPrefix + path("replayTarget") + QStringLiteral(".png")) continue;
                    QFile replayId(path("replayIncomingIdFile"));
                    if (!replayId.open(QIODevice::ReadOnly) || replayId.size() > 128
                        || QString::fromLatin1(replayId.readAll()).trimmed() != id) continue;
                }
                if (field(inbox, i, "canDownload").toBool() && !queued_.contains(id)) {
                    queued_.insert(id);
                    if (!share_->requireIncomingEncryption()
                        || (signedLab() && path("policyTarget").isEmpty()
                            && !share_->requireIncomingSenderAuthentication())
                        || !share_->download(id, filename)) {
                        fail(QStringLiteral("encrypted download policy or queue failed")); return;
                    }
                }
                const bool policyTarget = !path("policyTarget").isEmpty()
                    && filename == QStringLiteral("image-") + path("policyTarget") + QStringLiteral(".png");
                if (policyTarget && policyStage_ == 1 && share_->requireIncomingSenderAuthentication()) {
                    if (signedLab()) {
                        qInfo() << "[LAB][SHARE SESSION] pending signed acceptance allowed by sender policy";
                        policyStage_ = 4;
                    } else {
                        policyStage_ = 2;
                        if (!share_->accept(policyTransferId_)) fail(QStringLiteral("policy acceptance queue failed"));
                        return;
                    }
                }
                if (policyTarget && policyStage_ == 3 && !share_->requireIncomingSenderAuthentication()) policyStage_ = 4;
                if (policyTarget && policyStage_ > 0 && policyStage_ < 4) continue;
                if (field(inbox, i, "canAccept").toBool() && !copied_.contains(id)) {
                    if (policyTarget && policyStage_ == 0) {
                        policyTransferId_ = field(inbox, i, "transferId").toString();
                        if (policyTransferId_.isEmpty()) { fail(QStringLiteral("policy target missing transfer ID")); return; }
                        policyStage_ = 1;
                        if (!share_->setRequireIncomingSenderAuthentication(true)) fail(QStringLiteral("policy enable queue failed"));
                        return;
                    }
                    const auto handoff = field(inbox, i, "validatedHandoff").toMap();
                    const QString source = handoff.value(QStringLiteral("stagedCanonicalPath")).toString();
                    if (filename != QStringLiteral("image-a.png") && filename != QStringLiteral("image-b.png")
                        && filename != QStringLiteral("restart-a.png") && filename != QStringLiteral("restart-b.png"))
                        { fail(QStringLiteral("unexpected incoming filename")); return; }
                    if (config_.value(QStringLiteral("expiryWhileAwaiting")).toBool()
                        && path("expiryTarget") == QStringLiteral("a")
                        && filename == QStringLiteral("image-b.png")) continue;
                    if (config_.value(QStringLiteral("expiryWhileAwaiting")).toBool()
                        && filename == QStringLiteral("image-") + path("expiryTarget") + QStringLiteral(".png")) {
                        if (source.isEmpty() || !QFileInfo::exists(source)) {
                            fail(QStringLiteral("no validated image before expiry")); return;
                        }
                        if (path("expiryTarget") == QStringLiteral("b") && copied_.size() != 1) return;
                        if (qEnvironmentVariableIntValue("DECODIUM_LAB_EXPIRY_HOLD") == 1) {
                            const QString readyPath = path("expiryMarker") + QStringLiteral(".ready");
                            const QFileInfo readyInfo(readyPath);
                            if (readyInfo.isSymLink()) { fail(QStringLiteral("unsafe expiry ready marker")); return; }
                            if (!readyInfo.exists()) {
                                QFile ready(readyPath);
                                const auto data = QJsonDocument(QJsonObject {
                                    {QStringLiteral("transferId"), field(inbox, i, "transferId").toString()},
                                    {QStringLiteral("storageFolder"), share_->storageFolder()}
                                }).toJson(QJsonDocument::Compact);
                                if (!ready.open(QIODevice::WriteOnly | QIODevice::NewOnly)
                                    || ready.write(data) != data.size()) {
                                    fail(QStringLiteral("cannot signal awaiting expiry")); return;
                                }
                            }
                            continue;
                        }
                        QFile marker(path("expiryMarker"));
                        if (!marker.exists() && (!marker.open(QIODevice::WriteOnly | QIODevice::NewOnly)
                                                 || marker.write("expired\n") != 8)) {
                            fail(QStringLiteral("cannot trigger awaiting expiry")); return;
                        }
                        continue;
                    }
                    const QString dest = QDir(path("outputDir")).filePath(filename);
                    if (source.isEmpty() || !QFile::copy(source, dest)) { fail(QStringLiteral("cannot copy received image")); return; }
                    copied_.insert(id);
                    if (filename == QStringLiteral("image-") + path("acceptTarget") + QStringLiteral(".png")) {
                        acceptedId_ = field(inbox, i, "transferId").toString();
                        if (acceptedId_.isEmpty() || acceptedQueued_ || !share_->accept(acceptedId_)) {
                            fail(QStringLiteral("acceptance queue failed")); return;
                        }
                        acceptedQueued_ = true;
                    }
                }
            }
            bool galleryImported = path("acceptTarget").isEmpty();
            if (acceptedDone_ && !acceptedId_.isEmpty()) {
                const QString galleryRoot = QDir(share_->storageFolder()).filePath(QStringLiteral("images/imported"));
                QDirIterator entries(galleryRoot, {QStringLiteral("*") + acceptedId_ + QStringLiteral(".png")},
                                     QDir::Files, QDirIterator::Subdirectories);
                galleryImported = entries.hasNext();
            }
            const int expectedCopies=config_.value(QStringLiteral("expectedCopies")).toInt(2);
            if (copied_.size() >= expectedCopies && (path("replayTarget").isEmpty() || replayRejected_)
                && galleryImported && (path("policyTarget").isEmpty() || policyStage_ == 4)) { success(); return; }
            if (!refreshing_ && elapsed_.elapsed() - lastRefresh_ >= 1500) {
                refreshing_ = true; lastRefresh_ = elapsed_.elapsed();
                if (!share_->refreshInbox()) fail(QStringLiteral("inbox queue failed"));
            }
        }
    }
    decodium::sstv::SstvShareController* share_;
    QJsonObject config_;
    QTimer timer_;
    QElapsedTimer elapsed_;
    Phase phase_ {WaitReady};
    bool corruptPinInjected_ {false};
    bool capabilityReported_ {false};
    bool refreshing_ {false};
    bool replayRejected_ {false};
    bool acceptedQueued_ {false}, acceptedDone_ {false};
    qint64 lastRefresh_ {-1500};
    QSet<QString> queued_, copied_;
    QString expiredId_, acceptedId_, policyTransferId_, originalFingerprint_;
    int policyStage_ {0};
};
