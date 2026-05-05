#pragma once

// Ft2QsoEngine.hpp — single-pipeline FT2 QSO sequencer for Decodium 4.0.
//
// Replaces the dual (async + legacy mirror) pipeline used in 1.0.71 which
// caused TX duplication, missing DX call display and TX1->TX3 skips. Models
// the proven Decodium 3.0 auto_sequence behavior using a std::variant state
// machine + bounded caller queue + per-callsign cooldown map.
//
// Threading: all mutating operations run on the engine's owning thread (the
// Bridge's main thread). Workers post events via Qt::QueuedConnection.
//
// Lifecycle: Bridge owns one std::unique_ptr<Ft2QsoEngine> created when
// setMode("FT2") is called and destroyed when leaving FT2 mode. Outside FT2
// the legacy mirror pipeline remains active for FT8/FT4.

#include "Ft2Types.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QTimer>
#include <deque>
#include <memory>

class QTimer;

namespace decodium::ft2 {

class Ft2QsoEngine : public QObject {
    Q_OBJECT
public:
    explicit Ft2QsoEngine(EngineConfig const& cfg, QObject* parent = nullptr);
    ~Ft2QsoEngine() override;

    // ------------------------------------------------------------------
    // Configuration — pushed by the Bridge whenever user settings change.
    // ------------------------------------------------------------------
    void setConfig(EngineConfig const& cfg);
    EngineConfig const& config() const noexcept { return m_cfg; }

    // ------------------------------------------------------------------
    // Inputs
    // ------------------------------------------------------------------

    // Feed a batch of raw decode rows (output of the async FT2 decoder,
    // already in the 8-field format produced by parseFt8Row). The engine
    // performs deduplication, semantic filtering and may transition state.
    FeedOutcome feed(QStringList const& rawRows);

    // Convenience overload accepting pre-parsed rows (used by tests).
    FeedOutcome feedParsed(std::vector<DecodeRow> const& rows);

    // Periodic tick (100 ms). Drives the watchdog, drains the caller queue
    // when in CallingCq, and triggers Closing -> Idle log/cleanup.
    void tick();

    // UI: user double-clicked a decode line. The engine treats this as an
    // explicit "engage this station" command — bypasses the queue and
    // jumps to ReplyingTx1.
    void doubleClick(QString const& fromCall,
                     QString const& grid,
                     int            audioHz,
                     int            snr,
                     int            requestedTx = 0);

    // UI: TX Enable button. When false the engine stops requesting TX
    // messages but keeps consuming decodes for caller-queue maintenance.
    void enableTx(bool on);
    bool isTxEnabled() const noexcept { return m_txEnabled; }

    // UI: user explicitly halted the current QSO ("Stop"). Forces Idle.
    void abortQso(QString const& reason = {});

    // UI: caller-queue interactive controls (Ft2HudPanel).
    // promoteCaller: engage the named caller NOW, bypassing best-SNR order.
    //   If currently mid-QSO, aborts it first. Implies enableTx(true).
    // skipCaller:    remove from the queue and short-cooldown to prevent
    //                immediate re-enqueue from a follow-up CQ decode.
    // cancelPendingTx: emit txCancelRequested so the Bridge drops the next
    //                  outbound TX; engine state stays as-is.
    void promoteCaller(QString const& call);
    void skipCaller(QString const& call);
    void cancelPendingTx();

    // ------------------------------------------------------------------
    // Introspection — read-only views for diagnostics and tests.
    // ------------------------------------------------------------------
    StateVariant const& state() const noexcept { return m_state; }
    char const* stateLabel() const noexcept { return stateName(m_state); }
    int currentTx() const noexcept { return m_currentTx; }
    QString dxCall() const;

    std::deque<CallerEntry> callerQueueSnapshot() const { return m_callerQueue; }
    std::size_t callerQueueSize() const noexcept { return m_callerQueue.size(); }

    // ------------------------------------------------------------------
    // TX latency telemetry — microseconds elapsed between the entry of
    // dispatchDecodeToState() (i.e. a decode that the engine accepts and
    // routes) and the matching emit txMessageRequested(). In FT2 the
    // total slot is 3.75s; sustained latency above ~200ms means the
    // first TX of the slot will be cut short. Useful for diagnosing
    // contention with the decoder thread or QML rendering hiccups.
    // ------------------------------------------------------------------
    qint64 lastTxLatencyUs() const noexcept { return m_lastTxLatencyUs; }
    qint64 maxTxLatencyUs()  const noexcept { return m_maxTxLatencyUs; }
    qint64 avgTxLatencyUs()  const noexcept { return m_avgTxLatencyUs; }
    void   resetTxLatencyStats();

    // ------------------------------------------------------------------
    // Test hooks — inject a synthetic clock (production uses Clock::now).
    // ------------------------------------------------------------------
    void setClockOverride(std::function<TimePoint()> fn) { m_clockOverride = std::move(fn); }
    TimePoint clockNow() const { return m_clockOverride ? m_clockOverride() : Clock::now(); }

signals:
    // The engine wants the Bridge to set the active TX message. txNum is
    // 1..6 (WSJT-X TX number); msg is the formatted 13-char message body.
    void txMessageRequested(int txNum, QString msg);

    // The engaged DX callsign changed (empty when Idle).
    void dxCallChanged(QString call);

    // The active TX number changed (1..6, or 0 when Idle).
    void currentTxChanged(int txNum);

    // Human-readable diagnostic line — Bridge forwards to the QML log pane.
    void engineDiagnostic(QString line);

    // QSO completed and ready to be persisted in the log book.
    void qsoLogged(QString call,
                   QString grid,
                   QString sentReport,
                   QString receivedReport,
                   QString modeTag);

    // Caller queue contents changed — UI updates the "next callers" pane.
    void callerQueueChanged();

    // User asked to cancel the next outbound TX (Ft2HudPanel cancel button).
    // Bridge slot drops the queued txMessageRequested before it goes on air.
    void txCancelRequested();

    // Emitted whenever a decode-driven dispatch produces a TX request — the
    // microsecond delta and the resulting TX number are reported so the HUD
    // can render a live latency badge.
    void txLatencyMeasured(qint64 latencyUs, int txNum);

private:
    // ------------------------------------------------------------------
    // Decoding helpers
    // ------------------------------------------------------------------
    std::optional<DecodeRow> parseRawRow(QString const& raw) const;
    void enrichDecodeMeta(DecodeRow& row) const;
    DecodeKey makeKey(DecodeRow const& row) const;
    bool isDuplicate(DecodeKey const& key);
    void pruneStaleDedup();

    // ------------------------------------------------------------------
    // Caller queue management
    // ------------------------------------------------------------------
    void enqueueCaller(DecodeRow const& row, bool directReply = false);
    std::optional<CallerEntry> popBestCaller();
    void purgeCaller(QString const& call);

    // ------------------------------------------------------------------
    // Cooldown
    // ------------------------------------------------------------------
    void markCooldown(QString const& call);
    bool inCooldown(QString const& call) const;

    // ------------------------------------------------------------------
    // State transitions — each returns true if the transition fired.
    // ------------------------------------------------------------------
    bool dispatchDecodeToState(DecodeRow const& row);
    void transitionTo(StateVariant next, QString const& reason);
    void requestTx(int txNum, QString const& formatted);

    // Build TX1..TX6 strings from current state and partner info.
    QString buildTxMessage(int txNum,
                           QString const& partnerCall,
                           QString const& partnerGrid,
                           QString const& sentReport,
                           QString const& recvReport,
                           bool rrr) const;

    // Watchdog: returns true if the current state has exceeded its budget.
    bool watchdogExpired() const;

    // ------------------------------------------------------------------
    // Members
    // ------------------------------------------------------------------
    EngineConfig            m_cfg;
    StateVariant            m_state { state::Idle{} };
    int                     m_currentTx {0};       // 0 = none, 1..6 = WSJT TX
    bool                    m_txEnabled {false};

    std::deque<CallerEntry> m_callerQueue;
    CooldownMap             m_cooldown;

    // Dedup cache: key -> first-seen time. Pruned by pruneStaleDedup() on
    // every feed() call. Caps growth at the freshness window.
    std::unordered_map<DecodeKey, TimePoint, DecodeKeyHash> m_dedup;

    QTimer*                 m_tickTimer {nullptr};
    std::function<TimePoint()> m_clockOverride;

    // Monotonic event id — every feed() call increments to support replay
    // and idempotency assertions in tests.
    std::uint64_t           m_eventCounter {0};

    // TX latency profiling. m_currentDispatchStart is set right before
    // dispatchDecodeToState() runs and cleared right after; requestTx()
    // samples the elapsed delta only while it's valid, which excludes
    // tick()-driven CQ rotations and watchdog fallbacks (those have no
    // upstream "decode arrived" event to measure against).
    TimePoint m_currentDispatchStart {};
    qint64    m_lastTxLatencyUs {0};
    qint64    m_maxTxLatencyUs  {0};
    qint64    m_avgTxLatencyUs  {0};   // simple EMA, alpha = 1/8
    void      updateLatencyStats(qint64 us, int txNum);
};

} // namespace decodium::ft2
