QVariantList DecodiumBridge::runSelfCheck()
{
    DecodiumSelfCheck::State s;
    s.appVersion      = QStringLiteral(FORK_RELEASE_VERSION);
    s.latestVersion   = m_latestVersion;
    s.updateAvailable = m_updateAvailable;
    s.mode            = mode();
    s.callsign        = m_callsign;
    s.monitoring      = m_monitoring;
    s.ntpEnabled      = ntpEnabled();
    s.ntpSynced       = ntpSynced();
    s.ntpOffsetMs     = m_ntpOffsetMs;
    s.mamMultiStream  = mamMultiStream();
    s.ftThreads       = m_ftThreads;
    s.cpuCores        = QThread::idealThreadCount();
    s.totalDecodesReceived = totalDecodesReceived();
    s.monitoringSeconds = (m_monitoring && m_monitoringSince.isValid())
                              ? m_monitoringSince.secsTo(QDateTime::currentDateTimeUtc())
                              : 0;
    return DecodiumSelfCheck::toVariantList(DecodiumSelfCheck::run(s));
}
