// -*- Mode: C++ -*-
//
// DecoSyncTime (Fase 1): servizio centrale di time sync per Decodium.
//
// In Fase 1 wrappa il NtpClient esistente per garantire backward-compat
// con tutto il codice del bridge, ma:
//   - default ENABLED (NtpClient era OFF di default)
//   - espone wallclockMs() come API centrale che sara' il single point of
//     truth per tutti i timestamp (consumer migrati gradualmente)
//   - struttura ready per aggiungere fonti aggiuntive nelle fasi
//     successive: HTTPS Date, RoughTime, Cloudflare time, decoder-based
//     self-calibration.
//
// Fasi successive previste:
//   Fase 2  Source HTTPS Date in parallelo a NTP (Google/Cloudflare HEAD)
//   Fase 3  Kalman filter su offset + drift rate (compensa drift del PC)
//   Fase 4  Self-calibration dai decode FT8/FT4 ricevuti
//   Fase 5  UI panel con plot offset/drift + quality per source

#ifndef DECO_SYNC_TIME_HPP__
#define DECO_SYNC_TIME_HPP__

#include <QObject>
#include <QString>
#include <QTimer>

#include "DecoSyncKalman.hpp"

class NtpClient;
class HttpsTimeSource;

class DecoSyncTime : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool   enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(bool   synced  READ synced  NOTIFY syncedChanged)
    Q_PROPERTY(double offsetMs READ offsetMs NOTIFY offsetUpdated)

public:
    explicit DecoSyncTime(QObject *parent = nullptr);
    ~DecoSyncTime() override;

    bool   enabled() const { return m_enabled; }
    bool   synced()  const;
    double offsetMs() const;
    int    lastServerCount() const;

    void setEnabled(bool on);
    void setCustomServer(QString const& server);
    void setRefreshInterval(int ms);
    void setMaxRtt(double ms);
    void setInitialOffset(double offsetMs);

    // API centrale del time sync: timestamp ms epoch corretto dell'offset
    // misurato dal sistema. Tutti i consumer (decoder, sequencer, logger)
    // dovrebbero usare questa invece di QDateTime::currentMSecsSinceEpoch().
    Q_INVOKABLE qint64 wallclockMs() const;

    // Accesso al NtpClient interno per la migrazione graduale del codice
    // bridge legacy che ha riferimenti diretti. Verra' rimosso una volta
    // tutta la chiamateria sara' passata via DecoSyncTime API.
    NtpClient* legacyNtpClient() const { return m_ntp; }

    // Fase 2 — source HTTPS Date. Disponibile read-only per il QML / monitor.
    Q_INVOKABLE bool   httpsSynced()    const;
    Q_INVOKABLE double httpsOffsetMs()  const;
    Q_INVOKABLE int    httpsLastCount() const;

    // Fase 3 — Kalman filter offset+drift. Espone stato filtrato.
    Q_INVOKABLE double kalmanOffsetMs()      const { return m_kalman.offsetMs(); }
    Q_INVOKABLE double kalmanDriftMsPerSec() const { return m_kalman.driftMsPerSec(); }
    Q_INVOKABLE bool   kalmanHasLock()       const { return m_kalman.hasLock(); }

public Q_SLOTS:
    void syncNow();

Q_SIGNALS:
    void enabledChanged();
    void syncedChanged(bool synced, QString const& statusText);
    void offsetUpdated(double offsetMs);
    void errorOccurred(QString const& errorMsg);

private:
    void recomputeCombinedOffset(QString const& reason);
    void ingestKalmanSample(double offsetMs, double rttMs, char const* source);
    void onKalmanTick();

    NtpClient*       m_ntp {nullptr};
    HttpsTimeSource* m_https {nullptr};
    DecoSyncKalman   m_kalman;
    QTimer           m_kalmanTickTimer;
    qint64           m_kalmanLastTickMs {0};
    bool             m_enabled {true};  // default ON
    bool             m_lastEmittedSynced {false};
    double           m_lastEmittedOffsetMs {0.0};
};

#endif  // DECO_SYNC_TIME_HPP__
