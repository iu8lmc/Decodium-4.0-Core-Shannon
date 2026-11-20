#include "DecodiumBridge.h"

#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
#include "src/sstv/integration/SstvTxCoordinator.h"
#include <QAudioDevice>
#include <QCoreApplication>
#include <QThread>
#include <utility>
bool decodiumSstvOutputMatchesConfiguredSelection(const QString&, const QString&, bool, const QAudioDevice&);
#endif

#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
decodium::sstv::SstvTxCoordinatorResult
DecodiumBridge::startHamDrmPreparedAudio(
    std::unique_ptr<decodium::sstv::SstvPcm16Source> source,
    std::string mode)
{
    Q_ASSERT(QThread::currentThread() == thread());
    using decodium::sstv::SstvTxCoordinatorResult;
    using decodium::sstv::SstvTxErrorCode;
    auto reject = [](SstvTxErrorCode error, std::string detail) {
        return SstvTxCoordinatorResult {false, error, std::move(detail), 0U};
    };
    if (!source || source->sampleRate() != 48'000U
        || source->totalSamples() == 0U) {
        return reject(SstvTxErrorCode::EncodingFailure,
                      "HAMDRM prepared PCM source is invalid");
    }
    if (!m_sstvTxCoordinator) {
        initialiseSstvTx();
    }
    if (!m_sstvTxCoordinator || m_sstvTxShuttingDown
        || m_shuttingDown || QCoreApplication::closingDown()) {
        return reject(SstvTxErrorCode::Shutdown,
                      "Decodium SSTV TX authority is unavailable");
    }
    if (!checkSwrAllowsTransmission(QStringLiteral("hamdrm-tx"))) {
        return reject(SstvTxErrorCode::TxNotPermitted,
                      "HAMDRM TX was blocked by the Decodium SWR safety check");
    }

    bool requestedDeviceFound = false;
    const QAudioDevice output = resolveTxOutputDevice(
        &requestedDeviceFound);
    if (!decodiumSstvOutputMatchesConfiguredSelection(
            m_audioOutputDevice, m_audioOutputDeviceId,
            requestedDeviceFound, output)) {
        return reject(
            SstvTxErrorCode::AudioDeviceLoss,
            "the configured Decodium TX audio output is unavailable");
    }
    const unsigned channels = output.preferredFormat().channelCount() <= 1
        ? 1U : 2U;

    decodium::sstv::SstvTxPreparedAudioRequest request;
    request.headerEndFrame = 0U;
    request.imageEndFrame = source->totalSamples();
    request.mode = std::move(mode);
    request.source = std::move(source);
    request.channelCount = channels;
    if (channels == 1U || m_audioOutputChannel == 0
        || m_audioOutputChannel == 3) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Both;
    } else if (m_audioOutputChannel == 1) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Left;
    } else {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Right;
    }

    // 1.0.588: come per SSTV analogico, la sessione precedente va fatta
    // scadere prima del pin: il tick interno di startPrepared() rilascerebbe
    // il route appena fissato.
    if (m_sstvTxCoordinator) {
        static_cast<void>(m_sstvTxCoordinator->tick(sstvTxNowMs()));
    }

    // Pin the exact route before the coordinator runs its shared preflight.
    // The same release barrier used by analog SSTV clears this lease.
    m_sstvTxOutputDevice = output;
    m_sstvTxOutputChannels = channels;
    m_sstvTxOutputPinned = true;
    m_sstvTxError.clear();
    m_sstvTxProgress = 0.0;
    m_sstvTxStartInProgress = true;
    const SstvTxCoordinatorResult result =
        m_sstvTxCoordinator->startPrepared(
            sstvTxNowMs(), std::move(request));
    m_sstvTxStartInProgress = false;
    if (!result.accepted) {
        m_sstvTxOutputDevice = QAudioDevice {};
        m_sstvTxOutputChannels = 0U;
        m_sstvTxOutputPinned = false;
        return result;
    }
    m_sstvTxSessionId = static_cast<quint64>(result.sessionId);
    emit sstvTxStateChanged();
    emit statusMessage(tr("HAMDRM TX accepted by Decodium audio/PTT authority"));
    return result;
}

bool DecodiumBridge::cancelHamDrmPreparedAudio(
    std::uint64_t coordinatorSessionId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvTxCoordinator || coordinatorSessionId == 0U) {
        return false;
    }
    const auto snapshot = m_sstvTxCoordinator->snapshot();
    if (snapshot.stateMachine.currentSessionId != coordinatorSessionId
        || !m_sstvTxCoordinator->stateMachine().active()) {
        return false;
    }
    return m_sstvTxCoordinator->cancel(sstvTxNowMs());
}
#endif

