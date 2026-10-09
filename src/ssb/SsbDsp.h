#pragma once
#include <algorithm>
#include <cmath>
#include <array>
namespace decodium::ssb {
// 48 kHz speech processing. Four low-pass sections also suppress aliasing
// before decimation to the shared 12 kHz radio transport.
class SpeechDsp {
public:
    double gainDb = 0, lowHz = 200, highHz = 2800;
    bool automatic = true;
    void reset() { previous = hp = envelope = 0; low.fill(0); autoGain = 1; }
    float process(float input) {
        if (!std::isfinite(input)) input = 0;
        constexpr double pi = 3.14159265358979323846;
        const double highPass = std::exp(-2*pi*std::clamp(lowHz,80.,500.)/48000);
        const double lowPass = 1-std::exp(-2*pi*std::clamp(highHz,1800.,3500.)/48000);
        double x = highPass*(hp + input - previous);
        previous=input; hp=x;
        for(auto& z:low) { z += lowPass*(x-z); x=z; }
        x *= std::pow(10.,std::clamp(gainDb,-20.,30.)/20.);
        envelope += (std::abs(x)>envelope ? .02 : .0002)*(std::abs(x)-envelope);
        if(automatic) {
            // No noise pumping: below -40 dBFS gain cannot increase.
            double target = envelope>.01 ? std::clamp(.25/envelope,.1,4.) : 1.;
            autoGain += (target<autoGain ? .02 : .00005)*(target-autoGain);
            x *= autoGain;
        }
        return float(std::clamp(x,-.95,.95));
    }
private:
    double previous=0,hp=0,envelope=0,autoGain=1;
    std::array<double,4> low{};
};
}
