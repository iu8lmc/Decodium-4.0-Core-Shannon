    m_decoLogLink = new DecodiumDecoLogLink(this);
    connect(m_decoLogLink, &DecodiumDecoLogLink::connectedChanged, this, [this](bool connected) {
        m_decoLogConnected = connected;
        // Il nome mostrato e' quello dichiarato dal programma ("product"), non
        // quello di protocollo: dopo la rinomina in DecoDXLog l'utente deve
        // leggere il nome che vede sulla sua finestra.
        const QString nome = m_decoLogLink->peerProduct().isEmpty()
                                 ? QStringLiteral("DecoLog")
                                 : m_decoLogLink->peerProduct();
        m_decoLogStatus = connected
            ? tr("%1 %2 connected").arg(nome, m_decoLogLink->peerVersion())
            : tr("%1 not connected").arg(nome);
        bridgeLog(QStringLiteral("DecoLink: %1").arg(m_decoLogStatus));
        emit decoLogStateChanged();
    });
    connect(m_decoLogLink, &DecodiumDecoLogLink::workedRows, this,
            [this](const QJsonArray& rows, bool first, bool final) {
                if (first)
                    m_decoLogPendingRows.clear();
                for (const QJsonValue& v : rows) {
                    const QJsonArray r = v.toArray();
                    if (r.size() < 6)
                        continue;
                    m_decoLogPendingRows.append(DecoLogRow{r.at(0).toString().toUpper(), r.at(1).toString().toLower(),
                                                           r.at(2).toString().toUpper(), r.at(3).toString(),
                                                           r.at(4).toString().toUpper(), r.at(5).toInt() != 0});
                }
                if (!final)
                    return;
                m_decoLogRows = m_decoLogPendingRows;
                m_decoLogPendingRows.clear();
                applyDecoLogRows();
                refreshDecodeListDxcc();
                emit workedCountChanged();
                m_decoLogStatus = QStringLiteral("DecoLog: %1 QSO nel worked-before").arg(m_decoLogRows.size());
                bridgeLog(QStringLiteral("DecoLink: %1 righe dal log di DecoLog").arg(m_decoLogRows.size()));
                emit decoLogStateChanged();
            });
    connect(m_decoLogLink, &DecodiumDecoLogLink::qsoWritten, this, [this](const QJsonObject& msg) {
        const QJsonArray r = msg.value(QStringLiteral("row")).toArray();
        const QString call = r.at(0).toString().toUpper();
        const QString status = msg.value(QStringLiteral("status")).toString();
        const bool fromDecodium = msg.value(QStringLiteral("source")).toString() == QLatin1String("udp_decodium");
        if (status == QLatin1String("logged") && r.size() >= 6) {
            const DecoLogRow row{call, r.at(1).toString().toLower(), r.at(2).toString().toUpper(),
                                 r.at(3).toString(), r.at(4).toString().toUpper(), r.at(5).toInt() != 0};
            m_decoLogRows.append(row);
            m_workedCalls.insert(call);
            appendWorkedQsoBand(row.call, row.grid, row.band, row.mode, row.date);
            refreshWorkedBeforeDecodeEntriesForCall(call);
            m_decoLogStatus = QStringLiteral("DecoLog: %1 salvato").arg(call);
            if (fromDecodium)
                emit statusMessage(QStringLiteral("DecoLog: QSO con %1 salvato nel log").arg(call));
        } else if (status == QLatin1String("duplicate")) {
            m_decoLogStatus = QStringLiteral("DecoLog: %1 gia' nel log").arg(call);
            if (fromDecodium)
                emit statusMessage(QStringLiteral("DecoLog: %1 era gia' nel log").arg(call));
        } else if (status == QLatin1String("error")) {
            m_decoLogStatus = QStringLiteral("DecoLog: %1 non salvato").arg(call);
            if (fromDecodium)
                emit errorMessage(QStringLiteral("DecoLog: QSO con %1 NON salvato: %2")
                                      .arg(call, msg.value(QStringLiteral("message")).toString()));
        }
        emit decoLogStateChanged();
    });
    // Il cluster di DecoLog: gli spot entrano nella lista del DX Cluster (e quindi
    // nella cascata), marcati con quello che il log ne sa.
    connect(m_decoLogLink, &DecodiumDecoLogLink::spotReceived, this, [this](const QJsonObject& msg) {
        if (!m_dxCluster)
            return;
        const QString call = msg.value(QStringLiteral("call")).toString().trimmed().toUpper();
        const double freqKhz = msg.value(QStringLiteral("freqKhz")).toDouble();
        if (call.isEmpty() || freqKhz <= 0.0)
            return;
        const QString status = msg.value(QStringLiteral("status")).toString();
        const QString comment = msg.value(QStringLiteral("comment")).toString();
        QDateTime at = QDateTime::fromString(msg.value(QStringLiteral("time")).toString(), Qt::ISODate).toUTC();
        if (!at.isValid())
            at = QDateTime::currentDateTimeUtc();
        QVariantMap spot;
        spot[QStringLiteral("spotter")] = msg.value(QStringLiteral("spotter")).toString();
        spot[QStringLiteral("dxCall")] = call;
        spot[QStringLiteral("frequency")] = freqKhz;
        spot[QStringLiteral("comment")] = status.isEmpty() ? comment : QStringLiteral("[%1] %2").arg(status, comment);
        spot[QStringLiteral("time")] = at.toString(QStringLiteral("HHmm")) + QLatin1Char('Z');
        spot[QStringLiteral("band")] = DecodiumDxCluster::bandLabelFromFrequencyKhz(freqKhz);
        spot[QStringLiteral("mode")] = msg.value(QStringLiteral("mode")).toString();
        spot[QStringLiteral("timestamp")] = at.toString(Qt::ISODate);
        spot[QStringLiteral("source")] = QStringLiteral("DecoLog");
        spot[QStringLiteral("status")] = status;
        spot[QStringLiteral("statusBits")] = msg.value(QStringLiteral("statusBits")).toInt();
        spot[QStringLiteral("entity")] = msg.value(QStringLiteral("entity")).toString();
        spot[QStringLiteral("alert")] = msg.value(QStringLiteral("alert")).toBool();
        m_dxCluster->injectSpot(spot);
        if (msg.value(QStringLiteral("alert")).toBool()) {
            emit statusMessage(QStringLiteral("DecoLog cluster: %1 %2 %3 kHz %4 %5")
                                   .arg(status.isEmpty() ? QStringLiteral("spot") : status, call,
                                        QString::number(freqKhz, 'f', 1), msg.value(QStringLiteral("mode")).toString(),
                                        msg.value(QStringLiteral("entity")).toString()));
        }
    });
    // Doppio clic su uno spot in DecoLog: radio sulla frequenza, modo se Decodium lo
    // conosce, DX pronto nel pannello del QSO. Nessuna trasmissione.
    connect(m_decoLogLink, &DecodiumDecoLogLink::tuneRequested, this, [this](const QJsonObject& msg) {
        const QString call = msg.value(QStringLiteral("call")).toString().trimmed().toUpper();
        const double dialKhz = msg.value(QStringLiteral("dialKhz")).toDouble();
        const int audioHz = msg.value(QStringLiteral("audioHz")).toInt();
        const QString mode = msg.value(QStringLiteral("mode")).toString().trimmed().toUpper();
        if (dialKhz <= 0.0)
            return;
        bridgeLog(QStringLiteral("DecoLink tune: %1 dial=%2 kHz audio=%3 Hz mode=%4").arg(call).arg(dialKhz, 0, 'f', 1).arg(audioHz).arg(mode));
        static const QStringList digital{QStringLiteral("FT8"), QStringLiteral("FT4"), QStringLiteral("FT2")};
        if (digital.contains(mode) && m_mode.compare(mode, Qt::CaseInsensitive) != 0)
            setMode(mode);
        setFrequency(dialKhz * 1000.0);
        if (!call.isEmpty())
            setDxCall(call);
        const QString grid = msg.value(QStringLiteral("grid")).toString().trimmed();
        if (grid.size() >= 4)
            setDxGrid(grid);
        if (audioHz >= 100 && audioHz <= 5000)
            setRxFrequency(audioHz);
        emit statusMessage(QStringLiteral("DecoLog: sintonizzato su %1 %2 kHz %3").arg(call, QString::number(dialKhz, 'f', 1), mode));
    });
    connect(m_decoLogLink, &DecodiumDecoLogLink::awardChanged, this, [this](const QJsonObject& award) {
        QVariantMap map = award.value(QStringLiteral("ft2")).toObject().toVariantMap();
        const QJsonObject dxcc = award.value(QStringLiteral("dxcc")).toObject();
        map.insert(QStringLiteral("dxccAllWorked"), dxcc.value(QStringLiteral("worked")).toInt());
        map.insert(QStringLiteral("dxccAllConfirmed"), dxcc.value(QStringLiteral("confirmed")).toInt());
        m_decoLogAward = map;
        emit decoLogStateChanged();
    });

