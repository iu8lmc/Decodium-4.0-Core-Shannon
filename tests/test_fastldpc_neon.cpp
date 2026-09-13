#include "Detector/fastldpc/minsum_neon.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

namespace {

Code makeTestCode ()
{
    Code code;
    code.N = 32;
    code.M = 12;
    code.row_ptr.reserve (static_cast<std::size_t> (code.M) + 1);
    code.row_ptr.push_back (0);
    for (int row = 0; row < code.M; ++row) {
        int const degree = row % 3 == 2 ? 7 : 6;
        for (int edge = 0; edge < degree; ++edge)
            code.col_idx.push_back ((row * 5 + edge * 7) % code.N);
        code.row_ptr.push_back (static_cast<int> (code.col_idx.size ()));
    }
    return code;
}

bool compareOneTrial (Code const& code, std::mt19937& random, int trial)
{
    constexpr int batch = 16;
    constexpr int iterations = 30;
    std::uniform_real_distribution<float> llrDistribution (-18.0f, 18.0f);
    std::vector<float> llr (static_cast<std::size_t> (batch) * code.N);
    if (trial == 0)
        std::fill (llr.begin (), llr.end (), 8.0f); // parola nulla valida
    else
        for (float& value : llr) value = llrDistribution (random);

    MinSumV2 scalar (code, batch, iterations);
    MinSumV3 neon (code, batch, iterations);
    std::vector<std::uint8_t> scalarBits (llr.size ());
    std::vector<std::uint8_t> neonBits (llr.size ());
    std::vector<std::uint8_t> scalarOk (batch);
    std::vector<std::uint8_t> neonOk (batch);
    std::vector<int> scalarIterations (batch);
    std::vector<int> neonIterations (batch);

    scalar.decode (llr.data (), scalarBits.data (), scalarIterations.data (),
                   scalarOk.data ());
    neon.decode (llr.data (), neonBits.data (), neonIterations.data (),
                 neonOk.data ());

    if (scalarBits != neonBits || scalarOk != neonOk
        || scalarIterations != neonIterations) {
        std::cerr << "fastldpc NEON mismatch in trial " << trial
                  << " (bits/status/iterations)\n";
        return false;
    }
    if (trial == 0
        && !std::all_of (neonOk.begin (), neonOk.end (),
                         [] (std::uint8_t value) { return value != 0; })) {
        std::cerr << "fastldpc NEON did not close the known valid codeword\n";
        return false;
    }

    for (int word = 0; word < batch; ++word) {
        if (scalar.unsat (word) != neon.unsat (word)
            || !std::equal (scalar.posterior (word),
                            scalar.posterior (word) + code.N,
                            neon.posterior (word))) {
            std::cerr << "fastldpc NEON mismatch in trial " << trial
                      << ", word " << word << " (posterior/syndrome)\n";
            return false;
        }
    }
    return true;
}

} // namespace

int main ()
{
    Code const code = makeTestCode ();
    std::mt19937 random (0x4e454f4eu);
    for (int trial = 0; trial < 250; ++trial) {
        if (!compareOneTrial (code, random, trial)) return 1;
    }
    std::cout << "fastldpc NEON matches the scalar reference (250 trials)\n";
    return 0;
}
