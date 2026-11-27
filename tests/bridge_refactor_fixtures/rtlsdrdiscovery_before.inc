void DecodiumBridge::refreshRtlSdrDevices()
{
    if (m_rtlSdrDiscoveryPending) {
        return;
    }
    m_rtlSdrDiscoveryPending = true;
    m_rtlSdrStatus = QStringLiteral("RTL-SDR: searching for USB receivers…");
    emit rtlSdrStatusChanged();

    using RtlSdrDiscoveryResult = QPair<QStringList, QString>;
    auto *watcher = new QFutureWatcher<RtlSdrDiscoveryResult>(this);
    connect(watcher, &QFutureWatcher<RtlSdrDiscoveryResult>::finished,
            this, [this, watcher]() {
        const RtlSdrDiscoveryResult result = watcher->result();
        watcher->deleteLater();
        m_rtlSdrDiscoveryPending = false;
        if (m_rtlSdrDevices != result.first) {
            m_rtlSdrDevices = result.first;
            emit rtlSdrDevicesChanged();
        }
        const QString status = result.second.isEmpty()
            ? (m_rtlSdrDevices.isEmpty()
                   ? QStringLiteral("RTL-SDR: no receiver detected")
                   : QStringLiteral("RTL-SDR: %1 receiver(s) detected").arg(m_rtlSdrDevices.size()))
            : result.second;
        if (m_rtlSdrStatus != status) {
            m_rtlSdrStatus = status;
            emit rtlSdrStatusChanged();
        }
    });
    watcher->setFuture(QtConcurrent::run([]() -> RtlSdrDiscoveryResult {
        QString error;
        return qMakePair(RtlSdrInput::enumerateDevices(&error), error);
    }));
}

