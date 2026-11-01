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

void testIndependentQsstvMotGroups()
{
    // Generated by the separately compiled QSSTV 8c27d6d CMOTDABEnc using
    // sample.jpg, bytes 12 34 56 78 and 64 available bytes per group.
    const std::vector<std::uint8_t> header {
        0x73,0x00,0x80,0x00,0x12,0xee,0x55,0x00,0x1b,0x00,
        0x00,0x00,0x40,0x0d,0x84,0x01,0x85,0x00,0x00,0x00,
        0x00,0x46,0x00,0xcc,0x0b,0x00,0x73,0x61,0x6d,0x70,
        0x6c,0x65,0x2e,0x6a,0x70,0x67,0x75,0xb5};
    const std::vector<std::uint8_t> body {
        0x74,0x00,0x80,0x00,0x12,0xee,0x55,0x00,0x04,
        0x12,0x34,0x56,0x78,0xef,0x53};
    const auto headerGroup = hamdrm::parseHamDrmMotDataGroup(header.data(), header.size());
    const auto bodyGroup = hamdrm::parseHamDrmMotDataGroup(body.data(), body.size());
    CHECK(headerGroup.ok());
    CHECK(bodyGroup.ok());
    if (!headerGroup.ok() || !bodyGroup.ok()) return;
    CHECK(headerGroup.value->transportId == 0xee55U);
    CHECK(hamdrm::qsstvCompatibleTransportId("sample.jpg") == 0xee55U);
    const auto decodedHeader = hamdrm::parseHamDrmMotHeader(
        headerGroup.value->payload.data(), headerGroup.value->payload.size(), 0xee55U);
    CHECK(decodedHeader.ok());
    CHECK(decodedHeader.ok() && decodedHeader.value->filename == "sample.jpg");
    CHECK(decodedHeader.ok() && decodedHeader.value->bodySize == 4U);
    const auto reencodedHeader = hamdrm::encodeHamDrmMotDataGroup(*headerGroup.value);
    const auto reencodedBody = hamdrm::encodeHamDrmMotDataGroup(*bodyGroup.value);
    CHECK(reencodedHeader.ok() && *reencodedHeader.value == header);
    CHECK(reencodedBody.ok() && *reencodedBody.value == body);
    auto oldReflectedCrc = body;
    oldReflectedCrc[oldReflectedCrc.size() - 2U] = 0x43U;
    oldReflectedCrc.back() = 0x69U;
    const auto rejectedLegacy = hamdrm::parseHamDrmMotDataGroup(
        oldReflectedCrc.data(), oldReflectedCrc.size());
    CHECK(!rejectedLegacy.ok());
    CHECK(rejectedLegacy.status.code == hamdrm::HamDrmErrorCode::CrcMismatch);
    hamdrm::HamDrmMotObjectMetadata outgoing;
    outgoing.filename = "sample.jpg";
    const auto encoded = hamdrm::encodeHamDrmObject(outgoing, {0x12,0x34,0x56,0x78}, 50U);
    CHECK(encoded.ok());
    CHECK(encoded.ok() && encoded.value->headerGroups.size() == 1U
          && encoded.value->headerGroups[0] == header);
    CHECK(encoded.ok() && encoded.value->bodyGroups.size() == 1U
          && encoded.value->bodyGroups[0] == body);
    hamdrm::HamDrmObjectAssembler assembler(0xee55U);
    CHECK(assembler.ingest(header.data(), header.size()).ok());
    CHECK(assembler.ingest(body.data(), body.size()).ok());
    const auto object = assembler.assembledObject();
    CHECK(object.ok());
    CHECK(object.ok() && object.value->originalBytes
          == std::vector<std::uint8_t>({0x12,0x34,0x56,0x78}));
}

void testIndependentQsstvSegmentedObject()
{
    // QSSTV's run-in sends the last segment before segments 0 and 1; its
    // continuity index increments per transmission, including repetitions.
    const std::vector<std::string> hexGroups {
        "7300800012ee55001b000006700d840185000000004600cc0b0073616d706c652e6a706738d9",
        "7400800212ee5500036465665f15",
        "7410000012ee550032000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f30316ca4",
        "7420000112ee55003232333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f60616263fdc8",
        "7430800212ee550003646566a052"};
    auto fromHex = [](const std::string& text) {
        std::vector<std::uint8_t> bytes;
        for (std::size_t i = 0; i + 1U < text.size(); i += 2U) {
            bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i, 2U), nullptr, 16)));
        }
        return bytes;
    };
    std::vector<std::uint8_t> expected(103U);
    for (std::size_t i = 0; i < expected.size(); ++i) expected[i] = static_cast<std::uint8_t>(i);
    hamdrm::HamDrmMotObjectMetadata metadata;
    metadata.filename = "sample.jpg";
    const auto local = hamdrm::encodeHamDrmObject(metadata, expected, 50U);
    CHECK(local.ok());
    if (!local.ok()) return;
    CHECK(local.value->bodyGroups.size() == 3U);
    hamdrm::HamDrmObjectAssembler receiver(0xee55U);
    for (std::size_t i = 0; i < hexGroups.size(); ++i) {
        const auto raw = fromHex(hexGroups[i]);
        const auto group = hamdrm::parseHamDrmMotDataGroup(raw.data(), raw.size());
        CHECK(group.ok());
        if (!group.ok()) continue;
        const auto reencoded = hamdrm::encodeHamDrmMotDataGroup(*group.value);
        CHECK(reencoded.ok() && *reencoded.value == raw);
        CHECK(receiver.ingest(raw.data(), raw.size()).ok());
        if (i == 0U) CHECK(raw == local.value->headerGroups[0]);
        if (i > 0U && group.value->segmentNumber < local.value->bodyGroups.size()) {
            const auto& ownRaw = local.value->bodyGroups[group.value->segmentNumber];
            const auto own = hamdrm::parseHamDrmMotDataGroup(ownRaw.data(), ownRaw.size());
            CHECK(own.ok() && own.value->payload == group.value->payload);
            CHECK(own.ok() && own.value->lastSegment == group.value->lastSegment);
        }
    }
    const auto assembled = receiver.assembledObject();
    CHECK(assembled.ok());
    CHECK(assembled.ok() && assembled.value->originalBytes == expected);
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
    testIndependentQsstvMotGroups();
    testIndependentQsstvSegmentedObject();
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
