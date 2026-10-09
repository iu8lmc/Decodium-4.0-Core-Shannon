#include "lib/ft2link/FT2LinkAppModel.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace decodium
{
namespace ft2link
{
namespace
{
void setError (std::string* error, char const* message)
{
  if (error)
    {
      *error = message;
    }
}

bool isActiveAt (StationAdvertisement const& advertisement,
                 std::uint64_t nowMs,
                 std::uint64_t maxAgeMs)
{
  return advertisement.heardAtMs <= nowMs
      && nowMs - advertisement.heardAtMs <= maxAgeMs;
}

int normalizeSlotId (int slotId)
{
  if (slotId < -10 || slotId > 10)
    {
      return 0;
    }
  return slotId;
}

int normalizeSlotSizeHz (int slotSizeHz)
{
  return std::max (100, std::min (slotSizeHz, 5000));
}

std::uint16_t encodeSignedSlotId (int slotId)
{
  int const normalized = normalizeSlotId (slotId);
  return static_cast<std::uint16_t> (
      normalized < 0 ? 65536 + normalized : normalized);
}

int decodeSignedSlotId (std::uint16_t encoded)
{
  int const value = static_cast<int> (encoded);
  int const signedValue = value > 32767 ? value - 65536 : value;
  return normalizeSlotId (signedValue);
}

std::string uppercaseAscii (std::string const& value)
{
  std::string out;
  out.reserve (value.size ());
  for (char ch : value)
    {
      out.push_back (static_cast<char> (
          std::toupper (static_cast<unsigned char> (ch))));
    }
  return out;
}

std::string normalizeCqType (std::string const& type)
{
  std::string const upper = uppercaseAscii (type);
  if (upper == "CHAT" || upper == "NET" || upper == "EMCOMM"
      || upper == "TEST" || upper == "QSY")
    {
      return upper;
    }
  return "CQ";
}

std::uint16_t encodeCqType (std::string const& type)
{
  std::string const normalized = normalizeCqType (type);
  if (normalized == "CHAT") return 1u;
  if (normalized == "NET") return 2u;
  if (normalized == "EMCOMM") return 3u;
  if (normalized == "TEST") return 4u;
  if (normalized == "QSY") return 5u;
  return 0u;
}

std::string decodeCqType (std::uint16_t code)
{
  switch (code & 0x000fu)
    {
    case 1u: return "CHAT";
    case 2u: return "NET";
    case 3u: return "EMCOMM";
    case 4u: return "TEST";
    case 5u: return "QSY";
    default: return "CQ";
    }
}

std::string normalizeLocator (std::string const& locator)
{
  std::string const upper = uppercaseAscii (locator);
  std::string out;
  out.reserve (upper.size ());
  for (char ch : upper)
    {
      if (std::isalnum (static_cast<unsigned char> (ch)))
        {
          out.push_back (ch);
        }
      if (out.size () >= 8u)
        {
          break;
        }
    }
  return out;
}

HandshakeIdentity handshakeIdentityFromStation (StationIdentity const& station)
{
  HandshakeIdentity identity;
  identity.call = station.call;
  identity.locator = station.locator;
  return identity;
}

void appendSummaryToken (std::ostringstream* stream,
                         char const* token,
                         bool enabled)
{
  if (!stream || !enabled)
    {
      return;
}
  if (stream->tellp () > 0)
    {
      *stream << ' ';
    }
  *stream << token;
}
	}

std::uint16_t beaconWaveformCapabilityFlags (
    LinkCapabilities const& capabilities)
{
  std::uint16_t flags = 0;
  if (capabilities.supportsW500)
    {
      flags = static_cast<std::uint16_t> (flags | BeaconWaveW500);
    }
  if (capabilities.supportsW2300)
    {
      flags = static_cast<std::uint16_t> (flags | BeaconWaveW2300);
    }
  if (capabilities.supportsW2300Fast)
    {
      flags = static_cast<std::uint16_t> (flags | BeaconWaveW2300Fast);
    }
  if (capabilities.supportsW2300Robust)
    {
      flags = static_cast<std::uint16_t> (
          flags | BeaconWaveW2300Robust | BeaconWaveW2300Weak
          | BeaconWaveW2300Deep | BeaconWaveW2300Ultra);
    }

  switch (capabilities.preferredW2300RateMode)
    {
    case W2300RateMode::Fast:
      flags = static_cast<std::uint16_t> (flags | BeaconWaveW2300Fast);
      break;
    case W2300RateMode::Robust:
      flags = static_cast<std::uint16_t> (flags | BeaconWaveW2300Robust);
      break;
    case W2300RateMode::Weak:
      flags = static_cast<std::uint16_t> (flags | BeaconWaveW2300Weak);
      break;
    case W2300RateMode::Deep:
      flags = static_cast<std::uint16_t> (flags | BeaconWaveW2300Deep);
      break;
    case W2300RateMode::Ultra:
      flags = static_cast<std::uint16_t> (flags | BeaconWaveW2300Ultra);
      break;
    }

  if (capabilities.preferredProfile == Profile::Wide500)
    {
      flags = static_cast<std::uint16_t> (flags | BeaconWavePreferredW500);
    }
  else if (capabilities.preferredProfile == Profile::Wide2300)
    {
      flags = static_cast<std::uint16_t> (flags | BeaconWavePreferredW2300);
    }
  return flags;
}

std::uint16_t defaultBeaconServiceCapabilityFlags ()
{
  return static_cast<std::uint16_t> (
      BeaconServiceChat | BeaconServiceMail | BeaconServiceForm
      | BeaconServiceFile | BeaconServiceBbs | BeaconServiceBroadcast
      | BeaconServiceInfo | BeaconServiceQsy | BeaconServicePath
      | BeaconServicePing | BeaconServiceRelay | BeaconServiceAlert);
}

std::string beaconWaveformCapabilitySummary (std::uint16_t flags)
{
  std::ostringstream stream;
  appendSummaryToken (&stream, "W2300", (flags & BeaconWaveW2300) != 0u);
  appendSummaryToken (&stream, "W500", (flags & BeaconWaveW500) != 0u);
  appendSummaryToken (&stream, "FAST", (flags & BeaconWaveW2300Fast) != 0u);
  appendSummaryToken (&stream, "ROBUST", (flags & BeaconWaveW2300Robust) != 0u);
  appendSummaryToken (&stream, "WEAK", (flags & BeaconWaveW2300Weak) != 0u);
  appendSummaryToken (&stream, "DEEP", (flags & BeaconWaveW2300Deep) != 0u);
  appendSummaryToken (&stream, "ULTRA", (flags & BeaconWaveW2300Ultra) != 0u);
  return stream.str ();
}

std::string beaconServiceCapabilitySummary (std::uint16_t flags)
{
  std::ostringstream stream;
  appendSummaryToken (&stream, "CHAT", (flags & BeaconServiceChat) != 0u);
  appendSummaryToken (&stream, "MAIL", (flags & BeaconServiceMail) != 0u);
  appendSummaryToken (&stream, "FORM", (flags & BeaconServiceForm) != 0u);
  appendSummaryToken (&stream, "FILE", (flags & BeaconServiceFile) != 0u);
  appendSummaryToken (&stream, "BBS", (flags & BeaconServiceBbs) != 0u);
  appendSummaryToken (&stream, "BCAST", (flags & BeaconServiceBroadcast) != 0u);
  appendSummaryToken (&stream, "INFO", (flags & BeaconServiceInfo) != 0u);
  appendSummaryToken (&stream, "QSY", (flags & BeaconServiceQsy) != 0u);
  appendSummaryToken (&stream, "PATH", (flags & BeaconServicePath) != 0u);
  appendSummaryToken (&stream, "PING", (flags & BeaconServicePing) != 0u);
  appendSummaryToken (&stream, "RLY", (flags & BeaconServiceRelay) != 0u);
  appendSummaryToken (&stream, "ALERT", (flags & BeaconServiceAlert) != 0u);
  return stream.str ();
}

FT2LinkAppModel::FT2LinkAppModel (StationIdentity const& local)
  : m_local {local}
{
}

void FT2LinkAppModel::setLocalStation (StationIdentity const& station)
{
  m_local = station;
}

StationIdentity const& FT2LinkAppModel::localStation () const
{
  return m_local;
}

void FT2LinkAppModel::setLocalCapabilities (
    LinkCapabilities const& capabilities)
{
  m_localCapabilities = capabilities;
}

LinkCapabilities const& FT2LinkAppModel::localCapabilities () const
{
  return m_localCapabilities;
}

void FT2LinkAppModel::setNextSessionId (std::uint16_t nextSessionId)
{
  m_nextSessionId = nextSessionId == 0u ? 1u : nextSessionId;
}

StationAdvertisement FT2LinkAppModel::makeLocalAdvertisement (
    std::uint64_t nowMs,
    bool cq,
    int cqSlotId,
    int cqSlotSizeHz,
    std::string const& cqType,
    std::string const& cqLocator) const
{
  StationAdvertisement advertisement;
  advertisement.station = m_local;
  advertisement.capabilities = m_localCapabilities;
  advertisement.waveformCapabilityFlags =
      beaconWaveformCapabilityFlags (m_localCapabilities);
  advertisement.serviceCapabilityFlags = defaultBeaconServiceCapabilityFlags ();
  advertisement.cq = cq;
  advertisement.cqType = cq ? normalizeCqType (cqType) : std::string {};
  advertisement.cqLocator = cq
      ? (normalizeLocator (cqLocator).empty ()
         ? normalizeLocator (m_local.locator)
         : normalizeLocator (cqLocator))
      : std::string {};
  advertisement.cqSlotId = cq ? normalizeSlotId (cqSlotId) : 0;
  advertisement.cqSlotSizeHz = normalizeSlotSizeHz (cqSlotSizeHz);
  advertisement.cqSlotOffsetHz =
      advertisement.cqSlotId * advertisement.cqSlotSizeHz;
  advertisement.heardAtMs = nowMs;
  return advertisement;
}

Frame FT2LinkAppModel::makeLocalBeaconFrame (bool cq,
                                             int cqSlotId,
                                             int cqSlotSizeHz,
                                             std::string const& cqType,
                                             std::string const& cqLocator) const
{
  StationIdentity identity = m_local;
  if (cq)
    {
      std::string const locator = normalizeLocator (cqLocator);
      if (!locator.empty ())
        {
          identity.locator = locator;
        }
    }
  Frame frame = makeBeaconFrame (
      m_localCapabilities, handshakeIdentityFromStation (identity), cq);
  frame.sessionId = beaconWaveformCapabilityFlags (m_localCapabilities);
  frame.ackBitmap = defaultBeaconServiceCapabilityFlags ();
  int const normalizedSlotId = cq ? normalizeSlotId (cqSlotId) : 0;
  if (cq)
    {
      frame.ackBitmap = static_cast<std::uint16_t> (
          frame.ackBitmap | encodeCqType (cqType));
    }
  if (normalizedSlotId != 0)
    {
      frame.sequence = encodeSignedSlotId (normalizedSlotId);
      frame.ackBase = static_cast<std::uint16_t> (
          normalizeSlotSizeHz (cqSlotSizeHz));
    }
  return frame;
}

bool FT2LinkAppModel::observeStation (StationAdvertisement const& advertisement,
                                      std::string* error)
{
  if (advertisement.station.call.empty ())
    {
      setError (error, "FT2-Link station advertisement requires a callsign");
      return false;
    }
  if (!advertisement.capabilities.supportsW500
      && !advertisement.capabilities.supportsW2300)
    {
      setError (error, "FT2-Link station has no supported wide profile");
      return false;
    }

  m_stations[advertisement.station.call] = advertisement;
  return true;
}

bool FT2LinkAppModel::observeBeacon (Frame const& beacon,
                                     std::uint64_t nowMs,
                                     StationAdvertisement* advertisement,
                                     std::string* error)
{
  LinkCapabilities capabilities;
  HandshakeIdentity identity;
  bool cq = false;
  if (!parseBeaconFrame (beacon, &capabilities, &identity, &cq, error))
    {
      return false;
    }
  if (identity.call.empty ())
    {
      setError (error, "FT2-Link BEACON requires a callsign");
      return false;
    }

  StationAdvertisement observed;
  observed.station.call = identity.call;
  observed.station.locator = identity.locator;
  observed.capabilities = capabilities;
  observed.waveformCapabilityFlags = beacon.sessionId == 0u
      ? beaconWaveformCapabilityFlags (capabilities)
      : beacon.sessionId;
  observed.serviceCapabilityFlags = static_cast<std::uint16_t> (
      beacon.ackBitmap & 0xfff0u);
  observed.cq = cq;
  if (cq)
    {
      observed.cqType = decodeCqType (beacon.ackBitmap);
      observed.cqLocator = identity.locator;
      observed.cqSlotId = decodeSignedSlotId (beacon.sequence);
      observed.cqSlotSizeHz = beacon.ackBase == 0u
          ? 750
          : normalizeSlotSizeHz (static_cast<int> (beacon.ackBase));
      observed.cqSlotOffsetHz = observed.cqSlotId * observed.cqSlotSizeHz;
    }
  observed.heardAtMs = nowMs;
  if (advertisement)
    {
      *advertisement = observed;
    }
  if (observed.station.call == m_local.call)
    {
      return true;
    }
  return observeStation (observed, error);
}

std::vector<StationAdvertisement> FT2LinkAppModel::activeStations (
    std::uint64_t nowMs,
    std::uint64_t maxAgeMs,
    bool cqOnly) const
{
  std::vector<StationAdvertisement> active;
  for (std::map<std::string, StationAdvertisement>::const_iterator it =
           m_stations.begin ();
       it != m_stations.end ();
       ++it)
    {
      if (!isActiveAt (it->second, nowMs, maxAgeMs))
        {
          continue;
        }
      if (cqOnly && !it->second.cq)
        {
          continue;
        }
      active.push_back (it->second);
    }

  std::sort (active.begin (), active.end (),
             [] (StationAdvertisement const& lhs,
                 StationAdvertisement const& rhs) {
               if (lhs.heardAtMs != rhs.heardAtMs)
                 {
                   return lhs.heardAtMs > rhs.heardAtMs;
                 }
               return lhs.station.call < rhs.station.call;
             });
  return active;
}

bool FT2LinkAppModel::hasActiveStation (std::string const& call,
                                        std::uint64_t nowMs,
                                        std::uint64_t maxAgeMs) const
{
  std::map<std::string, StationAdvertisement>::const_iterator it =
      m_stations.find (call);
  return it != m_stations.end () && isActiveAt (it->second, nowMs, maxAgeMs);
}

StationAdvertisement const* FT2LinkAppModel::stationAdvertisement (
    std::string const& call) const
{
  std::map<std::string, StationAdvertisement>::const_iterator it =
      m_stations.find (call);
  if (it != m_stations.end ())
    {
      return &it->second;
    }

  std::string const normalized = uppercaseAscii (call);
  if (normalized != call)
    {
      it = m_stations.find (normalized);
      if (it != m_stations.end ())
        {
          return &it->second;
        }
    }
  return nullptr;
}

Frame FT2LinkAppModel::startSession (std::string const& remoteCall,
                                     std::uint64_t nowMs,
                                     std::string* error)
{
  Frame empty;
  if (remoteCall.empty ())
    {
      setError (error, "FT2-Link session requires a remote callsign");
      return empty;
    }

  std::map<std::string, StationAdvertisement>::const_iterator station =
      m_stations.find (remoteCall);
  if (station == m_stations.end ())
    {
      setError (error, "FT2-Link remote station is not known");
      return empty;
    }

  std::uint16_t const sessionId = allocateSessionId ();
  Frame hello = makeHelloFrame (
      sessionId, m_localCapabilities, handshakeIdentityFromStation (m_local));

  AppSession session;
  session.sessionId = sessionId;
  session.remoteCall = remoteCall;
  session.state = AppSessionState::Calling;
  session.remoteCapabilities = station->second.capabilities;
  session.openedAtMs = nowMs;
  session.updatedAtMs = nowMs;
  m_sessions[sessionId] = session;
  return hello;
}

bool FT2LinkAppModel::receiveHelloAck (Frame const& helloAck,
                                       std::uint64_t nowMs,
                                       std::string* error)
{
  AppSession* current = mutableSession (helloAck.sessionId);
  if (!current)
    {
      setError (error, "FT2-Link HELLO_ACK references an unknown session");
      return false;
    }

  LinkCapabilities remote;
  HandshakeIdentity remoteIdentity;
  NegotiatedLink negotiated;
  if (!parseHelloAckFrame (helloAck, &remote, &negotiated, &remoteIdentity, error))
    {
      return false;
    }

  current->remoteCapabilities = remote;
  current->negotiated = negotiated;
  current->state = negotiated.accepted
      ? AppSessionState::Connected
      : AppSessionState::Rejected;
  current->updatedAtMs = nowMs;
  if (!negotiated.accepted)
    {
      setError (error, "FT2-Link HELLO_ACK rejected the session");
    }
  return negotiated.accepted;
}

bool FT2LinkAppModel::answerHello (std::string const& remoteCall,
                                   Frame const& hello,
                                   std::uint64_t nowMs,
                                   Frame* helloAck,
                                   std::string* error)
{
  if (!helloAck)
    {
      setError (error, "missing FT2-Link HELLO_ACK output");
      return false;
    }
  LinkCapabilities remote;
  HandshakeIdentity remoteIdentity;
  std::string parseError;
  if (!parseHelloFrame (hello, &remote, &remoteIdentity, &parseError))
    {
      setError (error, parseError.c_str ());
      return false;
    }
  std::string resolvedRemoteCall = remoteCall.empty ()
      ? remoteIdentity.call
      : remoteCall;
  if (resolvedRemoteCall.empty ())
    {
      setError (error, "FT2-Link incoming session requires a remote callsign");
      return false;
    }

  NegotiatedLink negotiated;
  bool const accepted = answerHelloFrame (
      hello, m_localCapabilities, helloAck, &negotiated, error);
  if (helloAck)
    {
      *helloAck = makeHelloAckFrame (
          hello.sessionId,
          m_localCapabilities,
          negotiated,
          handshakeIdentityFromStation (m_local));
    }

  AppSession session;
  session.sessionId = hello.sessionId;
  session.remoteCall = resolvedRemoteCall;
  session.state = accepted ? AppSessionState::Connected : AppSessionState::Rejected;
  session.remoteCapabilities = remote;
  session.negotiated = negotiated;
  session.openedAtMs = nowMs;
  session.updatedAtMs = nowMs;
  m_sessions[session.sessionId] = session;
  return accepted;
}

bool FT2LinkAppModel::rejectHello (std::string const& remoteCall,
                                   Frame const& hello,
                                   std::uint64_t nowMs,
                                   Frame* helloAck,
                                   char const* reason,
                                   std::string* error)
{
  if (!helloAck)
    {
      setError (error, "missing FT2-Link HELLO_ACK output");
      return false;
    }

  LinkCapabilities remote;
  HandshakeIdentity remoteIdentity;
  std::string parseError;
  if (!parseHelloFrame (hello, &remote, &remoteIdentity, &parseError))
    {
      setError (error, parseError.c_str ());
      return false;
    }

  std::string resolvedRemoteCall = remoteCall.empty ()
      ? remoteIdentity.call
      : remoteCall;
  if (resolvedRemoteCall.empty ())
    {
      setError (error, "FT2-Link incoming session requires a remote callsign");
      return false;
    }

  NegotiatedLink negotiated;
  negotiated.accepted = false;
  negotiated.profile = Profile::Narrow;
  negotiated.w2300RateMode = W2300RateMode::Fast;
  *helloAck = makeHelloAckFrame (
      hello.sessionId,
      m_localCapabilities,
      negotiated,
      handshakeIdentityFromStation (m_local));

  AppSession session;
  session.sessionId = hello.sessionId;
  session.remoteCall = resolvedRemoteCall;
  session.state = AppSessionState::Rejected;
  session.remoteCapabilities = remote;
  session.negotiated = negotiated;
  session.openedAtMs = nowMs;
  session.updatedAtMs = nowMs;
  m_sessions[session.sessionId] = session;
  setError (error, reason && *reason ? reason : "FT2-Link handshake rejected");
  return true;
}

bool FT2LinkAppModel::queueOutgoingText (std::uint16_t sessionId,
                                         std::string const& text,
                                         std::uint64_t nowMs,
                                         std::string* error)
{
  AppSession* current = mutableSession (sessionId);
  if (!current)
    {
      setError (error, "FT2-Link outgoing message references an unknown session");
      return false;
    }
  if (current->state != AppSessionState::Connected)
    {
      setError (error, "FT2-Link outgoing message requires a connected session");
      return false;
    }
  if (text.empty ())
    {
      setError (error, "FT2-Link outgoing message is empty");
      return false;
    }

  ChatMessage message;
  message.direction = ChatMessageDirection::Outgoing;
  message.delivery = ChatDeliveryState::Pending;
  message.atMs = nowMs;
  message.text = text;
  current->messages.push_back (message);
  current->updatedAtMs = nowMs;
  return true;
}

bool FT2LinkAppModel::markOutgoingDelivered (std::uint16_t sessionId,
                                             std::size_t messageIndex,
                                             std::uint64_t nowMs,
                                             std::string* error)
{
  AppSession* current = mutableSession (sessionId);
  if (!current || messageIndex >= current->messages.size ())
    {
      setError (error, "FT2-Link delivered message index is invalid");
      return false;
    }
  ChatMessage& message = current->messages[messageIndex];
  if (message.direction != ChatMessageDirection::Outgoing)
    {
      setError (error, "FT2-Link delivered message must be outgoing");
      return false;
    }

  message.delivery = ChatDeliveryState::Delivered;
  current->updatedAtMs = nowMs;
  return true;
}

bool FT2LinkAppModel::markOutgoingFailed (std::uint16_t sessionId,
                                          std::size_t messageIndex,
                                          std::uint64_t nowMs,
                                          std::string* error)
{
  AppSession* current = mutableSession (sessionId);
  if (!current || messageIndex >= current->messages.size ())
    {
      setError (error, "FT2-Link failed message index is invalid");
      return false;
    }
  ChatMessage& message = current->messages[messageIndex];
  if (message.direction != ChatMessageDirection::Outgoing)
    {
      setError (error, "FT2-Link failed message must be outgoing");
      return false;
    }

  message.delivery = ChatDeliveryState::Failed;
  current->updatedAtMs = nowMs;
  return true;
}

bool FT2LinkAppModel::appendIncomingText (std::uint16_t sessionId,
                                          std::string const& text,
                                          std::uint64_t nowMs,
                                          std::string* error)
{
  AppSession* current = mutableSession (sessionId);
  if (!current)
    {
      setError (error, "FT2-Link incoming message references an unknown session");
      return false;
    }
  if (current->state != AppSessionState::Connected)
    {
      setError (error, "FT2-Link incoming message requires a connected session");
      return false;
    }
  if (text.empty ())
    {
      setError (error, "FT2-Link incoming message is empty");
      return false;
    }

  ChatMessage message;
  message.direction = ChatMessageDirection::Incoming;
  message.delivery = ChatDeliveryState::Received;
  message.atMs = nowMs;
  message.text = text;
  current->messages.push_back (message);
  current->updatedAtMs = nowMs;
  return true;
}

bool FT2LinkAppModel::appendSystemText (std::uint16_t sessionId,
                                        std::string const& text,
                                        std::uint64_t nowMs,
                                        std::string* error)
{
  AppSession* current = mutableSession (sessionId);
  if (!current)
    {
      setError (error, "FT2-Link system message references an unknown session");
      return false;
    }
  if (text.empty ())
    {
      setError (error, "FT2-Link system message is empty");
      return false;
    }

  ChatMessage message;
  message.direction = ChatMessageDirection::System;
  message.delivery = ChatDeliveryState::Received;
  message.atMs = nowMs;
  message.text = text;
  current->messages.push_back (message);
  current->updatedAtMs = nowMs;
  return true;
}

bool FT2LinkAppModel::closeSession (std::uint16_t sessionId,
                                    std::uint64_t nowMs,
                                    std::string* error)
{
  AppSession* current = mutableSession (sessionId);
  if (!current)
    {
      setError (error, "FT2-Link close references an unknown session");
      return false;
    }

  current->state = AppSessionState::Closed;
  current->updatedAtMs = nowMs;
  return true;
}

AppSession const* FT2LinkAppModel::session (std::uint16_t sessionId) const
{
  std::map<std::uint16_t, AppSession>::const_iterator it =
      m_sessions.find (sessionId);
  return it == m_sessions.end () ? nullptr : &it->second;
}

std::vector<AppSession> FT2LinkAppModel::sessions () const
{
  std::vector<AppSession> out;
  out.reserve (m_sessions.size ());
  for (std::map<std::uint16_t, AppSession>::const_iterator it =
           m_sessions.begin ();
       it != m_sessions.end ();
       ++it)
    {
      out.push_back (it->second);
    }
  return out;
}

AppSession* FT2LinkAppModel::mutableSession (std::uint16_t sessionId)
{
  std::map<std::uint16_t, AppSession>::iterator it = m_sessions.find (sessionId);
  return it == m_sessions.end () ? nullptr : &it->second;
}

std::uint16_t FT2LinkAppModel::allocateSessionId ()
{
  std::uint16_t candidate = m_nextSessionId;
  for (;;)
    {
      if (candidate == 0u)
        {
          candidate = 1u;
        }
      if (m_sessions.find (candidate) == m_sessions.end ())
        {
          m_nextSessionId = static_cast<std::uint16_t> (candidate + 1u);
          if (m_nextSessionId == 0u)
            {
              m_nextSessionId = 1u;
            }
          return candidate;
        }
      candidate = static_cast<std::uint16_t> (candidate + 1u);
    }
}

}
}
