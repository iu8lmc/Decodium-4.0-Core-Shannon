// CAT condivisa: Decodium possiede la seriale e la rivende in rete parlando
// il protocollo rigctld di Hamlib, quello che ogni programma della comunita'
// conosce come "Hamlib NET rigctl".
//
// Il formato del dialogo e' quello verificato sul campo e documentato in
// doc/cat-condivisa-protocollo.md: non va modificato a occhio, perche' un
// carattere di troppo nella risposta d'apertura impedisce a chiunque di
// collegarsi, con un messaggio d'errore che non lo dice.
//
// By IU8LMC

#ifndef DECODIUMCATSHARE_H
#define DECODIUMCATSHARE_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <functional>

class QTcpServer;
class QTcpSocket;
class DecodiumTransceiverManager;

class DecodiumCatShare : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool    enabled      READ enabled      NOTIFY configChanged)
    Q_PROPERTY(bool    allowControl READ allowControl NOTIFY configChanged)
    Q_PROPERTY(bool    allowPtt     READ allowPtt     NOTIFY configChanged)
    Q_PROPERTY(int     port         READ port         NOTIFY configChanged)
    Q_PROPERTY(bool    listening    READ listening    NOTIFY listeningChanged)
    Q_PROPERTY(int     clientCount  READ clientCount  NOTIFY clientsChanged)
    Q_PROPERTY(QString status       READ status       NOTIFY listeningChanged)

public:
    // Only cached telemetry is exposed here. CW operations complete on the
    // server thread after the serialised CAT worker has executed them.
    struct State {
        bool connected{false}, ptt{false}, split{false};
        bool strengthValid{false}, alcValid{false};
        double frequency{0}, txFrequency{0}, strengthDb{0};
        double swr{0}, alc{0}, powerWatts{0};
        QString mode;
    };
    using Completion = std::function<void(int)>;
    using SpeedCompletion = std::function<void(int, int)>;
    struct Hooks {
        std::function<State()> state;
        std::function<void(double, bool)> frequency;
        std::function<void(const QString&)> mode;
        std::function<void(bool)> ptt;
        std::function<bool()> morseAvailable;
        std::function<int()> morseTextLimit;
        std::function<void(const QString&, Completion)> sendMorse;
        std::function<void(bool, int, SpeedCompletion)> morseSpeed;
        std::function<void(Completion)> stopMorse;
    };
    explicit DecodiumCatShare(DecodiumTransceiverManager* rig, QObject* parent = nullptr);
    explicit DecodiumCatShare(Hooks hooks, QObject* parent = nullptr);
    ~DecodiumCatShare() override;

    bool    enabled()      const { return m_enabled; }
    bool    allowControl() const { return m_allowControl; }
    bool    allowPtt()     const { return m_allowPtt; }
    int     port()         const { return m_port; }
    bool    listening()    const;
    int     clientCount()  const { return m_buffers.size(); }
    QString status()       const;

    // Applica la configurazione e apre o chiude la porta di conseguenza.
    // Restituisce false se l'ascolto era richiesto ma non e' riuscito.
    bool configure(bool enabled, int port, bool allowControl, bool allowPtt);

    // Conversione fra i nomi di modo di Decodium e quelli di Hamlib.
    static QString toHamlibMode(const QString& decodiumMode);
    static QString fromHamlibMode(const QString& hamlibMode);

    // A local Stop or a lost CAT connection must also invalidate remote text.
    void cancelMorse();

signals:
    void configChanged();
    void listeningChanged();
    void clientsChanged();
    // Emesso quando un client ottiene o si vede rifiutare un comando di
    // scrittura: serve a rendere visibile in interfaccia chi sta comandando.
    void controlAttempt(const QString& command, bool accepted);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QString handleLine(const QString& line);
    QString dumpState() const;
    bool    refuseWrite(const QString& command, bool pttCommand);
    bool handleMorse(QTcpSocket*, const QString&, std::function<void(QString)>);
    void sendNextMorseChunk();
    void stopMorse(Completion completion = {});
    void finishReply(QTcpSocket*, quint64 id, const QString& reply);

    Hooks m_hooks;
    QTcpServer* m_server {nullptr};
    QHash<QTcpSocket*, QByteArray> m_buffers;
    struct Reply { quint64 id; QString text; bool ready{false}; };
    QHash<QTcpSocket*, QQueue<Reply>> m_replies;
    quint64 m_replyId{0};
    QPointer<QTcpSocket> m_morseOwner;
    QString m_morseText;
    QString m_morseChunk;
    QTimer m_morseTimer;
    quint64 m_morseGeneration{0};
    bool m_morseWaiting{false};
    std::function<void(QString)> m_morseAccepted;

    bool m_enabled {false};
    bool m_allowControl {false};
    bool m_allowPtt {false};
    int  m_port {4533};
    QString m_lastError;
};

#endif // DECODIUMCATSHARE_H
