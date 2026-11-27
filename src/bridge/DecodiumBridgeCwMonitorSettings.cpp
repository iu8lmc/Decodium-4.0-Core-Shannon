#include "DecodiumBridge.h"
#include "CwSidetone.h"
#include "CwTiming.h"
#include <QAudioDevice>
#include <QMediaDevices>
#include <QSettings>
#include <QTimer>

void decodiumBridgeAudioLevelLog(const QString&);

bool DecodiumBridge::cwMonitorEnabled() const { return QSettings().value("CW/monitorEnabled", true).toBool(); }
int DecodiumBridge::cwMonitorVolume() const { return qBound(0, QSettings().value("CW/monitorVolume", 35).toInt(), 100); }
QString DecodiumBridge::cwMonitorDevice() const { return QSettings().value("CW/monitorDevice").toString(); }
void DecodiumBridge::setCwMonitorEnabled(bool enabled) {
    QSettings().setValue("CW/monitorEnabled", enabled);
    if (!enabled && m_cwSidetone) m_cwSidetone->clear();
    emit cwMonitorChanged();
}
void DecodiumBridge::setCwMonitorVolume(int volume) {
    QSettings().setValue("CW/monitorVolume", qBound(0, volume, 100));
    if (m_cwSidetone) m_cwSidetone->setVolume(cwMonitorVolume() / 100.0);
    emit cwMonitorChanged();
}
void DecodiumBridge::setCwMonitorDevice(const QString& device) {
    QSettings().setValue("CW/monitorDevice", device);
    if (m_cwSidetone) m_cwSidetone->setDeviceId(QByteArray::fromBase64(device.toLatin1()));
    emit cwMonitorChanged();
}
QVariantList DecodiumBridge::cwMonitorOutputs() const {
    QVariantList outputs {QVariantMap{{"id", ""}, {"label", tr("System default output")}}};
    for (const auto& device : QMediaDevices::audioOutputs())
        outputs.append(QVariantMap{{"id", QString::fromLatin1(device.id().toBase64())}, {"label", device.description()}});
    return outputs;
}
void DecodiumBridge::testCwMonitor() {
    if (!cwMonitorEnabled()) {
        decodiumBridgeAudioLevelLog(QStringLiteral("CW sidetone test ignored: local sidetone disabled"));
        emit statusMessage(tr("CW tone test skipped: enable TX sidetone on this computer first."));
        return;
    }
    if (m_cwRemoteKeying || m_transmitting || m_tuning) {
        decodiumBridgeAudioLevelLog(QStringLiteral("CW sidetone test ignored: radio path busy remote_key=%1 tx=%2 tune=%3")
                      .arg(m_cwRemoteKeying ? 1 : 0)
                      .arg(m_transmitting ? 1 : 0)
                      .arg(m_tuning ? 1 : 0));
        emit statusMessage(tr("CW tone test skipped while the radio is transmitting or tuning."));
        return;
    }
    if (m_cwToneTestPending) {
        decodiumBridgeAudioLevelLog(QStringLiteral("CW sidetone test ignored: test already in progress"));
        emit statusMessage(tr("CW tone test is already running."));
        return;
    }
    decodiumBridgeAudioLevelLog(QStringLiteral("CW sidetone test requested: output_id=%1 volume=%2 tone_hz=700 message=V")
                  .arg(cwMonitorDevice().isEmpty() ? QStringLiteral("default") : cwMonitorDevice())
                  .arg(cwMonitorVolume()));
    QVector<CwKeyEvent> events;
    int durationMs = 0;
    for (const auto& e : decodium::cw::timelineFor(QStringLiteral("V"), 20)) {
        events.append({e.deltaMs, e.down});
        durationMs += static_cast<int>(e.deltaMs);
    }
    if (playCwMonitor(events, 700)) {
        m_cwToneTestPending = true;
        // The audio worker owns the sink; this timer only releases the UI-side
        // admission guard and never waits for audio or touches QAudioSink.
        QTimer::singleShot(qMax(250, durationMs + 200), this, [this]() {
            m_cwToneTestPending = false;
        });
        emit statusMessage(tr("CW tone test queued for the selected local audio output (V)."));
    }
}

bool DecodiumBridge::playCwMonitor(const QVector<CwKeyEvent>& events, int toneHz)
{
    if (!cwMonitorEnabled())
        return false;

    {
        // Il tono lo fa il gateway accanto alla radio: qui lo stesso ritmo
        // diventa il tono che sente chi sta davanti a Decodium.
        if (!m_cwSidetone) {
            m_cwSidetone = new decodium::cw::CwSidetone(this);
            connect(m_cwSidetone, &decodium::cw::CwSidetone::failed,
                    this, &DecodiumBridge::errorMessage);
        }
        m_cwSidetone->setDeviceId(QByteArray::fromBase64(cwMonitorDevice().toLatin1()));
        m_cwSidetone->setVolume(cwMonitorVolume() / 100.0);
        QList<int> deltas;
        QList<bool> down;
        for (const CwKeyEvent& ev : events) {
            deltas.append(ev.deltaMs);
            down.append(ev.down);
        }
        return m_cwSidetone->enqueue(deltas, down, toneHz);
    }
    return false;
}

