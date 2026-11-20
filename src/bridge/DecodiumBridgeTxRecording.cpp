#include "DecodiumBridge.h"
#include "DecodiumBridgeWavIO.h"
#include <QThread>

void decodiumBridgeTxRecordingLog(const QString& message);

void DecodiumBridge::saveTxRecordingAsync(const QString& path, QVector<float> wave, int sampleRate,
                                          const QString& logLabel)
{
    if (path.trimmed().isEmpty() || wave.isEmpty() || sampleRate <= 0) {
        return;
    }

    QThread* writer = QThread::create([path, wave, sampleRate, logLabel]() {
        bool const ok = writeMono16WavFile(path, wave, sampleRate);
        decodiumBridgeTxRecordingLog(QStringLiteral("%1 recording %2: %3")
                      .arg(logLabel,
                           ok ? QStringLiteral("saved") : QStringLiteral("failed"),
                           path));
    });
    writer->setObjectName(QStringLiteral("DecodiumTxWavWriter"));
    connect(writer, &QThread::finished, writer, &QObject::deleteLater);
    writer->start(QThread::LowPriority);
}

