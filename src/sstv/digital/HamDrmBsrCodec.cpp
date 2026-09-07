// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmBsrCodec.h"

#include "HamDrmMotCodec.h"
#include "HamDrmProfileRegistry.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>

namespace decodium::sstv::hamdrm {
namespace {

constexpr std::size_t kMaximumBsrBytes = 128U * 1024U;

template<typename T>
bool parseUnsigned(std::string_view text, T& output)
{
    if (text.empty() || text.front() == '+' || text.front() == '-') {
        return false;
    }
    unsigned long long value = 0U;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(),
                                        value, 10);
    if (parsed.ec != std::errc {} || parsed.ptr != text.data() + text.size()
        || value > std::numeric_limits<T>::max()) {
        return false;
    }
    output = static_cast<T>(value);
    return true;
}

bool parseSigned(std::string_view text, int& output)
{
    if (text.empty() || text.front() == '+') {
        return false;
    }
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(),
                                        output, 10);
    return parsed.ec == std::errc {}
        && parsed.ptr == text.data() + text.size();
}

std::vector<std::string_view> lines(const std::uint8_t* data, std::size_t size)
{
    std::vector<std::string_view> output;
    const char* begin = reinterpret_cast<const char*>(data);
    std::size_t lineBegin = 0U;
    for (std::size_t index = 0U; index < size; ++index) {
        if (data[index] == '\r') {
            continue;
        }
        if (data[index] == '\n') {
            std::size_t lineEnd = index;
            while (lineEnd > lineBegin && data[lineEnd - 1U] == '\r') {
                --lineEnd;
            }
            output.emplace_back(begin + lineBegin, lineEnd - lineBegin);
            lineBegin = index + 1U;
        }
    }
    if (lineBegin < size) {
        output.emplace_back(begin + lineBegin, size - lineBegin);
    }
    while (!output.empty() && output.back().empty()) {
        output.pop_back();
    }
    return output;
}

HamDrmStatus validateBsr(const HamDrmBsrRequest& request,
                         const HamDrmLimits& limits,
                         bool requireMissing)
{
    if (!request.headerReceived) {
        return HamDrmStatus::failure(
            HamDrmErrorCode::UnsupportedFeature,
            "only HAMDRM BSR requests with a valid MOT header are supported");
    }
    if (request.segmentSize == 0U
        || request.segmentSize > limits.maximumSegmentBytes) {
        return HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                     "BSR segment size is invalid");
    }
    if (requireMissing && request.missingSegments.empty()) {
        return HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                     "BSR must request at least one segment");
    }
    std::uint16_t previous = 0U;
    bool first = true;
    for (const auto segment : request.missingSegments) {
        if (segment >= limits.maximumSegments
            || (!first && segment <= previous)) {
            return HamDrmStatus::failure(
                HamDrmErrorCode::Malformed,
                "BSR missing segments must be unique and increasing");
        }
        first = false;
        previous = segment;
    }
    if (request.filename.has_value()) {
        if (const auto status = validateHamDrmFilename(*request.filename, limits);
            !status.ok()) {
            return status;
        }
    }
    if (request.qsstvCompatibilityCode.has_value()
        && HamDrmProfileRegistry::findByCompatibilityCode(
               *request.qsstvCompatibilityCode) == nullptr) {
        return HamDrmStatus::failure(HamDrmErrorCode::UnsupportedProfile,
                                     "BSR carries an unsupported HAMDRM profile");
    }
    return HamDrmStatus::success();
}

void appendLine(std::vector<std::uint8_t>& output, const std::string& line)
{
    output.insert(output.end(), line.begin(), line.end());
    output.push_back('\n');
}

} // namespace

HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmBsr(
    const HamDrmBsrRequest& request,
    HamDrmBsrDialect dialect,
    const HamDrmLimits& limits)
{
    if (const auto status = validateBsr(request, limits, true); !status.ok()) {
        return {std::nullopt, status};
    }
    if (dialect == HamDrmBsrDialect::QsstvExtended
        && (!request.filename.has_value()
            || !request.qsstvCompatibilityCode.has_value())) {
        return {std::nullopt,
                HamDrmStatus::failure(
                    HamDrmErrorCode::InvalidArgument,
                    "extended BSR requires filename and compatibility profile")};
    }

    std::vector<std::uint8_t> output;
    output.reserve(128U + request.missingSegments.size() * 6U);
    appendLine(output, std::to_string(request.transportId));
    appendLine(output, "H_OK");
    appendLine(output, std::to_string(request.segmentSize));

    const auto& missing = request.missingSegments;
    appendLine(output, std::to_string(missing.front()));
    if (dialect == HamDrmBsrDialect::EasyPalCompatible) {
        bool runOpen = false;
        std::uint16_t previous = missing.front();
        for (std::size_t index = 1U; index < missing.size(); ++index) {
            const std::uint16_t current = missing[index];
            if (static_cast<std::uint32_t>(previous) + 1U == current) {
                runOpen = true;
            } else {
                if (runOpen) {
                    appendLine(output, "-1");
                    runOpen = false;
                }
                appendLine(output, std::to_string(current));
            }
            previous = current;
        }
        if (runOpen) {
            appendLine(output, "-1");
            appendLine(output, std::to_string(missing.back()));
        }
    } else {
        for (std::size_t index = 1U; index < missing.size(); ++index) {
            appendLine(output, std::to_string(missing[index]));
        }
    }
    appendLine(output, "-99");

    if (dialect == HamDrmBsrDialect::QsstvExtended) {
        appendLine(output, *request.filename);
        std::string mode = std::to_string(*request.qsstvCompatibilityCode);
        if (mode.size() < 5U) {
            mode.insert(mode.begin(), 5U - mode.size(), '0');
        }
        appendLine(output, mode);
    }
    if (output.size() > kMaximumBsrBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "encoded BSR exceeds limit")};
    }
    return {std::move(output), HamDrmStatus::success()};
}

HamDrmValueResult<HamDrmBsrRequest> parseHamDrmBsr(
    const std::uint8_t* data,
    std::size_t size,
    const HamDrmLimits& limits)
{
    if (data == nullptr || size == 0U) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      "empty BSR")};
    }
    if (size > kMaximumBsrBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "BSR exceeds parser limit")};
    }
    for (std::size_t index = 0U; index < size; ++index) {
        if (data[index] == 0U || (data[index] < 0x20U && data[index] != '\n'
                                  && data[index] != '\r')) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                          "BSR contains non-text bytes")};
        }
    }
    const auto parsedLines = lines(data, size);
    if (parsedLines.size() < 5U || parsedLines[1] != "H_OK") {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "BSR header is malformed")};
    }

    HamDrmBsrRequest request;
    request.headerReceived = true;
    if (!parseUnsigned(parsedLines[0], request.transportId)
        || !parseUnsigned(parsedLines[2], request.segmentSize)) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "BSR numeric header is invalid")};
    }

    std::size_t index = 3U;
    bool runOpen = false;
    std::uint32_t nextInRun = 0U;
    bool terminatorSeen = false;
    for (; index < parsedLines.size(); ++index) {
        int value = 0;
        if (!parseSigned(parsedLines[index], value)) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                          "BSR segment token is invalid")};
        }
        if (value == -99) {
            if (runOpen || request.missingSegments.empty()) {
                return {std::nullopt,
                        HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                              "unterminated BSR range")};
            }
            terminatorSeen = true;
            ++index;
            break;
        }
        if (value == -1) {
            if (runOpen || request.missingSegments.empty()) {
                return {std::nullopt,
                        HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                              "invalid BSR range marker")};
            }
            runOpen = true;
            nextInRun = static_cast<std::uint32_t>(
                request.missingSegments.back()) + 1U;
            continue;
        }
        if (value < 0 || static_cast<std::size_t>(value) >= limits.maximumSegments) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                          "BSR segment is out of range")};
        }
        const auto segment = static_cast<std::uint16_t>(value);
        if (runOpen) {
            if (segment < nextInRun) {
                return {std::nullopt,
                        HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                              "BSR range endpoint is invalid")};
            }
            for (std::uint32_t expanded = nextInRun; expanded < segment;
                 ++expanded) {
                request.missingSegments.push_back(
                    static_cast<std::uint16_t>(expanded));
            }
            runOpen = false;
        }
        request.missingSegments.push_back(segment);
        if (request.missingSegments.size() > limits.maximumSegments) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                          "expanded BSR exceeds segment limit")};
        }
    }
    if (!terminatorSeen) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "BSR terminator is missing")};
    }
    const std::size_t remaining = parsedLines.size() - index;
    if (remaining != 0U && remaining != 2U) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "BSR extension has invalid arity")};
    }
    if (remaining == 2U) {
        request.filename = std::string(parsedLines[index]);
        std::uint32_t mode = 0U;
        if (!parseUnsigned(parsedLines[index + 1U], mode)) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                          "BSR profile code is invalid")};
        }
        request.qsstvCompatibilityCode = mode;
    }
    if (const auto status = validateBsr(request, limits, true); !status.ok()) {
        return {std::nullopt, status};
    }
    return {std::move(request), HamDrmStatus::success()};
}

} // namespace decodium::sstv::hamdrm
