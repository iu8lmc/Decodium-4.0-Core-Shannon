QVariantList DecodiumBridge::mirrorLegacyDecodeLines(QStringList const& lines,
                                                     bool rxPane,
                                                     QVariantList const& previousEntries) const
{
    DecodeUserFilterConfig const userFilterConfig = readDecodeUserFilterConfig();
    return mirrorLegacyDecodeLines(lines, rxPane, previousEntries, userFilterConfig);
}




QVariantList DecodiumBridge::mirrorLegacyDecodeLines(QStringList const& lines,
                                                     bool rxPane,
                                                     QVariantList const& previousEntries,
                                                     DecodeUserFilterConfig const& userFilterConfig) const
{
    int const sourceRowCount = lines.size();
    int const firstSourceRow =
        decodium::legacy::recent_decode_window_start(sourceRowCount);
    int const scannedRowCount = sourceRowCount - firstSourceRow;
    MainThreadTraceScope trace(QStringLiteral("mirror_legacy_decode_lines"),
                               QStringLiteral("source_rows=%1 scanned_rows=%2 previous_rows=%3 rx=%4")
                                   .arg(sourceRowCount)
                                   .arg(scannedRowCount)
                                   .arg(previousEntries.size())
                                   .arg(rxPane ? 1 : 0),
                               25);
    QVariantList mirroredDecodes;
    mirroredDecodes.reserve(scannedRowCount);

    static const QRegularExpression timePrefix {R"(^(\d{4}|\d{6}))"};
    static const QRegularExpression txPattern {
        R"(^(\d{4}|\d{6})\s+Tx\S*\s+(\d+)\s+\S+\s+(.*)$)",
        QRegularExpression::CaseInsensitiveOption
    };
    static const QRegularExpression rxPattern {
        R"(^(\d{4}|\d{6})\s+(-?\d+)\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+\S+\s+(.*)$)",
        QRegularExpression::CaseInsensitiveOption
    };

    auto rawIdentityKey = [](QVariantMap const& entry, QString const& rawTime) {
        return rawTime.left(4)
            + QLatin1Char('|') + entry.value(QStringLiteral("freq")).toString().trimmed()
            + QLatin1Char('|') + canonicalDecodeMessage(entry.value(QStringLiteral("message")).toString())
            + QLatin1Char('|') + entry.value(QStringLiteral("db")).toString().trimmed()
            + QLatin1Char('|') + entry.value(QStringLiteral("dt")).toString().trimmed()
            + QLatin1Char('|') + (entry.value(QStringLiteral("isTx")).toBool()
                                      ? QStringLiteral("1") : QStringLiteral("0"));
    };

    // A legacy snapshot contains the complete visible history.  Re-enriching
    // every old row performs DXCC/worked/grid/highlight lookups O(N) at each
    // decode pass.  Index the previous snapshot once and enrich only genuinely
    // new or changed rows.
    QHash<QString, QVariantMap> previousByMirrorKey;
    QMultiHash<QString, QVariantMap> previousBySemanticKey;
    QHash<QString, QString> previousTimeByRawIdentity;
    previousByMirrorKey.reserve(previousEntries.size());
    previousBySemanticKey.reserve(previousEntries.size());
    previousTimeByRawIdentity.reserve(previousEntries.size());
    for (QVariant const& value : previousEntries) {
        QVariantMap const previous = value.toMap();
        QString const mirrorKey = decodeMirrorEntryKey(previous);
        if (!mirrorKey.isEmpty()) {
            previousByMirrorKey.insert(mirrorKey, previous);
        }
        previousBySemanticKey.insert(decodeMirrorSemanticKey(previous), previous);
        QString const previousTime =
            normalizeUtcDisplayToken(previous.value(QStringLiteral("time")).toString());
        if (previousTime.size() >= 6) {
            previousTimeByRawIdentity.insert(rawIdentityKey(previous, previousTime),
                                             previousTime.left(6));
        }
    }

    QStringList const localCalls = localDecodeCallCandidates();
    auto mentionsLocalCall = [&localCalls](QString const& message) {
        for (QString const& call : localCalls) {
            if (messageContainsCallToken(message, call, normalizedBaseCall(call))) {
                return true;
            }
        }
        return false;
    };

    bool const isTimeSyncMode = isTimeSyncDecodeMode(m_mode);
    int const periodMs = periodMsForMode(m_mode);
    auto stabilizeLegacyTimeToken = [&](QVariantMap& entry) {
        QString const rawTime = normalizeUtcDisplayToken(entry.value(QStringLiteral("time")).toString());
        if (!(isTimeSyncMode && periodMs > 0 && periodMs < 60000 && rawTime.size() == 4)) {
            entry[QStringLiteral("time")] = rawTime;
            return;
        }

        QString resolvedTime =
            previousTimeByRawIdentity.value(rawIdentityKey(entry, rawTime));

        if (resolvedTime.isEmpty()) {
            // Match Decodium3's DecodedText HHMM handling: missing seconds are
            // interpreted as :00. Do not use the processing-time slot here; by
            // the time legacy rows are mirrored we may already be in the next
            // period, which flips TX parity and delays the response one slot.
            resolvedTime = rawTime + QStringLiteral("00");
        }

        entry[QStringLiteral("time")] = resolvedTime.isEmpty() ? rawTime : resolvedTime;
    };

    int reusedEntries = 0;
    int enrichedEntries = 0;
    auto enrichOrReuse = [this, &previousByMirrorKey, &previousBySemanticKey,
                          &mentionsLocalCall, &reusedEntries,
                          &enrichedEntries](QVariantMap& entry) {
        static const QStringList stableInputFields {
            QStringLiteral("db"), QStringLiteral("dt"), QStringLiteral("aptype"),
            QStringLiteral("quality"), QStringLiteral("mode"), QStringLiteral("isTx"),
            QStringLiteral("isCQ")
        };
        auto inputsMatch = [&entry](QVariantMap const& previous) {
            for (QString const& field : stableInputFields) {
                if (previous.value(field) != entry.value(field)) {
                    return false;
                }
            }
            return true;
        };
        auto reuse = [&entry, &reusedEntries](QVariantMap const& previous) {
            bool const forceRxPane = entry.value(QStringLiteral("forceRxPane")).toBool();
            QString const currentTime = entry.value(QStringLiteral("time")).toString();
            entry = previous;
            entry[QStringLiteral("forceRxPane")] = forceRxPane;
            entry[QStringLiteral("time")] = currentTime;

            QString digits;
            digits.reserve(currentTime.size());
            for (QChar const ch : currentTime) {
                if (ch.isDigit()) digits.append(ch);
            }
            entry[QStringLiteral("formattedTime")] = digits.size() >= 6
                ? digits.left(2) + QLatin1Char(':') + digits.mid(2, 2)
                    + QLatin1Char(':') + digits.mid(4, 2)
                : (digits.size() == 4
                       ? digits.left(2) + QLatin1Char(':') + digits.mid(2, 2)
                       : currentTime);
            ++reusedEntries;
        };

        QString const key = decodeMirrorEntryKey(entry);
        auto const previousIt = previousByMirrorKey.constFind(key);
        if (previousIt != previousByMirrorKey.constEnd() && inputsMatch(previousIt.value())) {
            reuse(previousIt.value());
            return;
        }

        // Legacy rows can initially expose HHMM and later HHMMSS for the same
        // decode. The enrichment metadata is unchanged; reuse it instead of
        // repeating DXCC/worked/grid/highlight lookups on the GUI thread.
        auto const semanticCandidates = previousBySemanticKey.values(decodeMirrorSemanticKey(entry));
        for (QVariantMap const& previous : semanticCandidates) {
            if (inputsMatch(previous)) {
                reuse(previous);
                return;
            }
        }

        QString const message = entry.value(QStringLiteral("message")).toString();
        bool const isCQ = entry.value(QStringLiteral("isCQ")).toBool();
        entry[QStringLiteral("isMyCall")] = mentionsLocalCall(message);
        entry[QStringLiteral("fromCall")] = extractDecodedCallsign(message, isCQ);
        enrichDecodeEntry(entry);
        ++enrichedEntries;
    };

    for (int row = firstSourceRow; row < sourceRowCount; ++row) {
        QString rawLine = lines.at(row);
        rawLine = stripLegacyDecodeAppendage(rawLine);
        if (rawLine.isEmpty() || rawLine.contains(QStringLiteral("------"))) {
            continue;
        }

        auto const txMatch = txPattern.match(rawLine);
        if (txMatch.hasMatch()) {
            QString aptype;
            QString const message = stripDecodeApAnnotation(txMatch.captured(3).trimmed(), &aptype);
            if (message.isEmpty()) {
                continue;
            }

            bool const isCQ = message.startsWith("CQ ", Qt::CaseInsensitive)
                           || message == "CQ"
                           || message.startsWith("QRZ ", Qt::CaseInsensitive);

            QVariantMap entry;
            entry["time"] = normalizeUtcDisplayToken(txMatch.captured(1));
            entry["db"] = QStringLiteral("TX");
            entry["dt"] = QStringLiteral("0.0");
            entry["freq"] = txMatch.captured(2);
            entry["message"] = message;
            entry["aptype"] = aptype;
            entry["quality"] = QString {};
            entry["mode"] = m_mode;
            entry["isTx"] = true;
            entry["isCQ"] = isCQ;
            entry["forceRxPane"] = rxPane;
            stabilizeLegacyTimeToken(entry);
            enrichOrReuse(entry);
            QString userFilterReason;
            if (!shouldAcceptDecodeEntryByUserFilters(entry, userFilterConfig, false, &userFilterReason)) {
                bridgeLog(QStringLiteral("user decode filter legacy-mirror: %1 msg='%2'")
                              .arg(userFilterReason, message));
                continue;
            }
            mirroredDecodes.append(entry);
            continue;
        }

        QString timeStr;
        QString dbStr;
        QString dtStr;
        QString freqStr;
        QString message;

        auto const rxMatch = rxPattern.match(rawLine);
        if (rxMatch.hasMatch()) {
            timeStr = normalizeUtcDisplayToken(rxMatch.captured(1));
            dbStr = rxMatch.captured(2).trimmed();
            dtStr = rxMatch.captured(3).trimmed();
            freqStr = rxMatch.captured(4).trimmed();
            message = rxMatch.captured(5).trimmed();
        } else {
            DecodedText decoded {rawLine};
            QString const cleanLine = decoded.clean_string().trimmed();
            if (cleanLine.isEmpty()) {
                continue;
            }

            int const padding = cleanLine.indexOf(QLatin1Char(' ')) > 4 ? 2 : 0;
            int const payloadStart = 22 + padding;
            if (cleanLine.size() <= payloadStart) {
                continue;
            }

            timeStr = normalizeUtcDisplayToken(timePrefix.match(cleanLine).hasMatch()
                                                   ? timePrefix.match(cleanLine).captured(1)
                                                   : QString {});
            dbStr = QString::number(decoded.snr());
            dtStr = QString::number(decoded.dt(), 'f', 1);
            freqStr = QString::number(decoded.frequencyOffset());
            message = cleanLine.mid(payloadStart).trimmed();
        }

        if (message.isEmpty()) {
            continue;
        }
        QString aptype;
        message = stripDecodeApAnnotation(message, &aptype);
        if (message.isEmpty()) {
            continue;
        }
        QString semanticRejectReason;
        if (!shouldAcceptDecodedMessage(message, &semanticRejectReason)) {
            bridgeLog(QStringLiteral("semantic decode filter legacy-mirror: %1 msg='%2'")
                          .arg(semanticRejectReason, message));
            continue;
        }

        bool const isCQ = message.startsWith("CQ ", Qt::CaseInsensitive)
                       || message == "CQ"
                       || message.startsWith("QRZ ", Qt::CaseInsensitive);

        QVariantMap entry;
        entry["time"] = timeStr;
        entry["db"] = dbStr;
        entry["dt"] = dtStr;
        entry["freq"] = freqStr;
        entry["message"] = message;
        entry["aptype"] = aptype;
        entry["quality"] = QStringLiteral("100");
        entry["mode"] = m_mode;
        entry["isTx"] = false;
        entry["isCQ"] = isCQ;
        entry["forceRxPane"] = rxPane;
        stabilizeLegacyTimeToken(entry);
        enrichOrReuse(entry);
        if (m_hideGhostDecodes && looksLikeGhostDecode(entry)) {
            bridgeLog(QStringLiteral("ghost decode filter legacy-mirror: msg='%1'")
                          .arg(message));
            continue;
        }
        QString userFilterReason;
        if (!shouldAcceptDecodeEntryByUserFilters(entry, userFilterConfig, false, &userFilterReason)) {
            bridgeLog(QStringLiteral("user decode filter legacy-mirror: %1 msg='%2'")
                          .arg(userFilterReason, message));
            continue;
        }
        mirroredDecodes.append(entry);
    }

    trace.addDetail(QStringLiteral("output_rows=%1 reused=%2 enriched=%3")
                        .arg(mirroredDecodes.size())
                        .arg(reusedEntries)
                        .arg(enrichedEntries));
    return mirroredDecodes;
}


