// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QFile>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#ifndef DECODIUM_SSTV_INCOMING_MEDIA_FUZZ_CORPUS_DIR
#  error "DECODIUM_SSTV_INCOMING_MEDIA_FUZZ_CORPUS_DIR is required"
#endif

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size);

namespace {

constexpr std::size_t kMaximumFuzzInputBytes = 1024U * 1024U;

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

QByteArray imageSeed(const QString& name)
{
    QFile file(QStringLiteral(DECODIUM_SSTV_INCOMING_MEDIA_FUZZ_CORPUS_DIR)
               + QLatin1Char('/') + name);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray encoded = file.readAll().trimmed();
    if (!encoded.startsWith("hex:")) {
        return {};
    }
    return QByteArray::fromHex(encoded.mid(4));
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application);
    const QByteArray png = imageSeed(QStringLiteral("valid_png.hex"));
    const QByteArray jpeg = imageSeed(QStringLiteral("valid_jpeg.hex"));
    if (png.isEmpty() || jpeg.isEmpty() || !runOne(nullptr, 0U)
        || !runOne(reinterpret_cast<const std::uint8_t*>(png.constData()),
                   static_cast<std::size_t>(png.size()))
        || !runOne(reinterpret_cast<const std::uint8_t*>(jpeg.constData()),
                   static_cast<std::size_t>(jpeg.size()))) {
        std::cerr << "SSTV incoming-media hostile-input smoke failed on seed\n";
        return 1;
    }

    constexpr std::size_t iterations = 256U;
    const std::size_t seedSize = static_cast<std::size_t>(png.size());
    std::uint64_t state = 0x8b61d3fe'20260824ULL;
    std::vector<std::uint8_t> input(kMaximumFuzzInputBytes);
    for (std::size_t iteration = 0U; iteration < iterations; ++iteration) {
        const std::size_t size = iteration < seedSize
            ? iteration
            : static_cast<std::size_t>(nextValue(state))
                % (kMaximumFuzzInputBytes + 1U);
        for (std::size_t index = 0U; index < size; ++index) {
            input[index] = static_cast<std::uint8_t>(nextValue(state));
        }
        if (!runOne(input.data(), size)) {
            std::cerr << "SSTV incoming-media hostile-input smoke failed at "
                      << iteration << '\n';
            return 1;
        }
    }

    std::cout << "SSTV incoming-media hostile-input smoke passed: "
              << (iterations + 3U) << " deterministic cases\n";
    return 0;
}
