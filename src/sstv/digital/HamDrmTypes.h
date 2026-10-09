// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace decodium::sstv::hamdrm {

// These enums describe the narrow-band amateur DRM profile observed in
// interoperable HAMDRM implementations.  They deliberately do not model or
// advertise the complete broadcast DRM profile space.
enum class HamDrmRobustness : std::uint8_t {
    A,
    B,
    E,
};

enum class HamDrmOccupiedBandwidth : std::uint8_t {
    Hz2300,
    Hz2500,
};

enum class HamDrmConstellation : std::uint8_t {
    Qam4,
    Qam16,
    Qam64,
};

enum class HamDrmProtection : std::uint8_t {
    High,
    Normal,
};

enum class HamDrmInterleaver : std::uint8_t {
    Long,
    Short,
};

struct HamDrmLimits final
{
    std::size_t maximumObjectBytes {16U * 1024U * 1024U};
    std::size_t maximumHeaderBytes {8U * 1024U};
    std::size_t maximumSegmentBytes {8'191U};
    std::size_t maximumSegments {32'768U};
    std::size_t maximumFilenameBytes {80U};
    std::uint32_t maximumImageDimension {8'192U};
    std::uint64_t maximumImagePixels {16'777'216ULL};
};

enum class HamDrmErrorCode : std::uint8_t {
    None,
    InvalidArgument,
    UnsupportedProfile,
    UnsupportedFeature,
    UnsupportedContent,
    UnsafeFilename,
    LimitExceeded,
    Truncated,
    Malformed,
    CrcMismatch,
    TransportMismatch,
    ConflictingDuplicate,
    InconsistentObject,
    Incomplete,
    IoFailure,
};

struct HamDrmStatus final
{
    HamDrmErrorCode code {HamDrmErrorCode::None};
    std::string detail;

    bool ok() const noexcept { return code == HamDrmErrorCode::None; }
    explicit operator bool() const noexcept { return ok(); }

    static HamDrmStatus success() { return {}; }
    static HamDrmStatus failure(HamDrmErrorCode failureCode,
                                std::string failureDetail)
    {
        return {failureCode, std::move(failureDetail)};
    }
};

template<typename T>
struct HamDrmValueResult final
{
    std::optional<T> value;
    HamDrmStatus status;

    bool ok() const noexcept { return value.has_value() && status.ok(); }
};

} // namespace decodium::sstv::hamdrm
