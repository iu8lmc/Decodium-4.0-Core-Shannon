static bool amplifierPortClashesWithCat(DecodiumTransceiverManager* cat, const QString& port)
{
    if (!cat || port.trimmed().isEmpty())
        return false;
    QString const wanted = port.trimmed().toUpper();
    return wanted == cat->serialPort().trimmed().toUpper()
        || wanted == cat->pttPort().trimmed().toUpper();
}

bool decodiumBridgeAmplifierPortClashes(DecodiumTransceiverManager* cat, const QString& port)
{
    return amplifierPortClashesWithCat(cat, port);
}

