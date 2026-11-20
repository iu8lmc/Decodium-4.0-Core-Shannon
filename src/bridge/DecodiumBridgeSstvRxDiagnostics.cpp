// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumAudioSink.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvRxRuntime.h"
#if DECODIUM_HAS_HAMDRM
#include "src/sstv/digital/waveform/HamDrmWaveformAdapters.h"
#endif
using decodium::sstv::SstvRxRuntime;
qulonglong decodiumSstvBoundedUnsigned(std::uint64_t);
QString decodiumSstvRxAfcModeName(decodium::sstv::SstvRxAfcMode);
QString decodiumSstvRxSlantModeName(decodium::sstv::SstvRxSlantMode);
#define boundedUnsigned decodiumSstvBoundedUnsigned
#define sstvRxAfcModeName decodiumSstvRxAfcModeName
#define sstvRxSlantModeName decodiumSstvRxSlantModeName
#endif

QVariantMap DecodiumBridge::sstvRxDiagnostics() const
{
    QVariantMap result;
    result.insert(QStringLiteral("available"), sstvAvailable());
    result.insert(QStringLiteral("requested"), sstvRxRequested());
    result.insert(QStringLiteral("active"), sstvRxActive());
    result.insert(QStringLiteral("state"), sstvRxState());
    result.insert(QStringLiteral("source"), sstvRxSource());
    result.insert(QStringLiteral("sourceDevice"), sstvRxSourceDevice());
    result.insert(QStringLiteral("audioSuspendedForTx"), m_rxAudioSuspendedForTx);
    result.insert(QStringLiteral("transmitting"), m_transmitting);
    result.insert(QStringLiteral("monitoring"), m_monitoring);
    result.insert(QStringLiteral("legacyRxSelected"), usingLegacyBackendForRx());
    result.insert(QStringLiteral("legacyBackendAvailable"), bool(m_legacyBackend));
    result.insert(QStringLiteral("legacyBackendMonitoring"),
                  m_legacyBackend && m_legacyBackend->monitoring());
    result.insert(QStringLiteral("legacyBackendStartPending"),
                  m_legacyBackend && m_legacyBackend->monitoringStartPending());
    result.insert(QStringLiteral("legacyPcmSpectrumFeed"), m_legacyPcmSpectrumFeed);
    result.insert(QStringLiteral("legacyPcmLastSampleMs"), m_lastLegacyPcmSampleMs);
#if DECODIUM_HAS_SSTV
    result.insert(QStringLiteral("audioDiscarding"), m_audioSink && m_audioSink->discardingSamples());
    result.insert(QStringLiteral("localAudioTapConnected"), bool(m_sstvLocalAudioTap));
    result.insert(QStringLiteral("legacyAudioTapConnected"), bool(m_sstvLegacyAudioTap));
    result.insert(QStringLiteral("sstvRestoresLegacyMonitoring"), m_sstvRestoresLegacyMonitoring);
#endif
    result.insert(QStringLiteral("audioSinkPresent"), bool(m_audioSink));
    result.insert(QStringLiteral("audioHealthTimestampMs"), m_lastAudioHealthMs);

#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
    if (m_hamDrmRxBackend) {
        const auto stats = m_hamDrmRxBackend->statistics();
        result.insert(QStringLiteral("hamdrmAcceptedChunks"), QVariant::fromValue<qulonglong>(stats.acceptedChunks));
        result.insert(QStringLiteral("hamdrmAcceptedSamples"), QVariant::fromValue<qulonglong>(stats.acceptedSamples));
        result.insert(QStringLiteral("hamdrmRejectedChunks"), QVariant::fromValue<qulonglong>(stats.rejectedChunks));
        result.insert(QStringLiteral("hamdrmQueueOverflows"), QVariant::fromValue<qulonglong>(stats.queueOverflows));
        result.insert(QStringLiteral("hamdrmDeliveredGroups"), QVariant::fromValue<qulonglong>(stats.deliveredGroups));
        result.insert(QStringLiteral("hamdrmRequested"), m_hamDrmRxRequested);
        result.insert(QStringLiteral("hamdrmActive"), m_hamDrmRxBackend->active());
        result.insert(QStringLiteral("hamdrmSourceKind"), m_hamDrmRxSourceKind);
        result.insert(QStringLiteral("hamdrmStreamId"), m_hamDrmRxStreamId);
    }
#endif
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return result;
    }

    const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
    result.insert(QStringLiteral("revision"), boundedUnsigned(snapshot.revision));
    result.insert(QStringLiteral("generation"), boundedUnsigned(snapshot.route.generation));
    result.insert(QStringLiteral("streamId"), snapshot.route.source.streamId);
    result.insert(QStringLiteral("workerRunning"), snapshot.workerRunning);
    result.insert(QStringLiteral("workerStarts"),
                  boundedUnsigned(snapshot.workerStarts));
    result.insert(QStringLiteral("workerStops"),
                  boundedUnsigned(snapshot.workerStops));
    result.insert(QStringLiteral("pipelineResets"),
                  boundedUnsigned(snapshot.pipelineResets));
    result.insert(QStringLiteral("chunksProcessed"), boundedUnsigned(snapshot.chunksProcessed));
    result.insert(QStringLiteral("samplesConverted"), boundedUnsigned(snapshot.samplesConverted));
    result.insert(QStringLiteral("samplesResampled"), boundedUnsigned(snapshot.samplesResampled));
    result.insert(QStringLiteral("frequencyObservations"), boundedUnsigned(snapshot.frequencyObservations));
    result.insert(QStringLiteral("lastFrequencyHz"), snapshot.lastFrequencyHz);
    result.insert(QStringLiteral("lastFrequencyConfidence"), snapshot.lastFrequencyConfidence);
    result.insert(QStringLiteral("discontinuities"), boundedUnsigned(snapshot.discontinuities));
    result.insert(QStringLiteral("staleChunksDiscarded"), boundedUnsigned(snapshot.staleChunksDiscarded));
    result.insert(QStringLiteral("processingFailures"), boundedUnsigned(snapshot.processingFailures));
    result.insert(QStringLiteral("producerRejectedCalls"), boundedUnsigned(snapshot.producerRejectedCalls));
    result.insert(QStringLiteral("wrongThreadLifecycleCalls"),
                  boundedUnsigned(snapshot.wrongThreadLifecycleCalls));
    result.insert(QStringLiteral("rxState"), static_cast<int>(snapshot.rxState));
    result.insert(QStringLiteral("rxCause"), static_cast<int>(snapshot.rxCause));
    result.insert(QStringLiteral("visAvailable"), snapshot.vis.available);
    result.insert(QStringLiteral("visValid"), snapshot.vis.valid);
    result.insert(QStringLiteral("visFormat"), static_cast<int>(snapshot.vis.format));
    result.insert(QStringLiteral("visPrimary"), snapshot.vis.primaryPayload);
    result.insert(QStringLiteral("visExtension"), snapshot.vis.extensionPayload);
    result.insert(QStringLiteral("visConfidence"), snapshot.vis.confidence);
    result.insert(QStringLiteral("visRawBits"), snapshot.vis.rawBits);
    result.insert(QStringLiteral("visMappedMode"), snapshot.vis.mappedMode);
    result.insert(QStringLiteral("detectedMode"),
                  snapshot.vis.modeMapped ? snapshot.vis.mappedMode : QString());
    result.insert(QStringLiteral("imageAvailable"), snapshot.image.available);
    result.insert(QStringLiteral("imageComplete"), snapshot.image.complete);
    result.insert(QStringLiteral("imageRadiofaxStopDetected"), snapshot.image.radiofaxStopDetected);
    result.insert(QStringLiteral("imageRadiofaxClockProvisional"), snapshot.image.radiofaxClockProvisional);
    result.insert(QStringLiteral("imageRadiofaxTimedPhasing"), snapshot.image.radiofaxTimedPhasing);
    result.insert(QStringLiteral("imagePartial"), snapshot.image.partial);
    result.insert(QStringLiteral("imageCancelled"), snapshot.image.cancelled);
    result.insert(QStringLiteral("imageGeneration"),
                  boundedUnsigned(snapshot.image.generation));
    result.insert(QStringLiteral("imageAcquisitionId"),
                  boundedUnsigned(snapshot.image.acquisitionId));
    result.insert(QStringLiteral("imageRevision"),
                  boundedUnsigned(snapshot.image.revision));
    result.insert(QStringLiteral("imageLinesPublished"),
                  boundedUnsigned(snapshot.image.linesPublished));
    result.insert(QStringLiteral("imageWidth"), snapshot.image.width);
    result.insert(QStringLiteral("imageHeight"), snapshot.image.height);
    result.insert(QStringLiteral("imageCoveredComponents"),
                  static_cast<qulonglong>(snapshot.image.coveredComponents));
    result.insert(QStringLiteral("imageCompletedPixels"),
                  static_cast<qulonglong>(snapshot.image.completedPixels));
    result.insert(QStringLiteral("imageCoverage"), snapshot.image.coverage);
    result.insert(QStringLiteral("imageMode"), snapshot.image.mode);
    result.insert(QStringLiteral("startUtc"),
                  m_sstvRxStartUtc.isValid()
                      ? m_sstvRxStartUtc.toString(Qt::ISODateWithMs)
                      : QString {});
    result.insert(QStringLiteral("rfFrequencyHz"), m_frequency);

    result.insert(QStringLiteral("afcMode"),
                  sstvRxAfcModeName(snapshot.afc.mode));
    result.insert(QStringLiteral("afcMeasuredOffsetHz"),
                  snapshot.afc.measuredOffsetHz);
    result.insert(QStringLiteral("afcCorrectionHz"),
                  snapshot.afc.correctionHz);
    result.insert(QStringLiteral("afcConfidence"),
                  snapshot.afc.confidence);
    result.insert(QStringLiteral("afcAcceptedReferences"),
                  boundedUnsigned(snapshot.afc.acceptedReferences));
    result.insert(QStringLiteral("afcRejectedReferences"),
                  boundedUnsigned(snapshot.afc.rejectedReferences));
    result.insert(QStringLiteral("afcRejectedImageObservations"),
                  boundedUnsigned(snapshot.afc.rejectedImageObservations));

    result.insert(QStringLiteral("slantMode"),
                  sstvRxSlantModeName(snapshot.slant.mode));
    result.insert(QStringLiteral("slantEstimateValid"),
                  snapshot.slant.estimateValid);
    result.insert(QStringLiteral("slantMeasuredPpm"),
                  snapshot.slant.measuredClockErrorPpm);
    result.insert(QStringLiteral("slantAppliedPpm"),
                  snapshot.slant.appliedClockErrorPpm);
    result.insert(QStringLiteral("slantConfidence"),
                  snapshot.slant.confidence);
    result.insert(QStringLiteral("slantObservedSyncs"),
                  boundedUnsigned(snapshot.slant.observedSyncs));
    result.insert(QStringLiteral("slantRejectedSyncs"),
                  boundedUnsigned(snapshot.slant.rejectedSyncs));

    result.insert(QStringLiteral("syncObserved"), snapshot.sync.observed);
    result.insert(QStringLiteral("syncLocked"), snapshot.sync.locked);
    result.insert(QStringLiteral("syncConfidence"),
                  snapshot.sync.confidence);
    result.insert(QStringLiteral("syncPulseCount"),
                  boundedUnsigned(snapshot.sync.pulseCount));
    result.insert(QStringLiteral("currentLine"),
                  boundedUnsigned(snapshot.sync.currentLine));
    result.insert(QStringLiteral("syncMissedLines"),
                  boundedUnsigned(snapshot.sync.missedLines));

    result.insert(QStringLiteral("fallbackStatus"),
                  static_cast<int>(snapshot.fallback.status));
    result.insert(QStringLiteral("fallbackMode"),
                  snapshot.fallback.selectedMode);
    result.insert(QStringLiteral("fallbackCandidateCount"),
                  static_cast<qulonglong>(snapshot.fallback.candidateCount));
    result.insert(QStringLiteral("fallbackConfidence"),
                  snapshot.fallback.confidence);
    result.insert(QStringLiteral("fallbackLinePeriodSamples"),
                  snapshot.fallback.observedLinePeriodSamples);
    result.insert(QStringLiteral("fallbackSyncDurationSamples"),
                  snapshot.fallback.observedSyncDurationSamples);
    result.insert(QStringLiteral("fallbackAmbiguityCount"),
                  boundedUnsigned(snapshot.fallback.ambiguityCount));

    result.insert(QStringLiteral("fskIdAvailable"),
                  snapshot.fskId.available);
    result.insert(QStringLiteral("fskIdValid"), snapshot.fskId.valid);
    result.insert(QStringLiteral("fskId"), snapshot.fskId.identifier);
    result.insert(QStringLiteral("fskIdConfidence"),
                  snapshot.fskId.confidence);
    result.insert(QStringLiteral("fskIdRawSymbolCount"),
                  static_cast<qulonglong>(snapshot.fskId.rawSymbolCount));

    result.insert(QStringLiteral("signalRms"), snapshot.signal.rms);
    result.insert(QStringLiteral("signalSnrDb"), snapshot.signal.snrDb);
    result.insert(QStringLiteral("signalConfidence"),
                  snapshot.signal.confidence);
    result.insert(QStringLiteral("signalClippingFraction"),
                  snapshot.signal.clippingFraction);

    result.insert(QStringLiteral("replaySampleRate"),
                  snapshot.replay.sampleRate);
    result.insert(QStringLiteral("replayRetentionSeconds"),
                  snapshot.replay.retentionSeconds);
    result.insert(QStringLiteral("replayRetainedSamples"),
                  static_cast<qulonglong>(snapshot.replay.retainedSamples));
    result.insert(QStringLiteral("replayCapacitySamples"),
                  static_cast<qulonglong>(snapshot.replay.capacitySamples));
    result.insert(QStringLiteral("replayAcquisitionDescriptors"),
                  static_cast<qulonglong>(
                      snapshot.replay.acquisitionDescriptors));
    result.insert(QStringLiteral("replayMostRecentAcquisitionId"),
                  boundedUnsigned(snapshot.replay.mostRecentAcquisitionId));

    result.insert(QStringLiteral("dspMeasuredBlocks"),
                  boundedUnsigned(snapshot.performance.measuredDspBlocks));
    result.insert(QStringLiteral("dspAverageBlockNs"), boundedUnsigned(
                      snapshot.performance.averageDspBlockNanoseconds));
    result.insert(QStringLiteral("dspMaximumBlockNs"), boundedUnsigned(
                      snapshot.performance.maximumDspBlockNanoseconds));
    result.insert(QStringLiteral("progressiveUpdates"), boundedUnsigned(
                      snapshot.performance.progressiveUpdates));
    result.insert(QStringLiteral("progressiveUpdatesPerSecond"),
                  snapshot.performance.progressiveUpdatesPerSecond);

    QVariantList scope;
    scope.reserve(snapshot.scope.size());
    for (const SstvRxRuntime::ScopePoint& point : snapshot.scope) {
        scope.push_back(QVariantMap {
            {QStringLiteral("sample"), boundedUnsigned(point.sample)},
            {QStringLiteral("frequencyHz"), point.frequencyHz},
            {QStringLiteral("confidence"), point.confidence},
            {QStringLiteral("rms"), point.rms},
            {QStringLiteral("snrDb"), point.snrDb},
        });
    }
    result.insert(QStringLiteral("scope"), scope);
    result.insert(QStringLiteral("queuedChunks"),
                  static_cast<qulonglong>(snapshot.ingress.queue.queuedChunks));
    result.insert(QStringLiteral("queuedSamples"),
                  static_cast<qulonglong>(snapshot.ingress.queue.queuedSamples));
    result.insert(QStringLiteral("droppedChunks"),
                  boundedUnsigned(snapshot.ingress.queue.droppedChunks));
    result.insert(QStringLiteral("droppedSamples"),
                  boundedUnsigned(snapshot.ingress.queue.droppedSamples));
    result.insert(QStringLiteral("lastError"), snapshot.lastError);
#endif
    return result;
}

