#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace decodium {
namespace foxbudget {

// Power budget of a multi-stream Fox.
//
// A transmitter limited in peak power splits its output between the streams
// of a frame: N equal streams lose 20*log10(N) dB each (measured 23/10/2026:
// 6.0 dB for 2, 9.5 dB for 3, 11.9 dB for 4 FT2 streams), because the peaks of
// the individual tones line up. So the sum of the stream AMPLITUDES is what is
// limited, not the sum of their powers.
//
// A strong station needs only a small share of the amplitude to be copied; a
// weak one needs a lot. Instead of a fixed number of equal streams, the Fox
// admits as many stations as fit in the budget, each with the amplitude its
// own link needs, and hands out the leftover in proportion.
//
// Pure logic, no Qt. Amplitudes are relative to a single stream at full scale
// (1.0). Link figures are the SNR a station would measure on a single
// full-scale stream, in dB, in the 2500 Hz reference bandwidth.

struct Config
{
    double thresholdDb {-13.5};       // 50% decode point of the mode
    double marginDb {3.0};            // margin on a measured link
    double marginUnmeasuredDb {4.0};  // margin when the link is only an estimate
    double minGain {0.06};            // never below -24 dB
    double unknownGain {0.5};         // nothing known about the link: half the budget, so two such stations fit in one frame
    int maxStreams {10};
    int agedAfterFrames {2};          // a station deferred this long goes first
};

struct Candidate
{
    Candidate(int id_ = 0, double linkDb_ = 0.0, bool measured_ = false, int waitFrames_ = 0, bool unknown_ = false)
        : id(id_), linkDb(linkDb_), measured(measured_), waitFrames(waitFrames_), unknown(unknown_) {}
    int id;
    double linkDb;                    // SNR on a single full-scale stream
    bool measured;                    // from the station's own report
    int waitFrames;                   // frames it has been left out of
    bool unknown;                     // no figure at all for its link (e.g. picked by a click)
};

struct Allocation
{
    std::vector<int> ids;             // chosen candidates, in the order given
    std::vector<double> gains;        // amplitudes, summing to 1
    std::vector<int> deferred;        // left for a later frame
};

inline double neededGain(Candidate const& c, Config const& cfg)
{
    if (c.unknown) {
        return cfg.unknownGain;
    }
    double const margin = c.measured ? cfg.marginDb : cfg.marginUnmeasuredDb;
    double const g = std::pow(10.0, (cfg.thresholdDb + margin - c.linkDb) / 20.0);
    return std::max(g, cfg.minGain);
}

// `candidates` arrive in priority order (closing a QSO before opening one).
inline Allocation allocate(std::vector<Candidate> candidates, Config const& cfg = {})
{
    // Stations left out for too long are served first, whatever their link.
    std::stable_sort(candidates.begin(), candidates.end(),
                     [&](Candidate const& a, Candidate const& b) {
                         return (a.waitFrames >= cfg.agedAfterFrames) > (b.waitFrames >= cfg.agedAfterFrames);
                     });

    Allocation out;
    std::vector<double> need;
    double total = 0.0;
    for (Candidate const& c : candidates) {
        double const g = neededGain(c, cfg);
        // A 5% overcommit costs each stream 0.4 dB at most (the gains are scaled to
        // sum to 1 below) and lets two stations that each need 0.501 share a frame.
        bool const fits = total + g <= 1.05;
        bool const room = static_cast<int>(out.ids.size()) < cfg.maxStreams;
        if (out.ids.empty() && g >= 1.0) {
            // A station that needs more than the whole budget gets the frame to itself.
            out.ids.push_back(c.id);
            need.push_back(1.0);
            total = 1.0;
            continue;
        }
        if (fits && room && !(out.ids.size() == 1 && need.front() >= 1.0)) {
            out.ids.push_back(c.id);
            need.push_back(g);
            total += g;
        } else {
            out.deferred.push_back(c.id);
        }
    }
    // Hand out what is left in proportion, so the sum is exactly 1.
    out.gains.reserve(need.size());
    for (double g : need) {
        out.gains.push_back(total > 0.0 ? g / total : 1.0);
    }
    return out;
}

} // namespace foxbudget
} // namespace decodium
