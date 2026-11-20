#include "Detector/LegacyJtDecodeWorker.hpp"
#include "src/bridge/DecodiumBridgeTxWave.h"

#include <QtGlobal>
#include <iostream>

int main()
{
    QString error;
    const auto waveform = buildTxWaveformForMessage(
        QStringLiteral("JT65"), QStringLiteral("CQ K1ABC FN42"), 1500, 60.0, &error);
    if (waveform.isEmpty()) {
        std::cerr << "JT65 waveform generation failed: " << error.toStdString() << '\n';
        return 1;
    }

    decodium::legacyjt::DecodeRequest request;
    request.mode = QStringLiteral("JT65");
    request.audio = QVector<short>(720000, 0);
    for (int i = 0; i < waveform.size() / 4 && i + 12000 < request.audio.size(); ++i)
        request.audio[i + 12000] = qBound(-32767, qRound(waveform[i * 4] * 22000.0f), 32767);
    quint32 noise = 1;
    for (auto &sample : request.audio) {
        noise = 1664525U * noise + 1013904223U;
        sample = qBound(-32767, int(sample) + (int(noise >> 16) - 32768) / 12, 32767);
    }
    request.nfqso = 1500;
    request.nfa = 200;
    request.nfb = 3000;
    request.ndepth = 3;
    decodium::jt65::AverageState state;
    const auto rows = decodium::jt65::decode_async_jt65(request, &state);
    for (const auto &row : rows) {
        if (row.contains(QStringLiteral("CQ K1ABC FN42"))) {
            std::cout << row.toStdString() << '\n';
            return 0;
        }
    }
    std::cerr << "JT65 message missing; decoded rows: " << rows.join(QStringLiteral(" | ")).toStdString() << '\n';
    return 1;
}
