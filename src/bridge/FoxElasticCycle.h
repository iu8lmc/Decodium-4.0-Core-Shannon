#pragma once

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace decodium {
namespace foxelastic {

// Elastic cycle of an FT2 Fox working several stations at once.
//
// On the slot grid a Fox transmits every second slot (7.5 s) even though a
// transmit frame lasts 2.5 s and the callers answer within about a second of
// hearing it. The Fox can instead start its next transmission as soon as every
// station it is waiting for has answered (or is clearly not going to), the way
// TCP derives its retransmission timeout from the measured round trip.
//
// Pure decision logic, no Qt and no clock: the caller passes millisecond
// timestamps from the same clock for every call. All times are in ms.
struct Config
{
    int minWindowMs {3000};      // never start sooner than this: a real answer is on the air until ~2.9-3.1 s after our TX ended
    int maxWindowMs {4700};      // never longer than the grid cycle: 2472 ms of TX + this + ~400 ms of PTT/audio latency ~ 7.5 s
    int initialReplyMs {3400};   // TX end -> decode of a reply, before any measurement
    int guardMs {100};           // margin added to the learned "everybody is done" delay
    int graceMs {300};           // margin added to a station's own timeout
    int memory {32};             // decode delays kept for the "newcomer" estimate
};

class Cycle
{
public:
    explicit Cycle(Config config = {}) : m_cfg(config) {}

    // Stations a directed message was addressed to in the frame being sent
    // (base callsigns). CQ streams are not listed: nobody is expected to answer.
    void setExpected(std::vector<std::string> calls)
    {
        m_expected.clear();
        for (auto& c : calls) {
            if (!c.empty()) {
                m_expected[std::move(c)] = false;
            }
        }
    }

    void txEnded(long long endMs)
    {
        m_endMs = endMs;
        m_open = true;
        m_seen.clear();
    }

    bool windowOpen() const { return m_open; }
    long long windowStartMs() const { return m_endMs; }

    // A decode arrived. `call` is the sender's base callsign. Only a reliable
    // decode counts, and each station counts once per window: the same frame is
    // decoded many times, and low-confidence a-priori decodes of an old message
    // show up early and would pull the learned delay down (seen on the air: it
    // fell to 2.0 s and the Fox keyed over the answers).
    void decodeHeard(const std::string& call, long long nowMs, bool reliable = true)
    {
        if (!reliable || !m_open || nowMs < m_endMs) {
            return;
        }
        if (!m_seen.insert(call).second) {
            return;
        }
        double const d = static_cast<double>(nowMs - m_endMs);
        m_samples.push_back(d);
        while (static_cast<int>(m_samples.size()) > m_cfg.memory) {
            m_samples.pop_front();
        }
        auto it = m_expected.find(call);
        if (it != m_expected.end() && !it->second) {
            it->second = true;
            if (d <= m_cfg.maxWindowMs) {
                learn(call, d);
            }
        }
    }

    // Earliest time the next transmission may start, 0 if no window is open.
    long long goAtMs() const
    {
        if (!m_open) {
            return 0;
        }
        long long const floorAt = m_endMs + std::max<long long>(m_cfg.minWindowMs, static_cast<long long>(newcomerDelayMs() + m_cfg.guardMs));
        long long resolvedAt = m_endMs;
        for (auto const& kv : m_expected) {
            if (kv.second) {
                // already answered: the moment it did is in the samples; the
                // caller polls often enough that "now" is when we noticed
                continue;
            }
            resolvedAt = std::max(resolvedAt, m_endMs + static_cast<long long>(deadlineDelayMs(kv.first)));
        }
        long long const cap = m_endMs + m_cfg.maxWindowMs;
        return std::min(cap, std::max(floorAt, resolvedAt));
    }

    bool shouldGo(long long nowMs) const
    {
        long long const at = goAtMs();
        return at > 0 && nowMs >= at;
    }

    int pendingCount() const
    {
        int n = 0;
        for (auto const& kv : m_expected) {
            n += kv.second ? 0 : 1;
        }
        return n;
    }

    void dispatched()
    {
        // A station that did not answer is worth less waiting next time.
        for (auto const& kv : m_expected) {
            auto it = m_rtt.find(kv.first);
            if (it != m_rtt.end()) {
                it->second.misses = kv.second ? 0 : it->second.misses + 1;
            } else if (!kv.second) {
                m_rtt[kv.first] = Rtt();
                m_rtt[kv.first].misses = 1;
            }
        }
        m_open = false;
        m_expected.clear();
    }

    // For the log: the delay after which a station is given up.
    double timeoutMs(std::string const& call) const { return deadlineDelayMs(call); }
    double newcomerDelayMs() const
    {
        if (static_cast<int>(m_samples.size()) < 5) {
            return m_cfg.initialReplyMs;
        }
        std::vector<double> v(m_samples.begin(), m_samples.end());
        std::sort(v.begin(), v.end());
        return v[static_cast<size_t>(0.9 * (v.size() - 1))];
    }

private:
    struct Rtt
    {
        Rtt() : srtt(0.0), var(0.0), misses(0) {}
        double srtt;
        double var;
        int misses;      // consecutive windows in which it did not answer
    };

    void learn(std::string const& call, double d)
    {
        auto it = m_rtt.find(call);
        if (it == m_rtt.end() || it->second.srtt <= 0.0) {
            // first sample: a quarter of it as variation (TCP uses half, which
            // makes the first timeout three times the round trip)
            Rtt r;
            r.srtt = d;
            r.var = d / 4.0;
            m_rtt[call] = r;
            return;
        }
        Rtt& r = it->second;
        r.misses = 0;
        r.var = 0.75 * r.var + 0.25 * std::fabs(r.srtt - d);
        r.srtt = 0.875 * r.srtt + 0.125 * d;
    }

    // How long after our TX end a station is still worth waiting for.
    double deadlineDelayMs(std::string const& call) const
    {
        auto it = m_rtt.find(call);
        double d = m_cfg.initialReplyMs + m_cfg.graceMs;
        if (it != m_rtt.end() && it->second.srtt > 0.0) {
            d = it->second.srtt + 4.0 * it->second.var + m_cfg.graceMs;
            // silent last time: do not hold the whole cycle for it again
            if (it->second.misses > 0) {
                d = std::min<double>(d, m_cfg.initialReplyMs + m_cfg.graceMs);
            }
        }
        return std::min<double>(d, m_cfg.maxWindowMs);
    }

    Config m_cfg;
    bool m_open {false};
    long long m_endMs {0};
    std::map<std::string, bool> m_expected;     // call -> already answered
    std::map<std::string, Rtt> m_rtt;
    std::deque<double> m_samples;
    std::set<std::string> m_seen;               // stations already counted in this window
};

} // namespace foxelastic
} // namespace decodium
