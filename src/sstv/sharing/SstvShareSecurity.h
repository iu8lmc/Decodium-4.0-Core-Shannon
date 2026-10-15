// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QUrl>

#include <cstddef>

namespace decodium::sstv::sharing {

inline constexpr qsizetype kMaximumManifestJsonBytes = 65'536;
inline constexpr qsizetype kMaximumPersistenceJsonBytes = 131'072;
inline constexpr int kMaximumJsonDepth = 8;
inline constexpr std::size_t kMaximumJsonNodes = 4'096U;

enum class SstvShareValidationCode
{
    None,
    JsonTooLarge,
    MalformedJson,
    JsonTooDeep,
    TooManyJsonNodes,
    DuplicateJsonKey,
    RootNotObject,
    UnknownProtocolVersion,
    UnknownField,
    MissingField,
    WrongType,
    InvalidUuid,
    InvalidIdentifier,
    InvalidTimestamp,
    InvalidExpiry,
    InvalidFilename,
    InvalidMimeType,
    InvalidHash,
    InvalidDimensions,
    InvalidByteSize,
    InvalidChunkCount,
    InvalidText,
    InvalidPrivacy,
    InvalidTransportSecurity,
    InvalidEncryption,
    Expired,
    UrlNotAllowed,
    InvalidState,
    InvalidTransition,
    InvalidRetryPolicy,
    InvalidProgress,
    InvalidIdempotencyKey,
    InternalEncodingError,
};

struct SstvShareValidationError final
{
    SstvShareValidationCode code {SstvShareValidationCode::None};
    QString field;

    bool ok() const noexcept
    {
        return code == SstvShareValidationCode::None;
    }

    static SstvShareValidationError failure(SstvShareValidationCode code,
                                            QString field = {});
};

QString sstvShareValidationCodeName(SstvShareValidationCode code);

struct SstvShareBoundedJsonResult final
{
    QJsonObject object;
    SstvShareValidationError error;

    bool ok() const noexcept { return error.ok(); }
};

SstvShareBoundedJsonResult parseBoundedJsonObject(
    const QByteArray& json,
    qsizetype maximumBytes = kMaximumManifestJsonBytes,
    int maximumDepth = kMaximumJsonDepth,
    std::size_t maximumNodes = kMaximumJsonNodes);

QByteArray canonicalJson(const QJsonValue& value,
                         SstvShareValidationError* error = nullptr,
                         int maximumDepth = kMaximumJsonDepth,
                         std::size_t maximumNodes = kMaximumJsonNodes);

QString sanitizeShareDisplayText(const QString& input,
                                 qsizetype maximumCharacters,
                                 bool allowNewlines = false);
QString sanitizeShareFilename(const QString& input,
                              qsizetype maximumCharacters = 128);
bool isSafeShareFilename(const QString& value,
                         qsizetype maximumCharacters = 128);
bool isSafeShareIdentifier(const QString& value,
                           qsizetype maximumCharacters = 128);
bool isLowercaseSha256(const QString& value) noexcept;
bool containsNetworkUrl(const QString& value);

// Diagnostics helpers deliberately remove every path, query, fragment,
// user-info and credential-like value. Provider implementations must keep
// signed URLs and tokens out of public result objects in the first place.
QString redactedShareUrl(const QUrl& url);
QString redactShareSecrets(const QString& text);

} // namespace decodium::sstv::sharing
