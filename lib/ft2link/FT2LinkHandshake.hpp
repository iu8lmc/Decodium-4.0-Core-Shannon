#ifndef DECODIUM_FT2LINK_HANDSHAKE_HPP
#define DECODIUM_FT2LINK_HANDSHAKE_HPP

#include "lib/ft2link/FT2LinkFrame.hpp"
#include "lib/ft2link/FT2LinkWaveform.hpp"

#include <cstdint>
#include <string>

namespace decodium
{
namespace ft2link
{

struct LinkCapabilities
{
  bool supportsW500 {true};
  bool supportsW2300 {true};
  bool supportsW2300Fast {true};
  bool supportsW2300Robust {true};
  Profile preferredProfile {Profile::Wide2300};
  W2300RateMode preferredW2300RateMode {W2300RateMode::Fast};
};

struct NegotiatedLink
{
  bool accepted {false};
  Profile profile {Profile::Narrow};
  W2300RateMode w2300RateMode {W2300RateMode::Fast};
};

struct HandshakeIdentity
{
  std::string call;
  std::string locator;
};

Frame makeHelloFrame (std::uint16_t sessionId, LinkCapabilities const& capabilities);
Frame makeHelloFrame (std::uint16_t sessionId,
                      LinkCapabilities const& capabilities,
                      HandshakeIdentity const& identity);
Frame makeBeaconFrame (LinkCapabilities const& capabilities,
                       HandshakeIdentity const& identity,
                       bool cq);
Frame makeHelloAckFrame (std::uint16_t sessionId,
                         LinkCapabilities const& capabilities,
                         NegotiatedLink const& negotiated);
Frame makeHelloAckFrame (std::uint16_t sessionId,
                         LinkCapabilities const& capabilities,
                         NegotiatedLink const& negotiated,
                         HandshakeIdentity const& identity);

bool parseHelloFrame (Frame const& frame,
                      LinkCapabilities* capabilities,
                      std::string* error = nullptr);
bool parseHelloFrame (Frame const& frame,
                      LinkCapabilities* capabilities,
                      HandshakeIdentity* identity,
                      std::string* error = nullptr);
bool parseBeaconFrame (Frame const& frame,
                       LinkCapabilities* capabilities,
                       HandshakeIdentity* identity,
                       bool* cq,
                       std::string* error = nullptr);
bool parseHelloAckFrame (Frame const& frame,
                         LinkCapabilities* capabilities,
                         NegotiatedLink* negotiated,
                         std::string* error = nullptr);
bool parseHelloAckFrame (Frame const& frame,
                         LinkCapabilities* capabilities,
                         NegotiatedLink* negotiated,
                         HandshakeIdentity* identity,
                         std::string* error = nullptr);

bool negotiateLink (LinkCapabilities const& local,
                    LinkCapabilities const& remote,
                    NegotiatedLink* negotiated,
                    std::string* error = nullptr);

bool answerHelloFrame (Frame const& hello,
                       LinkCapabilities const& local,
                       Frame* helloAck,
                       NegotiatedLink* negotiated,
                       std::string* error = nullptr);

}
}

#endif
