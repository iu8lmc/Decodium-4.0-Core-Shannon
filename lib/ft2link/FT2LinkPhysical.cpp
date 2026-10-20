#include "lib/ft2link/FT2LinkPhysical.hpp"

#include <algorithm>

namespace decodium
{
namespace ft2link
{
namespace
{
constexpr std::uint8_t kMagic0 {'F'};
constexpr std::uint8_t kMagic1 {'2'};
constexpr std::uint8_t kMagic2 {'L'};
constexpr std::uint8_t kMagic3 {'P'};
constexpr std::uint8_t kPhysicalVersion {1};
constexpr std::size_t kPhysicalHeaderBytes {8};
constexpr std::size_t kCrcBytes {2};

void setError (std::string* error, char const* message)
{
  if (error)
    {
      *error = message;
    }
}

std::size_t serializedFrameCapacity (Profile profile)
{
  return 15u + profilePayloadCapacity (profile) + kCrcBytes;
}

void appendU16 (std::vector<std::uint8_t>& out, std::uint16_t value)
{
  out.push_back (static_cast<std::uint8_t> ((value >> 8) & 0xffu));
  out.push_back (static_cast<std::uint8_t> (value & 0xffu));
}

std::uint16_t readU16 (std::vector<std::uint8_t> const& bytes, std::size_t offset)
{
  return static_cast<std::uint16_t> ((static_cast<std::uint16_t> (bytes[offset]) << 8)
                                    | static_cast<std::uint16_t> (bytes[offset + 1]));
}

unsigned gcdUnsigned (unsigned a, unsigned b)
{
  while (b != 0)
    {
      unsigned const next = a % b;
      a = b;
      b = next;
    }
  return a;
}

unsigned interleaverStride (std::size_t bitCount, Profile profile)
{
  unsigned preferred = profile == Profile::Wide2300 ? 149u : 97u;
  if (bitCount == 0)
    {
      return 1u;
    }
  preferred = std::max<unsigned> (1u, preferred % static_cast<unsigned> (bitCount));
  while (gcdUnsigned (preferred, static_cast<unsigned> (bitCount)) != 1u)
    {
      ++preferred;
      if (preferred >= bitCount)
        {
          preferred = 1u;
        }
    }
  return preferred;
}

std::uint8_t nextWhiteningByte (std::uint16_t& state)
{
  std::uint8_t value = 0;
  for (int bit = 0; bit < 8; ++bit)
    {
      std::uint16_t const feedback =
          static_cast<std::uint16_t> (((state >> 0) ^ (state >> 2) ^ (state >> 3) ^ (state >> 5)) & 1u);
      state = static_cast<std::uint16_t> ((state >> 1) | (feedback << 15));
      value = static_cast<std::uint8_t> ((value << 1) | (state & 1u));
    }
  return value;
}

void whitenInPlace (std::vector<std::uint8_t>& bytes, Profile profile)
{
  std::uint16_t state = static_cast<std::uint16_t> (0xace1u
      ^ (static_cast<std::uint16_t> (profile) * 0x1d0fu));
  for (std::uint8_t& byte : bytes)
    {
      byte = static_cast<std::uint8_t> (byte ^ nextWhiteningByte (state));
    }
}

std::vector<std::uint8_t> bytesToBits (std::vector<std::uint8_t> const& bytes)
{
  std::vector<std::uint8_t> bits;
  bits.reserve (bytes.size () * 8u);
  for (std::uint8_t byte : bytes)
    {
      for (int bit = 7; bit >= 0; --bit)
        {
          bits.push_back (static_cast<std::uint8_t> ((byte >> bit) & 1u));
        }
    }
  return bits;
}

std::vector<std::uint8_t> bitsToBytes (std::vector<std::uint8_t> const& bits)
{
  std::vector<std::uint8_t> bytes (bits.size () / 8u, 0);
  for (std::size_t i = 0; i < bits.size (); ++i)
    {
      if (bits[i] != 0)
        {
          bytes[i / 8u] = static_cast<std::uint8_t> (bytes[i / 8u] | (1u << (7u - (i % 8u))));
        }
    }
  return bytes;
}

std::vector<std::uint8_t> packPhysicalBits (std::vector<std::uint8_t> const& plain,
                                            PhysicalProfileSpec const& spec)
{
  std::vector<std::uint8_t> plainBits = bytesToBits (plain);
  std::vector<std::uint8_t> repeated;
  repeated.reserve (plainBits.size () * static_cast<std::size_t> (spec.repetitionFactor));
  for (std::uint8_t bit : plainBits)
    {
      for (int copy = 0; copy < spec.repetitionFactor; ++copy)
        {
          repeated.push_back (bit);
        }
    }

  std::vector<std::uint8_t> interleaved (repeated.size (), 0);
  unsigned const stride = interleaverStride (repeated.size (), spec.profile);
  for (std::size_t i = 0; i < repeated.size (); ++i)
    {
      interleaved[(i * stride) % repeated.size ()] = repeated[i];
    }
  return bitsToBytes (interleaved);
}

bool unpackPhysicalBits (std::vector<std::uint8_t> const& packet,
                         PhysicalProfileSpec const& spec,
                         std::vector<std::uint8_t>* plain,
                         std::string* error)
{
  if (!plain)
    {
      setError (error, "missing FT2-Link physical output");
      return false;
    }
  if (packet.empty () || spec.repetitionFactor <= 0)
    {
      setError (error, "invalid FT2-Link physical packet");
      return false;
    }

  std::vector<std::uint8_t> interleaved = bytesToBits (packet);
  if ((interleaved.size () % static_cast<std::size_t> (spec.repetitionFactor)) != 0)
    {
      setError (error, "FT2-Link physical packet has invalid coded length");
      return false;
    }

  std::vector<std::uint8_t> repeated (interleaved.size (), 0);
  unsigned const stride = interleaverStride (interleaved.size (), spec.profile);
  for (std::size_t i = 0; i < repeated.size (); ++i)
    {
      repeated[i] = interleaved[(i * stride) % repeated.size ()];
    }

  std::size_t const plainBitCount = repeated.size () / static_cast<std::size_t> (spec.repetitionFactor);
  if ((plainBitCount % 8u) != 0)
    {
      setError (error, "FT2-Link physical packet has invalid plain length");
      return false;
    }

  std::vector<std::uint8_t> plainBits;
  plainBits.reserve (plainBitCount);
  for (std::size_t i = 0; i < plainBitCount; ++i)
    {
      int ones = 0;
      for (int copy = 0; copy < spec.repetitionFactor; ++copy)
        {
          ones += repeated[i * static_cast<std::size_t> (spec.repetitionFactor)
                           + static_cast<std::size_t> (copy)] != 0 ? 1 : 0;
        }
      plainBits.push_back (static_cast<std::uint8_t> (ones > spec.repetitionFactor / 2 ? 1 : 0));
    }

  *plain = bitsToBytes (plainBits);
  return true;
}
}

bool physicalProfileSpec (Profile profile, PhysicalProfileSpec* spec)
{
  if (!spec)
    {
      return false;
    }

  PhysicalProfileSpec result;
  result.profile = profile;
  result.maxSerializedFrameBytes = serializedFrameCapacity (profile);

  switch (profile)
    {
    case Profile::Narrow:
      result.name = "FT2-Link Narrow";
      result.occupiedBandwidthHz = 75.0;
      result.symbolRate = 0;
      result.bitsPerSymbol = 0;
      result.repetitionFactor = 0;
      result.binaryPacket = false;
      break;
    case Profile::Wide500:
      result.name = "FT2-Link W500";
      result.occupiedBandwidthHz = 500.0;
      result.symbolRate = 125;
      result.bitsPerSymbol = 2;
      result.repetitionFactor = 3;
      result.binaryPacket = true;
      break;
    case Profile::Wide2300:
      result.name = "FT2-Link W2300";
      result.occupiedBandwidthHz = 2300.0;
      result.symbolRate = 600;
      result.bitsPerSymbol = 8;
      result.repetitionFactor = 1;
      result.binaryPacket = true;
      break;
    default:
      return false;
    }

  *spec = result;
  return true;
}

std::vector<std::uint8_t> encodePhysicalPacket (Frame const& frame, std::string* error)
{
  PhysicalProfileSpec spec;
  if (!physicalProfileSpec (frame.profile, &spec) || !spec.binaryPacket)
    {
      setError (error, "FT2-Link physical packets require a wide profile");
      return {};
    }

  std::vector<std::uint8_t> const wireFrame = serializeFrame (frame);
  if (wireFrame.empty ())
    {
      setError (error, "invalid FT2-Link frame for physical packet");
      return {};
    }
  if (wireFrame.size () > spec.maxSerializedFrameBytes || wireFrame.size () > 0xffffu)
    {
      setError (error, "FT2-Link frame exceeds physical profile capacity");
      return {};
    }

  std::vector<std::uint8_t> plain;
  plain.reserve (kPhysicalHeaderBytes + wireFrame.size () + kCrcBytes);
  plain.push_back (kMagic0);
  plain.push_back (kMagic1);
  plain.push_back (kMagic2);
  plain.push_back (kMagic3);
  plain.push_back (kPhysicalVersion);
  plain.push_back (static_cast<std::uint8_t> (frame.profile));
  appendU16 (plain, static_cast<std::uint16_t> (wireFrame.size ()));
  plain.insert (plain.end (), wireFrame.begin (), wireFrame.end ());
  appendU16 (plain, crc16Ccitt (plain));

  whitenInPlace (plain, frame.profile);
  return packPhysicalBits (plain, spec);
}

bool decodePhysicalPacket (Profile profile,
                           std::vector<std::uint8_t> const& packet,
                           Frame* frame,
                           std::string* error)
{
  PhysicalProfileSpec spec;
  if (!physicalProfileSpec (profile, &spec) || !spec.binaryPacket)
    {
      setError (error, "FT2-Link physical packets require a wide profile");
      return false;
    }

  std::vector<std::uint8_t> plain;
  if (!unpackPhysicalBits (packet, spec, &plain, error))
    {
      return false;
    }
  whitenInPlace (plain, profile);

  if (plain.size () < kPhysicalHeaderBytes + kCrcBytes)
    {
      setError (error, "FT2-Link physical packet is too short");
      return false;
    }
  if (plain[0] != kMagic0 || plain[1] != kMagic1 || plain[2] != kMagic2 || plain[3] != kMagic3)
    {
      setError (error, "invalid FT2-Link physical magic");
      return false;
    }
  if (plain[4] != kPhysicalVersion)
    {
      setError (error, "unsupported FT2-Link physical version");
      return false;
    }
  if (plain[5] != static_cast<std::uint8_t> (profile))
    {
      setError (error, "FT2-Link physical profile mismatch");
      return false;
    }

  std::uint16_t const frameLen = readU16 (plain, 6);
  std::size_t const expectedSize = kPhysicalHeaderBytes + static_cast<std::size_t> (frameLen) + kCrcBytes;
  if (plain.size () != expectedSize)
    {
      setError (error, "FT2-Link physical length mismatch");
      return false;
    }
  if (frameLen > spec.maxSerializedFrameBytes)
    {
      setError (error, "FT2-Link physical frame exceeds profile capacity");
      return false;
    }

  std::vector<std::uint8_t> withoutCrc (plain.begin (), plain.end () - 2);
  if (crc16Ccitt (withoutCrc) != readU16 (plain, plain.size () - 2))
    {
      setError (error, "FT2-Link physical CRC mismatch");
      return false;
    }

  std::vector<std::uint8_t> frameBytes (
      plain.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (kPhysicalHeaderBytes),
      plain.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (kPhysicalHeaderBytes + frameLen));
  Frame parsed;
  if (!parseFrame (frameBytes, &parsed, error))
    {
      return false;
    }
  if (parsed.profile != profile)
    {
      setError (error, "FT2-Link logical profile mismatch");
      return false;
    }

  *frame = parsed;
  return true;
}

}
}
