#ifndef DECODIUMCLOUDLOGLITE_H
#define DECODIUMCLOUDLOGLITE_H

#include <QObject>
#include <QString>
#include <QDateTime>

class QNetworkAccessManager;

// ---------------------------------------------------------------------------
// DecodiumCloudlogLite
//
// Lightweight Cloudlog integration that does NOT depend on Boost or the
// legacy pimpl/Configuration infrastructure. All settings are pushed in
// explicitly so the class can be driven from QML or plain C++.
// ---------------------------------------------------------------------------
class DecodiumCloudlogLite : public QObject
{
    Q_OBJECT

public:
    explicit DecodiumCloudlogLite(QObject* parent = nullptr);
    ~DecodiumCloudlogLite() override;

    // --- Configuration setters -------------------------------------------
    void setApiUrl(const QString& url);   // e.g. "http://192.168.1.10/index.php"
    void setApiKey(const QString& key);
    void setStationId(int id);            // station_profile_id, default 1

    // --- Enable / disable without destroying the object ------------------
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool v) { m_enabled = v; }

    // --- QSO logging -------------------------------------------------------
    // Builds an ADIF record and POSTs it to the Cloudlog API.
    // Parameters: DX callsign, DX grid, frequency in Hz, mode string,
    //             UTC QSO date/time, SNR, RST sent, RST received,
    //             own callsign, own grid.
    void logQso(const QString& dxCall,
                const QString& dxGrid,
                double         freqHz,
                const QString& mode,
                const QDateTime& utcTime,
                int            snr,
                const QString& reportSent,
                const QString& reportRcvd,
                const QString& myCall,
                const QString& myGrid);

    // --- Connectivity test -------------------------------------------------
    // GETs {m_apiUrl}/api/auth?key={m_apiKey} and emits apiKeyOk() or
    // apiKeyInvalid() depending on the JSON response.
    Q_INVOKABLE void testApi();

signals:
    void apiKeyOk();
    void apiKeyInvalid();
    void qsoLogged(const QString& dxCall);
    void errorOccurred(const QString& msg);

private:
    // Builds a standard ADIF record string from the given QSO parameters.
    QString buildAdifRecord(const QString& dxCall,
                            const QString& dxGrid,
                            double         freqHz,
                            const QString& mode,
                            const QDateTime& utcTime,
                            int            snr,
                            const QString& reportSent,
                            const QString& reportRcvd,
                            const QString& myCall,
                            const QString& myGrid) const;

    // Returns the amateur band string (e.g. "20M") for a frequency in Hz.
    static QString bandFromHz(double freqHz);

    QNetworkAccessManager* m_nam      {nullptr};
    QString                m_apiUrl;
    QString                m_apiKey;
    int                    m_stationId{1};
    bool                   m_enabled  {false};
};

#endif // DECODIUMCLOUDLOGLITE_H
