// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/rx/SstvAudioRingBuffer.h"
#include "../../src/sstv/rx/SstvReplayBuffer.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;
using namespace std::chrono_literals;

namespace {

SstvAudioChunk makeChunk(SstvAudioSource source,
                         std::uint64_t sequence,
                         std::size_t sampleCount,
                         float firstValue = 0.0F,
                         std::uint32_t sampleRate = 1'000U,
                         std::chrono::nanoseconds startTime = 0ns)
{
    SstvAudioChunk chunk;
    chunk.source = source;
    chunk.sampleRate = sampleRate;
    chunk.startTime = startTime;
    chunk.sequence = sequence;
    chunk.samples.resize(sampleCount);
    for (std::size_t index = 0U; index < sampleCount; ++index) {
        chunk.samples[index] = firstValue + static_cast<float>(index);
    }
    return chunk;
}

} // namespace

class TestSstvAudioBuffer final : public QObject
{
    Q_OBJECT

private slots:
    void ringRejectsInvalidCapacities()
    {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvAudioRingBuffer(0U, 1U));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvAudioRingBuffer(1U, 0U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvAudioRingBuffer(SstvAudioRingBuffer::kMaximumChunkCapacity + 1U, 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvAudioRingBuffer(1U, SstvAudioRingBuffer::kMaximumSampleCapacity + 1U));
    }

    void ringDropsOldestChunksDeterministically()
    {
        const SstvAudioSource local {SstvAudioSourceKind::LocalSoundCard, 1U};
        SstvAudioRingBuffer buffer(3U, 5U);
        QVERIFY(buffer.push(makeChunk(local, 1U, 2U, 10.0F)));
        QVERIFY(buffer.push(makeChunk(local, 2U, 2U, 20.0F)));
        QVERIFY(buffer.push(makeChunk(local, 3U, 2U, 30.0F)));

        QCOMPARE(buffer.queuedChunks(), std::size_t {2U});
        QCOMPARE(buffer.queuedSamples(), std::size_t {4U});
        auto stats = buffer.stats();
        QCOMPARE(stats.droppedChunks, std::uint64_t {1U});
        QCOMPARE(stats.droppedSamples, std::uint64_t {2U});

        SstvAudioChunk chunk;
        QVERIFY(buffer.tryPop(chunk));
        QCOMPARE(chunk.sequence, std::uint64_t {2U});
        QVERIFY(buffer.tryPop(chunk));
        QCOMPARE(chunk.sequence, std::uint64_t {3U});
        QVERIFY(!buffer.tryPop(chunk));
    }

    void ringPreservesSourceMetadataAndOrdering()
    {
        const SstvAudioSource soundCard {SstvAudioSourceKind::LocalSoundCard, 7U};
        const SstvAudioSource rtl {SstvAudioSourceKind::RtlSdr, 42U};
        SstvAudioRingBuffer buffer(4U, 20U);
        QVERIFY(buffer.push(makeChunk(soundCard, 10U, 3U)));
        QVERIFY(buffer.push(makeChunk(rtl, 11U, 4U)));

        SstvAudioChunk chunk;
        QVERIFY(buffer.tryPop(chunk));
        QVERIFY(chunk.source == soundCard);
        QCOMPARE(chunk.sequence, std::uint64_t {10U});
        QVERIFY(buffer.tryPop(chunk));
        QVERIFY(chunk.source == rtl);
        QCOMPARE(chunk.sequence, std::uint64_t {11U});
    }

    void oversizedChunkKeepsNewestBoundedTail()
    {
        const SstvAudioSource source {SstvAudioSourceKind::DecoPort, 3U};
        SstvAudioRingBuffer buffer(4U, 5U);
        QVERIFY(buffer.push(makeChunk(source, 1U, 8U, 0.0F, 1'000U, 2s)));
        QCOMPARE(buffer.queuedSamples(), std::size_t {5U});

        SstvAudioChunk retained;
        QVERIFY(buffer.tryPop(retained));
        QCOMPARE(retained.samples.size(), std::size_t {5U});
        QCOMPARE(retained.samples.front(), 3.0F);
        QCOMPARE(retained.samples.back(), 7.0F);
        QCOMPARE(retained.startTime, 2s + 3ms);
        const auto stats = buffer.stats();
        QCOMPARE(stats.droppedChunks, std::uint64_t {0U});
        QCOMPARE(stats.droppedSamples, std::uint64_t {3U});
    }

    void waitIsWokenByDataAndCancellation()
    {
        const SstvAudioSource source {SstvAudioSourceKind::Tci, 9U};
        SstvAudioRingBuffer buffer(4U, 100U);
        SstvAudioChunk received;
        auto dataWait = std::async(std::launch::async, [&] {
            return buffer.waitPop(received);
        });
        QCOMPARE(dataWait.wait_for(20ms), std::future_status::timeout);
        QVERIFY(buffer.push(makeChunk(source, 88U, 4U)));
        QCOMPARE(dataWait.wait_for(1s), std::future_status::ready);
        QCOMPARE(dataWait.get(), SstvAudioRingBuffer::WaitResult::Chunk);
        QCOMPARE(received.sequence, std::uint64_t {88U});

        auto cancelWait = std::async(std::launch::async, [&] {
            SstvAudioChunk ignored;
            return buffer.waitPop(ignored);
        });
        QCOMPARE(cancelWait.wait_for(20ms), std::future_status::timeout);
        buffer.cancel();
        QCOMPARE(cancelWait.wait_for(1s), std::future_status::ready);
        QCOMPARE(cancelWait.get(), SstvAudioRingBuffer::WaitResult::Cancelled);
        QVERIFY(buffer.isCancelled());
        QVERIFY(!buffer.push(makeChunk(source, 89U, 1U)));

        buffer.restart();
        QVERIFY(!buffer.isCancelled());
        QVERIFY(buffer.push(makeChunk(source, 90U, 1U)));
    }

    void resetCannotLoseCancellationWakeup()
    {
        SstvAudioRingBuffer buffer(2U, 10U);
        auto waiter = std::async(std::launch::async, [&] {
            SstvAudioChunk ignored;
            return buffer.waitPop(ignored);
        });
        QCOMPARE(waiter.wait_for(20ms), std::future_status::timeout);
        buffer.reset();
        QCOMPARE(waiter.wait_for(1s), std::future_status::ready);
        QCOMPARE(waiter.get(), SstvAudioRingBuffer::WaitResult::Cancelled);
        QVERIFY(!buffer.isCancelled());
        QCOMPARE(buffer.queuedChunks(), std::size_t {0U});
        QCOMPARE(buffer.stats().enqueuedChunks, std::uint64_t {0U});
    }

    void invalidChunksAreRejectedWithoutQueueMutation()
    {
        SstvAudioRingBuffer buffer(2U, 10U);
        SstvAudioChunk empty;
        empty.sampleRate = 12'000U;
        QVERIFY(!buffer.push(std::move(empty)));
        auto invalidRate = makeChunk({}, 1U, 1U);
        invalidRate.sampleRate = 0U;
        QVERIFY(!buffer.push(std::move(invalidRate)));
        QCOMPARE(buffer.queuedChunks(), std::size_t {0U});
        QCOMPARE(buffer.stats().rejectedChunks, std::uint64_t {2U});
    }

    void replayRetainsNewestDurationChronologically()
    {
        const SstvAudioSource source {SstvAudioSourceKind::LocalSoundCard, 1U};
        SstvReplayBuffer replay(2s, 1'000U);
        QVERIFY(replay.append(makeChunk(source, 1U, 1'200U, 0.0F, 1'000U, 0s)));
        QVERIFY(replay.append(makeChunk(source, 2U, 1'200U, 2'000.0F, 1'000U, 2s)));

        QCOMPARE(replay.capacitySamples(), std::size_t {2'000U});
        QCOMPARE(replay.retainedSamples(), std::size_t {2'000U});
        const auto snapshot = replay.snapshot();
        QCOMPARE(snapshot.size(), std::size_t {2U});
        QCOMPARE(snapshot[0].sequence, std::uint64_t {1U});
        QCOMPARE(snapshot[0].samples.size(), std::size_t {800U});
        QCOMPARE(snapshot[0].samples.front(), 400.0F);
        QCOMPARE(snapshot[0].startTime, 400ms);
        QCOMPARE(snapshot[1].sequence, std::uint64_t {2U});
        QCOMPARE(replay.stats().evictedSamples, std::uint64_t {400U});
    }

    void replayResizePreservesNewestAndRateChangeResets()
    {
        const SstvAudioSource source {SstvAudioSourceKind::WebSdr, 2U};
        SstvReplayBuffer replay(2s, 1'000U);
        QVERIFY(replay.append(makeChunk(source, 1U, 1'000U, 0.0F, 1'000U, 0s)));
        QVERIFY(replay.append(makeChunk(source, 2U, 1'000U, 2'000.0F, 1'000U, 1s)));

        replay.resize(1s, 1'000U);
        QCOMPARE(replay.retainedSamples(), std::size_t {1'000U});
        auto snapshot = replay.snapshot();
        QCOMPARE(snapshot.size(), std::size_t {1U});
        QCOMPARE(snapshot.front().sequence, std::uint64_t {2U});

        replay.resize(1s, 2'000U);
        QCOMPARE(replay.sampleRate(), std::uint32_t {2'000U});
        QCOMPARE(replay.capacitySamples(), std::size_t {2'000U});
        QCOMPARE(replay.retainedSamples(), std::size_t {0U});
        QVERIFY(replay.snapshot().empty());
    }

    void replayRejectsOutOfOrderAndWrongRate()
    {
        const SstvAudioSource source {SstvAudioSourceKind::RtlSdr, 4U};
        SstvReplayBuffer replay(1s, 1'000U);
        QVERIFY(replay.append(makeChunk(source, 2U, 10U, 0.0F, 1'000U, 2s)));
        QVERIFY(!replay.append(makeChunk(source, 1U, 10U, 0.0F, 1'000U, 1s)));
        QVERIFY(!replay.append(makeChunk(source, 3U, 10U, 0.0F, 2'000U, 3s)));
        QCOMPARE(replay.retainedChunks(), std::size_t {1U});
        QCOMPARE(replay.stats().rejectedChunks, std::uint64_t {2U});
    }

    void replayOversizeAndBoundsAreExplicit()
    {
        const SstvAudioSource source {SstvAudioSourceKind::Replay, 1U};
        SstvReplayBuffer replay(1s, 1'000U);
        QVERIFY(replay.append(makeChunk(source, 1U, 1'500U, 0.0F, 1'000U, 0s)));
        const auto snapshot = replay.snapshot();
        QCOMPARE(snapshot.size(), std::size_t {1U});
        QCOMPARE(snapshot.front().samples.size(), std::size_t {1'000U});
        QCOMPARE(snapshot.front().samples.front(), 500.0F);
        QCOMPARE(snapshot.front().startTime, 500ms);
        QCOMPARE(replay.stats().evictedSamples, std::uint64_t {500U});

        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvReplayBuffer(0s, 12'000U));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvReplayBuffer(601s, 12'000U));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvReplayBuffer(1s, 999U));
        QVERIFY_THROWS_EXCEPTION(std::length_error, SstvReplayBuffer(600s, 192'000U));

        replay.reset();
        QCOMPARE(replay.retainedSamples(), std::size_t {0U});
        QCOMPARE(replay.stats().appendedChunks, std::uint64_t {0U});
    }
};

QTEST_APPLESS_MAIN(TestSstvAudioBuffer)
#include "test_sstv_audio_buffer.moc"
