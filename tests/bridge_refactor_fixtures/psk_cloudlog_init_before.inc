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

