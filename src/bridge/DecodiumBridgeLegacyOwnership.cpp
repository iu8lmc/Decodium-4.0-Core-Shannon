#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "AudioModeChangeCapturePolicy.h"

bool decodiumBridgeIsFt2LinkApplicationMode(QString);
bool decodiumBridgeIsStreamingKeyboardMode(QString const&);
bool decodiumBridgeOwnsLegacyJtRxWhenCatSuppressed(QString const&);
bool decodiumBridgeOwnsFst4Audio(QString const&);

bool DecodiumBridge::usingLegacyBackendForTx() const
{
    if (rtlSdrEnabled()) {
        return false;
    }
    QString const normalizedMode = m_mode.trimmed().toUpper();
    // The embedded WSJT-X backend understands the base Q65 mode but not the
    // explicit period/submode labels.  Keep Q65-* on the bridge-owned path so
    // the native decoder and TX waveform receive both the period and A-E
    // submode instead of silently falling back to the legacy FT8 path.
    if (normalizedMode.startsWith(QStringLiteral("Q65-"))) {
        return false;
    }
    if (decodiumBridgeIsFt2LinkApplicationMode(m_mode)
        || decodiumBridgeOwnsFst4Audio(m_mode)
        // RTTY has its own QML/native modem.  The embedded legacy backend
        // deliberately has no RTTY mode, so letting it retain its previous
        // FT8/FT4/JT mode makes its state poll silently take ownership back.
        || decodiumBridgeIsStreamingKeyboardMode(normalizedMode)
        || normalizedMode == QStringLiteral("WSPR")) {
        return false;
    }
    return legacyTxBackendRequested() && legacyBackendAvailable();
}

bool DecodiumBridge::usingLegacyBackendForRx() const
{
    // DecoPort injects the remote PCM stream into DecodiumAudioSink.  The
    // embedded legacy backend owns its own local audio capture and cannot
    // consume that injected buffer, so leaving it selected here produces the
    // misleading combination of audible remote audio with no decodes and a
    // stale/empty local waterfall.  Remote radio use must therefore stay on
    // the bridge-owned decoder path on every platform.
    if (m_decoPortUseRemote) {
        return false;
    }
    // RTL-SDR is a complete RX source. It must win before the embedded legacy
    // backend is selected, otherwise startup opens the saved sound card and
    // the dongle never receives a start request.
    if (rtlSdrEnabled()) {
        return false;
    }
    // In no-CAT/lab operation the bridge already owns the JT4/JT9/JT65 PCM TX
    // path. Keep RX on the same bridge-owned audio/decode path, otherwise the
    // QML side can show waterfall energy while never dispatching the in-process
    // LegacyJtDecodeWorker.
    if (decodiumBridgeOwnsLegacyJtRxWhenCatSuppressed(m_mode)
        || decodiumBridgeOwnsFst4Audio(m_mode)) {
        return false;
    }
#if defined(Q_OS_MAC)
    if (decodiumBridgeIsFt2LinkApplicationMode(m_mode)) {
        return legacyTxBackendRequested() && legacyBackendAvailable();
    }
#endif
    return usingLegacyBackendForTx();
}

bool DecodiumBridge::legacyBackendRequestedForRx() const
{
    if (rtlSdrEnabled()) {
        return false;
    }
    // Q65 variants are fully bridge-owned.  In particular on macOS this must
    // be checked before the generic "legacy requested" branch below: otherwise
    // the worker is not created when Q65-60A is selected after FT8/FT4.
    if (m_mode.trimmed().toUpper().startsWith(QStringLiteral("Q65-"))) {
        return false;
    }
    if (decodiumBridgeIsStreamingKeyboardMode(m_mode)
        || decodiumBridgeOwnsLegacyJtRxWhenCatSuppressed(m_mode)
        || decodiumBridgeOwnsFst4Audio(m_mode)) {
        return false;
    }
#if defined(Q_OS_MAC)
    // WSPR is implemented by the modern bridge on macOS. Other digital modes
    // are owned by the requested legacy backend even while its deferred
    // construction is still pending.
    if (m_mode.trimmed().compare(QStringLiteral("WSPR"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    return legacyTxBackendRequested();
#else
    return usingLegacyBackendForRx();
#endif
}

bool DecodiumBridge::useModernSpectrumFeedWithLegacy() const
{
#if defined(Q_OS_MAC)
    if (decodiumBridgeIsFt2LinkApplicationMode(m_mode)) {
        return true;
    }
#endif
    // The legacy FT8/FT4 backend must own the input device while it is decoding.
    // Opening a second modern capture for the panadapter can attenuate or disturb
    // USB audio devices on different backends, which lowers decoded SNR.
    return m_directVisualAudioCaptureUnsafe;
}

bool DecodiumBridge::useDedicatedModernAudioCaptureWithLegacy() const
{
    // Direct Visual no longer opens a second QAudioSource by default when the
    // embedded legacy decoder is active. The legacy detector already exposes a
    // PCM tap; use that as the single RX source and keep the GPU-direct
    // panadapter path downstream. FT2-Link must follow the same rule on macOS:
    // on real USB radios the second modern QAudioSource can produce silence
    // while the legacy waterfall still sees RF. Keep dual capture as an
    // explicit lab/debug opt-in only.
#if defined(Q_OS_MAC)
    if (decodiumBridgeIsFt2LinkApplicationMode(m_mode)) {
        return useModernSpectrumFeedWithLegacy()
            && qEnvironmentVariableIntValue("DECODIUM_FT2LINK_DEDICATED_MODERN_RX", 0) != 0;
    }
#endif
    return useModernSpectrumFeedWithLegacy()
        && qEnvironmentVariableIntValue("DECODIUM_DUAL_AUDIO_PANADAPTER", 0) != 0;
}

bool DecodiumBridge::usesLegacyRxLevels() const
{
    bool nativeCaptureOverride = useDedicatedModernAudioCaptureWithLegacy()
        || m_tciAudioCaptureActive;
#if DECODIUM_HAS_SSTV
    nativeCaptureOverride = nativeCaptureOverride
        || m_sstvWorkspaceActive || nativeSstvRxForcesDedicatedAudioCapture();
#endif
    return decodium::audio::useLegacyRxLevels(usingLegacyBackendForRx(), nativeCaptureOverride);
}

bool DecodiumBridge::legacyBackendAvailable() const
{
    return m_legacyBackend && m_legacyBackend->available();
}

