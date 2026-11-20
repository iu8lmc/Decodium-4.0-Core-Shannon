// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmInterleaver.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace decodium::sstv::hamdrm::channel {

namespace {

void validateBinary(const std::vector<std::uint8_t>& bits)
{
    if (std::any_of(bits.begin(), bits.end(),
                    [](std::uint8_t bit) { return bit > 1U; })) {
        throw std::invalid_argument("HAMDRM interleaver input is not binary");
    }
}

void validateSymbolShape(std::size_t frameSize, std::size_t depth)
{
    if (frameSize < 4U || frameSize > kHamDrmMaximumInterleaverElements) {
        throw std::invalid_argument("HAMDRM symbol interleaver frame size is invalid");
    }
    if (depth != 1U && depth != 5U) {
        throw std::invalid_argument("HAMDRM symbol interleaver depth must be 1 or 5");
    }
}

} // namespace

std::vector<std::size_t> hamDrmBlockInterleaverPermutation(
    std::size_t frameSize,
    std::size_t t0)
{
    const bool supportedMultiplier = t0 == 5U || t0 == 13U || t0 == 21U;
    if (frameSize < 4U || frameSize > kHamDrmMaximumInterleaverElements
            || !supportedMultiplier) {
        throw std::invalid_argument("invalid HAMDRM block interleaver parameters");
    }

    std::size_t s = 1U;
    while (s <= frameSize) {
        if (s > std::numeric_limits<std::size_t>::max() / 2U) {
            throw std::length_error("HAMDRM interleaver modulus overflow");
        }
        s *= 2U;
    }
    const std::size_t q = s / 4U - 1U;
    std::vector<std::size_t> permutation(frameSize, 0U);
    std::vector<bool> seen(frameSize, false);
    seen[0] = true;
    for (std::size_t index = 1U; index < frameSize; ++index) {
        std::size_t candidate = (t0 * permutation[index - 1U] + q) % s;
        std::size_t attempts = 0U;
        while (candidate >= frameSize) {
            candidate = (t0 * candidate + q) % s;
            if (++attempts > s) {
                throw std::invalid_argument("HAMDRM interleaver recurrence did not converge");
            }
        }
        if (seen[candidate]) {
            throw std::invalid_argument("HAMDRM interleaver recurrence is not a permutation");
        }
        seen[candidate] = true;
        permutation[index] = candidate;
    }
    return permutation;
}

std::vector<std::uint8_t> hamDrmBitInterleave(
    const std::vector<std::uint8_t>& input,
    std::size_t t0)
{
    validateBinary(input);
    const auto permutation = hamDrmBlockInterleaverPermutation(input.size(),
                                                                t0);
    std::vector<std::uint8_t> output(input.size(), 0U);
    for (std::size_t index = 0U; index < input.size(); ++index) {
        output[index] = input[permutation[index]];
    }
    return output;
}

std::vector<std::uint8_t> hamDrmBitDeinterleave(
    const std::vector<std::uint8_t>& input,
    std::size_t t0)
{
    validateBinary(input);
    const auto permutation = hamDrmBlockInterleaverPermutation(input.size(),
                                                                t0);
    std::vector<std::uint8_t> output(input.size(), 0U);
    for (std::size_t index = 0U; index < input.size(); ++index) {
        output[permutation[index]] = input[index];
    }
    return output;
}

HamDrmSymbolInterleaverEncoder::HamDrmSymbolInterleaverEncoder(
    std::size_t frameSize,
    std::size_t depth)
    : frameSize_(frameSize),
      depth_(depth),
      permutation_(hamDrmBlockInterleaverPermutation(frameSize, 5U)),
      memory_(depth, std::vector<phy::HamDrmComplex>(frameSize)),
      currentIndex_(depth, 0U)
{
    validateSymbolShape(frameSize, depth);
    reset();
}

std::vector<phy::HamDrmComplex> HamDrmSymbolInterleaverEncoder::push(
    const std::vector<phy::HamDrmComplex>& frame)
{
    if (frame.size() != frameSize_) {
        throw std::invalid_argument("HAMDRM symbol interleaver frame size mismatch");
    }
    memory_[currentIndex_[0U]] = frame;
    std::vector<phy::HamDrmComplex> output(frameSize_);
    for (std::size_t index = 0U; index < frameSize_; ++index) {
        output[index] = memory_[currentIndex_[index % depth_]]
                               [permutation_[index]];
    }
    for (auto& current : currentIndex_) {
        current = current == 0U ? depth_ - 1U : current - 1U;
    }
    return output;
}

void HamDrmSymbolInterleaverEncoder::reset()
{
    for (auto& block : memory_) {
        std::fill(block.begin(), block.end(), phy::HamDrmComplex {});
    }
    for (std::size_t index = 0U; index < depth_; ++index) {
        currentIndex_[index] = index;
    }
}

HamDrmSymbolInterleaverDecoder::HamDrmSymbolInterleaverDecoder(
    std::size_t frameSize,
    std::size_t depth)
    : frameSize_(frameSize),
      depth_(depth),
      permutation_(hamDrmBlockInterleaverPermutation(frameSize, 5U)),
      pending_(depth, std::vector<phy::HamDrmComplex>(frameSize))
{
    validateSymbolShape(frameSize, depth);
}

std::optional<std::vector<phy::HamDrmComplex>>
HamDrmSymbolInterleaverDecoder::push(
    const std::vector<phy::HamDrmComplex>& transmittedFrame)
{
    if (transmittedFrame.size() != frameSize_) {
        throw std::invalid_argument("HAMDRM symbol deinterleaver frame size mismatch");
    }
    const std::size_t current = receivedFrames_;
    for (std::size_t index = 0U; index < frameSize_; ++index) {
        const std::size_t delay = index % depth_;
        if (current >= delay) {
            const std::size_t sourceFrame = current - delay;
            pending_[sourceFrame % depth_][permutation_[index]] =
                transmittedFrame[index];
        }
    }
    ++receivedFrames_;
    if (receivedFrames_ < depth_) {
        return std::nullopt;
    }
    const std::size_t completeFrame = receivedFrames_ - depth_;
    std::vector<phy::HamDrmComplex> result =
        pending_[completeFrame % depth_];
    std::fill(pending_[completeFrame % depth_].begin(),
              pending_[completeFrame % depth_].end(),
              phy::HamDrmComplex {});
    return result;
}

void HamDrmSymbolInterleaverDecoder::reset()
{
    receivedFrames_ = 0U;
    for (auto& block : pending_) {
        std::fill(block.begin(), block.end(), phy::HamDrmComplex {});
    }
}

} // namespace decodium::sstv::hamdrm::channel
