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
    explicit DecodiumCatShare(DecodiumTransceiverManager* rig, QObject* parent = nullptr);
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

    DecodiumTransceiverManager* m_rig {nullptr};
    QTcpServer* m_server {nullptr};
    QHash<QTcpSocket*, QByteArray> m_buffers;

    bool m_enabled {false};
    bool m_allowControl {false};
    bool m_allowPtt {false};
    int  m_port {4533};
    QString m_lastError;
};

#endif // DECODIUMCATSHARE_H
