void DecodiumBridge::verifyOtp(const QString& callsign, const QString& code, int audioHz)
{
    if (callsign.isEmpty() || code.isEmpty()) return;

    // Usa il timestamp UTC corrente arrotondato al periodo FT8
    QDateTime ts = QDateTime::currentDateTimeUtc();

    auto* verifier = new FoxVerifier(
        "Decodium/3.0",
        nullptr,  // FoxVerifier crea il proprio QNetworkAccessManager se null
        FOXVERIFIER_DEFAULT_BASE_URL,
        callsign,
        ts,
        code,
        static_cast<unsigned int>(audioHz > 0 ? audioHz : 750)
    );

    connect(verifier, &FoxVerifier::verifyComplete,
            this, [this, verifier](int /*status*/, QDateTime /*ts*/, QString call,
                                   QString /*code*/, unsigned int /*hz*/, QString response) {
        verifier->deleteLater();
        emit statusMessage("Fox OTP OK: " + call + " — " + response);
    });

    connect(verifier, &FoxVerifier::verifyError,
            this, [this, verifier](int /*status*/, QDateTime /*ts*/, QString call,
                                   QString /*code*/, unsigned int /*hz*/, QString response) {
        verifier->deleteLater();
        emit statusMessage("Fox OTP FAIL: " + call + " — " + response);
    });
}

