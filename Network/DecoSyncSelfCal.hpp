// -*- Mode: C++ -*-
//
// DecoSyncTime fase 4 — self-calibration dal network amateur.
//
// Idea: i decoder FT8/FT4/FT2 producono per ogni decode un valore `dt`
// (secondi) che rappresenta l'offset di timing del segnale ricevuto
// rispetto al boundary di period atteso (HH:MM:00, HH:MM:15, ...).
//
// Se TUTTI i decode in un period hanno lo stesso bias di dt (es. tutti
// +0.4s), significa che IL TUO clock locale e' in anticipo di 0.4s —
// la statistica del network amateur diventa source of truth.
//
// Convenzione: dt positivo = signal arrivato dopo my_boundary
//   ⇒ my_clock e' in anticipo (my_boundary e' prima del correct boundary)
//   ⇒ offset_correction = -median(dt) * 1000  [ms]
//
// Filtri:
//  - SNR threshold (default -15 dB): scarta decode marginali (dt noisy)
//  - |dt| < kMaxDtSec (default 4s): scarta decode fuori range del decoder
//  - n >= kMinSamples (default 5): bisogno di abbastanza decode per
//    affidabilita' statistica
//  - flush dopo periodMs millisecondi di silenzio: chiude finestra periodo
//
// Varianza emessa: derivata da dispersione campione + jitter floor
// (~150ms decoder timing accuracy tipica).

#ifndef DECO_SYNC_SELFCAL_HPP__
#define DECO_SYNC_SELFCAL_HPP__

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

class DecoSyncSelfCal : public QObject
{
    Q_OBJECT
public:
    explicit DecoSyncSelfCal(QObject* parent = nullptr);

    // Configurabile a runtime: durata media periodo per stimare quando
    // chiudere la finestra di accumulo (FT8=15s, FT4=7.5s, FT2=2.5s).
    void setPeriodMs(int ms);

    // Threshold SNR sotto cui i decode vengono ignorati (dt = noisy).
    void setSnrThreshold(int db) { m_snrThresholdDb = db; }

    // Numero minimo di sample in un period prima di emettere sample.
    void setMinSamples(int n) { m_minSamples = qMax(2, n); }

    // Abilita / disabilita il self-calibration. Quando off, addDecodeDt
    // viene ignorato (nessun cycle aperto).
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }

    // Chiamato dal bridge per ogni decode valido (chunked dal callback
    // decode-ready). dt in secondi, snr in dB.
    void addDecodeDt(double dtSec, int snrDb);

    // Stato della finestra corrente (per debug / UI panel).
    int    pendingSampleCount() const { return m_pending.size(); }
    double lastEmittedOffsetMs() const { return m_lastOffsetMs; }
    double lastEmittedVarianceMs2() const { return m_lastVarianceMs2; }

Q_SIGNALS:
    // Emesso quando una finestra di periodo si chiude con abbastanza
    // sample: offsetMs e' la correzione stimata (da ingerire nel
    // Kalman), varianceMs2 e' la varianza measurement consigliata.
    void offsetEstimateUpdated(double offsetMs, double varianceMs2, int sampleCount);

private Q_SLOTS:
    void onFlushTimeout();

private:
    void emitIfReady();

    QTimer  m_flushTimer;
    QVector<double> m_pending;  // dt in seconds, only after SNR/range filter
    int     m_periodMs {15000};
    int     m_snrThresholdDb {-15};
    int     m_minSamples {5};
    bool    m_enabled {true};
    double  m_lastOffsetMs {0.0};
    double  m_lastVarianceMs2 {0.0};

    static constexpr double kMaxDtSec = 4.0;
};

#endif  // DECO_SYNC_SELFCAL_HPP__
