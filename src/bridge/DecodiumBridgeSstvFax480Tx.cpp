#include "DecodiumBridge.h"

#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvStudioController.h"
#include "src/sstv/integration/SstvTxCoordinator.h"
#include "src/sstv/integration/SstvTxSources.h"
#include <QAudioDevice>
#include <QImage>
#include <QThread>
#include <exception>
#include <utility>
#include <vector>
bool decodiumSstvOutputMatchesConfiguredSelection(const QString&, const QString&, bool, const QAudioDevice&);
#endif

bool DecodiumBridge::startSstvFax480Tx(const QString& profile)
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvTxCoordinator) initialiseSstvTx();
    auto reject = [this](const QString& detail) {
        m_sstvTxError = detail;
        emit sstvTxStateChanged();
        emit errorMessage(detail);
        return false;
    };
    if (!m_sstvTxCoordinator || !m_sstvStudioController)
        return reject(tr("Native SSTV TX is unavailable"));
    using decodium::sstv::SstvFax480Profile;
    if (profile != QStringLiteral("dayton-480") && profile != QStringLiteral("qsstv-500")
        && profile != QStringLiteral("libsstv-vis-480"))
        return reject(tr("Invalid FAX480 profile"));
    const auto faxProfile = profile == QStringLiteral("dayton-480") ? SstvFax480Profile::Dayton480
        : (profile == QStringLiteral("qsstv-500") ? SstvFax480Profile::Qsstv500 : SstvFax480Profile::LibsstvVis480);
    const auto prepared = m_sstvStudioController->preparedSnapshot();
    if (!prepared || prepared->isNull())
        return reject(tr("Prepare an image before transmitting FAX480"));
    if (!sstvTxCanStart()) return reject(sstvTxPreflightRejection());
    if (!checkSwrAllowsTransmission(QStringLiteral("sstv-tx")))
        return reject(tr("FAX480 TX was blocked by the Decodium SWR safety check"));
    bool requestedDeviceFound = false;
    const QAudioDevice output = resolveTxOutputDevice(&requestedDeviceFound);
    if (!decodiumSstvOutputMatchesConfiguredSelection(m_audioOutputDevice, m_audioOutputDeviceId,
                                              requestedDeviceFound, output))
        return reject(tr("The configured Decodium TX audio output is unavailable"));
    const unsigned channels = output.preferredFormat().channelCount() <= 1 ? 1U : 2U;
    constexpr std::uint32_t sampleRate = 48'000U;
    const decodium::sstv::SstvFax480Mapper mapper(faxProfile, sampleRate);
    decodium::sstv::SstvTxPreparedAudioRequest request;
    try {
        const QImage gray = prepared->scaled(512, int(mapper.height()), Qt::IgnoreAspectRatio,
                                             Qt::SmoothTransformation).convertToFormat(QImage::Format_Grayscale8);
        if (gray.isNull()) return reject(tr("Cannot convert the image to grayscale"));
        std::vector<std::uint8_t> pixels;
        pixels.reserve(std::size_t(512U) * mapper.height());
        for (int y = 0; y < gray.height(); ++y) {
            const auto* line = gray.constScanLine(y);
            pixels.insert(pixels.end(), line, line + gray.width());
        }
        request.source = decodium::sstv::makeFax480Pcm16Source(pixels, faxProfile, sampleRate);
        request.headerEndFrame = mapper.imageStart();
        request.imageEndFrame = mapper.totalSamples();
    } catch (const std::exception& exception) {
        return reject(tr("Cannot prepare FAX480 audio: %1").arg(QString::fromUtf8(exception.what())));
    }
    request.mode = "fax480-experimental";
    request.width = 512U;
    request.height = mapper.height();
    request.channelCount = channels;
    if (channels == 1U || m_audioOutputChannel == 0 || m_audioOutputChannel == 3)
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Both;
    else if (m_audioOutputChannel == 1)
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Left;
    else request.channelRoute = decodium::sstv::SstvTxChannelRoute::Right;
    static_cast<void>(m_sstvTxCoordinator->tick(sstvTxNowMs()));
    m_sstvTxOutputDevice = output;
    m_sstvTxOutputChannels = channels;
    m_sstvTxOutputPinned = true;
    m_sstvTxError.clear();
    m_sstvTxProgress = 0.0;
    m_sstvTxStartInProgress = true;
    const auto result = m_sstvTxCoordinator->startPrepared(sstvTxNowMs(), std::move(request));
    m_sstvTxStartInProgress = false;
    if (!result.accepted) {
        m_sstvTxOutputDevice = QAudioDevice {};
        m_sstvTxOutputChannels = 0U;
        m_sstvTxOutputPinned = false;
        return reject(result.detail.empty() ? tr("FAX480 TX preflight failed")
                                            : QString::fromStdString(result.detail));
    }
    m_sstvTxSessionId = static_cast<quint64>(result.sessionId);
    emit sstvTxStateChanged();
    emit statusMessage(tr("Experimental FAX480 TX accepted"));
    return true;
#else
    Q_UNUSED(profile)
    emit errorMessage(tr("This Decodium build has no native SSTV support"));
    return false;
#endif
}

