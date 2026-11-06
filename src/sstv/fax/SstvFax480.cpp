// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvFax480.h"
#include <stdexcept>
#include <utility>

namespace decodium::sstv {
namespace {
constexpr std::uint64_t kSecondNs = 1'000'000'000ULL;
constexpr std::uint64_t kHeaderNs = 5'002'000'000ULL;
constexpr std::uint64_t kHalfCycleNs = 2'050'000ULL;
constexpr std::uint64_t kSyncNs = 5'120'000ULL;
constexpr std::uint64_t kDaytonLineNs = 267'264'000ULL;
constexpr std::uint64_t kQsstvLineNs = 267'266'000ULL;
}
SstvFax480Mapper::SstvFax480Mapper(SstvFax480Profile profile, std::uint32_t sampleRate)
    : profile_(profile), rate_(sampleRate),
      lineNs_(profile == SstvFax480Profile::Qsstv500 ? kQsstvLineNs : kDaytonLineNs),
      headerNs_(profile == SstvFax480Profile::LibsstvVis480 ? 910'000'000ULL : kHeaderNs)
{
    if ((profile != SstvFax480Profile::Dayton480 && profile != SstvFax480Profile::Qsstv500
         && profile != SstvFax480Profile::LibsstvVis480)
        || rate_ < 8'000U || rate_ > 384'000U)
        throw std::invalid_argument("invalid FAX480 profile");
}
std::uint64_t SstvFax480Mapper::sampleAtNs(std::uint64_t ns) const noexcept
{ return ns * rate_ / kSecondNs; }
std::uint32_t SstvFax480Mapper::height() const noexcept
{ return profile_ == SstvFax480Profile::Qsstv500 ? 500U : 480U; }
std::uint64_t SstvFax480Mapper::imageStart() const noexcept
{ return sampleAtNs(headerNs_ + (profile_ == SstvFax480Profile::Dayton480 ? 20U * lineNs_ : 0U)); }
std::uint64_t SstvFax480Mapper::lineStart(std::uint32_t line) const
{
    if (line > height()) throw std::out_of_range("FAX480 line");
    return sampleAtNs(headerNs_ + (profile_ == SstvFax480Profile::Dayton480 ? 20U * lineNs_ : 0U)
                      + std::uint64_t(line) * lineNs_);
}
std::uint64_t SstvFax480Mapper::totalSamples() const noexcept
{ return sampleAtNs(headerNs_ + (profile_ == SstvFax480Profile::Dayton480 ? 20U * lineNs_ : 0U)
                    + std::uint64_t(height()) * lineNs_); }
SstvFax480Position SstvFax480Mapper::position(std::uint64_t sample) const noexcept
{
    if (sample >= totalSamples()) return {};
    // Invert floor-based cumulative sample boundaries; do not accumulate
    // rounded pixel or line lengths, which would create long-frame drift.
    const auto ns = ((sample + 1U) * kSecondNs - 1U) / rate_;
    if (sample < sampleAtNs(headerNs_)) {
        if (profile_ == SstvFax480Profile::LibsstvVis480) {
            if (ns < 300'000'000ULL) return {SstvFax480Region::Start,0U,0U,1900.0};
            if (ns < 310'000'000ULL) return {SstvFax480Region::Start,0U,0U,1200.0};
            if (ns < 610'000'000ULL) return {SstvFax480Region::Start,0U,0U,1900.0};
            if (ns < 640'000'000ULL || ns >= 880'000'000ULL)
                return {SstvFax480Region::Start,0U,0U,1200.0};
            const auto bit=(ns-640'000'000ULL)/30'000'000ULL;
            const bool one=bit<7U && ((85U >> bit)&1U);
            return {SstvFax480Region::Start,0U,0U,one ? 1100.0 : 1300.0};
        }
        const bool even = (ns / kHalfCycleNs) % 2U == 0U;
        const bool white = profile_ == SstvFax480Profile::Dayton480 ? even : !even;
        return {SstvFax480Region::Start,0U,0U,white ? 2300.0 : 1500.0};
    }
    const auto afterHeader = ns - headerNs_;
    const auto wireLine = afterHeader / lineNs_;
    const auto within = afterHeader % lineNs_;
    if (profile_ == SstvFax480Profile::Dayton480 && wireLine < 20U)
        return {within < kSyncNs ? SstvFax480Region::PhasingSync : SstvFax480Region::PhasingWhite,
                static_cast<std::uint32_t>(wireLine),0U,within < kSyncNs ? 1200.0 : 2300.0};
    const auto imageLine = wireLine - (profile_ == SstvFax480Profile::Dayton480 ? 20U : 0U);
    const bool sync = profile_ == SstvFax480Profile::Qsstv500
        ? within >= lineNs_ - kSyncNs : within < kSyncNs;
    if (sync) return {SstvFax480Region::Sync,static_cast<std::uint32_t>(imageLine),0U,1200.0};
    const auto scanOffset = profile_ == SstvFax480Profile::Qsstv500 ? within : within - kSyncNs;
    const auto scanNs = lineNs_ - kSyncNs;
    const auto pixel = scanOffset * 512U / scanNs;
    return {SstvFax480Region::Pixel,static_cast<std::uint32_t>(imageLine),static_cast<std::uint32_t>(pixel),0.0};
}
SstvFax480Encoder::SstvFax480Encoder(SstvFax480Profile profile, std::uint32_t sampleRate,
                                     std::vector<std::uint8_t> pixels)
    : mapper_(profile,sampleRate), pixels_(std::move(pixels)), generator_(sampleRate)
{
    if (pixels_.size() != std::size_t(mapper_.height()) * 512U)
        throw std::invalid_argument("FAX480 image size mismatch");
}
double SstvFax480Encoder::frequency(std::uint64_t sample) const noexcept
{
    const auto p=mapper_.position(sample);
    if (p.region != SstvFax480Region::Pixel) return p.frequencyHz;
    return 1500.0 + 800.0 * pixels_[std::size_t(p.line)*512U+p.pixel] / 255.0;
}
std::size_t SstvFax480Encoder::pull(std::int16_t* output, std::size_t capacity)
{
    if (capacity > 262'144U) throw std::length_error("FAX480 pull exceeds bound");
    if (capacity && !output) throw std::invalid_argument("FAX480 null output");
    std::size_t written=0;
    while (written<capacity && !complete() && !generator_.cancelled()) {
        const auto f=frequency(cursor_);
        std::size_t n=1;
        while (n<capacity-written && cursor_+n<mapper_.totalSamples() && frequency(cursor_+n)==f) ++n;
        const auto made=generator_.generatePcm16(f,1.0,output+written,n);
        written+=made; cursor_+=made;
        if (made!=n) break;
    }
    return written;
}
bool SstvFax480Encoder::complete() const noexcept { return cursor_ >= mapper_.totalSamples(); }
void SstvFax480Encoder::cancel() noexcept { generator_.cancel(); }
void SstvFax480Encoder::reset() noexcept { generator_.reset(); cursor_=0U; }
} // namespace decodium::sstv
