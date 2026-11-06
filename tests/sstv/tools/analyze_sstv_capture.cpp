// SPDX-License-Identifier: GPL-3.0-or-later
// Replay a bounded stereo float32 48 kHz lab capture through the real RX runtime.
#include "src/sstv/integration/SstvRxRuntime.h"
#include <QCoreApplication>
#include <QFile>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2;
    QFile file(QString::fromLocal8Bit(argv[1]));
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0
        || file.size() > 100'000'000 || file.size() % 8 != 0) return 2;
    decodium::sstv::SstvRxRuntime runtime;
    if (!runtime.start(decodium::sstv::SstvAudioSourceKind::Replay, 1U)) return 3;
    const auto token = runtime.routeToken();
    quint64 chunks = 0;
    while (!file.atEnd()) {
        const QByteArray data = file.read(960 * 8);
        if (data.isEmpty() || data.size() % 8 != 0) return 4;
        QVector<short> pcm(data.size() / 8);
        for (qsizetype i = 0; i < pcm.size(); ++i) {
            float sample = 0;
            std::memcpy(&sample, data.constData() + i * 8, sizeof(sample));
            if (!std::isfinite(sample)) return 4;
            pcm[i] = static_cast<short>(std::lround(std::clamp(sample, -1.0F, 1.0F) * 32767.0F));
        }
        if (!runtime.enqueuePcm16At(std::move(pcm), 48000, token, 1'000'000'000)) return 5;
        ++chunks;
        int waits = 0;
        while (runtime.snapshot().generationChunksProcessed < chunks && waits++ < 5000) QThread::msleep(1);
        if (waits >= 5000) return 6;
    }
    const auto snapshot = runtime.snapshot();
    std::printf("mode=%s valid=%d available=%d complete=%d coverage=%.6f\n",
        qPrintable(snapshot.image.mode), snapshot.vis.valid, snapshot.image.available,
        snapshot.image.complete, snapshot.image.coverage);
    runtime.shutdown();
    return 0;
}
