// Command line front end of the Fox power budget, for the lab benches.
//   fox_power_budget_cli THRESHOLD_DB MAX_STREAMS LINK_DB [LINK_DB ...]
// prints one line per chosen station: "index gain" (0-based position in the
// arguments), then "deferred index ..." for the others.
#include <cstdio>
#include <cstdlib>

#include "src/bridge/FoxPowerBudget.h"

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s THRESHOLD_DB MAX_STREAMS LINK_DB...\n", argv[0]);
        return 2;
    }
    decodium::foxbudget::Config cfg;
    cfg.thresholdDb = std::atof(argv[1]);
    cfg.maxStreams = std::atoi(argv[2]);
    std::vector<decodium::foxbudget::Candidate> c;
    for (int i = 3; i < argc; ++i) {
        c.push_back({i - 3, std::atof(argv[i]), true, 0});
    }
    auto const a = decodium::foxbudget::allocate(c, cfg);
    for (size_t i = 0; i < a.ids.size(); ++i) {
        std::printf("%d %.6f\n", a.ids[i], a.gains[i]);
    }
    for (int d : a.deferred) {
        std::printf("deferred %d\n", d);
    }
    return 0;
}
