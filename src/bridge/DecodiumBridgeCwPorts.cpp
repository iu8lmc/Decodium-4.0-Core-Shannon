#include "DecodiumBridge.h"
#include "DecodiumSerialPortRegistry.h"
#include <QDir>
#include <QFileInfo>

bool DecodiumBridge::cwRigKeyerAvailable() const
{
    return m_mode == QStringLiteral("CW")
        && !m_decoPortUseRemote
        && m_catBackend == QStringLiteral("hamlib")
        && m_hamlibCat
        && m_hamlibCat->rigMorseAvailable();
}

QString DecodiumBridge::cwSerialKeyerPortReservation(const QString& port) const
{
    // The configured CAT path is not enough during asynchronous reconnects:
    // another Decodium component may already own the same physical endpoint
    // under an alias (COM5 vs \\.\\COM5, or /dev/by-id vs /dev/ttyUSB0).
    // Consult the process-wide lease first so CW never passes a stale
    // configuration check while CAT/DecoPort is opening or closing.
    if (const QString internalOwner = decodium::ports::SerialPortRegistry::ownerForPort(port);
        !internalOwner.isEmpty()) {
        return tr("Decodium's %1").arg(internalOwner);
    }
    if (!m_hamlibCat || m_catBackend != QLatin1String("hamlib"))
        return {};

    // A network/TCI CAT has no local COM/tty to contend with.  USB models may
    // still expose one through Hamlib, hence they use the same comparison as a
    // conventional serial model.
    const QString transport = m_hamlibCat->portType().trimmed().toLower();
    if (transport != QLatin1String("serial") && transport != QLatin1String("usb"))
        return {};

    auto comparablePort = [](QString value) {
        value = value.trimmed();
        if (value.startsWith(QStringLiteral("\\\\.\\")))
            value.remove(0, 4); // Windows permits both COM5 and \\.\COM5.
        if (value.isEmpty()
            || value.compare(QStringLiteral("CAT"), Qt::CaseInsensitive) == 0
            || value.compare(QStringLiteral("None"), Qt::CaseInsensitive) == 0) {
            return QString {};
        }
#if !defined(Q_OS_WIN)
        QFileInfo const info(value);
        const QString canonical = info.canonicalFilePath();
        if (!canonical.isEmpty())
            value = canonical;
#endif
        return QDir::cleanPath(value).toCaseFolded();
    };

    const QString candidate = comparablePort(port);
    if (candidate.isEmpty())
        return {};

    const QString catPort = m_hamlibCat->serialPort().trimmed();
    if (!catPort.isEmpty() && candidate == comparablePort(catPort)) {
        return tr("the Hamlib CAT port %1").arg(catPort);
    }

    // A separate DTR/RTS PTT port is opened by Hamlib too.  It is just as
    // exclusive as the CAT port and must not be reused by the CW keyer.
    const QString pttMethod = m_hamlibCat->pttMethod().trimmed().toUpper();
    const QString pttPort = m_hamlibCat->pttPort().trimmed();
    if ((pttMethod == QLatin1String("DTR") || pttMethod == QLatin1String("RTS"))
        && !pttPort.isEmpty() && candidate == comparablePort(pttPort)) {
        return tr("the Hamlib PTT port %1").arg(pttPort);
    }
    return {};
}

