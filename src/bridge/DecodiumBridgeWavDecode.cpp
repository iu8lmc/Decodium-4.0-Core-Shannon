#include "DecodiumBridge.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QTimer>
#include <cstdint>
#include <cstring>

void DecodiumBridge::openWavForDecode(const QString& path)
{
    if (path.isEmpty() || !QFile::exists(path)) {
        emit errorMessage("File WAV non trovato: " + path);
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit errorMessage("Impossibile aprire: " + path);
        return;
    }

    QByteArray data = f.readAll();
    f.close();

    // Header RIFF/WAV minimo: 44 byte
    if (data.size() < 44) {
        emit errorMessage("File WAV troppo corto: " + path);
        return;
    }

    const char* hdr = data.constData();

    // Verifica firma RIFF
    if (qstrncmp(hdr, "RIFF", 4) != 0 || qstrncmp(hdr + 8, "WAVE", 4) != 0) {
        emit errorMessage("Formato file non riconosciuto (non RIFF/WAVE): " + path);
        return;
    }

    // Leggi parametri dal fmt chunk (offset standard per RIFF lineare)
    uint16_t channels      = *reinterpret_cast<const uint16_t*>(hdr + 22);
    uint32_t sampleRate    = *reinterpret_cast<const uint32_t*>(hdr + 24);
    uint16_t bitsPerSample = *reinterpret_cast<const uint16_t*>(hdr + 34);

    if (channels == 0 || sampleRate == 0 || bitsPerSample == 0) {
        emit errorMessage("Parametri WAV non validi in: " + path);
        return;
    }

    // Scansiona i chunk per trovare 'data'
    int dataOffset = -1;
    int dataSize   = 0;
    // 1.0.376 (sec #13): scan a 64-bit + clamp al buffer; un chunkSize uint32 enorme
    // castato a int rendeva pos negativo -> OOB read su WAV malformato.
    qint64 pos = 12; // dopo RIFF header (4 "RIFF" + 4 size + 4 "WAVE")
    while (pos + 8 <= static_cast<qint64>(data.size())) {
        const char* chunkId   = hdr + pos;
        uint32_t    chunkSize = *reinterpret_cast<const uint32_t*>(hdr + pos + 4);
        if (qstrncmp(chunkId, "data", 4) == 0) {
            dataOffset = static_cast<int>(pos + 8);
            dataSize   = static_cast<int>(qMin<qint64>(chunkSize,
                                static_cast<qint64>(data.size()) - (pos + 8)));
            break;
        }
        pos += 8 + static_cast<qint64>(chunkSize);
        if (chunkSize & 1u) ++pos; // padding byte se chunkSize dispari
    }

    if (dataOffset < 0 || dataOffset >= data.size()) {
        emit errorMessage("Chunk 'data' non trovato nel WAV: " + path);
        return;
    }

    // Limita dataSize al contenuto effettivo del file
    dataSize = qMin(dataSize, data.size() - dataOffset);

    // Converti in QVector<short> mono a 12000 Hz
    QVector<short> samples;
    if (bitsPerSample == 16) {
        const int16_t* pcm = reinterpret_cast<const int16_t*>(data.constData() + dataOffset);
        int nFrames = dataSize / static_cast<int>(channels * sizeof(int16_t));
        // Decimazione: step = sampleRate / 12000 (e.g. 48000 Hz → step 4)
        int step = qMax(1, static_cast<int>(sampleRate / 12000));
        samples.reserve(nFrames / step + 1);
        for (int i = 0; i < nFrames; i += step) {
            samples.append(pcm[i * channels]); // canale 0 (left / mono)
        }
    } else if (bitsPerSample == 8) {
        // PCM unsigned 8-bit → signed 16-bit
        const uint8_t* pcm = reinterpret_cast<const uint8_t*>(data.constData() + dataOffset);
        int nFrames = dataSize / static_cast<int>(channels);
        int step = qMax(1, static_cast<int>(sampleRate / 12000));
        samples.reserve(nFrames / step + 1);
        for (int i = 0; i < nFrames; i += step) {
            int16_t s = static_cast<int16_t>((pcm[i * channels] - 128) << 8);
            samples.append(s);
        }
    } else {
        emit errorMessage(QString("Bit depth %1 non supportato (serve 8 o 16 bit): %2")
                          .arg(bitsPerSample).arg(path));
        return;
    }

    if (samples.isEmpty()) {
        emit errorMessage("Nessun campione audio estratto da: " + path);
        return;
    }

    emit statusMessage(QString("WAV: %1 campioni @ 12000 Hz → decodifica %2 (%3)...")
                       .arg(samples.size()).arg(m_mode).arg(QFileInfo(path).fileName()));

    // Carica i campioni nel buffer audio e avvia la decodifica.
    // QVector<short>::resize() vuole il NUMERO DI ELEMENTI, non byte: in
    // precedenza si moltiplicava per sizeof(short) raddoppiando inutilmente
    // la dimensione e lasciando metà buffer a zero. Lock perché la callback
    // del sink condivide m_audioBuffer (anche se il flusso WAV tipicamente
    // disabilita la cattura, manteniamo l'invariante).
    {
        QMutexLocker locker(&m_audioBufferMutex);
        m_audioBuffer.resize(samples.size());
        memcpy(m_audioBuffer.data(), samples.constData(),
               static_cast<size_t>(samples.size()) * sizeof(short));
    }

    // Avvia feedAudioToDecoder() in modo asincrono per non bloccare la UI
    QMetaObject::invokeMethod(this, [this]() {
        feedAudioToDecoder();
    }, Qt::QueuedConnection);
}

void DecodiumBridge::openWavFolderDecode(const QString& folderPath)
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        emit errorMessage("Cartella non trovata: " + folderPath);
        return;
    }

    QStringList wavFiles = dir.entryList({"*.wav", "*.WAV"}, QDir::Files, QDir::Name);
    if (wavFiles.isEmpty()) {
        emit statusMessage(tr("No WAV file found in: %1").arg(folderPath));
        return;
    }

    emit statusMessage(QString("Batch decode: %1 file WAV in %2...")
                       .arg(wavFiles.size()).arg(folderPath));

    // Decodifica ogni file con 500 ms di distanza tra un file e l'altro,
    // cosi i risultati non si sovrappongono nel buffer.
    int delay = 0;
    for (const QString& fname : wavFiles) {
        QString fullPath = dir.filePath(fname);
        QTimer::singleShot(delay, this, [this, fullPath]() {
            openWavForDecode(fullPath);
        });
        delay += 500;
    }
}

