// SPDX-License-Identifier: GPL-3.0-or-later
//
// RF (complex IQ) FFT for the RTL-SDR panadapter.  Kept separate from the
// 12 kHz audio spectrum so the displayed frequency span always means RF.

#pragma once

#include <QVector>

class RtlSdrRfSpectrum final
{
public:
    static constexpr int kFftSize = 4096;

    struct Frame {
        QVector<float> values;
        float minDb {0.0f};
        float maxDb {0.0f};
        float frequencyMinHz {0.0f};
        float frequencyMaxHz {0.0f};
    };

    // `interleavedIq` is signed I/Q at `sampleRate`, with frequency zero at
    // `centerFrequencyHz`.  The resulting row is FFT-shifted left-to-right.
    static Frame compute(const QVector<short>& interleavedIq,
                         int sampleRate,
                         quint32 centerFrequencyHz);
};
