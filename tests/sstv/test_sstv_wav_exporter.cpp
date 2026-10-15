// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/integration/SstvTxCoordinator.h"
#include "../../src/sstv/integration/SstvWavExporter.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QTemporaryDir>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace decodium::sstv;

namespace {

struct SourceObservation final
{
    std::size_t maximumRequested {0U};
    std::size_t pullCalls {0U};
    bool cancelCalled {false};
};

class VectorSource final : public SstvPcm16Source
{
public:
    enum class Behaviour : std::uint8_t
    {
        Normal,
        Stall,
        Throw,
        ThrowLong,
        FinishEarly,
        LieAboutProduced,
        OverReportCount,
        NoProducedAdvance,
        ChangeDeclaredLength,
    };

    explicit VectorSource(std::vector<std::int16_t> samples,
                          std::uint32_t sampleRate = 48'000U,
                          Behaviour behaviour = Behaviour::Normal,
                          std::function<void()> afterPull = {},
                          std::shared_ptr<SourceObservation> observation =
                              std::make_shared<SourceObservation>())
        : samples_(std::move(samples))
        , sampleRate_(sampleRate)
        , behaviour_(behaviour)
        , afterPull_(std::move(afterPull))
        , observation_(std::move(observation))
    {
        if (!observation_) {
            throw std::invalid_argument("test observation must not be null");
        }
    }

    std::uint32_t sampleRate() const noexcept override
    {
        return sampleRate_;
    }

    std::uint64_t totalSamples() const noexcept override
    {
        const std::uint64_t size = static_cast<std::uint64_t>(samples_.size());
        return behaviour_ == Behaviour::ChangeDeclaredLength
                && position_ != 0U
            ? size + 1U : size;
    }

    std::uint64_t producedSamples() const noexcept override
    {
        if (behaviour_ == Behaviour::LieAboutProduced && complete()) {
            return position_ + 1U;
        }
        return position_;
    }

    bool complete() const noexcept override
    {
        if (behaviour_ == Behaviour::FinishEarly) {
            return position_ >= samples_.size() / 2U;
        }
        return position_ >= samples_.size();
    }

    bool cancelled() const noexcept override
    {
        return cancelled_;
    }

    std::size_t pullPcm16(std::int16_t* output,
                          std::size_t capacity) override
    {
        observation_->maximumRequested = std::max(
            observation_->maximumRequested, capacity);
        ++observation_->pullCalls;
        if (behaviour_ == Behaviour::Throw) {
            throw std::runtime_error("synthetic source failure");
        }
        if (behaviour_ == Behaviour::ThrowLong) {
            throw std::runtime_error(std::string(100'000U, 'x'));
        }
        if (behaviour_ == Behaviour::Stall || cancelled_ || complete()) {
            return 0U;
        }
        const std::size_t count = std::min(capacity,
                                           samples_.size() - position_);
        std::copy_n(samples_.data() + position_, count, output);
        if (behaviour_ != Behaviour::NoProducedAdvance) {
            position_ += count;
        }
        if (afterPull_) {
            afterPull_();
        }
        return behaviour_ == Behaviour::OverReportCount
            ? count + 1U : count;
    }

    void cancel() noexcept override
    {
        cancelled_ = true;
        observation_->cancelCalled = true;
    }

    void reset() override
    {
        position_ = 0U;
        observation_->maximumRequested = 0U;
        observation_->pullCalls = 0U;
        observation_->cancelCalled = false;
        cancelled_ = false;
    }

    std::shared_ptr<SourceObservation> observation() const noexcept
    {
        return observation_;
    }

private:
    std::vector<std::int16_t> samples_;
    std::uint32_t sampleRate_ {48'000U};
    Behaviour behaviour_ {Behaviour::Normal};
    std::size_t position_ {0U};
    std::function<void()> afterPull_;
    std::shared_ptr<SourceObservation> observation_;
    bool cancelled_ {false};
};

class DeclaredLengthSource final : public SstvPcm16Source
{
public:
    explicit DeclaredLengthSource(std::uint64_t samples) noexcept
        : samples_(samples)
    {
    }

    std::uint32_t sampleRate() const noexcept override { return 48'000U; }
    std::uint64_t totalSamples() const noexcept override { return samples_; }
    std::uint64_t producedSamples() const noexcept override { return 0U; }
    bool complete() const noexcept override { return false; }
    bool cancelled() const noexcept override { return cancelled_; }
    std::size_t pullPcm16(std::int16_t*, std::size_t) override { return 0U; }
    void cancel() noexcept override { cancelled_ = true; }
    void reset() override { cancelled_ = false; }

private:
    std::uint64_t samples_ {0U};
    bool cancelled_ {false};
};

std::uint16_t littleEndian16(const QByteArray& bytes, qsizetype offset)
{
    return static_cast<std::uint16_t>(
               static_cast<unsigned char>(bytes.at(offset)))
        | static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(
                static_cast<unsigned char>(bytes.at(offset + 1))) << 8U);
}

std::uint32_t littleEndian32(const QByteArray& bytes, qsizetype offset)
{
    return static_cast<std::uint32_t>(
               static_cast<unsigned char>(bytes.at(offset)))
        | (static_cast<std::uint32_t>(
               static_cast<unsigned char>(bytes.at(offset + 1))) << 8U)
        | (static_cast<std::uint32_t>(
               static_cast<unsigned char>(bytes.at(offset + 2))) << 16U)
        | (static_cast<std::uint32_t>(
               static_cast<unsigned char>(bytes.at(offset + 3))) << 24U);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("test cannot read file");
    }
    return file.readAll();
}

SstvWavExportRequest requestFor(const QString& path)
{
    SstvWavExportRequest request;
    request.outputPath = path;
    request.mode = QStringLiteral("Martin M1");
    request.pullSamples = 256U;
    return request;
}

} // namespace

class TestSstvWavExporter final : public QObject
{
    Q_OBJECT

private slots:
    void atomicallyWritesCanonicalWavAndMetadata()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString wavPath = directory.filePath(QStringLiteral("frame.wav"));
        const std::vector<std::int16_t> samples {
            -32'768, -1, 0, 1, 32'767, 2'048, -4'096,
        };
        auto source = std::make_unique<VectorSource>(samples, 12'000U);
        const std::shared_ptr<SourceObservation> observation =
            source->observation();

        SstvWavExportRequest request = requestFor(wavPath);
        request.writeMetadataSidecar = true;
        request.metadata.insert(QStringLiteral("callsign"),
                                QStringLiteral("IU8LMC"));
        const SstvWavExportResult result = SstvWavExporter::exportAtomic(
            std::move(source), request);

        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.code, SstvWavExportError::None);
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.warning.isEmpty());
        QCOMPARE(result.wavPath, wavPath);
        QCOMPARE(result.fileSizeBytes,
                 qint64 {44 + static_cast<qint64>(samples.size() * 2U)});
        QCOMPARE(result.metrics.samplesWritten,
                 std::uint64_t {samples.size()});
        QCOMPARE(observation->maximumRequested, samples.size());
        QCOMPARE(result.sha256.size(), qsizetype {32});
        QVERIFY(result.metadataCommitted);
        QCOMPARE(result.metadataPath,
                 directory.filePath(QStringLiteral("frame.json")));

        const QByteArray wav = readFile(wavPath);
        QCOMPARE(wav.left(4), QByteArrayLiteral("RIFF"));
        QCOMPARE(wav.mid(8, 4), QByteArrayLiteral("WAVE"));
        QCOMPARE(wav.mid(12, 4), QByteArrayLiteral("fmt "));
        QCOMPARE(wav.mid(36, 4), QByteArrayLiteral("data"));
        QCOMPARE(littleEndian32(wav, 4),
                 static_cast<std::uint32_t>(36U + samples.size() * 2U));
        QCOMPARE(littleEndian16(wav, 20), std::uint16_t {1U});
        QCOMPARE(littleEndian16(wav, 22), std::uint16_t {1U});
        QCOMPARE(littleEndian32(wav, 24), std::uint32_t {12'000U});
        QCOMPARE(littleEndian16(wav, 34), std::uint16_t {16U});
        QCOMPARE(littleEndian32(wav, 40),
                 static_cast<std::uint32_t>(samples.size() * 2U));
        QCOMPARE(QCryptographicHash::hash(wav, QCryptographicHash::Sha256),
                 result.sha256);

        for (std::size_t index = 0U; index < samples.size(); ++index) {
            QCOMPARE(static_cast<std::int16_t>(
                         littleEndian16(wav,
                                        44 + static_cast<qsizetype>(index * 2U))),
                     samples.at(index));
        }

        const QJsonDocument sidecar = QJsonDocument::fromJson(
            readFile(result.metadataPath));
        QVERIFY(sidecar.isObject());
        const QJsonObject metadata = sidecar.object();
        QCOMPARE(metadata.value(QStringLiteral("schema")).toString(),
                 QStringLiteral("decodium-sstv-wav-metadata"));
        QCOMPARE(metadata.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("Martin M1"));
        QCOMPARE(metadata.value(QStringLiteral("sampleRate")).toInteger(),
                 qint64 {12'000});
        QCOMPARE(metadata.value(QStringLiteral("sampleCount")).toInteger(),
                 static_cast<qint64>(samples.size()));
        QCOMPARE(metadata.value(QStringLiteral("sha256")).toString(),
                 QString::fromLatin1(result.sha256.toHex()));
        QCOMPARE(metadata.value(QStringLiteral("metadata")).toObject()
                     .value(QStringLiteral("callsign")).toString(),
                 QStringLiteral("IU8LMC"));
        QVERIFY(!QFileInfo::exists(wavPath + QStringLiteral(".lock")));
    }

    void exportsNativeBuilderCompositeWithoutMaterializingWaveform()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SstvTxSourceBuilderConfig builderConfig;
        builderConfig.mode = SstvTxCoordinatorMode::ScottieS2;
        builderConfig.sampleRate = 8'000U;
        builderConfig.fskId = SstvTxFskIdPlan {
            "IU8LMC",
            SstvFskIdCodec::TextPolicy::Callsign,
            SstvFskIdCodec::InputHandling::Strict};
        const std::vector<SstvRgbPixel> pixels(
            SstvMartinM1Encoder::PixelCount,
            SstvRgbPixel {12U, 34U, 56U});
        SstvTxBuiltSource built = SstvTxSourceBuilder::build(
            pixels, builderConfig);
        QVERIFY(built.source != nullptr);
        QCOMPARE(built.source->producedSamples(), std::uint64_t {0U});
        QVERIFY(built.totalFrames > SstvWavExporter::MaximumPullSamples);
        QVERIFY(built.fskIdPlanned);
        QVERIFY(built.fskIdFrames > 0U);

        SstvWavExportRequest request = requestFor(
            directory.filePath(QStringLiteral("native.wav")));
        request.mode = QString::fromLatin1(built.mode);
        request.pullSamples = 4'096U;
        const SstvWavExportResult result = SstvWavExporter::exportAtomic(
            std::move(built.source), request);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.metrics.samplesWritten, built.totalFrames);
        QCOMPARE(result.fileSizeBytes,
                 static_cast<qint64>(SstvWavStreamWriter::kHeaderBytes
                                     + built.totalFrames * 2U));
        QCOMPARE(result.sha256.size(), qsizetype {32});
    }

    void exportsEveryMartinFamilyModeAtExactProtocolLength()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const struct {
            SstvTxCoordinatorMode mode;
            SstvMartinMode protocolMode;
            const char* id;
            std::uint64_t totalFramesAt12k;
        } cases[] {
            {SstvTxCoordinatorMode::MartinM2,
             SstvMartinMode::M2, "martin-m2", 707'643U},
            {SstvTxCoordinatorMode::MartinM3,
             SstvMartinMode::M3, "martin-m3", 696'661U},
            {SstvTxCoordinatorMode::MartinM4,
             SstvMartinMode::M4, "martin-m4", 359'281U},
        };

        for (const auto& item : cases) {
            SstvTxSourceBuilderConfig builderConfig;
            builderConfig.mode = item.mode;
            builderConfig.sampleRate = 12'000U;
            const std::vector<SstvRgbPixel> pixels(
                SstvMartinM1Encoder::pixelCount(item.protocolMode),
                SstvRgbPixel {255U, 0U, 128U});
            SstvTxBuiltSource built = SstvTxSourceBuilder::build(
                pixels, builderConfig);
            QCOMPARE(built.mode, std::string(item.id));
            QCOMPARE(built.headerFrames, std::uint64_t {10'920U});
            QCOMPARE(built.imageEndFrame, item.totalFramesAt12k);
            QCOMPARE(built.totalFrames, item.totalFramesAt12k);

            const QString wavPath = directory.filePath(
                QString::fromLatin1(item.id) + QStringLiteral(".wav"));
            SstvWavExportRequest request = requestFor(wavPath);
            request.mode = QString::fromLatin1(item.id);
            request.pullSamples = 4'096U;
            const SstvWavExportResult result = SstvWavExporter::exportAtomic(
                std::move(built.source), request);
            QVERIFY2(result.ok, qPrintable(result.error));
            QCOMPARE(result.metrics.samplesWritten, item.totalFramesAt12k);
            QCOMPARE(result.fileSizeBytes,
                     static_cast<qint64>(SstvWavStreamWriter::kHeaderBytes
                                         + item.totalFramesAt12k * 2U));

            const QByteArray wav = readFile(wavPath);
            QCOMPARE(wav.left(4), QByteArrayLiteral("RIFF"));
            QCOMPARE(littleEndian32(wav, 24), std::uint32_t {12'000U});
            QCOMPARE(littleEndian32(wav, 40),
                     static_cast<std::uint32_t>(
                         item.totalFramesAt12k * 2U));
            QCOMPARE(static_cast<std::uint64_t>(wav.size()),
                     SstvWavStreamWriter::kHeaderBytes
                         + item.totalFramesAt12k * 2U);
        }
    }

    void exportsEveryRobotModeAtExactProtocolLength()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const struct {
            SstvTxCoordinatorMode mode;
            SstvRobotMode protocolMode;
            std::uint64_t totalFramesAt12k;
        } cases[] {
            {SstvTxCoordinatorMode::RobotColour12,
             SstvRobotMode::Colour12, 154'920U},
            {SstvTxCoordinatorMode::RobotColour24,
             SstvRobotMode::Colour24, 298'920U},
            {SstvTxCoordinatorMode::RobotColour36,
             SstvRobotMode::Colour36, 442'920U},
            {SstvTxCoordinatorMode::RobotColour72,
             SstvRobotMode::Colour72, 874'920U},
            {SstvTxCoordinatorMode::RobotBw8,
             SstvRobotMode::Bw8, 105'960U},
            {SstvTxCoordinatorMode::RobotBw12,
             SstvRobotMode::Bw12, 154'920U},
            {SstvTxCoordinatorMode::RobotBw24,
             SstvRobotMode::Bw24, 313'320U},
            {SstvTxCoordinatorMode::RobotBw36,
             SstvRobotMode::Bw36, 442'920U},
        };

        for (const auto& item : cases) {
            const SstvRobotModeSpec spec = SstvRobotProtocol::spec(
                item.protocolMode);
            SstvTxSourceBuilderConfig builderConfig;
            builderConfig.mode = item.mode;
            builderConfig.sampleRate = 12'000U;
            const std::vector<SstvRgbPixel> pixels(
                SstvRobotEncoder::pixelCount(item.protocolMode),
                SstvRgbPixel {255U, 0U, 128U});
            SstvTxBuiltSource built = SstvTxSourceBuilder::build(
                pixels, builderConfig);
            QCOMPARE(built.mode, std::string(spec.stableId));
            QCOMPARE(built.width, spec.width);
            QCOMPARE(built.height, spec.height);
            QCOMPARE(built.headerFrames, std::uint64_t {10'920U});
            QCOMPARE(built.imageEndFrame, item.totalFramesAt12k);
            QCOMPARE(built.totalFrames, item.totalFramesAt12k);

            const QString wavPath = directory.filePath(
                QString::fromLatin1(spec.stableId)
                + QStringLiteral(".wav"));
            SstvWavExportRequest request = requestFor(wavPath);
            request.mode = QString::fromLatin1(spec.stableId);
            request.pullSamples = 4'096U;
            const SstvWavExportResult result = SstvWavExporter::exportAtomic(
                std::move(built.source), request);
            QVERIFY2(result.ok, qPrintable(result.error));
            QCOMPARE(result.metrics.samplesWritten, item.totalFramesAt12k);
            QCOMPARE(result.fileSizeBytes,
                     static_cast<qint64>(SstvWavStreamWriter::kHeaderBytes
                                         + item.totalFramesAt12k * 2U));

            const QByteArray wav = readFile(wavPath);
            QCOMPARE(wav.left(4), QByteArrayLiteral("RIFF"));
            QCOMPARE(littleEndian32(wav, 24), std::uint32_t {12'000U});
            QCOMPARE(littleEndian32(wav, 40),
                     static_cast<std::uint32_t>(
                         item.totalFramesAt12k * 2U));
            QCOMPARE(static_cast<std::uint64_t>(wav.size()),
                     SstvWavStreamWriter::kHeaderBytes
                         + item.totalFramesAt12k * 2U);
        }
    }

    void exportsEverySequentialRgbModeAtExactProtocolLength()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const struct {
            SstvTxCoordinatorMode mode;
            SstvSequentialRgbMode protocolMode;
            std::uint64_t totalFramesAt12k;
        } cases[] {
            {SstvTxCoordinatorMode::WraaseSc2_60,
             SstvSequentialRgbMode::WraaseSc2_60, 749'442U},
            {SstvTxCoordinatorMode::WraaseSc2_120,
             SstvSequentialRgbMode::WraaseSc2_120, 1'471'725U},
            {SstvTxCoordinatorMode::WraaseSc2_180,
             SstvSequentialRgbMode::WraaseSc2_180, 2'195'181U},
            {SstvTxCoordinatorMode::PasokonP3,
             SstvSequentialRgbMode::PasokonP3, 2'447'520U},
            {SstvTxCoordinatorMode::PasokonP5,
             SstvSequentialRgbMode::PasokonP5, 3'665'820U},
            {SstvTxCoordinatorMode::PasokonP7,
             SstvSequentialRgbMode::PasokonP7, 4'884'120U},
        };

        for (const auto& item : cases) {
            const SstvSequentialRgbModeSpec spec =
                SstvSequentialRgbProtocol::spec(item.protocolMode);
            SstvTxSourceBuilderConfig builderConfig;
            builderConfig.mode = item.mode;
            builderConfig.sampleRate = 12'000U;
            const std::vector<SstvRgbPixel> pixels(
                SstvSequentialRgbEncoder::pixelCount(item.protocolMode),
                SstvRgbPixel {255U, 0U, 128U});
            SstvTxBuiltSource built = SstvTxSourceBuilder::build(
                pixels, builderConfig);
            QCOMPARE(built.mode, std::string(spec.stableId));
            QCOMPARE(built.width, spec.width);
            QCOMPARE(built.height, spec.height);
            QCOMPARE(built.headerFrames, std::uint64_t {10'920U});
            QCOMPARE(built.imageEndFrame, item.totalFramesAt12k);
            QCOMPARE(built.totalFrames, item.totalFramesAt12k);

            const QString wavPath = directory.filePath(
                QString::fromLatin1(spec.stableId)
                + QStringLiteral(".wav"));
            SstvWavExportRequest request = requestFor(wavPath);
            request.mode = QString::fromLatin1(spec.stableId);
            request.pullSamples = 4'096U;
            const SstvWavExportResult result = SstvWavExporter::exportAtomic(
                std::move(built.source), request);
            QVERIFY2(result.ok, qPrintable(result.error));
            QCOMPARE(result.metrics.samplesWritten, item.totalFramesAt12k);
            QCOMPARE(result.fileSizeBytes,
                     static_cast<qint64>(SstvWavStreamWriter::kHeaderBytes
                                         + item.totalFramesAt12k * 2U));

            const QByteArray wav = readFile(wavPath);
            QCOMPARE(wav.left(4), QByteArrayLiteral("RIFF"));
            QCOMPARE(littleEndian32(wav, 24), std::uint32_t {12'000U});
            QCOMPARE(littleEndian32(wav, 40),
                     static_cast<std::uint32_t>(
                         item.totalFramesAt12k * 2U));
            QCOMPARE(static_cast<std::uint64_t>(wav.size()),
                     SstvWavStreamWriter::kHeaderBytes
                         + item.totalFramesAt12k * 2U);
        }
    }

    void exportsEveryPdModeAtExactCanonicalProtocolLength()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const struct {
            SstvTxCoordinatorMode mode;
            SstvPdMode protocolMode;
            std::uint64_t totalFramesAt12k;
        } cases[] {
            {SstvTxCoordinatorMode::Pd50, SstvPdMode::Pd50, 607'133U},
            {SstvTxCoordinatorMode::Pd90, SstvPdMode::Pd90, 1'090'789U},
            {SstvTxCoordinatorMode::Pd120, SstvPdMode::Pd120, 1'524'156U},
            {SstvTxCoordinatorMode::Pd160, SstvPdMode::Pd160, 1'941'518U},
            {SstvTxCoordinatorMode::Pd180, SstvPdMode::Pd180, 2'255'538U},
            {SstvTxCoordinatorMode::Pd240, SstvPdMode::Pd240, 2'986'920U},
            {SstvTxCoordinatorMode::Pd290, SstvPdMode::Pd290, 3'475'106U},
        };

        for (const auto& item : cases) {
            const SstvPdModeSpec spec = SstvPdProtocol::spec(
                item.protocolMode);
            SstvTxSourceBuilderConfig builderConfig;
            builderConfig.mode = item.mode;
            builderConfig.sampleRate = 12'000U;
            const std::vector<SstvRgbPixel> pixels(
                SstvPdEncoder::pixelCount(item.protocolMode),
                SstvRgbPixel {255U, 0U, 128U});
            SstvTxBuiltSource built = SstvTxSourceBuilder::build(
                pixels, builderConfig);
            QCOMPARE(built.mode, std::string(spec.stableId));
            QCOMPARE(built.width, spec.width);
            QCOMPARE(built.height, spec.height);
            QCOMPARE(built.headerFrames, std::uint64_t {10'920U});
            QCOMPARE(built.imageEndFrame, item.totalFramesAt12k);
            QCOMPARE(built.totalFrames, item.totalFramesAt12k);

            const QString wavPath = directory.filePath(
                QString::fromLatin1(spec.stableId)
                + QStringLiteral(".wav"));
            SstvWavExportRequest request = requestFor(wavPath);
            request.mode = QString::fromLatin1(spec.stableId);
            request.pullSamples = 4'096U;
            const SstvWavExportResult result = SstvWavExporter::exportAtomic(
                std::move(built.source), request);
            QVERIFY2(result.ok, qPrintable(result.error));
            QCOMPARE(result.metrics.samplesWritten,
                     item.totalFramesAt12k);
            QCOMPARE(result.fileSizeBytes,
                     static_cast<qint64>(SstvWavStreamWriter::kHeaderBytes
                                         + item.totalFramesAt12k * 2U));

            const QByteArray wav = readFile(wavPath);
            QCOMPARE(wav.left(4), QByteArrayLiteral("RIFF"));
            QCOMPARE(littleEndian32(wav, 24), std::uint32_t {12'000U});
            QCOMPARE(littleEndian32(wav, 40),
                     static_cast<std::uint32_t>(
                         item.totalFramesAt12k * 2U));
            QCOMPARE(static_cast<std::uint64_t>(wav.size()),
                     SstvWavStreamWriter::kHeaderBytes
                         + item.totalFramesAt12k * 2U);
        }
    }

    void exportsEveryNormalAvtModeAtExactProtectedProtocolLength()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const struct {
            SstvTxCoordinatorMode mode;
            SstvAvtMode protocolMode;
            std::uint64_t totalFramesAt12k;
        } cases[] {
            {SstvTxCoordinatorMode::Avt24,
             SstvAvtMode::Avt24, 366'510U},
            {SstvTxCoordinatorMode::Avt90,
             SstvAvtMode::Avt90, 1'176'510U},
            {SstvTxCoordinatorMode::Avt94,
             SstvAvtMode::Avt94, 1'221'510U},
        };

        for (const auto& item : cases) {
            const SstvAvtModeSpec spec = SstvAvtProtocol::spec(
                item.protocolMode);
            SstvTxSourceBuilderConfig builderConfig;
            builderConfig.mode = item.mode;
            builderConfig.sampleRate = 12'000U;
            const std::vector<SstvRgbPixel> pixels(
                SstvAvtEncoder::pixelCount(item.protocolMode),
                SstvRgbPixel {255U, 0U, 128U});
            SstvTxBuiltSource built = SstvTxSourceBuilder::build(
                pixels, builderConfig);
            QCOMPARE(built.mode, std::string(spec.stableId));
            QCOMPARE(built.width, spec.width);
            QCOMPARE(built.height, spec.height);
            QCOMPARE(built.headerFrames, std::uint64_t {96'510U});
            QCOMPARE(built.imageEndFrame, item.totalFramesAt12k);
            QCOMPARE(built.totalFrames, item.totalFramesAt12k);

            const QString wavPath = directory.filePath(
                QString::fromLatin1(spec.stableId)
                + QStringLiteral(".wav"));
            SstvWavExportRequest request = requestFor(wavPath);
            request.mode = QString::fromLatin1(spec.stableId);
            request.pullSamples = 4'096U;
            const SstvWavExportResult result = SstvWavExporter::exportAtomic(
                std::move(built.source), request);
            QVERIFY2(result.ok, qPrintable(result.error));
            QCOMPARE(result.metrics.samplesWritten,
                     item.totalFramesAt12k);
            QCOMPARE(result.fileSizeBytes,
                     static_cast<qint64>(SstvWavStreamWriter::kHeaderBytes
                                         + item.totalFramesAt12k * 2U));

            const QByteArray wav = readFile(wavPath);
            QCOMPARE(wav.left(4), QByteArrayLiteral("RIFF"));
            QCOMPARE(littleEndian32(wav, 24), std::uint32_t {12'000U});
            QCOMPARE(littleEndian32(wav, 40),
                     static_cast<std::uint32_t>(
                         item.totalFramesAt12k * 2U));
            QCOMPARE(static_cast<std::uint64_t>(wav.size()),
                     SstvWavStreamWriter::kHeaderBytes
                         + item.totalFramesAt12k * 2U);
        }
    }

    void exportsEveryMmsstvExtendedModeAtExactProtocolLength()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const struct {
            SstvTxCoordinatorMode mode;
            SstvMmsstvMode protocolMode;
            std::uint64_t totalFramesAt12k;
        } cases[] {
            {SstvTxCoordinatorMode::Mp73, SstvMmsstvMode::Mp73, 889'320U},
            {SstvTxCoordinatorMode::Mp115,
             SstvMmsstvMode::Mp115, 1'399'272U},
            {SstvTxCoordinatorMode::Mp140,
             SstvMmsstvMode::Mp140, 1'688'040U},
            {SstvTxCoordinatorMode::Mp175,
             SstvMmsstvMode::Mp175, 2'118'120U},
            {SstvTxCoordinatorMode::Mr73, SstvMmsstvMode::Mr73, 893'313U},
            {SstvTxCoordinatorMode::Mr90, SstvMmsstvMode::Mr90, 1'096'065U},
            {SstvTxCoordinatorMode::Mr115,
             SstvMmsstvMode::Mr115, 1'397'121U},
            {SstvTxCoordinatorMode::Mr140,
             SstvMmsstvMode::Mr140, 1'698'177U},
            {SstvTxCoordinatorMode::Mr175,
             SstvMmsstvMode::Mr175, 2'115'969U},
            {SstvTxCoordinatorMode::Ml180,
             SstvMmsstvMode::Ml180, 2'176'161U},
            {SstvTxCoordinatorMode::Ml240,
             SstvMmsstvMode::Ml240, 2'890'401U},
            {SstvTxCoordinatorMode::Ml280,
             SstvMmsstvMode::Ml280, 3'378'465U},
            {SstvTxCoordinatorMode::Ml320,
             SstvMmsstvMode::Ml320, 3'854'625U},
            {SstvTxCoordinatorMode::Mp73Narrow,
             SstvMmsstvMode::Mp73Narrow, 886'920U},
            {SstvTxCoordinatorMode::Mp110Narrow,
             SstvMmsstvMode::Mp110Narrow, 1'329'288U},
            {SstvTxCoordinatorMode::Mp140Narrow,
             SstvMmsstvMode::Mp140Narrow, 1'685'640U},
            {SstvTxCoordinatorMode::Mc110Narrow,
             SstvMmsstvMode::Mc110Narrow, 1'327'752U},
            {SstvTxCoordinatorMode::Mc140Narrow,
             SstvMmsstvMode::Mc140Narrow, 1'696'392U},
            {SstvTxCoordinatorMode::Mc180Narrow,
             SstvMmsstvMode::Mc180Narrow, 2'175'624U},
        };

        for (const auto& item : cases) {
            const SstvMmsstvModeSpec spec = SstvMmsstvProtocol::spec(
                item.protocolMode);
            SstvTxSourceBuilderConfig builderConfig;
            builderConfig.mode = item.mode;
            builderConfig.sampleRate = 12'000U;
            const std::vector<SstvRgbPixel> pixels(
                SstvMmsstvEncoder::pixelCount(item.protocolMode),
                SstvRgbPixel {255U, 0U, 128U});
            SstvTxBuiltSource built = SstvTxSourceBuilder::build(
                pixels, builderConfig);
            QCOMPARE(built.mode, std::string(spec.stableId));
            QCOMPARE(built.width, spec.width);
            QCOMPARE(built.height, spec.height);
            QCOMPARE(built.headerFrames,
                     spec.narrow ? std::uint64_t {11'400U}
                                 : std::uint64_t {13'800U});
            QCOMPARE(built.imageEndFrame, item.totalFramesAt12k);
            QCOMPARE(built.totalFrames, item.totalFramesAt12k);

            const QString wavPath = directory.filePath(
                QString::fromLatin1(spec.stableId)
                + QStringLiteral(".wav"));
            SstvWavExportRequest request = requestFor(wavPath);
            request.mode = QString::fromLatin1(spec.stableId);
            request.pullSamples = 4'096U;
            const SstvWavExportResult result = SstvWavExporter::exportAtomic(
                std::move(built.source), request);
            QVERIFY2(result.ok, qPrintable(result.error));
            QCOMPARE(result.metrics.samplesWritten, item.totalFramesAt12k);
            QCOMPARE(result.fileSizeBytes,
                     static_cast<qint64>(SstvWavStreamWriter::kHeaderBytes
                                         + item.totalFramesAt12k * 2U));

            const QByteArray wav = readFile(wavPath);
            QCOMPARE(wav.left(4), QByteArrayLiteral("RIFF"));
            QCOMPARE(littleEndian32(wav, 24), std::uint32_t {12'000U});
            QCOMPARE(littleEndian32(wav, 40),
                     static_cast<std::uint32_t>(
                         item.totalFramesAt12k * 2U));
            QCOMPARE(static_cast<std::uint64_t>(wav.size()),
                     SstvWavStreamWriter::kHeaderBytes
                         + item.totalFramesAt12k * 2U);
        }
    }

    void collisionPreservesExistingFileUnlessReplaceIsExplicit()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("existing.wav"));
        QFile existing(path);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write("original"), qint64 {8});
        existing.close();

        SstvWavExportRequest request = requestFor(path);
        auto result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            request);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::Collision);
        QCOMPARE(readFile(path), QByteArrayLiteral("original"));

        request.replaceExisting = true;
        result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            request);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(readFile(path).left(4), QByteArrayLiteral("RIFF"));

        const QByteArray committed = readFile(path);
        result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t>(1'024U, 7),
                48'000U,
                VectorSource::Behaviour::Stall),
            request);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::SourceFailure);
        QCOMPARE(readFile(path), committed);
    }

    void preCancelledExportLeavesNoDestination()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("cancel.wav"));
        auto cancelled = std::make_shared<std::atomic_bool>(true);

        const SstvWavExportResult result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            requestFor(path), cancelled);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::Cancelled);
        QVERIFY(!QFileInfo::exists(path));
        QVERIFY(!QFileInfo::exists(path + QStringLiteral(".lock")));
    }

    void cancellationOnFinalPullPreservesExistingReplacement()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("replace.wav"));
        QFile existing(path);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write("original"), qint64 {8});
        existing.close();

        auto cancelled = std::make_shared<std::atomic_bool>(false);
        auto observation = std::make_shared<SourceObservation>();
        auto source = std::make_unique<VectorSource>(
            std::vector<std::int16_t> {1, 2, 3},
            48'000U,
            VectorSource::Behaviour::Normal,
            [cancelled] {
                cancelled->store(true, std::memory_order_release);
            },
            observation);
        SstvWavExportRequest request = requestFor(path);
        request.replaceExisting = true;

        const SstvWavExportResult result = SstvWavExporter::exportAtomic(
            std::move(source), request, cancelled);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::Cancelled);
        QVERIFY(observation->cancelCalled);
        QCOMPARE(readFile(path), QByteArrayLiteral("original"));
        QVERIFY(!QFileInfo::exists(path + QStringLiteral(".lock")));
    }

    void concurrentCancellationStopsBeforeCommit()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("threaded.wav"));
        auto pullReached = std::make_shared<std::atomic_bool>(false);
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        auto observation = std::make_shared<SourceObservation>();
        auto source = std::make_unique<VectorSource>(
            std::vector<std::int16_t>(1'024U, 42),
            48'000U,
            VectorSource::Behaviour::Normal,
            [pullReached, cancelled] {
                pullReached->store(true, std::memory_order_release);
                while (!cancelled->load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
            },
            observation);
        std::thread cancellingThread([pullReached, cancelled] {
            while (!pullReached->load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            cancelled->store(true, std::memory_order_release);
        });

        const SstvWavExportResult result = SstvWavExporter::exportAtomic(
            std::move(source), requestFor(path), cancelled);
        cancellingThread.join();
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::Cancelled);
        QVERIFY(observation->cancelCalled);
        QVERIFY(!QFileInfo::exists(path));
        QVERIFY(!QFileInfo::exists(path + QStringLiteral(".lock")));
    }

    void destinationAppearingDuringRenderIsNotClobbered()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("late.wav"));
        bool intruderWritten = false;
        auto source = std::make_unique<VectorSource>(
            std::vector<std::int16_t>(512U, 17),
            48'000U,
            VectorSource::Behaviour::Normal,
            [&path, &intruderWritten] {
                if (intruderWritten) {
                    return;
                }
                QFile intruder(path);
                intruderWritten = intruder.open(QIODevice::WriteOnly)
                    && intruder.write("intruder") == 8;
            });

        const SstvWavExportResult result = SstvWavExporter::exportAtomic(
            std::move(source), requestFor(path));
        QVERIFY(intruderWritten);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::Collision);
        QCOMPARE(readFile(path), QByteArrayLiteral("intruder"));
        QCOMPARE(QDir(directory.path()).entryList(
                     QDir::Files | QDir::NoDotAndDotDot),
                 QStringList {QStringLiteral("late.wav")});

        const QString target = directory.filePath(QStringLiteral("target.bin"));
        QFile targetFile(target);
        QVERIFY(targetFile.open(QIODevice::WriteOnly));
        QCOMPARE(targetFile.write("safe"), qint64 {4});
        targetFile.close();
        const QString lateLink = directory.filePath(
            QStringLiteral("late-link.wav"));
        bool linkCreated = false;
        auto linkSource = std::make_unique<VectorSource>(
            std::vector<std::int16_t> {1, 2, 3},
            48'000U,
            VectorSource::Behaviour::Normal,
            [&target, &lateLink, &linkCreated] {
                linkCreated = QFile::link(target, lateLink);
            });
        SstvWavExportRequest replaceRequest = requestFor(lateLink);
        replaceRequest.replaceExisting = true;
        const SstvWavExportResult linkResult = SstvWavExporter::exportAtomic(
            std::move(linkSource), replaceRequest);
        if (!linkCreated) {
            QTest::qSkip("symbolic links are unavailable on this test platform",
                         __FILE__,
                         __LINE__);
            return;
        }
        QVERIFY(!linkResult.ok);
        QCOMPARE(linkResult.code, SstvWavExportError::InvalidRequest);
        QCOMPARE(readFile(target), QByteArrayLiteral("safe"));
        QVERIFY(QFileInfo(lateLink).isSymLink());
    }

    void cooperativeLockPreventsConcurrentExporter()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("locked.wav"));
        QLockFile lock(path + QStringLiteral(".lock"));
        QVERIFY(lock.tryLock(0));

        const SstvWavExportResult result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            requestFor(path));
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::Locked);
        QVERIFY(!QFileInfo::exists(path));
        lock.unlock();
    }

    void commitFailureDoesNotDamageExistingDirectory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("blocked.wav"));
        QVERIFY(QDir().mkpath(path));
        SstvWavExportRequest request = requestFor(path);
        request.replaceExisting = true;

        const SstvWavExportResult result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            request);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::CommitFailure);
        const QFileInfo stillDirectory(path);
        QVERIFY(stillDirectory.exists());
        QVERIFY(stillDirectory.isDir());
    }

    void sourceFailuresNeverPublishPartialWav_data()
    {
        QTest::addColumn<int>("behaviour");
        QTest::newRow("stall")
            << static_cast<int>(VectorSource::Behaviour::Stall);
        QTest::newRow("throw")
            << static_cast<int>(VectorSource::Behaviour::Throw);
        QTest::newRow("throw-long-detail")
            << static_cast<int>(VectorSource::Behaviour::ThrowLong);
        QTest::newRow("finish-early")
            << static_cast<int>(VectorSource::Behaviour::FinishEarly);
        QTest::newRow("lying-produced-count")
            << static_cast<int>(VectorSource::Behaviour::LieAboutProduced);
        QTest::newRow("over-reported-pull-count")
            << static_cast<int>(VectorSource::Behaviour::OverReportCount);
        QTest::newRow("produced-count-does-not-advance")
            << static_cast<int>(VectorSource::Behaviour::NoProducedAdvance);
        QTest::newRow("declared-length-changes")
            << static_cast<int>(VectorSource::Behaviour::ChangeDeclaredLength);
    }

    void sourceFailuresNeverPublishPartialWav()
    {
        QFETCH(int, behaviour);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("failed.wav"));
        const auto selected = static_cast<VectorSource::Behaviour>(behaviour);

        const SstvWavExportResult result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t>(1'024U, 123), 48'000U, selected),
            requestFor(path));
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::SourceFailure);
        QVERIFY(result.error.size()
                <= SstvWavExporter::MaximumErrorCharacters);
        QVERIFY(!QFileInfo::exists(path));
        QVERIFY(!QFileInfo::exists(path + QStringLiteral(".lock")));
    }

    void metadataFailureIsReportedWithoutDestroyingCommittedWav()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("bounded.wav"));
        SstvWavExportRequest request = requestFor(path);
        request.writeMetadataSidecar = true;
        request.metadata.insert(
            QStringLiteral("oversized"),
            QString(static_cast<qsizetype>(
                        SstvWavExporter::MaximumMetadataBytes + 1),
                    QLatin1Char('x')));

        const SstvWavExportResult result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            request);
        QVERIFY(result.ok);
        QCOMPARE(result.code, SstvWavExportError::None);
        QVERIFY(!result.warning.isEmpty());
        QVERIFY(!result.metadataCommitted);
        QVERIFY(result.metadataPath.isEmpty());
        QCOMPARE(readFile(path).left(4), QByteArrayLiteral("RIFF"));
        QVERIFY(!QFileInfo::exists(
            directory.filePath(QStringLiteral("bounded.json"))));

        QJsonObject nested;
        nested.insert(QStringLiteral("leaf"), 1);
        for (int depth = 0;
             depth < SstvWavExporter::MaximumMetadataDepth + 2;
             ++depth) {
            QJsonObject parent;
            parent.insert(QStringLiteral("nested"), nested);
            nested = std::move(parent);
        }
        SstvWavExportRequest deepRequest = requestFor(
            directory.filePath(QStringLiteral("deep.wav")));
        deepRequest.writeMetadataSidecar = true;
        deepRequest.metadata = std::move(nested);
        const SstvWavExportResult deepResult = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            deepRequest);
        QVERIFY(deepResult.ok);
        QVERIFY(!deepResult.warning.isEmpty());
        QVERIFY(deepResult.warning.size()
                <= SstvWavExporter::MaximumErrorCharacters);
        QVERIFY(!deepResult.metadataCommitted);
        QVERIFY(!QFileInfo::exists(
            directory.filePath(QStringLiteral("deep.json"))));

        SstvWavExportRequest boundedRequest = requestFor(
            directory.filePath(QStringLiteral("within-bound.wav")));
        boundedRequest.writeMetadataSidecar = true;
        boundedRequest.metadata.insert(
            QStringLiteral("payload"),
            QString(100'000, QLatin1Char('a')));
        const SstvWavExportResult boundedResult = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            boundedRequest);
        QVERIFY2(boundedResult.ok, qPrintable(boundedResult.error));
        QVERIFY(boundedResult.metadataCommitted);
        QVERIFY(QFileInfo(boundedResult.metadataPath).size()
                <= SstvWavExporter::MaximumMetadataBytes);
    }

    void sidecarCollisionIsPreservedUnlessReplaceIsExplicit()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString wavPath = directory.filePath(QStringLiteral("frame.wav"));
        const QString jsonPath = directory.filePath(QStringLiteral("frame.json"));
        QFile existing(jsonPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write("original-metadata"), qint64 {17});
        existing.close();

        SstvWavExportRequest request = requestFor(wavPath);
        request.writeMetadataSidecar = true;
        auto result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            request);
        QVERIFY(result.ok);
        QVERIFY(!result.metadataCommitted);
        QVERIFY(!result.warning.isEmpty());
        QCOMPARE(readFile(jsonPath), QByteArrayLiteral("original-metadata"));

        request.replaceExisting = true;
        result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            request);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.metadataCommitted);
        QVERIFY(result.warning.isEmpty());
        const QJsonDocument sidecar = QJsonDocument::fromJson(
            readFile(jsonPath));
        QVERIFY(sidecar.isObject());
        QCOMPARE(sidecar.object().value(QStringLiteral("schema")).toString(),
                 QStringLiteral("decodium-sstv-wav-metadata"));
    }

    void validatesFreshSourcePathAndPullBound()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("invalid.wav"));

        SstvWavExportRequest request = requestFor(path);
        request.pullSamples = SstvWavExporter::MaximumPullSamples + 1U;
        auto result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            request);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::InvalidRequest);

        request = requestFor(path);
        auto alreadyUsed = std::make_unique<VectorSource>(
            std::vector<std::int16_t> {1, 2, 3});
        std::int16_t one = 0;
        QCOMPARE(alreadyUsed->pullPcm16(&one, 1U), std::size_t {1U});
        result = SstvWavExporter::exportAtomic(std::move(alreadyUsed), request);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::InvalidRequest);

        request.outputPath = QStringLiteral("relative.wav");
        result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            request);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::InvalidRequest);
        QVERIFY(!QFileInfo::exists(path));

        request = requestFor(path);
        request.mode = QString(
            SstvWavExporter::MaximumModeCharacters + 1,
            QLatin1Char('m'));
        result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            request);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::InvalidRequest);

        request = requestFor(path);
        result = SstvWavExporter::exportAtomic(
            std::make_unique<DeclaredLengthSource>(
                SstvWavStreamWriter::kMaximumPcmSamples + 1U),
            request);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::InvalidRequest);

        auto observation = std::make_shared<SourceObservation>();
        request = requestFor(path);
        result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t>(4'097U, 12),
                48'000U,
                VectorSource::Behaviour::Normal,
                std::function<void()> {},
                observation),
            request);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(observation->maximumRequested, request.pullSamples);
        QVERIFY(observation->pullCalls > 1U);
    }

    void rejectsSymbolicLinkDestination()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString target = directory.filePath(QStringLiteral("target.wav"));
        QFile file(target);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("safe"), qint64 {4});
        file.close();

        const QString link = directory.filePath(QStringLiteral("link.wav"));
        if (!QFile::link(target, link)) {
            QTest::qSkip("symbolic links are unavailable on this test platform",
                         __FILE__,
                         __LINE__);
            return;
        }
        QVERIFY(QFileInfo(link).isSymLink());

        SstvWavExportRequest request = requestFor(link);
        request.replaceExisting = true;
        const SstvWavExportResult result = SstvWavExporter::exportAtomic(
            std::make_unique<VectorSource>(
                std::vector<std::int16_t> {1, 2, 3}),
            request);
        QVERIFY(!result.ok);
        QCOMPARE(result.code, SstvWavExportError::InvalidRequest);
        QCOMPARE(readFile(target), QByteArrayLiteral("safe"));
    }
};

QTEST_GUILESS_MAIN(TestSstvWavExporter)
#include "test_sstv_wav_exporter.moc"
