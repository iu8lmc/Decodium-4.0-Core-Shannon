#include "lib/ft2link/FT2LinkFrame.hpp"

#include <algorithm>

namespace decodium
{
namespace ft2link
{
namespace
{
constexpr std::uint8_t kMagic0 {'D'};
constexpr std::uint8_t kMagic1 {'4'};
constexpr std::uint8_t kVersion {1};
constexpr std::size_t kHeaderBytes {15};
constexpr std::size_t kCrcBytes {2};

void setError (std::string* error, char const* message)
{
  if (error)
    {
      *error = message;
    }
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
}

std::string profileName (Profile profile)
{
  switch (profile)
    {
    case Profile::Narrow: return "NARROW";
    case Profile::Wide500: return "W500";
    case Profile::Wide2300: return "W2300";
    }
  return "UNKNOWN";
}

std::string frameTypeName (FrameType type)
{
  switch (type)
    {
    case FrameType::Beacon: return "BEACON";
    case FrameType::Hello: return "HELLO";
    case FrameType::HelloAck: return "HELLO_ACK";
    case FrameType::Data: return "DATA";
    case FrameType::Ack: return "ACK";
    case FrameType::End: return "END";
    case FrameType::Reject: return "REJECT";
    case FrameType::Broadcast: return "BROADCAST";
    case FrameType::Ping: return "PING";
    case FrameType::PingAck: return "PING_ACK";
    }
  return "UNKNOWN";
}

bool isValidProfile (std::uint8_t value)
{
  return value <= static_cast<std::uint8_t> (Profile::Wide2300);
}

bool isValidFrameType (std::uint8_t value)
{
  return value <= static_cast<std::uint8_t> (FrameType::PingAck);
}

std::size_t profilePayloadCapacity (Profile profile)
{
  switch (profile)
    {
    case Profile::Narrow: return 32;
    case Profile::Wide500: return 48;
    case Profile::Wide2300: return 224;
    }
  return 0;
}

std::uint16_t crc16Ccitt (std::vector<std::uint8_t> const& data)
{
  std::uint16_t crc {0xffffu};
  for (std::uint8_t byte : data)
    {
      crc = static_cast<std::uint16_t> (crc ^ (static_cast<std::uint16_t> (byte) << 8));
      for (int bit = 0; bit < 8; ++bit)
        {
          if ((crc & 0x8000u) != 0)
            {
              crc = static_cast<std::uint16_t> ((crc << 1) ^ 0x1021u);
            }
          else
            {
              crc = static_cast<std::uint16_t> (crc << 1);
            }
        }
    }
  return crc;
}

bool validateFrame (Frame const& frame, std::string* error)
{
  if (frame.version != kVersion)
    {
      setError (error, "unsupported FT2-Link frame version");
      return false;
    }
  if (!isValidProfile (static_cast<std::uint8_t> (frame.profile)))
    {
      setError (error, "invalid FT2-Link profile");
      return false;
    }
  if (!isValidFrameType (static_cast<std::uint8_t> (frame.type)))
    {
      setError (error, "invalid FT2-Link frame type");
      return false;
    }
  if ((frame.flags & ~FlagEndOfMessage) != 0)
    {
      setError (error, "unsupported FT2-Link frame flags");
      return false;
    }
  if (frame.payload.size () > profilePayloadCapacity (frame.profile))
    {
      setError (error, "FT2-Link payload exceeds profile capacity");
      return false;
    }
  if (frame.payload.size () > 255u)
    {
      setError (error, "FT2-Link payload exceeds wire length field");
      return false;
    }
  return true;
}

std::vector<std::uint8_t> serializeFrame (Frame const& frame)
{
  std::string error;
  if (!validateFrame (frame, &error))
    {
      return {};
    }

  std::vector<std::uint8_t> out;
  out.reserve (kHeaderBytes + frame.payload.size () + kCrcBytes);
  out.push_back (kMagic0);
  out.push_back (kMagic1);
  out.push_back (frame.version);
  out.push_back (static_cast<std::uint8_t> (frame.type));
  out.push_back (static_cast<std::uint8_t> (frame.profile));
  out.push_back (frame.flags);
  appendU16 (out, frame.sessionId);
  appendU16 (out, frame.sequence);
  appendU16 (out, frame.ackBase);
  appendU16 (out, frame.ackBitmap);
  out.push_back (static_cast<std::uint8_t> (frame.payload.size ()));
  out.insert (out.end (), frame.payload.begin (), frame.payload.end ());

  std::uint16_t const crc = crc16Ccitt (out);
  appendU16 (out, crc);
  return out;
}

bool parseFrame (std::vector<std::uint8_t> const& bytes, Frame* frame, std::string* error)
{
  if (!frame)
    {
      setError (error, "missing FT2-Link frame output");
      return false;
    }
  if (bytes.size () < kHeaderBytes + kCrcBytes)
    {
      setError (error, "FT2-Link frame is too short");
      return false;
    }
  if (bytes[0] != kMagic0 || bytes[1] != kMagic1)
    {
      setError (error, "invalid FT2-Link frame magic");
      return false;
    }
  if (bytes[2] != kVersion)
    {
      setError (error, "unsupported FT2-Link frame version");
      return false;
    }
  if (!isValidFrameType (bytes[3]))
    {
      setError (error, "invalid FT2-Link frame type");
      return false;
    }
  if (!isValidProfile (bytes[4]))
    {
      setError (error, "invalid FT2-Link profile");
      return false;
    }

  std::size_t const payloadLen = bytes[14];
  std::size_t const expectedLen = kHeaderBytes + payloadLen + kCrcBytes;
  if (bytes.size () != expectedLen)
    {
      setError (error, "FT2-Link frame length mismatch");
      return false;
    }

  std::vector<std::uint8_t> withoutCrc (bytes.begin (), bytes.end () - 2);
  std::uint16_t const expectedCrc = readU16 (bytes, bytes.size () - 2);
  if (crc16Ccitt (withoutCrc) != expectedCrc)
    {
      setError (error, "FT2-Link CRC mismatch");
      return false;
    }

  Frame parsed;
  parsed.version = bytes[2];
  parsed.type = static_cast<FrameType> (bytes[3]);
  parsed.profile = static_cast<Profile> (bytes[4]);
  parsed.flags = bytes[5];
  parsed.sessionId = readU16 (bytes, 6);
  parsed.sequence = readU16 (bytes, 8);
  parsed.ackBase = readU16 (bytes, 10);
  parsed.ackBitmap = readU16 (bytes, 12);
  parsed.payload.assign (bytes.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (kHeaderBytes),
                         bytes.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (kHeaderBytes + payloadLen));

  if (!validateFrame (parsed, error))
    {
      return false;
    }

  *frame = parsed;
  return true;
}

Frame makeAckFrame (Profile profile, std::uint16_t sessionId,
                    std::uint16_t ackBase, std::uint16_t ackBitmap)
{
  Frame frame;
  frame.type = FrameType::Ack;
  frame.profile = profile;
  frame.sessionId = sessionId;
  frame.ackBase = ackBase;
  frame.ackBitmap = ackBitmap;
  return frame;
}

}
}
