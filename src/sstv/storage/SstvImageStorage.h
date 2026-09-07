// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QImage>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace decodium::sstv {

enum class SstvImageCategory : quint8
{
    Received = 1,
    Transmitted = 2,
    Imported = 3,
    Draft = 4
};

QString sstvImageCategoryName(SstvImageCategory category);
bool sstvImageCategoryFromName(const QString& name,
                               SstvImageCategory* category) noexcept;
bool isValidSstvImageCategory(SstvImageCategory category) noexcept;

enum class SstvUploadState : quint8
{
    NotRequested = 0,
    Pending = 1,
    Uploading = 2,
    Uploaded = 3,
    Failed = 4
};

QString sstvUploadStateName(SstvUploadState state);
bool sstvUploadStateFromName(const QString& name,
                             SstvUploadState* state) noexcept;
bool isValidSstvUploadState(SstvUploadState state) noexcept;

// QSO associations are opaque local identifiers, never filesystem paths.
// Empty is accepted only for the explicit disassociation operation.
inline constexpr qsizetype kMaximumSstvQsoIdCharacters = 256;
bool validateSstvQsoId(const QString& qsoId,
                       bool allowEmpty,
                       QString* error = nullptr);

struct SstvStorageLimits final
{
    int maximumWidth {8192};
    int maximumHeight {8192};
    qint64 maximumPixels {32LL * 1024LL * 1024LL};
    qint64 maximumDecodedBytes {128LL * 1024LL * 1024LL};
    qint64 maximumPngBytes {64LL * 1024LL * 1024LL};
    qint64 maximumMetadataBytes {64LL * 1024LL};
    int maximumFileNameUtf8Bytes {180};
    int maximumTags {32};
    int maximumTagCharacters {64};

    bool validate(QString* error = nullptr) const;
};

// Complete path-only database/sidecar representation.  The PNG itself never
// belongs in SQLite.  A persisted record is accepted only when all integrity,
// size, timestamp and path invariants below validate.
struct SstvImageRecord final
{
    static constexpr int kSidecarSchemaVersion = 4;

    QString id;
    SstvImageCategory category {SstvImageCategory::Received};
    // Compatibility sort timestamp.  New records keep this equal to
    // eventAtUtc, whose meaning is received/prepared/transmitted according to
    // category.
    QDateTime capturedAtUtc;
    QDateTime eventAtUtc;
    QDateTime createdAtUtc;
    QDateTime updatedAtUtc;
    QString mode;
    int visCode {-1};
    bool visValid {false};
    QString fskId;
    QString remoteCallsign;
    QString remoteGrid;
    QString localCallsign;
    QString localGrid;
    QString source;
    qint64 frequencyHz {0};
    qint64 audioFrequencyHz {0};
    int sourceSampleRateHz {0};
    bool digital {false};
    int completionPercent {0};
    bool complete {false};
    QJsonObject qualityMetrics;
    double slantCorrectionPpm {0.0};
    QString rawAudioPath;
    QString relatedQsoId;
    bool remote {false};
    SstvUploadState uploadState {SstvUploadState::NotRequested};
    QString remoteProvider;
    QString remoteObjectId;
    QDateTime expiresAtUtc;
    quint32 privacyFlags {0};
    // First-class local retention metadata.  A favourite is never an
    // automatic or manual-retention deletion candidate.
    bool favorite {false};
    QStringList tags;
    QString imagePath;
    QString thumbnailPath;
    QString metadataPath;
    QByteArray sha256;
    QString mimeType {QStringLiteral("image/png")};
    qint64 fileSizeBytes {0};
    int width {0};
    int height {0};
    int originalWidth {0};
    int originalHeight {0};
    QString note;

    bool validate(const SstvStorageLimits& limits = {},
                  QString* error = nullptr) const;
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& object,
                         SstvImageRecord* record,
                         QString* error = nullptr,
                         const SstvStorageLimits& limits = {});
};

bool operator==(const SstvImageRecord& left,
                const SstvImageRecord& right) noexcept;
bool operator!=(const SstvImageRecord& left,
                const SstvImageRecord& right) noexcept;

class SstvStorageLayout final
{
public:
    explicit SstvStorageLayout(QString rootPath = {});

    static SstvStorageLayout fromStandardPaths();

    QString rootPath() const;
    QString databasePath() const;
    // Native Studio/WAV export destination. Export files remain separate from
    // the image archive and are never indexed as gallery images implicitly.
    QString wavExportRoot() const;
    QString categoryRoot(SstvImageCategory category) const;
    QString datedCategoryDirectory(SstvImageCategory category,
                                   const QDate& utcDate) const;

    bool ensure(QString* error = nullptr) const;
    bool ensureDatedCategoryDirectory(SstvImageCategory category,
                                      const QDate& utcDate,
                                      QString* directory,
                                      QString* error = nullptr) const;
    bool containsPath(const QString& candidatePath,
                      bool requireExistingParent = true,
                      QString* error = nullptr) const;

private:
    QString m_rootPath;
};

struct SstvImageSaveRequest final
{
    SstvImageRecord record;
    QImage image;
    QString fileNameTemplate {
        QStringLiteral("{date}_{time}_{mode}_{remoteCall}_{id}")};
};

enum class SstvStoreError : quint8
{
    None = 0,
    InvalidRequest,
    InvalidLayout,
    LimitExceeded,
    EncodingFailed,
    Collision,
    IoFailure,
    IntegrityFailure
};

struct SstvImageSaveResult final
{
    bool ok {false};
    SstvStoreError code {SstvStoreError::InvalidRequest};
    SstvImageRecord record;
    QString error;
};

class SstvImageStore final
{
public:
    explicit SstvImageStore(SstvStorageLayout layout,
                            SstvStorageLimits limits = {});

    const SstvStorageLayout& layout() const noexcept { return m_layout; }
    const SstvStorageLimits& limits() const noexcept { return m_limits; }

    SstvImageSaveResult save(const SstvImageSaveRequest& request) const;
    // Publishes an already validated metadata-free PNG without re-encoding
    // its pixels. The bytes are decoded again and must describe exactly the
    // QImage in request; preserving them keeps the Gallery SHA-256 bound to
    // the accepted inbound handoff.
    SstvImageSaveResult savePreservingPng(
        const SstvImageSaveRequest& request,
        const QByteArray& encodedPng) const;
    // Explicit metadata update used before a database UPDATE.  The existing
    // PNG is never replaced; only its sidecar is atomically committed after
    // the path, dimensions, byte count and SHA-256 have been revalidated.
    bool updateMetadata(const SstvImageRecord& record,
                        QString* error = nullptr) const;
    bool verify(const SstvImageRecord& record,
                bool verifyHash,
                QString* error = nullptr) const;

    static QString sanitizeFileComponent(const QString& input,
                                         int maximumUtf8Bytes = 180);
    static bool renderFileBase(const QString& nameTemplate,
                               const SstvImageRecord& record,
                               int maximumUtf8Bytes,
                               QString* fileBase,
                               QString* error = nullptr);
    static QByteArray sha256File(const QString& path,
                                 qint64 maximumBytes,
                                 qint64* byteCount = nullptr,
                                 QString* error = nullptr);
    static bool loadMetadata(const QString& metadataPath,
                             SstvImageRecord* record,
                             QString* error = nullptr,
                             const SstvStorageLimits& limits = {});

private:
    SstvImageSaveResult saveImpl(const SstvImageSaveRequest& request,
                                 const QByteArray* encodedPng) const;

    SstvStorageLayout m_layout;
    SstvStorageLimits m_limits;
};

} // namespace decodium::sstv

Q_DECLARE_METATYPE(decodium::sstv::SstvImageCategory)
Q_DECLARE_METATYPE(decodium::sstv::SstvUploadState)
Q_DECLARE_METATYPE(decodium::sstv::SstvImageRecord)
Q_DECLARE_METATYPE(decodium::sstv::SstvImageSaveRequest)
Q_DECLARE_METATYPE(decodium::sstv::SstvStoreError)
Q_DECLARE_METATYPE(decodium::sstv::SstvImageSaveResult)
