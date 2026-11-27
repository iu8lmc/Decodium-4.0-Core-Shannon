#include "Network/DecodiumWsprUploader.h"
#include "Network/wsprnet.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>

DecodiumWsprUploader::DecodiumWsprUploader(QObject* parent)
    : QObject(parent)
{
    m_nam     = new QNetworkAccessManager(this);
    m_wsprNet = new WSPRNet(m_nam, this);

    connect(m_wsprNet, &WSPRNet::uploadStatus,
            this,      &DecodiumWsprUploader::uploadStatus);
}

void DecodiumWsprUploader::uploadSpot(const QString& decodeText, double dialFreqHz,
                                      const QString& txPct, const QString& dbm)
{
    if (!m_enabled || m_callsign.isEmpty())
        return;

    const QString freqMhz = QString::number(dialFreqHz / 1.0e6, 'f', 6);

    QString version = QCoreApplication::applicationVersion().trimmed();
    if (version.isEmpty())
        version = QStringLiteral("4.0");

    m_wsprNet->post(m_callsign, m_grid,
                    freqMhz, freqMhz,
                    "WSPR", 120.0f,
                    txPct, dbm,
                    QStringLiteral("Decodium/%1").arg(version),
                    decodeText);
}
