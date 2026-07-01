#include <QtTest>

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

#include <limits>

#include "controllers/FT2LinkQmlAdapter.hpp"
#include "controllers/FT2LinkAccessGate.hpp"
#include "lib/ft2link/FT2LinkAppModel.hpp"
#include "lib/ft2link/FT2LinkFrame.hpp"
#include "lib/ft2link/FT2LinkWaveform.hpp"

namespace
{
QByteArray frameBytes (decodium::ft2link::Frame const& frame)
{
  std::vector<std::uint8_t> const wire =
      decodium::ft2link::serializeFrame (frame);
  return QByteArray (
      reinterpret_cast<char const*> (wire.data ()),
      static_cast<int> (wire.size ()));
}

QVector<short> pcmFromWave (std::vector<float> const& wave)
{
  QVector<short> out;
  out.reserve (static_cast<qsizetype> (wave.size ()));
  for (float sample : wave)
    {
      int const value = qBound (-32768, qRound (sample * 30000.0f), 32767);
      out.push_back (static_cast<short> (value));
    }
  return out;
}

QVector<short> pcmFromSamples (QVector<float> const& samples, int stride = 1)
{
  QVector<short> out;
  if (stride < 1)
    {
      stride = 1;
    }
  out.reserve (samples.size () / stride + 1);
  for (qsizetype i = 0; i < samples.size (); i += stride)
    {
      int const value = qBound (-32768, qRound (samples[i] * 30000.0f), 32767);
      out.push_back (static_cast<short> (value));
  }
  return out;
}

QString hexUtf8 (QString const& text)
{
  return QString::fromLatin1 (text.toUtf8 ().toHex ());
}

QString sha256Hex (QByteArray const& bytes)
{
  return QString::fromLatin1 (
      QCryptographicHash::hash (bytes, QCryptographicHash::Sha256).toHex ());
}

quint16 connectWideSession (FT2LinkQmlAdapter& caller,
                            QString const& remoteCall,
                            QString const& remoteGrid,
                            quint64 nowMs)
{
  caller.observeStation (
      remoteCall, remoteGrid, "", true, true, true, true, true, 2, 0, nowMs);

  QByteArray const hello = caller.startSessionHelloBytes (remoteCall, nowMs + 1u);
  if (hello.isEmpty ())
    {
      return 0u;
    }

  FT2LinkQmlAdapter answerer;
  answerer.setLocalStation (remoteCall, remoteGrid, "");
  answerer.setLocalCapabilities (true, true, true, true, 2, 0);
  QByteArray const ack =
      answerer.answerHelloBytes ("IU8LMC", hello, nowMs + 2u);
  if (ack.isEmpty () || !caller.receiveHelloAckBytes (ack, nowMs + 3u))
    {
      return 0u;
    }

  return caller.activeSessionId ();
}
}

class TestFt2LinkQmlAdapter
  : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void adapterExposesStationsSessionsAndMessages ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter caller;
    FT2LinkQmlAdapter answerer;

    QSignalSpy stationSpy {&caller, &FT2LinkQmlAdapter::stationCountChanged};
    QSignalSpy sessionSpy {&caller, &FT2LinkQmlAdapter::sessionCountChanged};
    QSignalSpy messageSpy {&caller, &FT2LinkQmlAdapter::messagesChanged};
    QSignalSpy transportSpy {&caller, &FT2LinkQmlAdapter::transportStateChanged};
    QSignalSpy metricsSpy {&caller, &FT2LinkQmlAdapter::transportMetricsChanged};
    QSignalSpy radioArmSpy {&caller, &FT2LinkQmlAdapter::radioTxArmedChanged};
    QSignalSpy radioPlanSpy {&caller, &FT2LinkQmlAdapter::radioTxPlanChanged};
    QSignalSpy radioRequestSpy {&caller, &FT2LinkQmlAdapter::radioTxAudioRequested};

    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    answerer.setLocalStation ("K1ABC", "FN42", "Ann");
    answerer.setLocalCapabilities (true, true, true, true, 2, 1);

    QVERIFY (caller.observeStation (
        "k1abc", "fn42", "Ann", true, true, true, true, true, 2, 1, 1000));
    QVERIFY (caller.setContactTag (QStringLiteral ("K1ABC"),
                                   QStringLiteral ("friend"),
                                   1100));
    QCOMPARE (caller.stationCount (), 1);
    QCOMPARE (stationSpy.size (), 1);

    QVariantList stations = caller.activeStations (1200, 1000, true);
    QCOMPARE (stations.size (), 1);
    QVariantMap station = stations[0].toMap ();
    QCOMPARE (station.value ("call").toString (), QStringLiteral ("K1ABC"));
    QCOMPARE (station.value ("locator").toString (), QStringLiteral ("FN42"));
    QCOMPARE (station.value ("tag").toString (), QStringLiteral ("FRIEND"));
    QVERIFY (station.value ("cq").toBool ());

    QByteArray const hello = caller.startSessionHelloBytes ("K1ABC", 1300);
    QVERIFY (!hello.isEmpty ());
    QVERIFY (caller.lastError ().isEmpty ());
    QCOMPARE (caller.sessionCount (), 1);
    QCOMPARE (sessionSpy.size (), 1);
    quint16 const sessionId = caller.activeSessionId ();
    QVERIFY (sessionId != 0);

    QVariantMap calling = caller.sessionInfo (sessionId);
    QCOMPARE (calling.value ("stateName").toString (), QStringLiteral ("Calling"));
    QCOMPARE (calling.value ("remoteCall").toString (), QStringLiteral ("K1ABC"));

    QByteArray const helloAck = answerer.answerHelloBytes ("IU8LMC", hello, 1400);
    QVERIFY (!helloAck.isEmpty ());
    QVERIFY (answerer.lastError ().isEmpty ());
    QCOMPARE (answerer.sessionCount (), 1);
    QCOMPARE (answerer.sessionInfo (sessionId).value ("stateName").toString (),
              QStringLiteral ("Connected"));
    QCOMPARE (answerer.sessionInfo (sessionId).value ("profileName").toString (),
              QStringLiteral ("W2300"));

    QVERIFY (caller.receiveHelloAckBytes (helloAck, 1500));
    QVariantMap connected = caller.sessionInfo (sessionId);
    QCOMPARE (connected.value ("stateName").toString (), QStringLiteral ("Connected"));
    QCOMPARE (connected.value ("profileName").toString (), QStringLiteral ("W2300"));
    QCOMPARE (connected.value ("w2300RateMode").toInt (), 1);

    QVERIFY (caller.queueOutgoingText (sessionId, "Ciao via QML adapter", 1600));
    QVERIFY (caller.appendIncomingText (sessionId, "Ricevuto", 1700));
    QCOMPARE (messageSpy.size (), 2);

    QCOMPARE (caller.qsoLogCount (), 1);
    QCOMPARE (caller.contactCount (), 1);
    QVariantMap qso = caller.qsoLog ().first ().toMap ();
    QCOMPARE (qso.value ("remoteCall").toString (), QStringLiteral ("K1ABC"));
    QCOMPARE (qso.value ("profileName").toString (), QStringLiteral ("W2300"));
    QVERIFY (qso.value ("messageCount").toInt () >= 2);
    QVariantMap contact = caller.contactHistory ().first ().toMap ();
    QCOMPARE (contact.value ("call").toString (), QStringLiteral ("K1ABC"));
    QCOMPARE (contact.value ("tag").toString (), QStringLiteral ("FRIEND"));
    QVERIFY (contact.value ("qsoCount").toInt () >= 1);
    QVERIFY (caller.setContactDetails (QStringLiteral ("K1ABC"),
                                       QStringLiteral ("fn42"),
                                       QStringLiteral ("Ann"),
                                       QStringLiteral ("friend"),
                                       QStringLiteral ("Primary path"),
                                       1750));
    contact = caller.contactHistory ().first ().toMap ();
    QCOMPARE (contact.value ("locator").toString (), QStringLiteral ("FN42"));
    QCOMPARE (contact.value ("name").toString (), QStringLiteral ("Ann"));
    QCOMPARE (contact.value ("tag").toString (), QStringLiteral ("FRIEND"));
    QCOMPARE (contact.value ("comment").toString (),
              QStringLiteral ("Primary path"));
    QVERIFY (caller.qslCard (sessionId).contains (QStringLiteral ("K1ABC")));
    QString const adif = caller.adifRecord (sessionId);
    QVERIFY (adif.contains (QStringLiteral ("<CALL:5>K1ABC")));
    QVERIFY (adif.contains (QStringLiteral ("<MODE:4>MFSK")));
    QVERIFY (adif.contains (QStringLiteral ("<SUBMODE:3>FT2")));
    QVERIFY (adif.contains (QStringLiteral ("<APP_DECODIUM_MODE:8>FT2-LINK")));
    QVERIFY (adif.contains (QStringLiteral ("<APP_DECODIUM_PROFILE:5>W2300")));
    QVERIFY (adif.endsWith (QStringLiteral ("<EOR>")));
    QVERIFY (caller.adifLog ().contains (adif));

    QVariantList messages = caller.messages (sessionId);
    QCOMPARE (messages.size (), 2);
    QCOMPARE (messages[0].toMap ().value ("directionName").toString (),
              QStringLiteral ("Outgoing"));
    QCOMPARE (messages[0].toMap ().value ("deliveryName").toString (),
              QStringLiteral ("Pending"));
    QCOMPARE (messages[1].toMap ().value ("directionName").toString (),
              QStringLiteral ("Incoming"));
    QCOMPARE (messages[1].toMap ().value ("deliveryName").toString (),
              QStringLiteral ("Received"));

    QVERIFY (caller.transmitTextLocalAudio (
        sessionId, "RF local payload", 1800, true, false, false));
    QVERIFY (!caller.transportBusy ());
    QCOMPARE (caller.transportState (), QStringLiteral ("Delivered"));
    QVERIFY (transportSpy.size () >= 2);
    QCOMPARE (metricsSpy.size (), 1);

    QVariantMap metrics = caller.lastTransportMetrics ();
    QVERIFY (metrics.value ("complete").toBool ());
    QVERIFY (!metrics.value ("failed").toBool ());
    QCOMPARE (metrics.value ("profileName").toString (), QStringLiteral ("W2300"));
    QVERIFY (metrics.value ("payloadBytes").toULongLong () > 0);
    QVERIFY (metrics.value ("burstCount").toULongLong () > 0);
    QVERIFY (metrics.value ("effectivePayloadBps").toDouble () > 0.0);

    caller.setRadioTxArmed (true);
    QVERIFY (caller.radioTxArmed ());
    QCOMPARE (radioArmSpy.size (), 1);
    QVERIFY (caller.prepareRadioTxAudio (sessionId, "RF radio plan", 1900));
    QCOMPARE (radioPlanSpy.size (), 2);

    QVariantMap radioPlan = caller.lastRadioTxPlan ();
    QVERIFY (radioPlan.value ("ok").toBool ());
    QVERIFY (radioPlan.value ("armed").toBool ());
    QCOMPARE (radioPlan.value ("profileName").toString (), QStringLiteral ("W2300"));
    QCOMPARE (radioPlan.value ("sampleRate").toDouble (), 48000.0);
    QVERIFY (radioPlan.value ("sampleCount").toULongLong () > 0);
    QVERIFY (radioPlan.value ("burstCount").toULongLong () > 0);
    QVERIFY (radioPlan.value ("audioSeconds").toDouble () > 0.0);

    messages = caller.messages (sessionId);
    QCOMPARE (messages.size (), 3);
    QCOMPARE (messages[2].toMap ().value ("directionName").toString (),
              QStringLiteral ("Outgoing"));
    QCOMPARE (messages[2].toMap ().value ("deliveryName").toString (),
              QStringLiteral ("Delivered"));

    caller.setRadioTxArmed (true);
    QVERIFY (caller.radioTxArmed ());
    QVERIFY (caller.transmitPreparedRadioTxAudio (sessionId, "RF live request", 2000));
    QCOMPARE (radioRequestSpy.size (), 1);
    QVERIFY (!caller.radioTxArmed ());
    QVERIFY (radioArmSpy.size () >= 2);
    QVERIFY (radioPlanSpy.size () >= 4);

    QList<QVariant> const request = radioRequestSpy.takeFirst ();
    QCOMPARE (request[0].toString (), QStringLiteral ("FT2-Link RF live request"));
    QVector<float> requestSamples = request[1].value<QVector<float>> ();
    QVariantMap requestPlan = request[2].toMap ();
    QVERIFY (!requestSamples.isEmpty ());
    QVERIFY (requestPlan.value ("armed").toBool ());
    QCOMPARE (requestPlan.value ("profileName").toString (), QStringLiteral ("W2300"));

    messages = caller.messages (sessionId);
    QCOMPARE (messages.size (), 4);
    QCOMPARE (messages[3].toMap ().value ("directionName").toString (),
              QStringLiteral ("Outgoing"));
    QCOMPARE (messages[3].toMap ().value ("deliveryName").toString (),
              QStringLiteral ("Pending"));

    decodium::ft2link::Frame ack =
        decodium::ft2link::makeAckFrame (
            decodium::ft2link::Profile::Wide2300,
            sessionId,
            0u,
            0x0001u);
    QVERIFY (caller.ingestRadioFrameBytes (
        frameBytes (ack), "K1ABC", 2100, false));
    messages = caller.messages (sessionId);
    QCOMPARE (messages.size (), 4);
    QCOMPARE (messages[3].toMap ().value ("deliveryName").toString (),
              QStringLiteral ("Delivered"));
    QCOMPARE (caller.transportState (), QStringLiteral ("Delivered"));

    decodium::ft2link::Frame data;
    data.type = decodium::ft2link::FrameType::Data;
    data.profile = decodium::ft2link::Profile::Wide2300;
    data.sessionId = sessionId;
    data.sequence = 0u;
    data.flags = decodium::ft2link::FlagEndOfMessage;
    QByteArray const inboundPayload = QByteArrayLiteral ("RX radio payload");
    data.payload.assign (
        inboundPayload.begin (),
        inboundPayload.end ());

    QVERIFY (caller.ingestRadioFrameBytes (
        frameBytes (data), "K1ABC", 200000, true));
    QCOMPARE (radioRequestSpy.size (), 1);
    QList<QVariant> const ackRequest = radioRequestSpy.takeFirst ();
    QCOMPARE (ackRequest[0].toString (), QStringLiteral ("FT2-Link ACK"));
    QVector<float> ackSamples = ackRequest[1].value<QVector<float>> ();
    QVariantMap ackPlan = ackRequest[2].toMap ();
    QVERIFY (!ackSamples.isEmpty ());
    QVERIFY (ackPlan.value ("autoAck").toBool ());
    QCOMPARE (ackPlan.value ("kind").toString (), QStringLiteral ("ACK"));
    QCOMPARE (ackPlan.value ("profileName").toString (), QStringLiteral ("W2300"));
    QCOMPARE (ackPlan.value ("ackBase").toUInt (), 1u);

    messages = caller.messages (sessionId);
    QCOMPARE (messages.size (), 5);
    QCOMPARE (messages[4].toMap ().value ("directionName").toString (),
              QStringLiteral ("Incoming"));
    QCOMPARE (messages[4].toMap ().value ("deliveryName").toString (),
              QStringLiteral ("Received"));
    QCOMPARE (messages[4].toMap ().value ("text").toString (),
              QStringLiteral ("RX radio payload"));

    QVERIFY (caller.ingestRadioFrameBytes (
        frameBytes (data), "K1ABC", 201000, true));
    QCOMPARE (radioRequestSpy.size (), 1);
    messages = caller.messages (sessionId);
    QCOMPARE (messages.size (), 5);

    decodium::ft2link::Frame audioData = data;
    audioData.payload.clear ();
    QByteArray const audioPayload = QByteArrayLiteral ("RX audio decoded");
    audioData.payload.assign (audioPayload.begin (), audioPayload.end ());
    std::string waveformError;
    std::vector<float> const audioWave =
        decodium::ft2link::generateW2300FrameWaveform (
            audioData,
            decodium::ft2link::W2300WaveformConfig {},
            &waveformError);
    QVERIFY2 (!audioWave.empty (), waveformError.c_str ());
    QVERIFY (caller.ingestRxSamples (
        pcmFromWave (audioWave), "K1ABC", 206000));
    QTRY_VERIFY_WITH_TIMEOUT (radioRequestSpy.size () >= 2, 3000);
    radioRequestSpy.clear ();
    messages = caller.messages (sessionId);
    QCOMPARE (messages.size (), 6);
    QCOMPARE (messages[5].toMap ().value ("directionName").toString (),
              QStringLiteral ("Incoming"));
    QCOMPARE (messages[5].toMap ().value ("text").toString (),
              QStringLiteral ("RX audio decoded"));
  }

  void adapterReportsErrorsForUnknownSessions ()
  {
    FT2LinkQmlAdapter adapter;
    QSignalSpy errorSpy {&adapter, &FT2LinkQmlAdapter::lastErrorChanged};

    QVERIFY (!adapter.queueOutgoingText (0x9999u, "missing", 1000));
    QVERIFY (adapter.lastError ().contains ("unknown session"));
    QCOMPARE (errorSpy.size (), 1);
    QVERIFY (adapter.messages (0x9999u).isEmpty ());
    QVERIFY (adapter.sessionInfo (0x9999u).isEmpty ());
  }

  void closeSessionUpdatesStateAndBlocksFurtherMessages ()
  {
    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);

    QSignalSpy sessionsSpy {&caller, &FT2LinkQmlAdapter::sessionsChanged};
    QSignalSpy qsoSpy {&caller, &FT2LinkQmlAdapter::qsoLogChanged};
    QVERIFY (caller.closeSession (sessionId, 2000));
    QVERIFY (sessionsSpy.size () >= 1);
    QVERIFY (qsoSpy.size () >= 1);

    QVariantMap const closed = caller.sessionInfo (sessionId);
    QCOMPARE (closed.value ("stateName").toString (), QStringLiteral ("Closed"));
    QCOMPARE (caller.qsoLog ().first ().toMap ().value ("state").toString (),
              QStringLiteral ("Closed"));
    QVERIFY (!caller.queueOutgoingText (sessionId, "after close", 2100));
    QVERIFY (caller.lastError ().contains ("connected session"));
    QVERIFY (!caller.appendIncomingText (sessionId, "after close", 2200));
    QVERIFY (caller.lastError ().contains ("connected session"));
    QVERIFY (!caller.prepareRadioTxAudio (sessionId, "after close", 2300));
    QVERIFY (caller.lastError ().contains ("connected session"));
  }

  void cannedMessagesExpandOperatorAndSessionTags ()
  {
    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    caller.setLocalOperatorProfile (
        "Napoli", "salvo@example.test", "Radio and EmComm",
        "IC-7300", "Dipole", "50W", "40.8N 14.3E");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);

    QVariantList const canned = caller.cannedMessages ();
    QVERIFY (canned.size () >= 20);
    QCOMPARE (canned[0].toMap ().value ("label").toString (),
              QStringLiteral ("INFO"));
    bool hasQsyUp = false;
    bool hasDisconnect = false;
    bool hasRig = false;
    for (QVariant const& value : canned)
      {
        QVariantMap const item = value.toMap ();
        hasQsyUp = hasQsyUp
            || item.value ("templateText").toString () == QStringLiteral ("<QSYU>");
        hasDisconnect = hasDisconnect
            || item.value ("templateText").toString ().contains (
                QStringLiteral ("<DISC>"));
        hasRig = hasRig
            || item.value ("templateText").toString ().contains (
                QStringLiteral ("<RIG:<RIG>>"));
      }
    QVERIFY (hasQsyUp);
    QVERIFY (hasDisconnect);
    QVERIFY (hasRig);

    QString const expanded = caller.expandCannedMessage (
        QStringLiteral ("73 <CALL> DE <MYCALL> <MYGRID> <NAME> <PROFILE> <RATE> <UTC>"),
        sessionId,
        3600000u);
    QCOMPARE (expanded,
              QStringLiteral ("73 K1ABC DE IU8LMC JN70 Salvo W2300 FAST 0100Z"));

    QString const tagExpanded = caller.expandCannedMessage (
        QStringLiteral ("<NAME:<NAME>> <LOC:<MYGRID>> <UTCDT> <UTCD> <UTCT> <HCALL> <HLOC>"),
        sessionId,
        3600000u);
    QCOMPARE (tagExpanded,
              QStringLiteral ("<NAME:Salvo> <LOC:JN70> 1970-01-01 01:00Z 1970-01-01 01:00Z K1ABC FN42"));
    QString const profileExpanded = caller.expandCannedMessage (
        QStringLiteral ("<QTH:<QTH>> <EM:<EMAIL>> <RIG:<RIG>> <ANT:<ANT>> <PWR:<PWR>> <ICE:<ICE>> <GPS:<GPSLOC>>"),
        sessionId,
        3600000u);
    QCOMPARE (profileExpanded,
              QStringLiteral ("<QTH:Napoli> <EM:salvo@example.test> <RIG:IC-7300> <ANT:Dipole> <PWR:50W> <ICE:Radio and EmComm> <GPS:40.8N 14.3E>"));
  }

  void customCannedMessagesPersistAndCheckInGeneratorWorks ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath ("ft2link-store.json");

    FT2LinkQmlAdapter adapter;
    adapter.setLocalStorePath (storePath);
    adapter.setLocalStation ("IU8LMC", "JN70", "Salvo");
    adapter.setLocalOperatorProfile (
        "Napoli", "salvo@example.test", "Radio and EmComm",
        "IC-7300", "Dipole", "50W", "40.8N 14.3E");

    QVariantMap result = adapter.addOrUpdateCannedMessage (
        "wx1", "WX <MYCALL> <QTH>", "Weather quick note");
    QVERIFY (result.value ("ok").toBool ());
    QCOMPARE (result.value ("label").toString (), QStringLiteral ("WX1"));
    QCOMPARE (adapter.customCannedMessages ().size (), 1);

    result = adapter.addOrUpdateCannedMessage (
        "wx1", "WX2 <MYCALL> <QTH>", "Updated");
    QVERIFY (result.value ("ok").toBool ());
    QVERIFY (result.value ("updated").toBool ());
    QCOMPARE (adapter.customCannedMessages ().size (), 1);

    bool foundCustom = false;
    for (QVariant const& value : adapter.cannedMessages ())
      {
        QVariantMap const item = value.toMap ();
        if (item.value ("label").toString () == QStringLiteral ("WX1"))
          {
            foundCustom = item.value ("custom").toBool ()
                && item.value ("templateText").toString ()
                    == QStringLiteral ("WX2 <MYCALL> <QTH>");
          }
      }
    QVERIFY (foundCustom);

    QString const checkIn = adapter.checkInMessage (
        "Napoli", "Campania", "HF", "Clear 20C", 3600000u);
    QCOMPARE (checkIn,
              QStringLiteral ("IU8LMC, Salvo, Napoli, Campania, (HF)\n0100Z, Clear 20C"));

    result = adapter.setCustomAlertTags ("WX, NET");
    QVERIFY (result.value ("ok").toBool ());
    QCOMPARE (adapter.customAlertTags ().size (), 2);

    QVERIFY (adapter.saveLocalStore ());
    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QCOMPARE (restored.customCannedMessages ().size (), 1);
    QCOMPARE (restored.customCannedMessages ().first ().toMap ().value (
                  "label").toString (),
              QStringLiteral ("WX1"));
    QStringList const expectedAlertTags {
      QStringLiteral ("WX"), QStringLiteral ("NET")
    };
    QCOMPARE (restored.customAlertTags (), expectedAlertTags);

    result = restored.deleteCannedMessage ("WX1");
    QVERIFY (result.value ("ok").toBool ());
    QCOMPARE (restored.customCannedMessages ().size (), 0);
    result = restored.addOrUpdateCannedMessage ("A", "B", "C");
    QVERIFY (result.value ("ok").toBool ());
    result = restored.resetCannedMessages ();
    QVERIFY (result.value ("ok").toBool ());
    QCOMPARE (restored.customCannedMessages ().size (), 0);
    result = restored.clearCustomAlertTags ();
    QVERIFY (result.value ("ok").toBool ());
    QVERIFY (restored.customAlertTags ().isEmpty ());
  }

  void qsySlotHelpersGenerateVaracStyleTags ()
  {
    FT2LinkQmlAdapter adapter;
    QVariantList const qsySlotList = adapter.qsySlots (750, 2);
    QCOMPARE (qsySlotList.size (), 4);
    QCOMPARE (qsySlotList[0].toMap ().value ("offsetHz").toInt (), 750);
    QCOMPARE (qsySlotList[0].toMap ().value ("tag").toString (),
              QStringLiteral ("<QSYU>"));
    QCOMPARE (qsySlotList[1].toMap ().value ("offsetHz").toInt (), -750);
    QCOMPARE (qsySlotList[1].toMap ().value ("tag").toString (),
              QStringLiteral ("<QSYD>"));
    QCOMPARE (adapter.qsyTagForOffset (1500), QStringLiteral ("<Q:+150>"));
    QCOMPARE (adapter.qsyTagForOffset (-1500), QStringLiteral ("<Q:-150>"));
    QCOMPARE (adapter.qsyFrequencyTag (14105750),
              QStringLiteral ("<QF:14105750>"));
    QVERIFY (adapter.qsyFrequencyTag (0).isEmpty ());
  }

  void qsyPlanParsesInvitations ()
  {
    FT2LinkQmlAdapter adapter;

    QVariantMap const up = adapter.qsyPlanForText (
        QStringLiteral ("PSE <QSYU>"), 14105000);
    QVERIFY (up.value ("valid").toBool ());
    QCOMPARE (up.value ("offsetHz").toInt (), 750);
    QCOMPARE (up.value ("dialFrequencyHz").toLongLong (), 14105750);
    QVERIFY (up.value ("allowed").toBool ());
    QCOMPARE (up.value ("rangeStatus").toString (), QStringLiteral ("Allowed"));
    QCOMPARE (up.value ("acceptTag").toString (), QStringLiteral ("<QSYR>"));
    QCOMPARE (up.value ("rejectTag").toString (), QStringLiteral ("<QSYJ>"));

    QVariantMap const custom = adapter.qsyPlanForText (
        QStringLiteral ("PSE <Q:-75>"), 14105000);
    QVERIFY (custom.value ("valid").toBool ());
    QCOMPARE (custom.value ("offsetHz").toInt (), -750);
    QCOMPARE (custom.value ("dialFrequencyHz").toLongLong (), 14104250);

    QVariantMap const full = adapter.qsyPlanForText (
        QStringLiteral ("PSE <QF:14105750>"), 14105000);
    QVERIFY (full.value ("valid").toBool ());
    QCOMPARE (full.value ("kind").toString (), QStringLiteral ("frequency"));
    QCOMPARE (full.value ("offsetHz").toInt (), 750);
    QCOMPARE (full.value ("dialFrequencyHz").toLongLong (), 14105750);

    QVariantMap const outOfRange = adapter.qsyPlanForText (
        QStringLiteral ("PSE <QF:14250000>"), 14105000);
    QVERIFY (outOfRange.value ("valid").toBool ());
    QVERIFY (!outOfRange.value ("allowed").toBool ());
    QCOMPARE (outOfRange.value ("rangeStatus").toString (),
              QStringLiteral ("Out of range"));

    QVariantMap const none = adapter.qsyPlanForText (
        QStringLiteral ("hello"), 14105000);
    QVERIFY (!none.value ("valid").toBool ());
  }

  void frequencyPlanPersistsAndFeedsScheduleReply ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath ("ft2link-store.json");

    FT2LinkQmlAdapter adapter;
    QVERIFY (adapter.frequencyPresets ().size () >= 10);
    QVERIFY (adapter.allowedQsyRanges ().size () >= 10);
    QVERIFY (adapter.qsyFrequencyAllowed (14105750));
    QVERIFY (!adapter.qsyFrequencyAllowed (14250000));
    QVariantMap guard = adapter.callingFrequencyGuard (
        QStringLiteral ("MAIL"), 14105000, 14105000);
    QVERIFY (guard.value ("blocked").toBool ());
    QVERIFY (!guard.value ("allowed").toBool ());
    QVERIFY (guard.value ("message").toString ().contains (
        QStringLiteral ("Calling frequency guard")));
    guard = adapter.callingFrequencyGuard (
        QStringLiteral ("CQ"), 14105000, 14105000);
    QVERIFY (!guard.value ("blocked").toBool ());
    QVERIFY (guard.value ("allowed").toBool ());
    guard = adapter.callingFrequencyGuard (
        QStringLiteral ("MAIL"), 14105750, 14105000);
    QVERIFY (!guard.value ("blocked").toBool ());

    QVariantMap result = adapter.setFrequencyPresets (
        QStringLiteral ("14105750|20m|Net, 7105750|40m|Night"));
    QVERIFY (result.value ("ok").toBool ());
    QCOMPARE (adapter.frequencyPresets ().size (), 2);
    result = adapter.setAllowedQsyRanges (
        QStringLiteral ("14105000-14106000|20m narrow, 7105000-7106500|40m"));
    QVERIFY (result.value ("ok").toBool ());
    QVERIFY (adapter.qsyFrequencyAllowed (14105750));
    QVERIFY (!adapter.qsyFrequencyAllowed (14107000));
    result = adapter.setFrequencySchedule (
        QStringLiteral ("1200-1259|CALLING|14105000|20m call|CHAT, 1300-1359|DATA|14105750|20m data|CQ"));
    QVERIFY (result.value ("ok").toBool ());
    QCOMPARE (adapter.frequencySchedule ().size (), 2);

    quint64 const callWindowMs = static_cast<quint64> (
        QDateTime (QDate (2026, 6, 30),
                   QTime (12, 30),
                   QTimeZone(QByteArrayLiteral("UTC"))).toMSecsSinceEpoch ());
    QVariantMap activeSchedule = adapter.activeFrequencySchedule (callWindowMs);
    QVERIFY (activeSchedule.value ("active").toBool ());
    QCOMPARE (activeSchedule.value ("action").toString (),
              QStringLiteral ("CALLING"));
    QCOMPARE (activeSchedule.value ("cqType").toString (),
              QStringLiteral ("CHAT"));
    guard = adapter.callingFrequencyGuardAt (
        QStringLiteral ("MAIL"), 14105000, 0, callWindowMs);
    QVERIFY (guard.value ("blocked").toBool ());
    QVERIFY (guard.value ("message").toString ().contains (
        QStringLiteral ("Frequency schedule guard")));
    guard = adapter.callingFrequencyGuardAt (
        QStringLiteral ("CQ"), 14105000, 0, callWindowMs);
    QVERIFY (!guard.value ("blocked").toBool ());
    QVERIFY (guard.value ("warning").toString ().contains (
        QStringLiteral ("NARROW")));

    quint64 const dataWindowMs = static_cast<quint64> (
        QDateTime (QDate (2026, 6, 30),
                   QTime (13, 10),
                   QTimeZone(QByteArrayLiteral("UTC"))).toMSecsSinceEpoch ());
    guard = adapter.callingFrequencyGuardAt (
        QStringLiteral ("MAIL"), 14105750, 0, dataWindowMs);
    QVERIFY (!guard.value ("blocked").toBool ());
    QCOMPARE (guard.value ("warning").toString (),
              QStringLiteral ("DATA window active in frequency schedule"));

    adapter.setLocalStorePath (storePath);
    QVERIFY (adapter.saveLocalStore ());

    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QCOMPARE (restored.frequencyPresets ().size (), 2);
    QCOMPARE (restored.allowedQsyRanges ().size (), 2);
    QCOMPARE (restored.frequencySchedule ().size (), 2);
    QVERIFY (restored.frequencyPresetsText ().contains (
        QStringLiteral ("14105750|20m|Net")));
    QVERIFY (restored.allowedQsyRangesText ().contains (
        QStringLiteral ("14105000-14106000|20m narrow")));
    QVERIFY (restored.frequencyScheduleText ().contains (
        QStringLiteral ("1200-1259|CALLING|14105000|20m call|CHAT")));

    quint16 const sessionId =
        connectWideSession (restored, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);
    QVERIFY (restored.appendIncomingText (
        sessionId, QStringLiteral ("PSE <FSR>"), 1100));
    QVariantList const messages = restored.messages (sessionId);
    bool sawFrequencySchedule = false;
    for (QVariant const& value : messages)
      {
        QString const text = value.toMap ().value ("text").toString ();
        sawFrequencySchedule = sawFrequencySchedule
            || text.contains (QStringLiteral ("<FS:"));
    }
    QVERIFY (sawFrequencySchedule);
  }

  void clusterLastHeardExportsImportsAndPersists ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath ("ft2link-cluster.json");

    FT2LinkQmlAdapter source;
    source.setLocalStorePath (storePath);
    source.setLocalStation ("IU8LMC", "JN70", "Salvo");
    QVariantMap config = source.configureCluster (
        true, QStringLiteral ("IU8LMC-MAC"), QStringLiteral ("20m"), 14105000);
    QVERIFY (config.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (config.value (QStringLiteral ("band")).toString (),
              QStringLiteral ("20M"));
    QCOMPARE (config.value (QStringLiteral ("dialFrequencyHz")).toLongLong (),
              14105000ll);

    QVERIFY (source.observeStation (
        QStringLiteral ("K1ABC"),
        QStringLiteral ("FN42"),
        QStringLiteral ("Ann"),
        true,
        true,
        true,
        true,
        true,
        2,
        0,
        1000));
    QCOMPARE (source.clusterLastHeardCount (), 1);
    QVariantMap first = source.clusterLastHeard ().first ().toMap ();
    QCOMPARE (first.value (QStringLiteral ("call")).toString (),
              QStringLiteral ("K1ABC"));
    QCOMPARE (first.value (QStringLiteral ("band")).toString (),
              QStringLiteral ("20M"));
    QCOMPARE (first.value (QStringLiteral ("nodeId")).toString (),
              QStringLiteral ("IU8LMC-MAC"));
    QVERIFY (first.value (QStringLiteral ("cq")).toBool ());

    source.configureCluster (
        true, QStringLiteral ("IU8LMC-MAC"), QStringLiteral ("40m"), 7105000);
    QVERIFY (source.observeStation (
        QStringLiteral ("K1ABC"),
        QStringLiteral ("FN42"),
        QStringLiteral ("Ann"),
        false,
        true,
        true,
        true,
        true,
        2,
        0,
        2000));
    QCOMPARE (source.clusterLastHeardCount (), 2);

    FT2LinkQmlAdapter peer;
    peer.setLocalStation ("M0ABC", "IO91", "Martino");
    peer.configureCluster (
        true, QStringLiteral ("M0ABC-WIN"), QStringLiteral ("30m"), 10136000);
    QVERIFY (peer.observeStation (
        QStringLiteral ("N0XYZ"),
        QStringLiteral ("EM12"),
        QStringLiteral ("Bob"),
        false,
        true,
        true,
        true,
        true,
        2,
        0,
        3000));
    QString const peerExport = peer.clusterExportJson ();
    QVERIFY (peerExport.contains (
        QStringLiteral ("ft2link-cluster-last-heard")));

    QString const sharePath = tempDir.filePath ("ft2link-cluster-share.json");
    QVariantMap shared = peer.writeClusterShareFile (sharePath);
    QVERIFY (shared.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (shared.value (QStringLiteral ("records")).toULongLong (), 1ull);
    QVERIFY (QFileInfo::exists (sharePath));

    QVariantMap imported = source.syncClusterShareFile (sharePath, 4000);
    QVERIFY (imported.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (imported.value (QStringLiteral ("action")).toString (),
              QStringLiteral ("pull-push"));
    QCOMPARE (imported.value (QStringLiteral ("imported")).toInt (), 1);
    QCOMPARE (imported.value (QStringLiteral ("path")).toString (),
              QFileInfo (sharePath).absoluteFilePath ());
    QCOMPARE (source.clusterLastHeardCount (), 3);

    QString const newSharePath = tempDir.filePath ("ft2link-new-share.json");
    QVariantMap pushedNew = source.syncClusterShareFile (newSharePath, 4500);
    QVERIFY (pushedNew.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (pushedNew.value (QStringLiteral ("action")).toString (),
              QStringLiteral ("push-new"));
    QCOMPARE (pushedNew.value (QStringLiteral ("records")).toULongLong (), 3ull);
    QVERIFY (QFileInfo::exists (newSharePath));

    QVariantMap merged = source.importClusterLastHeard (peerExport, 5000);
    QVERIFY (merged.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (merged.value (QStringLiteral ("imported")).toInt (), 0);
    QVERIFY (merged.value (QStringLiteral ("merged")).toInt () >= 1);
    QCOMPARE (source.clusterLastHeardCount (), 3);

    QVERIFY (source.clusterLastHeardText ().contains (
        QStringLiteral ("N0XYZ")));
    QVERIFY (source.logsBundleText ().contains (
        QStringLiteral ("--- CLUSTER LAST HEARD ---")));
    QVERIFY (source.statistics ().value (
        QStringLiteral ("clusterLastHeardTotal")).toULongLong () == 3ull);
    QVERIFY (source.localStoreAudit ().value (
        QStringLiteral ("clusterLastHeardCount")).toULongLong () == 3ull);
    QVERIFY (source.saveLocalStore ());

    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QCOMPARE (restored.clusterLastHeardCount (), 3);
    QVERIFY (restored.localStoreJson ().contains (
        QStringLiteral ("clusterLastHeard")));
    QVariantList const restoredList = restored.clusterLastHeard ();
    QVERIFY (restoredList.first ().toMap ().value (
        QStringLiteral ("call")).toString () == QStringLiteral ("N0XYZ"));
    restored.clearClusterLastHeard ();
    QCOMPARE (restored.clusterLastHeardCount (), 0);
  }

  void presencePersistsAndQueuesIncomingHelloReply ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath ("ft2link-store.json");

    FT2LinkQmlAdapter adapter;
    QSignalSpy presenceSpy {&adapter, &FT2LinkQmlAdapter::presenceChanged};
    QVariantMap result = adapter.configurePresence (
        false,
        false,
        QStringLiteral ("QRX DE <MYCALL>"),
        true,
        QStringLiteral ("HELLO <CALL> DE <MYCALL>"));
    QVERIFY (result.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (presenceSpy.size (), 1);
    QVERIFY (adapter.welcomeEnabled ());
    QCOMPARE (adapter.welcomeMessage (),
              QStringLiteral ("HELLO <CALL> DE <MYCALL>"));

    adapter.setLocalStorePath (storePath);
    QVERIFY (adapter.saveLocalStore ());

    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QVERIFY (restored.welcomeEnabled ());
    QVERIFY (!restored.awayEnabled ());
    QCOMPARE (restored.presence ().value (
                  QStringLiteral ("welcomeMessage")).toString (),
              QStringLiteral ("HELLO <CALL> DE <MYCALL>"));

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    QVERIFY (caller.observeStation (
        "K1ABC", "FN42", "Ann", true, true, true, true, true, 2, 0, 1000));
    restored.setLocalStation ("K1ABC", "FN42", "Ann");
    restored.setLocalCapabilities (true, true, true, true, 2, 0);

    QByteArray const hello = caller.startSessionHelloBytes ("K1ABC", 1100);
    QVERIFY (!hello.isEmpty ());
    QByteArray const ack = restored.answerHelloBytes ("IU8LMC", hello, 1200);
    QVERIFY (!ack.isEmpty ());

    quint16 const sessionId = restored.activeSessionId ();
    QVERIFY (sessionId != 0u);
    QVariantList messages = restored.messages (sessionId);
    QCOMPARE (messages.size (), 1);
    QVariantMap const welcome = messages.first ().toMap ();
    QCOMPARE (welcome.value (QStringLiteral ("directionName")).toString (),
              QStringLiteral ("Outgoing"));
    QCOMPARE (welcome.value (QStringLiteral ("deliveryName")).toString (),
              QStringLiteral ("Pending"));
    QCOMPARE (welcome.value (QStringLiteral ("text")).toString (),
              QStringLiteral ("HELLO IU8LMC DE K1ABC"));

    FT2LinkQmlAdapter away;
    away.setLocalStation ("K1ABC", "FN42", "Ann");
    away.setLocalCapabilities (true, true, true, true, 2, 0);
    result = away.configurePresence (
        true,
        true,
        QStringLiteral ("QRX <CALL> DE <MYCALL>"),
        true,
        QStringLiteral ("WELCOME SHOULD NOT BE USED"));
    QVERIFY (result.value (QStringLiteral ("ok")).toBool ());
    QVERIFY (away.awayEnabled ());
    QVERIFY (away.awayAcceptsQsy ());

    FT2LinkQmlAdapter caller2;
    caller2.setLocalStation ("IU8LMC", "JN70", "Salvo");
    QVERIFY (caller2.observeStation (
        "K1ABC", "FN42", "Ann", true, true, true, true, true, 2, 0, 2000));
    QByteArray const hello2 = caller2.startSessionHelloBytes ("K1ABC", 2100);
    QVERIFY (!hello2.isEmpty ());
    QByteArray const ack2 = away.answerHelloBytes ("IU8LMC", hello2, 2200);
    QVERIFY (!ack2.isEmpty ());

    messages = away.messages (away.activeSessionId ());
    QCOMPARE (messages.size (), 1);
    QString const awayText = messages.first ().toMap ().value (
        QStringLiteral ("text")).toString ();
    QVERIFY (awayText.contains (QStringLiteral ("<AWQ>")));
    QVERIFY (awayText.contains (QStringLiteral ("QRX IU8LMC DE K1ABC")));
    QVERIFY (!awayText.contains (QStringLiteral ("WELCOME SHOULD NOT BE USED")));
  }

  void autoReplyQueuesSafeInquireResponses ()
  {
    FT2LinkQmlAdapter caller;
    FT2LinkQmlAdapter answerer;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    answerer.setLocalStation ("K1ABC", "FN42", "Ann");
    answerer.setLocalOperatorProfile (
        QStringLiteral ("Rome"),
        QStringLiteral ("ann@example.net"),
        QStringLiteral ("QRV"),
        QStringLiteral ("IC-7300"),
        QStringLiteral ("Dipole"),
        QStringLiteral ("50W"),
        QStringLiteral ("41.900N 12.500E"));
    answerer.setLocalCapabilities (true, true, true, true, 2, 0);
    QVERIFY (answerer.configurePresence (
        false,
        false,
        QStringLiteral ("QRX DE <MYCALL>"),
        false,
        QStringLiteral ("HELLO <CALL> DE <MYCALL>"))
        .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (answerer.configureAutoReply (true)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (answerer.autoReplyEnabled ());
    QVERIFY (answerer.setFrequencyPresets (
        QStringLiteral ("14105750|20m|Net")).value (
            QStringLiteral ("ok")).toBool ());

    QVERIFY (caller.observeStation (
        "K1ABC", "FN42", "Ann", true, true, true, true, true, 2, 0, 1000));
    QByteArray const hello = caller.startSessionHelloBytes ("K1ABC", 1100);
    QVERIFY (!hello.isEmpty ());
    QByteArray const ack = answerer.answerHelloBytes ("IU8LMC", hello, 1200);
    QVERIFY (!ack.isEmpty ());
    quint16 const sessionId = answerer.activeSessionId ();
    QVERIFY (sessionId != 0u);

    QVERIFY (answerer.appendIncomingText (
        sessionId,
        QStringLiteral (
            "REQ <INFO> <LOCR> <FSR> <GPSR> <VER> <BLR> <BG:missing.txt>"),
        1300));

    QString autoReply;
    bool sawQueuedNotice = false;
    for (QVariant const& value : answerer.messages (sessionId))
      {
        QVariantMap const message = value.toMap ();
        QString const direction =
            message.value (QStringLiteral ("directionName")).toString ();
        QString const text = message.value (
            QStringLiteral ("text")).toString ();
        if (direction == QStringLiteral ("Outgoing")
            && text.contains (QStringLiteral ("<NAME:Ann>")))
          {
            autoReply = text;
          }
        if (direction == QStringLiteral ("System")
            && text.contains (QStringLiteral ("AUTO REPLY queued")))
          {
            sawQueuedNotice = true;
          }
      }

    QVERIFY (!autoReply.isEmpty ());
    QVERIFY (autoReply.contains (QStringLiteral ("<NAME:Ann>")));
    QVERIFY (autoReply.contains (QStringLiteral ("<QTH:Rome>")));
    QVERIFY (autoReply.contains (QStringLiteral ("<LOC:FN42>")));
    QVERIFY (autoReply.contains (QStringLiteral ("<FS:20m 14105750 Net>")));
    QVERIFY (autoReply.contains (QStringLiteral ("<GPS:41.900N 12.500E>")));
    QVERIFY (autoReply.contains (QStringLiteral ("FT2-Link Decodium v0.1")));
    QVERIFY (autoReply.contains (QStringLiteral ("<BLJ>")));
    QVERIFY (autoReply.contains (QStringLiteral ("<BGJ>")));
    QVERIFY (sawQueuedNotice);
  }

  void autoAwayActivatesClearsAndPersistsOnlySettings ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath ("ft2link-store.json");

    FT2LinkQmlAdapter adapter;
    QVariantMap result = adapter.configureAutoAway (true, 1, 1000);
    QVERIFY (result.value (QStringLiteral ("ok")).toBool ());
    QVERIFY (adapter.autoAwayEnabled ());
    QCOMPARE (adapter.autoAwayMinutes (), 1);
    QVERIFY (!adapter.awayEnabled ());
    QVERIFY (!adapter.autoAwayActive ());

    result = adapter.evaluateAutoAway (60000);
    QVERIFY (!result.value (QStringLiteral ("activated")).toBool ());
    QVERIFY (!adapter.awayEnabled ());

    result = adapter.evaluateAutoAway (61000);
    QVERIFY (result.value (QStringLiteral ("activated")).toBool ());
    QVERIFY (adapter.awayEnabled ());
    QVERIFY (adapter.autoAwayActive ());

    adapter.setLocalStorePath (storePath);
    QVERIFY (adapter.saveLocalStore ());
    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QVERIFY (restored.autoAwayEnabled ());
    QCOMPARE (restored.autoAwayMinutes (), 1);
    QVERIFY (!restored.awayEnabled ());
    QVERIFY (!restored.autoAwayActive ());

    result = adapter.noteOperatorActivity (62000);
    QVERIFY (result.value (QStringLiteral ("cleared")).toBool ());
    QVERIFY (!adapter.awayEnabled ());
    QVERIFY (!adapter.autoAwayActive ());

    QVERIFY (adapter.configurePresence (
        true,
        false,
        QStringLiteral ("MANUAL AWAY"),
        false,
        QStringLiteral ("HELLO <CALL> DE <MYCALL>"))
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (adapter.awayEnabled ());
    result = adapter.noteOperatorActivity (63000);
    QVERIFY (!result.value (QStringLiteral ("cleared")).toBool ());
    QVERIFY (adapter.awayEnabled ());
    QVERIFY (!adapter.autoAwayActive ());
  }

  void qsoAutomationQueuesCallIdAndDisconnectsIdleSession ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath ("ft2link-store.json");

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);

    QVariantMap result = caller.configureQsoAutomation (1, 0);
    QVERIFY (result.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (caller.callIdIntervalMinutes (), 1);
    QCOMPARE (caller.autoDisconnectMinutes (), 0);

    result = caller.evaluateQsoAutomation (62000);
    QCOMPARE (result.value (QStringLiteral ("callIdsQueued")).toInt (), 1);
    QCOMPARE (caller.messages (sessionId).size (), 1);
    QVariantMap const callIdMessage = caller.messages (sessionId)
                                          .first ().toMap ();
    QCOMPARE (callIdMessage.value (
                  QStringLiteral ("directionName")).toString (),
              QStringLiteral ("Outgoing"));
    QCOMPARE (callIdMessage.value (QStringLiteral ("text")).toString (),
              QStringLiteral ("DE IU8LMC <ID>"));

    result = caller.evaluateQsoAutomation (130000);
    QCOMPARE (result.value (QStringLiteral ("callIdsQueued")).toInt (), 0);
    QCOMPARE (caller.messages (sessionId).size (), 1);

    result = caller.configureQsoAutomation (1, 3);
    QVERIFY (result.value (QStringLiteral ("ok")).toBool ());
    QVERIFY (caller.configureIncomingPings (false)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (caller.configureLastHeardPeeking (false)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (caller.configureLastConnectionsPeeking (false)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (caller.configureParkedVmailPeeking (false)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (caller.configureVmailParking (false)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (caller.configureSnrReportSending (false)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (caller.configureVerboseSnrAutoAccept (true)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (caller.configureInfoInquire (false)
                 .value (QStringLiteral ("ok")).toBool ());
    caller.setLocalStorePath (storePath);
    QVERIFY (caller.saveLocalStore ());
    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QCOMPARE (restored.callIdIntervalMinutes (), 1);
    QCOMPARE (restored.autoDisconnectMinutes (), 3);
    QVERIFY (!restored.incomingPingsEnabled ());
    QVERIFY (!restored.lastHeardPeekingEnabled ());
    QVERIFY (!restored.lastConnectionsPeekingEnabled ());
    QVERIFY (!restored.parkedVmailPeekingEnabled ());
    QVERIFY (!restored.vmailParkingEnabled ());
    QVERIFY (!restored.snrReportSendingEnabled ());
    QVERIFY (restored.verboseSnrAutoAcceptEnabled ());
    QVERIFY (!restored.infoInquireEnabled ());

    result = caller.evaluateQsoAutomation (182000);
    QCOMPARE (result.value (QStringLiteral ("sessionsClosed")).toInt (), 1);
    QCOMPARE (caller.sessionInfo (sessionId).value (
                  QStringLiteral ("stateName")).toString (),
              QStringLiteral ("Closed"));
    bool sawDisconnect = false;
    for (QVariant const& value : caller.messages (sessionId))
      {
        QString const text = value.toMap ().value (
            QStringLiteral ("text")).toString ();
        if (text.contains (QStringLiteral ("AUTO DISCONNECT")))
          {
            sawDisconnect = true;
          }
      }
    QVERIFY (sawDisconnect);
  }

  void privacyPresetsSummarizeAndPersist ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath ("ft2link-privacy.json");

    FT2LinkQmlAdapter adapter;
    adapter.setLocalStorePath (storePath);
    QVariantMap profile = adapter.privacyProfile ();
    QCOMPARE (profile.value (QStringLiteral ("preset")).toString (),
              QStringLiteral ("OPEN"));
    QVERIFY (profile.value (QStringLiteral ("summary")).toString ().contains (
        QStringLiteral ("LH")));

    QVariantMap result = adapter.applyPrivacyPreset (QStringLiteral ("CONTROL"));
    QCOMPARE (result.value (QStringLiteral ("preset")).toString (),
              QStringLiteral ("CONTROL"));
    QVERIFY (adapter.incomingPingsEnabled ());
    QVERIFY (adapter.lastHeardPeekingEnabled ());
    QVERIFY (!adapter.lastConnectionsPeekingEnabled ());
    QVERIFY (!adapter.parkedVmailPeekingEnabled ());
    QVERIFY (adapter.vmailParkingEnabled ());
    QVERIFY (adapter.snrReportSendingEnabled ());
    QVERIFY (adapter.infoInquireEnabled ());
    QCOMPARE (adapter.qsoAutomation ().value (
                  QStringLiteral ("privacyPreset")).toString (),
              QStringLiteral ("CONTROL"));

    result = adapter.applyPrivacyPreset (QStringLiteral ("QUIET"));
    QCOMPARE (result.value (QStringLiteral ("preset")).toString (),
              QStringLiteral ("QUIET"));
    QVERIFY (!adapter.incomingPingsEnabled ());
    QVERIFY (!adapter.lastHeardPeekingEnabled ());
    QVERIFY (!adapter.lastConnectionsPeekingEnabled ());
    QVERIFY (!adapter.parkedVmailPeekingEnabled ());
    QVERIFY (!adapter.vmailParkingEnabled ());
    QVERIFY (!adapter.snrReportSendingEnabled ());
    QVERIFY (!adapter.infoInquireEnabled ());
    QVERIFY (adapter.saveLocalStore ());

    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QCOMPARE (restored.privacyProfile ().value (
                  QStringLiteral ("preset")).toString (),
              QStringLiteral ("QUIET"));
    QVERIFY (!restored.incomingPingsEnabled ());
    QVERIFY (!restored.infoInquireEnabled ());

    result = adapter.applyPrivacyPreset (QStringLiteral ("OPEN"));
    QCOMPARE (result.value (QStringLiteral ("preset")).toString (),
              QStringLiteral ("OPEN"));
    QVERIFY (adapter.incomingPingsEnabled ());
    QVERIFY (adapter.lastHeardPeekingEnabled ());
    QVERIFY (adapter.lastConnectionsPeekingEnabled ());
    QVERIFY (adapter.parkedVmailPeekingEnabled ());
    QVERIFY (adapter.vmailParkingEnabled ());
    QVERIFY (adapter.snrReportSendingEnabled ());
    QVERIFY (adapter.infoInquireEnabled ());
  }

  void blockedCallsignsPersistFilterAndRejectHello ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath ("ft2link-store.json");

    FT2LinkQmlAdapter adapter;
    QVERIFY (adapter.observeStation (
        "K1ABC", "FN42", "Ann", true, true, true, true, true, 2, 0, 1000));
    QCOMPARE (adapter.stationCount (), 1);

    QVariantMap result = adapter.setBlockedCalls (
        QStringLiteral ("k1abc, n0bad, Z6/TEST"));
    QVERIFY (result.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (adapter.blockedCallCount (), 3);
    QVERIFY (adapter.isCallBlocked (QStringLiteral ("K1ABC")));
    QVERIFY (adapter.isCallBlocked (QStringLiteral ("z6/test")));
    QCOMPARE (adapter.stationCount (), 0);
    QVERIFY (adapter.activeStations (1200, 1000, true).isEmpty ());

    adapter.setLocalStorePath (storePath);
    QVERIFY (adapter.saveLocalStore ());
    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QCOMPARE (restored.blockedCallCount (), 3);
    QVERIFY (restored.blockedCallsText ().contains (
        QStringLiteral ("K1ABC")));

    FT2LinkQmlAdapter caller;
    FT2LinkQmlAdapter answerer;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    answerer.setLocalStation ("K1ABC", "FN42", "Ann");
    answerer.setLocalCapabilities (true, true, true, true, 2, 0);
    QVERIFY (answerer.addBlockedCall (QStringLiteral ("IU8LMC"))
                 .value (QStringLiteral ("ok")).toBool ());

    QVERIFY (caller.observeStation (
        "K1ABC", "FN42", "Ann", true, true, true, true, true, 2, 0, 2000));
    QByteArray const hello = caller.startSessionHelloBytes ("K1ABC", 2100);
    QVERIFY (!hello.isEmpty ());
    QByteArray const ack = answerer.answerHelloBytes ("IU8LMC", hello, 2200);
    QVERIFY (!ack.isEmpty ());
    quint16 const sessionId = answerer.activeSessionId ();
    QCOMPARE (answerer.sessionInfo (sessionId).value (
                  QStringLiteral ("stateName")).toString (),
              QStringLiteral ("Rejected"));
    QVERIFY (answerer.lastError ().contains (QStringLiteral ("blocked")));

    QVERIFY (!caller.receiveHelloAckBytes (ack, 2300));
    QCOMPARE (caller.sessionInfo (caller.activeSessionId ()).value (
                  QStringLiteral ("stateName")).toString (),
              QStringLiteral ("Rejected"));
    QVERIFY (caller.lastError ().contains (
        QStringLiteral ("rejected"), Qt::CaseInsensitive));

    QVERIFY (!answerer.observeStation (
        "IU8LMC", "JN70", "Salvo", true, true, true, true, true, 2, 0, 2400));
    QVERIFY (answerer.activeStations (2500, 1000, true).isEmpty ());
  }

  void adifLogWriterCreatesWatchableFile ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const adifPath = tempDir.filePath (
        QStringLiteral ("ft2link_qso_log.adi"));

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);
    QVERIFY (caller.queueOutgoingText (
        sessionId, QStringLiteral ("hello"), 1100));
    QVERIFY (caller.closeSession (sessionId, 1200));

    QVariantMap const written = caller.writeAdifLogFile (adifPath);
    QVERIFY (written.value ("ok").toBool ());
    QCOMPARE (written.value ("path").toString (), adifPath);
    QVERIFY (written.value ("bytes").toULongLong () > 0ull);
    QCOMPARE (written.value ("qsoCount").toULongLong (), 1ull);
    QVERIFY (QFile::exists (adifPath));

    QFile file {adifPath};
    QVERIFY (file.open (QIODevice::ReadOnly));
    QString const adif = QString::fromUtf8 (file.readAll ());
    QVERIFY (adif.contains (QStringLiteral ("<ADIF_VER:5>3.1.4")));
    QVERIFY (adif.contains (QStringLiteral ("<CALL:5>K1ABC")));
    QVERIFY (adif.contains (QStringLiteral ("<APP_DECODIUM_MODE:8>FT2-LINK")));
  }

  void logbookOutboxQueuesMarksAndPersists ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath (
        QStringLiteral ("ft2link_store.json"));

    FT2LinkQmlAdapter caller;
    caller.setLocalStorePath (storePath);
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);
    QVERIFY (caller.queueOutgoingText (
        sessionId, QStringLiteral ("log me"), 1100));
    QVERIFY (caller.closeSession (sessionId, 1200));

    QSignalSpy outboxSpy {
        &caller, &FT2LinkQmlAdapter::logbookOutboxChanged};
    QVariantMap queued = caller.queueLogbookUpload (
        sessionId, QStringLiteral ("qrz"), 1300);
    QVERIFY (queued.value (QStringLiteral ("ok")).toBool ());
    QVERIFY (queued.value (QStringLiteral ("queued")).toBool ());
    QCOMPARE (queued.value (QStringLiteral ("target")).toString (),
              QStringLiteral ("QRZ"));
    QCOMPARE (caller.logbookOutboxCount (), 1);
    QCOMPARE (outboxSpy.size (), 1);

    QVariantMap duplicate = caller.queueLogbookUpload (
        sessionId, QStringLiteral ("QRZ"), 1400);
    QVERIFY (duplicate.value (QStringLiteral ("ok")).toBool ());
    QVERIFY (duplicate.value (QStringLiteral ("duplicate")).toBool ());
    QCOMPARE (caller.logbookOutboxCount (), 1);

    quint32 const uploadId =
        queued.value (QStringLiteral ("id")).toUInt ();
    QVariantMap payload = caller.logbookUploadPayload (uploadId);
    QVERIFY (payload.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (payload.value (QStringLiteral ("remoteCall")).toString (),
              QStringLiteral ("K1ABC"));
    QVERIFY (payload.value (QStringLiteral ("adif")).toString ().contains (
        QStringLiteral ("<CALL:5>K1ABC")));
    QVERIFY (!payload.value (QStringLiteral ("adifSha256")).toString ().isEmpty ());

    QVariantMap marked = caller.markLogbookUpload (
        uploadId,
        QStringLiteral ("submitted"),
        QStringLiteral ("QRZ Logbook"),
        1500);
    QVERIFY (marked.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (marked.value (QStringLiteral ("state")).toString (),
              QStringLiteral ("Submitted"));
    QVERIFY (caller.logbookOutboxText ().contains (
        QStringLiteral ("Submitted QRZ K1ABC")));
    QVariantMap stats = caller.statistics ();
    QCOMPARE (stats.value (QStringLiteral ("logbookOutboxTotal")).toULongLong (),
              1ull);
    QCOMPARE (stats.value (QStringLiteral ("logbookSubmitted")).toULongLong (),
              1ull);
    QCOMPARE (caller.localStoreAudit ().value (
                  QStringLiteral ("logbookOutboxCount")).toULongLong (),
              1ull);

    QVERIFY (caller.saveLocalStore ());
    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QCOMPARE (restored.logbookOutboxCount (), 1);
    QVariantList restoredOutbox = restored.logbookOutbox ();
    QCOMPARE (restoredOutbox.size (), 1);
    QVariantMap restoredItem = restoredOutbox.first ().toMap ();
    QCOMPARE (restoredItem.value (QStringLiteral ("id")).toUInt (),
              uploadId);
    QCOMPARE (restoredItem.value (QStringLiteral ("state")).toString (),
              QStringLiteral ("Submitted"));
    QVERIFY (restored.logsBundleText ().contains (
        QStringLiteral ("LOGBOOK OUTBOX")));

    QVariantMap sent = restored.markLogbookUpload (
        uploadId, QStringLiteral ("sent"), QStringLiteral ("done"), 1600);
    QVERIFY (sent.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (sent.value (QStringLiteral ("state")).toString (),
              QStringLiteral ("Sent"));
    restored.clearLogbookOutbox ();
    QCOMPARE (restored.logbookOutboxCount (), 0);
  }

  void cqSlotBeaconCarriesSlotMetadata ()
  {
    decodium::ft2link::FT2LinkAppModel sender {
      decodium::ft2link::StationIdentity {"IU8LMC", "JN70", "Salvo"}};
    decodium::ft2link::Frame const beacon =
        sender.makeLocalBeaconFrame (true, -3, 750);

    decodium::ft2link::FT2LinkAppModel receiver {
      decodium::ft2link::StationIdentity {"K1ABC", "FN42", "Ann"}};
    decodium::ft2link::StationAdvertisement advertisement;
    QVERIFY (receiver.observeBeacon (beacon, 1000u, &advertisement));
    QCOMPARE (QString::fromStdString (advertisement.station.call),
              QStringLiteral ("IU8LMC"));
    QVERIFY (advertisement.cq);
    QCOMPARE (advertisement.cqSlotId, -3);
    QCOMPARE (advertisement.cqSlotSizeHz, 750);
    QCOMPARE (advertisement.cqSlotOffsetHz, -2250);

    FT2LinkQmlAdapter adapter;
    adapter.setLocalStation ("K1ABC", "FN42", "Ann");
    QVERIFY (adapter.ingestRadioFrameBytes (
        frameBytes (beacon), QStringLiteral ("IU8LMC"), 1000u, false));
    QVariantList const active = adapter.activeStations (2000u, 300000u, true);
    QCOMPARE (active.size (), 1);
    QVariantMap const station = active[0].toMap ();
    QCOMPARE (station.value ("cqSlotId").toInt (), -3);
    QCOMPARE (station.value ("cqSlotOffsetHz").toInt (), -2250);
    QCOMPARE (station.value ("cqSlotSizeHz").toInt (), 750);
    QCOMPARE (station.value ("cqSlotLabel").toString (), QStringLiteral ("S-3"));

    decodium::ft2link::Frame const specialBeacon =
        sender.makeLocalBeaconFrame (true, 1, 750, "NET", "JN71");
    QVERIFY (adapter.ingestRadioFrameBytes (
        frameBytes (specialBeacon), QStringLiteral ("IU8LMC"), 2200u, false));
    QVariantList const specialActive =
        adapter.activeStations (2300u, 300000u, true);
    QCOMPARE (specialActive.size (), 1);
    QVariantMap const specialStation = specialActive[0].toMap ();
    QCOMPARE (specialStation.value ("cqType").toString (),
              QStringLiteral ("NET"));
    QCOMPARE (specialStation.value ("cqLocator").toString (),
              QStringLiteral ("JN71"));
    QCOMPARE (specialStation.value ("locator").toString (),
              QStringLiteral ("JN71"));
    QCOMPARE (specialStation.value ("cqSlotId").toInt (), 1);
    QCOMPARE (specialStation.value ("cqSlotOffsetHz").toInt (), 750);
    QCOMPARE (adapter.beaconHistoryCount (), 2);
    QVariantList const history = adapter.beaconHistory ();
    QCOMPARE (history.first ().toMap ().value ("cqType").toString (),
              QStringLiteral ("NET"));
    QCOMPARE (history.first ().toMap ().value ("cqSlotLabel").toString (),
              QStringLiteral ("S+1"));
    QVERIFY (adapter.beaconHistoryText ().contains (
        QStringLiteral ("NET | IU8LMC")));
  }

  void specialCqRadioPlanCarriesTypeAndLocator ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter adapter;
    adapter.setLocalStation ("IU8LMC", "JN70", "Salvo");
    QSignalSpy radioSpy {&adapter, &FT2LinkQmlAdapter::radioTxAudioRequested};

    adapter.setRadioTxArmed (true);
    QVERIFY (adapter.transmitSpecialCqRadio (
        QStringLiteral ("EMCOMM"), QStringLiteral ("JN71"), 2, 750, 1000));
    QVERIFY (!adapter.radioTxArmed ());
    QCOMPARE (radioSpy.size (), 1);
    QList<QVariant> const request = radioSpy.takeFirst ();
    QCOMPARE (request[0].toString (), QStringLiteral ("FT2-Link CQ EMCOMM"));
    QVariantMap const plan = request[2].toMap ();
    QCOMPARE (plan.value ("kind").toString (), QStringLiteral ("CQ EMCOMM"));
    QCOMPARE (plan.value ("cqType").toString (), QStringLiteral ("EMCOMM"));
    QCOMPARE (plan.value ("cqLocator").toString (), QStringLiteral ("JN71"));
    QCOMPARE (plan.value ("sequence").toInt (), 2);
    QCOMPARE (plan.value ("ackBase").toInt (), 750);
    QCOMPARE (plan.value ("ackBitmap").toInt (), 3);
    QCOMPARE (adapter.beaconHistoryCount (), 1);
    QVariantMap const history = adapter.beaconHistory ().first ().toMap ();
    QCOMPARE (history.value ("direction").toString (), QStringLiteral ("TX"));
    QCOMPARE (history.value ("cqType").toString (), QStringLiteral ("EMCOMM"));
    QCOMPARE (history.value ("cqLocator").toString (), QStringLiteral ("JN71"));
    QCOMPARE (history.value ("source").toString (), QStringLiteral ("MANUAL"));
  }

  void incomingControlTagsCreateSystemEventsAndCloseSession ()
  {
    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    caller.setLocalOperatorProfile (
        "Napoli", "", "", "IC-7300", "Dipole", "50W", "40.8N 14.3E");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);
    QVERIFY (caller.observeStation (
        "N0NET", "EM12", "Relay", true, true, true, true, true, 2, 0, 3500));
    QVERIFY (caller.parkMailbox (
        QStringLiteral ("K1ABC"),
        QStringLiteral ("Parked"),
        QStringLiteral ("Waiting for pickup"),
        3600));

    QVERIFY (caller.appendIncomingText (
        sessionId,
        QStringLiteral (
            "HIHI! TU! LIKE! DING RING "
            "<NAME:Ann> <QTH:Boston> <LOC:FN42> <RIG:K3> <ANT:Yagi> <PWR:80W> <R+07> "
            "<AWAY> <TYP> <SR> <INFO> <LOCR> <LHR> <LHC:N0NET> <FSR> <FS:MON 14105750> "
            "<VRP> <VW> <LCR> <LC:K1ABC,N0NET> <GPSR> <VER> <QSYU> <QSYR> <QSYJ> <QJO> "
            "<VSI> <VSIR> <VSIJ> <VSS> <IE> <AE> <TL> <A> <EA> AI: WEATHER <ACIE> OK "
            "<AIJ> <AIE> <AIL> <DISAI> <FC:W/4Z1AC> <SF:file.txt|5> <SFRD> <SFOK> "
            "<SFFA> <SFAB> <SFB:1> <SM> <TO:IU8LMC> <FRM:K1ABC> <TME:2026-06-30> "
            "<SBJ:Hello> <MSG:Body> <EG> <EJ> <U> <SMR> <SMF> <SMFP> "
            "<P:chess> <PM:e2e4> <PA> <PE> <PJ> <PR>"),
        4000));
    QVariantList messages = caller.messages (sessionId);
    QVERIFY (messages.size () >= 9);

    bool sawAway = false;
    bool sawTyping = false;
    bool sawSnrRequest = false;
    bool sawQsy = false;
    bool sawLastHeard = false;
    bool sawLastHeardSpecific = false;
    bool sawFrequencySchedule = false;
    bool sawParkedMailbox = false;
    bool sawRecentConnections = false;
    bool sawGpsReply = false;
    bool sawProfile = false;
    bool sawInfoReply = false;
    bool sawVerboseSnr = false;
    bool sawTestLink = false;
    bool sawAiGateway = false;
    bool sawGesture = false;
    bool sawSoundCue = false;
    bool sawLegacyFile = false;
    bool sawLegacyVmail = false;
    bool sawHamPlay = false;
    int systemCount = 0;
    for (QVariant const& value : messages)
      {
        QVariantMap const message = value.toMap ();
        if (message.value ("directionName").toString ()
            != QStringLiteral ("System"))
          {
            continue;
          }
        ++systemCount;
        QString const text = message.value ("text").toString ();
        sawAway = sawAway || text.contains (QStringLiteral ("is away"));
        sawTyping = sawTyping || text.contains (QStringLiteral ("is typing"));
        sawSnrRequest = sawSnrRequest
            || text.contains (QStringLiteral ("requested SNR"));
        sawQsy = sawQsy || text.contains (QStringLiteral ("QSY up 750 Hz"));
        sawLastHeard = sawLastHeard
            || (text.contains (QStringLiteral ("<LH:"))
                && text.contains (QStringLiteral ("N0NET")));
        sawLastHeardSpecific = sawLastHeardSpecific
            || (text.contains (QStringLiteral ("requested last-heard for N0NET"))
                && text.contains (QStringLiteral ("<LHC:N0NET|EM12|0m>")));
        sawFrequencySchedule = sawFrequencySchedule
            || (text.contains (QStringLiteral ("requested frequency schedule"))
                && text.contains (QStringLiteral ("<FS:")))
            || text.contains (QStringLiteral ("frequency schedule MON 14105750"));
        sawParkedMailbox = sawParkedMailbox
            || (text.contains (QStringLiteral ("requested parked VMail peek"))
                && text.contains (QStringLiteral ("<VW>")));
        sawRecentConnections = sawRecentConnections
            || (text.contains (QStringLiteral ("requested recent connections"))
                && text.contains (QStringLiteral ("<LC:")))
            || text.contains (QStringLiteral ("recent connections K1ABC,N0NET"));
        sawGpsReply = sawGpsReply
            || (text.contains (QStringLiteral ("requested GPS"))
                && text.contains (QStringLiteral ("<GPS:40.8N 14.3E>")));
        sawProfile = sawProfile
            || (text.contains (QStringLiteral ("profile"))
                && text.contains (QStringLiteral ("Boston"))
                && text.contains (QStringLiteral ("K3")));
        sawInfoReply = sawInfoReply
            || (text.contains (QStringLiteral ("suggested reply"))
                && text.contains (QStringLiteral ("<QTH:Napoli>"))
                && text.contains (QStringLiteral ("<RIG:IC-7300>")));
        sawVerboseSnr = sawVerboseSnr
            || text.contains (QStringLiteral ("verbose SNR mode"));
        sawTestLink = sawTestLink
            || text.contains (QStringLiteral ("test-link marker"));
        sawAiGateway = sawAiGateway
            || text.contains (QStringLiteral ("AI gateway"));
        sawGesture = sawGesture
            || text.contains (QStringLiteral ("gesture HIHI"));
        sawSoundCue = sawSoundCue
            || text.contains (QStringLiteral ("sound cue DING"));
        sawLegacyFile = sawLegacyFile
            || text.contains (QStringLiteral ("legacy send-file header"))
            || text.contains (QStringLiteral ("legacy file received"));
        sawLegacyVmail = sawLegacyVmail
            || text.contains (QStringLiteral ("legacy VMail fields"))
            || text.contains (QStringLiteral ("legacy VMail received"));
        sawHamPlay = sawHamPlay
            || text.contains (QStringLiteral ("HamPlay"));
      }
    QVERIFY (systemCount >= 8);
    QVERIFY (sawAway);
    QVERIFY (sawTyping);
    QVERIFY (sawSnrRequest);
    QVERIFY (sawQsy);
    QVERIFY (sawLastHeard);
    QVERIFY (sawLastHeardSpecific);
    QVERIFY (sawFrequencySchedule);
    QVERIFY (sawParkedMailbox);
    QVERIFY (sawRecentConnections);
    QVERIFY (sawGpsReply);
    QVERIFY (sawProfile);
    QVERIFY (sawInfoReply);
    QVERIFY (sawVerboseSnr);
    QVERIFY (sawTestLink);
    QVERIFY (sawAiGateway);
    QVERIFY (sawGesture);
    QVERIFY (sawSoundCue);
    QVERIFY (sawLegacyFile);
    QVERIFY (sawLegacyVmail);
    QVERIFY (sawHamPlay);
    QCOMPARE (caller.typingPeerCount (), 1);
    QVERIFY (caller.typingSummary (4001).contains (QStringLiteral ("K1ABC")));
    QVERIFY (caller.typingIndicators (17001).isEmpty ());
    QCOMPARE (caller.typingPeerCount (), 0);
    QVERIFY (caller.appendIncomingText (
        sessionId, QStringLiteral ("<TYP>"), 18000));
    QCOMPARE (caller.typingPeerCount (), 1);
    QVERIFY (caller.appendIncomingText (
        sessionId, QStringLiteral ("<TYP0>"), 18100));
    QCOMPARE (caller.typingPeerCount (), 0);

    bool contactUpdated = false;
    for (QVariant const& value : caller.contactHistory ())
      {
        QVariantMap const contact = value.toMap ();
        if (contact.value ("call").toString () == QStringLiteral ("K1ABC"))
          {
            contactUpdated =
                contact.value ("name").toString () == QStringLiteral ("Ann")
                && contact.value ("locator").toString () == QStringLiteral ("FN42");
          }
      }
    QVERIFY (contactUpdated);
    QCOMPARE (caller.pathReportCount (), 1);
    QVariantMap const pathReport = caller.pathReports ().first ().toMap ();
    QCOMPARE (pathReport.value (QStringLiteral ("remoteCall")).toString (),
              QStringLiteral ("K1ABC"));
    QCOMPARE (pathReport.value (QStringLiteral ("locator")).toString (),
              QStringLiteral ("FN42"));
    QCOMPARE (pathReport.value (QStringLiteral ("snrValid")).toBool (), true);
    QCOMPARE (pathReport.value (QStringLiteral ("snrDb")).toInt (), 7);
    QVariantMap const pathAnalysis = caller.pathAnalysis (
        QStringLiteral ("K1ABC"),
        QStringLiteral ("FN"));
    QCOMPARE (pathAnalysis.value (QStringLiteral ("snrCount")).toInt (), 1);
    QCOMPARE (pathAnalysis.value (QStringLiteral ("bestHourUtc")).toInt (), 0);
    QCOMPARE (pathAnalysis.value (QStringLiteral ("avgSnr")).toDouble (), 7.0);
    QVariantMap const tagStats = caller.statistics ();
    QCOMPARE (tagStats.value (QStringLiteral ("snrsReceived")).toULongLong (),
              1ull);
    QVERIFY (caller.statisticsText ().contains (
        QStringLiteral ("SNRs received")));
    QCOMPARE (caller.sessionInfo (sessionId).value ("stateName").toString (),
              QStringLiteral ("Connected"));

    QVERIFY (caller.appendIncomingText (
        sessionId, QStringLiteral ("73 <DISC>"), 5000));
    QCOMPARE (caller.sessionInfo (sessionId).value ("stateName").toString (),
              QStringLiteral ("Closed"));
    messages = caller.messages (sessionId);
    bool sawDisconnect = false;
    for (QVariant const& value : messages)
      {
        QVariantMap const message = value.toMap ();
        sawDisconnect = sawDisconnect
            || message.value ("text").toString ().contains (
                QStringLiteral ("requested disconnect"));
      }
    QVERIFY (sawDisconnect);
  }

  void lastHeardAndSnrTogglesRejectInquireSuggestions ()
  {
    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);
    QVERIFY (caller.observeStation (
        "N0NET", "EM12", "Relay", true, true, true, true, true, 2, 0, 2000));
    QVERIFY (caller.configureAutoReply (true)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (caller.configureLastHeardPeeking (false)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (caller.configureLastConnectionsPeeking (false)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (caller.configureSnrReportSending (false)
                 .value (QStringLiteral ("ok")).toBool ());

    QVERIFY (caller.appendIncomingText (
        sessionId, QStringLiteral ("REQ <SR> <LHR> <LHC:N0NET> <LCR>"), 3000));

    bool sawSnrDisabled = false;
    bool sawSnrSuggestion = false;
    bool sawLastHeardRejected = false;
    bool sawLastConnectionsRejected = false;
    bool sawAutoReplyRejected = false;
    bool sawLeakedLastHeard = false;
    bool sawLeakedLastConnections = false;
    for (QVariant const& value : caller.messages (sessionId))
      {
        QVariantMap const message = value.toMap ();
        QString const direction = message.value (
            QStringLiteral ("directionName")).toString ();
        QString const text = message.value (
            QStringLiteral ("text")).toString ();
        if (direction == QStringLiteral ("System"))
          {
            sawSnrDisabled = sawSnrDisabled
                || text.contains (QStringLiteral ("SNR report sending is disabled"));
            sawSnrSuggestion = sawSnrSuggestion
                || text.contains (QStringLiteral ("<R+00>"));
            sawLastHeardRejected = sawLastHeardRejected
                || text.contains (QStringLiteral ("<LHJ>"));
            sawLastConnectionsRejected = sawLastConnectionsRejected
                || text.contains (QStringLiteral ("<LCJ>"));
            sawLeakedLastHeard = sawLeakedLastHeard
                || text.contains (QStringLiteral ("<LH:"))
                || text.contains (QStringLiteral ("<LHC:N0NET|"));
            sawLeakedLastConnections = sawLeakedLastConnections
                || text.contains (QStringLiteral ("<LC:"));
          }
        if (direction == QStringLiteral ("Outgoing"))
          {
            sawAutoReplyRejected = sawAutoReplyRejected
                || (text.contains (QStringLiteral ("<LHJ>"))
                    && text.contains (QStringLiteral ("<LCJ>")));
            sawLeakedLastHeard = sawLeakedLastHeard
                || text.contains (QStringLiteral ("<LH:"))
                || text.contains (QStringLiteral ("<LHC:N0NET|"));
            sawLeakedLastConnections = sawLeakedLastConnections
                || text.contains (QStringLiteral ("<LC:"));
          }
      }

    QVERIFY (sawSnrDisabled);
    QVERIFY (!sawSnrSuggestion);
    QVERIFY (sawLastHeardRejected);
    QVERIFY (sawLastConnectionsRejected);
    QVERIFY (sawAutoReplyRejected);
    QVERIFY (!sawLeakedLastHeard);
    QVERIFY (!sawLeakedLastConnections);
    QVERIFY (caller.statisticsText ().contains (
        QStringLiteral ("Last-heard peeking: off")));
    QVERIFY (caller.statisticsText ().contains (
        QStringLiteral ("Last-connections peeking: off")));
    QVERIFY (caller.statisticsText ().contains (
        QStringLiteral ("SNR report suggestions: off")));
  }

  void infoInquireToggleSuppressesPersonalAutoReplies ()
  {
    FT2LinkQmlAdapter answerer;
    answerer.setLocalStation ("K1ABC", "FN42", "Ann");
    answerer.setLocalOperatorProfile (
        QStringLiteral ("Rome"),
        QStringLiteral ("ann@example.net"),
        QStringLiteral ("QRV"),
        QStringLiteral ("IC-7300"),
        QStringLiteral ("Dipole"),
        QStringLiteral ("50W"),
        QStringLiteral ("41.900N 12.500E"));
    answerer.setLocalCapabilities (true, true, true, true, 2, 0);
    QVERIFY (answerer.configureAutoReply (true)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (answerer.configureInfoInquire (false)
                 .value (QStringLiteral ("ok")).toBool ());

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    QVERIFY (caller.observeStation (
        "K1ABC", "FN42", "Ann", true, true, true, true, true, 2, 0, 1000));
    QByteArray const hello = caller.startSessionHelloBytes ("K1ABC", 1100);
    QVERIFY (!hello.isEmpty ());
    QByteArray const ack = answerer.answerHelloBytes ("IU8LMC", hello, 1200);
    QVERIFY (!ack.isEmpty ());
    quint16 const sessionId = answerer.activeSessionId ();
    QVERIFY (sessionId != 0u);

    QVERIFY (answerer.appendIncomingText (
        sessionId,
        QStringLiteral ("REQ <INFO> <LOCR> <GPSR> <VER>"),
        1300));

    bool sawDisabledNotice = false;
    bool sawPersonalLeak = false;
    bool sawVersionAutoReply = false;
    for (QVariant const& value : answerer.messages (sessionId))
      {
        QVariantMap const message = value.toMap ();
        QString const direction = message.value (
            QStringLiteral ("directionName")).toString ();
        QString const text = message.value (
            QStringLiteral ("text")).toString ();
        sawDisabledNotice = sawDisabledNotice
            || text.contains (QStringLiteral ("information inquiries are disabled"));
        if (direction == QStringLiteral ("Outgoing"))
          {
            sawVersionAutoReply = sawVersionAutoReply
                || text.contains (QStringLiteral ("FT2-Link Decodium v0.1"));
            sawPersonalLeak = sawPersonalLeak
                || text.contains (QStringLiteral ("<NAME:"))
                || text.contains (QStringLiteral ("<QTH:"))
                || text.contains (QStringLiteral ("<LOC:"))
                || text.contains (QStringLiteral ("<GPS:"))
                || text.contains (QStringLiteral ("<RIG:"))
                || text.contains (QStringLiteral ("<ANT:"))
                || text.contains (QStringLiteral ("<PWR:"));
          }
      }

    QVERIFY (sawDisabledNotice);
    QVERIFY (sawVersionAutoReply);
    QVERIFY (!sawPersonalLeak);
    QVERIFY (answerer.statisticsText ().contains (
        QStringLiteral ("Info inquiries: off")));
  }

  void parkedVmailPeekingToggleRejectsMailboxDisclosure ()
  {
    FT2LinkQmlAdapter answerer;
    answerer.setLocalStation ("K1ABC", "FN42", "Ann");
    answerer.setLocalCapabilities (true, true, true, true, 2, 0);
    QVERIFY (answerer.parkMailbox (
        QStringLiteral ("IU8LMC"),
        QStringLiteral ("Relay"),
        QStringLiteral ("Parked traffic"),
        1000));
    QCOMPARE (answerer.mailboxCount (), 1);
    QVERIFY (answerer.configureAutoReply (true)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (answerer.configureParkedVmailPeeking (false)
                 .value (QStringLiteral ("ok")).toBool ());

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    QVERIFY (caller.observeStation (
        "K1ABC", "FN42", "Ann", true, true, true, true, true, 2, 0, 1100));
    QByteArray const hello = caller.startSessionHelloBytes ("K1ABC", 1200);
    QVERIFY (!hello.isEmpty ());
    QByteArray const ack = answerer.answerHelloBytes ("IU8LMC", hello, 1300);
    QVERIFY (!ack.isEmpty ());
    quint16 const sessionId = answerer.activeSessionId ();
    QVERIFY (sessionId != 0u);

    QVERIFY (answerer.appendIncomingText (
        sessionId, QStringLiteral ("REQ <VRP>"), 1400));

    bool sawReject = false;
    bool sawWaitingLeak = false;
    bool sawAutoReject = false;
    for (QVariant const& value : answerer.messages (sessionId))
      {
        QVariantMap const message = value.toMap ();
        QString const direction = message.value (
            QStringLiteral ("directionName")).toString ();
        QString const text = message.value (
            QStringLiteral ("text")).toString ();
        sawReject = sawReject || text.contains (QStringLiteral ("<VRPJ>"));
        sawWaitingLeak = sawWaitingLeak
            || text.contains (QStringLiteral ("<VW>"))
            || text.contains (QStringLiteral ("parked VMail waiting"));
        if (direction == QStringLiteral ("Outgoing"))
          {
            sawAutoReject = sawAutoReject
                || text.contains (QStringLiteral ("<VRPJ>"));
          }
      }

    QVERIFY (sawReject);
    QVERIFY (sawAutoReject);
    QVERIFY (!sawWaitingLeak);
    QVERIFY (answerer.statisticsText ().contains (
        QStringLiteral ("Parked VMail peeking: off")));
  }

  void verboseSnrAutoAcceptQueuesAcceptedReply ()
  {
    FT2LinkQmlAdapter answerer;
    answerer.setLocalStation ("K1ABC", "FN42", "Ann");
    answerer.setLocalCapabilities (true, true, true, true, 2, 0);
    QVERIFY (answerer.configureAutoReply (true)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (answerer.configureVerboseSnrAutoAccept (true)
                 .value (QStringLiteral ("ok")).toBool ());

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    QVERIFY (caller.observeStation (
        "K1ABC", "FN42", "Ann", true, true, true, true, true, 2, 0, 1000));
    QByteArray const hello = caller.startSessionHelloBytes ("K1ABC", 1100);
    QVERIFY (!hello.isEmpty ());
    QByteArray const ack = answerer.answerHelloBytes ("IU8LMC", hello, 1200);
    QVERIFY (!ack.isEmpty ());
    quint16 const sessionId = answerer.activeSessionId ();
    QVERIFY (sessionId != 0u);

    QVERIFY (answerer.appendIncomingText (
        sessionId, QStringLiteral ("REQ <VSI>"), 1300));

    bool sawAutoNotice = false;
    bool sawAcceptedReply = false;
    for (QVariant const& value : answerer.messages (sessionId))
      {
        QVariantMap const message = value.toMap ();
        QString const direction = message.value (
            QStringLiteral ("directionName")).toString ();
        QString const text = message.value (
            QStringLiteral ("text")).toString ();
        sawAutoNotice = sawAutoNotice
            || text.contains (QStringLiteral ("auto-accept reply: <VSIR>"));
        if (direction == QStringLiteral ("Outgoing"))
          {
            sawAcceptedReply = sawAcceptedReply
                || text.contains (QStringLiteral ("<VSIR>"));
          }
      }

    QVERIFY (sawAutoNotice);
    QVERIFY (sawAcceptedReply);
    QVERIFY (answerer.statisticsText ().contains (
        QStringLiteral ("Verbose SNR auto-accept: on")));
  }

  void vmailParkingToggleRejectsThirdPartyRelayEnvelope ()
  {
    FT2LinkQmlAdapter relay;
    relay.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (relay, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);
    QVERIFY (relay.configureVmailParking (false)
                 .value (QStringLiteral ("ok")).toBool ());

    QString const envelope = QStringLiteral ("FT2M1|N0CALL|K1ABC|%1|%2")
        .arg (hexUtf8 (QStringLiteral ("Relay needed")),
              hexUtf8 (QStringLiteral ("Do not park this")));
    QVERIFY (relay.appendIncomingText (sessionId, envelope, 1200));

    QCOMPARE (relay.mailboxCount (), 0);
    bool sawRejectedRelay = false;
    for (QVariant const& value : relay.messages (sessionId))
      {
        QString const text = value.toMap ().value (
            QStringLiteral ("text")).toString ();
        sawRejectedRelay = sawRejectedRelay
            || text.contains (QStringLiteral (
                "RELAY MAIL rejected for N0CALL from K1ABC: Relay needed"));
      }

    QVERIFY (sawRejectedRelay);
    QVERIFY (relay.statisticsText ().contains (
        QStringLiteral ("VMail parking: off")));
  }

  void broadcastRadioAndAlertTags ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter adapter;
    adapter.setLocalStation ("IU8LMC", "JN70", "Salvo");
    QSignalSpy radioSpy {&adapter, &FT2LinkQmlAdapter::radioTxAudioRequested};
    QSignalSpy broadcastSpy {&adapter, &FT2LinkQmlAdapter::broadcastsChanged};
    QSignalSpy alertSpy {&adapter, &FT2LinkQmlAdapter::alertsChanged};

    QVERIFY (adapter.alertTags ().contains (QStringLiteral ("SOS")));
    QVariantMap alertConfig = adapter.setCustomAlertTags ("wx, POTA, wx");
    QVERIFY (alertConfig.value ("ok").toBool ());
    QCOMPARE (adapter.customAlertTags ().size (), 2);
    QVERIFY (adapter.alertTags ().contains (QStringLiteral ("WX")));
    adapter.setRadioTxArmed (true);
    QVERIFY (adapter.transmitBroadcastRadio (QStringLiteral ("SOS CHECK NET"), 3000));
    QCOMPARE (radioSpy.size (), 1);
    QCOMPARE (adapter.broadcastCount (), 1);
    QCOMPARE (adapter.alertCount (), 1);
    QVERIFY (!adapter.radioTxArmed ());
    QVERIFY (broadcastSpy.size () >= 1);
    QVERIFY (alertSpy.size () >= 1);

    QList<QVariant> const request = radioSpy.takeFirst ();
    QCOMPARE (request[0].toString (), QStringLiteral ("FT2-Link BCAST"));
    QVariantMap const plan = request[2].toMap ();
    QCOMPARE (plan.value ("kind").toString (), QStringLiteral ("BCAST"));
    QCOMPARE (plan.value ("frameTypeName").toString (), QStringLiteral ("BROADCAST"));
    QCOMPARE (plan.value ("profileName").toString (), QStringLiteral ("NARROW"));

    QVariantMap firstBroadcast = adapter.broadcasts ().first ().toMap ();
    QCOMPARE (firstBroadcast.value ("source").toString (), QStringLiteral ("TX"));
    QCOMPARE (firstBroadcast.value ("fromCall").toString (), QStringLiteral ("IU8LMC"));
    QVERIFY (firstBroadcast.value ("alert").toBool ());

    adapter.setRadioTxArmed (true);
    QVERIFY (adapter.transmitBroadcastRadio (QStringLiteral ("WX UPDATE"), 3100));
    QCOMPARE (adapter.broadcastCount (), 2);
    QVERIFY (adapter.alertCount () >= 2);
    QVariantMap customBroadcast = adapter.broadcasts ().last ().toMap ();
    QCOMPARE (customBroadcast.value ("alertTags").toStringList (),
              QStringList {QStringLiteral ("WX")});

    decodium::ft2link::Frame incoming;
    incoming.type = decodium::ft2link::FrameType::Broadcast;
    incoming.profile = decodium::ft2link::Profile::Narrow;
    incoming.flags = decodium::ft2link::FlagEndOfMessage;
    QByteArray const payload = QByteArrayLiteral ("URGENT MEDICAL");
    incoming.payload.assign (
        reinterpret_cast<std::uint8_t const*> (payload.constData ()),
        reinterpret_cast<std::uint8_t const*> (payload.constData ()) + payload.size ());

    QVERIFY (adapter.ingestRadioFrameBytes (frameBytes (incoming),
                                           QStringLiteral ("K1ABC"),
                                           4000,
                                           true));
    QCOMPARE (adapter.broadcastCount (), 3);
    QVERIFY (adapter.alertCount () >= 3);
    QCOMPARE (adapter.transportState (), QStringLiteral ("ALERT RX"));
    QVariantMap secondBroadcast = adapter.broadcasts ().last ().toMap ();
    QCOMPARE (secondBroadcast.value ("source").toString (), QStringLiteral ("RX"));
    QCOMPARE (secondBroadcast.value ("fromCall").toString (), QStringLiteral ("K1ABC"));
    QVERIFY (secondBroadcast.value ("alert").toBool ());
  }

  void pathFinderBroadcastsUseContactHistory ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter relay;
    relay.setLocalStation ("IU8LMC", "JN70", "Salvo");
    QVERIFY (relay.observeStation (
        QStringLiteral ("N0CALL"),
        QStringLiteral ("EM12"),
        QStringLiteral ("Lee"),
        true,
        true,
        true,
        true,
        true,
        2,
        0,
        1000));

    QVariantMap candidate = relay.pathFinderCandidate (
        QStringLiteral ("N0CALL"), 61000);
    QVERIFY (candidate.value ("canRespond").toBool ());
    QCOMPARE (candidate.value ("locator").toString (), QStringLiteral ("EM12"));
    QCOMPARE (candidate.value ("ageMinutes").toULongLong (), 1ull);

    QSignalSpy radioSpy {&relay, &FT2LinkQmlAdapter::radioTxAudioRequested};
    relay.setRadioTxArmed (true);
    QVERIFY (relay.transmitPathFinderResponseRadio (
        QStringLiteral ("N0CALL"), 61000));
    QCOMPARE (radioSpy.size (), 1);
    QCOMPARE (relay.transportState (), QStringLiteral ("PATH RESP TX"));
    QVariantMap responsePlan = radioSpy.takeFirst ()[2].toMap ();
    QCOMPARE (responsePlan.value ("kind").toString (),
              QStringLiteral ("PATH_ACK"));
    QCOMPARE (responsePlan.value ("frameTypeName").toString (),
              QStringLiteral ("BROADCAST"));
    QVERIFY (relay.broadcasts ().last ().toMap ().value (
                 "text").toString ().startsWith (
                     QStringLiteral ("P! N0CALL IU8LMC")));

    FT2LinkQmlAdapter requester;
    requester.setLocalStation ("K1ABC", "FN42", "Ann");
    requester.setRadioTxArmed (true);
    QVERIFY (requester.transmitPathFinderRadio (
        QStringLiteral ("N0CALL"), 70000));
    QCOMPARE (requester.transportState (), QStringLiteral ("PATH TX"));
    QCOMPARE (requester.broadcasts ().last ().toMap ().value (
                  "text").toString (),
              QStringLiteral ("P? N0CALL K1ABC"));

    decodium::ft2link::Frame requestFrame;
    requestFrame.type = decodium::ft2link::FrameType::Broadcast;
    requestFrame.profile = decodium::ft2link::Profile::Narrow;
    requestFrame.flags = decodium::ft2link::FlagEndOfMessage;
    QByteArray const requestPayload = QByteArrayLiteral ("P? N0CALL K1ABC");
    requestFrame.payload.assign (
        reinterpret_cast<std::uint8_t const*> (requestPayload.constData ()),
        reinterpret_cast<std::uint8_t const*> (requestPayload.constData ())
            + requestPayload.size ());

    QVERIFY (relay.ingestRadioFrameBytes (
        frameBytes (requestFrame), QStringLiteral ("K1ABC"), 71000, true));
    QCOMPARE (relay.transportState (), QStringLiteral ("PATH RX"));
    QVERIFY (relay.alertCount () >= 1);
    QVariantMap pathAlert = relay.alertEvents ().last ().toMap ();
    QCOMPARE (pathAlert.value ("source").toString (), QStringLiteral ("Path"));
    QCOMPARE (pathAlert.value ("tag").toString (), QStringLiteral ("PATH"));
    QVERIFY (pathAlert.value ("text").toString ().contains (
        QStringLiteral ("PATH request from K1ABC for N0CALL")));

    decodium::ft2link::Frame responseFrame;
    responseFrame.type = decodium::ft2link::FrameType::Broadcast;
    responseFrame.profile = decodium::ft2link::Profile::Narrow;
    responseFrame.flags = decodium::ft2link::FlagEndOfMessage;
    QByteArray const responsePayload =
        QByteArrayLiteral ("P! N0CALL IU8LMC EM12 1M");
    responseFrame.payload.assign (
        reinterpret_cast<std::uint8_t const*> (responsePayload.constData ()),
        reinterpret_cast<std::uint8_t const*> (responsePayload.constData ())
            + responsePayload.size ());

    QVERIFY (requester.ingestRadioFrameBytes (
        frameBytes (responseFrame), QStringLiteral ("IU8LMC"), 72000, true));
    QCOMPARE (requester.transportState (), QStringLiteral ("PATH RX"));
    QVariantMap foundAlert = requester.alertEvents ().last ().toMap ();
    QCOMPARE (foundAlert.value ("source").toString (), QStringLiteral ("Path"));
    QVERIFY (foundAlert.value ("text").toString ().contains (
        QStringLiteral ("PATH found N0CALL via IU8LMC")));
    QVariantMap relayHint = requester.pathRelayCandidate (
        QStringLiteral ("N0CALL"), 73000);
    QVERIFY (relayHint.value ("canRelay").toBool ());
    QCOMPARE (relayHint.value ("relayCall").toString (),
              QStringLiteral ("IU8LMC"));
    QCOMPARE (relayHint.value ("locator").toString (),
              QStringLiteral ("EM12"));
    QCOMPARE (relayHint.value ("parkedMailboxCount").toInt (), 0);

    QVERIFY (requester.parkMailbox (
        QStringLiteral ("N0CALL"),
        QStringLiteral ("Path relay"),
        QStringLiteral ("Message to forward over path relay"),
        73500));
    relayHint = requester.pathRelayCandidate (
        QStringLiteral ("N0CALL"), 74000);
    QCOMPARE (relayHint.value ("parkedMailboxCount").toInt (), 1);
    QVERIFY (relayHint.value ("readyToForward").toBool ());
    QVariantMap workflow = requester.relayWorkflowForStation (
        QStringLiteral ("IU8LMC"), 74000);
    QVERIFY (workflow.value ("canRelay").toBool ());
    QCOMPARE (workflow.value ("targetCall").toString (),
              QStringLiteral ("N0CALL"));
    QVERIFY (requester.observeStation (
        QStringLiteral ("IU8LMC"),
        QStringLiteral ("JN70"),
        QStringLiteral ("Salvo"),
        true,
        true,
        true,
        true,
        true,
        2,
        0,
        74500));
    workflow = requester.relayWorkflowForStation (
        QStringLiteral ("IU8LMC"), 75000);
    QVERIFY (workflow.value ("canRelay").toBool ());
    QVERIFY (workflow.value ("readyToForward").toBool ());
    QCOMPARE (workflow.value ("targetCall").toString (),
              QStringLiteral ("N0CALL"));
    QCOMPARE (workflow.value ("relayCall").toString (),
              QStringLiteral ("IU8LMC"));
    QCOMPARE (workflow.value ("parkedMailboxCount").toInt (), 1);
    QCOMPARE (workflow.value ("priority").toString (),
              QStringLiteral ("NORMAL"));
    QVERIFY (workflow.value ("line").toString ().contains (
        QStringLiteral ("Relay IU8LMC -> N0CALL")));
    QVariantList const relayStations =
        requester.activeStations (75000, 300000, false);
    bool sawRelayBadge = false;
    for (QVariant const& value : relayStations)
      {
        QVariantMap const station = value.toMap ();
        if (station.value ("call").toString () == QStringLiteral ("IU8LMC"))
          {
            sawRelayBadge =
                station.value ("pathRelayTarget").toString ()
                    == QStringLiteral ("N0CALL")
                && station.value ("pathRelayMailboxCount").toInt () == 1
                && station.value ("relayWorkflowReady").toBool ()
                && station.value ("relayWorkflowBadge").toString ()
                    == QStringLiteral ("RLY>N0CALL")
                && station.value ("relayWorkflowLine").toString ().contains (
                    QStringLiteral ("mail 1"));
          }
      }
    QVERIFY (sawRelayBadge);
  }

  void pingRadioRoundTrip ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter sender;
    FT2LinkQmlAdapter receiver;
    sender.setLocalStation ("IU8LMC", "JN70", "Salvo");
    receiver.setLocalStation ("K1ABC", "FN42", "Ann");

    QSignalSpy senderRadioSpy {
        &sender, &FT2LinkQmlAdapter::radioTxAudioRequested};
    QSignalSpy receiverRadioSpy {
        &receiver, &FT2LinkQmlAdapter::radioTxAudioRequested};
    QSignalSpy senderPingSpy {&sender, &FT2LinkQmlAdapter::pingLogChanged};
    QSignalSpy receiverPingSpy {&receiver, &FT2LinkQmlAdapter::pingLogChanged};

    sender.setRadioTxArmed (true);
    QVERIFY (sender.transmitPingRadio (QStringLiteral ("K1ABC"), 3000));
    QCOMPARE (senderRadioSpy.size (), 1);
    QCOMPARE (sender.pingCount (), 1);
    QVERIFY (!sender.radioTxArmed ());
    QVERIFY (senderPingSpy.size () >= 1);

    QList<QVariant> const pingRequest = senderRadioSpy.takeFirst ();
    QCOMPARE (pingRequest[0].toString (), QStringLiteral ("FT2-Link PING"));
    QVariantMap const pingPlan = pingRequest[2].toMap ();
    QCOMPARE (pingPlan.value ("kind").toString (), QStringLiteral ("PING"));
    QCOMPARE (pingPlan.value ("frameTypeName").toString (), QStringLiteral ("PING"));
    QCOMPARE (pingPlan.value ("profileName").toString (), QStringLiteral ("NARROW"));
    QCOMPARE (pingPlan.value ("remoteCall").toString (), QStringLiteral ("K1ABC"));
    quint16 const token = static_cast<quint16> (
        pingPlan.value ("sequence").toUInt ());
    QVERIFY (token != 0u);

    QVariantMap pending = sender.pingLog ().first ().toMap ();
    QCOMPARE (pending.value ("remoteCall").toString (), QStringLiteral ("K1ABC"));
    QCOMPARE (pending.value ("state").toString (), QStringLiteral ("Pending"));
    QCOMPARE (pending.value ("token").toUInt (), static_cast<uint> (token));

    decodium::ft2link::Frame incomingPing;
    incomingPing.type = decodium::ft2link::FrameType::Ping;
    incomingPing.profile = decodium::ft2link::Profile::Narrow;
    incomingPing.flags = decodium::ft2link::FlagEndOfMessage;
    incomingPing.sequence = token;
    QByteArray const senderCall = QByteArrayLiteral ("IU8LMC");
    incomingPing.payload.assign (
        reinterpret_cast<std::uint8_t const*> (senderCall.constData ()),
        reinterpret_cast<std::uint8_t const*> (senderCall.constData ())
            + senderCall.size ());

    QVERIFY (receiver.ingestRadioFrameBytes (
        frameBytes (incomingPing), "", 3100, true));
    QCOMPARE (receiver.pingCount (), 1);
    QCOMPARE (receiver.transportState (), QStringLiteral ("PING RX"));
    QVERIFY (receiverPingSpy.size () >= 1);
    QCOMPARE (receiverRadioSpy.size (), 1);

    QVariantMap received = receiver.pingLog ().first ().toMap ();
    QCOMPARE (received.value ("direction").toString (), QStringLiteral ("Incoming"));
    QCOMPARE (received.value ("remoteCall").toString (), QStringLiteral ("IU8LMC"));
    QCOMPARE (received.value ("state").toString (), QStringLiteral ("Received"));
    QCOMPARE (received.value ("token").toUInt (), static_cast<uint> (token));

    QList<QVariant> const ackRequest = receiverRadioSpy.takeFirst ();
    QCOMPARE (ackRequest[0].toString (), QStringLiteral ("FT2-Link PING_ACK"));
    QVariantMap const ackPlan = ackRequest[2].toMap ();
    QCOMPARE (ackPlan.value ("kind").toString (), QStringLiteral ("PING_ACK"));
    QCOMPARE (ackPlan.value ("frameTypeName").toString (), QStringLiteral ("PING_ACK"));
    QCOMPARE (ackPlan.value ("profileName").toString (), QStringLiteral ("NARROW"));
    QCOMPARE (ackPlan.value ("sequence").toUInt (), static_cast<uint> (token));
    QVERIFY (ackPlan.value ("autoAck").toBool ());

    decodium::ft2link::Frame pingAck;
    pingAck.type = decodium::ft2link::FrameType::PingAck;
    pingAck.profile = decodium::ft2link::Profile::Narrow;
    pingAck.flags = decodium::ft2link::FlagEndOfMessage;
    pingAck.sequence = token;
    QByteArray const receiverCall = QByteArrayLiteral ("K1ABC");
    pingAck.payload.assign (
        reinterpret_cast<std::uint8_t const*> (receiverCall.constData ()),
        reinterpret_cast<std::uint8_t const*> (receiverCall.constData ())
            + receiverCall.size ());

    QVERIFY (sender.ingestRadioFrameBytes (
        frameBytes (pingAck), QStringLiteral ("K1ABC"), 3250, false));
    QCOMPARE (sender.pingCount (), 2);
    QCOMPARE (sender.transportState (), QStringLiteral ("PING OK"));

    QVariantMap reply = sender.pingLog ().first ().toMap ();
    QCOMPARE (reply.value ("direction").toString (), QStringLiteral ("Incoming"));
    QCOMPARE (reply.value ("remoteCall").toString (), QStringLiteral ("K1ABC"));
    QCOMPARE (reply.value ("state").toString (), QStringLiteral ("Reply"));
    QCOMPARE (reply.value ("token").toUInt (), static_cast<uint> (token));
    QCOMPARE (reply.value ("rttMs").toULongLong (), 250ull);

    sender.clearPingLog ();
    QCOMPARE (sender.pingCount (), 0);
    QVERIFY (sender.pingLog ().isEmpty ());

    FT2LinkQmlAdapter muted;
    muted.setLocalStation ("N0PING", "FN31", "Muted");
    QSignalSpy mutedRadioSpy {
        &muted, &FT2LinkQmlAdapter::radioTxAudioRequested};
    QVERIFY (muted.configureIncomingPings (false)
                 .value (QStringLiteral ("ok")).toBool ());
    QVERIFY (!muted.incomingPingsEnabled ());
    QVERIFY (muted.ingestRadioFrameBytes (
        frameBytes (incomingPing), "", 3300, true));
    QCOMPARE (muted.pingCount (), 1);
    QCOMPARE (muted.transportState (), QStringLiteral ("PING REJECTED"));
    QCOMPARE (mutedRadioSpy.size (), 0);
    QVariantMap rejected = muted.pingLog ().first ().toMap ();
    QCOMPARE (rejected.value ("direction").toString (),
              QStringLiteral ("Incoming"));
    QCOMPARE (rejected.value ("state").toString (),
              QStringLiteral ("Rejected"));
  }

  void mailboxEnvelopeUsesReliableDataSession ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);

    QSignalSpy radioSpy {&caller, &FT2LinkQmlAdapter::radioTxAudioRequested};
    QSignalSpy mailboxSpy {&caller, &FT2LinkQmlAdapter::mailboxChanged};

    caller.setRadioTxArmed (true);
    QVERIFY (caller.transmitMailboxRadio (
        sessionId,
        QStringLiteral ("K1ABC"),
        QStringLiteral ("SITREP"),
        QStringLiteral ("All stations normal"),
        2000));
    QCOMPARE (radioSpy.size (), 1);
    QCOMPARE (caller.mailboxCount (), 1);
    QVERIFY (mailboxSpy.size () >= 1);
    QVERIFY (!caller.radioTxArmed ());
    QCOMPARE (caller.transportState (), QStringLiteral ("MAIL TX"));

    QVariantList mailbox = caller.mailbox ();
    QVariantMap outgoing = mailbox.first ().toMap ();
    QCOMPARE (outgoing.value ("direction").toString (),
              QStringLiteral ("Outgoing"));
    QCOMPARE (outgoing.value ("toCall").toString (), QStringLiteral ("K1ABC"));
    QCOMPARE (outgoing.value ("subject").toString (), QStringLiteral ("SITREP"));
    QCOMPARE (outgoing.value ("state").toString (), QStringLiteral ("Pending"));

    QList<QVariant> const request = radioSpy.takeFirst ();
    QCOMPARE (request[0].toString (), QStringLiteral ("FT2-Link MAIL K1ABC"));
    QVariantMap plan = request[2].toMap ();
    QVERIFY (plan.value ("mailbox").toBool ());
    QCOMPARE (plan.value ("text").toString (),
              QStringLiteral ("MAIL to K1ABC: SITREP"));

    decodium::ft2link::Frame ack =
        decodium::ft2link::makeAckFrame (
            decodium::ft2link::Profile::Wide2300,
            sessionId,
            0u,
            0x0001u);
    QVERIFY (caller.ingestRadioFrameBytes (
        frameBytes (ack), "K1ABC", 2100, false));
    mailbox = caller.mailbox ();
    QCOMPARE (mailbox.first ().toMap ().value ("state").toString (),
              QStringLiteral ("Delivered"));

    QString const inboundEnvelope = QStringLiteral ("FT2M1|IU8LMC|K1ABC|%1|%2")
        .arg (QString::fromLatin1 (QByteArrayLiteral ("Reply").toHex ()),
              QString::fromLatin1 (QByteArrayLiteral ("Received and copied").toHex ()));
    decodium::ft2link::Frame data;
    data.type = decodium::ft2link::FrameType::Data;
    data.profile = decodium::ft2link::Profile::Wide2300;
    data.sessionId = sessionId;
    data.sequence = 0u;
    data.flags = decodium::ft2link::FlagEndOfMessage;
    QByteArray const payload = inboundEnvelope.toUtf8 ();
    data.payload.assign (payload.begin (), payload.end ());

    QVERIFY (caller.ingestRadioFrameBytes (
        frameBytes (data), "K1ABC", 10000, false));
    QCOMPARE (caller.mailboxCount (), 2);
    QVariantMap incoming = caller.mailbox ().last ().toMap ();
    QCOMPARE (incoming.value ("direction").toString (),
              QStringLiteral ("Incoming"));
    QCOMPARE (incoming.value ("fromCall").toString (), QStringLiteral ("K1ABC"));
    QCOMPARE (incoming.value ("subject").toString (), QStringLiteral ("Reply"));
    QCOMPARE (incoming.value ("body").toString (),
              QStringLiteral ("Received and copied"));
    QCOMPARE (incoming.value ("state").toString (), QStringLiteral ("Received"));

    QVariantList messages = caller.messages (sessionId);
    QCOMPARE (messages.last ().toMap ().value ("text").toString (),
              QStringLiteral ("MAIL from K1ABC: Reply"));

    quint32 const incomingMailId = incoming.value ("id").toUInt ();
    QVERIFY (caller.markMailboxRead (incomingMailId, true, 10100));
    QCOMPARE (caller.mailbox ().last ().toMap ().value ("state").toString (),
              QStringLiteral ("Read"));
    QVERIFY (caller.markMailboxRead (incomingMailId, false, 10200));
    QCOMPARE (caller.mailbox ().last ().toMap ().value ("state").toString (),
              QStringLiteral ("Received"));
    QVERIFY (caller.deleteMailboxMessage (incomingMailId));
    QCOMPARE (caller.mailboxCount (), 1);
  }

  void typedMailboxTracksUrgentEmcommUnreadAndExport ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath (
        QStringLiteral ("ft2link-mail-state.json"));

    FT2LinkQmlAdapter caller;
    caller.setLocalStorePath (storePath);
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);

    QSignalSpy radioSpy {&caller, &FT2LinkQmlAdapter::radioTxAudioRequested};
    caller.setRadioTxArmed (true);
    QVERIFY (caller.transmitMailboxRadioTyped (
        sessionId,
        QStringLiteral ("K1ABC"),
        QStringLiteral ("ICS"),
        QStringLiteral ("Shelter needs water"),
        true,
        true,
        2000));
    QCOMPARE (radioSpy.size (), 1);
    QVariantMap outgoing = caller.mailbox ().first ().toMap ();
    QVERIFY (outgoing.value ("urgent").toBool ());
    QVERIFY (outgoing.value ("emcomm").toBool ());
    QCOMPARE (outgoing.value ("priority").toString (),
              QStringLiteral ("URGENT+EMCOMM"));
    QCOMPARE (caller.mailboxUnreadCount (), 0);
    QVariantMap plan = radioSpy.takeFirst ()[2].toMap ();
    QVERIFY (plan.value ("mailboxUrgent").toBool ());
    QVERIFY (plan.value ("mailboxEmcomm").toBool ());
    QCOMPARE (plan.value ("text").toString (),
              QStringLiteral ("MAIL to K1ABC: URGENT EMCOMM ICS"));

    QString const inboundEnvelope = QStringLiteral ("FT2M2|IU8LMC|K1ABC|UE|%1|%2")
        .arg (hexUtf8 (QStringLiteral ("Reply")),
              hexUtf8 (QStringLiteral ("Copy urgent EmComm")));
    QVERIFY (caller.appendIncomingText (sessionId, inboundEnvelope, 3000));
    QCOMPARE (caller.mailboxCount (), 2);
    QCOMPARE (caller.mailboxUnreadCount (), 1);
    QVariantMap incoming = caller.mailbox ().last ().toMap ();
    QVERIFY (incoming.value ("urgent").toBool ());
    QVERIFY (incoming.value ("emcomm").toBool ());
    QVERIFY (incoming.value ("unread").toBool ());
    QCOMPARE (incoming.value ("priority").toString (),
              QStringLiteral ("URGENT+EMCOMM"));
    QCOMPARE (caller.messages (sessionId).last ().toMap ().value (
                  "text").toString (),
              QStringLiteral ("MAIL from K1ABC: URGENT EMCOMM Reply"));
    QString const mailboxExport = caller.mailboxText ();
    QVERIFY (mailboxExport.contains (QStringLiteral ("UNREAD")));
    QVERIFY (mailboxExport.contains (QStringLiteral ("URGENT+EMCOMM")));

    quint32 const incomingMailId = incoming.value ("id").toUInt ();
    QVERIFY (caller.markMailboxRead (incomingMailId, true, 3100));
    QCOMPARE (caller.mailboxUnreadCount (), 0);

    QVERIFY (caller.saveLocalStore ());
    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QCOMPARE (restored.mailboxCount (), 2);
    QVariantMap restoredIncoming = restored.mailbox ().last ().toMap ();
    QVERIFY (restoredIncoming.value ("urgent").toBool ());
    QVERIFY (restoredIncoming.value ("emcomm").toBool ());
    QCOMPARE (restoredIncoming.value ("state").toString (),
              QStringLiteral ("Read"));
  }

  void mailboxEmailGatewayBuildsMailtoAndEml ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath (
        QStringLiteral ("ft2link-email-gateway-state.json"));

    FT2LinkQmlAdapter adapter;
    adapter.setLocalStorePath (storePath);
    adapter.setLocalStation ("IU8LMC", "JN70", "Salvo");
    adapter.setLocalOperatorProfile (
        QStringLiteral ("Naples"),
        QStringLiteral ("salvo@example.net"),
        QString {},
        QString {},
        QString {},
        QString {},
        QString {});

    QVERIFY (adapter.parkMailboxTyped (
        QStringLiteral ("K1ABC"),
        QStringLiteral ("ICS update"),
        QStringLiteral ("Please forward to k1abc@example.com. Shelter ready."),
        true,
        true,
        1000));
    QCOMPARE (adapter.mailboxCount (), 1);
    QVariantMap mail = adapter.mailbox ().first ().toMap ();
    quint32 const mailId = mail.value (QStringLiteral ("id")).toUInt ();
    QVariantMap draft = adapter.mailboxEmailGateway (mailId, QString {});
    QVERIFY (draft.value (QStringLiteral ("ok")).toBool ());
    QVERIFY (draft.value (QStringLiteral ("mailtoReady")).toBool ());
    QCOMPARE (draft.value (QStringLiteral ("toEmail")).toString (),
              QStringLiteral ("k1abc@example.com"));
    QVERIFY (draft.value (QStringLiteral ("subject")).toString ().contains (
        QStringLiteral ("URGENT+EMCOMM")));
    QVERIFY (draft.value (QStringLiteral ("mailtoUrl")).toString ().startsWith (
        QStringLiteral ("mailto:")));
    QString const eml = draft.value (QStringLiteral ("eml")).toString ();
    QVERIFY (eml.contains (QStringLiteral ("X-Decodium-FT2Link-Mailbox-Id")));
    QVERIFY (eml.contains (QStringLiteral ("X-Decodium-FT2Link-Priority: URGENT+EMCOMM")));
    QVERIFY (eml.contains (QStringLiteral ("Content-Type: text/plain; charset=UTF-8")));
    QVERIFY (eml.contains (QStringLiteral ("Please forward to k1abc@example.com")));
    QVERIFY (draft.value (QStringLiteral ("emlFileName")).toString ().endsWith (
        QStringLiteral (".eml")));
    QCOMPARE (adapter.mailboxEmailGatewayText (mailId, QString {}), eml);
    QVERIFY (adapter.markMailboxEmailGateway (
        mailId,
        QStringLiteral ("Sent"),
        QStringLiteral ("SMTP accepted VMail email"),
        3000));
    mail = adapter.mailbox ().first ().toMap ();
    QCOMPARE (mail.value (QStringLiteral ("emailGatewayState")).toString (),
              QStringLiteral ("Sent"));
    QCOMPARE (mail.value (QStringLiteral ("emailGatewayDetail")).toString (),
              QStringLiteral ("SMTP accepted VMail email"));
    QVERIFY (adapter.mailboxText ().contains (
        QStringLiteral ("Email gateway: Sent")));
    QVERIFY (adapter.saveLocalStore ());

    FT2LinkQmlAdapter restoredGateway;
    restoredGateway.setLocalStorePath (storePath);
    QVERIFY (restoredGateway.loadLocalStore ());
    QVariantMap restoredMail = restoredGateway.mailbox ().first ().toMap ();
    QCOMPARE (restoredMail.value (
                  QStringLiteral ("emailGatewayState")).toString (),
              QStringLiteral ("Sent"));

    QVERIFY (adapter.parkMailboxTyped (
        QStringLiteral ("N0CALL"),
        QStringLiteral ("No address"),
        QStringLiteral ("No email in this VMail"),
        false,
        false,
        2000));
    QVariantMap second = adapter.mailbox ().last ().toMap ();
    QVariantMap fallbackDraft = adapter.mailboxEmailGateway (
        second.value (QStringLiteral ("id")).toUInt (),
        QStringLiteral ("fallback@example.org"));
    QVERIFY (fallbackDraft.value (QStringLiteral ("ok")).toBool ());
    QCOMPARE (fallbackDraft.value (QStringLiteral ("toEmail")).toString (),
              QStringLiteral ("fallback@example.org"));
  }

  void parkedMailboxPromotesWhenDestinationIsHeard ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath (
        QStringLiteral ("ft2link-relay-state.json"));

    FT2LinkQmlAdapter adapter;
    adapter.setLocalStorePath (storePath);
    adapter.setLocalStation ("IU8LMC", "JN70", "Salvo");
    QSignalSpy mailboxSpy {&adapter, &FT2LinkQmlAdapter::mailboxChanged};
    QSignalSpy alertSpy {&adapter, &FT2LinkQmlAdapter::alertsChanged};

    QVERIFY (adapter.parkMailbox (
        QStringLiteral ("K1ABC"),
        QStringLiteral ("Relay"),
        QStringLiteral ("Park this for relay"),
        1000));
    QCOMPARE (adapter.mailboxCount (), 1);
    QCOMPARE (adapter.transportState (), QStringLiteral ("MAIL PARK"));

    QVariantMap parked = adapter.mailbox ().first ().toMap ();
    QCOMPARE (parked.value ("direction").toString (),
              QStringLiteral ("Parked"));
    QCOMPARE (parked.value ("state").toString (), QStringLiteral ("Parked"));
    QCOMPARE (parked.value ("toCall").toString (), QStringLiteral ("K1ABC"));
    QCOMPARE (parked.value ("relayReady").toBool (), false);
    QCOMPARE (parked.value ("relayNotifiedAtMs").toULongLong (), 0ull);

    QVERIFY (adapter.observeStation (
        QStringLiteral ("K1ABC"),
        QStringLiteral ("FN42"),
        QStringLiteral ("Ann"),
        true,
        true,
        true,
        true,
        true,
        2,
        0,
        2000));
    QCOMPARE (adapter.mailboxCount (), 1);
    QVariantMap ready = adapter.mailbox ().first ().toMap ();
    QCOMPARE (ready.value ("state").toString (),
              QStringLiteral ("Relay ready"));
    QCOMPARE (ready.value ("relayReady").toBool (), true);
    QCOMPARE (ready.value ("relayNotifiedAtMs").toULongLong (), 2000ull);
    QVariantList const stations = adapter.activeStations (2000, 300000, false);
    QVERIFY (!stations.isEmpty ());
    QVariantMap const station = stations.first ().toMap ();
    QCOMPARE (station.value ("parkedMailboxCount").toInt (), 1);
    QCOMPARE (station.value ("relayReadyMailboxCount").toInt (), 1);
    QVariantMap const stationMail =
        station.value ("mailboxSummary").toMap ();
    QCOMPARE (stationMail.value ("subject").toString (),
              QStringLiteral ("Relay"));
    QCOMPARE (adapter.alertCount (), 1);
    QVariantMap alert = adapter.alertEvents ().first ().toMap ();
    QCOMPARE (alert.value ("fromCall").toString (), QStringLiteral ("K1ABC"));
    QCOMPARE (alert.value ("source").toString (), QStringLiteral ("Relay"));
    QCOMPARE (alert.value ("tag").toString (), QStringLiteral ("MAIL"));
    QVERIFY (alert.value ("text").toString ().contains (
        QStringLiteral ("Parked mail ready")));

    QVERIFY (adapter.observeStation (
        QStringLiteral ("K1ABC"),
        QStringLiteral ("FN42"),
        QStringLiteral ("Ann"),
        true,
        true,
        true,
        true,
        true,
        2,
        0,
        3000));
    QCOMPARE (adapter.alertCount (), 1);
    QVERIFY (mailboxSpy.size () >= 2);
    QCOMPARE (alertSpy.size (), 1);

    QVERIFY (adapter.saveLocalStore ());
    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QCOMPARE (restored.mailboxCount (), 1);
    QCOMPARE (restored.alertCount (), 1);
    QVariantMap restoredMail = restored.mailbox ().first ().toMap ();
    QCOMPARE (restoredMail.value ("state").toString (),
              QStringLiteral ("Relay ready"));
    QCOMPARE (restoredMail.value ("relayNotifiedAtMs").toULongLong (), 2000ull);
  }

  void relayMailboxForwardsParkedThirdPartyMail ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter relay;
    relay.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sourceSessionId =
        connectWideSession (relay, "K1ABC", "FN42", 1000);
    QVERIFY (sourceSessionId != 0u);

    QString const envelope = QStringLiteral ("FT2M1|N0CALL|K1ABC|%1|%2")
        .arg (hexUtf8 (QStringLiteral ("Relay needed")),
              hexUtf8 (QStringLiteral ("Message for N0CALL")));
    QVERIFY (relay.appendIncomingText (sourceSessionId, envelope, 1500));
    QCOMPARE (relay.mailboxCount (), 1);
    QVariantMap parked = relay.mailbox ().first ().toMap ();
    QCOMPARE (parked.value ("direction").toString (), QStringLiteral ("Relay"));
    QCOMPARE (parked.value ("state").toString (), QStringLiteral ("Parked"));
    QCOMPARE (parked.value ("fromCall").toString (), QStringLiteral ("K1ABC"));
    QCOMPARE (parked.value ("toCall").toString (), QStringLiteral ("N0CALL"));
    QCOMPARE (relay.messages (sourceSessionId).last ().toMap ().value (
                  "text").toString (),
              QStringLiteral ("RELAY MAIL for N0CALL from K1ABC: Relay needed"));

    QVERIFY (relay.observeStation (
        QStringLiteral ("N0CALL"),
        QStringLiteral ("EM12"),
        QStringLiteral ("Lee"),
        true,
        true,
        true,
        true,
        true,
        2,
        0,
        2000));
    QVariantMap ready = relay.mailbox ().first ().toMap ();
    QCOMPARE (ready.value ("state").toString (),
              QStringLiteral ("Relay ready"));
    QCOMPARE (relay.alertCount (), 1);

    quint16 const destinationSessionId =
        connectWideSession (relay, "N0CALL", "EM12", 3000);
    QVERIFY (destinationSessionId != 0u);
    QVariantMap candidate = relay.relayMailboxForSession (
        destinationSessionId);
    QCOMPARE (candidate.value ("id").toUInt (),
              ready.value ("id").toUInt ());

    QSignalSpy radioSpy {&relay, &FT2LinkQmlAdapter::radioTxAudioRequested};
    relay.setRadioTxArmed (true);
    QVERIFY (relay.transmitRelayMailboxRadio (
        destinationSessionId,
        candidate.value ("id").toUInt (),
        4000));
    QCOMPARE (relay.transportState (), QStringLiteral ("MAIL RELAY"));
    QCOMPARE (relay.mailbox ().first ().toMap ().value (
                  "state").toString (),
              QStringLiteral ("Pending relay"));
    QCOMPARE (radioSpy.size (), 1);
    QVariantMap plan = radioSpy.takeFirst ()[2].toMap ();
    QVERIFY (plan.value ("mailbox").toBool ());
    QVERIFY (plan.value ("relay").toBool ());
    QCOMPARE (plan.value ("mailboxTo").toString (),
              QStringLiteral ("N0CALL"));
    QCOMPARE (plan.value ("relayProtocol").toString (),
              QStringLiteral ("FT2RLY1"));
    QCOMPARE (plan.value ("relayViaCall").toString (),
              QStringLiteral ("N0CALL"));
    QCOMPARE (plan.value ("relayHopCount").toInt (), 1);
    QVariantMap pending = relay.mailbox ().first ().toMap ();
    QCOMPARE (pending.value ("relayProtocol").toString (),
              QStringLiteral ("FT2RLY1"));
    QCOMPARE (pending.value ("relayViaCall").toString (),
              QStringLiteral ("N0CALL"));
    QCOMPARE (pending.value ("relayHopCount").toInt (), 1);
    QCOMPARE (relay.relayQueueCount (), 1);

    decodium::ft2link::Frame ack =
        decodium::ft2link::makeAckFrame (
            decodium::ft2link::Profile::Wide2300,
            destinationSessionId,
            0u,
            0x0001u);
    QVERIFY (relay.ingestRadioFrameBytes (
        frameBytes (ack), "N0CALL", 4100, false));
    QCOMPARE (relay.mailbox ().first ().toMap ().value (
                  "state").toString (),
              QStringLiteral ("Delivered"));
    QCOMPARE (relay.relayQueueCount (), 0);
  }

  void structuredRelayEnvelopeIsStoredQueuedAndPersisted ()
  {
    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath (
        QStringLiteral ("ft2link-structured-relay-state.json"));

    FT2LinkQmlAdapter relay;
    relay.setLocalStorePath (storePath);
    relay.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (relay, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);

    QString const envelope = QStringLiteral ("FT2RLY1|N0CALL|IU8LMC|K1ABC|UE|2|%1|%2")
        .arg (hexUtf8 (QStringLiteral ("Relay needed")),
              hexUtf8 (QStringLiteral ("Structured relay body")));
    QVERIFY (relay.appendIncomingText (sessionId, envelope, 1500));
    QCOMPARE (relay.mailboxCount (), 1);
    QCOMPARE (relay.relayQueueCount (), 1);

    QVariantMap queued = relay.mailbox ().first ().toMap ();
    QCOMPARE (queued.value ("direction").toString (), QStringLiteral ("Relay"));
    QCOMPARE (queued.value ("state").toString (), QStringLiteral ("Parked"));
    QCOMPARE (queued.value ("fromCall").toString (), QStringLiteral ("K1ABC"));
    QCOMPARE (queued.value ("toCall").toString (), QStringLiteral ("N0CALL"));
    QCOMPARE (queued.value ("relayProtocol").toString (),
              QStringLiteral ("FT2RLY1"));
    QCOMPARE (queued.value ("relayViaCall").toString (),
              QStringLiteral ("IU8LMC"));
    QCOMPARE (queued.value ("relayHopCount").toInt (), 2);
    QVERIFY (queued.value ("relayEnvelope").toBool ());
    QVERIFY (queued.value ("urgent").toBool ());
    QVERIFY (queued.value ("emcomm").toBool ());
    QVERIFY (relay.messages (sessionId).last ().toMap ().value (
                 "text").toString ().contains (
                 QStringLiteral ("RELAY MAIL for N0CALL via IU8LMC")));

    QVariantList const queue = relay.relayQueue (2000);
    QCOMPARE (queue.size (), 1);
    QVariantMap const queueItem = queue.first ().toMap ();
    QCOMPARE (queueItem.value ("id").toUInt (),
              queued.value ("id").toUInt ());
    QCOMPARE (queueItem.value ("relayProtocol").toString (),
              QStringLiteral ("FT2RLY1"));
    QVERIFY (relay.relayQueueText (2000).contains (
        QStringLiteral ("FT2RLY1")));

    QVERIFY (relay.saveLocalStore ());
    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QCOMPARE (restored.relayQueueCount (), 1);
    QVariantMap restoredMail = restored.mailbox ().first ().toMap ();
    QCOMPARE (restoredMail.value ("relayProtocol").toString (),
              QStringLiteral ("FT2RLY1"));
    QCOMPARE (restoredMail.value ("relayViaCall").toString (),
              QStringLiteral ("IU8LMC"));
    QCOMPARE (restoredMail.value ("relayHopCount").toInt (), 2);
  }

  void structuredRelayHopLimitPreventsFurtherForwarding ()
  {
    FT2LinkQmlAdapter relay;
    relay.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const relaySessionId =
        connectWideSession (relay, "K1ABC", "FN42", 1000);
    QVERIFY (relaySessionId != 0u);

    QString const maxHopRelayEnvelope =
        QStringLiteral ("FT2RLY1|N0CALL|IU8LMC|K1ABC|U|9|%1|%2")
            .arg (hexUtf8 (QStringLiteral ("Max hop")),
                  hexUtf8 (QStringLiteral ("Do not forward again")));
    QVERIFY (relay.appendIncomingText (relaySessionId,
                                       maxHopRelayEnvelope,
                                       1500));
    QCOMPARE (relay.mailboxCount (), 0);
    QVERIFY (relay.messages (relaySessionId).last ().toMap ().value (
                 "text").toString ().contains (
                 QStringLiteral ("RELAY MAIL hop limit")));

    FT2LinkQmlAdapter final;
    final.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const finalSessionId =
        connectWideSession (final, "K1ABC", "FN42", 2000);
    QVERIFY (finalSessionId != 0u);
    QString const maxHopFinalEnvelope =
        QStringLiteral ("FT2RLY1|IU8LMC|N0CALL|K1ABC|E|9|%1|%2")
            .arg (hexUtf8 (QStringLiteral ("Final hop")),
                  hexUtf8 (QStringLiteral ("Deliver at destination")));
    QVERIFY (final.appendIncomingText (finalSessionId,
                                       maxHopFinalEnvelope,
                                       2500));
    QCOMPARE (final.mailboxCount (), 1);
    QVariantMap delivered = final.mailbox ().first ().toMap ();
    QCOMPARE (delivered.value ("direction").toString (),
              QStringLiteral ("Incoming"));
    QCOMPARE (delivered.value ("state").toString (),
              QStringLiteral ("Received"));
    QCOMPARE (delivered.value ("relayHopCount").toInt (), 9);
    QVERIFY (delivered.value ("relayEnvelope").toBool ());
    QVERIFY (delivered.value ("emcomm").toBool ());
  }

  void formsAndSmallFilesUseReliableDataSession ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);

    QSignalSpy radioSpy {&caller, &FT2LinkQmlAdapter::radioTxAudioRequested};
    QSignalSpy formsSpy {&caller, &FT2LinkQmlAdapter::formsChanged};
    QSignalSpy filesSpy {&caller, &FT2LinkQmlAdapter::fileTransfersChanged};

    QVariantList templates = caller.formTemplates ();
    QVERIFY (templates.size () >= 3);
    QCOMPARE (templates.first ().toMap ().value ("id").toString (),
              QStringLiteral ("ICS213"));

    QVariantMap fields;
    fields.insert (QStringLiteral ("to"), QStringLiteral ("NET"));
    fields.insert (QStringLiteral ("message"), QStringLiteral ("Need water"));

    caller.setRadioTxArmed (true);
    QVERIFY (caller.transmitFormRadio (
        sessionId,
        QStringLiteral ("K1ABC"),
        QStringLiteral ("SITREP"),
        fields,
        2000));
    QCOMPARE (radioSpy.size (), 1);
    QCOMPARE (caller.formCount (), 1);
    QVERIFY (formsSpy.size () >= 1);
    QCOMPARE (caller.transportState (), QStringLiteral ("FORM TX"));
    QVariantMap form = caller.forms ().first ().toMap ();
    QCOMPARE (form.value ("direction").toString (), QStringLiteral ("Outgoing"));
    QCOMPARE (form.value ("formType").toString (), QStringLiteral ("SITREP"));
    QCOMPARE (form.value ("state").toString (), QStringLiteral ("Pending"));
    QVariantMap formPlan = radioSpy.takeFirst ()[2].toMap ();
    QVERIFY (formPlan.value ("form").toBool ());

    decodium::ft2link::Frame ack =
        decodium::ft2link::makeAckFrame (
            decodium::ft2link::Profile::Wide2300,
            sessionId,
            0u,
            0x0001u);
    QVERIFY (caller.ingestRadioFrameBytes (
        frameBytes (ack), "K1ABC", 2100, false));
    QCOMPARE (caller.forms ().first ().toMap ().value ("state").toString (),
              QStringLiteral ("Delivered"));

    QJsonObject incomingFormFields;
    incomingFormFields.insert (QStringLiteral ("message"),
                               QStringLiteral ("Shelter open"));
    incomingFormFields.insert (QStringLiteral ("location"),
                               QStringLiteral ("FN42"));
    QString const formEnvelope = QStringLiteral ("FT2FORM1|IU8LMC|K1ABC|CHECKIN|%1")
        .arg (QString::fromLatin1 (
            QJsonDocument (incomingFormFields).toJson (
                QJsonDocument::Compact).toHex ()));

    decodium::ft2link::Frame formData;
    formData.type = decodium::ft2link::FrameType::Data;
    formData.profile = decodium::ft2link::Profile::Wide2300;
    formData.sessionId = sessionId;
    formData.sequence = 0u;
    formData.flags = decodium::ft2link::FlagEndOfMessage;
    QByteArray const formPayload = formEnvelope.toUtf8 ();
    formData.payload.assign (formPayload.begin (), formPayload.end ());
    QVERIFY (caller.ingestRadioFrameBytes (
        frameBytes (formData), "K1ABC", 10000, false));
    QCOMPARE (caller.formCount (), 2);
    QVariantMap incomingForm = caller.forms ().last ().toMap ();
    QCOMPARE (incomingForm.value ("direction").toString (),
              QStringLiteral ("Incoming"));
    QCOMPARE (incomingForm.value ("formType").toString (),
              QStringLiteral ("CHECKIN"));
    QCOMPARE (incomingForm.value ("fields").toMap ().value ("message").toString (),
              QStringLiteral ("Shelter open"));
    QCOMPARE (caller.messages (sessionId).last ().toMap ().value ("text").toString (),
              QStringLiteral ("FORM CHECKIN from K1ABC"));

    caller.setRadioTxArmed (true);
    QVERIFY (caller.transmitFileRadio (
        sessionId,
        QStringLiteral ("K1ABC"),
        QStringLiteral ("status.txt"),
        QStringLiteral ("line one\nline two"),
        12000));
    QCOMPARE (radioSpy.size (), 1);
    QCOMPARE (caller.fileTransferCount (), 1);
    QVERIFY (filesSpy.size () >= 1);
    QVariantMap filePlan = radioSpy.takeFirst ()[2].toMap ();
    QVERIFY (filePlan.value ("file").toBool ());
    QCOMPARE (caller.fileTransfers ().first ().toMap ().value ("state").toString (),
              QStringLiteral ("Pending"));
    QVERIFY (caller.ingestRadioFrameBytes (
        frameBytes (ack), "K1ABC", 12100, false));
    QCOMPARE (caller.fileTransfers ().first ().toMap ().value ("state").toString (),
              QStringLiteral ("Delivered"));

    QByteArray const fileBytes = QByteArrayLiteral ("remote payload");
    QString const fileEnvelope = QStringLiteral ("FT2FILE1|IU8LMC|K1ABC|%1|%2|%3|%4")
        .arg (hexUtf8 (QStringLiteral ("remote.txt")),
              QString::number (fileBytes.size ()),
              sha256Hex (fileBytes),
              QString::fromLatin1 (fileBytes.toBase64 ()));
    decodium::ft2link::Frame fileData = formData;
    fileData.payload.clear ();
    QByteArray const filePayload = fileEnvelope.toUtf8 ();
    fileData.payload.assign (filePayload.begin (), filePayload.end ());
    QVERIFY (caller.ingestRadioFrameBytes (
        frameBytes (fileData), "K1ABC", 15050, false));
    QCOMPARE (caller.fileTransferCount (), 2);
    QVariantMap incomingFile = caller.fileTransfers ().last ().toMap ();
    QCOMPARE (incomingFile.value ("direction").toString (),
              QStringLiteral ("Incoming"));
    QCOMPARE (incomingFile.value ("fileName").toString (),
              QStringLiteral ("remote.txt"));
    QCOMPARE (incomingFile.value ("content").toString (),
              QStringLiteral ("remote payload"));
    QVariantList const receivedFiles = caller.receivedFiles ();
    QVERIFY (!receivedFiles.isEmpty ());
    QVariantMap const receivedFile = receivedFiles.first ().toMap ();
    QCOMPARE (receivedFile.value ("fileName").toString (),
              QStringLiteral ("remote.txt"));
    QCOMPARE (receivedFile.value ("senderCall").toString (),
              QStringLiteral ("K1ABC"));
    QVERIFY (receivedFile.value ("receivedAtMs").toULongLong () >= 15050ull);
    QCOMPARE (receivedFile.value ("preview").toString (),
              QStringLiteral ("remote payload"));
    QVariantMap const fileStats = caller.statistics ();
    QCOMPARE (fileStats.value ("filesReceived").toULongLong (), 1ull);
    QCOMPARE (fileStats.value ("filesSent").toULongLong (), 1ull);
    QCOMPARE (fileStats.value ("receivedFileBytes").toULongLong (),
              static_cast<qulonglong> (fileBytes.size ()));
    QVERIFY (caller.statisticsText ().contains (
        QStringLiteral ("Received files: 1")));
    QCOMPARE (caller.messages (sessionId).last ().toMap ().value ("text").toString (),
              QStringLiteral ("FILE from K1ABC: remote.txt"));

    QVERIFY (caller.appendIncomingText (
        sessionId, QStringLiteral ("<BLR>"), 15100));
    bool sawBbsListReply = false;
    for (QVariant const& value : caller.messages (sessionId))
      {
        QString const text = value.toMap ().value ("text").toString ();
        sawBbsListReply = sawBbsListReply
            || (text.contains (QStringLiteral ("requested BBS file list"))
                && text.contains (QStringLiteral ("<BL:"))
                && text.contains (QStringLiteral ("remote.txt")));
      }
    QVERIFY (sawBbsListReply);

    QVERIFY (caller.appendIncomingText (
        sessionId,
        QStringLiteral (
            "<BL:hello.txt|2025-01-01|5> <BG:status.txt> <BG:missing.txt> <BLJ> <BGJ>"),
        15200));
    bool sawRemoteListing = false;
    bool sawAvailableRequest = false;
    bool sawMissingRequest = false;
    bool sawListReject = false;
    bool sawDownloadReject = false;
    for (QVariant const& value : caller.messages (sessionId))
      {
        QString const text = value.toMap ().value ("text").toString ();
        sawRemoteListing = sawRemoteListing
            || text.contains (QStringLiteral ("BBS file hello.txt"));
        sawAvailableRequest = sawAvailableRequest
            || text.contains (QStringLiteral ("requested BBS file status.txt; available"));
        sawMissingRequest = sawMissingRequest
            || text.contains (QStringLiteral ("requested BBS file missing.txt; not found"));
        sawListReject = sawListReject
            || text.contains (QStringLiteral ("rejected BBS file list request"));
        sawDownloadReject = sawDownloadReject
            || text.contains (QStringLiteral ("rejected BBS file download request"));
      }
    QVERIFY (sawRemoteListing);
    QVERIFY (sawAvailableRequest);
    QVERIFY (sawMissingRequest);
    QVERIFY (sawListReject);
    QVERIFY (sawDownloadReject);

    QSignalSpy bulletinSpy {&caller, &FT2LinkQmlAdapter::bulletinsChanged};
    caller.setRadioTxArmed (true);
    QVERIFY (caller.transmitBulletinRadio (
        sessionId,
        QStringLiteral ("NET"),
        QStringLiteral ("Water"),
        QStringLiteral ("Water point open at 1400Z"),
        17000));
    QCOMPARE (radioSpy.size (), 1);
    QCOMPARE (caller.bulletinCount (), 1);
    QVERIFY (bulletinSpy.size () >= 1);
    QVariantMap bulletinPlan = radioSpy.takeFirst ()[2].toMap ();
    QVERIFY (bulletinPlan.value ("bulletin").toBool ());
    QCOMPARE (caller.bulletins ().first ().toMap ().value ("state").toString (),
              QStringLiteral ("Pending"));
    QVERIFY (caller.ingestRadioFrameBytes (
        frameBytes (ack), "K1ABC", 17100, false));
    QCOMPARE (caller.bulletins ().first ().toMap ().value ("state").toString (),
              QStringLiteral ("Delivered"));

    QString const bulletinEnvelope = QStringLiteral ("FT2BBS1|K1ABC|NET|%1|%2")
        .arg (hexUtf8 (QStringLiteral ("Shelter")),
              hexUtf8 (QStringLiteral ("Shelter opens now")));
    decodium::ft2link::Frame bulletinData = formData;
    bulletinData.payload.clear ();
    QByteArray const bulletinPayload = bulletinEnvelope.toUtf8 ();
    bulletinData.payload.assign (bulletinPayload.begin (), bulletinPayload.end ());
    QVERIFY (caller.ingestRadioFrameBytes (
        frameBytes (bulletinData), "K1ABC", 21050, false));
    QCOMPARE (caller.bulletinCount (), 2);
    QVariantMap incomingBulletin = caller.bulletins ().last ().toMap ();
    QCOMPARE (incomingBulletin.value ("direction").toString (),
              QStringLiteral ("Incoming"));
    QCOMPARE (incomingBulletin.value ("group").toString (),
              QStringLiteral ("NET"));
    QCOMPARE (incomingBulletin.value ("title").toString (),
              QStringLiteral ("Shelter"));
    QCOMPARE (caller.messages (sessionId).last ().toMap ().value ("text").toString (),
              QStringLiteral ("BBS NET from K1ABC: Shelter"));
  }

  void localStorePersistsOperationalLogs ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    QTemporaryDir tempDir;
    QVERIFY (tempDir.isValid ());
    QString const storePath = tempDir.filePath (
        QStringLiteral ("ft2link-state.json"));

    FT2LinkQmlAdapter source;
    source.setLocalStorePath (storePath);
    QCOMPARE (source.localStorePath (), storePath);
    source.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (source, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);
    source.setRadioTxArmed (true);
    QVERIFY (source.transmitBeaconRadio (true, 1020));
    source.setRadioTxArmed (true);
    QVERIFY (source.transmitBeaconRadio (false, 1030));
    QVERIFY (source.observeStation (
        QStringLiteral ("N0XYZ"),
        QStringLiteral ("EM12"),
        QStringLiteral (""),
        false,
        true,
        true,
        true,
        true,
        2,
        0,
        1040));
    QVERIFY (source.observeStation (
        QStringLiteral ("W1AW"),
        QStringLiteral ("FN31"),
        QStringLiteral (""),
        true,
        true,
        true,
        true,
        true,
        2,
        0,
        1045));
    QVERIFY (source.setContactTag (QStringLiteral ("K1ABC"),
                                   QStringLiteral ("HQ"),
                                   1050));
    QVERIFY (source.setContactDetails (QStringLiteral ("K1ABC"),
                                       QStringLiteral ("fn42"),
                                       QStringLiteral ("Ann"),
                                       QStringLiteral ("HQ"),
                                       QStringLiteral ("Net control"),
                                       1060));
    QVERIFY (source.queueOutgoingText (
        sessionId, QStringLiteral ("stored qso line"), 1800));
    QVERIFY (source.queueOutgoingText (
        sessionId, QStringLiteral ("<R-03>"), 1805));

    source.setRadioTxArmed (true);
    QVERIFY (source.transmitBroadcastRadio (
        QStringLiteral ("SOS CHECK NET"), 1900));

    decodium::ft2link::Frame ack =
        decodium::ft2link::makeAckFrame (
            decodium::ft2link::Profile::Wide2300,
            sessionId,
            0u,
            0x0001u);

    source.setRadioTxArmed (true);
    QVERIFY (source.transmitMailboxRadio (
        sessionId,
        QStringLiteral ("K1ABC"),
        QStringLiteral ("Persist"),
        QStringLiteral ("Mailbox body"),
        2000));
    QVERIFY (source.ingestRadioFrameBytes (
        frameBytes (ack), "K1ABC", 2100, false));

    QVariantMap fields;
    fields.insert (QStringLiteral ("message"),
                   QStringLiteral ("Persisted form"));
    source.setRadioTxArmed (true);
    QVERIFY (source.transmitFormRadio (
        sessionId,
        QStringLiteral ("K1ABC"),
        QStringLiteral ("SITREP"),
        fields,
        2200));
    QVERIFY (source.ingestRadioFrameBytes (
        frameBytes (ack), "K1ABC", 2300, false));

    source.setRadioTxArmed (true);
    QVERIFY (source.transmitFileRadio (
        sessionId,
        QStringLiteral ("K1ABC"),
        QStringLiteral ("persist.txt"),
        QStringLiteral ("persistent content"),
        2400));
    QVERIFY (source.ingestRadioFrameBytes (
        frameBytes (ack), "K1ABC", 2500, false));

    source.setRadioTxArmed (true);
    QVERIFY (source.transmitBulletinRadio (
        sessionId,
        QStringLiteral ("NET"),
        QStringLiteral ("Persist BBS"),
        QStringLiteral ("Bulletin body"),
        2600));
    QVERIFY (source.ingestRadioFrameBytes (
        frameBytes (ack), "K1ABC", 2700, false));

    source.setRadioTxArmed (true);
    QVERIFY (source.transmitPingRadio (QStringLiteral ("K1ABC"), 2800));
    QVariantMap const pendingPing = source.pingLog ().first ().toMap ();
    quint16 const pingToken = static_cast<quint16> (
        pendingPing.value (QStringLiteral ("token")).toUInt ());
    decodium::ft2link::Frame pingAck;
    pingAck.type = decodium::ft2link::FrameType::PingAck;
    pingAck.profile = decodium::ft2link::Profile::Narrow;
    pingAck.flags = decodium::ft2link::FlagEndOfMessage;
    pingAck.sequence = pingToken;
    QByteArray const responder = QByteArrayLiteral ("K1ABC");
    pingAck.payload.assign (
        reinterpret_cast<std::uint8_t const*> (responder.constData ()),
        reinterpret_cast<std::uint8_t const*> (responder.constData ())
            + responder.size ());
    QVERIFY (source.ingestRadioFrameBytes (
        frameBytes (pingAck), "K1ABC", 2875, false));

    QVERIFY (source.saveLocalStore ());
    QVERIFY (QFile::exists (storePath));
    QVERIFY (source.lastLocalStoreError ().isEmpty ());

    FT2LinkQmlAdapter restored;
    restored.setLocalStorePath (storePath);
    QVERIFY (restored.loadLocalStore ());
    QVERIFY (restored.localStoreLoaded ());
    QVERIFY (restored.lastLocalStoreError ().isEmpty ());
    QVERIFY (restored.beaconHistoryCount () >= 4);
    QVERIFY (restored.beaconHistoryText ().contains (
        QStringLiteral ("CQ | IU8LMC")));
    QVERIFY (restored.beaconHistoryText ().contains (
        QStringLiteral ("BEACON | N0XYZ")));
    QCOMPARE (restored.broadcastCount (), 1);
    QVERIFY (restored.alertCount () >= 1);
    QCOMPARE (restored.mailboxCount (), 1);
    QCOMPARE (restored.formCount (), 1);
    QCOMPARE (restored.fileTransferCount (), 1);
    QCOMPARE (restored.bulletinCount (), 1);
    QCOMPARE (restored.pingCount (), 2);
    QCOMPARE (restored.qsoLogCount (), 1);

    QCOMPARE (restored.mailbox ().first ().toMap ().value (
                  QStringLiteral ("state")).toString (),
              QStringLiteral ("Delivered"));
    QCOMPARE (restored.forms ().first ().toMap ().value (
                  QStringLiteral ("state")).toString (),
              QStringLiteral ("Delivered"));
    QCOMPARE (restored.fileTransfers ().first ().toMap ().value (
                  QStringLiteral ("fileName")).toString (),
              QStringLiteral ("persist.txt"));
    QCOMPARE (restored.bulletins ().first ().toMap ().value (
                  QStringLiteral ("title")).toString (),
              QStringLiteral ("Persist BBS"));
    QCOMPARE (restored.pingLog ().first ().toMap ().value (
                  QStringLiteral ("state")).toString (),
              QStringLiteral ("Reply"));
    QCOMPARE (restored.pingLog ().first ().toMap ().value (
                  QStringLiteral ("rttMs")).toULongLong (),
              75ull);
    QCOMPARE (restored.qsoLog ().first ().toMap ().value (
                  QStringLiteral ("remoteCall")).toString (),
              QStringLiteral ("K1ABC"));

    QVariantMap contact;
    for (QVariant const& value : restored.contactHistory ())
      {
        QVariantMap const candidate = value.toMap ();
        if (candidate.value (QStringLiteral ("call")).toString ()
            == QStringLiteral ("K1ABC"))
          {
            contact = candidate;
            break;
          }
      }
    QVERIFY (!contact.isEmpty ());
    QCOMPARE (contact.value (QStringLiteral ("tag")).toString (),
              QStringLiteral ("HQ"));
    QCOMPARE (contact.value (QStringLiteral ("comment")).toString (),
              QStringLiteral ("Net control"));
	    QVERIFY (contact.value (QStringLiteral ("mailCount")).toInt () >= 1);
	    QVERIFY (contact.value (QStringLiteral ("formCount")).toInt () >= 1);
	    QVERIFY (contact.value (QStringLiteral ("fileCount")).toInt () >= 1);

	    QVariantList const timeline =
	        restored.contactTimeline (QStringLiteral ("k1abc"));
	    QVERIFY (timeline.size () >= 5);
	    QStringList timelineTypes;
	    quint64 previousAt = std::numeric_limits<quint64>::max ();
	    for (QVariant const& value : timeline)
	      {
	        QVariantMap const entry = value.toMap ();
	        quint64 const atMs = entry.value (
	            QStringLiteral ("atMs")).toULongLong ();
	        QVERIFY (atMs <= previousAt);
	        previousAt = atMs;
	        QString const type = entry.value (
	            QStringLiteral ("type")).toString ();
	        if (!timelineTypes.contains (type))
	          {
	            timelineTypes.push_back (type);
	          }
	      }
	    QVERIFY (timelineTypes.contains (QStringLiteral ("QSO")));
	    QVERIFY (timelineTypes.contains (QStringLiteral ("MAIL")));
	    QVERIFY (timelineTypes.contains (QStringLiteral ("FORM")));
	    QVERIFY (timelineTypes.contains (QStringLiteral ("FILE")));
	    QVERIFY (timelineTypes.contains (QStringLiteral ("PING")));
	    QVERIFY (timelineTypes.contains (QStringLiteral ("PATH")));
	    QVERIFY (restored.contactTimeline (QStringLiteral ("ZZ0ZZZ")).isEmpty ());

	    QVariantMap const stats = restored.statistics ();
	    QCOMPARE (stats.value (QStringLiteral ("qsoTotal")).toULongLong (),
	              1ull);
	    QCOMPARE (stats.value (QStringLiteral ("beaconsSent")).toULongLong (),
	              1ull);
	    QCOMPARE (stats.value (QStringLiteral ("cqsSent")).toULongLong (),
	              1ull);
	    QVERIFY (stats.value (QStringLiteral ("beaconsReceived")).toULongLong ()
	             >= 1ull);
	    QVERIFY (stats.value (QStringLiteral ("cqsReceived")).toULongLong ()
	             >= 2ull);
	    QCOMPARE (stats.value (QStringLiteral ("fileTransfersTotal")).toULongLong (),
	              1ull);
	    QVERIFY (stats.value (QStringLiteral ("broadcastsSent")).toULongLong ()
	             >= 1ull);
	    QCOMPARE (stats.value (QStringLiteral ("pathReportsTotal")).toULongLong (),
	              1ull);
	    QCOMPARE (stats.value (QStringLiteral ("snrsSent")).toULongLong (),
	              1ull);
	    QCOMPARE (stats.value (QStringLiteral ("snrSentMin")).toInt (), -3);
	    QVariantMap const path = restored.pathAnalysis (
	        QStringLiteral ("K1ABC"),
	        QStringLiteral ("FN"));
	    QCOMPARE (path.value (QStringLiteral ("snrCount")).toInt (), 1);
	    QCOMPARE (path.value (QStringLiteral ("avgSnr")).toDouble (), -3.0);
	    QVERIFY (restored.statisticsText ().contains (
	        QStringLiteral ("FT2-Link statistics")));

	    QVariantMap const audit = restored.localStoreAudit ();
	    QVERIFY (audit.value (QStringLiteral ("ok")).toBool ());
	    QVERIFY (audit.value (QStringLiteral ("recordCount")).toULongLong ()
	             >= 1ull);
	    QVERIFY (audit.value (QStringLiteral ("serializedBytes")).toULongLong ()
	             > 0ull);
	    QVERIFY (audit.value (QStringLiteral ("serializedSha256")).toString ()
	             .size () == 64);
	    QVERIFY (restored.chatHistoryLog ().contains (
	        QStringLiteral ("K1ABC")));
	    QVERIFY (restored.operationalLog ().contains (
	        QStringLiteral ("[QSO]")));
	    QVERIFY (restored.localStoreJson ().contains (
	        QStringLiteral ("pathReports")));
	    QVERIFY (restored.logsBundleText ().contains (
	        QStringLiteral ("--- ADIF ---")));

	    QVariantMap const backup = restored.backupLocalStore (tempDir.path ());
	    QVERIFY (backup.value (QStringLiteral ("ok")).toBool ());
	    QVERIFY (QFile::exists (backup.value (
	        QStringLiteral ("path")).toString ()));
	    QVariantMap const fixed = restored.fixLocalStore (true);
	    QVERIFY (fixed.value (QStringLiteral ("ok")).toBool ());
	    QVERIFY (fixed.value (QStringLiteral ("audit")).toMap ().value (
	        QStringLiteral ("ok")).toBool ());

	    QVariantMap localContact;
    for (QVariant const& value : restored.contactHistory ())
      {
        QVariantMap const candidate = value.toMap ();
        if (candidate.value (QStringLiteral ("call")).toString ()
            == QStringLiteral ("IU8LMC"))
          {
            localContact = candidate;
            break;
          }
      }
    QVERIFY (!localContact.isEmpty ());
    QVERIFY (localContact.value (
                 QStringLiteral ("bulletinCount")).toInt () >= 1);

    restored.clearMailbox ();
    FT2LinkQmlAdapter cleared;
    cleared.setLocalStorePath (storePath);
    QVERIFY (cleared.loadLocalStore ());
    QCOMPARE (cleared.mailboxCount (), 0);
    QCOMPARE (cleared.formCount (), 1);
  }

  void radioTxQueueInterleavesNormalTrafficAcrossSessions ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const firstSession =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    quint16 const secondSession =
        connectWideSession (caller, "N0XYZ", "EM12", 1100);
    QVERIFY (firstSession != 0u);
    QVERIFY (secondSession != 0u);
    QVERIFY (firstSession != secondSession);

    QSignalSpy radioSpy {&caller, &FT2LinkQmlAdapter::radioTxAudioRequested};

    auto transmit = [&caller] (quint16 sessionId,
                               QString const& text,
                               quint64 nowMs) {
      QVERIFY (caller.prepareRadioTxAudio (sessionId, text, nowMs));
      caller.setRadioTxArmed (true);
      QVERIFY (caller.transmitPreparedRadioTxAudio (sessionId, text, nowMs));
    };

    transmit (firstSession, QStringLiteral ("A1"), 2000);
    QCOMPARE (radioSpy.size (), 1);
    QList<QVariant> const firstRequest = radioSpy.takeFirst ();
    QVariantMap firstPlan = firstRequest[2].toMap ();
    QCOMPARE (firstPlan.value ("sessionId").toUInt (),
              static_cast<uint> (firstSession));
    QVERIFY (!firstPlan.value ("queued").toBool ());

    transmit (firstSession, QStringLiteral ("A2"), 2001);
    transmit (secondSession, QStringLiteral ("B1"), 2002);
    QCOMPARE (radioSpy.size (), 0);

    QTRY_VERIFY_WITH_TIMEOUT (radioSpy.size () >= 1, 10000);
    QList<QVariant> const queuedRequest = radioSpy.takeFirst ();
    QVariantMap queuedPlan = queuedRequest[2].toMap ();
    QCOMPARE (queuedPlan.value ("sessionId").toUInt (),
              static_cast<uint> (secondSession));
    QVERIFY (queuedPlan.value ("queued").toBool ());
  }

  void w2300LiveRetryUsesRobustRate ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);

    QSignalSpy radioSpy {&caller, &FT2LinkQmlAdapter::radioTxAudioRequested};

    caller.setRadioTxArmed (true);
    QVERIFY (caller.transmitPreparedRadioTxAudio (
        sessionId, QStringLiteral ("needs retry"), 2000));
    QCOMPARE (radioSpy.size (), 1);
    QList<QVariant> const firstRequest = radioSpy.takeFirst ();
    QVariantMap firstPlan = firstRequest[2].toMap ();
    QCOMPARE (firstPlan.value ("w2300RateModeName").toString (),
              QStringLiteral ("FAST"));

    QTRY_VERIFY_WITH_TIMEOUT (radioSpy.size () >= 1, 10000);
    QList<QVariant> const retryRequest = radioSpy.takeFirst ();
    QVariantMap retryPlan = retryRequest[2].toMap ();
    QCOMPARE (retryPlan.value ("retryAttempt").toInt (), 2);
    QVERIFY (retryPlan.value ("retryRateAdapted").toBool ());
    QCOMPARE (retryPlan.value ("w2300RateModeName").toString (),
              QStringLiteral ("ROBUST"));
  }

  void liveW2300RxMetricsAdaptNextPreparedRate ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);

    QVERIFY (caller.prepareRadioTxAudio (
        sessionId, QStringLiteral ("before metrics"), 1900));
    QVariantMap plan = caller.lastRadioTxPlan ();
    QCOMPARE (plan.value ("w2300RateModeName").toString (),
              QStringLiteral ("FAST"));
    QCOMPARE (plan.value ("w2300RateSource").toString (),
              QStringLiteral ("negotiated"));

    QSignalSpy metricsSpy {&caller, &FT2LinkQmlAdapter::transportMetricsChanged};

    decodium::ft2link::Frame weakData;
    weakData.type = decodium::ft2link::FrameType::Data;
    weakData.profile = decodium::ft2link::Profile::Wide2300;
    weakData.sessionId = sessionId;
    weakData.sequence = 0u;
    weakData.flags = decodium::ft2link::FlagEndOfMessage;
    QByteArray const weakPayload = QByteArrayLiteral ("offset rx metrics");
    weakData.payload.assign (weakPayload.begin (), weakPayload.end ());

    decodium::ft2link::W2300WaveformConfig weakConfig;
    weakConfig.centerFrequencyHz = 1520.0;
    std::string error;
    std::vector<float> weakWave =
        decodium::ft2link::generateW2300FrameWaveform (
            weakData, weakConfig, &error);
    QVERIFY2 (!weakWave.empty (), error.c_str ());
    QVERIFY (caller.ingestRxSamples (pcmFromWave (weakWave), "K1ABC", 2500));
    QVERIFY (metricsSpy.size () >= 1);
    QVariantMap metrics = caller.lastTransportMetrics ();
    QVERIFY (metrics.value ("liveRx").toBool ());
    QCOMPARE (metrics.value ("nextW2300RateModeName").toString (),
              QStringLiteral ("ROBUST"));

    QVERIFY (caller.prepareRadioTxAudio (
        sessionId, QStringLiteral ("after weak rx"), 2600));
    plan = caller.lastRadioTxPlan ();
    QCOMPARE (plan.value ("w2300RateModeName").toString (),
              QStringLiteral ("ROBUST"));
    QCOMPARE (plan.value ("w2300RateSource").toString (),
              QStringLiteral ("live_rx"));
    QVERIFY (plan.value ("liveRateAdapted").toBool ());

    decodium::ft2link::Frame cleanAck =
        decodium::ft2link::makeAckFrame (
            decodium::ft2link::Profile::Wide2300,
            sessionId,
            0u,
            0x0001u);
    std::vector<float> cleanWave =
        decodium::ft2link::generateW2300FrameWaveform (
            cleanAck,
            decodium::ft2link::W2300WaveformConfig {},
            &error);
    QVERIFY2 (!cleanWave.empty (), error.c_str ());
    QVERIFY (caller.ingestRxSamples (pcmFromWave (cleanWave), "K1ABC", 2700));
    metrics = caller.lastTransportMetrics ();
    QCOMPARE (metrics.value ("nextW2300RateModeName").toString (),
              QStringLiteral ("FAST"));

    QVERIFY (caller.prepareRadioTxAudio (
        sessionId, QStringLiteral ("after clean rx"), 2800));
    plan = caller.lastRadioTxPlan ();
    QCOMPARE (plan.value ("w2300RateModeName").toString (),
              QStringLiteral ("FAST"));
    QCOMPARE (plan.value ("w2300RateSource").toString (),
              QStringLiteral ("live_rx"));
    QVERIFY (!plan.value ("liveRateAdapted").toBool ());
  }

  void radioTxWaitsForLiveChannelBusy ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter caller;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    quint16 const sessionId =
        connectWideSession (caller, "K1ABC", "FN42", 1000);
    QVERIFY (sessionId != 0u);

    QVector<short> busySamples;
    busySamples.reserve (1200);
    for (int i = 0; i < 1200; ++i)
      {
        busySamples.push_back (i % 2 == 0 ? 12000 : -12000);
      }
    QVERIFY (!caller.ingestRxSamples (busySamples, "", 5000));

    QSignalSpy radioSpy {&caller, &FT2LinkQmlAdapter::radioTxAudioRequested};
    caller.setRadioTxArmed (true);
    QVERIFY (caller.transmitPreparedRadioTxAudio (
        sessionId, QStringLiteral ("defer until clear"), 5001));
    QCOMPARE (caller.transportState (), QStringLiteral ("LBT wait"));
    QCOMPARE (radioSpy.size (), 0);

    QTRY_VERIFY_WITH_TIMEOUT (radioSpy.size () >= 1, 3000);
    QList<QVariant> const request = radioSpy.takeFirst ();
    QVariantMap plan = request[2].toMap ();
    QVERIFY (plan.value ("queued").toBool ());
    QVERIFY (plan.value ("lbtDeferred").toBool ());
  }

  void adapterRoundTripsNarrowBeaconAudio ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter sender;
    FT2LinkQmlAdapter receiver;
    sender.setLocalStation ("IU8LMC", "JN70", "Salvo");

    QSignalSpy radioSpy {&sender, &FT2LinkQmlAdapter::radioTxAudioRequested};
    sender.setRadioTxArmed (true);
    QVERIFY (sender.transmitBeaconRadio (true, 1000));
    QVERIFY (!sender.radioTxArmed ());
    QCOMPARE (sender.beaconCooldownSeconds (1000), 60);
    QCOMPARE (sender.beaconCooldownSeconds (60000), 1);
    QCOMPARE (sender.beaconCooldownSeconds (61000), 0);
    QCOMPARE (radioSpy.size (), 1);

    QList<QVariant> const request = radioSpy.takeFirst ();
    QCOMPARE (request[0].toString (), QStringLiteral ("FT2-Link CQ"));
    QVector<float> samples = request[1].value<QVector<float>> ();
    QVariantMap plan = request[2].toMap ();
    QVERIFY (!samples.isEmpty ());
    QCOMPARE (plan.value ("kind").toString (), QStringLiteral ("CQ"));
    QCOMPARE (plan.value ("frameTypeName").toString (), QStringLiteral ("BEACON"));
    QVERIFY (plan.value ("cq").toBool ());

    QVERIFY (receiver.ingestRxSamples (pcmFromSamples (samples, 4), "", 2200));
    QCOMPARE (receiver.stationCount (), 1);
    QVariantList stations = receiver.activeStations (2300, 1000, true);
    QCOMPARE (stations.size (), 1);
    QCOMPARE (stations[0].toMap ().value ("call").toString (),
              QStringLiteral ("IU8LMC"));
	    QCOMPARE (stations[0].toMap ().value ("locator").toString (),
	              QStringLiteral ("JN70"));
	    QVERIFY (stations[0].toMap ().value ("cq").toBool ());

	    sender.setRadioTxArmed (true);
	    QVERIFY (sender.transmitBeaconRadio (true, 30000));
	    QVERIFY (!sender.radioTxArmed ());
	    QCOMPARE (radioSpy.size (), 1);
	    QList<QVariant> const secondManualRequest = radioSpy.takeFirst ();
	    QCOMPARE (secondManualRequest[0].toString (),
	              QStringLiteral ("FT2-Link CQ"));
	    QCOMPARE (secondManualRequest[2].toMap ().value ("kind").toString (),
	              QStringLiteral ("CQ"));

	    QVERIFY (!sender.configureAutoBeacon (true, 30, true, 31000));
	    QVERIFY (sender.lastError ().contains ("ARM"));
	    QVERIFY (!sender.autoBeaconEnabled ());

	    sender.setRadioTxArmed (true);
	    QVERIFY (sender.configureAutoBeacon (true, 30, true, 32000));
	    QVERIFY (sender.autoBeaconEnabled ());
    QCOMPARE (sender.autoBeaconIntervalSeconds (), 60);
    QVERIFY (!sender.radioTxArmed ());
    QCOMPARE (sender.transportState (), QStringLiteral ("AUTO CQ WAIT"));
    QCOMPARE (radioSpy.size (), 0);

	    QVERIFY (sender.configureAutoBeacon (false, 60, true, 33000));
	    QVERIFY (!sender.autoBeaconEnabled ());

	    sender.setRadioTxArmed (true);
	    QVERIFY (sender.configureAutoBeacon (true, 30, true, 91000));
    QVERIFY (sender.autoBeaconEnabled ());
    QCOMPARE (sender.autoBeaconIntervalSeconds (), 60);
    QVERIFY (sender.autoBeaconCq ());
    QVERIFY (!sender.radioTxArmed ());
    QCOMPARE (radioSpy.size (), 1);
    QList<QVariant> const autoRequest = radioSpy.takeFirst ();
    QCOMPARE (autoRequest[0].toString (), QStringLiteral ("FT2-Link AUTO CQ"));
    QVariantMap autoPlan = autoRequest[2].toMap ();
    QCOMPARE (autoPlan.value ("kind").toString (), QStringLiteral ("AUTO CQ"));
    QCOMPARE (autoPlan.value ("frameTypeName").toString (), QStringLiteral ("BEACON"));

	    QVERIFY (sender.configureAutoBeacon (true, 300, true, 92000));
    QCOMPARE (sender.autoBeaconIntervalSeconds (), 300);
    QCOMPARE (radioSpy.size (), 0);
	    QVERIFY (sender.configureAutoBeacon (false, 300, true, 93000));
    QVERIFY (!sender.autoBeaconEnabled ());
  }

  void adapterRoundTripsNarrowRadioHandshakeAudio ()
  {
    qRegisterMetaType<QVector<float>> ("QVector<float>");

    FT2LinkQmlAdapter caller;
    FT2LinkQmlAdapter answerer;
    caller.setLocalStation ("IU8LMC", "JN70", "Salvo");
    answerer.setLocalStation ("K1ABC", "FN42", "Ann");

    QVERIFY (caller.observeStation (
        "K1ABC", "FN42", "Ann", true, true, true, true, true, 2, 0, 1000));

    QSignalSpy callerRadioSpy {&caller, &FT2LinkQmlAdapter::radioTxAudioRequested};
    QSignalSpy answererRadioSpy {&answerer, &FT2LinkQmlAdapter::radioTxAudioRequested};

    caller.setRadioTxArmed (true);
    QVERIFY (caller.startSessionRadioHandshake ("K1ABC", 1100));
    QCOMPARE (callerRadioSpy.size (), 1);
    QList<QVariant> const helloRequest = callerRadioSpy.takeFirst ();
    QCOMPARE (helloRequest[0].toString (), QStringLiteral ("FT2-Link HELLO"));
    QVariantMap helloPlan = helloRequest[2].toMap ();
    QCOMPARE (helloPlan.value ("kind").toString (), QStringLiteral ("HELLO"));
    QCOMPARE (helloPlan.value ("profileName").toString (), QStringLiteral ("NARROW"));

    QVector<float> helloSamples = helloRequest[1].value<QVector<float>> ();
    QVERIFY (!helloSamples.isEmpty ());
    QVERIFY (answerer.ingestRxSamples (pcmFromSamples (helloSamples, 4), "", 2200));
    QTRY_VERIFY_WITH_TIMEOUT (answererRadioSpy.size () >= 1, 3000);
    QCOMPARE (answerer.sessionCount (), 1);
    quint16 const sessionId = caller.activeSessionId ();
    QCOMPARE (answerer.sessionInfo (sessionId).value ("stateName").toString (),
              QStringLiteral ("Connected"));
    QCOMPARE (answerer.sessionInfo (sessionId).value ("remoteCall").toString (),
              QStringLiteral ("IU8LMC"));

    QList<QVariant> const helloAckRequest = answererRadioSpy.takeFirst ();
    QCOMPARE (helloAckRequest[0].toString (), QStringLiteral ("FT2-Link HELLO_ACK"));
    QVariantMap helloAckPlan = helloAckRequest[2].toMap ();
    QCOMPARE (helloAckPlan.value ("kind").toString (), QStringLiteral ("HELLO_ACK"));

    QVector<float> helloAckSamples = helloAckRequest[1].value<QVector<float>> ();
    QVERIFY (!helloAckSamples.isEmpty ());
    QVERIFY (caller.ingestRxSamples (pcmFromSamples (helloAckSamples, 4), "", 3300));
    QCOMPARE (caller.sessionInfo (sessionId).value ("stateName").toString (),
              QStringLiteral ("Connected"));
    QCOMPARE (caller.sessionInfo (sessionId).value ("profileName").toString (),
              QStringLiteral ("W2300"));
  }

  // ===== Gate d'accesso FT2-Link (PBKDF2-HMAC-SHA256) — fail-closed + KAT =====
  // Copre controllers/FT2LinkAccessGate.hpp (single source usato da DecodiumBridge):
  // garanzia che una build non provisionata NON possa aprire il modo FT2-Link.
  void ft2LinkGatePbkdf2MatchesKnownAnswerVectors ()
  {
    using namespace decodium::ft2linkgate;
    // Known-answer vectors PBKDF2-HMAC-SHA256 (RFC 8018): valida l'ALGORITMO.
    QCOMPARE (pbkdf2Sha256 (QStringLiteral ("password"),
                            QByteArrayLiteral ("salt"), 1, 32).toHex (),
              QByteArray ("120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"));
    QCOMPARE (pbkdf2Sha256 (QStringLiteral ("password"),
                            QByteArrayLiteral ("salt"), 2, 32).toHex (),
              QByteArray ("ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43"));
  }

  void ft2LinkGateIsFailClosedWhenNotProvisioned ()
  {
    using namespace decodium::ft2linkgate;
    // Build non provisionata: salt/hash vuoti -> NON configurato -> verify sempre false.
    QVERIFY (!isConfigured (QByteArray (), QByteArray (), 160000));
    QVERIFY (!verifyPassword (QStringLiteral ("qualsiasi"), QByteArray (),
                              QByteArray (), 160000));
    QByteArray const salt16 (16, '\x01');
    QByteArray const hash32 (32, '\x02');
    QVERIFY (!isConfigured (QByteArray (15, '\x01'), hash32, 160000));  // salt corto
    QVERIFY (!isConfigured (salt16, QByteArray (31, '\x02'), 160000));  // hash corto
    QVERIFY (!isConfigured (salt16, hash32, 9999));                     // iter troppo basse
    QVERIFY (isConfigured (salt16, hash32, 160000));                    // parametri validi
  }

  void ft2LinkGateVerifiesCorrectPasswordAndRejectsWrong ()
  {
    using namespace decodium::ft2linkgate;
    // Vettore realistico: salt 00..0f (16B), pw "ft2link-test-pw", iter 10000.
    QByteArray const salt =
        QByteArray::fromHex ("000102030405060708090a0b0c0d0e0f");
    QByteArray const expected = QByteArray::fromHex (
        "59092daa3dd10513dca798bb62080e3dc2eba23129bdaec29170253dcb1c167e");
    QCOMPARE (salt.size (), 16);
    QCOMPARE (expected.size (), 32);
    QVERIFY (verifyPassword (QStringLiteral ("ft2link-test-pw"), salt, expected, 10000));
    QVERIFY (!verifyPassword (QStringLiteral ("ft2link-test-pX"), salt, expected, 10000));
    QVERIFY (!verifyPassword (QString (), salt, expected, 10000));
  }

  void ft2LinkGateConstantTimeEqualsIsCorrect ()
  {
    using namespace decodium::ft2linkgate;
    QByteArray const a = QByteArrayLiteral ("abcdef");
    QVERIFY (constantTimeEquals (a, QByteArrayLiteral ("abcdef")));
    QVERIFY (!constantTimeEquals (a, QByteArrayLiteral ("abcdeg")));
    QVERIFY (!constantTimeEquals (a, QByteArrayLiteral ("abcde")));  // dimensione diversa
    QVERIFY (!constantTimeEquals (a, QByteArray ()));
  }
};

QTEST_MAIN (TestFt2LinkQmlAdapter)

#include "test_ft2link_qml_adapter.moc"
