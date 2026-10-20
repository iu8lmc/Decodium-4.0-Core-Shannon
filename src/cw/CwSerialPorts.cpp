#include "CwSerialPorts.h"

#include <QFileInfo>
#include <QSerialPortInfo>
#include <QSet>

#include <algorithm>

namespace decodium::cw {

namespace {

bool isPseudoTerminal(const QString& path)
{
#if defined(Q_OS_MACOS)
    // /dev/ttys000, /dev/ttys001, ... sono le sessioni del terminale macOS.
    // Le porte USB/Bluetooth usabili per CAT sono normalmente /dev/cu.*.
    return path.startsWith(QStringLiteral("/dev/ttys"));
#elif defined(Q_OS_LINUX)
    return path.startsWith(QStringLiteral("/dev/pts/"));
#else
    Q_UNUSED(path)
    return false;
#endif
}

QString portPath(const QSerialPortInfo& info)
{
#if defined(Q_OS_WIN)
    return info.portName().trimmed();
#else
    const QString location = info.systemLocation().trimmed();
    return location.isEmpty() ? info.portName().trimmed() : location;
#endif
}

} // namespace

QStringList availableSerialPorts()
{
    QStringList ports;
    QSet<QString> seen;

    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        if (info.portName().contains(QStringLiteral("NULL"), Qt::CaseInsensitive))
            continue;

        const QString path = portPath(info);
        if (path.isEmpty() || isPseudoTerminal(path))
            continue;

        // Deduplica i due alias che alcuni driver pubblicano per la stessa USB.
        const QString canonical = QFileInfo(path).canonicalFilePath();
        const QString key = canonical.isEmpty() ? path : canonical;
        if (seen.contains(key))
            continue;
        seen.insert(key);
        ports << path;
    }

    std::sort(ports.begin(), ports.end(), [](const QString& a, const QString& b) {
        return QString::localeAwareCompare(a, b) < 0;
    });
    return ports;
}

} // namespace decodium::cw
