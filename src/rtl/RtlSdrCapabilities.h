// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QtGlobal>

namespace decodium::rtl_sdr {

// The Blog V3 direct-sampling input is usable from roughly 500 kHz to the
// driver's 24 MHz tuner/direct transition. Above that range the tuner path is
// the only meaningful choice. The RTL-SDR Blog V4 is deliberately excluded:
// it has a dedicated HF upconverter and no usable antenna path to the Q ADC.
inline constexpr qint64 kDirectSamplingMinimumFrequencyHz = 500000;
inline constexpr qint64 kDirectSamplingMaximumFrequencyHz = 24000000;

enum class DirectSamplingBlockReason {
    None,
    BlogV4UsesUpconverter,
    OutsideHfRange
};

inline bool isRtlSdrBlogV4Identity(const QString& identity)
{
    return identity.contains(QStringLiteral("Blog V4"), Qt::CaseInsensitive);
}

inline bool isDirectSamplingFrequency(qint64 frequencyHz)
{
    return frequencyHz >= kDirectSamplingMinimumFrequencyHz
        && frequencyHz <= kDirectSamplingMaximumFrequencyHz;
}

inline DirectSamplingBlockReason directSamplingBlockReason(const QString& deviceIdentity,
                                                            qint64 frequencyHz)
{
    if (isRtlSdrBlogV4Identity(deviceIdentity)) {
        return DirectSamplingBlockReason::BlogV4UsesUpconverter;
    }
    if (!isDirectSamplingFrequency(frequencyHz)) {
        return DirectSamplingBlockReason::OutsideHfRange;
    }
    return DirectSamplingBlockReason::None;
}

} // namespace decodium::rtl_sdr
