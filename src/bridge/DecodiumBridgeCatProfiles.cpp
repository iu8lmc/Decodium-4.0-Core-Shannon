#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QEventLoop>
#include <QRegularExpression>
#include <QSettings>
#include <algorithm>

void decodiumBridgeCatProfileLog(const QString& message);

static QString const kCatProfileRootGroup = QStringLiteral("CAT_Profiles");
static QString const kCatProfileActiveKey = QStringLiteral("CAT_ProfileActive");

// "decoport" e' una modalita', non una configurazione: quando la radio remota
// fa da CAT, cio' che va salvato resta il backend che l'utente ha scelto. Senza
// questo, salvare un profilo mentre si usa una radio in rete riscriverebbe di
// nascosto la sua impostazione in "hamlib".
static QString normalizedCatBackendForSettings(QString value)
{
    // Il backend "native" (QSerialPort) non esiste piu': un valore salvato in
    // passato non e' riconosciuto e torna "hamlib".
    value = value.trimmed().toLower();
    if (value != QStringLiteral("hamlib")
        && value != QStringLiteral("tci")
        && value != QStringLiteral("cat4om")
        && value != QStringLiteral("omnirig")) {
        return QStringLiteral("hamlib");
    }
    return value;
}

static QString normalizedCatProfileName(QString value)
{
    value = value.trimmed();
    value.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    return value.left(80);
}

static QString catProfileStorageKey(QString const& name)
{
    QByteArray const digest =
        QCryptographicHash::hash(name.trimmed().toUtf8().toLower(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("p_") + QString::fromLatin1(digest.left(24));
}

static QStringList catProfileSnapshotRootKeys()
{
    return {
        QStringLiteral("catBackend"),
        QStringLiteral("CATMode"),
        QStringLiteral("TXAudioSource"),
        QStringLiteral("audioInputDevice"),
        QStringLiteral("audioOutputDevice"),
        QStringLiteral("audioInputChannel"),
        QStringLiteral("audioOutputChannel"),
        QStringLiteral("CheckSWR"),
        QStringLiteral("PWRandSWR"),
        QStringLiteral("SWRStopThreshold")
    };
}

static QStringList catProfileSnapshotGroups()
{
    return {
        QStringLiteral("CAT_Native"),
        QStringLiteral("Transceiver"),
        QStringLiteral("CAT_Cat4OM"),
        QStringLiteral("CAT_OmniRig")
    };
}

static QVariantMap readSettingsGroupFlat(QSettings& settings, QString const& group)
{
    QVariantMap values;
    settings.beginGroup(group);
    for (QString const& key : settings.allKeys()) {
        values.insert(key, settings.value(key));
    }
    settings.endGroup();
    return values;
}

static void writeSettingsGroupFlat(QSettings& settings,
                                   QString const& group,
                                   QVariantMap const& values)
{
    settings.beginGroup(group);
    settings.remove(QString());
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
}

static QString findCatProfileStorageKey(QSettings& settings, QString const& profileName)
{
    QString const wanted = normalizedCatProfileName(profileName);
    if (wanted.isEmpty()) {
        return {};
    }

    settings.beginGroup(kCatProfileRootGroup);
    QString found;
    for (QString const& key : settings.childGroups()) {
        settings.beginGroup(key);
        QString const name = normalizedCatProfileName(settings.value(QStringLiteral("name")).toString());
        settings.endGroup();
        if (0 == name.compare(wanted, Qt::CaseInsensitive)) {
            found = key;
            break;
        }
    }
    settings.endGroup();
    return found;
}

static QStringList catProfileNamesFromSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    QStringList names;
    settings.beginGroup(kCatProfileRootGroup);
    for (QString const& key : settings.childGroups()) {
        settings.beginGroup(key);
        QString const name = normalizedCatProfileName(settings.value(QStringLiteral("name")).toString());
        settings.endGroup();
        if (!name.isEmpty() && !names.contains(name, Qt::CaseInsensitive)) {
            names << name;
        }
    }
    settings.endGroup();
    std::sort(names.begin(), names.end(), [](QString const& a, QString const& b) {
        return QString::localeAwareCompare(a, b) < 0;
    });
    return names;
}

QString DecodiumBridge::activeCatProfile() const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    return normalizedCatProfileName(settings.value(kCatProfileActiveKey).toString());
}

QStringList DecodiumBridge::catProfileList() const
{
    return catProfileNamesFromSettings();
}

QString DecodiumBridge::suggestedCatProfileName() const
{
    QObject const* cat = catManagerObj();
    QString rig = cat ? cat->property("rigName").toString().trimmed() : QString {};
    if (rig.isEmpty()) {
        rig = m_catBackend.trimmed().toUpper();
    }

    QString backend = m_catBackend.trimmed().toUpper();
    if (backend == QStringLiteral("HAMLIB")) backend = QStringLiteral("Hamlib");
    else if (backend == QStringLiteral("NATIVE")) backend = QStringLiteral("Native");
    else if (backend == QStringLiteral("TCI")) backend = QStringLiteral("TCI");
    else if (backend == QStringLiteral("CAT4OM")) backend = QStringLiteral("Cat4OM");
    else if (backend == QStringLiteral("OMNIRIG")) backend = QStringLiteral("OmniRig");

    QString name = QStringLiteral("%1 %2").arg(rig, backend).trimmed();
    return normalizedCatProfileName(name.isEmpty() ? QStringLiteral("Radio Profile") : name);
}

bool DecodiumBridge::saveCatProfile(const QString& rawName)
{
    QString const name = normalizedCatProfileName(rawName);
    if (name.isEmpty()) {
        emit errorMessage(QStringLiteral("CAT profile: enter a profile name"));
        return false;
    }

    if (m_cat4OmCat) m_cat4OmCat->saveSettings();
    if (m_hamlibCat) m_hamlibCat->saveSettings();
    if (m_omniRigCat) m_omniRigCat->saveSettings();

    QSettings source(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(source);
    source.setValue(QStringLiteral("catBackend"), normalizedCatBackendForSettings(catBackendForPersistence()));

    QVariantMap rootValues;
    for (QString const& key : catProfileSnapshotRootKeys()) {
        if (key == QStringLiteral("catBackend")) {
            rootValues.insert(key, normalizedCatBackendForSettings(m_catBackend));
        } else if (key == QStringLiteral("CATMode")) {
            rootValues.insert(key, source.value(key, QStringLiteral("Data/Pkt")));
        } else if (key == QStringLiteral("TXAudioSource")) {
            rootValues.insert(key, source.value(key, QStringLiteral("Rear/Data")));
        } else if (key == QStringLiteral("audioInputDevice")) {
            rootValues.insert(key, source.value(key, m_audioInputDevice));
        } else if (key == QStringLiteral("audioOutputDevice")) {
            rootValues.insert(key, source.value(key, m_audioOutputDevice));
        } else if (key == QStringLiteral("audioInputChannel")) {
            rootValues.insert(key, source.value(key, m_audioInputChannel));
        } else if (key == QStringLiteral("audioOutputChannel")) {
            rootValues.insert(key, source.value(key, m_audioOutputChannel));
        } else if (key == QStringLiteral("CheckSWR") || key == QStringLiteral("PWRandSWR")) {
            rootValues.insert(key, source.value(key, false));
        } else if (key == QStringLiteral("SWRStopThreshold")) {
            rootValues.insert(key, source.value(key, 2.5));
        }
    }

    QMap<QString, QVariantMap> groupValues;
    for (QString const& group : catProfileSnapshotGroups()) {
        groupValues.insert(group, readSettingsGroupFlat(source, group));
    }

    QString const key = catProfileStorageKey(name);
    QSettings dest(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(dest);
    dest.beginGroup(kCatProfileRootGroup);
    dest.beginGroup(key);
    dest.remove(QString());
    dest.setValue(QStringLiteral("name"), name);
    dest.setValue(QStringLiteral("catBackend"), normalizedCatBackendForSettings(catBackendForPersistence()));
    if (QObject const* cat = catManagerObj()) {
        dest.setValue(QStringLiteral("rigName"), cat->property("rigName").toString());
    }
    dest.setValue(QStringLiteral("updatedUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    writeSettingsGroupFlat(dest, QStringLiteral("Root"), rootValues);
    dest.beginGroup(QStringLiteral("Groups"));
    for (auto it = groupValues.constBegin(); it != groupValues.constEnd(); ++it) {
        writeSettingsGroupFlat(dest, it.key(), it.value());
    }
    dest.endGroup();
    dest.endGroup();
    dest.endGroup();
    dest.setValue(kCatProfileActiveKey, name);
    dest.sync();

    emit catProfilesChanged();
    emit activeCatProfileChanged();
    emit statusMessage(QStringLiteral("CAT profile saved: %1").arg(name));
    return true;
}

bool DecodiumBridge::applyCatProfileSnapshotToSettings(const QString& rawName, bool setActiveProfile)
{
    QString const requestedName = normalizedCatProfileName(rawName);
    if (requestedName.isEmpty()) {
        return false;
    }

    QSettings source(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(source);
    QString const key = findCatProfileStorageKey(source, requestedName);
    if (key.isEmpty()) {
        return false;
    }

    QString storedName;
    QVariantMap rootValues;
    QMap<QString, QVariantMap> groupValues;
    source.beginGroup(kCatProfileRootGroup);
    source.beginGroup(key);
    storedName = normalizedCatProfileName(source.value(QStringLiteral("name"), requestedName).toString());
    rootValues = readSettingsGroupFlat(source, QStringLiteral("Root"));
    for (QString const& group : catProfileSnapshotGroups()) {
        groupValues.insert(group, readSettingsGroupFlat(source, QStringLiteral("Groups/") + group));
    }
    source.endGroup();
    source.endGroup();

    if (storedName.isEmpty()) {
        storedName = requestedName;
    }

    if (rootValues.contains(QStringLiteral("catBackend"))) {
        rootValues.insert(QStringLiteral("catBackend"),
                          normalizedCatBackendForSettings(rootValues.value(QStringLiteral("catBackend")).toString()));
    }

    QSettings dest(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(dest);
    for (auto it = rootValues.constBegin(); it != rootValues.constEnd(); ++it) {
        dest.setValue(it.key(), it.value());
    }
    for (auto it = groupValues.constBegin(); it != groupValues.constEnd(); ++it) {
        writeSettingsGroupFlat(dest, it.key(), it.value());
    }
    if (setActiveProfile) {
        dest.setValue(kCatProfileActiveKey, storedName);
    }
    dest.sync();
    return true;
}

// Il profilo CAT attivo NON viene piu' riversato nelle impostazioni a ogni
// avvio. Le chiavi dello snapshot -- catBackend, CATMode, TXAudioSource, le
// soglie SWR e i gruppi per-backend -- sono tutte impostazioni VIVE: le scrive
// saveSettings() e le riscrive ogni manager con il proprio saveSettings(). Il
// profilo ne e' una COPIA, presa nel momento in cui lo si salva.
//
// Riapplicarla all'avvio non poteva quindi aggiungere nulla: poteva solo
// ANNULLARE in silenzio le modifiche fatte dopo. E' il difetto per cui, con un
// profilo salvato mentre il backend era "hamlib", scegliere TCI non reggeva:
// al riavvio lo snapshot rimetteva hamlib, l'audio TCI non si armava mai e
// nessuna forzatura poteva funzionare, perche' il valore veniva riscritto da
// dentro. Valeva per porta, velocita' e modo CAT allo stesso modo.
//
// Il profilo resta segnato come attivo e viene applicato quando lo si CARICA
// (loadCatProfile), cioe' quando l'utente lo chiede davvero. Qui ci si limita
// a segnalare nel log se il profilo e' divergente, invece di sovrascrivere.
void DecodiumBridge::noteActiveCatProfileAtStartup()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    QString const name = normalizedCatProfileName(settings.value(kCatProfileActiveKey).toString());
    if (name.isEmpty()) {
        return;
    }

    QString const liveBackend = normalizedCatBackendForSettings(
        settings.value(QStringLiteral("catBackend")).toString());

    QString const key = findCatProfileStorageKey(settings, name);
    if (key.isEmpty()) {
        decodiumBridgeCatProfileLog(QStringLiteral("CAT profile attivo ma senza snapshot: %1 (backend in uso: %2)")
                      .arg(name, liveBackend));
        return;
    }

    settings.beginGroup(kCatProfileRootGroup);
    settings.beginGroup(key);
    QString const profileBackend = normalizedCatBackendForSettings(
        settings.value(QStringLiteral("catBackend")).toString());
    settings.endGroup();
    settings.endGroup();

    if (profileBackend != liveBackend) {
        decodiumBridgeCatProfileLog(QStringLiteral("CAT profile %1: lo snapshot dice %2 ma vale il backend in uso %3"
                                 " (il profilo si applica solo quando lo si carica)")
                      .arg(name, profileBackend, liveBackend));
    } else {
        decodiumBridgeCatProfileLog(QStringLiteral("CAT profile attivo: %1 (backend %2)").arg(name, liveBackend));
    }
}

bool DecodiumBridge::loadCatProfile(const QString& rawName)
{
    QString const name = normalizedCatProfileName(rawName);
    if (name.isEmpty()) {
        emit errorMessage(QStringLiteral("CAT profile: select a profile to load"));
        return false;
    }
    if (sstvTxActive()) {
        cancelSstvTx();
        emit errorMessage(tr("SSTV TX is releasing PTT: load the CAT profile again after transmission has stopped"));
        return false;
    }
    if (!applyCatProfileSnapshotToSettings(name, true)) {
        emit errorMessage(QStringLiteral("CAT profile not found: %1").arg(name));
        return false;
    }

    halt();
    if (m_cat4OmCat) m_cat4OmCat->disconnectRig();
    if (m_omniRigCat) m_omniRigCat->disconnectRig();
    if (m_hamlibCat) m_hamlibCat->disconnectRig();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

    m_catConnected = false;
    m_catRigName.clear();
    m_catMode.clear();
    emit catConnectedChanged();
    emit catRigNameChanged();
    emit catModeChanged();
    refreshPskReporterLocalStation();
    updateRigTelemetry(0.0, 0.0, 0.0);

    reloadBridgeSettingsFromPersistentStore();
    emit catProfilesChanged();
    emit activeCatProfileChanged();
    emit statusMessage(QStringLiteral("CAT profile loaded: %1. Connect manually to the radio.").arg(activeCatProfile()));
    return true;
}

bool DecodiumBridge::deleteCatProfile(const QString& rawName)
{
    QString const name = normalizedCatProfileName(rawName);
    if (name.isEmpty()) {
        emit errorMessage(QStringLiteral("CAT profile: select a profile to delete"));
        return false;
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    QString const key = findCatProfileStorageKey(settings, name);
    if (key.isEmpty()) {
        emit errorMessage(QStringLiteral("CAT profile not found: %1").arg(name));
        return false;
    }

    settings.beginGroup(kCatProfileRootGroup);
    settings.remove(key);
    settings.endGroup();
    if (0 == activeCatProfile().compare(name, Qt::CaseInsensitive)) {
        settings.remove(kCatProfileActiveKey);
    }
    settings.sync();
    emit catProfilesChanged();
    emit activeCatProfileChanged();
    emit statusMessage(QStringLiteral("CAT profile deleted: %1").arg(name));
    return true;
}

bool DecodiumBridge::renameCatProfile(const QString& rawOldName, const QString& rawNewName)
{
    QString const oldName = normalizedCatProfileName(rawOldName);
    QString const newName = normalizedCatProfileName(rawNewName);
    if (oldName.isEmpty() || newName.isEmpty()) {
        emit errorMessage(QStringLiteral("CAT profile: enter a valid profile name"));
        return false;
    }
    if (0 == oldName.compare(newName, Qt::CaseInsensitive)) {
        return true;
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    QString const oldKey = findCatProfileStorageKey(settings, oldName);
    if (oldKey.isEmpty()) {
        emit errorMessage(QStringLiteral("CAT profile not found: %1").arg(oldName));
        return false;
    }
    if (!findCatProfileStorageKey(settings, newName).isEmpty()) {
        emit errorMessage(QStringLiteral("CAT profile already exists: %1").arg(newName));
        return false;
    }

    settings.beginGroup(kCatProfileRootGroup);
    QVariantMap profileValues = readSettingsGroupFlat(settings, oldKey);
    settings.remove(oldKey);
    QString const newKey = catProfileStorageKey(newName);
    writeSettingsGroupFlat(settings, newKey, profileValues);
    settings.beginGroup(newKey);
    settings.setValue(QStringLiteral("name"), newName);
    settings.setValue(QStringLiteral("updatedUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    settings.endGroup();
    settings.endGroup();

    if (0 == activeCatProfile().compare(oldName, Qt::CaseInsensitive)) {
        settings.setValue(kCatProfileActiveKey, newName);
    }
    settings.sync();
    emit catProfilesChanged();
    emit activeCatProfileChanged();
    emit statusMessage(QStringLiteral("CAT profile renamed: %1").arg(newName));
    return true;
}

