#include "src/bridge/AudioModeChangeCapturePolicy.h"

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

class TestAudioModeChangeCapturePolicy final : public QObject
{
    Q_OBJECT

private slots:
    void keepsTheSameWasapiCaptureWhenTheBridgeRetainsOwnership();
    void releasesLocalCaptureOnlyForAnExclusiveLegacyHandoff();
    void handlesNonLocalSourcesWithoutPretendingTheyAreQtCapture();
    void requiresACompleteLegacyMonitorHandoffAfterNativePcmModes();
    void requiresBridgeHandoffWhenLeavingLegacyForNativeQ65();
    void legacyMigrationStartsStateRefreshEvenWhenStartedInCw();
    void stoppedNativeCaptureDoesNotOwnLegacyMeters();
    void meterFollowsRxOwnershipAcrossRepeatedModeChanges();
    void bridgeUsesTheSharedOwnershipPolicy();
};

void TestAudioModeChangeCapturePolicy::keepsTheSameWasapiCaptureWhenTheBridgeRetainsOwnership()
{
    using decodium::audio::ModeChangeCaptureState;
    using decodium::audio::preserveExistingCaptureForModeChange;

    // Regression for CW -> FT4/FT8: both modes use the same local device,
    // while the modern bridge remains the sink owner.  No QAudioSource
    // stop/create cycle may be requested.
    ModeChangeCaptureState state;
    state.hasLocalQtCapture = true;
    QVERIFY(preserveExistingCaptureForModeChange(state));

    // Direct Visual is still bridge-owned even where a legacy decoder is
    // running beside it, so the existing capture remains valid.
    state.targetUsesLegacyBackend = true;
    state.targetKeepsDedicatedModernCapture = true;
    QVERIFY(preserveExistingCaptureForModeChange(state));
}

void TestAudioModeChangeCapturePolicy::releasesLocalCaptureOnlyForAnExclusiveLegacyHandoff()
{
    using decodium::audio::ModeChangeCaptureState;
    using decodium::audio::preserveExistingCaptureForModeChange;

    ModeChangeCaptureState state;
    state.hasLocalQtCapture = true;
    state.targetUsesLegacyBackend = true;
    QVERIFY(!preserveExistingCaptureForModeChange(state));
}

void TestAudioModeChangeCapturePolicy::handlesNonLocalSourcesWithoutPretendingTheyAreQtCapture()
{
    using decodium::audio::ModeChangeCaptureState;
    using decodium::audio::preserveExistingCaptureForModeChange;

    ModeChangeCaptureState tci;
    tci.hasLocalQtCapture = true;
    tci.tciCaptureActive = true;
    QVERIFY(!preserveExistingCaptureForModeChange(tci));

    ModeChangeCaptureState rtl;
    rtl.rtlSdrCaptureActive = true;
    QVERIFY(preserveExistingCaptureForModeChange(rtl));
}

void TestAudioModeChangeCapturePolicy::requiresACompleteLegacyMonitorHandoffAfterNativePcmModes()
{
    using decodium::audio::LegacyRxHandoffState;
    using decodium::audio::requiresLegacyPcmTapForHandoff;
    using decodium::audio::requiresLegacyRxHandoff;

    // Regression for CW -> FT8: the FT backend becomes the device owner and
    // must also re-enable its PCM tap.  Releasing QAudioSource alone leaves
    // the last GPU texture on screen indefinitely.
    LegacyRxHandoffState cwToFt8;
    cwToFt8.monitoringActive = true;
    cwToFt8.leavingNativePcmMode = true;
    cwToFt8.targetUsesLegacyBackend = true;
    QVERIFY(requiresLegacyRxHandoff(cwToFt8));
    QVERIFY(requiresLegacyPcmTapForHandoff(cwToFt8));

    // When the modern bridge intentionally retains the audio source, the
    // legacy monitor still starts but the visual PCM tap remains disabled.
    cwToFt8.targetKeepsDedicatedModernCapture = true;
    QVERIFY(requiresLegacyRxHandoff(cwToFt8));
    QVERIFY(!requiresLegacyPcmTapForHandoff(cwToFt8));

    LegacyRxHandoffState ordinaryModernSwitch;
    ordinaryModernSwitch.monitoringActive = true;
    ordinaryModernSwitch.leavingNativePcmMode = true;
    QVERIFY(!requiresLegacyRxHandoff(ordinaryModernSwitch));
}

void TestAudioModeChangeCapturePolicy::requiresBridgeHandoffWhenLeavingLegacyForNativeQ65()
{
    using decodium::audio::BridgeRxHandoffState;
    using decodium::audio::requiresBridgeRxHandoff;

    // Regression for FT8/FT4 -> Q65-xx.  Q65 variants keep period/submode in
    // the native bridge, so the previous embedded monitor must release its
    // source and the bridge must re-arm capture and the UTC timer.
    BridgeRxHandoffState ft8ToQ65;
    ft8ToQ65.monitoringActive = true;
    ft8ToQ65.sourceUsesLegacyBackend = true;
    QVERIFY(requiresBridgeRxHandoff(ft8ToQ65));

    ft8ToQ65.targetUsesLegacyBackend = true;
    QVERIFY(!requiresBridgeRxHandoff(ft8ToQ65));

    ft8ToQ65.targetUsesLegacyBackend = false;
    ft8ToQ65.monitoringActive = false;
    QVERIFY(!requiresBridgeRxHandoff(ft8ToQ65));
}

void TestAudioModeChangeCapturePolicy::legacyMigrationStartsStateRefreshEvenWhenStartedInCw()
{
    const QString source = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    const qsizetype begin = source.indexOf(QStringLiteral("void DecodiumBridge::migrateActiveMonitoringToLegacyBackend()"));
    const qsizetype end = source.indexOf(QStringLiteral("void DecodiumBridge::recoverGpuPanadapterAfterSourceHandoff"), begin);
    QVERIFY(begin >= 0 && end > begin);
    const QString migration = source.mid(begin, end - begin);
    // A CW startup never starts m_legacyStateTimer. The normal single-capture
    // handoff must arm it too, not just the optional dual-capture branch.
    QVERIFY(migration.contains(QStringLiteral("scheduleLegacyStateRefreshBurst();")));
    QVERIFY(migration.lastIndexOf(QStringLiteral("scheduleLegacyStateRefreshBurst();"))
            > migration.lastIndexOf(QStringLiteral("m_legacyPcmSpectrumFeed = true;")));
}

void TestAudioModeChangeCapturePolicy::stoppedNativeCaptureDoesNotOwnLegacyMeters()
{
    const QString source = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    const qsizetype begin = source.indexOf(QStringLiteral("void DecodiumBridge::syncLegacyBackendState()"));
    const qsizetype end = source.indexOf(QStringLiteral("QString const legacyModeRaw"), begin);
    QVERIFY(begin >= 0 && end > begin);
    const QString meters = source.mid(begin, end - begin);
    // stopAudioCapture intentionally retains m_soundInput. Its lifetime must
    // not decide whether RX levels/auto gain follow the active legacy input.
    QVERIFY(!meters.contains(QStringLiteral("!m_soundInput")));
    QVERIFY(meters.contains(QStringLiteral("usesLegacyRxLevels()")));
}

void TestAudioModeChangeCapturePolicy::meterFollowsRxOwnershipAcrossRepeatedModeChanges()
{
    using decodium::audio::useLegacyRxLevels;
    // CW -> FT8/FT4/FT2 -> CW. The stopped native SoundInput survives every
    // transition; it is deliberately not an input to this decision.
    for (int cycle = 0; cycle < 3; ++cycle) {
        QVERIFY(!useLegacyRxLevels(false, false)); // native CW
        QVERIFY(useLegacyRxLevels(true, false));   // exclusive legacy RX
        QVERIFY(!useLegacyRxLevels(false, false)); // native CW again
    }
    QVERIFY(!useLegacyRxLevels(true, true)); // dedicated modern/TCI/SSTV capture
    QVERIFY(!useLegacyRxLevels(false, true)); // remote/native-only RX
}

void TestAudioModeChangeCapturePolicy::bridgeUsesTheSharedOwnershipPolicy()
{
    const QString source = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral("AudioModeChangeCapturePolicy.h")));
    QVERIFY(source.contains(QStringLiteral("ModeChangeCaptureState captureState")));
    QVERIFY(source.contains(QStringLiteral("preserveExistingCaptureForModeChange(captureState)")));
    QVERIFY(source.contains(QStringLiteral("LegacyRxHandoffState legacyHandoffState")));
    QVERIFY(source.contains(QStringLiteral("requiresLegacyRxHandoff(legacyHandoffState)")));
    QVERIFY(source.contains(QStringLiteral("completeNativePcmToLegacyRxHandoff(previousMode)")));
    QVERIFY(source.contains(QStringLiteral("BridgeRxHandoffState bridgeHandoffState")));
    QVERIFY(source.contains(QStringLiteral("requiresBridgeRxHandoff(bridgeHandoffState)")));
    QVERIFY(source.contains(QStringLiteral("bridgeOwnedExplicitQ65")));
    QVERIFY(source.contains(QStringLiteral("normalizedMode == QStringLiteral(\"Q65\")")));
    QVERIFY(source.contains(QStringLiteral("normalizedMode = QStringLiteral(\"Q65-60A\")")));
    QVERIFY(source.contains(QStringLiteral("legacy dialog sync: Q65 variant remains bridge-owned")));
    QVERIFY(source.contains(QStringLiteral("qBound(1, decodeDepth, 3)")));
    QVERIFY(source.contains(QStringLiteral("req.nclearave = q65CoherentAverage ? 0 : 1")));
    QVERIFY(source.contains(QStringLiteral("[Q65DISPATCH]")));
}

QTEST_MAIN(TestAudioModeChangeCapturePolicy)
#include "test_audio_mode_change_capture_policy.moc"
