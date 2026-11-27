#include "DecodiumBridge.h"
#include "CallsignIntelligenceService.h"
#include "Network/DecodiumCloudlogLite.h"
#include "Network/DecodiumPskReporterLite.h"
#include "Network/DecodiumQrzLogbookLite.h"
#include "Network/DecodiumWsprUploader.h"

void DecodiumBridge::setCloudlogEnabled(bool v)
{
    if (m_cloudlogEnabled == v) {
        return;
    }
    m_cloudlogEnabled = v;
    if (m_cloudlog) {
        m_cloudlog->setEnabled(v);
    }
    emit cloudlogEnabledChanged();
}

void DecodiumBridge::setCloudlogUrl(const QString& v)
{
    if (m_cloudlogUrl == v) {
        return;
    }
    m_cloudlogUrl = v;
    if (m_cloudlog) {
        m_cloudlog->setApiUrl(v);
    }
    emit cloudlogUrlChanged();
}

void DecodiumBridge::setCloudlogApiKey(const QString& v)
{
    if (m_cloudlogApiKey == v) {
        return;
    }
    m_cloudlogApiKey = v;
    if (m_cloudlog) {
        m_cloudlog->setApiKey(v);
    }
    emit cloudlogApiKeyChanged();
}

void DecodiumBridge::testCloudlogApi()
{
    if (m_cloudlogUrl.isEmpty() || m_cloudlogApiKey.isEmpty()) {
        emit errorMessage("Cloudlog: URL o API key mancante");
        return;
    }
    if (!m_cloudlog) {
        emit errorMessage("Cloudlog: backend non inizializzato");
        return;
    }

    m_cloudlog->setApiUrl(m_cloudlogUrl);
    m_cloudlog->setApiKey(m_cloudlogApiKey);
    m_cloudlog->setStationId(qBound(0, getSetting(QStringLiteral("CloudLogStationID"), 1).toInt(), 999));
    m_cloudlog->testApi();
}

void DecodiumBridge::setQrzLogbookEnabled(bool v)
{
    if (m_qrzLogbookEnabled == v) {
        return;
    }
    m_qrzLogbookEnabled = v;
    if (m_qrzLogbook) {
        m_qrzLogbook->setEnabled(v);
    }
    emit qrzLogbookEnabledChanged();
}

void DecodiumBridge::setQrzLogbookApiKey(const QString& v)
{
    if (m_qrzLogbookApiKey == v) {
        return;
    }
    m_qrzLogbookApiKey = v;
    if (m_qrzLogbook) {
        m_qrzLogbook->setApiKey(v);
    }
    if (m_callsignIntelligence) {
        m_callsignIntelligence->setQrzApiKey(v);
    }
    emit qrzLogbookApiKeyChanged();
}

void DecodiumBridge::setQrzLogbookReplaceDuplicates(bool v)
{
    if (m_qrzLogbookReplaceDuplicates == v) {
        return;
    }
    m_qrzLogbookReplaceDuplicates = v;
    if (m_qrzLogbook) {
        m_qrzLogbook->setReplaceDuplicates(v);
    }
    emit qrzLogbookReplaceDuplicatesChanged();
}

void DecodiumBridge::testQrzLogbookApi()
{
    if (m_qrzLogbookApiKey.trimmed().isEmpty()) {
        emit errorMessage(QStringLiteral("QRZ Logbook: API key mancante"));
        return;
    }
    if (!m_qrzLogbook) {
        emit errorMessage(QStringLiteral("QRZ Logbook: client non inizializzato"));
        return;
    }
    m_qrzLogbook->setApiKey(m_qrzLogbookApiKey);
    emit statusMessage(QStringLiteral("QRZ Logbook: test in corso..."));
    m_qrzLogbook->testApi();
}


void decodiumBridgeDecodeFlowLog(const QString&);
#define bridgeLog decodiumBridgeDecodeFlowLog
void DecodiumBridge::initialisePskAndCloudlog()
{
    // PSK Reporter
    m_pskReporter = new DecodiumPskReporterLite(this);
    connect(m_pskReporter, &DecodiumPskReporterLite::connectedChanged,
            this, [this]() { emit pskReporterConnectedChanged(); });
    connect(m_pskReporter, &DecodiumPskReporterLite::errorOccurred,
            this, [](const QString& msg) { bridgeLog("PSK Reporter: " + msg); });
    connect(m_pskReporter, &DecodiumPskReporterLite::spotsUploaded,
            this, [](int count) {
                bridgeLog(QStringLiteral("PSK Reporter: IPFIX packet written, spots=%1").arg(count));
            });

    // Cloudlog
    m_cloudlog = new DecodiumCloudlogLite(this);
    connect(m_cloudlog, &DecodiumCloudlogLite::qsoLogged,
            this, [this](const QString& c) { emit statusMessage("Cloudlog: QSO loggato " + c); });
    connect(m_cloudlog, &DecodiumCloudlogLite::errorOccurred,
            this, [this](const QString& msg) { emit errorMessage("Cloudlog: " + msg); });
    connect(m_cloudlog, &DecodiumCloudlogLite::adifUploadFinished,
            this, [this](quint32 requestId, const QString& dxCall, bool ok, const QString& detail) {
                noteExternalAdifUploadBackend(requestId, QStringLiteral("Cloudlog"), dxCall, ok, detail);
            }, Qt::QueuedConnection);
    connect(m_cloudlog, &DecodiumCloudlogLite::apiKeyOk,
            this, [this]() { emit statusMessage(QStringLiteral("Cloudlog: API key OK, scrittura disponibile")); });
    connect(m_cloudlog, &DecodiumCloudlogLite::apiKeyInvalid,
            this, [this]() { emit errorMessage(QStringLiteral("Cloudlog: API key non valida o senza permessi di scrittura")); });

}
#undef bridgeLog

void DecodiumBridge::initialiseQrzAndWspr()
{
    m_qrzLogbook = new DecodiumQrzLogbookLite(this);
    connect(m_qrzLogbook, &DecodiumQrzLogbookLite::apiKeyOk,
            this, [this]() { emit statusMessage(QStringLiteral("QRZ Logbook: API key OK")); });
    connect(m_qrzLogbook, &DecodiumQrzLogbookLite::qsoLogged,
            this, [this](const QString& c) { emit statusMessage(QStringLiteral("QRZ Logbook: QSO loggato ") + c); });
    connect(m_qrzLogbook, &DecodiumQrzLogbookLite::errorOccurred,
            this, [this](const QString& msg) { emit errorMessage(QStringLiteral("QRZ Logbook: ") + msg); });
    connect(m_qrzLogbook, &DecodiumQrzLogbookLite::adifUploadFinished,
            this, [this](quint32 requestId, const QString& dxCall, bool ok, const QString& detail) {
                noteExternalAdifUploadBackend(requestId, QStringLiteral("QRZ Logbook"), dxCall, ok, detail);
            }, Qt::QueuedConnection);

    // WSPR uploader
    m_wsprUploader = new DecodiumWsprUploader(this);

}
