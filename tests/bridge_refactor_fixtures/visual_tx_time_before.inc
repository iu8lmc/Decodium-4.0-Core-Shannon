static QString currentTxVisualTimeToken(QString const& mode)
{
    QString const upperMode = mode.trimmed().toUpper();
    int periodMs = 0;
    if (upperMode == QStringLiteral("FT8")) {
        periodMs = 15000;
    } else if (upperMode == QStringLiteral("FT4")) {
        periodMs = 7500;
    } else if (upperMode == QStringLiteral("FT2")) {
        periodMs = 3750;
    }

    if (periodMs > 0 && periodMs < 60000) {
        qint64 const slotIndex = ntpCorrectedCurrentMSecsSinceEpoch() / static_cast<qint64>(periodMs);
        QString const slotTime = utcDisplayTokenForSlotStart(slotIndex, periodMs);
        if (!slotTime.isEmpty()) {
            return slotTime;
        }
    }

    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("HHmmss"));
}

QString decodiumBridgeCurrentTxVisualTimeToken(QString const& mode)
{
    return currentTxVisualTimeToken(mode);
}

