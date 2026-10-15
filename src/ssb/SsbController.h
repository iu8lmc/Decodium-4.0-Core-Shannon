#pragma once
#include "SsbDsp.h"
#include <QObject>
#include <QAudioSource>
#include <QMediaDevices>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantList>
#include <functional>
namespace decodium::ssb {
class SsbController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool transmitting READ transmitting NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(double micPeak READ micPeak NOTIFY changed)
    Q_PROPERTY(QVariantList microphones READ microphones NOTIFY devicesChanged)
    Q_PROPERTY(QString microphone READ microphone WRITE setMicrophone NOTIFY changed)
    Q_PROPERTY(double gainDb READ gainDb WRITE setGainDb NOTIFY changed)
    Q_PROPERTY(bool automatic READ automatic WRITE setAutomatic NOTIFY changed)
    Q_PROPERTY(int lowCut READ lowCut WRITE setLowCut NOTIFY changed)
    Q_PROPERTY(int highCut READ highCut WRITE setHighCut NOTIFY changed)
public:
    struct Hooks {
        std::function<bool()> begin;
        std::function<void()> end;
        std::function<bool()> valid;
        std::function<void(const QVector<short>&)> send;
        std::function<int()> leadMs;
    };
    explicit SsbController(QObject* parent=nullptr);
    ~SsbController() override;
    void setHooks(Hooks h) { hooks=std::move(h); }
    bool transmitting() const { return active; }
    QString status() const { return message; }
    double micPeak() const { return peak; }
    QVariantList microphones() const;
    QString microphone() const { return deviceId; }
    void setMicrophone(QString const& id);
    double gainDb() const { return dsp.gainDb; }
    void setGainDb(double v);
    bool automatic() const { return dsp.automatic; }
    void setAutomatic(bool v);
    int lowCut() const { return int(dsp.lowHz); }
    void setLowCut(int v);
    int highCut() const { return int(dsp.highHz); }
    void setHighCut(int v);
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
signals:
    void changed();
    void devicesChanged();
private:
    void capture();
    void fail(QString const& text);
    void save();
    Hooks hooks;
    SpeechDsp dsp;
    QMediaDevices devices;
    QPointer<QAudioSource> source;
    QPointer<QIODevice> input;
    QAudioFormat format;
    QTimer guard;
    QElapsedTimer clock, lastAudio;
    QByteArray pending;
    QVector<short> packet;
    QString deviceId, message;
    bool active=false;
    double peak=0;
    int phase=0, lead=0;
};
}
