// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvAvt.h"
#include "../../src/sstv/analog/SstvMartinM1.h"
#include "../../src/sstv/analog/SstvMmsstvExtended.h"
#include "../../src/sstv/analog/SstvPd.h"
#include "../../src/sstv/analog/SstvRobot.h"
#include "../../src/sstv/analog/SstvScottie.h"
#include "../../src/sstv/analog/SstvSequentialRgb.h"
#include "../../src/sstv/core/SstvModeRegistry.h"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <vector>

using namespace decodium::sstv;

namespace {

SstvModeSpec completeImplementedMode(const char* id,
                                     const char* longName,
                                     const char* shortName,
                                     std::uint8_t visCode)
{
    SstvModeSpec mode;
    mode.id = id;
    mode.longName = longName;
    mode.shortName = shortName;
    mode.family = "Test family";
    mode.classification = ModeClassification::AnalogSstv;
    mode.catalogStatus = CatalogStatus::Catalogued;
    mode.rxStatus = CapabilityStatus::Implemented;
    mode.protocolDataComplete = true;

    SstvVisSpec vis;
    vis.encoding = VisEncoding::StandardSevenBit;
    vis.bitCount = 7;
    vis.standardCode = visCode;
    vis.lsbFirst = true;
    vis.parity = Parity::Even;
    mode.vis = vis;

    mode.geometry.imageWidth = 320;
    mode.geometry.imageHeight = 256;
    mode.geometry.transmittedLineCount = 256;
    mode.geometry.displayedLineCount = 256;
    mode.geometry.linesPerScan = 1;

    mode.colour.colourSpace = ColourSpace::Grayscale;
    mode.colour.componentOrder = {ColourComponent::Gray};
    mode.colour.chromaSubsampling = ChromaSubsampling::NotApplicable;
    mode.colour.conversionRule = "full-range grayscale test rule";

    mode.timing.syncFrequencyHz = 1200;
    mode.timing.syncDuration = Picoseconds {5 * kPicosecondsPerMillisecond};
    mode.timing.frontPorch = Picoseconds {0};
    mode.timing.backPorch = Picoseconds {0};
    mode.timing.separatorFrequencyHz = 0;
    mode.timing.separatorDuration = Picoseconds {0};
    mode.timing.pixelDuration = Picoseconds {kPicosecondsPerMillisecond};
    mode.timing.lineDuration = Picoseconds {100 * kPicosecondsPerMillisecond};
    mode.timing.imageDuration = Picoseconds {10 * kPicosecondsPerSecond};
    mode.timing.nominalAudioBandwidth = SstvAudioBandwidth {1100, 2500};
    mode.timing.tolerancePpm = 1000;

    mode.leaderHeaderRules = "deterministic unit-test header";
    mode.specialLineOrdering = "none";
    mode.fallbackSignature.discriminator = "no fallback in unit test";
    mode.protocolProvenance = {"unit-test protocol specification"};
    mode.evidenceStatus = EvidenceStatus::DeterministicTests;
    mode.implementationEvidenceRefs = {"test_sstv_mode_registry"};
    mode.statusNote = "Synthetic validator fixture; not a canonical mode.";
    return mode;
}

} // namespace

class TestSstvModeRegistry final : public QObject
{
    Q_OBJECT

private slots:
    void canonicalRegistryIsValidAndCompleteAsCatalogue()
    {
        const auto registry = SstvModeRegistry::canonical();
        QCOMPARE(registry.modes().size(), std::size_t {64});
        QVERIFY2(registry.isValid(), "canonical SSTV catalogue must pass structural validation");

        std::set<std::string> ids;
        std::set<std::string> longNames;
        std::set<std::string> shortNames;
        for (const auto& mode : registry.modes()) {
            QVERIFY(!mode.id.empty());
            QVERIFY(!mode.longName.empty());
            QVERIFY(!mode.shortName.empty());
            QVERIFY(!mode.family.empty());
            QVERIFY(ids.insert(mode.id).second);
            QVERIFY(longNames.insert(mode.longName).second);
            QVERIFY(shortNames.insert(mode.shortName).second);

            if (mode.id == "martin-m1"
                || mode.id == "martin-m2"
                || mode.id == "martin-m3"
                || mode.id == "martin-m4"
                || mode.id == "scottie-s1"
                || mode.id == "scottie-s2"
                || mode.id == "scottie-s3"
                || mode.id == "scottie-s4"
                || mode.id == "scottie-dx"
                || mode.id == "robot-c12"
                || mode.id == "robot-c24"
                || mode.id == "robot-c36"
                || mode.id == "robot-c72"
                || mode.id == "robot-bw8"
                || mode.id == "robot-bw12"
                || mode.id == "robot-bw24"
                || mode.id == "robot-bw36"
                || mode.id == "wraase-sc2-60"
                || mode.id == "wraase-sc2-120"
                || mode.id == "wraase-sc2-180"
                || mode.id == "pasokon-p3"
                || mode.id == "pasokon-p5"
                || mode.id == "pasokon-p7"
                || mode.id == "pd-50"
                || mode.id == "pd-90"
                || mode.id == "pd-120"
                || mode.id == "pd-160"
                || mode.id == "pd-180"
                || mode.id == "pd-240"
                || mode.id == "pd-290"
                || mode.family == "AVT"
                || mode.family == "MMSSTV extended"
                || mode.family == "MMSSTV narrow") {
                QVERIFY(mode.protocolDataComplete);
                QVERIFY(mode.claimsRxSupport());
                QVERIFY(mode.claimsTxSupport());
                QVERIFY(mode.hasImplementationEvidence());
            } else {
                // Remaining discovery entries deliberately do not claim
                // implementation until their own protocol audits complete.
                QVERIFY(!mode.protocolDataComplete);
                QVERIFY(!mode.claimsRxSupport());
                QVERIFY(!mode.claimsTxSupport());
                QVERIFY(!mode.hasImplementationEvidence());
            }
            QVERIFY(!mode.catalogueReferences.empty());
        }
    }

    void canonicalRegistryContainsEveryMandatoryIdentity()
    {
        const auto registry = SstvModeRegistry::canonical();
        const std::array<const char*, 64> required {{
            "martin-m1", "martin-m2", "martin-m3", "martin-m4",
            "scottie-s1", "scottie-s2", "scottie-dx", "scottie-s3", "scottie-s4",
            "robot-c12", "robot-c24", "robot-c36", "robot-c72",
            "robot-bw8", "robot-bw12", "robot-bw24", "robot-bw36",
            "wraase-sc2-60", "wraase-sc2-120", "wraase-sc2-180",
            "pasokon-p3", "pasokon-p5", "pasokon-p7",
            "pd-50", "pd-90", "pd-120", "pd-160", "pd-180", "pd-240", "pd-290",
            "avt-24", "avt-90", "avt-94",
            "avt-24-narrow", "avt-24-qrm", "avt-24-narrow-qrm",
            "avt-90-narrow", "avt-90-qrm", "avt-90-narrow-qrm",
            "avt-94-narrow", "avt-94-qrm", "avt-94-narrow-qrm",
            "mp-73", "mp-115", "mp-140", "mp-175",
            "mr-73", "mr-90", "mr-115", "mr-140", "mr-175",
            "ml-180", "ml-240", "ml-280", "ml-320",
            "mp-73-narrow", "mp-110-narrow", "mp-140-narrow",
            "mc-110-narrow", "mc-140-narrow", "mc-180-narrow",
            "fax-480", "hffax", "wefax"
        }};

        for (const auto id : required) {
            QVERIFY2(registry.findById(id) != nullptr, id);
        }

        QCOMPARE(registry.findByName("Martin M1"), registry.findById("martin-m1"));
        QCOMPARE(registry.findByName("M1"), registry.findById("martin-m1"));
        QCOMPARE(registry.findByName("Martin M3"), registry.findById("martin-m3"));
        QCOMPARE(registry.findByName("M4"), registry.findById("martin-m4"));
        QCOMPARE(registry.findByName("Scottie S1"),
                 registry.findById("scottie-s1"));
        QCOMPARE(registry.findByName("S2"),
                 registry.findById("scottie-s2"));
        QCOMPARE(registry.findByName("Scottie S3"),
                 registry.findById("scottie-s3"));
        QCOMPARE(registry.findByName("S4"),
                 registry.findById("scottie-s4"));
        QCOMPARE(registry.findByName("Robot 12 Colour"),
                 registry.findById("robot-c12"));
        QCOMPARE(registry.findByName("RBW24"),
                 registry.findById("robot-bw24"));
        QCOMPARE(registry.findByName("Wraase SC2-120"),
                 registry.findById("wraase-sc2-120"));
        QCOMPARE(registry.findByName("P7"),
                 registry.findById("pasokon-p7"));
        QVERIFY(registry.findById("not-a-mode") == nullptr);
        QVERIFY(registry.findByName("not-a-mode") == nullptr);
    }

    void relatedFaxModesAreNotMisclassifiedAsAnalogSstv()
    {
        const auto registry = SstvModeRegistry::canonical();
        for (const auto id : {"fax-480", "hffax", "wefax"}) {
            const auto* mode = registry.findById(id);
            QVERIFY(mode != nullptr);
            QVERIFY(mode->classification == ModeClassification::RelatedFax);
            QCOMPARE(QString::fromStdString(mode->family), QStringLiteral("FAX"));
        }
        QVERIFY(registry.findById("martin-m1")->classification
                == ModeClassification::AnalogSstv);
    }

    void avtNormalModesAreCompleteAndVariantsRemainCatalogueOnly()
    {
        const auto registry = SstvModeRegistry::canonical();
        const struct {
            const char* id;
            SstvAvtMode protocolMode;
            std::uint32_t preparedWidth;
            std::uint32_t sampledWidth;
            std::uint32_t height;
            std::uint8_t vis;
            std::int64_t pixelPicoseconds;
        } normal[] {
            {"avt-24", SstvAvtMode::Avt24,
             128U, 128U, 120U, 64U, 488'281'250LL},
            {"avt-90", SstvAvtMode::Avt90,
             320U, 256U, 240U, 68U, 390'625'000LL},
            {"avt-94", SstvAvtMode::Avt94,
             320U, 320U, 200U, 72U, 488'281'250LL},
        };

        for (const auto& item : normal) {
            const auto* mode = registry.findById(item.id);
            QVERIFY2(mode != nullptr, item.id);
            QCOMPARE(mode->catalogStatus, CatalogStatus::Catalogued);
            QCOMPARE(mode->rxStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->txStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->autoDetectStatus, CapabilityStatus::Implemented);
            QVERIFY(mode->protocolDataComplete);
            QVERIFY(mode->vis.has_value());
            QCOMPARE(mode->vis->encoding, VisEncoding::StandardSevenBit);
            QCOMPARE(*mode->vis->standardCode, item.vis);
            QCOMPARE(*mode->geometry.imageWidth, item.preparedWidth);
            QCOMPARE(*mode->geometry.sampledPixelWidth, item.sampledWidth);
            QCOMPARE(*mode->geometry.transmittedPixelWidth,
                     item.preparedWidth);
            QCOMPARE(*mode->geometry.imageHeight, item.height);
            QCOMPARE(mode->colour.componentOrder,
                     std::vector<ColourComponent>({ColourComponent::Red,
                                                   ColourComponent::Green,
                                                   ColourComponent::Blue}));
            QVERIFY(!mode->timing.syncFrequencyHz.has_value());
            QCOMPARE(mode->timing.syncDuration->count, std::int64_t {0});
            QVERIFY(!mode->timing.separatorFrequencyHz.has_value());
            QCOMPARE(mode->timing.separatorDuration->count,
                     std::int64_t {0});
            QCOMPARE(mode->timing.pixelDuration->count,
                     item.pixelPicoseconds);
            QCOMPARE(mode->evidenceStatus,
                     EvidenceStatus::DeterministicTests);
            QCOMPARE(mode->fixtureStatus, FixtureStatus::SelfGeneratedOnly);
            QCOMPARE(mode->interoperabilityStatus,
                     InteroperabilityStatus::NotTested);
            QVERIFY(mode->leaderHeaderRules.find("Three complete")
                    != std::string::npos);
            QVERIFY(mode->specialLineOrdering.find("no per-line sync")
                    != std::string::npos);
            QVERIFY(std::find(mode->implementationEvidenceRefs.cbegin(),
                              mode->implementationEvidenceRefs.cend(),
                              "tests/sstv/test_sstv_avt.cpp")
                    != mode->implementationEvidenceRefs.cend());
            QCOMPARE(SstvAvtProtocol::spec(item.protocolMode).stableId,
                     item.id);
        }

        const struct {
            const char* id;
            std::uint8_t vis;
        } variants[] {
            {"avt-24-narrow", 65U},
            {"avt-24-qrm", 66U},
            {"avt-24-narrow-qrm", 67U},
            {"avt-90-narrow", 69U},
            {"avt-90-qrm", 70U},
            {"avt-90-narrow-qrm", 71U},
            {"avt-94-narrow", 73U},
            {"avt-94-qrm", 74U},
            {"avt-94-narrow-qrm", 75U},
        };
        for (const auto& item : variants) {
            const auto* mode = registry.findById(item.id);
            QVERIFY2(mode != nullptr, item.id);
            QVERIFY(!mode->protocolDataComplete);
            QVERIFY(!mode->claimsAnySupport());
            QCOMPARE(mode->autoDetectStatus,
                     CapabilityStatus::Unimplemented);
            QVERIFY(mode->vis.has_value());
            QCOMPARE(mode->vis->encoding, VisEncoding::StandardSevenBit);
            QCOMPARE(*mode->vis->standardCode, item.vis);
            QCOMPARE(mode->evidenceStatus, EvidenceStatus::AuditedSources);
            QCOMPARE(mode->fixtureStatus, FixtureStatus::Missing);
            QVERIFY(!mode->hasImplementationEvidence());
            QVERIFY(mode->statusNote.find("Catalogue-only")
                    != std::string::npos);
        }
    }

    void martinM1HasResolvedProtocolButNoVerificationClaim()
    {
        const auto registry = SstvModeRegistry::canonical();
        const auto* mode = registry.findById("martin-m1");
        QVERIFY(mode != nullptr);
        QCOMPARE(mode->catalogStatus, CatalogStatus::Catalogued);
        QCOMPARE(mode->rxStatus, CapabilityStatus::Implemented);
        QCOMPARE(mode->txStatus, CapabilityStatus::Implemented);
        QCOMPARE(mode->autoDetectStatus, CapabilityStatus::Implemented);
        QVERIFY(mode->protocolDataComplete);

        QVERIFY(mode->vis.has_value());
        QCOMPARE(mode->vis->encoding, VisEncoding::StandardSevenBit);
        QCOMPARE(mode->vis->bitCount, std::uint8_t {7U});
        QCOMPARE(*mode->vis->standardCode, std::uint8_t {44U});
        QVERIFY(mode->vis->lsbFirst);
        QCOMPARE(mode->vis->parity, Parity::Even);

        QCOMPARE(*mode->geometry.imageWidth, std::uint32_t {320U});
        QCOMPARE(*mode->geometry.imageHeight, std::uint32_t {256U});
        QCOMPARE(mode->colour.componentOrder,
                 std::vector<ColourComponent>({ColourComponent::Green,
                                               ColourComponent::Blue,
                                               ColourComponent::Red}));
        QCOMPARE(mode->timing.syncDuration->count,
                 std::int64_t {4'862'000'000LL});
        QCOMPARE(mode->timing.pixelDuration->count,
                 std::int64_t {457'600'000LL});
        QCOMPARE(mode->timing.componentDuration->count,
                 std::int64_t {146'432'000'000LL});
        QCOMPARE(mode->timing.lineDuration->count,
                 std::int64_t {446'446'000'000LL});
        QCOMPARE(mode->timing.imageDuration->count,
                 std::int64_t {114'290'176'000'000LL});
        QCOMPARE(*mode->timing.tolerancePpm, std::uint32_t {300U});

        QCOMPARE(mode->evidenceStatus, EvidenceStatus::DeterministicTests);
        QCOMPARE(mode->fixtureStatus, FixtureStatus::SelfGeneratedOnly);
        QCOMPARE(mode->interoperabilityStatus,
                 InteroperabilityStatus::NotTested);
        QVERIFY(!mode->hasIndependentEvidence());
        QVERIFY(mode->statusNote.find("self-test") != std::string::npos);
        QVERIFY(mode->statusNote.find("independent") != std::string::npos);
        QVERIFY(std::find(mode->implementationEvidenceRefs.cbegin(),
                          mode->implementationEvidenceRefs.cend(),
                          "tests/sstv/test_sstv_martin_m1_rx_session.cpp")
                != mode->implementationEvidenceRefs.cend());
        QVERIFY(std::find(mode->implementationEvidenceRefs.cbegin(),
                          mode->implementationEvidenceRefs.cend(),
                          "tests/sstv/test_sstv_rx_runtime.cpp")
                != mode->implementationEvidenceRefs.cend());

        const auto provenanceContains = [mode](const char* needle) {
            return std::any_of(
                mode->protocolProvenance.begin(),
                mode->protocolProvenance.end(),
                [needle](const std::string& entry) {
                    return entry.find(needle) != std::string::npos;
                });
        };
        QVERIFY(provenanceContains("SSTV Handbook"));
        QVERIFY(provenanceContains("UC Berkeley"));
        QVERIFY(provenanceContains("dnet/slowrx@a50a4e2"));
        QVERIFY(provenanceContains("dnet/pySSTV@d998fad"));
        QVERIFY(provenanceContains("ON4QZ/QSSTV@8c27d6d"));
        QVERIFY(provenanceContains("5.000 ms"));
    }

    void martinM2M3M4HaveResolvedProtocolsAndPinnedEvidence()
    {
        const auto registry = SstvModeRegistry::canonical();
        const std::array<SstvMartinMode, 3U> modes {{
            SstvMartinMode::M2,
            SstvMartinMode::M3,
            SstvMartinMode::M4,
        }};
        for (const SstvMartinMode variant : modes) {
            const SstvMartinModeSpec protocol =
                SstvMartinM1Protocol::spec(variant);
            const auto* mode = registry.findById(protocol.stableId);
            QVERIFY2(mode != nullptr, protocol.stableId);
            QCOMPARE(mode->catalogStatus, CatalogStatus::Catalogued);
            QCOMPARE(mode->rxStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->txStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->autoDetectStatus, CapabilityStatus::Implemented);
            QVERIFY(mode->protocolDataComplete);
            QVERIFY(mode->vis.has_value());
            QCOMPARE(*mode->vis->standardCode, protocol.visPayload);
            QCOMPARE(*mode->geometry.imageWidth, protocol.width);
            QCOMPARE(*mode->geometry.imageHeight, protocol.height);
            QCOMPARE(*mode->geometry.sampledPixelWidth,
                     protocol.effectiveSampledWidth);
            QCOMPARE(*mode->geometry.transmittedPixelWidth,
                     std::uint32_t {320U});
            QCOMPARE(mode->colour.componentOrder,
                     std::vector<ColourComponent>({ColourComponent::Green,
                                                   ColourComponent::Blue,
                                                   ColourComponent::Red}));
            QCOMPARE(mode->timing.pixelDuration->count,
                     protocol.pixelDuration.count);
            QCOMPARE(mode->timing.componentDuration->count,
                     protocol.componentDuration.count);
            QCOMPARE(mode->timing.lineDuration->count,
                     protocol.lineDuration.count);
            QCOMPARE(mode->timing.imageDuration->count,
                     protocol.imageDuration.count);
            QCOMPARE(mode->evidenceStatus,
                     EvidenceStatus::IndependentVector);
            QCOMPARE(mode->fixtureStatus, FixtureStatus::Independent);
            QCOMPARE(mode->interoperabilityStatus,
                     InteroperabilityStatus::UpstreamPathObserved);
            QVERIFY(!mode->hasIndependentEvidence());
            QVERIFY(mode->statusNote.find("not live-radio")
                    != std::string::npos);
            QVERIFY(std::find(
                        mode->implementationEvidenceRefs.cbegin(),
                        mode->implementationEvidenceRefs.cend(),
                        "tests/sstv/fixtures/"
                        "libsstv-193157-martin-m2-m3-m4-landmarks.json")
                    != mode->implementationEvidenceRefs.cend());
            QVERIFY(std::any_of(
                mode->protocolProvenance.cbegin(),
                mode->protocolProvenance.cend(),
                [](const std::string& entry) {
                    return entry.find("193157a993ac34bfa")
                        != std::string::npos;
                }));
        }
    }

    void scottieFamilyHasResolvedProtocolsButNoVerificationClaim()
    {
        const auto registry = SstvModeRegistry::canonical();
        const std::array<SstvScottieMode, 5> modes {{
            SstvScottieMode::S1,
            SstvScottieMode::S2,
            SstvScottieMode::S3,
            SstvScottieMode::S4,
            SstvScottieMode::DX,
        }};

        for (const SstvScottieMode variant : modes) {
            const SstvScottieModeSpec protocol =
                SstvScottieProtocol::spec(variant);
            const auto* mode = registry.findById(protocol.stableId);
            QVERIFY2(mode != nullptr, protocol.stableId);
            QCOMPARE(mode->catalogStatus, CatalogStatus::Catalogued);
            QCOMPARE(mode->rxStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->txStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->autoDetectStatus,
                     CapabilityStatus::Implemented);
            QVERIFY(mode->protocolDataComplete);

            QVERIFY(mode->vis.has_value());
            QCOMPARE(mode->vis->encoding, VisEncoding::StandardSevenBit);
            QCOMPARE(mode->vis->bitCount, std::uint8_t {7U});
            QCOMPARE(*mode->vis->standardCode, protocol.visPayload);
            QVERIFY(mode->vis->lsbFirst);
            QCOMPARE(mode->vis->parity, Parity::Even);

            QCOMPARE(*mode->geometry.imageWidth, protocol.width);
            QCOMPARE(*mode->geometry.imageHeight, protocol.height);
            QCOMPARE(*mode->geometry.transmittedPixelWidth,
                     std::uint32_t {320U});
            QCOMPARE(*mode->geometry.sampledPixelWidth,
                     variant == SstvScottieMode::S4
                         ? std::uint32_t {160U}
                         : std::uint32_t {320U});
            QCOMPARE(mode->colour.componentOrder,
                     std::vector<ColourComponent>({ColourComponent::Green,
                                                   ColourComponent::Blue,
                                                   ColourComponent::Red}));
            QCOMPARE(mode->timing.syncDuration->count,
                     protocol.syncDuration.count);
            QCOMPARE(mode->timing.frontPorch->count,
                     protocol.porchDuration.count);
            QCOMPARE(mode->timing.pixelDuration->count,
                     protocol.pixelDuration.count);
            QCOMPARE(mode->timing.componentDuration->count,
                     protocol.componentDuration.count);
            QCOMPARE(mode->timing.lineDuration->count,
                     protocol.lineDuration.count);
            QCOMPARE(mode->timing.imageDuration->count,
                     protocol.imageDuration.count);
            QCOMPARE(*mode->timing.tolerancePpm, std::uint32_t {300U});
            QVERIFY(mode->specialLineOrdering.find("embedded 9 ms")
                    != std::string::npos);
            QVERIFY(mode->specialLineOrdering.find("no invented")
                    != std::string::npos);

            const bool pinnedLandmarks = variant == SstvScottieMode::S3
                || variant == SstvScottieMode::S4;
            QCOMPARE(mode->evidenceStatus,
                     pinnedLandmarks
                         ? EvidenceStatus::IndependentVector
                         : EvidenceStatus::DeterministicTests);
            QCOMPARE(mode->fixtureStatus,
                     pinnedLandmarks
                         ? FixtureStatus::Independent
                         : FixtureStatus::SelfGeneratedOnly);
            QCOMPARE(mode->interoperabilityStatus,
                     pinnedLandmarks
                         ? InteroperabilityStatus::UpstreamPathObserved
                         : InteroperabilityStatus::NotTested);
            QVERIFY(!mode->hasIndependentEvidence());
            QVERIFY(mode->statusNote.find("independent")
                    != std::string::npos);
            if (pinnedLandmarks) {
                QVERIFY(mode->statusNote.find("not a live-radio")
                        != std::string::npos);
                QVERIFY(std::find(
                            mode->implementationEvidenceRefs.cbegin(),
                            mode->implementationEvidenceRefs.cend(),
                            "tests/sstv/fixtures/"
                            "libsstv-193157-scottie-s3-s4-landmarks.json")
                        != mode->implementationEvidenceRefs.cend());
                QVERIFY(std::any_of(
                    mode->protocolProvenance.cbegin(),
                    mode->protocolProvenance.cend(),
                    [](const std::string& entry) {
                        return entry.find("193157a993ac34bfa")
                            != std::string::npos;
                    }));
            } else {
                QVERIFY(mode->statusNote.find("self-test")
                        != std::string::npos);
                QVERIFY(std::find(
                            mode->implementationEvidenceRefs.cbegin(),
                            mode->implementationEvidenceRefs.cend(),
                            "tests/sstv/fixtures/"
                            "libsstv-193157-scottie-s3-s4-landmarks.json")
                        == mode->implementationEvidenceRefs.cend());
            }
            QVERIFY(std::find(
                        mode->implementationEvidenceRefs.cbegin(),
                        mode->implementationEvidenceRefs.cend(),
                        "tests/sstv/test_sstv_scottie_rx_session.cpp")
                    != mode->implementationEvidenceRefs.cend());
            QVERIFY(std::find(
                        mode->implementationEvidenceRefs.cbegin(),
                        mode->implementationEvidenceRefs.cend(),
                        "tests/sstv/test_sstv_rx_runtime.cpp")
                    != mode->implementationEvidenceRefs.cend());
            QVERIFY(mode->protocolProvenance.size() >= 3U);
        }

        const auto provenanceContains = [&registry](const char* id,
                                                    const char* needle) {
            const auto* mode = registry.findById(id);
            return mode != nullptr && std::any_of(
                mode->protocolProvenance.cbegin(),
                mode->protocolProvenance.cend(),
                [needle](const std::string& entry) {
                    return entry.find(needle) != std::string::npos;
                });
        };
        QVERIFY(provenanceContains("scottie-s2",
                                   "four other audited lineages"));
        QVERIFY(provenanceContains("scottie-dx", "1.08053"));
        QVERIFY(provenanceContains("scottie-s3", "SSTV Handbook table 4.5"));
        QVERIFY(provenanceContains("scottie-s4",
                                   "effective horizontal resolution as 160"));
    }

    void robotFamilyHasResolvedProtocolsAliasesAndPinnedEvidence()
    {
        const auto registry = SstvModeRegistry::canonical();
        const std::array<SstvRobotMode, 8U> modes {{
            SstvRobotMode::Colour12,
            SstvRobotMode::Colour24,
            SstvRobotMode::Colour36,
            SstvRobotMode::Colour72,
            SstvRobotMode::Bw8,
            SstvRobotMode::Bw12,
            SstvRobotMode::Bw24,
            SstvRobotMode::Bw36,
        }};

        for (const SstvRobotMode variant : modes) {
            const SstvRobotModeSpec protocol = SstvRobotProtocol::spec(
                variant);
            const auto* mode = registry.findById(protocol.stableId);
            QVERIFY2(mode != nullptr, protocol.stableId);
            QCOMPARE(mode->catalogStatus, CatalogStatus::Catalogued);
            QCOMPARE(mode->rxStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->txStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->autoDetectStatus,
                     CapabilityStatus::Implemented);
            QVERIFY(mode->protocolDataComplete);

            QVERIFY(mode->vis.has_value());
            QCOMPARE(mode->vis->encoding, VisEncoding::StandardSevenBit);
            QCOMPARE(mode->vis->bitCount, std::uint8_t {7U});
            QCOMPARE(*mode->vis->standardCode, protocol.visPayload);
            QCOMPARE(mode->vis->standardAliases.size(),
                     static_cast<std::size_t>(protocol.visAliasCount));
            for (std::uint8_t index = 0U;
                 index < protocol.visAliasCount;
                 ++index) {
                QCOMPARE(mode->vis->standardAliases[index],
                         protocol.visAliases[index]);
                QCOMPARE(SstvRobotProtocol::modeForVis(
                             protocol.visAliases[index]),
                         std::optional<SstvRobotMode> {variant});
            }
            QCOMPARE(SstvRobotProtocol::modeForVis(protocol.visPayload),
                     std::optional<SstvRobotMode> {variant});

            QCOMPARE(*mode->geometry.imageWidth, protocol.width);
            QCOMPARE(*mode->geometry.imageHeight, protocol.height);
            QCOMPARE(*mode->geometry.sampledPixelWidth, protocol.width);
            QCOMPARE(*mode->geometry.transmittedPixelWidth, protocol.width);
            QCOMPARE(*mode->geometry.transmittedLineCount, protocol.height);
            QCOMPARE(*mode->geometry.displayedLineCount, protocol.height);
            QCOMPARE(*mode->geometry.linesPerScan, std::uint32_t {1U});
            QCOMPARE(mode->colour.colourSpace,
                     protocol.colour
                         ? ColourSpace::YCbCr
                         : ColourSpace::Grayscale);
            QCOMPARE(mode->colour.chromaSubsampling,
                     protocol.chromaSubsampling);
            QCOMPARE(mode->timing.syncDuration->count,
                     protocol.syncDuration.count);
            QCOMPARE(mode->timing.separatorDuration->count,
                     protocol.markerDuration.count);
            QCOMPARE(mode->timing.pixelDuration->count,
                     protocol.luminancePixelDuration.count);
            QCOMPARE(mode->timing.componentDuration->count,
                     protocol.luminanceDuration.count);
            QCOMPARE(mode->timing.lineDuration->count,
                     protocol.lineDuration.count);
            QCOMPARE(mode->timing.imageDuration->count,
                     protocol.imageDuration.count);

            QCOMPARE(mode->evidenceStatus,
                     EvidenceStatus::IndependentVector);
            QCOMPARE(mode->fixtureStatus, FixtureStatus::Independent);
            QCOMPARE(mode->interoperabilityStatus,
                     InteroperabilityStatus::UpstreamPathObserved);
            QVERIFY(!mode->hasIndependentEvidence());
            QVERIFY(mode->statusNote.find("not live-radio")
                    != std::string::npos);
            QVERIFY(std::find(
                        mode->implementationEvidenceRefs.cbegin(),
                        mode->implementationEvidenceRefs.cend(),
                        "tests/sstv/fixtures/"
                        "libsstv-193157-robot-landmarks.json")
                    != mode->implementationEvidenceRefs.cend());
            QVERIFY(std::any_of(
                mode->protocolProvenance.cbegin(),
                mode->protocolProvenance.cend(),
                [](const std::string& entry) {
                    return entry.find("193157a993ac34bfa")
                        != std::string::npos;
                }));
        }
    }

    void sequentialRgbFamilyUsesExplicitResolvedProfiles()
    {
        const auto registry = SstvModeRegistry::canonical();
        const std::array<SstvSequentialRgbMode, 6U> modes {{
            SstvSequentialRgbMode::WraaseSc2_60,
            SstvSequentialRgbMode::WraaseSc2_120,
            SstvSequentialRgbMode::WraaseSc2_180,
            SstvSequentialRgbMode::PasokonP3,
            SstvSequentialRgbMode::PasokonP5,
            SstvSequentialRgbMode::PasokonP7,
        }};

        for (const SstvSequentialRgbMode variant : modes) {
            const SstvSequentialRgbModeSpec protocol =
                SstvSequentialRgbProtocol::spec(variant);
            const auto* mode = registry.findById(protocol.stableId);
            QVERIFY2(mode != nullptr, protocol.stableId);
            QCOMPARE(mode->catalogStatus, CatalogStatus::Catalogued);
            QCOMPARE(mode->rxStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->txStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->autoDetectStatus, CapabilityStatus::Implemented);
            QVERIFY(mode->protocolDataComplete);

            QVERIFY(mode->vis.has_value());
            QCOMPARE(mode->vis->encoding, VisEncoding::StandardSevenBit);
            QCOMPARE(mode->vis->bitCount, std::uint8_t {7U});
            QCOMPARE(*mode->vis->standardCode, protocol.visPayload);
            QCOMPARE(SstvSequentialRgbProtocol::modeForVis(
                         protocol.visPayload),
                     std::optional<SstvSequentialRgbMode> {variant});

            QCOMPARE(*mode->geometry.imageWidth, protocol.width);
            QCOMPARE(*mode->geometry.imageHeight, protocol.height);
            QCOMPARE(*mode->geometry.sampledPixelWidth,
                     protocol.effectiveSampledWidth);
            QCOMPARE(*mode->geometry.transmittedPixelWidth, protocol.width);
            QCOMPARE(*mode->geometry.transmittedLineCount, protocol.height);
            QCOMPARE(*mode->geometry.displayedLineCount, protocol.height);
            QCOMPARE(*mode->geometry.linesPerScan, std::uint32_t {1U});
            QCOMPARE(mode->colour.colourSpace, ColourSpace::Rgb);
            QCOMPARE(mode->colour.componentOrder,
                     std::vector<ColourComponent>({ColourComponent::Red,
                                                   ColourComponent::Green,
                                                   ColourComponent::Blue}));
            QCOMPARE(mode->colour.chromaSubsampling,
                     ChromaSubsampling::Cs444);
            QCOMPARE(mode->timing.syncDuration->count,
                     protocol.syncDuration.count);
            QCOMPARE(mode->timing.pixelDuration->count,
                     protocol.pixelDuration.count);
            QCOMPARE(mode->timing.componentDuration->count,
                     protocol.componentDuration.count);
            QCOMPARE(mode->timing.lineDuration->count,
                     protocol.lineDuration.count);
            QCOMPARE(mode->timing.imageDuration->count,
                     protocol.imageDuration.count);
            QVERIFY(mode->specialLineOrdering.find(
                        protocol.compatibilityProfile)
                    != std::string::npos);
            QVERIFY(std::any_of(
                mode->protocolProvenance.cbegin(),
                mode->protocolProvenance.cend(),
                [](const std::string& entry) {
                    return entry.find("Handbook") != std::string::npos;
                }));

            const bool hasIndependentLandmark =
                variant != SstvSequentialRgbMode::WraaseSc2_60;
            QCOMPARE(mode->evidenceStatus,
                     hasIndependentLandmark
                         ? EvidenceStatus::IndependentVector
                         : EvidenceStatus::DeterministicTests);
            QCOMPARE(mode->fixtureStatus,
                     hasIndependentLandmark
                         ? FixtureStatus::Independent
                         : FixtureStatus::SelfGeneratedOnly);
            QCOMPARE(mode->interoperabilityStatus,
                     hasIndependentLandmark
                         ? InteroperabilityStatus::UpstreamPathObserved
                         : InteroperabilityStatus::NotTested);
            QVERIFY(!mode->hasIndependentEvidence());

            const auto fixture = std::find(
                mode->implementationEvidenceRefs.cbegin(),
                mode->implementationEvidenceRefs.cend(),
                "tests/sstv/fixtures/"
                "pysstv-d998fad-sequential-rgb-landmarks.json");
            QCOMPARE(fixture != mode->implementationEvidenceRefs.cend(),
                     hasIndependentLandmark);
            QVERIFY(mode->statusNote.find("not live-radio")
                        != std::string::npos
                    || mode->statusNote.find("no independent waveform")
                        != std::string::npos);
        }
    }

    void pdFamilyUsesCanonicalLinePairsAndIndependentLandmarks()
    {
        const auto registry = SstvModeRegistry::canonical();
        const std::array<SstvPdMode, 7U> modes {{
            SstvPdMode::Pd50,
            SstvPdMode::Pd90,
            SstvPdMode::Pd120,
            SstvPdMode::Pd160,
            SstvPdMode::Pd180,
            SstvPdMode::Pd240,
            SstvPdMode::Pd290,
        }};

        for (const SstvPdMode variant : modes) {
            const SstvPdModeSpec protocol = SstvPdProtocol::spec(variant);
            const auto* mode = registry.findById(protocol.stableId);
            QVERIFY2(mode != nullptr, protocol.stableId);
            QCOMPARE(mode->catalogStatus, CatalogStatus::Catalogued);
            QCOMPARE(mode->rxStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->txStatus, CapabilityStatus::Implemented);
            QCOMPARE(mode->autoDetectStatus, CapabilityStatus::Implemented);
            QVERIFY(mode->protocolDataComplete);

            QVERIFY(mode->vis.has_value());
            QCOMPARE(mode->vis->encoding, VisEncoding::StandardSevenBit);
            QCOMPARE(*mode->vis->standardCode, protocol.visPayload);
            QCOMPARE(SstvPdProtocol::modeForVis(protocol.visPayload),
                     std::optional<SstvPdMode> {variant});
            QCOMPARE(*mode->geometry.imageWidth, protocol.width);
            QCOMPARE(*mode->geometry.imageHeight, protocol.height);
            QCOMPARE(*mode->geometry.transmittedPixelWidth, protocol.width);
            QCOMPARE(*mode->geometry.transmittedLineCount, protocol.height);
            QCOMPARE(*mode->geometry.displayedLineCount, protocol.height);
            QCOMPARE(*mode->geometry.linesPerScan, std::uint32_t {2U});
            QCOMPARE(mode->colour.colourSpace, ColourSpace::YCbCr);
            QCOMPARE(mode->colour.componentOrder,
                     std::vector<ColourComponent>({
                         ColourComponent::Luminance,
                         ColourComponent::ChrominanceRed,
                         ColourComponent::ChrominanceBlue,
                         ColourComponent::Luminance}));
            QCOMPARE(mode->colour.chromaSubsampling,
                     ChromaSubsampling::Cs440);
            QCOMPARE(mode->timing.syncDuration->count,
                     protocol.syncDuration.count);
            QCOMPARE(mode->timing.backPorch->count,
                     protocol.porchDuration.count);
            QCOMPARE(mode->timing.pixelDuration->count,
                     protocol.pixelDuration.count);
            QCOMPARE(mode->timing.componentDuration->count,
                     protocol.componentDuration.count);
            QCOMPARE(mode->timing.lineDuration->count,
                     protocol.linePairDuration.count);
            QCOMPARE(mode->timing.imageDuration->count,
                     protocol.imageDuration.count);
            QVERIFY(mode->specialLineOrdering.find("height/2")
                    != std::string::npos);

            QCOMPARE(mode->evidenceStatus,
                     EvidenceStatus::IndependentVector);
            QCOMPARE(mode->fixtureStatus, FixtureStatus::Independent);
            QCOMPARE(mode->interoperabilityStatus,
                     InteroperabilityStatus::UpstreamPathObserved);
            QVERIFY(!mode->hasIndependentEvidence());
            QVERIFY(mode->statusNote.find("not live-radio")
                    != std::string::npos
                    || mode->statusNote.find("No live-radio")
                        != std::string::npos);
            QVERIFY(std::find(
                        mode->implementationEvidenceRefs.cbegin(),
                        mode->implementationEvidenceRefs.cend(),
                        "tests/sstv/fixtures/"
                        "libsstv-193157-pd-landmarks.json")
                    != mode->implementationEvidenceRefs.cend());
            const bool hasPysstv = variant != SstvPdMode::Pd50;
            const auto pysstv = std::find(
                mode->implementationEvidenceRefs.cbegin(),
                mode->implementationEvidenceRefs.cend(),
                "tests/sstv/fixtures/"
                "pysstv-d998fad-pd-landmarks.json");
            QCOMPARE(pysstv != mode->implementationEvidenceRefs.cend(),
                     hasPysstv);
            QVERIFY(std::any_of(
                mode->protocolProvenance.cbegin(),
                mode->protocolProvenance.cend(),
                [](const std::string& entry) {
                    return entry.find("additional scan pair")
                        != std::string::npos;
                }));
        }
    }

    void auditedConflictsAreResolvedOrRemainExplicitlyBlocked()
    {
        const auto registry = SstvModeRegistry::canonical();
        const std::array<const char*, 1> blocked {{"fax-480"}};

        for (const auto id : blocked) {
            const auto* mode = registry.findById(id);
            QVERIFY2(mode != nullptr, id);
            QVERIFY2(mode->catalogStatus == CatalogStatus::Blocked, id);
            QVERIFY2(mode->rxStatus == CapabilityStatus::Blocked, id);
            QVERIFY2(mode->txStatus == CapabilityStatus::Blocked, id);
            QVERIFY2(!mode->statusNote.empty(), id);
        }

        QVERIFY(registry.findById("martin-m1")->catalogStatus
                != CatalogStatus::Blocked);
        QVERIFY(registry.findById("martin-m2")->catalogStatus
                != CatalogStatus::Blocked);
        QVERIFY(registry.findById("martin-m3")->catalogStatus
                != CatalogStatus::Blocked);

        const auto* mr140 = registry.findById("mr-140");
        const auto* mr175 = registry.findById("mr-175");
        QVERIFY(mr140->claimsRxSupport());
        QVERIFY(mr175->claimsRxSupport());
        QVERIFY(mr140->vis.has_value());
        QVERIFY(mr175->vis.has_value());
        QCOMPARE(mr140->vis->extendedSequence,
                 (std::vector<std::uint8_t> {0x23U, 0x4aU}));
        QCOMPARE(mr175->vis->extendedSequence,
                 (std::vector<std::uint8_t> {0x23U, 0x4cU}));
        QVERIFY(std::any_of(
            mr175->protocolProvenance.cbegin(),
            mr175->protocolProvenance.cend(),
            [](const std::string& entry) {
                return entry.find("QSSTV's duplicate 0x4a")
                    != std::string::npos;
            }));
    }

    void incompleteCatalogueRowsMayOmitUnknownProtocolValues()
    {
        SstvModeSpec discovery;
        discovery.id = "discovery-only";
        discovery.longName = "Discovery only";
        discovery.shortName = "DISC";
        discovery.family = "Test";
        discovery.statusNote = "No protocol data claimed.";

        auto issues = SstvModeRegistry::validate({discovery});
        QVERIFY(issues.empty());

        discovery.protocolDataComplete = true;
        issues = SstvModeRegistry::validate({discovery});
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::MissingProtocolField));
    }

    void duplicateIdsAndNamesAreRejected()
    {
        auto first = completeImplementedMode("one", "First mode", "ONE", 1);
        auto duplicate = completeImplementedMode("one", "First mode", "ONE", 2);
        const auto issues = SstvModeRegistry::validate({first, duplicate});
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::DuplicateId));
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::DuplicateLongName));
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::DuplicateShortName));
    }

    void supportClaimsRequireCompleteDataAndExecutableEvidence()
    {
        SstvModeSpec unsupported;
        unsupported.id = "unsupported-claim";
        unsupported.longName = "Unsupported claim";
        unsupported.shortName = "UNSUP";
        unsupported.family = "Test";
        unsupported.rxStatus = CapabilityStatus::Implemented;

        auto issues = SstvModeRegistry::validate({unsupported});
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::CapabilityWithoutProtocolData));
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::CapabilityWithoutEvidence));

        auto complete = completeImplementedMode("complete", "Complete mode", "COMP", 3);
        issues = SstvModeRegistry::validate({complete});
        QVERIFY2(issues.empty(), "implemented status with complete data and deterministic evidence must validate");

        complete.evidenceStatus = EvidenceStatus::AuditedSources;
        complete.implementationEvidenceRefs.clear();
        issues = SstvModeRegistry::validate({complete});
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::CapabilityWithoutEvidence));
    }

    void verifiedClaimsRequireIndependentEvidence()
    {
        auto mode = completeImplementedMode("verified", "Verified mode", "VER", 4);
        mode.catalogStatus = CatalogStatus::Verified;
        mode.rxStatus = CapabilityStatus::Verified;

        auto issues = SstvModeRegistry::validate({mode});
        QVERIFY(containsValidationIssue(issues,
                                        ModeValidationCode::VerifiedWithoutIndependentEvidence));

        mode.evidenceStatus = EvidenceStatus::IndependentVector;
        mode.interoperabilityStatus = InteroperabilityStatus::IndependentlyVerified;
        mode.fixtureStatus = FixtureStatus::Independent;
        mode.implementationEvidenceRefs.push_back("independent vector hash and test result");
        issues = SstvModeRegistry::validate({mode});
        QVERIFY(issues.empty());
    }

    void duplicateVisRequiresAnExplicitSharedCodeGroup()
    {
        auto first = completeImplementedMode("vis-one", "VIS one", "VIS1", 5);
        auto second = completeImplementedMode("vis-two", "VIS two", "VIS2", 5);

        auto issues = SstvModeRegistry::validate({first, second});
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::DuplicateVisCode));

        first.vis->documentedSharedCodeGroup = "documented-alias-test";
        second.vis->documentedSharedCodeGroup = "documented-alias-test";
        issues = SstvModeRegistry::validate({first, second});
        QVERIFY(!containsValidationIssue(issues, ModeValidationCode::DuplicateVisCode));
    }
};

QTEST_MAIN_WRAPPER(TestSstvModeRegistry, static_cast<void>(0);)
#include "test_sstv_mode_registry.moc"
