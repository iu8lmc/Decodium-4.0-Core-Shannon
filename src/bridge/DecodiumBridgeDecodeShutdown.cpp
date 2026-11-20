#include "DecodiumBridge.h"
#include "Q65DopplerTracker.h"
#include "controllers/ActiveStationsModel.hpp"
#include "Detector/FT8DecodeWorker.hpp"
#include "Detector/FT2DecodeWorker.hpp"
#include "Detector/FT4DecodeWorker.hpp"
#include "Detector/Q65DecodeWorker.hpp"
#include "Detector/MSK144DecodeWorker.hpp"
#include "Detector/WSPRDecodeWorker.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "Detector/FST4DecodeWorker.hpp"
#include <QCoreApplication>
#include <QEvent>
#include <QTimer>

bool DecodiumBridge::shouldIgnoreDecodeCallbacks() const
{
    if (m_shuttingDown || QCoreApplication::closingDown()) {
        return true;
    }

    auto *app = QCoreApplication::instance();
    return app && app->property("decodiumShuttingDown").toBool();
}

void DecodiumBridge::beginDecodeCallbackShutdown()
{
    if (!m_shuttingDown && m_q65Doppler && m_q65Doppler->applied()
        && m_catBackend == QStringLiteral("hamlib") && m_hamlibCat && m_hamlibCat->connected()) {
        // Stop keying before restoring either VFO; each following Hamlib state
        // retains PTT=OFF. Request nominal-dial restoration before CAT teardown.
        m_hamlibCat->setRigPtt(false);
        m_hamlibCat->setRigFrequency(applyFrequencyCalibration(m_frequency));
        m_hamlibCat->setRigTxFrequency(0);
        m_q65Doppler->disconnected();
    }
    m_shuttingDown = true;
    m_asyncDecodePending = false;
    if (m_asyncDecodeTimer) {
        m_asyncDecodeTimer->stop();
    }

    if (m_ft8Worker) {
        m_ft8Worker->beginShutdown();
    }
    if (m_ft2Worker) {
        m_ft2Worker->beginShutdown();
    }
    if (m_ft4Worker) {
        m_ft4Worker->beginShutdown();
    }
    if (m_fst4Worker) {
        m_fst4Worker->beginShutdown();
    }

    auto disconnectWorker = [this](QObject *worker) {
        if (worker) {
            QObject::disconnect(worker, nullptr, this, nullptr);
            QCoreApplication::removePostedEvents(worker, QEvent::MetaCall);
        }
    };

    disconnectWorker(m_ft8Worker);
    disconnectWorker(m_ft2Worker);
    disconnectWorker(m_ft4Worker);
    disconnectWorker(m_q65Worker);
    disconnectWorker(m_mskWorker);
    disconnectWorker(m_wsprWorker);
    disconnectWorker(m_legacyJtWorker);
    disconnectWorker(m_fst4Worker);

    // Drop already queued cross-thread decode deliveries before legacy teardown pumps the event loop.
    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
    m_activeStations = nullptr;
}

