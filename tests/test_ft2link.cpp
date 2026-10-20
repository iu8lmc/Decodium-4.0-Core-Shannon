#include <QtTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "lib/ft2link/FT2LinkAppModel.hpp"
#include "lib/ft2link/FT2LinkAudio.hpp"
#include "lib/ft2link/FT2LinkFrame.hpp"
#include "lib/ft2link/FT2LinkHandshake.hpp"
#include "lib/ft2link/FT2LinkOffline.hpp"
#include "lib/ft2link/FT2LinkPhysical.hpp"
#include "lib/ft2link/FT2LinkSession.hpp"
#include "lib/ft2link/FT2LinkWaveform.hpp"

namespace
{
std::vector<std::uint8_t> bytesFromString (std::string const& text)
{
  return std::vector<std::uint8_t> (text.begin (), text.end ());
}

std::vector<float> paddedWave (std::vector<float> const& wave,
                               std::size_t leadingSamples,
                               std::size_t trailingSamples)
{
  std::vector<float> out (leadingSamples, 0.0f);
  out.insert (out.end (), wave.begin (), wave.end ());
  out.insert (out.end (), trailingSamples, 0.0f);
  return out;
}

void addDeterministicNoise (std::vector<float>& wave, float amplitude)
{
  std::uint32_t state = 0xdec0d10u;
  for (float& sample : wave)
    {
      state = state * 1664525u + 1013904223u;
      float const unit = float ((state >> 8) & 0xffffu) / 32767.5f - 1.0f;
      sample += amplitude * unit;
    }
}

std::vector<float> fil4Decimate48kTo12k (std::vector<float> const& wave)
{
  static constexpr std::array<float, 49> weights {{
      0.000861074040f, 0.010051920210f, 0.010161983649f, 0.011363155076f,
      0.008706594219f, 0.002613872664f, -0.005202883094f, -0.011720748164f,
      -0.013752163325f, -0.009431602741f, 0.000539063909f, 0.012636767098f,
      0.021494659597f, 0.021951235065f, 0.011564169382f, -0.007656470131f,
      -0.028965787341f, -0.042637874109f, -0.039203309748f, -0.013153301537f,
      0.034320769178f, 0.094717832646f, 0.154224604789f, 0.197758325022f,
      0.213715139513f, 0.197758325022f, 0.154224604789f, 0.094717832646f,
      0.034320769178f, -0.013153301537f, -0.039203309748f, -0.042637874109f,
      -0.028965787341f, -0.007656470131f, 0.011564169382f, 0.021951235065f,
      0.021494659597f, 0.012636767098f, 0.000539063909f, -0.009431602741f,
      -0.013752163325f, -0.011720748164f, -0.005202883094f, 0.002613872664f,
      0.008706594219f, 0.011363155076f, 0.010161983649f, 0.010051920210f,
      0.000861074040f
  }};

  std::array<float, 49> delay {};
  std::vector<float> decimated;
  decimated.reserve (wave.size () / 4u);
  for (std::size_t index = 0; index + 4u <= wave.size (); index += 4u)
    {
      for (std::size_t i = 0; i < delay.size () - 4u; ++i)
        {
          delay[i] = delay[i + 4u];
        }
      for (std::size_t i = 0; i < 4u; ++i)
        {
          delay[delay.size () - 4u + i] = wave[index + i];
        }

      float sum = 0.0f;
      for (std::size_t i = 0; i < delay.size (); ++i)
        {
          sum += weights[i] * delay[i];
        }
      decimated.push_back (sum);
    }
  return decimated;
}
}

class TestFt2Link
  : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void frameRoundTripPreservesFields ()
  {
    decodium::ft2link::Frame frame;
    frame.type = decodium::ft2link::FrameType::Hello;
    frame.profile = decodium::ft2link::Profile::Wide2300;
    frame.sessionId = 0x1234u;
    frame.sequence = 7u;
    frame.ackBase = 3u;
    frame.ackBitmap = 0x0005u;
    frame.payload = bytesFromString ("W500,W2300");

    std::vector<std::uint8_t> const wire = decodium::ft2link::serializeFrame (frame);
    QVERIFY (!wire.empty ());

    decodium::ft2link::Frame parsed;
    std::string error;
    QVERIFY2 (decodium::ft2link::parseFrame (wire, &parsed, &error), error.c_str ());
    QCOMPARE (static_cast<int> (parsed.type), static_cast<int> (frame.type));
    QCOMPARE (static_cast<int> (parsed.profile), static_cast<int> (frame.profile));
    QCOMPARE (parsed.sessionId, frame.sessionId);
    QCOMPARE (parsed.sequence, frame.sequence);
    QCOMPARE (parsed.ackBase, frame.ackBase);
    QCOMPARE (parsed.ackBitmap, frame.ackBitmap);
    QVERIFY (parsed.payload == frame.payload);
  }

  void frameParserRejectsCrcDamage ()
  {
    decodium::ft2link::Frame frame;
    frame.type = decodium::ft2link::FrameType::Data;
    frame.profile = decodium::ft2link::Profile::Wide500;
    frame.sessionId = 0x2222u;
    frame.payload = bytesFromString ("hello");

    std::vector<std::uint8_t> wire = decodium::ft2link::serializeFrame (frame);
    QVERIFY (!wire.empty ());
    wire[wire.size () - 3] = static_cast<std::uint8_t> (wire[wire.size () - 3] ^ 0x55u);

    decodium::ft2link::Frame parsed;
    std::string error;
    QVERIFY (!decodium::ft2link::parseFrame (wire, &parsed, &error));
    QVERIFY (error.find ("CRC") != std::string::npos);
  }

  void profileCapacitiesModelWideModes ()
  {
    using decodium::ft2link::Profile;
    QVERIFY (decodium::ft2link::profilePayloadCapacity (Profile::Wide500)
             > decodium::ft2link::profilePayloadCapacity (Profile::Narrow));
    QVERIFY (decodium::ft2link::profilePayloadCapacity (Profile::Wide2300)
             > decodium::ft2link::profilePayloadCapacity (Profile::Wide500));
    QVERIFY (decodium::ft2link::profileName (Profile::Wide2300) == std::string ("W2300"));
  }

  void broadcastFrameRoundTrip ()
  {
    decodium::ft2link::Frame frame;
    frame.type = decodium::ft2link::FrameType::Broadcast;
    frame.profile = decodium::ft2link::Profile::Narrow;
    frame.flags = decodium::ft2link::FlagEndOfMessage;
    frame.payload = bytesFromString ("SOS CHECK NET");

    std::vector<std::uint8_t> const wire = decodium::ft2link::serializeFrame (frame);
    QVERIFY (!wire.empty ());

    decodium::ft2link::Frame parsed;
    std::string error;
    QVERIFY2 (decodium::ft2link::parseFrame (wire, &parsed, &error), error.c_str ());
    QCOMPARE (static_cast<int> (parsed.type),
              static_cast<int> (decodium::ft2link::FrameType::Broadcast));
    QCOMPARE (static_cast<int> (parsed.profile),
              static_cast<int> (decodium::ft2link::Profile::Narrow));
    QCOMPARE (decodium::ft2link::frameTypeName (parsed.type),
              std::string ("BROADCAST"));
    QVERIFY (parsed.payload == frame.payload);
  }

  void pingControlFramesRoundTrip ()
  {
    decodium::ft2link::Frame ping;
    ping.type = decodium::ft2link::FrameType::Ping;
    ping.profile = decodium::ft2link::Profile::Narrow;
    ping.flags = decodium::ft2link::FlagEndOfMessage;
    ping.sequence = 0x1234u;
    ping.payload = bytesFromString ("IU8LMC");

    std::vector<std::uint8_t> const pingWire =
        decodium::ft2link::serializeFrame (ping);
    QVERIFY (!pingWire.empty ());

    decodium::ft2link::Frame parsedPing;
    std::string error;
    QVERIFY2 (decodium::ft2link::parseFrame (
                  pingWire, &parsedPing, &error),
              error.c_str ());
    QCOMPARE (static_cast<int> (parsedPing.type),
              static_cast<int> (decodium::ft2link::FrameType::Ping));
    QCOMPARE (static_cast<int> (parsedPing.profile),
              static_cast<int> (decodium::ft2link::Profile::Narrow));
    QCOMPARE (parsedPing.sequence, ping.sequence);
    QCOMPARE (decodium::ft2link::frameTypeName (parsedPing.type),
              std::string ("PING"));
    QVERIFY (parsedPing.payload == ping.payload);

    decodium::ft2link::Frame pingAck = ping;
    pingAck.type = decodium::ft2link::FrameType::PingAck;
    pingAck.payload = bytesFromString ("K1ABC");
    std::vector<std::uint8_t> const ackWire =
        decodium::ft2link::serializeFrame (pingAck);
    QVERIFY (!ackWire.empty ());

    decodium::ft2link::Frame parsedAck;
    QVERIFY2 (decodium::ft2link::parseFrame (
                  ackWire, &parsedAck, &error),
              error.c_str ());
    QCOMPARE (static_cast<int> (parsedAck.type),
              static_cast<int> (decodium::ft2link::FrameType::PingAck));
    QCOMPARE (parsedAck.sequence, ping.sequence);
    QCOMPARE (decodium::ft2link::frameTypeName (parsedAck.type),
              std::string ("PING_ACK"));
    QVERIFY (parsedAck.payload == pingAck.payload);
  }

  void appModelTracksCqStationsWithExpiry ()
  {
    using decodium::ft2link::FT2LinkAppModel;
    using decodium::ft2link::LinkCapabilities;
    using decodium::ft2link::Profile;
    using decodium::ft2link::StationAdvertisement;
    using decodium::ft2link::StationIdentity;

    FT2LinkAppModel model {StationIdentity {"IU8LMC", "JN70", "Salvo"}};

    StationAdvertisement cq;
    cq.station.call = "K1ABC";
    cq.station.locator = "FN42";
    cq.capabilities.preferredProfile = Profile::Wide2300;
    cq.cq = true;
    cq.heardAtMs = 1000;
    QVERIFY (model.observeStation (cq));

    StationAdvertisement idle;
    idle.station.call = "G4XYZ";
    idle.station.locator = "IO91";
    idle.capabilities = LinkCapabilities {};
    idle.cq = false;
    idle.heardAtMs = 1500;
    QVERIFY (model.observeStation (idle));

    std::vector<StationAdvertisement> active = model.activeStations (1600, 1000);
    QCOMPARE (active.size (), static_cast<std::size_t> (2));
    QCOMPARE (active[0].station.call, std::string ("G4XYZ"));
    QCOMPARE (active[1].station.call, std::string ("K1ABC"));

    std::vector<StationAdvertisement> cqOnly = model.activeStations (1600, 1000, true);
    QCOMPARE (cqOnly.size (), static_cast<std::size_t> (1));
    QCOMPARE (cqOnly[0].station.call, std::string ("K1ABC"));
    QVERIFY (model.hasActiveStation ("K1ABC", 1600, 1000));
    QVERIFY (!model.hasActiveStation ("K1ABC", 2501, 1000));
  }

  void appModelNegotiatesSessionAndLogsChat ()
  {
    using decodium::ft2link::AppSession;
    using decodium::ft2link::AppSessionState;
    using decodium::ft2link::ChatDeliveryState;
    using decodium::ft2link::ChatMessageDirection;
    using decodium::ft2link::FT2LinkAppModel;
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::LinkCapabilities;
    using decodium::ft2link::Profile;
    using decodium::ft2link::StationAdvertisement;
    using decodium::ft2link::StationIdentity;
    using decodium::ft2link::W2300RateMode;

    FT2LinkAppModel caller {StationIdentity {"IU8LMC", "JN70", "Salvo"}};
    caller.setNextSessionId (0x7100u);

    FT2LinkAppModel answerer {StationIdentity {"K1ABC", "FN42", "Ann"}};
    LinkCapabilities answererCaps;
    answererCaps.preferredProfile = Profile::Wide2300;
    answererCaps.preferredW2300RateMode = W2300RateMode::Robust;
    answerer.setLocalCapabilities (answererCaps);

    StationAdvertisement remote = answerer.makeLocalAdvertisement (1000, true);
    QVERIFY (caller.observeStation (remote));

    std::string error;
    Frame hello = caller.startSession ("K1ABC", 1100, &error);
    QCOMPARE (static_cast<int> (hello.type), static_cast<int> (FrameType::Hello));
    QCOMPARE (hello.sessionId, static_cast<std::uint16_t> (0x7100u));
    AppSession const* callerSession = caller.session (hello.sessionId);
    QVERIFY (callerSession != nullptr);
    QCOMPARE (static_cast<int> (callerSession->state),
              static_cast<int> (AppSessionState::Calling));

    Frame helloAck;
    QVERIFY2 (answerer.answerHello ("IU8LMC", hello, 1200, &helloAck, &error),
              error.c_str ());
    AppSession const* answererSession = answerer.session (hello.sessionId);
    QVERIFY (answererSession != nullptr);
    QCOMPARE (static_cast<int> (answererSession->state),
              static_cast<int> (AppSessionState::Connected));
    QCOMPARE (static_cast<int> (answererSession->negotiated.profile),
              static_cast<int> (Profile::Wide2300));
    QCOMPARE (static_cast<int> (answererSession->negotiated.w2300RateMode),
              static_cast<int> (W2300RateMode::Robust));

    QVERIFY2 (caller.receiveHelloAck (helloAck, 1300, &error), error.c_str ());
    callerSession = caller.session (hello.sessionId);
    QVERIFY (callerSession != nullptr);
    QCOMPARE (static_cast<int> (callerSession->state),
              static_cast<int> (AppSessionState::Connected));
    QCOMPARE (static_cast<int> (callerSession->negotiated.profile),
              static_cast<int> (Profile::Wide2300));

    QVERIFY2 (caller.queueOutgoingText (
                  hello.sessionId, "Ciao da FT2-Link", 1400, &error), error.c_str ());
    QVERIFY2 (caller.markOutgoingDelivered (hello.sessionId, 0, 1500, &error),
              error.c_str ());
    QVERIFY2 (caller.appendIncomingText (
                  hello.sessionId, "Ricevuto forte", 1600, &error), error.c_str ());

    callerSession = caller.session (hello.sessionId);
    QVERIFY (callerSession != nullptr);
    QCOMPARE (callerSession->messages.size (), static_cast<std::size_t> (2));
    QCOMPARE (static_cast<int> (callerSession->messages[0].direction),
              static_cast<int> (ChatMessageDirection::Outgoing));
    QCOMPARE (static_cast<int> (callerSession->messages[0].delivery),
              static_cast<int> (ChatDeliveryState::Delivered));
    QCOMPARE (static_cast<int> (callerSession->messages[1].direction),
              static_cast<int> (ChatMessageDirection::Incoming));
    QCOMPARE (static_cast<int> (callerSession->messages[1].delivery),
              static_cast<int> (ChatDeliveryState::Received));
    QCOMPARE (callerSession->updatedAtMs, static_cast<std::uint64_t> (1600));

    QVERIFY2 (caller.closeSession (hello.sessionId, 1700, &error), error.c_str ());
    callerSession = caller.session (hello.sessionId);
    QVERIFY (callerSession != nullptr);
    QCOMPARE (static_cast<int> (callerSession->state),
              static_cast<int> (AppSessionState::Closed));
  }

  void appModelRejectsMessagesBeforeConnection ()
  {
    using decodium::ft2link::AppSessionState;
    using decodium::ft2link::FT2LinkAppModel;
    using decodium::ft2link::Frame;
    using decodium::ft2link::StationAdvertisement;
    using decodium::ft2link::StationIdentity;

    FT2LinkAppModel model {StationIdentity {"IU8LMC", "JN70", "Salvo"}};
    model.setNextSessionId (0x7200u);

    StationAdvertisement remote;
    remote.station.call = "K1ABC";
    remote.cq = true;
    remote.heardAtMs = 1000;
    QVERIFY (model.observeStation (remote));

    std::string error;
    Frame hello = model.startSession ("K1ABC", 1100, &error);
    QVERIFY (model.session (hello.sessionId) != nullptr);
    QVERIFY (!model.queueOutgoingText (hello.sessionId, "too early", 1200, &error));
    QVERIFY (error.find ("connected") != std::string::npos);
    QCOMPARE (static_cast<int> (model.session (hello.sessionId)->state),
              static_cast<int> (AppSessionState::Calling));
  }

  void handshakePayloadCarriesStationIdentity ()
  {
    using decodium::ft2link::FT2LinkAppModel;
    using decodium::ft2link::Frame;
    using decodium::ft2link::HandshakeIdentity;
    using decodium::ft2link::LinkCapabilities;
    using decodium::ft2link::NegotiatedLink;
    using decodium::ft2link::Profile;
    using decodium::ft2link::StationAdvertisement;
    using decodium::ft2link::StationIdentity;

    LinkCapabilities capabilities;
    capabilities.preferredProfile = Profile::Wide2300;
    HandshakeIdentity identity;
    identity.call = "iu8lmc/p";
    identity.locator = "jn70";
    Frame const hello = decodium::ft2link::makeHelloFrame (
        0x6123u, capabilities, identity);

    LinkCapabilities parsedCapabilities;
    HandshakeIdentity parsedIdentity;
    std::string error;
    QVERIFY2 (decodium::ft2link::parseHelloFrame (
                  hello, &parsedCapabilities, &parsedIdentity, &error), error.c_str ());
    QCOMPARE (parsedIdentity.call, std::string ("IU8LMC/P"));
    QCOMPARE (parsedIdentity.locator, std::string ("JN70"));

    FT2LinkAppModel caller {StationIdentity {"IU8LMC", "JN70", "Salvo"}};
    FT2LinkAppModel answerer {StationIdentity {"K1ABC", "FN42", "Ann"}};
    StationAdvertisement remote = answerer.makeLocalAdvertisement (1000, true);
    QVERIFY (caller.observeStation (remote));
    Frame const rfHello = caller.startSession ("K1ABC", 1100, &error);
    Frame helloAck;
    QVERIFY2 (answerer.answerHello ("", rfHello, 1200, &helloAck, &error),
              error.c_str ());
    QVERIFY (answerer.session (rfHello.sessionId) != nullptr);
    QCOMPARE (answerer.session (rfHello.sessionId)->remoteCall,
              std::string ("IU8LMC"));

    HandshakeIdentity responderIdentity;
    NegotiatedLink negotiated;
    QVERIFY2 (decodium::ft2link::parseHelloAckFrame (
                  helloAck, nullptr, &negotiated, &responderIdentity, &error),
              error.c_str ());
    QVERIFY (negotiated.accepted);
    QCOMPARE (responderIdentity.call, std::string ("K1ABC"));
    QCOMPARE (responderIdentity.locator, std::string ("FN42"));
  }

  void beaconPayloadUpdatesStationAdvertisement ()
  {
    using decodium::ft2link::FT2LinkAppModel;
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;
    using decodium::ft2link::StationAdvertisement;
    using decodium::ft2link::StationIdentity;

    FT2LinkAppModel source {StationIdentity {"IU8LMC", "JN70", "Salvo"}};
    Frame const beacon = source.makeLocalBeaconFrame (true);
    QCOMPARE (static_cast<int> (beacon.type), static_cast<int> (FrameType::Beacon));
    QCOMPARE (static_cast<int> (beacon.profile), static_cast<int> (Profile::Narrow));
    QVERIFY ((beacon.flags & decodium::ft2link::FlagEndOfMessage) != 0u);
    QVERIFY ((beacon.sessionId & decodium::ft2link::BeaconWaveW2300) != 0u);
    QVERIFY ((beacon.sessionId & decodium::ft2link::BeaconWaveW2300Deep) != 0u);
    QVERIFY ((beacon.ackBitmap & decodium::ft2link::BeaconServiceChat) != 0u);
    QVERIFY ((beacon.ackBitmap & decodium::ft2link::BeaconServiceQsy) != 0u);

    FT2LinkAppModel receiver {StationIdentity {"K1ABC", "FN42", "Ann"}};
    StationAdvertisement advertisement;
    std::string error;
    QVERIFY2 (receiver.observeBeacon (beacon, 2100, &advertisement, &error),
              error.c_str ());
    QCOMPARE (advertisement.station.call, std::string ("IU8LMC"));
    QCOMPARE (advertisement.station.locator, std::string ("JN70"));
    QVERIFY (advertisement.cq);
    QCOMPARE (advertisement.cqType, std::string ("CQ"));
    QCOMPARE (advertisement.cqLocator, std::string ("JN70"));
    QVERIFY ((advertisement.waveformCapabilityFlags
              & decodium::ft2link::BeaconWaveW2300Ultra) != 0u);
    QVERIFY ((advertisement.serviceCapabilityFlags
              & decodium::ft2link::BeaconServiceFile) != 0u);

    std::vector<StationAdvertisement> active = receiver.activeStations (
        2200, 1000, true);
    QCOMPARE (active.size (), static_cast<std::size_t> (1));
    QCOMPARE (active[0].station.call, std::string ("IU8LMC"));

    Frame const specialBeacon =
        source.makeLocalBeaconFrame (true, 2, 750, "EMCOMM", "JN71");
    QCOMPARE (static_cast<std::uint16_t> (specialBeacon.ackBitmap & 0x000fu),
              static_cast<std::uint16_t> (3u));
    QVERIFY ((specialBeacon.ackBitmap & 0xfff0u) != 0u);
    QVERIFY2 (receiver.observeBeacon (
                  specialBeacon, 2300, &advertisement, &error),
              error.c_str ());
    QCOMPARE (advertisement.station.call, std::string ("IU8LMC"));
    QCOMPARE (advertisement.station.locator, std::string ("JN71"));
    QCOMPARE (advertisement.cqType, std::string ("EMCOMM"));
    QCOMPARE (advertisement.cqLocator, std::string ("JN71"));
    QCOMPARE (advertisement.cqSlotId, 2);
    QCOMPARE (advertisement.cqSlotOffsetHz, 1500);
    QCOMPARE (advertisement.serviceCapabilityFlags,
              static_cast<std::uint16_t> (
                  specialBeacon.ackBitmap & 0xfff0u));
  }

  void narrowWaveformRoundTripsHelloFrame ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::HandshakeIdentity;
    using decodium::ft2link::LinkCapabilities;
    using decodium::ft2link::NarrowDecodeMetrics;
    using decodium::ft2link::NarrowWaveformConfig;
    using decodium::ft2link::Profile;

    LinkCapabilities capabilities;
    capabilities.preferredProfile = Profile::Wide2300;
    HandshakeIdentity identity {"IU8LMC", "JN70"};
    Frame const hello = decodium::ft2link::makeHelloFrame (
        0x6201u, capabilities, identity);

    NarrowWaveformConfig config;
    std::string error;
    std::vector<float> const burst =
        decodium::ft2link::generateNarrowFrameWaveform (
            hello, config, &error);
    QVERIFY2 (!burst.empty (), error.c_str ());
    std::vector<float> stream = paddedWave (burst, 37, 120);
    addDeterministicNoise (stream, 0.01f);

    Frame parsed;
    NarrowDecodeMetrics metrics;
    QVERIFY2 (decodium::ft2link::decodeNarrowFrameWaveformWithMetrics (
                  stream, &parsed, &metrics, config, &error), error.c_str ());
    QCOMPARE (static_cast<int> (parsed.profile), static_cast<int> (Profile::Narrow));
    QCOMPARE (parsed.sessionId, hello.sessionId);
    QCOMPARE (parsed.payload, hello.payload);
    QVERIFY (metrics.sampleOffset <= 48u);
    QVERIFY (metrics.symbolCount > 0u);
    QVERIFY (metrics.packetBytes > 0u);
    QVERIFY (metrics.quality > 0.45);
  }

  void narrowControlAudioSurvivesMacFil4Path ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::HandshakeIdentity;
    using decodium::ft2link::LinkCapabilities;
    using decodium::ft2link::NarrowDecodeMetrics;
    using decodium::ft2link::NarrowWaveformConfig;
    using decodium::ft2link::Profile;

    LinkCapabilities capabilities;
    capabilities.preferredProfile = Profile::Wide2300;
    HandshakeIdentity identity {"TESTA", "JN70"};
    Frame const hello = decodium::ft2link::makeHelloFrame (
        0x6203u, capabilities, identity);

    NarrowWaveformConfig txConfig;
    txConfig.sampleRate = 48000.0;
    std::string error;
    std::vector<float> const burst =
        decodium::ft2link::generateNarrowFrameWaveform (
            hello, txConfig, &error);
    QVERIFY2 (!burst.empty (), error.c_str ());

    std::vector<float> txStream (4800u, 0.0f);
    txStream.insert (txStream.end (), burst.begin (), burst.end ());
    txStream.insert (txStream.end (), 4800u, 0.0f);

    std::vector<float> rxStream = fil4Decimate48kTo12k (txStream);
    addDeterministicNoise (rxStream, 0.001f);

    NarrowWaveformConfig rxConfig;
    Frame parsed;
    NarrowDecodeMetrics metrics;
    error.clear ();
    QVERIFY2 (decodium::ft2link::decodeNarrowFrameWaveformWithMetrics (
                  rxStream, &parsed, &metrics, rxConfig, &error), error.c_str ());
    QCOMPARE (static_cast<int> (parsed.profile), static_cast<int> (Profile::Narrow));
    QCOMPARE (parsed.sessionId, hello.sessionId);
    QCOMPARE (parsed.payload, hello.payload);
    QVERIFY (metrics.sampleOffset < 1600u);
    QVERIFY (metrics.quality > 0.40);
  }

  void narrowWaveformAcquiresLargeAudioOffset ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::HandshakeIdentity;
    using decodium::ft2link::LinkCapabilities;
    using decodium::ft2link::NarrowDecodeMetrics;
    using decodium::ft2link::NarrowWaveformConfig;
    using decodium::ft2link::Profile;

    LinkCapabilities capabilities;
    capabilities.preferredProfile = Profile::Wide2300;
    HandshakeIdentity identity {"TESTA", "JN70"};
    Frame const hello = decodium::ft2link::makeHelloFrame (
        0x6202u, capabilities, identity);

    NarrowWaveformConfig txConfig;
    txConfig.centerFrequencyHz = 600.0;
    std::string error;
    std::vector<float> const burst =
        decodium::ft2link::generateNarrowFrameWaveform (
            hello, txConfig, &error);
    QVERIFY2 (!burst.empty (), error.c_str ());
    std::vector<float> stream = paddedWave (burst, 53, 180);
    addDeterministicNoise (stream, 0.006f);

    NarrowWaveformConfig rxConfig;
    Frame parsed;
    NarrowDecodeMetrics metrics;
    error.clear ();
    QVERIFY2 (decodium::ft2link::decodeNarrowFrameWaveformWithMetrics (
                  stream, &parsed, &metrics, rxConfig, &error), error.c_str ());
    QCOMPARE (static_cast<int> (parsed.profile), static_cast<int> (Profile::Narrow));
    QCOMPARE (parsed.sessionId, hello.sessionId);
    QCOMPARE (parsed.payload, hello.payload);
    QVERIFY (std::fabs (metrics.estimatedCenterFrequencyHz - 600.0) <= 75.0);
    QVERIFY (std::fabs (metrics.estimatedFrequencyOffsetHz + 900.0) <= 75.0);
    QVERIFY (metrics.quality > 0.40);
  }

  void handshakeNegotiatesW2300FastBeforeData ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::LinkCapabilities;
    using decodium::ft2link::NegotiatedLink;
    using decodium::ft2link::OutboundTransfer;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300RateController;
    using decodium::ft2link::W2300RateMode;

    LinkCapabilities initiator;
    initiator.preferredProfile = Profile::Wide2300;
    initiator.preferredW2300RateMode = W2300RateMode::Fast;

    LinkCapabilities responder;
    responder.preferredProfile = Profile::Wide2300;
    responder.preferredW2300RateMode = W2300RateMode::Fast;

    Frame const hello = decodium::ft2link::makeHelloFrame (0x6000u, initiator);
    std::string error;
    NegotiatedLink responderNegotiated;
    Frame helloAck;
    QVERIFY2 (decodium::ft2link::answerHelloFrame (
                  hello, responder, &helloAck, &responderNegotiated, &error), error.c_str ());
    QCOMPARE (static_cast<int> (responderNegotiated.profile), static_cast<int> (Profile::Wide2300));
    QCOMPARE (static_cast<int> (responderNegotiated.w2300RateMode),
              static_cast<int> (W2300RateMode::Fast));

    LinkCapabilities parsedResponder;
    NegotiatedLink initiatorNegotiated;
    QVERIFY2 (decodium::ft2link::parseHelloAckFrame (
                  helloAck, &parsedResponder, &initiatorNegotiated, &error), error.c_str ());
    QVERIFY (initiatorNegotiated.accepted);
    QCOMPARE (static_cast<int> (initiatorNegotiated.profile), static_cast<int> (Profile::Wide2300));

    OutboundTransfer tx {initiatorNegotiated.profile, 0x6000u,
                         bytesFromString ("data starts after negotiated hello ack")};
    std::vector<Frame> frames = tx.framesToSend (0);
    QCOMPARE (frames.size (), static_cast<std::size_t> (1));
    QCOMPARE (static_cast<int> (frames[0].profile), static_cast<int> (Profile::Wide2300));

    W2300RateController controller {initiatorNegotiated.w2300RateMode};
    QCOMPARE (static_cast<int> (controller.configForAttempt (1).rateMode),
              static_cast<int> (W2300RateMode::Fast));
  }

  void handshakeCanNegotiateW2300WeakPreference ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::LinkCapabilities;
    using decodium::ft2link::NegotiatedLink;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300RateMode;

    LinkCapabilities initiator;
    initiator.preferredProfile = Profile::Wide2300;
    initiator.preferredW2300RateMode = W2300RateMode::Weak;

    LinkCapabilities responder;
    responder.preferredProfile = Profile::Wide2300;
    responder.preferredW2300RateMode = W2300RateMode::Robust;

    Frame const hello = decodium::ft2link::makeHelloFrame (0x6003u, initiator);
    Frame helloAck;
    NegotiatedLink responderNegotiated;
    std::string error;
    QVERIFY2 (decodium::ft2link::answerHelloFrame (
                  hello, responder, &helloAck, &responderNegotiated, &error), error.c_str ());
    QVERIFY (responderNegotiated.accepted);
    QCOMPARE (static_cast<int> (responderNegotiated.profile), static_cast<int> (Profile::Wide2300));
    QCOMPARE (static_cast<int> (responderNegotiated.w2300RateMode),
              static_cast<int> (W2300RateMode::Weak));

    LinkCapabilities parsedResponder;
    NegotiatedLink initiatorNegotiated;
    QVERIFY2 (decodium::ft2link::parseHelloAckFrame (
                  helloAck, &parsedResponder, &initiatorNegotiated, &error), error.c_str ());
    QVERIFY (initiatorNegotiated.accepted);
    QCOMPARE (static_cast<int> (initiatorNegotiated.w2300RateMode),
              static_cast<int> (W2300RateMode::Weak));
  }

  void handshakeCanNegotiateW2300UltraPreference ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::LinkCapabilities;
    using decodium::ft2link::NegotiatedLink;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300RateMode;

    LinkCapabilities initiator;
    initiator.preferredProfile = Profile::Wide2300;
    initiator.preferredW2300RateMode = W2300RateMode::Ultra;

    LinkCapabilities responder;
    responder.preferredProfile = Profile::Wide2300;
    responder.preferredW2300RateMode = W2300RateMode::Robust;

    Frame const hello = decodium::ft2link::makeHelloFrame (0x6004u, initiator);
    Frame helloAck;
    NegotiatedLink responderNegotiated;
    std::string error;
    QVERIFY2 (decodium::ft2link::answerHelloFrame (
                  hello, responder, &helloAck, &responderNegotiated, &error), error.c_str ());
    QVERIFY (responderNegotiated.accepted);
    QCOMPARE (static_cast<int> (responderNegotiated.profile), static_cast<int> (Profile::Wide2300));
    QCOMPARE (static_cast<int> (responderNegotiated.w2300RateMode),
              static_cast<int> (W2300RateMode::Ultra));

    LinkCapabilities parsedResponder;
    NegotiatedLink initiatorNegotiated;
    QVERIFY2 (decodium::ft2link::parseHelloAckFrame (
                  helloAck, &parsedResponder, &initiatorNegotiated, &error), error.c_str ());
    QVERIFY (initiatorNegotiated.accepted);
    QCOMPARE (static_cast<int> (initiatorNegotiated.w2300RateMode),
              static_cast<int> (W2300RateMode::Ultra));
  }

  void handshakeFallsBackToW500WhenW2300IsNotCommon ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::LinkCapabilities;
    using decodium::ft2link::NegotiatedLink;
    using decodium::ft2link::Profile;

    LinkCapabilities initiator;
    initiator.supportsW2300 = true;
    initiator.supportsW500 = true;
    initiator.preferredProfile = Profile::Wide2300;

    LinkCapabilities responder;
    responder.supportsW2300 = false;
    responder.supportsW2300Fast = false;
    responder.supportsW2300Robust = false;
    responder.supportsW500 = true;
    responder.preferredProfile = Profile::Wide500;

    Frame const hello = decodium::ft2link::makeHelloFrame (0x6001u, initiator);
    Frame helloAck;
    NegotiatedLink negotiated;
    std::string error;
    QVERIFY2 (decodium::ft2link::answerHelloFrame (
                  hello, responder, &helloAck, &negotiated, &error), error.c_str ());
    QVERIFY (negotiated.accepted);
    QCOMPARE (static_cast<int> (negotiated.profile), static_cast<int> (Profile::Wide500));
  }

  void handshakeRejectsWhenNoWideProfileIsCommon ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::LinkCapabilities;
    using decodium::ft2link::NegotiatedLink;

    LinkCapabilities initiator;
    initiator.supportsW500 = false;
    initiator.supportsW2300 = true;

    LinkCapabilities responder;
    responder.supportsW500 = true;
    responder.supportsW2300 = false;
    responder.supportsW2300Fast = false;
    responder.supportsW2300Robust = false;

    Frame const hello = decodium::ft2link::makeHelloFrame (0x6002u, initiator);
    Frame helloAck;
    NegotiatedLink negotiated;
    std::string error;
    QVERIFY (!decodium::ft2link::answerHelloFrame (
        hello, responder, &helloAck, &negotiated, &error));
    QVERIFY (!negotiated.accepted);

    LinkCapabilities parsedResponder;
    NegotiatedLink parsedNegotiated;
    QVERIFY2 (decodium::ft2link::parseHelloAckFrame (
                  helloAck, &parsedResponder, &parsedNegotiated, &error), error.c_str ());
    QVERIFY (!parsedNegotiated.accepted);
  }

  void physicalProfileSpecsExposeWideModes ()
  {
    using decodium::ft2link::PhysicalProfileSpec;
    using decodium::ft2link::Profile;

    PhysicalProfileSpec narrow;
    PhysicalProfileSpec w500;
    PhysicalProfileSpec w2300;
    QVERIFY (decodium::ft2link::physicalProfileSpec (Profile::Narrow, &narrow));
    QVERIFY (decodium::ft2link::physicalProfileSpec (Profile::Wide500, &w500));
    QVERIFY (decodium::ft2link::physicalProfileSpec (Profile::Wide2300, &w2300));

    QVERIFY (!narrow.binaryPacket);
    QVERIFY (w500.binaryPacket);
    QVERIFY (w2300.binaryPacket);
    QCOMPARE (w500.occupiedBandwidthHz, 500.0);
    QCOMPARE (w2300.occupiedBandwidthHz, 2300.0);
    QVERIFY (w500.repetitionFactor > w2300.repetitionFactor);
    QVERIFY (w2300.symbolRate > w500.symbolRate);
    QVERIFY (w2300.bitsPerSymbol > w500.bitsPerSymbol);
    QVERIFY (w2300.maxSerializedFrameBytes > w500.maxSerializedFrameBytes);
  }

  void physicalPacketRoundTripsWideProfiles ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;

    for (Profile profile : {Profile::Wide500, Profile::Wide2300})
      {
        Frame frame;
        frame.type = FrameType::Data;
        frame.profile = profile;
        frame.flags = decodium::ft2link::FlagEndOfMessage;
        frame.sessionId = 0x7777u;
        frame.sequence = 9u;
        frame.payload = bytesFromString (profile == Profile::Wide500
                                         ? "W500 payload"
                                         : "W2300 payload with more room for data frames");

        std::string error;
        std::vector<std::uint8_t> const packet = decodium::ft2link::encodePhysicalPacket (frame, &error);
        QVERIFY2 (!packet.empty (), error.c_str ());

        Frame parsed;
        QVERIFY2 (decodium::ft2link::decodePhysicalPacket (profile, packet, &parsed, &error),
                  error.c_str ());
        QCOMPARE (static_cast<int> (parsed.profile), static_cast<int> (profile));
        QCOMPARE (parsed.sessionId, frame.sessionId);
        QCOMPARE (parsed.sequence, frame.sequence);
        QVERIFY (parsed.payload == frame.payload);
      }
  }

  void physicalPacketW500CorrectsSingleCodedBitDamage ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide500;
    frame.sessionId = 0x5150u;
    frame.sequence = 2u;
    frame.payload = bytesFromString ("robust packet");

    std::string error;
    std::vector<std::uint8_t> packet = decodium::ft2link::encodePhysicalPacket (frame, &error);
    QVERIFY2 (!packet.empty (), error.c_str ());
    packet.at (3) = static_cast<std::uint8_t> (packet.at (3) ^ 0x04u);

    Frame parsed;
    QVERIFY2 (decodium::ft2link::decodePhysicalPacket (Profile::Wide500, packet, &parsed, &error),
              error.c_str ());
    QVERIFY (parsed.payload == frame.payload);
  }

  void physicalPacketW2300RejectsBitDamage ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide2300;
    frame.sessionId = 0x2300u;
    frame.sequence = 4u;
    frame.payload = bytesFromString ("fast packet");

    std::string error;
    std::vector<std::uint8_t> packet = decodium::ft2link::encodePhysicalPacket (frame, &error);
    QVERIFY2 (!packet.empty (), error.c_str ());
    packet.at (3) = static_cast<std::uint8_t> (packet.at (3) ^ 0x04u);

    Frame parsed;
    QVERIFY (!decodium::ft2link::decodePhysicalPacket (Profile::Wide2300, packet, &parsed, &error));
    QVERIFY (!error.empty ());
  }

  void w500SymbolMappingRoundTripsPacket ()
  {
    std::vector<std::uint8_t> packet = bytesFromString ("symbol mapper");
    std::string error;
    std::vector<std::uint8_t> const symbols = decodium::ft2link::w500PacketToSymbols (packet, &error);
    QVERIFY2 (!symbols.empty (), error.c_str ());

    std::vector<std::uint8_t> parsed;
    QVERIFY2 (decodium::ft2link::w500SymbolsToPacket (symbols, &parsed, &error), error.c_str ());
    QVERIFY (parsed == packet);
  }

  void w500WaveformRoundTripsFrame ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide500;
    frame.flags = decodium::ft2link::FlagEndOfMessage;
    frame.sessionId = 0x500u;
    frame.sequence = 12u;
    frame.payload = bytesFromString ("W500 audio test payload");

    std::string error;
    std::vector<float> const wave = decodium::ft2link::generateW500FrameWaveform (frame, {}, &error);
    QVERIFY2 (!wave.empty (), error.c_str ());

    Frame parsed;
    QVERIFY2 (decodium::ft2link::decodeW500FrameWaveform (wave, &parsed, {}, &error),
              error.c_str ());
    QCOMPARE (static_cast<int> (parsed.type), static_cast<int> (frame.type));
    QCOMPARE (static_cast<int> (parsed.profile), static_cast<int> (frame.profile));
    QCOMPARE (parsed.sessionId, frame.sessionId);
    QCOMPARE (parsed.sequence, frame.sequence);
    QVERIFY (parsed.payload == frame.payload);
  }

  void w500WaveformScannerFindsOffsetBurst ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide500;
    frame.flags = decodium::ft2link::FlagEndOfMessage;
    frame.sessionId = 0x501u;
    frame.sequence = 13u;
    frame.payload = bytesFromString ("offset W500 burst");

    std::string error;
    std::vector<float> const burst = decodium::ft2link::generateW500FrameWaveform (frame, {}, &error);
    QVERIFY2 (!burst.empty (), error.c_str ());
    std::vector<float> const stream = paddedWave (burst, 137, 211);

    Frame parsed;
    decodium::ft2link::W500DecodeMetrics metrics;
    QVERIFY2 (decodium::ft2link::decodeW500FrameWaveformWithMetrics (stream, &parsed, &metrics, {}, &error),
              error.c_str ());
    QCOMPARE (parsed.sessionId, frame.sessionId);
    QCOMPARE (parsed.sequence, frame.sequence);
    QVERIFY (parsed.payload == frame.payload);
    QCOMPARE (metrics.sampleOffset, static_cast<std::size_t> (137));
    QVERIFY (metrics.symbolCount > 0u);
    QVERIFY (metrics.packetBytes > 0u);
    QVERIFY (metrics.quality > 0.70);
  }

  void w500WaveformToleratesNoiseAndSmallFrequencyOffset ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W500WaveformConfig;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide500;
    frame.flags = decodium::ft2link::FlagEndOfMessage;
    frame.sessionId = 0x502u;
    frame.sequence = 14u;
    frame.payload = bytesFromString ("noisy W500 burst");

    W500WaveformConfig txConfig;
    txConfig.centerFrequencyHz = 1504.0;
    txConfig.gain = 0.75;

    std::string error;
    std::vector<float> burst = decodium::ft2link::generateW500FrameWaveform (frame, txConfig, &error);
    QVERIFY2 (!burst.empty (), error.c_str ());
    std::vector<float> stream = paddedWave (burst, 61, 113);
    addDeterministicNoise (stream, 0.015f);

    Frame parsed;
    decodium::ft2link::W500DecodeMetrics metrics;
    QVERIFY2 (decodium::ft2link::decodeW500FrameWaveformWithMetrics (stream, &parsed, &metrics, {}, &error),
              error.c_str ());
    QCOMPARE (parsed.sessionId, frame.sessionId);
    QCOMPARE (parsed.sequence, frame.sequence);
    QVERIFY (parsed.payload == frame.payload);
    QVERIFY (std::fabs (metrics.estimatedFrequencyOffsetHz - 4.0) <= 2.0);
    QVERIFY (std::fabs (metrics.estimatedCenterFrequencyHz - 1504.0) <= 2.0);
    QVERIFY (metrics.quality > 0.55);
  }

  void w500WaveformRejectsBrokenPreamble ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide500;
    frame.sessionId = 0x500u;
    frame.payload = bytesFromString ("bad preamble");

    std::string error;
    std::vector<float> wave = decodium::ft2link::generateW500FrameWaveform (frame, {}, &error);
    QVERIFY2 (!wave.empty (), error.c_str ());
    std::fill (wave.begin (), wave.begin () + 192, 0.0f);

    Frame parsed;
    QVERIFY (!decodium::ft2link::decodeW500FrameWaveform (wave, &parsed, {}, &error));
    QVERIFY (!error.empty ());
  }

  void w2300SymbolMappingRoundTripsPacket ()
  {
    std::vector<std::uint8_t> packet = bytesFromString ("wide 2300 symbol mapper");
    std::string error;
    std::vector<std::uint8_t> const symbols = decodium::ft2link::w2300PacketToSymbols (packet, &error);
    QVERIFY2 (!symbols.empty (), error.c_str ());
    QVERIFY (symbols.size () < decodium::ft2link::w500PacketToSymbols (packet, &error).size ());

    std::vector<std::uint8_t> parsed;
    QVERIFY2 (decodium::ft2link::w2300SymbolsToPacket (symbols, &parsed, &error), error.c_str ());
    QVERIFY (parsed == packet);
  }

  void w2300WaveformRoundTripsFrame ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;

    std::string text;
    for (int i = 0; i < 4; ++i)
      {
        text += "W2300 carries a wider and faster FT2-Link payload. ";
      }

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide2300;
    frame.flags = decodium::ft2link::FlagEndOfMessage;
    frame.sessionId = 0x2300u;
    frame.sequence = 21u;
    frame.payload = bytesFromString (text);

    std::string error;
    std::vector<float> const wave = decodium::ft2link::generateW2300FrameWaveform (frame, {}, &error);
    QVERIFY2 (!wave.empty (), error.c_str ());

    Frame parsed;
    decodium::ft2link::W2300DecodeMetrics metrics;
    QVERIFY2 (decodium::ft2link::decodeW2300FrameWaveformWithMetrics (wave, &parsed, &metrics, {}, &error),
              error.c_str ());
    QCOMPARE (static_cast<int> (parsed.type), static_cast<int> (frame.type));
    QCOMPARE (static_cast<int> (parsed.profile), static_cast<int> (frame.profile));
    QCOMPARE (parsed.sessionId, frame.sessionId);
    QCOMPARE (parsed.sequence, frame.sequence);
    QVERIFY (parsed.payload == frame.payload);
    QCOMPARE (static_cast<int> (metrics.rateMode), static_cast<int> (decodium::ft2link::W2300RateMode::Fast));
    QCOMPARE (metrics.repetitionFactor, 1);
    QCOMPARE (metrics.rawBitRate, 4800.0);
    QCOMPARE (metrics.payloadBitRate, 4800.0);
    QVERIFY (metrics.packetBytes > 0u);
    QVERIFY (metrics.symbolCount > 0u);
    QVERIFY (metrics.quality > 0.55);
  }

  void w2300WaveformScannerFindsOffsetBurst ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide2300;
    frame.flags = decodium::ft2link::FlagEndOfMessage;
    frame.sessionId = 0x2301u;
    frame.sequence = 22u;
    frame.payload = bytesFromString ("offset W2300 burst with byte symbols");

    std::string error;
    std::vector<float> const burst = decodium::ft2link::generateW2300FrameWaveform (frame, {}, &error);
    QVERIFY2 (!burst.empty (), error.c_str ());
    std::vector<float> const stream = paddedWave (burst, 47, 93);

    Frame parsed;
    decodium::ft2link::W2300DecodeMetrics metrics;
    QVERIFY2 (decodium::ft2link::decodeW2300FrameWaveformWithMetrics (stream, &parsed, &metrics, {}, &error),
              error.c_str ());
    QCOMPARE (parsed.sessionId, frame.sessionId);
    QCOMPARE (parsed.sequence, frame.sequence);
    QVERIFY (parsed.payload == frame.payload);
    QCOMPARE (metrics.sampleOffset, static_cast<std::size_t> (47));
    QVERIFY (metrics.quality > 0.55);
  }

  void w2300RobustSymbolMappingInterleavesAndCorrectsBurstDamage ()
  {
    std::vector<std::uint8_t> packet = bytesFromString ("robust interleaved W2300 byte symbols");
    std::string error;
    std::vector<std::uint8_t> symbols = decodium::ft2link::w2300PacketToSymbols (
        packet, decodium::ft2link::W2300RateMode::Robust, &error);
    QVERIFY2 (!symbols.empty (), error.c_str ());
    QCOMPARE (decodium::ft2link::w2300RateModeRepetitionFactor (
                  decodium::ft2link::W2300RateMode::Robust), 3);

    std::size_t const robustPayloadOffset = symbols.size () - packet.size () * 3u;
    QVERIFY (symbols.size () >= robustPayloadOffset + packet.size () * 3u);
    for (std::size_t i = 0; i < packet.size (); ++i)
      {
        symbols[robustPayloadOffset + i] =
            static_cast<std::uint8_t> (symbols[robustPayloadOffset + i] ^ 0xffu);
      }

    std::vector<std::uint8_t> parsed;
    QVERIFY2 (decodium::ft2link::w2300SymbolsToPacket (symbols, &parsed, &error), error.c_str ());
    QVERIFY (parsed == packet);
  }

  void w2300WeakSymbolMappingInterleavesAndCorrectsBurstDamage ()
  {
    using decodium::ft2link::W2300RateMode;

    std::vector<std::uint8_t> packet = bytesFromString ("weak interleaved W2300 byte symbols survive bursts");
    std::string error;
    std::vector<std::uint8_t> symbols = decodium::ft2link::w2300PacketToSymbols (
        packet, W2300RateMode::Weak, &error);
    QVERIFY2 (!symbols.empty (), error.c_str ());
    QCOMPARE (decodium::ft2link::w2300RateModeRepetitionFactor (W2300RateMode::Weak), 5);

    std::size_t const weakPayloadOffset = symbols.size () - packet.size () * 5u;
    QVERIFY (symbols.size () >= weakPayloadOffset + packet.size () * 5u);
    for (std::size_t pass = 0; pass < 2u; ++pass)
      {
        for (std::size_t i = 0; i < packet.size (); ++i)
          {
            std::size_t const offset = weakPayloadOffset + pass * packet.size () + i;
            symbols[offset] = static_cast<std::uint8_t> (symbols[offset] ^ 0xffu);
          }
      }

    std::vector<std::uint8_t> parsed;
    QVERIFY2 (decodium::ft2link::w2300SymbolsToPacket (symbols, &parsed, &error), error.c_str ());
    QVERIFY (parsed == packet);
  }

  void w2300DeepSymbolMappingInterleavesAndCorrectsBurstDamage ()
  {
    using decodium::ft2link::W2300RateMode;

    std::vector<std::uint8_t> packet = bytesFromString ("deep interleaved W2300 byte symbols");
    std::string error;
    std::vector<std::uint8_t> symbols = decodium::ft2link::w2300PacketToSymbols (
        packet, W2300RateMode::Deep, &error);
    QVERIFY2 (!symbols.empty (), error.c_str ());
    QCOMPARE (decodium::ft2link::w2300RateModeRepetitionFactor (W2300RateMode::Deep), 17);

    std::size_t const deepPayloadOffset = symbols.size () - packet.size () * 17u;
    QVERIFY (symbols.size () >= deepPayloadOffset + packet.size () * 17u);
    for (std::size_t pass = 0; pass < 8u; ++pass)
      {
        for (std::size_t i = 0; i < packet.size (); ++i)
          {
            std::size_t const offset = deepPayloadOffset + pass * packet.size () + i;
            symbols[offset] = static_cast<std::uint8_t> (symbols[offset] ^ 0xffu);
          }
      }

    std::vector<std::uint8_t> parsed;
    QVERIFY2 (decodium::ft2link::w2300SymbolsToPacket (symbols, &parsed, &error), error.c_str ());
    QVERIFY (parsed == packet);
  }

  void w2300UltraSymbolMappingInterleavesAndCorrectsBurstDamage ()
  {
    using decodium::ft2link::W2300RateMode;

    std::vector<std::uint8_t> packet = bytesFromString ("ultra interleaved W2300 byte symbols");
    std::string error;
    std::vector<std::uint8_t> symbols = decodium::ft2link::w2300PacketToSymbols (
        packet, W2300RateMode::Ultra, &error);
    QVERIFY2 (!symbols.empty (), error.c_str ());
    QCOMPARE (decodium::ft2link::w2300RateModeRepetitionFactor (W2300RateMode::Ultra), 25);

    std::size_t const ultraPayloadOffset = symbols.size () - packet.size () * 25u;
    QVERIFY (symbols.size () >= ultraPayloadOffset + packet.size () * 25u);
    for (std::size_t pass = 0; pass < 12u; ++pass)
      {
        for (std::size_t i = 0; i < packet.size (); ++i)
          {
            std::size_t const offset = ultraPayloadOffset + pass * packet.size () + i;
            symbols[offset] = static_cast<std::uint8_t> (symbols[offset] ^ 0xffu);
          }
      }

    std::vector<std::uint8_t> parsed;
    QVERIFY2 (decodium::ft2link::w2300SymbolsToPacket (symbols, &parsed, &error), error.c_str ());
    QVERIFY (parsed == packet);
  }

  void w2300RobustWaveformRoundTripsAndReportsMode ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300RateMode;
    using decodium::ft2link::W2300WaveformConfig;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide2300;
    frame.flags = decodium::ft2link::FlagEndOfMessage;
    frame.sessionId = 0x2303u;
    frame.sequence = 24u;
    frame.payload = bytesFromString ("ROBUST W2300 trades rate for correction margin");

    W2300WaveformConfig robustConfig;
    robustConfig.rateMode = W2300RateMode::Robust;

    std::string error;
    std::vector<float> const robustWave = decodium::ft2link::generateW2300FrameWaveform (
        frame, robustConfig, &error);
    QVERIFY2 (!robustWave.empty (), error.c_str ());
    std::vector<float> const fastWave = decodium::ft2link::generateW2300FrameWaveform (frame, {}, &error);
    QVERIFY (robustWave.size () > fastWave.size ());

    Frame parsed;
    decodium::ft2link::W2300DecodeMetrics metrics;
    QVERIFY2 (decodium::ft2link::decodeW2300FrameWaveformWithMetrics (
                  robustWave, &parsed, &metrics, {}, &error), error.c_str ());
    QCOMPARE (parsed.sessionId, frame.sessionId);
    QCOMPARE (parsed.sequence, frame.sequence);
    QVERIFY (parsed.payload == frame.payload);
    QCOMPARE (static_cast<int> (metrics.rateMode), static_cast<int> (W2300RateMode::Robust));
    QCOMPARE (metrics.repetitionFactor, 3);
    QCOMPARE (metrics.rawBitRate, 4800.0);
    QCOMPARE (metrics.payloadBitRate, 1600.0);
    QVERIFY (metrics.quality > 0.45);
  }

  void w2300WeakWaveformRoundTripsAndReportsMode ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300RateMode;
    using decodium::ft2link::W2300WaveformConfig;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide2300;
    frame.flags = decodium::ft2link::FlagEndOfMessage;
    frame.sessionId = 0x2304u;
    frame.sequence = 25u;
    frame.payload = bytesFromString ("WEAK W2300 trades speed for interleaved correction margin");

    W2300WaveformConfig weakConfig;
    weakConfig.rateMode = W2300RateMode::Weak;

    std::string error;
    std::vector<float> const weakWave = decodium::ft2link::generateW2300FrameWaveform (
        frame, weakConfig, &error);
    QVERIFY2 (!weakWave.empty (), error.c_str ());
    W2300WaveformConfig robustConfig;
    robustConfig.rateMode = W2300RateMode::Robust;
    std::vector<float> const robustWave = decodium::ft2link::generateW2300FrameWaveform (
        frame, robustConfig, &error);
    QVERIFY (weakWave.size () > robustWave.size ());

    Frame parsed;
    decodium::ft2link::W2300DecodeMetrics metrics;
    QVERIFY2 (decodium::ft2link::decodeW2300FrameWaveformWithMetrics (
                  weakWave, &parsed, &metrics, {}, &error), error.c_str ());
    QCOMPARE (parsed.sessionId, frame.sessionId);
    QCOMPARE (parsed.sequence, frame.sequence);
    QVERIFY (parsed.payload == frame.payload);
    QCOMPARE (static_cast<int> (metrics.rateMode), static_cast<int> (W2300RateMode::Weak));
    QCOMPARE (metrics.repetitionFactor, 5);
    QCOMPARE (metrics.rawBitRate, 4800.0);
    QCOMPARE (metrics.payloadBitRate, 960.0);
    QVERIFY (metrics.quality > 0.45);
  }

  void w2300DeepWaveformRoundTripsAndReportsMode ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300RateMode;
    using decodium::ft2link::W2300WaveformConfig;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide2300;
    frame.flags = decodium::ft2link::FlagEndOfMessage;
    frame.sessionId = 0x2305u;
    frame.sequence = 26u;
    frame.payload = bytesFromString ("DEEP W2300 slow fallback");

    W2300WaveformConfig deepConfig;
    deepConfig.rateMode = W2300RateMode::Deep;

    std::string error;
    std::vector<float> const deepWave = decodium::ft2link::generateW2300FrameWaveform (
        frame, deepConfig, &error);
    QVERIFY2 (!deepWave.empty (), error.c_str ());
    W2300WaveformConfig weakConfig;
    weakConfig.rateMode = W2300RateMode::Weak;
    std::vector<float> const weakWave = decodium::ft2link::generateW2300FrameWaveform (
        frame, weakConfig, &error);
    QVERIFY (deepWave.size () > weakWave.size ());

    Frame parsed;
    decodium::ft2link::W2300DecodeMetrics metrics;
    QVERIFY2 (decodium::ft2link::decodeW2300FrameWaveformWithMetrics (
                  deepWave, &parsed, &metrics, {}, &error), error.c_str ());
    QCOMPARE (parsed.sessionId, frame.sessionId);
    QCOMPARE (parsed.sequence, frame.sequence);
    QVERIFY (parsed.payload == frame.payload);
    QCOMPARE (static_cast<int> (metrics.rateMode), static_cast<int> (W2300RateMode::Deep));
    QCOMPARE (metrics.repetitionFactor, 17);
    QCOMPARE (metrics.rawBitRate, 4800.0);
    QCOMPARE (metrics.payloadBitRate, 4800.0 / 17.0);
    QVERIFY (metrics.quality > 0.40);
  }

  void w2300UltraWaveformRoundTripsAndReportsMode ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300RateMode;
    using decodium::ft2link::W2300WaveformConfig;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide2300;
    frame.flags = decodium::ft2link::FlagEndOfMessage;
    frame.sessionId = 0x2306u;
    frame.sequence = 27u;
    frame.payload = bytesFromString ("ULTRA W2300 weak signal fallback");

    W2300WaveformConfig ultraConfig;
    ultraConfig.rateMode = W2300RateMode::Ultra;

    std::string error;
    std::vector<float> const ultraWave = decodium::ft2link::generateW2300FrameWaveform (
        frame, ultraConfig, &error);
    QVERIFY2 (!ultraWave.empty (), error.c_str ());
    W2300WaveformConfig deepConfig;
    deepConfig.rateMode = W2300RateMode::Deep;
    std::vector<float> const deepWave = decodium::ft2link::generateW2300FrameWaveform (
        frame, deepConfig, &error);
    QVERIFY (ultraWave.size () > deepWave.size ());

    Frame parsed;
    decodium::ft2link::W2300DecodeMetrics metrics;
    QVERIFY2 (decodium::ft2link::decodeW2300FrameWaveformWithMetrics (
                  ultraWave, &parsed, &metrics, {}, &error), error.c_str ());
    QCOMPARE (parsed.sessionId, frame.sessionId);
    QCOMPARE (parsed.sequence, frame.sequence);
    QVERIFY (parsed.payload == frame.payload);
    QCOMPARE (static_cast<int> (metrics.rateMode), static_cast<int> (W2300RateMode::Ultra));
    QCOMPARE (metrics.repetitionFactor, 25);
    QCOMPARE (metrics.rawBitRate, 4800.0);
    QCOMPARE (metrics.payloadBitRate, 4800.0 / 25.0);
    QVERIFY (metrics.quality > 0.35);
  }

  void w2300WaveformToleratesNoiseAndSmallFrequencyOffset ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300WaveformConfig;

    Frame frame;
    frame.type = FrameType::Data;
    frame.profile = Profile::Wide2300;
    frame.flags = decodium::ft2link::FlagEndOfMessage;
    frame.sessionId = 0x2302u;
    frame.sequence = 23u;
    frame.payload = bytesFromString ("noisy W2300 multicarrier DQPSK burst");

    W2300WaveformConfig txConfig;
    txConfig.centerFrequencyHz = 1510.0;
    txConfig.gain = 0.52;

    std::string error;
    std::vector<float> burst = decodium::ft2link::generateW2300FrameWaveform (frame, txConfig, &error);
    QVERIFY2 (!burst.empty (), error.c_str ());
    std::vector<float> stream = paddedWave (burst, 37, 79);
    addDeterministicNoise (stream, 0.010f);

    Frame parsed;
    decodium::ft2link::W2300DecodeMetrics metrics;
    QVERIFY2 (decodium::ft2link::decodeW2300FrameWaveformWithMetrics (stream, &parsed, &metrics, {}, &error),
              error.c_str ());
    QCOMPARE (parsed.sessionId, frame.sessionId);
    QCOMPARE (parsed.sequence, frame.sequence);
    QVERIFY (parsed.payload == frame.payload);
    QVERIFY (std::fabs (metrics.estimatedFrequencyOffsetHz - 10.0) <= 5.0);
    QVERIFY (std::fabs (metrics.estimatedCenterFrequencyHz - 1510.0) <= 5.0);
    QVERIFY (metrics.quality > 0.45);
  }

  void w2300WaveformCorrectsLargeFrequencyOffset ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300WaveformConfig;

    for (double const offsetHz : {-50.0, -25.0, 25.0, 50.0})
      {
        Frame frame;
        frame.type = FrameType::Data;
        frame.profile = Profile::Wide2300;
        frame.flags = decodium::ft2link::FlagEndOfMessage;
        frame.sessionId = static_cast<std::uint16_t> (0x2310u + int (offsetHz + 50.0));
        frame.sequence = 24u;
        frame.payload = bytesFromString ("W2300 CFO correction sweep payload");

        W2300WaveformConfig txConfig;
        txConfig.centerFrequencyHz = 1500.0 + offsetHz;
        txConfig.gain = 0.54;

        std::string error;
        std::vector<float> burst = decodium::ft2link::generateW2300FrameWaveform (frame, txConfig, &error);
        QVERIFY2 (!burst.empty (), error.c_str ());
        std::vector<float> stream = paddedWave (burst, 31, 57);
        addDeterministicNoise (stream, 0.004f);

        Frame parsed;
        decodium::ft2link::W2300DecodeMetrics metrics;
        bool const decoded = decodium::ft2link::decodeW2300FrameWaveformWithMetrics (
            stream, &parsed, &metrics, {}, &error);
        std::string const failure = "offset " + std::to_string (offsetHz) + " Hz: " + error;
        QVERIFY2 (decoded, failure.c_str ());
        QCOMPARE (parsed.sessionId, frame.sessionId);
        QCOMPARE (parsed.sequence, frame.sequence);
        QVERIFY (parsed.payload == frame.payload);
        QVERIFY (std::fabs (metrics.estimatedFrequencyOffsetHz - offsetHz) <= 7.5);
        QVERIFY (std::fabs (metrics.estimatedCenterFrequencyHz - txConfig.centerFrequencyHz) <= 7.5);
        QVERIFY (metrics.quality > 0.45);
      }
  }

  void w2300RateRecommendationUsesQualityOffsetAndRetries ()
  {
    using decodium::ft2link::W2300DecodeMetrics;
    using decodium::ft2link::W2300RateMode;

    W2300DecodeMetrics metrics;
    metrics.rateMode = W2300RateMode::Robust;
    metrics.quality = 0.82;
    metrics.estimatedFrequencyOffsetHz = 4.0;
    QCOMPARE (static_cast<int> (decodium::ft2link::recommendedW2300RateMode (metrics)),
              static_cast<int> (W2300RateMode::Fast));

    metrics.rateMode = W2300RateMode::Fast;
    metrics.quality = 0.40;
    metrics.estimatedFrequencyOffsetHz = 4.0;
    QCOMPARE (static_cast<int> (decodium::ft2link::recommendedW2300RateMode (metrics)),
              static_cast<int> (W2300RateMode::Robust));

    metrics.quality = 0.82;
    metrics.estimatedFrequencyOffsetHz = 20.0;
    QCOMPARE (static_cast<int> (decodium::ft2link::recommendedW2300RateMode (metrics)),
              static_cast<int> (W2300RateMode::Robust));

    metrics.estimatedFrequencyOffsetHz = 4.0;
    QCOMPARE (static_cast<int> (decodium::ft2link::recommendedW2300RateMode (metrics, 1u)),
              static_cast<int> (W2300RateMode::Robust));
    QCOMPARE (static_cast<int> (decodium::ft2link::recommendedW2300RateMode (metrics, 2u)),
              static_cast<int> (W2300RateMode::Weak));
    QCOMPARE (static_cast<int> (decodium::ft2link::recommendedW2300RateMode (metrics, 3u)),
              static_cast<int> (W2300RateMode::Deep));
    QCOMPARE (static_cast<int> (decodium::ft2link::recommendedW2300RateMode (metrics, 4u)),
              static_cast<int> (W2300RateMode::Ultra));

    metrics.quality = 0.20;
    metrics.estimatedFrequencyOffsetHz = 4.0;
    QCOMPARE (static_cast<int> (decodium::ft2link::recommendedW2300RateMode (metrics)),
              static_cast<int> (W2300RateMode::Deep));
    metrics.quality = 0.12;
    QCOMPARE (static_cast<int> (decodium::ft2link::recommendedW2300RateMode (metrics)),
              static_cast<int> (W2300RateMode::Ultra));
  }

  void w2300RateControllerUsesRobustForArqRetries ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::OutboundTransfer;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300RateController;
    using decodium::ft2link::W2300RateMode;

    OutboundTransfer tx {Profile::Wide2300, 0x3300u,
                         bytesFromString ("adaptive retry should use robust waveform")};
    tx.setWindowSize (1);
    tx.setRetryMs (1000);

    W2300RateController controller;
    std::vector<Frame> first = tx.framesToSend (0);
    QCOMPARE (first.size (), static_cast<std::size_t> (1));
    QCOMPARE (tx.attemptsForSequence (first[0].sequence), 1);
    QCOMPARE (static_cast<int> (controller.configForAttempt (
                  tx.attemptsForSequence (first[0].sequence)).rateMode),
              static_cast<int> (W2300RateMode::Fast));

    std::vector<Frame> retry = tx.framesToSend (1000);
    QCOMPARE (retry.size (), static_cast<std::size_t> (1));
    QCOMPARE (retry[0].sequence, first[0].sequence);
    QCOMPARE (tx.attemptsForSequence (retry[0].sequence), 2);
    QCOMPARE (static_cast<int> (controller.configForAttempt (
                  tx.attemptsForSequence (retry[0].sequence)).rateMode),
              static_cast<int> (W2300RateMode::Robust));
    QCOMPARE (static_cast<int> (controller.configForAttempt (3).rateMode),
              static_cast<int> (W2300RateMode::Weak));
    QCOMPARE (static_cast<int> (controller.configForAttempt (4).rateMode),
              static_cast<int> (W2300RateMode::Deep));
    QCOMPARE (static_cast<int> (controller.configForAttempt (5).rateMode),
              static_cast<int> (W2300RateMode::Ultra));
  }

  void w2300RateControllerLearnsFromDecodeMetrics ()
  {
    using decodium::ft2link::W2300DecodeMetrics;
    using decodium::ft2link::W2300RateController;
    using decodium::ft2link::W2300RateMode;

    W2300RateController controller;
    W2300DecodeMetrics metrics;
    metrics.rateMode = W2300RateMode::Fast;
    metrics.quality = 0.42;
    metrics.estimatedFrequencyOffsetHz = 6.0;
    controller.observe (metrics);
    QCOMPARE (static_cast<int> (controller.currentMode ()),
              static_cast<int> (W2300RateMode::Robust));
    QCOMPARE (static_cast<int> (controller.configForAttempt (1).rateMode),
              static_cast<int> (W2300RateMode::Robust));

    metrics.rateMode = W2300RateMode::Robust;
    metrics.quality = 0.82;
    metrics.estimatedFrequencyOffsetHz = 4.0;
    controller.observe (metrics);
    QCOMPARE (static_cast<int> (controller.currentMode ()),
              static_cast<int> (W2300RateMode::Fast));
    QCOMPARE (static_cast<int> (controller.configForAttempt (1).rateMode),
              static_cast<int> (W2300RateMode::Fast));
  }

  void w2300OfflinePipelineCompletesCleanExchange ()
  {
    using decodium::ft2link::W2300OfflinePipelineOptions;
    using decodium::ft2link::W2300OfflinePipelineResult;
    using decodium::ft2link::W2300RateMode;

    std::string text;
    for (int i = 0; i < 5; ++i)
      {
        text += "Offline W2300 pipeline moves frames through waveform decode and ACK. ";
      }
    std::vector<std::uint8_t> const payload = bytesFromString (text);

    W2300OfflinePipelineOptions options;
    options.windowSize = 1;
    options.retryMs = 500;
    options.maxIterations = 20;
    options.leadingSamples = 23;
    options.trailingSamples = 41;
    options.noiseAmplitude = 0.004f;

    W2300OfflinePipelineResult const result = decodium::ft2link::runW2300OfflinePipeline (
        payload, 0x4400u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.receivedMessage == payload);
    QVERIFY (result.bursts.size () >= 2u);
    for (decodium::ft2link::W2300OfflineBurstTrace const& burst : result.bursts)
      {
        QVERIFY (burst.decoded);
        QCOMPARE (static_cast<int> (burst.rateMode), static_cast<int> (W2300RateMode::Fast));
        QCOMPARE (burst.attempt, 1);
      }
  }

  void w2300OfflinePipelineRetriesDroppedFrameInRobustMode ()
  {
    using decodium::ft2link::W2300OfflinePipelineOptions;
    using decodium::ft2link::W2300OfflinePipelineResult;
    using decodium::ft2link::W2300RateMode;

    std::string text;
    for (int i = 0; i < 6; ++i)
      {
        text += "Dropped W2300 frame should be retried as ROBUST. ";
      }
    std::vector<std::uint8_t> const payload = bytesFromString (text);

    W2300OfflinePipelineOptions options;
    options.windowSize = 1;
    options.retryMs = 500;
    options.maxAttempts = 4;
    options.maxIterations = 24;
    options.leadingSamples = 17;
    options.trailingSamples = 29;
    options.noiseAmplitude = 0.003f;
    options.dropFirstAttemptSequences.push_back (1u);

    W2300OfflinePipelineResult const result = decodium::ft2link::runW2300OfflinePipeline (
        payload, 0x4401u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.receivedMessage == payload);

    bool sawDroppedSequenceOne = false;
    bool sawRobustRetrySequenceOne = false;
    for (decodium::ft2link::W2300OfflineBurstTrace const& burst : result.bursts)
      {
        if (burst.sequence == 1u && burst.attempt == 1 && burst.dropped)
          {
            sawDroppedSequenceOne = true;
          }
        if (burst.sequence == 1u && burst.attempt == 2 && burst.decoded)
          {
            sawRobustRetrySequenceOne = burst.rateMode == W2300RateMode::Robust
                && burst.metrics.rateMode == W2300RateMode::Robust
                && burst.metrics.repetitionFactor == 3;
          }
      }
    QVERIFY (sawDroppedSequenceOne);
    QVERIFY (sawRobustRetrySequenceOne);
  }

  void w2300AudioPipelineCompletesChunkedStream ()
  {
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300AudioBurstTrace;
    using decodium::ft2link::W2300AudioPipelineOptions;
    using decodium::ft2link::W2300AudioPipelineResult;
    using decodium::ft2link::W2300RateMode;

    std::string text;
    for (int i = 0; i < 5; ++i)
      {
        text += "Audio-buffer W2300 pipeline feeds decoder chunks and ACKs frames. ";
      }
    std::vector<std::uint8_t> const payload = bytesFromString (text);

    W2300AudioPipelineOptions options;
    options.windowSize = 1;
    options.retryMs = 500;
    options.maxIterations = 20;
    options.guardBeforeSamples = 31;
    options.guardAfterSamples = 47;
    options.interBurstGapSamples = 97;
    options.rxChunkSamples = 211;
    options.noiseAmplitude = 0.003f;

    W2300AudioPipelineResult const result = decodium::ft2link::runW2300AudioPipeline (
        payload, 0x5500u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.receivedMessage == payload);
    QVERIFY (result.totalSamples > 0u);
    QVERIFY (result.bursts.size () >= 2u);
    QCOMPARE (static_cast<int> (result.throughput.profile),
              static_cast<int> (Profile::Wide2300));
    QCOMPARE (result.throughput.payloadBytes, payload.size ());
    QCOMPARE (result.throughput.burstCount, result.bursts.size ());
    QCOMPARE (result.throughput.droppedBurstCount, static_cast<std::size_t> (0));
    QCOMPARE (result.throughput.retryBurstCount, static_cast<std::size_t> (0));
    QVERIFY (result.throughput.sessionDurationMs > 0u);
    QVERIFY (result.throughput.activeTransmitMs > 0u);
    QVERIFY (result.throughput.effectivePayloadBytesPerSecond > 0.0);
    QVERIFY (result.throughput.activePayloadBytesPerSecond
             >= result.throughput.effectivePayloadBytesPerSecond);
    QVERIFY (result.throughput.channelUtilization > 0.0);
    QVERIFY (result.throughput.channelUtilization <= 1.0);

    std::size_t previousStart = 0;
    for (std::size_t i = 0; i < result.bursts.size (); ++i)
      {
        W2300AudioBurstTrace const& burst = result.bursts[i];
        QVERIFY (burst.decoded);
        QCOMPARE (burst.attempt, 1);
        QCOMPARE (static_cast<int> (burst.rateMode), static_cast<int> (W2300RateMode::Fast));
        QVERIFY (burst.sampleCount > 0u);
        if (i > 0)
          {
            QVERIFY (burst.startSample > previousStart);
          }
        previousStart = burst.startSample;
      }
  }

  void w2300AudioPipelineRetriesDroppedFrameInRobustMode ()
  {
    using decodium::ft2link::W2300AudioBurstTrace;
    using decodium::ft2link::W2300AudioPipelineOptions;
    using decodium::ft2link::W2300AudioPipelineResult;
    using decodium::ft2link::W2300RateMode;

    std::string text;
    for (int i = 0; i < 6; ++i)
      {
        text += "Audio scheduling should retry a dropped frame using ROBUST. ";
      }
    std::vector<std::uint8_t> const payload = bytesFromString (text);

    W2300AudioPipelineOptions options;
    options.windowSize = 1;
    options.retryMs = 500;
    options.maxAttempts = 4;
    options.maxIterations = 24;
    options.guardBeforeSamples = 23;
    options.guardAfterSamples = 37;
    options.interBurstGapSamples = 83;
    options.rxChunkSamples = 157;
    options.noiseAmplitude = 0.002f;
    options.dropFirstAttemptSequences.push_back (1u);

    W2300AudioPipelineResult const result = decodium::ft2link::runW2300AudioPipeline (
        payload, 0x5501u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.receivedMessage == payload);

    bool sawDroppedSequenceOne = false;
    bool sawRobustRetrySequenceOne = false;
    for (W2300AudioBurstTrace const& burst : result.bursts)
      {
        if (burst.sequence == 1u && burst.attempt == 1 && burst.dropped)
          {
            sawDroppedSequenceOne = true;
          }
        if (burst.sequence == 1u && burst.attempt == 2 && burst.decoded)
          {
            sawRobustRetrySequenceOne = burst.rateMode == W2300RateMode::Robust
                && burst.metrics.rateMode == W2300RateMode::Robust
                && burst.metrics.repetitionFactor == 3;
          }
      }
    QVERIFY (sawDroppedSequenceOne);
    QVERIFY (sawRobustRetrySequenceOne);
    QVERIFY (result.throughput.droppedBurstCount >= 1u);
    QVERIFY (result.throughput.retryBurstCount >= 1u);
  }

  void w2300AudioPipelineRunsHandshakeBeforeData ()
  {
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300AudioPipelineOptions;
    using decodium::ft2link::W2300AudioPipelineResult;
    using decodium::ft2link::W2300RateMode;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "HELLO HELLO_ACK DATA ACK audio pipeline path.");

    W2300AudioPipelineOptions options;
    options.performHandshake = true;
    options.maxIterations = 12;
    options.rxChunkSamples = 199;
    options.noiseAmplitude = 0.001f;
    options.initiatorCapabilities.preferredW2300RateMode = W2300RateMode::Robust;
    options.responderCapabilities.preferredW2300RateMode = W2300RateMode::Robust;

    W2300AudioPipelineResult const result = decodium::ft2link::runW2300AudioPipeline (
        payload, 0x5503u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.handshakeAttempted);
    QVERIFY (result.handshakeAccepted);
    QCOMPARE (result.handshakeFrames.size (), static_cast<std::size_t> (2));
    QCOMPARE (static_cast<int> (result.handshakeFrames[0].type),
              static_cast<int> (FrameType::Hello));
    QCOMPARE (static_cast<int> (result.handshakeFrames[0].profile),
              static_cast<int> (Profile::Narrow));
    QCOMPARE (static_cast<int> (result.handshakeFrames[1].type),
              static_cast<int> (FrameType::HelloAck));
    QCOMPARE (static_cast<int> (result.negotiatedLink.profile),
              static_cast<int> (Profile::Wide2300));
    QCOMPARE (static_cast<int> (result.negotiatedLink.w2300RateMode),
              static_cast<int> (W2300RateMode::Robust));
    QVERIFY (result.receivedMessage == payload);
    QVERIFY (!result.bursts.empty ());
    QCOMPARE (result.bursts[0].attempt, 1);
    QCOMPARE (static_cast<int> (result.bursts[0].rateMode),
              static_cast<int> (W2300RateMode::Robust));
    QVERIFY (result.bursts[0].decoded);
  }

  void w2300AudioPipelineModelsHalfDuplexAckAudio ()
  {
    using decodium::ft2link::AudioAckTrace;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300AudioPipelineOptions;
    using decodium::ft2link::W2300AudioPipelineResult;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "Half-duplex ACK audio model.");

    W2300AudioPipelineOptions baselineOptions;
    baselineOptions.maxIterations = 8;
    baselineOptions.guardBeforeSamples = 0;
    baselineOptions.guardAfterSamples = 0;
    baselineOptions.interBurstGapSamples = 0;

    W2300AudioPipelineResult const baseline = decodium::ft2link::runW2300AudioPipeline (
        payload, 0x5508u, baselineOptions);
    QVERIFY2 (baseline.complete, baseline.error.c_str ());
    QVERIFY (!baseline.failed);
    QCOMPARE (baseline.ackBursts.size (), static_cast<std::size_t> (0));
    QCOMPARE (baseline.throughput.ackBurstCount, static_cast<std::size_t> (0));

    W2300AudioPipelineOptions options = baselineOptions;
    options.modelAckAudio = true;
    options.dataToAckTurnaroundMs = 120;
    options.ackToDataTurnaroundMs = 180;

    W2300AudioPipelineResult const result = decodium::ft2link::runW2300AudioPipeline (
        payload, 0x5509u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.receivedMessage == payload);
    QCOMPARE (result.ackBursts.size (), result.bursts.size ());
    QCOMPARE (result.throughput.ackBurstCount, result.ackBursts.size ());
    QCOMPARE (result.throughput.decodedAckBurstCount, result.ackBursts.size ());
    QVERIFY (result.throughput.ackTransmitMs > 0u);
    QVERIFY (result.throughput.activeTransmitMs
             == result.throughput.dataTransmitMs + result.throughput.ackTransmitMs);
    QVERIFY (result.throughput.sessionDurationMs > baseline.throughput.sessionDurationMs);
    QVERIFY (result.throughput.effectivePayloadBytesPerSecond
             < baseline.throughput.effectivePayloadBytesPerSecond);

    for (AudioAckTrace const& ack : result.ackBursts)
      {
        QCOMPARE (static_cast<int> (ack.profile), static_cast<int> (Profile::Wide2300));
        QVERIFY (ack.decoded);
        QVERIFY (ack.sampleCount > 0u);
        QVERIFY (ack.transmitStartMs >= result.bursts[0].transmitEndMs);
      }
  }

  void w2300AudioPipelineRetriesWhenAckAudioIsLost ()
  {
    using decodium::ft2link::W2300AudioBurstTrace;
    using decodium::ft2link::W2300AudioPipelineOptions;
    using decodium::ft2link::W2300AudioPipelineResult;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "DATA is received, but the first ACK is lost.");

    W2300AudioPipelineOptions options;
    options.modelAckAudio = true;
    options.retryMs = 300;
    options.maxAttempts = 4;
    options.maxIterations = 12;
    options.guardBeforeSamples = 0;
    options.guardAfterSamples = 0;
    options.interBurstGapSamples = 0;
    options.dataToAckTurnaroundMs = 50;
    options.ackToDataTurnaroundMs = 50;
    options.dropFirstAckForSequences.push_back (0u);

    W2300AudioPipelineResult const result = decodium::ft2link::runW2300AudioPipeline (
        payload, 0x5511u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.receivedMessage == payload);
    QVERIFY (result.bursts.size () >= 2u);
    QVERIFY (result.ackBursts.size () >= 2u);
    QCOMPARE (result.throughput.droppedBurstCount, static_cast<std::size_t> (0));
    QCOMPARE (result.throughput.droppedAckBurstCount, static_cast<std::size_t> (1));
    QVERIFY (result.throughput.retryBurstCount >= 1u);
    QVERIFY (result.throughput.decodedAckBurstCount >= 1u);

    bool sawInitialData = false;
    bool sawRetryData = false;
    for (W2300AudioBurstTrace const& burst : result.bursts)
      {
        if (burst.sequence == 0u && burst.attempt == 1)
          {
            sawInitialData = burst.decoded && !burst.dropped;
          }
        if (burst.sequence == 0u && burst.attempt == 2)
          {
            sawRetryData = burst.decoded && !burst.dropped;
          }
      }
    QVERIFY (sawInitialData);
    QVERIFY (sawRetryData);
    QVERIFY (result.ackBursts[0].dropped);
    QVERIFY (!result.ackBursts[0].decoded);
    QVERIFY (result.ackBursts[1].decoded);
  }

  void w2300AudioPipelineWindowUsesAckBitmapWithMixedLosses ()
  {
    using decodium::ft2link::AudioAckTrace;
    using decodium::ft2link::W2300AudioBurstTrace;
    using decodium::ft2link::W2300AudioPipelineOptions;
    using decodium::ft2link::W2300AudioPipelineResult;

    std::string text (1000, 'W');
    std::vector<std::uint8_t> const payload = bytesFromString (text);

    W2300AudioPipelineOptions options;
    options.windowSize = 4;
    options.modelAckAudio = true;
    options.retryMs = 300;
    options.maxAttempts = 5;
    options.maxIterations = 20;
    options.guardBeforeSamples = 0;
    options.guardAfterSamples = 0;
    options.interBurstGapSamples = 0;
    options.dataToAckTurnaroundMs = 50;
    options.ackToDataTurnaroundMs = 50;
    options.dropFirstAttemptSequences.push_back (1u);
    options.dropFirstAckForSequences.push_back (4u);

    W2300AudioPipelineResult const result = decodium::ft2link::runW2300AudioPipeline (
        payload, 0x5513u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.receivedMessage == payload);
    QCOMPARE (result.throughput.droppedBurstCount, static_cast<std::size_t> (1));
    QCOMPARE (result.throughput.droppedAckBurstCount, static_cast<std::size_t> (1));
    QVERIFY (result.throughput.retryBurstCount >= 2u);

    bool sawSequenceOneDrop = false;
    bool sawSequenceOneRetry = false;
    bool sawSequenceFourAckRetry = false;
    for (W2300AudioBurstTrace const& burst : result.bursts)
      {
        if (burst.sequence == 1u && burst.attempt == 1)
          {
            sawSequenceOneDrop = burst.dropped;
          }
        if (burst.sequence == 1u && burst.attempt == 2)
          {
            sawSequenceOneRetry = burst.decoded && !burst.dropped;
          }
        if (burst.sequence == 4u && burst.attempt == 2)
          {
            sawSequenceFourAckRetry = burst.decoded && !burst.dropped;
          }
      }
    QVERIFY (sawSequenceOneDrop);
    QVERIFY (sawSequenceOneRetry);
    QVERIFY (sawSequenceFourAckRetry);

    bool sawSparseBitmap = false;
    bool sawDroppedAckForSequenceFour = false;
    bool sawDecodedRetryAckForSequenceFour = false;
    for (AudioAckTrace const& ack : result.ackBursts)
      {
        if (ack.ackBase == 1u && (ack.ackBitmap & 0x0006u) == 0x0006u)
          {
            sawSparseBitmap = true;
          }
        if (ack.sourceSequence == 4u && ack.sourceAttempt == 1)
          {
            sawDroppedAckForSequenceFour = ack.dropped && !ack.decoded
                && ack.ackBase >= 5u;
          }
        if (ack.sourceSequence == 4u && ack.sourceAttempt == 2)
          {
            sawDecodedRetryAckForSequenceFour = ack.decoded && !ack.dropped
                && ack.ackBase >= 5u;
          }
      }
    QVERIFY (sawSparseBitmap);
    QVERIFY (sawDroppedAckForSequenceFour);
    QVERIFY (sawDecodedRetryAckForSequenceFour);
  }

  void w2300AudioPipelineStopsWhenHandshakeNegotiatesW500 ()
  {
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300AudioPipelineOptions;
    using decodium::ft2link::W2300AudioPipelineResult;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "W500 negotiation is valid but not supported by the W2300 audio path.");

    W2300AudioPipelineOptions options;
    options.performHandshake = true;
    options.initiatorCapabilities.supportsW2300 = true;
    options.initiatorCapabilities.supportsW500 = true;
    options.initiatorCapabilities.preferredProfile = Profile::Wide2300;
    options.responderCapabilities.supportsW2300 = false;
    options.responderCapabilities.supportsW2300Fast = false;
    options.responderCapabilities.supportsW2300Robust = false;
    options.responderCapabilities.supportsW500 = true;
    options.responderCapabilities.preferredProfile = Profile::Wide500;

    W2300AudioPipelineResult const result = decodium::ft2link::runW2300AudioPipeline (
        payload, 0x5504u, options);
    QVERIFY (!result.complete);
    QVERIFY (result.failed);
    QVERIFY (result.handshakeAttempted);
    QVERIFY (result.handshakeAccepted);
    QCOMPARE (static_cast<int> (result.negotiatedLink.profile),
              static_cast<int> (Profile::Wide500));
    QCOMPARE (result.handshakeFrames.size (), static_cast<std::size_t> (2));
    QVERIFY (result.bursts.empty ());
    QVERIFY (result.error.find ("W2300") != std::string::npos);
  }

  void wideAudioPipelineFallsBackToW500AfterHandshake ()
  {
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;
    using decodium::ft2link::WideAudioBurstTrace;
    using decodium::ft2link::WideAudioPipelineOptions;
    using decodium::ft2link::WideAudioPipelineResult;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "W500 fallback after handshake.");

    WideAudioPipelineOptions options;
    options.maxIterations = 8;
    options.guardBeforeSamples = 19;
    options.guardAfterSamples = 23;
    options.interBurstGapSamples = 0;
    options.initiatorCapabilities.supportsW2300 = true;
    options.initiatorCapabilities.supportsW500 = true;
    options.initiatorCapabilities.preferredProfile = Profile::Wide2300;
    options.responderCapabilities.supportsW2300 = false;
    options.responderCapabilities.supportsW2300Fast = false;
    options.responderCapabilities.supportsW2300Robust = false;
    options.responderCapabilities.supportsW500 = true;
    options.responderCapabilities.preferredProfile = Profile::Wide500;

    WideAudioPipelineResult const result = decodium::ft2link::runWideAudioPipeline (
        payload, 0x5505u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.handshakeAttempted);
    QVERIFY (result.handshakeAccepted);
    QCOMPARE (result.handshakeFrames.size (), static_cast<std::size_t> (2));
    QCOMPARE (static_cast<int> (result.handshakeFrames[0].type),
              static_cast<int> (FrameType::Hello));
    QCOMPARE (static_cast<int> (result.handshakeFrames[1].type),
              static_cast<int> (FrameType::HelloAck));
    QCOMPARE (static_cast<int> (result.negotiatedLink.profile),
              static_cast<int> (Profile::Wide500));
    QVERIFY (result.receivedMessage == payload);
    QVERIFY (!result.bursts.empty ());
    QCOMPARE (static_cast<int> (result.throughput.profile),
              static_cast<int> (Profile::Wide500));
    QCOMPARE (result.throughput.payloadBytes, payload.size ());
    QCOMPARE (result.throughput.burstCount, result.bursts.size ());
    QCOMPARE (result.throughput.droppedBurstCount, static_cast<std::size_t> (0));
    QCOMPARE (result.throughput.retryBurstCount, static_cast<std::size_t> (0));
    QVERIFY (result.throughput.effectivePayloadBytesPerSecond > 0.0);
    QVERIFY (result.throughput.activePayloadBytesPerSecond > 0.0);

    for (WideAudioBurstTrace const& burst : result.bursts)
      {
        QCOMPARE (static_cast<int> (burst.profile), static_cast<int> (Profile::Wide500));
        QVERIFY (burst.decoded);
        QCOMPARE (burst.attempt, 1);
        QVERIFY (burst.sampleCount > 0u);
        QVERIFY (burst.w500Metrics.packetBytes > 0u);
      }
  }

  void wideAudioPipelineReportsProfileThroughput ()
  {
    using decodium::ft2link::Profile;
    using decodium::ft2link::WideAudioPipelineOptions;
    using decodium::ft2link::WideAudioPipelineResult;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "Same payload goes through W500 and W2300 throughput estimation.");

    WideAudioPipelineOptions w2300Options;
    w2300Options.performHandshake = false;
    w2300Options.profile = Profile::Wide2300;
    w2300Options.maxIterations = 8;
    w2300Options.guardBeforeSamples = 0;
    w2300Options.guardAfterSamples = 0;
    w2300Options.interBurstGapSamples = 0;

    WideAudioPipelineResult const w2300 = decodium::ft2link::runWideAudioPipeline (
        payload, 0x5506u, w2300Options);
    QVERIFY2 (w2300.complete, w2300.error.c_str ());
    QVERIFY (!w2300.failed);
    QCOMPARE (static_cast<int> (w2300.throughput.profile),
              static_cast<int> (Profile::Wide2300));
    QCOMPARE (w2300.throughput.payloadBytes, payload.size ());
    QVERIFY (w2300.throughput.activePayloadBytesPerSecond > 0.0);

    WideAudioPipelineOptions w500Options;
    w500Options.performHandshake = false;
    w500Options.profile = Profile::Wide500;
    w500Options.maxIterations = 8;
    w500Options.guardBeforeSamples = 0;
    w500Options.guardAfterSamples = 0;
    w500Options.interBurstGapSamples = 0;

    WideAudioPipelineResult const w500 = decodium::ft2link::runWideAudioPipeline (
        payload, 0x5507u, w500Options);
    QVERIFY2 (w500.complete, w500.error.c_str ());
    QVERIFY (!w500.failed);
    QCOMPARE (static_cast<int> (w500.throughput.profile),
              static_cast<int> (Profile::Wide500));
    QCOMPARE (w500.throughput.payloadBytes, payload.size ());
    QVERIFY (w500.throughput.activePayloadBytesPerSecond > 0.0);
    QVERIFY (w2300.throughput.activePayloadBytesPerSecond
             > w500.throughput.activePayloadBytesPerSecond);
  }

  void wideAudioPipelineModelsW500AudioAck ()
  {
    using decodium::ft2link::AudioAckTrace;
    using decodium::ft2link::Profile;
    using decodium::ft2link::WideAudioPipelineOptions;
    using decodium::ft2link::WideAudioPipelineResult;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "W500 ACK audio is slower but explicit.");

    WideAudioPipelineOptions options;
    options.performHandshake = false;
    options.profile = Profile::Wide500;
    options.modelAckAudio = true;
    options.maxIterations = 8;
    options.guardBeforeSamples = 0;
    options.guardAfterSamples = 0;
    options.interBurstGapSamples = 0;
    options.dataToAckTurnaroundMs = 150;
    options.ackToDataTurnaroundMs = 200;

    WideAudioPipelineResult const result = decodium::ft2link::runWideAudioPipeline (
        payload, 0x5510u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QCOMPARE (static_cast<int> (result.throughput.profile),
              static_cast<int> (Profile::Wide500));
    QCOMPARE (result.ackBursts.size (), result.bursts.size ());
    QCOMPARE (result.throughput.ackBurstCount, result.ackBursts.size ());
    QCOMPARE (result.throughput.decodedAckBurstCount, result.ackBursts.size ());
    QVERIFY (result.throughput.ackTransmitMs > 0u);
    QVERIFY (result.throughput.channelUtilization > 0.0);
    QVERIFY (result.throughput.channelUtilization < 1.0);

    for (AudioAckTrace const& ack : result.ackBursts)
      {
        QCOMPARE (static_cast<int> (ack.profile), static_cast<int> (Profile::Wide500));
        QVERIFY (ack.decoded);
        QVERIFY (ack.w500Metrics.packetBytes > 0u);
      }
  }

  void wideAudioPipelineRetriesW500WhenAckAudioIsLost ()
  {
    using decodium::ft2link::Profile;
    using decodium::ft2link::WideAudioBurstTrace;
    using decodium::ft2link::WideAudioPipelineOptions;
    using decodium::ft2link::WideAudioPipelineResult;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "W500 DATA duplicate is safe after a lost ACK.");

    WideAudioPipelineOptions options;
    options.performHandshake = false;
    options.profile = Profile::Wide500;
    options.modelAckAudio = true;
    options.retryMs = 300;
    options.maxAttempts = 4;
    options.maxIterations = 12;
    options.guardBeforeSamples = 0;
    options.guardAfterSamples = 0;
    options.interBurstGapSamples = 0;
    options.dataToAckTurnaroundMs = 50;
    options.ackToDataTurnaroundMs = 50;
    options.dropFirstAckForSequences.push_back (0u);

    WideAudioPipelineResult const result = decodium::ft2link::runWideAudioPipeline (
        payload, 0x5512u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.receivedMessage == payload);
    QVERIFY (result.bursts.size () >= 2u);
    QVERIFY (result.ackBursts.size () >= 2u);
    QCOMPARE (result.throughput.droppedBurstCount, static_cast<std::size_t> (0));
    QCOMPARE (result.throughput.droppedAckBurstCount, static_cast<std::size_t> (1));
    QVERIFY (result.throughput.retryBurstCount >= 1u);

    bool sawInitialData = false;
    bool sawRetryData = false;
    for (WideAudioBurstTrace const& burst : result.bursts)
      {
        if (burst.sequence == 0u && burst.attempt == 1)
          {
            sawInitialData = burst.decoded && !burst.dropped;
          }
        if (burst.sequence == 0u && burst.attempt == 2)
          {
            sawRetryData = burst.decoded && !burst.dropped;
          }
      }
    QVERIFY (sawInitialData);
    QVERIFY (sawRetryData);
    QVERIFY (result.ackBursts[0].dropped);
    QVERIFY (!result.ackBursts[0].decoded);
    QVERIFY (result.ackBursts[1].decoded);
  }

  void wideAudioPipelineWindowedW500UsesAckBitmapWithMixedLosses ()
  {
    using decodium::ft2link::AudioAckTrace;
    using decodium::ft2link::Profile;
    using decodium::ft2link::WideAudioBurstTrace;
    using decodium::ft2link::WideAudioPipelineOptions;
    using decodium::ft2link::WideAudioPipelineResult;

    std::string text (220, '5');
    std::vector<std::uint8_t> const payload = bytesFromString (text);

    WideAudioPipelineOptions options;
    options.performHandshake = false;
    options.profile = Profile::Wide500;
    options.windowSize = 4;
    options.modelAckAudio = true;
    options.retryMs = 300;
    options.maxAttempts = 5;
    options.maxIterations = 24;
    options.guardBeforeSamples = 0;
    options.guardAfterSamples = 0;
    options.interBurstGapSamples = 0;
    options.dataToAckTurnaroundMs = 50;
    options.ackToDataTurnaroundMs = 50;
    options.dropFirstAttemptSequences.push_back (1u);
    options.dropFirstAckForSequences.push_back (4u);

    WideAudioPipelineResult const result = decodium::ft2link::runWideAudioPipeline (
        payload, 0x5514u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.receivedMessage == payload);
    QCOMPARE (result.throughput.droppedBurstCount, static_cast<std::size_t> (1));
    QCOMPARE (result.throughput.droppedAckBurstCount, static_cast<std::size_t> (1));
    QVERIFY (result.throughput.retryBurstCount >= 2u);

    bool sawSequenceOneDrop = false;
    bool sawSequenceOneRetry = false;
    bool sawSequenceFourAckRetry = false;
    for (WideAudioBurstTrace const& burst : result.bursts)
      {
        if (burst.sequence == 1u && burst.attempt == 1)
          {
            sawSequenceOneDrop = burst.dropped;
          }
        if (burst.sequence == 1u && burst.attempt == 2)
          {
            sawSequenceOneRetry = burst.decoded && !burst.dropped;
          }
        if (burst.sequence == 4u && burst.attempt == 2)
          {
            sawSequenceFourAckRetry = burst.decoded && !burst.dropped;
          }
      }
    QVERIFY (sawSequenceOneDrop);
    QVERIFY (sawSequenceOneRetry);
    QVERIFY (sawSequenceFourAckRetry);

    bool sawSparseBitmap = false;
    bool sawDroppedAckForSequenceFour = false;
    bool sawDecodedRetryAckForSequenceFour = false;
    for (AudioAckTrace const& ack : result.ackBursts)
      {
        if (ack.ackBase == 1u && (ack.ackBitmap & 0x0006u) == 0x0006u)
          {
            sawSparseBitmap = true;
          }
        if (ack.sourceSequence == 4u && ack.sourceAttempt == 1)
          {
            sawDroppedAckForSequenceFour = ack.dropped && !ack.decoded
                && ack.ackBase >= 5u;
          }
        if (ack.sourceSequence == 4u && ack.sourceAttempt == 2)
          {
            sawDecodedRetryAckForSequenceFour = ack.decoded && !ack.dropped
                && ack.ackBase >= 5u;
          }
      }
    QVERIFY (sawSparseBitmap);
    QVERIFY (sawDroppedAckForSequenceFour);
    QVERIFY (sawDecodedRetryAckForSequenceFour);
  }

  void wideTxAudioPlanBuildsDecodableW2300At48k ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300RateMode;
    using decodium::ft2link::W2300WaveformConfig;
    using decodium::ft2link::WideAudioBurstTrace;
    using decodium::ft2link::WideTxAudioPlan;
    using decodium::ft2link::WideTxAudioPlanOptions;

    std::string text = "W2300 radio TX plan ";
    text += std::string (260, 'R');
    std::vector<std::uint8_t> const payload = bytesFromString (text);

    WideTxAudioPlanOptions options;
    options.profile = Profile::Wide2300;
    options.w2300RateMode = W2300RateMode::Fast;
    options.sampleRate = 48000.0;
    options.guardBeforeSamples = 240;
    options.guardAfterSamples = 240;
    options.interBurstGapSamples = 480;

    WideTxAudioPlan const plan = decodium::ft2link::buildWideTxAudioPlan (
        payload, 0x6610u, options);
    QVERIFY2 (plan.ok, plan.error.c_str ());
    QCOMPARE (static_cast<int> (plan.profile), static_cast<int> (Profile::Wide2300));
    QCOMPARE (plan.sampleRate, 48000.0);
    QCOMPARE (plan.frames.size (), static_cast<std::size_t> (2));
    QCOMPARE (plan.bursts.size (), plan.frames.size ());
    QCOMPARE (plan.samples.size (), plan.totalSamples);
    QVERIFY (!plan.samples.empty ());
    QVERIFY (plan.throughput.activePayloadBytesPerSecond > 0.0);

    WideAudioBurstTrace const& firstBurst = plan.bursts.front ();
    std::vector<float> firstWave (
        plan.samples.begin () + static_cast<std::vector<float>::difference_type> (firstBurst.startSample),
        plan.samples.begin () + static_cast<std::vector<float>::difference_type> (
            firstBurst.startSample + firstBurst.sampleCount));

    W2300WaveformConfig decodeConfig;
    decodeConfig.sampleRate = 48000.0;
    decodeConfig.rateMode = W2300RateMode::Fast;

    Frame decoded;
    std::string error;
    QVERIFY2 (decodium::ft2link::decodeW2300FrameWaveform (
                  firstWave, &decoded, decodeConfig, &error),
              error.c_str ());
    QCOMPARE (decoded.sessionId, static_cast<std::uint16_t> (0x6610u));
    QCOMPARE (decoded.sequence, static_cast<std::uint16_t> (0u));
    QCOMPARE (static_cast<int> (decoded.profile), static_cast<int> (Profile::Wide2300));
    QVERIFY (decoded.payload == plan.frames.front ().payload);
  }

  void wideRxAudioBufferDecodesCombinedW2300WindowInSequence ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::OutboundTransfer;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300DecodeMetrics;
    using decodium::ft2link::W2300RateMode;
    using decodium::ft2link::W2300RxAudioBuffer;
    using decodium::ft2link::W2300WaveformConfig;
    using decodium::ft2link::WideTxAudioPlan;
    using decodium::ft2link::WideTxAudioPlanOptions;

    std::vector<std::uint8_t> payload (1400u, 0x46u);
    OutboundTransfer tx {Profile::Wide2300, 0x6611u, payload};
    tx.setWindowSize (4u);
    std::vector<Frame> const frames = tx.framesToSend (0u);
    QCOMPARE (frames.size (), static_cast<std::size_t> (4u));
    for (Frame const& frame : frames)
      {
        QVERIFY ((frame.flags & decodium::ft2link::FlagEndOfMessage) == 0u);
      }

    WideTxAudioPlanOptions options;
    options.profile = Profile::Wide2300;
    options.w2300RateMode = W2300RateMode::Fast;
    options.sampleRate = 48000.0;
    WideTxAudioPlan const plan =
        decodium::ft2link::buildWideTxAudioPlanForFrames (frames, options);
    QVERIFY2 (plan.ok, plan.error.c_str ());

    std::vector<float> pcmWave = plan.samples;
    for (float& sample : pcmWave)
      {
        int const pcm = qBound (-32768, qRound (sample * 30000.0f), 32767);
        sample = static_cast<float> (static_cast<short> (pcm)) / 32768.0f;
      }
    std::vector<float> rxWave = fil4Decimate48kTo12k (pcmWave);
    rxWave.insert (rxWave.end (), 8000u, 0.0f);

    W2300WaveformConfig config;
    config.sampleRate = 12000.0;
    config.rateMode = W2300RateMode::Fast;
    config.maxDecodeMillis = 2500;
    W2300RxAudioBuffer rx {config};
    rx.append (rxWave);

    for (std::uint16_t sequence = 0u; sequence < 4u; ++sequence)
      {
        Frame decoded;
        W2300DecodeMetrics metrics;
        std::string error;
        QVERIFY2 (rx.decodeNext (&decoded, &metrics, &error), error.c_str ());
        QCOMPARE (decoded.sessionId, static_cast<std::uint16_t> (0x6611u));
        QCOMPARE (decoded.sequence, sequence);
        QVERIFY (decoded.payload == frames[sequence].payload);
      }
  }

  void wideTxAudioPlanDecodesShortW2300WithRuntimeGuard ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W2300DecodeMetrics;
    using decodium::ft2link::W2300RateMode;
    using decodium::ft2link::W2300RxAudioBuffer;
    using decodium::ft2link::W2300WaveformConfig;
    using decodium::ft2link::WideTxAudioPlan;
    using decodium::ft2link::WideTxAudioPlanOptions;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "W2300 lab payload");

    WideTxAudioPlanOptions options;
    options.profile = Profile::Wide2300;
    options.w2300RateMode = W2300RateMode::Fast;
    options.sampleRate = 48000.0;

    WideTxAudioPlan const plan = decodium::ft2link::buildWideTxAudioPlan (
        payload, 0x7000u, options);
    QVERIFY2 (plan.ok, plan.error.c_str ());
    QCOMPARE (plan.frames.size (), static_cast<std::size_t> (1));

    std::vector<float> guarded = paddedWave (
        plan.samples, 5760u, 32000u);
    for (float& sample : guarded)
      {
        int const pcm = qBound (-32768, qRound (sample * 30000.0f), 32767);
        sample = static_cast<float> (static_cast<short> (pcm)) / 32768.0f;
      }
    W2300WaveformConfig config;
    config.sampleRate = 48000.0;
    config.rateMode = W2300RateMode::Fast;
    W2300RxAudioBuffer rx {config};
    rx.append (guarded);

    Frame decoded;
    W2300DecodeMetrics metrics;
    std::string error;
    QVERIFY2 (rx.decodeNext (&decoded, &metrics, &error), error.c_str ());
    QCOMPARE (decoded.sessionId, static_cast<std::uint16_t> (0x7000u));
    QCOMPARE (decoded.sequence, static_cast<std::uint16_t> (0u));
    QCOMPARE (static_cast<int> (decoded.profile),
              static_cast<int> (Profile::Wide2300));
    QVERIFY (decoded.payload == plan.frames.front ().payload);
  }

  void wideTxAudioPlanBuildsDecodableW500At48k ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W500WaveformConfig;
    using decodium::ft2link::WideAudioBurstTrace;
    using decodium::ft2link::WideTxAudioPlan;
    using decodium::ft2link::WideTxAudioPlanOptions;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "W500 radio TX plan keeps the narrow fallback portable.");

    WideTxAudioPlanOptions options;
    options.profile = Profile::Wide500;
    options.sampleRate = 48000.0;
    options.guardBeforeSamples = 240;
    options.guardAfterSamples = 240;
    options.interBurstGapSamples = 480;

    WideTxAudioPlan const plan = decodium::ft2link::buildWideTxAudioPlan (
        payload, 0x6611u, options);
    QVERIFY2 (plan.ok, plan.error.c_str ());
    QCOMPARE (static_cast<int> (plan.profile), static_cast<int> (Profile::Wide500));
    QCOMPARE (plan.sampleRate, 48000.0);
    QCOMPARE (plan.frames.size (), static_cast<std::size_t> (2));
    QCOMPARE (plan.bursts.size (), plan.frames.size ());
    QCOMPARE (plan.samples.size (), plan.totalSamples);
    QVERIFY (!plan.samples.empty ());

    WideAudioBurstTrace const& firstBurst = plan.bursts.front ();
    std::vector<float> firstWave (
        plan.samples.begin () + static_cast<std::vector<float>::difference_type> (firstBurst.startSample),
        plan.samples.begin () + static_cast<std::vector<float>::difference_type> (
            firstBurst.startSample + firstBurst.sampleCount));

    W500WaveformConfig decodeConfig;
    decodeConfig.sampleRate = 48000.0;

    Frame decoded;
    std::string error;
    QVERIFY2 (decodium::ft2link::decodeW500FrameWaveform (
                  firstWave, &decoded, decodeConfig, &error),
              error.c_str ());
    QCOMPARE (decoded.sessionId, static_cast<std::uint16_t> (0x6611u));
    QCOMPARE (decoded.sequence, static_cast<std::uint16_t> (0u));
    QCOMPARE (static_cast<int> (decoded.profile), static_cast<int> (Profile::Wide500));
    QVERIFY (decoded.payload == plan.frames.front ().payload);
  }

  void wideTxAudioPlanCanBuildBroadcastFrames ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::Profile;
    using decodium::ft2link::W500WaveformConfig;
    using decodium::ft2link::WideAudioBurstTrace;
    using decodium::ft2link::WideTxAudioPlan;
    using decodium::ft2link::WideTxAudioPlanOptions;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "W500 broadcast payload over narrow");

    WideTxAudioPlanOptions options;
    options.profile = Profile::Wide500;
    options.frameType = FrameType::Broadcast;
    options.sampleRate = 48000.0;
    options.guardBeforeSamples = 240;
    options.guardAfterSamples = 240;
    options.interBurstGapSamples = 480;

    WideTxAudioPlan const plan = decodium::ft2link::buildWideTxAudioPlan (
        payload, 0u, options);
    QVERIFY2 (plan.ok, plan.error.c_str ());
    QCOMPARE (plan.frames.size (), static_cast<std::size_t> (1));
    QCOMPARE (static_cast<int> (plan.frames.front ().type),
              static_cast<int> (FrameType::Broadcast));
    QCOMPARE (plan.frames.front ().sessionId, static_cast<std::uint16_t> (0u));

    WideAudioBurstTrace const& burst = plan.bursts.front ();
    std::vector<float> wave (
        plan.samples.begin ()
            + static_cast<std::vector<float>::difference_type> (
                burst.startSample),
        plan.samples.begin ()
            + static_cast<std::vector<float>::difference_type> (
                burst.startSample + burst.sampleCount));

    W500WaveformConfig decodeConfig;
    decodeConfig.sampleRate = 48000.0;

    Frame decoded;
    std::string error;
    QVERIFY2 (decodium::ft2link::decodeW500FrameWaveform (
                  wave, &decoded, decodeConfig, &error),
              error.c_str ());
    QCOMPARE (static_cast<int> (decoded.type),
              static_cast<int> (FrameType::Broadcast));
    QCOMPARE (static_cast<int> (decoded.profile),
              static_cast<int> (Profile::Wide500));
    QVERIFY (decoded.payload == payload);
  }

  void w2300AudioPipelineDefersWhenChannelIsBusy ()
  {
    using decodium::ft2link::W2300AudioPipelineOptions;
    using decodium::ft2link::W2300AudioPipelineResult;
    using decodium::ft2link::W2300ChannelBusyWindow;

    std::vector<std::uint8_t> const payload = bytesFromString (
        "Listen before transmit must defer without burning ARQ attempts.");

    W2300AudioPipelineOptions options;
    options.windowSize = 1;
    options.retryMs = 100;
    options.maxIterations = 20;
    options.guardBeforeSamples = 19;
    options.guardAfterSamples = 23;
    options.interBurstGapSamples = 0;
    options.busyBackoffMs = 100;

    W2300ChannelBusyWindow externalBusy;
    externalBusy.startMs = 0;
    externalBusy.endMs = 1200;
    options.externalBusyWindows.push_back (externalBusy);

    W2300ChannelBusyWindow rxBusy;
    rxBusy.startMs = 1300;
    rxBusy.endMs = 1600;
    options.rxBusyWindows.push_back (rxBusy);

    W2300AudioPipelineResult const result = decodium::ft2link::runW2300AudioPipeline (
        payload, 0x5502u, options);
    QVERIFY2 (result.complete, result.error.c_str ());
    QVERIFY (!result.failed);
    QVERIFY (result.receivedMessage == payload);
    QVERIFY (result.deferrals.size () >= 2u);
    QVERIFY (result.deferrals[0].externalBusy);
    QCOMPARE (result.deferrals[0].resumeMs, static_cast<std::uint64_t> (1300));
    QVERIFY (result.deferrals[1].rxBusy);
    QCOMPARE (result.deferrals[1].resumeMs, static_cast<std::uint64_t> (1700));
    QVERIFY (!result.bursts.empty ());
    QCOMPARE (result.bursts[0].attempt, 1);
    QVERIFY (result.bursts[0].transmitStartMs >= 1700u);
    QVERIFY (result.bursts[0].decoded);
  }

  void arqRetransmitsLostFrameAndReassemblesMessage ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::FrameType;
    using decodium::ft2link::InboundTransfer;
    using decodium::ft2link::OutboundTransfer;
    using decodium::ft2link::Profile;

    std::string text;
    for (int i = 0; i < 20; ++i)
      {
        text += "FT2-Link W500/W2300 ARQ test block ";
      }
    std::vector<std::uint8_t> const payload = bytesFromString (text);

    OutboundTransfer tx {Profile::Wide500, 0x4567u, payload};
    tx.setWindowSize (4);
    tx.setRetryMs (1000);
    tx.setMaxAttempts (5);
    InboundTransfer rx {Profile::Wide500, 0x4567u};

    bool droppedSequenceOne = false;
    for (std::uint64_t now = 0; now < 20000 && !tx.complete () && !tx.failed (); now += 500)
      {
        std::vector<Frame> const frames = tx.framesToSend (now);
        for (Frame const& frame : frames)
          {
            if (frame.type == FrameType::Data && frame.sequence == 1u && !droppedSequenceOne)
              {
                droppedSequenceOne = true;
                continue;
              }
            QVERIFY (rx.receive (frame));
            tx.handleAckFrame (rx.makeAckFrame ());
          }
      }

    QVERIFY (droppedSequenceOne);
    QVERIFY (!tx.failed ());
    QVERIFY (tx.complete ());
    QVERIFY (rx.complete ());
    QVERIFY (rx.message () == payload);
    QCOMPARE (tx.acknowledgedCount (), tx.frameCount ());
  }

  void txAudioPlanCanRebuildOnlyPendingArqWindow ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::OutboundTransfer;
    using decodium::ft2link::Profile;
    using decodium::ft2link::WideTxAudioPlan;
    using decodium::ft2link::WideTxAudioPlanOptions;

    std::string text;
    for (int i = 0; i < 20; ++i)
      {
        text += "FT2-Link file ARQ window rebuild test block ";
      }
    std::vector<std::uint8_t> const payload = bytesFromString (text);
    OutboundTransfer tx {Profile::Wide2300, 0x5a5au, payload};
    tx.setWindowSize (4);
    tx.setRetryMs (1000);

    std::vector<Frame> const firstWindow = tx.framesToSend (0);
    QCOMPARE (firstWindow.size (), static_cast<std::size_t> (4));

    tx.handleAckFrame (decodium::ft2link::makeAckFrame (
        Profile::Wide2300, 0x5a5au, 0u, 0x0003u));
    std::vector<Frame> const pendingWindow = tx.framesToSend (1000);
    QVERIFY (!pendingWindow.empty ());
    for (Frame const& frame : pendingWindow)
      {
        QVERIFY (frame.sequence != 0u);
        QVERIFY (frame.sequence != 1u);
      }

    WideTxAudioPlanOptions options;
    options.profile = Profile::Wide2300;
    options.w2300RateMode = decodium::ft2link::W2300RateMode::Robust;
    WideTxAudioPlan const plan =
        decodium::ft2link::buildWideTxAudioPlanForFrames (
            pendingWindow, options);

    QVERIFY (plan.ok);
    QCOMPARE (plan.frames.size (), pendingWindow.size ());
    QCOMPARE (plan.bursts.size (), pendingWindow.size ());
    QVERIFY (plan.samples.size () > 0u);
    for (std::size_t i = 0; i < pendingWindow.size (); ++i)
      {
        QCOMPARE (plan.frames[i].sequence, pendingWindow[i].sequence);
      }
  }

  void selectiveAckImmediatelyRefillsArqWindow ()
  {
    using decodium::ft2link::Frame;
    using decodium::ft2link::OutboundTransfer;
    using decodium::ft2link::Profile;

    std::vector<std::uint8_t> payload (1600u, 0x41u);
    OutboundTransfer tx {Profile::Wide2300, 0x5a5bu, payload};
    tx.setWindowSize (4u);
    tx.setRetryMs (8000u);

    std::vector<Frame> const first = tx.framesToSend (100u);
    QCOMPARE (first.size (), static_cast<std::size_t> (4u));
    tx.handleAckFrame (decodium::ft2link::makeAckFrame (
        Profile::Wide2300, 0x5a5bu, 1u, 0x0006u));
    tx.makeUnacknowledgedDue (200u);

    std::vector<Frame> const next = tx.framesToSend (200u);
    QCOMPARE (next.size (), static_cast<std::size_t> (4u));
    QCOMPARE (next[0].sequence, static_cast<std::uint16_t> (1u));
    QCOMPARE (next[1].sequence, static_cast<std::uint16_t> (4u));
    QCOMPARE (next[2].sequence, static_cast<std::uint16_t> (5u));
    QCOMPARE (next[3].sequence, static_cast<std::uint16_t> (6u));
    QCOMPARE (tx.attemptsForSequence (1u), 2);
    QCOMPARE (tx.attemptsForSequence (4u), 1);
  }
};

QTEST_MAIN (TestFt2Link)
#include "test_ft2link.moc"
