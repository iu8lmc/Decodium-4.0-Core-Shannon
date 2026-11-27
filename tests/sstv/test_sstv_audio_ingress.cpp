// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/integration/SstvAudioIngress.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QMetaType>
#include <QSignalSpy>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr int kSampleRate = 12'000;

qint64 durationNs(qsizetype sampleCount, int sampleRate)
{
    constexpr qint64 nanosecondsPerSecond = 1'000'000'000LL;
    const qint64 numerator = static_cast<qint64>(sampleCount)
        * nanosecondsPerSecond;
    return numerator / sampleRate
        + (numerator % sampleRate == 0 ? 0 : 1);
}

QVector<short> smallPcm(short first = 100)
{
    return {first, static_cast<short>(first + 1),
            static_cast<short>(first + 2)};
}

bool enqueue(SstvAudioIngress& ingress,
             QVector<short> samples,
             SstvAudioSource source,
             qint64 timestampNs,
             std::uint64_t generation,
             int sampleRate = kSampleRate)
{
    return ingress.enqueuePcm16(
        std::move(samples),
        sampleRate,
        source.kind,
        static_cast<quint32>(source.streamId),
        timestampNs,
        static_cast<quint64>(generation));
}

class RealAudioSource final : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

Q_SIGNALS:
    // This is the same by-value PCM-only signature used by DecodiumAudioSink
    // and the legacy backend fanout.  Metadata is supplied by the relay.
    void audioSamplesReady(QVector<short> samples);
};

class BoundedMetadataRelay final : public QObject
{
    Q_OBJECT

public:
    BoundedMetadataRelay(SstvAudioIngress& ingress,
                         SstvPcm16Metadata metadata,
                         qint64 firstTimestampNs,
                         QObject* parent = nullptr)
        : QObject(parent)
        , m_ingress(ingress)
        , m_metadata(metadata)
        , m_nextTimestampNs(firstTimestampNs)
    {
    }

    void relay(QVector<short> samples)
    {
        SstvPcm16Metadata event = m_metadata;
        event.monotonicTimestampNs = m_nextTimestampNs;
        m_nextTimestampNs += durationNs(samples.size(), event.sampleRate);
        ++m_forwarded;
        if (m_ingress.enqueuePcm16(std::move(samples), event)) {
            ++m_accepted;
        }
    }

    std::uint64_t forwarded() const noexcept
    {
        return m_forwarded;
    }

    std::uint64_t accepted() const noexcept
    {
        return m_accepted;
    }

private:
    SstvAudioIngress& m_ingress;
    SstvPcm16Metadata m_metadata;
    qint64 m_nextTimestampNs {0};
    std::uint64_t m_forwarded {0U};
    std::uint64_t m_accepted {0U};
};

struct WaitOutcome final
{
    SstvAudioIngress::WaitResult result {
        SstvAudioIngress::WaitResult::Cancelled};
    SstvPcm16Chunk chunk;
};

} // namespace

class TestSstvAudioIngress final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<QVector<short>>("QVector<short>");
        qRegisterMetaType<SstvAudioSourceKind>();
        QVERIFY(QMetaType::fromType<QVector<short>>().isValid());
        QVERIFY(QMetaType::fromType<SstvAudioSourceKind>().isValid());
    }

    void sourceIdentity_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<quint32>("streamId");
        QTest::newRow("local-sound-card")
            << static_cast<int>(SstvAudioSourceKind::LocalSoundCard) << 1U;
        QTest::newRow("legacy-backend")
            << static_cast<int>(SstvAudioSourceKind::LegacyBackend) << 2U;
        QTest::newRow("deco-port")
            << static_cast<int>(SstvAudioSourceKind::DecoPort) << 7U;
        QTest::newRow("tci")
            << static_cast<int>(SstvAudioSourceKind::Tci) << 19U;
        QTest::newRow("rtl-sdr")
            << static_cast<int>(SstvAudioSourceKind::RtlSdr) << 42U;
        QTest::newRow("offline-replay")
            << static_cast<int>(SstvAudioSourceKind::Replay) << 99U;
    }

    void sourceIdentity()
    {
        QFETCH(int, kind);
        QFETCH(quint32, streamId);
        const auto sourceKind = static_cast<SstvAudioSourceKind>(kind);
        const SstvAudioSource source {
            sourceKind, static_cast<std::uint32_t>(streamId)};

        SstvAudioIngress ingress;
        QVERIFY(ingress.activateSource(sourceKind, streamId));
        const std::uint64_t generation = ingress.generation();
        QVERIFY(generation != 0U);
        QVERIFY(enqueue(ingress,
                        smallPcm(),
                        source,
                        123'456,
                        generation));

        SstvPcm16Chunk chunk;
        QVERIFY(ingress.tryPop(chunk));
        QVERIFY(chunk.source == source);
        QCOMPARE(chunk.sampleRate, std::uint32_t {12'000U});
        QCOMPARE(chunk.startTime.count(), std::int64_t {123'456});
        QCOMPARE(chunk.sequence, std::uint64_t {0U});
        QCOMPARE(chunk.generation, generation);
        QCOMPARE(chunk.samples, smallPcm());
        QVERIFY(!ingress.tryPop(chunk));
    }

    void pcm16IsPreservedExactly()
    {
        const SstvAudioSource source {
            SstvAudioSourceKind::LocalSoundCard, 3U};
        SstvAudioIngress ingress;
        QVERIFY(ingress.activateSource(source.kind, source.streamId));

        const QVector<short> pcm {
            std::numeric_limits<short>::min(), -16'384, -1, 0, 1, 16'384,
            std::numeric_limits<short>::max()};
        QVERIFY(enqueue(ingress, pcm, source, 0, ingress.generation()));

        SstvPcm16Chunk chunk;
        QVERIFY(ingress.tryPop(chunk));
        QCOMPARE(chunk.samples, pcm);
        QCOMPARE(chunk.samples.constData()[0],
                 std::numeric_limits<short>::min());
        QCOMPARE(chunk.samples.constData()[6],
                 std::numeric_limits<short>::max());
    }

    void inactivePathDoesNotQueueOrDetachPayload()
    {
        SstvAudioIngress ingress;
        QVector<short> pcm(
            static_cast<qsizetype>(SstvAudioIngress::kHardMaximumSamplesPerCall),
            17);
        const short* const originalStorage = pcm.constData();
        const qsizetype originalCapacity = pcm.capacity();

        QVERIFY(!enqueue(ingress,
                         pcm,
                         {SstvAudioSourceKind::LocalSoundCard, 1U},
                         -1,
                         999U,
                         -123));

        // A by-value QVector may update only its implicit-share refcount.  The
        // inactive fast path never detaches or allocates proportional storage.
        QCOMPARE(pcm.constData(), originalStorage);
        QCOMPARE(pcm.capacity(), originalCapacity);
        const auto stats = ingress.stats();
        QCOMPARE(stats.receivedCalls, std::uint64_t {1U});
        QCOMPARE(stats.rejectedInactiveCalls, std::uint64_t {1U});
        QCOMPARE(stats.rejectedOversizeCalls, std::uint64_t {0U});
        QCOMPARE(stats.rejectedInvalidRateCalls, std::uint64_t {0U});
        QCOMPARE(stats.rejectedInvalidTimestampCalls, std::uint64_t {0U});
        QCOMPARE(stats.queue.enqueuedChunks, std::uint64_t {0U});
        QCOMPARE(stats.queue.queuedChunks, std::size_t {0U});
    }

    void configurationAndInputValidationAreBounded()
    {
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            (void)SstvAudioIngress(SstvAudioIngress::Config {0U, 4U, 4U}));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            (void)SstvAudioIngress(SstvAudioIngress::Config {2U, 4U, 5U}));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            (void)SstvAudioIngress(SstvAudioIngress::Config {
                2U,
                SstvPcm16Queue::kHardMaximumSamples + 1U,
                4U}));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            (void)SstvPcm16Queue(
                std::numeric_limits<std::size_t>::max(), 1U));

        const SstvAudioSource source {
            SstvAudioSourceKind::RtlSdr, 8U};
        SstvAudioIngress ingress({4U, 8U, 4U});
        QVERIFY(ingress.activateSource(source.kind, source.streamId));
        const std::uint64_t generation = ingress.generation();

        QVERIFY(!enqueue(ingress, {}, source, 0, generation));
        QVERIFY(!enqueue(ingress,
                         QVector<short>(5, 1),
                         source,
                         0,
                         generation));
        QVERIFY(!enqueue(ingress,
                         smallPcm(),
                         source,
                         0,
                         generation,
                         0));
        QVERIFY(!enqueue(ingress,
                         smallPcm(),
                         source,
                         0,
                         generation,
                         384'000));
        QVERIFY(!enqueue(ingress,
                         smallPcm(),
                         source,
                         -1,
                         generation));
        QVERIFY(!enqueue(
            ingress,
            smallPcm(),
            {static_cast<SstvAudioSourceKind>(255), 8U},
            0,
            generation));
        QVERIFY(!enqueue(
            ingress,
            smallPcm(),
            {SstvAudioSourceKind::RtlSdr, 9U},
            0,
            generation));
        QVERIFY(!enqueue(ingress,
                         smallPcm(),
                         source,
                         std::numeric_limits<qint64>::max(),
                         generation));

        const auto stats = ingress.stats();
        QCOMPARE(stats.rejectedEmptyCalls, std::uint64_t {1U});
        QCOMPARE(stats.rejectedOversizeCalls, std::uint64_t {1U});
        QCOMPARE(stats.rejectedInvalidRateCalls, std::uint64_t {2U});
        QCOMPARE(stats.rejectedInvalidTimestampCalls, std::uint64_t {2U});
        QCOMPARE(stats.rejectedInvalidSourceCalls, std::uint64_t {1U});
        QCOMPARE(stats.rejectedSourceMismatchCalls, std::uint64_t {1U});
        QCOMPARE(stats.queue.queuedChunks, std::size_t {0U});
    }

    void acceptsExactlyTheResamplerRateSet()
    {
        constexpr std::array<int, 10> supported {
            8'000, 11'025, 12'000, 16'000, 22'050,
            24'000, 32'000, 44'100, 48'000, 96'000};
        constexpr std::array<int, 7> rejected {
            -1, 0, 1'000, 7'999, 12'345, 96'001, 384'000};

        const SstvAudioSource source {SstvAudioSourceKind::Tci, 7U};
        SstvAudioIngress ingress;
        QVERIFY(ingress.activateSource(source.kind, source.streamId));

        bool first = true;
        for (const int rate : supported) {
            if (!first) {
                QVERIFY(ingress.resetStream());
            }
            first = false;
            const std::uint64_t generation = ingress.generation();
            QVERIFY2(enqueue(ingress,
                             QVector<short> {123},
                             source,
                             0,
                             generation,
                             rate),
                     qPrintable(QStringLiteral("rate %1").arg(rate)));
            SstvPcm16Chunk chunk;
            QVERIFY(ingress.tryPop(chunk));
            QCOMPARE(chunk.sampleRate, static_cast<std::uint32_t>(rate));
        }

        for (const int rate : rejected) {
            QVERIFY(ingress.resetStream());
            QVERIFY(!enqueue(ingress,
                             QVector<short> {123},
                             source,
                             0,
                             ingress.generation(),
                             rate));
            QCOMPARE(ingress.stats().generationSampleRate,
                     std::uint32_t {0U});
        }

        QCOMPARE(ingress.stats().rejectedInvalidRateCalls,
                 static_cast<std::uint64_t>(rejected.size()));
    }

    void rateAndTimestampsAreCoherentWithinGeneration()
    {
        const SstvAudioSource source {
            SstvAudioSourceKind::DecoPort, 5U};
        SstvAudioIngress ingress;
        QVERIFY(ingress.activateSource(source.kind, source.streamId));
        const std::uint64_t firstGeneration = ingress.generation();
        const QVector<short> block(12, 44);
        constexpr qint64 firstStart = 100;
        constexpr qint64 blockDuration = 1'000'000;

        QVERIFY(enqueue(ingress,
                        block,
                        source,
                        firstStart,
                        firstGeneration));
        QVERIFY(!enqueue(ingress,
                         block,
                         source,
                         firstStart,
                         firstGeneration));
        QVERIFY(!enqueue(ingress,
                         block,
                         source,
                         firstStart + blockDuration - 1,
                         firstGeneration));
        QVERIFY(!enqueue(ingress,
                         block,
                         source,
                         firstStart + blockDuration,
                         firstGeneration,
                         8'000));
        QVERIFY(enqueue(ingress,
                        block,
                        source,
                        firstStart + blockDuration,
                        firstGeneration));

        auto stats = ingress.stats();
        QCOMPARE(stats.generationSampleRate, std::uint32_t {12'000U});
        QCOMPARE(stats.rejectedRateChangeCalls, std::uint64_t {1U});
        QCOMPARE(stats.rejectedOverlappingTimestampCalls,
                 std::uint64_t {2U});
        QCOMPARE(stats.lastAcceptedTimestampNs,
                 firstStart + blockDuration);
        QCOMPARE(stats.nextAllowedTimestampNs,
                 firstStart + 2 * blockDuration);

        QVERIFY(ingress.resetStream());
        QVERIFY(ingress.generation() > firstGeneration);
        QVERIFY(enqueue(ingress,
                        QVector<short> {1},
                        source,
                        0,
                        ingress.generation(),
                        44'100));
        stats = ingress.stats();
        QCOMPARE(stats.generationSampleRate, std::uint32_t {44'100U});
        QCOMPARE(stats.nextAllowedTimestampNs, qint64 {22'676});
    }

    void staleGenerationAndSourceAudioCannotCrossBoundaries()
    {
        const SstvAudioSource local {
            SstvAudioSourceKind::LocalSoundCard, 1U};
        const SstvAudioSource tci {SstvAudioSourceKind::Tci, 2U};
        SstvAudioIngress ingress;
        QVERIFY(ingress.activateSource(local.kind, local.streamId));
        const std::uint64_t generation1 = ingress.generation();
        QVERIFY(enqueue(ingress,
                        smallPcm(),
                        local,
                        0,
                        generation1));

        // Even a switch to the identical source creates a new epoch and
        // clears audio already queued under the old one.
        QVERIFY(ingress.switchSource(local.kind, local.streamId));
        const std::uint64_t generation2 = ingress.generation();
        QVERIFY(generation2 > generation1);
        QCOMPARE(ingress.stats().queue.queuedChunks, std::size_t {0U});
        QVERIFY(!enqueue(ingress,
                         smallPcm(),
                         local,
                         0,
                         generation1));
        QVERIFY(enqueue(ingress,
                        smallPcm(200),
                        local,
                        0,
                        generation2));

        QVERIFY(ingress.switchSource(tci.kind, tci.streamId));
        const std::uint64_t generation3 = ingress.generation();
        QVERIFY(!enqueue(ingress,
                         smallPcm(),
                         local,
                         0,
                         generation3));
        QVERIFY(!enqueue(ingress,
                         smallPcm(),
                         tci,
                         0,
                         generation2));
        QVERIFY(enqueue(ingress,
                        smallPcm(300),
                        tci,
                        0,
                        generation3));

        QVERIFY(ingress.resetStream());
        const std::uint64_t generation4 = ingress.generation();
        QVERIFY(!enqueue(ingress,
                         smallPcm(),
                         tci,
                         0,
                         generation3));
        QVERIFY(ingress.cancel());
        QVERIFY(!enqueue(ingress,
                         smallPcm(),
                         tci,
                         0,
                         generation4));
        QVERIFY(ingress.restart());
        const std::uint64_t generation5 = ingress.generation();
        QVERIFY(generation5 > generation4);
        QVERIFY(!enqueue(ingress,
                         smallPcm(),
                         tci,
                         0,
                         generation4));
        QVERIFY(enqueue(ingress,
                        smallPcm(400),
                        tci,
                        0,
                        generation5));

        SstvPcm16Chunk chunk;
        QVERIFY(ingress.tryPop(chunk));
        QCOMPARE(chunk.generation, generation5);
        QCOMPARE(chunk.samples, smallPcm(400));
        QVERIFY(!ingress.tryPop(chunk));

        const auto stats = ingress.stats();
        QCOMPARE(stats.rejectedStaleGenerationCalls,
                 std::uint64_t {4U});
        QCOMPARE(stats.rejectedSourceMismatchCalls, std::uint64_t {1U});
        QCOMPARE(stats.rejectedCancelledCalls, std::uint64_t {1U});
        QVERIFY(stats.queue.clearedChunks >= std::uint64_t {3U});
    }

    void boundedQueueDropsWholeOldestChunks()
    {
        const SstvAudioSource source {SstvAudioSourceKind::RtlSdr, 8U};
        SstvAudioIngress ingress({2U, 6U, 3U});
        QVERIFY(ingress.activateSource(source.kind, source.streamId));
        const std::uint64_t generation = ingress.generation();
        constexpr qint64 duration = 250'000;

        QVERIFY(enqueue(ingress,
                        smallPcm(10),
                        source,
                        0,
                        generation));
        QVERIFY(enqueue(ingress,
                        smallPcm(20),
                        source,
                        duration,
                        generation));
        QVERIFY(enqueue(ingress,
                        smallPcm(30),
                        source,
                        2 * duration,
                        generation));

        const auto stats = ingress.stats();
        QCOMPARE(stats.queue.queuedChunks, std::size_t {2U});
        QCOMPARE(stats.queue.queuedSamples, std::size_t {6U});
        QCOMPARE(stats.queue.droppedChunks, std::uint64_t {1U});
        QCOMPARE(stats.queue.droppedSamples, std::uint64_t {3U});
        QCOMPARE(stats.queue.enqueuedChunks, std::uint64_t {3U});

        SstvPcm16Chunk chunk;
        QVERIFY(ingress.tryPop(chunk));
        QCOMPARE(chunk.sequence, std::uint64_t {1U});
        QCOMPARE(chunk.samples, smallPcm(20));
        QVERIFY(ingress.tryPop(chunk));
        QCOMPARE(chunk.sequence, std::uint64_t {2U});
        QCOMPARE(chunk.samples, smallPcm(30));
        QVERIFY(!ingress.tryPop(chunk));
    }

    void waitPopDistinguishesChunkAndLosslessCancelRestart()
    {
        const SstvAudioSource source {
            SstvAudioSourceKind::LegacyBackend, 11U};
        SstvAudioIngress ingress;
        QVERIFY(ingress.activateSource(source.kind, source.streamId));
        std::uint64_t generation = ingress.generation();

        auto chunkFuture = std::async(std::launch::async, [&ingress] {
            WaitOutcome outcome;
            outcome.result = ingress.waitPop(outcome.chunk);
            return outcome;
        });
        QTRY_VERIFY_WITH_TIMEOUT(
            ingress.stats().queue.waitingConsumers == 1U, 1'000);
        const bool firstQueued = enqueue(ingress,
                                         smallPcm(50),
                                         source,
                                         0,
                                         generation);
        if (!firstQueued) {
            (void)ingress.cancel();
        }
        QVERIFY(firstQueued);
        QCOMPARE(chunkFuture.wait_for(std::chrono::seconds(1)),
                 std::future_status::ready);
        WaitOutcome outcome = chunkFuture.get();
        QCOMPARE(outcome.result, SstvAudioIngress::WaitResult::Chunk);
        QCOMPARE(outcome.chunk.samples, smallPcm(50));

        auto cancelledFuture = std::async(std::launch::async, [&ingress] {
            WaitOutcome result;
            result.result = ingress.waitPop(result.chunk);
            return result;
        });
        QTRY_VERIFY_WITH_TIMEOUT(
            ingress.stats().queue.waitingConsumers == 1U, 1'000);
        QVERIFY(ingress.cancel());
        // Restart before the old waiter reacquires the queue lock.  The wake
        // epoch must still make the old wait return Cancelled.
        QVERIFY(ingress.restart());
        generation = ingress.generation();
        QCOMPARE(cancelledFuture.wait_for(std::chrono::seconds(1)),
                 std::future_status::ready);
        outcome = cancelledFuture.get();
        QCOMPARE(outcome.result, SstvAudioIngress::WaitResult::Cancelled);

        auto restartedFuture = std::async(std::launch::async, [&ingress] {
            WaitOutcome result;
            result.result = ingress.waitPop(result.chunk);
            return result;
        });
        QTRY_VERIFY_WITH_TIMEOUT(
            ingress.stats().queue.waitingConsumers == 1U, 1'000);
        const bool restartedQueued = enqueue(ingress,
                                             smallPcm(70),
                                             source,
                                             0,
                                             generation);
        if (!restartedQueued) {
            (void)ingress.cancel();
        }
        QVERIFY(restartedQueued);
        QCOMPARE(restartedFuture.wait_for(std::chrono::seconds(1)),
                 std::future_status::ready);
        outcome = restartedFuture.get();
        QCOMPARE(outcome.result, SstvAudioIngress::WaitResult::Chunk);
        QCOMPARE(outcome.chunk.generation, generation);
        QCOMPARE(outcome.chunk.samples, smallPcm(70));
    }

    void sequenceMaximumIsIssuedOnceThenRejected()
    {
        const SstvAudioSource source {
            SstvAudioSourceKind::Replay, 4U};
        SstvAudioIngress ingress;
        const quint64 maximum = std::numeric_limits<quint64>::max();
        QVERIFY(ingress.activateSource(source.kind,
                                       source.streamId,
                                       maximum));
        const std::uint64_t generation = ingress.generation();
        const QVector<short> sample {1};
        const qint64 duration = durationNs(sample.size(), kSampleRate);

        QVERIFY(enqueue(ingress,
                        sample,
                        source,
                        0,
                        generation));
        QVERIFY(!enqueue(ingress,
                         sample,
                         source,
                         duration,
                         generation));

        SstvPcm16Chunk chunk;
        QVERIFY(ingress.tryPop(chunk));
        QCOMPARE(chunk.sequence,
                 std::numeric_limits<std::uint64_t>::max());
        QVERIFY(!ingress.tryPop(chunk));
        const auto stats = ingress.stats();
        QVERIFY(stats.sequenceExhausted);
        QCOMPARE(stats.nextSequence,
                 std::numeric_limits<std::uint64_t>::max());
        QCOMPARE(stats.rejectedSequenceExhaustedCalls,
                 std::uint64_t {1U});
    }

    void realPcmSignalUsesDirectBoundedRelayAndCoalescedWake()
    {
        constexpr std::size_t blockCount = 24U;
        const SstvAudioSource source {
            SstvAudioSourceKind::LegacyBackend, 23U};
        SstvAudioIngress ingress({32U, 256U, 8U});
        QVERIFY(ingress.activateSource(source.kind, source.streamId));

        SstvPcm16Metadata metadata;
        metadata.sampleRate = kSampleRate;
        metadata.source = source;
        metadata.generation = ingress.generation();
        BoundedMetadataRelay relay(ingress, metadata, 10'000);
        QSignalSpy wakeSpy(&ingress, &SstvAudioIngress::pcmAvailable);
        QVERIFY(wakeSpy.isValid());

        const QVector<short> block {1, -2, 3, -4};
        std::atomic<bool> directConnected {false};
        std::thread audioCallback {[&] {
            // The source is created in its actual callback thread.  A direct
            // connection executes only the lightweight relay there; no PCM
            // payload is ever placed in Qt's unbounded event queue.
            RealAudioSource audioSource;
            const QMetaObject::Connection direct = QObject::connect(
                &audioSource,
                &RealAudioSource::audioSamplesReady,
                &relay,
                &BoundedMetadataRelay::relay,
                Qt::DirectConnection);
            directConnected.store(static_cast<bool>(direct),
                                  std::memory_order_release);
            for (std::size_t index = 0U; index < blockCount; ++index) {
                Q_EMIT audioSource.audioSamplesReady(block);
            }
        }};
        audioCallback.join();

        QVERIFY(directConnected.load(std::memory_order_acquire));
        QCOMPARE(relay.forwarded(), static_cast<std::uint64_t>(blockCount));
        QCOMPARE(relay.accepted(), static_cast<std::uint64_t>(blockCount));
        QCOMPARE(wakeSpy.count(), 0);
        auto stats = ingress.stats();
        QCOMPARE(stats.coalescedWakePosts, std::uint64_t {1U});
        QCOMPARE(stats.coalescedWakeSuppressions,
                 static_cast<std::uint64_t>(blockCount - 1U));

        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QTRY_COMPARE_WITH_TIMEOUT(wakeSpy.count(), 1, 1'000);
        stats = ingress.stats();
        QCOMPARE(stats.coalescedWakeDeliveries, std::uint64_t {1U});

        std::size_t popped = 0U;
        SstvPcm16Chunk chunk;
        while (ingress.tryPop(chunk)) {
            QCOMPARE(chunk.samples, block);
            QCOMPARE(chunk.generation, metadata.generation);
            ++popped;
        }
        QCOMPARE(popped, blockCount);

        // The relay explicitly captured the previous generation.  A lifecycle
        // reset makes subsequent real-signal deliveries stale until the relay
        // metadata is refreshed by the owner thread.
        QVERIFY(ingress.resetStream());
        RealAudioSource staleSource;
        const QMetaObject::Connection staleDirect = QObject::connect(
            &staleSource,
            &RealAudioSource::audioSamplesReady,
            &relay,
            &BoundedMetadataRelay::relay,
            Qt::DirectConnection);
        QVERIFY(staleDirect);
        Q_EMIT staleSource.audioSamplesReady(block);
        QCOMPARE(relay.accepted(), static_cast<std::uint64_t>(blockCount));
        QCOMPARE(ingress.stats().rejectedStaleGenerationCalls,
                 std::uint64_t {1U});
    }

    void callbackApiIsConcurrentAndQueueRemainsBounded()
    {
        constexpr std::size_t producerCount = 4U;
        constexpr std::size_t callsPerProducer = 400U;
        constexpr std::size_t totalCalls = producerCount * callsPerProducer;
        const SstvAudioSource source {
            SstvAudioSourceKind::LocalSoundCard, 91U};
        SstvAudioIngress ingress({64U, 1'024U, 8U});
        QVERIFY(ingress.activateSource(source.kind, source.streamId));
        const std::uint64_t generation = ingress.generation();
        const QVector<short> seed {7};
        const qint64 step = durationNs(seed.size(), kSampleRate);
        QVERIFY(enqueue(ingress, seed, source, 0, generation));

        std::atomic<qint64> nextTimestamp {step};
        std::atomic<std::uint64_t> concurrentAccepted {0U};
        std::atomic<bool> stopObserver {false};
        std::thread observer {[&] {
            while (!stopObserver.load(std::memory_order_acquire)) {
                const auto snapshot = ingress.stats();
                if (snapshot.queue.queuedChunks
                    > ingress.configuration().maximumChunks) {
                    Q_UNREACHABLE();
                }
                std::this_thread::yield();
            }
        }};

        std::vector<std::thread> producers;
        producers.reserve(producerCount);
        for (std::size_t producer = 0U;
             producer < producerCount;
             ++producer) {
            producers.emplace_back([&, producer] {
                const short value = static_cast<short>(producer + 1U);
                for (std::size_t call = 0U;
                     call < callsPerProducer;
                     ++call) {
                    const qint64 timestamp =
                        nextTimestamp.fetch_add(step,
                                                std::memory_order_relaxed);
                    if (enqueue(ingress,
                                QVector<short> {value},
                                source,
                                timestamp,
                                generation)) {
                        concurrentAccepted.fetch_add(
                            1U, std::memory_order_relaxed);
                    }
                }
            });
        }
        for (auto& producer : producers) {
            producer.join();
        }
        stopObserver.store(true, std::memory_order_release);
        observer.join();

        const auto stats = ingress.stats();
        const std::uint64_t accepted =
            concurrentAccepted.load(std::memory_order_relaxed);
        QCOMPARE(stats.receivedCalls,
                 static_cast<std::uint64_t>(totalCalls + 1U));
        QCOMPARE(stats.acceptedChunks, accepted + 1U);
        QCOMPARE(stats.rejectedOverlappingTimestampCalls,
                 static_cast<std::uint64_t>(totalCalls) - accepted);
        QCOMPARE(stats.queue.enqueuedChunks, stats.acceptedChunks);
        QVERIFY(stats.queue.queuedChunks
                <= ingress.configuration().maximumChunks);
        QVERIFY(stats.queue.queuedSamples
                <= ingress.configuration().maximumQueuedSamples);
        QCOMPARE(stats.queue.droppedChunks
                     + static_cast<std::uint64_t>(stats.queue.queuedChunks),
                 stats.queue.enqueuedChunks);

        bool first = true;
        std::uint64_t previousSequence = 0U;
        qint64 previousEnd = 0;
        SstvPcm16Chunk chunk;
        while (ingress.tryPop(chunk)) {
            if (!first) {
                QVERIFY(chunk.sequence > previousSequence);
                QVERIFY(chunk.startTime.count() >= previousEnd);
            }
            first = false;
            previousSequence = chunk.sequence;
            previousEnd = chunk.startTime.count() + step;
        }
    }

    void lifecycleIsOwnerThreadOnlyButCallbackIsNot()
    {
        const SstvAudioSource source {
            SstvAudioSourceKind::DecoPort, 77U};
        SstvAudioIngress ingress;
        QVERIFY(ingress.activateSource(source.kind, source.streamId));
        const std::uint64_t generation = ingress.generation();

        std::atomic<bool> callbackAccepted {false};
        std::atomic<bool> foreignCancelResult {true};
        std::thread foreign {[&] {
            callbackAccepted.store(
                enqueue(ingress,
                        smallPcm(),
                        source,
                        0,
                        generation),
                std::memory_order_release);
            foreignCancelResult.store(ingress.cancel(),
                                      std::memory_order_release);
        }};
        foreign.join();

        QVERIFY(callbackAccepted.load(std::memory_order_acquire));
        QVERIFY(!foreignCancelResult.load(std::memory_order_acquire));
        QCOMPARE(ingress.state(), SstvAudioIngress::State::Active);
        QCOMPARE(ingress.stats().rejectedWrongThreadLifecycleCalls,
                 std::uint64_t {1U});
        SstvPcm16Chunk chunk;
        QVERIFY(ingress.tryPop(chunk));

        QVERIFY(ingress.cancel());
        QCOMPARE(ingress.state(), SstvAudioIngress::State::Cancelled);
        QVERIFY(ingress.restart());
        QVERIFY(ingress.generation() > generation);
        QVERIFY(ingress.shutdown());
        QCOMPARE(ingress.state(), SstvAudioIngress::State::Shutdown);
        QVERIFY(!enqueue(ingress,
                         smallPcm(),
                         source,
                         0,
                         ingress.generation()));
        QCOMPARE(ingress.stats().rejectedShutdownCalls,
                 std::uint64_t {1U});
    }
};

QTEST_GUILESS_MAIN(TestSstvAudioIngress)
#include "test_sstv_audio_ingress.moc"
