// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/core/SstvVisCodec.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace decodium::sstv
{
namespace
{

SstvVisSymbol symbolForBit (bool bit) noexcept
{
  return bit ? SstvVisSymbol::One : SstvVisSymbol::Zero;
}

bool symbolIsBit (SstvVisSymbol symbol) noexcept
{
  return symbol == SstvVisSymbol::Zero || symbol == SstvVisSymbol::One;
}

unsigned populationCount7 (std::uint8_t payload) noexcept
{
  payload &= SstvVisCodec::PayloadMask;
  unsigned count = 0;
  while (payload != 0u)
    {
      count += payload & 1u;
      payload >>= 1u;
    }
  return count;
}

void requirePayload7 (std::uint8_t payload)
{
  if ((payload & static_cast<std::uint8_t> (~SstvVisCodec::PayloadMask)) != 0u)
    {
      throw std::invalid_argument ("VIS payload must contain exactly seven bits");
    }
}

SstvVisCodeword makeCodeword (std::uint8_t payload, SstvVisParity parity)
{
  requirePayload7 (payload);

  SstvVisCodeword codeword;
  codeword.payload = payload;
  codeword.payloadKnown = true;
  codeword.parityKind = parity;
  for (std::size_t bit = 0; bit < codeword.payloadBitsLsbFirst.size (); ++bit)
    {
      codeword.payloadBitsLsbFirst[bit] =
          symbolForBit (((payload >> bit) & 1u) != 0u);
    }

  codeword.parityBit = SstvVisCodec::parityBitFor (payload, parity);
  codeword.parityKnown = true;
  codeword.parityValid = true;
  codeword.rawOctet = static_cast<std::uint8_t> (
      payload | (codeword.parityBit == SstvVisSymbol::One ? 0x80u : 0u));
  codeword.rawOctetKnown = true;
  return codeword;
}

void appendCodeword (std::vector<SstvVisSymbol>& destination,
                     SstvVisCodeword const& codeword)
{
  destination.insert (destination.end (),
                      codeword.payloadBitsLsbFirst.begin (),
                      codeword.payloadBitsLsbFirst.end ());
  destination.push_back (codeword.parityBit);
}

void appendError (SstvVisDecodeResult& result, SstvVisError error)
{
  if (std::find (result.errors.begin (), result.errors.end (), error)
      == result.errors.end ())
    {
      result.errors.push_back (error);
    }
}

double normalizedConfidence (double confidence) noexcept
{
  if (!std::isfinite (confidence))
    {
      return 0.0;
    }
  return std::max (0.0, std::min (1.0, confidence));
}

double meanConfidence (std::vector<SstvVisObservation> const& observations,
                       std::size_t count) noexcept
{
  count = std::min (count, observations.size ());
  if (count == 0u)
    {
      return 0.0;
    }

  double sum = 0.0;
  for (std::size_t index = 0; index < count; ++index)
    {
      sum += normalizedConfidence (observations[index].confidence);
    }
  return sum / static_cast<double> (count);
}

SstvVisCodeword parseCodeword (std::vector<SstvVisSymbol> const& rawBits,
                               std::size_t offset,
                               SstvVisParity parityKind,
                               SstvVisDecodeResult& result)
{
  SstvVisCodeword codeword;
  codeword.parityKind = parityKind;

  bool payloadKnown = true;
  std::uint8_t payload = 0u;
  for (std::size_t bit = 0; bit < codeword.payloadBitsLsbFirst.size (); ++bit)
    {
      if (offset + bit >= rawBits.size ())
        {
          payloadKnown = false;
          break;
        }

      SstvVisSymbol const symbol = rawBits[offset + bit];
      codeword.payloadBitsLsbFirst[bit] = symbol;
      if (!symbolIsBit (symbol))
        {
          payloadKnown = false;
          appendError (result, SstvVisError::InvalidSymbol);
        }
      else if (symbol == SstvVisSymbol::One)
        {
          payload = static_cast<std::uint8_t> (payload | (1u << bit));
        }
    }

  codeword.payload = payload;
  codeword.payloadKnown = payloadKnown;

  std::size_t const parityOffset = offset + 7u;
  if (parityOffset < rawBits.size ())
    {
      codeword.parityBit = rawBits[parityOffset];
      codeword.parityKnown = symbolIsBit (codeword.parityBit);
      if (!codeword.parityKnown)
        {
          appendError (result, SstvVisError::InvalidSymbol);
        }
    }

  codeword.rawOctetKnown = codeword.payloadKnown && codeword.parityKnown;
  if (codeword.rawOctetKnown)
    {
      codeword.rawOctet = static_cast<std::uint8_t> (
          codeword.payload
          | (codeword.parityBit == SstvVisSymbol::One ? 0x80u : 0u));
      codeword.parityValid =
          codeword.parityBit
          == SstvVisCodec::parityBitFor (codeword.payload, parityKind);
      if (!codeword.parityValid)
        {
          appendError (result, SstvVisError::ParityMismatch);
        }
    }

  return codeword;
}

void classifyAndParse (SstvVisDecodeResult& result)
{
  std::size_t const bitCount = result.rawBitsLsbFirst.size ();
  result.observedRawBitCount = bitCount;

  bool const standardLength = result.complete
                              && bitCount == SstvVisCodec::StandardRawBitCount;
  bool const extendedLength = result.complete
                              && bitCount == SstvVisCodec::ExtendedRawBitCount;

  if (standardLength)
    {
      result.format = SstvVisFormat::Standard;
      result.primary = parseCodeword (result.rawBitsLsbFirst, 0u,
                                      SstvVisParity::Even, result);
    }
  else if (extendedLength)
    {
      result.format = SstvVisFormat::Extended;
      result.primary = parseCodeword (result.rawBitsLsbFirst, 0u,
                                      SstvVisParity::Odd, result);
      result.extension = parseCodeword (result.rawBitsLsbFirst, 8u,
                                        SstvVisParity::Odd, result);
      if (result.primary.rawOctetKnown
          && result.primary.rawOctet
                 != SstvVisCodec::ExtendedMarkerRawOctet)
        {
          appendError (result, SstvVisError::InvalidExtendedMarker);
        }
    }
  else if (!result.complete)
    {
      // More than one codeword, or a complete 0x23 marker without its second
      // codeword, is enough to identify an intended extended frame while still
      // reporting it as truncated.
      if (bitCount > SstvVisCodec::StandardRawBitCount)
        {
          result.format = SstvVisFormat::Extended;
          result.primary = parseCodeword (result.rawBitsLsbFirst, 0u,
                                          SstvVisParity::Odd, result);
          if (bitCount > SstvVisCodec::StandardRawBitCount)
            {
              result.extension = parseCodeword (result.rawBitsLsbFirst, 8u,
                                                SstvVisParity::Odd, result);
            }
        }
      else if (bitCount == SstvVisCodec::StandardRawBitCount)
        {
          SstvVisDecodeResult markerProbe;
          SstvVisCodeword const marker = parseCodeword (
              result.rawBitsLsbFirst, 0u, SstvVisParity::Odd, markerProbe);
          if (marker.rawOctetKnown
              && marker.rawOctet == SstvVisCodec::ExtendedMarkerRawOctet)
            {
              result.format = SstvVisFormat::Extended;
              result.primary = marker;
            }
          else
            {
              result.format = SstvVisFormat::Standard;
              result.primary = parseCodeword (result.rawBitsLsbFirst, 0u,
                                              SstvVisParity::Even, result);
            }
        }
      else
        {
          result.format = SstvVisFormat::Unknown;
        }
    }

  if (result.complete && !standardLength && !extendedLength)
    {
      appendError (result, SstvVisError::UnexpectedBitCount);
    }

  for (SstvVisSymbol const symbol : result.rawBitsLsbFirst)
    {
      if (!symbolIsBit (symbol))
        {
          appendError (result, SstvVisError::InvalidSymbol);
          break;
        }
    }

  result.valid = result.complete && result.startValid && result.stopValid
                 && result.errors.empty ();
}

void applyOverflow (SstvVisDecodeResult& result,
                    std::size_t overflowBitCount,
                    double overflowConfidenceSum)
{
  if (overflowBitCount == 0u)
    {
      return;
    }

  std::size_t const retainedSymbolCount = result.symbolsConsumed;
  std::size_t const totalSymbolCount = retainedSymbolCount + overflowBitCount;
  result.confidence =
      (result.confidence * static_cast<double> (retainedSymbolCount)
       + overflowConfidenceSum)
      / static_cast<double> (totalSymbolCount);
  result.symbolsConsumed = totalSymbolCount;
  result.observedRawBitCount += overflowBitCount;
  appendError (result, SstvVisError::UnexpectedBitCount);
  result.valid = false;
}

} // namespace

SstvVisEncodedFrame SstvVisCodec::encodeStandard (std::uint8_t payload7)
{
  SstvVisEncodedFrame frame;
  frame.format = SstvVisFormat::Standard;
  frame.primary = makeCodeword (payload7, SstvVisParity::Even);
  frame.symbols.reserve (StandardRawBitCount + 2u);
  frame.symbols.push_back (SstvVisSymbol::Separator);
  appendCodeword (frame.symbols, frame.primary);
  frame.symbols.push_back (SstvVisSymbol::Separator);
  return frame;
}

SstvVisEncodedFrame SstvVisCodec::encodeExtended (
    std::uint8_t extendedPayload7)
{
  // Clean-room protocol representation checked against QSSTV's transmitted
  // 16-bit sequence and the independently documented MMSSTV extended-VIS
  // description: one start separator, raw 0x23 first (odd-parity) codeword,
  // one odd-parity extension codeword, then one stop separator.  No MMSSTV
  // implementation code is used here.
  SstvVisEncodedFrame frame;
  frame.format = SstvVisFormat::Extended;
  frame.primary = makeCodeword (ExtendedMarkerPayload, SstvVisParity::Odd);
  frame.extension = makeCodeword (extendedPayload7, SstvVisParity::Odd);
  frame.symbols.reserve (ExtendedRawBitCount + 2u);
  frame.symbols.push_back (SstvVisSymbol::Separator);
  appendCodeword (frame.symbols, frame.primary);
  appendCodeword (frame.symbols, *frame.extension);
  frame.symbols.push_back (SstvVisSymbol::Separator);
  return frame;
}

SstvVisDecodeResult SstvVisCodec::decodeFrame (
    std::vector<SstvVisObservation> const& observations)
{
  SstvVisDecodeResult result;
  if (observations.empty ())
    {
      appendError (result, SstvVisError::MissingStart);
      appendError (result, SstvVisError::TruncatedFrame);
      return result;
    }

  if (observations.front ().symbol != SstvVisSymbol::Separator)
    {
      result.symbolsConsumed = 1u;
      result.confidence = meanConfidence (observations, 1u);
      appendError (result, SstvVisError::MissingStart);
      return result;
    }

  result.startValid = true;
  std::size_t stopOffset = observations.size ();
  for (std::size_t index = 1u; index < observations.size (); ++index)
    {
      if (observations[index].symbol == SstvVisSymbol::Separator)
        {
          stopOffset = index;
          break;
        }
      result.rawBitsLsbFirst.push_back (observations[index].symbol);
    }

  if (stopOffset < observations.size ())
    {
      result.complete = true;
      result.stopValid = true;
      result.symbolsConsumed = stopOffset + 1u;
    }
  else
    {
      result.complete = false;
      result.stopValid = false;
      result.symbolsConsumed = observations.size ();
      appendError (result, SstvVisError::MissingStop);
      appendError (result, SstvVisError::TruncatedFrame);
    }

  result.confidence = meanConfidence (observations, result.symbolsConsumed);
  classifyAndParse (result);
  return result;
}

SstvVisDecodeResult SstvVisCodec::decodeFrame (
    std::vector<SstvVisSymbol> const& symbols)
{
  std::vector<SstvVisObservation> observations;
  observations.reserve (symbols.size ());
  for (SstvVisSymbol const symbol : symbols)
    {
      observations.push_back ({symbol, 1.0});
    }
  return decodeFrame (observations);
}

SstvVisSymbol SstvVisCodec::parityBitFor (std::uint8_t payload7,
                                          SstvVisParity parity)
{
  requirePayload7 (payload7);
  bool const payloadIsOdd = (populationCount7 (payload7) & 1u) != 0u;
  bool const parityBit = parity == SstvVisParity::Even
                             ? payloadIsOdd
                             : !payloadIsOdd;
  return symbolForBit (parityBit);
}

char const* SstvVisCodec::errorName (SstvVisError error) noexcept
{
  switch (error)
    {
    case SstvVisError::MissingStart:
      return "missing-start";
    case SstvVisError::MissingStop:
      return "missing-stop";
    case SstvVisError::TruncatedFrame:
      return "truncated-frame";
    case SstvVisError::UnexpectedBitCount:
      return "unexpected-bit-count";
    case SstvVisError::InvalidSymbol:
      return "invalid-symbol";
    case SstvVisError::ParityMismatch:
      return "parity-mismatch";
    case SstvVisError::InvalidExtendedMarker:
      return "invalid-extended-marker";
    }
  return "unknown";
}

std::vector<SstvVisDecodeResult> SstvVisStreamDecoder::consume (
    SstvVisObservation const& observation)
{
  std::vector<SstvVisDecodeResult> results;
  if (!inFrame_)
    {
      if (observation.symbol == SstvVisSymbol::Separator)
        {
          inFrame_ = true;
          buffer_.clear ();
          overflowBitCount_ = 0u;
          overflowConfidenceSum_ = 0.0;
          buffer_.push_back (observation);
        }
      else
        {
          ++discardedSymbolCount_;
        }
      return results;
    }

  if (observation.symbol == SstvVisSymbol::Separator)
    {
      buffer_.push_back (observation);
      results.push_back (finishBufferedFrame (true));
      return results;
    }

  // One start separator, at most sixteen raw bits, and one stop separator are
  // retained.  Overflow is counted but not buffered so hostile input cannot
  // grow memory without bound while a separator is absent.
  if (buffer_.size () <= SstvVisCodec::ExtendedRawBitCount)
    {
      buffer_.push_back (observation);
    }
  else
    {
      ++overflowBitCount_;
      overflowConfidenceSum_ += normalizedConfidence (observation.confidence);
    }
  return results;
}

std::vector<SstvVisDecodeResult> SstvVisStreamDecoder::consume (
    std::vector<SstvVisObservation> const& observations)
{
  std::vector<SstvVisDecodeResult> results;
  for (SstvVisObservation const& observation : observations)
    {
      std::vector<SstvVisDecodeResult> current = consume (observation);
      results.insert (results.end (), current.begin (), current.end ());
    }
  return results;
}

std::vector<SstvVisDecodeResult> SstvVisStreamDecoder::consumeSymbols (
    std::vector<SstvVisSymbol> const& symbols)
{
  std::vector<SstvVisObservation> observations;
  observations.reserve (symbols.size ());
  for (SstvVisSymbol const symbol : symbols)
    {
      observations.push_back ({symbol, 1.0});
    }
  return consume (observations);
}

std::optional<SstvVisDecodeResult> SstvVisStreamDecoder::finish ()
{
  if (!inFrame_)
    {
      return std::nullopt;
    }
  return finishBufferedFrame (false);
}

void SstvVisStreamDecoder::reset () noexcept
{
  buffer_.clear ();
  overflowBitCount_ = 0u;
  overflowConfidenceSum_ = 0.0;
  discardedSymbolCount_ = 0u;
  inFrame_ = false;
}

bool SstvVisStreamDecoder::hasPartialFrame () const noexcept
{
  return inFrame_;
}

std::size_t SstvVisStreamDecoder::discardedSymbolCount () const noexcept
{
  return discardedSymbolCount_;
}

SstvVisDecodeResult SstvVisStreamDecoder::finishBufferedFrame (bool hasStop)
{
  if (!hasStop && !buffer_.empty ()
      && buffer_.back ().symbol == SstvVisSymbol::Separator
      && buffer_.size () > 1u)
    {
      // This branch is defensive: finish(false) is only called while waiting
      // for a stop symbol, so the buffer should contain only its start
      // separator and raw bits.
      buffer_.pop_back ();
    }

  SstvVisDecodeResult result = SstvVisCodec::decodeFrame (buffer_);
  applyOverflow (result, overflowBitCount_, overflowConfidenceSum_);
  buffer_.clear ();
  overflowBitCount_ = 0u;
  overflowConfidenceSum_ = 0.0;
  inFrame_ = false;
  return result;
}

} // namespace decodium::sstv
