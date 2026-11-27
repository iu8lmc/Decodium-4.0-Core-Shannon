QVariantList DecodiumBridge::sstvExistingQsoChoices(
    const QString& search, int maximumRows)
{
#if DECODIUM_HAS_SSTV
    const int limit = qBound(1, maximumRows, 100);
    const QString path = effectiveAdifLogPath();
    const QFileInfo info(path);
    const QDateTime modified = info.exists()
        ? info.lastModified() : QDateTime {};
    const qint64 size = info.exists() ? info.size() : 0;
    QVariantList rows;
    bool hadReadyCache = false;
    {
        QMutexLocker locker(&m_qsoSearchCacheMutex);
        hadReadyCache = m_qsoSearchCacheReady;
        if (m_qsoSearchCacheReady
            && m_qsoSearchCachePath == path
            && m_qsoSearchCacheModified == modified
            && m_qsoSearchCacheSize == size) {
            rows = m_qsoSearchCacheRows;
        }
    }
    if (!hadReadyCache || rows.isEmpty()) {
        // An empty active logbook is a valid ready snapshot. Distinguish it
        // from a cache miss without ever parsing ADIF on the QML thread.
        bool cacheCurrent = false;
        {
            QMutexLocker locker(&m_qsoSearchCacheMutex);
            cacheCurrent = m_qsoSearchCacheReady
                && m_qsoSearchCachePath == path
                && m_qsoSearchCacheModified == modified
                && m_qsoSearchCacheSize == size;
        }
        if (!cacheCurrent) {
            m_sstvIssuedExistingQsoIds.clear();
            if (hadReadyCache) {
                invalidateQsoSearchCache();
            }
            warmLogCacheAsync();
            return {};
        }
    }

    const quint64 generation =
        m_qsoSearchCacheGeneration.load(std::memory_order_relaxed);
    if (m_sstvIssuedExistingQsoGeneration != generation) {
        m_sstvIssuedExistingQsoIds.clear();
        m_sstvIssuedExistingQsoGeneration = generation;
    }
    const QString needle = search.trimmed().left(128).toUpper();
    QVariantList choices;
    choices.reserve(std::min(limit, static_cast<int>(rows.size())));
    for (const QVariant& rowValue : rows) {
        QVariantMap row = rowValue.toMap();
        if (!needle.isEmpty()) {
            const QString searchable = QStringLiteral("%1\n%2\n%3\n%4\n%5\n%6")
                .arg(row.value(QStringLiteral("call")).toString(),
                     row.value(QStringLiteral("grid")).toString(),
                     row.value(QStringLiteral("comment")).toString(),
                     row.value(QStringLiteral("dateTime")).toString(),
                     row.value(QStringLiteral("band")).toString(),
                     row.value(QStringLiteral("mode")).toString())
                .toUpper();
            if (!searchable.contains(needle)) {
                continue;
            }
        }
        QString error;
        const QString qsoId = decodium::sstv::SstvQsoLog::associationIdForExistingQso(
            row.value(QStringLiteral("call")).toString(),
            row.value(QStringLiteral("dateTime")).toString(),
            row.value(QStringLiteral("mode")).toString(),
            row.value(QStringLiteral("band")).toString(), &error);
        if (qsoId.isEmpty()) {
            continue;
        }
        row.insert(QStringLiteral("qsoId"), qsoId);
        choices.append(row);
        m_sstvIssuedExistingQsoIds.insert(qsoId);
        if (choices.size() >= limit) {
            break;
        }
    }
    constexpr qsizetype kMaximumIssuedChoices = 4'096;
    if (m_sstvIssuedExistingQsoIds.size() > kMaximumIssuedChoices) {
        m_sstvIssuedExistingQsoIds.clear();
        for (const QVariant& choice : choices) {
            m_sstvIssuedExistingQsoIds.insert(
                choice.toMap().value(QStringLiteral("qsoId")).toString());
        }
    }
    return choices;
#else
    Q_UNUSED(search)
    Q_UNUSED(maximumRows)
    return {};
#endif
}

#if DECODIUM_HAS_SSTV
bool DecodiumBridge::isSstvExistingQsoChoiceCurrent(
    const QString& qsoId) const
{
    const QString normalized = qsoId.trimmed();
    const quint64 generation =
        m_qsoSearchCacheGeneration.load(std::memory_order_relaxed);
    if (normalized.isEmpty()
        || m_sstvIssuedExistingQsoGeneration != generation
        || !m_sstvIssuedExistingQsoIds.contains(normalized)) {
        return false;
    }

    const QString path = effectiveAdifLogPath();
    const QFileInfo info(path);
    const QDateTime modified = info.exists()
        ? info.lastModified() : QDateTime {};
    const qint64 size = info.exists() ? info.size() : 0;
    QMutexLocker locker(&m_qsoSearchCacheMutex);
    return m_qsoSearchCacheReady
        && m_qsoSearchCachePath == path
        && m_qsoSearchCacheModified == modified
        && m_qsoSearchCacheSize == size;
}
#endif

