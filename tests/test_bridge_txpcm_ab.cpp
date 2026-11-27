#include <QAudioFormat>
#include <QByteArray>
#include <QCryptographicHash>
#include <QVector>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

#include "src/bridge/DecodiumBridgeTxPcm.h"
#define boundedAudioChannel originalBoundedAudioChannel
#define txChannelCarriesSample originalTxChannelCarriesSample
#define buildTxPcmBuffer originalBuildTxPcmBuffer
#include "bridge_refactor_fixtures/txpcm_before.inc"
#undef boundedAudioChannel
#undef txChannelCarriesSample
#undef buildTxPcmBuffer

int main()
{
    const QVector<float> wave {-2.0f, -1.0f, -0.5f, 0.0f, 0.25f, 0.75f, 1.0f, 2.0f};
    const int rates[] {12000, 44100, 48000, 96000};
    const QAudioFormat::SampleFormat formats[] {QAudioFormat::UInt8, QAudioFormat::Int16,
                                                QAudioFormat::Int32, QAudioFormat::Float};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    int cases = 0;
    for (int rate : rates) for (int channels : {1, 2}) for (auto sample : formats) {
        QAudioFormat fmt; fmt.setSampleRate(rate); fmt.setChannelCount(channels); fmt.setSampleFormat(sample);
        for (int channel = -1; channel <= 4; ++channel) {
            if (originalBoundedAudioChannel(channel) != boundedAudioChannel(channel)) return 1;
            const auto selection = boundedAudioChannel(channel);
            for (int c = 0; c < channels; ++c)
                if (originalTxChannelCarriesSample(c, channels, selection) != txChannelCarriesSample(c, channels, selection)) return 2;
            const QByteArray a = originalBuildTxPcmBuffer(wave, fmt, selection);
            const QByteArray b = buildTxPcmBuffer(wave, fmt, selection);
            if (a.isEmpty() || a != b) return 3;
            hash.addData(b); ++cases;
        }
    }
    const QAudioFormat invalid;
    if (!originalBuildTxPcmBuffer(wave, invalid).isEmpty() || !buildTxPcmBuffer(wave, invalid).isEmpty()) return 4;
    QAudioFormat valid; valid.setSampleRate(48000); valid.setChannelCount(1); valid.setSampleFormat(QAudioFormat::Int16);
    if (!originalBuildTxPcmBuffer({}, valid).isEmpty() || !buildTxPcmBuffer({}, valid).isEmpty()) return 5;

    // Independent values complement the A/B comparison: the same mistake in
    // both implementations must not pass unnoticed. At 48 kHz no resampling
    // is needed, and a right-channel selection must silence the left channel.
    QAudioFormat stereo;
    stereo.setSampleRate(48000);
    stereo.setChannelCount(2);
    stereo.setSampleFormat(QAudioFormat::Int16);
    const QVector<float> known {-2.0f, -0.5f, 0.0f, 0.5f, 2.0f};
    const qint16 expected[] {0, -32767, 0, -16384, 0, 0, 0, 16384, 0, 32767};
    const QByteArray actual = buildTxPcmBuffer(known, stereo, AudioDevice::Right);
    if (actual.size() != static_cast<qsizetype>(sizeof(expected))) return 6;
    for (std::size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        qint16 value = 0;
        std::memcpy(&value, actual.constData() + i * sizeof(value), sizeof(value));
        if (value != expected[i]) {
            std::cerr << "Unexpected PCM sample at " << i << ": " << value
                      << " instead of " << expected[i] << '\n';
            return 7;
        }
    }
    std::cout << "TX PCM A/B OK: " << cases << " cases sha256=" << hash.result().toHex().constData() << '\n';
    return 0;
}
