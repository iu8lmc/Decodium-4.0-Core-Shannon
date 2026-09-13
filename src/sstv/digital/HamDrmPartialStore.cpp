// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmPartialStore.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace decodium::sstv::hamdrm {
namespace {

constexpr std::array<char, 8> kMagic {'D', 'C', 'D', 'H', 'P', 'R', 'T', '1'};
constexpr std::uint32_t kFormatVersion = 1U;
constexpr std::size_t kDigestBytes = 32U;
constexpr std::size_t kFixedBytes = kMagic.size() + 4U + 2U + 4U;
constexpr std::size_t kEncodedGroupOverhead = 11U;

void append16(QByteArray& output, std::uint16_t value)
{
    output.append(static_cast<char>(value >> 8U));
    output.append(static_cast<char>(value));
}

void append32(QByteArray& output, std::uint32_t value)
{
    output.append(static_cast<char>(value >> 24U));
    output.append(static_cast<char>(value >> 16U));
    output.append(static_cast<char>(value >> 8U));
    output.append(static_cast<char>(value));
}

std::uint16_t read16(const char* data) noexcept
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(data);
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[0]) << 8U) | bytes[1]);
}

std::uint32_t read32(const char* data) noexcept
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(data);
    return (static_cast<std::uint32_t>(bytes[0]) << 24U)
        | (static_cast<std::uint32_t>(bytes[1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[2]) << 8U)
        | bytes[3];
}

std::size_t maximumGroupCount(const HamDrmLimits& limits) noexcept
{
    // Header groups can be one byte each in adversarial input, but their
    // aggregate remains bounded by maximumHeaderBytes.
    return limits.maximumSegments + limits.maximumHeaderBytes;
}

std::optional<std::size_t> maximumFileBytes(const HamDrmLimits& limits)
{
    const std::size_t groups = maximumGroupCount(limits);
    if (groups > (std::numeric_limits<std::size_t>::max() - kFixedBytes
                  - kDigestBytes - limits.maximumObjectBytes
                  - limits.maximumHeaderBytes) / (4U + kEncodedGroupOverhead)) {
        return std::nullopt;
    }
    return kFixedBytes + kDigestBytes + limits.maximumObjectBytes
        + limits.maximumHeaderBytes
        + groups * (4U + kEncodedGroupOverhead);
}

HamDrmStatus ioFailure(const char* detail)
{
    return HamDrmStatus::failure(HamDrmErrorCode::IoFailure, detail);
}

} // namespace

HamDrmPartialStore::HamDrmPartialStore(QString rootDirectory,
                                       HamDrmLimits limits)
    : rootDirectory_(QDir::cleanPath(std::move(rootDirectory)))
    , limits_(limits)
{
}

QString HamDrmPartialStore::partialDirectory() const
{
    return QDir(rootDirectory_).filePath(QStringLiteral("hamdrm-partials"));
}

QString HamDrmPartialStore::pathForTransportId(
    std::uint16_t transportId) const
{
    const QString filename = QStringLiteral("%1.hamdrm-partial")
        .arg(transportId, 4, 16, QLatin1Char('0'));
    return QDir(partialDirectory()).filePath(filename);
}

HamDrmStatus HamDrmPartialStore::save(
    const HamDrmObjectAssembler& assembler) const
{
    if (rootDirectory_.isEmpty()) {
        return HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                     "partial-store root is empty");
    }
    const auto progress = assembler.progress();
    auto snapshot = assembler.snapshotGroups();
    if (!snapshot.ok()) {
        return snapshot.status;
    }
    if (snapshot.value->empty()
        || snapshot.value->size() > maximumGroupCount(limits_)) {
        return HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                     "partial snapshot group count is invalid");
    }
    const auto maximumBytes = maximumFileBytes(limits_);
    if (!maximumBytes.has_value()) {
        return HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                     "partial-store limits overflow");
    }

    QByteArray serialized;
    serialized.reserve(static_cast<qsizetype>(
        std::min<std::size_t>(*maximumBytes, 4U * 1024U * 1024U)));
    serialized.append(kMagic.data(), static_cast<qsizetype>(kMagic.size()));
    append32(serialized, kFormatVersion);
    append16(serialized, progress.transportId);
    append32(serialized, static_cast<std::uint32_t>(snapshot.value->size()));
    for (const auto& group : *snapshot.value) {
        if (group.empty()
            || group.size() > limits_.maximumSegmentBytes
                    + kEncodedGroupOverhead
            || group.size() > std::numeric_limits<std::uint32_t>::max()) {
            return HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                         "partial group exceeds limit");
        }
        append32(serialized, static_cast<std::uint32_t>(group.size()));
        serialized.append(reinterpret_cast<const char*>(group.data()),
                          static_cast<qsizetype>(group.size()));
        if (static_cast<std::size_t>(serialized.size()) > *maximumBytes) {
            return HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                         "partial snapshot exceeds limit");
        }
    }
    serialized.append(QCryptographicHash::hash(serialized,
                                                QCryptographicHash::Sha256));
    if (static_cast<std::size_t>(serialized.size()) > *maximumBytes) {
        return HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                     "partial snapshot exceeds limit");
    }

    QDir directory;
    if (!directory.mkpath(partialDirectory())) {
        return ioFailure("cannot create HAMDRM partial directory");
    }
    QSaveFile file(pathForTransportId(progress.transportId));
    if (!file.open(QIODevice::WriteOnly)) {
        return ioFailure("cannot open HAMDRM partial for atomic write");
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(serialized) != serialized.size()) {
        file.cancelWriting();
        return ioFailure("short HAMDRM partial write");
    }
    if (!file.commit()) {
        return ioFailure("cannot commit HAMDRM partial atomically");
    }
    return HamDrmStatus::success();
}

HamDrmValueResult<HamDrmObjectAssembler> HamDrmPartialStore::load(
    std::uint16_t transportId) const
{
    const auto maximumBytes = maximumFileBytes(limits_);
    if (!maximumBytes.has_value()) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "partial-store limits overflow")};
    }
    QFile file(pathForTransportId(transportId));
    const QFileInfo info(file);
    if (!info.exists() || !info.isFile() || info.isSymLink()
        || info.size() < static_cast<qint64>(kFixedBytes + kDigestBytes)
        || static_cast<quint64>(info.size()) > *maximumBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                      "HAMDRM partial file is unavailable or invalid")};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return {std::nullopt, ioFailure("cannot open HAMDRM partial")};
    }
    const QByteArray bytes = file.readAll();
    if (bytes.size() != info.size()) {
        return {std::nullopt, ioFailure("short HAMDRM partial read")};
    }
    const qsizetype digestOffset = bytes.size()
        - static_cast<qsizetype>(kDigestBytes);
    const QByteArray expectedDigest = QCryptographicHash::hash(
        bytes.first(digestOffset), QCryptographicHash::Sha256);
    if (bytes.sliced(digestOffset) != expectedDigest) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::CrcMismatch,
                                      "HAMDRM partial checksum failed")};
    }
    const char* data = bytes.constData();
    if (!std::equal(kMagic.begin(), kMagic.end(), data)
        || read32(data + kMagic.size()) != kFormatVersion
        || read16(data + kMagic.size() + 4U) != transportId) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "HAMDRM partial header is invalid")};
    }
    const std::uint32_t groupCount = read32(data + kMagic.size() + 6U);
    if (groupCount == 0U || groupCount > maximumGroupCount(limits_)) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "HAMDRM partial group count is invalid")};
    }

    HamDrmObjectAssembler assembler(transportId, limits_);
    std::size_t offset = kFixedBytes;
    const std::size_t payloadEnd = static_cast<std::size_t>(digestOffset);
    for (std::uint32_t index = 0U; index < groupCount; ++index) {
        if (offset > payloadEnd || payloadEnd - offset < 4U) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Truncated,
                                          "partial group length is truncated")};
        }
        const std::size_t groupBytes = read32(data + offset);
        offset += 4U;
        if (groupBytes == 0U
            || groupBytes > limits_.maximumSegmentBytes
                    + kEncodedGroupOverhead
            || groupBytes > payloadEnd - offset) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                          "partial group length is invalid")};
        }
        const auto ingested = assembler.ingest(
            reinterpret_cast<const std::uint8_t*>(data + offset), groupBytes);
        if (!ingested.ok()) {
            return {std::nullopt, ingested.status};
        }
        offset += groupBytes;
    }
    if (offset != payloadEnd) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "partial file has trailing payload")};
    }
    return {std::move(assembler), HamDrmStatus::success()};
}

HamDrmStatus HamDrmPartialStore::remove(std::uint16_t transportId) const
{
    const QString path = pathForTransportId(transportId);
    const QFileInfo info(path);
    if (!info.exists()) {
        return HamDrmStatus::success();
    }
    if (!info.isFile() || info.isSymLink()) {
        return ioFailure("refusing to remove non-regular HAMDRM partial");
    }
    return QFile::remove(path)
        ? HamDrmStatus::success()
        : ioFailure("cannot remove HAMDRM partial");
}

} // namespace decodium::sstv::hamdrm
