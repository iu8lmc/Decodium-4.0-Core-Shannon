// SPDX-License-Identifier: MIT
//
// Ownership policy for a local RX capture while the application mode changes.
// Keep this independent of Qt Multimedia so it can be regression-tested
// without a real sound card or a platform-specific audio backend.

#pragma once

namespace decodium {
namespace audio {

struct ModeChangeCaptureState
{
    ModeChangeCaptureState() noexcept
        : hasLocalQtCapture(false)
        , tciCaptureActive(false)
        , rtlSdrCaptureActive(false)
        , targetUsesLegacyBackend(false)
        , targetKeepsDedicatedModernCapture(false)
    {
    }

    bool hasLocalQtCapture;
    bool tciCaptureActive;
    bool rtlSdrCaptureActive;
    bool targetUsesLegacyBackend;
    bool targetKeepsDedicatedModernCapture;
};

// A mode change alone does not alter the local device, format or sink.  When
// the bridge remains the audio owner, stopping/recreating QAudioSource creates
// an avoidable race with backend callbacks (notably WASAPI on Windows).  The
// only deliberate hand-off is to a legacy backend which opens the device for
// itself.  A configured dedicated modern capture is still bridge-owned.
inline bool preserveExistingCaptureForModeChange(ModeChangeCaptureState const& state) noexcept
{
    if (state.rtlSdrCaptureActive) {
        return true;
    }
    if (!state.hasLocalQtCapture || state.tciCaptureActive) {
        return false;
    }
    return !state.targetUsesLegacyBackend || state.targetKeepsDedicatedModernCapture;
}

// SoundInput is retained after stopAudioCapture(), so object existence says
// nothing about who currently produces RX levels. Select the meter source by
// RX ownership (not TX ownership), including an explicit native capture override.
inline bool useLegacyRxLevels(bool legacyOwnsRx, bool nativeCaptureOverride) noexcept
{
    return legacyOwnsRx && !nativeCaptureOverride;
}

// Moving away from CW/JTTY/SSB is not merely a QAudioSource ownership change:
// the FT8/FT4 legacy backend has to resume its monitor and its PCM tap has to
// be selected for the visual pipeline.  Keep this decision separate from the
// actual Qt/legacy calls so the complete hand-off is testable on every OS.
struct LegacyRxHandoffState
{
    LegacyRxHandoffState() noexcept
        : monitoringActive(false)
        , leavingNativePcmMode(false)
        , targetUsesLegacyBackend(false)
        , targetKeepsDedicatedModernCapture(false)
    {
    }

    bool monitoringActive;
    bool leavingNativePcmMode;
    bool targetUsesLegacyBackend;
    bool targetKeepsDedicatedModernCapture;
};

inline bool requiresLegacyRxHandoff(LegacyRxHandoffState const& state) noexcept
{
    return state.monitoringActive
        && state.leavingNativePcmMode
        && state.targetUsesLegacyBackend;
}

inline bool requiresLegacyPcmTapForHandoff(LegacyRxHandoffState const& state) noexcept
{
    return requiresLegacyRxHandoff(state)
        && !state.targetKeepsDedicatedModernCapture;
}

// The inverse transition needs an equally explicit decision.  FT8/FT4 may
// have handed the sound device to the embedded legacy backend, while Q65-* is
// decoded by Decodium's native PCM pipeline so that its period and A-E
// submode are preserved.  Merely changing the mode label leaves the native
// capture and UTC period timer dormant, which looks like one initial decode
// followed by silence.
struct BridgeRxHandoffState
{
    BridgeRxHandoffState() noexcept
        : monitoringActive(false)
        , sourceUsesLegacyBackend(false)
        , targetUsesLegacyBackend(false)
    {
    }

    bool monitoringActive;
    bool sourceUsesLegacyBackend;
    bool targetUsesLegacyBackend;
};

inline bool requiresBridgeRxHandoff(BridgeRxHandoffState const& state) noexcept
{
    return state.monitoringActive
        && state.sourceUsesLegacyBackend
        && !state.targetUsesLegacyBackend;
}

} // namespace audio
} // namespace decodium
