QVariantMap DecodiumBridge::ft2LinkEmailGatewayPasswordStatus(const QVariantMap& config) const
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("backendAvailable"), secure_settings::default_backend().available());
    QString const account = ft2LinkSmtpSecretKey(config);
    result.insert(QStringLiteral("account"), account);
    auto const lookup = secure_settings::default_backend().lookup(
        secure_settings::service(m_callsign.trimmed().toUpper()), account);
    result.insert(QStringLiteral("found"), lookup.found);
    result.insert(QStringLiteral("error"), lookup.error);
    return result;
}

QVariantMap DecodiumBridge::setFt2LinkEmailGatewayPassword(const QVariantMap& config,
                                                           const QString& password)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    QString const account = ft2LinkSmtpSecretKey(config);
    result.insert(QStringLiteral("account"), account);
    if (password.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Missing SMTP password"));
        return result;
    }
    auto const& backend = secure_settings::default_backend();
    result.insert(QStringLiteral("backendAvailable"), backend.available());
    if (!backend.available()) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("Secure password backend unavailable"));
        return result;
    }
    QString error;
    bool const ok = backend.store(secure_settings::service(m_callsign.trimmed().toUpper()),
                                  account,
                                  password,
                                  &error);
    result.insert(QStringLiteral("ok"), ok);
    result.insert(QStringLiteral("error"), error);
    if (ok) {
        emit statusMessage(QStringLiteral("FT2-Link SMTP password saved securely"));
    } else if (!error.isEmpty()) {
        emit errorMessage(QStringLiteral("FT2-Link SMTP password save failed: %1").arg(error));
    }
    return result;
}

QVariantMap DecodiumBridge::clearFt2LinkEmailGatewayPassword(const QVariantMap& config)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    QString const account = ft2LinkSmtpSecretKey(config);
    result.insert(QStringLiteral("account"), account);
    auto const& backend = secure_settings::default_backend();
    result.insert(QStringLiteral("backendAvailable"), backend.available());
    if (!backend.available()) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("Secure password backend unavailable"));
        return result;
    }
    QString error;
    bool const ok = backend.remove(secure_settings::service(m_callsign.trimmed().toUpper()),
                                   account,
                                   &error);
    result.insert(QStringLiteral("ok"), ok);
    result.insert(QStringLiteral("error"), error);
    if (ok) {
        emit statusMessage(QStringLiteral("FT2-Link SMTP password cleared"));
    } else if (!error.isEmpty()) {
        emit errorMessage(QStringLiteral("FT2-Link SMTP password clear failed: %1").arg(error));
    }
    return result;
}

QVariantMap DecodiumBridge::testFt2LinkEmailGateway(const QVariantMap& config)
{
    QVariantMap testConfig = config;
    testConfig.insert(QStringLiteral("testOnly"), true);
    if (!testConfig.contains(QStringLiteral("toEmail"))) {
        testConfig.insert(QStringLiteral("toEmail"),
                          ft2LinkSmtpConfigString(config, QStringLiteral("fromEmail"),
                                                  ft2LinkSmtpConfigString(config, QStringLiteral("username"))));
    }
    return sendFt2LinkGatewayEmail(0u, testConfig, QStringLiteral("FT2-Link SMTP test"));
}

