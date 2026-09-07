// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace decodium::sstv {

// Deterministic symbol/framing codec for the analogue-SSTV FSK identifier.
//
// This is a clean-room, behaviour-only implementation. The audited behaviour
// is recorded in docs/sstv/UPSTREAM_PROVENANCE.md. In particular, the codec
// does not contain the detector, oscillator, or application state of any
// upstream program. Decodium's native streaming detector supplies classified
// symbols and its native TX stream consumes the returned tone plan.
class SstvFskIdCodec final
{
public:
    using Bit = std::uint8_t;
    using Symbol = std::uint8_t;

    // Six-bit FSK ID can represent ASCII 0x20..0x5f, but audited receivers do
    // not agree on the lower payload range: SlowRX stops below symbol 0x0d.
    // PermittedText therefore uses the interoperable ASCII 0x2d..0x5f subset.
    // Callsign is a deliberately conservative UI/TX policy, not a claim that
    // all national callsign formats can be decided by this codec.
    enum class TextPolicy {
        Callsign,
        PermittedText
    };

    enum class InputHandling {
        Strict,
        Sanitize
    };

    enum class ValidationStatus {
        Valid,
        Empty,
        TooLong,
        InvalidCharacter
    };

    enum class DecodeStatus {
        Valid,
        EmptyInput,
        InvalidBit,
        TruncatedBits,
        InvalidSymbol,
        MissingStartSymbol,
        MissingHeader,
        MissingTerminator,
        MissingChecksum,
        TooLong,
        InvalidCharacter,
        ChecksumMismatch,
        LeadingSymbols,
        TrailingSymbols
    };

    // "SingleAuditedImplementation" and "ConflictingAuditedImplementations"
    // are intentional non-validation states. They prevent a behaviour seen in
    // one implementation from being silently presented as a normative fact.
    enum class EvidenceStatus {
        IndependentlyCorroborated,
        SingleAuditedImplementation,
        ConflictingAuditedImplementations
    };

    enum class ConfidenceStatus {
        NotProvided,
        Provided,
        Invalid
    };

    enum class ToneRole {
        Leader,
        SpacePreamble,
        StartBit,
        FramedBit,
        Trailer
    };

    static constexpr unsigned kBitsPerSymbol = 6;
    static constexpr std::uint32_t kBitDurationMicroseconds = 22000;
    static constexpr std::uint16_t kOneFrequencyHz = 1900;
    static constexpr std::uint16_t kZeroFrequencyHz = 2100;
    static constexpr std::uint16_t kLeaderFrequencyHz = 1500;
    static constexpr std::uint32_t kLeaderDurationMicroseconds = 300000;
    static constexpr std::uint32_t kSpacePreambleDurationMicroseconds = 100000;
    static constexpr std::uint32_t kTrailerDurationMicroseconds = 100000;
    static constexpr Symbol kStartSymbol = 0x20;
    static constexpr Symbol kHeaderSymbol = 0x2a;
    static constexpr Symbol kEndSymbol = 0x01;
    static constexpr Symbol kMaximumSymbol = 0x3f;

    // QSSTV rejects the tenth received character, while SlowRX accepts ten and
    // pySSTV does not enforce a bound. Nine is the conservative interoperable
    // subset; protocolEvidence() exposes that this selection is disputed.
    static constexpr std::size_t kMaximumTextLength = 9;
    static constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

    struct ProtocolEvidence {
        EvidenceStatus symbolEncoding = EvidenceStatus::IndependentlyCorroborated;
        EvidenceStatus logicalFraming = EvidenceStatus::IndependentlyCorroborated;
        EvidenceStatus checksum = EvidenceStatus::SingleAuditedImplementation;
        EvidenceStatus toneEnvelope = EvidenceStatus::SingleAuditedImplementation;
        EvidenceStatus permittedTextAlphabet = EvidenceStatus::ConflictingAuditedImplementations;
        EvidenceStatus maximumTextLength = EvidenceStatus::ConflictingAuditedImplementations;
        std::size_t selectedMaximumTextLength = kMaximumTextLength;
    };

    struct TextValidation {
        ValidationStatus status = ValidationStatus::Empty;
        std::string text;
        std::vector<std::size_t> changedIndices;
        std::vector<std::size_t> rejectedIndices;
        std::vector<std::size_t> truncatedIndices;
        bool changed = false;

        [[nodiscard]] bool valid() const noexcept
        {
            return status == ValidationStatus::Valid;
        }
    };

    struct ToneSegment {
        std::uint16_t frequencyHz = 0;
        std::uint32_t durationMicroseconds = 0;
        ToneRole role = ToneRole::FramedBit;
        std::size_t symbolIndex = kNoIndex;
        std::size_t bitIndex = kNoIndex;

        friend bool operator==(const ToneSegment& lhs, const ToneSegment& rhs) noexcept
        {
            return lhs.frequencyHz == rhs.frequencyHz
                && lhs.durationMicroseconds == rhs.durationMicroseconds
                && lhs.role == rhs.role
                && lhs.symbolIndex == rhs.symbolIndex
                && lhs.bitIndex == rhs.bitIndex;
        }

        friend bool operator!=(const ToneSegment& lhs, const ToneSegment& rhs) noexcept
        {
            return !(lhs == rhs);
        }
    };

    struct EncodedFrame {
        TextValidation validation;

        // Logical receiver framing: 0x20, 0x2a, payload, 0x01, XOR checksum.
        // The 0x20 sync symbol is represented by QSSTV's timed 2100/1900
        // preamble in tones; bits remains the uniform six-bit diagnostic form.
        std::vector<Symbol> symbols;
        std::vector<Bit> bits;
        std::vector<ToneSegment> tones;

        [[nodiscard]] bool valid() const noexcept
        {
            return validation.valid() && !symbols.empty();
        }
    };

    struct DecodeOptions {
        TextPolicy textPolicy = TextPolicy::PermittedText;
        bool allowHeaderWithoutStartSymbol = false;
        bool allowLeadingSymbols = false;
        bool allowTrailingSymbols = false;
        std::optional<double> detectorConfidence;
    };

    struct DecodeResult {
        DecodeStatus status = DecodeStatus::EmptyInput;
        TextPolicy textPolicy = TextPolicy::PermittedText;
        std::string text;

        std::vector<Symbol> rawSymbols;
        std::vector<Symbol> frameSymbols;
        std::vector<Symbol> payloadSymbols;
        std::vector<Bit> rawBits;

        // Indices are relative to rawSymbols/rawBits; character indices are
        // relative to the decoded text. Raw data is never discarded merely
        // because validation fails.
        std::vector<std::size_t> invalidSymbolIndices;
        std::vector<std::size_t> invalidBitIndices;
        std::vector<std::size_t> invalidCharacterIndices;

        std::size_t frameStartIndex = kNoIndex;
        std::size_t frameEndIndex = kNoIndex;
        std::size_t leadingSymbolCount = 0;
        std::size_t trailingSymbolCount = 0;
        std::size_t trailingBitCount = 0;

        bool startSymbolPresent = false;
        bool headerPresent = false;
        bool terminatorPresent = false;
        bool checksumPresent = false;
        bool checksumValid = false;
        bool symbolsValid = true;
        bool bitsValid = true;
        bool charactersValid = true;
        bool lengthValid = true;

        std::optional<Symbol> computedChecksum;
        std::optional<Symbol> receivedChecksum;

        // Confidence is detector metadata. The symbol codec neither invents
        // it nor changes frame validity based on it.
        ConfidenceStatus confidenceStatus = ConfidenceStatus::NotProvided;
        std::optional<double> confidence;

        [[nodiscard]] bool valid() const noexcept
        {
            return status == DecodeStatus::Valid;
        }
    };

    [[nodiscard]] static ProtocolEvidence protocolEvidence() noexcept;

    [[nodiscard]] static bool isPermittedCharacter(char character,
                                                   TextPolicy policy) noexcept;
    [[nodiscard]] static TextValidation validateText(std::string_view text,
                                                     TextPolicy policy);
    [[nodiscard]] static TextValidation sanitizeText(std::string_view text,
                                                     TextPolicy policy);

    [[nodiscard]] static Symbol checksum(const std::vector<Symbol>& payload) noexcept;
    [[nodiscard]] static std::vector<Bit> symbolsToBits(const std::vector<Symbol>& symbols);
    [[nodiscard]] static std::vector<Symbol> bitsToSymbols(const std::vector<Bit>& bits);

    [[nodiscard]] static EncodedFrame encode(std::string_view text,
                                             TextPolicy policy = TextPolicy::Callsign,
                                             InputHandling handling = InputHandling::Strict);

    [[nodiscard]] static DecodeResult decodeSymbols(const std::vector<Symbol>& symbols);
    [[nodiscard]] static DecodeResult decodeSymbols(const std::vector<Symbol>& symbols,
                                                    const DecodeOptions& options);
    [[nodiscard]] static DecodeResult decodeBits(const std::vector<Bit>& bits);
    [[nodiscard]] static DecodeResult decodeBits(const std::vector<Bit>& bits,
                                                 const DecodeOptions& options);
};

} // namespace decodium::sstv
