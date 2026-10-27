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
    // A decoder that hovers around the deadline (1.1-1.7 s, IT9MRM 27/10/2026) loses
    // every other cycle to a few milliseconds. When 2 of the last 6 results were late
    // the gate waits longer for it, and past that point sends instead of skipping.
    static constexpr int slowWindow = 6;
    static constexpr int slowLateSamples = 2;
    static constexpr int slowDecodeDeadlineMs = 1600;
    static constexpr int slowCommitDeadlineMs = 1800;

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

    // True when the RX result for this slot arrived in time and was applied.
    bool resultReady(const Context& context) const { return matches(context) && m_ready; }

    bool slowDecoder() const
    {
        int late = 0;
        for (int i = 0; i < m_sampleCount; ++i)
            if (m_samples[i] >= decodeDeadlineMs) ++late;
        return late >= slowLateSamples;
    }

    int decodeDeadline() const { return slowDecoder() ? slowDecodeDeadlineMs : decodeDeadlineMs; }
    int commitDeadline() const { return slowDecoder() ? slowCommitDeadlineMs : commitDeadlineMs; }

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
        int const deadline = decodeDeadline();   // judged before this sample moves it
        if (elapsedMs >= 0 && elapsedMs < deadline) {
            m_lateStreak = 0;
            m_lastLateSlot = -1;
        } else if (elapsedMs >= deadline) {
            noteLate(context);
        }
        if (elapsedMs >= 0) addSample(context, elapsedMs);
        // A late callback may update the next QSO message, but cannot reopen
        // this slot or replace a payload that has already been committed.
        if (!m_committed && elapsedMs >= 0 && elapsedMs < deadline)
            m_ready = true;
    }

    // The wait for the previous RX slot ran out: count it as a late slot.
    void deadlineExpired(const Context& context)
    {
        select(context);
        noteLate(context);
        addSample(context, decodeDeadlineMs);
    }

    bool lateDecoderBypass(bool finalMessage) const
    {
        return m_lateStreak >= (finalMessage ? lateStreakForFinalMessage
                                             : lateStreakForAnyMessage);
    }

    int lateStreak() const { return m_lateStreak; }

    Decision decision(const Context& context, int elapsedMs, bool finalMessage = false) const
    {
        if (elapsedMs < 0 || elapsedMs >= commitDeadline()
            || (matches(context) && m_committed))
            return Decision::SkipSlot;
        if (matches(context) && m_ready) return Decision::Ready;
        if (lateDecoderBypass(finalMessage)) return Decision::Ready;
        if (elapsedMs < decodeDeadline()) return Decision::Wait;
        // Past the (extended) deadline: a slow decoder is not going to make it, and
        // skipping the slot only loses a whole cycle.
        return slowDecoder() ? Decision::Ready : Decision::SkipSlot;
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
    // What the gate has learned about the decoder's speed outlives a single slot.
    struct Speed {
        int lateStreak = 0;
        std::int64_t lastLateSlot = -1;
        int samples[slowWindow] = {};
        int sampleCount = 0;
        int samplePos = 0;
        std::int64_t lastSampleSlot = -1;
    };

    void select(const Context& context)
    {
        if (matches(context)) return;
        Speed keep;
        keep.lateStreak = m_lateStreak;
        keep.lastLateSlot = m_lastLateSlot;
        for (int i = 0; i < slowWindow; ++i) keep.samples[i] = m_samples[i];
        keep.sampleCount = m_sampleCount;
        keep.samplePos = m_samplePos;
        keep.lastSampleSlot = m_lastSampleSlot;
        reset();
        m_context = context;
        m_lateStreak = keep.lateStreak;
        m_lastLateSlot = keep.lastLateSlot;
        for (int i = 0; i < slowWindow; ++i) m_samples[i] = keep.samples[i];
        m_sampleCount = keep.sampleCount;
        m_samplePos = keep.samplePos;
        m_lastSampleSlot = keep.lastSampleSlot;
    }

    void noteLate(const Context& context)
    {
        if (m_lastLateSlot == context.slot) return;   // one miss per slot
        m_lastLateSlot = context.slot;
        if (m_lateStreak < 8) ++m_lateStreak;
    }

    void addSample(const Context& context, int elapsedMs)
    {
        if (m_lastSampleSlot == context.slot) return;   // one sample per slot
        m_lastSampleSlot = context.slot;
        m_samples[m_samplePos] = elapsedMs;
        m_samplePos = (m_samplePos + 1) % slowWindow;
        if (m_sampleCount < slowWindow) ++m_sampleCount;
    }

    Context m_context;
    int m_lateStreak = 0;
    std::int64_t m_lastLateSlot = -1;
    int m_samples[slowWindow] = {};
    int m_sampleCount = 0;
    int m_samplePos = 0;
    std::int64_t m_lastSampleSlot = -1;
    bool m_ready = false;
    bool m_timerArmed = false;
    bool m_committed = false;
};

} // namespace decodium
