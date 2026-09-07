// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "HamDrmMotCodec.h"
#include "HamDrmTypes.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace decodium::sstv::hamdrm {

enum class HamDrmIngestOutcome : std::uint8_t {
    SegmentAdded,
    DuplicateIgnored,
    HeaderCompleted,
    ObjectCompleted,
};

struct HamDrmAssemblyProgress final
{
    std::uint16_t transportId {0U};
    bool headerComplete {false};
    bool bodyExtentKnown {false};
    bool objectComplete {false};
    std::size_t headerSegmentsReceived {0U};
    std::size_t bodySegmentsReceived {0U};
    std::size_t totalBodySegments {0U};
    std::size_t bodyBytesReceived {0U};
    std::size_t expectedBodyBytes {0U};
};

struct HamDrmAssembledObject final
{
    HamDrmMotObjectMetadata metadata;
    std::vector<std::uint8_t> originalBytes;
};

class HamDrmObjectAssembler final
{
public:
    explicit HamDrmObjectAssembler(std::uint16_t transportId,
                                   HamDrmLimits limits = {});

    HamDrmValueResult<HamDrmIngestOutcome> ingest(
        const std::uint8_t* encodedGroup,
        std::size_t encodedSize);
    HamDrmValueResult<HamDrmIngestOutcome> ingest(
        const HamDrmMotDataGroup& group);

    HamDrmAssemblyProgress progress() const noexcept;
    std::vector<std::uint16_t> missingBodySegments() const;
    HamDrmValueResult<HamDrmAssembledObject> assembledObject() const;

    // Produces canonical, CRC-protected groups suitable for an atomic partial
    // persistence layer.  Replaying them reconstructs the same state.
    HamDrmValueResult<std::vector<std::vector<std::uint8_t>>>
    snapshotGroups() const;

private:
    struct SegmentCollection final
    {
        std::map<std::uint16_t, std::vector<std::uint8_t>> segments;
        std::optional<std::size_t> totalSegments;
        std::size_t storedBytes {0U};
    };

    HamDrmStatus validateCompleteCollection(
        const SegmentCollection& collection,
        bool header) const;
    std::vector<std::uint8_t> concatenate(
        const SegmentCollection& collection) const;
    HamDrmStatus updateMetadataIfReady();
    bool isComplete() const noexcept;

    std::uint16_t transportId_ {0U};
    HamDrmLimits limits_;
    SegmentCollection header_;
    SegmentCollection body_;
    std::optional<HamDrmMotObjectMetadata> metadata_;
};

} // namespace decodium::sstv::hamdrm
