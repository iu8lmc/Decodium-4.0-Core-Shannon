// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmObjectAssembler.h"

#include <algorithm>
#include <utility>

namespace decodium::sstv::hamdrm {

HamDrmObjectAssembler::HamDrmObjectAssembler(std::uint16_t transportId,
                                             HamDrmLimits limits)
    : transportId_(transportId)
    , limits_(limits)
{
}

HamDrmValueResult<HamDrmIngestOutcome> HamDrmObjectAssembler::ingest(
    const std::uint8_t* encodedGroup,
    std::size_t encodedSize)
{
    auto parsed = parseHamDrmMotDataGroup(encodedGroup, encodedSize, limits_);
    if (!parsed.ok()) {
        return {std::nullopt, parsed.status};
    }
    return ingest(*parsed.value);
}

HamDrmValueResult<HamDrmIngestOutcome> HamDrmObjectAssembler::ingest(
    const HamDrmMotDataGroup& group)
{
    if (group.transportId != transportId_) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::TransportMismatch,
                                      "MOT group belongs to another object")};
    }
    if (group.segmentNumber >= limits_.maximumSegments
        || group.payload.empty()
        || group.payload.size() > limits_.maximumSegmentBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "MOT segment exceeds assembler limits")};
    }

    SegmentCollection& collection = group.kind == HamDrmMotGroupKind::Header
        ? header_ : body_;
    const std::size_t byteLimit = group.kind == HamDrmMotGroupKind::Header
        ? limits_.maximumHeaderBytes : limits_.maximumObjectBytes;
    if (collection.totalSegments.has_value()
        && group.segmentNumber >= *collection.totalSegments) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InconsistentObject,
                                      "segment lies beyond declared last segment")};
    }
    if (group.lastSegment) {
        const std::size_t declaredTotal =
            static_cast<std::size_t>(group.segmentNumber) + 1U;
        if (collection.totalSegments.has_value()
            && *collection.totalSegments != declaredTotal) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::InconsistentObject,
                                          "conflicting last-segment markers")};
        }
        if (!collection.segments.empty()
            && collection.segments.rbegin()->first >= declaredTotal) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::InconsistentObject,
                                          "last segment precedes received data")};
        }
        collection.totalSegments = declaredTotal;
    }

    const auto existing = collection.segments.find(group.segmentNumber);
    if (existing != collection.segments.end()) {
        if (existing->second != group.payload) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::ConflictingDuplicate,
                                          "duplicate MOT segment differs")};
        }
        return {HamDrmIngestOutcome::DuplicateIgnored,
                HamDrmStatus::success()};
    }
    if (group.payload.size() > byteLimit - collection.storedBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "stored MOT segments exceed object limit")};
    }
    collection.storedBytes += group.payload.size();
    collection.segments.emplace(group.segmentNumber, group.payload);

    if (collection.totalSegments.has_value()
        && collection.segments.size() == *collection.totalSegments) {
        if (const auto status = validateCompleteCollection(
                collection, group.kind == HamDrmMotGroupKind::Header);
            !status.ok()) {
            return {std::nullopt, status};
        }
    }
    const bool headerWasComplete = metadata_.has_value();
    if (const auto status = updateMetadataIfReady(); !status.ok()) {
        return {std::nullopt, status};
    }
    if (isComplete()) {
        return {HamDrmIngestOutcome::ObjectCompleted,
                HamDrmStatus::success()};
    }
    if (!headerWasComplete && metadata_.has_value()) {
        return {HamDrmIngestOutcome::HeaderCompleted,
                HamDrmStatus::success()};
    }
    return {HamDrmIngestOutcome::SegmentAdded, HamDrmStatus::success()};
}

HamDrmStatus HamDrmObjectAssembler::validateCompleteCollection(
    const SegmentCollection& collection,
    bool header) const
{
    if (!collection.totalSegments.has_value()
        || collection.segments.size() != *collection.totalSegments
        || *collection.totalSegments == 0U) {
        return HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                     "MOT segment collection is incomplete");
    }
    std::size_t expectedRegularSize = 0U;
    for (std::size_t index = 0U; index < *collection.totalSegments; ++index) {
        const auto found = collection.segments.find(
            static_cast<std::uint16_t>(index));
        if (found == collection.segments.end()) {
            return HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                         "MOT segment gap remains");
        }
        if (index + 1U < *collection.totalSegments) {
            if (expectedRegularSize == 0U) {
                expectedRegularSize = found->second.size();
            } else if (found->second.size() != expectedRegularSize) {
                return HamDrmStatus::failure(
                    HamDrmErrorCode::InconsistentObject,
                    "non-final MOT segments have inconsistent sizes");
            }
        } else if (expectedRegularSize != 0U
                   && found->second.size() > expectedRegularSize) {
            return HamDrmStatus::failure(
                HamDrmErrorCode::InconsistentObject,
                "final MOT segment is larger than regular segments");
        }
    }
    const std::size_t limit = header ? limits_.maximumHeaderBytes
                                     : limits_.maximumObjectBytes;
    if (collection.storedBytes == 0U || collection.storedBytes > limit) {
        return HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                     "complete MOT collection exceeds limit");
    }
    return HamDrmStatus::success();
}

std::vector<std::uint8_t> HamDrmObjectAssembler::concatenate(
    const SegmentCollection& collection) const
{
    std::vector<std::uint8_t> output;
    output.reserve(collection.storedBytes);
    if (!collection.totalSegments.has_value()) {
        return output;
    }
    for (std::size_t index = 0U; index < *collection.totalSegments; ++index) {
        const auto found = collection.segments.find(
            static_cast<std::uint16_t>(index));
        if (found == collection.segments.end()) {
            return {};
        }
        output.insert(output.end(), found->second.begin(), found->second.end());
    }
    return output;
}

HamDrmStatus HamDrmObjectAssembler::updateMetadataIfReady()
{
    if (!metadata_.has_value() && header_.totalSegments.has_value()
        && header_.segments.size() == *header_.totalSegments) {
        const auto bytes = concatenate(header_);
        auto parsed = parseHamDrmMotHeader(bytes.data(), bytes.size(),
                                           transportId_, limits_);
        if (!parsed.ok()) {
            return parsed.status;
        }
        metadata_ = std::move(*parsed.value);
    }
    if (metadata_.has_value()) {
        if (body_.storedBytes > metadata_->bodySize) {
            return HamDrmStatus::failure(HamDrmErrorCode::InconsistentObject,
                                         "received body exceeds MOT body size");
        }
        if (body_.totalSegments.has_value()
            && body_.segments.size() == *body_.totalSegments
            && body_.storedBytes != metadata_->bodySize) {
            return HamDrmStatus::failure(HamDrmErrorCode::InconsistentObject,
                                         "complete body size differs from header");
        }
    }
    return HamDrmStatus::success();
}

bool HamDrmObjectAssembler::isComplete() const noexcept
{
    return metadata_.has_value() && body_.totalSegments.has_value()
        && body_.segments.size() == *body_.totalSegments
        && body_.storedBytes == metadata_->bodySize;
}

HamDrmAssemblyProgress HamDrmObjectAssembler::progress() const noexcept
{
    HamDrmAssemblyProgress result;
    result.transportId = transportId_;
    result.headerComplete = metadata_.has_value();
    result.bodyExtentKnown = body_.totalSegments.has_value();
    result.objectComplete = isComplete();
    result.headerSegmentsReceived = header_.segments.size();
    result.bodySegmentsReceived = body_.segments.size();
    result.totalBodySegments = body_.totalSegments.value_or(0U);
    result.bodyBytesReceived = body_.storedBytes;
    result.expectedBodyBytes = metadata_.has_value() ? metadata_->bodySize : 0U;
    return result;
}

std::vector<std::uint16_t> HamDrmObjectAssembler::missingBodySegments() const
{
    std::vector<std::uint16_t> output;
    if (!body_.totalSegments.has_value()) {
        return output;
    }
    output.reserve(*body_.totalSegments - body_.segments.size());
    for (std::size_t index = 0U; index < *body_.totalSegments; ++index) {
        if (body_.segments.find(static_cast<std::uint16_t>(index))
            == body_.segments.end()) {
            output.push_back(static_cast<std::uint16_t>(index));
        }
    }
    return output;
}

HamDrmValueResult<HamDrmAssembledObject>
HamDrmObjectAssembler::assembledObject() const
{
    if (!isComplete()) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                      "HAMDRM object is incomplete")};
    }
    HamDrmAssembledObject output;
    output.metadata = *metadata_;
    output.originalBytes = concatenate(body_);
    return {std::move(output), HamDrmStatus::success()};
}

HamDrmValueResult<std::vector<std::vector<std::uint8_t>>>
HamDrmObjectAssembler::snapshotGroups() const
{
    std::vector<std::vector<std::uint8_t>> output;
    output.reserve(header_.segments.size() + body_.segments.size());
    const auto appendCollection = [&](const SegmentCollection& collection,
                                      HamDrmMotGroupKind kind)
        -> HamDrmStatus {
        std::uint8_t continuity = 0U;
        for (const auto& entry : collection.segments) {
            HamDrmMotDataGroup group;
            group.kind = kind;
            group.continuityIndex = continuity++ & 0x0fU;
            group.segmentNumber = entry.first;
            group.lastSegment = collection.totalSegments.has_value()
                && static_cast<std::size_t>(entry.first) + 1U
                    == *collection.totalSegments;
            group.transportId = transportId_;
            group.payload = entry.second;
            auto encoded = encodeHamDrmMotDataGroup(group, limits_);
            if (!encoded.ok()) {
                return encoded.status;
            }
            output.push_back(std::move(*encoded.value));
        }
        return HamDrmStatus::success();
    };
    if (const auto status = appendCollection(header_,
                                             HamDrmMotGroupKind::Header);
        !status.ok()) {
        return {std::nullopt, status};
    }
    if (const auto status = appendCollection(body_, HamDrmMotGroupKind::Body);
        !status.ok()) {
        return {std::nullopt, status};
    }
    return {std::move(output), HamDrmStatus::success()};
}

} // namespace decodium::sstv::hamdrm
