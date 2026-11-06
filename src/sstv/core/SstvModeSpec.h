// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace decodium::sstv {

struct SstvVisSpec final
{
    VisEncoding encoding {VisEncoding::Unknown};
    std::uint8_t bitCount {0};
    std::optional<std::uint8_t> standardCode;
    std::vector<std::uint8_t> standardAliases;
    std::vector<std::uint8_t> extendedSequence;
    bool lsbFirst {false};
    Parity parity {Parity::None};

    // A non-empty, equal value permits a deliberately shared code.  The
    // corresponding conflict or alias still has to be explained in statusNote.
    std::string documentedSharedCodeGroup;
};

struct SstvGeometry final
{
    std::optional<std::uint32_t> imageWidth;
    std::optional<std::uint32_t> imageHeight;
    std::optional<std::uint32_t> sampledPixelWidth;
    std::optional<std::uint32_t> transmittedPixelWidth;
    std::optional<std::uint32_t> transmittedLineCount;
    std::optional<std::uint32_t> displayedLineCount;
    std::optional<std::uint32_t> linesPerScan;
};

struct SstvColourSpec final
{
    ColourSpace colourSpace {ColourSpace::Unknown};
    std::vector<ColourComponent> componentOrder;
    ChromaSubsampling chromaSubsampling {ChromaSubsampling::Unknown};
    std::string conversionRule;
};

struct SstvAudioBandwidth final
{
    std::uint32_t lowHz {0};
    std::uint32_t highHz {0};
};

struct SstvTimingSpec final
{
    std::optional<std::uint32_t> syncFrequencyHz;
    std::optional<Picoseconds> syncDuration;
    std::optional<Picoseconds> frontPorch;
    std::optional<Picoseconds> backPorch;
    std::optional<std::uint32_t> separatorFrequencyHz;
    std::optional<Picoseconds> separatorDuration;
    std::optional<Picoseconds> pixelDuration;
    std::optional<Picoseconds> componentDuration;
    std::optional<Picoseconds> lineDuration;
    std::optional<Picoseconds> imageDuration;
    std::optional<SstvAudioBandwidth> nominalAudioBandwidth;
    std::optional<std::uint32_t> tolerancePpm;
};

struct SstvFallbackSignature final
{
    std::optional<Picoseconds> nominalLineDuration;
    std::optional<Picoseconds> nominalSyncDuration;
    std::optional<std::uint32_t> syncFrequencyHz;
    std::string discriminator;
};

struct SstvModeSpec final
{
    std::string id;
    std::string longName;
    std::string shortName;
    std::string family;
    ModeClassification classification {ModeClassification::AnalogSstv};

    CatalogStatus catalogStatus {CatalogStatus::Catalogued};
    CapabilityStatus rxStatus {CapabilityStatus::Unimplemented};
    CapabilityStatus txStatus {CapabilityStatus::Unimplemented};
    CapabilityStatus autoDetectStatus {CapabilityStatus::Unimplemented};

    // This is true only after every protocol field below has an authoritative
    // value and all recorded conflicts have been resolved.
    bool protocolDataComplete {false};
    std::optional<SstvVisSpec> vis;
    SstvGeometry geometry;
    SstvColourSpec colour;
    SstvTimingSpec timing;
    std::string leaderHeaderRules;
    std::string specialLineOrdering;
    SstvFallbackSignature fallbackSignature;

    std::vector<std::string> catalogueReferences;
    std::vector<std::string> protocolProvenance;
    EvidenceStatus evidenceStatus {EvidenceStatus::None};
    std::vector<std::string> implementationEvidenceRefs;
    InteroperabilityStatus interoperabilityStatus {InteroperabilityStatus::NotTested};
    FixtureStatus fixtureStatus {FixtureStatus::Missing};
    std::string statusNote;

    bool claimsRxSupport() const noexcept;
    bool claimsTxSupport() const noexcept;
    bool claimsAnySupport() const noexcept;
    bool hasImplementationEvidence() const noexcept;
    bool hasIndependentEvidence() const noexcept;
};

} // namespace decodium::sstv
