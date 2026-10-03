#include "lib/ft2link/FT2LinkHandshake.hpp"

#include <algorithm>
#include <cctype>

namespace decodium
{
namespace ft2link
{
namespace
{
constexpr std::uint8_t kHandshakeMagic {'H'};
constexpr std::uint8_t kHandshakeVersion {1};
constexpr std::size_t kBaseHandshakePayloadBytes {6};
constexpr std::size_t kMaxHandshakePayloadBytes {32};
constexpr std::size_t kMaxHandshakeCallBytes {12};
constexpr std::size_t kMaxHandshakeLocatorBytes {8};

enum CapabilityFlags : std::uint8_t
{
  CapW500 = 0x01,
  CapW2300 = 0x02,
  CapW2300Fast = 0x04,
  CapW2300Robust = 0x08
};

enum HandshakeStatus : std::uint8_t
{
  StatusOffer = 0,
  StatusAccepted = 1,
  StatusRejected = 2
};

void setError (std::string* error, char const* message)
{
  if (error)
    {
      *error = message;
    }
}

std::uint8_t capabilityFlags (LinkCapabilities const& capabilities)
{
  std::uint8_t flags = 0;
  if (capabilities.supportsW500)
    {
      flags = static_cast<std::uint8_t> (flags | CapW500);
    }
  if (capabilities.supportsW2300)
    {
      flags = static_cast<std::uint8_t> (flags | CapW2300);
    }
  if (capabilities.supportsW2300Fast)
    {
      flags = static_cast<std::uint8_t> (flags | CapW2300Fast);
    }
  if (capabilities.supportsW2300Robust)
    {
      flags = static_cast<std::uint8_t> (flags | CapW2300Robust);
    }
  return flags;
}

LinkCapabilities capabilitiesFromFlags (std::uint8_t flags,
                                        Profile preferredProfile,
                                        W2300RateMode preferredRateMode)
{
  LinkCapabilities capabilities;
  capabilities.supportsW500 = (flags & CapW500) != 0;
  capabilities.supportsW2300 = (flags & CapW2300) != 0;
  capabilities.supportsW2300Fast = (flags & CapW2300Fast) != 0;
  capabilities.supportsW2300Robust = (flags & CapW2300Robust) != 0;
  capabilities.preferredProfile = preferredProfile;
  capabilities.preferredW2300RateMode = preferredRateMode;
  return capabilities;
}

bool validW2300RateMode (std::uint8_t value)
{
  return value == static_cast<std::uint8_t> (W2300RateMode::Fast)
      || value == static_cast<std::uint8_t> (W2300RateMode::Robust)
      || value == static_cast<std::uint8_t> (W2300RateMode::Weak)
      || value == static_cast<std::uint8_t> (W2300RateMode::Deep)
      || value == static_cast<std::uint8_t> (W2300RateMode::Ultra);
}

std::string normalizedIdentityToken (std::string const& value,
                                     std::size_t maxBytes,
                                     bool locator)
{
  std::string out;
  out.reserve (std::min (value.size (), maxBytes));
  for (char ch : value)
    {
      unsigned char const uch = static_cast<unsigned char> (ch);
      char const up = static_cast<char> (std::toupper (uch));
      bool const accepted = std::isalnum (uch)
          || (!locator && (ch == '/' || ch == '-'));
      if (!accepted)
        {
          continue;
        }
      out.push_back (up);
      if (out.size () >= maxBytes)
        {
          break;
        }
    }
  return out;
}

void appendIdentity (std::vector<std::uint8_t>& payload,
                     HandshakeIdentity const& identity)
{
  std::string const call = normalizedIdentityToken (
      identity.call, kMaxHandshakeCallBytes, false);
  std::string const locator = normalizedIdentityToken (
      identity.locator, kMaxHandshakeLocatorBytes, true);
  if (call.empty () && locator.empty ())
    {
      return;
    }

  std::size_t remaining = kMaxHandshakePayloadBytes - payload.size ();
  if (remaining < 2u)
    {
      return;
    }

  std::size_t const callBytes = std::min (call.size (), remaining - 2u);
  remaining -= 1u + callBytes;
  std::size_t const locatorBytes = remaining > 0u
      ? std::min (locator.size (), remaining - 1u)
      : 0u;

  payload.push_back (static_cast<std::uint8_t> (callBytes));
  payload.insert (payload.end (), call.begin (),
                  call.begin () + static_cast<std::string::difference_type> (callBytes));
  payload.push_back (static_cast<std::uint8_t> (locatorBytes));
  payload.insert (payload.end (), locator.begin (),
                  locator.begin () + static_cast<std::string::difference_type> (locatorBytes));
}

std::vector<std::uint8_t> makePayload (LinkCapabilities const& capabilities,
                                       HandshakeStatus status,
                                       NegotiatedLink const* negotiated,
                                       HandshakeIdentity const* identity)
{
  Profile profile = capabilities.preferredProfile;
  W2300RateMode rateMode = capabilities.preferredW2300RateMode;
  if (negotiated)
    {
      profile = negotiated->profile;
      rateMode = negotiated->w2300RateMode;
    }

  std::vector<std::uint8_t> payload;
  payload.reserve (kMaxHandshakePayloadBytes);
  payload.push_back (kHandshakeMagic);
  payload.push_back (kHandshakeVersion);
  payload.push_back (capabilityFlags (capabilities));
  payload.push_back (static_cast<std::uint8_t> (profile));
  payload.push_back (static_cast<std::uint8_t> (rateMode));
  payload.push_back (static_cast<std::uint8_t> (status));
  if (identity)
    {
      appendIdentity (payload, *identity);
    }
  return payload;
}

bool parsePayload (std::vector<std::uint8_t> const& payload,
                   LinkCapabilities* capabilities,
                   NegotiatedLink* negotiated,
                   HandshakeIdentity* identity,
                   HandshakeStatus* status,
                   std::string* error)
{
  if (payload.size () < kBaseHandshakePayloadBytes
      || payload.size () > kMaxHandshakePayloadBytes)
    {
      setError (error, "invalid FT2-Link handshake payload length");
      return false;
    }
  if (payload[0] != kHandshakeMagic)
    {
      setError (error, "invalid FT2-Link handshake magic");
      return false;
    }
  if (payload[1] != kHandshakeVersion)
    {
      setError (error, "unsupported FT2-Link handshake version");
      return false;
    }
  if (!isValidProfile (payload[3]) || !validW2300RateMode (payload[4]))
    {
      setError (error, "invalid FT2-Link handshake choice");
      return false;
    }
  if (payload[5] > StatusRejected)
    {
      setError (error, "invalid FT2-Link handshake status");
      return false;
    }

  Profile const profile = static_cast<Profile> (payload[3]);
  W2300RateMode const rateMode = static_cast<W2300RateMode> (payload[4]);
  if (capabilities)
    {
      *capabilities = capabilitiesFromFlags (payload[2], profile, rateMode);
    }
  if (negotiated)
    {
      negotiated->accepted = payload[5] == StatusAccepted;
      negotiated->profile = profile;
      negotiated->w2300RateMode = rateMode;
    }
  if (status)
    {
      *status = static_cast<HandshakeStatus> (payload[5]);
    }
  if (identity)
    {
      identity->call.clear ();
      identity->locator.clear ();
      if (payload.size () > kBaseHandshakePayloadBytes)
        {
          std::size_t offset = kBaseHandshakePayloadBytes;
          if (offset >= payload.size ())
            {
              setError (error, "invalid FT2-Link handshake identity");
              return false;
            }
          std::size_t const callBytes = payload[offset++];
          if (offset + callBytes > payload.size ())
            {
              setError (error, "truncated FT2-Link handshake callsign");
              return false;
            }
          identity->call.assign (
              payload.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (offset),
              payload.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (offset + callBytes));
          offset += callBytes;
          if (offset >= payload.size ())
            {
              setError (error, "truncated FT2-Link handshake locator length");
              return false;
            }
          std::size_t const locatorBytes = payload[offset++];
          if (offset + locatorBytes != payload.size ())
            {
              setError (error, "truncated FT2-Link handshake locator");
              return false;
            }
          identity->locator.assign (
              payload.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (offset),
              payload.end ());
        }
    }
  return true;
}
}

Frame makeHelloFrame (std::uint16_t sessionId, LinkCapabilities const& capabilities)
{
  return makeHelloFrame (sessionId, capabilities, HandshakeIdentity {});
}

Frame makeHelloFrame (std::uint16_t sessionId,
                      LinkCapabilities const& capabilities,
                      HandshakeIdentity const& identity)
{
  Frame frame;
  frame.type = FrameType::Hello;
  frame.profile = Profile::Narrow;
  frame.sessionId = sessionId;
  frame.payload = makePayload (capabilities, StatusOffer, nullptr, &identity);
  return frame;
}

Frame makeBeaconFrame (LinkCapabilities const& capabilities,
                       HandshakeIdentity const& identity,
                       bool cq)
{
  Frame frame;
  frame.type = FrameType::Beacon;
  frame.profile = Profile::Narrow;
  frame.flags = cq ? static_cast<std::uint8_t> (FlagEndOfMessage)
                   : static_cast<std::uint8_t> (0);
  frame.payload = makePayload (capabilities, StatusOffer, nullptr, &identity);
  return frame;
}

Frame makeHelloAckFrame (std::uint16_t sessionId,
                         LinkCapabilities const& capabilities,
                         NegotiatedLink const& negotiated)
{
  return makeHelloAckFrame (sessionId, capabilities, negotiated, HandshakeIdentity {});
}

Frame makeHelloAckFrame (std::uint16_t sessionId,
                         LinkCapabilities const& capabilities,
                         NegotiatedLink const& negotiated,
                         HandshakeIdentity const& identity)
{
  Frame frame;
  frame.type = FrameType::HelloAck;
  frame.profile = Profile::Narrow;
  frame.sessionId = sessionId;
  frame.payload = makePayload (capabilities,
                               negotiated.accepted ? StatusAccepted : StatusRejected,
                               &negotiated,
                               &identity);
  return frame;
}

bool parseHelloFrame (Frame const& frame,
                      LinkCapabilities* capabilities,
                      std::string* error)
{
  return parseHelloFrame (frame, capabilities, nullptr, error);
}

bool parseHelloFrame (Frame const& frame,
                      LinkCapabilities* capabilities,
                      HandshakeIdentity* identity,
                      std::string* error)
{
  if (frame.type != FrameType::Hello || frame.profile != Profile::Narrow)
    {
      setError (error, "expected FT2-Link NARROW HELLO frame");
      return false;
    }

  HandshakeStatus status;
  if (!parsePayload (frame.payload, capabilities, nullptr, identity, &status, error))
    {
      return false;
    }
  if (status != StatusOffer)
    {
      setError (error, "FT2-Link HELLO must contain an offer");
      return false;
    }
  return true;
}

bool parseBeaconFrame (Frame const& frame,
                       LinkCapabilities* capabilities,
                       HandshakeIdentity* identity,
                       bool* cq,
                       std::string* error)
{
  if (frame.type != FrameType::Beacon || frame.profile != Profile::Narrow)
    {
      setError (error, "expected FT2-Link NARROW BEACON frame");
      return false;
    }

  HandshakeStatus status;
  if (!parsePayload (frame.payload, capabilities, nullptr, identity, &status, error))
    {
      return false;
    }
  if (status != StatusOffer)
    {
      setError (error, "FT2-Link BEACON must contain an offer");
      return false;
    }
  if (cq)
    {
      *cq = (frame.flags & FlagEndOfMessage) != 0u;
    }
  return true;
}

bool parseHelloAckFrame (Frame const& frame,
                         LinkCapabilities* capabilities,
                         NegotiatedLink* negotiated,
                         std::string* error)
{
  return parseHelloAckFrame (frame, capabilities, negotiated, nullptr, error);
}

bool parseHelloAckFrame (Frame const& frame,
                         LinkCapabilities* capabilities,
                         NegotiatedLink* negotiated,
                         HandshakeIdentity* identity,
                         std::string* error)
{
  if (frame.type != FrameType::HelloAck || frame.profile != Profile::Narrow)
    {
      setError (error, "expected FT2-Link NARROW HELLO_ACK frame");
      return false;
    }

  HandshakeStatus status;
  if (!parsePayload (frame.payload, capabilities, negotiated, identity, &status, error))
    {
      return false;
    }
  if (status != StatusAccepted && status != StatusRejected)
    {
      setError (error, "FT2-Link HELLO_ACK must accept or reject");
      return false;
    }
  return true;
}

bool negotiateLink (LinkCapabilities const& local,
                    LinkCapabilities const& remote,
                    NegotiatedLink* negotiated,
                    std::string* error)
{
  if (!negotiated)
    {
      setError (error, "missing FT2-Link negotiation output");
      return false;
    }

  NegotiatedLink result;
  bool const canW2300 = local.supportsW2300 && remote.supportsW2300;
  bool const canW500 = local.supportsW500 && remote.supportsW500;

  if (canW2300)
    {
      result.accepted = true;
      result.profile = Profile::Wide2300;
      bool const bothFast = local.supportsW2300Fast && remote.supportsW2300Fast;
      bool const bothRobust = local.supportsW2300Robust && remote.supportsW2300Robust;
      bool const wantsUltra = local.preferredW2300RateMode == W2300RateMode::Ultra
          || remote.preferredW2300RateMode == W2300RateMode::Ultra;
      bool const wantsDeep = local.preferredW2300RateMode == W2300RateMode::Deep
          || remote.preferredW2300RateMode == W2300RateMode::Deep;
      bool const wantsWeak = local.preferredW2300RateMode == W2300RateMode::Weak
          || remote.preferredW2300RateMode == W2300RateMode::Weak;
      bool const wantsRobust = local.preferredW2300RateMode == W2300RateMode::Robust
          || remote.preferredW2300RateMode == W2300RateMode::Robust;
      if (bothRobust && wantsUltra)
        {
          result.w2300RateMode = W2300RateMode::Ultra;
        }
      else if (bothRobust && wantsDeep)
        {
          result.w2300RateMode = W2300RateMode::Deep;
        }
      else if (bothRobust && wantsWeak)
        {
          result.w2300RateMode = W2300RateMode::Weak;
        }
      else if (bothRobust && (wantsRobust || !bothFast))
        {
          result.w2300RateMode = W2300RateMode::Robust;
        }
      else if (bothFast)
        {
          result.w2300RateMode = W2300RateMode::Fast;
        }
      else if (bothRobust)
        {
          result.w2300RateMode = W2300RateMode::Robust;
        }
      else
        {
          result.accepted = false;
        }
    }
  else if (canW500)
    {
      result.accepted = true;
      result.profile = Profile::Wide500;
      result.w2300RateMode = W2300RateMode::Fast;
    }

  if (!result.accepted)
    {
      setError (error, "no common FT2-Link wide profile");
      result.profile = Profile::Narrow;
    }

  *negotiated = result;
  return result.accepted;
}

bool answerHelloFrame (Frame const& hello,
                       LinkCapabilities const& local,
                       Frame* helloAck,
                       NegotiatedLink* negotiated,
                       std::string* error)
{
  if (!helloAck || !negotiated)
    {
      setError (error, "missing FT2-Link handshake answer output");
      return false;
    }

  LinkCapabilities remote;
  if (!parseHelloFrame (hello, &remote, error))
    {
      return false;
    }

  std::string negotiationError;
  bool const accepted = negotiateLink (local, remote, negotiated, &negotiationError);
  *helloAck = makeHelloAckFrame (hello.sessionId, local, *negotiated);
  if (!accepted)
    {
      setError (error, negotiationError.empty ()
                ? "FT2-Link handshake rejected"
                : negotiationError.c_str ());
    }
  return accepted;
}

}
}
