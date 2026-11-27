#include "src/ui/PanadapterAutoRange.hpp"
#include <algorithm>
#include <cstdlib>
#include <cmath>

// Models the raw-dB shader with a fixed received signal and identical range.
static float displayed(float db, int black, float minDb, float span)
{
    const float threshold = black * 0.0022f;
    const float normal = std::clamp((db - minDb) / span, 0.0f, 1.0f);
    return std::pow(std::clamp((normal - threshold) / (1.0f - threshold), 0.0f, 1.0f), 1.5f);
}
int main()
{
    constexpr float floor = -90.0f, margin = 5.0f, span = 50.0f, signal = -80.0f;
    const float minDb = panadapterDirectAutoRangeMinDb(floor, margin, 0.0f);
    if (!(minDb == -95.0f)) return EXIT_FAILURE;
    const float at0 = displayed(signal, 0, minDb, span);
    const float at50 = displayed(signal, 50, minDb, span);
    const float at100 = displayed(signal, 100, minDb, span);
    if (!(at0 > at50 && at50 > at100)) return EXIT_FAILURE;
    // The old minimum shifted by Black, cancelling the slider and brightening
    // this representative signal. This is the negative control.
    const float old100 = displayed(signal, 100, minDb - 0.22f * span, span);
    if (!(old100 > at0 && old100 > at100)) return EXIT_FAILURE;
    if (!(panadapterDirectAutoRangeMinDb(floor, margin, 3.0f) == -92.0f)) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
