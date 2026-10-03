// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/integration/SstvTxAudioDevice.h"
#include "../../src/sstv/integration/SstvTxSources.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using namespace decodium::sstv;

namespace {

class VectorSource : public SstvPcm16Source
{
public:
    explicit VectorSource(std::vector<std::int16_t> samples,
                          std::uint32_t sampleRate = 48'000U)
        : samples_(std::move(samples))
        , sampleRate_(sampleRate)
    {
    }

    std::uint32_t sampleRate() const noexcept override
    {
        return sampleRate_;
    }

    std::uint64_t totalSamples() const noexcept override
    {
        return samples_.size();
    }

    std::uint64_t producedSamples() const noexcept override
    {
        return position_;
    }

    bool complete() const noexcept override
    {
        return position_ >= samples_.size();
    }

    bool cancelled() const noexcept override
    {
        return cancelled_;
    }

    std::size_t pullPcm16(std::int16_t* output,
                          std::size_t capacity) override
    {
        maximumRequested_ = std::max(maximumRequested_, capacity);
        if (cancelled_ || complete()) {
            return 0U;
        }
        const std::size_t count = std::min(capacity, samples_.size() - position_);
        std::copy_n(samples_.data() + position_, count, output);
        position_ += count;
        return count;
    }

    void cancel() noexcept override
    {
        cancelled_ = true;
    }

    void reset() override
    {
        position_ = 0U;
        maximumRequested_ = 0U;
        cancelled_ = false;
    }

    std::size_t maximumRequested() const noexcept
    {
        return maximumRequested_;
    }

private:
    std::vector<std::int16_t> samples_;
    std::uint32_t sampleRate_ {48'000U};
    std::size_t position_ {0U};
    std::size_t maximumRequested_ {0U};
    bool cancelled_ {false};
};

class DeclaredLengthSource final : public SstvPcm16Source
{
public:
    explicit DeclaredLengthSource(std::uint64_t declared,
                                  std::uint32_t sampleRate = 48'000U)
        : declared_(declared)
        , sampleRate_(sampleRate)
    {
    }

    std::uint32_t sampleRate() const noexcept override { return sampleRate_; }
    std::uint64_t totalSamples() const noexcept override { return declared_; }
    std::uint64_t producedSamples() const noexcept override { return 0U; }
    bool complete() const noexcept override { return false; }
    bool cancelled() const noexcept override { return cancelled_; }
    std::size_t pullPcm16(std::int16_t*, std::size_t) override { return 0U; }
    void cancel() noexcept override { cancelled_ = true; }
    void reset() override { cancelled_ = false; }

private:
    std::uint64_t declared_ {0U};
    std::uint32_t sampleRate_ {48'000U};
    bool cancelled_ {false};
};

std::vector<std::int16_t> decodeNativePcm16(const QByteArray& bytes)
{
    if ((bytes.size() % static_cast<qsizetype>(sizeof(std::int16_t))) != 0) {
        throw std::runtime_error("test PCM byte count is not sample aligned");
    }
    std::vector<std::int16_t> samples(
        static_cast<std::size_t>(bytes.size()) / sizeof(std::int16_t));
    if (!bytes.isEmpty()) {
        std::memcpy(samples.data(), bytes.constData(),
                    static_cast<std::size_t>(bytes.size()));
    }
    return samples;
}

QByteArray readInChunks(SstvTxAudioDevice& device, qint64 chunkSize)
{
    QByteArray result;
    while (!device.atEnd()) {
        const QByteArray chunk = device.read(chunkSize);
        if (chunk.isEmpty()) {
            throw std::runtime_error("test audio device made no progress");
        }
        result.append(chunk);
    }
    return result;
}

} // namespace

class TestSstvTxAudioDevice final : public QObject
{
    Q_OBJECT

private slots:
    void monoReadHandlesUnalignedCallerChunks()
    {
        const std::vector<std::int16_t> expected {
            std::numeric_limits<std::int16_t>::min(), -2, -1, 0, 1, 2,
            std::numeric_limits<std::int16_t>::max()
        };
        SstvTxAudioDevice device(
            std::make_unique<VectorSource>(expected), 1U);

        QVERIFY(device.open(QIODevice::ReadOnly));
        QVERIFY(device.isSequential());
        QCOMPARE(device.sampleRate(), std::uint32_t {48'000U});
        QCOMPARE(device.channelCount(), 1U);
        QCOMPARE(device.channelRoute(), SstvTxChannelRoute::Both);
        QCOMPARE(device.totalFrames(), std::uint64_t {expected.size()});
        QCOMPARE(device.size(), static_cast<qint64>(expected.size() * 2U));
        QCOMPARE(device.pos(), qint64 {0});

        const QByteArray bytes = readInChunks(device, 3);
        QCOMPARE(bytes.size(), device.size());
        QCOMPARE(decodeNativePcm16(bytes), expected);
        QCOMPARE(device.pos(), device.size());
        QCOMPARE(device.bytesReadFromDevice(), device.size());
        QCOMPARE(device.framesProducedBySource(), device.totalFrames());
        QCOMPARE(device.bytesAvailable(), qint64 {0});
        QVERIFY(device.atEnd());
        QCOMPARE(device.peakPcm16Magnitude(), std::uint32_t {32'768U});
        QCOMPARE(device.peakNormalized(), 1.0);
        QCOMPARE(device.clippedFrames(), std::uint64_t {2U});
        QVERIFY(!device.cancelled());
        QVERIFY(!device.failed());
    }

    void calibrationReferencesAreExactBoundedAndChunkInvariant()
    {
        const struct {
            const char* id;
            SstvCalibrationToneKind kind;
            double frequency;
        } cases[] {
            {"sync-1200", SstvCalibrationToneKind::SyncReference, 1'200.0},
            {"black-1500", SstvCalibrationToneKind::BlackReference, 1'500.0},
            {"leader-1900", SstvCalibrationToneKind::LeaderReference, 1'900.0},
            {"white-2300", SstvCalibrationToneKind::WhiteReference, 2'300.0},
        };

        for (const auto& item : cases) {
            const auto parsed = calibrationToneKindFromId(item.id);
            QVERIFY(parsed.has_value());
            QCOMPARE(*parsed, item.kind);
            const SstvCalibrationToneSpec& spec = calibrationToneSpec(item.kind);
            QCOMPARE(QString::fromLatin1(spec.id), QString::fromLatin1(item.id));
            QCOMPARE(spec.frequencyHz, item.frequency);

            auto source = makeCalibrationTonePcm16Source(
                item.kind, 12'000U, 1'000U);
            QCOMPARE(source->totalSamples(), std::uint64_t {12'000U});
            std::vector<std::int16_t> split;
            split.reserve(12'000U);
            std::array<std::int16_t, 257U> scratch {};
            while (!source->complete()) {
                const std::size_t count = source->pullPcm16(
                    scratch.data(), scratch.size());
                QVERIFY(count > 0U);
                split.insert(split.end(), scratch.cbegin(),
                             scratch.cbegin() + static_cast<std::ptrdiff_t>(count));
            }
            QCOMPARE(split.size(), std::size_t {12'000U});

            auto wholeSource = makeCalibrationTonePcm16Source(
                item.kind, 12'000U, 1'000U);
            std::vector<std::int16_t> whole(12'000U);
            QCOMPARE(wholeSource->pullPcm16(whole.data(), whole.size()),
                     whole.size());
            QCOMPARE(split, whole);

            std::size_t positiveCrossings = 0U;
            for (std::size_t index = 1U; index < split.size(); ++index) {
                if (split[index - 1U] <= 0 && split[index] > 0) {
                    ++positiveCrossings;
                }
            }
            QVERIFY(std::abs(static_cast<double>(positiveCrossings)
                             - item.frequency) <= 1.0);
            source->reset();
            source->cancel();
            QCOMPARE(source->pullPcm16(scratch.data(), scratch.size()),
                     std::size_t {0U});
        }

        QVERIFY(!calibrationToneKindFromId("SYNC-1200").has_value());
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            makeCalibrationTonePcm16Source(
                SstvCalibrationToneKind::SyncReference,
                12'000U,
                249U));
    }

    void stereoRoutingIsExact()
    {
        const std::vector<std::int16_t> mono {100, -200, 300};
        const auto verify = [&](SstvTxChannelRoute route,
                                const std::vector<std::int16_t>& expected) {
            SstvTxAudioDevice device(
                std::make_unique<VectorSource>(mono), 2U, route);
            QVERIFY(device.open(QIODevice::ReadOnly));
            const auto stereo = decodeNativePcm16(device.readAll());
            QCOMPARE(stereo, expected);
            QCOMPARE(device.size(), qint64 {12});
            QVERIFY(device.atEnd());
        };

        verify(SstvTxChannelRoute::Both,
               {100, 100, -200, -200, 300, 300});
        verify(SstvTxChannelRoute::Left,
               {100, 0, -200, 0, 300, 0});
        verify(SstvTxChannelRoute::Right,
               {0, 100, 0, -200, 0, 300});
    }

    void eachReadKeepsSourceWorkBounded()
    {
        std::vector<std::int16_t> samples(20'000U, 123);
        auto source = std::make_unique<VectorSource>(samples);
        VectorSource* const observed = source.get();
        SstvTxAudioDevice device(std::move(source), 2U);
        QVERIFY(device.open(QIODevice::ReadOnly));

        QByteArray destination(1'000'000, Qt::Uninitialized);
        const qint64 count = device.read(destination.data(), destination.size());
        QCOMPARE(count, static_cast<qint64>(
                            SstvTxAudioDevice::MaximumFramesPerPull * 4U));
        QCOMPARE(observed->maximumRequested(),
                 SstvTxAudioDevice::MaximumFramesPerPull);
        QCOMPARE(device.framesProducedBySource(),
                 std::uint64_t {SstvTxAudioDevice::MaximumFramesPerPull});
        QVERIFY(!device.atEnd());
    }

    void nativeEncodersFeedTheDecodiumAudioContract()
    {
        const std::vector<SstvRgbPixel> pixels(
            SstvMartinM1Encoder::PixelCount,
            SstvRgbPixel {0U, 127U, 255U});

        SstvMartinM1EncoderConfig martinConfig;
        martinConfig.sampleRate = 48'000U;
        SstvTxAudioDevice martin(
            makeMartinM1Pcm16Source(pixels, martinConfig),
            2U,
            SstvTxChannelRoute::Both);
        const std::uint64_t expectedMartinFrames =
            static_cast<std::uint64_t>(SstvMartinM1Protocol::HeaderDuration.count)
                * martinConfig.sampleRate
                / static_cast<std::uint64_t>(kPicosecondsPerSecond)
            + static_cast<std::uint64_t>(SstvMartinM1Protocol::ImageDuration.count)
                * martinConfig.sampleRate
                / static_cast<std::uint64_t>(kPicosecondsPerSecond);
        QCOMPARE(martin.totalFrames(), expectedMartinFrames);
        QCOMPARE(martin.size(), static_cast<qint64>(expectedMartinFrames * 4U));
        QVERIFY(martin.open(QIODevice::ReadOnly));
        const QByteArray martinChunk = martin.read(1'000'000);
        QCOMPARE(martinChunk.size(), qsizetype {16'384});
        QCOMPARE(martin.framesProducedBySource(), std::uint64_t {4'096U});
        QVERIFY(!martin.atEnd());
        martin.cancel();

        SstvScottieEncoderConfig scottieConfig;
        scottieConfig.mode = SstvScottieMode::S2;
        scottieConfig.sampleRate = 48'000U;
        SstvTxAudioDevice scottie(
            makeScottiePcm16Source(pixels, scottieConfig), 1U);
        const auto scottieSpec = SstvScottieProtocol::spec(SstvScottieMode::S2);
        const std::uint64_t expectedScottieFrames =
            static_cast<std::uint64_t>(SstvScottieProtocol::HeaderDuration.count)
                * scottieConfig.sampleRate
                / static_cast<std::uint64_t>(kPicosecondsPerSecond)
            + static_cast<std::uint64_t>(scottieSpec.imageDuration.count)
                * scottieConfig.sampleRate
                / static_cast<std::uint64_t>(kPicosecondsPerSecond);
        QCOMPARE(scottie.totalFrames(), expectedScottieFrames);
        QVERIFY(scottie.open(QIODevice::ReadOnly));
        QCOMPARE(scottie.read(7'777).size(), qsizetype {7'777});
        QVERIFY(!scottie.atEnd());
        scottie.cancel();

        SstvFskIdTxConfig fskConfig;
        fskConfig.sampleRate = 48'000U;
        SstvTxAudioDevice fsk(
            makeFskIdPcm16Source("IU8LMC", fskConfig), 1U);
        QVERIFY(fsk.open(QIODevice::ReadOnly));
        const QByteArray fskPcm = fsk.readAll();
        QCOMPARE(fskPcm.size(), fsk.size());
        QCOMPARE(fsk.framesProducedBySource(), fsk.totalFrames());
        QVERIFY(fsk.atEnd());
    }

    void cancelStopsFurtherPcmAndResetRestarts()
    {
        std::vector<std::int16_t> samples(10'000U, 321);
        SstvTxAudioDevice device(
            std::make_unique<VectorSource>(samples), 1U);
        QVERIFY(device.open(QIODevice::ReadOnly));
        QCOMPARE(device.read(17).size(), qsizetype {17});
        const qint64 position = device.pos();

        device.cancel();
        QVERIFY(device.cancelled());
        QVERIFY(device.atEnd());
        QCOMPARE(device.bytesAvailable(), qint64 {0});
        QCOMPARE(device.read(100).size(), qsizetype {0});
        QCOMPARE(device.pos(), position);

        device.close();
        QVERIFY(device.reset());
        QVERIFY(!device.cancelled());
        QVERIFY(!device.failed());
        QCOMPARE(device.pos(), qint64 {0});
        QVERIFY(device.open(QIODevice::ReadOnly));
        QCOMPARE(device.read(17).size(), qsizetype {17});
    }

    void sourceContractFailureIsReported()
    {
        SstvTxAudioDevice device(
            std::make_unique<DeclaredLengthSource>(10U), 1U);
        QVERIFY(device.open(QIODevice::ReadOnly));
        char output[16] {};
        QCOMPARE(device.read(output, sizeof(output)), qint64 {-1});
        QVERIFY(device.failed());
        QVERIFY(device.atEnd());
        QVERIFY(device.errorString().contains(QStringLiteral("no progress")));
    }

    void ownerThreadTelemetryIsAtomicDuringBackendPull()
    {
        constexpr std::size_t frameCount = 250'000U;
        SstvTxAudioDevice device(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t>(frameCount, 777)),
            2U);
        QVERIFY(device.open(QIODevice::ReadOnly));

        std::atomic_bool readerDone {false};
        std::atomic_bool readerFailed {false};
        std::thread reader([&]() {
            QByteArray chunk(3'337, Qt::Uninitialized);
            const qint64 target = device.size();
            qint64 consumed = 0;
            while (consumed < target) {
                const qint64 count = device.read(
                    chunk.data(), std::min(chunk.size(), target - consumed));
                if (count <= 0) {
                    readerFailed.store(true, std::memory_order_release);
                    break;
                }
                consumed += count;
            }
            readerDone.store(true, std::memory_order_release);
        });

        bool monotonic = true;
        std::uint64_t lastRead = 0U;
        std::uint64_t lastProduced = 0U;
        while (!readerDone.load(std::memory_order_acquire)) {
            const std::uint64_t read = device.framesReadFromDevice();
            const std::uint64_t produced = device.framesProducedBySource();
            monotonic = monotonic
                && read >= lastRead && read <= device.totalFrames()
                && produced >= lastProduced
                && produced <= device.totalFrames();
            lastRead = read;
            lastProduced = produced;
            std::this_thread::yield();
        }
        reader.join();

        QVERIFY(monotonic);
        QVERIFY(!readerFailed.load(std::memory_order_acquire));
        QCOMPARE(device.framesReadFromDevice(), device.totalFrames());
        QCOMPARE(device.framesProducedBySource(), device.totalFrames());
        QCOMPARE(device.bytesReadFromDevice(), device.size());
    }

    void rejectsInvalidConstructionAndOpenModes()
    {
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvTxAudioDevice(nullptr, 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvTxAudioDevice(
                std::make_unique<VectorSource>(std::vector<std::int16_t>{}),
                0U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvTxAudioDevice(
                std::make_unique<VectorSource>(std::vector<std::int16_t>{}),
                3U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvTxAudioDevice(
                std::make_unique<VectorSource>(std::vector<std::int16_t>{}),
                1U,
                SstvTxChannelRoute::Left));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvTxAudioDevice(
                std::make_unique<VectorSource>(std::vector<std::int16_t>{}),
                2U,
                static_cast<SstvTxChannelRoute>(255)));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvTxAudioDevice(
                std::make_unique<VectorSource>(
                    std::vector<std::int16_t>{}, 7'999U),
                1U));

        constexpr std::uint64_t bytesPerStereoFrame = 4U;
        const std::uint64_t overflowingFrames =
            static_cast<std::uint64_t>(std::numeric_limits<qint64>::max())
                / bytesPerStereoFrame + 1U;
        QVERIFY_THROWS_EXCEPTION(
            std::overflow_error,
            SstvTxAudioDevice(
                std::make_unique<DeclaredLengthSource>(overflowingFrames),
                2U));

        SstvTxAudioDevice device(
            std::make_unique<VectorSource>(std::vector<std::int16_t>{1}),
            1U);
        QVERIFY(!device.open(QIODevice::WriteOnly));
        QVERIFY(device.open(QIODevice::ReadOnly));
        QVERIFY(!device.open(QIODevice::ReadOnly));
        QVERIFY(!device.reset());
    }
};

QTEST_APPLESS_MAIN(TestSstvTxAudioDevice)
#include "test_sstv_tx_audio_device.moc"
