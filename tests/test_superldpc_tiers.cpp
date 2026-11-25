// test_superldpc_tiers.cpp — the x86 superldpc backends must agree.
//
// The same decoder source is compiled twice (decodium_bridge.cpp with
// AVX2/FMA, decodium_bridge_sse2.cpp for the x86-64 baseline). This test feeds
// identical noisy codewords to both,
// in FT8 and FT2 mode, single-word and batch, with and without a priori masks,
// plus the BICM-ID extrinsic output, and requires identical results. It also
// times each backend and the original generic decoder (ftx_decode174_91_c).
//
// Every backend is called directly, so the CPU running the test needs AVX2 and
// FMA. On an older CPU "max=sse2" runs only the SSE2 backend (no comparison,
// but the decoder still has to work there).
//
// Arguments: words=N (default 3000), seed=N, max=avx2|sse2, bench=1, scale=F.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#define TIER_DECLS(prefix)                                                              \
    void prefix##set_ft8_mode_c (int);                                                  \
    void prefix##set_ft2_max_hard_c (int);                                              \
    void prefix##decode174_91_c (float const*, int, int, int, signed char const*,      \
                                 signed char*, signed char*, int*, int*, float*);       \
    void prefix##decode174_91_batch_c (int, float const*, signed char const*, int, int,\
                                       int, signed char*, signed char*, int*, int*,    \
                                       float*);                                         \
    int prefix##extrinsic174_91_c (float const*, int, float, float*);

extern "C" {
TIER_DECLS (superldpc_simd_)
TIER_DECLS (superldpc_sse2_)
void ftx_decode174_91_c (float const*, int, int, int, signed char const*, signed char*,
                         signed char*, int*, int*, float*);
void encode174_91_nocrc_ (signed char* message_in, signed char* codeword_out);
void get_crc14_ (signed char* mc, int* len, int* ncrc);
void ftx_ft2_rvec_c (signed char* out77);

// Symbols the decoders expect from the rest of the program, when the test is
// built standalone (the CMake target links the real ones from wsjt_qt).
#if !defined(SUPERLDPC_TIERS_LINK_APP)
void ftx_ft2_rvec_c (signed char* out77)
{
    static int const raw_rvec[77] = {
        0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
        1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
        0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
    };
    for (int i = 0; i < 77; ++i) out77[i] = static_cast<signed char> (raw_rvec[i]);
}

void ftx_ldpc174_91_metrics_c (signed char const* cw, float const* llr,
                               int* nharderrors_out, float* dmin_out)
{
    int nhard = 0;
    float dmin = 0.0f;
    for (int i = 0; i < 174; ++i) {
        int const bit = cw[i] != 0 ? 1 : 0;
        if (static_cast<float> (2 * bit - 1) * llr[i] < 0.0f) ++nhard;
        if (((llr[i] >= 0.0f ? 1 : 0) ^ bit) != 0) dmin += std::fabs (llr[i]);
    }
    if (nharderrors_out) *nharderrors_out = nhard;
    if (dmin_out) *dmin_out = dmin;
}

int ftx_ldpc174_91_finalize_c (signed char const* cw, float const* llr,
                               signed char* message91_out, int* nharderrors_out,
                               float* dmin_out)
{
    if (message91_out) std::copy_n (cw, 91, message91_out);
    ftx_ldpc174_91_metrics_c (cw, llr, nharderrors_out, dmin_out);
    signed char m96[96] {};
    std::copy_n (cw, 77, m96);
    std::copy_n (cw + 77, 14, m96 + 82);
    int len = 96, ncrc = 0;
    get_crc14_ (m96, &len, &ncrc);
    return ncrc == 0 ? 1 : 0;
}
#endif
}

namespace {

constexpr int kN = 174;
float g_scale = 0.35f;

struct Tier {
    char const* name;
    void (*set_ft8) (int);
    void (*set_cap) (int);
    void (*decode) (float const*, int, int, int, signed char const*, signed char*,
                    signed char*, int*, int*, float*);
    void (*batch) (int, float const*, signed char const*, int, int, int, signed char*,
                   signed char*, int*, int*, float*);
    int (*extrinsic) (float const*, int, float, float*);
};

#define TIER(prefix, label) Tier { label, prefix##set_ft8_mode_c, prefix##set_ft2_max_hard_c, \
    prefix##decode174_91_c, prefix##decode174_91_batch_c, prefix##extrinsic174_91_c }

struct Result {
    std::vector<signed char> msg, cw;
    std::vector<int> ntype, nhard;
    std::vector<float> dmin;
    void resize (int n) {
        msg.assign (static_cast<size_t> (n) * 91, 0);
        cw.assign (static_cast<size_t> (n) * kN, 0);
        ntype.assign (n, -9); nhard.assign (n, -9); dmin.assign (n, -9.0f);
    }
};

// A standard (i3 = 1) message the decoder's plausibility filter accepts:
// CQ or a callsign, then a callsign, report bits and a grid field. Random
// payloads are almost always implausible and would never exercise the OSD
// acceptance path, the gate or the a priori checks.
void plausible_payload (std::mt19937& rng, signed char out77[77])
{
    std::uniform_int_distribution<int> any (0, 1 << 30);
    auto put = [&] (int from, int nbits, std::uint32_t v) {
        for (int i = 0; i < nbits; ++i) out77[from + i] = static_cast<signed char> ((v >> (nbits - 1 - i)) & 1);
    };
    auto call28 = [&] () -> std::uint32_t {
        std::uint32_t const i0 = 11 + any (rng) % 26;            // letter
        std::uint32_t const i1 = any (rng) % 36;                 // letter or digit
        std::uint32_t const i2 = any (rng) % 10;                 // digit
        std::uint32_t const i3 = 1 + any (rng) % 26;             // letter
        std::uint32_t const i4 = any (rng) % 27;                 // letter or space
        std::uint32_t const i5 = i4 == 0 ? 0 : any (rng) % 27;   // no hole in the suffix
        std::uint32_t const n = ((((i0 * 36 + i1) * 10 + i2) * 27 + i3) * 27 + i4) * 27 + i5;
        return n + 2063592u + 4194304u;
    };
    put (0, 28, (any (rng) % 4 == 0) ? 2u : call28 ());          // 2 = "CQ"
    put (28, 1, 0);
    put (29, 28, call28 ());
    put (57, 1, 0);
    put (58, 1, static_cast<std::uint32_t> (any (rng) & 1));
    put (59, 15, static_cast<std::uint32_t> (any (rng) % 32400));
    put (74, 3, 1u);                                             // i3 = 1
}

std::vector<float> make_llrs (int words, unsigned seed, bool ft2, std::vector<signed char>& truth)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> noise (0.0f, 1.0f);
    std::vector<float> llr (static_cast<size_t> (words) * kN);
    truth.assign (static_cast<size_t> (words) * kN, 0);
    signed char rvec[77] {};
    ftx_ft2_rvec_c (rvec);
    for (int w = 0; w < words; ++w) {
        signed char m96[96] {};
        plausible_payload (rng, m96);
        if (ft2)                                                 // FT2 sends the payload scrambled
            for (int i = 0; i < 77; ++i) m96[i] = static_cast<signed char> (m96[i] ^ rvec[i]);
        int len = 96, ncrc = 0;
        get_crc14_ (m96, &len, &ncrc);
        signed char m91[91] {}, cw[kN] {};
        std::copy_n (m96, 77, m91);
        for (int i = 0; i < 14; ++i) m91[77 + i] = static_cast<signed char> ((ncrc >> (13 - i)) & 1);
        encode174_91_nocrc_ (m91, cw);
        // Eb/N0 spread over the waterfall region: easy, BP-limited and OSD-limited words.
        float const ebn0_db = 0.5f + 3.5f * static_cast<float> (w % 8) / 7.0f;
        float const rate = 91.0f / 174.0f;
        float const sigma = std::sqrt (1.0f / (2.0f * rate * std::pow (10.0f, ebn0_db / 10.0f)));
        for (int i = 0; i < kN; ++i) {
            float const s = cw[i] ? 1.0f : -1.0f;      // Decodium: positive LLR = bit 1
            float const y = s + sigma * noise (rng);
            llr[static_cast<size_t> (w) * kN + i] = 2.0f * y / (sigma * sigma) * g_scale;
            truth[static_cast<size_t> (w) * kN + i] = cw[i];
        }
    }
    return llr;
}

std::vector<signed char> make_apmask (int words, unsigned seed)
{
    std::mt19937 rng (seed ^ 0x5a5a5a5au);
    std::vector<signed char> ap (static_cast<size_t> (words) * kN, 0);
    for (int w = 0; w < words; ++w)
        if (w % 3 == 1)                                   // a priori on the first 29 bits
            for (int i = 0; i < 29; ++i) ap[static_cast<size_t> (w) * kN + i] = 1;
    (void) rng;
    return ap;
}

bool same (Result const& a, Result const& b, char const* what, char const* na, char const* nb)
{
    size_t const n = a.ntype.size ();
    for (size_t w = 0; w < n; ++w) {
        bool ok = a.ntype[w] == b.ntype[w] && a.nhard[w] == b.nhard[w]
               && std::memcmp (&a.dmin[w], &b.dmin[w], sizeof (float)) == 0
               && std::memcmp (&a.msg[w * 91], &b.msg[w * 91], 91) == 0
               && std::memcmp (&a.cw[w * kN], &b.cw[w * kN], kN) == 0;
        if (!ok) {
            std::fprintf (stderr, "MISMATCH %s word %zu: %s ntype=%d nhard=%d dmin=%.6f | "
                          "%s ntype=%d nhard=%d dmin=%.6f\n", what, w, na, a.ntype[w],
                          a.nhard[w], a.dmin[w], nb, b.ntype[w], b.nhard[w], b.dmin[w]);
            return false;
        }
    }
    return true;
}

double now_ms ()
{
    return std::chrono::duration<double, std::milli> (
               std::chrono::steady_clock::now ().time_since_epoch ()).count ();
}

} // namespace

int main (int argc, char** argv)
{
    int words = 3000;
    unsigned seed = 20261125u;
    std::string max = "avx2";
    bool bench = false;
    for (int i = 1; i < argc; ++i) {
        std::string const a = argv[i];
        if (a.rfind ("words=", 0) == 0) words = std::max (16, std::atoi (a.c_str () + 6));
        else if (a.rfind ("seed=", 0) == 0) seed = static_cast<unsigned> (std::strtoul (a.c_str () + 5, nullptr, 10));
        else if (a.rfind ("max=", 0) == 0) max = a.substr (4);
        else if (a == "bench=1") bench = true;
        else if (a.rfind ("scale=", 0) == 0) g_scale = std::strtof (a.c_str () + 6, nullptr);
    }

#if defined(__GNUC__)
    // The AVX2 backend may only be called on a CPU that has AVX2 and FMA.
    __builtin_cpu_init ();
    if (max == "avx2" && !(__builtin_cpu_supports ("avx2") && __builtin_cpu_supports ("fma"))) {
        std::printf ("CPU without AVX2/FMA: running the SSE2 backend only\n");
        max = "sse2";
    }
#endif
    std::vector<Tier> tiers;
    if (max == "avx2") tiers.push_back (TIER (superldpc_simd_, "avx2"));
    tiers.push_back (TIER (superldpc_sse2_, "sse2"));

    std::vector<signed char> truth8, truth2;
    std::vector<float> const llr8 = make_llrs (words, seed, false, truth8);
    std::vector<float> const llr2 = make_llrs (words, seed + 1, true, truth2);
    std::vector<signed char> const ap0 (static_cast<size_t> (words) * kN, 0);
    std::vector<signed char> const ap = make_apmask (words, seed);

    struct Case { char const* name; int ft8; int cap; int maxosd; int norder; bool apmask; bool batch; };
    Case const cases[] = {
        {"ft8-batch",        1, 0,  2, 3, false, true},
        {"ft8-batch-ap",     1, 0,  2, 3, true,  true},
        {"ft8-single-ord2",  1, 0,  2, 2, false, false},
        {"ft8-noosd",        1, 0, -1, 3, false, true},
        {"ft2-batch",        0, 0,  3, 3, false, true},
        {"ft2-batch-cap",    0, 30, 3, 3, true,  true},
        {"ft2-single",       0, 0,  3, 3, false, false},
    };

    int failures = 0;
    for (Case const& c : cases) {
        std::vector<float> const& llr = c.ft8 ? llr8 : llr2;
        std::vector<signed char> const& truth = c.ft8 ? truth8 : truth2;
        std::vector<Result> res (tiers.size ());
        std::vector<double> ms (tiers.size (), 0.0);
        for (size_t t = 0; t < tiers.size (); ++t) {
            Tier const& tier = tiers[t];
            Result& r = res[t];
            r.resize (words);
            tier.set_ft8 (c.ft8);
            tier.set_cap (c.cap);
            signed char const* apm = (c.apmask ? ap : ap0).data ();
            double const t0 = now_ms ();
            if (c.batch) {
                for (int w0 = 0; w0 < words; w0 += 64) {
                    int const n = std::min (64, words - w0);
                    tier.batch (n, &llr[static_cast<size_t> (w0) * kN], apm + static_cast<size_t> (w0) * kN,
                                91, c.maxosd, c.norder, &r.msg[static_cast<size_t> (w0) * 91],
                                &r.cw[static_cast<size_t> (w0) * kN], &r.ntype[w0], &r.nhard[w0], &r.dmin[w0]);
                }
            } else {
                for (int w = 0; w < words; ++w)
                    tier.decode (&llr[static_cast<size_t> (w) * kN], 91, c.maxosd, c.norder,
                                 apm + static_cast<size_t> (w) * kN, &r.msg[static_cast<size_t> (w) * 91],
                                 &r.cw[static_cast<size_t> (w) * kN], &r.ntype[w], &r.nhard[w], &r.dmin[w]);
            }
            ms[t] = now_ms () - t0;
        }
        int ok_words = 0, correct = 0;
        for (int w = 0; w < words; ++w) {
            if (res[0].ntype[w] > 0) {
                ++ok_words;
                if (std::memcmp (&res[0].cw[static_cast<size_t> (w) * kN], &truth[static_cast<size_t> (w) * kN], kN) == 0)
                    ++correct;
            }
        }
        bool all_same = true;
        for (size_t t = 1; t < tiers.size (); ++t)
            all_same = same (res[0], res[t], c.name, tiers[0].name, tiers[t].name) && all_same;
        if (!all_same) ++failures;
        std::printf ("%-16s %s decoded=%d/%d correct=%d", c.name, all_same ? "SAME" : "DIFF",
                     ok_words, words, correct);
        if (bench)
            for (size_t t = 0; t < tiers.size (); ++t)
                std::printf ("  %s=%.1fus/word", tiers[t].name, 1000.0 * ms[t] / words);
        std::printf ("\n");
    }

    // BICM-ID extrinsic output: same 174 floats from every tier.
    {
        int mismatches = 0;
        for (int w = 0; w < std::min (words, 400); ++w) {
            std::array<float, kN> ref {};
            int ref_ret = 0;
            for (size_t t = 0; t < tiers.size (); ++t) {
                std::array<float, kN> ext {};
                tiers[t].set_ft8 (1);
                int const ret = tiers[t].extrinsic (&llr8[static_cast<size_t> (w) * kN], 3, 8.0f, ext.data ());
                if (t == 0) { ref = ext; ref_ret = ret; }
                else if (ret != ref_ret
                         || !std::equal (ext.begin (), ext.end (), ref.begin (),
                                         [] (float a, float b) { return a == b; })) {
                    // Compared as numbers: +0 and -0 count as equal. They are
                    // added to channel LLRs, where the sign of a zero has no
                    // effect, and the AVX2/FMA build can produce either.
                    ++mismatches;
                    float maxd = 0.0f; int at = -1;
                    for (int i = 0; i < kN; ++i)
                        if (std::fabs (ext[i] - ref[i]) > maxd) { maxd = std::fabs (ext[i] - ref[i]); at = i; }
                    std::fprintf (stderr, "extrinsic word %d: %s vs %s ret %d/%d max|diff|=%g at bit %d (%g vs %g)\n",
                                  w, tiers[0].name, tiers[t].name, ref_ret, ret, maxd, at,
                                  at >= 0 ? ref[at] : 0.0f, at >= 0 ? ext[at] : 0.0f);
                }
            }
        }
        std::printf ("%-16s %s\n", "ft8-extrinsic", mismatches == 0 ? "SAME" : "DIFF");
        if (mismatches) { ++failures; std::fprintf (stderr, "extrinsic mismatches: %d\n", mismatches); }
    }

    if (bench) {
        // Original generic decoder on the FT2 words (the "compatibility" path uses maxosd 0).
        std::vector<signed char> msg (91), cw (kN);
        int ntype = 0, nhard = 0, ok = 0, good = 0;
        float dmin = 0.0f;
        for (int maxosd : {0, 3}) {
            ok = 0; good = 0;
            double const t0 = now_ms ();
            for (int w = 0; w < words; ++w) {
                ftx_decode174_91_c (&llr2[static_cast<size_t> (w) * kN], 91, maxosd, 3,
                                    &ap0[static_cast<size_t> (w) * kN], msg.data (), cw.data (),
                                    &ntype, &nhard, &dmin);
                if (ntype > 0) {
                    ++ok;
                    if (std::memcmp (cw.data (), &truth2[static_cast<size_t> (w) * kN], kN) == 0) ++good;
                }
            }
            std::printf ("generic maxosd=%d decoded=%d/%d correct=%d  %.1fus/word\n", maxosd, ok,
                         words, good, 1000.0 * (now_ms () - t0) / words);
        }
    }

    std::printf (failures ? "FAIL: %d case(s) differ between tiers\n" : "PASS: all tiers identical\n",
                 failures);
    return failures ? 1 : 0;
}
