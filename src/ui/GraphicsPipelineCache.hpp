#pragma once

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

#include <utility>
#include <functional>

namespace decodium {
namespace graphics_cache {

inline QString sanitisedToken(QString value, const QString& fallback)
{
    value = value.trimmed().toLower();
    for (QChar& character : value) {
        if (!character.isLetterOrNumber()
            && character != QLatin1Char('-')
            && character != QLatin1Char('_')) {
            character = QLatin1Char('_');
        }
    }
    while (value.contains(QStringLiteral("__")))
        value.replace(QStringLiteral("__"), QStringLiteral("_"));
    return value.isEmpty() ? fallback : value;
}

inline QString applicationIdentityToken()
{
    QString identity = QCoreApplication::applicationName().trimmed();
    if (identity.isEmpty())
        identity = QStringLiteral("Decodium");
    return QString::fromLatin1(
        QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256)
            .toHex().left(12));
}

inline QString graphicsBackendToken()
{
    QByteArray backend = qgetenv("QSG_RHI_BACKEND").trimmed().toLower();
    if (backend.isEmpty())
        backend = qgetenv("QT_QUICK_BACKEND").trimmed().toLower();
    // D3D11 hardware and D3D11 WARP do not share the same adapter, driver or
    // compiled pipeline representation.  Keeping their caches separate is
    // essential when WARP was selected after a hardware-driver crash: loading
    // the hardware cache would immediately reintroduce state from the failed
    // graphics session into the recovery renderer.
    if (backend == QByteArrayLiteral("d3d11")
        && qEnvironmentVariableIntValue("QSG_RHI_PREFER_SOFTWARE_RENDERER") != 0) {
        backend += QByteArrayLiteral("_warp");
    }
    // Linux hybrid-GPU recovery can select Intel instead of NVIDIA while
    // keeping Vulkan. Never seed the new adapter with the old adapter's cache.
    // The key includes vendor/device/driver and Vulkan pipeline-cache UUID,
    // not the enumeration index (which may change after a driver update).
    if (backend == QByteArrayLiteral("vulkan") && QCoreApplication::instance()) {
        const QByteArray deviceKey = QCoreApplication::instance()
            ->property("decodiumVulkanCacheDeviceKey").toByteArray();
        if (!deviceKey.isEmpty())
            backend += '_' + QCryptographicHash::hash(deviceKey, QCryptographicHash::Sha256).toHex().left(16);
    }
    return sanitisedToken(QString::fromLocal8Bit(backend), QStringLiteral("auto"));
}

inline QString qtVersionToken()
{
    return sanitisedToken(QString::fromLatin1(qVersion()), QStringLiteral("qt"));
}

inline QString cacheDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

inline QString cachePrefix()
{
    // v2 deliberately invalidates the legacy files that were written directly
    // in place. Those files could be left incomplete by a compositor/GPU
    // freeze and then be reloaded on every subsequent start.
    return QStringLiteral("qsg_pipeline_cache_v2_%1_").arg(applicationIdentityToken());
}

inline QString stableCacheFile(const QString& directory, const QString& role)
{
    QString const fileName = QStringLiteral("%1%2_%3_%4.bin")
                                 .arg(cachePrefix(),
                                      sanitisedToken(role, QStringLiteral("window")),
                                      graphicsBackendToken(),
                                      qtVersionToken());
    return QDir(directory).absoluteFilePath(fileName);
}

inline QString stableCacheFile(const QString& role)
{
    return stableCacheFile(cacheDirectory(), role);
}

inline QString stagingCacheFile(const QString& stableFile)
{
    return stableFile + QStringLiteral(".stage.%1")
                            .arg(QCoreApplication::applicationPid());
}

inline bool pipelineCacheLoadAllowed()
{
    QObject* application = QCoreApplication::instance();
    if (!application)
        return true;
    QVariant const value = application->property("decodiumPipelineCacheLoadAllowed");
    return !value.isValid() || value.toBool();
}

inline bool isUsableCacheFile(const QString& path)
{
    QFileInfo const info(path);
    return pipelineCacheLoadAllowed() && info.isFile() && info.size() > 0;
}

inline bool atomicallyPromoteCache(const QString& stagingFile,
                                   const QString& stableFile)
{
    QFile input(stagingFile);
    if (!input.open(QIODevice::ReadOnly))
        return false;
    if (input.size() <= 0) {
        input.close();
        QFile::remove(stagingFile);
        return true;
    }

    QSaveFile output(stableFile);
    if (!output.open(QIODevice::WriteOnly))
        return false;

    QByteArray buffer;
    buffer.resize(256 * 1024);
    while (!input.atEnd()) {
        qint64 const bytesRead = input.read(buffer.data(), buffer.size());
        if (bytesRead < 0 || output.write(buffer.constData(), bytesRead) != bytesRead) {
            output.cancelWriting();
            return false;
        }
    }
    input.close();
    if (!output.commit())
        return false;

    QFile::remove(stagingFile);
    return true;
}

class PipelineCacheSession final
{
public:
    explicit PipelineCacheSession(QString directory = cacheDirectory(),
                                  bool forceFresh = false,
                                  std::function<bool()> healthyAtTeardown = {})
        : m_directory(std::move(directory))
        , m_markerFile(QDir(m_directory).absoluteFilePath(
              cachePrefix() + QStringLiteral("session.pending")))
        , m_stageSuffix(QStringLiteral(".stage.%1")
                            .arg(QCoreApplication::applicationPid()))
        , m_healthyAtTeardown(std::move(healthyAtTeardown))
    {
        QDir().mkpath(m_directory);
        bool const interruptedSession = QFileInfo::exists(m_markerFile);
        m_recovered = interruptedSession;

        QDir directoryView(m_directory);
        QStringList const ownFiles = directoryView.entryList(
            QStringList {cachePrefix() + QStringLiteral("*")}, QDir::Files);
        for (const QString& fileName : ownFiles) {
            QString const absolutePath = directoryView.absoluteFilePath(fileName);
            bool const isStagingFile = fileName.contains(QStringLiteral(".stage."));
            bool const isStableCache = fileName.endsWith(QStringLiteral(".bin"));
            if ((interruptedSession || forceFresh) && isStableCache) {
                if (QFile::remove(absolutePath))
                    ++m_removedFileCount;
            }
            // This session is armed only after Decodium acquired its instance
            // lock. A matching stage file therefore belongs to an aborted run.
            if (isStagingFile) {
                if (QFile::remove(absolutePath))
                    ++m_removedFileCount;
            }
        }

        if (QObject* application = QCoreApplication::instance()) {
            application->setProperty("decodiumPipelineCacheLoadAllowed",
                                     !(interruptedSession || forceFresh));
        }

        QSaveFile marker(m_markerFile);
        if (marker.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QByteArray payload("pid=");
            payload += QByteArray::number(QCoreApplication::applicationPid());
            payload += "\nutc=";
            payload += QDateTime::currentDateTimeUtc()
                           .toString(Qt::ISODateWithMs).toUtf8();
            payload += '\n';
            m_markerArmed = marker.write(payload) == payload.size()
                && marker.commit();
        }
    }

    ~PipelineCacheSession()
    {
        if (!m_cleanShutdown || (m_healthyAtTeardown && !m_healthyAtTeardown()))
            return;

        bool promotionSucceeded = true;
        QDir directoryView(m_directory);
        QStringList const stagedFiles = directoryView.entryList(
            QStringList {cachePrefix() + QStringLiteral("*") + m_stageSuffix},
            QDir::Files);
        for (const QString& fileName : stagedFiles) {
            QString const stagingFile = directoryView.absoluteFilePath(fileName);
            QString stableFile = stagingFile;
            stableFile.chop(m_stageSuffix.size());
            if (!atomicallyPromoteCache(stagingFile, stableFile))
                promotionSucceeded = false;
        }

        // Retaining the marker is intentional when promotion fails: the next
        // launch will discard both the old cache and the incomplete stage.
        if (promotionSucceeded && m_markerArmed)
            QFile::remove(m_markerFile);
    }

    PipelineCacheSession(const PipelineCacheSession&) = delete;
    PipelineCacheSession& operator=(const PipelineCacheSession&) = delete;

    void markCleanShutdown() { m_cleanShutdown = true; }
    bool recoveredFromInterruptedSession() const { return m_recovered; }
    bool markerArmed() const { return m_markerArmed; }
    int removedFileCount() const { return m_removedFileCount; }
    QString markerFile() const { return m_markerFile; }

private:
    QString m_directory;
    QString m_markerFile;
    QString m_stageSuffix;
    std::function<bool()> m_healthyAtTeardown;
    bool m_recovered {false};
    bool m_markerArmed {false};
    bool m_cleanShutdown {false};
    int m_removedFileCount {0};
};

} // namespace graphics_cache
} // namespace decodium
