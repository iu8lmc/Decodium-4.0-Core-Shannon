// DecoPort — il lato client: trova le radio da solo e ne usa una.
//
// DecoPortDiscovery ascolta gli annunci: l'operatore non digita indirizzi e non
// sceglie protocolli, vede comparire le radio che ci sono.
// DecoPortLink e' il collegamento a una di quelle, dietro l'interfaccia neutra
// RadioLink: chi lo usa non sa, e non deve sapere, che radio ci sia in fondo.
#pragma once

#include "DecoPortPacket.h"
#include "RadioLink.h"

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

class QTimer;
class QUdpSocket;

// ── scoperta ────────────────────────────────────────────────────────────────

class DecoPortDiscovery : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList radios READ radios NOTIFY radiosChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)

public:
    explicit DecoPortDiscovery(QObject* parent = nullptr);
    ~DecoPortDiscovery() override;

    // Senza chiave non si mostra nessuna radio: un annuncio non firmato puo'
    // venire da chiunque, e una radio falsa nell'elenco e' un invito a
    // collegarsi alla macchina sbagliata.
    void setAuthKey(const QByteArray& key) { m_authKey = key; }

    bool listening() const { return m_socket != nullptr; }
    // Una mappa per radio: host, port, rigLabel, streamId, catOnline, ageMs.
    QVariantList radios() const;

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

signals:
    void radiosChanged();
    void listeningChanged();

private slots:
    void onDatagrams();
    void onReap();

private:
    struct Seen {
        QHostAddress address;
        quint16      sessionPort {decoport::kSessionPort};
        quint32      streamId {0};
        QString      rigLabel;
        quint32      stateFlags {0};
        qint64       lastSeenMs {0};
    };

    QUdpSocket* m_socket {nullptr};
    QTimer*     m_reap {nullptr};
    QVector<Seen> m_seen;
    QByteArray  m_authKey;
};

// ── collegamento ────────────────────────────────────────────────────────────

class DecoPortLink : public RadioLink {
    Q_OBJECT
    Q_PROPERTY(bool linked READ isLinked NOTIFY linkedChanged)
    Q_PROPERTY(QString rigLabel READ rigLabel NOTIFY stateChanged)
    Q_PROPERTY(double frequencyHz READ frequencyHz NOTIFY stateChanged)
    Q_PROPERTY(QString modeName READ modeName NOTIFY stateChanged)
    Q_PROPERTY(bool ptt READ ptt NOTIFY stateChanged)
    Q_PROPERTY(double sMeterDbm READ sMeterDbm NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int txAudioLeadMs READ txAudioLeadMs NOTIFY stateChanged)
    // L'indirizzo della radio che stiamo usando.
    Q_PROPERTY(QString peerAddress READ peerAddress NOTIFY linkedChanged)

public:
    explicit DecoPortLink(QObject* parent = nullptr);
    ~DecoPortLink() override;

    void setAuthKey(const QByteArray& key) { m_authKey = key; }
    bool hasAuthKey() const { return !m_authKey.isEmpty(); }

    bool    isLinked() const override { return m_linked; }
    QString rigLabel() const override { return m_state.rigLabel; }
    decoport::Context state() const override { return m_state; }

    void setFrequency(qint64 hz) override;
    void setMode(decoport::Mode mode) override;
    void setPtt(bool on, quint64 whenNs = 0) override;
    void sendTxAudio(const QVector<short>& samples, quint64 playAtNs) override;

    double  frequencyHz() const { return static_cast<double>(m_state.frequencyHz); }
    QString modeName() const { return decoport::modeToString(m_state.mode); }
    bool    ptt() const { return m_state.ptt; }
    double  sMeterDbm() const { return m_state.sMeterDbm(); }
    QString status() const { return m_status; }
    int     txAudioLeadMs() const { return m_state.txAudioLeadMs; }
    quint32 streamId() const { return m_remoteStreamId; }
    QString peerAddress() const {
        return m_peer.isNull() ? QString()
                               : (m_peer.toString() + QLatin1Char(':') + QString::number(m_peerPort));
    }

    // Comodita' per QML: stesse cose con tipi che il QML maneggia bene.
    Q_INVOKABLE bool connectTo(const QString& host, int port = decoport::kSessionPort);
    Q_INVOKABLE void disconnectFromGateway();
    Q_INVOKABLE void tune(double hz);
    Q_INVOKABLE void setModeName(const QString& name);
    Q_INVOKABLE void key(bool on);

signals:
    void statusChanged();
    void remoteStreamChanged(quint32 streamId);
    // Emitted synchronously at authenticated packet decode, before the
    // ordinary RadioLink::rxAudio delivery. Only bounded DirectConnection
    // consumers may attach here.
    void rxAudioProduced(const QVector<short>& samples,
                         quint64 captureTsNs,
                         quint32 streamId);

private slots:
    void onDatagrams();
    void onKeepAlive();

private:
    void setStatus(const QString& s);
    void setLinked(bool v);
    void setRemoteStreamId(quint32 streamId);
    void sendCommand(const decoport::Context& cmd, quint64 whenNs);
    void sendBare(decoport::Type type);

    QByteArray   m_authKey;
    QUdpSocket*  m_socket {nullptr};
    QTimer*      m_keepAlive {nullptr};
    QHostAddress m_peer;
    quint16      m_peerPort {decoport::kSessionPort};

    decoport::Context m_state;
    QString m_status;
    bool    m_linked {false};
    qint64  m_lastContextMs {0};
    quint32 m_commandSeq {0};
    quint32 m_txAudioSeq {0};
    quint32 m_remoteStreamId {0};

    // Diagnosi: buchi nella sequenza dell'audio ricevuto.
    quint32 m_lastRxSeq {0};
    bool    m_haveRxSeq {false};
    qint64  m_rxGaps {0};
};
