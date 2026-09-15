// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvModeRegistry.h"

#include "../diagnostics/SstvDiagnosticLogging.h"

#include "../analog/SstvAvt.h"
#include "../analog/SstvMmsstvExtended.h"
#include "../analog/SstvPd.h"
#include "../analog/SstvRobot.h"
#include "../analog/SstvSequentialRgb.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace decodium::sstv {
namespace {

struct CatalogueSeed final
{
    const char* id;
    const char* longName;
    const char* shortName;
    const char* family;
    ModeClassification classification;
    CatalogStatus status;
    const char* note;
};

constexpr const char* kCatalogueReference = "Mission section 5.1 and docs/sstv/MODE_CATALOG.md";

SstvModeSpec fromSeed(const CatalogueSeed& seed)
{
    SstvModeSpec mode;
    mode.id = seed.id;
    mode.longName = seed.longName;
    mode.shortName = seed.shortName;
    mode.family = seed.family;
    mode.classification = seed.classification;
    mode.catalogStatus = seed.status;
    mode.statusNote = seed.note;
    mode.catalogueReferences.emplace_back(kCatalogueReference);

    if (seed.status == CatalogStatus::Blocked) {
        mode.rxStatus = CapabilityStatus::Blocked;
        mode.txStatus = CapabilityStatus::Blocked;
        mode.autoDetectStatus = CapabilityStatus::Blocked;
        mode.interoperabilityStatus = InteroperabilityStatus::Blocked;
        mode.fixtureStatus = FixtureStatus::Blocked;
    }
    return mode;
}

void populateMartin(SstvModeSpec& mode,
                    std::uint8_t visPayload,
                    std::uint32_t imageHeight,
                    std::uint32_t sampledPixelWidth,
                    Picoseconds pixelDuration,
                    Picoseconds componentDuration,
                    Picoseconds lineDuration,
                    Picoseconds imageDuration,
                    const char* timingResolution,
                    bool hasPinnedLibsstvLandmarks)
{
    mode.catalogStatus = CatalogStatus::Catalogued;
    mode.rxStatus = CapabilityStatus::Implemented;
    mode.txStatus = CapabilityStatus::Implemented;
    mode.autoDetectStatus = CapabilityStatus::Implemented;
    mode.protocolDataComplete = true;

    SstvVisSpec vis;
    vis.encoding = VisEncoding::StandardSevenBit;
    vis.bitCount = 7U;
    vis.standardCode = visPayload;
    vis.lsbFirst = true;
    vis.parity = Parity::Even;
    mode.vis = std::move(vis);

    mode.geometry.imageWidth = 320U;
    mode.geometry.imageHeight = imageHeight;
    mode.geometry.sampledPixelWidth = sampledPixelWidth;
    mode.geometry.transmittedPixelWidth = 320U;
    mode.geometry.transmittedLineCount = imageHeight;
    mode.geometry.displayedLineCount = imageHeight;
    mode.geometry.linesPerScan = 1U;

    mode.colour.colourSpace = ColourSpace::Rgb;
    mode.colour.componentOrder = {
        ColourComponent::Green,
        ColourComponent::Blue,
        ColourComponent::Red
    };
    mode.colour.chromaSubsampling = ChromaSubsampling::Cs444;
    mode.colour.conversionRule =
        "Full-range RGB8; component value v maps to 1500 + 800*v/255 Hz.";

    mode.timing.syncFrequencyHz = 1'200U;
    mode.timing.syncDuration = Picoseconds {4'862'000'000LL};
    mode.timing.frontPorch = Picoseconds {572'000'000LL};
    mode.timing.backPorch = Picoseconds {572'000'000LL};
    mode.timing.separatorFrequencyHz = 1'500U;
    mode.timing.separatorDuration = Picoseconds {572'000'000LL};
    mode.timing.pixelDuration = pixelDuration;
    mode.timing.componentDuration = componentDuration;
    mode.timing.lineDuration = lineDuration;
    mode.timing.imageDuration = imageDuration;
    mode.timing.nominalAudioBandwidth = SstvAudioBandwidth {1'100U, 2'300U};
    mode.timing.tolerancePpm = 300U;

    mode.leaderHeaderRules =
        "1900 Hz for 300 ms, 1200 Hz for 10 ms, 1900 Hz for 300 ms; "
        "then standard seven-bit VIS, LSB first with even parity, framed "
        "by 1200 Hz and using 30 ms symbols (1100 Hz one, 1300 Hz zero).";
    mode.specialLineOrdering =
        "Each line is sync, 1500 Hz porch, 320 green pixels, "
        "1500 Hz separator, 320 blue pixels, separator, 320 red pixels, "
        "then a trailing 1500 Hz separator.";
    mode.fallbackSignature.nominalLineDuration =
        lineDuration;
    mode.fallbackSignature.nominalSyncDuration =
        Picoseconds {4'862'000'000LL};
    mode.fallbackSignature.syncFrequencyHz = 1'200U;
    mode.fallbackSignature.discriminator =
        "No mode claim from timing alone; a controller must supply VIS/manual "
        "selection and line-sync anchors.";

    mode.catalogueReferences.push_back(
        "SSTV Handbook, chapter 4 table 4.4 and chapter 5 VIS mode list");
    mode.protocolProvenance = {
        "SSTV Handbook table 4.4 specifies the Martin family GBR order, "
        "4.862 ms sync, 0.572 ms gaps, 320 transmitted/display columns, "
        "and distinguishes 320/160 effective sampled columns.",
        "The Handbook chapter 5 mode list records VIS 44/40/36/32, 256 "
        "rows for M1/M2 and 128 rows for M3/M4.",
        timingResolution ? timingResolution : "No timing conflict note.",
        hasPinnedLibsstvLandmarks
            ? "Developer-generated raw-PCM landmarks from the independent "
              "rimio/libsstv@193157a993ac34bfa074074004c9ddadcfe6fd15 "
              "encoder are recorded in tests/sstv/fixtures/"
              "libsstv-193157-martin-m2-m3-m4-landmarks.json."
            : "UC Berkeley EE123 Spring 2014 and the pinned audited "
              "implementations independently confirm Martin M1 timing.",
        "Behavior-only comparison: dnet/slowrx@a50a4e2c291d852a950f25e77d411e77efd9cd89 "
        "is retained with its M3 pixel-duration inconsistency explicitly "
        "resolved by the Handbook and libsstv.",
        "Behavior-only comparison: dnet/pySSTV@d998fad154d3e6ad2d73af5add49beec0d2ab59f "
        "uses an effective-width M2 raster; Decodium keeps that metadata "
        "separate from the 320-column wire/display raster.",
        "Recorded divergence: ON4QZ/QSSTV@8c27d6d169d8c6c197eb47c2089870e39bc06a02 "
        "rounds Martin sync to 5.000 ms; the Handbook's 4.862 ms controls."
    };
    mode.evidenceStatus = hasPinnedLibsstvLandmarks
        ? EvidenceStatus::IndependentVector
        : EvidenceStatus::DeterministicTests;
    mode.implementationEvidenceRefs = {
        "src/sstv/analog/SstvMartinM1.h",
        "src/sstv/analog/SstvMartinM1.cpp",
        "src/sstv/analog/SstvMartinM1RxSession.h",
        "src/sstv/analog/SstvMartinM1RxSession.cpp",
        "src/sstv/integration/SstvRxRuntime.cpp",
        "tests/sstv/test_sstv_martin_m1.cpp",
        "tests/sstv/test_sstv_martin_m1_rx_session.cpp",
        "tests/sstv/test_sstv_martin_family.cpp",
        "tests/sstv/test_sstv_martin_family_rx_session.cpp",
        "tests/sstv/test_sstv_rx_runtime.cpp",
        "tests/sstv/test_sstv_mode_registry.cpp"
    };
    if (hasPinnedLibsstvLandmarks) {
        mode.implementationEvidenceRefs.push_back(
            "tests/sstv/fixtures/"
            "libsstv-193157-martin-m2-m3-m4-landmarks.json");
    }
    mode.interoperabilityStatus = hasPinnedLibsstvLandmarks
        ? InteroperabilityStatus::UpstreamPathObserved
        : InteroperabilityStatus::NotTested;
    mode.fixtureStatus = hasPinnedLibsstvLandmarks
        ? FixtureStatus::Independent
        : FixtureStatus::SelfGeneratedOnly;
    mode.statusNote = hasPinnedLibsstvLandmarks
        ? "Native bounded Martin RX/TX and automatic VIS-to-session runtime "
          "selection are implemented. Deterministic tests consume compact "
          "landmarks from a separately executed pinned libsstv encoder. This "
          "is independent developer evidence, not live-radio, on-air or "
          "cross-application interoperability verification."
        : "Native bounded Martin M1 RX/TX and automatic VIS-to-session runtime "
          "selection are implemented with deterministic tests. Loopback is a "
          "self-test only; no independent waveform or on-air interoperability "
          "fixture has been verified.";
}

void populateScottie(SstvModeSpec& mode,
                     std::uint8_t visPayload,
                     std::uint32_t imageHeight,
                     std::uint32_t sampledPixelWidth,
                     Picoseconds pixelDuration,
                     Picoseconds componentDuration,
                     Picoseconds lineDuration,
                     Picoseconds imageDuration,
                     const char* timingResolution,
                     bool hasPinnedLibsstvLandmarks = false)
{
    mode.catalogStatus = CatalogStatus::Catalogued;
    mode.rxStatus = CapabilityStatus::Implemented;
    mode.txStatus = CapabilityStatus::Implemented;
    mode.autoDetectStatus = CapabilityStatus::Implemented;
    mode.protocolDataComplete = true;

    SstvVisSpec vis;
    vis.encoding = VisEncoding::StandardSevenBit;
    vis.bitCount = 7U;
    vis.standardCode = visPayload;
    vis.lsbFirst = true;
    vis.parity = Parity::Even;
    mode.vis = std::move(vis);

    mode.geometry.imageWidth = 320U;
    mode.geometry.imageHeight = imageHeight;
    mode.geometry.sampledPixelWidth = sampledPixelWidth;
    mode.geometry.transmittedPixelWidth = 320U;
    mode.geometry.transmittedLineCount = imageHeight;
    mode.geometry.displayedLineCount = imageHeight;
    mode.geometry.linesPerScan = 1U;

    mode.colour.colourSpace = ColourSpace::Rgb;
    mode.colour.componentOrder = {
        ColourComponent::Green,
        ColourComponent::Blue,
        ColourComponent::Red
    };
    mode.colour.chromaSubsampling = ChromaSubsampling::Cs444;
    mode.colour.conversionRule =
        "Full-range RGB8; component value v maps to 1500 + 800*v/255 Hz.";

    mode.timing.syncFrequencyHz = 1'200U;
    mode.timing.syncDuration = Picoseconds {9'000'000'000LL};
    mode.timing.frontPorch = Picoseconds {1'500'000'000LL};
    mode.timing.backPorch = Picoseconds {1'500'000'000LL};
    mode.timing.separatorFrequencyHz = 1'500U;
    mode.timing.separatorDuration = Picoseconds {1'500'000'000LL};
    mode.timing.pixelDuration = pixelDuration;
    mode.timing.componentDuration = componentDuration;
    mode.timing.lineDuration = lineDuration;
    mode.timing.imageDuration = imageDuration;
    mode.timing.nominalAudioBandwidth = SstvAudioBandwidth {1'100U, 2'300U};
    mode.timing.tolerancePpm = 300U;

    mode.leaderHeaderRules =
        "1900 Hz for 300 ms, 1200 Hz for 10 ms, 1900 Hz for 300 ms; "
        "then standard seven-bit VIS, LSB first with even parity, framed by "
        "1200 Hz and using 30 ms symbols.";
    mode.specialLineOrdering =
        "The first row starts immediately after the VIS stop: 1.5 ms porch, "
        "320 green pixels, 1.5 ms porch, 320 blue pixels, embedded 9 ms "
        "1200 Hz sync, 1.5 ms porch, then 320 red pixels. There is no "
        "invented line-leading sync before the first green component.";
    mode.fallbackSignature.nominalLineDuration = lineDuration;
    mode.fallbackSignature.nominalSyncDuration =
        Picoseconds {9'000'000'000LL};
    mode.fallbackSignature.syncFrequencyHz = 1'200U;
    mode.fallbackSignature.discriminator =
        "Scottie embedded sync follows green and blue; timing alone never "
        "overrides VIS or an explicit manual selection.";

    mode.catalogueReferences.push_back(
        "docs/sstv/UPSTREAM_PROVENANCE.md (pinned clean-room behaviour audit)");
    mode.catalogueReferences.push_back(
        "SSTV Handbook, chapter 4 table 4.5 and chapter 5 mode list");
    mode.protocolProvenance = {
        "SSTV Handbook table 4.5 specifies Scottie S1/S3 component timing "
        "138.240 ms, S2/S4 component timing 88.064 ms, 9 ms embedded sync, "
        "128 rows for S3/S4 and 256 rows for S1/S2/DX.",
        "The Handbook chapter 5 list records 320 wire/display columns for the "
        "whole Scottie family and VIS 60/56/52/48/76; table 4.5 separately "
        "describes S2/S4 effective horizontal resolution as 160.",
        "dnet/pySSTV@d998fad154d3e6ad2d73af5add49beec0d2ab59f "
        "is retained as a behaviour comparison, not as the Scottie S2 geometry "
        "oracle; four other audited lineages use 320 pixels.",
        timingResolution ? timingResolution : "No timing conflict note.",
        hasPinnedLibsstvLandmarks
            ? "Developer-generated raw-PCM landmarks from the independent "
              "rimio/libsstv@193157a993ac34bfa074074004c9ddadcfe6fd15 "
              "encoder are recorded in tests/sstv/fixtures/"
              "libsstv-193157-scottie-s3-s4-landmarks.json; the fixture also "
              "records libsstv's extra initial 9 ms Robot-1200C-style sync and "
              "does not misrepresent it as Decodium's selected first-line order."
            : "No separately executed upstream PCM landmark fixture for this "
              "mode is committed yet."
    };
    mode.evidenceStatus = hasPinnedLibsstvLandmarks
        ? EvidenceStatus::IndependentVector
        : EvidenceStatus::DeterministicTests;
    mode.implementationEvidenceRefs = {
        "src/sstv/analog/SstvScottie.h",
        "src/sstv/analog/SstvScottie.cpp",
        "src/sstv/analog/SstvScottieRxSession.h",
        "src/sstv/analog/SstvScottieRxSession.cpp",
        "src/sstv/integration/SstvRxRuntime.cpp",
        "tests/sstv/test_sstv_scottie.cpp",
        "tests/sstv/test_sstv_scottie_rx_session.cpp",
        "tests/sstv/test_sstv_rx_runtime.cpp",
        "tests/sstv/test_sstv_mode_registry.cpp"
    };
    if (hasPinnedLibsstvLandmarks) {
        mode.implementationEvidenceRefs.push_back(
            "tests/sstv/fixtures/"
            "libsstv-193157-scottie-s3-s4-landmarks.json");
    }
    mode.interoperabilityStatus = hasPinnedLibsstvLandmarks
        ? InteroperabilityStatus::UpstreamPathObserved
        : InteroperabilityStatus::NotTested;
    mode.fixtureStatus = hasPinnedLibsstvLandmarks
        ? FixtureStatus::Independent
        : FixtureStatus::SelfGeneratedOnly;
    mode.statusNote = hasPinnedLibsstvLandmarks
        ? "Native bounded Scottie RX/TX and automatic VIS-to-session runtime "
          "selection are implemented. Deterministic tests consume compact "
          "landmarks from a separately executed pinned libsstv encoder. This "
          "is independent developer evidence, not a live-radio, on-air or "
          "cross-application interoperability verification claim."
        : "Native bounded Scottie RX/TX and automatic VIS-to-session runtime "
          "selection are implemented with deterministic synthetic tests. The "
          "loopback is a self-test only; no independent waveform or on-air "
          "interoperability fixture has been verified.";
}

void populateRobot(SstvModeSpec& mode, const SstvRobotModeSpec& protocol)
{
    mode.catalogStatus = CatalogStatus::Catalogued;
    mode.rxStatus = CapabilityStatus::Implemented;
    mode.txStatus = CapabilityStatus::Implemented;
    mode.autoDetectStatus = CapabilityStatus::Implemented;
    mode.protocolDataComplete = true;

    SstvVisSpec vis;
    vis.encoding = VisEncoding::StandardSevenBit;
    vis.bitCount = 7U;
    vis.standardCode = protocol.visPayload;
    for (std::uint8_t index = 0U;
         index < protocol.visAliasCount;
         ++index) {
        vis.standardAliases.push_back(protocol.visAliases[index]);
    }
    vis.lsbFirst = true;
    vis.parity = Parity::Even;
    mode.vis = std::move(vis);

    mode.geometry.imageWidth = protocol.width;
    mode.geometry.imageHeight = protocol.height;
    mode.geometry.sampledPixelWidth = protocol.width;
    mode.geometry.transmittedPixelWidth = protocol.width;
    mode.geometry.transmittedLineCount = protocol.height;
    mode.geometry.displayedLineCount = protocol.height;
    mode.geometry.linesPerScan = 1U;

    mode.colour.colourSpace = protocol.colour
        ? ColourSpace::YCbCr : ColourSpace::Grayscale;
    mode.colour.componentOrder = protocol.colour
        ? std::vector<ColourComponent> {
              ColourComponent::Luminance,
              ColourComponent::ChrominanceRed,
              ColourComponent::ChrominanceBlue}
        : std::vector<ColourComponent> {ColourComponent::Gray};
    mode.colour.chromaSubsampling = protocol.chromaSubsampling;
    mode.colour.conversionRule = protocol.colour
        ? "Full-range BT.601 YCbCr. Chroma has half horizontal resolution; "
          "4:2:0 averages each 2x2 RGB source block and alternates Cr/Cb by "
          "line, while 4:2:2 averages each horizontal RGB pair per line. "
          "Every value v maps to 1500 + 800*v/255 Hz."
        : "Full-range BT.601 luminance; value v maps to "
          "1500 + 800*v/255 Hz and expands to neutral RGB.";

    mode.timing.syncFrequencyHz = 1'200U;
    mode.timing.syncDuration = protocol.syncDuration;
    mode.timing.frontPorch = Picoseconds {0LL};
    mode.timing.backPorch = Picoseconds {0LL};
    mode.timing.separatorFrequencyHz = protocol.colour ? 1'900U : 0U;
    mode.timing.separatorDuration = protocol.markerDuration;
    mode.timing.pixelDuration = protocol.luminancePixelDuration;
    mode.timing.componentDuration = protocol.luminanceDuration;
    mode.timing.lineDuration = protocol.lineDuration;
    mode.timing.imageDuration = protocol.imageDuration;
    mode.timing.nominalAudioBandwidth = SstvAudioBandwidth {1'100U, 2'300U};
    mode.timing.tolerancePpm = 300U;

    mode.leaderHeaderRules =
        "1900 Hz for 300 ms, 1200 Hz for 10 ms, 1900 Hz for 300 ms; "
        "then standard seven-bit VIS, LSB first with even parity, framed by "
        "1200 Hz and using 30 ms symbols. Robot B/W transmits the green VIS "
        "as canonical and accepts the documented red/blue aliases.";
    if (!protocol.colour) {
        mode.specialLineOrdering =
            "Each line is a 1200 Hz sync followed immediately by one "
            "full-range grayscale scan.";
    } else if (protocol.chromaSubsampling == ChromaSubsampling::Cs420) {
        mode.specialLineOrdering =
            "Each line is 1200 Hz sync, Y, then one marker and half-width "
            "chroma scan. Even rows carry Cr and odd rows Cb for a shared "
            "2x2 chroma block. A Cr marker is 1500 Hz for two thirds then "
            "1900 Hz for one third; Cb uses 2300 Hz then 1900 Hz.";
    } else {
        mode.specialLineOrdering =
            "Each line is 1200 Hz sync, Y, Cr marker, half-width Cr, Cb "
            "marker, half-width Cb. Marker frequencies are 1500/2300 Hz "
            "for two thirds followed by 1900 Hz for one third.";
    }
    mode.fallbackSignature.nominalLineDuration = protocol.lineDuration;
    mode.fallbackSignature.nominalSyncDuration = protocol.syncDuration;
    mode.fallbackSignature.syncFrequencyHz = 1'200U;
    mode.fallbackSignature.discriminator =
        "Timing is only a reacquisition aid; VIS or explicit manual selection "
        "remains authoritative across Robot modes and B/W aliases.";

    mode.catalogueReferences.push_back(
        "SSTV Handbook, chapter 4 tables 4.1/4.3 and chapter 5 VIS list");
    mode.catalogueReferences.push_back(
        "docs/sstv/UPSTREAM_PROVENANCE.md (pinned clean-room behaviour audit)");
    mode.protocolProvenance = {
        "SSTV Handbook table 4.3 controls Robot colour geometry, Y/Cr/Cb "
        "timing and 4:2:0/4:2:2 classification; table 4.1 controls B/W "
        "geometry, sync and scan durations.",
        "SSTV Handbook chapter 5 records colour VIS 0/4/8/12 and the Robot "
        "B/W red/green/blue alias groups 1-3, 5-7, 9-11 and 13-15.",
        "rimio/libsstv@193157a993ac34bfa074074004c9ddadcfe6fd15 "
        "independently confirms all requested image geometries, B/W component "
        "timings, VIS alias identities and alternating/full Robot encoders.",
        "Recorded divergence: pinned libsstv represents chroma as a full-width "
        "scan at half pixel duration and uses 9 ms sync plus extra porch/marker "
        "segments for colour. Decodium follows the Handbook structural sums "
        "and equivalent effective half-width chroma sampling.",
        "Recorded divergence: ON4QZ/QSSTV@8c27d6d169d8c6c197eb47c2089870e39bc06a02 "
        "declares Robot 24 as 160 columns and uses implementation-specific "
        "porches; the Handbook and pinned libsstv 320x120 geometry control.",
        "Robot B/W 24 uses the Handbook/libsstv 12 ms sync plus 93 ms scan. "
        "The resulting 105 ms structural line conflicts with the historical "
        "24 name/600 lpm column and is retained rather than silently changed.",
        "Developer-generated raw-PCM landmarks from the separately executed "
        "pinned libsstv encoder are recorded in tests/sstv/fixtures/"
        "libsstv-193157-robot-landmarks.json; incompatible colour timing is "
        "labelled as reference behaviour rather than an on-air oracle."
    };
    mode.evidenceStatus = EvidenceStatus::IndependentVector;
    mode.implementationEvidenceRefs = {
        "src/sstv/analog/SstvRobot.h",
        "src/sstv/analog/SstvRobot.cpp",
        "src/sstv/analog/SstvRobotRxSession.h",
        "src/sstv/analog/SstvRobotRxSession.cpp",
        "src/sstv/integration/SstvRxRuntime.cpp",
        "tests/sstv/test_sstv_robot.cpp",
        "tests/sstv/test_sstv_robot_rx_session.cpp",
        "tests/sstv/test_sstv_rx_runtime.cpp",
        "tests/sstv/test_sstv_mode_registry.cpp",
        "tests/sstv/fixtures/libsstv-193157-robot-landmarks.json"
    };
    mode.interoperabilityStatus = InteroperabilityStatus::UpstreamPathObserved;
    mode.fixtureStatus = FixtureStatus::Independent;
    mode.statusNote =
        "Native bounded Robot RX/TX and automatic VIS selection are "
        "implemented. Deterministic tests consume compact landmarks from a "
        "separately executed pinned libsstv encoder and explicitly isolate "
        "known colour-layout divergences. This is independent developer "
        "evidence, not live-radio, on-air or cross-application verification.";
}

void populateSequentialRgb(
    SstvModeSpec& mode,
    const SstvSequentialRgbModeSpec& protocol,
    bool hasPinnedPysstvLandmarks)
{
    mode.catalogStatus = CatalogStatus::Catalogued;
    mode.rxStatus = CapabilityStatus::Implemented;
    mode.txStatus = CapabilityStatus::Implemented;
    mode.autoDetectStatus = CapabilityStatus::Implemented;
    mode.protocolDataComplete = true;

    SstvVisSpec vis;
    vis.encoding = VisEncoding::StandardSevenBit;
    vis.bitCount = 7U;
    vis.standardCode = protocol.visPayload;
    vis.lsbFirst = true;
    vis.parity = Parity::Even;
    mode.vis = std::move(vis);

    mode.geometry.imageWidth = protocol.width;
    mode.geometry.imageHeight = protocol.height;
    mode.geometry.sampledPixelWidth = protocol.effectiveSampledWidth;
    mode.geometry.transmittedPixelWidth = protocol.width;
    mode.geometry.transmittedLineCount = protocol.height;
    mode.geometry.displayedLineCount = protocol.height;
    mode.geometry.linesPerScan = 1U;

    mode.colour.colourSpace = ColourSpace::Rgb;
    mode.colour.componentOrder = {
        ColourComponent::Red,
        ColourComponent::Green,
        ColourComponent::Blue};
    mode.colour.chromaSubsampling = ChromaSubsampling::Cs444;
    mode.colour.conversionRule =
        "Full-range RGB8; each component value v maps to "
        "1500 + 800*v/255 Hz.";

    mode.timing.syncFrequencyHz = 1'200U;
    mode.timing.syncDuration = protocol.syncDuration;
    mode.timing.frontPorch = protocol.gapDurations[3];
    mode.timing.backPorch = protocol.gapDurations[0];
    mode.timing.separatorFrequencyHz = 1'500U;
    mode.timing.separatorDuration = protocol.gapDurations[1];
    mode.timing.pixelDuration = protocol.pixelDuration;
    mode.timing.componentDuration = protocol.componentDuration;
    mode.timing.lineDuration = protocol.lineDuration;
    mode.timing.imageDuration = protocol.imageDuration;
    mode.timing.nominalAudioBandwidth = SstvAudioBandwidth {1'100U, 2'300U};
    mode.timing.tolerancePpm = 300U;

    mode.leaderHeaderRules =
        "1900 Hz for 300 ms, 1200 Hz for 10 ms, 1900 Hz for 300 ms; "
        "then standard seven-bit VIS, LSB first with even parity, framed "
        "by 1200 Hz and using 30 ms symbols.";
    mode.specialLineOrdering =
        std::string("Each row is a line-leading 1200 Hz sync, a 1500 Hz ")
        + "black gap, red pixels, black gap, green pixels, black gap, "
          "blue pixels and the configured trailing black gap. Profile: "
        + (protocol.compatibilityProfile
               ? protocol.compatibilityProfile : "unspecified");
    mode.fallbackSignature.nominalLineDuration = protocol.lineDuration;
    mode.fallbackSignature.nominalSyncDuration = protocol.syncDuration;
    mode.fallbackSignature.syncFrequencyHz = 1'200U;
    mode.fallbackSignature.discriminator =
        "Line timing is a reacquisition aid only; standard VIS or explicit "
        "manual selection remains authoritative.";

    mode.catalogueReferences.push_back(
        "SSTV Handbook chapter 4 tables 4.7/4.8 and chapter 5 mode list");
    mode.catalogueReferences.push_back(
        "docs/sstv/UPSTREAM_PROVENANCE.md (pinned clean-room behaviour audit)");
    mode.protocolProvenance = {
        "SSTV Handbook chapter 5 controls standard VIS 59/63/55 for "
        "SC2-60/120/180 and 113/114/115 for Pasokon P3/P5/P7.",
        "The Handbook mode list, SlowRX and pySSTV distinguish effective "
        "sampled resolution from the 320-column SC2 and 640-column Pasokon "
        "wire/display rasters; P3 is retained as 320 effective samples.",
        "Pasokon uses 4800/3200/2400 Hz time-unit-derived cumulative "
        "boundaries, a 25-unit sync, four 5-unit black gaps and three "
        "640-unit RGB scans. The Handbook table values, SlowRX and pySSTV "
        "agree; the Handbook prose's conflicting 20-unit sync is excluded.",
        "Wraase SC2-120 selects the executed pySSTV equal-RGB 475.5225 ms "
        "compatibility profile, which agrees at line level with SlowRX and "
        "QSSTV. It does not silently combine the Handbook's conflicting "
        "rounded 2:4:2 component table.",
        "Wraase SC2-180 selects pySSTV/SlowRX's 711.0225 ms line. SlowRX's "
        "listed 0.734532 ms pixel is internally inconsistent with that line; "
        "the coherent 235/320 ms cumulative pixel map controls.",
        "Wraase SC2-60 selects QSSTV's RX-side 61.5435 s / 256-line "
        "equal-RGB profile. QSSTV's TX-side porches/scans differ, and no "
        "second executable pinned implementation or independent waveform "
        "exists; no QSSTV-TX or interoperability equivalence is claimed."
    };
    mode.implementationEvidenceRefs = {
        "src/sstv/analog/SstvSequentialRgb.h",
        "src/sstv/analog/SstvSequentialRgb.cpp",
        "src/sstv/analog/SstvSequentialRgbRxSession.h",
        "src/sstv/analog/SstvSequentialRgbRxSession.cpp",
        "src/sstv/integration/SstvRxRuntime.cpp",
        "src/sstv/integration/SstvTxCoordinator.cpp",
        "src/sstv/integration/SstvStudioController.cpp",
        "tests/sstv/test_sstv_sequential_rgb.cpp",
        "tests/sstv/test_sstv_sequential_rgb_rx_session.cpp",
        "tests/sstv/test_sstv_rx_runtime.cpp",
        "tests/sstv/test_sstv_tx_coordinator.cpp",
        "tests/sstv/test_sstv_wav_exporter.cpp",
        "tests/sstv/test_sstv_studio_controller.cpp",
        "tests/sstv/test_sstv_mode_registry.cpp"};
    if (hasPinnedPysstvLandmarks) {
        mode.implementationEvidenceRefs.push_back(
            "tests/sstv/fixtures/"
            "pysstv-d998fad-sequential-rgb-landmarks.json");
    }
    mode.evidenceStatus = hasPinnedPysstvLandmarks
        ? EvidenceStatus::IndependentVector
        : EvidenceStatus::DeterministicTests;
    mode.interoperabilityStatus = hasPinnedPysstvLandmarks
        ? InteroperabilityStatus::UpstreamPathObserved
        : InteroperabilityStatus::NotTested;
    mode.fixtureStatus = hasPinnedPysstvLandmarks
        ? FixtureStatus::Independent
        : FixtureStatus::SelfGeneratedOnly;
    mode.statusNote = hasPinnedPysstvLandmarks
        ? "Native bounded RX/TX and automatic VIS selection are implemented. "
          "Tests consume timing landmarks from a separately executed pinned "
          "MIT pySSTV path; this is developer evidence, not live-radio or "
          "cross-application interoperability verification."
        : "Native bounded RX/TX and automatic VIS selection are implemented "
          "for the explicit QSSTV compatibility profile. Only deterministic "
          "tests exist; no independent waveform, live-radio or cross-application "
          "interoperability verification is claimed.";
}

void populatePd(SstvModeSpec& mode,
                const SstvPdModeSpec& protocol,
                bool hasPinnedPysstvLandmarks)
{
    mode.catalogStatus = CatalogStatus::Catalogued;
    mode.rxStatus = CapabilityStatus::Implemented;
    mode.txStatus = CapabilityStatus::Implemented;
    mode.autoDetectStatus = CapabilityStatus::Implemented;
    mode.protocolDataComplete = true;

    SstvVisSpec vis;
    vis.encoding = VisEncoding::StandardSevenBit;
    vis.bitCount = 7U;
    vis.standardCode = protocol.visPayload;
    vis.lsbFirst = true;
    vis.parity = Parity::Even;
    mode.vis = std::move(vis);

    mode.geometry.imageWidth = protocol.width;
    mode.geometry.imageHeight = protocol.height;
    mode.geometry.sampledPixelWidth = protocol.width;
    mode.geometry.transmittedPixelWidth = protocol.width;
    mode.geometry.transmittedLineCount = protocol.height;
    mode.geometry.displayedLineCount = protocol.height;
    mode.geometry.linesPerScan = 2U;

    mode.colour.colourSpace = ColourSpace::YCbCr;
    mode.colour.componentOrder = {
        ColourComponent::Luminance,
        ColourComponent::ChrominanceRed,
        ColourComponent::ChrominanceBlue,
        ColourComponent::Luminance};
    mode.colour.chromaSubsampling = ChromaSubsampling::Cs440;
    mode.colour.conversionRule =
        "Full-range BT.601/JPEG YCbCr. For each vertical row pair, transmit "
        "Y-even, floor-average Cr, floor-average Cb, then Y-odd; value v "
        "maps to 1500 + 800*v/255 Hz.";

    mode.timing.syncFrequencyHz = 1'200U;
    mode.timing.syncDuration = protocol.syncDuration;
    mode.timing.frontPorch = Picoseconds {0};
    mode.timing.backPorch = protocol.porchDuration;
    mode.timing.separatorFrequencyHz = 1'500U;
    mode.timing.separatorDuration = Picoseconds {0};
    mode.timing.pixelDuration = protocol.pixelDuration;
    mode.timing.componentDuration = protocol.componentDuration;
    mode.timing.lineDuration = protocol.linePairDuration;
    mode.timing.imageDuration = protocol.imageDuration;
    mode.timing.nominalAudioBandwidth = SstvAudioBandwidth {1'100U, 2'300U};
    mode.timing.tolerancePpm = 300U;

    mode.leaderHeaderRules =
        "1900 Hz for 300 ms, 1200 Hz for 10 ms, 1900 Hz for 300 ms; "
        "then standard seven-bit VIS, LSB first with even parity, framed "
        "by 1200 Hz and using 30 ms symbols.";
    mode.specialLineOrdering =
        "Each radio scan represents exactly two destination rows: 20 ms "
        "1200 Hz sync, 2.08 ms 1500 Hz porch, Y-even, vertical-average Cr, "
        "vertical-average Cb, Y-odd. Stop after height/2 scan pairs; no "
        "trailing extra pair is permitted.";
    mode.fallbackSignature.nominalLineDuration = protocol.linePairDuration;
    mode.fallbackSignature.nominalSyncDuration = protocol.syncDuration;
    mode.fallbackSignature.syncFrequencyHz = 1'200U;
    mode.fallbackSignature.discriminator =
        "Pair timing is a bounded reacquisition aid only; standard VIS or "
        "explicit manual mode selection remains authoritative.";

    mode.catalogueReferences.push_back(
        "SSTV Handbook chapter 4 PD table and chapter 5 VIS mode list");
    mode.catalogueReferences.push_back(
        "docs/sstv/UPSTREAM_PROVENANCE.md (pinned clean-room behaviour audit)");
    mode.protocolProvenance = {
        "The SSTV Handbook, QSSTV, SlowRX, pySSTV and libsstv agree on the "
        "20 ms sync, 2.08 ms porch, Y-even/Cr/Cb/Y-odd pair order and VIS "
        "93/99/95/98/96/97/94 for PD50/90/120/160/180/240/290.",
        "Executable implementations retain the full transmitted rasters "
        "320x256, 640x496, 512x400 and 800x616, including calibration rows; "
        "the Handbook's smaller visible-height prose is display metadata, "
        "not a license to truncate the waveform.",
        "PD160 uses the coherent 512 * 382 us = 195.584 ms component and "
        "804.416 ms pair. The Handbook's isolated rounded 195.854 ms entry "
        "conflicts with its pixel width and total pair and is not used.",
        "Pinned pySSTV landmarks cover PD90 through PD290. Pinned libsstv "
        "landmarks cover all seven modes up to the canonical end boundary.",
        "The executed libsstv path starts one additional scan pair after the "
        "last valid row and reads beyond its input raster. Decodium bounds "
        "the mapper to height/2 pairs and tests the defect as a non-oracle; "
        "the trailing libsstv frames are never reproduced."
    };
    mode.evidenceStatus = EvidenceStatus::IndependentVector;
    mode.implementationEvidenceRefs = {
        "src/sstv/analog/SstvPd.h",
        "src/sstv/analog/SstvPd.cpp",
        "src/sstv/analog/SstvPdRxSession.h",
        "src/sstv/analog/SstvPdRxSession.cpp",
        "src/sstv/integration/SstvRxRuntime.cpp",
        "src/sstv/integration/SstvTxCoordinator.cpp",
        "src/sstv/integration/SstvStudioController.cpp",
        "tests/sstv/test_sstv_pd.cpp",
        "tests/sstv/test_sstv_pd_rx_session.cpp",
        "tests/sstv/test_sstv_rx_runtime.cpp",
        "tests/sstv/test_sstv_tx_coordinator.cpp",
        "tests/sstv/test_sstv_wav_exporter.cpp",
        "tests/sstv/test_sstv_studio_controller.cpp",
        "tests/sstv/test_sstv_mode_registry.cpp",
        "tests/sstv/fixtures/libsstv-193157-pd-landmarks.json"};
    if (hasPinnedPysstvLandmarks) {
        mode.implementationEvidenceRefs.push_back(
            "tests/sstv/fixtures/pysstv-d998fad-pd-landmarks.json");
    }
    mode.interoperabilityStatus = InteroperabilityStatus::UpstreamPathObserved;
    mode.fixtureStatus = FixtureStatus::Independent;
    mode.statusNote = hasPinnedPysstvLandmarks
        ? "Native bounded RX/TX and automatic VIS selection are implemented. "
          "Tests consume independent pinned pySSTV and pre-defect libsstv "
          "landmarks. This is developer evidence, not live-radio or "
          "cross-application interoperability verification."
        : "Native bounded RX/TX and automatic VIS selection are implemented. "
          "PD50 has independent pre-defect libsstv landmarks but no pySSTV "
          "vector; the known libsstv extra-pair/OOB defect is explicitly "
          "excluded. No live-radio interoperability is claimed.";
}

void populateMmsstvExtended(SstvModeSpec& mode,
                            const SstvMmsstvModeSpec& protocol)
{
    mode.catalogStatus = CatalogStatus::Catalogued;
    mode.rxStatus = CapabilityStatus::Implemented;
    mode.txStatus = CapabilityStatus::Implemented;
    mode.autoDetectStatus = CapabilityStatus::Implemented;
    mode.protocolDataComplete = true;

    SstvVisSpec vis;
    vis.bitCount = protocol.narrow ? 24U : 16U;
    vis.lsbFirst = true;
    if (protocol.narrow) {
        vis.encoding = VisEncoding::Narrow24Bit;
        vis.parity = Parity::None;
        vis.extendedSequence = {
            0x2dU,
            0x15U,
            protocol.visWireCode,
            static_cast<std::uint8_t>(0x15U ^ protocol.visWireCode)};
    } else {
        vis.encoding = VisEncoding::Extended;
        vis.parity = Parity::Odd;
        vis.extendedSequence = {
            0x23U,
            static_cast<std::uint8_t>(protocol.visWireCode & 0x7fU)};
    }
    mode.vis = std::move(vis);

    mode.geometry.imageWidth = protocol.width;
    mode.geometry.imageHeight = protocol.height;
    mode.geometry.sampledPixelWidth = protocol.width;
    mode.geometry.transmittedPixelWidth = protocol.width;
    mode.geometry.transmittedLineCount = protocol.height;
    mode.geometry.displayedLineCount = protocol.height;
    mode.geometry.linesPerScan = protocol.linesPerScan;

    if (protocol.layout == SstvMmsstvLayout::McSequentialRgb) {
        mode.colour.colourSpace = ColourSpace::Rgb;
        mode.colour.componentOrder = {
            ColourComponent::Red,
            ColourComponent::Green,
            ColourComponent::Blue};
        mode.colour.chromaSubsampling = ChromaSubsampling::Cs444;
        mode.colour.conversionRule =
            "Full-range RGB8; value v maps to 2044 + 256*v/255 Hz. ";
    } else if (protocol.layout
               == SstvMmsstvLayout::MpPairedYCbCr) {
        mode.colour.colourSpace = ColourSpace::YCbCr;
        mode.colour.componentOrder = {
            ColourComponent::Luminance,
            ColourComponent::ChrominanceRed,
            ColourComponent::ChrominanceBlue,
            ColourComponent::Luminance};
        mode.colour.chromaSubsampling = ChromaSubsampling::Cs440;
        mode.colour.conversionRule =
            "Full-range BT.601/JPEG YCbCr. Each scan carries Y-even, "
            "vertical floor-average Cr, vertical floor-average Cb, Y-odd; ";
    } else {
        mode.colour.colourSpace = ColourSpace::YCbCr;
        mode.colour.componentOrder = {
            ColourComponent::Luminance,
            ColourComponent::ChrominanceRed,
            ColourComponent::ChrominanceBlue};
        mode.colour.chromaSubsampling = ChromaSubsampling::Cs422;
        mode.colour.conversionRule =
            "Full-range BT.601/JPEG YCbCr. Luminance has full horizontal "
            "resolution; Cr and Cb each floor-average adjacent pixel pairs; ";
    }
    if (protocol.layout != SstvMmsstvLayout::McSequentialRgb) {
        mode.colour.conversionRule += protocol.narrow
            ? "value v maps to 2044 + 256*v/255 Hz."
            : "value v maps to 1500 + 800*v/255 Hz.";
    }

    mode.timing.syncFrequencyHz = static_cast<std::uint32_t>(
        protocol.syncFrequencyHz);
    mode.timing.syncDuration = protocol.syncDuration;
    mode.timing.frontPorch = protocol.porchDuration;
    mode.timing.backPorch = Picoseconds {0};
    mode.timing.separatorFrequencyHz = static_cast<std::uint32_t>(
        protocol.porchFrequencyHz);
    mode.timing.separatorDuration = protocol.porchDuration;
    mode.timing.pixelDuration = Picoseconds {
        protocol.primaryComponentDuration.count
        / static_cast<std::int64_t>(protocol.width)};
    mode.timing.componentDuration = protocol.primaryComponentDuration;
    mode.timing.lineDuration = protocol.scanDuration;
    mode.timing.imageDuration = protocol.imageDuration;
    mode.timing.nominalAudioBandwidth = protocol.narrow
        ? SstvAudioBandwidth {1'900U, 2'300U}
        : SstvAudioBandwidth {1'100U, 2'300U};
    mode.timing.tolerancePpm = 300U;

    mode.leaderHeaderRules = protocol.narrow
        ? "Dedicated N-VIS: 1900 Hz 300 ms, 2100 Hz 100 ms, 1900 Hz "
          "22 ms, then four six-bit LSB-first groups 0x2d, 0x15, payload, "
          "0x15 xor payload at 22 ms/bit (one=1900 Hz, zero=2100 Hz)."
        : "Wide extended VIS: standard 1900/1200/1900 leader, then one "
          "odd-parity raw 0x23 marker and one odd-parity extension octet, "
          "LSB first at 30 ms/symbol with 1200 Hz start/stop.";
    switch (protocol.layout) {
    case SstvMmsstvLayout::MpPairedYCbCr:
        mode.specialLineOrdering =
            "Each scan starts with sync and porch and carries two rows as "
            "Y-even, vertical-average Cr, vertical-average Cb, Y-odd.";
        break;
    case SstvMmsstvLayout::MrHorizontal422:
        mode.specialLineOrdering =
            "Each scan starts with sync and porch, then full-width Y, a "
            "0.1 ms last-value hold, half-horizontal Cr plus hold, and "
            "half-horizontal Cb plus hold.";
        break;
    case SstvMmsstvLayout::McSequentialRgb:
        mode.specialLineOrdering =
            "Each narrow scan starts with 8 ms 1900 Hz sync and 0.5 ms "
            "2044 Hz porch, then full-width red, green and blue.";
        break;
    }
    mode.fallbackSignature.nominalLineDuration = protocol.scanDuration;
    mode.fallbackSignature.nominalSyncDuration = protocol.syncDuration;
    mode.fallbackSignature.syncFrequencyHz = static_cast<std::uint32_t>(
        protocol.syncFrequencyHz);
    mode.fallbackSignature.discriminator =
        "Scan timing is a bounded reacquisition aid only; valid wide VIS, "
        "valid N-VIS, or explicit manual selection remains authoritative.";

    mode.catalogueReferences.push_back(
        "tests/sstv/fixtures/mmsstv-8060b5-extended-mode-landmarks.json");
    mode.catalogueReferences.push_back(
        "docs/sstv/UPSTREAM_PROVENANCE.md (pinned clean-room behaviour audit)");
    mode.protocolProvenance = {
        "Clean-room protocol landmarks were audited from MMSSTV mirror "
        "8060b5f1e9727b0052d74108081c6db7b26babad and independently "
        "cross-checked against QSSTV "
        "8c27d6d169d8c6c197eb47c2089870e39bc06a02.",
        "No MMSSTV implementation code is copied. The fixture records only "
        "mode identities, geometry, physical headers, scan ordering, "
        "frequencies and exact timing landmarks.",
        protocol.mode == SstvMmsstvMode::Mr175
            ? "MR175 uses raw extension 0x4c as transmitted by original "
              "MMSSTV and listed by mode.txt/Handbook; QSSTV's duplicate "
              "0x4a table entry is documented as a typo and is not accepted."
            : "Wide extension and N-VIS identifiers are unique in the "
              "canonical native registry.",
        protocol.mode == SstvMmsstvMode::Mc110Narrow
            ? "MC110 uses the MMSSTV executable's exact 140 ms component "
              "timing. The 143 ms mode.txt prose conflicts with executable "
              "behaviour; QSSTV's quantised 428.52734375 ms line supports "
              "the selected 428.5 ms profile."
            : "Exact scan duration is the sum of its audited structural "
              "segments; cumulative rational sample mapping avoids drift.",
        "The pinned fixture is a source-landmark oracle, not an independent "
        "PCM capture, live-radio result or cross-application interoperability "
        "proof."
    };
    mode.evidenceStatus = EvidenceStatus::DeterministicTests;
    mode.implementationEvidenceRefs = {
        "src/sstv/analog/SstvMmsstvExtended.h",
        "src/sstv/analog/SstvMmsstvExtended.cpp",
        "src/sstv/analog/SstvMmsstvExtendedRxSession.h",
        "src/sstv/analog/SstvMmsstvExtendedRxSession.cpp",
        "src/sstv/core/SstvNarrowVisCodec.cpp",
        "src/sstv/rx/SstvNarrowVisDetector.cpp",
        "src/sstv/integration/SstvRxRuntime.cpp",
        "src/sstv/integration/SstvTxCoordinator.cpp",
        "src/sstv/integration/SstvStudioController.cpp",
        "tests/sstv/test_sstv_mmsstv_extended.cpp",
        "tests/sstv/test_sstv_narrow_vis_detector.cpp",
        "tests/sstv/test_sstv_rx_runtime.cpp",
        "tests/sstv/test_sstv_tx_coordinator.cpp",
        "tests/sstv/test_sstv_wav_exporter.cpp",
        "tests/sstv/test_sstv_studio_controller.cpp",
        "tests/sstv/test_sstv_mode_registry.cpp",
        "tests/sstv/fixtures/mmsstv-8060b5-extended-mode-landmarks.json"
    };
    mode.interoperabilityStatus = InteroperabilityStatus::UpstreamPathObserved;
    mode.fixtureStatus = FixtureStatus::Independent;
    mode.statusNote =
        "Native bounded RX/TX, wide extended-VIS/N-VIS auto-selection, "
        "shared SoundOutput/WAV and Studio integration are implemented. "
        "Deterministic tests consume pinned source landmarks; no independent "
        "PCM, live-radio or cross-application interoperability verification "
        "is claimed.";
}

void populateAvt(SstvModeSpec& mode, const SstvAvtModeSpec& protocol)
{
    mode.catalogStatus = CatalogStatus::Catalogued;
    mode.rxStatus = CapabilityStatus::Implemented;
    mode.txStatus = CapabilityStatus::Implemented;
    mode.autoDetectStatus = CapabilityStatus::Implemented;
    mode.protocolDataComplete = true;

    SstvVisSpec vis;
    vis.encoding = VisEncoding::StandardSevenBit;
    vis.bitCount = 7U;
    vis.standardCode = protocol.visPayload;
    vis.lsbFirst = true;
    vis.parity = Parity::Even;
    mode.vis = std::move(vis);

    mode.geometry.imageWidth = protocol.width;
    mode.geometry.imageHeight = protocol.height;
    mode.geometry.sampledPixelWidth = protocol.effectiveSampledWidth;
    mode.geometry.transmittedPixelWidth = protocol.width;
    mode.geometry.transmittedLineCount = protocol.height;
    mode.geometry.displayedLineCount = protocol.height;
    mode.geometry.linesPerScan = 1U;

    mode.colour.colourSpace = ColourSpace::Rgb;
    mode.colour.componentOrder = {
        ColourComponent::Red,
        ColourComponent::Green,
        ColourComponent::Blue};
    mode.colour.chromaSubsampling = ChromaSubsampling::Cs444;
    mode.colour.conversionRule =
        "Full-range RGB8; component value v maps to 1500 + 800*v/255 Hz.";

    // AVT intentionally has no physical per-line sync, porch or separator.
    // Explicit zero durations distinguish that protocol fact from missing
    // catalogue data.
    mode.timing.syncDuration = Picoseconds {0};
    mode.timing.frontPorch = Picoseconds {0};
    mode.timing.backPorch = Picoseconds {0};
    mode.timing.separatorDuration = Picoseconds {0};
    mode.timing.pixelDuration = Picoseconds {
        protocol.componentDuration.count
        / static_cast<std::int64_t>(protocol.width)};
    mode.timing.componentDuration = protocol.componentDuration;
    mode.timing.lineDuration = protocol.lineDuration;
    mode.timing.imageDuration = protocol.imageDuration;
    mode.timing.nominalAudioBandwidth = SstvAudioBandwidth {1'100U, 2'300U};
    mode.timing.tolerancePpm = 300U;

    mode.leaderHeaderRules =
        "Three complete standard 910 ms VIS headers, each with the normal "
        "1900/1200/1900 leader and seven-bit even-parity payload, followed "
        "by a protected 32x17-symbol digital countdown at exactly 102.4 "
        "baud (5.3125 s). Image time begins at the cumulative countdown end.";
    mode.specialLineOrdering =
        "Continuous red, green, blue components for every row, with no "
        "per-line sync, porch, separator or gap. AVT90 keeps 256 effective "
        "sampled columns as metadata while preparing and transmitting the "
        "audited common 320-column raster.";
    mode.fallbackSignature.nominalLineDuration = protocol.lineDuration;
    mode.fallbackSignature.discriminator =
        "No timing-only or line-sync fallback exists. A complete normal "
        "VIS identity plus a valid protected countdown, or explicit manual "
        "normal-mode selection, is required.";

    mode.catalogueReferences.push_back(
        "tests/sstv/fixtures/avt-handbook-qsstv-landmarks.json");
    mode.catalogueReferences.push_back("docs/sstv/AVT_PROTOCOL.md");
    mode.catalogueReferences.push_back(
        "docs/sstv/UPSTREAM_PROVENANCE.md (pinned clean-room behaviour audit)");
    mode.protocolProvenance = {
        "The SSTV Handbook PDF pinned by SHA-256 "
        "e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d "
        "defines the triple standard VIS header, protected AVT countdown "
        "and continuous no-line-sync RGB scan.",
        "Clean-room source-landmark audits pin QSSTV commit "
        "8c27d6d169d8c6c197eb47c2089870e39bc06a02 and MMSSTV mirror "
        "8060b5f1e9727b0052d74108081c6db7b26babad; no implementation code "
        "from either project is copied.",
        protocol.mode == SstvAvtMode::Avt90
            ? "AVT90 uses countdown prefix 101. The audited MMSSTV path's "
              "010 prefix conflicts with its mode identity and is recorded "
              "as a defect, not reproduced. Its 256 effective columns remain "
              "distinct from the observed 320-column prepared/wire raster."
            : "The normal countdown prefix and standard VIS payload are "
              "kept as separate protected identities and tested exactly.",
        "The pinned fixture records documentary and source landmarks only; "
        "it is not an independent PCM capture, live-radio result or "
        "cross-application interoperability proof."
    };
    mode.evidenceStatus = EvidenceStatus::DeterministicTests;
    mode.implementationEvidenceRefs = {
        "src/sstv/analog/SstvAvt.h",
        "src/sstv/analog/SstvAvt.cpp",
        "src/sstv/analog/SstvAvtRxSession.h",
        "src/sstv/analog/SstvAvtRxSession.cpp",
        "src/sstv/integration/SstvRxRuntime.cpp",
        "src/sstv/integration/SstvTxCoordinator.cpp",
        "src/sstv/integration/SstvStudioController.cpp",
        "tests/sstv/test_sstv_avt.cpp",
        "tests/sstv/test_sstv_avt_rx_session.cpp",
        "tests/sstv/test_sstv_rx_runtime.cpp",
        "tests/sstv/test_sstv_tx_coordinator.cpp",
        "tests/sstv/test_sstv_wav_exporter.cpp",
        "tests/sstv/test_sstv_studio_controller.cpp",
        "tests/sstv/test_sstv_mode_registry.cpp",
        "tests/sstv/fixtures/avt-handbook-qsstv-landmarks.json"
    };
    mode.interoperabilityStatus = InteroperabilityStatus::NotTested;
    mode.fixtureStatus = FixtureStatus::SelfGeneratedOnly;
    mode.statusNote =
        "Native bounded normal-mode RX/TX, protected-countdown auto-selection, "
        "shared SoundOutput/WAV and Studio integration are implemented. "
        "Deterministic loopback and source-landmark tests are not independent "
        "PCM, live-radio or cross-application verification.";
}

void populateAvtCatalogueVariant(SstvModeSpec& mode,
                                 SstvAvtMode avtMode,
                                 SstvAvtVariant variant)
{
    SstvVisSpec vis;
    vis.encoding = VisEncoding::StandardSevenBit;
    vis.bitCount = 7U;
    vis.standardCode = SstvAvtSyncCodec::visPayload(avtMode, variant);
    vis.lsbFirst = true;
    vis.parity = Parity::Even;
    mode.vis = std::move(vis);
    mode.catalogueReferences.push_back(
        "tests/sstv/fixtures/avt-handbook-qsstv-landmarks.json");
    mode.catalogueReferences.push_back("docs/sstv/AVT_PROTOCOL.md");
    mode.protocolProvenance = {
        "The standard VIS identity and its Narrow/QRM flags were audited "
        "from pinned documentary/source landmarks.",
        "Complete variant countdown, carrier treatment and picture semantics "
        "have not been established independently, so no executable RX, TX "
        "or auto-detect capability is claimed."
    };
    mode.evidenceStatus = EvidenceStatus::AuditedSources;
    mode.interoperabilityStatus = InteroperabilityStatus::NotTested;
    mode.fixtureStatus = FixtureStatus::Missing;
    mode.protocolDataComplete = false;
    mode.statusNote =
        "Catalogue-only AVT identity: the standard VIS header is known, but "
        "complete variant semantics and independent evidence are missing. "
        "RX, TX and auto-detect remain deliberately unimplemented.";
}

void addIssue(std::vector<ModeValidationIssue>& issues,
              ModeValidationCode code,
              const SstvModeSpec& mode,
              std::string message)
{
    issues.push_back({code, mode.id, std::move(message)});
}

template<typename T>
bool presentAndPositive(const std::optional<T>& value)
{
    return value.has_value() && *value > 0;
}

bool presentAndPositive(const std::optional<Picoseconds>& value)
{
    return value.has_value() && value->count > 0;
}

bool isNegative(const std::optional<Picoseconds>& value)
{
    return value.has_value() && value->count < 0;
}

bool claimsCapability(CapabilityStatus status) noexcept
{
    return status == CapabilityStatus::Implemented || status == CapabilityStatus::Verified;
}

bool hasVerifiedCapability(const SstvModeSpec& mode) noexcept
{
    return mode.rxStatus == CapabilityStatus::Verified
        || mode.txStatus == CapabilityStatus::Verified
        || mode.autoDetectStatus == CapabilityStatus::Verified;
}

std::string standardVisKey(std::uint8_t code)
{
    return std::string("standard:") + std::to_string(code);
}

std::string extendedVisKey(const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream stream;
    stream << "extended:";
    for (const auto byte : bytes) {
        stream << static_cast<unsigned int>(byte) << ',';
    }
    return stream.str();
}

void validateProtocolFields(const SstvModeSpec& mode,
                            std::vector<ModeValidationIssue>& issues)
{
    const std::array<std::optional<Picoseconds>, 8> durations {{
        mode.timing.syncDuration,
        mode.timing.frontPorch,
        mode.timing.backPorch,
        mode.timing.separatorDuration,
        mode.timing.pixelDuration,
        mode.timing.componentDuration,
        mode.timing.lineDuration,
        mode.timing.imageDuration
    }};
    if (std::any_of(durations.begin(), durations.end(), isNegative)) {
        addIssue(issues, ModeValidationCode::InvalidProtocolValue, mode,
                 "timing values must not be negative");
    }

    if (mode.timing.syncFrequencyHz.has_value() && *mode.timing.syncFrequencyHz == 0U) {
        addIssue(issues, ModeValidationCode::InvalidProtocolValue, mode,
                 "a specified sync frequency must be positive");
    }
    if (mode.timing.separatorDuration.has_value()
        && mode.timing.separatorDuration->count > 0
        && !presentAndPositive(mode.timing.separatorFrequencyHz)) {
        addIssue(issues, ModeValidationCode::InvalidProtocolValue, mode,
                 "a positive separator duration requires a positive frequency");
    }
    if (mode.timing.nominalAudioBandwidth.has_value()) {
        const auto& bandwidth = *mode.timing.nominalAudioBandwidth;
        if (bandwidth.lowHz == 0U || bandwidth.highHz <= bandwidth.lowHz) {
            addIssue(issues, ModeValidationCode::InvalidProtocolValue, mode,
                     "nominal audio bandwidth must have positive ordered limits");
        }
    }

    if (!mode.protocolDataComplete) {
        return;
    }

    const bool geometryComplete = presentAndPositive(mode.geometry.imageWidth)
        && presentAndPositive(mode.geometry.imageHeight)
        && presentAndPositive(mode.geometry.transmittedLineCount)
        && presentAndPositive(mode.geometry.displayedLineCount)
        && presentAndPositive(mode.geometry.linesPerScan);
    if (!geometryComplete) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires image and line geometry");
    }

    if (!mode.vis.has_value()) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires an explicit VIS rule, including no-VIS");
    }

    const bool colourComplete = mode.colour.colourSpace != ColourSpace::Unknown
        && !mode.colour.componentOrder.empty()
        && mode.colour.chromaSubsampling != ChromaSubsampling::Unknown
        && !mode.colour.conversionRule.empty();
    if (!colourComplete) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires colour order, conversion and subsampling");
    }

    const bool hasPhysicalSync =
        presentAndPositive(mode.timing.syncFrequencyHz)
        && presentAndPositive(mode.timing.syncDuration);
    const bool explicitlySyncFree =
        !mode.timing.syncFrequencyHz.has_value()
        && mode.timing.syncDuration.has_value()
        && mode.timing.syncDuration->count == 0;
    const bool hasPhysicalSeparator =
        presentAndPositive(mode.timing.separatorFrequencyHz)
        && presentAndPositive(mode.timing.separatorDuration);
    const bool explicitlySeparatorFree =
        mode.timing.separatorDuration.has_value()
        && mode.timing.separatorDuration->count == 0;
    const bool timingComplete = (hasPhysicalSync || explicitlySyncFree)
        && mode.timing.frontPorch.has_value()
        && mode.timing.backPorch.has_value()
        && (hasPhysicalSeparator || explicitlySeparatorFree)
        && (presentAndPositive(mode.timing.pixelDuration)
            || presentAndPositive(mode.timing.componentDuration))
        && presentAndPositive(mode.timing.lineDuration)
        && presentAndPositive(mode.timing.imageDuration)
        && mode.timing.nominalAudioBandwidth.has_value()
        && presentAndPositive(mode.timing.tolerancePpm);
    if (!timingComplete) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires every timing and bandwidth field");
    }

    if (mode.leaderHeaderRules.empty() || mode.specialLineOrdering.empty()) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires header and line-order rules");
    }
    if (mode.fallbackSignature.discriminator.empty()) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires an explicit fallback rule or no-fallback marker");
    }
    if (mode.protocolProvenance.empty()) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires authoritative provenance");
    }
}

void validateVis(const SstvModeSpec& mode, std::vector<ModeValidationIssue>& issues)
{
    if (!mode.vis.has_value()) {
        return;
    }
    const auto& vis = *mode.vis;
    bool valid = true;
    switch (vis.encoding) {
    case VisEncoding::None:
        valid = vis.bitCount == 0U && !vis.standardCode.has_value()
            && vis.standardAliases.empty() && vis.extendedSequence.empty()
            && vis.parity == Parity::None;
        break;
    case VisEncoding::StandardSevenBit:
        valid = vis.bitCount == 7U && vis.standardCode.has_value()
            && *vis.standardCode < 128U && vis.extendedSequence.empty()
            && vis.lsbFirst && vis.parity == Parity::Even;
        valid = valid && std::all_of(vis.standardAliases.begin(), vis.standardAliases.end(),
                                    [](std::uint8_t code) { return code < 128U; });
        break;
    case VisEncoding::Extended:
        valid = vis.bitCount > 7U && !vis.standardCode.has_value()
            && vis.standardAliases.empty() && !vis.extendedSequence.empty();
        break;
    case VisEncoding::Narrow24Bit:
        valid = vis.bitCount == 24U && !vis.standardCode.has_value()
            && vis.standardAliases.empty()
            && vis.extendedSequence.size() == 4U
            && vis.extendedSequence[0U] == 0x2dU
            && vis.extendedSequence[1U] == 0x15U
            && vis.extendedSequence[3U]
                == static_cast<std::uint8_t>(
                    0x15U ^ vis.extendedSequence[2U])
            && vis.lsbFirst && vis.parity == Parity::None;
        break;
    case VisEncoding::Unknown:
        valid = false;
        break;
    }
    if (!valid) {
        addIssue(issues, ModeValidationCode::InvalidVis, mode,
                 "VIS encoding fields are internally inconsistent");
    }
}

void validateCapability(const SstvModeSpec& mode,
                        CapabilityStatus status,
                        const char* direction,
                        std::vector<ModeValidationIssue>& issues)
{
    if (!claimsCapability(status)) {
        return;
    }
    if (!mode.protocolDataComplete) {
        addIssue(issues, ModeValidationCode::CapabilityWithoutProtocolData, mode,
                 std::string(direction) + " capability requires complete protocol data");
    }
    if (!mode.hasImplementationEvidence() || mode.implementationEvidenceRefs.empty()) {
        addIssue(issues, ModeValidationCode::CapabilityWithoutEvidence, mode,
                 std::string(direction) + " capability requires executable implementation evidence");
    }
    if (status == CapabilityStatus::Verified) {
        if (mode.catalogStatus != CatalogStatus::Verified) {
            addIssue(issues, ModeValidationCode::InconsistentStatus, mode,
                     std::string(direction) + " verified capability requires verified catalogue state");
        }
        if (!mode.hasIndependentEvidence()) {
            addIssue(issues, ModeValidationCode::VerifiedWithoutIndependentEvidence, mode,
                     std::string(direction) + " verification requires an independent fixture and interoperability result");
        }
    }
}

} // namespace

SstvModeRegistry::SstvModeRegistry(std::vector<SstvModeSpec> modes)
    : m_modes(std::move(modes))
{
}

SstvModeRegistry SstvModeRegistry::canonical()
{
    // Entries remain catalogue identities only until their individual audit
    // and implementation gates are complete. Implemented Martin, Scottie and
    // Robot family rows are populated below; every other row intentionally
    // remains discovery-only.
    const CatalogueSeed seeds[] = {
        {"martin-m1", "Martin M1", "M1", "Martin", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Martin M1 protocol and native bounded RX/TX are implemented; independent interoperability evidence is pending."},
        {"martin-m2", "Martin M2", "M2", "Martin", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native bounded RX/TX and automatic VIS are implemented; 320 wire/display columns remain distinct from effective sampled width 160."},
        {"martin-m3", "Martin M3", "M3", "Martin", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native bounded RX/TX and automatic VIS are implemented from Handbook table 4.4 with pinned libsstv landmarks."},
        {"martin-m4", "Martin M4", "M4", "Martin", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native bounded RX/TX and automatic VIS are implemented; 320 wire/display columns remain distinct from effective sampled width 160."},

        {"scottie-s1", "Scottie S1", "S1", "Scottie", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Scottie S1 protocol and native bounded RX/TX are implemented; independent interoperability evidence is pending."},
        {"scottie-s2", "Scottie S2", "S2", "Scottie", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Scottie S2 uses the 320-pixel geometry shared by four audited lineages; native bounded RX/TX are implemented."},
        {"scottie-dx", "Scottie DX", "SDX", "Scottie", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Scottie DX protocol and native bounded RX/TX are implemented; independent long-duration evidence is pending."},
        {"scottie-s3", "Scottie S3", "S3", "Scottie", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native bounded RX/TX and automatic VIS are implemented from Handbook table 4.5 with pinned libsstv landmarks."},
        {"scottie-s4", "Scottie S4", "S4", "Scottie", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native bounded RX/TX and automatic VIS are implemented; 320 wire/display columns remain distinct from effective sampled width 160."},

        {"robot-c12", "Robot 12 Colour", "R12C", "Robot colour", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native Handbook-timed 160x120 4:2:0 Robot RX/TX is implemented."},
        {"robot-c24", "Robot 24 Colour", "R24C", "Robot colour", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native Handbook-timed 320x120 4:2:2 Robot RX/TX is implemented."},
        {"robot-c36", "Robot 36 Colour", "R36C", "Robot colour", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native Handbook-timed 320x240 4:2:0 Robot RX/TX is implemented."},
        {"robot-c72", "Robot 72 Colour", "R72C", "Robot colour", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native Handbook-timed 320x240 4:2:2 Robot RX/TX is implemented."},

        {"robot-bw8", "Robot B/W 8", "RBW8", "Robot monochrome", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native Robot B/W RX/TX is implemented with documented VIS 1/2/3 aliases."},
        {"robot-bw12", "Robot B/W 12", "RBW12", "Robot monochrome", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native Robot B/W RX/TX is implemented with documented VIS 5/6/7 aliases."},
        {"robot-bw24", "Robot B/W 24", "RBW24", "Robot monochrome", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native Robot B/W RX/TX retains the authoritative 105 ms structural line and VIS 9/10/11 aliases."},
        {"robot-bw36", "Robot B/W 36", "RBW36", "Robot monochrome", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native Robot B/W RX/TX is implemented with documented VIS 13/14/15 aliases."},

        {"wraase-sc2-60", "Wraase SC2-60", "SC2-60", "Wraase", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; only one audited implementation lineage and no independent vector."},
        {"wraase-sc2-120", "Wraase SC2-120", "SC2-120", "Wraase", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: an audited implementation uses empirical porch/sync timing that is not normative."},
        {"wraase-sc2-180", "Wraase SC2-180", "SC2-180", "Wraase", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; authoritative timing and an independent vector remain required."},

        {"pasokon-p3", "Pasokon P3", "P3", "Pasokon", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; sequential-colour ordering needs an independent vector."},
        {"pasokon-p5", "Pasokon P5", "P5", "Pasokon", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; sequential-colour ordering needs an independent vector."},
        {"pasokon-p7", "Pasokon P7", "P7", "Pasokon", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; sequential-colour ordering needs an independent vector."},

        {"pd-50", "PD50", "PD50", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent RX/TX evidence is still required."},
        {"pd-90", "PD90", "PD90", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent two-line luminance/chroma evidence is still required."},
        {"pd-120", "PD120", "PD120", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent two-line luminance/chroma evidence is still required."},
        {"pd-160", "PD160", "PD160", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent two-line luminance/chroma evidence is still required."},
        {"pd-180", "PD180", "PD180", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; the available self-generated WAV is not independent evidence."},
        {"pd-240", "PD240", "PD240", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent long-duration evidence is still required."},
        {"pd-290", "PD290", "PD290", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent long-duration evidence is still required."},

        {"avt-24", "AVT24", "AVT24", "AVT", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native normal-mode RX/TX and protected-countdown auto-selection are implemented; independent PCM/live verification is pending."},
        {"avt-90", "AVT90", "AVT90", "AVT", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native normal-mode RX/TX is implemented with 256 effective columns kept distinct from the audited 320-column prepared/wire raster."},
        {"avt-94", "AVT94", "AVT94", "AVT", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Native normal-mode RX/TX and protected-countdown auto-selection are implemented; independent PCM/live verification is pending."},
        {"avt-24-narrow", "AVT24 Narrow", "AVT24N", "AVT variant", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogue-only: complete Narrow picture semantics and independent evidence are missing."},
        {"avt-24-qrm", "AVT24 QRM", "AVT24Q", "AVT variant", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogue-only: complete QRM picture semantics and independent evidence are missing."},
        {"avt-24-narrow-qrm", "AVT24 Narrow QRM", "AVT24NQ", "AVT variant", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogue-only: complete Narrow-QRM picture semantics and independent evidence are missing."},
        {"avt-90-narrow", "AVT90 Narrow", "AVT90N", "AVT variant", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogue-only: complete Narrow picture semantics and independent evidence are missing."},
        {"avt-90-qrm", "AVT90 QRM", "AVT90Q", "AVT variant", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogue-only: complete QRM picture semantics and independent evidence are missing."},
        {"avt-90-narrow-qrm", "AVT90 Narrow QRM", "AVT90NQ", "AVT variant", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogue-only: complete Narrow-QRM picture semantics and independent evidence are missing."},
        {"avt-94-narrow", "AVT94 Narrow", "AVT94N", "AVT variant", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogue-only: complete Narrow picture semantics and independent evidence are missing."},
        {"avt-94-qrm", "AVT94 QRM", "AVT94Q", "AVT variant", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogue-only: complete QRM picture semantics and independent evidence are missing."},
        {"avt-94-narrow-qrm", "AVT94 Narrow QRM", "AVT94NQ", "AVT variant", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogue-only: complete Narrow-QRM picture semantics and independent evidence are missing."},

        {"mp-73", "MP73", "MP73", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mp-115", "MP115", "MP115", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mp-140", "MP140", "MP140", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mp-175", "MP175", "MP175", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mr-73", "MR73", "MR73", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mr-90", "MR90", "MR90", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mr-115", "MR115", "MR115", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mr-140", "MR140", "MR140", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: audited QSSTV lineage assigns extended VIS 0x4A23 to both MR140 and MR175."},
        {"mr-175", "MR175", "MR175", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: QSSTV assigns 0x4A23 but the SSTV Handbook lists 0x4C23; independent waveform validation is missing."},
        {"ml-180", "ML180", "ML180", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"ml-240", "ML240", "ML240", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"ml-280", "ML280", "ML280", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"ml-320", "ML320", "ML320", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},

        {"mp-73-narrow", "MP73-Narrow", "MP73N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},
        {"mp-110-narrow", "MP110-Narrow", "MP110N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},
        {"mp-140-narrow", "MP140-Narrow", "MP140N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},
        {"mc-110-narrow", "MC110-Narrow", "MC110N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},
        {"mc-140-narrow", "MC140-Narrow", "MC140N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},
        {"mc-180-narrow", "MC180-Narrow", "MC180N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},

        {"fax-480", "FAX480", "FAX480", "FAX", ModeClassification::RelatedFax, CatalogStatus::Blocked,
         "Blocked related mode: audited sources disagree on 512x500 versus 512x480 geometry."},
        {"hffax", "HFFAX", "HFFAX", "FAX", ModeClassification::RelatedFax, CatalogStatus::Catalogued,
         "Catalogued related-mode category; authoritative IOC/line-rate variants are not enumerated yet."},
        {"wefax", "WEFAX", "WEFAX", "FAX", ModeClassification::RelatedFax, CatalogStatus::Catalogued,
         "Catalogued related-mode category; authoritative IOC/RPM variants and legal vectors are missing."}
    };

    std::vector<SstvModeSpec> modes;
    modes.reserve(std::size(seeds));
    for (const auto& seed : seeds) {
        auto mode = fromSeed(seed);
        if (mode.id == "martin-m1") {
            populateMartin(
                mode,
                44U,
                256U,
                320U,
                Picoseconds {457'600'000LL},
                Picoseconds {146'432'000'000LL},
                Picoseconds {446'446'000'000LL},
                Picoseconds {114'290'176'000'000LL},
                "M1 uses the exact Handbook structural sum; the QSSTV "
                "5.000 ms rounded sync is not used.",
                false);
        } else if (mode.id == "martin-m2") {
            populateMartin(
                mode,
                40U,
                256U,
                160U,
                Picoseconds {228'800'000LL},
                Picoseconds {73'216'000'000LL},
                Picoseconds {226'798'000'000LL},
                Picoseconds {58'060'288'000'000LL},
                "M2's 160-column effective sampled resolution is metadata, "
                "not a reduction of its 320 transmitted/display columns.",
                true);
        } else if (mode.id == "martin-m3") {
            populateMartin(
                mode,
                36U,
                128U,
                320U,
                Picoseconds {457'600'000LL},
                Picoseconds {146'432'000'000LL},
                Picoseconds {446'446'000'000LL},
                Picoseconds {57'145'088'000'000LL},
                "M3 reuses the M1 line timing at 128 rows. SlowRX's 228.8 us "
                "pixel conflicts with its 446.446 ms line; the Handbook and "
                "pinned libsstv agree on 457.6 us.",
                true);
        } else if (mode.id == "martin-m4") {
            populateMartin(
                mode,
                32U,
                128U,
                160U,
                Picoseconds {228'800'000LL},
                Picoseconds {73'216'000'000LL},
                Picoseconds {226'798'000'000LL},
                Picoseconds {29'030'144'000'000LL},
                "M4 reuses the M2 line timing at 128 rows, with 160 effective "
                "sampled columns kept separate from the 320-column wire raster.",
                true);
        } else if (mode.id == "scottie-s1") {
            populateScottie(
                mode,
                60U,
                256U,
                320U,
                Picoseconds {432'000'000LL},
                Picoseconds {138'240'000'000LL},
                Picoseconds {428'220'000'000LL},
                Picoseconds {109'624'320'000'000LL},
                "S1 uses the segment-sum line duration 428.220 ms; the "
                "additional 428.380 ms SlowRX value is internally 160 us "
                "longer than its own declared segments.");
        } else if (mode.id == "scottie-s2") {
            populateScottie(
                mode,
                56U,
                256U,
                320U,
                Picoseconds {275'200'000LL},
                Picoseconds {88'064'000'000LL},
                Picoseconds {277'692'000'000LL},
                Picoseconds {71'089'152'000'000LL},
                "S2 uses 320 pixels as agreed by QSSTV, SlowRX, Robot36 and "
                "libsstv; the isolated pySSTV width 160 declaration is not "
                "used as the geometry oracle.");
        } else if (mode.id == "scottie-s3") {
            populateScottie(
                mode,
                52U,
                128U,
                320U,
                Picoseconds {432'000'000LL},
                Picoseconds {138'240'000'000LL},
                Picoseconds {428'220'000'000LL},
                Picoseconds {54'812'160'000'000LL},
                "S3 reuses the S1 scanline at half the row count, exactly as "
                "specified by SSTV Handbook table 4.5 and observed in the "
                "pinned libsstv encoder.",
                true);
        } else if (mode.id == "scottie-s4") {
            populateScottie(
                mode,
                48U,
                128U,
                160U,
                Picoseconds {275'200'000LL},
                Picoseconds {88'064'000'000LL},
                Picoseconds {277'692'000'000LL},
                Picoseconds {35'544'576'000'000LL},
                "S4 reuses the S2 scanline at half the row count. The 160 "
                "effective sampled width from Handbook table 4.5 is retained "
                "separately from its 320-column wire/display raster.",
                true);
        } else if (mode.id == "scottie-dx") {
            populateScottie(
                mode,
                76U,
                256U,
                320U,
                Picoseconds {1'080'000'000LL},
                Picoseconds {345'600'000'000LL},
                Picoseconds {1'050'300'000'000LL},
                Picoseconds {268'876'800'000'000LL},
                "DX uses 1.080 ms pixels and the coherent 1.050300 s segment "
                "sum; SlowRX's alternate 1.08053 ms value conflicts with "
                "that same declared line duration.");
        } else if (mode.id == "robot-c12") {
            populateRobot(mode, SstvRobotProtocol::spec(
                SstvRobotMode::Colour12));
        } else if (mode.id == "robot-c24") {
            populateRobot(mode, SstvRobotProtocol::spec(
                SstvRobotMode::Colour24));
        } else if (mode.id == "robot-c36") {
            populateRobot(mode, SstvRobotProtocol::spec(
                SstvRobotMode::Colour36));
        } else if (mode.id == "robot-c72") {
            populateRobot(mode, SstvRobotProtocol::spec(
                SstvRobotMode::Colour72));
        } else if (mode.id == "robot-bw8") {
            populateRobot(mode, SstvRobotProtocol::spec(
                SstvRobotMode::Bw8));
        } else if (mode.id == "robot-bw12") {
            populateRobot(mode, SstvRobotProtocol::spec(
                SstvRobotMode::Bw12));
        } else if (mode.id == "robot-bw24") {
            populateRobot(mode, SstvRobotProtocol::spec(
                SstvRobotMode::Bw24));
        } else if (mode.id == "robot-bw36") {
            populateRobot(mode, SstvRobotProtocol::spec(
                SstvRobotMode::Bw36));
        } else if (mode.id == "wraase-sc2-60") {
            populateSequentialRgb(mode, SstvSequentialRgbProtocol::spec(
                SstvSequentialRgbMode::WraaseSc2_60), false);
        } else if (mode.id == "wraase-sc2-120") {
            populateSequentialRgb(mode, SstvSequentialRgbProtocol::spec(
                SstvSequentialRgbMode::WraaseSc2_120), true);
        } else if (mode.id == "wraase-sc2-180") {
            populateSequentialRgb(mode, SstvSequentialRgbProtocol::spec(
                SstvSequentialRgbMode::WraaseSc2_180), true);
        } else if (mode.id == "pasokon-p3") {
            populateSequentialRgb(mode, SstvSequentialRgbProtocol::spec(
                SstvSequentialRgbMode::PasokonP3), true);
        } else if (mode.id == "pasokon-p5") {
            populateSequentialRgb(mode, SstvSequentialRgbProtocol::spec(
                SstvSequentialRgbMode::PasokonP5), true);
        } else if (mode.id == "pasokon-p7") {
            populateSequentialRgb(mode, SstvSequentialRgbProtocol::spec(
                SstvSequentialRgbMode::PasokonP7), true);
        } else if (mode.id == "pd-50") {
            populatePd(mode, SstvPdProtocol::spec(SstvPdMode::Pd50), false);
        } else if (mode.id == "pd-90") {
            populatePd(mode, SstvPdProtocol::spec(SstvPdMode::Pd90), true);
        } else if (mode.id == "pd-120") {
            populatePd(mode, SstvPdProtocol::spec(SstvPdMode::Pd120), true);
        } else if (mode.id == "pd-160") {
            populatePd(mode, SstvPdProtocol::spec(SstvPdMode::Pd160), true);
        } else if (mode.id == "pd-180") {
            populatePd(mode, SstvPdProtocol::spec(SstvPdMode::Pd180), true);
        } else if (mode.id == "pd-240") {
            populatePd(mode, SstvPdProtocol::spec(SstvPdMode::Pd240), true);
        } else if (mode.id == "pd-290") {
            populatePd(mode, SstvPdProtocol::spec(SstvPdMode::Pd290), true);
        } else if (mode.id == "avt-24") {
            populateAvt(mode, SstvAvtProtocol::spec(SstvAvtMode::Avt24));
        } else if (mode.id == "avt-90") {
            populateAvt(mode, SstvAvtProtocol::spec(SstvAvtMode::Avt90));
        } else if (mode.id == "avt-94") {
            populateAvt(mode, SstvAvtProtocol::spec(SstvAvtMode::Avt94));
        } else if (mode.id == "avt-24-narrow") {
            populateAvtCatalogueVariant(
                mode, SstvAvtMode::Avt24, SstvAvtVariant::Narrow);
        } else if (mode.id == "avt-24-qrm") {
            populateAvtCatalogueVariant(
                mode, SstvAvtMode::Avt24, SstvAvtVariant::Qrm);
        } else if (mode.id == "avt-24-narrow-qrm") {
            populateAvtCatalogueVariant(
                mode, SstvAvtMode::Avt24, SstvAvtVariant::NarrowQrm);
        } else if (mode.id == "avt-90-narrow") {
            populateAvtCatalogueVariant(
                mode, SstvAvtMode::Avt90, SstvAvtVariant::Narrow);
        } else if (mode.id == "avt-90-qrm") {
            populateAvtCatalogueVariant(
                mode, SstvAvtMode::Avt90, SstvAvtVariant::Qrm);
        } else if (mode.id == "avt-90-narrow-qrm") {
            populateAvtCatalogueVariant(
                mode, SstvAvtMode::Avt90, SstvAvtVariant::NarrowQrm);
        } else if (mode.id == "avt-94-narrow") {
            populateAvtCatalogueVariant(
                mode, SstvAvtMode::Avt94, SstvAvtVariant::Narrow);
        } else if (mode.id == "avt-94-qrm") {
            populateAvtCatalogueVariant(
                mode, SstvAvtMode::Avt94, SstvAvtVariant::Qrm);
        } else if (mode.id == "avt-94-narrow-qrm") {
            populateAvtCatalogueVariant(
                mode, SstvAvtMode::Avt94, SstvAvtVariant::NarrowQrm);
        } else if (mode.id == "mp-73") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Mp73));
        } else if (mode.id == "mp-115") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Mp115));
        } else if (mode.id == "mp-140") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Mp140));
        } else if (mode.id == "mp-175") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Mp175));
        } else if (mode.id == "mr-73") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Mr73));
        } else if (mode.id == "mr-90") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Mr90));
        } else if (mode.id == "mr-115") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Mr115));
        } else if (mode.id == "mr-140") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Mr140));
        } else if (mode.id == "mr-175") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Mr175));
        } else if (mode.id == "ml-180") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Ml180));
        } else if (mode.id == "ml-240") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Ml240));
        } else if (mode.id == "ml-280") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Ml280));
        } else if (mode.id == "ml-320") {
            populateMmsstvExtended(
                mode, SstvMmsstvProtocol::spec(SstvMmsstvMode::Ml320));
        } else if (mode.id == "mp-73-narrow") {
            populateMmsstvExtended(
                mode,
                SstvMmsstvProtocol::spec(SstvMmsstvMode::Mp73Narrow));
        } else if (mode.id == "mp-110-narrow") {
            populateMmsstvExtended(
                mode,
                SstvMmsstvProtocol::spec(SstvMmsstvMode::Mp110Narrow));
        } else if (mode.id == "mp-140-narrow") {
            populateMmsstvExtended(
                mode,
                SstvMmsstvProtocol::spec(SstvMmsstvMode::Mp140Narrow));
        } else if (mode.id == "mc-110-narrow") {
            populateMmsstvExtended(
                mode,
                SstvMmsstvProtocol::spec(SstvMmsstvMode::Mc110Narrow));
        } else if (mode.id == "mc-140-narrow") {
            populateMmsstvExtended(
                mode,
                SstvMmsstvProtocol::spec(SstvMmsstvMode::Mc140Narrow));
        } else if (mode.id == "mc-180-narrow") {
            populateMmsstvExtended(
                mode,
                SstvMmsstvProtocol::spec(SstvMmsstvMode::Mc180Narrow));
        }
        modes.push_back(std::move(mode));
    }
    static std::once_flag registryDiagnosticOnce;
    const qulonglong modeCount = static_cast<qulonglong>(modes.size());
    std::call_once(registryDiagnosticOnce, [modeCount] {
        recordSstvDiagnosticEvent(
            sstvCoreLog(), QtInfoMsg,
            QStringLiteral("registry.canonical-loaded"),
            {{QStringLiteral("schemaVersion"), 1},
             {QStringLiteral("count"), modeCount}});
    });
    return SstvModeRegistry(std::move(modes));
}

std::vector<ModeValidationIssue> SstvModeRegistry::validate(const std::vector<SstvModeSpec>& modes)
{
    std::vector<ModeValidationIssue> issues;
    std::unordered_map<std::string, std::size_t> ids;
    std::unordered_map<std::string, std::size_t> longNames;
    std::unordered_map<std::string, std::size_t> shortNames;
    struct VisOwner final {
        std::string id;
        std::string group;
    };
    std::unordered_map<std::string, VisOwner> visOwners;

    for (std::size_t index = 0; index < modes.size(); ++index) {
        const auto& mode = modes[index];
        if (mode.id.empty()) {
            addIssue(issues, ModeValidationCode::EmptyId, mode, "mode ID must not be empty");
        } else if (!ids.emplace(mode.id, index).second) {
            addIssue(issues, ModeValidationCode::DuplicateId, mode, "mode ID must be unique");
        }
        if (mode.longName.empty()) {
            addIssue(issues, ModeValidationCode::EmptyLongName, mode, "long name must not be empty");
        } else if (!longNames.emplace(mode.longName, index).second) {
            addIssue(issues, ModeValidationCode::DuplicateLongName, mode, "long name must be unique");
        }
        if (mode.shortName.empty()) {
            addIssue(issues, ModeValidationCode::EmptyShortName, mode, "short name must not be empty");
        } else if (!shortNames.emplace(mode.shortName, index).second) {
            addIssue(issues, ModeValidationCode::DuplicateShortName, mode, "short name must be unique");
        }
        if (mode.family.empty()) {
            addIssue(issues, ModeValidationCode::EmptyFamily, mode, "mode family must not be empty");
        }
        if (mode.catalogStatus == CatalogStatus::Blocked && mode.statusNote.empty()) {
            addIssue(issues, ModeValidationCode::BlockedWithoutNote, mode,
                     "blocked catalogue rows require an exact blocker note");
        }
        if ((mode.rxStatus == CapabilityStatus::Blocked
             || mode.txStatus == CapabilityStatus::Blocked
             || mode.autoDetectStatus == CapabilityStatus::Blocked)
            && mode.statusNote.empty()) {
            addIssue(issues, ModeValidationCode::BlockedWithoutNote, mode,
                     "blocked capabilities require an exact blocker note");
        }

        validateProtocolFields(mode, issues);
        validateVis(mode, issues);
        validateCapability(mode, mode.rxStatus, "RX", issues);
        validateCapability(mode, mode.txStatus, "TX", issues);
        validateCapability(mode, mode.autoDetectStatus, "auto-detect", issues);

        if (mode.catalogStatus == CatalogStatus::Verified) {
            if (!mode.protocolDataComplete || !hasVerifiedCapability(mode)) {
                addIssue(issues, ModeValidationCode::InconsistentStatus, mode,
                         "verified catalogue state requires complete data and a verified capability");
            }
            if (!mode.hasIndependentEvidence()) {
                addIssue(issues, ModeValidationCode::VerifiedWithoutIndependentEvidence, mode,
                         "verified catalogue state requires independent interoperability evidence");
            }
        }

        if (mode.vis.has_value()) {
            const auto addVisOwner = [&](const std::string& key) {
                const auto existing = visOwners.find(key);
                if (existing == visOwners.end()) {
                    visOwners.emplace(key, VisOwner {mode.id, mode.vis->documentedSharedCodeGroup});
                    return;
                }
                const bool documentedShare = !mode.vis->documentedSharedCodeGroup.empty()
                    && mode.vis->documentedSharedCodeGroup == existing->second.group;
                if (!documentedShare) {
                    addIssue(issues, ModeValidationCode::DuplicateVisCode, mode,
                             "VIS code is also owned by mode " + existing->second.id);
                }
            };

            if (mode.vis->standardCode.has_value()) {
                addVisOwner(standardVisKey(*mode.vis->standardCode));
            }
            for (const auto alias : mode.vis->standardAliases) {
                addVisOwner(standardVisKey(alias));
            }
            if (!mode.vis->extendedSequence.empty()) {
                addVisOwner(extendedVisKey(mode.vis->extendedSequence));
            }
        }
    }
    return issues;
}

const std::vector<SstvModeSpec>& SstvModeRegistry::modes() const noexcept
{
    return m_modes;
}

const SstvModeSpec* SstvModeRegistry::findById(std::string_view id) const noexcept
{
    const auto found = std::find_if(m_modes.begin(), m_modes.end(),
                                    [id](const SstvModeSpec& mode) { return mode.id == id; });
    return found == m_modes.end() ? nullptr : &*found;
}

const SstvModeSpec* SstvModeRegistry::findByName(std::string_view name) const noexcept
{
    const auto found = std::find_if(m_modes.begin(), m_modes.end(),
                                    [name](const SstvModeSpec& mode) {
                                        return mode.longName == name || mode.shortName == name;
                                    });
    return found == m_modes.end() ? nullptr : &*found;
}

std::vector<ModeValidationIssue> SstvModeRegistry::validationIssues() const
{
    return validate(m_modes);
}

bool SstvModeRegistry::isValid() const
{
    return validationIssues().empty();
}

bool containsValidationIssue(const std::vector<ModeValidationIssue>& issues,
                             ModeValidationCode code) noexcept
{
    return std::any_of(issues.begin(), issues.end(),
                       [code](const ModeValidationIssue& issue) { return issue.code == code; });
}

} // namespace decodium::sstv
