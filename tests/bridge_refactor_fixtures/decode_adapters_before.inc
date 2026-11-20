static QString extractRightCallsign(const QString& msg)
{
    return decodedDxCallToken(msg);
}


bool decodiumBridgeUnresolvedGridOnlyDecode(const QString& message, QString* grid)
{
    return messageIsUnresolvedGridOnlyDecode(message, grid);
}
bool decodiumBridgeFst4wUnresolvedHashDecode(QString const& mode, QString const& message)
{
    return isFst4wUnresolvedHashDecode(mode, message);
}
QString decodiumBridgeExtractRightCallsign(const QString& message)
{
    return extractRightCallsign(message);
}


