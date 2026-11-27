#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace decodium::radio {

// The "native" CAT backend (QSerialPort, 15 radios) was removed. Someone who
// used it keeps the same radio, port and speed in the Hamlib backend: the
// fields have the same names, so they are copied from CAT_Native/* to
// Transceiver/* and the backend becomes "hamlib".
//
// `settings` must already be positioned inside the active settings profile.
// Returns the number of values copied, or -1 when the stored backend is not
// "native" and nothing was done. After the first call catBackend is "hamlib",
// so a second call changes nothing.
inline int migrateRemovedNativeCatBackend(QSettings& settings)
{
    if (settings.value(QStringLiteral("catBackend")).toString().trimmed()
            .compare(QStringLiteral("native"), Qt::CaseInsensitive) != 0)
        return -1;

    static QStringList const keys = {
        QStringLiteral("rigName"),        QStringLiteral("serialPort"),
        QStringLiteral("baudRate"),       QStringLiteral("dataBits"),
        QStringLiteral("stopBits"),       QStringLiteral("handshake"),
        QStringLiteral("pttMethod"),      QStringLiteral("pttPort"),
        QStringLiteral("civAddress"),     QStringLiteral("catKeepAlive"),
        QStringLiteral("pollInterval"),   QStringLiteral("forceDtr"),
        QStringLiteral("dtrHigh"),        QStringLiteral("forceRts"),
        QStringLiteral("rtsHigh"),        QStringLiteral("catAutoConnect"),
        QStringLiteral("audioAutoStart"), QStringLiteral("splitMode")};

    int copied = 0;
    for (QString const& key : keys) {
        QString const from = QStringLiteral("CAT_Native/") + key;
        if (!settings.contains(from))
            continue;
        settings.setValue(QStringLiteral("Transceiver/") + key, settings.value(from));
        ++copied;
    }
    settings.setValue(QStringLiteral("catBackend"), QStringLiteral("hamlib"));
    return copied;
}

} // namespace decodium::radio
