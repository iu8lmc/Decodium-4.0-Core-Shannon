void DecodiumBridge::qmlDebugLog(QString const& msg) const
{
    bridgeLog(QStringLiteral("[QML] %1").arg(msg));
}

QString DecodiumBridge::callsign() const { return m_callsign; }

QString DecodiumBridge::mode() const { return m_mode; }

bool DecodiumBridge::transmitting() const { return m_transmitting; }

bool DecodiumBridge::decoding() const { return m_decoding; }

QVariantList DecodiumBridge::decodeList() const { return m_decodeList; }

QVariantList DecodiumBridge::rxDecodeList() const { return m_rxDecodeList; }

int DecodiumBridge::periodProgress() const { return m_periodProgress; }

QString DecodiumBridge::utcTime() const { return m_utcTime; }

void DecodiumBridge::openCatSettings()
{
    emit catSettingsRequested();
}

void DecodiumBridge::onUtcTimer()
{
    m_utcTime = QDateTime::currentDateTimeUtc().toString("hh:mm:ss");
    emit utcTimeChanged();
}

