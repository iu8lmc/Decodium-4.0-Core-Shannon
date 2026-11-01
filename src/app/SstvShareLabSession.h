// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// This helper is invoked only by --test-mode with an isolated keychain and a
// loopback HTTPS fixture. It exercises the same controller and queue as QML.
#include "src/sstv/models/SstvShareController.h"
#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <QUrl>

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
            if (!ok) { fail(op + QStringLiteral(": ") + detail); return; }
            if (op == QStringLiteral("configure") && phase_ == Configuring) phase_ = Enable;
            else if (op == QStringLiteral("enable") && phase_ == Enabling) phase_ = WaitCapability;
            else if (op == QStringLiteral("sharing-pin") && phase_ == Pinning) phase_ = UploadFirst;
            else if (op == QStringLiteral("upload") && phase_ == UploadingFirst) phase_ = UploadSecond;
            else if (op == QStringLiteral("upload") && phase_ == UploadingSecond) phase_ = WaitUploads;
            else if (op == QStringLiteral("refresh-inbox")) refreshing_ = false;
        });
        connect(share_, &decodium::sstv::SstvShareController::sharingIdentityReady,
                this, [this](const QString& key, const QString& fingerprint) {
            if (phase_ != IdentityPending) return;
            QSaveFile file(path("identity"));
            const QByteArray data = QJsonDocument(QJsonObject {{"publicKey", key},
                {"fingerprint", fingerprint}}).toJson(QJsonDocument::Compact);
            if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
                fail(QStringLiteral("cannot publish identity")); return;
            }
            phase_ = Receive;
        });
        elapsed_.start();
        timer_.start();
    }

private:
    enum Phase { WaitReady, Configure, Configuring, Enable, Enabling,
                 WaitCapability, IdentityPending, WaitPeer, Pinning,
                 UploadFirst, UploadingFirst, UploadSecond, UploadingSecond,
                 WaitUploads, Receive, Finished };
    QString path(const char* name) const { return config_.value(QLatin1String(name)).toString(); }
    void fail(const QString& error) {
        if (phase_ == Finished) return;
        phase_ = Finished; timer_.stop();
        qCritical().noquote() << "[LAB][SHARE SESSION]" << error;
        QCoreApplication::exit(2);
    }
    void success() {
        if (phase_ == Finished) return;
        QSaveFile file(path("result"));
        const QByteArray data = QByteArrayLiteral("ok\n");
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
        return share_->uploadEncryptedWithOptions(QUrl::fromLocalFile(localImage),
            path("recipient"), QStringLiteral("lab"), QString {}, true, options);
    }
    void tick() {
        if (elapsed_.elapsed() > 90'000) { fail(QStringLiteral("session timeout")); return; }
        if (!share_ || phase_ == Finished) return;
        if (phase_ == WaitReady && share_->ready()) phase_ = Configure;
        if (phase_ == Configure) {
            const QVariantMap cfg {{"type", "rest"}, {"providerId", "rest-encrypted"},
                {"endpoint", path("endpoint")}, {"createPath", "/api/uploads"},
                {"chunkPath", "/api/uploads/{uploadId}/content"},
                {"statusPath", "/api/uploads/{uploadId}/status"},
                {"completePath", "/api/uploads/{uploadId}/complete"},
                {"cancelPath", "/api/uploads/{uploadId}"},
                {"authType", "bearer"}, {"credentialsRequired", false}};
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
                phase_ = IdentityPending;
                if (!share_->createSharingIdentity()) fail(QStringLiteral("identity queue failed"));
            } else phase_ = WaitPeer;
        } else if (phase_ == WaitPeer) {
            QFile file(path("identity"));
            if (!file.exists()) return;
            if (!file.open(QIODevice::ReadOnly) || file.size() > 4096) { fail(QStringLiteral("invalid peer identity")); return; }
            const QJsonObject identity = QJsonDocument::fromJson(file.readAll()).object();
            phase_ = Pinning;
            if (!share_->pinSharingRecipient(path("recipient"), identity.value("publicKey").toString(),
                                             identity.value("fingerprint").toString(), true))
                fail(QStringLiteral("pin queue failed"));
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
            if (completed >= 2) success();
        } else if (phase_ == Receive) {
            auto* history = share_->transferHistory();
            for (int i = 0; i < history->rowCount(); ++i) {
                if (field(history, i, "transferState").toString() == QStringLiteral("Failed")) {
                    fail(QStringLiteral("download failed: ") + field(history, i, "error").toString());
                    return;
                }
            }
            auto* inbox = share_->inbox();
            for (int i = 0; i < inbox->rowCount(); ++i) {
                const QString id = field(inbox, i, "incomingId").toString();
                const QString filename = field(inbox, i, "fileName").toString();
                if (field(inbox, i, "canDownload").toBool() && !queued_.contains(id)) {
                    queued_.insert(id);
                    if (!share_->requireIncomingEncryption() || !share_->download(id, filename)) {
                        fail(QStringLiteral("encrypted download policy or queue failed")); return;
                    }
                }
                if (field(inbox, i, "canAccept").toBool() && !copied_.contains(id)) {
                    const auto handoff = field(inbox, i, "validatedHandoff").toMap();
                    const QString source = handoff.value(QStringLiteral("stagedCanonicalPath")).toString();
                    if (filename != QStringLiteral("image-a.png") && filename != QStringLiteral("image-b.png"))
                        { fail(QStringLiteral("unexpected incoming filename")); return; }
                    const QString dest = QDir(path("outputDir")).filePath(filename);
                    if (source.isEmpty() || !QFile::copy(source, dest)) { fail(QStringLiteral("cannot copy received image")); return; }
                    copied_.insert(id);
                }
            }
            if (copied_.size() >= 2) { success(); return; }
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
    bool refreshing_ {false};
    qint64 lastRefresh_ {-1500};
    QSet<QString> queued_, copied_;
};
