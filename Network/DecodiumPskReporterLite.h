#ifndef DECODIUM_PSK_REPORTER_LITE_H
#define DECODIUM_PSK_REPORTER_LITE_H

#include <QObject>
#include <QList>
#include <QString>
#include <QDateTime>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

// Lightweight PSK Reporter client that submits spots via HTTP POST with an
// XML payload to http://report.pskreporter.info/mreport.php.
// Spots are buffered and flushed every FLUSH_INTERVAL_MS milliseconds (5 min).
class DecodiumPskReporterLite : public QObject
{
    Q_OBJECT

public:
    explicit DecodiumPskReporterLite(QObject* parent = nullptr);
    ~DecodiumPskReporterLite() override;

    // Configure the local (receiving) station identity.
    // programInfo is sent as the PSK Reporter decoder software name/version.
    void setLocalStation(const QString& callsign,
                         const QString& grid,
                         const QString& programInfo = QStringLiteral("Decodium4"));

    // Buffer a received-station spot. Does not transmit immediately.
    // call    – callsign of the decoded station
    // grid    – Maidenhead locator (may be empty)
    // freqHz  – reception frequency in Hz
    // mode    – e.g. "FT8", "FT4", "FT2"
    // snr     – signal-to-noise ratio in dB
    void addSpot(const QString& call,
                 const QString& grid,
                 quint64 freqHz,
                 const QString& mode,
                 int snr);

    // Transmit all buffered spots to PSK Reporter immediately.
    // If the buffer is empty the call is a no-op.
    // When last == true the flush timer is also stopped.
    void sendReport(bool last = false);

    bool isEnabled()   const { return m_enabled;    }
    void setEnabled(bool v);

    // True when the uploader is enabled and not currently in a failure state.
    // Optimistic-on-enable: the LED turns green as soon as the user activates
    // PSK Reporter, instead of waiting up to 5 minutes for the first batch
    // POST. Only flips to false after a real HTTP failure or auto-disable.
    bool isConnected() const { return m_enabled && !m_autoDisabled && m_consecutiveFailures == 0; }

signals:
    void connectedChanged();
    void errorOccurred(const QString& msg);
    void spotsUploaded(int count);

private slots:
    void onFlushTimer();
    void onReplyFinished(QNetworkReply* reply);

private:
    // Build the XML payload for all currently buffered spots.
    QByteArray buildXmlPayload() const;

    struct Spot {
        QString  call;
        QString  grid;
        QString  mode;
        quint64  freqHz {};
        int      snr    {};
        QDateTime time;
    };

    QNetworkAccessManager* m_nam       {nullptr};
    QTimer*                m_flushTimer{nullptr};
    QList<Spot>            m_spots;
    // Snapshot of the spot list sent in the most recent in-flight POST so that
    // we can report the correct count when the reply arrives.
    int                    m_pendingCount{0};

    QString m_myCall;
    QString m_myGrid;
    QString m_programInfo{QStringLiteral("Decodium4")};

    bool m_enabled    {false};
    bool m_lastSendOk {false};
    int  m_consecutiveFailures {0};
    bool m_autoDisabled        {false};

    // 5 minutes – same cadence as the Shannon IPFIX implementation.
    static constexpr int FLUSH_INTERVAL_MS = 300'000;

    // After this many consecutive HTTP failures, auto-disable the Lite
    // uploader to stop flooding the log. The main IPFIX uploader still works.
    static constexpr int MAX_CONSECUTIVE_FAILURES = 3;

    static constexpr char PSK_REPORTER_URL[] =
        "http://report.pskreporter.info/mreport.php";
};

#endif // DECODIUM_PSK_REPORTER_LITE_H
