// Il tono del CW a tasto remoto, sentito qui.
//
// Col tasto remoto il tono lo fa il gateway accanto alla radio: chi sta davanti
// a Decodium non sentirebbe niente. Qui lo stesso ritmo (gli istanti del tasto
// che vanno al gateway) diventa un tono sulla scheda audio locale, con le
// rampe che tolgono i colpi di tasto. Segue la stessa coda del gateway: gli
// istanti si mettono in fila, e lo Stop svuota tutto.
#pragma once

#include <QAudioSink>
#include <QIODevice>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <memory>

namespace decodium::cw {

class SidetoneSource;

class CwSidetone : public QObject {
    Q_OBJECT
public:
    explicit CwSidetone(QObject* parent = nullptr);
    ~CwSidetone() override;

    // `deltas`: durata in ms dello stato che PRECEDE ogni istante; `down`: lo
    // stato dopo l'istante (lo stesso modello degli eventi del tasto remoto).
    void enqueue(const QList<int>& deltasMs, const QList<bool>& down, int toneHz);
    void clear();
    void setVolume(double volume);   // 0..1

private:
    void ensureSink();
    void idleCheck();

    std::unique_ptr<SidetoneSource> m_source;
    QPointer<QAudioSink> m_sink;
    QTimer m_idle;
    double m_volume {0.35};
};

}  // namespace decodium::cw
