#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QDataStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <cmath>
#include <functional>
#include <limits>

// --- WavManager — registrazione audio WAV 16-bit mono a 12 kHz ---
class WavManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(int recordedSeconds READ recordedSeconds NOTIFY recordedSecondsChanged)
    Q_PROPERTY(QString lastFilePath READ lastFilePath NOTIFY lastFilePathChanged)
public:
    explicit WavManager(QObject* parent = nullptr)
        : QObject(parent)
    {
        m_timer.setInterval(1000);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            ++m_recordedSeconds;
            emit recordedSecondsChanged();
        });
    }
    ~WavManager() override { if (m_recording) stopRecording(); }

    bool recording() const { return m_recording; }
    int recordedSeconds() const { return m_recordedSeconds; }
    QString lastFilePath() const { return m_lastFilePath; }

    // Chiamato da DecodiumAudioSink per ogni campione decimato (12 kHz, 16-bit mono)
    void feedSample(short sample)
    {
        if (!m_recording) return;
        QMutexLocker lock(&m_mutex);
        m_samples.append(sample);
    }

    void feedSamples(const QVector<short>& samples)
    {
        if (!m_recording || samples.isEmpty()) return;
        QMutexLocker lock(&m_mutex);
        m_samples += samples;
    }

public slots:
    Q_INVOKABLE void startRecording()
    {
        if (m_recording) return;

        // Directory di salvataggio: ~/Documents/Decodium/recordings/
        QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                      + "/Decodium/recordings";
        QDir().mkpath(dir);

        QString ts = QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss");
        m_lastFilePath = dir + "/decodium_" + ts + ".wav";
        emit lastFilePathChanged();

        {
            QMutexLocker lock(&m_mutex);
            m_samples.clear();
            m_samples.reserve(12000 * 60); // pre-alloca ~1 minuto
        }

        m_recordedSeconds = 0;
        m_recording = true;
        m_timer.start();
        emit recordingChanged();
        emit recordedSecondsChanged();
        qDebug() << "WavManager: recording started →" << m_lastFilePath;
    }

    Q_INVOKABLE void stopRecording()
    {
        if (!m_recording) return;
        m_recording = false;
        m_timer.stop();
        emit recordingChanged();

        // Scrivi WAV su file in modo sincrono (dati gia' in memoria)
        QVector<short> data;
        {
            QMutexLocker lock(&m_mutex);
            data = m_samples;
            m_samples.clear();
        }
        writeWav(m_lastFilePath, data, 12000);
        qDebug() << "WavManager: recording stopped —" << data.size() << "samples →" << m_lastFilePath;
    }

signals:
    void recordingChanged();
    void recordedSecondsChanged();
    void lastFilePathChanged();

private:
    static void writeWav(const QString& path, const QVector<short>& samples, int sampleRate)
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            qWarning() << "WavManager: cannot open" << path;
            return;
        }
        QDataStream s(&f);
        s.setByteOrder(QDataStream::LittleEndian);

        const quint32 dataSize = static_cast<quint32>(samples.size() * 2);
        const quint32 fileSize = 36 + dataSize;

        // RIFF header
        f.write("RIFF", 4);
        s << fileSize;
        f.write("WAVE", 4);

        // fmt chunk
        f.write("fmt ", 4);
        s << quint32(16);            // chunk size
        s << quint16(1);             // PCM
        s << quint16(1);             // mono
        s << quint32(sampleRate);    // sample rate
        s << quint32(sampleRate * 2);// byte rate
        s << quint16(2);             // block align
        s << quint16(16);            // bits per sample

        // data chunk
        f.write("data", 4);
        s << dataSize;
        for (short v : samples) s << qint16(v);
    }

    bool m_recording {false};
    int m_recordedSeconds {0};
    QString m_lastFilePath;
    QTimer m_timer;
    QMutex m_mutex;
    QVector<short> m_samples;
};

// --- MacroManager stub ---
class MacroManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool contestMode READ contestMode NOTIFY contestModeChanged)
    Q_PROPERTY(QString contestName READ contestName NOTIFY contestNameChanged)
    Q_PROPERTY(QStringList contestList READ contestList CONSTANT)
    Q_PROPERTY(int contestId READ contestId WRITE setContestId NOTIFY contestIdChanged)
    Q_PROPERTY(int serialNumber READ serialNumber WRITE setSerialNumber NOTIFY serialNumberChanged)
    Q_PROPERTY(QString exchange READ exchange WRITE setExchange NOTIFY exchangeChanged)
public:
    explicit MacroManager(QObject* parent = nullptr)
        : QObject(parent)
    {
        resetMacrosToDefault();
    }

    bool contestMode() const { return m_contestMode; }
    QString contestName() const
    {
        return m_contestId >= 0 && m_contestId < m_contests.size()
            ? m_contests.at(m_contestId)
            : QStringLiteral("NONE");
    }
    QStringList contestList() const { return m_contests; }
    int contestId() const { return m_contestId; }
    void setContestId(int value)
    {
        int clamped = value;
        if (clamped < 0) clamped = 0;
        if (clamped >= m_contests.size()) clamped = m_contests.size() - 1;
        if (m_contestId == clamped) return;
        m_contestId = clamped;
        bool newContestMode = m_contestId > 0;
        if (m_contestMode != newContestMode) {
            m_contestMode = newContestMode;
            emit contestModeChanged();
        }
        emit contestIdChanged();
        emit contestNameChanged();
    }
    int serialNumber() const { return m_serialNumber; }
    void setSerialNumber(int value)
    {
        int clamped = value < 1 ? 1 : value;
        if (m_serialNumber == clamped) return;
        m_serialNumber = clamped;
        emit serialNumberChanged();
    }
    QString exchange() const { return m_exchange; }
    void setExchange(QString value)
    {
        value = value.trimmed().toUpper();
        if (m_exchange == value) return;
        m_exchange = value;
        emit exchangeChanged();
    }

public slots:
    Q_INVOKABLE void toggleContest() {
        setContestId(m_contestMode ? 0 : 1);
    }

    Q_INVOKABLE int getSerialNumberMax() const { return 9999; }

    Q_INVOKABLE QString getMacroTemplate(int index) const
    {
        return index >= 0 && index < m_macros.size() ? m_macros.at(index) : QString {};
    }

    Q_INVOKABLE void setMacroTemplate(int index, const QString& value)
    {
        if (index < 0) return;
        while (index >= m_macros.size()) m_macros.append(QString {});
        if (m_macros[index] == value) return;
        m_macros[index] = value;
        emit macrosChanged();
    }

    Q_INVOKABLE QString getMacro(int index) const
    {
        return getMacroTemplate(index);
    }

    Q_INVOKABLE void setMacro(int index, const QString& value)
    {
        setMacroTemplate(index, value);
    }

    Q_INVOKABLE void resetMacrosToDefault()
    {
        m_macros = {
            QStringLiteral("%T %M %G4"),
            QStringLiteral("%T %M %R"),
            QStringLiteral("%T %M R%R"),
            QStringLiteral("%T %M RR73"),
            QStringLiteral("%T %M 73"),
            QStringLiteral("CQ %M %G4"),
            QStringLiteral("%T %M %E")
        };
        emit macrosChanged();
    }

    Q_INVOKABLE QString generateContestTx(int txNumber,
                                          const QString& myCall,
                                          const QString& myGrid,
                                          const QString& dxCall,
                                          const QString& dxGrid,
                                          const QString& report,
                                          int) const
    {
        QString result = getMacroTemplate(txNumber - 1);
        if (result.isEmpty()) return QString {};

        const QString grid4 = myGrid.left(4);
        const QString serial = QStringLiteral("%1").arg(m_serialNumber, 4, 10, QChar('0'));
        result.replace("%M", myCall);
        result.replace("%T", dxCall);
        result.replace("%R", report);
        result.replace("%RST", report);
        result.replace("%N", serial);
        result.replace("%G6", myGrid);
        result.replace("%G4", grid4);
        result.replace("%G", dxGrid);
        result.replace("%EXCH", m_exchange);
        result.replace("%E", m_exchange);
        result.replace("%73", "73");
        return result.simplified();
    }

signals:
    void contestModeChanged();
    void contestNameChanged();
    void contestIdChanged();
    void serialNumberChanged();
    void exchangeChanged();
    void macrosChanged();

private:
    bool m_contestMode {false};
    QStringList m_contests {
        QStringLiteral("None"),
        QStringLiteral("Generic"),
        QStringLiteral("ARRL Field Day"),
        QStringLiteral("RTTY Roundup"),
        QStringLiteral("EU VHF"),
        QStringLiteral("Ft2.it Award 2026")
    };
    int m_contestId {0};
    int m_serialNumber {1};
    QString m_exchange;
    QStringList m_macros;
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
        double  lowerBoundHz;
        double  upperBoundHz;
        double  ft8Freq;    // FT8 standard (Hz)
        double  ft2Freq;    // FT2 standard (Hz)
        double  ft4Freq;    // FT4 standard (Hz)
        double  wsprFreq;   // WSPR/FST4W standard (Hz)
        double  jt65Freq;   // JT65 standard (Hz)
        double  jt9Freq;    // JT9 standard (Hz)
        double  jt4Freq;    // JT4 standard (Hz)
        double  q65Freq;    // Q65 standard (Hz)
        double  echoFreq;   // Echo standard (Hz)
        double  msk144Freq; // MSK144 standard (Hz)
    };

    explicit BandManager(QObject* parent = nullptr) : QObject(parent)
    {
        // Frequenze nominali per banda/modo. 0 = assente; il cambio banda usa
        // il primo modo disponibile come fallback quando il modo corrente non
        // ha una QRG specifica su quella banda.
        m_bands = {
            {  3, "160M",   "1.8 MHz",      1800000.0,     2000000.0,     1840000.0,     1843000.0,        0.0,     1836600.0,     1838000.0,     1839000.0,        0.0,           0.0,           0.0,         0.0 },
            {  4, "80M",    "3.5 MHz",      3500000.0,     4000000.0,     3573000.0,     3568000.0,  3575000.0,     3568600.0,     3570000.0,     3572000.0,        0.0,           0.0,           0.0,         0.0 },
            {  5, "60M",    "5 MHz",        5060000.0,     5450000.0,     5357000.0,     5360000.0,        0.0,           0.0,     5357000.0,     5357000.0,        0.0,           0.0,           0.0,         0.0 },
            {  6, "40M",    "7 MHz",        7000000.0,     7300000.0,     7074000.0,     7062000.0,  7047500.0,     7038600.0,     7076000.0,     7078000.0,        0.0,           0.0,           0.0,         0.0 },
            {  7, "30M",    "10 MHz",      10100000.0,    10150000.0,    10136000.0,    10144000.0, 10140000.0,    10138700.0,          0.0,    10140000.0,        0.0,           0.0,           0.0,         0.0 },
            {  8, "20M",    "14 MHz",      14000000.0,    14350000.0,    14074000.0,    14084000.0, 14080000.0,    14095600.0,          0.0,    14078000.0,        0.0,           0.0,           0.0,         0.0 },
            {  9, "17M",    "18 MHz",      18068000.0,    18168000.0,    18100000.0,    18108000.0, 18104000.0,    18104600.0,          0.0,    18104000.0,        0.0,           0.0,           0.0,         0.0 },
            { 10, "15M",    "21 MHz",      21000000.0,    21450000.0,    21074000.0,    21144000.0, 21140000.0,    21094600.0,          0.0,    21078000.0,        0.0,           0.0,           0.0,         0.0 },
            { 11, "12M",    "24 MHz",      24890000.0,    24990000.0,    24915000.0,    24923000.0, 24919000.0,    24924600.0,          0.0,    24919000.0,        0.0,           0.0,           0.0,         0.0 },
            { 12, "10M",    "28 MHz",      28000000.0,    29700000.0,    28074000.0,    28184000.0, 28180000.0,    28124600.0,          0.0,    28078000.0,        0.0,           0.0,           0.0,         0.0 },
            { 13, "8M",     "40 MHz",      40000000.0,    45000000.0,    40680000.0,          0.0,       0.0,           0.0,          0.0,          0.0,        0.0,           0.0,           0.0,         0.0 },
            { 14, "6M",     "50 MHz",      50000000.0,    54000000.0,    50313000.0,    50316000.0, 50318000.0,    50293000.0,    50310000.0,    50312000.0,        0.0,    50275000.0,           0.0,  50380000.0 },
            { 16, "4M",     "70 MHz",      70000000.0,    71000000.0,    70154000.0,    70157000.0,       0.0,    70091000.0,    70102000.0,    70104000.0,        0.0,           0.0,           0.0,  70230000.0 },
            { 17, "2M",     "144 MHz",    144000000.0,   148000000.0,   144174000.0,   144177000.0,144170000.0,   144489000.0,   144120000.0,          0.0,        0.0,   144180000.0,   144120000.0, 144360000.0 },
            { 18, "1.25M",  "222 MHz",    222000000.0,   225000000.0,   222174000.0,   222177000.0,       0.0,           0.0,   222065000.0,          0.0,        0.0,   222065000.0,   222065000.0,         0.0 },
            { 19, "70CM",   "432 MHz",    420000000.0,   450000000.0,   432174000.0,   432177000.0,432170000.0,   432300000.0,   432065000.0,          0.0,        0.0,   432065000.0,   432065000.0, 432360000.0 },
            { 20, "33CM",   "902 MHz",    902000000.0,   928000000.0,          0.0,          0.0,       0.0,           0.0,   902065000.0,          0.0,        0.0,   902065000.0,           0.0,         0.0 },
            { 21, "23CM",   "1296 MHz",  1240000000.0,  1300000000.0,  1296174000.0,  1296177000.0,1296170000.0,  1296500000.0,  1296065000.0,          0.0,        0.0,  1296065000.0,  1296065000.0,         0.0 },
            { 22, "13CM",   "2304 MHz",  2300000000.0,  2450000000.0,          0.0,          0.0,       0.0,           0.0,  2304065000.0,          0.0, 2304065000.0,  2304065000.0,  2304065000.0,         0.0 },
            { 23, "9CM",    "3400 MHz",  3300000000.0,  3500000000.0,          0.0,          0.0,       0.0,           0.0,  3400065000.0,          0.0, 3400065000.0,  3400065000.0,  3400065000.0,         0.0 },
            { 24, "6CM",    "5760 MHz",  5650000000.0,  5925000000.0,          0.0,          0.0,       0.0,           0.0,  5760065000.0,          0.0, 5760065000.0,  5760200000.0,  5760065000.0,         0.0 },
            { 25, "3CM",    "10 GHz",   10000000000.0, 10500000000.0,          0.0,          0.0,       0.0,           0.0,          0.0,          0.0,10368200000.0, 10368200000.0, 10368100000.0,         0.0 },
            { 26, "1.25CM", "24 GHz",   24000000000.0, 24250000000.0,          0.0,          0.0,       0.0,           0.0,          0.0,          0.0,24048200000.0, 24048200000.0, 24048100000.0,         0.0 },
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

    using FrequencyResolver = std::function<double(const QString& bandLambda, const QString& mode)>;
    void setFrequencyResolver(FrequencyResolver resolver) { m_frequencyResolver = resolver; }

    // Ritorna la frequenza per una banda e un modo
    double freqForMode(const Band& b, const QString& mode) const
    {
        QString const normalized = mode.trimmed().toUpper();
        double const configured = configuredFreqForMode(b, normalized);
        if (configured > 0.0) return configured;
        if (normalized == "FT8" && b.ft8Freq > 0.0) return b.ft8Freq;
        if ((normalized == "FT2" || normalized == "FT2-LINK" || normalized == "FT2LINK")
            && b.ft2Freq > 0.0) return b.ft2Freq;
        if (normalized == "FT4" && b.ft4Freq > 0.0) return b.ft4Freq;
        if ((normalized == "WSPR" || normalized == "FST4W") && b.wsprFreq > 0.0) return b.wsprFreq;
        if (normalized == "JT65" && b.jt65Freq > 0.0) return b.jt65Freq;
        if (normalized == "JT9" && b.jt9Freq > 0.0) return b.jt9Freq;
        if (normalized == "JT4" && b.jt4Freq > 0.0) return b.jt4Freq;
        if (normalized == "Q65" && b.q65Freq > 0.0) return b.q65Freq;
        if (normalized == "ECHO" && b.echoFreq > 0.0) return b.echoFreq;
        if (normalized == "MSK144" && b.msk144Freq > 0.0) return b.msk144Freq;
        return firstAvailableFreq(b);
    }

    double firstAvailableFreq(const Band& b) const
    {
        double const candidates[] {
            b.ft8Freq, b.ft2Freq, b.ft4Freq, b.q65Freq, b.jt65Freq,
            b.jt9Freq, b.jt4Freq, b.echoFreq, b.msk144Freq, b.wsprFreq
        };
        for (double const freq : candidates) {
            if (freq > 0.0) return freq;
        }
        return 0.0;
    }

    // Riconosce il modo "nominale" più vicino per una frequenza di dial.
    // Usato per l'auto-selezione iniziale e per seguire le QSY CAT su frequenze assegnate.
    QString detectModeForFrequency(double freqHz, double overrideMaxOffsetHz = -1.0) const
    {
        if (freqHz <= 0.0) return {};

        // Auto-mode deve riconoscere solo le frequenze nominali, non sequestrare
        // QSY manuali vicine a una sub-banda. Esempio: 14.085 MHz puo' essere
        // usata in FT8 da una DXpedition anche se e' vicina alla nominale FT2.
        double const defaultMaxOffsetHz = freqHz >= 50000000.0 ? 1500.0 : 500.0;
        double const maxOffsetHz = overrideMaxOffsetHz > 0.0
            ? overrideMaxOffsetHz
            : defaultMaxOffsetHz;
        double bestDelta = std::numeric_limits<double>::max();
        QString bestMode;

        auto consider = [&](double candidateFreq, const QString& candidateMode) {
            if (candidateFreq <= 0.0) return;
            double const delta = std::abs(candidateFreq - freqHz);
            if (delta < bestDelta) {
                bestDelta = delta;
                bestMode = candidateMode;
            }
        };

        for (const Band& b : m_bands) {
            consider(configuredFreqForMode(b, QStringLiteral("FT8")), QStringLiteral("FT8"));
            consider(configuredFreqForMode(b, QStringLiteral("FT4")), QStringLiteral("FT4"));
            consider(configuredFreqForMode(b, QStringLiteral("FT2")), QStringLiteral("FT2"));
            consider(configuredFreqForMode(b, QStringLiteral("Q65")), QStringLiteral("Q65"));
            consider(configuredFreqForMode(b, QStringLiteral("JT65")), QStringLiteral("JT65"));
            consider(configuredFreqForMode(b, QStringLiteral("JT9")), QStringLiteral("JT9"));
            consider(configuredFreqForMode(b, QStringLiteral("JT4")), QStringLiteral("JT4"));
            consider(configuredFreqForMode(b, QStringLiteral("MSK144")), QStringLiteral("MSK144"));
            consider(configuredFreqForMode(b, QStringLiteral("WSPR")), QStringLiteral("WSPR"));
            consider(configuredFreqForMode(b, QStringLiteral("FST4W")), QStringLiteral("WSPR"));
            consider(configuredFreqForMode(b, QStringLiteral("ECHO")), QStringLiteral("Echo"));
            consider(b.ft8Freq, QStringLiteral("FT8"));
            consider(b.ft4Freq, QStringLiteral("FT4"));
            consider(b.ft2Freq, QStringLiteral("FT2"));
            consider(b.q65Freq, QStringLiteral("Q65"));
            consider(b.jt65Freq, QStringLiteral("JT65"));
            consider(b.jt9Freq, QStringLiteral("JT9"));
            consider(b.jt4Freq, QStringLiteral("JT4"));
            consider(b.msk144Freq, QStringLiteral("MSK144"));
            consider(b.wsprFreq, QStringLiteral("WSPR"));
            consider(b.echoFreq, QStringLiteral("Echo"));
        }

        if (bestMode.isEmpty() || bestDelta > maxOffsetHz) {
            return {};
        }
        return bestMode;
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
        if (freqHz <= 0.0) return;

        for (const Band& b : m_bands) {
            if (b.lowerBoundHz <= freqHz && freqHz <= b.upperBoundHz) {
                if (b.index != m_currentIndex || !qFuzzyCompare(m_dialFreq + 1.0, freqHz + 1.0)) {
                    m_currentIndex = b.index;
                    m_dialFreq = freqHz;
                    emit currentBandChanged();
                }
                return;
            }
        }

        double bestDelta = std::numeric_limits<double>::max();
        int    bestIndex = m_currentIndex;
        for (const Band& b : m_bands) {
            double const reference = firstAvailableFreq(b);
            if (reference <= 0.0) continue;
            double delta = std::abs(reference - freqHz);
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
    double configuredFreqForMode(const Band& b, const QString& normalizedMode) const
    {
        if (!m_frequencyResolver) return 0.0;
        double const freq = m_frequencyResolver(b.lambda, normalizedMode);
        if (freq < b.lowerBoundHz || freq > b.upperBoundHz) return 0.0;
        return freq;
    }

    QList<Band> m_bands;
    int    m_currentIndex {8};
    double m_dialFreq     {14074000.0};
    QString m_currentMode {"FT8"};
    FrequencyResolver m_frequencyResolver;
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
