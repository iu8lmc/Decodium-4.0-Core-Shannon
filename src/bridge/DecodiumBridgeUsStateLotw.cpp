#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include "UsStateDataManager.h"
#include "Network/DecodiumWsprUploader.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDebug>
#include <QtConcurrent/QtConcurrentRun>
#include <cstring>
#include <utility>

extern void decodiumBridgeLogbookLog(const QString&);

namespace {
constexpr qint64 kLotwCacheMaxAgeSeconds = 7 * 24 * 60 * 60;

struct LotwUsersLoadResult
{
    QSet<QString> users;
    QString error;
};

static QString sharedLotwUsersCachePath()
{
    QString organization = QCoreApplication::organizationName().trimmed();
    if (organization.isEmpty()) {
        organization = QStringLiteral("Decodium");
    }

    QString const root =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return QDir(root).filePath(
        organization + QStringLiteral("/Decodium/lotw-user-activity.csv"));
}

static LotwUsersLoadResult parseLotwUsersCsv(QByteArray const& data)
{
    LotwUsersLoadResult result;
    result.users.reserve(qMax<qsizetype>(1024, data.count('\n')));

    qsizetype lineStart = 0;
    while (lineStart < data.size()) {
        qsizetype lineEnd = data.indexOf('\n', lineStart);
        if (lineEnd < 0) {
            lineEnd = data.size();
        }

        // Search only this line: malformed lines without commas must not
        // repeatedly scan the remainder of a potentially large cache file.
        char const* const line = data.constData() + lineStart;
        char const* const separator = static_cast<char const*>(
            std::memchr(line, ',', static_cast<size_t>(lineEnd - lineStart)));
        if (separator) {
            qsizetype const comma = lineStart + (separator - line);
            QString const call =
                QString::fromUtf8(data.constData() + lineStart, comma - lineStart)
                    .trimmed()
                    .toUpper();
            if (!call.isEmpty()
                && call.compare(QStringLiteral("CALLSIGN"), Qt::CaseInsensitive) != 0) {
                result.users.insert(call);
            }
        }

        lineStart = lineEnd + 1;
    }

    if (result.users.isEmpty()) {
        result.error = QStringLiteral("CSV contains no valid callsigns");
    }
    return result;
}

static LotwUsersLoadResult loadLotwUsersFile(QString const& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {{}, file.errorString()};
    }
    return parseLotwUsersCsv(file.readAll());
}

static bool copyLotwCacheAtomically(QString const& source, QString const& destination)
{
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }

    QDir().mkpath(QFileInfo(destination).absolutePath());
    QSaveFile output(destination);
    if (!output.open(QIODevice::WriteOnly)) {
        return false;
    }

    QByteArray const data = input.readAll();
    return output.write(data) == data.size() && output.commit();
}
}

void DecodiumBridge::setShowUsState(bool v)
{
    if (m_showUsState == v) {
        return;
    }
    m_showUsState = v;
    {
        QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        decodium::beginActiveSettingsProfile(s);
        s.setValue(QStringLiteral("showUsState"), m_showUsState);
    }
    emit showUsStateChanged();
    if (m_showUsState && m_usStateData) {
        m_usStateData->ensureLoadedAsync();
    }
    refreshDecodeListDxcc();
}

bool DecodiumBridge::usStateDataReady() const
{
    return m_usStateData && m_usStateData->ready();
}

bool DecodiumBridge::usStateDataUpdating() const
{
    return m_usStateData && m_usStateData->updating();
}

int DecodiumBridge::usStateGridCount() const
{
    return m_usStateData ? m_usStateData->gridCount() : 0;
}

int DecodiumBridge::usStateLocatorCount() const
{
    return m_usStateData ? m_usStateData->stateCount() : 0;
}

void DecodiumBridge::updateUsStateData()
{
    if (offlineMode()) {
        emit statusMessage(QStringLiteral("Offline: U.S. state data update disabled; cached data remains available"));
        return;
    }
    if (m_usStateData) {
        m_usStateData->updateNow();
    }
}

bool DecodiumBridge::isLotwUser(const QString& call) const
{
    return m_lotwUsers.contains(call.toUpper());
}

void DecodiumBridge::setLotwEnabled(bool v)
{
    if (m_lotwEnabled == v) {
        return;
    }

    m_lotwEnabled = v;
    emit lotwEnabledChanged();
    if (m_lotwEnabled) {
        updateLotwUsers();
    } else {
        refreshDecodeListDxcc();
    }
}

void DecodiumBridge::updateLotwUsers()
{
    reloadLotwUsers(false);
}

void DecodiumBridge::forceUpdateLotwUsers()
{
    reloadLotwUsers(true);
}


void DecodiumBridge::reloadLotwUsers(bool forceDownload)
{
    if (m_lotwUpdating || m_shuttingDown) {
        return;
    }

    QString const cachePath = sharedLotwUsersCachePath();
    QFileInfo cacheInfo(cachePath);

    if (!cacheInfo.exists()) {
        QStringList const legacyCandidates {
            QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("lotw-user-activity.csv")),
            QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("lotw_users.txt")),
            QDir(QFileInfo(cachePath).absolutePath())
                .filePath(QStringLiteral("lotw_users.txt"))
        };

        QString migrationSource;
        QDateTime newestTimestamp;
        for (QString const& candidate : legacyCandidates) {
            QFileInfo const candidateInfo(candidate);
            if (!candidateInfo.exists() || candidateInfo.size() <= 0) {
                continue;
            }
            if (migrationSource.isEmpty()
                || candidateInfo.lastModified() > newestTimestamp) {
                migrationSource = candidate;
                newestTimestamp = candidateInfo.lastModified();
            }
        }

        if (!migrationSource.isEmpty()
            && copyLotwCacheAtomically(migrationSource, cachePath)) {
            decodiumBridgeLogbookLog(QStringLiteral("LotW: migrated profile cache [%1] -> [%2]")
                          .arg(migrationSource, cachePath));
            cacheInfo = QFileInfo(cachePath);
        }
    }

    if (offlineMode() && !cacheInfo.exists()) {
        emit statusMessage(QStringLiteral("Offline: no local LotW users cache available"));
        return;
    }

    m_lotwUpdating = true;
    emit lotwUpdatingChanged();

    if (forceDownload || !cacheInfo.exists()) {
        startLotwUsersDownload(cachePath, forceDownload);
        return;
    }

    qint64 const cacheAgeSeconds =
        cacheInfo.lastModified().secsTo(QDateTime::currentDateTime());
    bool const refreshAfterLoad =
        cacheAgeSeconds < 0 || cacheAgeSeconds >= kLotwCacheMaxAgeSeconds;
    emit statusMessage(QStringLiteral("LotW: loading cached users..."));

    auto* watcher = new QFutureWatcher<LotwUsersLoadResult>(this);
    connect(watcher, &QFutureWatcher<LotwUsersLoadResult>::finished,
            this, [this, watcher, cachePath, refreshAfterLoad]() {
        LotwUsersLoadResult result = watcher->result();
        watcher->deleteLater();

        if (m_shuttingDown) {
            return;
        }

        if (!result.users.isEmpty()) {
            m_lotwUsers = std::move(result.users);
            emit lotwUsersChanged();
            emit statusMessage(
                QStringLiteral("LotW: %1 users loaded from cache")
                    .arg(m_lotwUsers.size()));
            refreshDecodeListDxcc();
        } else {
            qWarning().noquote()
                << "[LOTW] Cached users file could not be loaded"
                << "path=" << cachePath
                << "reason=" << result.error;
        }

        if (!offlineMode() && (refreshAfterLoad || m_lotwUsers.isEmpty())) {
            startLotwUsersDownload(cachePath, false);
            return;
        }

        m_lotwUpdating = false;
        emit lotwUpdatingChanged();
    });
    watcher->setFuture(QtConcurrent::run([cachePath]() {
        return loadLotwUsersFile(cachePath);
    }));
}

void DecodiumBridge::startLotwUsersDownload(QString const& cachePath,
                                            bool reportErrors)
{
    if (offlineMode()) {
        if (m_lotwUpdating) {
            m_lotwUpdating = false;
            emit lotwUpdatingChanged();
        }
        emit statusMessage(QStringLiteral("Offline: LotW download disabled; cached users remain available"));
        return;
    }
    emit statusMessage(QStringLiteral("LotW: downloading user list..."));
    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl(QStringLiteral(
        "https://lotw.arrl.org/lotw-user-activity.csv")));
    request.setTransferTimeout(30000);
    QNetworkReply* reply = nam->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, nam, cachePath, reportErrors]() {
        QByteArray const data = reply->error() == QNetworkReply::NoError && reply->isReadable()
            ? reply->readAll() : QByteArray();
        QNetworkReply::NetworkError const networkError = reply->error();
        QString const networkErrorText = reply->errorString();
        reply->deleteLater();
        nam->deleteLater();

        if (m_shuttingDown) {
            return;
        }

        if (networkError != QNetworkReply::NoError) {
            m_lotwUpdating = false;
            emit lotwUpdatingChanged();
            QString const message =
                QStringLiteral("LotW download failed: %1").arg(networkErrorText);
            qWarning().noquote() << "[LOTW]" << message
                                 << "cachedUsers=" << m_lotwUsers.size();
            emit statusMessage(
                m_lotwUsers.isEmpty()
                    ? message
                    : QStringLiteral("LotW: offline, keeping %1 cached users")
                          .arg(m_lotwUsers.size()));
            if (reportErrors && m_lotwUsers.isEmpty()) {
                emit errorMessage(message);
            }
            return;
        }

        QDir().mkpath(QFileInfo(cachePath).absolutePath());
        QSaveFile cache(cachePath);
        if (!cache.open(QIODevice::WriteOnly)
            || cache.write(data) != data.size()
            || !cache.commit()) {
            qWarning().noquote()
                << "[LOTW] Unable to persist downloaded users"
                << "path=" << cachePath
                << "error=" << cache.errorString();
        }

        auto* watcher = new QFutureWatcher<LotwUsersLoadResult>(this);
        connect(watcher, &QFutureWatcher<LotwUsersLoadResult>::finished,
                this, [this, watcher]() {
            LotwUsersLoadResult result = watcher->result();
            watcher->deleteLater();

            if (m_shuttingDown) {
                return;
            }

            m_lotwUpdating = false;
            emit lotwUpdatingChanged();
            if (result.users.isEmpty()) {
                QString const message =
                    QStringLiteral("LotW download contains no valid users: %1")
                        .arg(result.error);
                qWarning().noquote() << "[LOTW]" << message;
                emit statusMessage(message);
                return;
            }

            m_lotwUsers = std::move(result.users);
            emit lotwUsersChanged();
            emit statusMessage(
                QStringLiteral("LotW: %1 users updated")
                    .arg(m_lotwUsers.size()));
            refreshDecodeListDxcc();
        });
        watcher->setFuture(QtConcurrent::run([data]() {
            return parseLotwUsersCsv(data);
        }));
    });
}


#define bridgeLog decodiumBridgeLogbookLog
void DecodiumBridge::initialiseUsStateAndWsprNotifications()
{
    m_usStateData = new UsStateDataManager(this);
    connect(m_usStateData, &UsStateDataManager::statusMessage, this, [this](const QString& msg) {
        bridgeLog(msg);
        emit statusMessage(msg);
    });
    connect(m_usStateData, &UsStateDataManager::updatingChanged, this, [this]() {
        emit usStateDataChanged();
    });
    connect(m_usStateData, &UsStateDataManager::dataChanged, this, [this]() {
        emit usStateDataChanged();
        refreshDecodeListDxcc();
    });
    connect(m_wsprUploader, &DecodiumWsprUploader::uploadStatus,
            this, [this](const QString& msg) {
                QString const status = QStringLiteral("WSPR: ") + msg;
                bridgeLog(status);
                emit statusMessage(status);
            });

}
#undef bridgeLog
