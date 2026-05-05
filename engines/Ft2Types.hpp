#pragma once

// Ft2Types.hpp — foundational types for Ft2QsoEngine.
//
// This module replaces the FT2 sequencer pipeline with a single-pipeline
// state machine (std::variant) that mirrors the proven Decodium 3.0 behavior
// while using modern C++17. See plan: lazy-forging-wall.md (Ft2QsoEngine).
//
// Design constraints:
//  - Header-only types; no Qt dependency beyond QString for interop with Bridge
//  - POD-like state structs visited via std::visit
//  - All time accounting via std::chrono::steady_clock (monotonic, mockable)
//  - Bounded containers (caller queue cap = 50) — no unbounded growth in contest

#include <QString>
#include <QStringList>
#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

namespace decodium::ft2 {

// ============================================================================
// Time
// ============================================================================
using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration  = Clock::duration;

inline TimePoint now() noexcept { return Clock::now(); }

// ============================================================================
// Slot parity (RX/TX timing). FT2 is async (rolling decode) so Async is the
// default; Even/Odd retained for completeness when a sync overlay is desired.
// ============================================================================
enum class Slot : std::uint8_t { Async, Even, Odd };

// ============================================================================
// Decoded row (output of DecodiumBridge::parseFt8Row, normalized)
// ============================================================================
struct DecodeRow {
    QString    timeToken;     // "" | "HHMM" | "HHMMSS"  (raw from parser)
    int        snr      {0};  // dB
    double     dt       {0.0};// seconds
    int        df       {0};  // audio Hz - lower band edge
    int        audioHz  {0};  // absolute audio Hz
    QString    message;       // 37-char trimmed message body
    QString    aptype;        // "" or "1".."9"
    bool       lowConfidence {false}; // trailing '?' mark in payload

    // True if the decoded message is directed AT us (mention of myCall).
    // Set by the engine after parsing; not derived from raw fields.
    bool       directedToMe {false};

    // Convenience: is the decode marked as a CQ message?
    bool       isCq {false};

    // Partner inferred from the message tokens (FROM call), upper-case base.
    QString    fromCall;
    // Optional grid extracted from the trailing token.
    QString    grid;
    // Optional report extracted ("-NN", "+NN", or "RNN" after stripping R).
    QString    report;
    // Whether the message was an R+report variant (TX3/TX4 marker).
    bool       hasRogerPrefix {false};

    TimePoint  receivedAt {};  // engine-local arrival timestamp
};

// ============================================================================
// Dedup key — stable across both legacy and async pipelines so that the same
// physical decode is recognized as one event regardless of the source path.
//
// We deliberately do NOT include the time token: in 1.0.71 the legacy mirror
// produced HHMM tokens while the async path produced HHMMSS, and the resulting
// key mismatch caused autoSequenceStep to fire twice. Using {fromCall,
// msgFingerprint, freqBucket} keeps both paths aligned.
// ============================================================================
struct DecodeKey {
    QString  fromCall;       // upper-case base call ("" if not extractable)
    quint64  msgFingerprint {0}; // 64-bit hash of normalized message body
    int      freqBucket {0}; // audioHz / 20  (20Hz quantization)

    bool operator==(DecodeKey const& other) const noexcept {
        return msgFingerprint == other.msgFingerprint
            && freqBucket     == other.freqBucket
            && fromCall       == other.fromCall;
    }
};

struct DecodeKeyHash {
    std::size_t operator()(DecodeKey const& k) const noexcept {
        // Mix the three components — qHash on QString gives a stable hash.
        std::size_t h = std::hash<quint64>{}(k.msgFingerprint);
        h ^= std::hash<int>{}(k.freqBucket)              + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h ^= std::hash<std::string>{}(k.fromCall.toStdString()) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

// ============================================================================
// Caller queue entry. When in CQ mode, every directed answer to my CQ is
// queued; the engine drains the queue one QSO at a time, preserving SNR order
// (best signal first).
// ============================================================================
struct CallerEntry {
    QString    call;          // upper-case base call
    QString    grid;          // 4-char locator if known
    int        snr {-30};
    double     dt  {0.0};
    int        audioHz {0};
    TimePoint  enqueuedAt {};
    // True when this entry was learned from a decode directed AT us (the
    // station explicitly called myCall). Such "direct callers" outrank
    // generic CQ entries: they have already targeted us once, so they're
    // the most likely to convert if we engage them next.
    bool       directReply {false};

    // Used by stable sort: directReply > !directReply, then best SNR first,
    // then FIFO on tie.
    bool betterThan(CallerEntry const& other) const noexcept {
        if (directReply != other.directReply) return directReply;
        if (snr != other.snr) return snr > other.snr;
        return enqueuedAt < other.enqueuedAt;
    }
};

constexpr std::size_t kCallerQueueCapacity = 50;

// ============================================================================
// Cooldown map: callsign → "do not re-engage before this time". Mirrors the
// Decodium 3.0 30s post-73 cooldown that prevents a stale 73 decode from
// re-triggering a new QSO with the same station within seconds.
// ============================================================================
using CooldownMap = std::unordered_map<QString, TimePoint>;

constexpr Duration kCooldown73    = std::chrono::seconds(30);
constexpr Duration kFreshnessWindow = std::chrono::seconds(20);
constexpr Duration kWatchdogPerStateMax = std::chrono::seconds(60); // ~16 slots
// ReplyingTx1 gets a longer budget: in 1.0.72 live ops we observed the
// watchdog firing 1s before legitimate partner replies (e.g. F1PBZ at 61s),
// because slow operators can take 15+ slots to acknowledge a call.
constexpr Duration kWatchdogReplyingTx1 = std::chrono::seconds(120);
constexpr Duration kEngineTickInterval  = std::chrono::milliseconds(100);
// Closing drain — the engine waits in Closing until either:
//   (a) the backend signals "TX finished" (transmittingChanged true→false)
//       AFTER the engine has seen "TX started" inside Closing, OR
//   (b) this safety fallback elapses, in case the backend never emits the
//       transition (e.g. legacy mirror disabled, MAC bridge audio path).
// 7000ms = ~1.86 slots: enough headroom for entry-time variance against
// slot snapshots, but still under 2 full slots (which would let the backend
// retransmit RR73 a second time). The TX-end signal short-circuits this in
// the common case so most QSOs leave Closing in 4-5s on air.
constexpr Duration kCloseDrainAfter = std::chrono::milliseconds(7000);

// ============================================================================
// TX Sync policy: FT2 async = emit immediately on decode-driven advance.
//
// History: 1.0.86 shipped an Adaptive TX Sync (ATS) predictor that buffered
// partner-arrival samples and deferred requestTx() to land in their predicted
// RX window. Field log analysis (1.0.86 session, 21:43-21:51) showed the
// predictor saturating its 500ms cap on every firing — the model assumes a
// fixed slot period, but FT2 has none, so the prediction was structurally
// invalid. Removed in 1.0.87. Decodium 3.0 has run on immediate-emit for
// years on real on-air QSO traffic; that is now the canonical async policy.
// ============================================================================

// ============================================================================
// State machine — std::variant of plain structs. Each state carries the
// minimal context needed for transitions out of that state.
// ============================================================================
namespace state {

struct Idle {
    // No active QSO. Engine consumes decodes only to populate the caller
    // queue (if in CQ mode) and to update the band activity feed.
};

struct CallingCq {
    TimePoint  startedAt {};
    int        cqRepetitionCount {0};   // TX6 transmissions sent so far
};

struct ReplyingTx1 {
    QString    partnerCall;     // who we are calling
    QString    partnerGrid;     // their grid (if known from CQ)
    TimePoint  enteredAt {};
    int        retries {0};     // TX1 sent count without acknowledgment
};

struct AwaitingReport {
    // We have received a grid acknowledgement; we are now sending TX2 (report)
    // and awaiting their TX3 (R+report).
    QString    partnerCall;
    QString    partnerGrid;
    QString    sentReport;      // e.g. "-12"
    TimePoint  enteredAt {};
    int        retries {0};
};

struct AwaitingRRR {
    // We have sent TX3 (R+report). Awaiting their TX4 (RR73 / RRR).
    QString    partnerCall;
    QString    sentReport;
    QString    receivedReport;
    TimePoint  enteredAt {};
    int        retries {0};
};

struct AwaitingSignoff {
    // We have sent TX4 (RR73). Awaiting their TX5 (73) or another RR73.
    QString    partnerCall;
    TimePoint  enteredAt {};
    int        retries {0};
};

struct Closing {
    // QSO complete (we sent or received 73). Will log + transition to Idle on
    // next tick. Distinct state so logging is observable in the diagnostic
    // stream.
    QString    partnerCall;
    QString    partnerGrid;
    QString    sentReport;
    QString    receivedReport;
    TimePoint  closedAt {};
    bool       loggedQso {false};
};

} // namespace state

using StateVariant = std::variant<
    state::Idle,
    state::CallingCq,
    state::ReplyingTx1,
    state::AwaitingReport,
    state::AwaitingRRR,
    state::AwaitingSignoff,
    state::Closing
>;

// String label for the active variant alternative — used by diagnostic logs.
inline char const* stateName(StateVariant const& s) noexcept {
    return std::visit([](auto const& v) -> char const* {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, state::Idle>)            return "Idle";
        else if constexpr (std::is_same_v<T, state::CallingCq>)  return "CallingCq";
        else if constexpr (std::is_same_v<T, state::ReplyingTx1>)return "ReplyingTx1";
        else if constexpr (std::is_same_v<T, state::AwaitingReport>) return "AwaitingReport";
        else if constexpr (std::is_same_v<T, state::AwaitingRRR>) return "AwaitingRRR";
        else if constexpr (std::is_same_v<T, state::AwaitingSignoff>) return "AwaitingSignoff";
        else if constexpr (std::is_same_v<T, state::Closing>)    return "Closing";
        else                                                     return "?";
    }, s);
}

// ============================================================================
// Engine configuration injected at construction. Keeps the engine decoupled
// from QSettings/registry — the Bridge owns persistence and pushes a snapshot.
// ============================================================================
struct EngineConfig {
    QString  myCall;             // upper-case full call (e.g. "IK8TWX")
    QString  myBaseCall;         // upper-case base (suffix-stripped)
    QString  myGrid;             // 4-char locator
    bool     multiAnswerMode {true};   // user pref: queue all answers in CQ
    bool     quickQsoEnabled {false};  // FT2 Ultra2 fast exchange
    bool     contestExchange {false};  // R+grid contest mode
    Slot     localSlot { Slot::Async };
};

// ============================================================================
// Outcome of feeding a single decode row — summary surfaced to the Bridge for
// signal emission (UI updates) without exposing the engine's internal state.
// ============================================================================
struct FeedOutcome {
    bool        accepted {false};   // false = duplicate / stale / invalid
    bool        triggeredAdvance {false};
    int         newCurrentTx {0};   // 0 = no change, else 1..6
    QString     newDxCall;          // empty unless changed
    QString     diagnostic;         // human-readable trace line
};

} // namespace decodium::ft2
