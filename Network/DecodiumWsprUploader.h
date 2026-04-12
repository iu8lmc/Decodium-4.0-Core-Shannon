#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class WSPRNet;

class DecodiumWsprUploader : public QObject
{
    Q_OBJECT

public:
    explicit DecodiumWsprUploader(QObject* parent = nullptr);

    void setCallsign(const QString& call) { m_callsign = call; }
    void setGrid(const QString& grid)     { m_grid = grid; }
    void setEnabled(bool v)               { m_enabled = v; }
    bool isEnabled() const                { return m_enabled; }

    // Upload a single decoded WSPR spot to WSPRnet.
    // decodeText : raw decode line from WSPRDecodeWorker
    // dialFreqHz : dial frequency in Hz
    // txPct      : TX percentage string (default "0")
    // dbm        : TX power in dBm string (default "0")
    void uploadSpot(const QString& decodeText, double dialFreqHz,
                    const QString& txPct = "0", const QString& dbm = "0");

signals:
    void uploadStatus(const QString& msg);

private:
    WSPRNet*                m_wsprNet {nullptr};
    QNetworkAccessManager*  m_nam     {nullptr};
    QString                 m_callsign;
    QString                 m_grid;
    bool                    m_enabled {false};
};
