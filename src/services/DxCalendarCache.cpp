#include "DxCalendarCache.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThreadPool>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>

namespace {
QThreadPool& calendarPool()
{
    static QThreadPool pool;
    static const bool initialized = [] { pool.setMaxThreadCount(1); return true; }();
    Q_UNUSED(initialized)
    return pool;
}
constexpr qint64 maxCacheBytes = 3 * 1024 * 1024;
}

DxCalendarCache::DxCalendarCache(QObject* parent, const QString& filePath)
    : QObject(parent), m_path(filePath.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
              .filePath(QStringLiteral("dxcalendar-cache.json"))
        : filePath)
{
}

void DxCalendarCache::readCache()
{
    auto* watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher] {
        const auto value = watcher->result();
        watcher->deleteLater();
        emit cacheRead(value.value(QStringLiteral("data")).toString(),
                       value.value(QStringLiteral("updated")).toString(),
                       value.value(QStringLiteral("error")).toString());
    });
    const QString path = m_path;
    watcher->setFuture(QtConcurrent::run(&calendarPool(), [path] {
        QVariantMap result;
        QFile file(path);
        if (!file.exists()) return result;
        if (!file.open(QIODevice::ReadOnly) || file.size() > maxCacheBytes) {
            result.insert(QStringLiteral("error"), QStringLiteral("Cannot read calendar cache"));
            return result;
        }
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            result.insert(QStringLiteral("error"), QStringLiteral("Invalid calendar cache"));
            return result;
        }
        const auto object = doc.object();
        result.insert(QStringLiteral("data"), object.value(QStringLiteral("data")).toString());
        result.insert(QStringLiteral("updated"), object.value(QStringLiteral("updated")).toString());
        return result;
    }));
}

void DxCalendarCache::writeCache(const QString& data, const QString& updated)
{
    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher] {
        const QString error = watcher->result();
        watcher->deleteLater();
        emit cacheWritten(error);
    });
    const QString path = m_path;
    watcher->setFuture(QtConcurrent::run(&calendarPool(), [path, data, updated] {
        if (data.size() > 2000000) return QStringLiteral("Calendar cache exceeds size limit");
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) return QStringLiteral("Cannot create cache directory");
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return file.errorString();
        const QByteArray encoded = QJsonDocument(QJsonObject{{QStringLiteral("data"), data},
                                                              {QStringLiteral("updated"), updated}}).toJson(QJsonDocument::Compact);
        if (encoded.size() > maxCacheBytes) return QStringLiteral("Calendar cache exceeds size limit");
        if (file.write(encoded) != encoded.size() || !file.commit()) return file.errorString();
        return QString();
    }));
}
