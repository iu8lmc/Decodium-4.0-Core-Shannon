// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QVector>
#include <QtGlobal>
#include <array>
#include <cmath>

template <typename ColorFunction>
inline std::array<QRgb, 256> waterfallRowIntensityPalette(float blackThresh,
                                                          float usableRange,
                                                          float gamma,
                                                          QRgb fallback,
                                                          ColorFunction color)
{
    std::array<QRgb, 256> colors;
    for (int i = 0; i < 256; ++i) {
        float const rawNorm = static_cast<float>(i) / 255.0f;
        float const adjusted = (rawNorm - blackThresh) / usableRange;
        colors[i] = adjusted <= 0.0f
            ? fallback : color(std::pow(qBound(0.0f, adjusted, 1.0f), gamma));
    }
    return colors;
}

inline std::array<QRgb, 256> waterfallIntensityPalette(float blackThresh,
                                                       float usableRange,
                                                       float gamma,
                                                       const QVector<QRgb>& palette,
                                                       QRgb fallback)
{
    std::array<QRgb, 256> colors;
    for (int i = 0; i < 256; ++i) {
        float const adjusted = ((static_cast<float>(i) / 255.0f) - blackThresh) / usableRange;
        if (adjusted <= 0.0f) {
            colors[i] = fallback;
            continue;
        }
        float const pct = std::pow(qBound(0.0f, adjusted, 1.0f), gamma);
        int const idx = qBound(0, static_cast<int>(pct * 255.0f + 0.5f), 255);
        colors[i] = palette.value(idx, fallback);
    }
    return colors;
}
