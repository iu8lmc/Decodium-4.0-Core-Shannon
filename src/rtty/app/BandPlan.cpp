#include "app/BandPlan.h"

namespace decortty::app {

const std::vector<Band>& bandPlan()
{
    // I nomi delle bande non si traducono: "20 m" si dice cosi' in ogni lingua
    // in cui si fa radio, ed e' scritto cosi' sulla manopola di ogni apparato.
    static const std::vector<Band> plan = {
        { QStringLiteral("160 m"),  1.838,   1.810,   2.000 },
        { QStringLiteral("80 m"),   3.580,   3.500,   3.800 },
        { QStringLiteral("40 m"),   7.040,   7.000,   7.200 },
        { QStringLiteral("30 m"),  10.142,  10.100,  10.150 },
        { QStringLiteral("20 m"),  14.080,  14.000,  14.350 },
        { QStringLiteral("17 m"),  18.100,  18.068,  18.168 },
        { QStringLiteral("15 m"),  21.080,  21.000,  21.450 },
        { QStringLiteral("12 m"),  24.925,  24.890,  24.990 },
        { QStringLiteral("10 m"),  28.080,  28.000,  29.700 },
        { QStringLiteral("6 m"),   50.300,  50.000,  52.000 },
    };
    return plan;
}

int bandAt(double mhz)
{
    const auto& plan = bandPlan();
    for (int i = 0; i < static_cast<int>(plan.size()); ++i) {
        if (mhz >= plan[i].lowMhz && mhz <= plan[i].highMhz)
            return i;
    }
    return -1;
}

} // namespace decortty::app
