template <typename SettingReader>
static QStringList decodeFilterTerms(SettingReader const& readSetting,
                                     const QString& prefix,
                                     int count)
{
    QStringList terms;
    terms.reserve(count);
    for (int i = 1; i <= count; ++i) {
        QString const term =
            readSetting(prefix + QString::number(i), QString())
                .toString()
                .trimmed()
                .toUpper();
        if (!term.isEmpty() && !terms.contains(term)) {
            terms.append(term);
        }
    }
    return terms;
}

static QString canonicalDecodeFilterTerritoryTerm(QString term)
{
    term = term.trimmed().toUpper();
    term.replace(QLatin1Char('/'), QLatin1Char(' '));
    term.replace(QLatin1Char('-'), QLatin1Char(' '));
    term.replace(QLatin1Char('_'), QLatin1Char(' '));
    term.replace(QLatin1Char('.'), QLatin1Char(' '));
    term = term.simplified();
    if (term.isEmpty()) {
        return {};
    }

    auto hasToken = [&term](QString const& token) {
        QString const paddedTerm = QStringLiteral(" ") + term + QStringLiteral(" ");
        QString const paddedToken = QStringLiteral(" ") + token + QStringLiteral(" ");
        return term == token || paddedTerm.contains(paddedToken);
    };

    if (hasToken(QStringLiteral("EU")) || term.contains(QStringLiteral("EUROPE"))
        || term.contains(QStringLiteral("EUROPA"))) {
        return QStringLiteral("EU");
    }
    if (hasToken(QStringLiteral("AF")) || term.contains(QStringLiteral("AFRICA"))) {
        return QStringLiteral("AF");
    }
    if (hasToken(QStringLiteral("OC")) || term.contains(QStringLiteral("OCEANIA"))) {
        return QStringLiteral("OC");
    }
    if (hasToken(QStringLiteral("AS")) || term.contains(QStringLiteral("ASIA"))) {
        return QStringLiteral("AS");
    }
    if (hasToken(QStringLiteral("NA")) || term.contains(QStringLiteral("NORTH AMERICA"))
        || term.contains(QStringLiteral("N AMERICA"))) {
        return QStringLiteral("NA");
    }
    if (hasToken(QStringLiteral("SA")) || term.contains(QStringLiteral("SOUTH AMERICA"))
        || term.contains(QStringLiteral("S AMERICA"))) {
        return QStringLiteral("SA");
    }

    return {};
}

template <typename SettingReader>
static QStringList decodeFilterTerritoryTerms(SettingReader const& readSetting)
{
    QStringList terms;
    terms.reserve(6);
    for (int i = 1; i <= 6; ++i) {
        QString const term = canonicalDecodeFilterTerritoryTerm(
            readSetting(QStringLiteral("Territory") + QString::number(i),
                        QString()).toString());
        if (!term.isEmpty() && !terms.contains(term)) {
            terms.append(term);
        }
    }
    return terms;
}

DecodeUserFilterConfig DecodiumBridge::readDecodeUserFilterConfig() const
{
    DecodeUserFilterConfig filters;
    if (filtersBypassed()) {
        return filters;
    }

    // Keep getSetting()'s profile-aware precedence, but open each backing
    // store only once for the complete filter snapshot. Constructing two or
    // more QSettings objects per key was a visible decode-delivery spike on
    // Windows and also performed repeated filesystem/profile discovery.
    QSettings legacySettings(legacyIniPath(), QSettings::IniFormat);
    legacySettings.beginGroup(QStringLiteral("MultiSettings"));
    QString legacyCurrentGroup = legacySettings.value(QStringLiteral("CurrentName")).toString();
    legacySettings.endGroup();
    if (legacyCurrentGroup.isEmpty()) {
        legacyCurrentGroup = QStringLiteral("Default");
    }
    QStringList legacyGroups {
        legacyCurrentGroup,
        QStringLiteral("Configuration"),
        QStringLiteral("Default")
    };
    legacyGroups.removeDuplicates();

    QSettings profileSettings(QSettings::IniFormat, QSettings::UserScope,
                              QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    beginConfiguredBridgeSettingsGroup(profileSettings);

    auto readLegacySetting = [&legacySettings, &legacyGroups](QString const& key) {
        QVariant value = legacySettings.value(key);
        if (value.isValid()) {
            return value;
        }
        for (QString const& group : legacyGroups) {
            legacySettings.beginGroup(group);
            value = legacySettings.value(key);
            legacySettings.endGroup();
            if (value.isValid()) {
                return value;
            }
        }
        return QVariant {};
    };

    auto readSetting = [&readLegacySetting, &profileSettings](QString const& key,
                                                              QVariant const& fallback) {
        QString const alias = aliasedBridgeSettingKey(key);
        if (!decodium::activeSettingsProfileName().isEmpty()) {
            if (profileSettings.contains(key)) return profileSettings.value(key);
            if (alias != key && profileSettings.contains(alias)) return profileSettings.value(alias);
        }
        QVariant value = readLegacySetting(key);
        if (!value.isValid() && alias != key) {
            value = readLegacySetting(alias);
        }
        if (!value.isValid()) {
            value = profileSettings.value(key);
        }
        if (!value.isValid() && alias != key) {
            value = profileSettings.value(alias);
        }
        return value.isValid() ? value : fallback;
    };

    filters.waitPounceOnly =
        readSetting(QStringLiteral("FiltersForWaitAndPounceOnly"), false).toBool();
    filters.callingOnly =
        readSetting(QStringLiteral("FiltersForWord2"), false).toBool();
    filters.blacklistEnabled =
        readSetting(QStringLiteral("Blacklisted"), false).toBool();
    filters.whitelistEnabled =
        readSetting(QStringLiteral("Whitelisted"), false).toBool();
    filters.alwaysPassEnabled =
        readSetting(QStringLiteral("AlwaysPass"), false).toBool();
    filters.blacklistTerms = decodeFilterTerms(readSetting, QStringLiteral("Blacklist"), 12);
    filters.whitelistTerms = decodeFilterTerms(readSetting, QStringLiteral("Whitelist"), 12);
    filters.passTerms = decodeFilterTerms(readSetting, QStringLiteral("Pass"), 12);
    filters.territoryTerms = decodeFilterTerritoryTerms(readSetting);
    return filters;
}

