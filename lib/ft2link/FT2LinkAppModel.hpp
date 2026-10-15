#ifndef DECODIUM_FT2LINK_APP_MODEL_HPP
#define DECODIUM_FT2LINK_APP_MODEL_HPP

#include "lib/ft2link/FT2LinkHandshake.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace decodium
{
namespace ft2link
{

struct StationIdentity
{
  std::string call;
  std::string locator;
  std::string name;
};

struct StationAdvertisement
{
  StationIdentity station;
  LinkCapabilities capabilities;
  std::uint16_t waveformCapabilityFlags {0};
  std::uint16_t serviceCapabilityFlags {0};
  bool cq {false};
  std::string cqType;
  std::string cqLocator;
  int cqSlotId {0};
  int cqSlotOffsetHz {0};
  int cqSlotSizeHz {750};
  std::uint64_t heardAtMs {0};
};

enum BeaconWaveformCapabilityFlags : std::uint16_t
{
  BeaconWaveW500 = 0x0001,
  BeaconWaveW2300 = 0x0002,
  BeaconWaveW2300Fast = 0x0004,
  BeaconWaveW2300Robust = 0x0008,
  BeaconWaveW2300Weak = 0x0010,
  BeaconWaveW2300Deep = 0x0020,
  BeaconWaveW2300Ultra = 0x0040,
  BeaconWavePreferredW500 = 0x0100,
  BeaconWavePreferredW2300 = 0x0200
};

enum BeaconServiceCapabilityFlags : std::uint16_t
{
  BeaconServiceChat = 0x0010,
  BeaconServiceMail = 0x0020,
  BeaconServiceForm = 0x0040,
  BeaconServiceFile = 0x0080,
  BeaconServiceBbs = 0x0100,
  BeaconServiceBroadcast = 0x0200,
  BeaconServiceInfo = 0x0400,
  BeaconServiceQsy = 0x0800,
  BeaconServicePath = 0x1000,
  BeaconServicePing = 0x2000,
  BeaconServiceRelay = 0x4000,
  BeaconServiceAlert = 0x8000
};

std::uint16_t beaconWaveformCapabilityFlags (
    LinkCapabilities const& capabilities);
std::uint16_t defaultBeaconServiceCapabilityFlags ();
std::string beaconWaveformCapabilitySummary (std::uint16_t flags);
std::string beaconServiceCapabilitySummary (std::uint16_t flags);

enum class AppSessionState
{
  Calling,
  Connected,
  Rejected,
  Closed
};

enum class ChatMessageDirection
{
  Outgoing,
  Incoming,
  System
};

enum class ChatDeliveryState
{
  Pending,
  Delivered,
  Received,
  Failed
};

struct ChatMessage
{
  ChatMessageDirection direction {ChatMessageDirection::System};
  ChatDeliveryState delivery {ChatDeliveryState::Pending};
  std::uint64_t atMs {0};
  std::string text;
};

struct AppSession
{
  std::uint16_t sessionId {0};
  std::string remoteCall;
  AppSessionState state {AppSessionState::Calling};
  LinkCapabilities remoteCapabilities;
  NegotiatedLink negotiated;
  std::uint64_t openedAtMs {0};
  std::uint64_t updatedAtMs {0};
  std::vector<ChatMessage> messages;
};

class FT2LinkAppModel
{
public:
  explicit FT2LinkAppModel (StationIdentity const& local = StationIdentity {});

  void setLocalStation (StationIdentity const& station);
  StationIdentity const& localStation () const;

  void setLocalCapabilities (LinkCapabilities const& capabilities);
  LinkCapabilities const& localCapabilities () const;

  void setNextSessionId (std::uint16_t nextSessionId);

  StationAdvertisement makeLocalAdvertisement (std::uint64_t nowMs,
                                               bool cq,
                                               int cqSlotId = 0,
                                               int cqSlotSizeHz = 750,
                                               std::string const& cqType = {},
                                               std::string const& cqLocator = {}) const;
  Frame makeLocalBeaconFrame (bool cq,
                              int cqSlotId = 0,
                              int cqSlotSizeHz = 750,
                              std::string const& cqType = {},
                              std::string const& cqLocator = {}) const;
  bool observeStation (StationAdvertisement const& advertisement,
                       std::string* error = nullptr);
  bool observeBeacon (Frame const& beacon,
                      std::uint64_t nowMs,
                      StationAdvertisement* advertisement = nullptr,
                      std::string* error = nullptr);
  std::vector<StationAdvertisement> activeStations (
      std::uint64_t nowMs,
      std::uint64_t maxAgeMs,
      bool cqOnly = false) const;
  bool hasActiveStation (std::string const& call,
                         std::uint64_t nowMs,
                         std::uint64_t maxAgeMs) const;
  StationAdvertisement const* stationAdvertisement (
      std::string const& call) const;

  Frame startSession (std::string const& remoteCall,
                      std::uint64_t nowMs,
                      std::string* error = nullptr);
  bool receiveHelloAck (Frame const& helloAck,
                        std::uint64_t nowMs,
                        std::string* error = nullptr);
  bool answerHello (std::string const& remoteCall,
                    Frame const& hello,
                    std::uint64_t nowMs,
                    Frame* helloAck,
                    std::string* error = nullptr);
  bool rejectHello (std::string const& remoteCall,
                    Frame const& hello,
                    std::uint64_t nowMs,
                    Frame* helloAck,
                    char const* reason,
                    std::string* error = nullptr);

  bool queueOutgoingText (std::uint16_t sessionId,
                          std::string const& text,
                          std::uint64_t nowMs,
                          std::string* error = nullptr);
  bool markOutgoingDelivered (std::uint16_t sessionId,
                              std::size_t messageIndex,
                              std::uint64_t nowMs,
                              std::string* error = nullptr);
  bool markOutgoingFailed (std::uint16_t sessionId,
                           std::size_t messageIndex,
                           std::uint64_t nowMs,
                           std::string* error = nullptr);
  bool appendIncomingText (std::uint16_t sessionId,
                           std::string const& text,
                           std::uint64_t nowMs,
                           std::string* error = nullptr);
  bool appendSystemText (std::uint16_t sessionId,
                         std::string const& text,
                         std::uint64_t nowMs,
                         std::string* error = nullptr);
  bool closeSession (std::uint16_t sessionId,
                     std::uint64_t nowMs,
                     std::string* error = nullptr);

  AppSession const* session (std::uint16_t sessionId) const;
  std::vector<AppSession> sessions () const;

private:
  AppSession* mutableSession (std::uint16_t sessionId);
  std::uint16_t allocateSessionId ();

  StationIdentity m_local;
  LinkCapabilities m_localCapabilities;
  std::uint16_t m_nextSessionId {0x7000u};
  std::map<std::string, StationAdvertisement> m_stations;
  std::map<std::uint16_t, AppSession> m_sessions;
};

}
}

#endif
