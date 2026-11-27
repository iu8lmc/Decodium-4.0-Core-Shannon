// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvFax480Receiver.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace decodium::sstv {
SstvFax480Receiver::SstvFax480Receiver(SstvFax480Profile profile, std::uint32_t sampleRate)
    : mapper_(profile,sampleRate), sums_(std::size_t(mapper_.height())*512U,0.0),
      weights_(sums_.size(),0.0) {}
void SstvFax480Receiver::consume(const SstvFrequencyObservation* observations, std::size_t count)
{
    if (count>MaximumObservationsPerConsume) throw std::length_error("FAX480 observation bound");
    if (count && !observations) throw std::invalid_argument("FAX480 null observations");
    auto previous=last_; bool have=haveLast_;
    for (std::size_t i=0;i<count;++i) {
        if (have && observations[i].centreSample<previous) throw std::invalid_argument("FAX480 timestamp regression");
        previous=observations[i].centreSample; have=true;
    }
    for (std::size_t i=0;i<count;++i) {
        const auto& obs=observations[i];
        if (haveLast_ && obs.centreSample==last_) continue;
        last_=obs.centreSample; haveLast_=true;
        const auto pos=mapper_.position(obs.centreSample);
        if (pos.region!=SstvFax480Region::Pixel || !obs.valid()
            || !std::isfinite(obs.correctedFrequencyHz) || !std::isfinite(obs.confidence)
            || obs.confidence<0.2 || obs.confidence>1.0) continue;
        const auto idx=std::size_t(pos.line)*512U+pos.pixel;
        sums_[idx]+=obs.correctedFrequencyHz*obs.confidence;
        weights_[idx]+=obs.confidence;
    }
}
std::vector<std::uint8_t> SstvFax480Receiver::image() const
{
    std::vector<std::uint8_t> out(sums_.size());
    for (std::size_t i=0;i<out.size();++i) {
        if (weights_[i]<=0.0) continue;
        const auto value=(sums_[i]/weights_[i]-1500.0)*255.0/800.0;
        out[i]=static_cast<std::uint8_t>(std::lround(std::clamp(value,0.0,255.0)));
    }
    return out;
}
std::vector<std::uint8_t> SstvFax480Receiver::coverage() const
{
    std::vector<std::uint8_t> out(weights_.size());
    for (std::size_t i=0;i<out.size();++i) out[i]=weights_[i]>0.0;
    return out;
}
} // namespace decodium::sstv
