#include <QFile>
#include <QtTest>

namespace {

QString readSource(const QString& relativePath)
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

} // namespace

class TestCwCatSpeedFallback final : public QObject
{
    Q_OBJECT

private slots:
    void unavailableSpeedControlDoesNotBlockMorseSend();
    void yaesuSingleCharacterUsesLiteralTextPath();
    void cwModeIsReassertedBeforeMorseSend();
    void managerKeepsDesiredModeInCwDuringNativeKeying();
    void networkBridgeDispatchHasABoundedThirtySecondWatchdog();
    void nativeStopForcesPttOffBeforeOptionalStopMorse();
    void yaesuStopAlsoForcesCatOffWhenPttUsesAnExternalLine();
    void managerClearsDesiredPttBeforeDispatchingEmergencyStop();
    void stopInvalidatesMorseWaitingOnCatWorker();
};

void TestCwCatSpeedFallback::unavailableSpeedControlDoesNotBlockMorseSend()
{
    const QString source = readSource(QStringLiteral("Transceiver/HamlibTransceiver.cpp"));
    QVERIFY(!source.isEmpty());

    const int functionStart = source.indexOf(
        QStringLiteral("QString HamlibTransceiver::send_morse_checked"));
    const int functionEnd = source.indexOf(
        QStringLiteral("void HamlibTransceiver::send_morse"), functionStart);
    QVERIFY(functionStart >= 0);
    QVERIFY(functionEnd > functionStart);

    const QString implementation = source.mid(functionStart, functionEnd - functionStart);
    QVERIFY(implementation.contains(QStringLiteral("rc == -RIG_ENAVAIL")));
    QVERIFY(implementation.contains(QStringLiteral("rc == -RIG_ENIMPL")));
    QVERIFY(implementation.contains(QStringLiteral("using the radio's configured speed")));

    const int optionalBranch = implementation.indexOf(
        QStringLiteral("if (rc == -RIG_ENAVAIL || rc == -RIG_ENIMPL)"));
    const int morseSend = implementation.indexOf(
        QStringLiteral("int const rc = rig_send_morse"));
    QVERIFY(optionalBranch >= 0);
    QVERIFY(morseSend > optionalBranch);
}

void TestCwCatSpeedFallback::yaesuSingleCharacterUsesLiteralTextPath()
{
    const QString source = readSource(QStringLiteral("Transceiver/HamlibTransceiver.cpp"));
    QVERIFY(!source.isEmpty());

    const int functionStart = source.indexOf(
        QStringLiteral("QString HamlibTransceiver::send_morse_checked"));
    const int functionEnd = source.indexOf(
        QStringLiteral("void HamlibTransceiver::send_morse"), functionStart);
    QVERIFY(functionStart >= 0);
    QVERIFY(functionEnd > functionStart);

    const QString implementation = source.mid(functionStart, functionEnd - functionStart);
    QVERIFY(implementation.contains(QStringLiteral("is_yaesu_rig")));
    QVERIFY(implementation.contains(QStringLiteral("literal_single_char")));
    QVERIFY(implementation.contains(QStringLiteral("payload += QLatin1Char (' ');")));
    const int normalization = implementation.indexOf(
        QStringLiteral("payload += QLatin1Char (' ');"));
    const int morseSend = implementation.indexOf(
        QStringLiteral("int const rc = rig_send_morse"));
    QVERIFY(normalization >= 0);
    QVERIFY(morseSend > normalization);
}

void TestCwCatSpeedFallback::cwModeIsReassertedBeforeMorseSend()
{
    const QString source = readSource(QStringLiteral("Transceiver/HamlibTransceiver.cpp"));
    QVERIFY(!source.isEmpty());

    const int functionStart = source.indexOf(
        QStringLiteral("QString HamlibTransceiver::send_morse_checked"));
    const int functionEnd = source.indexOf(
        QStringLiteral("void HamlibTransceiver::send_morse"), functionStart);
    QVERIFY(functionStart >= 0);
    QVERIFY(functionEnd > functionStart);

    const QString implementation = source.mid(functionStart, functionEnd - functionStart);
    const int readMode = implementation.indexOf(QStringLiteral("rig_get_mode"));
    const int enforceCw = implementation.indexOf(QStringLiteral("rig_set_mode"));
    const int setSpeed = implementation.indexOf(QStringLiteral("rig_set_level"));
    const int morseSend = implementation.indexOf(QStringLiteral("rig_send_morse"));
    QVERIFY(readMode >= 0);
    QVERIFY(enforceCw > readMode);
    QVERIFY(implementation.contains(QStringLiteral("current_mode != RIG_MODE_CW")));
    QVERIFY(implementation.contains(QStringLiteral("current_mode != RIG_MODE_CWR")));
    QVERIFY(implementation.contains(QStringLiteral("RIG_PASSBAND_NOCHANGE")));
    QVERIFY(setSpeed > enforceCw);
    QVERIFY(morseSend > setSpeed);
}

void TestCwCatSpeedFallback::managerKeepsDesiredModeInCwDuringNativeKeying()
{
    const QString source = readSource(
        QStringLiteral("src/radio/DecodiumTransceiverManager.cpp"));
    QVERIFY(!source.isEmpty());

    const int functionStart = source.indexOf(
        QStringLiteral("bool DecodiumTransceiverManager::sendRigMorse"));
    const int functionEnd = source.indexOf(
        QStringLiteral("void DecodiumTransceiverManager::stopRigMorse"), functionStart);
    QVERIFY(functionStart >= 0);
    QVERIFY(functionEnd > functionStart);

    const QString implementation = source.mid(functionStart, functionEnd - functionStart);
    const int desiredCw = implementation.indexOf(
        QStringLiteral("d->desired.mode(Transceiver::CW)"));
    const int desiredRx = implementation.indexOf(
        QStringLiteral("d->desired.ptt(false)"));
    const int workerDispatch = implementation.indexOf(
        QStringLiteral("QMetaObject::invokeMethod"));
    QVERIFY(desiredCw >= 0);
    QVERIFY(desiredRx > desiredCw);
    QVERIFY(workerDispatch > desiredRx);
}

void TestCwCatSpeedFallback::networkBridgeDispatchHasABoundedThirtySecondWatchdog()
{
    const QString source = readSource(QStringLiteral("src/cw/CwController.cpp"));
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(
        QStringLiteral("constexpr int kRigDispatchWatchdogMs = 30000")));
    QVERIFY(source.contains(
        QStringLiteral("m_audioDone.start(kRigDispatchWatchdogMs)")));
}

void TestCwCatSpeedFallback::nativeStopForcesPttOffBeforeOptionalStopMorse()
{
    const QString source = readSource(QStringLiteral("Transceiver/HamlibTransceiver.cpp"));
    QVERIFY(!source.isEmpty());
    const int functionStart = source.indexOf(QStringLiteral("void HamlibTransceiver::stop_morse"));
    const int functionEnd = source.indexOf(QStringLiteral("void HamlibTransceiver::do_tune"), functionStart);
    QVERIFY(functionStart >= 0);
    QVERIFY(functionEnd > functionStart);
    const QString implementation = source.mid(functionStart, functionEnd - functionStart);
    const int pttOff = implementation.indexOf(QStringLiteral("rig_set_ptt"));
    const int capabilityCheck = implementation.indexOf(QStringLiteral("rig->caps->stop_morse"));
    const int stopMorse = implementation.indexOf(QStringLiteral("rig_stop_morse"));
    QVERIFY(pttOff >= 0);
    QVERIFY(capabilityCheck > pttOff);
    QVERIFY(stopMorse > capabilityCheck);
    QVERIFY(implementation.contains(QStringLiteral("RIG_PTT_OFF")));
}

void TestCwCatSpeedFallback::yaesuStopAlsoForcesCatOffWhenPttUsesAnExternalLine()
{
    const QString source = readSource(QStringLiteral("Transceiver/HamlibTransceiver.cpp"));
    QVERIFY(!source.isEmpty());
    const int functionStart = source.indexOf(QStringLiteral("void HamlibTransceiver::stop_morse"));
    const int functionEnd = source.indexOf(QStringLiteral("void HamlibTransceiver::do_tune"), functionStart);
    QVERIFY(functionStart >= 0);
    QVERIFY(functionEnd > functionStart);
    const QString implementation = source.mid(functionStart, functionEnd - functionStart);

    const int rememberRoute = implementation.indexOf(
        QStringLiteral("configured_ptt = rig->state.pttport.type.ptt"));
    const int detectYaesu = implementation.indexOf(QStringLiteral("is_yaesu_rig (rig)"));
    const int forceCatRoute = implementation.indexOf(
        QStringLiteral("rig->state.pttport.type.ptt = RIG_PTT_RIG"));
    const int forceOff = implementation.indexOf(
        QStringLiteral("cat_ptt_rc = rig_set_ptt"), forceCatRoute);
    const int restoreRoute = implementation.indexOf(
        QStringLiteral("rig->state.pttport.type.ptt = configured_ptt"), forceOff);
    QVERIFY(rememberRoute >= 0);
    QVERIFY(detectYaesu > rememberRoute);
    QVERIFY(implementation.contains(QStringLiteral("!rig->caps->stop_morse")));
    QVERIFY(forceCatRoute > detectYaesu);
    QVERIFY(forceOff > forceCatRoute);
    QVERIFY(restoreRoute > forceOff);
}

void TestCwCatSpeedFallback::managerClearsDesiredPttBeforeDispatchingEmergencyStop()
{
    const QString source = readSource(
        QStringLiteral("src/radio/DecodiumTransceiverManager.cpp"));
    QVERIFY(!source.isEmpty());
    const int functionStart = source.indexOf(
        QStringLiteral("void DecodiumTransceiverManager::stopRigMorse"));
    const int functionEnd = source.indexOf(
        QStringLiteral("void DecodiumTransceiverManager::setTciRxGainDb"), functionStart);
    QVERIFY(functionStart >= 0);
    QVERIFY(functionEnd > functionStart);
    const QString implementation = source.mid(functionStart, functionEnd - functionStart);
    const int desiredRx = implementation.indexOf(QStringLiteral("d->desired.ptt(false)"));
    const int dispatch = implementation.indexOf(QStringLiteral("QMetaObject::invokeMethod"));
    const int stop = implementation.indexOf(QStringLiteral("xcv->stop_morse_checked()"));
    QVERIFY(desiredRx >= 0);
    QVERIFY(dispatch > desiredRx);
    QVERIFY(stop > dispatch);
}

void TestCwCatSpeedFallback::stopInvalidatesMorseWaitingOnCatWorker()
{
    const QString source = readSource(QStringLiteral("src/radio/DecodiumTransceiverManager.cpp"));
    const int send = source.indexOf(QStringLiteral("bool DecodiumTransceiverManager::sendRigMorse"));
    const int guard = source.indexOf(QStringLiteral("cancellation->load() != generation"), send);
    const int dispatch = source.indexOf(QStringLiteral("xcv->send_morse_checked"), send);
    QVERIFY(send >= 0);
    QVERIFY(guard > send && guard < dispatch);
    const int stop = source.indexOf(QStringLiteral("void DecodiumTransceiverManager::stopRigMorse"));
    const int invalidate = source.indexOf(QStringLiteral("++*d->morseGeneration"), stop);
    const int notify = source.indexOf(QStringLiteral("emit morseStopRequested()"), stop);
    QVERIFY(invalidate > stop && invalidate < notify);
}

QTEST_APPLESS_MAIN(TestCwCatSpeedFallback)
#include "test_cw_cat_speed_fallback.moc"
