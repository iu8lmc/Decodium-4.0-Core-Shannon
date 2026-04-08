#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

// --- WavManager stub ---
class WavManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(int recordedSeconds READ recordedSeconds NOTIFY recordedSecondsChanged)
public:
    explicit WavManager(QObject* parent = nullptr) : QObject(parent) {}
    bool recording() const { return m_recording; }
    int recordedSeconds() const { return m_recordedSeconds; }

public slots:
    Q_INVOKABLE void startRecording() { m_recording = true; m_recordedSeconds = 0; emit recordingChanged(); }
    Q_INVOKABLE void stopRecording()  { m_recording = false; emit recordingChanged(); }

signals:
    void recordingChanged();
    void recordedSecondsChanged();

private:
    bool m_recording {false};
    int m_recordedSeconds {0};
};

// --- MacroManager stub ---
class MacroManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool contestMode READ contestMode NOTIFY contestModeChanged)
    Q_PROPERTY(QString contestName READ contestName NOTIFY contestNameChanged)
public:
    explicit MacroManager(QObject* parent = nullptr) : QObject(parent) {}
    bool contestMode() const { return m_contestMode; }
    QString contestName() const { return m_contestName; }

public slots:
    Q_INVOKABLE void toggleContest() {
        m_contestMode = !m_contestMode;
        emit contestModeChanged();
    }

signals:
    void contestModeChanged();
    void contestNameChanged();

private:
    bool m_contestMode {false};
    QString m_contestName {"NONE"};
};

// --- BandManager ---
class BandManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int     currentBandIndex  READ currentBandIndex  NOTIFY currentBandChanged)
    Q_PROPERTY(QString currentBandLambda READ currentBandLambda NOTIFY currentBandChanged)
    Q_PROPERTY(QString currentBandName   READ currentBandName   NOTIFY currentBandChanged)
    Q_PROPERTY(double  dialFrequency     READ dialFrequency     NOTIFY currentBandChanged)

public:
    struct Band {
        int     index;
        QString lambda;
        QString name;
        double  ft8Freq;   // FT8 standard (Hz)
        double  ft2Freq;   // FT2 standard (Hz)
        double  ft4Freq;   // FT4 standard (Hz)
    };

    explicit BandManager(QObject* parent = nullptr) : QObject(parent)
    {
        // ft8Freq, ft2Freq, ft4Freq  (0 = usa ft8Freq come fallback)
        m_bands = {
            {  3, "160M", "1.8 MHz",   1840000.0,  1843000.0,        0.0  },
            {  4, "80M",  "3.5 MHz",   3573000.0,  3578000.0,  3575000.0  },
            {  5, "60M",  "5 MHz",     5357000.0,  5360000.0,        0.0  },
            {  6, "40M",  "7 MHz",     7074000.0,  7052000.0,  7047500.0  },
            {  7, "30M",  "10 MHz",   10136000.0, 10144000.0, 10140000.0  },
            {  8, "20M",  "14 MHz",   14074000.0, 14084000.0, 14080000.0  },
            {  9, "17M",  "18 MHz",   18100000.0, 18108000.0, 18104000.0  },
            { 10, "15M",  "21 MHz",   21074000.0, 21144000.0, 21140000.0  },
            { 11, "12M",  "24 MHz",   24915000.0, 24923000.0, 24919000.0  },
            { 12, "10M",  "28 MHz",   28074000.0, 28184000.0, 28180000.0  },
            { 14, "6M",   "50 MHz",   50313000.0, 50313000.0, 50318000.0  },
            { 16, "4M",   "70 MHz",   70154000.0, 70154000.0,       0.0   },
            { 17, "2M",   "144 MHz", 144174000.0,144174000.0,       0.0   },
            { 19, "70CM", "432 MHz", 432174000.0,432174000.0,       0.0   },
        };
    }

    int     currentBandIndex()  const { return m_currentIndex; }
    QString currentBandLambda() const
    {
        for (const Band& b : m_bands) if (b.index == m_currentIndex) return b.lambda;
        return "--";
    }
    QString currentBandName() const
    {
        for (const Band& b : m_bands) if (b.index == m_currentIndex) return b.name;
        return "--";
    }
    double dialFrequency() const { return m_dialFreq; }

    // Ritorna la frequenza per una banda e un modo
    double freqForMode(const Band& b, const QString& mode) const
    {
        if (mode == "FT2" && b.ft2Freq > 0.0) return b.ft2Freq;
        if (mode == "FT4" && b.ft4Freq > 0.0) return b.ft4Freq;
        return b.ft8Freq;
    }

    // Chiamato dalla bridge quando il modo cambia — aggiorna la frequenza senza cambiare banda
    void updateForMode(const QString& mode)
    {
        for (const Band& b : m_bands) {
            if (b.index == m_currentIndex) {
                double freq = freqForMode(b, mode);
                m_dialFreq = freq;
                emit currentBandChanged();
                emit bandFrequencyRequested(freq);
                return;
            }
        }
    }

    // Chiamato dalla bridge quando la frequenza CAT cambia
    void updateFromFrequency(double freqHz)
    {
        double bestDelta = 1e18;
        int    bestIndex = m_currentIndex;
        for (const Band& b : m_bands) {
            double delta = std::abs(b.ft8Freq - freqHz);
            if (delta < bestDelta) { bestDelta = delta; bestIndex = b.index; }
        }
        // Aggiorna solo se siamo entro 500 kHz dalla frequenza nominale della banda
        if (bestDelta < 500000.0 && (bestIndex != m_currentIndex || freqHz != m_dialFreq)) {
            m_currentIndex = bestIndex;
            m_dialFreq = freqHz;
            emit currentBandChanged();
        }
    }

public slots:
    Q_INVOKABLE void changeBand(int bandIndex)
    {
        for (const Band& b : m_bands) {
            if (b.index == bandIndex) {
                m_currentIndex = b.index;
                double freq = freqForMode(b, m_currentMode);
                m_dialFreq = freq;
                emit currentBandChanged();
                emit bandFrequencyRequested(freq);
                return;
            }
        }
    }

    Q_INVOKABLE void changeBandByLambda(const QString& lambda)
    {
        for (const Band& b : m_bands) {
            if (b.lambda.compare(lambda, Qt::CaseInsensitive) == 0) {
                m_currentIndex = b.index;
                double freq = freqForMode(b, m_currentMode);
                m_dialFreq = freq;
                emit currentBandChanged();
                emit bandFrequencyRequested(freq);
                return;
            }
        }
    }

    void setCurrentMode(const QString& mode) { m_currentMode = mode; }

signals:
    void currentBandChanged();
    void bandFrequencyRequested(double freqHz);  // bridge → setFrequency + CAT

private:
    QList<Band> m_bands;
    int    m_currentIndex {8};
    double m_dialFreq     {14074000.0};
    QString m_currentMode {"FT8"};
};

// --- DxClusterManager stub ---
class DxClusterManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled     READ enabled     WRITE setEnabled     NOTIFY enabledChanged)
    Q_PROPERTY(bool connected   READ connected                        NOTIFY connectedChanged)
    Q_PROPERTY(QString host     READ host        WRITE setHost        NOTIFY hostChanged)
    Q_PROPERTY(int    port      READ port        WRITE setPort        NOTIFY portChanged)
    Q_PROPERTY(QString callsign READ callsign    WRITE setCallsign    NOTIFY callsignChanged)

public:
    explicit DxClusterManager(QObject* parent = nullptr) : QObject(parent) {}

    bool    enabled()   const { return m_enabled; }
    void    setEnabled(bool v) { if (m_enabled != v) { m_enabled = v; emit enabledChanged(); } }
    bool    connected() const { return false; }   // stub: mai connesso
    QString host()      const { return m_host; }
    void    setHost(const QString& v) { if (m_host != v) { m_host = v; emit hostChanged(); } }
    int     port()      const { return m_port; }
    void    setPort(int v) { if (m_port != v) { m_port = v; emit portChanged(); } }
    QString callsign()  const { return m_callsign; }
    void    setCallsign(const QString& v) { if (m_callsign != v) { m_callsign = v; emit callsignChanged(); } }

public slots:
    Q_INVOKABLE void connectCluster() {}
    Q_INVOKABLE void disconnectCluster() {}
    Q_INVOKABLE void sendSpot(const QString& /*call*/, double /*freqKHz*/) {}

signals:
    void enabledChanged();
    void connectedChanged();
    void hostChanged();
    void portChanged();
    void callsignChanged();
    void spotReceived(const QString& call, double freqKHz, const QString& info);

private:
    bool    m_enabled  {false};
    QString m_host     {"dxfun.com"};
    int     m_port     {7300};
    QString m_callsign;
};
