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

QString functionBody(const QString& source, const QString& signature,
                     const QString& nextSignature)
{
    const int start = source.indexOf(signature);
    if (start < 0)
        return {};
    const int end = source.indexOf(nextSignature, start + signature.size());
    return source.mid(start, end > start ? end - start : -1);
}

} // namespace

class TestPanadapterGpuRecovery final : public QObject
{
    Q_OBJECT

private slots:
    void directPathKeepsStableGpuTexturesAcrossModePassbandChanges();
    void directTextureRebindRetriesGpuBeforeCpuFallback();
    void directGpuMaterialAcknowledgesTheNewTextureGeneration();
    void sourceHandoffRebuildsGpuResourcesBeforeAnyCpuFallback();
};

void TestPanadapterGpuRecovery::directPathKeepsStableGpuTexturesAcrossModePassbandChanges()
{
    const QString source = readSource(QStringLiteral("src/ui/PanadapterItem.cpp"));
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral("kGpuDirectStableOutputBins")));
    QVERIFY(source.contains(QStringLiteral("gpuDirectOutputBinCount")));
    QVERIFY(source.contains(QStringLiteral("The compute shader resamples sourceBins to this stable GPU output width.")));

    const QString compute = functionBody(source,
        QStringLiteral("void PanadapterItem::recordGpuFftCompute()"),
        QStringLiteral("QSGNode* PanadapterItem::updatePaintNode"));
    QVERIFY(!compute.isEmpty());
    QVERIFY(compute.contains(QStringLiteral("const int nBins = directTexturePath")));
    QVERIFY(compute.contains(QStringLiteral("? defaultGpuBins")));
    QVERIFY(compute.contains(QStringLiteral("static_cast<float>(sourceBins - 1)")));
    QVERIFY(compute.contains(QStringLiteral("static_cast<float>(nBins - 1)")));
}

void TestPanadapterGpuRecovery::directTextureRebindRetriesGpuBeforeCpuFallback()
{
    const QString source = readSource(QStringLiteral("src/ui/PanadapterItem.cpp"));
    QVERIFY(!source.isEmpty());
    const QString rebind = functionBody(source,
        QStringLiteral("void PanadapterItem::scheduleGpuDirectTextureRebind(quint64 textureGeneration)"),
        QStringLiteral("void PanadapterItem::recordGpuFftCompute()"));
    QVERIFY(!rebind.isEmpty());
    QVERIFY(rebind.contains(QStringLiteral("QTimer::singleShot")));
    QVERIFY(rebind.contains(QStringLiteral("m_gpuDirectTextureRebuildRequested.store(true")));
    QVERIFY(rebind.contains(QStringLiteral("GPU resource retry")));
    QVERIFY(rebind.contains(QStringLiteral("m_gpuDirectTextureRebuildAttempts")));
    QVERIFY(rebind.contains(QStringLiteral("failGpuFft")));

    // The order is intentional: resource retry must be scheduled before the
    // sole CPU fallback invocation can be reached.
    QVERIFY(rebind.indexOf(QStringLiteral("m_gpuDirectTextureRebuildRequested.store(true"))
            < rebind.indexOf(QStringLiteral("guard->failGpuFft")));

    const QString retry = functionBody(source,
        QStringLiteral("void PanadapterItem::prepareGpuSpectrumRetry()"),
        QStringLiteral("// Compatibilità:"));
    QVERIFY(!retry.isEmpty());
    QVERIFY(retry.contains(QStringLiteral("m_gpuDirectTextureRebuildAttempts.store(0")));
    QVERIFY(retry.contains(QStringLiteral("m_gpuDirectTextureRebuildRequested.store(false")));
}

void TestPanadapterGpuRecovery::directGpuMaterialAcknowledgesTheNewTextureGeneration()
{
    const QString source = readSource(QStringLiteral("src/ui/PanadapterItem.cpp"));
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral("directTexturesRecreated")));
    QVERIFY(source.contains(QStringLiteral("qsg_rebind=queued")));
    QVERIFY(source.contains(QStringLiteral("m_gpuDirectTextureBoundGeneration.store(")));
    QVERIFY(source.contains(QStringLiteral("m_gpuDirectTextureRebuildAttempts.store(0")));
}

void TestPanadapterGpuRecovery::sourceHandoffRebuildsGpuResourcesBeforeAnyCpuFallback()
{
    const QString source = readSource(QStringLiteral("src/ui/PanadapterItem.cpp"));
    QVERIFY(!source.isEmpty());
    const QString handoff = functionBody(source,
        QStringLiteral("void PanadapterItem::recoverGpuDirectVisualAfterSourceHandoff()"),
        QStringLiteral("// Compatibilità:"));
    QVERIFY(!handoff.isEmpty());
    QVERIFY(handoff.contains(QStringLiteral("m_gpuDirectTextureRebindEpoch.fetch_add")));
    QVERIFY(handoff.contains(QStringLiteral("m_gpuDirectTextureRebuildRequested.store(true")));
    QVERIFY(handoff.contains(QStringLiteral("fallback=deferred_until_gpu_retries_exhausted")));
    QVERIFY(!handoff.contains(QStringLiteral("failGpuFft(")));

    const QString bridge = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!bridge.isEmpty());
    const QString bridgeHandoff = functionBody(bridge,
        QStringLiteral("void DecodiumBridge::completeNativePcmToLegacyRxHandoff(const QString& previousMode)"),
        QStringLiteral("void DecodiumBridge::applyDirectVisualAudioCaptureMode"));
    QVERIFY(!bridgeHandoff.isEmpty());
    QVERIFY(bridgeHandoff.contains(QStringLiteral("migrateActiveMonitoringToLegacyBackend()")));
    QVERIFY(bridgeHandoff.contains(QStringLiteral("recoverGpuPanadapterAfterSourceHandoff(reason)")));
}

QTEST_MAIN(TestPanadapterGpuRecovery)
#include "test_panadapter_gpu_recovery.moc"
