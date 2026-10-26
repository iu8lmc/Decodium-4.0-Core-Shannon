#pragma once

#include <cstdint>

namespace decodium {

// One bounded rendezvous between the completed RX slot and automatic FT8 TX.
// Deadlines are absolute offsets in the TX slot, not delays from a timer tick.
// Keep manual TX, other modes, MAM and legacy-owned scheduling independent.
class Ft8AutoTxDecodeGate
{
public:
    static constexpr int periodMs = 15000;
    static constexpr int decodeDeadlineMs = 1200;
    static constexpr int commitDeadlineMs = 1400;
    static constexpr int audioDeadlineMs = 1800;
    static constexpr int tailMarginMs = 250;
    // A decoder that keeps missing decodeDeadlineMs (slow or stalled PC: 2.3-2.9 s
    // on 26/10/2026) can never be waited for: a full frame must start before
    // audioDeadlineMs. After this many consecutive late slots the gate stops
    // waiting and sends what the sequencer already holds. A closing message
    // (RR73/73) needs no reply, so one late slot is enough for it.
    static constexpr int lateStreakForFinalMessage = 1;
    static constexpr int lateStreakForAnyMessage = 2;

    struct Context {
        std::int64_t slot = -1;
        std::uint64_t session = 0;
        std::uint64_t generation = 0;

        bool operator==(const Context& other) const
        {
            return slot == other.slot && session == other.session
                && generation == other.generation;
        }
    };
    enum class Decision { Wait, Ready, SkipSlot };

    void reset() { *this = Ft8AutoTxDecodeGate {}; }

    bool matches(const Context& context) const { return m_context == context; }

    bool armTimer(const Context& context)
    {
        select(context);
        if (m_timerArmed) return false;
        m_timerArmed = true;
        return true;
    }

    void decoded(const Context& context, int elapsedMs)
    {
        select(context);
        if (elapsedMs >= 0 && elapsedMs < decodeDeadlineMs) {
            m_lateStreak = 0;
            m_lastLateSlot = -1;
        } else if (elapsedMs >= decodeDeadlineMs) {
            noteLate(context);
        }
        // A late callback may update the next QSO message, but cannot reopen
        // this slot or replace a payload that has already been committed.
        if (!m_committed && elapsedMs >= 0 && elapsedMs < decodeDeadlineMs)
            m_ready = true;
    }

    // The wait for the previous RX slot ran out: count it as a late slot.
    void deadlineExpired(const Context& context)
    {
        select(context);
        noteLate(context);
    }

    bool lateDecoderBypass(bool finalMessage) const
    {
        return m_lateStreak >= (finalMessage ? lateStreakForFinalMessage
                                             : lateStreakForAnyMessage);
    }

    int lateStreak() const { return m_lateStreak; }

    Decision decision(const Context& context, int elapsedMs, bool finalMessage = false) const
    {
        if (elapsedMs < 0 || elapsedMs >= commitDeadlineMs
            || (matches(context) && m_committed))
            return Decision::SkipSlot;
        if (matches(context) && m_ready) return Decision::Ready;
        if (lateDecoderBypass(finalMessage)) return Decision::Ready;
        return elapsedMs < decodeDeadlineMs ? Decision::Wait : Decision::SkipSlot;
    }

    void committed(const Context& context)
    {
        select(context);
        m_committed = true;
    }

    static bool fullFrameFits(int elapsedMs, std::int64_t payloadMs)
    {
        return elapsedMs >= 0 && elapsedMs <= audioDeadlineMs && payloadMs > 0
            && elapsedMs + payloadMs + tailMarginMs <= periodMs;
    }

private:
    void select(const Context& context)
    {
        if (matches(context)) return;
        int const streak = m_lateStreak;
        std::int64_t const lastLate = m_lastLateSlot;
        reset();
        m_context = context;
        m_lateStreak = streak;   // the decoder's speed outlives a single slot
        m_lastLateSlot = lastLate;
    }

    void noteLate(const Context& context)
    {
        if (m_lastLateSlot == context.slot) return;   // one miss per slot
        m_lastLateSlot = context.slot;
        if (m_lateStreak < 8) ++m_lateStreak;
    }

    Context m_context;
    int m_lateStreak = 0;
    std::int64_t m_lastLateSlot = -1;
    bool m_ready = false;
    bool m_timerArmed = false;
    bool m_committed = false;
};

} // namespace decodium
