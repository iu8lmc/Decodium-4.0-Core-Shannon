#include "DecodiumBridge.h"
#include "Ft2LogBridgeLauncher.h"
#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>

void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::setFt2LogBridgeStatus(const QString& status)
{
    if (m_ft2LogBridgeStatus == status) {
        return;
    }
    m_ft2LogBridgeStatus = status;
    emit ft2LogBridgeStatusChanged();
}

bool DecodiumBridge::ft2LogBridgeRunning() const
{
    return decodium::logbridge::isRunning();
}

void DecodiumBridge::startFt2LogBridgeNow()
{
    launchFt2LogBridge(false);
}

void DecodiumBridge::detectFt2LogBridgePath()
{
    setFt2LogBridgeStatus(tr("Searching for FT2 Log Bridge..."));
    QPointer<DecodiumBridge> self(this);
    auto* task = QRunnable::create([self]() {
        QString const found = decodium::logbridge::findConfiguredCopy();
        QMetaObject::invokeMethod(qApp, [self, found]() {
            if (!self) {
                return;
            }
            if (found.isEmpty()) {
                self->setFt2LogBridgeStatus(
                    DecodiumBridge::tr("No configured FT2 Log Bridge found: download it from the community.ft2.it Dashboard."));
                return;
            }
            self->setSetting(QStringLiteral("FT2LogBridgePath"), found);
            self->setFt2LogBridgeStatus(DecodiumBridge::tr("Found: %1").arg(found));
        }, Qt::QueuedConnection);
    });
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task);
}

void DecodiumBridge::launchFt2LogBridge(bool automatic)
{
    if (automatic && !getSetting(QStringLiteral("FT2LogBridgeAutoStart"), true).toBool()) {
        decodiumBridgeAudioLevelLog(QStringLiteral("[FT2LOGBRIDGE] avvio automatico disattivato"));
        return;
    }

    QString const savedPath = getSetting(QStringLiteral("FT2LogBridgePath"), QString()).toString().trimmed();
    QString const udpServer = getSetting(QStringLiteral("UDPServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
    int const udpPort = getSetting(QStringLiteral("UDPServerPort"), 2237).toInt();
    bool const sendsQso = getSetting(QStringLiteral("UDPPrimarySendQso"),
                                     getSetting(QStringLiteral("UDPPrimaryLoggedAdifEnabled"), true)).toBool();

    // Senza una copia gia' nota, all'avvio automatico si cerca su disco al
    // massimo una volta al giorno: chi non usa il client non deve pagare la
    // ricerca a ogni avvio.
    if (automatic && (savedPath.isEmpty() || !QFileInfo::exists(savedPath))) {
        QDateTime const lastScan = getSetting(QStringLiteral("FT2LogBridgeLastScanUtc")).toDateTime();
        if (lastScan.isValid() && lastScan.secsTo(QDateTime::currentDateTimeUtc()) < 24 * 3600) {
            decodiumBridgeAudioLevelLog(QStringLiteral("[FT2LOGBRIDGE] nessuna copia nota, ricerca gia' fatta nelle ultime 24 ore"));
            return;
        }
    }

    if (!automatic) {
        setFt2LogBridgeStatus(tr("Starting FT2 Log Bridge..."));
    }
    QPointer<DecodiumBridge> self(this);
    auto* task = QRunnable::create([self, automatic, savedPath, udpServer, udpPort, sendsQso]() {
        namespace lb = decodium::logbridge;
        QString path = savedPath;
        bool searched = false;
        if (path.isEmpty() || !lb::isConfigured(path)) {
            path = lb::findConfiguredCopy();
            searched = true;
        }
        lb::StartResult result;
        if (!path.isEmpty()) {
            result = lb::start(path);
        }
        int const bridgePort = path.isEmpty() ? 2237 : lb::configuredUdpPort(path);

        QMetaObject::invokeMethod(qApp, [self, automatic, path, searched, result, bridgePort,
                                         udpServer, udpPort, sendsQso]() {
            if (!self) {
                return;
            }
            if (searched) {
                self->setSetting(QStringLiteral("FT2LogBridgeLastScanUtc"), QDateTime::currentDateTimeUtc());
            }
            if (path.isEmpty()) {
                decodiumBridgeAudioLevelLog(QStringLiteral("[FT2LOGBRIDGE] nessuna copia configurata trovata"));
                self->setFt2LogBridgeStatus(
                    DecodiumBridge::tr("No configured FT2 Log Bridge found: download it from the community.ft2.it Dashboard."));
                return;
            }
            if (searched) {
                self->setSetting(QStringLiteral("FT2LogBridgePath"), path);
            }

            QString status;
            if (result.alreadyRunning) {
                status = DecodiumBridge::tr("FT2 Log Bridge is already running.");
            } else if (result.ok) {
                status = DecodiumBridge::tr("FT2 Log Bridge started.");
            } else {
                status = DecodiumBridge::tr("FT2 Log Bridge could not start: %1").arg(result.message);
            }

            // Il client ascolta su 127.0.0.1:<udp_port di config.json>: se
            // Decodium manda altrove, o non manda i QSO loggati, non arriva nulla.
            QString const server = udpServer.toLower();
            bool const local = server == QStringLiteral("127.0.0.1") || server == QStringLiteral("localhost")
                               || server == QStringLiteral("::1");
            if (!local || udpPort != bridgePort) {
                status += QLatin1Char(' ')
                          + DecodiumBridge::tr("Warning: Decodium sends UDP to %1:%2, the bridge listens on 127.0.0.1:%3.")
                                .arg(udpServer).arg(udpPort).arg(bridgePort);
            } else if (!sendsQso) {
                status += QLatin1Char(' ')
                          + DecodiumBridge::tr("Warning: \"QSO logged\" is not sent over UDP.");
            }
            self->setFt2LogBridgeStatus(status);
            decodiumBridgeAudioLevelLog(QStringLiteral("[FT2LOGBRIDGE] %1 %2: %3 (pid %4) path=%5 udp=%6:%7 bridge=%8")
                                .arg(automatic ? QStringLiteral("avvio automatico") : QStringLiteral("avvio manuale"),
                                     result.alreadyRunning ? QStringLiteral("gia' attivo")
                                                           : (result.ok ? QStringLiteral("avviato") : QStringLiteral("FALLITO")),
                                     result.message)
                                .arg(result.pid)
                                .arg(path, udpServer)
                                .arg(udpPort)
                                .arg(bridgePort));
        }, Qt::QueuedConnection);
    });
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task);
}

