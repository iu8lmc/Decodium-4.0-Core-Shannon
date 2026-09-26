// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace decodium::sstv::hamdrm::phy {

using HamDrmComplex = std::complex<double>;

enum class HamDrmTransformDirection {
    Forward,
    Inverse,
};

class HamDrmComplexTransform
{
public:
    virtual ~HamDrmComplexTransform() = default;

    virtual std::size_t maximumSize() const noexcept = 0;
    virtual std::vector<HamDrmComplex> execute(
        const std::vector<HamDrmComplex>& input,
        HamDrmTransformDirection direction) const = 0;
};

// Audit-friendly mixed-radix FFT.  The production HAMDRM sizes factor into
// 2, 3 and 5; a bounded direct DFT is retained only for a residual prime leaf.
// Forward transforms are unscaled and inverse transforms use 1/N scaling.
class HamDrmMixedRadixTransform final : public HamDrmComplexTransform
{
public:
    static constexpr std::size_t kDefaultMaximumSize = 4'096U;

    explicit HamDrmMixedRadixTransform(
        std::size_t maximumTransformSize = kDefaultMaximumSize);

    std::size_t maximumSize() const noexcept override;
    std::vector<HamDrmComplex> execute(
        const std::vector<HamDrmComplex>& input,
        HamDrmTransformDirection direction) const override;

private:
    std::size_t maximumTransformSize_;
};

} // namespace decodium::sstv::hamdrm::phy
