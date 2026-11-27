    // DXCC lookup (cty.dat)
    m_dxccLookup = new DxccLookup();
    m_callsignIntelligence = new CallsignIntelligenceService(this);
    if (m_mapIntelligenceService) {
        m_callsignIntelligence->setOfflineMode(m_mapIntelligenceService->offlineMode());
    }
    m_callsignIntelligence->setDxccLookup(m_dxccLookup);
    m_callsignIntelligence->setOperatorCallsign(m_callsign);
    m_callsignIntelligence->setQrzApiKey(m_qrzLogbookApiKey);
    m_callsignIntelligence->setLotwPassword(getSetting(QStringLiteral("Lotw_pwd"), QString()).toString());
    connect(this, &DecodiumBridge::callsignChanged, this, [this]() {
        if (m_callsignIntelligence) {
            m_callsignIntelligence->setOperatorCallsign(m_callsign);
            m_callsignIntelligence->setLotwPassword(getSetting(QStringLiteral("Lotw_pwd"), QString()).toString());
        }
    });
    connect(this, &DecodiumBridge::dxCallChanged, this, [this]() {
        if (m_callsignIntelligence) {
            m_callsignIntelligence->notifyQsoStarted(m_dxCall);
        }
    });
    connect(m_callsignIntelligence, &CallsignIntelligenceService::enrichmentReady,
            this, [this](const QString& call, const QVariantMap& fields) {
        const QString active = Radio::base_callsign(m_dxCall).trimmed().toUpper();
        const QString incoming = Radio::base_callsign(call).trimmed().toUpper();
        if (active.isEmpty() || active != incoming) {
            return;
        }
        if (m_dxGrid.trimmed().isEmpty()) {
            const QString grid = fields.value(QStringLiteral("grid")).toString().trimmed().toUpper();
            if (!grid.isEmpty()) {
                setDxGrid(grid);
            }
        }
        if (m_nextLogName.trimmed().isEmpty()) {
            m_nextLogName = fields.value(QStringLiteral("name")).toString().trimmed();
        }
        if (m_nextLogQth.trimmed().isEmpty()) {
            m_nextLogQth = fields.value(QStringLiteral("qth")).toString().trimmed();
        }
    });
    connect(m_callsignIntelligence, &CallsignIntelligenceService::confirmedAdifDownloaded,
            this, [this](const QString& provider, const QString& path) {
                importConfirmedAdifIntoLogbookAsync(provider, path);
            });
    connect(m_callsignIntelligence, &CallsignIntelligenceService::externalLookupRequested,
            this, [this](const QString& url) {
                openExternalUrl(url);
            });
    bridgeLog(QStringLiteral("DXCC: load deferred until Main.qml ready"));
