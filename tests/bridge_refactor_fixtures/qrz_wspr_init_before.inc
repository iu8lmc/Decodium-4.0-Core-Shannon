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

