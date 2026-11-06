// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/core/SstvFskIdCodec.h"
#include "src/sstv/core/SstvNarrowVisCodec.h"
#include "src/sstv/core/SstvVisCodec.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::size_t kMaximumFuzzInputBytes = 4U * 1024U;

[[noreturn]] void invariantFailure()
{
    std::abort();
}

SstvVisSymbol visSymbol(std::uint8_t value) noexcept
{
    switch (value & 0x03U) {
    case 0U:
        return SstvVisSymbol::Zero;
    case 1U:
        return SstvVisSymbol::One;
    case 2U:
        return SstvVisSymbol::Separator;
    default:
        return SstvVisSymbol::Invalid;
    }
}

double narrowFrequency(std::uint8_t value) noexcept
{
    switch (value % 8U) {
    case 0U:
        return SstvNarrowVisCodec::OneFrequencyHz;
    case 1U:
        return SstvNarrowVisCodec::ZeroFrequencyHz;
    case 2U:
        return 1'200.0;
    case 3U:
        return 2'000.0;
    case 4U:
        return 0.0;
    case 5U:
        return -1.0;
    case 6U:
        return std::numeric_limits<double>::infinity();
    default:
        return std::numeric_limits<double>::quiet_NaN();
    }
}

void exerciseVis(const std::uint8_t* data, std::size_t size)
{
    std::vector<SstvVisObservation> observations;
    observations.reserve(size);
    for (std::size_t index = 0U; index < size; ++index) {
        SstvVisObservation observation;
        observation.symbol = visSymbol(data[index]);
        observation.confidence = static_cast<double>(data[index]) / 255.0;
        observations.push_back(observation);
    }

    const SstvVisDecodeResult direct = SstvVisCodec::decodeFrame(observations);
    if (direct.symbolsConsumed > observations.size()
        || direct.observedRawBitCount > observations.size()
        || (direct.valid && (!direct.complete || !direct.startValid
                             || !direct.stopValid))) {
        invariantFailure();
    }

    SstvVisStreamDecoder stream;
    const std::vector<SstvVisDecodeResult> frames = stream.consume(observations);
    for (const SstvVisDecodeResult& frame : frames) {
        if (frame.symbolsConsumed > observations.size()
            || (frame.valid && (!frame.complete || !frame.startValid
                                || !frame.stopValid))) {
            invariantFailure();
        }
    }
    const auto tail = stream.finish();
    if (tail && tail->valid && !tail->complete) {
        invariantFailure();
    }
}

void exerciseNarrowVis(const std::uint8_t* data, std::size_t size)
{
    std::array<double, SstvNarrowVisCodec::ToneCount> frequencies {};
    for (std::size_t index = 0U; index < frequencies.size(); ++index) {
        frequencies[index] = index < size
            ? narrowFrequency(data[index])
            : 2'000.0;
    }

    const std::size_t suppliedCount = size == 0U
        ? 0U : static_cast<std::size_t>(data[0])
              % (SstvNarrowVisCodec::ToneCount + 1U);
    const double tolerance = size > 1U
        ? static_cast<double>(data[1]) : 90.0;
    try {
        const SstvNarrowVisDecodeResult decoded = SstvNarrowVisCodec::decode(
            frequencies.data(), suppliedCount, tolerance);
        if (decoded.valid
            && (!decoded.mode.has_value()
                || decoded.error != SstvNarrowVisError::None
                || !std::isfinite(decoded.confidence))) {
            invariantFailure();
        }
    } catch (const std::invalid_argument&) {
        // Invalid count/tolerance is the documented fail-closed API result.
    }

    const SstvNarrowVisDecodeResult complete = SstvNarrowVisCodec::decode(
        frequencies, 90.0);
    if (complete.valid
        && (!complete.mode.has_value()
            || complete.error != SstvNarrowVisError::None
            || !std::isfinite(complete.confidence))) {
        invariantFailure();
    }
}

void exerciseFskId(const std::uint8_t* data, std::size_t size)
{
    std::vector<SstvFskIdCodec::Symbol> symbols;
    std::vector<SstvFskIdCodec::Bit> bits;
    symbols.reserve(size);
    bits.reserve(size);
    for (std::size_t index = 0U; index < size; ++index) {
        symbols.push_back(data[index]);
        bits.push_back(data[index]);
    }

    SstvFskIdCodec::DecodeOptions options;
    if (size > 0U) {
        options.textPolicy = (data[0] & 0x01U) != 0U
            ? SstvFskIdCodec::TextPolicy::Callsign
            : SstvFskIdCodec::TextPolicy::PermittedText;
        options.allowHeaderWithoutStartSymbol = (data[0] & 0x02U) != 0U;
        options.allowLeadingSymbols = (data[0] & 0x04U) != 0U;
        options.allowTrailingSymbols = (data[0] & 0x08U) != 0U;
        options.detectorConfidence = static_cast<double>(data[0]) / 255.0;
    }

    const auto symbolResult = SstvFskIdCodec::decodeSymbols(symbols, options);
    if (symbolResult.rawSymbols != symbols
        || (symbolResult.valid()
            && (!symbolResult.checksumPresent
                || !symbolResult.checksumValid
                || !symbolResult.symbolsValid
                || !symbolResult.charactersValid
                || !symbolResult.lengthValid))) {
        invariantFailure();
    }

    const auto bitResult = SstvFskIdCodec::decodeBits(bits, options);
    if (bitResult.rawBits != bits
        || (bitResult.valid()
            && (!bitResult.checksumPresent
                || !bitResult.checksumValid
                || !bitResult.bitsValid
                || !bitResult.charactersValid
                || !bitResult.lengthValid))) {
        invariantFailure();
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size)
{
    if ((!data && size != 0U) || size > kMaximumFuzzInputBytes) {
        return 0;
    }
    static constexpr std::uint8_t empty = 0U;
    const std::uint8_t* const bytes = data ? data : &empty;
    exerciseVis(bytes, size);
    exerciseNarrowVis(bytes, size);
    exerciseFskId(bytes, size);
    return 0;
}
