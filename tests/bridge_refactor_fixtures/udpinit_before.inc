void DecodiumBridge::initUdpMessageClient(bool forceForBridgeCommittedQso)
{
    // Read UDP settings from the legacy INI (canonical source)
    QString serverName = getSetting(QStringLiteral("UDPServer"), QStringLiteral("127.0.0.1")).toString();
    quint16 serverPort = udpPortFromSettingValue(getSetting(QStringLiteral("UDPServerPort"), 2237), 2237);
    quint16 listenPort = udpPortFromSettingValue(getSetting(QStringLiteral("UDPListenPort"), 0), 0, true);
    int ttl = qBound(0, getSetting(QStringLiteral("UDPTTL"), 1).toInt(), 255);
    QStringList interfaces;
    QVariant ifaceVal = readSettingFromLegacyIni(QStringLiteral("UDPInterface"));
    if (ifaceVal.isValid()) interfaces = ifaceVal.toStringList();
    auto const boolText = [] (bool value) {
        return value ? QStringLiteral("enabled") : QStringLiteral("disabled");
    };
    auto const interfaceText = [] (QStringList const& names) {
        return names.isEmpty() ? QStringLiteral("all") : names.join(QStringLiteral(","));
    };
    auto const trafficText = [&boolText] (bool decode, bool status, bool qso, bool wspr) {
        return QStringLiteral("Decode:%1 Status:%2 QSO:%3 WSPR:%4")
            .arg(boolText(decode), boolText(status), boolText(qso), boolText(wspr));
    };
    bool const acceptUdpRequests = getSetting(QStringLiteral("AcceptUDPRequests"), true).toBool();
    bool const primaryDecodeEnabled = udpTrafficEnabled(QStringLiteral("UDPPrimary"), QStringLiteral("Decode"));
    bool const primaryStatusEnabled = udpTrafficEnabled(QStringLiteral("UDPPrimary"), QStringLiteral("Status"));
    bool const primaryQsoEnabled = udpTrafficEnabled(QStringLiteral("UDPPrimary"), QStringLiteral("Qso"));
    bool const primaryWsprEnabled = udpTrafficEnabled(QStringLiteral("UDPPrimary"), QStringLiteral("Wspr"));
    // "QSO logged" is now the sole user-facing control for logged-QSO
    // traffic. Older profiles can still contain LoggedAdifEnabled, but it
    // must never suppress the WSJT-X LoggedADIF record once QSO logging is
    // enabled: LogHX and other compatible loggers require that record.
    bool const primaryAdifEnabled = primaryQsoEnabled;
    bool const secondaryEnabled = getSetting(QStringLiteral("UDPSecondaryEnabled"), true).toBool();
    QString const secondaryServerName =
        getSetting(QStringLiteral("UDPSecondaryServer"), serverName).toString().trimmed();
    quint16 const secondaryPort = udpPortFromSettingValue(
        getSetting(QStringLiteral("UDPSecondaryServerPort"), 2239), 2239);
    int const secondaryTtl = qBound(0, getSetting(QStringLiteral("UDPSecondaryTTL"), ttl).toInt(), 255);
    QStringList secondaryInterfaces;
    QString const secondaryInterface =
        getSetting(QStringLiteral("UDPSecondaryInterface"), QString()).toString().trimmed();
    if (!secondaryInterface.isEmpty()) {
        secondaryInterfaces << secondaryInterface;
    }
    bool const secondaryDecodeEnabled = udpTrafficEnabled(QStringLiteral("UDPSecondary"), QStringLiteral("Decode"));
    bool const secondaryStatusEnabled = udpTrafficEnabled(QStringLiteral("UDPSecondary"), QStringLiteral("Status"));
    bool const secondaryQsoEnabled = udpTrafficEnabled(QStringLiteral("UDPSecondary"), QStringLiteral("Qso"));
    bool const secondaryWsprEnabled = udpTrafficEnabled(QStringLiteral("UDPSecondary"), QStringLiteral("Wspr"));
    bool const secondaryAdifEnabled = secondaryQsoEnabled;
    bool const secondaryRealtimeEnabled = secondaryEnabled
        && (secondaryDecodeEnabled || secondaryStatusEnabled || secondaryWsprEnabled);
    bool const tertiaryEnabled = getSetting(QStringLiteral("UDPTertiaryEnabled"), false).toBool();
    QString const tertiaryServerName =
        getSetting(QStringLiteral("UDPTertiaryServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
    quint16 const tertiaryPort = udpPortFromSettingValue(
        getSetting(QStringLiteral("UDPTertiaryServerPort"), 2237), 2237);
    int const tertiaryTtl = qBound(0, getSetting(QStringLiteral("UDPTertiaryTTL"), ttl).toInt(), 255);
    QStringList tertiaryInterfaces;
    QString const tertiaryInterface =
        getSetting(QStringLiteral("UDPTertiaryInterface"), QString()).toString().trimmed();
    if (!tertiaryInterface.isEmpty()) {
        tertiaryInterfaces << tertiaryInterface;
    }
    bool const tertiaryDecodeEnabled = udpTrafficEnabled(QStringLiteral("UDPTertiary"), QStringLiteral("Decode"));
    bool const tertiaryStatusEnabled = udpTrafficEnabled(QStringLiteral("UDPTertiary"), QStringLiteral("Status"));
    bool const tertiaryQsoEnabled = udpTrafficEnabled(QStringLiteral("UDPTertiary"), QStringLiteral("Qso"));
    bool const tertiaryWsprEnabled = udpTrafficEnabled(QStringLiteral("UDPTertiary"), QStringLiteral("Wspr"));
    bool const tertiaryAdifEnabled = tertiaryQsoEnabled;
    bool const tertiaryRealtimeEnabled = tertiaryEnabled
        && (tertiaryDecodeEnabled || tertiaryStatusEnabled || tertiaryWsprEnabled);
    bool const adifTcpEnabled = getSetting(QStringLiteral("ADIFTcpEnabled"), false).toBool();
    QString const adifTcpServer = getSetting(QStringLiteral("ADIFTcpServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
    quint16 const adifTcpPort = udpPortFromSettingValue(getSetting(QStringLiteral("ADIFTcpPort"), 52001), 52001);
    bool const n1mmEnabled = getSetting(QStringLiteral("BroadcastToN1MM"), false).toBool();
    QString const n1mmServer = getSetting(QStringLiteral("N1MMServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
    quint16 const n1mmPort = udpPortFromSettingValue(getSetting(QStringLiteral("N1MMServerPort"), 2333), 2333);
    // 1.0.538 iu8lmc - un identificativo per destinazione. I programmi
    // locali (JTAlert, GridTracker) filtrano su "WSJTX" e devono continuare
    // a trovarlo sulla porta primaria; i collettori remoti leggevano invece
    // quel nome insieme alla nostra versione 1.0.x, la confrontavano con il
    // WSJT-X 2.7.x e scartavano tutto con "OLD software version". Con un
    // campo per destinazione si servono entrambi senza raddoppiare i
    // pacchetti.
    QString const clientId = decodium::network::normalizedUdpClientId(
        getSetting(QStringLiteral("UDPClientId"), QStringLiteral("Decodium")).toString(),
        QStringLiteral("Decodium"));
    QString const secondaryClientId = decodium::network::normalizedUdpClientId(
        getSetting(QStringLiteral("UDPSecondaryClientId"), QStringLiteral("Decodium")).toString());
    QString const tertiaryClientId = decodium::network::normalizedUdpClientId(
        getSetting(QStringLiteral("UDPTertiaryClientId"), QStringLiteral("Decodium")).toString());

    bridgeLog(QStringLiteral("Reporting config: UDP primary server=%1:%2 listen=%3 clientId=%4 interface=%5 ttl=%6 acceptRequests=%7 filters=[%8] loggedADIF=%9 (coupled-to-QSO)")
                  .arg(serverName.trimmed(), QString::number(serverPort), QString::number(listenPort),
                       clientId, interfaceText(interfaces), QString::number(ttl),
                       boolText(acceptUdpRequests),
                       trafficText(primaryDecodeEnabled, primaryStatusEnabled, primaryQsoEnabled, primaryWsprEnabled),
                       boolText(primaryAdifEnabled)));
    bridgeLog(QStringLiteral("Reporting config: UDP secondary %1 server=%2:%3 listen=%4 clientId=%5 interface=%6 ttl=%7 filters=[%8] loggedADIF=%9 (coupled-to-QSO)")
                  .arg(boolText(secondaryEnabled), secondaryServerName, QString::number(secondaryPort),
                       secondaryRealtimeEnabled ? QStringLiteral("ephemeral") : QStringLiteral("QSO-only"),
                       secondaryClientId, interfaceText(secondaryInterfaces), QString::number(secondaryTtl),
                       trafficText(secondaryDecodeEnabled, secondaryStatusEnabled, secondaryQsoEnabled, secondaryWsprEnabled),
                       boolText(secondaryAdifEnabled)));
    bridgeLog(QStringLiteral("Reporting config: UDP tertiary %1 server=%2:%3 listen=%4 clientId=%5 interface=%6 ttl=%7 filters=[%8] loggedADIF=%9 (coupled-to-QSO)")
                  .arg(boolText(tertiaryEnabled), tertiaryServerName, QString::number(tertiaryPort),
                       tertiaryRealtimeEnabled ? QStringLiteral("ephemeral") : QStringLiteral("QSO-only"),
                       tertiaryClientId, interfaceText(tertiaryInterfaces), QString::number(tertiaryTtl),
                       trafficText(tertiaryDecodeEnabled, tertiaryStatusEnabled, tertiaryQsoEnabled, tertiaryWsprEnabled),
                       boolText(tertiaryAdifEnabled)));
    bridgeLog(QStringLiteral("Reporting config: ADIF TCP %1 target=%2:%3")
                  .arg(boolText(adifTcpEnabled), adifTcpServer, QString::number(adifTcpPort)));
    bridgeLog(QStringLiteral("Reporting config: N1MM UDP %1 target=%2:%3")
                  .arg(boolText(n1mmEnabled), n1mmServer, QString::number(n1mmPort)));

    // Even in native modes (e.g. RTTY), the embedded backend can still send
    // heartbeats. Keep its wire identity aligned with the visible settings.
    if (m_legacyBackend && legacyBackendAvailable()) {
        m_legacyBackend->refreshUdpReporting(clientId, secondaryClientId, tertiaryClientId);
    }

    if (usingLegacyBackendForTx() && !forceForBridgeCommittedQso) {
        bridgeLog(QStringLiteral("Standalone UDP MessageClient suppressed: legacy TX backend active"));
        shutdownUdpMessageClient();
        return;
    }
    if (usingLegacyBackendForTx()) {
        bridgeLog(QStringLiteral("Standalone UDP MessageClient enabled for bridge-committed legacy QSO"));
    }

    QString const ver = version();
    QString const rev = revision();

    if (!m_udpMessageClient) {
        m_udpMessageClient = new MessageClient(clientId, ver, rev,
                                               serverName, serverPort,
                                               listenPort, interfaces, ttl,
                                               this, QStringLiteral("primary"));

        m_udpMessageClient->enable(acceptUdpRequests);

        connect(m_udpMessageClient, &MessageClient::error, this, [](const QString& msg) {
            bridgeLog("UDP MessageClient error: " + msg);
        });

        // Handle incoming Reply packets from external apps (GridTracker Call
        // Roster, JTAlert, etc.) as the same user action as a decode double-click.
        connect(m_udpMessageClient, &MessageClient::reply, this,
                [this](QTime time, qint32 snr, float /*dt*/, quint32 df,
                       QString const& /*mode*/, QString const& message, bool /*lowConf*/, quint8 /*mods*/) {
            QString const trimmed = message.trimmed();
            if (trimmed.isEmpty()) {
                return;
            }

            QString const timeStr = time.isValid()
                ? time.toString(QStringLiteral("HHmmss"))
                : QDateTime::currentDateTimeUtc().time().toString(QStringLiteral("HHmmss"));

            bridgeLog("UDP reply accepted: " + trimmed + " df=" + QString::number(df));
            processDecodeDoubleClick(trimmed, timeStr, QString::number(snr), static_cast<int>(df));
        });

        connect(m_udpMessageClient, &MessageClient::halt_tx, this, [this](bool autoOnly) {
            if (!autoOnly) {
                bridgeLog("UDP halt_tx received");
                setTxEnabled(false);
            }
        });

        connect(m_udpMessageClient, &MessageClient::clear_decodes, this, [this](quint8 window) {
            bridgeLog(QStringLiteral("UDP clear_decodes received: window=%1").arg(window));
            if (window == 0 || window == 2) {
                clearDecodeList();
            }
            if (window == 1 || window == 2) {
                clearRxDecodes();
            }
        });

        connect(m_udpMessageClient, &MessageClient::replay, this, [this]() {
            QSet<QString> seen;
            int sent = 0;

            auto replayList = [this, &seen, &sent](QVariantList const& entries) {
                for (QVariant const& value : entries) {
                    QVariantMap const entry = value.toMap();
                    QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
                    if (message.isEmpty()) {
                        continue;
                    }

                    QString key = decodeMirrorEntryKey(entry);
                    if (key.isEmpty()) {
                        key = message;
                    }
                    if (seen.contains(key)) {
                        continue;
                    }
                    seen.insert(key);

                    QString const time = entry.value(QStringLiteral("time")).toString().trimmed();
                    QString const db = entry.value(QStringLiteral("db"), QStringLiteral("0")).toString().trimmed();
                    QString dt = entry.value(QStringLiteral("dt"), QStringLiteral("0.0")).toString().trimmed();
                    if (!dt.contains(QLatin1Char('.'))) {
                        dt += QStringLiteral(".0");
                    }
                    QString const freq = entry.value(QStringLiteral("freq"), QString::number(m_rxFrequency)).toString().trimmed();
                    QString const rawLine = QStringLiteral("%1 %2 %3 %4 ~  %5")
                                                .arg(time, db, dt, freq, message);
                    udpSendDecode(false, rawLine, 0);
                    ++sent;
                }
            };

            replayList(m_decodeList);
            replayList(m_rxDecodeList);
            bridgeLog(QStringLiteral("UDP replay received: sent %1 decodes").arg(sent));
        });

        connect(m_udpMessageClient, &MessageClient::free_text, this,
                [this](QString const& message, bool send) {
            QString const text = message.trimmed();
            if (text.isEmpty()) {
                return;
            }

            bridgeLog(QStringLiteral("UDP free_text received: '%1' send=%2")
                          .arg(text, send ? QStringLiteral("true") : QStringLiteral("false")));
            setTx6(text);
            bool const selected = selectCurrentTxIfAllowed(6, QStringLiteral("udp-free-text"));
            if (send && !selected) {
                return;
            }
            if (send) {
                setAutoCqRepeat(false);
                clearManualTxHold(QStringLiteral("udp-free-text"));
                armExplicitManualTxRequest(6, QStringLiteral("UDP free-text"));
                setTxEnabled(true);
            }
        });

        connect(m_udpMessageClient, &MessageClient::setup_tx, this,
                [this](int newTxMsgIdx, QString const& msg, bool /*skipGrid*/,
                       bool /*useRR73*/, QString const& /*check*/, quint32 offset) {
            int const tx = qBound(1, newTxMsgIdx, 6);
            QString const text = msg.trimmed();
            bridgeLog(QStringLiteral("UDP setup_tx received: TX%1 msg='%2' offset=%3")
                          .arg(tx)
                          .arg(text)
                          .arg(offset));

            if (!text.isEmpty()) {
                switch (tx) {
                case 1: setTx1(text); break;
                case 2: setTx2(text); break;
                case 3: setTx3(text); break;
                case 4: setTx4(text); break;
                case 5: setTx5(text); break;
                case 6: setTx6(text); break;
                default: break;
                }
            }
            setCurrentTx(tx);
            if (offset > 0 && offset <= 5000) {
                if (m_holdTxFreq) {
                    bridgeLog(QStringLiteral("UDP setup_tx offset ignored because HOLD is active: %1 Hz")
                                  .arg(offset));
                } else {
                    setTxFrequency(static_cast<int>(offset));
                }
            }
        });

        bridgeLog("UDP MessageClient started: server=" + serverName + ":" + QString::number(serverPort)
                  + " listen=" + QString::number(listenPort));
    }

    // A persistent MessageClient emits protocol heartbeats. Keep it alive only
    // when this destination has at least one real-time category enabled. A
    // QSO-only secondary receives its raw ADIF datagram at commit time instead.
    if (secondaryRealtimeEnabled && !m_udpSecondaryMessageClient) {
        m_udpSecondaryMessageClient = new MessageClient(secondaryClientId, ver, rev,
                                                        secondaryServerName, secondaryPort,
                                                        0, secondaryInterfaces, secondaryTtl,
                                                        this, QStringLiteral("secondary"));
        m_udpSecondaryMessageClient->enable(false);
        connect(m_udpSecondaryMessageClient, &MessageClient::error, this, [](const QString& msg) {
            bridgeLog("Secondary UDP MessageClient error: " + msg);
        });
        bridgeLog("Secondary UDP MessageClient started: server=" + secondaryServerName + ":"
                  + QString::number(secondaryPort) + " listen=0 interface="
                  + interfaceText(secondaryInterfaces) + " ttl="
                  + QString::number(secondaryTtl));
    } else if (!secondaryRealtimeEnabled && m_udpSecondaryMessageClient) {
        bridgeLog(QStringLiteral("Stopping secondary UDP MessageClient; destination has no real-time traffic"));
        m_udpSecondaryMessageClient->disconnect(this);
        m_udpSecondaryMessageClient->deleteLater();
        m_udpSecondaryMessageClient = nullptr;
    }

    if (tertiaryRealtimeEnabled && !m_udpTertiaryMessageClient) {
        m_udpTertiaryMessageClient = new MessageClient(tertiaryClientId, ver, rev,
                                                       tertiaryServerName, tertiaryPort,
                                                       0, tertiaryInterfaces, tertiaryTtl,
                                                       this, QStringLiteral("tertiary"));
        m_udpTertiaryMessageClient->enable(false);

        connect(m_udpTertiaryMessageClient, &MessageClient::error, this, [](const QString& msg) {
            bridgeLog("Tertiary UDP MessageClient error: " + msg);
        });

        bridgeLog("Tertiary UDP MessageClient started: server=" + tertiaryServerName + ":"
                  + QString::number(tertiaryPort) + " listen=0 interface="
                  + interfaceText(tertiaryInterfaces) + " ttl=" + QString::number(tertiaryTtl));
    } else if (!tertiaryRealtimeEnabled && m_udpTertiaryMessageClient) {
        bridgeLog(QStringLiteral("Stopping tertiary UDP MessageClient; destination has no real-time traffic"));
        m_udpTertiaryMessageClient->disconnect(this);
        m_udpTertiaryMessageClient->deleteLater();
        m_udpTertiaryMessageClient = nullptr;
    }

    // Automatically send status updates when key properties change
    connect(this, &DecodiumBridge::frequencyChanged,    this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::modeChanged,         this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::monitoringChanged,   this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::transmittingChanged, this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::decodingChanged,     this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::txEnabledChanged,    this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::callsignChanged,     this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::gridChanged,         this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::specialOperationActivityChanged, this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);

    // Send initial status
    udpSendStatus();
    m_udpNoTargetWarningSent = false;
}

