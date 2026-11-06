// SPDX-License-Identifier: GPL-3.0-or-later
#include "HamDrmRsCodec.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace decodium::sstv::hamdrm {
namespace {

constexpr std::size_t blockSize = 255U;
constexpr std::size_t maximumRepairWork = 80'000'000U;

std::size_t dataSize(unsigned level)
{
    constexpr std::array<std::size_t, 4> sizes {224U, 192U, 160U, 128U};
    return level >= 1U && level <= sizes.size() ? sizes[level - 1U] : 0U;
}

bool validExtension(const std::string& extension)
{
    return extension == "jpg" || extension == "jp2" || extension == "png"
        || extension == "gif" || extension == "bmp";
}

struct Field final {
    std::array<std::uint8_t, 510> powers {};
    std::array<unsigned, 256> logarithms {};
    Field()
    {
        unsigned value = 1U;
        for (unsigned i = 0; i < 255U; ++i) {
            powers[i] = static_cast<std::uint8_t>(value);
            logarithms[value] = i;
            value <<= 1U;
            if (value & 0x100U) value ^= 0x11dU;
        }
        for (unsigned i = 255U; i < powers.size(); ++i) powers[i] = powers[i - 255U];
    }
    std::uint8_t multiply(std::uint8_t a, std::uint8_t b) const
    {
        return a == 0U || b == 0U ? 0U : powers[logarithms[a] + logarithms[b]];
    }
    std::uint8_t inverse(std::uint8_t a) const { return powers[255U - logarithms[a]]; }
};

const Field& field() { static const Field instance; return instance; }

std::vector<std::uint8_t> generator(std::size_t parity)
{
    std::vector<std::uint8_t> polynomial {1U};
    const auto& gf = field();
    for (std::size_t root = 1U; root <= parity; ++root) {
        std::vector<std::uint8_t> next(polynomial.size() + 1U, 0U);
        for (std::size_t i = 0; i < polynomial.size(); ++i) {
            next[i] ^= polynomial[i];
            next[i + 1U] ^= gf.multiply(polynomial[i], gf.powers[root]);
        }
        polynomial = std::move(next);
    }
    return polynomial;
}

void addParity(std::array<std::uint8_t, blockSize>& block, std::size_t dataBytes,
               const std::vector<std::uint8_t>& polynomial)
{
    std::array<std::uint8_t, blockSize> remainder {};
    std::reverse_copy(block.begin(), block.begin() + static_cast<std::ptrdiff_t>(dataBytes), remainder.begin());
    const auto& gf = field();
    for (std::size_t i = 0; i < dataBytes; ++i) {
        const auto coefficient = remainder[i];
        if (!coefficient) continue;
        for (std::size_t j = 1; j < polynomial.size(); ++j)
            remainder[i + j] ^= gf.multiply(coefficient, polynomial[j]);
    }
    std::reverse_copy(remainder.begin() + static_cast<std::ptrdiff_t>(dataBytes),
                      remainder.end(), block.begin() + static_cast<std::ptrdiff_t>(dataBytes));
}

std::uint8_t syndrome(const std::array<std::uint8_t, blockSize>& block, std::size_t root)
{
    const auto& gf = field();
    std::uint8_t value = 0U;
    for (auto it = block.rbegin(); it != block.rend(); ++it)
        value = gf.multiply(value, gf.powers[root]) ^ *it;
    return value;
}

bool repair(std::array<std::uint8_t, blockSize>& block,
            const std::vector<std::size_t>& missing, std::size_t parity, std::size_t& work)
{
    if (missing.size() > parity) return false;
    const auto& gf = field();
    // Remove known erasures, then use their locator to remove their influence
    // from the syndrome sequence before locating unknown symbol errors.
    for (const auto position : missing) block[position] = 0U;
    std::vector<std::uint8_t> syndromes(parity);
    for (std::size_t i = 0; i < parity; ++i) syndromes[i] = syndrome(block, i + 1U);
    std::vector<std::uint8_t> erasureLocator {1U};
    for (const auto position : missing) {
        std::vector<std::uint8_t> next(erasureLocator.size() + 1U, 0U);
        for (std::size_t i = 0; i < erasureLocator.size(); ++i) {
            next[i] ^= erasureLocator[i];
            next[i + 1U] ^= gf.multiply(erasureLocator[i], gf.powers[position]);
        }
        erasureLocator = std::move(next);
    }
    std::vector<std::uint8_t> reduced;
    for (std::size_t i = missing.size(); i < parity; ++i) {
        std::uint8_t value = 0U;
        for (std::size_t j = 0; j < erasureLocator.size(); ++j)
            value ^= gf.multiply(erasureLocator[j], syndromes[i - j]);
        reduced.push_back(value);
    }
    // Berlekamp-Massey, with coefficients in ascending power order.
    std::vector<std::uint8_t> locator {1U}, previous {1U};
    std::size_t degree = 0U, shift = 1U;
    std::uint8_t previousDiscrepancy = 1U;
    for (std::size_t n = 0; n < reduced.size(); ++n) {
        std::uint8_t discrepancy = reduced[n];
        for (std::size_t i = 1; i <= degree; ++i)
            discrepancy ^= gf.multiply(locator[i], reduced[n - i]);
        if (!discrepancy) { ++shift; continue; }
        auto saved = locator;
        const auto factor = gf.multiply(discrepancy, gf.inverse(previousDiscrepancy));
        if (locator.size() < previous.size() + shift) locator.resize(previous.size() + shift, 0U);
        for (std::size_t i = 0; i < previous.size(); ++i)
            locator[i + shift] ^= gf.multiply(factor, previous[i]);
        if (2U * degree <= n) {
            degree = n + 1U - degree;
            previous = std::move(saved);
            previousDiscrepancy = discrepancy;
            shift = 1U;
        } else ++shift;
    }
    if (2U * degree + missing.size() > parity) return false;
    std::vector<std::size_t> positions = missing;
    for (std::size_t position = 0; position < blockSize && degree; ++position) {
        if (std::find(missing.begin(), missing.end(), position) != missing.end()) continue;
        const auto x = gf.powers[(255U - position) % 255U];
        std::uint8_t value = 0U;
        for (std::size_t i = degree + 1U; i > 0U; --i)
            value = gf.multiply(value, x) ^ locator[i - 1U];
        if (!value) positions.push_back(position);
    }
    if (positions.size() != missing.size() + degree) return false;
    for (const auto position : positions) block[position] = 0U;
    const std::size_t count = positions.size();
    const std::size_t extraWork = count * count * count + count * blockSize;
    if (extraWork > maximumRepairWork - work) return false;
    work += extraWork;
    // Solve the located symbol values, then validate every syndrome.
    std::vector<std::vector<std::uint8_t>> matrix(count, std::vector<std::uint8_t>(count + 1U));
    for (std::size_t row = 0; row < count; ++row) {
        const std::size_t root = row + 1U;
        for (std::size_t column = 0; column < count; ++column)
            matrix[row][column] = gf.powers[(positions[column] * root) % 255U];
        matrix[row][count] = syndrome(block, root);
    }
    for (std::size_t column = 0; column < count; ++column) {
        std::size_t pivot = column;
        while (pivot < count && matrix[pivot][column] == 0U) ++pivot;
        if (pivot == count) return false;
        std::swap(matrix[pivot], matrix[column]);
        const auto scale = gf.inverse(matrix[column][column]);
        for (std::size_t j = column; j <= count; ++j)
            matrix[column][j] = gf.multiply(matrix[column][j], scale);
        for (std::size_t row = 0; row < count; ++row) {
            if (row == column || matrix[row][column] == 0U) continue;
            const auto factor = matrix[row][column];
            for (std::size_t j = column; j <= count; ++j)
                matrix[row][j] ^= gf.multiply(factor, matrix[column][j]);
        }
    }
    for (std::size_t i = 0; i < count; ++i) block[positions[i]] = matrix[i][count];
    for (std::size_t root = 1U; root <= parity; ++root)
        if (syndrome(block, root) != 0U) return false;
    return true;
}

template<typename T> HamDrmValueResult<T> failure(HamDrmErrorCode code, const char* detail)
{
    return {{}, HamDrmStatus::failure(code, detail)};
}
} // namespace

HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmRs(
    const std::vector<std::uint8_t>& image, const std::string& extension,
    unsigned level, const HamDrmLimits& limits)
{
    const std::size_t k = dataSize(level);
    if (!k || !validExtension(extension) || image.empty())
        return failure<std::vector<std::uint8_t>>(HamDrmErrorCode::InvalidArgument, "invalid RS level, extension or empty image");
    if (image.size() > limits.maximumObjectBytes || image.size() > std::numeric_limits<std::size_t>::max() - 7U)
        return failure<std::vector<std::uint8_t>>(HamDrmErrorCode::LimitExceeded, "RS source exceeds size limit");
    const std::size_t chunks = (image.size() + 7U + k - 1U) / k;
    if (chunks > 65535U || chunks > limits.maximumObjectBytes / blockSize)
        return failure<std::vector<std::uint8_t>>(HamDrmErrorCode::LimitExceeded, "RS container exceeds size limit");
    if (chunks > maximumRepairWork / (k * (blockSize - k)))
        return failure<std::vector<std::uint8_t>>(HamDrmErrorCode::LimitExceeded, "RS encode work limit exceeded");
    std::vector<std::uint8_t> source(chunks * k, 0U);
    source[0] = static_cast<std::uint8_t>(k - image.size() % k);
    source[1] = static_cast<std::uint8_t>(chunks & 0xffU);
    source[2] = static_cast<std::uint8_t>(chunks >> 8U);
    std::copy(extension.begin(), extension.end(), source.begin() + 3);
    std::copy(image.begin(), image.end(), source.begin() + 7);
    std::vector<std::uint8_t> output(chunks * blockSize);
    const auto polynomial = generator(blockSize - k);
    for (std::size_t row = 0; row < chunks; ++row) {
        std::array<std::uint8_t, blockSize> block {};
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(row * k), k, block.begin());
        addParity(block, k, polynomial);
        for (std::size_t column = 0; column < blockSize; ++column)
            output[column * chunks + row] = block[column];
    }
    return {std::move(output), {}};
}

HamDrmValueResult<HamDrmRsDecoded> decodeHamDrmRs(
    const std::vector<std::uint8_t>& container, unsigned level,
    const std::vector<std::size_t>& erasures, const HamDrmLimits& limits)
{
    const std::size_t k = dataSize(level);
    if (!k || container.empty() || container.size() % blockSize != 0U)
        return failure<HamDrmRsDecoded>(HamDrmErrorCode::Malformed, "invalid RS level or container size");
    if (container.size() > limits.maximumObjectBytes || container.size() / blockSize > 65535U)
        return failure<HamDrmRsDecoded>(HamDrmErrorCode::LimitExceeded, "RS container exceeds size limit");
    const std::size_t chunks = container.size() / blockSize;
    std::vector<std::vector<std::size_t>> missing(chunks);
    std::vector<bool> marked(container.size(), false);
    for (const auto position : erasures) {
        if (position >= container.size() || marked[position])
            return failure<HamDrmRsDecoded>(HamDrmErrorCode::InvalidArgument, "invalid or duplicate RS erasure");
        marked[position] = true;
        missing[position % chunks].push_back(position / chunks);
    }
    std::size_t work = 0U;
    std::vector<std::uint8_t> decoded(chunks * k);
    std::vector<std::uint8_t> repairedContainer = container;
    for (std::size_t row = 0; row < chunks; ++row) {
        const std::size_t count = missing[row].size();
        if (count > blockSize - k)
            return failure<HamDrmRsDecoded>(HamDrmErrorCode::Incomplete, "too many RS erasures in a block");
        const std::size_t parity = blockSize - k;
        const std::size_t cost = 2U * blockSize * parity + parity * parity;
        if (cost > maximumRepairWork - work)
            return failure<HamDrmRsDecoded>(HamDrmErrorCode::LimitExceeded, "RS repair work limit exceeded");
        work += cost;
        std::array<std::uint8_t, blockSize> block {};
        for (std::size_t column = 0; column < blockSize; ++column)
            block[column] = container[column * chunks + row];
        if (!repair(block, missing[row], parity, work))
            return failure<HamDrmRsDecoded>(HamDrmErrorCode::Malformed, "RS block cannot be validated or repaired");
        for (std::size_t column = 0; column < blockSize; ++column)
            repairedContainer[column * chunks + row] = block[column];
        std::copy_n(block.begin(), k, decoded.begin() + static_cast<std::ptrdiff_t>(row * k));
    }
    const std::size_t surplus = decoded[0];
    if (surplus == 0U || surplus > k || (std::size_t(decoded[1]) | (std::size_t(decoded[2]) << 8U)) != chunks
        || decoded[6] != 0U)
        return failure<HamDrmRsDecoded>(HamDrmErrorCode::Malformed, "invalid RS container header");
    const std::string extension(decoded.begin() + 3, decoded.begin() + 6);
    if (!validExtension(extension))
        return failure<HamDrmRsDecoded>(HamDrmErrorCode::UnsupportedContent, "unsupported RS image extension");
    std::size_t size = chunks * k - surplus;
    if (surplus < 7U) {
        if (size < k) return failure<HamDrmRsDecoded>(HamDrmErrorCode::Malformed, "invalid RS length");
        size -= k;
    }
    if (size == 0U || size > decoded.size() - 7U || size > limits.maximumObjectBytes)
        return failure<HamDrmRsDecoded>(HamDrmErrorCode::Malformed, "invalid RS image size");
    HamDrmRsDecoded result {extension, {decoded.begin() + 7, decoded.begin() + 7 + static_cast<std::ptrdiff_t>(size)},
                            std::move(repairedContainer)};
    return {std::move(result), {}};
}
} // namespace decodium::sstv::hamdrm
