#ifndef DECODIUM_FT2LINK_PHYSICAL_HPP
#define DECODIUM_FT2LINK_PHYSICAL_HPP

#include "lib/ft2link/FT2LinkFrame.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace decodium
{
namespace ft2link
{

struct PhysicalProfileSpec
{
  Profile profile {Profile::Narrow};
  std::string name;
  double occupiedBandwidthHz {0.0};
  int symbolRate {0};
  int bitsPerSymbol {0};
  int repetitionFactor {0};
  bool binaryPacket {false};
  std::size_t maxSerializedFrameBytes {0};
};

bool physicalProfileSpec (Profile profile, PhysicalProfileSpec* spec);
std::vector<std::uint8_t> encodePhysicalPacket (Frame const& frame,
                                                std::string* error = nullptr);
bool decodePhysicalPacket (Profile profile,
                           std::vector<std::uint8_t> const& packet,
                           Frame* frame,
                           std::string* error = nullptr);

}
}

#endif
