// Logica del rotore: polling, target, finecorsa software e watchdog.
//
// Un solo thread parla con la seriale. Tutto il resto (finestra, server di
// rete) e' lettore: chiede una fotografia (snapshot) o accoda un comando, senza
// mai toccare la porta direttamente.
#pragma once

#include "RotorConfig.h"
#include "RotorGeo.h"
#include "RotorProtocol.h"
#include "RotorTransport.h"

#include <QElapsedTimer>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>

namespace decodium::rotor {

// Una richiesta che non si puo' soddisfare (angolo, locatore, memoria).
struct RotorError : std::runtime_error {
    explicit RotorError(const QString& what) : std::runtime_error(what.toStdString()) {}
    QString message() const { return QString::fromStdString(what()); }
};

class RotorController : public QObject {
    Q_OBJECT

public:
    explicit RotorController(const Config& config, QObject* parent = nullptr);
    ~RotorController() override;

    void start();
    void shutdown();
    bool isRunning() const { return m_thread != nullptr; }

    // ── comandi (thread-safe: li esegue il thread del rotore) ──
    // Muove uno o entrambi gli assi, applicando i finecorsa software. Torna gli
    // angoli davvero applicati ({"az": ..., "el": ...}).
    QVariantMap goTo(std::optional<double> az, std::optional<double> el);
    void halt(const QString& axis = QStringLiteral("all"), bool fast = false);
    QVariantMap park();
    void clientAttached();
    // Se l'ultimo client cade durante un movimento, ferma il rotore.
    void clientDetached();

    // ── puntamento assistito e memorie ──
    Bearing bearingTo(const QString& locator) const;
    QVariantMap goToLocator(const QString& locator, bool longPath = false);
    QVariantList presets() const;
    QVariantList savePreset(const QString& name, double az, std::optional<double> el = std::nullopt);
    QVariantList deletePreset(const QString& name);
    QVariantMap recallPreset(const QString& name);

    // ── stazione e configurazione ──
    void setStation(const QString& callsign, const QString& locator);
    QString locator() const;
    Config config() const;
    // Sostituisce la configurazione "a caldo" (finecorsa, tolleranze, memorie...).
    // Seriale e modello valgono alla prossima apertura.
    void applyConfig(const Config& config);

    // ── diagnostica ──
    QVariantList recentTraffic(int limit = 50) const;
    QVariantList recentHistory(int limit = 300) const;
    // Fotografia dello stato, con le stesse chiavi che il protocollo di rete
    // consegna ai client.
    QVariantMap snapshot() const;

signals:
    // A ogni giro di polling. Il corpo dello stato si legge con snapshot().
    void stateUpdated();

private:
    struct AxisState {
        std::optional<double> position;
        std::optional<double> target;
        bool moving {false};
        qint64 lastChangeMs {0};
    };

    void enqueue(std::function<void()> action);
    int multiplier() const;
    char axisId(const QString& name) const;
    bool openLink();
    const Model* detectModel();
    void pollAxis(const QString& name, AxisState& state);
    void recordFrame(const QString& direction, const QByteArray& frame);
    void recordHistory();
    void runLoop();
    qint64 nowMs() const { return m_clock.elapsed(); }

    class Loop;

    mutable QMutex m_lock;
    Config m_config;
    QString m_callsign;
    QString m_locator;
    std::optional<Model> m_model;
    AxisState m_az;
    AxisState m_el;
    bool m_connected {false};
    QString m_error;
    int m_clients {0};
    QList<std::function<void()>> m_commands;
    QList<QVariantMap> m_history;
    QList<QVariantMap> m_traffic;
    qint64 m_txFrames {0};
    qint64 m_rxFrames {0};
    qint64 m_errors {0};
    qint64 m_reconnects {0};
    qint64 m_startedMs {0};
    QElapsedTimer m_clock;
    qint64 m_epochStartMs {0};

    std::unique_ptr<Link> m_link;       // vive solo nel thread del rotore
    QThread* m_thread {nullptr};
    std::atomic<bool> m_running {false};
};

}  // namespace decodium::rotor
