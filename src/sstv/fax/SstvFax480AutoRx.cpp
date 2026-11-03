// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvFax480AutoRx.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace decodium::sstv {
SstvFax480AutoRx::SstvFax480AutoRx(SstvFax480Profile profile,std::uint32_t rate)
    : profile_(profile),rate_(rate),mapper_(profile,rate) {}
std::size_t SstvFax480AutoRx::consume(const SstvFrequencyObservation* obs,std::size_t count)
{
    if (count>SstvFax480Receiver::MaximumObservationsPerConsume) throw std::length_error("FAX480 observations too large");
    if (count && !obs) throw std::invalid_argument("FAX480 null observations");
    auto previous=last_;bool have=haveLast_;
    for (std::size_t i=0;i<count;++i) {
        if (have && obs[i].centreSample<previous) throw std::invalid_argument("FAX480 timestamp regression");
        previous=obs[i].centreSample;have=true;
    }
    std::size_t consumed=0;
    while (consumed<count && !complete_) observe(obs[consumed++]);
    return consumed;
}
void SstvFax480AutoRx::feed(const SstvFrequencyObservation& obs)
{
    if (!anchor_ || obs.centreSample<*anchor_) return;
    auto relative=obs;relative.centreSample-=*anchor_;
    receiver_->consume(&relative,1);
    if (relative.centreSample>=mapper_.totalSamples()) complete_=true;
}
void SstvFax480AutoRx::observe(const SstvFrequencyObservation& obs)
{
    if (haveLast_ && obs.centreSample==last_) return;
    if (!receiver_ && haveLast_ && obs.centreSample-last_>rate_/50U) {
        replay_.clear(); header_=false; inSync_=false; transitions_=0; tone_=0;
        vis_.reset(); visFrequency_=0.0; visPendingFrequency_=0.0;
    }
    last_=obs.centreSample;haveLast_=true;
    if (complete_) return;
    if (receiver_) { feed(obs); return; }
    replay_.push_back(obs);
    while (!replay_.empty() && (replay_.size()>8192U
           || obs.centreSample-replay_.front().centreSample>rate_)) replay_.pop_front();
    if (!obs.valid() || !std::isfinite(obs.correctedFrequencyHz) || !std::isfinite(obs.confidence) || obs.confidence<0.2) {
        inSync_=false;
        visPendingFrequency_=0.0;
        // A carrier before a silent gap must not extend a later VIS leader.
        // Brief demodulator dropouts are tolerated as in observeVis().
        // A short dropout can split a leader or a 30 ms VIS symbol. Longer
        // missing intervals reset acquisition rather than inventing a bit.
        const auto visGapLimit = visFrequency_==1900.0 ? rate_*15U/1000U : rate_*12U/1000U;
        if (profile_==SstvFax480Profile::LibsstvVis480 && visFrequency_
            && obs.centreSample-visRunEnd_>=visGapLimit) {
            vis_.reset(); visFrequency_=0.0; visPendingFrequency_=0.0;
        }
        return;
    }
    if (profile_==SstvFax480Profile::LibsstvVis480) { observeVis(obs); return; }
    const double f=obs.correctedFrequencyHz;
    if (header_) {
        if (obs.centreSample-runStart_>std::uint64_t(rate_)*7U) {
            header_=false;transitions_=0;tone_=0;inSync_=false;return;
        }
        if (obs.centreSample-runStart_<std::uint64_t(rate_)*4U) return;
        if (f>=1080.0 && f<=1320.0) {
            if (!inSync_) { inSync_=true;syncStart_=obs.centreSample; }
            if (obs.centreSample-syncStart_ >= rate_*3U/1000U) {
                const std::uint64_t offset=profile_==SstvFax480Profile::Dayton480
                    ? std::uint64_t(rate_)*5'002U/1000U
                    : std::uint64_t(rate_)*5'264'146U/1'000'000U;
                if (syncStart_<offset) { header_=false;transitions_=0;return; }
                anchor_=syncStart_-offset;
                receiver_=std::make_unique<SstvFax480Receiver>(profile_,rate_);
                for (const auto& old:replay_) feed(old);
                replay_.clear();
            }
        } else inSync_=false;
        return;
    }
    const int tone=f>=1350.0 && f<1750.0 ? 1 : (f>2050.0 && f<=2450.0 ? 2 : 0);
    if (!tone) return;
    if (!tone_) { tone_=tone;lastChange_=obs.centreSample;runStart_=obs.centreSample;return; }
    if (tone!=tone_) {
        const auto elapsed=obs.centreSample-lastChange_;
        if (elapsed>=rate_/1000U && elapsed<=rate_*4U/1000U) {
            if (++transitions_>=100U) header_=true;
        } else { transitions_=0;runStart_=obs.centreSample; }
        tone_=tone;lastChange_=obs.centreSample;
    } else if (obs.centreSample-lastChange_>rate_/100U) {
        tone_=0;transitions_=0;
    }
}
void SstvFax480AutoRx::observeVis(const SstvFrequencyObservation& obs)
{
    double selected=0.0, distance=46.0;
    for (const double tone : {1100.0,1200.0,1300.0,1900.0}) {
        const double d=std::abs(obs.correctedFrequencyHz-tone);
        if (d<distance) { distance=d; selected=tone; }
    }
    const auto unknownGapLimit = visFrequency_==1900.0 ? rate_*15U/1000U : rate_*12U/1000U;
    if (!selected) {
        visPendingFrequency_=0.0;
        if (!visFrequency_ || obs.centreSample-visRunEnd_<unknownGapLimit) return;
    }
    if (!visFrequency_) {
        visFrequency_=selected;visRunStart_=visRunEnd_=obs.centreSample;return;
    }
    if (selected==visFrequency_) { visRunEnd_=obs.centreSample;visPendingFrequency_=0.0;return; }
    // Ignore sub-millisecond classified spikes without splitting a leader or
    // symbol. Confirm a different tone before handing its boundary to VIS.
    if (selected!=0.0) {
        if (visPendingFrequency_!=selected) {
            visPendingFrequency_=selected;visPendingStart_=obs.centreSample;return;
        }
        if (obs.centreSample-visPendingStart_<rate_/1000U) return;
    }
    const auto nextStart=selected!=0.0 && visPendingFrequency_==selected
        ? visPendingStart_ : obs.centreSample;
    visPendingFrequency_=0.0;
    const auto duration=visRunEnd_-visRunStart_+rate_/2000U;
    if (duration<rate_/1000U) {
        visFrequency_=selected; visRunStart_=nextStart; visRunEnd_=obs.centreSample;
        return;
    }
    const auto runEnd=visRunEnd_+rate_/2000U;
    // The shared VIS detector accepts a bounded event, then trims a long VOX
    // leader to its final canonical window. Bound this adapter's aggregated
    // 1900 Hz run before submitting it, while preserving its actual end time.
    const auto eventDuration = visFrequency_==1900.0 ? std::min<std::uint64_t>(duration,rate_) : duration;
    const SstvVisToneEvent event {(runEnd-eventDuration)*1'000'000U/rate_,
                                 eventDuration*1'000'000U/rate_,visFrequency_,1.0};
    const auto results=vis_.consume(&event,1);
    const auto prior=visFrequency_;
    visFrequency_=selected;visRunStart_=nextStart;visRunEnd_=obs.centreSample;
    for (const auto& result:results) {
        if (!result.valid() || result.codecResult.primary.payload!=85U || prior!=1200.0) continue;
        const auto endOffset=std::uint64_t(rate_)*915'120U/1'000'000U;
        if (runEnd<endOffset && endOffset-runEnd>rate_/100U) continue;
        anchor_=runEnd<endOffset ? 0U : runEnd-endOffset;
        receiver_=std::make_unique<SstvFax480Receiver>(profile_,rate_);
        for (const auto& old:replay_) feed(old);
        replay_.clear();
        break;
    }
}
std::vector<std::uint8_t> SstvFax480AutoRx::image() const
{ return receiver_ ? receiver_->image() : std::vector<std::uint8_t>{}; }
std::vector<std::uint8_t> SstvFax480AutoRx::coverage() const
{ return receiver_ ? receiver_->coverage() : std::vector<std::uint8_t>{}; }
} // namespace decodium::sstv
