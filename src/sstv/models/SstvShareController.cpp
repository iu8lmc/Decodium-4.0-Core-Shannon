// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvShareController.h"

#include "src/security/SecureSettings.hpp"
#include "src/sstv/diagnostics/SstvDiagnosticLogging.h"
#include "src/sstv/sharing/SstvHttpShareProviders.h"
#include "src/sstv/sharing/SstvShareQueueManager.h"
#include "src/sstv/sharing/SstvShareSecurity.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QNetworkInformation>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace decodium::sstv {
namespace {

using namespace decodium::sstv::sharing;

constexpr qsizetype kUiQueryLimit = 100;
constexpr quint64 kHashReadBlockBytes = 256U * 1024U;
constexpr quint64 kRestChunkBytes = 1024U * 1024U;
constexpr int kQueueTickMs = 750;
constexpr qsizetype kMaximumCredentialCharacters = 4'096;

struct UploadPrivacyOptions final
{
    int expiryHours {168};
    bool includeCallsign {false};
    QString callsign;
    bool includeGrid {false};
    QString grid;
    bool meteredNetworkAllowed {false};
};

std::optional<UploadPrivacyOptions> parseUploadPrivacyOptions(
    const QVariantMap& values,
    QString* error)
{
    static const QSet<QString> expectedKeys {
        QStringLiteral("expiryHours"),
        QStringLiteral("includeCallsign"),
        QStringLiteral("callsign"),
        QStringLiteral("includeGrid"),
        QStringLiteral("grid"),
        QStringLiteral("meteredNetworkAllowed"),
    };
    const QStringList suppliedKeyList = values.keys();
    const QSet<QString> suppliedKeys(suppliedKeyList.cbegin(),
                                     suppliedKeyList.cend());
    if (suppliedKeys != expectedKeys) {
        if (error) {
            *error = QStringLiteral("Upload privacy options are incomplete or unknown");
        }
        return {};
    }
    const auto exactBoolean = [&values](const QString& key,
                                        bool* output) {
        const QVariant value = values.value(key);
        if (value.metaType().id() != QMetaType::Bool || !output) {
            return false;
        }
        *output = value.toBool();
        return true;
    };
    const QVariant expiryValue = values.value(QStringLiteral("expiryHours"));
    bool expiryOk = false;
    const int expiryHours = expiryValue.toInt(&expiryOk);
    UploadPrivacyOptions options;
    if (!expiryOk || expiryHours < 1 || expiryHours > 720
        || !exactBoolean(QStringLiteral("includeCallsign"),
                         &options.includeCallsign)
        || !exactBoolean(QStringLiteral("includeGrid"), &options.includeGrid)
        || !exactBoolean(QStringLiteral("meteredNetworkAllowed"),
                         &options.meteredNetworkAllowed)
        || values.value(QStringLiteral("callsign")).metaType().id()
            != QMetaType::QString
        || values.value(QStringLiteral("grid")).metaType().id()
            != QMetaType::QString) {
        if (error) {
            *error = QStringLiteral("Upload privacy option types or bounds are invalid");
        }
        return {};
    }
    options.expiryHours = expiryHours;
    options.callsign = values.value(QStringLiteral("callsign"))
                           .toString().trimmed().toUpper();
    options.grid = values.value(QStringLiteral("grid"))
                       .toString().trimmed().toUpper();
    static const QRegularExpression callsignPattern(
        QStringLiteral(R"(^[A-Z0-9][A-Z0-9/-]{1,22}[A-Z0-9]$)"));
    static const QRegularExpression gridPattern(
        QStringLiteral(R"(^[A-R]{2}[0-9]{2}(?:[A-X]{2}(?:[0-9]{2})?)?$)"));
    if ((options.includeCallsign
         && !callsignPattern.match(options.callsign).hasMatch())
        || (options.includeGrid
            && !gridPattern.match(options.grid).hasMatch())) {
        if (error) {
            *error = QStringLiteral("Included callsign or Maidenhead grid is invalid");
        }
        return {};
    }
    return options;
}

QVariantMap privateUploadDefaults()
{
    return {
        {QStringLiteral("expiryHours"), 168},
        {QStringLiteral("includeCallsign"), false},
        {QStringLiteral("callsign"), QString {}},
        {QStringLiteral("includeGrid"), false},
        {QStringLiteral("grid"), QString {}},
        {QStringLiteral("meteredNetworkAllowed"), false},
    };
}

QString boundedError(const QString& detail)
{
    return redactShareSecrets(detail).left(512);
}

quintptr currentThreadToken() noexcept
{
    return reinterpret_cast<quintptr>(QThread::currentThreadId());
}

QString safeProfile(QString profile)
{
    profile = profile.trimmed().toUpper();
    if (profile.isEmpty()) {
        profile = QStringLiteral("DEFAULT");
    }
    profile.replace(QRegularExpression(QStringLiteral("[^A-Z0-9._-]")),
                    QStringLiteral("_"));
    return profile.left(64);
}

QString senderIdentifier(const QString& profile)
{
    QString value = safeProfile(profile).toLower();
    value.replace(QRegularExpression(QStringLiteral("[^a-z0-9._+-]")),
                  QStringLiteral("_"));
    return isSafeShareIdentifier(value) ? value
                                        : QStringLiteral("decodium-user");
}

QString credentialService(const QString& profile)
{
    return QStringLiteral("org.decodium4.sstv.share.%1")
        .arg(safeProfile(profile));
}

QString credentialAccount(const QString& providerId,
                          const QString& authType,
                          const QString& username)
{
    const QByteArray binding = providerId.toUtf8() + QByteArray(1, '\0')
        + authType.toUtf8() + QByteArray(1, '\0') + username.toUtf8();
    return QStringLiteral("provider-%1").arg(QString::fromLatin1(
        QCryptographicHash::hash(binding, QCryptographicHash::Sha256).toHex()));
}

bool plainHttpsUrl(const QString& text)
{
    const QUrl url(text, QUrl::StrictMode);
    return url.isValid() && url.scheme() == QStringLiteral("https")
        && !url.host().isEmpty() && url.userInfo().isEmpty()
        && url.query().isEmpty() && !url.hasFragment();
}

bool validCredentialText(const QString& value)
{
    return !value.isEmpty() && value.size() <= kMaximumCredentialCharacters
        && !value.contains(QLatin1Char('\0'))
        && !value.contains(QLatin1Char('\r'))
        && !value.contains(QLatin1Char('\n'));
}

struct ProviderConfiguration final
{
    QString type;
    QString providerId;
    QString endpoint;
    QString createPath;
    QString chunkPath;
    QString statusPath;
    QString completePath;
    QString cancelPath;
    QString authType;
    QString username;
    bool credentialsRequired {true};

    QVariantMap toVariantMap(bool credentialStored) const
    {
        return {
            {QStringLiteral("type"), type},
            {QStringLiteral("providerId"), providerId},
            {QStringLiteral("endpoint"), endpoint},
            {QStringLiteral("createPath"), createPath},
            {QStringLiteral("chunkPath"), chunkPath},
            {QStringLiteral("statusPath"), statusPath},
            {QStringLiteral("completePath"), completePath},
            {QStringLiteral("cancelPath"), cancelPath},
            {QStringLiteral("authType"), authType},
            {QStringLiteral("username"), username},
            {QStringLiteral("credentialsRequired"), credentialsRequired},
            {QStringLiteral("credentialStored"), credentialStored},
        };
    }
};

class PreparedCredentialLease final : public SstvShareCredentialLease
{
public:
    PreparedCredentialLease(QString authType,
                            QByteArray username,
                            QByteArray secret)
        : m_authType(std::move(authType))
        , m_username(std::move(username))
        , m_secret(std::move(secret))
    {
    }

    ~PreparedCredentialLease() override
    {
        m_username.fill('\0');
        m_secret.fill('\0');
    }

    bool applyTo(QNetworkRequest& request) const override
    {
        if (m_authType == QStringLiteral("bearer")) {
            request.setRawHeader("Authorization", "Bearer " + m_secret);
            return true;
        }
        if (m_authType == QStringLiteral("basic")) {
            request.setRawHeader(
                "Authorization",
                "Basic " + (m_username + ':' + m_secret).toBase64());
            return true;
        }
        return false;
    }

private:
    QString m_authType;
    mutable QByteArray m_username;
    mutable QByteArray m_secret;
};

class PreparedCredentialSource final : public SstvShareCredentialSource
{
public:
    PreparedCredentialSource(QString authType,
                             QByteArray username,
                             QByteArray secret)
        : m_authType(std::move(authType))
        , m_username(std::move(username))
        , m_secret(std::move(secret))
    {
    }

    ~PreparedCredentialSource() override
    {
        m_username.fill('\0');
        m_secret.fill('\0');
    }

    SstvShareAuthenticationStatus status() const noexcept override
    {
        return m_secret.isEmpty()
            ? SstvShareAuthenticationStatus::CredentialsRequired
            : SstvShareAuthenticationStatus::Authenticated;
    }

    std::shared_ptr<const SstvShareCredentialLease> acquireLease(
        const QString&, SstvShareCredentialPurpose) override
    {
        if (m_secret.isEmpty()) {
            return {};
        }
        return std::make_shared<PreparedCredentialLease>(
            m_authType, m_username, m_secret);
    }

private:
    QString m_authType;
    QByteArray m_username;
    QByteArray m_secret;
};

bool pathWithinRoot(const QString& root, const QString& candidate)
{
    const QFileInfo rootInfo(root);
    const QFileInfo candidateInfo(candidate);
    if (!rootInfo.exists() || !rootInfo.isDir() || rootInfo.isSymLink()
        || !candidateInfo.exists() || !candidateInfo.isFile()
        || candidateInfo.isSymLink()) {
        return false;
    }
    const QString canonicalRoot = QDir::cleanPath(rootInfo.canonicalFilePath());
    const QString canonicalCandidate = QDir::cleanPath(
        candidateInfo.canonicalFilePath());
    if (canonicalRoot.isEmpty() || canonicalCandidate.isEmpty()) {
        return false;
    }
    const QString relative = QDir(canonicalRoot).relativeFilePath(
        canonicalCandidate);
    return relative != QStringLiteral("..")
        && !relative.startsWith(QStringLiteral("../"))
        && !QDir::isAbsolutePath(relative);
}

std::optional<QString> writeSanitizedUpload(const QString& storageRoot,
                                            const QUuid& transferId,
                                            const QImage& decoded,
                                            QString* error)
{
    if (decoded.isNull()) {
        if (error) {
            *error = QStringLiteral("Could not decode the selected SSTV image");
        }
        return {};
    }
    const QString outgoingRoot = QDir(storageRoot).absoluteFilePath(
        QStringLiteral("sharing/outgoing"));
    const QFileInfo before(outgoingRoot);
    if ((before.exists() && (before.isSymLink() || !before.isDir()))
        || (!before.exists() && !QDir().mkpath(outgoingRoot))) {
        if (error) {
            *error = QStringLiteral("Could not create private sharing staging");
        }
        return {};
    }
    const QFileInfo directory(outgoingRoot);
    if (!directory.exists() || !directory.isDir() || directory.isSymLink()
        || directory.canonicalFilePath().isEmpty()) {
        if (error) {
            *error = QStringLiteral("Private sharing staging is unsafe");
        }
        return {};
    }

    // Reconstruct pixels into a new image. QImageReader text/EXIF metadata is
    // deliberately not copied to the payload that leaves Decodium.
    QImage sanitized(decoded.size(), QImage::Format_RGB888);
    if (sanitized.isNull()) {
        if (error) {
            *error = QStringLiteral("Could not allocate sanitized SSTV pixels");
        }
        return {};
    }
    sanitized.fill(Qt::black);
    QPainter painter(&sanitized);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(QPoint(0, 0), decoded);
    painter.end();

    const QString destination = QDir(outgoingRoot).absoluteFilePath(
        transferId.toString(QUuid::WithoutBraces).toLower()
            + QStringLiteral(".png"));
    if (QFileInfo::exists(destination) || QFileInfo(destination).isSymLink()) {
        if (error) {
            *error = QStringLiteral("Private sharing staging name collision");
        }
        return {};
    }
    QSaveFile output(destination);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)
        || !output.setPermissions(QFileDevice::ReadOwner
                                  | QFileDevice::WriteOwner)
        || !sanitized.save(&output, "PNG") || !output.commit()) {
        output.cancelWriting();
        if (error) {
            *error = QStringLiteral("Could not atomically store sanitized upload");
        }
        return {};
    }
    const QFileInfo written(destination);
    if (!written.exists() || !written.isFile() || written.isSymLink()
        || written.size() <= 0
        || static_cast<quint64>(written.size()) > kMaximumSharedImageBytes
        || !pathWithinRoot(outgoingRoot, destination)) {
        QFile::remove(destination);
        if (error) {
            *error = QStringLiteral("Sanitized upload failed its storage bounds");
        }
        return {};
    }
    return written.canonicalFilePath();
}

std::optional<QString> hashFile(const QString& path,
                                quint64 expectedBytes,
                                QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Could not open the selected SSTV image");
        }
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    quint64 total = 0U;
    while (!file.atEnd()) {
        const QByteArray block = file.read(
            static_cast<qint64>(kHashReadBlockBytes));
        if (block.isEmpty() && file.error() != QFileDevice::NoError) {
            if (error) {
                *error = QStringLiteral("Could not read the selected SSTV image");
            }
            return {};
        }
        if (static_cast<quint64>(block.size()) > expectedBytes - total) {
            if (error) {
                *error = QStringLiteral("The selected image changed while reading");
            }
            return {};
        }
        total += static_cast<quint64>(block.size());
        hash.addData(block);
    }
    if (total != expectedBytes) {
        if (error) {
            *error = QStringLiteral("The selected image changed while hashing");
        }
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString manifestFileName(const QByteArray& canonicalManifest)
{
    const auto parsed = parseSstvShareManifestV1(canonicalManifest);
    return parsed.ok() ? parsed.manifest->safeDisplayFilename : QString {};
}

QString manifestMode(const QByteArray& canonicalManifest)
{
    const auto parsed = parseSstvShareManifestV1(canonicalManifest);
    return parsed.ok() ? parsed.manifest->sstvMode : QString {};
}

QVariantMap manifestPresentation(const QByteArray& canonicalManifest)
{
    const auto parsed = parseSstvShareManifestV1(canonicalManifest);
    if (!parsed.ok()) {
        return {};
    }
    const auto& manifest = *parsed.manifest;
    return {
        {QStringLiteral("expiresUtc"), manifest.expiresUtc},
        {QStringLiteral("message"), manifest.message},
        {QStringLiteral("sha256"), manifest.sha256},
        {QStringLiteral("privacy"), QVariantMap {
             {QStringLiteral("publicShare"), manifest.privacy.publicShare},
             {QStringLiteral("recipientConfirmed"),
              manifest.privacy.recipientConfirmed},
             {QStringLiteral("callsignIncluded"),
              manifest.privacy.callsignIncluded},
             {QStringLiteral("gridIncluded"),
              manifest.privacy.gridIncluded},
             {QStringLiteral("meteredNetworkAllowed"),
              manifest.privacy.meteredNetworkAllowed},
             {QStringLiteral("endToEndEncrypted"),
              manifest.encryption.mode == SstvShareEncryptionMode::EndToEnd},
             {QStringLiteral("providerCanReadContent"),
              manifest.transport.providerCanReadContent},
         }},
    };
}

QVariantMap transferRow(
    const SstvManagedTransferRecord& record,
    SstvRemoteCopyAction remoteCopyAction =
        SstvRemoteCopyAction::Unavailable)
{
    const QVariantMap presentation = manifestPresentation(
        record.canonicalManifestJson);
    const double progress = record.byteSize == 0U ? 0.0
        : std::clamp(static_cast<double>(record.byteOffset)
                         / static_cast<double>(record.byteSize),
                     0.0, 1.0);
    const bool pausableDownload =
        record.direction == SstvManagedTransferDirection::Download
        && (record.state == SstvManagedTransferState::DownloadQueued
            || record.state == SstvManagedTransferState::Downloading
            || record.state == SstvManagedTransferState::RetryScheduled);
    const bool pausableUpload =
        record.direction == SstvManagedTransferDirection::Upload
        && record.state != SstvManagedTransferState::Paused
        && !isTerminalManagedTransferState(record.state);
    return {
        {QStringLiteral("transferId"), record.transferId},
        {QStringLiteral("incomingId"), record.incomingId},
        {QStringLiteral("direction"),
         record.direction == SstvManagedTransferDirection::Upload
             ? QStringLiteral("Upload") : QStringLiteral("Download")},
        {QStringLiteral("transferState"), record.cancelRequested
             ? QStringLiteral("CancelPending")
             : sstvManagedTransferStateName(record.state)},
        {QStringLiteral("providerId"), record.providerId},
        {QStringLiteral("peerId"), record.recipientId},
        {QStringLiteral("fileName"), manifestFileName(
             record.canonicalManifestJson)},
        {QStringLiteral("mode"), manifestMode(record.canonicalManifestJson)},
        {QStringLiteral("byteSize"), QVariant::fromValue(record.byteSize)},
        {QStringLiteral("byteOffset"), QVariant::fromValue(record.byteOffset)},
        {QStringLiteral("progress"), progress},
        {QStringLiteral("error"), boundedError(record.lastErrorRedacted)},
        {QStringLiteral("createdUtc"), record.createdUtc},
        {QStringLiteral("updatedUtc"), record.updatedUtc},
        {QStringLiteral("expiresUtc"),
         presentation.value(QStringLiteral("expiresUtc"))},
        {QStringLiteral("canPause"), !record.cancelRequested
             && (pausableUpload || pausableDownload)},
        {QStringLiteral("canResume"), !record.cancelRequested
             && record.state == SstvManagedTransferState::Paused},
        {QStringLiteral("canCancel"), !record.cancelRequested
             && !isTerminalManagedTransferState(record.state)},
        {QStringLiteral("canDownload"), false},
        {QStringLiteral("canAccept"),
         record.state == SstvManagedTransferState::AwaitingAcceptance},
        {QStringLiteral("canAcknowledge"),
         record.state == SstvManagedTransferState::Accepted},
        {QStringLiteral("canReject"), false},
        {QStringLiteral("canSaveAs"), false},
        {QStringLiteral("canDeleteLocalCopy"), false},
        {QStringLiteral("canRequestProviderDeletion"), false},
        {QStringLiteral("canBlockSender"), false},
        {QStringLiteral("blockSenderScope"), QStringLiteral("unavailable")},
        {QStringLiteral("message"),
         presentation.value(QStringLiteral("message"))},
        {QStringLiteral("sha256"),
         presentation.value(QStringLiteral("sha256"))},
        {QStringLiteral("privacySummary"),
         presentation.value(QStringLiteral("privacy"))},
        {QStringLiteral("canRemoveRemoteCopy"),
         remoteCopyAction != SstvRemoteCopyAction::Unavailable},
        {QStringLiteral("remoteCopyAction"),
         sstvRemoteCopyActionName(remoteCopyAction)},
    };
}

QVariantMap handoffMap(const SstvValidatedIncomingHandoff& handoff)
{
    return {
        {QStringLiteral("schemaVersion"), handoff.schemaVersion},
        {QStringLiteral("transferId"), handoff.transferId},
        {QStringLiteral("providerId"), handoff.providerId},
        {QStringLiteral("incomingId"), handoff.incomingId},
        {QStringLiteral("senderId"), handoff.senderId},
        {QStringLiteral("safeDisplayFilename"),
         handoff.safeDisplayFilename},
        {QStringLiteral("sstvMode"), handoff.sstvMode},
        {QStringLiteral("sourceMimeType"), handoff.sourceMimeType},
        {QStringLiteral("sourceSha256"), handoff.sourceSha256},
        {QStringLiteral("sourceByteSize"),
         QVariant::fromValue(handoff.sourceByteSize)},
        {QStringLiteral("stagedCanonicalPath"),
         handoff.stagedCanonicalPath},
        {QStringLiteral("stagedMimeType"), handoff.stagedMimeType},
        {QStringLiteral("stagedSha256"), handoff.stagedSha256},
        {QStringLiteral("stagedByteSize"),
         QVariant::fromValue(handoff.stagedByteSize)},
        {QStringLiteral("width"), handoff.width},
        {QStringLiteral("height"), handoff.height},
        {QStringLiteral("receivedUtc"), handoff.receivedUtc},
        {QStringLiteral("expiresUtc"), handoff.expiresUtc},
    };
}

void addHandoffToRow(QVariantMap& row,
                     const std::optional<SstvValidatedIncomingHandoff>& handoff)
{
    if (!handoff) {
        row.insert(QStringLiteral("previewSource"), QUrl {});
        row.insert(QStringLiteral("validatedHandoff"), QVariantMap {});
        return;
    }
    row.insert(QStringLiteral("previewSource"),
               QUrl::fromLocalFile(handoff->stagedCanonicalPath));
    row.insert(QStringLiteral("validatedHandoff"), handoffMap(*handoff));
}

QVariantMap inboxRow(const SstvPersistentInboxItem& item,
                     const SstvShareProviderCapabilities& capabilities,
                     const QDateTime& nowUtc)
{
    const bool isNew = item.disposition == SstvInboxDisposition::New;
    const bool locallyBlocked = item.disposition
        == SstvInboxDisposition::BlockedLocally;
    const bool providerDeleted = item.disposition
        == SstvInboxDisposition::ProviderDeleted;
    const bool unexpired = item.expiresUtc.isValid()
        && item.expiresUtc > nowUtc;
    const QVariantMap presentation = manifestPresentation(
        item.canonicalManifestJson);
    return {
        {QStringLiteral("transferId"), item.transferId},
        {QStringLiteral("incomingId"), item.incomingId},
        {QStringLiteral("direction"), QStringLiteral("Incoming")},
        {QStringLiteral("transferState"),
         sstvInboxDispositionName(item.disposition)},
        {QStringLiteral("providerId"), item.providerId},
        {QStringLiteral("peerId"), item.senderId},
        {QStringLiteral("fileName"), manifestFileName(
             item.canonicalManifestJson)},
        {QStringLiteral("mode"), manifestMode(item.canonicalManifestJson)},
        {QStringLiteral("byteSize"), QVariant::fromValue(item.byteSize)},
        {QStringLiteral("byteOffset"), QVariant::fromValue(quint64 {0U})},
        {QStringLiteral("progress"), 0.0},
        {QStringLiteral("error"), QString {}},
        {QStringLiteral("createdUtc"), item.receivedUtc},
        {QStringLiteral("updatedUtc"), item.updatedUtc},
        {QStringLiteral("expiresUtc"), item.expiresUtc},
        {QStringLiteral("canPause"), false},
        {QStringLiteral("canResume"), false},
        {QStringLiteral("canCancel"), false},
        {QStringLiteral("canDownload"), isNew && capabilities.incomingList},
        {QStringLiteral("canAccept"),
         item.disposition == SstvInboxDisposition::AwaitingAcceptance},
        {QStringLiteral("canAcknowledge"),
         item.disposition == SstvInboxDisposition::Accepted},
        {QStringLiteral("canReject"),
         capabilities.incomingList
             && (isNew
                 || item.disposition == SstvInboxDisposition::DownloadQueued
                 || item.disposition
                     == SstvInboxDisposition::AwaitingAcceptance)},
        {QStringLiteral("canSaveAs"), false},
        {QStringLiteral("canDeleteLocalCopy"), false},
        {QStringLiteral("canRequestProviderDeletion"),
         capabilities.incomingDelete && !providerDeleted && unexpired},
        {QStringLiteral("canBlockSender"),
         unexpired && (isNew || locallyBlocked)},
        {QStringLiteral("blockSenderScope"),
         capabilities.senderBlocking ? QStringLiteral("provider-or-local")
                                     : QStringLiteral("local-only")},
        {QStringLiteral("message"),
         presentation.value(QStringLiteral("message"))},
        {QStringLiteral("sha256"),
         presentation.value(QStringLiteral("sha256"))},
        {QStringLiteral("privacySummary"),
         presentation.value(QStringLiteral("privacy"))},
        {QStringLiteral("canRemoveRemoteCopy"), false},
        {QStringLiteral("remoteCopyAction"), QStringLiteral("unavailable")},
    };
}

void cleanupGeneratedUpload(const QString& storageRoot,
                            const SstvManagedTransferRecord& record)
{
    if (record.direction != SstvManagedTransferDirection::Upload
        || !isTerminalManagedTransferState(record.state)) {
        return;
    }
    const QString outgoingRoot = QDir(storageRoot).absoluteFilePath(
        QStringLiteral("sharing/outgoing"));
    const QString expected = QDir(outgoingRoot).absoluteFilePath(
        record.transferId + QStringLiteral(".png"));
    if (QDir::cleanPath(record.sourcePath) != QDir::cleanPath(expected)
        || !pathWithinRoot(outgoingRoot, record.sourcePath)) {
        return;
    }
    QFile::remove(record.sourcePath);
}

} // namespace

class SstvShareWorker final : public QObject
{
    Q_OBJECT

public:
    explicit SstvShareWorker(
        const secure_settings::Backend* secureBackend,
        std::shared_ptr<std::atomic<int>> meteredNetworkState,
        std::shared_ptr<SstvShareProvider> localIntegrationProvider = {})
        : m_secureBackend(secureBackend)
        , m_meteredNetworkState(std::move(meteredNetworkState))
        , m_localIntegrationProvider(std::move(localIntegrationProvider))
    {
    }

signals:
    void snapshotReady(QVariantList active,
                       QVariantList history,
                       QVariantList inbox,
                       QVariantMap state);
    void operationResult(QString operation, bool ok, QString message);
    void incomingHandoffResult(QVariantMap handoff);

public slots:
    void initialize(QString storageRoot, QString stationProfile)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (m_shutdown || m_initialized) {
            return;
        }
        const QFileInfo rootInfo(storageRoot);
        if (!rootInfo.exists() || !rootInfo.isDir() || rootInfo.isSymLink()
            || rootInfo.canonicalFilePath().isEmpty()) {
            failOperation(QStringLiteral("initialize"),
                          QStringLiteral("Invalid native SSTV storage root"));
            publishSnapshot();
            return;
        }
        m_storageRoot = QDir::cleanPath(rootInfo.canonicalFilePath());
        m_profile = safeProfile(std::move(stationProfile));
        m_settings = std::make_unique<QSettings>();
        m_secureStorageAvailable = m_secureBackend
            && m_secureBackend->available();
        loadConfiguration();
        m_enabled = m_settings->value(
            QStringLiteral("SSTV/Share/Enabled"), false).toBool();

        QString error;
        if (!rebuildManager(m_enabled, &error)) {
            m_enabled = false;
            m_settings->setValue(QStringLiteral("SSTV/Share/Enabled"), false);
            m_settings->sync();
            m_error = boundedError(error);
            rebuildManager(false, nullptr);
        }
        m_initialized = static_cast<bool>(m_manager);
        if (!m_tick) {
            m_tick = new QTimer(this);
            m_tick->setInterval(kQueueTickMs);
            m_tick->setTimerType(Qt::CoarseTimer);
            connect(m_tick, &QTimer::timeout, this,
                    &SstvShareWorker::processTick);
            m_tick->start();
        }
        if (m_enabled) {
            processTick();
        } else {
            publishSnapshot();
        }
    }

    void setEnabled(bool enabled)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireInitialized(QStringLiteral("enable"))) {
            return;
        }
        if (enabled == m_enabled) {
            finishOperation(QStringLiteral("enable"), true,
                            enabled ? tr("Remote sharing is already enabled")
                                    : tr("Remote sharing is already disabled"));
            return;
        }
        QString error;
        if (!rebuildManager(enabled, &error)) {
            failOperation(QStringLiteral("enable"), error);
            publishSnapshot();
            return;
        }
        m_enabled = enabled;
        m_settings->setValue(QStringLiteral("SSTV/Share/Enabled"), enabled);
        m_settings->sync();
        finishOperation(QStringLiteral("enable"), true,
                        enabled ? tr("Remote sharing enabled")
                                : tr("Remote sharing disabled"));
        if (enabled) {
            processTick();
        } else {
            publishSnapshot();
        }
    }

    void configure(QVariantMap values)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireInitialized(QStringLiteral("configure"))) {
            return;
        }
        if (hasPendingTransfers()) {
            failOperation(QStringLiteral("configure"), tr(
                "Cancel or complete every active transfer before changing provider"));
            publishSnapshot();
            return;
        }

        ProviderConfiguration candidate;
        candidate.type = values.value(QStringLiteral("type")).toString()
            .trimmed().toLower();
        candidate.providerId = values.value(
            QStringLiteral("providerId")).toString().trimmed();
        candidate.endpoint = values.value(
            QStringLiteral("endpoint")).toString().trimmed();
        candidate.createPath = values.value(
            QStringLiteral("createPath")).toString().trimmed();
        candidate.chunkPath = values.value(
            QStringLiteral("chunkPath")).toString().trimmed();
        candidate.statusPath = values.value(
            QStringLiteral("statusPath")).toString().trimmed();
        candidate.completePath = values.value(
            QStringLiteral("completePath")).toString().trimmed();
        candidate.cancelPath = values.value(
            QStringLiteral("cancelPath")).toString().trimmed();
        candidate.authType = values.value(
            QStringLiteral("authType"), QStringLiteral("bearer"))
                                 .toString().trimmed().toLower();
        candidate.username = values.value(
            QStringLiteral("username")).toString().trimmed();
        candidate.credentialsRequired = values.value(
            QStringLiteral("credentialsRequired"), true).toBool();
        QString secret = values.value(QStringLiteral("secret")).toString();

        QString error;
        if (!validateConfiguration(candidate, &error)) {
            secret.fill(QChar('\0'));
            failOperation(QStringLiteral("configure"), error);
            publishSnapshot();
            return;
        }
        if (!secret.isEmpty()) {
            if (!candidate.credentialsRequired) {
                secret.fill(QChar('\0'));
                failOperation(QStringLiteral("configure"), tr(
                    "A credential cannot be saved while authentication is disabled"));
                publishSnapshot();
                return;
            }
            if (!storeCredential(candidate, secret, &error)) {
                secret.fill(QChar('\0'));
                failOperation(QStringLiteral("configure"), error);
                publishSnapshot();
                return;
            }
        }
        secret.fill(QChar('\0'));

        const ProviderConfiguration previous = m_configuration;
        m_configuration = candidate;
        saveConfiguration();
        if (!rebuildManager(m_enabled, &error)) {
            m_configuration = previous;
            saveConfiguration();
            rebuildManager(m_enabled, nullptr);
            failOperation(QStringLiteral("configure"), error);
            publishSnapshot();
            return;
        }
        finishOperation(QStringLiteral("configure"), true,
                        tr("Secure provider configuration saved"));
        publishSnapshot();
    }

    void clearCredentials()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireInitialized(QStringLiteral("credentials"))) {
            return;
        }
        if (!m_secureBackend || !m_secureBackend->available()) {
            failOperation(QStringLiteral("credentials"),
                          tr("Secure credential storage is unavailable"));
            publishSnapshot();
            return;
        }
        if (m_enabled) {
            m_enabled = false;
            m_settings->setValue(QStringLiteral("SSTV/Share/Enabled"), false);
            rebuildManager(false, nullptr);
            m_settings->sync();
        }
        QString error;
        if (!m_secureBackend->remove(
                credentialService(m_profile),
                credentialAccount(m_configuration.providerId,
                                  m_configuration.authType,
                                  m_configuration.username),
                &error)) {
            failOperation(QStringLiteral("credentials"), boundedError(error));
            publishSnapshot();
            return;
        }
        m_credentialStored = false;
        finishOperation(QStringLiteral("credentials"), true,
                        tr("Provider credential removed; sharing is off"));
        publishSnapshot();
    }

    void upload(QString sourcePath,
                QString recipientId,
                QString mode,
                QString message,
                bool recipientConfirmed,
                QVariantMap privacyValues)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireEnabled(QStringLiteral("upload"))) {
            return;
        }
        QString optionError;
        const auto privacyOptions = parseUploadPrivacyOptions(
            privacyValues, &optionError);
        if (!privacyOptions) {
            failOperation(QStringLiteral("upload"), optionError);
            publishSnapshot();
            return;
        }
        if (!recipientConfirmed) {
            failOperation(QStringLiteral("upload"), tr(
                "Confirm the intended recipient before queuing an upload"));
            publishSnapshot();
            return;
        }
        recipientId = recipientId.trimmed();
        mode = sanitizeShareDisplayText(mode.trimmed(), 64);
        message = sanitizeShareDisplayText(message, 1'000, true);
        if (!isSafeShareIdentifier(recipientId) || mode.isEmpty()) {
            failOperation(QStringLiteral("upload"),
                          tr("Recipient or SSTV mode is invalid"));
            publishSnapshot();
            return;
        }
        const QFileInfo info(sourcePath);
        if (!pathWithinRoot(m_storageRoot, sourcePath) || info.size() <= 0
            || static_cast<quint64>(info.size()) > kMaximumSharedImageBytes) {
            failOperation(QStringLiteral("upload"), tr(
                "Choose a bounded PNG or JPEG inside Decodium's SSTV storage"));
            publishSnapshot();
            return;
        }
        QImageReader reader(sourcePath);
        reader.setAutoTransform(true);
        reader.setDecideFormatFromContent(true);
        const QByteArray format = reader.format().toLower();
        const QSize dimensions = reader.size();
        const bool supportedFormat = format == QByteArrayLiteral("png")
            || format == QByteArrayLiteral("jpg")
            || format == QByteArrayLiteral("jpeg");
        if (!supportedFormat || !dimensions.isValid()
            || dimensions.width() > static_cast<int>(kMaximumSharedImageDimension)
            || dimensions.height() > static_cast<int>(kMaximumSharedImageDimension)
            || static_cast<quint64>(dimensions.width())
                > kMaximumSharedImagePixels
                    / static_cast<quint64>(dimensions.height())) {
            failOperation(QStringLiteral("upload"),
                          tr("The selected image format or dimensions are invalid"));
            publishSnapshot();
            return;
        }
        const QImage decoded = reader.read();
        if (decoded.isNull() || decoded.width() <= 0 || decoded.height() <= 0
            || decoded.width()
                > static_cast<int>(kMaximumSharedImageDimension)
            || decoded.height()
                > static_cast<int>(kMaximumSharedImageDimension)
            || static_cast<quint64>(decoded.width())
                > kMaximumSharedImagePixels
                    / static_cast<quint64>(decoded.height())) {
            failOperation(QStringLiteral("upload"),
                          tr("The selected image could not be decoded safely"));
            publishSnapshot();
            return;
        }

        const QUuid transferUuid = QUuid::createUuid();
        QString stagingError;
        const auto stagedPath = writeSanitizedUpload(
            m_storageRoot, transferUuid, decoded, &stagingError);
        if (!stagedPath) {
            failOperation(QStringLiteral("upload"), stagingError);
            publishSnapshot();
            return;
        }
        const QFileInfo stagedInfo(*stagedPath);
        const quint64 bytes = static_cast<quint64>(stagedInfo.size());
        const auto digest = hashFile(*stagedPath, bytes, &stagingError);
        if (!digest) {
            QFile::remove(*stagedPath);
            failOperation(QStringLiteral("upload"), stagingError);
            publishSnapshot();
            return;
        }

        QString fileName = sanitizeShareFilename(
            info.completeBaseName() + QStringLiteral(".png"));
        if (!isSafeShareFilename(fileName)) {
            fileName = QStringLiteral("sstv-image.png");
        }
        const QDateTime now = QDateTime::currentDateTimeUtc();
        SstvShareManifestV1 manifest;
        manifest.transferId = transferUuid;
        manifest.providerId = m_configuration.providerId;
        manifest.senderId = senderIdentifier(m_profile);
        manifest.recipientId = recipientId;
        manifest.createdUtc = now;
        manifest.expiresUtc = now.addSecs(
            static_cast<qint64>(privacyOptions->expiryHours) * 60LL * 60LL);
        manifest.originalFilename = fileName;
        manifest.safeDisplayFilename = fileName;
        manifest.mimeType = QStringLiteral("image/png");
        manifest.byteSize = bytes;
        manifest.sha256 = *digest;
        manifest.width = static_cast<quint32>(decoded.width());
        manifest.height = static_cast<quint32>(decoded.height());
        manifest.sstvMode = mode;
        manifest.mediaUtc = now;
        manifest.callsign.senderCallsign = privacyOptions->includeCallsign
            ? privacyOptions->callsign : QString {};
        manifest.callsign.grid = privacyOptions->includeGrid
            ? privacyOptions->grid : QString {};
        manifest.message = message;
        manifest.privacy.automaticUploadAllowed = false;
        manifest.privacy.automaticIncomingDownloadAllowed = false;
        manifest.privacy.locationIncluded = privacyOptions->includeGrid;
        manifest.privacy.recipientConfirmed = true;
        manifest.privacy.exifRetained = false;
        manifest.privacy.callsignIncluded = privacyOptions->includeCallsign;
        manifest.privacy.gridIncluded = privacyOptions->includeGrid;
        manifest.privacy.publicShare = false;
        manifest.privacy.meteredNetworkAllowed =
            privacyOptions->meteredNetworkAllowed;
        manifest.privacy.explicitExpiry = true;
        manifest.encryption.mode = SstvShareEncryptionMode::TransportTls;
        manifest.encryption.algorithm = QStringLiteral("none");
        manifest.transport.providerCanReadContent = true;
        manifest.chunkCount = m_providerCapabilities.chunkedUpload
            && m_providerCapabilities.maximumChunkBytes > 0U
            ? static_cast<quint32>(std::max<quint64>(
                  1U, (bytes + m_providerCapabilities.maximumChunkBytes - 1U)
                      / m_providerCapabilities.maximumChunkBytes))
            : 1U;
        QString error;
        const QString transferId = m_manager->queueUpload(
            manifest, *stagedPath, &error);
        if (transferId.isEmpty()) {
            QFile::remove(*stagedPath);
            failOperation(QStringLiteral("upload"), error);
            publishSnapshot();
            return;
        }
        m_manager->processDue(&error);
        finishOperation(QStringLiteral("upload"), true,
                        tr("Upload queued for the confirmed recipient"));
        publishSnapshot();
    }

    void refreshInbox()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireEnabled(QStringLiteral("refresh-inbox"))) {
            return;
        }
        if (!m_providerCapabilities.incomingList) {
            discoverRestCapabilities(true);
            return;
        }
        performInboxRefresh(true);
    }

    void performInboxRefresh(bool reportOperation)
    {
        const quint64 generation = m_providerGeneration;
        const QPointer<SstvShareWorker> guard(this);
        m_manager->refreshInboxAsync(
            m_configuration.providerId,
            [guard, generation, reportOperation](
                SstvShareProviderResult result) {
                if (!guard || guard->m_shutdown
                    || guard->m_providerGeneration != generation
                    || !guard->m_manager) {
                    return;
                }
                if (reportOperation) {
                    guard->finishOperation(
                        QStringLiteral("refresh-inbox"), result.ok(),
                        result.ok() ? tr("Incoming inbox refreshed")
                                    : boundedError(result.redactedDiagnostic()));
                } else if (!result.ok()) {
                    guard->m_error = boundedError(
                        result.redactedDiagnostic());
                }
                if (result.ok()) {
                    guard->m_manager->processDue();
                }
                guard->publishSnapshot();
            });
        publishSnapshot();
    }

    void download(QString incomingId, QString destinationFileName)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireEnabled(QStringLiteral("download"))) {
            return;
        }
        destinationFileName = destinationFileName.trimmed();
        if (!isSafeShareIdentifier(incomingId)
            || !isSafeShareFilename(destinationFileName)) {
            failOperation(QStringLiteral("download"),
                          tr("Incoming item or destination filename is invalid"));
            publishSnapshot();
            return;
        }
        QString error;
        const QString id = m_manager->queueDownload(
            m_configuration.providerId, incomingId,
            destinationFileName, &error);
        if (id.isEmpty()) {
            failOperation(QStringLiteral("download"), error);
        } else {
            m_manager->processDue();
            finishOperation(QStringLiteral("download"), true,
                            tr("Download queued; acceptance remains manual"));
        }
        publishSnapshot();
    }

    void accept(QString transferId)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireEnabled(QStringLiteral("accept"))) {
            return;
        }
        QString error;
        const bool ok = m_manager->acceptDownload(transferId, &error);
        const auto handoff = ok
            ? m_manager->validatedIncomingHandoff(transferId, &error)
            : std::optional<SstvValidatedIncomingHandoff> {};
        if (ok && handoff) {
            m_emittedIncomingHandoffs.insert(transferId);
            emit incomingHandoffResult(handoffMap(*handoff));
        }
        finishOperation(
            QStringLiteral("accept"), ok && handoff.has_value(),
            ok && handoff
                ? tr("Validated incoming image queued for native Gallery import")
                : boundedError(error));
        publishSnapshot();
    }

    void acknowledge(QString transferId)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireEnabled(QStringLiteral("acknowledge"))) {
            return;
        }
        const QPointer<SstvShareWorker> guard(this);
        m_manager->acknowledgeDownloadAsync(
            transferId, [guard](SstvShareProviderResult result) {
                if (!guard) {
                    return;
                }
                guard->finishOperation(
                    QStringLiteral("acknowledge"), result.ok(),
                    result.ok() ? tr("Receipt acknowledged")
                                : boundedError(result.redactedDiagnostic()));
                guard->publishSnapshot();
            });
        publishSnapshot();
    }

    void reject(QString incomingId)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireEnabled(QStringLiteral("reject"))) {
            return;
        }
        const QPointer<SstvShareWorker> guard(this);
        m_manager->rejectIncomingAsync(
            m_configuration.providerId, incomingId,
            [guard](SstvShareProviderResult result) {
                if (!guard) {
                    return;
                }
                guard->finishOperation(
                    QStringLiteral("reject"), result.ok(),
                    result.ok() ? tr("Incoming item rejected")
                                : boundedError(result.redactedDiagnostic()));
                guard->publishSnapshot();
            });
        publishSnapshot();
    }

    void saveAs(QString transferId, QString destinationPath)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireInitialized(QStringLiteral("save-as"))) {
            return;
        }
        QString error;
        const bool ok = m_manager->saveValidatedCopy(
            transferId, destinationPath, &error);
        finishOperation(QStringLiteral("save-as"), ok,
                        ok ? tr("Validated incoming image saved as a private PNG")
                           : boundedError(error));
        publishSnapshot();
    }

    void deleteLocalCopy(QString transferId)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireInitialized(QStringLiteral("delete-local-copy"))) {
            return;
        }
        QString error;
        const bool ok = m_manager->deleteLocalCopy(transferId, &error);
        finishOperation(
            QStringLiteral("delete-local-copy"), ok,
            ok ? tr("Private sharing copy deleted; the native Gallery and provider were not changed")
               : boundedError(error));
        publishSnapshot();
    }

    void requestProviderDeletion(QString incomingId)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireEnabled(QStringLiteral("delete-provider-incoming"))) {
            return;
        }
        if (!m_providerCapabilities.incomingDelete) {
            failOperation(QStringLiteral("delete-provider-incoming"), tr(
                "Provider deletion is unavailable without a verified capability"));
            publishSnapshot();
            return;
        }
        const quint64 generation = m_providerGeneration;
        const QPointer<SstvShareWorker> guard(this);
        m_manager->requestIncomingDeletionAsync(
            m_configuration.providerId, incomingId,
            [guard, generation](SstvShareProviderResult result) {
                if (!guard || guard->m_shutdown
                    || guard->m_providerGeneration != generation
                    || !guard->m_manager) {
                    return;
                }
                guard->finishOperation(
                    QStringLiteral("delete-provider-incoming"), result.ok(),
                    result.ok()
                        ? tr("Provider confirmed deletion of its incoming copy; local files were not changed")
                        : boundedError(result.redactedDiagnostic()));
                guard->publishSnapshot();
            });
        publishSnapshot();
    }

    void blockSender(QString incomingId, bool localOnly)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        const QString operation = localOnly
            ? QStringLiteral("block-sender-local")
            : QStringLiteral("block-sender-provider");
        if (localOnly ? !requireInitialized(operation)
                      : !requireEnabled(operation)) {
            return;
        }
        if (!localOnly && !m_providerCapabilities.senderBlocking) {
            failOperation(operation, tr(
                "Provider blocking is unavailable without a verified capability; choose the separate local-only block if desired"));
            publishSnapshot();
            return;
        }
        const quint64 generation = m_providerGeneration;
        const QPointer<SstvShareWorker> guard(this);
        m_manager->blockSenderAsync(
            m_configuration.providerId, incomingId,
            localOnly ? SstvSenderBlockScope::LocalOnly
                      : SstvSenderBlockScope::Provider,
            [guard, generation, localOnly, operation](
                SstvShareProviderResult result) {
                if (!guard || guard->m_shutdown
                    || guard->m_providerGeneration != generation
                    || !guard->m_manager) {
                    return;
                }
                guard->finishOperation(
                    operation, result.ok(),
                    result.ok()
                        ? (localOnly
                               ? tr("Sender blocked only in this Decodium profile; the provider was not contacted")
                               : tr("Provider confirmed the sender block and Decodium also stored it locally"))
                        : boundedError(result.redactedDiagnostic()));
                guard->publishSnapshot();
            });
        publishSnapshot();
    }

    void cancel(QString transferId)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireInitialized(QStringLiteral("cancel"))) {
            return;
        }
        QString error;
        const bool ok = m_manager->cancelTransfer(transferId, &error);
        finishOperation(QStringLiteral("cancel"), ok,
                        ok ? tr("Transfer cancellation requested")
                           : boundedError(error));
        publishSnapshot();
    }

    void removeRemoteCopy(QString transferId)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireEnabled(QStringLiteral("remove-remote-copy"))) {
            return;
        }
        const quint64 generation = m_providerGeneration;
        const QPointer<SstvShareWorker> guard(this);
        m_manager->removeRemoteCopyAsync(
            transferId,
            [guard, generation](SstvShareProviderResult result) {
                if (!guard || guard->m_shutdown
                    || guard->m_providerGeneration != generation
                    || !guard->m_manager) {
                    return;
                }
                const QString message = result.ok()
                    ? tr("Provider remote copy removed; the local Decodium Gallery was not changed")
                    : boundedError(result.redactedDiagnostic());
                guard->finishOperation(QStringLiteral("remove-remote-copy"),
                                       result.ok(), message);
                guard->publishSnapshot();
            });
        publishSnapshot();
    }

    void pause(QString transferId)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireInitialized(QStringLiteral("pause"))) {
            return;
        }
        QString error;
        const bool ok = m_manager->pauseTransfer(transferId, &error);
        finishOperation(QStringLiteral("pause"), ok,
                        ok ? tr("Transfer paused at its durable checkpoint")
                           : boundedError(error));
        publishSnapshot();
    }

    void resume(QString transferId)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireEnabled(QStringLiteral("resume"))) {
            return;
        }
        QString error;
        const bool ok = m_manager->resumeTransfer(transferId, &error);
        finishOperation(QStringLiteral("resume"), ok,
                        ok ? tr("Transfer resumed from its durable checkpoint")
                           : boundedError(error));
        publishSnapshot();
    }

    void refresh()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireInitialized(QStringLiteral("refresh"))) {
            return;
        }
        if (m_enabled) {
            m_manager->processDue();
        }
        finishOperation(QStringLiteral("refresh"), true,
                        tr("Sharing queue refreshed"));
        publishSnapshot();
    }

    void resetDiagnostics()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireInitialized(QStringLiteral("reset-diagnostics"))) {
            return;
        }
        m_manager->resetDiagnostics();
        finishOperation(QStringLiteral("reset-diagnostics"), true,
                        tr("Sharing diagnostics counters reset"));
        publishSnapshot();
    }

    void shutdown()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (m_shutdown) {
            return;
        }
        m_shutdown = true;
        if (m_tick) {
            m_tick->stop();
        }
        m_manager.reset();
        m_credentialSource.reset();
        if (m_settings) {
            m_settings->sync();
            m_settings.reset();
        }
    }

private:
    void discoverRestCapabilities(bool refreshInboxAfter)
    {
        auto rest = std::dynamic_pointer_cast<SstvGenericRestShareProvider>(
            m_provider);
        if (!rest || m_capabilityDiscoveryActive) {
            failOperation(QStringLiteral("refresh-inbox"), tr(
                "This provider has no verifiable REST inbox capability endpoint"));
            publishSnapshot();
            return;
        }
        m_capabilityDiscoveryActive = true;
        const quint64 generation = m_providerGeneration;
        const QPointer<SstvShareWorker> guard(this);
        rest->refreshCapabilitiesAsync(
            [guard, rest, generation, refreshInboxAfter](
                SstvShareProviderResult result) {
                if (!guard || guard->m_shutdown
                    || guard->m_providerGeneration != generation
                    || guard->m_provider != rest) {
                    return;
                }
                guard->m_capabilityDiscoveryActive = false;
                guard->m_providerCapabilities = rest->capabilities();
                if (!result.ok()) {
                    if (refreshInboxAfter) {
                        guard->finishOperation(
                            QStringLiteral("refresh-inbox"), false,
                            boundedError(result.redactedDiagnostic()));
                    } else {
                        guard->m_error = boundedError(
                            result.redactedDiagnostic());
                    }
                    guard->publishSnapshot();
                    return;
                }
                if (!refreshInboxAfter) {
                    guard->m_error.clear();
                    guard->m_lastStatus = tr(
                        "Provider capabilities verified");
                }
                if (guard->m_providerCapabilities.incomingList) {
                    // A fresh provider owns no ephemeral inbox metadata. List
                    // before driving durable downloads so restart cannot turn
                    // a valid queued item into a permanent local failure.
                    guard->performInboxRefresh(refreshInboxAfter);
                } else if (refreshInboxAfter) {
                    guard->finishOperation(
                        QStringLiteral("refresh-inbox"), false,
                        tr("The provider does not advertise an inbox"));
                    guard->publishSnapshot();
                } else {
                    guard->publishSnapshot();
                }
            });
    }

    bool requireInitialized(const QString& operation)
    {
        if (m_initialized && m_manager && !m_shutdown) {
            return true;
        }
        failOperation(operation, tr("The sharing queue is not ready"));
        publishSnapshot();
        return false;
    }

    bool requireEnabled(const QString& operation)
    {
        if (requireInitialized(operation) && m_enabled) {
            return true;
        }
        if (!m_enabled) {
            failOperation(operation, tr(
                "Remote sharing is off; enable it explicitly before transferring media"));
            publishSnapshot();
        }
        return false;
    }

    void loadConfiguration()
    {
        m_configuration.type = m_settings->value(
            QStringLiteral("SSTV/Share/ProviderType")).toString();
        m_configuration.providerId = m_settings->value(
            QStringLiteral("SSTV/Share/ProviderId")).toString();
        m_configuration.endpoint = m_settings->value(
            QStringLiteral("SSTV/Share/Endpoint")).toString();
        m_configuration.createPath = m_settings->value(
            QStringLiteral("SSTV/Share/RestCreatePath")).toString();
        m_configuration.chunkPath = m_settings->value(
            QStringLiteral("SSTV/Share/RestChunkPath")).toString();
        m_configuration.statusPath = m_settings->value(
            QStringLiteral("SSTV/Share/RestStatusPath")).toString();
        m_configuration.completePath = m_settings->value(
            QStringLiteral("SSTV/Share/RestCompletePath")).toString();
        m_configuration.cancelPath = m_settings->value(
            QStringLiteral("SSTV/Share/RestCancelPath")).toString();
        m_configuration.authType = m_settings->value(
            QStringLiteral("SSTV/Share/AuthType"),
            QStringLiteral("bearer")).toString();
        m_configuration.username = m_settings->value(
            QStringLiteral("SSTV/Share/Username")).toString();
        m_configuration.credentialsRequired = m_settings->value(
            QStringLiteral("SSTV/Share/CredentialsRequired"), true).toBool();
        if (m_localIntegrationProvider) {
            m_configuration = {};
            m_configuration.type = QStringLiteral("local-integration");
            m_configuration.providerId =
                m_localIntegrationProvider->providerId();
            m_configuration.credentialsRequired = false;
        }
        m_credentialStored = lookupCredential(false, nullptr) != nullptr;
    }

    void saveConfiguration()
    {
        if (m_localIntegrationProvider) {
            return;
        }
        m_settings->setValue(QStringLiteral("SSTV/Share/ProviderType"),
                             m_configuration.type);
        m_settings->setValue(QStringLiteral("SSTV/Share/ProviderId"),
                             m_configuration.providerId);
        m_settings->setValue(QStringLiteral("SSTV/Share/Endpoint"),
                             m_configuration.endpoint);
        m_settings->setValue(QStringLiteral("SSTV/Share/RestCreatePath"),
                             m_configuration.createPath);
        m_settings->setValue(QStringLiteral("SSTV/Share/RestChunkPath"),
                             m_configuration.chunkPath);
        m_settings->setValue(QStringLiteral("SSTV/Share/RestStatusPath"),
                             m_configuration.statusPath);
        m_settings->setValue(QStringLiteral("SSTV/Share/RestCompletePath"),
                             m_configuration.completePath);
        m_settings->setValue(QStringLiteral("SSTV/Share/RestCancelPath"),
                             m_configuration.cancelPath);
        m_settings->setValue(QStringLiteral("SSTV/Share/AuthType"),
                             m_configuration.authType);
        m_settings->setValue(QStringLiteral("SSTV/Share/Username"),
                             m_configuration.username);
        m_settings->setValue(QStringLiteral("SSTV/Share/CredentialsRequired"),
                             m_configuration.credentialsRequired);
        m_settings->sync();
    }

    bool validateConfiguration(const ProviderConfiguration& configuration,
                               QString* error) const
    {
        if (m_localIntegrationProvider
            && configuration.type == QStringLiteral("local-integration")
            && configuration.providerId
                == m_localIntegrationProvider->providerId()
            && configuration.providerId
                == QStringLiteral("local-integration")
            && !configuration.credentialsRequired
            && m_localIntegrationProvider->authenticationStatus()
                == SstvShareAuthenticationStatus::NotRequired) {
            const SstvShareProviderCapabilities capabilities =
                m_localIntegrationProvider->capabilities();
            if (capabilities.strictTlsRequired
                && capabilities.download && capabilities.incomingList
                && capabilities.maximumChunkBytes > 0U
                && capabilities.maximumResponseBytes > 0U
                && !capabilities.endToEndEncryptionEnvelope) {
                return true;
            }
        }
        if ((configuration.type != QStringLiteral("rest")
             && configuration.type != QStringLiteral("webdav"))
            || !isSafeShareIdentifier(configuration.providerId)
            || !plainHttpsUrl(configuration.endpoint)) {
            if (error) {
                *error = tr("Provider type, identifier or HTTPS endpoint is invalid");
            }
            return false;
        }
        if (configuration.authType != QStringLiteral("bearer")
            && configuration.authType != QStringLiteral("basic")) {
            if (error) {
                *error = tr("Authentication type must be Bearer or Basic");
            }
            return false;
        }
        if (configuration.authType == QStringLiteral("basic")
            && (!validCredentialText(configuration.username)
                || configuration.username.contains(QLatin1Char(':')))) {
            if (error) {
                *error = tr("Basic authentication requires a bounded username");
            }
            return false;
        }
        if (configuration.type == QStringLiteral("rest")) {
            const auto pathOk = [](const QString& path, bool templated) {
                return path.startsWith(QLatin1Char('/'))
                    && path.size() <= 512 && !path.contains(QStringLiteral(".."))
                    && !path.contains(QLatin1Char('\\'))
                    && !path.contains(QLatin1Char('?'))
                    && !path.contains(QLatin1Char('#'))
                    && !path.contains(QStringLiteral("://"))
                    && !path.contains(QLatin1Char('@'))
                    && sanitizeShareDisplayText(path, 512) == path
                    && (templated
                        ? path.count(QStringLiteral("{uploadId}")) == 1
                        : !path.contains(QStringLiteral("{uploadId}")));
            };
            if (!pathOk(configuration.createPath, false)
                || !pathOk(configuration.chunkPath, true)
                || !pathOk(configuration.statusPath, true)
                || !pathOk(configuration.completePath, true)
                || !pathOk(configuration.cancelPath, true)) {
                if (error) {
                    *error = tr(
                        "Every REST path is required; upload paths must contain {uploadId} exactly once");
                }
                return false;
            }
        } else if (QUrl(configuration.endpoint, QUrl::StrictMode)
                       .path().isEmpty()) {
            if (error) {
                *error = tr("The WebDAV collection URL requires a path");
            }
            return false;
        }
        return true;
    }

    bool storeCredential(const ProviderConfiguration& configuration,
                         const QString& secret,
                         QString* error)
    {
        if (!m_secureBackend || !m_secureBackend->available()) {
            if (error) {
                *error = tr(
                    "Secure credential storage is unavailable; the secret was not persisted");
            }
            return false;
        }
        if (!validCredentialText(secret)) {
            if (error) {
                *error = tr("Credential is empty, oversized or contains an unsafe newline");
            }
            return false;
        }
        QString backendError;
        const bool stored = m_secureBackend->store(
            credentialService(m_profile),
            credentialAccount(configuration.providerId,
                              configuration.authType,
                              configuration.username),
            secret, &backendError);
        if (!stored && error) {
            *error = backendError.isEmpty()
                ? tr("Secure credential storage rejected the credential")
                : boundedError(backendError);
        }
        if (stored) {
            m_credentialStored = true;
        }
        return stored;
    }

    std::shared_ptr<SstvShareCredentialSource> lookupCredential(
        bool required,
        QString* error)
    {
        if (!m_configuration.credentialsRequired) {
            m_credentialStored = false;
            return {};
        }
        if (!m_secureBackend || !m_secureBackend->available()) {
            if (required && error) {
                *error = tr("Secure credential storage is unavailable");
            }
            m_credentialStored = false;
            return {};
        }
        secure_settings::LookupResult result = m_secureBackend->lookup(
            credentialService(m_profile),
            credentialAccount(m_configuration.providerId,
                              m_configuration.authType,
                              m_configuration.username));
        if (!result.error.isEmpty()) {
            if (required && error) {
                *error = boundedError(result.error);
            }
            result.value.fill(QChar('\0'));
            m_credentialStored = false;
            return {};
        }
        if (!result.found || !validCredentialText(result.value)) {
            if (required && error) {
                *error = tr("No valid credential is stored for this provider");
            }
            result.value.fill(QChar('\0'));
            m_credentialStored = false;
            return {};
        }
        QByteArray secret = result.value.toUtf8();
        result.value.fill(QChar('\0'));
        m_credentialStored = true;
        return std::make_shared<PreparedCredentialSource>(
            m_configuration.authType, m_configuration.username.toUtf8(),
            std::move(secret));
    }

    std::shared_ptr<SstvShareProvider> makeProvider(QString* error)
    {
        if (!validateConfiguration(m_configuration, error)) {
            return {};
        }
        if (m_localIntegrationProvider) {
            return m_localIntegrationProvider;
        }
        m_credentialSource = lookupCredential(
            m_configuration.credentialsRequired, error);
        if (m_configuration.credentialsRequired && !m_credentialSource) {
            return {};
        }
        if (m_configuration.type == QStringLiteral("rest")) {
            SstvGenericRestProviderConfig configuration;
            configuration.providerId = m_configuration.providerId;
            configuration.baseUrl = QUrl(m_configuration.endpoint,
                                         QUrl::StrictMode);
            configuration.createUploadPath = m_configuration.createPath;
            configuration.uploadChunkPathTemplate = m_configuration.chunkPath;
            configuration.queryStatusPathTemplate = m_configuration.statusPath;
            configuration.completeUploadPathTemplate =
                m_configuration.completePath;
            configuration.cancelUploadPathTemplate =
                m_configuration.cancelPath;
            configuration.credentialsRequired =
                m_configuration.credentialsRequired;
            configuration.requireServerSha256 = true;
            configuration.maximumChunkBytes = kRestChunkBytes;
            auto provider = std::make_shared<SstvGenericRestShareProvider>(
                configuration, m_credentialSource);
            if (!provider->isConfigurationValid()) {
                if (error) {
                    *error = tr("The REST provider configuration was rejected");
                }
                return {};
            }
            return provider;
        }
        SstvWebDavProviderConfig configuration;
        configuration.providerId = m_configuration.providerId;
        configuration.collectionUrl = QUrl(m_configuration.endpoint,
                                           QUrl::StrictMode);
        configuration.credentialsRequired = m_configuration.credentialsRequired;
        configuration.overwriteExisting = false;
        configuration.requireServerSha256 = true;
        auto provider = std::make_shared<SstvWebDavShareProvider>(
            configuration, m_credentialSource);
        if (!provider->isConfigurationValid()) {
            if (error) {
                *error = tr("The WebDAV provider configuration was rejected");
            }
            return {};
        }
        return provider;
    }

    bool rebuildManager(bool withProvider, QString* error)
    {
        ++m_providerGeneration;
        m_emittedIncomingHandoffs.clear();
        m_manager.reset();
        m_provider.reset();
        m_credentialSource.reset();
        m_providerCapabilities = {};
        m_capabilityDiscoveryActive = false;

        SstvShareQueueConfig configuration;
        const QString sharingRoot = QDir(m_storageRoot).absoluteFilePath(
            QStringLiteral("sharing"));
        configuration.databasePath = QDir(sharingRoot).absoluteFilePath(
            QStringLiteral("queue.sqlite"));
        configuration.allowedUploadRoots = {m_storageRoot};
        configuration.downloadRoot = QDir(sharingRoot).absoluteFilePath(
            QStringLiteral("downloads"));
        configuration.limits.maximumQueryItems = kUiQueryLimit;
        configuration.limits.maximumConcurrentTransfers = 2;
        configuration.limits.maximumConcurrentPerProvider = 1;
        configuration.limits.uploadChunkBytes = kMaximumSharedImageBytes;
        configuration.limits.downloadChunkBytes = kRestChunkBytes;
        const std::shared_ptr<std::atomic<int>> meteredState =
            m_meteredNetworkState;
        configuration.meteredNetworkProbe = [meteredState]()
            -> std::optional<bool> {
            if (!meteredState) {
                return {};
            }
            const int value = meteredState->load(std::memory_order_relaxed);
            return value < 0 ? std::optional<bool> {}
                             : std::optional<bool> {value != 0};
        };

        auto candidate = std::make_unique<SstvShareQueueManager>(configuration);
        if (withProvider) {
            std::shared_ptr<SstvShareProvider> provider = makeProvider(error);
            if (!provider) {
                return false;
            }
            m_providerCapabilities = provider->capabilities();
            m_provider = provider;
            if (!candidate->registerProvider(provider, error)) {
                return false;
            }
        }
        if (!candidate->initialize(error)) {
            return false;
        }
        m_manager = std::move(candidate);
        if (withProvider
            && std::dynamic_pointer_cast<SstvGenericRestShareProvider>(
                   m_provider)) {
            discoverRestCapabilities(false);
        }
        return true;
    }

    bool hasPendingTransfers() const
    {
        if (!m_manager) {
            return false;
        }
        QString error;
        return !m_manager->activeTransfers(1, &error).isEmpty();
    }

    template <typename Callback>
    void managerBooleanAction(const QString& operation,
                              const QString& transferId,
                              Callback&& callback,
                              const QString& successMessage,
                              bool drive = false)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!requireEnabled(operation)) {
            return;
        }
        QString error;
        const bool ok = callback(transferId, &error);
        if (ok && drive) {
            m_manager->processDue();
        }
        finishOperation(operation, ok,
                        ok ? successMessage : boundedError(error));
        publishSnapshot();
    }

    void processTick()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!m_initialized || !m_manager || m_shutdown) {
            return;
        }
        if (m_enabled) {
            QString error;
            m_manager->processDue(&error);
            if (!error.isEmpty()) {
                m_error = boundedError(error);
            }
        }
        publishSnapshot();
    }

    void finishOperation(const QString& operation,
                         bool ok,
                         const QString& message)
    {
        const QString bounded = ok ? message.left(512) : boundedError(message);
        if (ok) {
            m_error.clear();
            m_lastStatus = bounded;
        } else {
            m_error = bounded;
        }
        recordSstvDiagnosticEvent(
            sstvShareLog(), ok ? QtInfoMsg : QtWarningMsg,
            QStringLiteral("share.operation-finished"),
            {{QStringLiteral("operation"), operation},
             {QStringLiteral("success"), ok}});
        emit operationResult(operation, ok, bounded);
    }

    void failOperation(const QString& operation, const QString& message)
    {
        finishOperation(operation, false, message);
    }

    void publishSnapshot()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        QVariantList activeRows;
        QVariantList historyRows;
        QVariantList inboxRows;
        QVariantMap diagnosticsMap {
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("uploadedBytes"), QVariant::fromValue<qulonglong>(0)},
            {QStringLiteral("downloadedBytes"), QVariant::fromValue<qulonglong>(0)},
            {QStringLiteral("reclaimedRows"), QVariant::fromValue<qulonglong>(0)},
            {QStringLiteral("uploadBytesPerSecond"), QVariant::fromValue<qulonglong>(0)},
            {QStringLiteral("downloadBytesPerSecond"), QVariant::fromValue<qulonglong>(0)},
            {QStringLiteral("activeQueueDepth"), 0},
            {QStringLiteral("uploadQueueDepth"), 0},
            {QStringLiteral("downloadQueueDepth"), 0},
            {QStringLiteral("resetUtc"), QDateTime {}},
        };
        QString queryError;
        if (m_manager) {
            const auto publishRecoveredHandoff =
                [this](const SstvManagedTransferRecord& record,
                       const std::optional<SstvValidatedIncomingHandoff>& handoff) {
                    const bool acceptedDownload = record.direction
                            == SstvManagedTransferDirection::Download
                        && (record.state == SstvManagedTransferState::Accepted
                            || record.state
                                == SstvManagedTransferState::Acknowledging
                            || record.state
                                == SstvManagedTransferState::Acknowledged);
                    if (acceptedDownload && handoff
                        && !m_emittedIncomingHandoffs.contains(
                            record.transferId)) {
                        m_emittedIncomingHandoffs.insert(record.transferId);
                        emit incomingHandoffResult(handoffMap(*handoff));
                    }
                };
            const auto active = m_manager->activeTransfers(
                kUiQueryLimit, &queryError);
            for (const auto& record : active) {
                QVariantMap row = transferRow(record);
                const auto handoff = m_manager->validatedIncomingHandoff(
                    record.transferId);
                addHandoffToRow(row, handoff);
                row.insert(QStringLiteral("canSaveAs"), handoff.has_value());
                row.insert(QStringLiteral("canDeleteLocalCopy"),
                           handoff.has_value()
                               && (record.state
                                       == SstvManagedTransferState::Accepted
                                   || record.state
                                       == SstvManagedTransferState::Acknowledged));
                if (record.state == SstvManagedTransferState::Accepted) {
                    row.insert(QStringLiteral("canAccept"),
                               handoff.has_value());
                }
                publishRecoveredHandoff(record, handoff);
                activeRows.push_back(std::move(row));
            }
            if (queryError.isEmpty()) {
                const auto history = m_manager->transferHistory(
                    kUiQueryLimit, &queryError);
                for (const auto& record : history) {
                    cleanupGeneratedUpload(m_storageRoot, record);
                    const SstvRemoteCopyAction remoteAction = m_enabled
                        ? m_manager->remoteCopyAction(record.transferId)
                        : SstvRemoteCopyAction::Unavailable;
                    QVariantMap row = transferRow(record, remoteAction);
                    const auto handoff = m_manager->validatedIncomingHandoff(
                        record.transferId);
                    addHandoffToRow(row, handoff);
                    row.insert(QStringLiteral("canSaveAs"),
                               handoff.has_value());
                    row.insert(QStringLiteral("canDeleteLocalCopy"),
                               handoff.has_value()
                                   && (record.state
                                           == SstvManagedTransferState::Accepted
                                       || record.state
                                           == SstvManagedTransferState::Acknowledged));
                    publishRecoveredHandoff(record, handoff);
                    historyRows.push_back(std::move(row));
                }
            }
            if (queryError.isEmpty()) {
                const auto items = m_manager->inbox(kUiQueryLimit,
                                                    &queryError);
                const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
                for (const auto& item : items) {
                    QVariantMap row = inboxRow(
                        item, m_providerCapabilities, nowUtc);
                    const auto handoff = item.transferId.isEmpty()
                        ? std::optional<SstvValidatedIncomingHandoff> {}
                        : m_manager->validatedIncomingHandoff(
                              item.transferId);
                    addHandoffToRow(row, handoff);
                    row.insert(QStringLiteral("canSaveAs"),
                               handoff.has_value());
                    row.insert(QStringLiteral("canDeleteLocalCopy"),
                               handoff.has_value()
                                   && (item.disposition
                                           == SstvInboxDisposition::Accepted
                                       || item.disposition
                                           == SstvInboxDisposition::Acknowledged));
                    if (item.disposition
                            == SstvInboxDisposition::Accepted) {
                        row.insert(QStringLiteral("canAccept"),
                                   handoff.has_value());
                    }
                    inboxRows.push_back(std::move(row));
                }
            }
            if (queryError.isEmpty()) {
                const SstvShareQueueDiagnostics diagnostics =
                    m_manager->diagnostics(&queryError);
                if (queryError.isEmpty()) {
                    diagnosticsMap = {
                        {QStringLiteral("schemaVersion"), 1},
                        {QStringLiteral("uploadedBytes"),
                         QVariant::fromValue<qulonglong>(
                             diagnostics.uploadedBytes)},
                        {QStringLiteral("downloadedBytes"),
                         QVariant::fromValue<qulonglong>(
                             diagnostics.downloadedBytes)},
                        {QStringLiteral("reclaimedRows"),
                         QVariant::fromValue<qulonglong>(
                             diagnostics.reclaimedRows)},
                        {QStringLiteral("uploadBytesPerSecond"),
                         QVariant::fromValue<qulonglong>(
                             diagnostics.uploadBytesPerSecond)},
                        {QStringLiteral("downloadBytesPerSecond"),
                         QVariant::fromValue<qulonglong>(
                             diagnostics.downloadBytesPerSecond)},
                        {QStringLiteral("activeQueueDepth"),
                         static_cast<qlonglong>(diagnostics.activeQueueDepth)},
                        {QStringLiteral("uploadQueueDepth"),
                         static_cast<qlonglong>(diagnostics.uploadQueueDepth)},
                        {QStringLiteral("downloadQueueDepth"),
                         static_cast<qlonglong>(diagnostics.downloadQueueDepth)},
                        {QStringLiteral("resetUtc"), diagnostics.resetUtc},
                    };
                }
            }
        }
        if (!queryError.isEmpty()) {
            m_error = boundedError(queryError);
        }
        QString configurationError;
        const bool structurallyConfigured = validateConfiguration(
            m_configuration, &configurationError);
        QString status = m_lastStatus;
        if (status.isEmpty()) {
            status = !m_enabled ? tr("Remote sharing is off")
                : tr("Sharing enabled for %1").arg(
                      m_configuration.providerId);
        }
        QVariantMap state {
            {QStringLiteral("ready"), m_initialized && m_manager},
            {QStringLiteral("enabled"), m_enabled},
            {QStringLiteral("configured"), structurallyConfigured},
            {QStringLiteral("busy"), m_manager
                 && m_manager->activeOperationCount() > 0},
            {QStringLiteral("secureStorageAvailable"),
             m_secureStorageAvailable},
            {QStringLiteral("providerSupportsInbox"),
             m_providerCapabilities.incomingList},
            {QStringLiteral("providerSupportsIncomingDelete"),
             m_providerCapabilities.incomingDelete},
            {QStringLiteral("providerSupportsSenderBlocking"),
             m_providerCapabilities.senderBlocking},
            {QStringLiteral("meteredNetworkStatus"),
             !m_meteredNetworkState
                 || m_meteredNetworkState->load(std::memory_order_relaxed) < 0
                 ? QStringLiteral("unknown")
                 : (m_meteredNetworkState->load(std::memory_order_relaxed) != 0
                        ? QStringLiteral("metered")
                        : QStringLiteral("unmetered"))},
            {QStringLiteral("diagnostics"), diagnosticsMap},
            {QStringLiteral("statusText"), status},
            {QStringLiteral("errorString"), m_error},
            {QStringLiteral("storageFolder"), m_storageRoot},
            {QStringLiteral("workerThreadToken"),
             QVariant::fromValue<qulonglong>(currentThreadToken())},
            {QStringLiteral("configuration"),
             m_configuration.toVariantMap(m_credentialStored)},
        };
        emit snapshotReady(std::move(activeRows), std::move(historyRows),
                           std::move(inboxRows), std::move(state));
    }

    const secure_settings::Backend* m_secureBackend {nullptr};
    std::shared_ptr<std::atomic<int>> m_meteredNetworkState;
    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<SstvShareQueueManager> m_manager;
    std::shared_ptr<SstvShareProvider> m_provider;
    std::shared_ptr<SstvShareProvider> m_localIntegrationProvider;
    std::shared_ptr<SstvShareCredentialSource> m_credentialSource;
    SstvShareProviderCapabilities m_providerCapabilities;
    ProviderConfiguration m_configuration;
    QTimer* m_tick {nullptr};
    QString m_storageRoot;
    QString m_profile;
    QString m_lastStatus;
    QString m_error;
    bool m_secureStorageAvailable {false};
    bool m_credentialStored {false};
    bool m_initialized {false};
    bool m_enabled {false};
    bool m_shutdown {false};
    bool m_capabilityDiscoveryActive {false};
    quint64 m_providerGeneration {0U};
    QSet<QString> m_emittedIncomingHandoffs;
};

SstvShareListModel::SstvShareListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int SstvShareListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant SstvShareListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= m_rows.size()) {
        return {};
    }
    const QVariantMap row = m_rows.at(index.row()).toMap();
    const QByteArray name = roleNames().value(role);
    return name.isEmpty() ? QVariant {} : row.value(QString::fromLatin1(name));
}

QHash<int, QByteArray> SstvShareListModel::roleNames() const
{
    return {
        {TransferIdRole, "transferId"},
        {IncomingIdRole, "incomingId"},
        {DirectionRole, "direction"},
        {StateRole, "transferState"},
        {ProviderIdRole, "providerId"},
        {PeerIdRole, "peerId"},
        {FileNameRole, "fileName"},
        {ModeRole, "mode"},
        {ByteSizeRole, "byteSize"},
        {ByteOffsetRole, "byteOffset"},
        {ProgressRole, "progress"},
        {ErrorRole, "error"},
        {CreatedUtcRole, "createdUtc"},
        {UpdatedUtcRole, "updatedUtc"},
        {ExpiresUtcRole, "expiresUtc"},
        {CanPauseRole, "canPause"},
        {CanResumeRole, "canResume"},
        {CanCancelRole, "canCancel"},
        {CanDownloadRole, "canDownload"},
        {CanAcceptRole, "canAccept"},
        {CanAcknowledgeRole, "canAcknowledge"},
        {CanRejectRole, "canReject"},
        {CanSaveAsRole, "canSaveAs"},
        {CanDeleteLocalCopyRole, "canDeleteLocalCopy"},
        {CanRequestProviderDeletionRole, "canRequestProviderDeletion"},
        {CanBlockSenderRole, "canBlockSender"},
        {BlockSenderScopeRole, "blockSenderScope"},
        {MessageRole, "message"},
        {Sha256Role, "sha256"},
        {PrivacySummaryRole, "privacySummary"},
        {CanRemoveRemoteCopyRole, "canRemoveRemoteCopy"},
        {RemoteCopyActionRole, "remoteCopyAction"},
        {PreviewSourceRole, "previewSource"},
        {ValidatedHandoffRole, "validatedHandoff"},
    };
}

void SstvShareListModel::replaceRows(QVariantList rows)
{
    if (rows == m_rows) {
        return;
    }
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

SstvShareController::SstvShareController(
    const secure_settings::Backend* secureBackend,
    QObject* parent)
    : SstvShareController(secureBackend, {}, parent)
{
}

SstvShareController::SstvShareController(
    const secure_settings::Backend* secureBackend,
    std::shared_ptr<SstvShareProvider> localIntegrationProvider,
    QObject* parent)
    : QObject(parent)
    , m_activeModel(this)
    , m_historyModel(this)
    , m_inboxModel(this)
{
    m_meteredNetworkState = std::make_shared<std::atomic<int>>(-1);
    QNetworkInformation* networkInformation = QNetworkInformation::instance();
    if (!networkInformation
        || !networkInformation->supports(QNetworkInformation::Feature::Metered)) {
        QNetworkInformation::loadBackendByFeatures(
            QNetworkInformation::Feature::Metered);
        networkInformation = QNetworkInformation::instance();
    }
    if (networkInformation
        && networkInformation->supports(QNetworkInformation::Feature::Metered)) {
        m_meteredNetworkState->store(networkInformation->isMetered() ? 1 : 0,
                                     std::memory_order_relaxed);
        const std::shared_ptr<std::atomic<int>> meteredState =
            m_meteredNetworkState;
        connect(networkInformation, &QNetworkInformation::isMeteredChanged,
                this, [this, meteredState](bool metered) {
                    meteredState->store(metered ? 1 : 0,
                                        std::memory_order_relaxed);
                    refresh();
                });
    }
    m_workerThread.setObjectName(QStringLiteral("SSTV sharing worker"));
    auto* worker = new SstvShareWorker(
        secureBackend, m_meteredNetworkState,
        std::move(localIntegrationProvider));
    m_worker = worker;
    worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished,
            worker, &QObject::deleteLater);
    connect(worker, &SstvShareWorker::snapshotReady,
            this, &SstvShareController::applySnapshot,
            Qt::QueuedConnection);
    connect(worker, &SstvShareWorker::operationResult,
            this, &SstvShareController::operationFinished,
            Qt::QueuedConnection);
    connect(worker, &SstvShareWorker::incomingHandoffResult,
            this, &SstvShareController::incomingHandoffReady,
            Qt::QueuedConnection);
    m_workerThread.start();
}

SstvShareController::~SstvShareController()
{
    shutdown();
}

void SstvShareController::setStorageRoot(const QString& storageRoot,
                                         const QString& stationProfile)
{
    queueWorkerCall([storageRoot, stationProfile](SstvShareWorker* worker) {
        worker->initialize(storageRoot, stationProfile);
    });
}

bool SstvShareController::setEnabled(bool enabled)
{
    return queueWorkerCall([enabled](SstvShareWorker* worker) {
        worker->setEnabled(enabled);
    });
}

bool SstvShareController::configureProvider(const QVariantMap& configuration)
{
    return queueWorkerCall([configuration](SstvShareWorker* worker) mutable {
        worker->configure(std::move(configuration));
    });
}

bool SstvShareController::clearCredentials()
{
    return queueWorkerCall([](SstvShareWorker* worker) {
        worker->clearCredentials();
    });
}

bool SstvShareController::upload(const QUrl& source,
                                 const QString& recipientId,
                                 const QString& mode,
                                 const QString& message,
                                 bool recipientConfirmed)
{
    return uploadWithOptions(source, recipientId, mode, message,
                             recipientConfirmed, privateUploadDefaults());
}

bool SstvShareController::uploadWithOptions(
    const QUrl& source,
    const QString& recipientId,
    const QString& mode,
    const QString& message,
    bool recipientConfirmed,
    const QVariantMap& options)
{
    if (!source.isLocalFile()) {
        return false;
    }
    const QString path = source.toLocalFile();
    return queueWorkerCall(
        [path, recipientId, mode, message, recipientConfirmed, options](
            SstvShareWorker* worker) {
            worker->upload(path, recipientId, mode, message,
                           recipientConfirmed, options);
        });
}

bool SstvShareController::refreshInbox()
{
    return queueWorkerCall([](SstvShareWorker* worker) {
        worker->refreshInbox();
    });
}

bool SstvShareController::download(const QString& incomingId,
                                   const QString& destinationFileName)
{
    return queueWorkerCall(
        [incomingId, destinationFileName](SstvShareWorker* worker) {
            worker->download(incomingId, destinationFileName);
        });
}

bool SstvShareController::accept(const QString& transferId)
{
    return queueWorkerCall([transferId](SstvShareWorker* worker) {
        worker->accept(transferId);
    });
}

bool SstvShareController::acknowledge(const QString& transferId)
{
    return queueWorkerCall([transferId](SstvShareWorker* worker) {
        worker->acknowledge(transferId);
    });
}

bool SstvShareController::reject(const QString& incomingId)
{
    return queueWorkerCall([incomingId](SstvShareWorker* worker) {
        worker->reject(incomingId);
    });
}

bool SstvShareController::saveAs(const QString& transferId,
                                 const QUrl& destination)
{
    if (!destination.isLocalFile()) {
        return false;
    }
    const QString path = destination.toLocalFile();
    return queueWorkerCall([transferId, path](SstvShareWorker* worker) {
        worker->saveAs(transferId, path);
    });
}

bool SstvShareController::deleteLocalCopy(const QString& transferId)
{
    return queueWorkerCall([transferId](SstvShareWorker* worker) {
        worker->deleteLocalCopy(transferId);
    });
}

bool SstvShareController::requestProviderDeletion(
    const QString& incomingId)
{
    return queueWorkerCall([incomingId](SstvShareWorker* worker) {
        worker->requestProviderDeletion(incomingId);
    });
}

bool SstvShareController::blockSender(const QString& incomingId,
                                      bool localOnly)
{
    return queueWorkerCall([incomingId, localOnly](SstvShareWorker* worker) {
        worker->blockSender(incomingId, localOnly);
    });
}

bool SstvShareController::cancel(const QString& transferId)
{
    return queueWorkerCall([transferId](SstvShareWorker* worker) {
        worker->cancel(transferId);
    });
}

bool SstvShareController::removeRemoteCopy(const QString& transferId)
{
    return queueWorkerCall([transferId](SstvShareWorker* worker) {
        worker->removeRemoteCopy(transferId);
    });
}

bool SstvShareController::pause(const QString& transferId)
{
    return queueWorkerCall([transferId](SstvShareWorker* worker) {
        worker->pause(transferId);
    });
}

bool SstvShareController::resume(const QString& transferId)
{
    return queueWorkerCall([transferId](SstvShareWorker* worker) {
        worker->resume(transferId);
    });
}

bool SstvShareController::refresh()
{
    return queueWorkerCall([](SstvShareWorker* worker) {
        worker->refresh();
    });
}

bool SstvShareController::resetDiagnostics()
{
    return queueWorkerCall([](SstvShareWorker* worker) {
        worker->resetDiagnostics();
    });
}

QString SstvShareController::preSignedUnavailableReason() const
{
    return tr("Pre-signed upload is unavailable because no trusted broker is configured; Decodium never accepts a provider URL or credential from untrusted manifest data.");
}

QString SstvShareController::peerRelayUnavailableReason() const
{
    return tr("Peer/relay sharing is unavailable because Decodium has no authenticated relay backend; no peer discovery or direct network listener is started.");
}

void SstvShareController::shutdown()
{
    if (m_shutdown) {
        return;
    }
    m_shutdown = true;
    const QPointer<SstvShareWorker> worker = m_worker;
    if (worker && m_workerThread.isRunning()) {
        QMetaObject::invokeMethod(worker, &SstvShareWorker::shutdown,
                                  Qt::BlockingQueuedConnection);
    }
    m_workerThread.quit();
    m_workerThread.wait();
    m_worker = nullptr;
}

void SstvShareController::applySnapshot(QVariantList active,
                                        QVariantList history,
                                        QVariantList inbox,
                                        QVariantMap state)
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_activeModel.replaceRows(std::move(active));
    m_historyModel.replaceRows(std::move(history));
    m_inboxModel.replaceRows(std::move(inbox));

    const QVariantMap nextConfiguration = state.value(
        QStringLiteral("configuration")).toMap();
    const bool changedConfiguration = nextConfiguration != m_configuration;
    m_configuration = nextConfiguration;
    m_ready = state.value(QStringLiteral("ready")).toBool();
    m_enabled = state.value(QStringLiteral("enabled")).toBool();
    m_configured = state.value(QStringLiteral("configured")).toBool();
    m_busy = state.value(QStringLiteral("busy")).toBool();
    m_secureStorageAvailable = state.value(
        QStringLiteral("secureStorageAvailable")).toBool();
    m_providerSupportsInbox = state.value(
        QStringLiteral("providerSupportsInbox")).toBool();
    m_providerSupportsIncomingDelete = state.value(
        QStringLiteral("providerSupportsIncomingDelete")).toBool();
    m_providerSupportsSenderBlocking = state.value(
        QStringLiteral("providerSupportsSenderBlocking")).toBool();
    m_meteredNetworkStatus = state.value(
        QStringLiteral("meteredNetworkStatus"),
        QStringLiteral("unknown")).toString();
    m_diagnostics = state.value(QStringLiteral("diagnostics")).toMap();
    m_statusText = state.value(QStringLiteral("statusText")).toString();
    m_errorString = state.value(QStringLiteral("errorString")).toString();
    m_storageFolder = state.value(QStringLiteral("storageFolder")).toString();
    m_workerThreadToken = static_cast<quintptr>(state.value(
        QStringLiteral("workerThreadToken")).toULongLong());
    if (changedConfiguration) {
        emit configurationChanged();
    }
    emit stateChanged();
}

bool SstvShareController::queueWorkerCall(
    std::function<void(SstvShareWorker*)> call)
{
    const QPointer<SstvShareWorker> worker = m_worker;
    if (m_shutdown || !worker || !m_workerThread.isRunning()) {
        return false;
    }
    return QMetaObject::invokeMethod(
        worker, [worker, call = std::move(call)]() mutable {
            if (worker) {
                call(worker.data());
            }
        }, Qt::QueuedConnection);
}

} // namespace decodium::sstv

#include "SstvShareController.moc"
