#include "../src/bridge/WaterfallRingPosition.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>

int main()
{
    constexpr int capacity = 16384;
    constexpr std::array<std::uint64_t, 7> starts {
        0, 1, capacity - 1, capacity, 2 * capacity - 1,
        static_cast<std::uint64_t>(std::numeric_limits<int>::max()) - 100,
        UINT64_C(12000) * 60 * 60 * 24 * 30
    };
    for (auto initial : starts) {
        std::array<short, capacity> actual {}, reference {};
        std::uint64_t total = initial;
        int position = initial < capacity ? static_cast<int>(initial)
            : capacity + static_cast<int>(initial % capacity);
        for (int batch = 0; batch < 400; ++batch) {
            int count = batch % 13 == 0 ? capacity : (batch * 137 + 1) % 1200;
            std::array<short, capacity> samples {};
            for (int i = 0; i < count; ++i) {
                const short sample = static_cast<short>((batch * 257 + i) % 32768);
                samples[i] = sample;
                reference[total++ % capacity] = sample;
            }
            decodium::appendWaterfallRing(actual.data(), capacity, position, samples.data(), count);
            if (actual != reference || position % capacity != int(total % capacity)
                || std::min(position, capacity) != int(std::min<std::uint64_t>(total, capacity))
                || position >= 2 * capacity) {
                std::cerr << "ring mismatch at initial=" << initial << " batch=" << batch << '\n';
                return 1;
            }
        }
    }
    std::cout << "waterfall ring wrap, availability and long-run continuity passed\n";
}
