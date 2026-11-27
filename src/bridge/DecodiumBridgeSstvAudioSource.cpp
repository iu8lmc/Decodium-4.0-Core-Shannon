#include "DecodiumBridge.h"

#if DECODIUM_HAS_SSTV
#include "src/sstv/rx/SstvAudioRingBuffer.h"
using decodium::sstv::SstvAudioSourceKind;
namespace {
quint32 stableSstvStreamId(QStringView identity)
{
    // Stable FNV-1a over UTF-16 code units. This runs only on owner-thread
    // source transitions; the audio callback receives the resulting scalar.
    quint32 value = 2'166'136'261U;
    for (QChar character : identity) {
        const quint16 codeUnit = character.unicode();
        value ^= static_cast<quint8>(codeUnit & 0xffU);
        value *= 16'777'619U;
        value ^= static_cast<quint8>((codeUnit >> 8U) & 0xffU);
        value *= 16'777'619U;
    }
    return value == 0U ? 1U : value;
}

}
#endif

decodium::sstv::SstvAudioSourceKind DecodiumBridge::currentSstvAudioSourceKind() const
{
#if DECODIUM_HAS_SSTV
    if (m_decoPortUseRemote) {
        return SstvAudioSourceKind::DecoPort;
    }
    // Mirror startAudioCapture(): an enabled RTL path falls through only when
    // discovery is complete, no dongle exists, and TCI audio is available.
    if (rtlSdrEnabled()
        && (m_rtlSdrDiscoveryPending
            || !m_rtlSdrDevices.isEmpty()
            || !usingTciAudioInput())) {
        return SstvAudioSourceKind::RtlSdr;
    }
    // A live native SSTV session is deliberately independent from the
    // application mode's legacy decoder.  In particular, FT8/FT2 normally
    // expose only the legacy PCM tap and startAudioCapture() is skipped; that
    // route is not a stable source for the bounded SSTV worker.  Once SSTV RX
    // has been requested, prefer the Decodium-owned local capture (unless a
    // higher-priority explicit source such as TCI/RTL/DecoPort was selected).
#ifdef Q_OS_LINUX
    if (m_audioInputDevice.startsWith(QStringLiteral("Pulse/PipeWire monitor: "),
                                      Qt::CaseInsensitive)) {
        return SstvAudioSourceKind::WebSdr;
    }
#endif
    if (m_sstvRxRequested && !usingTciAudioInput()) {
        return SstvAudioSourceKind::LocalSoundCard;
    }
    if (usingLegacyBackendForRx()
        && (!useModernSpectrumFeedWithLegacy()
            || !useDedicatedModernAudioCaptureWithLegacy())) {
        return SstvAudioSourceKind::LegacyBackend;
    }
    if (usingTciAudioInput()) {
        return SstvAudioSourceKind::Tci;
    }
    return SstvAudioSourceKind::LocalSoundCard;
#else
    return static_cast<decodium::sstv::SstvAudioSourceKind>(0U);
#endif
}

bool DecodiumBridge::nativeSstvRxForcesDedicatedAudioCapture() const
{
#if DECODIUM_HAS_SSTV
    // The flag is intentionally narrow: it is only true while analog SSTV
    // owns a live RX session.  HAMDRM and normal legacy/FT8 monitoring keep
    // their existing source arbitration.
    return m_sstvRxRequested
        && !m_decoPortUseRemote
        && !rtlSdrEnabled()
        && !usingTciAudioInput();
#else
    return false;
#endif
}

quint32 DecodiumBridge::currentSstvAudioStreamId(
    decodium::sstv::SstvAudioSourceKind kind) const
{
#if DECODIUM_HAS_SSTV
    switch (kind) {
    case SstvAudioSourceKind::LocalSoundCard:
    case SstvAudioSourceKind::WebSdr: {
        QString identity = m_activeRxInputDeviceId.isEmpty()
            ? m_audioInputDeviceId : m_activeRxInputDeviceId;
        if (identity.isEmpty()) {
            identity = m_activeRxInputDeviceName.isEmpty()
                ? m_audioInputDevice : m_activeRxInputDeviceName;
        }
        return stableSstvStreamId(identity.isEmpty()
                                      ? QStringView(u"decodium-default-input")
                                      : QStringView(identity));
    }
    case SstvAudioSourceKind::DecoPort:
        if (remoteLinkIfAny() && remoteLinkIfAny()->streamId() != 0U) {
            return remoteLinkIfAny()->streamId();
        }
        if (remoteLinkIfAny() && !remoteLinkIfAny()->peerAddress().isEmpty()) {
            return stableSstvStreamId(QStringView(remoteLinkIfAny()->peerAddress()));
        }
        return stableSstvStreamId(QStringView(u"decodium-decoport-rx"));
    case SstvAudioSourceKind::Tci:
        return stableSstvStreamId(QStringView(u"decodium-tci-rx"));
    case SstvAudioSourceKind::RtlSdr:
        return stableSstvStreamId(QStringView(u"decodium-rtl-sdr-rx"));
    case SstvAudioSourceKind::LegacyBackend:
        return stableSstvStreamId(QStringView(u"decodium-legacy-rx"));
    case SstvAudioSourceKind::Replay:
        return stableSstvStreamId(QStringView(u"decodium-replay"));
    case SstvAudioSourceKind::Unknown:
        break;
    }
#else
    Q_UNUSED(kind)
#endif
    return 0U;
}

