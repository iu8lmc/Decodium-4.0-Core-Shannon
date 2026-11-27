#include "DecodiumBridge.h"
#include "AdifDocumentInternal.h"
#include "MapIntelligenceService.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QTextStream>
#include <QTimeZone>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

extern void decodiumBridgeLogbookLog(const QString&);
extern void rebuildWorkedCallsFromDocument(QSet<QString>&, QList<ParsedAdifRecord> const&);
using decodium::adif_document::loadAdifDocument;
extern bool decodiumBridgeWriteAdifDocument(const QString&, const ParsedAdifDocument&);
extern int decodiumBridgeCountAdifRecords(const QString&);

QString DecodiumBridge::effectiveAdifLogPath() const
{
    if (!m_adifLogPath.trimmed().isEmpty()) {
        return m_adifLogPath;
    }
    return defaultAdifLogPath();
}

QString DecodiumBridge::defaultAdifLogPath() const
{
    // Usa la directory legacy (AppData\Local\Decodium) se il log esiste gia' li',
    // altrimenti la directory standard Qt (AppData\Local\IU8LMC\Decodium)
    QString const legacyDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/Decodium/decodium_log.adi");
    if (QFile::exists(legacyDir)) {
        return legacyDir;
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/decodium_log.adi");
}

QString DecodiumBridge::ensureAdifLogPath()
{
    QString const resolved = effectiveAdifLogPath();
    if (m_adifLogPath != resolved) {
        m_adifLogPath = resolved;
    }
    return m_adifLogPath;
}

QString DecodiumBridge::logbookDirectoryPath() const
{
    QString const base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(base.isEmpty() ? QDir::homePath() : base).absoluteFilePath(QStringLiteral("logbooks"));
}

QString DecodiumBridge::sanitizedLogbookFileStem(const QString& name) const
{
    QString stem = name.trimmed();
    if (stem.isEmpty()) {
        stem = m_callsign.trimmed().isEmpty() ? QStringLiteral("Default") : m_callsign.trimmed();
    }
    stem.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")), QStringLiteral("_"));
    stem.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    stem = stem.trimmed();
    while (stem.startsWith(QLatin1Char('.')) || stem.startsWith(QLatin1Char('_'))) {
        stem.remove(0, 1);
    }
    while (stem.endsWith(QLatin1Char('.')) || stem.endsWith(QLatin1Char('_'))) {
        stem.chop(1);
    }
    return stem.isEmpty() ? QStringLiteral("Default") : stem.left(48);
}

void DecodiumBridge::ensureLogbookProfile(const QString& name, const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return;
    }
    QString const cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (cleanPath.isEmpty()) {
        return;
    }
    QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) {
        cleanName = QFileInfo(cleanPath).completeBaseName();
    }
    if (cleanName.isEmpty()) {
        cleanName = QStringLiteral("Default");
    }

    int const existingPathIndex = m_logbookProfilePaths.indexOf(cleanPath);
    if (existingPathIndex >= 0) {
        m_logbookProfileNames[existingPathIndex] = cleanName;
        return;
    }

    QString uniqueName = cleanName;
    int suffix = 2;
    while (m_logbookProfileNames.contains(uniqueName)) {
        uniqueName = QStringLiteral("%1 %2").arg(cleanName).arg(suffix++);
    }
    m_logbookProfileNames << uniqueName;
    m_logbookProfilePaths << cleanPath;
}

void DecodiumBridge::loadLogbookSettings()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    m_logbookProfileNames.clear();
    m_logbookProfilePaths.clear();

    s.beginGroup(QStringLiteral("Logbooks"));
    int const count = s.beginReadArray(QStringLiteral("Profiles"));
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        QString const name = s.value(QStringLiteral("Name")).toString().trimmed();
        QString const path = s.value(QStringLiteral("Path")).toString().trimmed();
        if (!path.isEmpty()) {
            ensureLogbookProfile(name, path);
        }
    }
    s.endArray();
    QString activePath = s.value(QStringLiteral("ActivePath")).toString().trimmed();
    QString activeName = s.value(QStringLiteral("ActiveName")).toString().trimmed();
    s.endGroup();

    QString const fallbackPath = defaultAdifLogPath();
    if (m_logbookProfilePaths.isEmpty()) {
        QString defaultName = m_callsign.trimmed().toUpper();
        if (defaultName.isEmpty()) {
            defaultName = QStringLiteral("Default");
        }
        ensureLogbookProfile(defaultName, fallbackPath);
    }

    if (activePath.isEmpty()) {
        activePath = fallbackPath;
    }
    activePath = QDir::cleanPath(QFileInfo(activePath).absoluteFilePath());
    int activeIndex = m_logbookProfilePaths.indexOf(activePath);
    if (activeIndex < 0) {
        ensureLogbookProfile(activeName.isEmpty() ? QFileInfo(activePath).completeBaseName() : activeName,
                             activePath);
        activeIndex = m_logbookProfilePaths.indexOf(activePath);
    }

    if (activeIndex >= 0) {
        m_activeLogbookName = m_logbookProfileNames.at(activeIndex);
        m_adifLogPath = m_logbookProfilePaths.at(activeIndex);
    } else {
        m_activeLogbookName = QStringLiteral("Default");
        m_adifLogPath = fallbackPath;
        ensureLogbookProfile(m_activeLogbookName, m_adifLogPath);
    }

    saveLogbookSettings();
    decodiumBridgeLogbookLog(QStringLiteral("Logbook active: name=%1 path=%2")
                  .arg(m_activeLogbookName, m_adifLogPath));
}

void DecodiumBridge::saveLogbookSettings() const
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    s.beginGroup(QStringLiteral("Logbooks"));
    s.setValue(QStringLiteral("ActiveName"), m_activeLogbookName);
    s.setValue(QStringLiteral("ActivePath"), m_adifLogPath);
    s.beginWriteArray(QStringLiteral("Profiles"));
    for (int i = 0; i < m_logbookProfilePaths.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("Name"), m_logbookProfileNames.value(i));
        s.setValue(QStringLiteral("Path"), m_logbookProfilePaths.value(i));
    }
    s.endArray();
    s.endGroup();
    s.sync();
}

QVariantList DecodiumBridge::logbookProfiles() const
{
    QVariantList profiles;
    QString const activePath = QDir::cleanPath(QFileInfo(effectiveAdifLogPath()).absoluteFilePath());
    for (int i = 0; i < m_logbookProfilePaths.size(); ++i) {
        QString const path = QDir::cleanPath(QFileInfo(m_logbookProfilePaths.at(i)).absoluteFilePath());
        QFileInfo const info(path);
        QVariantMap profile;
        profile.insert(QStringLiteral("name"), m_logbookProfileNames.value(i));
        profile.insert(QStringLiteral("path"), path);
        profile.insert(QStringLiteral("active"), path == activePath);
        profile.insert(QStringLiteral("exists"), info.exists());
        profile.insert(QStringLiteral("qsoCount"), decodiumBridgeCountAdifRecords(path));
        profiles.append(profile);
    }
    return profiles;
}

bool DecodiumBridge::backupActiveLogbook()
{
    QString const path = ensureAdifLogPath();
    QFileInfo const info(path);
    if (!info.exists() || !info.isFile()) {
        emit statusMessage(QStringLiteral("Logbook backup non necessario: file non ancora creato"));
        return true;
    }

    QString const stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString const backupPath = info.absoluteDir().absoluteFilePath(
        QStringLiteral("%1_%2.backup.adi").arg(info.completeBaseName(), stamp));
    if (!QFile::copy(path, backupPath)) {
        emit errorMessage(QStringLiteral("Impossibile creare backup logbook: %1").arg(backupPath));
        return false;
    }
    emit statusMessage(QStringLiteral("Backup logbook creato: %1").arg(QDir::toNativeSeparators(backupPath)));
    return true;
}

bool DecodiumBridge::createLogbook(const QString& name, bool backupCurrent)
{
    QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) {
        cleanName = m_callsign.trimmed().isEmpty() ? QStringLiteral("Default") : m_callsign.trimmed().toUpper();
    }
    if (backupCurrent && !backupActiveLogbook()) {
        return false;
    }

    QDir dir(logbookDirectoryPath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        emit errorMessage(QStringLiteral("Impossibile creare cartella logbook: %1").arg(dir.absolutePath()));
        return false;
    }

    QString stem = sanitizedLogbookFileStem(cleanName);
    QString path = dir.absoluteFilePath(stem + QStringLiteral(".adi"));
    int suffix = 2;
    while (QFileInfo::exists(path)) {
        path = dir.absoluteFilePath(QStringLiteral("%1_%2.adi").arg(stem).arg(suffix++));
    }

    ParsedAdifDocument emptyDoc;
    emptyDoc.header = QStringLiteral("Decodium4 ADIF Log\n<EOH>\n");
    emptyDoc.loaded = true;
    if (!decodiumBridgeWriteAdifDocument(path, emptyDoc)) {
        emit errorMessage(QStringLiteral("Impossibile creare logbook: %1").arg(path));
        return false;
    }

    m_adifLogPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    ensureLogbookProfile(cleanName, m_adifLogPath);
    m_activeLogbookName = m_logbookProfileNames.value(m_logbookProfilePaths.indexOf(m_adifLogPath), cleanName);
    saveLogbookSettings();
    reloadActiveLogbookState(QStringLiteral("create"));
    emit statusMessage(QStringLiteral("Logbook attivo: %1").arg(m_activeLogbookName));
    return true;
}

bool DecodiumBridge::addLogbook(const QString& filename, const QString& name)
{
    QString path = filename.trimmed();
    if (path.startsWith(QStringLiteral("file://"))) {
        path = QUrl(path).toLocalFile();
    }
    if (path.isEmpty()) {
        return false;
    }

    QFileInfo const info(path);
    QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) {
        cleanName = info.completeBaseName();
    }
    if (cleanName.isEmpty()) {
        cleanName = QStringLiteral("Imported");
    }

    if (!info.exists()) {
        ParsedAdifDocument emptyDoc;
        emptyDoc.header = QStringLiteral("Decodium4 ADIF Log\n<EOH>\n");
        emptyDoc.loaded = true;
        if (!decodiumBridgeWriteAdifDocument(info.absoluteFilePath(), emptyDoc)) {
            emit errorMessage(QStringLiteral("Impossibile creare logbook: %1").arg(path));
            return false;
        }
    }

    m_adifLogPath = QDir::cleanPath(info.absoluteFilePath());
    ensureLogbookProfile(cleanName, m_adifLogPath);
    m_activeLogbookName = m_logbookProfileNames.value(m_logbookProfilePaths.indexOf(m_adifLogPath), cleanName);
    saveLogbookSettings();
    reloadActiveLogbookState(QStringLiteral("add"));
    emit statusMessage(QStringLiteral("Logbook caricato: %1").arg(m_activeLogbookName));
    return true;
}

bool DecodiumBridge::switchLogbook(const QString& path)
{
    QString cleanPath = path.trimmed();
    if (cleanPath.startsWith(QStringLiteral("file://"))) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    cleanPath = QDir::cleanPath(QFileInfo(cleanPath).absoluteFilePath());
    if (cleanPath.isEmpty()) {
        return false;
    }

    int index = m_logbookProfilePaths.indexOf(cleanPath);
    if (index < 0) {
        ensureLogbookProfile(QFileInfo(cleanPath).completeBaseName(), cleanPath);
        index = m_logbookProfilePaths.indexOf(cleanPath);
    }
    if (index < 0) {
        return false;
    }

    if (!QFileInfo::exists(cleanPath)) {
        ParsedAdifDocument emptyDoc;
        emptyDoc.header = QStringLiteral("Decodium4 ADIF Log\n<EOH>\n");
        emptyDoc.loaded = true;
        if (!decodiumBridgeWriteAdifDocument(cleanPath, emptyDoc)) {
            emit errorMessage(QStringLiteral("Impossibile aprire logbook: %1").arg(cleanPath));
            return false;
        }
    }

    m_activeLogbookName = m_logbookProfileNames.value(index, QFileInfo(cleanPath).completeBaseName());
    m_adifLogPath = cleanPath;
    saveLogbookSettings();
    reloadActiveLogbookState(QStringLiteral("switch"));
    emit statusMessage(QStringLiteral("Logbook attivo: %1").arg(m_activeLogbookName));
    return true;
}

bool DecodiumBridge::deleteLogbook(const QString& path, bool deleteFile)
{
    QString cleanPath = path.trimmed();
    if (cleanPath.startsWith(QStringLiteral("file://"))) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    if (cleanPath.trimmed().isEmpty()) {
        emit errorMessage(QStringLiteral("Logbook non valido"));
        return false;
    }
    cleanPath = QDir::cleanPath(QFileInfo(cleanPath).absoluteFilePath());
    if (cleanPath.isEmpty()) {
        emit errorMessage(QStringLiteral("Logbook non valido"));
        return false;
    }

    int const index = m_logbookProfilePaths.indexOf(cleanPath);
    if (index < 0) {
        emit errorMessage(QStringLiteral("Logbook non trovato: %1").arg(QDir::toNativeSeparators(cleanPath)));
        return false;
    }

    QString const removedName = m_logbookProfileNames.value(index, QFileInfo(cleanPath).completeBaseName());
    QString const activePath = QDir::cleanPath(QFileInfo(effectiveAdifLogPath()).absoluteFilePath());
    bool const wasActive = cleanPath == activePath;
    bool const removingLastProfile = m_logbookProfilePaths.size() == 1;

    if (deleteFile) {
        QFileInfo const info(cleanPath);
        if (info.exists()) {
            if (!info.isFile()) {
                emit errorMessage(QStringLiteral("Il percorso del logbook non e' un file: %1")
                                      .arg(QDir::toNativeSeparators(cleanPath)));
                return false;
            }
        }
    }

    QString replacementName;
    QString replacementPath;
    if (removingLastProfile) {
        replacementName = m_callsign.trimmed().toUpper();
        if (replacementName.isEmpty()) {
            replacementName = QStringLiteral("Default");
        }

        QDir dir(logbookDirectoryPath());
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            emit errorMessage(QStringLiteral("Impossibile creare cartella logbook: %1").arg(dir.absolutePath()));
            return false;
        }

        QString const stem = sanitizedLogbookFileStem(replacementName);
        QString newPath = dir.absoluteFilePath(stem + QStringLiteral(".adi"));
        int suffix = 2;
        while (QFileInfo::exists(newPath)
               || QDir::cleanPath(QFileInfo(newPath).absoluteFilePath()) == cleanPath) {
            newPath = dir.absoluteFilePath(QStringLiteral("%1_%2.adi").arg(stem).arg(suffix++));
        }

        ParsedAdifDocument emptyDoc;
        emptyDoc.header = QStringLiteral("Decodium4 ADIF Log\n<EOH>\n");
        emptyDoc.loaded = true;
        if (!decodiumBridgeWriteAdifDocument(newPath, emptyDoc)) {
            emit errorMessage(QStringLiteral("Impossibile creare nuovo logbook vuoto: %1")
                                  .arg(QDir::toNativeSeparators(newPath)));
            return false;
        }
        replacementPath = QDir::cleanPath(QFileInfo(newPath).absoluteFilePath());
    }

    if (deleteFile) {
        QFileInfo const info(cleanPath);
        if (info.exists() && !QFile::remove(cleanPath)) {
            if (!replacementPath.isEmpty()) {
                QFile::remove(replacementPath);
            }
            emit errorMessage(QStringLiteral("Impossibile cancellare logbook: %1")
                                  .arg(QDir::toNativeSeparators(cleanPath)));
            return false;
        }
    }

    m_logbookProfileNames.removeAt(index);
    m_logbookProfilePaths.removeAt(index);

    if (!replacementPath.isEmpty()) {
        m_adifLogPath = replacementPath;
        ensureLogbookProfile(replacementName, m_adifLogPath);
        m_activeLogbookName = m_logbookProfileNames.value(m_logbookProfilePaths.indexOf(m_adifLogPath), replacementName);
    } else if (wasActive) {
        int const nextIndex = qBound(0, index, m_logbookProfilePaths.size() - 1);
        m_adifLogPath = m_logbookProfilePaths.at(nextIndex);
        m_activeLogbookName = m_logbookProfileNames.value(nextIndex, QFileInfo(m_adifLogPath).completeBaseName());
    }

    saveLogbookSettings();
    if (wasActive || !replacementPath.isEmpty()) {
        reloadActiveLogbookState(QStringLiteral("delete"));
    } else {
        emit activeLogbookChanged();
    }

    emit statusMessage(deleteFile
                           ? QStringLiteral("Logbook cancellato: %1").arg(removedName)
                           : QStringLiteral("Logbook rimosso dalla lista: %1").arg(removedName));
    return true;
}


extern QString decodiumBridgeBandFromFreqHz(double);
extern QTimeZone decodiumBridgeUtcTimeZone();

bool DecodiumBridge::exportAdif(const QString& filename)
{
    if (filename.isEmpty()) return false;
    QString const adifPath = effectiveAdifLogPath();
    QFileInfo const adifInfo {adifPath};
    QFileInfo const outInfo {filename};
    QString const cleanAdifPath = QDir::cleanPath(adifInfo.absoluteFilePath());
    QString const cleanOutPath = QDir::cleanPath(outInfo.absoluteFilePath());
    if (adifInfo.exists() && adifInfo.isFile() && cleanAdifPath == cleanOutPath) {
        emit statusMessage(QString("ADIF esportato: %1").arg(filename));
        return true;
    }
    if (adifInfo.exists() && adifInfo.isFile()) {
        QFile::remove(filename);
        if (QFile::copy(adifPath, filename)) {
            emit statusMessage(QString("ADIF esportato: %1").arg(filename));
            return true;
        }
    }

    QString logPath = logAllTxtPath();
    QFile src(logPath);
    if (!src.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (QFile::exists(adifPath))
            return QFile::copy(adifPath, filename);
        emit errorMessage("Log non trovato: " + logPath);
        return false;
    }
    QFile out(filename);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorMessage("Impossibile scrivere: " + filename);
        return false;
    }
    QTextStream ts(&out);
    ts << "Decodium3 ADIF Export — " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "\n<EOH>\n";
    auto field = [](const QString& tag, const QString& val) -> QString {
        return QString("<%1:%2>%3 ").arg(tag).arg(val.toUtf8().size()).arg(val);
    };
    QTextStream in(&src);
    int count = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList f = line.split(' ', Qt::SkipEmptyParts);
        if (f.size() < 5) continue;
        QString dtStr  = f[0];
        QString dxCall = f[2];
        QString dxGrid = (f.size() > 3) ? f[3] : "";
        QString freqStr= (f.size() > 4) ? f[4] : "";
        QString mode   = (f.size() > 5) ? f[5] : "FT8";
        QDateTime utc  = QDateTime::fromString(dtStr, "yyyyMMdd_hhmmss");
        utc.setTimeZone(decodiumBridgeUtcTimeZone());
        double freqMhz = freqStr.replace("MHz","").toDouble();
        double freqHz  = freqMhz * 1e6;
        ts << field("CALL",          dxCall)
           << field("BAND",          decodiumBridgeBandFromFreqHz(freqHz))
           << field("FREQ",          QString::number(freqMhz, 'f', 6))
           << field("MODE",          mode)
           << field("QSO_DATE",      utc.toString("yyyyMMdd"))
           << field("TIME_ON",       utc.toString("HHmmss"))
           << field("RST_SENT",      "-10")
           << field("RST_RCVD",      "-10")
           << field("GRIDSQUARE",    dxGrid)
           << field("MY_CALL",       m_callsign)
           << field("MY_GRIDSQUARE", m_grid)
           << "<EOR>\n";
        ++count;
    }
    emit statusMessage(QString("ADIF esportato: %1 QSO → %2").arg(count).arg(filename));
    return true;
}

bool DecodiumBridge::importAdif(const QString& filename)
{
    return importFromAdifAsync(filename);
}


void DecodiumBridge::reloadActiveLogbookState(const QString& reason)
{
    QString const path = ensureAdifLogPath();
    ParsedAdifDocument doc = loadAdifDocument(path);
    rebuildWorkedCallsFromDocument(m_workedCalls, doc.records);
    rebuildWorkedSetsFromAdifRecords(doc.records);
    m_qsoCountCache = doc.records.size();
    invalidateQsoSearchCache();
    warmLogCacheAsync();
    emit qsoCountChanged();
    emit workedCountChanged();
    emit qsoLogCacheChanged();
    emit activeLogbookChanged();
    if (m_mapIntelligenceService) {
        m_mapIntelligenceService->reloadFromAdif(path);
    }
    decodiumBridgeLogbookLog(QStringLiteral("Logbook reloaded: %1 QSO reason=%2 path=%3")
                  .arg(m_qsoCountCache)
                  .arg(reason, path));
}

