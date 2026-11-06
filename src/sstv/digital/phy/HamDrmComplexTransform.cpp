// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmComplexTransform.h"

#include <cmath>
#include <stdexcept>

namespace decodium::sstv::hamdrm::phy {

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

std::size_t smallestFactor(std::size_t size) noexcept
{
    for (const std::size_t candidate : {2U, 3U, 5U}) {
        if ((size % candidate) == 0U) {
            return candidate;
        }
    }
    return size;
}

std::vector<HamDrmComplex> directTransform(
    const std::vector<HamDrmComplex>& input,
    double directionSign)
{
    const std::size_t size = input.size();
    std::vector<HamDrmComplex> output(size, HamDrmComplex {});
    for (std::size_t frequency = 0U; frequency < size; ++frequency) {
        HamDrmComplex sum {};
        for (std::size_t time = 0U; time < size; ++time) {
            const double angle = directionSign * 2.0 * kPi
                * static_cast<double>(frequency)
                * static_cast<double>(time) / static_cast<double>(size);
            sum += input[time] * HamDrmComplex {std::cos(angle),
                                                std::sin(angle)};
        }
        output[frequency] = sum;
    }
    return output;
}

std::vector<HamDrmComplex> mixedRadixTransform(
    const std::vector<HamDrmComplex>& input,
    double directionSign)
{
    const std::size_t size = input.size();
    if (size == 1U) {
        return input;
    }

    const std::size_t radix = smallestFactor(size);
    if (radix == size) {
        return directTransform(input, directionSign);
    }

    const std::size_t branchSize = size / radix;
    std::vector<std::vector<HamDrmComplex>> branches;
    branches.reserve(radix);
    for (std::size_t branchIndex = 0U; branchIndex < radix;
         ++branchIndex) {
        std::vector<HamDrmComplex> branch(branchSize);
        for (std::size_t index = 0U; index < branchSize; ++index) {
            branch[index] = input[branchIndex + radix * index];
        }
        branches.push_back(mixedRadixTransform(branch, directionSign));
    }

    std::vector<HamDrmComplex> output(size, HamDrmComplex {});
    for (std::size_t frequency = 0U; frequency < size; ++frequency) {
        HamDrmComplex sum {};
        for (std::size_t branchIndex = 0U; branchIndex < radix;
             ++branchIndex) {
            const double angle = directionSign * 2.0 * kPi
                * static_cast<double>(frequency)
                * static_cast<double>(branchIndex)
                / static_cast<double>(size);
            sum += branches[branchIndex][frequency % branchSize]
                * HamDrmComplex {std::cos(angle), std::sin(angle)};
        }
        output[frequency] = sum;
    }
    return output;
}

} // namespace

HamDrmMixedRadixTransform::HamDrmMixedRadixTransform(
    std::size_t maximumTransformSize)
    : maximumTransformSize_(maximumTransformSize)
{
    if (maximumTransformSize_ == 0U) {
        throw std::invalid_argument("HAMDRM FFT maximum size must be positive");
    }
}

std::size_t HamDrmMixedRadixTransform::maximumSize() const noexcept
{
    return maximumTransformSize_;
}

std::vector<HamDrmComplex> HamDrmMixedRadixTransform::execute(
    const std::vector<HamDrmComplex>& input,
    HamDrmTransformDirection direction) const
{
    if (input.empty()) {
        throw std::invalid_argument("HAMDRM FFT input must not be empty");
    }
    if (input.size() > maximumTransformSize_) {
        throw std::length_error("HAMDRM FFT input exceeds configured bound");
    }

    const double sign = direction == HamDrmTransformDirection::Forward
        ? -1.0 : 1.0;
    auto output = mixedRadixTransform(input, sign);
    if (direction == HamDrmTransformDirection::Inverse) {
        const double scale = 1.0 / static_cast<double>(input.size());
        for (auto& sample : output) {
            sample *= scale;
        }
    }
    return output;
}

} // namespace decodium::sstv::hamdrm::phy
