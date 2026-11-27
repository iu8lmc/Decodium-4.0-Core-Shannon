#if DECODIUM_HAS_SSTV
bool DecodiumBridge::commitSstvNewQso(
    const decodium::sstv::SstvQsoLogRequest& request,
    QString* associationId,
    QString* error)
{
    if (associationId) {
        associationId->clear();
    }
    if (error) {
        error->clear();
    }
    QString validationError;
    if (!request.createNewQso || !request.validate(&validationError)) {
        if (error) {
            *error = validationError.isEmpty()
                ? QStringLiteral("SSTV request is not a new QSO")
                : validationError;
        }
        return false;
    }

    const QString dxCall = request.remoteCallsign.trimmed().toUpper();
    const QString dxGrid = request.remoteGrid.trimmed().toUpper();
    const QString rstSent = request.reportSent.trimmed().isEmpty()
        ? QStringLiteral("59") : request.reportSent.trimmed();
    const QString rstRcvd = request.reportReceived.trimmed().isEmpty()
        ? QStringLiteral("59") : request.reportReceived.trimmed();
    const QDateTime onUtc = request.timeOnUtc;
    const QDateTime offUtc = request.timeOffUtc.isValid()
        ? request.timeOffUtc : onUtc;
    const QString comments = decodium::sstv::SstvQsoLog::mergedComment(
        request.comments, request.imageMode, &validationError);
    if (comments.isEmpty()) {
        if (error) {
            *error = validationError.isEmpty()
                ? QStringLiteral("SSTV QSO comment is invalid")
                : validationError;
        }
        return false;
    }

    int cqZone = 0;
    int ituZone = 0;
    if (m_dxccLookup && m_dxccLookup->isLoaded()) {
        const DxccEntity entity = m_dxccLookup->lookup(dxCall);
        if (entity.cqZone >= 1 && entity.cqZone <= 40) {
            cqZone = entity.cqZone;
        }
        if (entity.ituZone >= 1 && entity.ituZone <= 90) {
            ituZone = entity.ituZone;
        }
    }

    constexpr auto kAdifMode = "SSTV";
    const QString serialized = bridgeAdifRecordText(
        dxCall, dxGrid, static_cast<double>(request.frequencyHz),
        QString::fromLatin1(kAdifMode), onUtc, offUtc,
        rstSent, rstRcvd, m_callsign, m_grid, comments,
        {}, {}, {}, {}, m_nextLogName, m_nextLogQth, cqZone, ituZone);
    const auto checked = decodium::sstv::SstvQsoLog::validateGeneratedAdif(
        serialized, {request.imageRecordId});
    if (!checked.ok) {
        if (error) {
            *error = QStringLiteral("SSTV ADIF validation failed: %1")
                         .arg(checked.error);
        }
        return false;
    }

    // The local ADIF log has no stable attachment field. Its canonical QSO
    // identity is checked against Decodium's asynchronously prepared native
    // cache, never by reparsing the active ADIF file on the GUI thread.
    const QString logPath = effectiveAdifLogPath();
    const QFileInfo logInfo(logPath);
    const QDateTime logModified = logInfo.exists()
        ? logInfo.lastModified() : QDateTime {};
    const qint64 logSize = logInfo.exists() ? logInfo.size() : 0;
    QVariantList existingRows;
    bool hadReadyCache = false;
    bool cacheCurrent = false;
    {
        QMutexLocker locker(&m_qsoSearchCacheMutex);
        hadReadyCache = m_qsoSearchCacheReady;
        cacheCurrent = m_qsoSearchCacheReady
            && m_qsoSearchCachePath == logPath
            && m_qsoSearchCacheModified == logModified
            && m_qsoSearchCacheSize == logSize;
        if (cacheCurrent) {
            existingRows = m_qsoSearchCacheRows;
        }
    }
    if (!cacheCurrent) {
        if (hadReadyCache) {
            invalidateQsoSearchCache();
        }
        warmLogCacheAsync();
        if (error) {
            *error = QStringLiteral(
                "The active Decodium logbook index is loading; retry this explicit QSO operation when it is ready");
        }
        return false;
    }

    const QString targetDateTime = onUtc.toUTC().toString(
        QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString targetBand = freqHzToBandToken(
        static_cast<quint64>(request.frequencyHz)).trimmed().toUpper();
    for (const QVariant& rowValue : existingRows) {
        const QVariantMap row = rowValue.toMap();
        if (row.value(QStringLiteral("call")).toString().trimmed().toUpper()
                == dxCall
            && row.value(QStringLiteral("dateTime")).toString().trimmed()
                == targetDateTime
            && row.value(QStringLiteral("mode")).toString().trimmed().toUpper()
                == QString::fromLatin1(kAdifMode)
            && row.value(QStringLiteral("band")).toString().trimmed().toUpper()
                == targetBand) {
            if (error) {
                *error = QStringLiteral(
                    "This QSO already exists in the active logbook; associate the image with the existing record");
            }
            return false;
        }
    }

    if (!appendAdifRecord(dxCall, dxGrid,
                          static_cast<double>(request.frequencyHz),
                          QString::fromLatin1(kAdifMode), onUtc, offUtc,
                          rstSent, rstRcvd, comments, {}, {}, {}, {},
                          cqZone, ituZone)) {
        if (error) {
            *error = QStringLiteral(
                "the active Decodium ADIF logbook could not be written");
        }
        return false;
    }

    const QString allTxtPath = logAllTxtPath();
    QDir().mkpath(QFileInfo(allTxtPath).absolutePath());
    QFile allTxt(allTxtPath);
    if (allTxt.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&allTxt);
        stream << onUtc.toString(QStringLiteral("yyyyMMdd_hhmmss"))
               << ' ' << m_callsign
               << ' ' << dxCall
               << ' ' << dxGrid
               << ' ' << QString::number(
                      static_cast<double>(request.frequencyHz) / 1e6,
                      'f', 6) << "MHz SSTV\n";
    }

    const QByteArray adifRecord = serialized.toUtf8();
    udpSendLoggedQso(dxCall, dxGrid,
                     static_cast<double>(request.frequencyHz),
                     QString::fromLatin1(kAdifMode), onUtc, offUtc,
                     rstSent, rstRcvd, adifRecord, comments);
    udpSendN1mmLoggedQso(dxCall, adifRecord);
    tcpSendLoggedAdifQso(dxCall, adifRecord);
    if (m_qrzLogbookEnabled && m_qrzLogbook) {
        m_qrzLogbook->uploadAdif(dxCall, adifRecord);
    }
    if (m_cloudlogEnabled && m_cloudlog) {
        m_cloudlog->setEnabled(m_cloudlogEnabled);
        m_cloudlog->setApiUrl(m_cloudlogUrl);
        m_cloudlog->setApiKey(m_cloudlogApiKey);
        m_cloudlog->setStationId(qBound(
            0, getSetting(QStringLiteral("CloudLogStationID"), 1).toInt(),
            999));
        m_cloudlog->logQso(dxCall, dxGrid,
                           static_cast<double>(request.frequencyHz),
                           QString::fromLatin1(kAdifMode), onUtc, 0,
                           rstSent, rstRcvd, m_callsign, m_grid);
    }
    if (m_callsignIntelligence) {
        m_callsignIntelligence->notifyQsoLogged(dxCall);
    }
    if (associationId) {
        *associationId = checked.associationId;
    }
    return true;
}
#endif

