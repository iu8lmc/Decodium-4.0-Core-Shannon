// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "HamDrmTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace decodium::sstv::hamdrm {

struct HamDrmProfile final
{
    std::string id;
    std::string displayName;
    std::string compatibilityName;
    HamDrmRobustness robustness {HamDrmRobustness::A};
    HamDrmOccupiedBandwidth occupiedBandwidth {
        HamDrmOccupiedBandwidth::Hz2300};
    HamDrmConstellation constellation {HamDrmConstellation::Qam4};
    HamDrmProtection protection {HamDrmProtection::High};
    HamDrmInterleaver interleaver {HamDrmInterleaver::Long};
    std::uint32_t occupiedBandwidthHz {2'300U};
    std::uint32_t payloadBytesPer400msFrame {0U};
    std::uint32_t expectedPayloadBitrate {0U};
    // Compatibility code used at the QSSTV object/BSR boundary.  It is never
    // exposed as the profile identity in QML.
    std::uint32_t qsstvCompatibilityCode {0U};
};

class HamDrmProfileRegistry final
{
public:
    static const std::vector<HamDrmProfile>& all();
    static const HamDrmProfile* findById(const std::string& id) noexcept;
    static const HamDrmProfile* findByCompatibilityCode(
        std::uint32_t code) noexcept;
    static const HamDrmProfile* find(HamDrmRobustness robustness,
                                     HamDrmOccupiedBandwidth bandwidth,
                                     HamDrmProtection protection,
                                     HamDrmConstellation constellation,
                                     HamDrmInterleaver interleaver) noexcept;
    static HamDrmStatus validate(const HamDrmProfile& profile);
};

std::string hamDrmRobustnessName(HamDrmRobustness value);
std::string hamDrmBandwidthName(HamDrmOccupiedBandwidth value);
std::string hamDrmConstellationName(HamDrmConstellation value);
std::string hamDrmProtectionName(HamDrmProtection value);
std::string hamDrmInterleaverName(HamDrmInterleaver value);

} // namespace decodium::sstv::hamdrm
