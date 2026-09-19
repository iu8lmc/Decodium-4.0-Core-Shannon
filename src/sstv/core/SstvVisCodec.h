// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DECODIUM_SSTV_CORE_SSTVVISCODEC_H
#define DECODIUM_SSTV_CORE_SSTVVISCODEC_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace decodium::sstv
{

// Start and stop are the same 1200 Hz separator on the wire.  Keeping one
// symbol for both prevents a symbol-level caller from inventing information
// which is only known from frame position.
enum class SstvVisSymbol : std::uint8_t
{
  Zero,
  One,
  Separator,
  Invalid
};

enum class SstvVisFormat : std::uint8_t
{
  Unknown,
  Standard,
  Extended,
  Narrow
};

enum class SstvVisParity : std::uint8_t
{
  Even,
  Odd
};

enum class SstvVisError : std::uint8_t
{
  MissingStart,
  MissingStop,
  TruncatedFrame,
  UnexpectedBitCount,
  InvalidSymbol,
  ParityMismatch,
  InvalidExtendedMarker
};

struct SstvVisObservation
{
  SstvVisSymbol symbol {SstvVisSymbol::Invalid};
  double confidence {1.0};
};

// A VIS codeword always carries seven payload bits followed by a separate
// parity bit.  rawOctet is supplied only as a convenient wire representation;
// callers must not confuse it with the seven-bit payload.
struct SstvVisCodeword
{
  std::array<SstvVisSymbol, 7> payloadBitsLsbFirst {{
      SstvVisSymbol::Invalid,
      SstvVisSymbol::Invalid,
      SstvVisSymbol::Invalid,
      SstvVisSymbol::Invalid,
      SstvVisSymbol::Invalid,
      SstvVisSymbol::Invalid,
      SstvVisSymbol::Invalid
  }};
  std::uint8_t payload {0};
  SstvVisSymbol parityBit {SstvVisSymbol::Invalid};
  SstvVisParity parityKind {SstvVisParity::Even};
  std::uint8_t rawOctet {0};
  bool payloadKnown {false};
  bool parityKnown {false};
  bool rawOctetKnown {false};
  bool parityValid {false};
};

struct SstvVisEncodedFrame
{
  SstvVisFormat format {SstvVisFormat::Unknown};
  SstvVisCodeword primary;
  std::optional<SstvVisCodeword> extension;
  std::vector<SstvVisSymbol> symbols;
};

struct SstvVisDecodeResult
{
  SstvVisFormat format {SstvVisFormat::Unknown};
  SstvVisCodeword primary;
  std::optional<SstvVisCodeword> extension;

  // Raw on-air data/parity symbols in transmission order.  Framing
  // separators are intentionally excluded.
  std::vector<SstvVisSymbol> rawBitsLsbFirst;
  std::vector<SstvVisError> errors;

  std::size_t symbolsConsumed {0};
  std::size_t observedRawBitCount {0};
  bool complete {false};
  bool valid {false};
  bool startValid {false};
  bool stopValid {false};

  // Mean classifier confidence for consumed symbols, clamped to [0, 1].
  // Protocol validity never depends on this value.
  double confidence {0.0};
};

class SstvVisCodec final
{
public:
  static constexpr std::uint8_t PayloadMask = 0x7fu;
  static constexpr std::uint8_t ExtendedMarkerPayload = 0x23u;
  static constexpr std::uint8_t ExtendedMarkerRawOctet = 0x23u;
  static constexpr std::size_t StandardRawBitCount = 8u;
  static constexpr std::size_t ExtendedRawBitCount = 16u;

  // Throws std::invalid_argument if payload7 contains bit 7.
  static SstvVisEncodedFrame encodeStandard (std::uint8_t payload7);

  // MMSSTV-compatible wide extended VIS.  The first odd-parity codeword is
  // the raw 0x23 marker; extendedPayload7 is the payload of the second
  // odd-parity codeword.  Narrow 24-bit N-VIS is a distinct protocol and is
  // intentionally not represented by this API.
  static SstvVisEncodedFrame encodeExtended (std::uint8_t extendedPayload7);

  // Decodes the first explicitly framed symbol sequence.  symbolsConsumed
  // permits a caller to retain any following data.  For arbitrary chunks and
  // back-to-back frames, prefer SstvVisStreamDecoder.
  static SstvVisDecodeResult decodeFrame (
      std::vector<SstvVisObservation> const& observations);
  static SstvVisDecodeResult decodeFrame (
      std::vector<SstvVisSymbol> const& symbols);

  static SstvVisSymbol parityBitFor (std::uint8_t payload7,
                                     SstvVisParity parity);
  static char const* errorName (SstvVisError error) noexcept;
};

// Streaming, symbol-level framing.  The DSP layer remains responsible for
// turning tone observations into Zero/One/Separator/Invalid symbols and for
// supplying confidence values.
class SstvVisStreamDecoder final
{
public:
  std::vector<SstvVisDecodeResult> consume (
      SstvVisObservation const& observation);
  std::vector<SstvVisDecodeResult> consume (
      std::vector<SstvVisObservation> const& observations);
  std::vector<SstvVisDecodeResult> consumeSymbols (
      std::vector<SstvVisSymbol> const& symbols);

  // Returns a TruncatedFrame result when the stream ends inside a frame.
  std::optional<SstvVisDecodeResult> finish ();

  void reset () noexcept;
  bool hasPartialFrame () const noexcept;
  std::size_t discardedSymbolCount () const noexcept;

private:
  SstvVisDecodeResult finishBufferedFrame (bool hasStop);

  std::vector<SstvVisObservation> buffer_;
  std::size_t overflowBitCount_ {0};
  double overflowConfidenceSum_ {0.0};
  std::size_t discardedSymbolCount_ {0};
  bool inFrame_ {false};
};

} // namespace decodium::sstv

#endif // DECODIUM_SSTV_CORE_SSTVVISCODEC_H
