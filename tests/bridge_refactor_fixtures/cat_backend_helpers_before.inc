static QString normalizedCatSplitMode(QString const& value)
{
    QString const normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("rig")) {
        return QStringLiteral("rig");
    }
    if (normalized == QStringLiteral("emulate")
        || normalized == QStringLiteral("fake")
        || normalized == QStringLiteral("fake it")) {
        return QStringLiteral("emulate");
    }
    return QStringLiteral("none");
}
QString decodiumBridgeNormalizedCatSplitMode(const QString& value) { return normalizedCatSplitMode(value); }


static inline bool isHamlibFamilyBackend(QString const& backend)
{
    QString const normalized = backend.trimmed().toLower();
    return normalized == QStringLiteral("hamlib") || normalized == QStringLiteral("tci");
}

bool decodiumBridgeIsHamlibFamilyBackend(QString const& backend)
{
    return isHamlibFamilyBackend(backend);
}

static inline bool isNetworkRigctlCat(QString const& backend, QObject const* manager)
{
    if (backend.trimmed().compare(QStringLiteral("hamlib"), Qt::CaseInsensitive) != 0
        || !manager) {
        return false;
    }
    return manager->property("portType").toString().trimmed()
               .compare(QStringLiteral("network"), Qt::CaseInsensitive) == 0
        && manager->property("rigName").toString()
               .contains(QStringLiteral("rigctl"), Qt::CaseInsensitive);
}

bool decodiumBridgeIsNetworkRigctlCat(const QString& backend, const QObject* manager)
{
    return isNetworkRigctlCat(backend, manager);
}

static inline bool catSignalMatchesBackend(QString const& activeBackend, QString const& signalBackend)
{
    QString const active = activeBackend.trimmed().toLower();
    QString const signal = signalBackend.trimmed().toLower();
    return active == signal
        || (active == QStringLiteral("tci") && signal == QStringLiteral("hamlib"));
}

bool decodiumBridgePttSignalMatches(const QString& active, const QString& source)
{
    return catSignalMatchesBackend(active, source);
}

