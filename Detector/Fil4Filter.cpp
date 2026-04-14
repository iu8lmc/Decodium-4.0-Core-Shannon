#include "Fil4Filter.hpp"
#include <cmath>

void fil4_cpp (int16_t const* id1, int32_t n1, int16_t* id2, int32_t& n2)
{
    static constexpr int NTAPS = 49;
    static constexpr int NDOWN = 4;

    // Delay line — retained between calls (Fortran SAVE equivalent)
    static float t[NTAPS] = {};

    static constexpr float w[NTAPS] = {
        0.000861074040f, 0.010051920210f, 0.010161983649f, 0.011363155076f,
        0.008706594219f, 0.002613872664f,-0.005202883094f,-0.011720748164f,
       -0.013752163325f,-0.009431602741f, 0.000539063909f, 0.012636767098f,
        0.021494659597f, 0.021951235065f, 0.011564169382f,-0.007656470131f,
       -0.028965787341f,-0.042637874109f,-0.039203309748f,-0.013153301537f,
        0.034320769178f, 0.094717832646f, 0.154224604789f, 0.197758325022f,
        0.213715139513f, 0.197758325022f, 0.154224604789f, 0.094717832646f,
        0.034320769178f,-0.013153301537f,-0.039203309748f,-0.042637874109f,
       -0.028965787341f,-0.007656470131f, 0.011564169382f, 0.021951235065f,
        0.021494659597f, 0.012636767098f, 0.000539063909f,-0.009431602741f,
       -0.013752163325f,-0.011720748164f,-0.005202883094f, 0.002613872664f,
        0.008706594219f, 0.011363155076f, 0.010161983649f, 0.010051920210f,
        0.000861074040f
    };

    n2 = n1 / NDOWN;
    int k = -NDOWN;
    for (int i = 0; i < n2; ++i) {
        k += NDOWN;
        // Shift delay line: t[0..NTAPS-NDOWN-1] = t[NDOWN..NTAPS-1]
        for (int j = 0; j < NTAPS - NDOWN; ++j)
            t[j] = t[j + NDOWN];
        // Insert NDOWN new input samples at the end of the delay line
        for (int j = 0; j < NDOWN; ++j)
            t[NTAPS - NDOWN + j] = static_cast<float>(id1[k + j]);
        // Compute output sample via dot product
        float sum = 0.0f;
        for (int j = 0; j < NTAPS; ++j)
            sum += w[j] * t[j];
        id2[i] = static_cast<int16_t>(std::lroundf(sum));
    }
}
