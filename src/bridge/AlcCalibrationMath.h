#pragma once

#include <QtGlobal>

namespace decodium::alc {

// The TX output level is an attenuation value in tenths of a dB:
// 0 = maximum audio drive, 450 = 45 dB attenuation/minimum drive.
constexpr double kMinAttenuation = 0.0;
constexpr double kMaxAttenuation = 450.0;

enum class DriveLimit {
    None,
    MaxDrive,
    MinDrive
};

inline double adjustedAttenuation(double oldLevel, double alcError)
{
    const double boundedOld = qBound(kMinAttenuation, oldLevel, kMaxAttenuation);
    const double step = qBound(-20.0, alcError * 2.0, 20.0);

    // Positive ALC error means that the ALC is too high: increase attenuation
    // and reduce the audio drive. Negative error does the opposite.
    return qBound(kMinAttenuation, boundedOld + step, kMaxAttenuation);
}

inline DriveLimit limitReached(double oldLevel, double newLevel, double alcError)
{
    if (qAbs(alcError) <= 5.0
        || qAbs(newLevel - oldLevel) > 0.000001) {
        return DriveLimit::None;
    }

    const double boundedOld = qBound(kMinAttenuation, oldLevel, kMaxAttenuation);
    if (boundedOld <= kMinAttenuation + 0.000001) {
        return DriveLimit::MaxDrive;
    }
    if (boundedOld >= kMaxAttenuation - 0.000001) {
        return DriveLimit::MinDrive;
    }
    return DriveLimit::None;
}

} // namespace decodium::alc
