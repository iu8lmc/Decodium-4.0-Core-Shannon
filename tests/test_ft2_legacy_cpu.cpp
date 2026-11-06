// Exercise the real FT2 audio decoder and scalar LDPC with an Ivy Bridge
// CPUID snapshot, even on ARM CI. No decoder or DSP routines are stubbed.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QString>

#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"

extern "C" unsigned superldpc_test_x86_features_c () { return 1u | 8u | 16u; }
extern "C" int superldpc_is_available_c ();
extern "C" void superldpc_set_enabled_c (int);
extern "C" void ftx_ft2_stage7_set_cancel_c (int);
extern "C" void ftx_ft2_stage7_clravg_c ();
extern "C" void ftx_ft2_async_decode_stage7_c (
    short const*, int*, int*, int*, int*, int*, int*, char const*, char const*,
    int*, float*, float*, int*, float*, signed char*, char*, int*);

namespace {
using Clock = std::chrono::steady_clock;
constexpr int samplesPerFrame = 45000;
constexpr int offset = 9480;
constexpr char target[] = "CQ K1ABC FN42";

std::vector<short> makeFrame (float snrDb, unsigned seed, bool signal = true)
{
    auto const encoded = decodium::txmsg::encodeFt2 (QString::fromLatin1 (target));
    if (!encoded.ok) return {};
    auto const wave = decodium::txwave::generateFt2Wave (
        encoded.tones.constData (), encoded.tones.size (), 288, 12000.0f, 1500.0f);
    if (wave.isEmpty ()) return {};
    float const scale = signal ? std::sqrt (2.0f * 2500.0f / 6000.0f)
        * std::pow (10.0f, 0.05f * snrDb) : 0.0f;
    std::mt19937 rng (seed);
    std::normal_distribution<float> noise (0.0f, 1.0f);
    std::vector<short> pcm (samplesPerFrame);
    for (int i = 0; i < samplesPerFrame; ++i) {
        int const k = i - offset;
        float const s = k >= 0 && k < wave.size () ? wave[k] : 0.0f;
        pcm[i] = static_cast<short> (std::lround (
            std::clamp (100.0f * (scale * s + noise (rng)), -32767.0f, 32767.0f)));
    }
    return pcm;
}

std::vector<short> makeBusyFrame (bool corrupt = false)
{
    auto pcm = makeFrame (-6.0f, 0x2F2403u);
    if (pcm.empty ()) return {};
    int station = 2;
    for (float frequency : {400, 650, 900, 1150, 1750, 2000, 2250, 2500, 2750, 3000, 3250}) {
        auto encoded = decodium::txmsg::encodeFt2 (
            QStringLiteral ("CQ K%1ABC FN42").arg (station++));
        if (!encoded.ok) return {};
        if (corrupt) {
            // Strong synchronisation with random payload: expensive failed
            // candidates surrounding the valid station at the RX frequency.
            std::mt19937 rng (static_cast<unsigned> (frequency));
            for (int k = 0; k < encoded.tones.size (); ++k)
                if (k % 33 >= 4) encoded.tones[k] = static_cast<int> (rng () % 4);
        }
        auto const wave = decodium::txwave::generateFt2Wave (
            encoded.tones.constData (), encoded.tones.size (), 288, 12000.0f, frequency);
        float const scale = 100.0f * std::sqrt (2.0f * 2500.0f / 6000.0f)
            * std::pow (10.0f, -9.0f / 20.0f);
        for (int k = 0; k < wave.size () && k + offset < samplesPerFrame; ++k)
            pcm[k + offset] += static_cast<short> (std::lround (scale * wave[k]));
    }
    return pcm;
}

struct Result {
    int count = 0;
    bool recovered = false;
    long long elapsedMs = 0;
};

Result decode (std::vector<short> const& pcm, int depth)
{
    std::array<int, 100> snrs {}, naps {};
    std::array<float, 100> dts {}, freqs {}, quals {};
    std::array<signed char, 100 * 77> bits {};
    std::array<char, 100 * 37> messages {};
    int progress = 0, nfqso = 1500, nfa = 196, nfb = 3500, contest = 0;
    Result result;
    auto const started = Clock::now ();
    ftx_ft2_async_decode_stage7_c (
        pcm.data (), &progress, &nfqso, &nfa, &nfb, &depth, &contest,
        "K1ABC       ", "            ", snrs.data (), dts.data (), freqs.data (),
        naps.data (), quals.data (), bits.data (), messages.data (), &result.count);
    result.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds> (
        Clock::now () - started).count ();
    for (int i = 0; i < result.count && i < 100; ++i) {
        QString const text = QString::fromLatin1 (messages.data () + i * 37, 37).trimmed ();
        result.recovered |= text == QString::fromLatin1 (target);
        std::cout << "  " << text.toStdString () << '\n';
    }
    std::cout << "depth=" << depth << " decode_ms=" << result.elapsedMs
              << " nout=" << result.count << " recovered=" << result.recovered << std::endl;
    return result;
}
}

int main (int argc, char** argv)
{
    QCoreApplication app (argc, argv);
    if (superldpc_is_available_c ()) return 1;
    auto const strong = makeFrame (-10.0f, 0x2F2403u);
    auto const weak = makeFrame (-15.0f, 0x2F2403u);
    auto const noise = makeFrame (0.0f, 0x2F2403u, false);
    if (strong.empty () || weak.empty () || noise.empty ()) return 1;
    ftx_ft2_stage7_set_cancel_c (0);

    // Same non-AVX2 path with LDPC on/off, including the user's depth=20
    // and full 196..3500 Hz scan. Each invocation must have a fresh budget.
    for (int enabled : {1, 0}) {
        superldpc_set_enabled_c (enabled);
        for (int depth : {4, 20}) {
            ftx_ft2_stage7_clravg_c ();
            auto const result = decode (strong, depth);
            if (!result.recovered || result.elapsedMs > 3000) return 1;
        }
    }

    ftx_ft2_stage7_clravg_c ();
    auto const weakResult = decode (weak, 20);
    if (!weakResult.recovered || weakResult.elapsedMs > 3000) return 1;

    ftx_ft2_stage7_clravg_c ();
    auto const noiseResult = decode (noise, 20);
    if (noiseResult.count != 0 || noiseResult.elapsedMs > 3000) return 1;

    // A crowded band can exhaust the budget after recovering some stations.
    // Those valid rows must survive the deadline instead of becoming nout=0.
    auto const busy = makeBusyFrame ();
    if (busy.empty ()) return 1;
    ftx_ft2_stage7_clravg_c ();
    auto const busyResult = decode (busy, 20);
    if (!busyResult.recovered || busyResult.elapsedMs > 3000) return 1;

    auto const corrupt = makeBusyFrame (true);
    if (corrupt.empty ()) return 1;
    ftx_ft2_stage7_clravg_c ();
    auto const limited = decode (corrupt, 20);
    if (!limited.recovered || limited.elapsedMs > 3000) return 1;

    // Stop during real work, then decode again: cancellation and budget
    // state must not poison subsequent slots or leave a worker occupied.
    ftx_ft2_stage7_clravg_c ();
    std::thread cancel ([] {
        std::this_thread::sleep_for (std::chrono::milliseconds (20));
        ftx_ft2_stage7_set_cancel_c (1);
    });
    auto const cancelled = decode (noise, 20);
    cancel.join ();
    ftx_ft2_stage7_set_cancel_c (0);
    if (cancelled.count != 0 || cancelled.elapsedMs > 1000) return 1;
    auto const resumed = decode (strong, 20);
    if (!resumed.recovered || resumed.elapsedMs > 3000) return 1;
    return 0;
}
