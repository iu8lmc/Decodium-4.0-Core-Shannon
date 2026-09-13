// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvAvt.h"
#include "../../src/sstv/analog/SstvMartinM1.h"
#include "../../src/sstv/analog/SstvMmsstvExtended.h"
#include "../../src/sstv/analog/SstvPd.h"
#include "../../src/sstv/analog/SstvRobot.h"
#include "../../src/sstv/analog/SstvScottie.h"
#include "../../src/sstv/analog/SstvSequentialRgb.h"
#include "../../src/sstv/core/SstvTimingAccumulator.h"
#include "../../src/sstv/core/SstvVisCodec.h"
#include "../../src/sstv/diagnostics/SstvDiagnosticLogging.h"
#include "../../src/sstv/integration/SstvRxRuntime.h"
#include "../../src/sstv/tx/SstvToneGenerator.h"

#include <QElapsedTimer>
#include <QSignalSpy>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr int kRate = 12'000;

QVector<short> tone(std::size_t count,
                    double frequencyHz = 1'900.0,
                    std::size_t startSample = 0U)
{
    constexpr double pi = 3.141592653589793238462643383279502884;
    QVector<short> result(static_cast<qsizetype>(count));
    for (std::size_t index = 0U; index < count; ++index) {
        const double phase = 2.0 * pi * frequencyHz
            * static_cast<double>(startSample + index)
            / static_cast<double>(kRate);
        result[static_cast<qsizetype>(index)] =
            static_cast<short>(std::lround(12'000.0 * std::sin(phase)));
    }
    return result;
}

SstvRxRuntime::Config smallConfig()
{
    SstvRxRuntime::Config config;
    config.ingress.maximumChunks = 8U;
    config.ingress.maximumQueuedSamples = 8'192U;
    config.ingress.maximumSamplesPerCall = 4'096U;
    config.timestampJitterToleranceNs = 2'000'000;
    config.snapshotNotificationIntervalMs = 20U;
    config.maximumErrorCharacters = 128U;
    return config;
}

std::vector<std::int16_t> martinPrefix(SstvMartinMode mode,
                                       std::uint32_t imageLines)
{
    const SstvMartinModeSpec spec = SstvMartinM1Protocol::spec(mode);
    std::vector<SstvRgbPixel> pixels(
        SstvMartinM1Encoder::pixelCount(mode),
        SstvRgbPixel {210U, 70U, 145U});
    SstvMartinM1EncoderConfig encoderConfig;
    encoderConfig.mode = mode;
    encoderConfig.sampleRate = kRate;
    SstvMartinM1Encoder encoder(pixels, encoderConfig);
    SstvMartinM1Mapper mapper({kRate, 0, mode});
    SstvTimingAccumulator headerTiming(kRate);
    const std::uint64_t headerSamples = headerTiming.samplesFor(
        SstvMartinM1Protocol::HeaderDuration);
    const std::uint64_t imageSamples = imageLines >= spec.height
        ? mapper.imageSampleCount()
        : mapper.lineStartSample(imageLines);
    const std::uint64_t requested = headerSamples + imageSamples;
    if (requested > std::numeric_limits<std::size_t>::max()) {
        return {};
    }
    std::vector<std::int16_t> result(
        static_cast<std::size_t>(requested));
    std::size_t produced = 0U;
    while (produced < result.size()) {
        const std::size_t count = std::min(
            result.size() - produced,
            SstvMartinM1Encoder::MaximumSamplesPerPull);
        const std::size_t pulled = encoder.pullPcm16(
            result.data() + produced, count);
        if (pulled == 0U) {
            break;
        }
        produced += pulled;
    }
    result.resize(produced);
    return result;
}

std::vector<std::int16_t> scottiePrefix(SstvScottieMode mode,
                                        std::uint32_t imageLines)
{
    const SstvScottieModeSpec spec = SstvScottieProtocol::spec(mode);
    std::vector<SstvRgbPixel> pixels(
        SstvScottieEncoder::pixelCount(mode),
        SstvRgbPixel {210U, 70U, 145U});
    SstvScottieEncoderConfig encoderConfig;
    encoderConfig.mode = mode;
    encoderConfig.sampleRate = kRate;
    SstvScottieEncoder encoder(pixels, encoderConfig);
    SstvScottieMapper mapper({mode, kRate, 0});
    SstvTimingAccumulator headerTiming(kRate);
    const std::uint64_t headerSamples = headerTiming.samplesFor(
        SstvScottieProtocol::HeaderDuration);
    const std::uint64_t imageSamples = imageLines
            >= spec.height
        ? mapper.imageSampleCount()
        : mapper.lineStartSample(imageLines);
    const std::uint64_t requested = headerSamples + imageSamples;
    if (requested > std::numeric_limits<std::size_t>::max()) {
        return {};
    }
    std::vector<std::int16_t> result(
        static_cast<std::size_t>(requested));
    std::size_t produced = 0U;
    while (produced < result.size()) {
        const std::size_t count = std::min(
            result.size() - produced,
            SstvScottieEncoder::MaximumSamplesPerPull);
        const std::size_t pulled = encoder.pullPcm16(
            result.data() + produced, count);
        if (pulled == 0U) {
            break;
        }
        produced += pulled;
    }
    result.resize(produced);
    return result;
}

double visFrequency(SstvVisSymbol symbol)
{
    switch (symbol) {
    case SstvVisSymbol::Separator:
        return 1'200.0;
    case SstvVisSymbol::Zero:
        return 1'300.0;
    case SstvVisSymbol::One:
        return 1'100.0;
    case SstvVisSymbol::Invalid:
        throw std::logic_error("invalid symbol in encoded VIS frame");
    }
    throw std::logic_error("unknown symbol in encoded VIS frame");
}

std::vector<std::int16_t> standardVisHeader(std::uint8_t payload)
{
    SstvToneGenerator generator(kRate);
    std::vector<std::int16_t> result;
    result.reserve(10'920U);
    auto append = [&](double frequencyHz, Picoseconds duration) {
        const std::uint64_t scheduled = generator.samplesForDuration(duration);
        const std::size_t sampleCount = static_cast<std::size_t>(scheduled);
        const std::size_t offset = result.size();
        result.resize(offset + sampleCount);
        const std::size_t produced = generator.generatePcm16(
            frequencyHz, 1.0, result.data() + offset, sampleCount);
        if (produced != sampleCount) {
            throw std::logic_error("VIS header generator was cancelled");
        }
    };

    append(1'900.0, Picoseconds {300'000'000'000LL});
    append(1'200.0, Picoseconds {10'000'000'000LL});
    append(1'900.0, Picoseconds {300'000'000'000LL});
    const SstvVisEncodedFrame encoded = SstvVisCodec::encodeStandard(payload);
    for (const SstvVisSymbol symbol : encoded.symbols) {
        append(visFrequency(symbol), Picoseconds {30'000'000'000LL});
    }
    return result;
}

std::vector<std::int16_t> robotPrefix(
    SstvRobotMode mode,
    std::uint32_t imageLines,
    std::optional<std::uint8_t> visOverride = std::nullopt)
{
    const SstvRobotModeSpec spec = SstvRobotProtocol::spec(mode);
    std::vector<SstvRgbPixel> pixels(
        SstvRobotEncoder::pixelCount(mode),
        SstvRgbPixel {210U, 70U, 145U});
    SstvRobotEncoderConfig encoderConfig;
    encoderConfig.mode = mode;
    encoderConfig.sampleRate = kRate;
    SstvRobotEncoder encoder(pixels, encoderConfig);
    SstvRobotMapper mapper({mode, kRate, 0});
    SstvTimingAccumulator headerTiming(kRate);
    const std::uint64_t headerSamples = headerTiming.samplesFor(
        SstvRobotProtocol::HeaderDuration);
    const std::uint64_t imageSamples = imageLines >= spec.height
        ? mapper.imageSampleCount()
        : mapper.lineStartSample(imageLines);
    const std::uint64_t requested = headerSamples + imageSamples;
    if (requested > std::numeric_limits<std::size_t>::max()) {
        return {};
    }
    std::vector<std::int16_t> result(
        static_cast<std::size_t>(requested));
    std::size_t produced = 0U;
    while (produced < result.size()) {
        const std::size_t count = std::min(
            result.size() - produced,
            SstvRobotEncoder::MaximumSamplesPerPull);
        const std::size_t pulled = encoder.pullPcm16(
            result.data() + produced, count);
        if (pulled == 0U) {
            break;
        }
        produced += pulled;
    }
    result.resize(produced);
    if (visOverride.has_value()) {
        const std::vector<std::int16_t> header = standardVisHeader(
            *visOverride);
        if (header.size() > result.size()) {
            return {};
        }
        std::copy(header.cbegin(), header.cend(), result.begin());
    }
    return result;
}

std::vector<std::int16_t> sequentialRgbPrefix(
    SstvSequentialRgbMode mode,
    std::uint32_t imageLines)
{
    const SstvSequentialRgbModeSpec spec =
        SstvSequentialRgbProtocol::spec(mode);
    std::vector<SstvRgbPixel> pixels(
        SstvSequentialRgbEncoder::pixelCount(mode),
        SstvRgbPixel {210U, 70U, 145U});
    SstvSequentialRgbEncoderConfig encoderConfig;
    encoderConfig.mode = mode;
    encoderConfig.sampleRate = kRate;
    SstvSequentialRgbEncoder encoder(pixels, encoderConfig);
    SstvSequentialRgbMapper mapper({mode, kRate, 0});
    SstvTimingAccumulator headerTiming(kRate);
    const std::uint64_t headerSamples = headerTiming.samplesFor(
        SstvSequentialRgbProtocol::HeaderDuration);
    const std::uint64_t imageSamples = imageLines >= spec.height
        ? mapper.imageSampleCount()
        : mapper.lineStartSample(imageLines);
    const std::uint64_t requested = headerSamples + imageSamples;
    if (requested > std::numeric_limits<std::size_t>::max()) {
        return {};
    }
    std::vector<std::int16_t> result(
        static_cast<std::size_t>(requested));
    std::size_t produced = 0U;
    while (produced < result.size()) {
        const std::size_t count = std::min(
            result.size() - produced,
            SstvSequentialRgbEncoder::MaximumSamplesPerPull);
        const std::size_t pulled = encoder.pullPcm16(
            result.data() + produced, count);
        if (pulled == 0U) {
            break;
        }
        produced += pulled;
    }
    result.resize(produced);
    return result;
}

std::vector<std::int16_t> pdPrefix(SstvPdMode mode,
                                   std::uint32_t linePairs)
{
    std::vector<SstvRgbPixel> pixels(
        SstvPdEncoder::pixelCount(mode),
        SstvRgbPixel {210U, 70U, 145U});
    SstvPdEncoderConfig encoderConfig;
    encoderConfig.mode = mode;
    encoderConfig.sampleRate = kRate;
    SstvPdEncoder encoder(pixels, encoderConfig);
    SstvPdMapper mapper({mode, static_cast<std::uint32_t>(kRate), 0});
    SstvTimingAccumulator headerTiming(kRate);
    const std::uint64_t headerSamples = headerTiming.samplesFor(
        SstvPdProtocol::HeaderDuration);
    const std::uint64_t imageSamples = linePairs >= mapper.linePairCount()
        ? mapper.imageSampleCount()
        : mapper.linePairStartSample(linePairs);
    const std::uint64_t requested = headerSamples + imageSamples;
    if (requested > std::numeric_limits<std::size_t>::max()) {
        return {};
    }
    std::vector<std::int16_t> result(static_cast<std::size_t>(requested));
    std::size_t produced = 0U;
    while (produced < result.size()) {
        const std::size_t count = std::min(
            result.size() - produced,
            SstvPdEncoder::MaximumSamplesPerPull);
        const std::size_t pulled = encoder.pullPcm16(
            result.data() + produced, count);
        if (pulled == 0U) {
            break;
        }
        produced += pulled;
    }
    result.resize(produced);
    return result;
}

std::vector<std::int16_t> avtPrefix(SstvAvtMode mode,
                                    std::uint32_t imageLines)
{
    const SstvAvtModeSpec spec = SstvAvtProtocol::spec(mode);
    std::vector<SstvRgbPixel> pixels(
        SstvAvtEncoder::pixelCount(mode),
        SstvRgbPixel {210U, 70U, 145U});
    SstvAvtEncoderConfig encoderConfig;
    encoderConfig.mode = mode;
    encoderConfig.sampleRate = kRate;
    SstvAvtEncoder encoder(pixels, encoderConfig);
    SstvAvtMapper mapper({mode, static_cast<std::uint32_t>(kRate), 0});
    SstvTimingAccumulator headerTiming(kRate);
    const std::uint64_t headerSamples = headerTiming.samplesFor(
        SstvAvtProtocol::HeaderDuration);
    const std::uint64_t imageSamples = imageLines >= spec.height
        ? mapper.imageSampleCount()
        : mapper.lineStartSample(imageLines);
    const std::uint64_t requested = headerSamples + imageSamples;
    if (requested > std::numeric_limits<std::size_t>::max()) {
        return {};
    }
    std::vector<std::int16_t> result(static_cast<std::size_t>(requested));
    std::size_t produced = 0U;
    while (produced < result.size()) {
        const std::size_t count = std::min(
            result.size() - produced,
            SstvAvtEncoder::MaximumSamplesPerPull);
        const std::size_t pulled = encoder.pullPcm16(
            result.data() + produced, count);
        if (pulled == 0U) {
            break;
        }
        produced += pulled;
    }
    result.resize(produced);
    return result;
}

std::vector<std::int16_t> mmsstvPrefix(SstvMmsstvMode mode,
                                       std::uint32_t scans)
{
    const SstvMmsstvModeSpec spec = SstvMmsstvProtocol::spec(mode);
    std::vector<SstvRgbPixel> pixels(
        SstvMmsstvEncoder::pixelCount(mode),
        SstvRgbPixel {210U, 70U, 145U});
    SstvMmsstvEncoderConfig encoderConfig;
    encoderConfig.mode = mode;
    encoderConfig.sampleRate = kRate;
    SstvMmsstvEncoder encoder(pixels, encoderConfig);
    SstvMmsstvMapper mapper(
        {mode, static_cast<std::uint32_t>(kRate), 0});
    SstvTimingAccumulator headerTiming(kRate);
    const std::uint64_t headerSamples = headerTiming.samplesFor(
        spec.headerDuration);
    const std::uint64_t imageSamples = scans >= spec.scanCount
        ? mapper.imageSampleCount()
        : mapper.scanStartSample(scans);
    const std::uint64_t requested = headerSamples + imageSamples;
    if (requested > std::numeric_limits<std::size_t>::max()) {
        return {};
    }
    std::vector<std::int16_t> result(static_cast<std::size_t>(requested));
    std::size_t produced = 0U;
    while (produced < result.size()) {
        const std::size_t count = std::min(
            result.size() - produced,
            SstvMmsstvEncoder::MaximumSamplesPerPull);
        const std::size_t pulled = encoder.pullPcm16(
            result.data() + produced, count);
        if (pulled == 0U) {
            break;
        }
        produced += pulled;
    }
    result.resize(produced);
    return result;
}

QVector<short> pcmChunk(const std::vector<std::int16_t>& samples,
                        std::size_t offset,
                        std::size_t count)
{
    QVector<short> result(static_cast<qsizetype>(count));
    for (std::size_t index = 0U; index < count; ++index) {
        result[static_cast<qsizetype>(index)] =
            static_cast<short>(samples[offset + index]);
    }
    return result;
}

} // namespace

class TestSstvRxRuntime final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<SstvRxRuntime::State>();
        QVERIFY(QMetaType::fromType<SstvRxRuntime::State>().isValid());
    }

    void structuredLifecycleAndControlDiagnosticsStayScalarAndPrivate()
    {
        SstvDiagnosticLogBuffer& diagnostics =
            SstvDiagnosticLogBuffer::instance();
        diagnostics.clear();

        SstvRxRuntime runtime(smallConfig());
        QVERIFY(runtime.setRxModeControl(
            SstvRxModeControl::Manual, "martin-m1"));
        QVERIFY(runtime.setRxModeLock(true, "martin-m1"));
        QVERIFY(runtime.setRxTimingFallbackEnabled(false));
        QVERIFY(runtime.setRxAfc(SstvRxAfcMode::Manual, 100.0));
        QVERIFY(runtime.setRxSlant(SstvRxSlantMode::Manual, -300.0));
        runtime.resetRxAfc();
        runtime.resetRxSlant();
        SstvRxRedecodeParameters redecode;
        redecode.mode = "martin-m1";
        redecode.afcMode = SstvRxAfcMode::Manual;
        redecode.frequencyCorrectionHz = 100.0;
        redecode.slantMode = SstvRxSlantMode::Manual;
        redecode.clockErrorPpm = -300.0;
        QVERIFY(runtime.requestRxRedecode(redecode));

        QVERIFY(runtime.start(SstvAudioSourceKind::Replay, 900U));
        QVERIFY(runtime.resetStream(4U));
        QVERIFY(runtime.stop());
        QVERIFY(runtime.shutdown());

        const QVariantList events = diagnostics.snapshot();
        QStringList names;
        for (const QVariant& item : events) {
            const QVariantMap event = item.toMap();
            const QString category =
                event.value(QStringLiteral("category")).toString();
            if (category != QStringLiteral("sstv.rx")
                && category != QStringLiteral("sstv.sync")) {
                continue;
            }
            names.push_back(
                event.value(QStringLiteral("event")).toString());
            const QVariantMap fields =
                event.value(QStringLiteral("fields")).toMap();
            QVERIFY(!fields.contains(QStringLiteral("path")));
            QVERIFY(!fields.contains(QStringLiteral("callsign")));
            QVERIFY(!fields.contains(QStringLiteral("audio")));
            QVERIFY(!fields.contains(QStringLiteral("image")));
            for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
                QVERIFY(it.value().metaType().id() != QMetaType::QVariantMap);
                QVERIFY(it.value().metaType().id() != QMetaType::QVariantList);
            }
        }
        const QStringList expected {
            QStringLiteral("sync.mode-control-changed"),
            QStringLiteral("sync.mode-lock-changed"),
            QStringLiteral("sync.fallback-setting-changed"),
            QStringLiteral("sync.afc-setting-changed"),
            QStringLiteral("sync.slant-setting-changed"),
            QStringLiteral("sync.afc-reset"),
            QStringLiteral("sync.slant-reset"),
            QStringLiteral("rx.redecode-requested"),
            QStringLiteral("rx.started"),
            QStringLiteral("rx.reset"),
            QStringLiteral("rx.stopped"),
            QStringLiteral("rx.shutdown"),
        };
        for (const QString& name : expected) {
            QVERIFY2(names.contains(name), qPrintable(name));
        }
    }

    void monotonicTimelinePreventsOverlapAndPreservesRealGaps()
    {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvMonotonicTimeline(-1));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvMonotonicTimeline(1'000'000'001));

        SstvMonotonicTimeline timeline(2'000'000);
        auto first = timeline.propose(100'000'000, 120U, 12'000U);
        QVERIFY(first.valid);
        QVERIFY(!first.hadPreviousBlock);
        QCOMPARE(first.startNs, qint64 {100'000'000});
        QCOMPARE(first.endNs, qint64 {110'000'000});
        QVERIFY(timeline.commit(first));

        // A burst callback observed before the preceding media block ends is
        // placed exactly at that end, never overlapped or rejected.
        auto burst = timeline.propose(101'000'000, 120U, 12'000U);
        QVERIFY(burst.valid);
        QVERIFY(burst.hadPreviousBlock);
        QVERIFY(!burst.preservedGap);
        QCOMPARE(burst.startNs, qint64 {110'000'000});
        QCOMPARE(burst.endNs, qint64 {120'000'000});
        QVERIFY(timeline.commit(burst));

        // Five milliseconds beyond the expected end exceeds jitter tolerance
        // and remains visible as a real capture gap.
        auto delayed = timeline.propose(125'000'000, 120U, 12'000U);
        QVERIFY(delayed.valid);
        QVERIFY(delayed.preservedGap);
        QCOMPARE(delayed.gapNs, qint64 {5'000'000});
        QCOMPARE(delayed.startNs, qint64 {125'000'000});
        QVERIFY(timeline.commit(delayed));

        auto staleCandidate = timeline.propose(0, 120U, 12'000U);
        QVERIFY(staleCandidate.valid);
        timeline.reset();
        QVERIFY(!timeline.commit(staleCandidate));
        QVERIFY(!timeline.hasTimestamp());

        QVERIFY(!timeline.propose(-1, 1U, 12'000U).valid);
        QVERIFY(!timeline.propose(0, 1U, 12'345U).valid);
        QVERIFY(!timeline.propose(std::numeric_limits<qint64>::max(),
                                  1U,
                                  12'000U).valid);
    }

    void lifecycleWakeCancelRestartAndShutdownAreDeterministic()
    {
        SstvRxRuntime runtime(smallConfig());
        QSignalSpy stateSpy(&runtime, &SstvRxRuntime::runtimeStateChanged);
        QSignalSpy snapshotSpy(&runtime, &SstvRxRuntime::snapshotAvailable);
        QVERIFY(stateSpy.isValid());
        QVERIFY(snapshotSpy.isValid());

        QVERIFY(runtime.start(SstvAudioSourceKind::LocalSoundCard, 4U));
        const SstvRxRouteToken first = runtime.routeToken();
        QVERIFY(first.valid());
        QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().workerRunning, 1'000);

        QVERIFY(runtime.enqueuePcm16At(tone(600U), kRate, first, 1'000'000));
        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                  std::uint64_t {1U},
                                  2'000);

        QVERIFY(runtime.cancel());
        QCOMPARE(runtime.state(), SstvRxRuntime::State::Cancelled);
        QVERIFY(!runtime.enqueuePcm16At(tone(64U),
                                        kRate,
                                        first,
                                        2'000'000));

        const auto beforeRestart = runtime.snapshot();
        QVERIFY(runtime.restart(17U));
        const SstvRxRouteToken second = runtime.routeToken();
        QVERIFY(second.valid());
        QVERIFY(second.generation > first.generation);
        QVERIFY(second.source == first.source);
        QVERIFY(!runtime.enqueuePcm16At(tone(64U),
                                        kRate,
                                        first,
                                        0));
        QVERIFY(runtime.enqueuePcm16At(tone(600U), kRate, second, 0));
        QTRY_VERIFY_WITH_TIMEOUT(
            runtime.snapshot().generationChunksProcessed >= 1U,
            2'000);

        QVERIFY(runtime.stop());
        QCOMPARE(runtime.state(), SstvRxRuntime::State::Inactive);
        QVERIFY(!runtime.snapshot().workerRunning);
        QVERIFY(runtime.shutdown());
        QCOMPARE(runtime.state(), SstvRxRuntime::State::Shutdown);
        QVERIFY(!runtime.restart());
        QTRY_VERIFY_WITH_TIMEOUT(stateSpy.count() >= 1, 1'000);
        QTRY_COMPARE_WITH_TIMEOUT(
            qvariant_cast<SstvRxRuntime::State>(stateSpy.last().at(0)),
            SstvRxRuntime::State::Shutdown,
            1'000);
        QVERIFY(snapshotSpy.count() >= 1);

        // State delivery is queued until the outer lifecycle call has fully
        // committed.  A same-thread consumer can therefore start the next
        // transition without recursively entering start().
        SstvRxRuntime reentrant(smallConfig());
        bool startReturned = false;
        bool runningDelivered = false;
        bool stopAccepted = false;
        connect(&reentrant,
                &SstvRxRuntime::runtimeStateChanged,
                &reentrant,
                [&](SstvRxRuntime::State delivered, quint64) {
                    if (delivered == SstvRxRuntime::State::Running) {
                        runningDelivered = true;
                        QVERIFY(startReturned);
                        stopAccepted = reentrant.stop();
                    }
                });
        QVERIFY(reentrant.start(SstvAudioSourceKind::Replay, 91U));
        startReturned = true;
        QVERIFY(!runningDelivered);
        QTRY_VERIFY_WITH_TIMEOUT(runningDelivered, 1'000);
        QVERIFY(stopAccepted);
        QCOMPARE(reentrant.state(), SstvRxRuntime::State::Inactive);

        // A non-forced update inside the notification interval must retain a
        // trailing delivery even when the worker goes straight back to sleep.
        auto trailingConfig = smallConfig();
        trailingConfig.snapshotNotificationIntervalMs = 100U;
        SstvRxRuntime trailing(trailingConfig);
        QSignalSpy trailingSpy(&trailing,
                               &SstvRxRuntime::snapshotAvailable);
        QVERIFY(trailing.start(SstvAudioSourceKind::Replay, 92U));
        const auto trailingToken = trailing.routeToken();
        QTRY_VERIFY_WITH_TIMEOUT(trailingSpy.count() >= 1, 1'000);
        QTest::qWait(110);
        const qsizetype beforeFirstDelivery = trailingSpy.count();
        QVERIFY(trailing.enqueuePcm16At(tone(120U),
                                        kRate,
                                        trailingToken,
                                        0));
        QTRY_COMPARE_WITH_TIMEOUT(trailing.snapshot().chunksProcessed,
                                  std::uint64_t {1U},
                                  1'000);
        QTRY_VERIFY_WITH_TIMEOUT(trailingSpy.count() > beforeFirstDelivery,
                                 1'000);
        const qsizetype afterFirstDelivery = trailingSpy.count();
        QVERIFY(trailing.enqueuePcm16At(tone(120U, 1'900.0, 120U),
                                        kRate,
                                        trailingToken,
                                        1));
        QTRY_COMPARE_WITH_TIMEOUT(trailing.snapshot().chunksProcessed,
                                  std::uint64_t {2U},
                                  1'000);
        const std::uint64_t trailingRevision =
            trailing.snapshot().revision;
        QTRY_VERIFY_WITH_TIMEOUT(trailingSpy.count() > afterFirstDelivery,
                                 1'000);
        QVERIFY(trailingSpy.last().at(0).toULongLong()
                >= trailingRevision);
        QVERIFY(trailing.stop());
    }

    void sameSourceResetChangesGenerationAndClearsAllDspState()
    {
        SstvRxRuntime runtime(smallConfig());
        QVERIFY(runtime.start(SstvAudioSourceKind::Tci, 22U, 5U));
        const SstvRxRouteToken oldToken = runtime.routeToken();
        QVERIFY(runtime.enqueuePcm16At(tone(1'200U),
                                       kRate,
                                       oldToken,
                                       20'000'000));
        QTRY_COMPARE_WITH_TIMEOUT(
            runtime.snapshot().generationChunksProcessed,
            std::uint64_t {1U},
            2'000);
        const auto before = runtime.snapshot();
        QVERIFY(before.processedPcmHash
                != std::uint64_t {14'695'981'039'346'656'037ULL});

        QVERIFY(runtime.resetStream(99U));
        const SstvRxRouteToken freshToken = runtime.routeToken();
        QVERIFY(freshToken.generation > oldToken.generation);
        QVERIFY(freshToken.source == oldToken.source);
        const auto immediatelyReset = runtime.snapshot();
        QCOMPARE(immediatelyReset.generationChunksProcessed,
                 std::uint64_t {0U});
        QCOMPARE(immediatelyReset.processedPcmHash,
                 std::uint64_t {14'695'981'039'346'656'037ULL});
        QCOMPARE(immediatelyReset.activeSampleRate, std::uint32_t {0U});
        QCOMPARE(immediatelyReset.vis.available, false);
        QTRY_VERIFY_WITH_TIMEOUT(
            runtime.snapshot().pipelineResets > before.pipelineResets,
            1'000);

        const auto reset = runtime.snapshot();
        QCOMPARE(reset.generationChunksProcessed, std::uint64_t {0U});
        QCOMPARE(reset.processedPcmHash,
                 std::uint64_t {14'695'981'039'346'656'037ULL});
        QCOMPARE(reset.activeSampleRate, std::uint32_t {0U});
        QCOMPARE(reset.vis.available, false);

        QVERIFY(!runtime.enqueuePcm16At(tone(128U),
                                        kRate,
                                        oldToken,
                                        0));
        QVERIFY(runtime.enqueuePcm16At(tone(600U),
                                       kRate,
                                       freshToken,
                                       0));
        QTRY_COMPARE_WITH_TIMEOUT(
            runtime.snapshot().generationChunksProcessed,
            std::uint64_t {1U},
            2'000);
        QVERIFY(runtime.snapshot().producerRejectedCalls >= 1U);
        QVERIFY(runtime.stop());
    }

    void processingIsEquivalentAcrossArbitraryChunkBoundaries()
    {
        SstvRxRuntime::Config equivalenceConfig = smallConfig();
        equivalenceConfig.ingress.maximumChunks = 32U;
        SstvRxRuntime whole(equivalenceConfig);
        SstvRxRuntime split(equivalenceConfig);
        QVERIFY(whole.start(SstvAudioSourceKind::Replay, 1U));
        QVERIFY(split.start(SstvAudioSourceKind::Replay, 2U));
        const auto wholeToken = whole.routeToken();
        const auto splitToken = split.routeToken();

        constexpr std::size_t total = 4'000U;
        const QVector<short> pcm = tone(total, 1'900.0);
        QVERIFY(whole.enqueuePcm16At(pcm,
                                     kRate,
                                     wholeToken,
                                     1'000'000'000));

        const std::size_t pattern[] {37U, 211U, 64U, 509U, 3U, 997U};
        std::size_t offset = 0U;
        std::size_t patternIndex = 0U;
        std::uint64_t chunks = 0U;
        while (offset < total) {
            const std::size_t count = std::min(
                pattern[patternIndex
                        % (sizeof(pattern) / sizeof(pattern[0]))],
                total - offset);
            QVERIFY(split.enqueuePcm16At(
                pcm.sliced(static_cast<qsizetype>(offset),
                           static_cast<qsizetype>(count)),
                kRate,
                splitToken,
                1'000'000'000));
            offset += count;
            ++patternIndex;
            ++chunks;
        }

        QTRY_COMPARE_WITH_TIMEOUT(whole.snapshot().chunksProcessed,
                                  std::uint64_t {1U},
                                  4'000);
        QTRY_COMPARE_WITH_TIMEOUT(split.snapshot().chunksProcessed,
                                  chunks,
                                  4'000);
        const auto one = whole.snapshot();
        const auto many = split.snapshot();
        QCOMPARE(many.samplesConverted, one.samplesConverted);
        QCOMPARE(many.samplesResampled, one.samplesResampled);
        QCOMPARE(many.samplesPreprocessed, one.samplesPreprocessed);
        QCOMPARE(many.frequencyObservations, one.frequencyObservations);
        QCOMPARE(many.toneObservations, one.toneObservations);
        QCOMPARE(many.processedPcmHash, one.processedPcmHash);
        QCOMPARE(many.discontinuities, std::uint64_t {0U});

        QVERIFY(whole.stop());
        QVERIFY(split.stop());
    }

    void supportedRateIsFixedAndProducerBoundsRemainHard()
    {
        SstvRxRuntime::Config config = smallConfig();
        config.ingress.maximumChunks = 2U;
        config.ingress.maximumQueuedSamples = 512U;
        config.ingress.maximumSamplesPerCall = 256U;
        SstvRxRuntime runtime(config);

        const SstvRxRouteToken fabricated {
            {SstvAudioSourceKind::RtlSdr, 8U}, 1U};
        QVERIFY(!runtime.enqueuePcm16At(tone(64U),
                                        kRate,
                                        fabricated,
                                        0));
        const auto inactive = runtime.snapshot();
        QCOMPARE(inactive.workerStarts, std::uint64_t {0U});
        QCOMPARE(inactive.pipelineResets, std::uint64_t {0U});
        QCOMPARE(inactive.chunksProcessed, std::uint64_t {0U});
        QCOMPARE(inactive.samplesConverted, std::uint64_t {0U});
        QCOMPARE(inactive.samplesResampled, std::uint64_t {0U});
        QCOMPARE(inactive.frequencyObservations, std::uint64_t {0U});
        QCOMPARE(inactive.toneObservations, std::uint64_t {0U});

        QVERIFY(runtime.start(SstvAudioSourceKind::RtlSdr, 8U));
        const auto token = runtime.routeToken();
        QVERIFY(!runtime.enqueuePcm16At(tone(64U),
                                        12'345,
                                        token,
                                        0));
        QVERIFY(!runtime.enqueuePcm16At(tone(257U),
                                        kRate,
                                        token,
                                        0));
        QVERIFY(runtime.enqueuePcm16At(tone(128U),
                                       kRate,
                                       token,
                                       0));
        // 48 kHz is globally supported, but changing rate inside one generation
        // must be rejected before it can poison the stateful resampler.
        QVERIFY(!runtime.enqueuePcm16At(QVector<short>(128, 1),
                                        48'000,
                                        token,
                                        20'000'000));
        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                  std::uint64_t {1U},
                                  2'000);

        const auto result = runtime.snapshot();
        QVERIFY(result.ingress.rejectedRateChangeCalls >= 1U);
        QVERIFY(result.producerRejectedCalls >= 3U);
        QVERIFY(result.ingress.queue.queuedChunks
                <= config.ingress.maximumChunks);
        QVERIFY(result.ingress.queue.queuedSamples
                <= config.ingress.maximumQueuedSamples);
        QVERIFY(runtime.stop());
    }

    void idleWorkerIsJoinableAndShutdownWakesWaitPop()
    {
        SstvRxRuntime runtime(smallConfig());
        QVERIFY(runtime.start(SstvAudioSourceKind::DecoPort, 31U));
        QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().workerRunning, 1'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            runtime.snapshot().ingress.queue.waitingConsumers >= 1U,
            1'000);

        QElapsedTimer elapsed;
        elapsed.start();
        QVERIFY(runtime.shutdown());
        QVERIFY(elapsed.elapsed() < 1'000);
        QCOMPARE(runtime.state(), SstvRxRuntime::State::Shutdown);
        QVERIFY(!runtime.snapshot().workerRunning);
        QCOMPARE(runtime.snapshot().chunksProcessed, std::uint64_t {0U});
    }

    void martinM1VisStartsProgressiveNativeImageSession()
    {
        SstvDiagnosticLogBuffer::instance().clear();
        SstvRxRuntime::Config config = smallConfig();
        config.ingress.maximumChunks = 16U;
        config.ingress.maximumQueuedSamples = 65'536U;
        const std::vector<std::int16_t> pcm = martinPrefix(
            SstvMartinMode::M1, 8U);
        QVERIFY(pcm.size() > 40'000U);

        SstvRxRuntime runtime(config);
        QVERIFY(runtime.start(SstvAudioSourceKind::Replay, 44U));
        const SstvRxRouteToken token = runtime.routeToken();
        const std::size_t pattern[] {1'337U, 4'093U, 977U, 2'048U};
        std::size_t offset = 0U;
        std::size_t patternIndex = 0U;
        std::uint64_t chunks = 0U;
        while (offset < pcm.size()) {
            const std::size_t count = std::min(
                pattern[patternIndex
                        % (sizeof(pattern) / sizeof(pattern[0]))],
                pcm.size() - offset);
            QVERIFY(runtime.enqueuePcm16At(
                pcmChunk(pcm, offset, count),
                kRate,
                token,
                1'000'000'000));
            ++chunks;
            QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                      chunks,
                                      4'000);
            offset += count;
            ++patternIndex;
        }

        QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().vis.valid, 4'000);
        const auto snapshot = runtime.snapshot();
        QVERIFY(snapshot.vis.modeMapped);
        QCOMPARE(snapshot.vis.mappedMode, QStringLiteral("martin-m1"));
        QVERIFY(snapshot.image.available);
        QCOMPARE(snapshot.image.acquisitionId, std::uint64_t {1U});
        QCOMPARE(snapshot.image.mode, QStringLiteral("martin-m1"));
        QCOMPARE(snapshot.image.width, std::uint32_t {320U});
        QCOMPARE(snapshot.image.height, std::uint32_t {256U});
        QVERIFY(snapshot.image.linesPublished >= 5U);
        QVERIFY(snapshot.image.coverage > 0.015);
        QVERIFY(!snapshot.image.complete);
        QVERIFY(!snapshot.image.partial);
        QCOMPARE(snapshot.rxState, SstvRxState::Receiving);

        bool foundAcceptedVis = false;
        for (const QVariant& item :
             SstvDiagnosticLogBuffer::instance().snapshot()) {
            const QVariantMap event = item.toMap();
            if (event.value(QStringLiteral("category")).toString()
                    != QStringLiteral("sstv.vis")
                || event.value(QStringLiteral("event")).toString()
                    != QStringLiteral("vis.accepted")) {
                continue;
            }
            const QVariantMap fields =
                event.value(QStringLiteral("fields")).toMap();
            if (fields.value(QStringLiteral("modeId")).toString()
                    == QStringLiteral("martin-m1")) {
                QCOMPARE(fields.value(QStringLiteral("visCode")).toInt(), 44);
                QCOMPARE(fields.value(QStringLiteral("success")).toBool(),
                         true);
                foundAcceptedVis = true;
                break;
            }
        }
        QVERIFY(foundAcceptedVis);

        const std::shared_ptr<const SstvImageSnapshot> image =
            runtime.latestImageSnapshot();
        QVERIFY(image != nullptr);
        QCOMPARE(image->width, snapshot.image.width);
        QCOMPARE(image->height, snapshot.image.height);
        QCOMPARE(image->revision, snapshot.image.revision);
        QCOMPARE(image->coverage(), snapshot.image.coverage);
        QVERIFY(image->completedPixels > 0U);

        // A real media gap ends the continuously clocked decode as a usable
        // partial image.  The following chunk cannot silently bridge the gap.
        const QVector<short> afterGap = tone(600U, 1'900.0);
        QVERIFY(runtime.enqueuePcm16At(afterGap,
                                      kRate,
                                      token,
                                      20'000'000'000));
        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                  chunks + 1U,
                                  4'000);
        const auto partial = runtime.snapshot();
        QVERIFY(partial.image.available);
        QVERIFY(partial.image.partial);
        QVERIFY(!partial.image.complete);
        QVERIFY(partial.discontinuities >= 1U);
        QVERIFY(runtime.latestImageSnapshot() != nullptr);

        QVERIFY(runtime.resetStream());
        QCOMPARE(runtime.snapshot().image.available, false);
        QVERIFY(runtime.latestImageSnapshot() == nullptr);
        QVERIFY(runtime.stop());
    }

    void martinM2M3M4VisSelectNativeFamilySessions()
    {
        const struct {
            SstvMartinMode mode;
            const char* id;
            int vis;
            std::uint32_t height;
        } cases[] {
            {SstvMartinMode::M2, "martin-m2", 40, 256U},
            {SstvMartinMode::M3, "martin-m3", 36, 128U},
            {SstvMartinMode::M4, "martin-m4", 32, 128U},
        };

        std::uint64_t sourceHandle = 60U;
        for (const auto& item : cases) {
            SstvRxRuntime::Config config = smallConfig();
            config.ingress.maximumChunks = 16U;
            config.ingress.maximumQueuedSamples = 65'536U;
            const std::vector<std::int16_t> pcm = martinPrefix(
                item.mode, 8U);
            QVERIFY(pcm.size() > 30'000U);

            SstvRxRuntime runtime(config);
            QVERIFY(runtime.start(SstvAudioSourceKind::Replay,
                                  sourceHandle++));
            const SstvRxRouteToken token = runtime.routeToken();
            const std::size_t pattern[] {1'337U, 4'093U, 977U, 2'048U};
            std::size_t offset = 0U;
            std::size_t patternIndex = 0U;
            std::uint64_t chunks = 0U;
            while (offset < pcm.size()) {
                const std::size_t count = std::min(
                    pattern[patternIndex
                            % (sizeof(pattern) / sizeof(pattern[0]))],
                    pcm.size() - offset);
                QVERIFY(runtime.enqueuePcm16At(
                    pcmChunk(pcm, offset, count),
                    kRate,
                    token,
                    4'000'000'000));
                ++chunks;
                QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                          chunks,
                                          4'000);
                offset += count;
                ++patternIndex;
            }

            QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().vis.valid, 4'000);
            QTRY_VERIFY_WITH_TIMEOUT(
                runtime.snapshot().image.linesPublished >= 5U, 4'000);
            const auto snapshot = runtime.snapshot();
            QCOMPARE(snapshot.vis.primaryPayload, item.vis);
            QCOMPARE(snapshot.vis.mappedMode,
                     QString::fromLatin1(item.id));
            QCOMPARE(snapshot.image.mode, QString::fromLatin1(item.id));
            QCOMPARE(snapshot.image.width, std::uint32_t {320U});
            QCOMPARE(snapshot.image.height, item.height);
            QCOMPARE(snapshot.image.acquisitionId, std::uint64_t {1U});
            QVERIFY(snapshot.image.coverage > 0.015);
            QVERIFY(!snapshot.image.complete);
            QVERIFY(!snapshot.image.partial);
            QCOMPARE(snapshot.rxState, SstvRxState::Receiving);
            const auto image = runtime.latestImageSnapshot();
            QVERIFY(image != nullptr);
            QCOMPARE(image->height, item.height);
            QVERIFY(image->completedPixels > 0U);
            QVERIFY(runtime.stop());
        }
    }

    void martinBackToBackFrameInOneBoundaryChunkAutoResets()
    {
        SstvRxRuntime::Config config = smallConfig();
        config.ingress.maximumChunks = 4U;
        config.ingress.maximumQueuedSamples = 100'000U;
        config.ingress.maximumSamplesPerCall = 65'536U;
        const SstvMartinModeSpec firstSpec =
            SstvMartinM1Protocol::spec(SstvMartinMode::M4);
        const std::vector<std::int16_t> first = martinPrefix(
            SstvMartinMode::M4, firstSpec.height);
        const std::vector<std::int16_t> second = martinPrefix(
            SstvMartinMode::M3, 8U);
        SstvMartinM1Mapper mapper({kRate, 0, SstvMartinMode::M4});
        SstvTimingAccumulator headerTiming(kRate);
        const std::uint64_t expected = headerTiming.samplesFor(
            SstvMartinM1Protocol::HeaderDuration)
            + mapper.imageSampleCount();
        QCOMPARE(first.size(), static_cast<std::size_t>(expected));
        QVERIFY(second.size() > 40'000U);
        std::vector<std::int16_t> pcm;
        pcm.reserve(first.size() + second.size());
        pcm.insert(pcm.end(), first.cbegin(), first.cend());
        pcm.insert(pcm.end(), second.cbegin(), second.cend());

        constexpr std::size_t chunkSize = 65'521U;
        const std::size_t boundaryOffset = first.size() % chunkSize;
        QVERIFY(boundaryOffset != 0U);
        QVERIFY(chunkSize - boundaryOffset
                > static_cast<std::size_t>(10'920U));

        SstvRxRuntime runtime(config);
        QVERIFY(runtime.start(SstvAudioSourceKind::Replay, 64U));
        const SstvRxRouteToken token = runtime.routeToken();
        std::size_t offset = 0U;
        std::uint64_t chunks = 0U;
        while (offset < pcm.size()) {
            const std::size_t count = std::min<std::size_t>(
                chunkSize, pcm.size() - offset);
            QVERIFY(runtime.enqueuePcm16At(
                pcmChunk(pcm, offset, count),
                kRate,
                token,
                5'000'000'000));
            ++chunks;
            QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                      chunks,
                                      10'000);
            offset += count;
        }

        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().vis.mappedMode,
                                  QStringLiteral("martin-m3"),
                                  10'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            runtime.snapshot().image.linesPublished >= 5U,
            10'000);
        const auto active = runtime.snapshot();
        QVERIFY(active.vis.valid);
        QCOMPARE(active.vis.primaryPayload, 36);
        QCOMPARE(active.image.mode, QStringLiteral("martin-m3"));
        QCOMPARE(active.image.height, std::uint32_t {128U});
        QVERIFY(active.image.acquisitionId > std::uint64_t {1U});
        QVERIFY(!active.image.complete);
        QVERIFY(!active.image.partial);
        QCOMPARE(active.rxState, SstvRxState::Receiving);
        const auto image = runtime.latestImageSnapshot();
        QVERIFY(image != nullptr);
        QVERIFY(!image->isComplete());
        QVERIFY(image->completedPixels > 0U);
        QVERIFY(runtime.stop());
    }

    void scottieS2VisStartsProgressiveNativeImageSession()
    {
        SstvRxRuntime::Config config = smallConfig();
        config.ingress.maximumChunks = 16U;
        config.ingress.maximumQueuedSamples = 65'536U;
        const std::vector<std::int16_t> pcm = scottiePrefix(
            SstvScottieMode::S2, 8U);
        QVERIFY(pcm.size() > 30'000U);

        SstvRxRuntime runtime(config);
        QVERIFY(runtime.start(SstvAudioSourceKind::Replay, 45U));
        const SstvRxRouteToken token = runtime.routeToken();
        const std::size_t pattern[] {1'337U, 4'093U, 977U, 2'048U};
        std::size_t offset = 0U;
        std::size_t patternIndex = 0U;
        std::uint64_t chunks = 0U;
        while (offset < pcm.size()) {
            const std::size_t count = std::min(
                pattern[patternIndex
                        % (sizeof(pattern) / sizeof(pattern[0]))],
                pcm.size() - offset);
            QVERIFY(runtime.enqueuePcm16At(
                pcmChunk(pcm, offset, count),
                kRate,
                token,
                1'000'000'000));
            ++chunks;
            QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                      chunks,
                                      4'000);
            offset += count;
            ++patternIndex;
        }

        QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().vis.valid, 4'000);
        const auto snapshot = runtime.snapshot();
        QVERIFY(snapshot.vis.modeMapped);
        QCOMPARE(snapshot.vis.mappedMode, QStringLiteral("scottie-s2"));
        QVERIFY(snapshot.image.available);
        QCOMPARE(snapshot.image.acquisitionId, std::uint64_t {1U});
        QCOMPARE(snapshot.image.mode, QStringLiteral("scottie-s2"));
        QCOMPARE(snapshot.image.width, std::uint32_t {320U});
        QCOMPARE(snapshot.image.height, std::uint32_t {256U});
        QVERIFY(snapshot.image.linesPublished >= 5U);
        QVERIFY(snapshot.image.coverage > 0.015);
        QVERIFY(!snapshot.image.complete);
        QVERIFY(!snapshot.image.partial);
        QCOMPARE(snapshot.rxState, SstvRxState::Receiving);

        const std::shared_ptr<const SstvImageSnapshot> image =
            runtime.latestImageSnapshot();
        QVERIFY(image != nullptr);
        QCOMPARE(image->revision, snapshot.image.revision);
        QCOMPARE(image->coverage(), snapshot.image.coverage);
        QVERIFY(image->completedPixels > 0U);

        const QVector<short> afterGap = tone(600U, 1'900.0);
        QVERIFY(runtime.enqueuePcm16At(afterGap,
                                      kRate,
                                      token,
                                      20'000'000'000));
        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                  chunks + 1U,
                                  4'000);
        const auto partial = runtime.snapshot();
        QVERIFY(partial.image.available);
        QVERIFY(partial.image.partial);
        QVERIFY(!partial.image.complete);
        QCOMPARE(partial.image.mode, QStringLiteral("scottie-s2"));
        QCOMPARE(partial.rxState, SstvRxState::SearchingLeader);
        QVERIFY(partial.discontinuities >= 1U);
        QVERIFY(runtime.stop());
    }

    void scottieS3AndS4VisSelectHalfHeightNativeSessions()
    {
        const struct {
            SstvScottieMode mode;
            const char* id;
            int vis;
        } cases[] {
            {SstvScottieMode::S3, "scottie-s3", 52},
            {SstvScottieMode::S4, "scottie-s4", 48},
        };

        std::uint64_t sourceHandle = 50U;
        for (const auto& item : cases) {
            SstvRxRuntime::Config config = smallConfig();
            config.ingress.maximumChunks = 16U;
            config.ingress.maximumQueuedSamples = 65'536U;
            const std::vector<std::int16_t> pcm = scottiePrefix(
                item.mode, 8U);
            QVERIFY(pcm.size() > 25'000U);

            SstvRxRuntime runtime(config);
            QVERIFY(runtime.start(SstvAudioSourceKind::Replay,
                                  sourceHandle++));
            const SstvRxRouteToken token = runtime.routeToken();
            const std::size_t pattern[] {1'337U, 4'093U, 977U, 2'048U};
            std::size_t offset = 0U;
            std::size_t patternIndex = 0U;
            std::uint64_t chunks = 0U;
            while (offset < pcm.size()) {
                const std::size_t count = std::min(
                    pattern[patternIndex
                            % (sizeof(pattern) / sizeof(pattern[0]))],
                    pcm.size() - offset);
                QVERIFY(runtime.enqueuePcm16At(
                    pcmChunk(pcm, offset, count),
                    kRate,
                    token,
                    3'000'000'000));
                ++chunks;
                QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                          chunks,
                                          4'000);
                offset += count;
                ++patternIndex;
            }

            QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().vis.valid, 4'000);
            QTRY_VERIFY_WITH_TIMEOUT(
                runtime.snapshot().image.linesPublished >= 5U, 4'000);
            const auto snapshot = runtime.snapshot();
            QCOMPARE(snapshot.vis.primaryPayload, item.vis);
            QCOMPARE(snapshot.vis.mappedMode,
                     QString::fromLatin1(item.id));
            QCOMPARE(snapshot.image.mode, QString::fromLatin1(item.id));
            QCOMPARE(snapshot.image.width, std::uint32_t {320U});
            QCOMPARE(snapshot.image.height, std::uint32_t {128U});
            QCOMPARE(snapshot.image.acquisitionId, std::uint64_t {1U});
            QVERIFY(snapshot.image.coverage > 0.03);
            QVERIFY(!snapshot.image.complete);
            QCOMPARE(snapshot.rxState, SstvRxState::Receiving);
            const auto image = runtime.latestImageSnapshot();
            QVERIFY(image != nullptr);
            QCOMPARE(image->height, std::uint32_t {128U});
            QVERIFY(runtime.stop());
        }
    }

    void scottieBackToBackFrameInOneBoundaryChunkAutoResets()
    {
        SstvRxRuntime::Config config = smallConfig();
        config.ingress.maximumChunks = 4U;
        config.ingress.maximumQueuedSamples = 100'000U;
        config.ingress.maximumSamplesPerCall = 65'536U;
        const std::vector<std::int16_t> first = scottiePrefix(
            SstvScottieMode::S2, SstvScottieProtocol::Height);
        const std::vector<std::int16_t> second = scottiePrefix(
            SstvScottieMode::S1, 8U);
        SstvScottieMapper mapper({SstvScottieMode::S2, kRate, 0});
        SstvTimingAccumulator headerTiming(kRate);
        const std::uint64_t expected = headerTiming.samplesFor(
            SstvScottieProtocol::HeaderDuration)
            + mapper.imageSampleCount();
        QCOMPARE(first.size(), static_cast<std::size_t>(expected));
        QVERIFY(second.size() > 40'000U);
        std::vector<std::int16_t> pcm;
        pcm.reserve(first.size() + second.size());
        pcm.insert(pcm.end(), first.cbegin(), first.cend());
        pcm.insert(pcm.end(), second.cbegin(), second.cend());

        // The final S2 samples and the next S1 leader deliberately share one
        // worker chunk, exercising terminal leader acceptance rather than a
        // later Tick-driven reset at a chunk boundary.
        constexpr std::size_t chunkSize = 65'521U;
        const std::size_t boundaryOffset = first.size() % chunkSize;
        QVERIFY(boundaryOffset != 0U);
        QVERIFY(chunkSize - boundaryOffset
                > static_cast<std::size_t>(10'920U));

        SstvRxRuntime runtime(config);
        QVERIFY(runtime.start(SstvAudioSourceKind::Replay, 46U));
        const SstvRxRouteToken token = runtime.routeToken();
        std::size_t offset = 0U;
        std::uint64_t chunks = 0U;
        while (offset < pcm.size()) {
            const std::size_t count = std::min<std::size_t>(
                chunkSize, pcm.size() - offset);
            QVERIFY(runtime.enqueuePcm16At(
                pcmChunk(pcm, offset, count),
                kRate,
                token,
                2'000'000'000));
            ++chunks;
            QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                      chunks,
                                      10'000);
            offset += count;
        }

        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().vis.mappedMode,
                                  QStringLiteral("scottie-s1"),
                                  10'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            runtime.snapshot().image.linesPublished >= 5U,
            10'000);
        const auto active = runtime.snapshot();
        QVERIFY(active.vis.valid);
        QCOMPARE(active.vis.primaryPayload, 60);
        QCOMPARE(active.image.mode, QStringLiteral("scottie-s1"));
        QVERIFY(active.image.acquisitionId > std::uint64_t {1U});
        QVERIFY(!active.image.complete);
        QVERIFY(!active.image.partial);
        QCOMPARE(active.rxState, SstvRxState::Receiving);
        const auto image = runtime.latestImageSnapshot();
        QVERIFY(image != nullptr);
        QVERIFY(!image->isComplete());
        QVERIFY(image->completedPixels > 0U);
        QVERIFY(runtime.stop());
    }

    void robotVisSelectsEveryNativeFamilySessionAndPhysicalAlias()
    {
        const struct {
            SstvRobotMode mode;
            const char* id;
            std::uint32_t width;
            std::uint32_t height;
            std::array<std::uint8_t, 3U> payloads;
            std::size_t payloadCount;
        } cases[] {
            {SstvRobotMode::Colour12,
             "robot-c12", 160U, 120U, {{0U, 0U, 0U}}, 1U},
            {SstvRobotMode::Colour24,
             "robot-c24", 320U, 120U, {{4U, 0U, 0U}}, 1U},
            {SstvRobotMode::Colour36,
             "robot-c36", 320U, 240U, {{8U, 0U, 0U}}, 1U},
            {SstvRobotMode::Colour72,
             "robot-c72", 320U, 240U, {{12U, 0U, 0U}}, 1U},
            {SstvRobotMode::Bw8,
             "robot-bw8", 160U, 120U, {{2U, 1U, 3U}}, 3U},
            {SstvRobotMode::Bw12,
             "robot-bw12", 160U, 120U, {{6U, 5U, 7U}}, 3U},
            {SstvRobotMode::Bw24,
             "robot-bw24", 320U, 240U, {{10U, 9U, 11U}}, 3U},
            {SstvRobotMode::Bw36,
             "robot-bw36", 320U, 240U, {{14U, 13U, 15U}}, 3U},
        };

        std::uint64_t sourceHandle = 70U;
        for (const auto& item : cases) {
            for (std::size_t payloadIndex = 0U;
                 payloadIndex < item.payloadCount;
                 ++payloadIndex) {
                const std::uint8_t payload = item.payloads[payloadIndex];
                SstvRxRuntime::Config config = smallConfig();
                config.ingress.maximumChunks = 16U;
                config.ingress.maximumQueuedSamples = 65'536U;
                const std::optional<std::uint8_t> visOverride =
                    payloadIndex == 0U
                    ? std::nullopt
                    : std::optional<std::uint8_t> {payload};
                const std::vector<std::int16_t> pcm = robotPrefix(
                    item.mode, 8U, visOverride);
                QVERIFY(pcm.size() > 15'000U);

                SstvRxRuntime runtime(config);
                QVERIFY(runtime.start(SstvAudioSourceKind::Replay,
                                      sourceHandle++));
                const SstvRxRouteToken token = runtime.routeToken();
                const std::size_t pattern[] {
                    1'337U, 4'093U, 977U, 2'048U};
                std::size_t offset = 0U;
                std::size_t patternIndex = 0U;
                std::uint64_t chunks = 0U;
                while (offset < pcm.size()) {
                    const std::size_t count = std::min(
                        pattern[patternIndex
                                % (sizeof(pattern) / sizeof(pattern[0]))],
                        pcm.size() - offset);
                    QVERIFY(runtime.enqueuePcm16At(
                        pcmChunk(pcm, offset, count),
                        kRate,
                        token,
                        6'000'000'000));
                    ++chunks;
                    QTRY_COMPARE_WITH_TIMEOUT(
                        runtime.snapshot().chunksProcessed,
                        chunks,
                        4'000);
                    offset += count;
                    ++patternIndex;
                }

                QTRY_VERIFY_WITH_TIMEOUT(
                    runtime.snapshot().vis.valid, 4'000);
                QTRY_VERIFY_WITH_TIMEOUT(
                    runtime.snapshot().image.linesPublished >= 5U,
                    4'000);
                const auto snapshot = runtime.snapshot();
                QCOMPARE(snapshot.vis.primaryPayload,
                         static_cast<int>(payload));
                QCOMPARE(snapshot.vis.mappedMode,
                         QString::fromLatin1(item.id));
                QVERIFY(snapshot.vis.modeMapped);
                QVERIFY(snapshot.image.available);
                QCOMPARE(snapshot.image.mode,
                         QString::fromLatin1(item.id));
                QCOMPARE(snapshot.image.width, item.width);
                QCOMPARE(snapshot.image.height, item.height);
                QCOMPARE(snapshot.image.acquisitionId,
                         std::uint64_t {1U});
                QVERIFY(snapshot.image.coverage > 0.015);
                QVERIFY(!snapshot.image.complete);
                QVERIFY(!snapshot.image.partial);
                QCOMPARE(snapshot.rxState, SstvRxState::Receiving);
                const auto image = runtime.latestImageSnapshot();
                QVERIFY(image != nullptr);
                QCOMPARE(image->width, item.width);
                QCOMPARE(image->height, item.height);
                QVERIFY(image->completedPixels > 0U);
                QVERIFY(runtime.stop());
            }
        }
    }

    void sequentialRgbVisSelectsEveryNativeFamilySession()
    {
        const struct {
            SstvSequentialRgbMode mode;
            const char* id;
            std::uint32_t width;
            std::uint32_t height;
            int payload;
        } cases[] {
            {SstvSequentialRgbMode::WraaseSc2_60,
             "wraase-sc2-60", 320U, 256U, 59},
            {SstvSequentialRgbMode::WraaseSc2_120,
             "wraase-sc2-120", 320U, 256U, 63},
            {SstvSequentialRgbMode::WraaseSc2_180,
             "wraase-sc2-180", 320U, 256U, 55},
            {SstvSequentialRgbMode::PasokonP3,
             "pasokon-p3", 640U, 496U, 113},
            {SstvSequentialRgbMode::PasokonP5,
             "pasokon-p5", 640U, 496U, 114},
            {SstvSequentialRgbMode::PasokonP7,
             "pasokon-p7", 640U, 496U, 115},
        };

        std::uint64_t sourceHandle = 100U;
        for (const auto& item : cases) {
            SstvRxRuntime::Config config = smallConfig();
            config.ingress.maximumChunks = 16U;
            config.ingress.maximumQueuedSamples = 65'536U;
            const std::vector<std::int16_t> pcm = sequentialRgbPrefix(
                item.mode, 8U);
            QVERIFY(pcm.size() > 30'000U);

            SstvRxRuntime runtime(config);
            QVERIFY(runtime.start(SstvAudioSourceKind::Replay,
                                  sourceHandle++));
            const SstvRxRouteToken token = runtime.routeToken();
            const std::size_t pattern[] {
                1'337U, 4'093U, 977U, 2'048U};
            std::size_t offset = 0U;
            std::size_t patternIndex = 0U;
            std::uint64_t chunks = 0U;
            while (offset < pcm.size()) {
                const std::size_t count = std::min(
                    pattern[patternIndex
                            % (sizeof(pattern) / sizeof(pattern[0]))],
                    pcm.size() - offset);
                QVERIFY(runtime.enqueuePcm16At(
                    pcmChunk(pcm, offset, count),
                    kRate,
                    token,
                    8'000'000'000));
                ++chunks;
                QTRY_COMPARE_WITH_TIMEOUT(
                    runtime.snapshot().chunksProcessed,
                    chunks,
                    4'000);
                offset += count;
                ++patternIndex;
            }

            QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().vis.valid, 4'000);
            QTRY_VERIFY_WITH_TIMEOUT(
                runtime.snapshot().image.linesPublished >= 5U,
                4'000);
            const auto snapshot = runtime.snapshot();
            QCOMPARE(snapshot.vis.primaryPayload, item.payload);
            QCOMPARE(snapshot.vis.mappedMode,
                     QString::fromLatin1(item.id));
            QVERIFY(snapshot.vis.modeMapped);
            QVERIFY(snapshot.image.available);
            QCOMPARE(snapshot.image.mode, QString::fromLatin1(item.id));
            QCOMPARE(snapshot.image.width, item.width);
            QCOMPARE(snapshot.image.height, item.height);
            QCOMPARE(snapshot.image.acquisitionId, std::uint64_t {1U});
            QVERIFY(snapshot.image.coverage > 0.01);
            QVERIFY(!snapshot.image.complete);
            QVERIFY(!snapshot.image.partial);
            QCOMPARE(snapshot.rxState, SstvRxState::Receiving);
            const auto image = runtime.latestImageSnapshot();
            QVERIFY(image != nullptr);
            QCOMPARE(image->width, item.width);
            QCOMPARE(image->height, item.height);
            QVERIFY(image->completedPixels > 0U);
            QVERIFY(runtime.stop());
        }
    }

    void pdVisSelectsEveryNativeLinePairSession()
    {
        const struct {
            SstvPdMode mode;
            const char* id;
            std::uint32_t width;
            std::uint32_t height;
            int payload;
        } cases[] {
            {SstvPdMode::Pd50, "pd-50", 320U, 256U, 93},
            {SstvPdMode::Pd90, "pd-90", 320U, 256U, 99},
            {SstvPdMode::Pd120, "pd-120", 640U, 496U, 95},
            {SstvPdMode::Pd160, "pd-160", 512U, 400U, 98},
            {SstvPdMode::Pd180, "pd-180", 640U, 496U, 96},
            {SstvPdMode::Pd240, "pd-240", 640U, 496U, 97},
            {SstvPdMode::Pd290, "pd-290", 800U, 616U, 94},
        };

        std::uint64_t sourceHandle = 140U;
        for (const auto& item : cases) {
            SstvRxRuntime::Config config = smallConfig();
            config.ingress.maximumChunks = 16U;
            config.ingress.maximumQueuedSamples = 65'536U;
            const std::vector<std::int16_t> pcm = pdPrefix(item.mode, 4U);
            QVERIFY(pcm.size() > 20'000U);

            SstvRxRuntime runtime(config);
            QVERIFY(runtime.start(SstvAudioSourceKind::Replay,
                                  sourceHandle++));
            const SstvRxRouteToken token = runtime.routeToken();
            const std::size_t pattern[] {
                1'117U, 4'091U, 983U, 2'053U};
            std::size_t offset = 0U;
            std::size_t patternIndex = 0U;
            std::uint64_t chunks = 0U;
            while (offset < pcm.size()) {
                const std::size_t count = std::min(
                    pattern[patternIndex
                            % (sizeof(pattern) / sizeof(pattern[0]))],
                    pcm.size() - offset);
                QVERIFY(runtime.enqueuePcm16At(
                    pcmChunk(pcm, offset, count),
                    kRate,
                    token,
                    9'000'000'000));
                ++chunks;
                QTRY_COMPARE_WITH_TIMEOUT(
                    runtime.snapshot().chunksProcessed,
                    chunks,
                    4'000);
                offset += count;
                ++patternIndex;
            }

            QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().vis.valid, 4'000);
            QTRY_VERIFY_WITH_TIMEOUT(
                runtime.snapshot().image.linesPublished >= 4U,
                4'000);
            const auto snapshot = runtime.snapshot();
            QCOMPARE(snapshot.vis.primaryPayload, item.payload);
            QCOMPARE(snapshot.vis.mappedMode,
                     QString::fromLatin1(item.id));
            QVERIFY(snapshot.vis.modeMapped);
            QVERIFY(snapshot.image.available);
            QCOMPARE(snapshot.image.mode, QString::fromLatin1(item.id));
            QCOMPARE(snapshot.image.width, item.width);
            QCOMPARE(snapshot.image.height, item.height);
            QCOMPARE(snapshot.image.acquisitionId, std::uint64_t {1U});
            QVERIFY(snapshot.image.coverage > 0.005);
            QVERIFY(!snapshot.image.complete);
            QVERIFY(!snapshot.image.partial);
            QCOMPARE(snapshot.rxState, SstvRxState::Receiving);
            const auto image = runtime.latestImageSnapshot();
            QVERIFY(image != nullptr);
            QCOMPARE(image->width, item.width);
            QCOMPARE(image->height, item.height);
            QVERIFY(image->completedPixels > 0U);
            QVERIFY(runtime.stop());
        }
    }

    void avtNormalVisAndProtectedCountdownSelectEveryNativeSession()
    {
        const struct {
            SstvAvtMode mode;
            const char* id;
            std::uint32_t width;
            std::uint32_t height;
            int payload;
        } cases[] {
            {SstvAvtMode::Avt24, "avt-24", 128U, 120U, 64},
            {SstvAvtMode::Avt90, "avt-90", 320U, 240U, 68},
            {SstvAvtMode::Avt94, "avt-94", 320U, 200U, 72},
        };

        std::uint64_t sourceHandle = 170U;
        for (const auto& item : cases) {
            SstvRxRuntime::Config config = smallConfig();
            config.ingress.maximumChunks = 16U;
            config.ingress.maximumQueuedSamples = 65'536U;
            const std::vector<std::int16_t> pcm = avtPrefix(item.mode, 8U);
            QVERIFY(pcm.size() > 100'000U);

            SstvRxRuntime runtime(config);
            QVERIFY(runtime.start(SstvAudioSourceKind::Replay,
                                  sourceHandle++));
            const SstvRxRouteToken token = runtime.routeToken();
            const std::size_t pattern[] {
                1'117U, 4'091U, 983U, 2'053U};
            std::size_t offset = 0U;
            std::size_t patternIndex = 0U;
            std::uint64_t chunks = 0U;
            while (offset < pcm.size()) {
                const std::size_t count = std::min(
                    pattern[patternIndex
                            % (sizeof(pattern) / sizeof(pattern[0]))],
                    pcm.size() - offset);
                QVERIFY(runtime.enqueuePcm16At(
                    pcmChunk(pcm, offset, count),
                    kRate,
                    token,
                    9'500'000'000));
                ++chunks;
                QTRY_COMPARE_WITH_TIMEOUT(
                    runtime.snapshot().chunksProcessed,
                    chunks,
                    4'000);
                offset += count;
                ++patternIndex;
            }

            QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().vis.valid, 4'000);
            QTRY_VERIFY_WITH_TIMEOUT(
                runtime.snapshot().image.linesPublished >= 5U,
                4'000);
            const auto snapshot = runtime.snapshot();
            QCOMPARE(snapshot.vis.format, SstvVisFormat::Standard);
            QCOMPARE(snapshot.vis.primaryPayload, item.payload);
            QVERIFY(snapshot.vis.modeMapped);
            QCOMPARE(snapshot.vis.mappedMode,
                     QString::fromLatin1(item.id));
            QVERIFY(snapshot.image.available);
            QCOMPARE(snapshot.image.mode, QString::fromLatin1(item.id));
            QCOMPARE(snapshot.image.width, item.width);
            QCOMPARE(snapshot.image.height, item.height);
            QCOMPARE(snapshot.image.acquisitionId, std::uint64_t {1U});
            QVERIFY(snapshot.image.coverage > 0.015);
            QVERIFY(!snapshot.image.complete);
            QVERIFY(!snapshot.image.partial);
            QCOMPARE(snapshot.rxState, SstvRxState::Receiving);
            const auto image = runtime.latestImageSnapshot();
            QVERIFY(image != nullptr);
            QCOMPARE(image->width, item.width);
            QCOMPARE(image->height, item.height);
            QVERIFY(image->completedPixels > 0U);
            QVERIFY(runtime.stop());
        }
    }

    void avt24FullPcmLoopbackCompletesProgressiveRuntimeImage()
    {
        SstvRxRuntime::Config config = smallConfig();
        config.ingress.maximumChunks = 4U;
        config.ingress.maximumQueuedSamples = 131'072U;
        config.ingress.maximumSamplesPerCall = 65'536U;
        std::vector<std::int16_t> pcm = avtPrefix(
            SstvAvtMode::Avt24, 120U);
        QCOMPARE(pcm.size(), std::size_t {366'510U});
        // Keep the end-of-image observation inside the final worker chunk so
        // the demodulator analysis window closes the last pixel/line without
        // creating a discontinuity or a second Tick-driven terminal reset.
        pcm.insert(pcm.end(), 256U, std::int16_t {0});

        SstvRxRuntime runtime(config);
        QVERIFY(runtime.start(SstvAudioSourceKind::Replay, 179U));
        const SstvRxRouteToken token = runtime.routeToken();
        constexpr std::size_t chunkSize = 60'001U;
        std::size_t offset = 0U;
        std::uint64_t chunks = 0U;
        while (offset < pcm.size()) {
            const std::size_t count = std::min(
                chunkSize, pcm.size() - offset);
            QVERIFY(runtime.enqueuePcm16At(
                pcmChunk(pcm, offset, count),
                kRate,
                token,
                9'750'000'000));
            ++chunks;
            QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                      chunks,
                                      10'000);
            offset += count;
        }

        QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().image.complete, 10'000);
        const auto snapshot = runtime.snapshot();
        QCOMPARE(snapshot.vis.mappedMode, QStringLiteral("avt-24"));
        QCOMPARE(snapshot.image.mode, QStringLiteral("avt-24"));
        QCOMPARE(snapshot.image.linesPublished, std::uint64_t {120U});
        QCOMPARE(snapshot.image.width, std::uint32_t {128U});
        QCOMPARE(snapshot.image.height, std::uint32_t {120U});
        QCOMPARE(snapshot.rxState, SstvRxState::Completed);
        const auto image = runtime.latestImageSnapshot();
        QVERIFY(image != nullptr);
        QVERIFY(image->isComplete());
        QCOMPARE(image->completedPixels, std::size_t {128U * 120U});
        // Check interior pixels: the first/last analysis windows intentionally
        // straddle the protected-header and end-of-frame boundaries, while
        // the bounded decoder still completes those edge pixels.
        for (const auto coordinate : {
                 std::pair<std::uint32_t, std::uint32_t> {16U, 1U},
                 std::pair<std::uint32_t, std::uint32_t> {64U, 60U},
                 std::pair<std::uint32_t, std::uint32_t> {112U, 118U}}) {
            const SstvRgbPixel pixel = image->pixel(
                coordinate.first, coordinate.second);
            QVERIFY(std::abs(static_cast<int>(pixel.red) - 210) <= 10);
            QVERIFY(std::abs(static_cast<int>(pixel.green) - 70) <= 10);
            QVERIFY(std::abs(static_cast<int>(pixel.blue) - 145) <= 10);
        }
        QVERIFY(runtime.stop());
    }

    void avtSummaryGuardClearsOnResetAndDoesNotMaskLaterVis()
    {
        SstvRxRuntime::Config config = smallConfig();
        config.ingress.maximumChunks = 16U;
        config.ingress.maximumQueuedSamples = 65'536U;
        SstvRxRuntime runtime(config);
        QVERIFY(runtime.start(SstvAudioSourceKind::Replay, 178U));

        const auto feedGeneration = [&](
            const std::vector<std::int16_t>& pcm,
            SstvRxRouteToken token,
            qint64 timestampNs) {
            std::size_t offset = 0U;
            std::uint64_t chunks = 0U;
            while (offset < pcm.size()) {
                const std::size_t count = std::min<std::size_t>(
                    4'093U, pcm.size() - offset);
                if (!runtime.enqueuePcm16At(
                        pcmChunk(pcm, offset, count),
                        kRate,
                        token,
                        timestampNs)) {
                    return false;
                }
                ++chunks;
                QElapsedTimer timer;
                timer.start();
                while (runtime.snapshot().generationChunksProcessed < chunks
                       && timer.elapsed() < 4'000) {
                    QTest::qWait(1);
                }
                if (runtime.snapshot().generationChunksProcessed != chunks) {
                    return false;
                }
                offset += count;
            }
            return true;
        };

        const SstvRxRouteToken avtToken = runtime.routeToken();
        QVERIFY(feedGeneration(avtPrefix(SstvAvtMode::Avt24, 8U),
                               avtToken,
                               9'900'000'000));
        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().vis.mappedMode,
                                  QStringLiteral("avt-24"),
                                  4'000);
        QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().image.available, 4'000);

        QVERIFY(runtime.resetStream());
        const SstvRxRouteToken martinToken = runtime.routeToken();
        QVERIFY(martinToken.generation != avtToken.generation);
        QVERIFY(feedGeneration(martinPrefix(SstvMartinMode::M4, 8U),
                               martinToken,
                               10'500'000'000));
        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().vis.mappedMode,
                                  QStringLiteral("martin-m4"),
                                  4'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            runtime.snapshot().image.linesPublished >= 5U,
            4'000);
        const auto snapshot = runtime.snapshot();
        QCOMPARE(snapshot.vis.primaryPayload, 32);
        QCOMPARE(snapshot.image.mode, QStringLiteral("martin-m4"));
        QCOMPARE(snapshot.rxState, SstvRxState::Receiving);
        QVERIFY(runtime.stop());
    }

    void mmsstvExtendedVisSelectsEveryNativeSession()
    {
        constexpr std::array<SstvMmsstvMode, 19U> modes {{
            SstvMmsstvMode::Mp73,
            SstvMmsstvMode::Mp115,
            SstvMmsstvMode::Mp140,
            SstvMmsstvMode::Mp175,
            SstvMmsstvMode::Mr73,
            SstvMmsstvMode::Mr90,
            SstvMmsstvMode::Mr115,
            SstvMmsstvMode::Mr140,
            SstvMmsstvMode::Mr175,
            SstvMmsstvMode::Ml180,
            SstvMmsstvMode::Ml240,
            SstvMmsstvMode::Ml280,
            SstvMmsstvMode::Ml320,
            SstvMmsstvMode::Mp73Narrow,
            SstvMmsstvMode::Mp110Narrow,
            SstvMmsstvMode::Mp140Narrow,
            SstvMmsstvMode::Mc110Narrow,
            SstvMmsstvMode::Mc140Narrow,
            SstvMmsstvMode::Mc180Narrow,
        }};

        std::uint64_t sourceHandle = 180U;
        for (const SstvMmsstvMode mode : modes) {
            const SstvMmsstvModeSpec spec = SstvMmsstvProtocol::spec(mode);
            SstvRxRuntime::Config config = smallConfig();
            config.ingress.maximumChunks = 16U;
            config.ingress.maximumQueuedSamples = 65'536U;
            const std::vector<std::int16_t> pcm = mmsstvPrefix(mode, 8U);
            QVERIFY(pcm.size() > 20'000U);

            SstvRxRuntime runtime(config);
            QVERIFY(runtime.start(SstvAudioSourceKind::Replay,
                                  sourceHandle++));
            const SstvRxRouteToken token = runtime.routeToken();
            const std::size_t pattern[] {
                1'117U, 4'091U, 983U, 2'053U};
            std::size_t offset = 0U;
            std::size_t patternIndex = 0U;
            std::uint64_t chunks = 0U;
            while (offset < pcm.size()) {
                const std::size_t count = std::min(
                    pattern[patternIndex
                            % (sizeof(pattern) / sizeof(pattern[0]))],
                    pcm.size() - offset);
                QVERIFY(runtime.enqueuePcm16At(
                    pcmChunk(pcm, offset, count),
                    kRate,
                    token,
                    10'000'000'000));
                ++chunks;
                QTRY_COMPARE_WITH_TIMEOUT(
                    runtime.snapshot().chunksProcessed,
                    chunks,
                    4'000);
                offset += count;
                ++patternIndex;
            }

            QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot().vis.valid, 4'000);
            QTRY_VERIFY_WITH_TIMEOUT(
                runtime.snapshot().image.linesPublished >= 3U,
                4'000);
            const auto snapshot = runtime.snapshot();
            QCOMPARE(snapshot.vis.format,
                     spec.narrow ? SstvVisFormat::Narrow
                                 : SstvVisFormat::Extended);
            QCOMPARE(snapshot.vis.primaryPayload,
                     spec.narrow ? static_cast<int>(spec.visWireCode)
                                 : 0x23);
            if (spec.narrow) {
                QCOMPARE(snapshot.vis.extensionPayload, -1);
            } else {
                QCOMPARE(snapshot.vis.extensionPayload,
                         static_cast<int>(spec.visWireCode & 0x7fU));
            }
            QVERIFY(snapshot.vis.modeMapped);
            QCOMPARE(snapshot.vis.mappedMode,
                     QString::fromLatin1(spec.stableId));
            QVERIFY(snapshot.image.available);
            QCOMPARE(snapshot.image.mode, QString::fromLatin1(spec.stableId));
            QCOMPARE(snapshot.image.width, spec.width);
            QCOMPARE(snapshot.image.height, spec.height);
            QVERIFY(snapshot.image.acquisitionId >= std::uint64_t {1U});
            QVERIFY(snapshot.image.coverage > 0.002);
            QVERIFY(!snapshot.image.complete);
            QVERIFY(!snapshot.image.partial);
            QCOMPARE(snapshot.rxState, SstvRxState::Receiving);
            const auto image = runtime.latestImageSnapshot();
            QVERIFY(image != nullptr);
            QCOMPARE(image->width, spec.width);
            QCOMPARE(image->height, spec.height);
            QVERIFY(image->completedPixels > 0U);
            QVERIFY(runtime.stop());
        }
    }

    void robotBackToBackFrameInOneBoundaryChunkAutoResets()
    {
        SstvRxRuntime::Config config = smallConfig();
        config.ingress.maximumChunks = 4U;
        config.ingress.maximumQueuedSamples = 100'000U;
        config.ingress.maximumSamplesPerCall = 65'536U;
        const SstvRobotModeSpec firstSpec = SstvRobotProtocol::spec(
            SstvRobotMode::Bw8);
        const std::vector<std::int16_t> first = robotPrefix(
            SstvRobotMode::Bw8, firstSpec.height);
        const std::vector<std::int16_t> second = robotPrefix(
            SstvRobotMode::Colour12, 8U);
        SstvRobotMapper mapper({SstvRobotMode::Bw8, kRate, 0});
        SstvTimingAccumulator headerTiming(kRate);
        const std::uint64_t expected = headerTiming.samplesFor(
            SstvRobotProtocol::HeaderDuration)
            + mapper.imageSampleCount();
        QCOMPARE(first.size(), static_cast<std::size_t>(expected));
        QVERIFY(second.size() > 15'000U);
        std::vector<std::int16_t> pcm;
        pcm.reserve(first.size() + second.size());
        pcm.insert(pcm.end(), first.cbegin(), first.cend());
        pcm.insert(pcm.end(), second.cbegin(), second.cend());

        constexpr std::size_t chunkSize = 65'521U;
        const std::size_t boundaryOffset = first.size() % chunkSize;
        QVERIFY(boundaryOffset != 0U);
        QVERIFY(chunkSize - boundaryOffset
                > static_cast<std::size_t>(10'920U));

        SstvRxRuntime runtime(config);
        QVERIFY(runtime.start(SstvAudioSourceKind::Replay, 90U));
        const SstvRxRouteToken token = runtime.routeToken();
        std::size_t offset = 0U;
        std::uint64_t chunks = 0U;
        while (offset < pcm.size()) {
            const std::size_t count = std::min<std::size_t>(
                chunkSize, pcm.size() - offset);
            QVERIFY(runtime.enqueuePcm16At(
                pcmChunk(pcm, offset, count),
                kRate,
                token,
                7'000'000'000));
            ++chunks;
            QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                                      chunks,
                                      10'000);
            offset += count;
        }

        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().vis.mappedMode,
                                  QStringLiteral("robot-c12"),
                                  10'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            runtime.snapshot().image.linesPublished >= 5U,
            10'000);
        const auto active = runtime.snapshot();
        QVERIFY(active.vis.valid);
        QCOMPARE(active.vis.primaryPayload, 0);
        QCOMPARE(active.image.mode, QStringLiteral("robot-c12"));
        QCOMPARE(active.image.width, std::uint32_t {160U});
        QCOMPARE(active.image.height, std::uint32_t {120U});
        QVERIFY(active.image.acquisitionId > std::uint64_t {1U});
        QVERIFY(!active.image.complete);
        QVERIFY(!active.image.partial);
        QCOMPARE(active.rxState, SstvRxState::Receiving);
        const auto image = runtime.latestImageSnapshot();
        QVERIFY(image != nullptr);
        QVERIFY(!image->isComplete());
        QVERIFY(image->completedPixels > 0U);
        QVERIFY(runtime.stop());
    }
};

QTEST_GUILESS_MAIN(TestSstvRxRuntime)
#include "test_sstv_rx_runtime.moc"
