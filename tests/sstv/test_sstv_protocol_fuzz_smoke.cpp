// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size);

namespace {

std::uint64_t nextValue(std::uint64_t& state) noexcept
{
    state ^= state << 13U;
    state ^= state >> 7U;
    state ^= state << 17U;
    return state;
}

bool runOne(const std::uint8_t* data, std::size_t size)
{
    return LLVMFuzzerTestOneInput(data, size) == 0;
}

} // namespace

int main()
{
    static constexpr std::array<std::uint8_t, 16U> edgeValues {{
        0U, 1U, 2U, 3U, 0x20U, 0x2aU, 0x3fU, 0x40U,
        0x7fU, 0x80U, 0xfeU, 0xffU, 0x15U, 0x2dU, 90U, 200U,
    }};
    if (!runOne(nullptr, 0U)
        || !runOne(edgeValues.data(), edgeValues.size())) {
        std::cerr << "SSTV protocol hostile-input smoke failed on edge seed\n";
        return 1;
    }

    constexpr std::size_t maximumBytes = 4U * 1024U;
    constexpr std::size_t iterations = 2'048U;
    std::uint64_t state = 0x5a17c0de'20260824ULL;
    std::vector<std::uint8_t> input(maximumBytes);
    for (std::size_t iteration = 0U; iteration < iterations; ++iteration) {
        const std::size_t size = iteration < edgeValues.size()
            ? iteration
            : static_cast<std::size_t>(nextValue(state))
                % (maximumBytes + 1U);
        for (std::size_t index = 0U; index < size; ++index) {
            input[index] = static_cast<std::uint8_t>(nextValue(state));
        }
        if (!runOne(input.data(), size)) {
            std::cerr << "SSTV protocol hostile-input smoke failed at iteration "
                      << iteration << '\n';
            return 1;
        }
    }

    std::cout << "SSTV VIS/N-VIS/FSK-ID hostile-input smoke passed: "
              << (iterations + 2U) << " deterministic cases\n";
    return 0;
}
