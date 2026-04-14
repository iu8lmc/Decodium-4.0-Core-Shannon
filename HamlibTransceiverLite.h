#pragma once
#include <QObject>
#include <QString>
#include <QList>

// Forward declare Hamlib's opaque RIG struct
struct RIG;

class HamlibTransceiverLite : public QObject
{
    Q_OBJECT

public:
    struct RigInfo {
        int modelId;
        QString manufacturer;
        QString modelName;
        QString portType;   // "serial", "network", "usb", "other"
    };

    explicit HamlibTransceiverLite(QObject* parent = nullptr);
    ~HamlibTransceiverLite();

    /// Enumerate all Hamlib-supported rigs (loads backends on first call)
    static QList<RigInfo> availableRigs();

    // --- Connection ---
    bool open(int modelId, const QString& port, int baudRate = 9600,
              int dataBits = 8, int stopBits = 1,
              const QString& handshake = "None");
    void close();
    bool isOpen() const;

    // --- Frequency ---
    double frequency() const;           // Hz
    bool setFrequency(double hz);

    // --- Mode ---
    QString mode() const;               // "USB", "LSB", "CW", "FM", etc.
    bool setMode(const QString& mode);

    // --- PTT ---
    bool ptt() const;
    bool setPtt(bool on);

    // --- Split ---
    bool isSplit() const;
    bool setSplit(bool on, double txFreqHz = 0);

    // --- Info ---
    QString rigName() const;
    QString lastError() const;

signals:
    void frequencyChanged(double hz);
    void modeChanged(const QString& mode);
    void pttChanged(bool on);
    void errorOccurred(const QString& msg);
    void connected();
    void disconnected();

private:
    struct RIG* m_rig = nullptr;
    bool m_isOpen = false;
    QString m_lastError;
    int m_modelId = 0;
};
