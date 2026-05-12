// -*- Mode: C++ -*-
//
// DecoSyncTime fase 2 — HTTPS Date time source.
//
// Esegue richieste HTTP HEAD a una lista di server (Google, Cloudflare,
// Microsoft, NIST) e parsa il `Date:` header per ricavare un offset di
// tempo rispetto al clock locale. Utile quando la porta UDP 123 (NTP)
// e' bloccata dal firewall ISP / router consumer.
//
// Accuracy attesa: ~50-300 ms (limitato dalla risoluzione 1s del header
// HTTP e dall'asimmetria RTT). NTP UDP nominalmente migliore quando
// disponibile, ma HTTPS funziona ovunque c'e' web.
//
// Algoritmo per ogni endpoint:
//   T1 = clock locale al SEND
//   T2 = clock locale alla RICEZIONE
//   T_server = parse(Date: header)
//   RTT = T2 - T1
//   server_at_T2_estimated = T_server + RTT/2  (assume symmetric latency)
//   offset_ms = server_at_T2_estimated - T2
//
// Multi-endpoint = median(offsets), filtrato RTT < maxRtt e ABS(offset) < 1h.

#ifndef HTTPS_TIME_SOURCE_HPP__
#define HTTPS_TIME_SOURCE_HPP__

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QHash>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

class HttpsTimeSource : public QObject
{
    Q_OBJECT

public:
    explicit HttpsTimeSource(QObject* parent = nullptr);
    ~HttpsTimeSource() override;

    void setEnabled(bool on);
    void syncNow();

    bool   isSynced() const { return m_synced; }
    double offsetMs() const { return m_offsetMs; }
    double lastRttMs() const { return m_lastRttMs; }
    int    lastEndpointCount() const { return m_lastEndpointCount; }

Q_SIGNALS:
    // Emessi quando un cycle di query completa con almeno 1 risposta valida.
    // offsetMs = median pesato degli endpoint validi
    // rttMs    = RTT medio degli endpoint che hanno contribuito
    // count    = numero endpoint validi nel cycle
    void offsetUpdated(double offsetMs, double rttMs, int endpointCount);
    void syncStatusChanged(bool synced, QString const& statusText);
    void errorOccurred(QString const& errorMsg);

private Q_SLOTS:
    void onReply(QNetworkReply* reply);
    void onRefreshTimeout();

private:
    struct PendingRequest {
        qint64 t1Ms;   // local clock at send
    };
    struct EndpointSample {
        double offsetMs {0.0};
        double rttMs {0.0};
    };

    void launchOneQueryCycle();

    QNetworkAccessManager* m_nam {nullptr};
    QTimer m_refreshTimer;
    QStringList m_endpoints;
    QHash<QNetworkReply*, PendingRequest> m_pending;
    QVector<EndpointSample> m_collected;
    int m_inFlight {0};

    bool   m_enabled {false};
    bool   m_synced {false};
    double m_offsetMs {0.0};
    double m_lastRttMs {0.0};
    int    m_lastEndpointCount {0};

    static constexpr int kRefreshIntervalMs = 300000;  // 5 min in steady state
    static constexpr int kRetryIntervalMs = 30000;     // 30 s when not synced
    static constexpr int kRequestTimeoutMs = 8000;     // 8 s per request
    static constexpr double kMaxRttMs = 2000.0;        // ignora replies > 2 s RTT
    static constexpr double kMaxOffsetMs = 3600000.0;  // 1 h sanity gate
};

#endif  // HTTPS_TIME_SOURCE_HPP__
