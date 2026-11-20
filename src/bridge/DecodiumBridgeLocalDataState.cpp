#include "DecodiumBridge.h"
#include "DxccLookup.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

QStringList worldMapCall3CandidatePaths()
{
    QString const appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString const appLocalData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString const appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    if (!appData.isEmpty()) {
        candidates << QDir(appData).absoluteFilePath(QStringLiteral("CALL3.TXT"));
    }
    if (!appLocalData.isEmpty() && appLocalData != appData) {
        candidates << QDir(appLocalData).absoluteFilePath(QStringLiteral("CALL3.TXT"));
    }
    candidates << QDir(appDir).absoluteFilePath(QStringLiteral("CALL3.TXT"))
               << QDir(appDir).absoluteFilePath(QStringLiteral("../CALL3.TXT"))
               << QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/CALL3.TXT"))
               << QDir::current().absoluteFilePath(QStringLiteral("CALL3.TXT"));
#ifdef CMAKE_SOURCE_DIR
    candidates << QDir(QStringLiteral(CMAKE_SOURCE_DIR))
                      .absoluteFilePath(QStringLiteral("resources/runtime/CALL3.TXT"));
#endif
    candidates.removeDuplicates();
    return candidates;
}

QStringList DecodiumBridge::ctyDatSearchPaths() const
{
    QString const appDir = QCoreApplication::applicationDirPath();
    QStringList paths = {
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cty.dat",
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/cty.dat",
        appDir + "/cty.dat",
        QDir(appDir).absoluteFilePath("../cty.dat"),
        QDir(appDir).absoluteFilePath("../Resources/cty.dat"),
        QDir(appDir).absoluteFilePath("../share/Decodium/cty.dat"),
        QDir(appDir).absoluteFilePath("../share/wsjtx/cty.dat"),
        QDir::current().absoluteFilePath("cty.dat")
    };

#ifdef CMAKE_SOURCE_DIR
    paths << QDir(QStringLiteral(CMAKE_SOURCE_DIR))
                 .absoluteFilePath(QStringLiteral("resources/runtime/cty.dat"));
#endif

    QStringList uniquePaths;
    QSet<QString> seen;
    for (const QString& path : paths) {
        QString const cleanPath = QDir::cleanPath(path);
        if (cleanPath.isEmpty() || seen.contains(cleanPath))
            continue;
        seen.insert(cleanPath);
        uniquePaths << cleanPath;
    }
    return uniquePaths;
}

QVariantMap DecodiumBridge::ctyDatState() const
{
    QVariantMap state;
    QString path;
    for (const QString& candidate : ctyDatSearchPaths()) {
        if (QFileInfo::exists(candidate)) {
            path = candidate;
            break;
        }
    }

    const QFileInfo info(path);
    const int entityCount = m_dxccLookup ? m_dxccLookup->entityCount() : 0;
    const bool loading = m_dxccLookupLoading;
    state.insert(QStringLiteral("id"), QStringLiteral("cty_dat"));
    state.insert(QStringLiteral("label"), QStringLiteral("DXCC cty.dat"));
    state.insert(QStringLiteral("updateable"), true);
    state.insert(QStringLiteral("managedFile"), true);
    state.insert(QStringLiteral("localPath"), path);
    state.insert(QStringLiteral("rowCount"), loading && !path.isEmpty() ? -1 : entityCount);
    state.insert(QStringLiteral("updatedAt"), info.exists() ? info.lastModified().toMSecsSinceEpoch() : 0);
    state.insert(QStringLiteral("status"), m_ctyDatUpdating
                 ? tr("Update in progress...")
                 : (!m_ctyDatLastError.isEmpty()
                    ? tr("Error")
                    : (loading
                       ? tr("Loading...")
                       : (entityCount > 0 ? tr("Local")
                                          : tr("File not loaded")))));
    state.insert(QStringLiteral("error"), m_ctyDatLastError);
    return state;
}

QVariantMap DecodiumBridge::call3TxtState() const
{
    QVariantMap state;
    QString path;
    for (const QString& candidate : worldMapCall3CandidatePaths()) {
        if (QFileInfo::exists(candidate)) {
            path = candidate;
            break;
        }
    }

    const QFileInfo info(path);
    const bool loading = m_worldMapCall3Loading;
    state.insert(QStringLiteral("id"), QStringLiteral("call3_txt"));
    state.insert(QStringLiteral("label"), QStringLiteral("CALL3.TXT"));
    state.insert(QStringLiteral("updateable"), true);
    state.insert(QStringLiteral("managedFile"), true);
    state.insert(QStringLiteral("localPath"), path);
    state.insert(QStringLiteral("rowCount"), loading && !path.isEmpty() ? -1 : m_worldMapCall3RecordCount);
    state.insert(QStringLiteral("updatedAt"), info.exists() ? info.lastModified().toMSecsSinceEpoch() : 0);
    state.insert(QStringLiteral("status"), m_call3TxtUpdating
                 ? tr("Update in progress...")
                 : (!m_call3TxtLastError.isEmpty()
                    ? tr("Error")
                    : (loading
                       ? tr("Loading...")
                       : (m_worldMapCall3RecordCount > 0 ? tr("Local")
                                                         : tr("File not loaded")))));
    state.insert(QStringLiteral("error"), m_call3TxtLastError);
    return state;
}

