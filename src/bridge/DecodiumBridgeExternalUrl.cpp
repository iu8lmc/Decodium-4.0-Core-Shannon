#include "DecodiumBridge.h"
#include "src/core/HostProcessEnvironment.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QDebug>

bool DecodiumBridge::openExternalUrl(const QString& url)
{
    QString const clean = url.trimmed();
    if (clean.isEmpty()) {
        emit errorMessage(QStringLiteral("URL vuoto"));
        return false;
    }

    QUrl target;
    if (clean.startsWith(QLatin1String("file:"))) {
        target = QUrl(clean);
    } else if (clean.contains(QStringLiteral("://"))
               || clean.startsWith(QLatin1String("mailto:"), Qt::CaseInsensitive)) {
        target = QUrl(clean);
    } else {
        target = QUrl::fromLocalFile(clean);
    }

    QString const scheme = target.scheme().toLower();
    if (!target.isValid()
        || !(scheme == QStringLiteral("mailto")
             || scheme == QStringLiteral("file")
             || scheme == QStringLiteral("http")
             || scheme == QStringLiteral("https"))) {
        emit errorMessage(QStringLiteral("URL non valido o schema non permesso: %1")
                          .arg(clean));
        return false;
    }

    bool ok = false;
#if defined(Q_OS_LINUX)
    const bool appImageRuntime = qEnvironmentVariableIsSet("APPIMAGE")
        || qEnvironmentVariableIsSet("APPDIR");
    if (appImageRuntime) {
        QString opener;
        QStringList arguments;
        const QString systemXdgOpen = QStringLiteral("/usr/bin/xdg-open");
        const QString systemGio = QStringLiteral("/usr/bin/gio");
        if (QFileInfo(systemXdgOpen).isExecutable()) {
            opener = systemXdgOpen;
        } else if (QFileInfo(systemGio).isExecutable()) {
            opener = systemGio;
            arguments.append(QStringLiteral("open"));
        } else {
            opener = QStandardPaths::findExecutable(QStringLiteral("xdg-open"));
        }

        if (!opener.isEmpty()) {
            arguments.append(target.toString(QUrl::FullyEncoded));
            auto* process = new QProcess(this);
            process->setProcessEnvironment(
                decodium::host_process::sanitizedSystemEnvironment());
            process->setProgram(opener);
            process->setArguments(arguments);
            process->setProcessChannelMode(QProcess::MergedChannels);
            process->setProperty("decodiumExternalUrlHandled", false);

            auto const fallbackToQt = [this, process, target, clean, scheme, opener]
                (QString const& reason) {
                if (process->property("decodiumExternalUrlHandled").toBool()) {
                    return;
                }
                process->setProperty("decodiumExternalUrlHandled", true);
                QString const helperOutput = QString::fromLocal8Bit(
                    process->readAll()).trimmed();
                qWarning().noquote()
                    << "[EXTERNAL-URL] host helper failed; trying Qt desktop services"
                    << "helper=" << QFileInfo(opener).fileName()
                    << "reason=" << reason
                    << "output=" << (helperOutput.isEmpty()
                                            ? QStringLiteral("<empty>")
                                            : helperOutput)
                    << "scheme=" << scheme
                    << "host=" << target.host();

                bool const fallbackOk = QDesktopServices::openUrl(target);
                qInfo().noquote()
                    << "[EXTERNAL-URL] Qt desktop services fallback result"
                    << "ok=" << fallbackOk
                    << "scheme=" << scheme
                    << "host=" << target.host();
                if (fallbackOk) {
                    emit statusMessage(
                        QStringLiteral("URL aperto nel browser di sistema: %1")
                            .arg(target.host().isEmpty() ? scheme : target.host()));
                } else {
                    emit errorMessage(QStringLiteral("Impossibile aprire URL: %1").arg(clean));
                }
                process->deleteLater();
            };

            connect(process, &QProcess::started, this,
                    [process, opener, scheme, target] {
                qInfo().noquote()
                    << "[EXTERNAL-URL] host helper started"
                    << "helper=" << QFileInfo(opener).fileName()
                    << "pid=" << process->processId()
                    << "scheme=" << scheme
                    << "host=" << target.host();
            });
            connect(process, &QProcess::errorOccurred, this,
                    [fallbackToQt](QProcess::ProcessError error) {
                fallbackToQt(QStringLiteral("process error %1").arg(int(error)));
            });
            connect(process,
                    qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                    this,
                    [this, process, target, scheme, opener, fallbackToQt]
                    (int exitCode, QProcess::ExitStatus exitStatus) {
                if (process->property("decodiumExternalUrlHandled").toBool()) {
                    return;
                }
                if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                    fallbackToQt(
                        QStringLiteral("exit status %1, code %2")
                            .arg(int(exitStatus))
                            .arg(exitCode));
                    return;
                }

                process->setProperty("decodiumExternalUrlHandled", true);
                qInfo().noquote()
                    << "[EXTERNAL-URL] host helper completed successfully"
                    << "helper=" << QFileInfo(opener).fileName()
                    << "scheme=" << scheme
                    << "host=" << target.host();
                emit statusMessage(
                    QStringLiteral("URL aperto nel browser di sistema: %1")
                        .arg(target.host().isEmpty() ? scheme : target.host()));
                process->deleteLater();
            });
            process->start();

            // The helper is monitored asynchronously.  A non-zero immediate
            // exit is no longer mistaken for a successful browser launch.
            return true;
        }
    }
#endif
    if (!ok) {
        ok = QDesktopServices::openUrl(target);
        qInfo().noquote()
            << "[EXTERNAL-URL] Qt desktop services result"
            << "ok=" << ok
            << "scheme=" << scheme
            << "host=" << target.host();
    }
    if (!ok) {
        emit errorMessage(QStringLiteral("Impossibile aprire URL: %1").arg(clean));
    } else {
        emit statusMessage(QStringLiteral("URL aperto nel browser di sistema: %1")
                               .arg(target.host().isEmpty() ? scheme : target.host()));
    }
    return ok;
}

