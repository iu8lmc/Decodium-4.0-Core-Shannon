// Spot condivisi: Decodium tiene la linea col nodo DX Cluster e ne rivende
// gli spot in rete locale, gia' interpretati, a chi non puo' aprirne una
// propria — il telefono in mano all'operatore, prima di tutto.
//
// E' il gemello di DecodiumCatShare: stessa idea (una risorsa che il PC
// possiede e nessun altro puo' duplicare, offerta in sola lettura sulla rete
// di casa), stessa forma (un interruttore, una porta, nessun comando accettato
// in entrata). La differenza e' cosa viaggia: li' il protocollo rigctl di
// Hamlib, qui una riga JSON per spot.
//
// Il formato e' deliberatamente banale — JSON, una riga per messaggio,
// terminata da avanzamento riga — perche' chi legge dall'altra parte non
// debba portarsi dietro nulla per capirlo:
//
//   {"tipo":"benvenuto","servizio":"decodium-spot-share","versione":1,"attesi":37}
//   {"tipo":"spot","dxCall":"JA1YYY","frequency":14074.0,"spotter":"IK1XXX",
//    "comment":"FT8 -12 dB","time":"1234Z","band":"20m","mode":"FT8",
//    "timestamp":"2026-08-19T10:12:33Z"}
//   {"tipo":"battito"}
//
// Alla connessione partono gli ultimi spot gia' raccolti ("attesi" dice
// quanti), cosi' chi arriva non guarda una lista vuota finche' il cluster non
// si degna; poi ogni spot nuovo appena arriva. Il battito serve a distinguere
// una linea viva e silenziosa da una caduta: su TCP le due cose si somigliano
// per minuti.
//
// By IU8LMC

#ifndef DECODIUMSPOTSHARE_H
#define DECODIUMSPOTSHARE_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>

class QTcpServer;
class QTcpSocket;
class QTimer;
class DecodiumDxCluster;

class DecodiumSpotShare : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool    enabled     READ enabled     NOTIFY configChanged)
    Q_PROPERTY(int     port        READ port        NOTIFY configChanged)
    Q_PROPERTY(bool    listening   READ listening   NOTIFY listeningChanged)
    Q_PROPERTY(int     clientCount READ clientCount NOTIFY clientsChanged)
    Q_PROPERTY(QString status      READ status      NOTIFY listeningChanged)

public:
    explicit DecodiumSpotShare(DecodiumDxCluster* cluster, QObject* parent = nullptr);
    ~DecodiumSpotShare() override;

    bool    enabled()     const { return m_enabled; }
    int     port()        const { return m_port; }
    bool    listening()   const;
    int     clientCount() const { return m_clienti.size(); }
    QString status()      const;

    // Applica la configurazione e apre o chiude la porta di conseguenza.
    // Restituisce false se l'ascolto era richiesto ma non e' riuscito.
    bool configure(bool enabled, int port);

    // Un solo spot come riga JSON. Statica e pubblica perche' e' la
    // definizione del formato: chi scrive un client la legge da qui.
    static QByteArray rigaSpot(const QVariantMap& spot);

signals:
    void configChanged();
    void listeningChanged();
    void clientsChanged();

private slots:
    void onNewConnection();
    void onDisconnected();
    void onNewSpot(const QVariantMap& spot);

private:
    void invia(QTcpSocket* c, const QByteArray& riga);
    void inviaATutti(const QByteArray& riga);

    DecodiumDxCluster* m_cluster {nullptr};
    QTcpServer* m_server {nullptr};
    QTimer* m_battito {nullptr};
    QHash<QTcpSocket*, QByteArray> m_clienti;   // il valore e' il buffer in entrata

    bool m_enabled {false};
    int  m_port {4534};
    QString m_lastError;

    // Quanti spot riceve chi si collega adesso. Cento righe sono una
    // schermata abbondante su un telefono e restano sotto i cinquanta
    // kilobyte: mandare tutto lo storico costerebbe di piu' di quanto valga.
    static constexpr int kSnapshotMax = 100;
    // Un client che non svuota il proprio socket sta trascinando la coda di
    // tutti: oltre questa soglia si considera perso e si stacca. Non e' un
    // limite di banda, e' il segno che dall'altra parte non c'e' piu' nessuno.
    static constexpr qint64 kArretratoMax = 1 << 20;   // 1 MiB
    static constexpr int kBattitoMs = 20000;

    // La porta e' diversa da quella della CAT condivisa (4533) perche' sono
    // due servizi distinti: chi vuole solo gli spot non deve aprire anche la
    // radio, e viceversa.
    static constexpr int kPortaPredefinita = 4534;
};

#endif
