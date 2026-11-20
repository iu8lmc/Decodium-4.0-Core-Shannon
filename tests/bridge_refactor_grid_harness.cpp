#include <QtCore>
#include <cmath>
#include <cstdio>
#include "helper_functions.h"
class DecodiumBridge { public:
    double calcDistance(const QString&, const QString&) const;
    double calcBearing(const QString&, const QString&) const;
    double latFromGrid(const QString&) const;
    double lonFromGrid(const QString&) const;
};
#include "bridge_refactor_under_test.inc"
static void check(bool ok, const char* name) { if (!ok) { fprintf(stderr, "FAIL %s\n", name); exit(1); } puts(name); }
int main() {
    DecodiumBridge bridge;
    check(bridge.calcDistance("invalid", "JM75") == -1.0, "invalid-origin");
    check(bridge.calcDistance("JM75", "invalid") == -1.0, "invalid-destination");
    check(bridge.calcBearing("invalid", "JM75") == -1.0, "bearing-invalid-origin");
    check(bridge.calcBearing("JM75", "invalid") == -1.0, "bearing-invalid-destination");
    check(bridge.latFromGrid("invalid") == 0.0 && bridge.lonFromGrid("invalid") == 0.0, "invalid-coordinates");
    check(bridge.calcDistance("JM75", "JM75") < 0.001, "same-grid");
    check(bridge.latFromGrid("JM75") > 35 && bridge.latFromGrid("JM75") < 37, "grid-latitude");
    check(bridge.lonFromGrid("JM75") > 14 && bridge.lonFromGrid("JM75") < 16, "grid-longitude");
    double ab = bridge.calcDistance("JM75", "JN45"), ba = bridge.calcDistance("JN45", "JM75");
    check(ab > 900 && ab < 1600 && std::abs(ab-ba) < 0.001, "distance-symmetry");
    double bearing = bridge.calcBearing("JM75", "JN45");
    check(bearing >= 0 && bearing < 360, "bearing-range");
    check(std::abs(bridge.calcDistance("JM75", "JN45") - bridge.calcDistance("jm75", "jn45")) < 0.001, "lowercase");
}
