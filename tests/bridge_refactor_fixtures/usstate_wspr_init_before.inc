    m_usStateData = new UsStateDataManager(this);
    connect(m_usStateData, &UsStateDataManager::statusMessage, this, [this](const QString& msg) {
        bridgeLog(msg);
        emit statusMessage(msg);
    });
    connect(m_usStateData, &UsStateDataManager::updatingChanged, this, [this]() {
        emit usStateDataChanged();
    });
    connect(m_usStateData, &UsStateDataManager::dataChanged, this, [this]() {
        emit usStateDataChanged();
        refreshDecodeListDxcc();
    });
    connect(m_wsprUploader, &DecodiumWsprUploader::uploadStatus,
            this, [this](const QString& msg) {
                QString const status = QStringLiteral("WSPR: ") + msg;
                bridgeLog(status);
                emit statusMessage(status);
            });

