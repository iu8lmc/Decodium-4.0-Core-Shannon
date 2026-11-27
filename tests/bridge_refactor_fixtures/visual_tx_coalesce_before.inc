static bool shouldCoalesceVisualTxRows(QString const& mode)
{
    QString const upperMode = mode.trimmed().toUpper();
    return upperMode == QStringLiteral("FT8")
        || upperMode == QStringLiteral("FT4")
        || upperMode == QStringLiteral("FT2");
}

static QString normalizedVisualTxTimeToken(QString const& rawTime, QString const& mode)
{
    QString const normalizedTime = normalizeUtcDisplayToken(rawTime);
    if (!shouldCoalesceVisualTxRows(mode)) {
        return normalizedTime;
    }

    QString const upperMode = mode.trimmed().toUpper();
    int periodMs = 0;
    if (upperMode == QStringLiteral("FT8")) {
        periodMs = 15000;
    } else if (upperMode == QStringLiteral("FT4")) {
        periodMs = 7500;
    } else if (upperMode == QStringLiteral("FT2")) {
        periodMs = 3750;
    }
    if (periodMs <= 0 || periodMs >= 60000) {
        return normalizedTime;
    }

    QDateTime const approxUtc = approxUtcDateTimeForDisplayToken(normalizedTime);
    if (!approxUtc.isValid()) {
        return normalizedTime;
    }

    qint64 const slotIndex = approxUtc.toMSecsSinceEpoch() / static_cast<qint64>(periodMs);
    QString const slotTime = utcDisplayTokenForSlotStart(slotIndex, periodMs);
    if (slotTime == normalizedTime || periodMs % 1000 == 0) {
        return slotTime.isEmpty() ? normalizedTime : slotTime;
    }

    // FT2/FT4 slot boundaries can land at .750/.500 seconds. Their display
    // token is floored to HHMMSS, so reconstructing it as exactly .000 would
    // incorrectly push it into the previous slot. If any point inside the
    // displayed second maps back to the displayed token, keep that token.
    QDateTime const endOfDisplayedSecond = approxUtc.addMSecs(999);
    qint64 const endSlotIndex = endOfDisplayedSecond.toMSecsSinceEpoch() / static_cast<qint64>(periodMs);
    QString const endSlotTime = utcDisplayTokenForSlotStart(endSlotIndex, periodMs);
    if (endSlotTime == normalizedTime) {
        return normalizedTime;
    }

    return slotTime.isEmpty() ? normalizedTime : slotTime;
}

static QString rxPaneVisualTxSlotKey(QVariantMap const& entry, QString const& mode)
{
    if (!shouldCoalesceVisualTxRows(mode) || !entry.value(QStringLiteral("isTx")).toBool()) {
        return {};
    }

    QString const time = normalizedVisualTxTimeToken(entry.value(QStringLiteral("time")).toString(), mode).trimmed();
    if (time.isEmpty()) {
        return {};
    }

    // Only one local TX can be valid in a time-synced slot. Different mirror
    // sources may report different audio frequencies for the same TX event, so
    // frequency must not be part of the visual coalescing key.
    return time;
}

static void coalesceRxPaneTxRows(QVariantList& entries, QString const& mode)
{
    if (!shouldCoalesceVisualTxRows(mode) || entries.isEmpty()) {
        return;
    }

    QHash<QString, int> keptIndexBySlot;
    for (int i = 0; i < entries.size(); ) {
        QVariantMap const entry = entries.at(i).toMap();
        QString const slotKey = rxPaneVisualTxSlotKey(entry, mode);
        if (slotKey.isEmpty()) {
            ++i;
            continue;
        }

        auto const it = keptIndexBySlot.constFind(slotKey);
        if (it == keptIndexBySlot.constEnd()) {
            keptIndexBySlot.insert(slotKey, i);
            ++i;
            continue;
        }

        int const keptIndex = it.value();
        QVariantMap const keptEntry = entries.at(keptIndex).toMap();
        bool const keptLocal = keptEntry.value(QStringLiteral("localTxMirror")).toBool();
        bool const currentLocal = entry.value(QStringLiteral("localTxMirror")).toBool();
        bool const preferCurrent =
            currentLocal && !keptLocal;

        if (preferCurrent) {
            entries[keptIndex] = entry;
        }

        entries.removeAt(i);
    }
}

void decodiumBridgeCoalesceRxPaneTxRows(QVariantList& entries, QString const& mode)
{
    coalesceRxPaneTxRows(entries, mode);
}

