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

struct HamDrmPartialBody final
{
    std::vector<std::uint8_t> bytes;
    std::vector<std::size_t> erasures;
    std::vector<std::uint16_t> missingSegments;
    std::size_t segmentSize {0U};
    std::size_t totalSegments {0U};
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
    // Requires the final marker, metadata and consistent regular segment
    // lengths to make every missing byte's offset unambiguous.
    std::optional<HamDrmPartialBody> partialBodyForRepair(bool allowMissingFinal = false) const;

    // Produces canonical, CRC-protected groups suitable for an atomic partial
    // persistence layer.  Replaying them reconstructs the same state.
    HamDrmValueResult<std::vector<std::vector<std::uint8_t>>>
    snapshotGroups() const;

    // Visit the existing segment payloads without creating a second copy of
    // the object. References are valid only for the duration of the callback.
    template<typename Visitor>
    void forEachSegment(Visitor&& visitor) const
    {
        const auto visit = [&](const SegmentCollection& collection, HamDrmMotGroupKind kind) {
            for (const auto& entry : collection.segments) {
                const bool last = collection.totalSegments.has_value()
                    && static_cast<std::size_t>(entry.first) + 1U == *collection.totalSegments;
                visitor(kind, entry.first, last, entry.second);
            }
        };
        visit(header_, HamDrmMotGroupKind::Header);
        visit(body_, HamDrmMotGroupKind::Body);
    }

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
