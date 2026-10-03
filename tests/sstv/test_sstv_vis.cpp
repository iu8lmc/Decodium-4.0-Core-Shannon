// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "src/sstv/core/SstvVisCodec.h"

namespace
{
using decodium::sstv::SstvVisCodec;
using decodium::sstv::SstvVisDecodeResult;
using decodium::sstv::SstvVisError;
using decodium::sstv::SstvVisFormat;
using decodium::sstv::SstvVisObservation;
using decodium::sstv::SstvVisParity;
using decodium::sstv::SstvVisStreamDecoder;
using decodium::sstv::SstvVisSymbol;

bool hasError (SstvVisDecodeResult const& result, SstvVisError error)
{
  return std::find (result.errors.begin (), result.errors.end (), error)
         != result.errors.end ();
}

unsigned bitCount (std::uint8_t value)
{
  unsigned count = 0u;
  while (value != 0u)
    {
      count += value & 1u;
      value >>= 1u;
    }
  return count;
}

SstvVisSymbol flippedBit (SstvVisSymbol symbol)
{
  return symbol == SstvVisSymbol::One ? SstvVisSymbol::Zero
                                      : SstvVisSymbol::One;
}

void addAllPayloadRows ()
{
  QTest::addColumn<int> ("payload");
  for (int payload = 0; payload < 128; ++payload)
    {
      QByteArray const row = QByteArray::number (payload);
      QTest::newRow (row.constData ()) << payload;
    }
}
}

class TestSstvVis final : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void standardRoundTrip_data ()
  {
    addAllPayloadRows ();
  }

  void standardRoundTrip ()
  {
    QFETCH (int, payload);
    auto const frame = SstvVisCodec::encodeStandard (
        static_cast<std::uint8_t> (payload));

    QCOMPARE (static_cast<int> (frame.format),
              static_cast<int> (SstvVisFormat::Standard));
    QCOMPARE (frame.primary.payload, static_cast<std::uint8_t> (payload));
    QVERIFY (frame.primary.payloadKnown);
    QVERIFY (frame.primary.parityKnown);
    QVERIFY (frame.primary.parityValid);
    QVERIFY (frame.primary.rawOctetKnown);
    QVERIFY (!frame.extension.has_value ());

    SstvVisDecodeResult const decoded =
        SstvVisCodec::decodeFrame (frame.symbols);
    QVERIFY (decoded.complete);
    QVERIFY (decoded.valid);
    QVERIFY (decoded.startValid);
    QVERIFY (decoded.stopValid);
    QCOMPARE (decoded.symbolsConsumed, frame.symbols.size ());
    QCOMPARE (decoded.observedRawBitCount,
              SstvVisCodec::StandardRawBitCount);
    QCOMPARE (decoded.primary.payload, static_cast<std::uint8_t> (payload));
    QCOMPARE (static_cast<int> (decoded.primary.parityKind),
              static_cast<int> (SstvVisParity::Even));
    QCOMPARE (decoded.confidence, 1.0);
    QVERIFY (decoded.errors.empty ());
  }

  void evenParityForEveryPayload_data ()
  {
    addAllPayloadRows ();
  }

  void evenParityForEveryPayload ()
  {
    QFETCH (int, payload);
    auto const frame = SstvVisCodec::encodeStandard (
        static_cast<std::uint8_t> (payload));
    bool const expectedOne = (bitCount (static_cast<std::uint8_t> (payload))
                              & 1u) != 0u;
    QCOMPARE (static_cast<int> (frame.primary.parityBit),
              static_cast<int> (expectedOne ? SstvVisSymbol::One
                                            : SstvVisSymbol::Zero));
    QCOMPARE (bitCount (frame.primary.rawOctet) & 1u, 0u);

    std::vector<SstvVisSymbol> damaged = frame.symbols;
    damaged[8] = flippedBit (damaged[8]);
    SstvVisDecodeResult const decoded = SstvVisCodec::decodeFrame (damaged);
    QVERIFY (decoded.complete);
    QVERIFY (!decoded.valid);
    QVERIFY (!decoded.primary.parityValid);
    QVERIFY (hasError (decoded, SstvVisError::ParityMismatch));
  }

  void payloadIsTransmittedLsbFirst ()
  {
    auto const frame = SstvVisCodec::encodeStandard (0x53u);
    std::vector<SstvVisSymbol> const expected {
        SstvVisSymbol::Separator,
        SstvVisSymbol::One,  // bit 0
        SstvVisSymbol::One,  // bit 1
        SstvVisSymbol::Zero, // bit 2
        SstvVisSymbol::Zero, // bit 3
        SstvVisSymbol::One,  // bit 4
        SstvVisSymbol::Zero, // bit 5
        SstvVisSymbol::One,  // bit 6
        SstvVisSymbol::Zero, // even parity
        SstvVisSymbol::Separator
    };
    QVERIFY (frame.symbols == expected);
  }

  void startAndStopAreExplicitSeparators ()
  {
    auto const standard = SstvVisCodec::encodeStandard (0x2cu);
    QCOMPARE (standard.symbols.size (), std::size_t (10u));
    QCOMPARE (static_cast<int> (standard.symbols.front ()),
              static_cast<int> (SstvVisSymbol::Separator));
    QCOMPARE (static_cast<int> (standard.symbols.back ()),
              static_cast<int> (SstvVisSymbol::Separator));

    auto const extended = SstvVisCodec::encodeExtended (0x29u);
    QCOMPARE (extended.symbols.size (), std::size_t (18u));
    QCOMPARE (static_cast<int> (extended.symbols.front ()),
              static_cast<int> (SstvVisSymbol::Separator));
    QCOMPARE (static_cast<int> (extended.symbols.back ()),
              static_cast<int> (SstvVisSymbol::Separator));
  }

  void truncationAtEveryStandardBoundary_data ()
  {
    QTest::addColumn<int> ("retainedSymbols");
    for (int retained = 0; retained < 10; ++retained)
      {
        QByteArray const row = QByteArray::number (retained);
        QTest::newRow (row.constData ()) << retained;
      }
  }

  void truncationAtEveryStandardBoundary ()
  {
    QFETCH (int, retainedSymbols);
    auto const full = SstvVisCodec::encodeStandard (0x2cu).symbols;
    std::vector<SstvVisSymbol> truncated (
        full.begin (), full.begin () + retainedSymbols);
    SstvVisDecodeResult const decoded =
        SstvVisCodec::decodeFrame (truncated);

    QVERIFY (!decoded.complete);
    QVERIFY (!decoded.valid);
    QVERIFY (hasError (decoded, SstvVisError::TruncatedFrame));
    if (retainedSymbols == 0)
      {
        QVERIFY (hasError (decoded, SstvVisError::MissingStart));
      }
    else
      {
        QVERIFY (decoded.startValid);
        QVERIFY (hasError (decoded, SstvVisError::MissingStop));
      }
  }

  void truncationAtEveryExtendedBoundary_data ()
  {
    QTest::addColumn<int> ("retainedSymbols");
    for (int retained = 1; retained < 18; ++retained)
      {
        QByteArray const row = QByteArray::number (retained);
        QTest::newRow (row.constData ()) << retained;
      }
  }

  void truncationAtEveryExtendedBoundary ()
  {
    QFETCH (int, retainedSymbols);
    auto const full = SstvVisCodec::encodeExtended (0x29u).symbols;
    std::vector<SstvVisSymbol> truncated (
        full.begin (), full.begin () + retainedSymbols);
    SstvVisDecodeResult const decoded =
        SstvVisCodec::decodeFrame (truncated);
    QVERIFY (!decoded.complete);
    QVERIFY (!decoded.valid);
    QVERIFY (hasError (decoded, SstvVisError::TruncatedFrame));
    QVERIFY (hasError (decoded, SstvVisError::MissingStop));
    if (retainedSymbols >= 9)
      {
        QCOMPARE (static_cast<int> (decoded.format),
                  static_cast<int> (SstvVisFormat::Extended));
      }
  }

  void invalidSymbolAtEveryRawBit_data ()
  {
    QTest::addColumn<int> ("rawBitOffset");
    for (int offset = 0; offset < 8; ++offset)
      {
        QByteArray const row = QByteArray::number (offset);
        QTest::newRow (row.constData ()) << offset;
      }
  }

  void invalidSymbolAtEveryRawBit ()
  {
    QFETCH (int, rawBitOffset);
    std::vector<SstvVisSymbol> damaged =
        SstvVisCodec::encodeStandard (0x2cu).symbols;
    damaged[static_cast<std::size_t> (rawBitOffset) + 1u] =
        SstvVisSymbol::Invalid;
    SstvVisDecodeResult const decoded = SstvVisCodec::decodeFrame (damaged);
    QVERIFY (decoded.complete);
    QVERIFY (!decoded.valid);
    QVERIFY (hasError (decoded, SstvVisError::InvalidSymbol));
  }

  void missingAndEarlyFramingAreRejected ()
  {
    std::vector<SstvVisSymbol> missingStart =
        SstvVisCodec::encodeStandard (0x2cu).symbols;
    missingStart.erase (missingStart.begin ());
    SstvVisDecodeResult const noStart =
        SstvVisCodec::decodeFrame (missingStart);
    QVERIFY (!noStart.valid);
    QVERIFY (!noStart.startValid);
    QVERIFY (hasError (noStart, SstvVisError::MissingStart));

    std::vector<SstvVisSymbol> missingStop =
        SstvVisCodec::encodeStandard (0x2cu).symbols;
    missingStop.pop_back ();
    SstvVisDecodeResult const noStop =
        SstvVisCodec::decodeFrame (missingStop);
    QVERIFY (!noStop.complete);
    QVERIFY (!noStop.stopValid);
    QVERIFY (hasError (noStop, SstvVisError::MissingStop));

    std::vector<SstvVisSymbol> earlyStop =
        SstvVisCodec::encodeStandard (0x2cu).symbols;
    earlyStop[4] = SstvVisSymbol::Separator;
    SstvVisDecodeResult const early =
        SstvVisCodec::decodeFrame (earlyStop);
    QVERIFY (early.complete);
    QVERIFY (!early.valid);
    QVERIFY (hasError (early, SstvVisError::UnexpectedBitCount));
  }

  void confidenceIsIndependentFromValidity ()
  {
    auto const frame = SstvVisCodec::encodeStandard (0x2cu);
    std::vector<SstvVisObservation> lowConfidence;
    for (SstvVisSymbol const symbol : frame.symbols)
      {
        lowConfidence.push_back ({symbol, 0.25});
      }
    SstvVisDecodeResult const valid =
        SstvVisCodec::decodeFrame (lowConfidence);
    QVERIFY (valid.valid);
    QCOMPARE (valid.confidence, 0.25);

    lowConfidence[8].symbol = flippedBit (lowConfidence[8].symbol);
    for (SstvVisObservation& observation : lowConfidence)
      {
        observation.confidence = 1.0;
      }
    SstvVisDecodeResult const invalid =
        SstvVisCodec::decodeFrame (lowConfidence);
    QVERIFY (!invalid.valid);
    QCOMPARE (invalid.confidence, 1.0);
    QVERIFY (hasError (invalid, SstvVisError::ParityMismatch));
  }

  void backToBackFramesAcrossOneChunk ()
  {
    auto first = SstvVisCodec::encodeStandard (0x2cu).symbols;
    auto const second = SstvVisCodec::encodeStandard (0x3cu).symbols;
    first.insert (first.end (), second.begin (), second.end ());

    SstvVisStreamDecoder stream;
    std::vector<SstvVisDecodeResult> const decoded =
        stream.consumeSymbols (first);
    QCOMPARE (decoded.size (), std::size_t (2u));
    QVERIFY (decoded[0].valid);
    QVERIFY (decoded[1].valid);
    QCOMPARE (decoded[0].primary.payload, std::uint8_t (0x2cu));
    QCOMPARE (decoded[1].primary.payload, std::uint8_t (0x3cu));
    QVERIFY (!stream.hasPartialFrame ());
    QVERIFY (!stream.finish ().has_value ());
  }

  void chunkedStreamingWaitsForACompleteFrame ()
  {
    auto const symbols = SstvVisCodec::encodeExtended (0x29u).symbols;
    SstvVisStreamDecoder stream;
    std::vector<SstvVisSymbol> const first (symbols.begin (),
                                           symbols.begin () + 7);
    std::vector<SstvVisSymbol> const second (symbols.begin () + 7,
                                            symbols.end ());
    QVERIFY (stream.consumeSymbols (first).empty ());
    QVERIFY (stream.hasPartialFrame ());
    auto const decoded = stream.consumeSymbols (second);
    QCOMPARE (decoded.size (), std::size_t (1u));
    QVERIFY (decoded.front ().valid);
    QCOMPARE (static_cast<int> (decoded.front ().format),
              static_cast<int> (SstvVisFormat::Extended));
  }

  void streamFinishReportsTruncation ()
  {
    auto symbols = SstvVisCodec::encodeStandard (0x2cu).symbols;
    symbols.pop_back ();
    SstvVisStreamDecoder stream;
    QVERIFY (stream.consumeSymbols (symbols).empty ());
    auto const decoded = stream.finish ();
    QVERIFY (decoded.has_value ());
    QVERIFY (!decoded->valid);
    QVERIFY (!decoded->complete);
    QVERIFY (hasError (*decoded, SstvVisError::TruncatedFrame));
    QVERIFY (!stream.hasPartialFrame ());
  }

  void extendedReferenceVectors_data ()
  {
    QTest::addColumn<int> ("extendedPayload");
    QTest::addColumn<int> ("expectedRawCode");

    // These wire values agree between QSSTV and the independently authored
    // SSTV Handbook's MMSSTV mode table.  Mode lookup remains registry work;
    // this test covers only the extended VIS framing and parity semantics.
    QTest::newRow ("MP73") << 0x25 << 0x2523;
    QTest::newRow ("MP115") << 0x29 << 0x2923;
    QTest::newRow ("ML180") << 0x05 << 0x8523;
  }

  void extendedReferenceVectors ()
  {
    QFETCH (int, extendedPayload);
    QFETCH (int, expectedRawCode);
    auto const frame = SstvVisCodec::encodeExtended (
        static_cast<std::uint8_t> (extendedPayload));
    QVERIFY (frame.extension.has_value ());
    QCOMPARE (static_cast<int> (frame.primary.parityKind),
              static_cast<int> (SstvVisParity::Odd));
    QCOMPARE (static_cast<int> (frame.extension->parityKind),
              static_cast<int> (SstvVisParity::Odd));
    QCOMPARE (bitCount (frame.primary.rawOctet) & 1u, 1u);
    QCOMPARE (bitCount (frame.extension->rawOctet) & 1u, 1u);
    int const rawCode = frame.primary.rawOctet
                        | (static_cast<int> (frame.extension->rawOctet) << 8);
    QCOMPARE (rawCode, expectedRawCode);

    SstvVisDecodeResult const decoded =
        SstvVisCodec::decodeFrame (frame.symbols);
    QVERIFY (decoded.complete);
    QVERIFY (decoded.valid);
    QCOMPARE (static_cast<int> (decoded.format),
              static_cast<int> (SstvVisFormat::Extended));
    QCOMPARE (decoded.primary.rawOctet, std::uint8_t (0x23u));
    QVERIFY (decoded.extension.has_value ());
    QCOMPARE (decoded.extension->payload,
              static_cast<std::uint8_t> (extendedPayload));
  }

  void extendedParityAndMarkerDamageAreDistinct ()
  {
    auto const frame = SstvVisCodec::encodeExtended (0x29u);

    std::vector<SstvVisSymbol> badParity = frame.symbols;
    badParity[16] = flippedBit (badParity[16]);
    SstvVisDecodeResult const parity =
        SstvVisCodec::decodeFrame (badParity);
    QVERIFY (!parity.valid);
    QVERIFY (parity.extension.has_value ());
    QVERIFY (!parity.extension->parityValid);
    QVERIFY (hasError (parity, SstvVisError::ParityMismatch));
    QVERIFY (!hasError (parity, SstvVisError::InvalidExtendedMarker));

    // Change the first raw codeword from odd-parity 0x23 to odd-parity 0x25.
    std::vector<SstvVisSymbol> badMarker = frame.symbols;
    badMarker[2] = SstvVisSymbol::Zero;
    badMarker[3] = SstvVisSymbol::One;
    SstvVisDecodeResult const marker =
        SstvVisCodec::decodeFrame (badMarker);
    QVERIFY (!marker.valid);
    QVERIFY (marker.primary.parityValid);
    QVERIFY (hasError (marker, SstvVisError::InvalidExtendedMarker));
    QVERIFY (!hasError (marker, SstvVisError::ParityMismatch));
  }

  void sevenBitPayloadRangeIsEnforced ()
  {
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvVisCodec::encodeStandard (0x80u));
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvVisCodec::encodeExtended (0xffu));
  }

  void streamBufferIsBoundedOnMissingStop ()
  {
    SstvVisStreamDecoder stream;
    std::vector<SstvVisSymbol> hostile (1000u, SstvVisSymbol::One);
    hostile.insert (hostile.begin (), SstvVisSymbol::Separator);
    hostile.push_back (SstvVisSymbol::Separator);
    auto const decoded = stream.consumeSymbols (hostile);
    QCOMPARE (decoded.size (), std::size_t (1u));
    QVERIFY (!decoded.front ().valid);
    QCOMPARE (decoded.front ().rawBitsLsbFirst.size (), std::size_t (16u));
    QCOMPARE (decoded.front ().observedRawBitCount, std::size_t (1000u));
    QVERIFY (hasError (decoded.front (), SstvVisError::UnexpectedBitCount));
  }

  void errorNamesAreStableAndExplicit ()
  {
    QCOMPARE (QString::fromLatin1 (
                  SstvVisCodec::errorName (SstvVisError::MissingStart)),
              QStringLiteral ("missing-start"));
    QCOMPARE (QString::fromLatin1 (
                  SstvVisCodec::errorName (SstvVisError::InvalidSymbol)),
              QStringLiteral ("invalid-symbol"));
    QCOMPARE (QString::fromLatin1 (
                  SstvVisCodec::errorName (
                      SstvVisError::InvalidExtendedMarker)),
              QStringLiteral ("invalid-extended-marker"));
  }
};

QTEST_APPLESS_MAIN (TestSstvVis)

#include "test_sstv_vis.moc"
