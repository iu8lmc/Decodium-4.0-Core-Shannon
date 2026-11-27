#include "DecodiumBridge.h"
#include "GraphicsPipelineCache.hpp"
#include "DecodiumDiagnostics.h"
#include "DecodiumLogging.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QQuickWindow>
#include <QQuickGraphicsConfiguration>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

void decodiumBridgeDecodeFlowLog(const QString&);

#ifdef Q_OS_WIN
static QString decodiumSlowQmlStartupFlagPath()
{
    return QDir {QStandardPaths::writableLocation(QStandardPaths::TempLocation)}
        .absoluteFilePath(QStringLiteral("decodium-slow-qml-startup.flag"));
}

static QString decodiumStartupLogPath()
{
    return QDir {QStandardPaths::writableLocation(QStandardPaths::TempLocation)}
        .absoluteFilePath(QStringLiteral("decodium-start.log"));
}

static void appendStartupLogLine(const QString& line)
{
    QFile file {decodiumStartupLogPath()};
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream out {&file};
        out << line << '\n';
    }
}

static void writeSlowQmlStartupMarker(const QString& reason)
{
    QFile flagFile {decodiumSlowQmlStartupFlagPath()};
    if (flagFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out {&flagFile};
        out << (reason.trimmed().isEmpty()
                    ? QStringLiteral("QML startup exceeded watchdog threshold")
                    : reason.trimmed())
            << '\n'
            << QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
            << '\n';
    }
}
#endif

void DecodiumBridge::notifyMainQmlReady()
{
#ifdef Q_OS_WIN
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const qmlLoadMs = m_mainQmlAsyncLoadStartedMs > 0
        ? nowMs - m_mainQmlAsyncLoadStartedMs
        : -1;
#endif
    if (m_mainQmlAsyncLoadDone) {
        m_mainQmlAsyncLoadDone->store(true, std::memory_order_relaxed);
    }
    if (m_mainQmlReady) {
        return;
    }

    m_mainQmlReady = true;
#ifdef Q_OS_WIN
    if (qmlLoadMs >= 8000) {
        m_ft8StartupEarlyDecodeGuardUntilMs =
            qMax(m_ft8StartupEarlyDecodeGuardUntilMs, nowMs + qint64 {45000});
        decodiumBridgeDecodeFlowLog(QStringLiteral("FT8 startup early-decode guard armed: qmlLoadMs=%1 untilMs=%2")
                      .arg(qmlLoadMs)
                      .arg(qMax<qint64>(0, m_ft8StartupEarlyDecodeGuardUntilMs - nowMs)));
    }
#endif
    emit mainQmlReadyForNativeWindowing();
    decodiumBridgeDecodeFlowLog(QStringLiteral("Main.qml ready: releasing deferred startup services"));
    QTimer::singleShot(500, this, [this]() {
        if (m_shuttingDown) {
            return;
        }
        runPostQmlStartupServices();
    });
}

void DecodiumBridge::requestSafeGraphicsNextLaunch(const QString& reason)
{
#ifdef Q_OS_WIN
    writeSlowQmlStartupMarker(reason.trimmed().isEmpty()
                                  ? QStringLiteral("QML startup timed out; use safe graphics on next launch")
                                  : reason.trimmed());
    QString const flagPath = decodiumSlowQmlStartupFlagPath();
    decodiumBridgeDecodeFlowLog(QStringLiteral("Safe graphics marker requested for next launch: %1").arg(flagPath));
#else
    Q_UNUSED(reason)
#endif
}

bool DecodiumBridge::configureQuickWindowGraphics(QObject* windowObject)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    auto* window = qobject_cast<QQuickWindow*>(windowObject);
    if (!window) {
        DIAG_WARN(QStringLiteral("[UI] Main pipeline cache setup rejected: object is not a QQuickWindow"));
        return false;
    }

    QString const cacheFile =
        decodium::graphics_cache::stableCacheFile(QStringLiteral("main"));
    QString const cacheSaveFile =
        decodium::graphics_cache::stagingCacheFile(cacheFile);

    QQuickGraphicsConfiguration config;
    config.setAutomaticPipelineCache(false);
    config.setPipelineCacheSaveFile(cacheSaveFile);
    bool const canLoad = decodium::graphics_cache::isUsableCacheFile(cacheFile);
    if (canLoad) config.setPipelineCacheLoadFile(cacheFile);
    window->setGraphicsConfiguration(config);

    DIAG_INFO(QStringLiteral("[UI] Main window transactional pipeline cache configured before show load_path=%1 save_path=%2 seeded=%3")
                  .arg(cacheFile)
                  .arg(cacheSaveFile)
                  .arg(canLoad));
    return true;
#else
    Q_UNUSED(windowObject)
    return false;
#endif
}

void DecodiumBridge::notifyMainQmlLoadStarted()
{
    m_mainQmlAsyncLoadStartedMs = QDateTime::currentMSecsSinceEpoch();
#ifdef Q_OS_WIN
    auto done = std::make_shared<std::atomic_bool>(false);
    m_mainQmlAsyncLoadDone = done;
    appendStartupLogLine(QStringLiteral("Main.qml async load watchdog armed"));
    std::thread([done]() {
        for (int elapsedSeconds = 30; elapsedSeconds <= 300; elapsedSeconds += 30) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            if (done->load(std::memory_order_relaxed)) {
                return;
            }
            QString const msg = QStringLiteral("Main.qml async load still running after %1 s")
                                    .arg(elapsedSeconds);
            appendStartupLogLine(msg);
            if (elapsedSeconds == 30) {
                writeSlowQmlStartupMarker(
                    QStringLiteral("Main.qml async load exceeded 30 seconds; use safe graphics on next launch"));
                appendStartupLogLine(QStringLiteral("Main.qml async load watchdog wrote safe graphics marker"));
            }
        }
    }).detach();
#endif
}
