#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QString>
#include <functional>
#include <utility>

namespace decodium::audio {

// Event-loop controller independent of the UTC/decoder period generation.
// Hooks also allow regression testing without opening audio or keying a rig.
class RttyRxRecovery final : public QObject {
public:
    struct State {
        QString cancelReason;
        bool requested = true;
        bool monitoring = true;
        bool busy = false;
        qint64 pcmStamp = 0;
        QString details;
    };
    struct Hooks {
        std::function<State()> state;
        std::function<void()> rearmMonitor;
        std::function<void(bool)> recover; // false: initial; true: watchdog retry
        std::function<void(const QString&)> log;
    };
    struct Timing { int initial = 250; int retry = 500; int check = 3500; int deadline = 15000; };

    explicit RttyRxRecovery(Hooks hooks, QObject* parent = nullptr)
        : QObject(parent), hooks_(std::move(hooks)) {}

    void cancel(const QString& reason) {
        ++generation_;
        if (active_) hooks_.log("RTTY exit RX recovery: cancelled reason=" + reason);
        active_ = false;
    }
    void start() { start(Timing{}); }
    void start(Timing timing) {
        cancel(QStringLiteral("superseded by new recovery"));
        timing_ = timing;
        active_ = true;
        checking_ = false;
        monitorRearmed_ = false;
        attempts_ = 0;
        elapsed_.start();
        hooks_.log(QStringLiteral("RTTY exit RX recovery: scheduled"));
        later(timing_.initial);
    }

private:
    void later(int delay) {
        const auto generation = generation_;
        QTimer::singleShot(delay, this, [this, generation] {
            if (active_ && generation == generation_) step();
        });
    }
    void step() {
        const auto generation = generation_;
        const auto state = hooks_.state();
        if (!state.cancelReason.isEmpty() || !state.requested) {
            cancel(state.cancelReason.isEmpty() ? QStringLiteral("monitor stopped") : state.cancelReason);
            return;
        }
        if (state.busy || !state.monitoring) {
            const auto reason = state.busy ? QStringLiteral("TX/Tune active")
                                           : QStringLiteral("monitor hand-off pending");
            if (elapsed_.elapsed() >= timing_.deadline) {
                hooks_.log("RTTY exit RX check: unavailable reason=" + reason);
                cancel(QStringLiteral("recovery deadline exceeded"));
                return;
            }
            hooks_.log("RTTY exit RX recovery: deferred reason=" + reason);
            if (!state.busy && !monitorRearmed_) {
                monitorRearmed_ = true;
                hooks_.log(QStringLiteral("RTTY exit RX recovery: rearming requested monitor"));
                hooks_.rearmMonitor();
            }
            if (active_ && generation == generation_) later(timing_.retry);
            return;
        }
        if (checking_) {
            const bool pcm = state.pcmStamp > baseline_;
            hooks_.log(QStringLiteral("RTTY exit RX check: pcm=%1 attempt=%2 %3")
                           .arg(pcm ? 1 : 0).arg(attempts_).arg(state.details));
            if (pcm || attempts_ >= 2) {
                active_ = false;
                hooks_.log(pcm ? QStringLiteral("RTTY exit RX recovery: completed, fresh PCM received")
                               : QStringLiteral("RTTY exit RX recovery: exhausted, no fresh PCM"));
                return;
            }
        }
        baseline_ = state.pcmStamp;
        hooks_.log(QStringLiteral("RTTY exit RX recovery: executing attempt=%1").arg(attempts_ + 1));
        hooks_.recover(attempts_++ > 0);
        if (!active_ || generation != generation_) return;
        checking_ = true;
        later(timing_.check);
    }
    Hooks hooks_;
    Timing timing_;
    QElapsedTimer elapsed_;
    quint64 generation_ = 0;
    qint64 baseline_ = 0;
    bool active_ = false;
    bool checking_ = false;
    bool monitorRearmed_ = false;
    int attempts_ = 0;
};
}
