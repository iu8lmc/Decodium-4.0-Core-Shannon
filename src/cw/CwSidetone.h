// Il tono del CW a tasto remoto, sentito qui.
//
// Col tasto remoto il tono lo fa il gateway accanto alla radio: chi sta davanti
// a Decodium non sentirebbe niente. Qui lo stesso ritmo (gli istanti del tasto
// che vanno al gateway) diventa un tono sulla scheda audio locale, con le
// rampe che tolgono i colpi di tasto. Segue la stessa coda del gateway: gli
// istanti si mettono in fila, e lo Stop svuota tutto.
#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>

class QThread;

namespace decodium::cw {

class SidetoneSource;
class CwSidetoneWorker;

class CwSidetone : public QObject {
    Q_OBJECT
public:
    explicit CwSidetone(QObject* parent = nullptr);
    ~CwSidetone() override;

    // `deltas`: durata in ms dello stato che PRECEDE ogni istante; `down`: lo
    // stato dopo l'istante (lo stesso modello degli eventi del tasto remoto).
    bool enqueue(const QList<int>& deltasMs, const QList<bool>& down, int toneHz);
    void clear();
    void setDeviceId(const QByteArray& id);
    void setVolume(double volume);   // 0..1
signals:
    void failed(const QString& message);
private:
    QThread* m_thread {nullptr};
    CwSidetoneWorker* m_worker {nullptr};
};

}  // namespace decodium::cw
