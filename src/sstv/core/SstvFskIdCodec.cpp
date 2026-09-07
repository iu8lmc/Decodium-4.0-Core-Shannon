// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvFskIdCodec.h"

#include <algorithm>
#include <cmath>

namespace decodium::sstv {
namespace {

using Codec = SstvFskIdCodec;

constexpr bool isAsciiLower(char character) noexcept
{
    const auto value = static_cast<unsigned char>(character);
    return value >= static_cast<unsigned char>('a')
        && value <= static_cast<unsigned char>('z');
}

constexpr char toAsciiUpper(char character) noexcept
{
    return isAsciiLower(character)
        ? static_cast<char>(static_cast<unsigned char>(character)
                            - static_cast<unsigned char>('a')
                            + static_cast<unsigned char>('A'))
        : character;
}

void assignConfidence(Codec::DecodeResult& result,
                      const std::optional<double>& suppliedConfidence) noexcept
{
    if (!suppliedConfidence.has_value()) {
        result.confidenceStatus = Codec::ConfidenceStatus::NotProvided;
        return;
    }

    if (!std::isfinite(*suppliedConfidence)
        || *suppliedConfidence < 0.0
        || *suppliedConfidence > 1.0) {
        result.confidenceStatus = Codec::ConfidenceStatus::Invalid;
        return;
    }

    result.confidenceStatus = Codec::ConfidenceStatus::Provided;
    result.confidence = *suppliedConfidence;
}

std::size_t findFullHeader(const std::vector<Codec::Symbol>& symbols) noexcept
{
    if (symbols.size() < 2) {
        return Codec::kNoIndex;
    }

    for (std::size_t index = 0; index + 1 < symbols.size(); ++index) {
        if (symbols[index] == Codec::kStartSymbol
            && symbols[index + 1] == Codec::kHeaderSymbol) {
            return index;
        }
    }
    return Codec::kNoIndex;
}

std::size_t findHeaderOnly(const std::vector<Codec::Symbol>& symbols) noexcept
{
    const auto it = std::find(symbols.cbegin(), symbols.cend(), Codec::kHeaderSymbol);
    if (it == symbols.cend()) {
        return Codec::kNoIndex;
    }
    return static_cast<std::size_t>(std::distance(symbols.cbegin(), it));
}

Codec::DecodeStatus finalStatus(const Codec::DecodeResult& result,
                                const Codec::DecodeOptions& options) noexcept
{
    if (result.rawSymbols.empty()) {
        return Codec::DecodeStatus::EmptyInput;
    }
    if (!result.symbolsValid) {
        return Codec::DecodeStatus::InvalidSymbol;
    }
    if (!result.headerPresent) {
        return Codec::DecodeStatus::MissingHeader;
    }
    if (!result.startSymbolPresent && !options.allowHeaderWithoutStartSymbol) {
        return Codec::DecodeStatus::MissingStartSymbol;
    }
    if (!result.terminatorPresent) {
        return Codec::DecodeStatus::MissingTerminator;
    }
    if (!result.checksumPresent) {
        return Codec::DecodeStatus::MissingChecksum;
    }
    if (!result.lengthValid) {
        return Codec::DecodeStatus::TooLong;
    }
    if (!result.charactersValid) {
        return Codec::DecodeStatus::InvalidCharacter;
    }
    if (!result.checksumValid) {
        return Codec::DecodeStatus::ChecksumMismatch;
    }
    if (result.leadingSymbolCount != 0 && !options.allowLeadingSymbols) {
        return Codec::DecodeStatus::LeadingSymbols;
    }
    if (result.trailingSymbolCount != 0 && !options.allowTrailingSymbols) {
        return Codec::DecodeStatus::TrailingSymbols;
    }
    return Codec::DecodeStatus::Valid;
}

} // namespace

SstvFskIdCodec::ProtocolEvidence SstvFskIdCodec::protocolEvidence() noexcept
{
    return {};
}

bool SstvFskIdCodec::isPermittedCharacter(char character, TextPolicy policy) noexcept
{
    const auto value = static_cast<unsigned char>(character);
    if (policy == TextPolicy::Callsign) {
        return (value >= static_cast<unsigned char>('A')
                && value <= static_cast<unsigned char>('Z'))
            || (value >= static_cast<unsigned char>('0')
                && value <= static_cast<unsigned char>('9'))
            || value == static_cast<unsigned char>('/');
    }

    return value >= 0x2dU && value <= 0x5fU;
}

SstvFskIdCodec::TextValidation
SstvFskIdCodec::validateText(std::string_view text, TextPolicy policy)
{
    TextValidation result;
    result.text.assign(text.begin(), text.end());

    if (text.empty()) {
        result.status = ValidationStatus::Empty;
        return result;
    }

    for (std::size_t index = 0; index < text.size(); ++index) {
        if (!isPermittedCharacter(text[index], policy)) {
            result.rejectedIndices.push_back(index);
        }
        if (index >= kMaximumTextLength) {
            result.truncatedIndices.push_back(index);
        }
    }

    if (text.size() > kMaximumTextLength) {
        result.status = ValidationStatus::TooLong;
    } else if (!result.rejectedIndices.empty()) {
        result.status = ValidationStatus::InvalidCharacter;
    } else {
        result.status = ValidationStatus::Valid;
    }
    return result;
}

SstvFskIdCodec::TextValidation
SstvFskIdCodec::sanitizeText(std::string_view text, TextPolicy policy)
{
    TextValidation result;
    result.text.reserve(std::min(text.size(), kMaximumTextLength));

    for (std::size_t index = 0; index < text.size(); ++index) {
        const char normalized = toAsciiUpper(text[index]);
        if (normalized != text[index]) {
            result.changedIndices.push_back(index);
            result.changed = true;
        }

        if (!isPermittedCharacter(normalized, policy)) {
            result.rejectedIndices.push_back(index);
            result.changed = true;
            continue;
        }

        if (result.text.size() >= kMaximumTextLength) {
            result.truncatedIndices.push_back(index);
            result.changed = true;
            continue;
        }

        result.text.push_back(normalized);
    }

    result.status = result.text.empty() ? ValidationStatus::Empty : ValidationStatus::Valid;
    return result;
}

SstvFskIdCodec::Symbol
SstvFskIdCodec::checksum(const std::vector<Symbol>& payload) noexcept
{
    Symbol value = 0;
    for (const Symbol symbol : payload) {
        value = static_cast<Symbol>(value ^ static_cast<Symbol>(symbol & kMaximumSymbol));
    }
    return static_cast<Symbol>(value & kMaximumSymbol);
}

std::vector<SstvFskIdCodec::Bit>
SstvFskIdCodec::symbolsToBits(const std::vector<Symbol>& symbols)
{
    std::vector<Bit> bits;
    bits.reserve(symbols.size() * kBitsPerSymbol);
    for (const Symbol symbol : symbols) {
        for (unsigned bitIndex = 0; bitIndex < kBitsPerSymbol; ++bitIndex) {
            bits.push_back(static_cast<Bit>((symbol >> bitIndex) & 0x01U));
        }
    }
    return bits;
}

std::vector<SstvFskIdCodec::Symbol>
SstvFskIdCodec::bitsToSymbols(const std::vector<Bit>& bits)
{
    std::vector<Symbol> symbols;
    symbols.reserve(bits.size() / kBitsPerSymbol);

    for (std::size_t offset = 0; offset + kBitsPerSymbol <= bits.size();
         offset += kBitsPerSymbol) {
        Symbol symbol = 0;
        bool valid = true;
        for (unsigned bitIndex = 0; bitIndex < kBitsPerSymbol; ++bitIndex) {
            const Bit bit = bits[offset + bitIndex];
            if (bit > 1U) {
                valid = false;
            } else {
                symbol = static_cast<Symbol>(symbol | static_cast<Symbol>(bit << bitIndex));
            }
        }
        symbols.push_back(valid ? symbol : static_cast<Symbol>(0xffU));
    }
    return symbols;
}

SstvFskIdCodec::EncodedFrame
SstvFskIdCodec::encode(std::string_view text, TextPolicy policy, InputHandling handling)
{
    EncodedFrame frame;
    frame.validation = handling == InputHandling::Sanitize
        ? sanitizeText(text, policy)
        : validateText(text, policy);
    if (!frame.validation.valid()) {
        return frame;
    }

    std::vector<Symbol> payload;
    payload.reserve(frame.validation.text.size());
    for (const char character : frame.validation.text) {
        payload.push_back(static_cast<Symbol>(static_cast<unsigned char>(character) - 0x20U));
    }

    frame.symbols.reserve(payload.size() + 4U);
    frame.symbols.push_back(kStartSymbol);
    frame.symbols.push_back(kHeaderSymbol);
    frame.symbols.insert(frame.symbols.end(), payload.cbegin(), payload.cend());
    frame.symbols.push_back(kEndSymbol);
    frame.symbols.push_back(checksum(payload));
    frame.bits = symbolsToBits(frame.symbols);

    // QSSTV's primary TX path emits the logical 0x20 sync marker as 100 ms of
    // zero tone followed by one 22 ms mark, rather than six uniform bit cells.
    // Keeping that envelope separate avoids silently changing it to 110 ms.
    frame.tones.reserve(4U + (frame.symbols.size() - 1U) * kBitsPerSymbol);
    frame.tones.push_back({kLeaderFrequencyHz,
                           kLeaderDurationMicroseconds,
                           ToneRole::Leader,
                           kNoIndex,
                           kNoIndex});
    frame.tones.push_back({kZeroFrequencyHz,
                           kSpacePreambleDurationMicroseconds,
                           ToneRole::SpacePreamble,
                           kNoIndex,
                           kNoIndex});
    frame.tones.push_back({kOneFrequencyHz,
                           kBitDurationMicroseconds,
                           ToneRole::StartBit,
                           0,
                           kBitsPerSymbol - 1U});

    for (std::size_t symbolIndex = 1; symbolIndex < frame.symbols.size(); ++symbolIndex) {
        const Symbol symbol = frame.symbols[symbolIndex];
        for (unsigned bitIndex = 0; bitIndex < kBitsPerSymbol; ++bitIndex) {
            const Bit bit = static_cast<Bit>((symbol >> bitIndex) & 0x01U);
            frame.tones.push_back({bit == 1U ? kOneFrequencyHz : kZeroFrequencyHz,
                                   kBitDurationMicroseconds,
                                   ToneRole::FramedBit,
                                   symbolIndex,
                                   bitIndex});
        }
    }

    frame.tones.push_back({kOneFrequencyHz,
                           kTrailerDurationMicroseconds,
                           ToneRole::Trailer,
                           kNoIndex,
                           kNoIndex});
    return frame;
}

SstvFskIdCodec::DecodeResult
SstvFskIdCodec::decodeSymbols(const std::vector<Symbol>& symbols)
{
    return decodeSymbols(symbols, DecodeOptions{});
}

SstvFskIdCodec::DecodeResult
SstvFskIdCodec::decodeSymbols(const std::vector<Symbol>& symbols,
                              const DecodeOptions& options)
{
    DecodeResult result;
    result.textPolicy = options.textPolicy;
    result.rawSymbols = symbols;
    assignConfidence(result, options.detectorConfidence);

    for (std::size_t index = 0; index < symbols.size(); ++index) {
        if (symbols[index] > kMaximumSymbol) {
            result.invalidSymbolIndices.push_back(index);
        }
    }
    result.symbolsValid = result.invalidSymbolIndices.empty();

    if (symbols.empty()) {
        result.status = DecodeStatus::EmptyInput;
        return result;
    }

    const std::size_t fullHeaderIndex = findFullHeader(symbols);
    const std::size_t headerOnlyIndex = findHeaderOnly(symbols);
    std::size_t headerIndex = kNoIndex;
    std::size_t payloadStart = kNoIndex;

    if (fullHeaderIndex != kNoIndex) {
        result.frameStartIndex = fullHeaderIndex;
        result.startSymbolPresent = true;
        result.headerPresent = true;
        headerIndex = fullHeaderIndex + 1U;
        payloadStart = headerIndex + 1U;
    } else if (headerOnlyIndex != kNoIndex) {
        result.frameStartIndex = headerOnlyIndex;
        result.headerPresent = true;
        headerIndex = headerOnlyIndex;
        payloadStart = headerIndex + 1U;
    }

    if (!result.headerPresent) {
        result.status = finalStatus(result, options);
        return result;
    }

    result.leadingSymbolCount = result.frameStartIndex;

    std::size_t terminatorIndex = kNoIndex;
    for (std::size_t index = payloadStart; index < symbols.size(); ++index) {
        if (symbols[index] == kEndSymbol) {
            terminatorIndex = index;
            break;
        }

        const Symbol symbol = symbols[index];
        result.payloadSymbols.push_back(symbol);
        const std::size_t characterIndex = result.text.size();
        if (symbol > kMaximumSymbol) {
            result.text.push_back('?');
            result.invalidCharacterIndices.push_back(characterIndex);
            result.charactersValid = false;
            continue;
        }

        const char character = static_cast<char>(symbol + 0x20U);
        result.text.push_back(character);
        if (!isPermittedCharacter(character, options.textPolicy)) {
            result.invalidCharacterIndices.push_back(characterIndex);
            result.charactersValid = false;
        }
    }

    result.lengthValid = result.payloadSymbols.size() <= kMaximumTextLength;

    if (terminatorIndex == kNoIndex) {
        result.frameSymbols.assign(symbols.cbegin() + static_cast<std::ptrdiff_t>(result.frameStartIndex),
                                   symbols.cend());
        result.status = finalStatus(result, options);
        return result;
    }

    result.terminatorPresent = true;
    result.frameEndIndex = terminatorIndex;
    if (result.symbolsValid) {
        result.computedChecksum = checksum(result.payloadSymbols);
    }

    const std::size_t checksumIndex = terminatorIndex + 1U;
    if (checksumIndex >= symbols.size()) {
        result.frameSymbols.assign(symbols.cbegin() + static_cast<std::ptrdiff_t>(result.frameStartIndex),
                                   symbols.cbegin() + static_cast<std::ptrdiff_t>(terminatorIndex + 1U));
        result.status = finalStatus(result, options);
        return result;
    }

    result.checksumPresent = true;
    result.receivedChecksum = symbols[checksumIndex];
    result.frameEndIndex = checksumIndex;
    result.frameSymbols.assign(symbols.cbegin() + static_cast<std::ptrdiff_t>(result.frameStartIndex),
                               symbols.cbegin() + static_cast<std::ptrdiff_t>(checksumIndex + 1U));
    result.trailingSymbolCount = symbols.size() - checksumIndex - 1U;

    if (result.computedChecksum.has_value()
        && symbols[checksumIndex] <= kMaximumSymbol) {
        result.checksumValid = *result.computedChecksum == symbols[checksumIndex];
    }

    result.status = finalStatus(result, options);
    return result;
}

SstvFskIdCodec::DecodeResult
SstvFskIdCodec::decodeBits(const std::vector<Bit>& bits)
{
    return decodeBits(bits, DecodeOptions{});
}

SstvFskIdCodec::DecodeResult
SstvFskIdCodec::decodeBits(const std::vector<Bit>& bits,
                           const DecodeOptions& options)
{
    DecodeResult result = decodeSymbols(bitsToSymbols(bits), options);
    result.rawBits = bits;
    result.trailingBitCount = bits.size() % kBitsPerSymbol;

    for (std::size_t index = 0; index < bits.size(); ++index) {
        if (bits[index] > 1U) {
            result.invalidBitIndices.push_back(index);
        }
    }

    result.bitsValid = result.invalidBitIndices.empty() && result.trailingBitCount == 0;
    if (!result.invalidBitIndices.empty()) {
        result.status = DecodeStatus::InvalidBit;
    } else if (result.trailingBitCount != 0) {
        result.status = DecodeStatus::TruncatedBits;
    }
    return result;
}

} // namespace decodium::sstv
