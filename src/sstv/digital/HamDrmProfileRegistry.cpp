// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmProfileRegistry.h"

#include <array>
#include <limits>

namespace decodium::sstv::hamdrm {
namespace {

constexpr std::uint16_t kPayloadBytesPerFrame[2][3][2][3] = {
    {
        {{96U, 160U, 240U}, {67U, 140U, 201U}},
        {{49U, 103U, 149U}, {96U, 160U, 240U}},
        {{67U, 112U, 168U}, {49U, 83U, 124U}},
    },
    {
        {{104U, 174U, 261U}, {78U, 163U, 235U}},
        {{54U, 113U, 163U}, {104U, 174U, 261U}},
        {{78U, 130U, 196U}, {54U, 90U, 135U}},
    },
};

constexpr std::array<HamDrmRobustness, 3> kRobustness {
    HamDrmRobustness::A,
    HamDrmRobustness::B,
    HamDrmRobustness::E,
};
constexpr std::array<HamDrmOccupiedBandwidth, 2> kBandwidth {
    HamDrmOccupiedBandwidth::Hz2300,
    HamDrmOccupiedBandwidth::Hz2500,
};
constexpr std::array<HamDrmProtection, 2> kProtection {
    HamDrmProtection::High,
    HamDrmProtection::Normal,
};
constexpr std::array<HamDrmConstellation, 3> kConstellation {
    HamDrmConstellation::Qam4,
    HamDrmConstellation::Qam16,
    HamDrmConstellation::Qam64,
};
constexpr std::array<HamDrmInterleaver, 2> kInterleaver {
    HamDrmInterleaver::Long,
    HamDrmInterleaver::Short,
};

std::size_t robustnessIndex(HamDrmRobustness value)
{
    switch (value) {
    case HamDrmRobustness::A: return 0U;
    case HamDrmRobustness::B: return 1U;
    case HamDrmRobustness::E: return 2U;
    }
    return std::numeric_limits<std::size_t>::max();
}

std::size_t bandwidthIndex(HamDrmOccupiedBandwidth value)
{
    switch (value) {
    case HamDrmOccupiedBandwidth::Hz2300: return 0U;
    case HamDrmOccupiedBandwidth::Hz2500: return 1U;
    }
    return std::numeric_limits<std::size_t>::max();
}

std::size_t protectionIndex(HamDrmProtection value)
{
    switch (value) {
    case HamDrmProtection::High: return 0U;
    case HamDrmProtection::Normal: return 1U;
    }
    return std::numeric_limits<std::size_t>::max();
}

std::size_t constellationIndex(HamDrmConstellation value)
{
    switch (value) {
    case HamDrmConstellation::Qam4: return 0U;
    case HamDrmConstellation::Qam16: return 1U;
    case HamDrmConstellation::Qam64: return 2U;
    }
    return std::numeric_limits<std::size_t>::max();
}

std::size_t interleaverIndex(HamDrmInterleaver value)
{
    switch (value) {
    case HamDrmInterleaver::Long: return 0U;
    case HamDrmInterleaver::Short: return 1U;
    }
    return std::numeric_limits<std::size_t>::max();
}

std::uint32_t bandwidthHz(HamDrmOccupiedBandwidth value)
{
    return value == HamDrmOccupiedBandwidth::Hz2300 ? 2'300U : 2'500U;
}

std::uint32_t compatibilityCode(HamDrmRobustness robustness,
                                HamDrmOccupiedBandwidth bandwidth,
                                HamDrmProtection protection,
                                HamDrmConstellation constellation,
                                HamDrmInterleaver interleaver)
{
    return static_cast<std::uint32_t>(robustnessIndex(robustness) * 10'000U
        + bandwidthIndex(bandwidth) * 1'000U
        + protectionIndex(protection) * 100U
        + constellationIndex(constellation) * 10U
        + interleaverIndex(interleaver));
}

std::vector<HamDrmProfile> makeProfiles()
{
    std::vector<HamDrmProfile> profiles;
    profiles.reserve(kRobustness.size() * kBandwidth.size()
                     * kProtection.size() * kConstellation.size()
                     * kInterleaver.size());

    for (const auto bandwidth : kBandwidth) {
        for (const auto robustness : kRobustness) {
            for (const auto protection : kProtection) {
                for (const auto constellation : kConstellation) {
                    for (const auto interleaver : kInterleaver) {
                        const std::uint32_t frameBytes = kPayloadBytesPerFrame
                            [bandwidthIndex(bandwidth)]
                            [robustnessIndex(robustness)]
                            [protectionIndex(protection)]
                            [constellationIndex(constellation)];

                        const std::string shortName = hamDrmRobustnessName(robustness)
                            + "-" + std::to_string(bandwidthHz(bandwidth))
                            + "-" + hamDrmProtectionName(protection)
                            + "-" + hamDrmConstellationName(constellation)
                            + "-" + hamDrmInterleaverName(interleaver);
                        const std::string readable = "HAMDRM "
                            + hamDrmRobustnessName(robustness) + " / "
                            + hamDrmBandwidthName(bandwidth) + " / "
                            + hamDrmProtectionName(protection) + " / "
                            + hamDrmConstellationName(constellation) + " / "
                            + hamDrmInterleaverName(interleaver);

                        profiles.push_back({
                            "hamdrm-" + shortName,
                            readable,
                            "QSSTV " + readable,
                            robustness,
                            bandwidth,
                            constellation,
                            protection,
                            interleaver,
                            bandwidthHz(bandwidth),
                            frameBytes,
                            frameBytes * 20U,
                            compatibilityCode(robustness, bandwidth, protection,
                                              constellation, interleaver),
                        });
                    }
                }
            }
        }
    }
    return profiles;
}

} // namespace

std::string hamDrmRobustnessName(HamDrmRobustness value)
{
    switch (value) {
    case HamDrmRobustness::A: return "A";
    case HamDrmRobustness::B: return "B";
    case HamDrmRobustness::E: return "E";
    }
    return "unknown";
}

std::string hamDrmBandwidthName(HamDrmOccupiedBandwidth value)
{
    switch (value) {
    case HamDrmOccupiedBandwidth::Hz2300: return "2.3 kHz";
    case HamDrmOccupiedBandwidth::Hz2500: return "2.5 kHz";
    }
    return "unknown";
}

std::string hamDrmConstellationName(HamDrmConstellation value)
{
    switch (value) {
    case HamDrmConstellation::Qam4: return "4-QAM";
    case HamDrmConstellation::Qam16: return "16-QAM";
    case HamDrmConstellation::Qam64: return "64-QAM";
    }
    return "unknown";
}

std::string hamDrmProtectionName(HamDrmProtection value)
{
    switch (value) {
    case HamDrmProtection::High: return "high";
    case HamDrmProtection::Normal: return "normal";
    }
    return "unknown";
}

std::string hamDrmInterleaverName(HamDrmInterleaver value)
{
    switch (value) {
    case HamDrmInterleaver::Long: return "long";
    case HamDrmInterleaver::Short: return "short";
    }
    return "unknown";
}

const std::vector<HamDrmProfile>& HamDrmProfileRegistry::all()
{
    static const std::vector<HamDrmProfile> profiles = makeProfiles();
    return profiles;
}

const HamDrmProfile* HamDrmProfileRegistry::findById(
    const std::string& id) noexcept
{
    for (const auto& profile : all()) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

const HamDrmProfile* HamDrmProfileRegistry::findByCompatibilityCode(
    std::uint32_t code) noexcept
{
    for (const auto& profile : all()) {
        if (profile.qsstvCompatibilityCode == code) {
            return &profile;
        }
    }
    return nullptr;
}

const HamDrmProfile* HamDrmProfileRegistry::find(
    HamDrmRobustness robustness,
    HamDrmOccupiedBandwidth bandwidth,
    HamDrmProtection protection,
    HamDrmConstellation constellation,
    HamDrmInterleaver interleaver) noexcept
{
    for (const auto& profile : all()) {
        if (profile.robustness == robustness
            && profile.occupiedBandwidth == bandwidth
            && profile.protection == protection
            && profile.constellation == constellation
            && profile.interleaver == interleaver) {
            return &profile;
        }
    }
    return nullptr;
}

HamDrmStatus HamDrmProfileRegistry::validate(const HamDrmProfile& profile)
{
    const auto* canonical = find(profile.robustness,
                                 profile.occupiedBandwidth,
                                 profile.protection,
                                 profile.constellation,
                                 profile.interleaver);
    if (canonical == nullptr) {
        return HamDrmStatus::failure(HamDrmErrorCode::UnsupportedProfile,
                                     "unsupported HAMDRM parameter tuple");
    }
    if (profile.id != canonical->id
        || profile.occupiedBandwidthHz != canonical->occupiedBandwidthHz
        || profile.payloadBytesPer400msFrame
            != canonical->payloadBytesPer400msFrame
        || profile.expectedPayloadBitrate
            != canonical->expectedPayloadBitrate
        || profile.qsstvCompatibilityCode
            != canonical->qsstvCompatibilityCode) {
        return HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                     "profile metadata does not match registry");
    }
    return HamDrmStatus::success();
}

} // namespace decodium::sstv::hamdrm
