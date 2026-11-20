namespace {

constexpr int kMaxFtDecodeThreads {decodium::decode::kMaxFtDecodeThreads};

QTimeZone decodiumUtcTimeZone()
{
    return QTimeZone(QByteArrayLiteral("UTC"));
}

bool decodiumPskDebugEnabled()
{
    static const bool enabled = []() {
        QByteArray value = qgetenv("DECODIUM_PSK_DEBUG").trimmed().toLower();
        return value == "1" || value == "true" || value == "yes" || value == "on";
    }();
    return enabled;
}

}

bool decodiumBridgePskDebugEnabled() { return decodiumPskDebugEnabled(); }


int decodiumBridgeMaxFtDecodeThreads() { return kMaxFtDecodeThreads; }

QTimeZone decodiumBridgeUtcTimeZone()
{
    return decodiumUtcTimeZone();
}

