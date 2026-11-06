// SPDX-License-Identifier: GPL-3.0-or-later
// Offline, bounded diagnostic: 12 kHz mono s16le captured PCM, never an RF claim.
#include "src/sstv/digital/HamDrmObjectAssembler.h"
#include "src/sstv/digital/HamDrmRsCodec.h"
#include "src/sstv/digital/waveform/HamDrmWaveformCodec.h"
#include "src/sstv/digital/waveform/HamDrmFractionalDelay.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace decodium::sstv::hamdrm;
using namespace decodium::sstv::hamdrm::waveform;

int main(int argc, char** argv)
{
    if (argc < 8 || argc > 11) {
        std::cerr << "usage: analyze_hamdrm_capture PCM.s16 A|B|E 2300|2500 4|16|64 pilot-limit corrupt-fraction expected-object [short|long] [flat|flat-multistage|flat-timing-hybrid|selective|adaptive|adaptive-fallback|adaptive-multistage|adaptive-early|hybrid|timing-hybrid|half-hybrid|sinc-hybrid] [high|normal]\n";
        return 2;
    }
    try {
        const std::string mode = argv[2], bandwidth = argv[3], qam = argv[4];
        if ((mode != "A" && mode != "B" && mode != "E")
            || (bandwidth != "2300" && bandwidth != "2500")
            || (qam != "4" && qam != "16" && qam != "64")) return 2;
        HamDrmWaveformConfig config;
        config.robustness = mode == "A" ? HamDrmRobustness::A
            : mode == "B" ? HamDrmRobustness::B : HamDrmRobustness::E;
        config.occupiedBandwidth = bandwidth == "2300" ? HamDrmOccupiedBandwidth::Hz2300 : HamDrmOccupiedBandwidth::Hz2500;
        config.constellation = qam == "4" ? HamDrmConstellation::Qam4
            : qam == "16" ? HamDrmConstellation::Qam16 : HamDrmConstellation::Qam64;
        const std::string protection = argc >= 11 ? argv[10] : "high";
        if (protection != "high" && protection != "normal") return 2;
        config.protection = protection == "normal" ? HamDrmProtection::Normal : HamDrmProtection::High;
        const std::string interleaver = argc >= 9 ? argv[8] : "long";
        if (interleaver != "short" && interleaver != "long") return 2;
        config.interleaver = interleaver == "short" ? HamDrmInterleaver::Short : HamDrmInterleaver::Long;
        HamDrmWaveformLimits limits;
        bool hybrid = false;
        bool timingHybrid = false;
        bool halfHybrid = false;
        bool sincHybrid = false;
        if (argc >= 10) {
            const std::string equalizer = argv[9];
            if (equalizer != "flat" && equalizer != "flat-multistage" && equalizer != "flat-timing-hybrid" && equalizer != "selective" && equalizer != "adaptive" && equalizer != "adaptive-fallback" && equalizer != "adaptive-multistage" && equalizer != "adaptive-early" && equalizer != "hybrid" && equalizer != "timing-hybrid" && equalizer != "half-hybrid" && equalizer != "sinc-hybrid") return 2;
            limits.experimentalFrequencySelective = equalizer != "flat" && equalizer != "flat-multistage" && equalizer != "flat-timing-hybrid";
            limits.experimentalAdaptiveEqualizer = equalizer == "adaptive" || equalizer == "adaptive-fallback" || equalizer == "adaptive-multistage" || equalizer == "adaptive-early" || equalizer == "timing-hybrid" || equalizer == "half-hybrid" || equalizer == "sinc-hybrid";
            limits.experimentalFlatPacketFallback = equalizer == "adaptive-fallback" || equalizer == "adaptive-multistage" || equalizer == "adaptive-early" || equalizer == "timing-hybrid" || equalizer == "half-hybrid" || equalizer == "sinc-hybrid";
            limits.experimentalMultistagePacketRetry = equalizer == "adaptive-multistage" || equalizer == "adaptive-early" || equalizer == "timing-hybrid" || equalizer == "flat-multistage" || equalizer == "flat-timing-hybrid" || equalizer == "half-hybrid" || equalizer == "sinc-hybrid";
            limits.experimentalEarlyTiming = equalizer == "adaptive-early";
            hybrid = equalizer == "hybrid";
            timingHybrid = equalizer == "timing-hybrid" || equalizer == "flat-timing-hybrid" || equalizer == "half-hybrid" || equalizer == "sinc-hybrid";
            halfHybrid = equalizer == "half-hybrid" || equalizer == "sinc-hybrid";
            sincHybrid = equalizer == "sinc-hybrid";
        }
        limits.maximumNormalizedPilotSquaredError = std::stod(argv[5]);
        limits.experimentalTemporalPilots = std::getenv("DECODIUM_HAMDRM_TEMPORAL_PILOTS") != nullptr;
        limits.experimentalFrameTimingTracking = std::getenv("DECODIUM_HAMDRM_FRAME_TRACKING") != nullptr;
        if (limits.experimentalTemporalPilots) {
            limits.experimentalAdaptiveEqualizer = false;
            limits.experimentalFlatPacketFallback = false;
        }
        limits.maximumCorruptedPilotFraction = std::stod(argv[6]);
        if (!std::isfinite(limits.maximumNormalizedPilotSquaredError)
            || limits.maximumNormalizedPilotSquaredError < 0.0 || limits.maximumNormalizedPilotSquaredError > 10.0
            || !std::isfinite(limits.maximumCorruptedPilotFraction)
            || limits.maximumCorruptedPilotFraction < 0.0 || limits.maximumCorruptedPilotFraction > 1.0) return 2;
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        std::ifstream expectedFile(argv[7], std::ios::binary | std::ios::ate);
        if (!input || !expectedFile || input.tellg() < 2 || input.tellg() > 16'000'000
            || expectedFile.tellg() < 1 || expectedFile.tellg() > 16'777'216) return 2;
        const auto size = static_cast<std::size_t>(input.tellg());
        if (size % 2U) return 2;
        input.seekg(0); expectedFile.seekg(0);
        const std::vector<std::uint8_t> expected((std::istreambuf_iterator<char>(expectedFile)), {});
        HamDrmWaveformReceiver receiver(config, limits);
        std::unique_ptr<HamDrmWaveformReceiver> temporalReceiver;
        if (std::getenv("DECODIUM_HAMDRM_TEMPORAL_MERGE") != nullptr) {
            if (limits.experimentalTemporalPilots) return 2;
            auto temporalLimits = limits;
            temporalLimits.experimentalFrequencySelective = false;
            temporalLimits.experimentalAdaptiveEqualizer = false;
            temporalLimits.experimentalFlatPacketFallback = false;
            temporalLimits.experimentalEarlyTiming = false;
            temporalLimits.experimentalTemporalPilots = true;
            temporalReceiver = std::make_unique<HamDrmWaveformReceiver>(config, temporalLimits);
        }
        std::unique_ptr<HamDrmWaveformReceiver> flatReceiver;
        if (hybrid) flatReceiver = std::make_unique<HamDrmWaveformReceiver>(config);
        if (timingHybrid) {
            auto earlyLimits = limits;
            earlyLimits.experimentalEarlyTiming = true;
            flatReceiver = std::make_unique<HamDrmWaveformReceiver>(config, earlyLimits);
        }
        std::unique_ptr<HamDrmWaveformReceiver> halfReceiver;
        if (halfHybrid) halfReceiver = std::make_unique<HamDrmWaveformReceiver>(config, limits);
        double previousSample = 0.0;
        bool havePrevious = false;
        HamDrmFractionalDelay sincDelay;
        std::map<std::uint16_t, std::unique_ptr<HamDrmObjectAssembler>> objects;
        std::size_t ingestErrors = 0U;
        const bool trace = std::getenv("DECODIUM_HAMDRM_TRACE") != nullptr;
        std::size_t inputSamples = 0U;
        std::size_t lastFrameCount = 0U;
        std::vector<double> rejectedPilotErrors;
        std::map<std::string, std::size_t> statusCounts;
        bool exact = false;
        std::size_t referenceZeroPaddingBytes = 0U;
        std::vector<unsigned char> bytes(4096U);
        while (input) {
            input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            const auto count = static_cast<std::size_t>(input.gcount());
            if (!count) break;
            std::vector<double> pcm(count / 2U);
            for (std::size_t i = 0; i < pcm.size(); ++i) {
                const unsigned value = bytes[2U * i] | (unsigned(bytes[2U * i + 1U]) << 8U);
                const int sample = value < 32768U ? int(value) : int(value) - 65536;
                pcm[i] = double(sample) / 32768.0;
            }
            auto batch = receiver.pushPcm(pcm);
            if (temporalReceiver) {
                auto temporalBatch = temporalReceiver->pushPcm(pcm);
                batch.dataGroups.insert(batch.dataGroups.end(), temporalBatch.dataGroups.begin(), temporalBatch.dataGroups.end());
            }
            if (flatReceiver) {
                auto flatBatch = flatReceiver->pushPcm(pcm);
                batch.dataGroups.insert(batch.dataGroups.end(), flatBatch.dataGroups.begin(), flatBatch.dataGroups.end());
            }
            if (halfReceiver) {
                auto halfPcm = sincHybrid ? sincDelay.process(pcm) : pcm;
                if (!sincHybrid) {
                    for (std::size_t i = 0; i < pcm.size(); ++i) {
                        halfPcm[i] = havePrevious ? 0.5 * (previousSample + pcm[i]) : pcm[i];
                        previousSample = pcm[i];
                        havePrevious = true;
                    }
                }
                auto halfBatch = halfReceiver->pushPcm(halfPcm);
                batch.dataGroups.insert(batch.dataGroups.end(), halfBatch.dataGroups.begin(), halfBatch.dataGroups.end());
            }
            if (!batch.status.ok() && !batch.status.detail.empty())
                ++statusCounts[batch.status.detail];
            inputSamples += pcm.size();
            if (trace && batch.statistics.receivedFrames != lastFrameCount) {
                lastFrameCount = batch.statistics.receivedFrames;
                std::cerr << "sample=" << inputSamples
                          << " frames=" << lastFrameCount
                          << " cfo=" << batch.statistics.estimatedFrequencyOffsetHz
                          << " start=" << batch.statistics.lastFrameStartSample
                          << " cp=" << batch.statistics.lastCyclicPrefixMetric
                          << " pilot=" << batch.statistics.lastNormalizedPilotSquaredError
                          << " groups=" << batch.statistics.validatedDataGroups
                          << " pilot_failures=" << batch.statistics.pilotFailures
                          << " msc_failures=" << batch.statistics.mscFailures << '\n';
            }
            if (batch.status.detail == "HAMDRM pilot validation rejected the frame"
                && std::isfinite(batch.statistics.lastNormalizedPilotSquaredError)) {
                rejectedPilotErrors.push_back(batch.statistics.lastNormalizedPilotSquaredError);
                if (trace) std::cerr << "rejected_sample=" << inputSamples
                    << " cfo=" << batch.statistics.estimatedFrequencyOffsetHz
                    << " start=" << batch.statistics.lastFrameStartSample
                    << " cp=" << batch.statistics.lastCyclicPrefixMetric
                    << " pilot=" << batch.statistics.lastNormalizedPilotSquaredError << '\n';
            }
            for (const auto& bytesGroup : batch.dataGroups) {
                const auto group = parseHamDrmMotDataGroup(bytesGroup.data(), bytesGroup.size());
                if (!group.ok()) { ++ingestErrors; continue; }
                auto found = objects.find(group.value->transportId);
                if (found == objects.end()) {
                    if (objects.size() >= 64U) return 2;
                    found = objects.emplace(group.value->transportId,
                        std::make_unique<HamDrmObjectAssembler>(group.value->transportId)).first;
                }
                if (!found->second->ingest(*group.value).ok()) ++ingestErrors;
                const auto object = found->second->assembledObject();
                if (object.ok()) {
                    auto actual = object.value->originalBytes;
                    if (object.value->metadata.mimeType == "application/x-hamdrm-rs") {
                        const auto& name = object.value->metadata.filename;
                        if (name.empty() || name.back() < '1' || name.back() > '4') return 2;
                        const auto decoded = decodeHamDrmRs(actual, unsigned(name.back() - '0'));
                        if (!decoded.ok()) return 2;
                        actual = decoded.value->bytes;
                    }
                    if (actual == expected) exact = true;
                    if (expected.size() > actual.size()
                        && std::equal(actual.begin(), actual.end(), expected.begin())
                        && std::all_of(expected.begin() + static_cast<std::ptrdiff_t>(actual.size()),
                                       expected.end(), [](std::uint8_t byte) { return byte == 0U; }))
                        referenceZeroPaddingBytes = expected.size() - actual.size();
                    if (const char* outputPath = std::getenv("DECODIUM_HAMDRM_OBJECT_OUTPUT")) {
                        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
                        output.write(reinterpret_cast<const char*>(actual.data()),
                                     static_cast<std::streamsize>(actual.size()));
                        if (!output) return 2;
                    }
                }
            }
        }
        const auto stats = receiver.statistics();
        std::cout << "flat_fallback_packets=" << stats.flatFallbackPacketRecoveries << '\n';
        std::cout << "multistage_packets=" << stats.multistagePacketRecoveries << '\n';
        if (flatReceiver) {
            const auto flat = flatReceiver->statistics();
            std::cout << "hybrid_secondary_frames=" << flat.receivedFrames
                      << " hybrid_secondary_groups=" << flat.validatedDataGroups << '\n';
        }
        if (halfReceiver) {
            const auto half = halfReceiver->statistics();
            std::cout << "hybrid_half_frames=" << half.receivedFrames
                      << " hybrid_half_groups=" << half.validatedDataGroups << '\n';
        }
        if (!rejectedPilotErrors.empty()) {
            std::sort(rejectedPilotErrors.begin(), rejectedPilotErrors.end());
            std::cout << "pilot_error_samples=" << rejectedPilotErrors.size()
                      << " min=" << rejectedPilotErrors.front()
                      << " median=" << rejectedPilotErrors[rejectedPilotErrors.size() / 2U]
                      << " max=" << rejectedPilotErrors.back() << '\n';
        }
        if (trace) for (const auto& entry : statusCounts)
            std::cerr << "status_count=" << entry.second << " detail=" << entry.first << '\n';
        for (const auto& entry : objects) {
            const auto progress = entry.second->progress();
            std::cout << "transport=" << entry.first << " header=" << progress.headerComplete
                      << " received=" << progress.bodySegmentsReceived << " total=" << progress.totalBodySegments
                      << " complete=" << progress.objectComplete << '\n';
            const auto partial = entry.second->partialBodyForRepair(true);
            if (partial) {
                std::vector<std::uint8_t> header;
                const auto snapshot = entry.second->snapshotGroups();
                if (snapshot.ok()) for (const auto& raw : *snapshot.value) {
                    const auto group = parseHamDrmMotDataGroup(raw.data(), raw.size());
                    if (group.ok() && group.value->kind == HamDrmMotGroupKind::Header)
                        header.insert(header.end(), group.value->payload.begin(), group.value->payload.end());
                }
                const auto metadata = parseHamDrmMotHeader(header.data(), header.size(), entry.first);
                if (metadata.ok()) std::cout << "rs_filename=" << metadata.value->filename << '\n';
                std::cout << "rs_missing_segments=" << partial->missingSegments.size()
                          << " rs_erasures=" << partial->erasures.size()
                          << " rs_container_bytes=" << partial->bytes.size() << '\n';
                for (unsigned level = 1; level <= 4; ++level) {
                    const auto repaired = decodeHamDrmRs(partial->bytes, level, partial->erasures);
                    if (repaired.ok()) {
                        const auto& data = repaired.value->bytes;
                        const bool match = data == expected;
                        const bool padded = expected.size() > data.size()
                            && std::equal(data.begin(), data.end(), expected.begin())
                            && std::all_of(expected.begin() + static_cast<std::ptrdiff_t>(data.size()),
                                           expected.end(), [](std::uint8_t byte) { return byte == 0U; });
                        std::cout << "rs_level=" << level << " repaired=1 exact=" << match
                                  << " reference_zero_padding="
                                  << (padded ? expected.size() - data.size() : 0U) << '\n';
                        if (match) {
                            exact = true;
                            if (const char* outputPath = std::getenv("DECODIUM_HAMDRM_OBJECT_OUTPUT")) {
                                std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
                                output.write(reinterpret_cast<const char*>(data.data()),
                                             static_cast<std::streamsize>(data.size()));
                                if (!output) return 2;
                            }
                        }
                    }
                }
            }
        }
        std::cout << "frames=" << stats.receivedFrames << " pilot_failures=" << stats.pilotFailures
                  << " fac_failures=" << stats.facFailures << " msc_failures=" << stats.mscFailures
                  << " packet_failures=" << stats.packetCrcFailures << " mot_failures=" << stats.motCrcFailures
                  << " groups=" << stats.validatedDataGroups << " ingest_errors=" << ingestErrors
                  << " objects=" << objects.size() << " exact=" << exact
                  << " reference_zero_padding=" << referenceZeroPaddingBytes << '\n';
        return exact ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
