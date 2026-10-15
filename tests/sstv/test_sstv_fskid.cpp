// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "src/sstv/core/SstvFskIdCodec.h"

#include <limits>
#include <string>
#include <vector>

using decodium::sstv::SstvFskIdCodec;

class TestSstvFskId final : public QObject
{
    Q_OBJECT

private slots:
    void exposesProtocolEvidenceWithoutOverclaiming();
    void validatesAndSanitizesTxText();
    void roundTripsTheRepresentableAlphabet();
    void emitsTheAuditedPreambleAndLsbFirstBits();
    void validatesGoodAndBadChecksums();
    void rejectsTruncatedFramesAndBits();
    void preservesInvalidCharacterAndSymbolDiagnostics();
    void enforcesTheConservativeMaximumLength();
    void preservesRawDiagnosticsAndIndependentConfidence();
    void framingIsDeterministic();
};

void TestSstvFskId::exposesProtocolEvidenceWithoutOverclaiming()
{
    using Evidence = SstvFskIdCodec::EvidenceStatus;

    const auto evidence = SstvFskIdCodec::protocolEvidence();
    QCOMPARE(evidence.symbolEncoding, Evidence::IndependentlyCorroborated);
    QCOMPARE(evidence.logicalFraming, Evidence::IndependentlyCorroborated);
    QCOMPARE(evidence.checksum, Evidence::SingleAuditedImplementation);
    QCOMPARE(evidence.toneEnvelope, Evidence::SingleAuditedImplementation);
    QCOMPARE(evidence.permittedTextAlphabet,
             Evidence::ConflictingAuditedImplementations);
    QCOMPARE(evidence.maximumTextLength, Evidence::ConflictingAuditedImplementations);
    QCOMPARE(evidence.selectedMaximumTextLength, std::size_t{9});

    QCOMPARE(SstvFskIdCodec::kBitsPerSymbol, 6U);
    QCOMPARE(SstvFskIdCodec::kBitDurationMicroseconds, std::uint32_t{22000});
    QCOMPARE(SstvFskIdCodec::kOneFrequencyHz, std::uint16_t{1900});
    QCOMPARE(SstvFskIdCodec::kZeroFrequencyHz, std::uint16_t{2100});
}

void TestSstvFskId::validatesAndSanitizesTxText()
{
    using Policy = SstvFskIdCodec::TextPolicy;
    using Status = SstvFskIdCodec::ValidationStatus;

    const auto canonical = SstvFskIdCodec::validateText("IU8LMC/P", Policy::Callsign);
    QVERIFY(canonical.valid());
    QCOMPARE(canonical.text, std::string("IU8LMC/P"));
    QVERIFY(!canonical.changed);

    const auto lowerCase = SstvFskIdCodec::validateText("iu8lmc/p", Policy::Callsign);
    QCOMPARE(lowerCase.status, Status::InvalidCharacter);
    QCOMPARE(lowerCase.rejectedIndices.size(), std::size_t{6});

    const auto sanitized = SstvFskIdCodec::sanitizeText(" iu8lmc-/p!", Policy::Callsign);
    QVERIFY(sanitized.valid());
    QCOMPARE(sanitized.text, std::string("IU8LMC/P"));
    QVERIFY(sanitized.changed);
    QCOMPARE(sanitized.changedIndices.size(), std::size_t{6});
    QCOMPARE(sanitized.rejectedIndices.size(), std::size_t{3});

    const auto custom = SstvFskIdCodec::sanitizeText("cq dx!", Policy::PermittedText);
    QVERIFY(custom.valid());
    QCOMPARE(custom.text, std::string("CQDX"));
    QCOMPARE(custom.rejectedIndices, std::vector<std::size_t>({2, 5}));

    QCOMPARE(SstvFskIdCodec::validateText("", Policy::PermittedText).status,
             Status::Empty);
    QCOMPARE(SstvFskIdCodec::validateText("!", Policy::PermittedText).status,
             Status::InvalidCharacter);
}

void TestSstvFskId::roundTripsTheRepresentableAlphabet()
{
    using Handling = SstvFskIdCodec::InputHandling;
    using Policy = SstvFskIdCodec::TextPolicy;

    std::size_t testedCharacters = 0;
    for (unsigned value = 0x20; value <= 0x5f; ++value) {
        const char character = static_cast<char>(value);
        if (!SstvFskIdCodec::isPermittedCharacter(character, Policy::PermittedText)) {
            continue;
        }

        const std::string text(1, character);
        const auto encoded = SstvFskIdCodec::encode(text,
                                                    Policy::PermittedText,
                                                    Handling::Strict);
        QVERIFY2(encoded.valid(), text.c_str());
        const auto decoded = SstvFskIdCodec::decodeSymbols(encoded.symbols);
        QVERIFY2(decoded.valid(), text.c_str());
        QCOMPARE(decoded.text, text);
        QCOMPARE(decoded.rawSymbols, encoded.symbols);
        ++testedCharacters;
    }

    // SlowRX accepts symbols 0x0d..0x3f, i.e. ASCII '-' through '_'.
    QCOMPARE(testedCharacters, std::size_t{51});
}

void TestSstvFskId::emitsTheAuditedPreambleAndLsbFirstBits()
{
    using Role = SstvFskIdCodec::ToneRole;

    const auto frame = SstvFskIdCodec::encode("A");
    QVERIFY(frame.valid());
    QCOMPARE(frame.symbols.front(), SstvFskIdCodec::kStartSymbol);
    QCOMPARE(frame.symbols[1], SstvFskIdCodec::kHeaderSymbol);

    const std::vector<SstvFskIdCodec::Bit> expectedPrefixBits{
        0, 0, 0, 0, 0, 1, // logical 0x20 search marker
        0, 1, 0, 1, 0, 1  // 0x2a header, LSB first
    };
    QVERIFY(frame.bits.size() >= expectedPrefixBits.size());
    QVERIFY(std::equal(expectedPrefixBits.cbegin(), expectedPrefixBits.cend(),
                       frame.bits.cbegin()));

    QCOMPARE(frame.tones.size(),
             std::size_t{4} + (frame.symbols.size() - 1U)
                 * SstvFskIdCodec::kBitsPerSymbol);
    QCOMPARE(frame.tones[0].frequencyHz, SstvFskIdCodec::kLeaderFrequencyHz);
    QCOMPARE(frame.tones[0].durationMicroseconds,
             SstvFskIdCodec::kLeaderDurationMicroseconds);
    QCOMPARE(frame.tones[0].role, Role::Leader);
    QCOMPARE(frame.tones[1].frequencyHz, SstvFskIdCodec::kZeroFrequencyHz);
    QCOMPARE(frame.tones[1].durationMicroseconds,
             SstvFskIdCodec::kSpacePreambleDurationMicroseconds);
    QCOMPARE(frame.tones[1].role, Role::SpacePreamble);
    QCOMPARE(frame.tones[2].frequencyHz, SstvFskIdCodec::kOneFrequencyHz);
    QCOMPARE(frame.tones[2].durationMicroseconds,
             SstvFskIdCodec::kBitDurationMicroseconds);
    QCOMPARE(frame.tones[2].role, Role::StartBit);

    // First body segment is bit zero of 0x2a, hence the 2100 Hz zero tone.
    QCOMPARE(frame.tones[3].frequencyHz, SstvFskIdCodec::kZeroFrequencyHz);
    QCOMPARE(frame.tones[3].symbolIndex, std::size_t{1});
    QCOMPARE(frame.tones[3].bitIndex, std::size_t{0});
    QCOMPARE(frame.tones.back().frequencyHz, SstvFskIdCodec::kOneFrequencyHz);
    QCOMPARE(frame.tones.back().durationMicroseconds,
             SstvFskIdCodec::kTrailerDurationMicroseconds);
    QCOMPARE(frame.tones.back().role, Role::Trailer);
}

void TestSstvFskId::validatesGoodAndBadChecksums()
{
    using Status = SstvFskIdCodec::DecodeStatus;

    const auto encoded = SstvFskIdCodec::encode("IU8LMC");
    QVERIFY(encoded.valid());
    QCOMPARE(encoded.symbols.back(), SstvFskIdCodec::Symbol{0x26});

    const auto good = SstvFskIdCodec::decodeSymbols(encoded.symbols);
    QVERIFY(good.valid());
    QVERIFY(good.checksumPresent);
    QVERIFY(good.checksumValid);
    QCOMPARE(good.computedChecksum, std::optional<SstvFskIdCodec::Symbol>{0x26});
    QCOMPARE(good.receivedChecksum, std::optional<SstvFskIdCodec::Symbol>{0x26});

    auto corrupted = encoded.symbols;
    corrupted.back() ^= 0x01U;
    const auto bad = SstvFskIdCodec::decodeSymbols(corrupted);
    QCOMPARE(bad.status, Status::ChecksumMismatch);
    QVERIFY(!bad.valid());
    QVERIFY(!bad.checksumValid);
    QCOMPARE(bad.text, std::string("IU8LMC"));
    QCOMPARE(bad.rawSymbols, corrupted);
}

void TestSstvFskId::rejectsTruncatedFramesAndBits()
{
    using Status = SstvFskIdCodec::DecodeStatus;

    const auto encoded = SstvFskIdCodec::encode("TEST");
    QVERIFY(encoded.valid());

    auto noChecksum = encoded.symbols;
    noChecksum.pop_back();
    const auto missingChecksum = SstvFskIdCodec::decodeSymbols(noChecksum);
    QCOMPARE(missingChecksum.status, Status::MissingChecksum);
    QVERIFY(missingChecksum.terminatorPresent);
    QVERIFY(!missingChecksum.checksumPresent);

    auto noTerminator = noChecksum;
    noTerminator.pop_back();
    const auto missingTerminator = SstvFskIdCodec::decodeSymbols(noTerminator);
    QCOMPARE(missingTerminator.status, Status::MissingTerminator);
    QVERIFY(!missingTerminator.terminatorPresent);
    QCOMPARE(missingTerminator.text, std::string("TEST"));

    auto truncatedBits = encoded.bits;
    truncatedBits.pop_back();
    const auto bitResult = SstvFskIdCodec::decodeBits(truncatedBits);
    QCOMPARE(bitResult.status, Status::TruncatedBits);
    QCOMPARE(bitResult.trailingBitCount, std::size_t{5});
    QCOMPARE(bitResult.rawBits, truncatedBits);
    QVERIFY(!bitResult.bitsValid);
}

void TestSstvFskId::preservesInvalidCharacterAndSymbolDiagnostics()
{
    using Handling = SstvFskIdCodec::InputHandling;
    using Policy = SstvFskIdCodec::TextPolicy;
    using Status = SstvFskIdCodec::DecodeStatus;

    const auto strict = SstvFskIdCodec::encode("iu-8", Policy::Callsign,
                                               Handling::Strict);
    QVERIFY(!strict.valid());
    QCOMPARE(strict.validation.status,
             SstvFskIdCodec::ValidationStatus::InvalidCharacter);
    QVERIFY(strict.symbols.empty());

    const auto sanitized = SstvFskIdCodec::encode("iu-8", Policy::Callsign,
                                                  Handling::Sanitize);
    QVERIFY(sanitized.valid());
    QCOMPARE(sanitized.validation.text, std::string("IU8"));

    const auto customDash = SstvFskIdCodec::encode("-", Policy::PermittedText,
                                                   Handling::Strict);
    QVERIFY(customDash.valid());
    SstvFskIdCodec::DecodeOptions callsignOptions;
    callsignOptions.textPolicy = Policy::Callsign;
    const auto invalidCallsign = SstvFskIdCodec::decodeSymbols(customDash.symbols,
                                                               callsignOptions);
    QCOMPARE(invalidCallsign.status, Status::InvalidCharacter);
    QCOMPARE(invalidCallsign.text, std::string("-"));
    QCOMPARE(invalidCallsign.invalidCharacterIndices,
             std::vector<std::size_t>({0}));

    auto invalidSymbols = sanitized.symbols;
    invalidSymbols[2] = 0x80U;
    const auto invalidSymbol = SstvFskIdCodec::decodeSymbols(invalidSymbols,
                                                             callsignOptions);
    QCOMPARE(invalidSymbol.status, Status::InvalidSymbol);
    QCOMPARE(invalidSymbol.rawSymbols, invalidSymbols);
    QCOMPARE(invalidSymbol.invalidSymbolIndices, std::vector<std::size_t>({2}));
    QVERIFY(!invalidSymbol.symbolsValid);

    auto invalidBits = sanitized.bits;
    invalidBits[12] = 2U;
    const auto invalidBit = SstvFskIdCodec::decodeBits(invalidBits,
                                                       callsignOptions);
    QCOMPARE(invalidBit.status, Status::InvalidBit);
    QCOMPARE(invalidBit.invalidBitIndices, std::vector<std::size_t>({12}));
    QCOMPARE(invalidBit.rawBits, invalidBits);
}

void TestSstvFskId::enforcesTheConservativeMaximumLength()
{
    using Handling = SstvFskIdCodec::InputHandling;
    using Policy = SstvFskIdCodec::TextPolicy;
    using Status = SstvFskIdCodec::DecodeStatus;

    const auto maximum = SstvFskIdCodec::encode("123456789");
    QVERIFY(maximum.valid());

    const auto tooLong = SstvFskIdCodec::encode("1234567890");
    QVERIFY(!tooLong.valid());
    QCOMPARE(tooLong.validation.status,
             SstvFskIdCodec::ValidationStatus::TooLong);

    const auto truncated = SstvFskIdCodec::encode("1234567890",
                                                  Policy::Callsign,
                                                  Handling::Sanitize);
    QVERIFY(truncated.valid());
    QCOMPARE(truncated.validation.text, std::string("123456789"));
    QCOMPARE(truncated.validation.truncatedIndices,
             std::vector<std::size_t>({9}));

    std::vector<SstvFskIdCodec::Symbol> oversized{
        SstvFskIdCodec::kStartSymbol,
        SstvFskIdCodec::kHeaderSymbol
    };
    const SstvFskIdCodec::Symbol letterA = 0x21;
    std::vector<SstvFskIdCodec::Symbol> payload(10, letterA);
    oversized.insert(oversized.end(), payload.cbegin(), payload.cend());
    oversized.push_back(SstvFskIdCodec::kEndSymbol);
    oversized.push_back(SstvFskIdCodec::checksum(payload));

    const auto decoded = SstvFskIdCodec::decodeSymbols(oversized);
    QCOMPARE(decoded.status, Status::TooLong);
    QVERIFY(!decoded.lengthValid);
    QCOMPARE(decoded.text, std::string(10, 'A'));
}

void TestSstvFskId::preservesRawDiagnosticsAndIndependentConfidence()
{
    using Confidence = SstvFskIdCodec::ConfidenceStatus;
    using Status = SstvFskIdCodec::DecodeStatus;

    const auto encoded = SstvFskIdCodec::encode("TEST");
    QVERIFY(encoded.valid());

    std::vector<SstvFskIdCodec::Symbol> noisy{0x00};
    noisy.insert(noisy.end(), encoded.symbols.cbegin(), encoded.symbols.cend());
    noisy.push_back(0x00);

    SstvFskIdCodec::DecodeOptions permissive;
    permissive.allowLeadingSymbols = true;
    permissive.allowTrailingSymbols = true;
    permissive.detectorConfidence = 0.03;
    const auto accepted = SstvFskIdCodec::decodeSymbols(noisy, permissive);
    QVERIFY(accepted.valid());
    QCOMPARE(accepted.rawSymbols, noisy);
    QCOMPARE(accepted.frameSymbols, encoded.symbols);
    QCOMPARE(accepted.leadingSymbolCount, std::size_t{1});
    QCOMPARE(accepted.trailingSymbolCount, std::size_t{1});
    QCOMPARE(accepted.confidenceStatus, Confidence::Provided);
    QVERIFY(accepted.confidence.has_value());
    QCOMPARE(*accepted.confidence, 0.03);

    const auto strict = SstvFskIdCodec::decodeSymbols(noisy);
    QCOMPARE(strict.status, Status::LeadingSymbols);
    QVERIFY(strict.checksumValid);

    SstvFskIdCodec::DecodeOptions invalidConfidence;
    invalidConfidence.detectorConfidence = std::numeric_limits<double>::quiet_NaN();
    const auto confidenceMetadata = SstvFskIdCodec::decodeSymbols(encoded.symbols,
                                                                  invalidConfidence);
    // Confidence metadata is reported, but is not a substitute for validity.
    QVERIFY(confidenceMetadata.valid());
    QCOMPARE(confidenceMetadata.confidenceStatus, Confidence::Invalid);
    QVERIFY(!confidenceMetadata.confidence.has_value());
}

void TestSstvFskId::framingIsDeterministic()
{
    const auto first = SstvFskIdCodec::encode("IU8LMC");
    const auto second = SstvFskIdCodec::encode("IU8LMC");
    QVERIFY(first.valid());
    QVERIFY(second.valid());
    QCOMPARE(first.validation.text, second.validation.text);
    QCOMPARE(first.symbols, second.symbols);
    QCOMPARE(first.bits, second.bits);
    QCOMPARE(first.tones, second.tones);

    QCOMPARE(SstvFskIdCodec::bitsToSymbols(first.bits), first.symbols);
    const auto decodedBits = SstvFskIdCodec::decodeBits(first.bits);
    QVERIFY(decodedBits.valid());
    QCOMPARE(decodedBits.text, std::string("IU8LMC"));
    QCOMPARE(decodedBits.rawBits, first.bits);
}

QTEST_APPLESS_MAIN(TestSstvFskId)
#include "test_sstv_fskid.moc"
