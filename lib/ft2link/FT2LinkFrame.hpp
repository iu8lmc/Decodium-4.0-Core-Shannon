#ifndef DECODIUM_FT2LINK_FRAME_HPP
#define DECODIUM_FT2LINK_FRAME_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace decodium
{
namespace ft2link
{

enum class Profile : std::uint8_t
{
  Narrow = 0,
  Wide500 = 1,
  Wide2300 = 2
};

enum class FrameType : std::uint8_t
{
  Beacon = 0,
  Hello = 1,
  HelloAck = 2,
  Data = 3,
  Ack = 4,
  End = 5,
  Reject = 6,
  Broadcast = 7,
  Ping = 8,
  PingAck = 9
};

enum FrameFlags : std::uint8_t
{
  FlagEndOfMessage = 0x01
};

struct Frame
{
  std::uint8_t version {1};
  FrameType type {FrameType::Data};
  Profile profile {Profile::Narrow};
  std::uint8_t flags {0};
  std::uint16_t sessionId {0};
  std::uint16_t sequence {0};
  std::uint16_t ackBase {0};
  std::uint16_t ackBitmap {0};
  std::vector<std::uint8_t> payload;
};

std::string profileName (Profile profile);
std::string frameTypeName (FrameType type);

bool isValidProfile (std::uint8_t value);
bool isValidFrameType (std::uint8_t value);

std::size_t profilePayloadCapacity (Profile profile);
std::uint16_t crc16Ccitt (std::vector<std::uint8_t> const& data);

bool validateFrame (Frame const& frame, std::string* error = nullptr);
std::vector<std::uint8_t> serializeFrame (Frame const& frame);
bool parseFrame (std::vector<std::uint8_t> const& bytes, Frame* frame,
                 std::string* error = nullptr);

Frame makeAckFrame (Profile profile, std::uint16_t sessionId,
                    std::uint16_t ackBase, std::uint16_t ackBitmap);

}
}

#endif
