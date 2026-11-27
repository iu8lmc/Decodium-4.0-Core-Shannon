#if defined(Q_OS_MAC)
    auto const envFlagEnabled = [](const char* name) {
        if (!qEnvironmentVariableIsSet(name)) {
            return false;
        }
        QByteArray const value = qgetenv(name).trimmed().toLower();
        return value.isEmpty()
            || value == "1"
            || value == "true"
            || value == "yes"
            || value == "on";
    };
    bool const legacyTxOverride = envFlagEnabled("DECODIUM_MAC_LEGACY_TX_BACKEND");
    bool const forceStandaloneUdp = envFlagEnabled("DECODIUM_FORCE_STANDALONE_UDP");

    if (forceStandaloneUdp) {
        m_useLegacyTxBackend = false;
        bridgeLog(QStringLiteral("Legacy TX backend disabled by DECODIUM_FORCE_STANDALONE_UDP"));
    } else {
        m_useLegacyTxBackend = true;
        bridgeLog(legacyTxOverride
                  ? QStringLiteral("Legacy TX backend enabled by DECODIUM_MAC_LEGACY_TX_BACKEND")
                  : QStringLiteral("Legacy TX backend enabled by macOS default"));
        // The timer starts during construction, before the QML event loop.
        // If startup itself takes longer than three seconds, its callback can
        // otherwise construct the legacy window ahead of the main QML window.
        const auto initializeLegacy = [this]() {
            if (m_shuttingDown || !m_useLegacyTxBackend) {
                return;
            }
            if (!ensureLegacyBackendAvailable()) {
                m_useLegacyTxBackend = false;
                bridgeLog(QStringLiteral("Legacy RX backend unavailable: enabling the modern decoder fallback"));
                if (m_mainQmlReady || m_startupServicesStarted) {
                    ensureDecodeWorkerForMode(m_mode);
                }
            }
        };
        QTimer::singleShot(3000, this, [this, initializeLegacy]() {
            if (m_shuttingDown) {
                return;
            }
            if (m_mainQmlReady) {
                initializeLegacy();
                return;
            }
            connect(this, &DecodiumBridge::mainQmlReadyForNativeWindowing, this,
                    [this, initializeLegacy]() {
                        QTimer::singleShot(500, this, initializeLegacy);
                    }, Qt::SingleShotConnection);
        });
    }
#endif
    // Su Windows/Linux NON creare il legacy backend all'avvio — viene creato lazy
    // quando l'utente apre Settings/Setup (ensureLegacyBackendAvailable)

