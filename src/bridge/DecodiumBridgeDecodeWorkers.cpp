#include "DecodiumBridge.h"
#include "Detector/FT8DecodeWorker.hpp"
#include "Detector/FT2DecodeWorker.hpp"
#include "Detector/FT4DecodeWorker.hpp"
#include "Detector/Q65DecodeWorker.hpp"
#include "Detector/MSK144DecodeWorker.hpp"
#include "Detector/WSPRDecodeWorker.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "Detector/FST4DecodeWorker.hpp"

#include <QElapsedTimer>
#include <QThread>

QString decodiumBridgeCanonicalApplicationDecodeMode(QString mode);
void decodiumBridgeWorkerLog(const QString& message);

void DecodiumBridge::invalidateDecodeSession()
{
    m_retiredDecodeSerial = m_decodeSerial;
    ++m_decodeSessionId;
    if (m_q65Worker) {
        m_q65Worker->retireThrough(m_retiredDecodeSerial);
    }
    if (m_fst4Worker) {
        m_fst4Worker->retireThrough(m_retiredDecodeSerial);
    }
    if (m_wsprWorker) {
        m_wsprWorker->retireThrough(m_retiredDecodeSerial);
    }
}

void DecodiumBridge::ensureDecodeWorkerForMode(const QString& mode)
{
    if (m_shuttingDown) {
        return;
    }

    QString normalizedMode = decodiumBridgeCanonicalApplicationDecodeMode(mode);
    if (normalizedMode.isEmpty()) {
        normalizedMode = mode.trimmed();
    }
    QString const upperMode = normalizedMode.toUpper();
    QElapsedTimer elapsed;
    elapsed.start();
    QString workerName;

    if (upperMode == QStringLiteral("FT8") && !m_ft8Worker) {
        m_workerThread = new QThread(this);
        m_workerThread->setObjectName(QStringLiteral("FT8DecodeWorker"));
#if defined(Q_OS_LINUX)
        m_workerThread->setStackSize(16 * 1024 * 1024);
#else
        m_workerThread->setStackSize(8 * 1024 * 1024);
#endif
        m_ft8Worker = new decodium::ft8::FT8DecodeWorker();
        m_ft8Worker->moveToThread(m_workerThread);
        connect(m_ft8Worker, &decodium::ft8::FT8DecodeWorker::decodeReady,
                this, &DecodiumBridge::onFt8DecodeReady);
        connect(m_workerThread, &QThread::finished, m_ft8Worker, &QObject::deleteLater);
        m_workerThread->start(QThread::LowPriority);
        workerName = QStringLiteral("FT8");
    } else if (upperMode == QStringLiteral("FT2") && !m_ft2Worker) {
        m_workerThreadFt2 = new QThread(this);
        m_workerThreadFt2->setObjectName(QStringLiteral("FT2DecodeWorker"));
        m_workerThreadFt2->setStackSize(8 * 1024 * 1024);
        m_ft2Worker = new decodium::ft2::FT2DecodeWorker();
        m_ft2Worker->setDecodeEnabled(true);
        m_ft2Worker->moveToThread(m_workerThreadFt2);
        connect(m_ft2Worker, &decodium::ft2::FT2DecodeWorker::decodeReady,
                this, &DecodiumBridge::onFt2DecodeReady);
        connect(m_ft2Worker, &decodium::ft2::FT2DecodeWorker::asyncDecodeReady,
                this, &DecodiumBridge::onFt2AsyncDecodeReady, Qt::QueuedConnection);
        connect(m_workerThreadFt2, &QThread::finished, m_ft2Worker, &QObject::deleteLater);
        m_workerThreadFt2->start(QThread::LowPriority);
        workerName = QStringLiteral("FT2");
    } else if (upperMode == QStringLiteral("FT4") && !m_ft4Worker) {
        m_workerThreadFt4 = new QThread(this);
        m_workerThreadFt4->setObjectName(QStringLiteral("FT4DecodeWorker"));
#if defined(Q_OS_LINUX)
        m_workerThreadFt4->setStackSize(16 * 1024 * 1024);
#else
        m_workerThreadFt4->setStackSize(8 * 1024 * 1024);
#endif
        m_ft4Worker = new decodium::ft4::FT4DecodeWorker();
        m_ft4Worker->moveToThread(m_workerThreadFt4);
        connect(m_ft4Worker, &decodium::ft4::FT4DecodeWorker::decodeReady,
                this, &DecodiumBridge::onFt4DecodeReady);
        connect(m_workerThreadFt4, &QThread::finished, m_ft4Worker, &QObject::deleteLater);
        m_workerThreadFt4->start(QThread::LowPriority);
        workerName = QStringLiteral("FT4");
    } else if (upperMode.startsWith(QStringLiteral("Q65")) && !m_q65Worker) {
        m_workerThreadQ65 = new QThread(this);
        m_workerThreadQ65->setObjectName(QStringLiteral("Q65DecodeWorker"));
        m_q65Worker = new decodium::q65::Q65DecodeWorker();
        m_q65Worker->moveToThread(m_workerThreadQ65);
        connect(m_q65Worker, &decodium::q65::Q65DecodeWorker::decodeReady,
                this, &DecodiumBridge::onQ65DecodeReady);
        connect(m_q65Worker, &decodium::q65::Q65DecodeWorker::coherentCount,
                this, &DecodiumBridge::notifyCoherentAveraging);
        connect(m_workerThreadQ65, &QThread::finished, m_q65Worker, &QObject::deleteLater);
        m_workerThreadQ65->start(QThread::LowPriority);
        workerName = QStringLiteral("Q65");
    } else if (upperMode == QStringLiteral("MSK144") && !m_mskWorker) {
        m_workerThreadMsk = new QThread(this);
        m_workerThreadMsk->setObjectName(QStringLiteral("MSK144DecodeWorker"));
        m_mskWorker = new decodium::msk144::MSK144DecodeWorker();
        m_mskWorker->moveToThread(m_workerThreadMsk);
        connect(m_mskWorker, &decodium::msk144::MSK144DecodeWorker::decodeReady,
                this, &DecodiumBridge::onMsk144DecodeReady);
        connect(m_workerThreadMsk, &QThread::finished, m_mskWorker, &QObject::deleteLater);
        m_workerThreadMsk->start(QThread::LowPriority);
        workerName = QStringLiteral("MSK144");
    } else if (upperMode == QStringLiteral("WSPR") && !m_wsprWorker) {
        m_workerThreadWspr = new QThread(this);
        m_workerThreadWspr->setObjectName(QStringLiteral("WSPRDecodeWorker"));
#if defined(Q_OS_LINUX)
        m_workerThreadWspr->setStackSize(32 * 1024 * 1024);
#else
        m_workerThreadWspr->setStackSize(16 * 1024 * 1024);
#endif
        m_wsprWorker = new decodium::wspr::WSPRDecodeWorker();
        m_wsprWorker->moveToThread(m_workerThreadWspr);
        connect(m_wsprWorker, &decodium::wspr::WSPRDecodeWorker::decodeReady,
                this, &DecodiumBridge::onWsprDecodeReady);
        connect(m_workerThreadWspr, &QThread::finished, m_wsprWorker, &QObject::deleteLater);
        m_workerThreadWspr->start(QThread::LowPriority);
        workerName = QStringLiteral("WSPR");
    } else if ((upperMode == QStringLiteral("JT65")
                || upperMode == QStringLiteral("JT9")
                || upperMode == QStringLiteral("JT4"))
               && !m_legacyJtWorker) {
        m_workerThreadLegacyJt = new QThread(this);
        m_workerThreadLegacyJt->setObjectName(QStringLiteral("LegacyJtDecodeWorker"));
        m_legacyJtWorker = new decodium::legacyjt::LegacyJtDecodeWorker();
        m_legacyJtWorker->moveToThread(m_workerThreadLegacyJt);
        connect(m_legacyJtWorker, &decodium::legacyjt::LegacyJtDecodeWorker::decodeReady,
                this, &DecodiumBridge::onLegacyJtDecodeReady);
        connect(m_workerThreadLegacyJt, &QThread::finished,
                m_legacyJtWorker, &QObject::deleteLater);
        m_workerThreadLegacyJt->start(QThread::LowPriority);
        workerName = QStringLiteral("LegacyJT");
    } else if (upperMode.startsWith(QStringLiteral("FST4")) && !m_fst4Worker) {
        m_workerThreadFst4 = new QThread(this);
        m_workerThreadFst4->setObjectName(QStringLiteral("FST4DecodeWorker"));
        m_fst4Worker = new decodium::fst4::FST4DecodeWorker();
        m_fst4Worker->moveToThread(m_workerThreadFst4);
        connect(m_fst4Worker, &decodium::fst4::FST4DecodeWorker::decodeReady,
                this, &DecodiumBridge::onFst4DecodeReady);
        connect(m_workerThreadFst4, &QThread::finished, m_fst4Worker, &QObject::deleteLater);
        m_workerThreadFst4->start(QThread::LowPriority);
        workerName = QStringLiteral("FST4");
    }

    if (!workerName.isEmpty()) {
        decodiumBridgeWorkerLog(QStringLiteral("decoder worker initialized lazily: requested=%1 worker=%2 elapsed_ms=%3")
                      .arg(normalizedMode, workerName)
                      .arg(elapsed.elapsed()));
    }
}

