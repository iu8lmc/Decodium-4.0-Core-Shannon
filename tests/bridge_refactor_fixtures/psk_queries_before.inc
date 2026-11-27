void DecodiumBridge::sendPskReporterNow()
{
    if (offlineMode()) {
        emit statusMessage(QStringLiteral("Offline: PSK Reporter disabled"));
        return;
    }
    if (m_pskReporter && m_pskReporterEnabled)
        m_pskReporter->sendReport();
}

void DecodiumBridge::searchPskReporter(const QString& callsign)
{
    if (offlineMode()) {
        emit statusMessage(QStringLiteral("Offline: PSK Reporter search disabled"));
        return;
    }
    if (m_pskSearching) return;
    QString const call = callsign.trimmed().toUpper();
    if (call.isEmpty()) return;

    m_pskSearchCallsign = call;
    m_pskSearchFound = false;
    m_pskSearchBands.clear();
    m_pskSearching = true;
    emit pskSearchingChanged();
    emit pskSearchCallsignChanged();
    emit pskSearchFoundChanged();
    emit pskSearchBandsChanged();

    // Query reale a PSK Reporter:
    //   GET https://retrieve.pskreporter.info/query
    //     ?senderCallsign=XXX       (callsign cercato come trasmittente)
    //     &flowStartSeconds=-N      (N = intervallo configurato, 5-60 min)
    //     &rptlimit=50              (max 50 spot)
    //     &rronly=1                 (solo reception reports, XML compatto)
    // Risposta: XML con elementi <receptionReport frequency="7074000" ... />.
    // Se almeno uno è presente la stazione è "online" e aggrega le bande.
    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    QUrl url(QStringLiteral("https://retrieve.pskreporter.info/query"));
    QUrlQuery q;
    int const spanMinutes = m_pskReporterTimeSpanMinutes;
    q.addQueryItem(QStringLiteral("senderCallsign"), call);
    q.addQueryItem(QStringLiteral("flowStartSeconds"), QString::number(-spanMinutes * 60));
    q.addQueryItem(QStringLiteral("rptlimit"), QStringLiteral("50"));
    q.addQueryItem(QStringLiteral("rronly"), QStringLiteral("1"));
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Decodium/4.0 (+pskreporter search)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = nam->get(req);

    // Timeout hard: 10s. Qt 5.15+ ha transferTimeout ma teniamolo esplicito.
    QTimer* timeout = new QTimer(this);
    timeout->setSingleShot(true);
    timeout->setInterval(10000);
    connect(timeout, &QTimer::timeout, reply, [reply]() {
        if (reply && reply->isRunning()) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, timeout, &QTimer::deleteLater);
    timeout->start();

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, spanMinutes]() {
        // 1.0.376 (sec #2): cap 1 MiB sulla risposta, come fetchPskHeardBy.
        static constexpr qsizetype kPskSearchMaxReplyBytes = 1024 * 1024;
        QVariant const contentLength = reply->header(QNetworkRequest::ContentLengthHeader);
        QByteArray data;
        bool const declaredTooLarge =
            contentLength.isValid()
            && contentLength.toLongLong() > static_cast<qlonglong>(kPskSearchMaxReplyBytes);
        if (!declaredTooLarge) {
            data = reply->read(static_cast<qint64>(kPskSearchMaxReplyBytes + 1));
            if (data.size() > kPskSearchMaxReplyBytes) data.clear();
        }
        QNetworkReply::NetworkError const err = reply->error();
        QString const errStr = reply->errorString();
        reply->deleteLater();
        nam->deleteLater();

        bool found = false;
        QSet<QString> bandSet;

        if (err == QNetworkReply::NoError) {
            QString const xml = QString::fromUtf8(data);
            // Parsing via regex: XML di PSK Reporter è deterministico,
            // ogni spot è <receptionReport ... frequency="12345678" ... />
            QRegularExpression re(QStringLiteral("<receptionReport\\b[^>]*\\bfrequency=\"([0-9]+)\""));
            auto it = re.globalMatch(xml);
            while (it.hasNext()) {
                auto m = it.next();
                bool ok = false;
                qint64 const freqHz = m.captured(1).toLongLong(&ok);
                if (!ok) continue;
                found = true;
                double const mhz = freqHz / 1000000.0;
                QString band;
                if (mhz < 2.0)        band = QStringLiteral("160m");
                else if (mhz < 4.5)   band = QStringLiteral("80m");
                else if (mhz < 5.8)   band = QStringLiteral("60m");
                else if (mhz < 8.0)   band = QStringLiteral("40m");
                else if (mhz < 11.0)  band = QStringLiteral("30m");
                else if (mhz < 15.0)  band = QStringLiteral("20m");
                else if (mhz < 19.0)  band = QStringLiteral("17m");
                else if (mhz < 22.0)  band = QStringLiteral("15m");
                else if (mhz < 26.0)  band = QStringLiteral("12m");
                else if (mhz < 30.0)  band = QStringLiteral("10m");
                else if (mhz < 55.0)  band = QStringLiteral("6m");
                else if (mhz < 150.0) band = QStringLiteral("2m");
                else                  band = QString::number(mhz, 'f', 1) + QStringLiteral("MHz");
                bandSet.insert(band);
            }
            bridgeLog(QStringLiteral("PSK Reporter search '%1': span=%2min found=%3 bands=%4")
                          .arg(m_pskSearchCallsign)
                          .arg(spanMinutes)
                          .arg(found ? "YES" : "NO")
                          .arg(bandSet.size()));
        } else {
            bridgeLog(QStringLiteral("PSK Reporter search '%1': network error: %2")
                          .arg(m_pskSearchCallsign, errStr));
        }

        // Ordina bande per frequenza crescente (160m → 2m)
        static const QStringList bandOrder {
            QStringLiteral("160m"), QStringLiteral("80m"), QStringLiteral("60m"),
            QStringLiteral("40m"),  QStringLiteral("30m"), QStringLiteral("20m"),
            QStringLiteral("17m"),  QStringLiteral("15m"), QStringLiteral("12m"),
            QStringLiteral("10m"),  QStringLiteral("6m"),  QStringLiteral("2m")
        };
        QStringList bands;
        for (const QString& b : bandOrder) {
            if (bandSet.contains(b)) bands.append(b);
        }
        // Eventuali bande VHF/UHF non nella lista conosciuta
        for (const QString& b : std::as_const(bandSet)) {
            if (!bands.contains(b)) bands.append(b);
        }

        m_pskSearchFound = found;
        m_pskSearchBands = bands;
        m_pskSearching = false;
        emit pskSearchFoundChanged();
        emit pskSearchBandsChanged();
        emit pskSearchingChanged();
    });
}

// ============================================================
// DX-Pedition Mode Fase 3 — PSK Reporter "heard-by"
// Stessa query/endpoint di searchPskReporter, ma con senderCallsign = il MIO
// call: PSK Reporter risponde con chi MI ha ricevuto nell'intervallo scelto. Parser
// esteso: receiverCallsign / receiverLocator / sNR / frequency per ogni
// <receptionReport>. distKm via calcDistance() esistente (grid2deg+azdist),
// dxcc via m_dxccLookup esistente (cty.dat). Nessun NAM/thread/lib nuovo.
// Anti-spam: ignora se fetch in corso o se < 60s dall'ultima fetch.
// ============================================================
void DecodiumBridge::fetchPskHeardBy()
{
    fetchPskReporterSnapshot(m_pskReporterTimeSpanMinutes, false, false);
}

void DecodiumBridge::fetchPskMapSpots()
{
    fetchPskReporterSnapshot(m_pskMapSpotWindowMinutes, true, false);
}

void DecodiumBridge::refreshPskMapSpots()
{
    fetchPskReporterSnapshot(m_pskMapSpotWindowMinutes, true, true);
}

void DecodiumBridge::fetchPskReporterSnapshot(int spanMinutes, bool mapRequest, bool force)
{
    static constexpr qsizetype kPskHeardByMaxReplyBytes = 1024 * 1024;
    static constexpr qsizetype kPskHeardByMaxTagAttrs = 4096;
    static constexpr int kPskHeardByMaxRows = 50;
    static constexpr int kPskHeardByMaxCallLen = 18;
    static constexpr int kPskHeardByMaxGridLen = 8;

    if (offlineMode()) {
        emit statusMessage(mapRequest
            ? QStringLiteral("Offline: PSK Reporter map fetch disabled")
            : QStringLiteral("Offline: PSK Reporter fetch disabled"));
        return;
    }
    if (m_pskHeardByFetching || m_pskMapSpotFetching) {
        bridgeLog(QStringLiteral("PSK Reporter: fetch gia' in corso, ignoro"));
        return;
    }
    QString const call = m_callsign.trimmed().toUpper();
    if (call.isEmpty()) {
        bridgeLog(QStringLiteral("PSK heard-by: callsign vuoto, no-op"));
        return;
    }
    // Rate-limit each independent consumer to one periodic request per minute.
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64& lastFetchMs = mapRequest ? m_pskMapSpotLastFetchMs : m_pskHeardByLastFetchMs;
    if (!force && lastFetchMs > 0 && (nowMs - lastFetchMs) < 60000) {
        bridgeLog(QStringLiteral("PSK %1: rate-limit 60s (%2ms fa), ignoro")
                      .arg(mapRequest ? QStringLiteral("map") : QStringLiteral("heard-by"))
                      .arg(nowMs - lastFetchMs));
        return;
    }
    lastFetchMs = nowMs;
    if (mapRequest) {
        m_pskMapSpotFetching = true;
        emit pskMapSpotFetchingChanged();
    } else {
        m_pskHeardByFetching = true;
        emit pskHeardByFetchingChanged();
    }

    QString const myGrid = m_grid.trimmed();

    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    QUrl url(QStringLiteral("https://retrieve.pskreporter.info/query"));
    QUrlQuery q;
    spanMinutes = qBound(5, ((qBound(5, spanMinutes, 60) + 2) / 5) * 5, 60);
    q.addQueryItem(QStringLiteral("senderCallsign"), call);     // il MIO call = chi trasmette
    q.addQueryItem(QStringLiteral("flowStartSeconds"), QString::number(-spanMinutes * 60));
    q.addQueryItem(QStringLiteral("rptlimit"), QStringLiteral("50"));
    q.addQueryItem(QStringLiteral("rronly"), QStringLiteral("1"));
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", mapRequest
                     ? "Decodium/4.0 (+pskreporter live map)"
                     : "Decodium/4.0 (+pskreporter heard-by)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = nam->get(req);

    QTimer* timeout = new QTimer(this);
    timeout->setSingleShot(true);
    timeout->setInterval(10000);
    connect(timeout, &QTimer::timeout, reply, [reply]() {
        if (reply && reply->isRunning()) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, timeout, &QTimer::deleteLater);
    timeout->start();

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, call, myGrid, spanMinutes, mapRequest]() {
        QVariant const contentLength = reply->header(QNetworkRequest::ContentLengthHeader);
        bool const declaredTooLarge =
            contentLength.isValid()
            && contentLength.toLongLong() > static_cast<qlonglong>(kPskHeardByMaxReplyBytes);
        QByteArray data;
        bool tooLarge = declaredTooLarge;
        if (!declaredTooLarge) {
            data = reply->read(static_cast<qint64>(kPskHeardByMaxReplyBytes + 1));
            tooLarge = data.size() > kPskHeardByMaxReplyBytes || reply->bytesAvailable() > 0;
        }
        QNetworkReply::NetworkError const err = reply->error();
        QString const errStr = reply->errorString();
        bool const validSnapshot = err == QNetworkReply::NoError && !tooLarge;
        reply->deleteLater();
        nam->deleteLater();

        QVariantList rows;
        QSet<QString> dxccSet;
        double maxKm = 0.0;

        if (validSnapshot) {
            QString const xml = QString::fromUtf8(data);
            // Ogni spot e' un <receptionReport ... /> con attributi in ordine
            // variabile: catturiamo l'intero tag e poi i singoli attributi.
            QRegularExpression reTag(QStringLiteral("<receptionReport\\b([^>]*)/?>"));
            QRegularExpression reRx(QStringLiteral("\\breceiverCallsign=\"([^\"]*)\""));
            QRegularExpression reLoc(QStringLiteral("\\breceiverLocator=\"([^\"]*)\""));
            QRegularExpression reSnr(QStringLiteral("\\bsNR=\"(-?[0-9]+)\""));
            QRegularExpression reFreq(QStringLiteral("\\bfrequency=\"([0-9]+)\""));
            QRegularExpression reCall(QStringLiteral("^[A-Z0-9/]{2,18}$"));
            QRegularExpression reGrid(QStringLiteral("^[A-R]{2}[0-9]{2}([A-X]{2})?([0-9]{2})?$"),
                                      QRegularExpression::CaseInsensitiveOption);
            QSet<QString> seenReceivers;   // dedup per receiverCallsign (spot multipli)
            auto it = reTag.globalMatch(xml);
            while (it.hasNext() && rows.size() < kPskHeardByMaxRows) {
                QString const attrs = it.next().captured(1);
                if (attrs.size() > kPskHeardByMaxTagAttrs) {
                    continue;
                }

                QString rxCall;
                auto mRx = reRx.match(attrs);
                if (mRx.hasMatch()) rxCall = mRx.captured(1).trimmed().toUpper();
                if (rxCall.isEmpty()) continue;
                if (rxCall.size() > kPskHeardByMaxCallLen || !reCall.match(rxCall).hasMatch()) continue;
                if (seenReceivers.contains(rxCall)) continue;   // tieni solo 1 riga/ricevitore
                seenReceivers.insert(rxCall);

                QString rxGrid;
                auto mLoc = reLoc.match(attrs);
                if (mLoc.hasMatch()) rxGrid = mLoc.captured(1).trimmed().toUpper();
                if (rxGrid.size() > kPskHeardByMaxGridLen || (!rxGrid.isEmpty() && !reGrid.match(rxGrid).hasMatch())) {
                    rxGrid.clear();
                }

                int snr = 0; bool hasSnr = false;
                auto mSnr = reSnr.match(attrs);
                if (mSnr.hasMatch()) { snr = mSnr.captured(1).toInt(); hasSnr = true; }

                qint64 freqHz = 0;
                auto mFreq = reFreq.match(attrs);
                if (mFreq.hasMatch()) freqHz = mFreq.captured(1).toLongLong();

                // Distanza: riusa calcDistance (grid2deg + azdist). -1 se grid assente.
                double distKm = -1.0;
                if (!rxGrid.isEmpty() && !myGrid.isEmpty()) {
                    double const d = calcDistance(myGrid, rxGrid);
                    if (d >= 0.0) {
                        distKm = d;
                        if (d > maxKm) maxKm = d;
                    }
                }

                // DXCC: riusa il lookup cty.dat esistente.
                QString dxccName;
                QString continent;
                int cqZone = 0;
                int ituZone = 0;
                if (m_dxccLookup && m_dxccLookup->isLoaded()) {
                    DxccEntity const ent = m_dxccLookup->lookup(rxCall);
                    dxccName = ent.name;
                    continent = ent.continent;
                    cqZone = ent.cqZone;
                    ituZone = ent.ituZone;
                    if (!dxccName.isEmpty()) dxccSet.insert(dxccName);
                }

                QVariantMap row;
                row.insert(QStringLiteral("call"), rxCall);
                row.insert(QStringLiteral("grid"), rxGrid);
                row.insert(QStringLiteral("snr"), hasSnr ? QVariant(snr) : QVariant());
                row.insert(QStringLiteral("freq"), freqHz);
                row.insert(QStringLiteral("distKm"), distKm);
                row.insert(QStringLiteral("dxcc"), dxccName);
                row.insert(QStringLiteral("continent"), continent);
                row.insert(QStringLiteral("cqZone"), cqZone);
                row.insert(QStringLiteral("ituZone"), ituZone);
                rows.append(row);
            }
            bridgeLog(QStringLiteral("PSK %1 '%2': span=%3min receivers=%4 DXCC=%5 maxKm=%6")
                          .arg(mapRequest ? QStringLiteral("map") : QStringLiteral("heard-by"))
                          .arg(call)
                          .arg(spanMinutes)
                          .arg(rows.size())
                          .arg(dxccSet.size())
                          .arg(static_cast<int>(maxKm)));
        } else if (tooLarge) {
            bridgeLog(QStringLiteral("PSK %1 '%2': risposta troppo grande, scartata")
                          .arg(mapRequest ? QStringLiteral("map") : QStringLiteral("heard-by"), call));
        } else {
            bridgeLog(QStringLiteral("PSK %1 '%2': network error: %3")
                          .arg(mapRequest ? QStringLiteral("map") : QStringLiteral("heard-by"),
                               call, errStr));
        }

        if (mapRequest) {
            if (m_mapIntelligenceService && validSnapshot) {
                m_mapIntelligenceService->replacePskHeardBySpots(rows, call, myGrid);
            }
            m_pskMapSpotFetching = false;
            emit pskMapSpotFetchingChanged();
        } else {
            m_pskHeardByList = rows;
            m_pskHeardByCount = static_cast<int>(rows.size());
            m_pskHeardByDxccCount = static_cast<int>(dxccSet.size());
            m_pskHeardByMaxKm = maxKm;
            m_pskHeardByFetching = false;
            emit pskHeardByChanged();
            emit pskHeardByFetchingChanged();
        }
    });
}
