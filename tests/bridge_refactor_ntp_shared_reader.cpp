#include "PrecisionTime.hpp"
#include <cstdio>
void testNtpOffsetWrite(double value);
int main() {
    testNtpOffsetWrite(42.5);
    if (globalNtpOffsetMs() != 43) return 1;
    testNtpOffsetWrite(-21.1);
    if (globalNtpOffsetMs() != -21) return 2;
    std::puts("global NTP offset shared across translation units");
}
