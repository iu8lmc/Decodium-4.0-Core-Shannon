// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/digital/HamDrmBsrCodec.h"
#include "src/sstv/digital/HamDrmCrc.h"
#include "src/sstv/digital/HamDrmImageValidator.h"
#include "src/sstv/digital/HamDrmMotCodec.h"
#include "src/sstv/digital/HamDrmObjectAssembler.h"
#include "src/sstv/digital/HamDrmProfileRegistry.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace hamdrm = decodium::sstv::hamdrm;

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line)
{
    if (!condition) {
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

std::vector<std::uint8_t> patternedBytes(std::size_t size)
{
    std::vector<std::uint8_t> output(size);
    for (std::size_t index = 0U; index < size; ++index) {
        output[index] = static_cast<std::uint8_t>(
            (index * 73U + index / 7U + 19U) & 0xffU);
    }
    return output;
}

void appendBig32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
    output.push_back(static_cast<std::uint8_t>(value >> 24U));
    output.push_back(static_cast<std::uint8_t>(value >> 16U));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
    output.push_back(static_cast<std::uint8_t>(value));
}

std::vector<std::uint8_t> minimalJpeg(std::uint16_t width,
                                      std::uint16_t height)
{
    return {
        0xffU, 0xd8U,
        0xffU, 0xc0U, 0x00U, 0x11U, 0x08U,
        static_cast<std::uint8_t>(height >> 8U),
        static_cast<std::uint8_t>(height),
        static_cast<std::uint8_t>(width >> 8U),
        static_cast<std::uint8_t>(width),
        0x03U,
        0x01U, 0x11U, 0x00U,
        0x02U, 0x11U, 0x00U,
        0x03U, 0x11U, 0x00U,
        0xffU, 0xd9U,
    };
}

std::vector<std::uint8_t> minimalPng(std::uint32_t width,
                                     std::uint32_t height)
{
    std::vector<std::uint8_t> output {
        0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU,
    };
    appendBig32(output, 13U);
    output.insert(output.end(), {'I', 'H', 'D', 'R'});
    appendBig32(output, width);
    appendBig32(output, height);
    output.insert(output.end(), {8U, 2U, 0U, 0U, 0U});
    output.insert(output.end(), 4U, 0U); // CRC is checked by the full decoder.
    return output;
}

std::vector<std::uint8_t> minimalJp2(std::uint32_t width,
                                     std::uint32_t height)
{
    std::vector<std::uint8_t> output {
        0x00U, 0x00U, 0x00U, 0x0cU, 0x6aU, 0x50U,
        0x20U, 0x20U, 0x0dU, 0x0aU, 0x87U, 0x0aU,
    };
    appendBig32(output, 30U);
    output.insert(output.end(), {'j', 'p', '2', 'h'});
    appendBig32(output, 22U);
    output.insert(output.end(), {'i', 'h', 'd', 'r'});
    appendBig32(output, height);
    appendBig32(output, width);
    output.insert(output.end(), {0U, 3U, 7U, 7U, 0U, 0U});
    return output;
}

void testCrcAndProfiles()
{
    const std::string checkText = "123456789";
    CHECK(hamdrm::hamDrmCrc16X25(
              reinterpret_cast<const std::uint8_t*>(checkText.data()),
              checkText.size()) == 0x906eU);

    const auto& profiles = hamdrm::HamDrmProfileRegistry::all();
    CHECK(profiles.size() == 72U);
    std::set<std::string> ids;
    std::set<std::uint32_t> codes;
    for (const auto& profile : profiles) {
        CHECK(ids.insert(profile.id).second);
        CHECK(codes.insert(profile.qsstvCompatibilityCode).second);
        CHECK(profile.expectedPayloadBitrate
              == profile.payloadBytesPer400msFrame * 20U);
        CHECK(hamdrm::HamDrmProfileRegistry::validate(profile).ok());
    }

    const auto* robust = hamdrm::HamDrmProfileRegistry::find(
        hamdrm::HamDrmRobustness::A,
        hamdrm::HamDrmOccupiedBandwidth::Hz2300,
        hamdrm::HamDrmProtection::High,
        hamdrm::HamDrmConstellation::Qam4,
        hamdrm::HamDrmInterleaver::Long);
    CHECK(robust != nullptr);
    CHECK(robust != nullptr && robust->payloadBytesPer400msFrame == 96U);
    CHECK(robust != nullptr && robust->expectedPayloadBitrate == 1'920U);
    CHECK(robust != nullptr && robust->qsstvCompatibilityCode == 0U);

    const auto* fast = hamdrm::HamDrmProfileRegistry::findByCompatibilityCode(
        21'121U);
    CHECK(fast != nullptr);
    CHECK(fast != nullptr && fast->payloadBytesPer400msFrame == 135U);
}

void testDataGroupCodec()
{
    hamdrm::HamDrmMotDataGroup group;
    group.kind = hamdrm::HamDrmMotGroupKind::Body;
    group.continuityIndex = 5U;
    group.repetitionIndex = 2U;
    group.segmentNumber = 3U;
    group.lastSegment = true;
    group.transportId = 0x1234U;
    group.segmentRepetitionCount = 2U;
    group.payload = {1U, 2U, 3U};

    const auto encoded = hamdrm::encodeHamDrmMotDataGroup(group);
    CHECK(encoded.ok());
    if (!encoded.ok()) {
        return;
    }
    const std::vector<std::uint8_t> prefix {
        0x74U, 0x52U, 0x80U, 0x03U, 0x12U, 0x12U, 0x34U,
        0x40U, 0x03U, 0x01U, 0x02U, 0x03U,
    };
    CHECK(encoded.value->size() == prefix.size() + 2U);
    CHECK(std::equal(prefix.begin(), prefix.end(), encoded.value->begin()));

    const auto parsed = hamdrm::parseHamDrmMotDataGroup(
        encoded.value->data(), encoded.value->size());
    CHECK(parsed.ok());
    CHECK(parsed.ok() && parsed.value->kind == group.kind);
    CHECK(parsed.ok() && parsed.value->continuityIndex == 5U);
    CHECK(parsed.ok() && parsed.value->repetitionIndex == 2U);
    CHECK(parsed.ok() && parsed.value->segmentNumber == 3U);
    CHECK(parsed.ok() && parsed.value->lastSegment);
    CHECK(parsed.ok() && parsed.value->transportId == 0x1234U);
    CHECK(parsed.ok() && parsed.value->segmentRepetitionCount == 2U);
    CHECK(parsed.ok() && parsed.value->payload == group.payload);

    auto corrupted = *encoded.value;
    corrupted[10] ^= 0x80U;
    const auto crcFailure = hamdrm::parseHamDrmMotDataGroup(
        corrupted.data(), corrupted.size());
    CHECK(!crcFailure.ok());
    CHECK(crcFailure.status.code == hamdrm::HamDrmErrorCode::CrcMismatch);

    auto unsupported = *encoded.value;
    unsupported[0] |= 0x80U;
    const auto unsupportedResult = hamdrm::parseHamDrmMotDataGroup(
        unsupported.data(), unsupported.size());
    CHECK(!unsupportedResult.ok());
    CHECK(unsupportedResult.status.code
          == hamdrm::HamDrmErrorCode::UnsupportedFeature);
}

void testMotHeaderAndObjectAssembly()
{
    hamdrm::HamDrmMotObjectMetadata metadata;
    metadata.transportId = 0x2345U;
    metadata.bodySize = 513U;
    metadata.filename = "test-image.jpg";
    metadata.version = 7U;

    const auto header = hamdrm::encodeHamDrmMotHeader(metadata);
    CHECK(header.ok());
    if (!header.ok()) {
        return;
    }
    const auto parsedHeader = hamdrm::parseHamDrmMotHeader(
        header.value->data(), header.value->size(), metadata.transportId);
    CHECK(parsedHeader.ok());
    CHECK(parsedHeader.ok() && parsedHeader.value->bodySize == 513U);
    CHECK(parsedHeader.ok()
          && parsedHeader.value->headerSize == header.value->size());
    CHECK(parsedHeader.ok()
          && parsedHeader.value->filename == "test-image.jpg");
    CHECK(parsedHeader.ok()
          && parsedHeader.value->mimeType == "image/jpeg");
    CHECK(parsedHeader.ok() && parsedHeader.value->version == 7U);

    auto spoofed = *header.value;
    spoofed[6] = 3U; // PNG subtype with a JPEG ContentName.
    const auto spoofedResult = hamdrm::parseHamDrmMotHeader(
        spoofed.data(), spoofed.size(), metadata.transportId);
    CHECK(!spoofedResult.ok());
    CHECK(spoofedResult.status.code
          == hamdrm::HamDrmErrorCode::InconsistentObject);

    auto unsafeMetadata = metadata;
    unsafeMetadata.filename = "../escape.jpg";
    CHECK(!hamdrm::encodeHamDrmMotHeader(unsafeMetadata).ok());

    const auto body = patternedBytes(metadata.bodySize);
    const auto encodedObject = hamdrm::encodeHamDrmObject(
        metadata, body, 82U);
    CHECK(encodedObject.ok());
    if (!encodedObject.ok()) {
        return;
    }
    CHECK(encodedObject.value->headerGroups.size() == 1U);
    CHECK(encodedObject.value->bodyGroups.size() == 7U);

    hamdrm::HamDrmObjectAssembler assembler(metadata.transportId);
    const auto lastFirst = assembler.ingest(
        encodedObject.value->bodyGroups.back().data(),
        encodedObject.value->bodyGroups.back().size());
    CHECK(lastFirst.ok());
    CHECK(assembler.progress().bodyExtentKnown);
    CHECK(assembler.missingBodySegments().size() == 6U);

    const auto headerOutcome = assembler.ingest(
        encodedObject.value->headerGroups.front().data(),
        encodedObject.value->headerGroups.front().size());
    CHECK(headerOutcome.ok());
    CHECK(headerOutcome.ok()
          && *headerOutcome.value == hamdrm::HamDrmIngestOutcome::HeaderCompleted);
    CHECK(assembler.progress().headerComplete);

    const auto segmentTwo = assembler.ingest(
        encodedObject.value->bodyGroups[2].data(),
        encodedObject.value->bodyGroups[2].size());
    CHECK(segmentTwo.ok());
    const auto duplicate = assembler.ingest(
        encodedObject.value->bodyGroups[2].data(),
        encodedObject.value->bodyGroups[2].size());
    CHECK(duplicate.ok());
    CHECK(duplicate.ok()
          && *duplicate.value == hamdrm::HamDrmIngestOutcome::DuplicateIgnored);

    const auto decodedTwo = hamdrm::parseHamDrmMotDataGroup(
        encodedObject.value->bodyGroups[2].data(),
        encodedObject.value->bodyGroups[2].size());
    CHECK(decodedTwo.ok());
    if (decodedTwo.ok()) {
        auto conflicting = *decodedTwo.value;
        conflicting.payload.front() ^= 1U;
        const auto conflictResult = assembler.ingest(conflicting);
        CHECK(!conflictResult.ok());
        CHECK(conflictResult.status.code
              == hamdrm::HamDrmErrorCode::ConflictingDuplicate);
    }

    for (std::size_t index = 0U;
         index + 1U < encodedObject.value->bodyGroups.size(); ++index) {
        if (index == 2U) {
            continue;
        }
        const auto result = assembler.ingest(
            encodedObject.value->bodyGroups[index].data(),
            encodedObject.value->bodyGroups[index].size());
        CHECK(result.ok());
    }
    CHECK(assembler.progress().objectComplete);
    CHECK(assembler.missingBodySegments().empty());
    const auto assembled = assembler.assembledObject();
    CHECK(assembled.ok());
    CHECK(assembled.ok() && assembled.value->originalBytes == body);
    CHECK(assembled.ok()
          && assembled.value->metadata.filename == metadata.filename);

    hamdrm::HamDrmObjectAssembler partial(metadata.transportId);
    CHECK(partial.ingest(encodedObject.value->headerGroups[0].data(),
                         encodedObject.value->headerGroups[0].size()).ok());
    CHECK(partial.ingest(encodedObject.value->bodyGroups[0].data(),
                         encodedObject.value->bodyGroups[0].size()).ok());
    CHECK(partial.ingest(encodedObject.value->bodyGroups.back().data(),
                         encodedObject.value->bodyGroups.back().size()).ok());
    const auto snapshot = partial.snapshotGroups();
    CHECK(snapshot.ok());
    hamdrm::HamDrmObjectAssembler resumed(metadata.transportId);
    if (snapshot.ok()) {
        for (const auto& group : *snapshot.value) {
            CHECK(resumed.ingest(group.data(), group.size()).ok());
        }
    }
    CHECK(resumed.progress().headerComplete);
    CHECK(resumed.progress().bodySegmentsReceived == 2U);
    CHECK(resumed.missingBodySegments() == partial.missingBodySegments());
}

void testBsr()
{
    hamdrm::HamDrmBsrRequest request;
    request.transportId = 0x2345U;
    request.segmentSize = 82U;
    request.missingSegments = {1U, 2U, 3U, 7U, 10U, 11U};

    const auto compatible = hamdrm::encodeHamDrmBsr(
        request, hamdrm::HamDrmBsrDialect::EasyPalCompatible);
    CHECK(compatible.ok());
    if (compatible.ok()) {
        const std::string text(compatible.value->begin(),
                               compatible.value->end());
        // The compatibility range marker deliberately over-requests any
        // already received filler blocks between two sparse runs, matching
        // the interoperable EasyPal/QSSTV behaviour.
        CHECK(text.find("1\n-1\n7\n10\n-1\n11\n-99\n")
              != std::string::npos);
        const auto parsed = hamdrm::parseHamDrmBsr(
            compatible.value->data(), compatible.value->size());
        CHECK(parsed.ok());
        const std::vector<std::uint16_t> compatibleRequest {
            1U, 2U, 3U, 4U, 5U, 6U, 7U, 10U, 11U,
        };
        CHECK(parsed.ok()
              && parsed.value->missingSegments == compatibleRequest);
    }

    request.filename = "test-image.jpg";
    request.qsstvCompatibilityCode = 0U;
    const auto extended = hamdrm::encodeHamDrmBsr(
        request, hamdrm::HamDrmBsrDialect::QsstvExtended);
    CHECK(extended.ok());
    if (extended.ok()) {
        const auto parsed = hamdrm::parseHamDrmBsr(
            extended.value->data(), extended.value->size());
        CHECK(parsed.ok());
        CHECK(parsed.ok()
              && parsed.value->missingSegments == request.missingSegments);
        CHECK(parsed.ok() && parsed.value->filename == request.filename);
        CHECK(parsed.ok()
              && parsed.value->qsstvCompatibilityCode
                    == request.qsstvCompatibilityCode);
    }

    const std::string badRange = "9029\nH_OK\n82\n3\n-1\n2\n-99\n";
    const auto badRangeResult = hamdrm::parseHamDrmBsr(
        reinterpret_cast<const std::uint8_t*>(badRange.data()),
        badRange.size());
    CHECK(!badRangeResult.ok());

    const std::string unsupportedProfile =
        "9029\nH_OK\n82\n3\n-99\ntest-image.jpg\n99999\n";
    const auto unsupportedResult = hamdrm::parseHamDrmBsr(
        reinterpret_cast<const std::uint8_t*>(unsupportedProfile.data()),
        unsupportedProfile.size());
    CHECK(!unsupportedResult.ok());
    CHECK(unsupportedResult.status.code
          == hamdrm::HamDrmErrorCode::UnsupportedProfile);
}

void testImageValidation()
{
    hamdrm::HamDrmMotObjectMetadata metadata;
    metadata.filename = "received.jpg";
    metadata.mimeType = "image/jpeg";
    const auto jpeg = minimalJpeg(320U, 240U);
    metadata.bodySize = static_cast<std::uint32_t>(jpeg.size());
    const auto jpegInfo = hamdrm::validateHamDrmImage(
        metadata, jpeg.data(), jpeg.size());
    CHECK(jpegInfo.ok());
    CHECK(jpegInfo.ok() && jpegInfo.value->width == 320U);
    CHECK(jpegInfo.ok() && jpegInfo.value->height == 240U);
    CHECK(jpegInfo.ok()
          && jpegInfo.value->format == hamdrm::HamDrmImageFormat::Jpeg);

    auto corruptedJpeg = jpeg;
    corruptedJpeg[1] = 0U;
    CHECK(!hamdrm::validateHamDrmImage(
               metadata, corruptedJpeg.data(), corruptedJpeg.size()).ok());

    const auto png = minimalPng(640U, 480U);
    metadata.filename = "received.png";
    metadata.mimeType = "image/png";
    metadata.bodySize = static_cast<std::uint32_t>(png.size());
    const auto pngInfo = hamdrm::validateHamDrmImage(
        metadata, png.data(), png.size());
    CHECK(pngInfo.ok());
    CHECK(pngInfo.ok() && pngInfo.value->width == 640U);

    const auto oversizedPng = minimalPng(9'000U, 2U);
    metadata.bodySize = static_cast<std::uint32_t>(oversizedPng.size());
    const auto oversizedResult = hamdrm::validateHamDrmImage(
        metadata, oversizedPng.data(), oversizedPng.size());
    CHECK(!oversizedResult.ok());
    CHECK(oversizedResult.status.code
          == hamdrm::HamDrmErrorCode::LimitExceeded);

    const auto jp2 = minimalJp2(1'024U, 768U);
    metadata.filename = "received.jp2";
    metadata.mimeType = "image/jp2";
    metadata.bodySize = static_cast<std::uint32_t>(jp2.size());
    const auto jp2Info = hamdrm::validateHamDrmImage(
        metadata, jp2.data(), jp2.size());
    CHECK(jp2Info.ok());
    CHECK(jp2Info.ok() && jp2Info.value->width == 1'024U);
    CHECK(jp2Info.ok()
          && jp2Info.value->format == hamdrm::HamDrmImageFormat::Jpeg2000);

    auto malformedJp2 = jp2;
    malformedJp2[12] = 0x7fU; // impossible top-level box length.
    CHECK(!hamdrm::validateHamDrmImage(
               metadata, malformedJp2.data(), malformedJp2.size()).ok());
}

} // namespace

int main()
{
    testCrcAndProfiles();
    testDataGroupCodec();
    testMotHeaderAndObjectAssembly();
    testBsr();
    testImageValidation();
    if (failures != 0) {
        std::cerr << failures << " HAMDRM core checks failed\n";
        return 1;
    }
    std::cout << "HAMDRM core checks passed\n";
    return 0;
}
