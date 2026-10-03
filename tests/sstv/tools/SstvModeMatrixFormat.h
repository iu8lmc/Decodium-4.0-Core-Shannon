// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "src/sstv/core/SstvModeSpec.h"

#include <QString>
#include <QStringList>

#include <cstdint>

namespace decodium::sstv::mode_matrix {

inline QString capability(CapabilityStatus status)
{
    switch (status) {
    case CapabilityStatus::Unimplemented: return QStringLiteral("—");
    case CapabilityStatus::Implemented: return QStringLiteral("impl.");
    case CapabilityStatus::Verified: return QStringLiteral("verified");
    case CapabilityStatus::Blocked: return QStringLiteral("blocked");
    }
    return QStringLiteral("—");
}

inline QString evidence(EvidenceStatus status)
{
    switch (status) {
    case EvidenceStatus::None: return QStringLiteral("none");
    case EvidenceStatus::AuditedSources:
        return QStringLiteral("audited sources");
    case EvidenceStatus::DeterministicTests:
        return QStringLiteral("deterministic tests");
    case EvidenceStatus::IndependentVector:
        return QStringLiteral("independent vector");
    }
    return QStringLiteral("none");
}

inline QString interoperability(InteroperabilityStatus status)
{
    switch (status) {
    case InteroperabilityStatus::NotTested:
        return QStringLiteral("not tested");
    case InteroperabilityStatus::UpstreamPathObserved:
        return QStringLiteral("upstream path observed");
    case InteroperabilityStatus::IndependentlyVerified:
        return QStringLiteral("independently verified");
    case InteroperabilityStatus::Blocked:
        return QStringLiteral("blocked");
    }
    return QStringLiteral("not tested");
}

inline QString externalApplicationStatus(const SstvModeSpec& mode)
{
    return mode.claimsAnySupport() ? QStringLiteral("unverified")
                                   : QStringLiteral("not available");
}

inline QString dimensions(const SstvModeSpec& mode)
{
    if (!mode.geometry.imageWidth || !mode.geometry.imageHeight) {
        return QStringLiteral("—");
    }
    const QString raster = QStringLiteral("%1×%2")
        .arg(*mode.geometry.imageWidth)
        .arg(*mode.geometry.imageHeight);
    if (mode.geometry.sampledPixelWidth
        && *mode.geometry.sampledPixelWidth != *mode.geometry.imageWidth) {
        return QStringLiteral("%1 (effective %2×%3)")
            .arg(raster)
            .arg(*mode.geometry.sampledPixelWidth)
            .arg(*mode.geometry.imageHeight);
    }
    return raster;
}

inline QString nominalDurationSeconds(const SstvModeSpec& mode)
{
    if (!mode.timing.imageDuration
        || mode.timing.imageDuration->count < 0) {
        return QStringLiteral("—");
    }
    constexpr std::int64_t picosecondsPerMicrosecond = 1'000'000LL;
    std::int64_t seconds = mode.timing.imageDuration->count
        / kPicosecondsPerSecond;
    const std::int64_t remainder = mode.timing.imageDuration->count
        % kPicosecondsPerSecond;
    std::int64_t microseconds =
        (remainder + picosecondsPerMicrosecond / 2)
        / picosecondsPerMicrosecond;
    if (microseconds >= 1'000'000LL) {
        ++seconds;
        microseconds = 0;
    }
    return QStringLiteral("%1.%2")
        .arg(seconds)
        .arg(microseconds, 6, 10, QLatin1Char('0'));
}

inline QString vis(const SstvModeSpec& mode)
{
    if (!mode.vis) {
        return QStringLiteral("—");
    }
    const SstvVisSpec& value = *mode.vis;
    if (value.encoding == VisEncoding::StandardSevenBit
        && value.standardCode) {
        QString result = QStringLiteral("standard %1").arg(
            *value.standardCode);
        if (!value.standardAliases.empty()) {
            QStringList aliases;
            for (const std::uint8_t alias : value.standardAliases) {
                aliases.append(QString::number(alias));
            }
            result += QStringLiteral(" (aliases %1)")
                .arg(aliases.join(QLatin1Char(',')));
        }
        return result;
    }
    if ((value.encoding == VisEncoding::Extended
         || value.encoding == VisEncoding::Narrow24Bit)
        && !value.extendedSequence.empty()) {
        QStringList bytes;
        for (const std::uint8_t byte : value.extendedSequence) {
            bytes.append(QStringLiteral("0x%1")
                .arg(byte, 2, 16, QLatin1Char('0')));
        }
        return QStringLiteral("%1 %2")
            .arg(value.encoding == VisEncoding::Narrow24Bit
                     ? QStringLiteral("N-VIS")
                     : QStringLiteral("extended VIS"),
                 bytes.join(QLatin1Char('/')));
    }
    return QStringLiteral("—");
}

} // namespace decodium::sstv::mode_matrix
