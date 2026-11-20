#include "DecodiumBridge.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

void decodiumBridgeAudioLevelLog(const QString&);

// ── Decodium RX dal menu ───────────────────────────────────────────────────
// Il ricevitore da terminale e' un programma a se' (come su DecodiumOS): il
// menu lo avvia in una console propria. Su Windows QProcess::startDetached,
// da un programma senza console, crea il figlio con CREATE_NO_WINDOW: la
// console di decodium-rx sarebbe invisibile. Si chiede quindi una console
// nuova. Su Linux serve un emulatore di terminale.

static QString decodiumRxTerminalPath()
{
#ifdef Q_OS_WIN
    QString const name = QStringLiteral("decodium-rx.exe");
#else
    QString const name = QStringLiteral("decodium-rx");
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(name);
}

bool DecodiumBridge::decodiumRxTerminalAvailable() const
{
    return QFileInfo::exists(decodiumRxTerminalPath());
}

void DecodiumBridge::openDecodiumRxTerminal()
{
    QString const exe = decodiumRxTerminalPath();
    if (!QFileInfo::exists(exe)) {
        emit errorMessage(tr("Decodium RX was not found next to Decodium: %1")
                              .arg(QDir::toNativeSeparators(exe)));
        return;
    }

    QProcess process;
    process.setWorkingDirectory(QCoreApplication::applicationDirPath());
#ifdef Q_OS_WIN
    process.setProgram(exe);
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags &= ~static_cast<DWORD>(CREATE_NO_WINDOW | DETACHED_PROCESS);
        args->flags |= CREATE_NEW_CONSOLE;
        // Con la console nuova valgono i suoi handle, non quelli ereditati.
        args->startupInfo->dwFlags &= ~static_cast<DWORD>(STARTF_USESTDHANDLES);
    });
#else
    // Il primo emulatore di terminale disponibile, con il programma dentro.
    QString terminal;
    for (QString const& candidate : {QStringLiteral("x-terminal-emulator"), QStringLiteral("gnome-terminal"),
                                     QStringLiteral("konsole"), QStringLiteral("xfce4-terminal"),
                                     QStringLiteral("xterm")}) {
        terminal = QStandardPaths::findExecutable(candidate);
        if (!terminal.isEmpty()) {
            break;
        }
    }
    if (terminal.isEmpty()) {
        emit errorMessage(tr("Decodium RX needs a terminal window: start %1 from a terminal")
                              .arg(exe));
        return;
    }
    process.setProgram(terminal);
    process.setArguments(QFileInfo(terminal).fileName() == QStringLiteral("gnome-terminal")
                             ? QStringList {QStringLiteral("--"), exe}
                             : QStringList {QStringLiteral("-e"), exe});
#endif
    qint64 pid = 0;
    if (!process.startDetached(&pid)) {
        emit errorMessage(tr("Decodium RX could not be started"));
        decodiumBridgeAudioLevelLog(QStringLiteral("[DECODIUM-RX] avvio non riuscito: %1").arg(exe));
        return;
    }
    decodiumBridgeAudioLevelLog(QStringLiteral("[DECODIUM-RX] aperto dal menu (pid %1)").arg(pid));
}
