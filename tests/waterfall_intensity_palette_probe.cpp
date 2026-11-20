// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/ui/WaterfallIntensityPalette.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    QVector<QRgb> palette(256);
    for (int i = 0; i < 256; ++i)
        palette[i] = qRgb(i, 255 - i, (i * 73) % 256);
    QRgb const fallback = qRgb(255, 255, 255);
    for (int black = 0; black <= 100; ++black) {
        float const threshold = qBound(0.0f, black * 0.006f, 0.95f);
        float const range = qMax(0.001f, 1.0f - threshold);
        for (int gain = 0; gain <= 100; ++gain) {
            float gamma = 2.5f - gain * 0.02f;
            if (gamma < 0.3f) gamma = 0.3f;
            auto const table = waterfallIntensityPalette(threshold, range, gamma, palette, fallback);
            auto const continuousColor = [](float pct) {
                return qRgb(static_cast<int>(pct * 255.0f),
                            static_cast<int>(pct * pct * 255.0f),
                            static_cast<int>((1.0f - pct) * 255.0f));
            };
            auto const rowTable = waterfallRowIntensityPalette(threshold, range, gamma, fallback, continuousColor);
            for (int pixel = 0; pixel < 256; ++pixel) {
                float const adjusted = ((static_cast<float>(pixel) / 255.0f) - threshold) / range;
                QRgb expected = fallback;
                if (adjusted > 0.0f) {
                    float const pct = std::pow(qBound(0.0f, adjusted, 1.0f), gamma);
                    int const idx = qBound(0, static_cast<int>(pct * 255.0f + 0.5f), 255);
                    expected = palette.value(idx, fallback);
                }
                if (table[pixel] != expected) return 1;
                QRgb rowExpected = adjusted <= 0.0f ? fallback
                    : continuousColor(std::pow(qBound(0.0f, adjusted, 1.0f), gamma));
                if (rowTable[pixel] != rowExpected) return 2;
            }
        }
    }
    std::puts("5,222,912 intensity/color comparisons passed (history and live rows)");
    constexpr int pixels = 1200 * 600;
    QVector<uchar> image(pixels);
    for (int i = 0; i < pixels; ++i) image[i] = (i * 73 + i / 997) % 256;
    volatile quint64 checksum = 0;
    for (bool tableMode : {false, true, true, false}) {
        auto const start = std::chrono::steady_clock::now();
        quint64 sum = 0;
        for (int frame = 0; frame < 10; ++frame) {
            float const threshold = 0.3f, range = 0.7f, gamma = 1.5f;
            auto const table = waterfallIntensityPalette(threshold, range, gamma, palette, fallback);
            for (uchar pixel : image) {
                if (tableMode) {
                    sum += table[pixel];
                } else {
                    float const adjusted = ((static_cast<float>(pixel) / 255.0f) - threshold) / range;
                    if (adjusted <= 0.0f) sum += fallback;
                    else {
                        float const pct = std::pow(qBound(0.0f, adjusted, 1.0f), gamma);
                        int const idx = qBound(0, static_cast<int>(pct * 255.0f + 0.5f), 255);
                        sum += palette.value(idx, fallback);
                    }
                }
            }
        }
        checksum = sum;
        auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        std::printf("%s: %lld ms, checksum %llu\n", tableMode ? "table" : "original", static_cast<long long>(ms), static_cast<unsigned long long>(checksum));
    }
}
