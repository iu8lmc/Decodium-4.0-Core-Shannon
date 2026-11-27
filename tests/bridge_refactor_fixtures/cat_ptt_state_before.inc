QString DecodiumBridge::activePttMethod() const
{
    QObject const* const manager = catManagerObj();
    QString const method = manager
        ? manager->property("pttMethod").toString().trimmed().toUpper()
        : QString();
    return method.isEmpty() ? QStringLiteral("CAT") : method;
}

bool DecodiumBridge::activeCatReportsPttActive() const
{
    if (m_catBackend == QStringLiteral("cat4om")) {
        return m_cat4OmCat && m_cat4OmCat->pttActive();
    }
    if (m_catBackend == QStringLiteral("omnirig")) {
        return m_omniRigCat && m_omniRigCat->pttActive();
    }
    return m_hamlibCat && m_hamlibCat->pttActive();
}

