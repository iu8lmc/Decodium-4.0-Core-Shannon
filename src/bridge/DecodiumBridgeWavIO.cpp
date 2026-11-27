#include "DecodiumBridgeWavIO.h"
#include <QDataStream>
#include <QDebug>
#include <QFile>

bool decodiumBridgeEnsureParentDirectoryForFile(QString const&, QString*);

bool writeMono16WavFile(QString const& path, QVector<float> const& wave, int sampleRate)
{
    if (wave.isEmpty() || sampleRate <= 0) {
        return false;
    }

    QString error;
    if (!decodiumBridgeEnsureParentDirectoryForFile(path, &error)) {
        qWarning() << error;
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot open TX WAV for write:" << path;
        return false;
    }

    QDataStream s(&f);
    s.setByteOrder(QDataStream::LittleEndian);

    quint32 const dataSize = static_cast<quint32>(wave.size() * sizeof(qint16));
    quint32 const fileSize = 36u + dataSize;

    f.write("RIFF", 4);
    s << fileSize;
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    s << quint32(16);
    s << quint16(1);
    s << quint16(1);
    s << quint32(sampleRate);
    s << quint32(sampleRate * sizeof(qint16));
    s << quint16(sizeof(qint16));
    s << quint16(16);
    f.write("data", 4);
    s << dataSize;

    for (float sample : wave) {
        qint16 const v = static_cast<qint16>(qRound(qBound(-1.0f, sample, 1.0f) * 32767.0f));
        s << v;
    }

    return true;
}

bool writeMono16WavFile(QString const& path, QVector<short> const& samples, int sampleRate)
{
    if (samples.isEmpty() || sampleRate <= 0) {
        return false;
    }

    QString error;
    if (!decodiumBridgeEnsureParentDirectoryForFile(path, &error)) {
        qWarning() << error;
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot open RX WAV for write:" << path;
        return false;
    }

    QDataStream s(&f);
    s.setByteOrder(QDataStream::LittleEndian);

    quint32 const dataSize = static_cast<quint32>(samples.size() * sizeof(qint16));
    quint32 const fileSize = 36u + dataSize;

    f.write("RIFF", 4);
    s << fileSize;
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    s << quint32(16);
    s << quint16(1);
    s << quint16(1);
    s << quint32(sampleRate);
    s << quint32(sampleRate * sizeof(qint16));
    s << quint16(sizeof(qint16));
    s << quint16(16);
    f.write("data", 4);
    s << dataSize;

    for (short sample : samples) {
        s << static_cast<qint16>(sample);
    }

    return true;
}

