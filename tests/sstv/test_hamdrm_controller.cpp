// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/digital/HamDrmController.h"
#include "src/sstv/digital/HamDrmBsrCodec.h"
#include "src/sstv/digital/HamDrmHybridCodec.h"
#include "src/sstv/digital/HamDrmRsCodec.h"

#include <QtTest/QtTest>

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace hamdrm = decodium::sstv::hamdrm;

namespace {

QByteArray bytes(const std::vector<std::uint8_t>& input)
{
    return QByteArray(reinterpret_cast<const char*>(input.data()),
                      static_cast<qsizetype>(input.size()));
}

std::vector<std::uint8_t> jpegFixture(std::uint16_t width,
                                      std::uint16_t height,
                                      std::size_t size = 160U)
{
    std::vector<std::uint8_t> result {
        0xffU, 0xd8U,
        0xffU, 0xc0U, 0x00U, 0x11U, 0x08U,
        static_cast<std::uint8_t>(height >> 8U),
        static_cast<std::uint8_t>(height),
        static_cast<std::uint8_t>(width >> 8U),
        static_cast<std::uint8_t>(width),
        0x03U,
        0x01U, 0x11U, 0x00U,
        0x02U, 0x11U, 0x00U,
        0x03U, 0x11U, 0x00U,
        0xffU, 0xd9U,
    };
    result.resize(std::max(result.size(), size), 0U);
    return result;
}

QUrl writeFixture(const QString& path,
                  const std::vector<std::uint8_t>& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(bytes(content)) != static_cast<qint64>(content.size())
        || !file.flush()) {
        return {};
    }
    file.close();
    return QUrl::fromLocalFile(path);
}

class FakeRxBackend final : public hamdrm::HamDrmWaveformRxBackend
{
public:
    hamdrm::HamDrmWaveformCapability advertised {
        true,
        QStringLiteral("deterministic RX fixture"),
        QStringLiteral("complete test waveform boundary"),
    };
    hamdrm::HamDrmStatus startResult = hamdrm::HamDrmStatus::success();
    hamdrm::HamDrmWaveformRxSink* sink {nullptr};
    hamdrm::HamDrmWaveformRxSink* lastSink {nullptr};
    std::uint64_t session {0U};
    QString profileId;
    int starts {0};
    int cancels {0};
    bool timing {false};
    bool supportsExperimentalTiming() const noexcept override { return true; }
    bool setExperimentalTiming(bool enabled) noexcept override { timing = enabled; return true; }

    hamdrm::HamDrmWaveformCapability capability() const override
    {
        return advertised;
    }

    hamdrm::HamDrmStatus start(const hamdrm::HamDrmProfile& profile,
                               std::uint64_t sessionId,
                               hamdrm::HamDrmWaveformRxSink& target) override
    {
        ++starts;
        profileId = QString::fromStdString(profile.id);
        session = sessionId;
        sink = &target;
        lastSink = &target;
        return startResult;
    }

    void cancel(std::uint64_t sessionId) noexcept override
    {
        if (sessionId == session) {
            ++cancels;
            sink = nullptr;
        }
    }

    void progress(double value)
    {
        QVERIFY(sink != nullptr);
        sink->hamDrmRxProgress(session, value);
    }

    void group(const std::vector<std::uint8_t>& encoded)
    {
        QVERIFY(sink != nullptr);
        sink->hamDrmRxMotGroup(session, bytes(encoded));
    }

    void finish(hamdrm::HamDrmStatus status = hamdrm::HamDrmStatus::success())
    {
        QVERIFY(sink != nullptr);
        auto* target = sink;
        sink = nullptr;
        target->hamDrmRxFinished(session, std::move(status));
    }
};

class FakeTxBackend final : public hamdrm::HamDrmWaveformTxBackend
{
public:
    hamdrm::HamDrmWaveformCapability advertised {
        true,
        QStringLiteral("deterministic TX fixture"),
        QStringLiteral("complete test waveform boundary"),
    };
    hamdrm::HamDrmStatus startResult = hamdrm::HamDrmStatus::success();
    hamdrm::HamDrmWaveformTxSink* sink {nullptr};
    std::uint64_t session {0U};
    QString profileId;
    std::optional<hamdrm::HamDrmEncodedObject> object;
    int starts {0};
    int cancels {0};

    hamdrm::HamDrmWaveformCapability capability() const override
    {
        return advertised;
    }

    hamdrm::HamDrmStatus start(const hamdrm::HamDrmProfile& profile,
                               hamdrm::HamDrmEncodedObject encoded,
                               std::uint64_t sessionId,
                               hamdrm::HamDrmWaveformTxSink& target) override
    {
        ++starts;
        profileId = QString::fromStdString(profile.id);
        object = std::move(encoded);
        session = sessionId;
        sink = &target;
        return startResult;
    }

    void cancel(std::uint64_t sessionId) noexcept override
    {
        if (sessionId == session) {
            ++cancels;
            sink = nullptr;
        }
    }

    void progress(double value)
    {
        QVERIFY(sink != nullptr);
        sink->hamDrmTxProgress(session, value);
    }

    void finish(hamdrm::HamDrmStatus status = hamdrm::HamDrmStatus::success())
    {
        QVERIFY(sink != nullptr);
        auto* target = sink;
        sink = nullptr;
        target->hamDrmTxFinished(session, std::move(status));
    }
};

class CountingJpeg2000Backend final : public hamdrm::HamDrmJpeg2000Backend
{
public:
    explicit CountingJpeg2000Backend(
        std::shared_ptr<hamdrm::HamDrmJpeg2000Backend> delegate)
        : delegate_(std::move(delegate))
    {
    }

    hamdrm::HamDrmJpeg2000Capability capability() const override
    {
        return delegate_->capability();
    }

    hamdrm::HamDrmValueResult<hamdrm::HamDrmRgbaImage> decode(
        const std::uint8_t* data,
        std::size_t size,
        const hamdrm::HamDrmLimits& limits) const override
    {
        ++decodeCalls;
        return delegate_->decode(data, size, limits);
    }

    hamdrm::HamDrmValueResult<std::vector<std::uint8_t>> encodeLossless(
        const hamdrm::HamDrmRgbaImage& image,
        const hamdrm::HamDrmLimits& limits) const override
    {
        return delegate_->encodeLossless(image, limits);
    }

    mutable int decodeCalls {0};

private:
    std::shared_ptr<hamdrm::HamDrmJpeg2000Backend> delegate_;
};

hamdrm::HamDrmControllerBackends backends(
    const std::shared_ptr<FakeRxBackend>& rx,
    const std::shared_ptr<FakeTxBackend>& tx,
    std::shared_ptr<hamdrm::HamDrmJpeg2000Backend> jpeg = {})
{
    return {rx, tx, std::move(jpeg)};
}

} // namespace

class TestHamDrmController final : public QObject
{
    Q_OBJECT

private slots:
    void partialProfileSurvivesProcessRestart()
    {
        const QString phase = qEnvironmentVariable("DECODIUM_TEST_PARTIAL_PHASE");
        if (phase.isEmpty()) {
            QTemporaryDir root;
            QVERIFY(root.isValid());
            for (const QString& variant : {QStringLiteral("a"), QStringLiteral("b"),
                                           QStringLiteral("legacy-a"), QStringLiteral("legacy-b")}) {
                const QString directory = root.filePath(variant);
                QVERIFY(QDir().mkpath(directory));
                for (const QString& action : {QStringLiteral("seed"), QStringLiteral("resume")}) {
                    QProcess child;
                    auto environment = QProcessEnvironment::systemEnvironment();
                    environment.insert(QStringLiteral("DECODIUM_TEST_PARTIAL_PHASE"), action);
                    environment.insert(QStringLiteral("DECODIUM_TEST_PARTIAL_VARIANT"), variant);
                    environment.insert(QStringLiteral("DECODIUM_TEST_PARTIAL_ROOT"), directory);
                    child.setProcessEnvironment(environment);
                    child.start(QCoreApplication::applicationFilePath(),
                                {QStringLiteral("partialProfileSurvivesProcessRestart")});
                    QVERIFY(child.waitForFinished(15000));
                    const QByteArray output = child.readAllStandardOutput() + child.readAllStandardError();
                    QVERIFY2(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0,
                             output.constData());
                }
            }
            return;
        }
        QVERIFY(phase == QStringLiteral("seed") || phase == QStringLiteral("resume"));
        const QString variant = qEnvironmentVariable("DECODIUM_TEST_PARTIAL_VARIANT");
        QVERIFY(variant == QStringLiteral("a") || variant == QStringLiteral("b")
                || variant == QStringLiteral("legacy-a") || variant == QStringLiteral("legacy-b"));
        const QString root = qEnvironmentVariable("DECODIUM_TEST_PARTIAL_ROOT");
        QVERIFY(QDir::isAbsolutePath(root));
        hamdrm::HamDrmControllerConfig config;
        config.partialStoreRoot = root;
        hamdrm::HamDrmController controller(config, {});
        const auto& profiles = hamdrm::HamDrmProfileRegistry::all();
        QVERIFY(profiles.size() >= 2U);
        const bool legacy = variant.startsWith(QStringLiteral("legacy-"));
        const std::size_t source = variant.endsWith(QLatin1Char('a')) ? 0U : 1U;
        const QString sourceId = QString::fromStdString(profiles[source].id);
        const QString otherId = QString::fromStdString(profiles[1U - source].id);
        constexpr std::uint16_t transportId = 0x4271U;
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.transportId = transportId;
        metadata.filename = "restart.jpg";
        const auto image = jpegFixture(48U, 32U, source == 0U ? 192U : 224U);
        const auto encoded = hamdrm::encodeHamDrmObject(metadata, image, 24U);
        QVERIFY(encoded.ok());
        QVERIFY(encoded.value->bodyGroups.size() > 2U);
        if (phase == QStringLiteral("seed")) {
            controller.setSelectedProfileId(sourceId);
            for (const auto& group : encoded.value->headerGroups)
                QVERIFY(controller.ingestMotGroup(bytes(group)).ok());
            QVERIFY(controller.ingestMotGroup(bytes(encoded.value->bodyGroups.front())).ok());
            QVERIFY(controller.canBuildBsr());
            QCOMPARE(controller.savedPartialIds(), QVariantList {QStringLiteral("4271")});
            if (legacy) {
                const hamdrm::HamDrmPartialStore store(root);
                const auto snapshot = store.load(transportId);
                QVERIFY(snapshot.ok());
                QVERIFY(store.save(*snapshot.value).ok());
            }
        } else {
            controller.setSelectedProfileId(otherId);
            QCOMPARE(controller.savedPartialIds(), QVariantList {QStringLiteral("4271")});
            QVERIFY(controller.resumePartial(transportId));
            const QByteArray bsr = controller.buildBsr(transportId, true).toLatin1();
            const auto request = hamdrm::parseHamDrmBsr(
                reinterpret_cast<const std::uint8_t*>(bsr.constData()), std::size_t(bsr.size()));
            QVERIFY(request.ok());
            QVERIFY(request.value->qsstvCompatibilityCode.has_value());
            QCOMPARE(*request.value->qsstvCompatibilityCode,
                     profiles[legacy ? 1U - source : source].qsstvCompatibilityCode);
            for (std::size_t i = 1U; i < encoded.value->bodyGroups.size(); ++i)
                QVERIFY(controller.ingestMotGroup(bytes(encoded.value->bodyGroups[i])).ok());
            const auto complete = controller.completedObject(transportId);
            QVERIFY(complete.ok());
            QCOMPARE(complete.value->originalBytes, image);
            QVERIFY(controller.savedPartialIds().isEmpty());
        }
    }
    void experimentalTimingRequiresIdleBackend()
    {
        auto rx = std::make_shared<FakeRxBackend>();
        hamdrm::HamDrmController controller({}, backends(rx, {}));
        QVERIFY(controller.experimentalTimingAvailable());
        QVERIFY(!controller.experimentalTiming());
        controller.setExperimentalTiming(true);
        QVERIFY(controller.experimentalTiming());
        QVERIFY(rx->timing);
        QVERIFY(controller.startRx());
        controller.setExperimentalTiming(false);
        QVERIFY(controller.experimentalTiming());
        QVERIFY(rx->timing);
        QVERIFY(controller.cancelRx());
        controller.setExperimentalTiming(false);
        QVERIFY(!controller.experimentalTiming());
        QVERIFY(!rx->timing);
    }
    void capabilitiesAndProfileModelAreTypedAndHonest()
    {
        hamdrm::HamDrmControllerConfig config;
        hamdrm::HamDrmController controller(config, {});

        QCOMPARE(controller.profiles().size(), 72);
        QSet<QString> profileIds;
        for (const QVariant& value : controller.profiles()) {
            const QVariantMap profile = value.toMap();
            const QString id = profile.value(QStringLiteral("id")).toString();
            QVERIFY(!id.isEmpty());
            QVERIFY(!profileIds.contains(id));
            profileIds.insert(id);
            QVERIFY(!profile.value(QStringLiteral("displayName")).toString().isEmpty());
            QVERIFY(!profile.value(QStringLiteral("robustness")).toString().isEmpty());
            QVERIFY(!profile.value(QStringLiteral("bandwidth")).toString().isEmpty());
            QVERIFY(profile.value(QStringLiteral("bandwidthHz")).toULongLong() > 0U);
            QVERIFY(!profile.value(QStringLiteral("protection")).toString().isEmpty());
            QVERIFY(!profile.value(QStringLiteral("constellation")).toString().isEmpty());
            QVERIFY(!profile.value(QStringLiteral("interleaver")).toString().isEmpty());
            QVERIFY(profile.value(
                QStringLiteral("payloadBytesPer400msFrame")).toULongLong() > 0U);
            QVERIFY(profile.value(
                QStringLiteral("expectedPayloadBitrate")).toULongLong() > 0U);
            QVERIFY(!profile.contains(QStringLiteral("index")));
            QVERIFY(!profile.contains(QStringLiteral("compatibilityCode")));
            QVERIFY(!profile.contains(QStringLiteral("qsstvCompatibilityCode")));
        }

        QVERIFY(!controller.waveformRxAvailable());
        QVERIFY(!controller.waveformTxAvailable());
        QVERIFY(!controller.jpeg2000DecodeAvailable());
        QVERIFY(!controller.jpeg2000EncodeAvailable());
        QVERIFY(!controller.partialResumeAvailable());
        QCOMPARE(controller.rxState(),
                 hamdrm::HamDrmController::OperationState::Unavailable);
        QCOMPARE(controller.txState(),
                 hamdrm::HamDrmController::OperationState::Unavailable);
        QVERIFY(controller.capabilityMessage().contains(
            QStringLiteral("72 named profiles"), Qt::CaseInsensitive));
        QVERIFY(controller.capabilityMessage().contains(
            QStringLiteral("not connected"), Qt::CaseInsensitive));

        QSignalSpy rejected(&controller,
                            &hamdrm::HamDrmController::operationRejected);
        QVERIFY(!controller.startRx());
        QCOMPARE(rejected.size(), 1);
        QVERIFY(controller.error().contains(
            QStringLiteral("not connected"), Qt::CaseInsensitive));

        hamdrm::HamDrmController productionDefault;
        QVERIFY(productionDefault.jpeg2000DecodeAvailable());
        QVERIFY(productionDefault.jpeg2000EncodeAvailable());
        QVERIFY(productionDefault.capabilities()
                    .value(QStringLiteral("jpeg2000BackendName"))
                    .toString()
                    .contains(QStringLiteral("OpenJPEG")));
    }

    void rxProgressFailsClosedAndCanBeCancelled()
    {
        auto rx = std::make_shared<FakeRxBackend>();
        hamdrm::HamDrmController controller(
            {}, backends(rx, {}));
        QVERIFY(controller.startRx());
        QCOMPARE(controller.rxState(),
                 hamdrm::HamDrmController::OperationState::Active);
        rx->progress(0.5);
        QCOMPARE(controller.rxProgress(), 0.5);
        rx->progress(0.4);
        QCOMPARE(rx->cancels, 1);
        QCOMPARE(controller.rxState(),
                 hamdrm::HamDrmController::OperationState::Error);
        QVERIFY(controller.error().contains(
            QStringLiteral("invalid progress"), Qt::CaseInsensitive));

        QVERIFY(controller.startRx());
        QVERIFY(controller.cancelRx());
        QCOMPARE(rx->cancels, 2);
        QCOMPARE(controller.rxState(),
                 hamdrm::HamDrmController::OperationState::Cancelled);
        QVERIFY(!controller.busy());
    }

    void liveRepairLossInjectionIsTestModeOnly()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString marker = temporary.filePath(QStringLiteral("sender-ready"));
        struct Restore final {
            bool testMode = QStandardPaths::isTestModeEnabled();
            bool hadValue = qEnvironmentVariableIsSet("DECODIUM_LAB_DROP_MOT_UNTIL");
            QByteArray value = qgetenv("DECODIUM_LAB_DROP_MOT_UNTIL");
            bool hadFinal = qEnvironmentVariableIsSet("DECODIUM_LAB_DROP_MOT_FINAL");
            QByteArray final = qgetenv("DECODIUM_LAB_DROP_MOT_FINAL");
            ~Restore() {
                QStandardPaths::setTestModeEnabled(testMode);
                if (hadValue) qputenv("DECODIUM_LAB_DROP_MOT_UNTIL", value);
                else qunsetenv("DECODIUM_LAB_DROP_MOT_UNTIL");
                if (hadFinal) qputenv("DECODIUM_LAB_DROP_MOT_FINAL", final);
                else qunsetenv("DECODIUM_LAB_DROP_MOT_FINAL");
            }
        } restore;
        QVERIFY(qputenv("DECODIUM_LAB_DROP_MOT_UNTIL", marker.toUtf8()));
        for (const bool testMode : {false, true}) {
            QStandardPaths::setTestModeEnabled(testMode);
            for (const unsigned variant : {0U, 1U}) {
                QVERIFY(qputenv("DECODIUM_LAB_DROP_MOT_FINAL", variant ? "1" : "0"));
                hamdrm::HamDrmMotObjectMetadata metadata;
                metadata.transportId = static_cast<std::uint16_t>(0x4770U + variant);
                metadata.filename = variant ? "live-b.jpg" : "live-a.jpg";
                const auto original = jpegFixture(48U, 32U, 192U + variant);
                const auto object = hamdrm::encodeHamDrmObject(metadata, original, 64U);
                QVERIFY(object.ok());
                hamdrm::HamDrmController receiver({}, backends({}, {}));
                for (const auto& group : object.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
                for (const auto& group : object.value->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
                if (testMode) {
                    const auto expectedIndex = variant ? object.value->bodyGroups.size() - 1U : 1U;
                    QCOMPARE(receiver.missingSegments().size(), 1);
                    QCOMPARE(receiver.missingSegments().front().toInt(), static_cast<int>(expectedIndex));
                    QFile done(marker);
                    QVERIFY(done.open(QIODevice::WriteOnly));
                    done.close();
                    QVERIFY(receiver.ingestMotGroup(bytes(object.value->bodyGroups[expectedIndex])).ok());
                    QVERIFY(done.remove());
                }
                const auto received = receiver.takeCompletedObject(metadata.transportId);
                QVERIFY(received.ok());
                QCOMPARE(received.value->originalBytes, original);
            }
        }
    }

    void hybridReferencesAreNotImagesAB()
    {
        for (unsigned variant : {0U, 1U}) {
            const hamdrm::HamDrmHybridServer server {
                variant ? "b.example.invalid" : "a.example.invalid", "operator",
                variant ? "dummy-b" : "dummy-a", "radio"
            };
            const auto body = hamdrm::encodeHamDrmHybridServer(server);
            QVERIFY(body.ok());
            hamdrm::HamDrmMotObjectMetadata metadata;
            metadata.transportId = static_cast<std::uint16_t>(0x6100U + variant);
            metadata.filename = variant ? "de_TEST2-1-b.jpg" : "de_TEST1-1-a.jpg";
            const auto object = hamdrm::encodeHamDrmObject(metadata, *body.value, 64U);
            QVERIFY(object.ok());
            hamdrm::HamDrmController receiver({}, backends({}, {}));
            QSignalSpy hybrid(&receiver, &hamdrm::HamDrmController::hybridReferenceReceived);
            QSignalSpy image(&receiver, &hamdrm::HamDrmController::objectCompleted);
            QSignalSpy gallery(&receiver, &hamdrm::HamDrmController::receivedImageReady);
            for (const auto& group : object.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
            for (const auto& group : object.value->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
            QCOMPARE(hybrid.count(), 1);
            QCOMPARE(image.count(), 0);
            QCOMPARE(gallery.count(), 0);
            const auto rows = receiver.inbox();
            QCOMPARE(rows.size(), 1);
            const QVariantMap row = rows.front().toMap();
            QCOMPARE(row.value("state").toString(), QStringLiteral("hybrid-reference"));
            QVERIFY(row.value("hybridReference").toBool());
            QVERIFY(!row.value("imageStructurallyValidated").toBool());
            const QByteArray json = QJsonDocument::fromVariant(row).toJson();
            QVERIFY(!json.contains(QByteArray::fromStdString(server.password)));
            QCOMPARE(row.value("hybridHost").toString(), QString::fromStdString(server.host));
            const auto completed = receiver.completedObject(metadata.transportId);
            QVERIFY(completed.ok());
            QCOMPARE(completed.value->originalBytes, *body.value);
            QTemporaryDir saveDir;
            QVERIFY(saveDir.isValid());
            QVERIFY(!receiver.saveCompletedImage(metadata.transportId,
                                                   QUrl::fromLocalFile(saveDir.filePath("not-an-image.jpg"))));
            QVERIFY(!QFile::exists(saveDir.filePath("not-an-image.jpg")));

            // A normal image with a de_ prefix must still be received as an image.
            metadata.transportId += 0x100;
            const auto ordinary = hamdrm::encodeHamDrmObject(metadata, jpegFixture(16, 16), 64U);
            QVERIFY(ordinary.ok());
            hamdrm::HamDrmController ordinaryReceiver({}, backends({}, {}));
            QSignalSpy ordinaryImage(&ordinaryReceiver, &hamdrm::HamDrmController::objectCompleted);
            QSignalSpy ordinaryHybrid(&ordinaryReceiver, &hamdrm::HamDrmController::hybridReferenceReceived);
            for (const auto& group : ordinary.value->headerGroups) QVERIFY(ordinaryReceiver.ingestMotGroup(bytes(group)).ok());
            for (const auto& group : ordinary.value->bodyGroups) QVERIFY(ordinaryReceiver.ingestMotGroup(bytes(group)).ok());
            QCOMPARE(ordinaryImage.count(), 1);
            QCOMPARE(ordinaryHybrid.count(), 0);
            QVERIFY(!ordinaryReceiver.inbox().front().toMap().value("hybridReference").toBool());

            // A truncated hybrid body must not be promoted to an image or reference.
            auto bad = *body.value; bad.pop_back();
            metadata.transportId += 0x100;
            const auto invalid = hamdrm::encodeHamDrmObject(metadata, bad, 64U);
            QVERIFY(invalid.ok());
            hamdrm::HamDrmController rejected({}, backends({}, {}));
            for (const auto& group : invalid.value->headerGroups) QVERIFY(rejected.ingestMotGroup(bytes(group)).ok());
            bool failed = false;
            for (const auto& group : invalid.value->bodyGroups)
                if (!rejected.ingestMotGroup(bytes(group)).ok()) failed = true;
            QVERIFY(failed);
            QVERIFY(!rejected.completedObject(metadata.transportId).ok());
        }
    }

    void legacyDotHybridReferencesAB()
    {
        for (unsigned variant : {0U, 1U}) {
            const auto body = hamdrm::encodeHamDrmHybridServer({"server.invalid", "operator", "dummy", "radio"});
            QVERIFY(body.ok());
            hamdrm::HamDrmMotObjectMetadata metadata;
            metadata.transportId = static_cast<std::uint16_t>(0x6300U + variant);
            metadata.filename = variant ? ".de_TEST2-1-b.jpg" : ".de_TEST1-1-a.jpg";
            const auto object = hamdrm::encodeHamDrmObject(metadata, *body.value, 64U);
            QVERIFY(object.ok());
            hamdrm::HamDrmController receiver({}, backends({}, {}));
            QSignalSpy hybrid(&receiver, &hamdrm::HamDrmController::hybridReferenceReceived);
            QSignalSpy image(&receiver, &hamdrm::HamDrmController::objectCompleted);
            for (const auto& group : object.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
            for (const auto& group : object.value->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
            QCOMPARE(hybrid.count(), 1);
            QCOMPARE(image.count(), 0);
            QVERIFY(receiver.inbox().front().toMap().value("hybridReference").toBool());
        }
        QVERIFY(!hamdrm::validateHamDrmFilename(".de/../image.jpg").ok());
        QVERIFY(!hamdrm::validateHamDrmFilename(".de\\image.jpg").ok());
        QVERIFY(!hamdrm::validateHamDrmFilename("..hidden.jpg").ok());
        QVERIFY(!hamdrm::validateHamDrmFilename(".hidden.jpg").ok());
        QVERIFY(!hamdrm::validateHamDrmFilename(".de").ok());
    }

    void structuralOnlyImageIsNotArchivedAB()
    {
        for (unsigned variant : {0U, 1U}) {
            const auto original = jpegFixture(16 + variant, 16 + variant);
            QVERIFY(QImage::fromData(bytes(original)).isNull());
            hamdrm::HamDrmMotObjectMetadata metadata;
            metadata.filename = variant ? "damaged-b.jpg" : "damaged-a.jpg";
            metadata.transportId = static_cast<std::uint16_t>(0x6400U + variant);
            const auto object = hamdrm::encodeHamDrmObject(metadata, original, 64U);
            QVERIFY(object.ok());
            hamdrm::HamDrmController receiver({}, backends({}, {}));
            QSignalSpy gallery(&receiver, &hamdrm::HamDrmController::receivedImageReady);
            for (const auto& group : object.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
            for (const auto& group : object.value->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
            QCOMPARE(gallery.count(), 0);
            QCOMPARE(receiver.inbox().front().toMap().value("state").toString(), QStringLiteral("complete-structural"));
            QVERIFY(receiver.selectedImageStructuralOnly());
            QVERIFY(receiver.canSaveSelectedImage());
            QTemporaryDir temp;
            QVERIFY(temp.isValid());
            const QString path = temp.filePath("structural.jpg");
            QVERIFY(receiver.saveCompletedImage(metadata.transportId, QUrl::fromLocalFile(path)));
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), bytes(original));
        }
    }

    void qsstvPngMislabelledAsJp2AB()
    {
        for (const unsigned variant : {0U, 1U}) {
            QImage image(24 + variant, 16 + variant, QImage::Format_RGB32);
            image.fill(variant ? Qt::blue : Qt::red);
            QByteArray payload;
            QBuffer output(&payload);
            QVERIFY(output.open(QIODevice::WriteOnly));
            QVERIFY(image.save(&output, "PNG"));
            const std::vector<std::uint8_t> original(payload.begin(), payload.end());
            hamdrm::HamDrmMotObjectMetadata metadata;
            metadata.transportId = static_cast<std::uint16_t>(0x5160U + variant);
            metadata.filename = variant ? "small-b.jp2" : "small-a.jp2";
            const auto object = hamdrm::encodeHamDrmObject(metadata, original, 64U);
            QVERIFY(object.ok());
            hamdrm::HamDrmController receiver({}, backends({}, {}));
            QSignalSpy gallery(&receiver, &hamdrm::HamDrmController::receivedImageReady);
            for (const auto& group : object.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
            for (const auto& group : object.value->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
            QCOMPARE(gallery.count(), 1);
            const QImage snapshot = qvariant_cast<QImage>(gallery[0][0]);
            QCOMPARE(snapshot.size(), image.size());
            QCOMPARE(snapshot.pixelColor(0, 0), image.pixelColor(0, 0));
            QVERIFY(!gallery[0][2].toBool());
            for (const auto& group : object.value->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
            QCOMPARE(gallery.count(), 1);
            QTemporaryDir savedDir;
            QVERIFY(savedDir.isValid());
            const QString savedPath = savedDir.filePath(variant ? "saved-b.png" : "saved-a.png");
            QVERIFY(receiver.saveCompletedImage(metadata.transportId, QUrl::fromLocalFile(savedPath)));
            QFile saved(savedPath);
            QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(saved.readAll(), payload);
            QVERIFY(!receiver.saveCompletedImage(metadata.transportId, QUrl(QStringLiteral("https://example.invalid/file.png"))));
            const auto received = receiver.takeCompletedObject(metadata.transportId);
            QVERIFY2(received.ok(), received.status.detail.c_str());
            QCOMPARE(received.value->originalBytes, original);
            QCOMPARE(received.value->metadata.mimeType, std::string("image/png"));
            QCOMPARE(received.value->metadata.filename, std::string(variant ? "small-b.png" : "small-a.png"));
            auto bad = original;
            bad[12] = 'X'; // Exact signature but invalid IHDR remains rejected.
            metadata.transportId += 0x100U;
            const auto invalid = hamdrm::encodeHamDrmObject(metadata, bad, 64U);
            QVERIFY(invalid.ok());
            hamdrm::HamDrmController reject({}, backends({}, {}));
            for (const auto& group : invalid.value->headerGroups) QVERIFY(reject.ingestMotGroup(bytes(group)).ok());
            bool failed = false;
            for (const auto& group : invalid.value->bodyGroups)
                if (!reject.ingestMotGroup(bytes(group)).ok()) failed = true;
            QVERIFY(failed);
            QVERIFY(!reject.completedObject(metadata.transportId).ok());
        }
    }

    void partialMotObjectPersistsResumesBuildsBsrAndCompletes_data()
    {
        QTest::addColumn<bool>("alternateSourceProfile");
        QTest::newRow("a-default") << false;
        QTest::newRow("b-alternate") << true;
    }

    void partialMotObjectPersistsResumesBuildsBsrAndCompletes()
    {
        QFETCH(bool, alternateSourceProfile);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        constexpr std::uint16_t transportId = 0x4265U;
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.transportId = transportId;
        metadata.filename = "resume.jpg";
        QImage sourceImage(48, 32, QImage::Format_RGB32);
        sourceImage.fill(alternateSourceProfile ? QColor(20, 130, 240) : QColor(220, 90, 30));
        QByteArray imageBytes;
        QBuffer buffer(&imageBytes);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(sourceImage.save(&buffer, "JPG"));
        const std::vector<std::uint8_t> image(imageBytes.cbegin(), imageBytes.cend());
        const auto encoded = hamdrm::encodeHamDrmObject(
            metadata, image, 24U);
        QVERIFY2(encoded.ok(), encoded.status.detail.c_str());
        QVERIFY(encoded.value->bodyGroups.size() >= 4U);

        hamdrm::HamDrmControllerConfig config;
        config.partialStoreRoot = temporary.path();
        auto rx = std::make_shared<FakeRxBackend>();
        QString originalProfile;
        QString alternateProfile;
        QString expectedBsr;
        {
            hamdrm::HamDrmController controller(
                config, backends(rx, {}));
            originalProfile = controller.selectedProfileId();
            for (const QVariant& item : controller.profiles()) {
                const QString id = item.toMap().value(QStringLiteral("id")).toString();
                if (id != originalProfile) { alternateProfile = id; break; }
            }
            QVERIFY(!alternateProfile.isEmpty());
            if (alternateSourceProfile) {
                controller.setSelectedProfileId(alternateProfile);
                std::swap(originalProfile, alternateProfile);
            }
            QVERIFY(controller.partialResumeAvailable());
            QVERIFY(controller.startRx());
            rx->group(encoded.value->headerGroups.front());
            rx->group(encoded.value->bodyGroups.back());
            rx->group(encoded.value->bodyGroups.front());

            QCOMPARE(controller.selectedTransportId(),
                     static_cast<int>(transportId));
            QVERIFY(controller.canBuildBsr());
            QCOMPARE(controller.missingSegments().size(),
                     static_cast<qsizetype>(
                         encoded.value->bodyGroups.size() - 2U));
            const QString bsr = controller.buildBsr(transportId, true);
            expectedBsr = bsr;
            QVERIFY(!bsr.isEmpty());
            QVERIFY(bsr.contains(QStringLiteral("resume.jpg")));
            QVERIFY(controller.inbox().front().toMap()
                        .value(QStringLiteral("persisted")).toBool());
        }
        QCOMPARE(rx->cancels, 1);

        const hamdrm::HamDrmPartialStore store(temporary.path());
        QVERIFY(QFile::exists(store.pathForTransportId(transportId)));

        auto resumedRx = std::make_shared<FakeRxBackend>();
        hamdrm::HamDrmController resumed(
            config, backends(resumedRx, {}));
        resumed.setSelectedProfileId(alternateProfile);
        QCOMPARE(resumed.selectedProfileId(), alternateProfile);
        QVERIFY(resumed.resumePartial(transportId));
        QCOMPARE(resumed.missingSegments().size(),
                 static_cast<qsizetype>(
                     encoded.value->bodyGroups.size() - 2U));
        QVERIFY(resumed.canBuildBsr());
        QCOMPARE(resumed.buildBsr(transportId, true), expectedBsr);
        QVERIFY(resumed.startRx());

        QSignalSpy completed(&resumed,
                             &hamdrm::HamDrmController::objectCompleted);
        QSignalSpy gallery(&resumed,
                           &hamdrm::HamDrmController::receivedImageReady);
        for (std::size_t index = 1U;
             index + 1U < encoded.value->bodyGroups.size(); ++index) {
            resumedRx->group(encoded.value->bodyGroups[index]);
        }
        QCOMPARE(completed.size(), 1);
        QCOMPARE(gallery.size(), 1);
        QCOMPARE(gallery.front().at(1).toString(), originalProfile);
        QCOMPARE(completed.front().at(0).toUInt(),
                 static_cast<uint>(transportId));
        QVERIFY(!QFile::exists(store.pathForTransportId(transportId)));
        QCOMPARE(resumed.inbox().size(), 1);
        QCOMPARE(resumed.inbox().front().toMap()
                     .value(QStringLiteral("state")).toString(),
                 QStringLiteral("complete"));

        const auto assembled = resumed.takeCompletedObject(transportId);
        QVERIFY2(assembled.ok(), assembled.status.detail.c_str());
        QCOMPARE(assembled.value->originalBytes, image);
        QVERIFY(resumed.inbox().isEmpty());
    }

    void sendsRsContainers()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        for (unsigned variant = 0; variant < 2; ++variant) {
            QImage image(48, 32, QImage::Format_RGB32);
            image.fill(variant ? QColor(20, 140, 80) : QColor(150, 30, 90));
            const QString path = temporary.filePath(QStringLiteral("image-%1.png").arg(variant));
            QVERIFY(image.save(path, "PNG"));
            QFile source(path);
            QVERIFY(source.open(QIODevice::ReadOnly));
            const auto original = source.readAll();
            for (int level = 1; level <= 4; ++level) {
                auto tx = std::make_shared<FakeTxBackend>();
                hamdrm::HamDrmController sender({}, backends({}, tx));
                QVERIFY2(sender.startTxWithRs(QUrl::fromLocalFile(path), level), qPrintable(sender.error()));
                QVERIFY(tx->object.has_value());
                QVERIFY(tx->object->bodyGroups.size() >= 2U);
                QCOMPARE(QString::fromStdString(tx->object->metadata.filename),
                         QStringLiteral("image-%1.rs%2").arg(variant).arg(level));
                hamdrm::HamDrmController receiver({}, backends({}, {}));
                for (const auto& group : tx->object->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
                for (const auto& group : tx->object->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
                const auto received = receiver.takeCompletedObject(tx->object->metadata.transportId);
                QVERIFY(received.ok());
                QCOMPARE(bytes(received.value->originalBytes), original);
            }
        }
    }

    void qsstvCompatibleRsIsExplicitAndPreservesPixels()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        for (int level = 1; level <= 4; ++level) {
            for (int header : {127, 128, 256 - 32 * level}) {
                for (unsigned variant = 0; variant < 2; ++variant) {
                    QImage image(48, 32, QImage::Format_RGB32);
                    image.fill(variant ? QColor(20, 140, 80) : QColor(150, 30, 90));
                    const QString path = temporary.filePath(QStringLiteral("compat-%1.jpg").arg(variant));
                    QVERIFY(image.save(path, "JPEG"));
                    QFile file(path);
                    QVERIFY(file.open(QIODevice::ReadWrite));
                    QByteArray original = file.readAll();
                    const qsizetype k = 256 - 32 * level;
                    const qsizetype adjustment = (k - header - original.size() % k + k) % k;
                    original.append(QByteArray(adjustment, '\0'));
                    QVERIFY(file.resize(0));
                    QVERIFY(file.seek(0));
                    QCOMPARE(file.write(original), original.size());
                    file.close();
                    for (const bool compatible : {false, true}) {
                        auto tx = std::make_shared<FakeTxBackend>();
                        hamdrm::HamDrmController sender({}, backends({}, tx));
                        const auto url = QUrl::fromLocalFile(path);
                        QVERIFY(compatible ? sender.startTxWithRsQsstvCompatibility(url, level)
                                           : sender.startTxWithRs(url, level));
                        QVERIFY(tx->object.has_value());
                        hamdrm::HamDrmController receiver({}, backends({}, {}));
                        for (const auto& group : tx->object->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
                        for (const auto& group : tx->object->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
                        const auto received = receiver.takeCompletedObject(tx->object->metadata.transportId);
                        QVERIFY(received.ok());
                        const QByteArray actual = bytes(received.value->originalBytes);
                        QCOMPARE(actual, original + (compatible && header >= 128 ? QByteArray(header - 127, '\0') : QByteArray()));
                        QCOMPARE(QImage::fromData(actual), QImage::fromData(original));
                    }
                }
            }
        }
        QImage png(16, 16, QImage::Format_RGB32);
        png.fill(Qt::red);
        const QString pngPath = temporary.filePath(QStringLiteral("unsupported.png"));
        QVERIFY(png.save(pngPath, "PNG"));
        auto tx = std::make_shared<FakeTxBackend>();
        hamdrm::HamDrmController sender({}, backends({}, tx));
        QVERIFY(!sender.startTxWithRsQsstvCompatibility(QUrl::fromLocalFile(pngPath), 2));
        QVERIFY(!tx->object.has_value());
    }

    void receivesCompleteRsContainers()
    {
        for (unsigned level = 1; level <= 4; ++level) {
            for (unsigned variant = 0; variant < 2; ++variant) {
                const auto original = jpegFixture(32U + variant, 24U, 400U + variant * 137U);
                const auto protectedBytes = hamdrm::encodeHamDrmRs(original, "jpg", level);
                QVERIFY2(protectedBytes.ok(), protectedBytes.status.detail.c_str());
                hamdrm::HamDrmMotObjectMetadata metadata;
                metadata.transportId = static_cast<std::uint16_t>(0x4300U + level * 2U + variant);
                metadata.filename = "protected.rs" + std::to_string(level);
                const auto object = hamdrm::encodeHamDrmObject(metadata, *protectedBytes.value, 128U);
                QVERIFY2(object.ok(), object.status.detail.c_str());
                hamdrm::HamDrmController receiver({}, backends({}, {}));
                QSignalSpy completed(&receiver, &hamdrm::HamDrmController::objectCompleted);
                for (const auto& group : object.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
                for (const auto& group : object.value->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
                QCOMPARE(completed.size(), 1);
                const auto received = receiver.takeCompletedObject(metadata.transportId);
                QVERIFY2(received.ok(), received.status.detail.c_str());
                QCOMPARE(received.value->metadata.filename, std::string("protected.jpg"));
                QCOMPARE(received.value->originalBytes, original);
            }
        }
    }

    void repairsMissingRsSegments()
    {
        for (unsigned level = 1; level <= 4; ++level) {
            for (unsigned variant = 0; variant < 2; ++variant) {
                const auto original = jpegFixture(32U + variant, 24U, 997U + variant * 101U);
                const auto protectedBytes = hamdrm::encodeHamDrmRs(original, "jpg", level);
                QVERIFY(protectedBytes.ok());
                hamdrm::HamDrmMotObjectMetadata metadata;
                metadata.transportId = static_cast<std::uint16_t>(0x4400U + level * 2U + variant);
                metadata.filename = "repair.rs" + std::to_string(level);
                const auto object = hamdrm::encodeHamDrmObject(metadata, *protectedBytes.value, 64U);
                QVERIFY(object.ok());
                hamdrm::HamDrmController receiver({}, backends({}, {}));
                QSignalSpy completed(&receiver, &hamdrm::HamDrmController::objectCompleted);
                for (const auto& group : object.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
                for (std::size_t index = 0; index < object.value->bodyGroups.size(); ++index) {
                    if (index != 1U) QVERIFY(receiver.ingestMotGroup(bytes(object.value->bodyGroups[index])).ok());
                }
                QCOMPARE(completed.size(), 1);
                const auto received = receiver.takeCompletedObject(metadata.transportId);
                QVERIFY2(received.ok(), received.status.detail.c_str());
                QCOMPARE(received.value->originalBytes, original);
            }
        }
    }

    void rsRepairCorrectsUnmarkedDamage()
    {
        const auto original = jpegFixture(40U, 24U, 997U);
        auto protectedBytes = hamdrm::encodeHamDrmRs(original, "jpg", 1U);
        QVERIFY(protectedBytes.ok());
        // A damaged, received segment has a fresh MOT CRC. RS must correct
        // its unknown byte error as well as the declared missing segment.
        protectedBytes.value->at(10U) ^= 0x40U;
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.transportId = 0x4512U;
        metadata.filename = "damage.rs1";
        const auto object = hamdrm::encodeHamDrmObject(metadata, *protectedBytes.value, 64U);
        QVERIFY(object.ok());
        hamdrm::HamDrmController receiver({}, backends({}, {}));
        QSignalSpy completed(&receiver, &hamdrm::HamDrmController::objectCompleted);
        for (const auto& group : object.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        for (std::size_t index = 0; index < object.value->bodyGroups.size(); ++index)
            if (index != 1U) QVERIFY(receiver.ingestMotGroup(bytes(object.value->bodyGroups[index])).ok());
        QCOMPARE(completed.size(), 1);
        const auto received = receiver.takeCompletedObject(metadata.transportId);
        QVERIFY(received.ok());
        QCOMPARE(received.value->originalBytes, original);
    }

    void rsRecoveryRespectsAvailableParity()
    {
        const auto original = jpegFixture(32U, 24U, 2725U);
        for (const unsigned level : {1U, 4U}) {
            const auto protectedBytes = hamdrm::encodeHamDrmRs(original, "jpg", level);
            QVERIFY(protectedBytes.ok());
            hamdrm::HamDrmMotObjectMetadata metadata;
            metadata.transportId = static_cast<std::uint16_t>(0x4600U + level);
            metadata.filename = "budget.rs" + std::to_string(level);
            const auto object = hamdrm::encodeHamDrmObject(metadata, *protectedBytes.value, 1024U);
            QVERIFY(object.ok());
            QVERIFY(object.value->bodyGroups.size() > 2U);
            hamdrm::HamDrmController receiver({}, backends({}, {}));
            QSignalSpy completed(&receiver, &hamdrm::HamDrmController::objectCompleted);
            for (const auto& group : object.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
            for (std::size_t index = 0; index < object.value->bodyGroups.size(); ++index)
                if (index != 1U) QVERIFY(receiver.ingestMotGroup(bytes(object.value->bodyGroups[index])).ok());
            QCOMPARE(completed.size(), level == 4U ? 1 : 0);
            if (level == 4U) {
                const auto received = receiver.takeCompletedObject(metadata.transportId);
                QVERIFY(received.ok());
                QCOMPARE(received.value->originalBytes, original);
            } else {
                QVERIFY(receiver.canBuildBsr());
            }
        }
    }

    void manuallyRecoversMissingFinalRsSegment()
    {
        for (unsigned level = 1; level <= 4; ++level) {
            for (unsigned variant = 0; variant < 2; ++variant) {
                const auto original = jpegFixture(32U + variant, 24U, 997U + variant * 101U);
                const auto protectedBytes = hamdrm::encodeHamDrmRs(original, "jpg", level);
                QVERIFY(protectedBytes.ok());
                hamdrm::HamDrmMotObjectMetadata metadata;
                metadata.transportId = static_cast<std::uint16_t>(0x4700U + level * 2U + variant);
                metadata.filename = "final.rs" + std::to_string(level);
                const auto object = hamdrm::encodeHamDrmObject(metadata, *protectedBytes.value, 64U);
                QVERIFY(object.ok());
                hamdrm::HamDrmController receiver({}, backends({}, {}));
                QSignalSpy completed(&receiver, &hamdrm::HamDrmController::objectCompleted);
                for (const auto& group : object.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
                for (std::size_t index = 0; index + 1U < object.value->bodyGroups.size(); ++index)
                    QVERIFY(receiver.ingestMotGroup(bytes(object.value->bodyGroups[index])).ok());
                QCOMPARE(completed.size(), 0);
                QVERIFY2(receiver.tryRsRecovery(metadata.transportId), qPrintable(receiver.error()));
                QCOMPARE(completed.size(), 1);
                const auto received = receiver.takeCompletedObject(metadata.transportId);
                QVERIFY(received.ok());
                QCOMPARE(received.value->originalBytes, original);
            }
        }
    }

    void bsrTransfersBetweenControllers_data()
    {
        QTest::addColumn<int>("transportId");
        QTest::addColumn<bool>("extended");
        QTest::newRow("A-extended") << 0x2451 << true;
        QTest::newRow("B-compact") << 0x5372 << false;
    }

    void bsrTransfersBetweenControllers()
    {
        QFETCH(int, transportId);
        QFETCH(bool, extended);
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.transportId = static_cast<std::uint16_t>(transportId);
        metadata.filename = extended ? "image-a.jpg" : "image-b.jpg";
        const auto source = hamdrm::encodeHamDrmObject(metadata, jpegFixture(48, 32, 400), 64);
        QVERIFY(source.ok());
        QVERIFY(source.value->bodyGroups.size() > 3);
        auto tx = std::make_shared<FakeTxBackend>();
        hamdrm::HamDrmController sender({}, backends({}, tx));
        for (const auto& group : source.value->headerGroups) QVERIFY(sender.ingestMotGroup(bytes(group)).ok());
        QVERIFY(sender.ingestMotGroup(bytes(source.value->bodyGroups.front())).ok());
        QVERIFY(sender.ingestMotGroup(bytes(source.value->bodyGroups.back())).ok());
        QVERIFY(sender.canBuildBsr());
        QVERIFY(sender.startBsrTx(transportId, extended));
        QVERIFY(tx->object.has_value());
        QCOMPARE(sender.txState(), hamdrm::HamDrmController::OperationState::Active);
        auto recipientRx = std::make_shared<FakeRxBackend>();
        hamdrm::HamDrmController recipient({}, backends(recipientRx, {}));
        QSignalSpy received(&recipient, &hamdrm::HamDrmController::bsrReceived);
        QSignalSpy image(&recipient, &hamdrm::HamDrmController::objectCompleted);
        for (const auto& group : tx->object->headerGroups) QVERIFY(recipient.ingestMotGroup(bytes(group)).ok());
        for (const auto& group : tx->object->bodyGroups) QVERIFY(recipient.ingestMotGroup(bytes(group)).ok());
        QCOMPARE(received.size(), 1);
        // QSSTV can repeat body groups after completion without first
        // repeating the header. They must not become terminal-object errors.
        for (const auto& group : tx->object->bodyGroups)
            QVERIFY(recipient.ingestMotGroup(bytes(group)).ok());
        QCOMPARE(received.size(), 1);
        const auto& bodyGroup = tx->object->bodyGroups.front();
        const auto parsedBody = hamdrm::parseHamDrmMotDataGroup(bodyGroup.data(), bodyGroup.size());
        QVERIFY(parsedBody.ok());
        auto changedBody = *parsedBody.value;
        QVERIFY(!changedBody.payload.empty());
        changedBody.payload.front() ^= 1U;
        const auto conflictingBody = hamdrm::encodeHamDrmMotDataGroup(changedBody);
        QVERIFY(conflictingBody.ok());
        QVERIFY(!recipient.ingestMotGroup(bytes(*conflictingBody.value)).ok());
        QCOMPARE(received.size(), 1);
        QSignalSpy textChanged(&recipient, &hamdrm::HamDrmController::receivedBsrChanged);
        for (int repeat = 0; repeat < 4; ++repeat) {
            for (const auto& group : tx->object->headerGroups) QVERIFY(recipient.ingestMotGroup(bytes(group)).ok());
            for (const auto& group : tx->object->bodyGroups) QVERIFY(recipient.ingestMotGroup(bytes(group)).ok());
        }
        QCOMPARE(received.size(), 1);
        QCOMPARE(textChanged.size(), 0);
        QCOMPARE(received.front().at(1).toUInt(), static_cast<uint>(transportId));
        QCOMPARE(image.size(), 0);
        const QByteArray payload = recipient.receivedBsrText().toLatin1();
        const auto parsed = hamdrm::parseHamDrmBsr(
            reinterpret_cast<const std::uint8_t*>(payload.constData()), std::size_t(payload.size()));
        QVERIFY(parsed.ok());
        QCOMPARE(parsed.value->transportId, static_cast<std::uint16_t>(transportId));
        QCOMPARE(parsed.value->missingSegments.size(), source.value->bodyGroups.size() - 2);
        // A new RX session must accept another BSR with the fixed bsr.bin ID.
        QVERIFY(recipient.startRx());
        QVERIFY(recipient.receivedBsrText().isEmpty());
        for (const auto& group : tx->object->headerGroups) recipientRx->group(group);
        for (const auto& group : tx->object->bodyGroups) recipientRx->group(group);
        QCOMPARE(received.size(), 2);
        QCOMPARE(recipient.receivedBsrText().toLatin1(), payload);
        // The waveform carousel can repeat the previous header after its BSR
        // has completed, before the next BSR with a different body arrives.
        for (const auto& group : tx->object->headerGroups) recipientRx->group(group);
        QCOMPARE(recipient.receivedBsrText().toLatin1(), payload);
        tx->finish();
        QVERIFY(sender.ingestMotGroup(bytes(source.value->bodyGroups[1])).ok());
        QVERIFY(sender.startBsrTx(transportId, extended));
        for (const auto& group : tx->object->headerGroups) recipientRx->group(group);
        for (const auto& group : tx->object->bodyGroups) recipientRx->group(group);
        QCOMPARE(received.size(), 3);
        const auto updatedPayload = recipient.receivedBsrText().toLatin1();
        QVERIFY(updatedPayload != payload);
        const auto updated = hamdrm::parseHamDrmBsr(
            reinterpret_cast<const std::uint8_t*>(updatedPayload.constData()), std::size_t(updatedPayload.size()));
        QVERIFY(updated.ok());
        QCOMPARE(updated.value->missingSegments.size(), source.value->bodyGroups.size() - 3);
        // Identical request bytes in a different MOT object must not be
        // mistaken for a retransmission of the previous object.
        auto otherMetadata = tx->object->metadata;
        otherMetadata.transportId ^= 0x0001U;
        const std::vector<std::uint8_t> updatedBytes(updatedPayload.begin(), updatedPayload.end());
        const auto other = hamdrm::encodeHamDrmObject(otherMetadata, updatedBytes, 64);
        QVERIFY(other.ok());
        textChanged.clear();
        for (const auto& group : other.value->headerGroups) recipientRx->group(group);
        for (const auto& group : other.value->bodyGroups) recipientRx->group(group);
        QCOMPARE(received.size(), 4);
        QCOMPARE(received.back().at(0).toUInt(), uint(otherMetadata.transportId));
        QCOMPARE(received.back().at(1).toUInt(), uint(transportId));
        QCOMPARE(recipient.receivedBsrText().toLatin1(), updatedPayload);
        QCOMPARE(textChanged.size(), 0);
        for (const auto& group : other.value->headerGroups) recipientRx->group(group);
        for (const auto& group : other.value->bodyGroups) recipientRx->group(group);
        QCOMPARE(received.size(), 4);
        QVERIFY(recipient.cancelRx());
        QVERIFY(!sender.startBsrTx(transportId, extended));
        QVERIFY(sender.cancelTx());
    }

    void bsrInfersRegularSizeFromFinalOnly_data()
    {
        QTest::addColumn<bool>("extended");
        QTest::addColumn<int>("segmentSize");
        QTest::newRow("A-extended-64") << true << 64;
        QTest::newRow("B-compact-128") << false << 128;
    }

    void bsrInfersRegularSizeFromFinalOnly()
    {
        QFETCH(bool, extended);
        QFETCH(int, segmentSize);
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.transportId = 0x2451;
        metadata.filename = "repair.jpg";
        const auto source = hamdrm::encodeHamDrmObject(metadata, jpegFixture(48, 32, 400), segmentSize);
        QVERIFY(source.ok());
        QVERIFY(source.value->bodyGroups.size() > 2);
        hamdrm::HamDrmController controller({}, backends({}, std::make_shared<FakeTxBackend>()));
        for (const auto& group : source.value->headerGroups) QVERIFY(controller.ingestMotGroup(bytes(group)).ok());
        QVERIFY(controller.ingestMotGroup(bytes(source.value->bodyGroups.back())).ok());
        QVERIFY(controller.canBuildBsr());
        const QByteArray bsr = controller.buildBsr(metadata.transportId, extended).toLatin1();
        const auto parsed = hamdrm::parseHamDrmBsr(reinterpret_cast<const std::uint8_t*>(bsr.constData()),
                                                   std::size_t(bsr.size()));
        QVERIFY(parsed.ok());
        QCOMPARE(parsed.value->segmentSize, static_cast<std::uint16_t>(segmentSize));
        QCOMPARE(parsed.value->missingSegments.size(), source.value->bodyGroups.size() - 1);

        QCOMPARE(source.value->headerGroups.size(), std::size_t(1));
        auto corruptMetadata = source.value->metadata;
        ++corruptMetadata.bodySize;
        const auto header = hamdrm::encodeHamDrmMotHeader(corruptMetadata);
        QVERIFY(header.ok());
        const auto originalHeader = hamdrm::parseHamDrmMotDataGroup(
            source.value->headerGroups.front().data(), source.value->headerGroups.front().size());
        QVERIFY(originalHeader.ok());
        auto inconsistent = *originalHeader.value;
        inconsistent.payload = *header.value;
        const auto encoded = hamdrm::encodeHamDrmMotDataGroup(inconsistent);
        QVERIFY(encoded.ok());
        hamdrm::HamDrmController invalid({}, backends({}, std::make_shared<FakeTxBackend>()));
        QVERIFY(invalid.ingestMotGroup(bytes(*encoded.value)).ok());
        QVERIFY(invalid.ingestMotGroup(bytes(source.value->bodyGroups.back())).ok());
        QVERIFY(!invalid.canBuildBsr());
        QVERIFY(invalid.buildBsr(metadata.transportId, extended).isEmpty());

        const auto firstGroup = hamdrm::parseHamDrmMotDataGroup(
            source.value->bodyGroups.front().data(), source.value->bodyGroups.front().size());
        QVERIFY(firstGroup.ok());
        auto shortened = *firstGroup.value;
        shortened.payload.pop_back();
        const auto shortenedGroup = hamdrm::encodeHamDrmMotDataGroup(shortened);
        QVERIFY(shortenedGroup.ok());
        hamdrm::HamDrmController mismatched({}, backends({}, std::make_shared<FakeTxBackend>()));
        for (const auto& group : source.value->headerGroups) QVERIFY(mismatched.ingestMotGroup(bytes(group)).ok());
        QVERIFY(mismatched.ingestMotGroup(bytes(*shortenedGroup.value)).ok());
        QVERIFY(mismatched.ingestMotGroup(bytes(source.value->bodyGroups.back())).ok());
        QVERIFY(!mismatched.canBuildBsr());
        QVERIFY(mismatched.buildBsr(metadata.transportId, extended).isEmpty());
    }

    void missingFinalBsrRejectsInconsistentSegments()
    {
        for (const int segmentSize : {64, 128}) {
            hamdrm::HamDrmMotObjectMetadata metadata;
            metadata.transportId = 0x2451;
            metadata.filename = "reject.jpg";
            const auto source = hamdrm::encodeHamDrmObject(metadata, jpegFixture(48, 32, 400), segmentSize);
            QVERIFY(source.ok());
            for (const bool outOfRange : {false, true}) {
                hamdrm::HamDrmController controller({}, backends({}, std::make_shared<FakeTxBackend>()));
                for (const auto& group : source.value->headerGroups) QVERIFY(controller.ingestMotGroup(bytes(group)).ok());
                QVERIFY(controller.ingestMotGroup(bytes(source.value->bodyGroups.front())).ok());
                auto bad = hamdrm::parseHamDrmMotDataGroup(
                    source.value->bodyGroups[1].data(), source.value->bodyGroups[1].size());
                QVERIFY(bad.ok());
                if (outOfRange) bad.value->segmentNumber = static_cast<std::uint16_t>(source.value->bodyGroups.size() - 1U);
                else bad.value->payload.pop_back();
                const auto wire = hamdrm::encodeHamDrmMotDataGroup(*bad.value);
                QVERIFY(wire.ok());
                QVERIFY(controller.ingestMotGroup(bytes(*wire.value)).ok());
                QVERIFY(!controller.canBuildBsr());
                QVERIFY(controller.missingSegments().isEmpty());
                QVERIFY(controller.buildBsr(metadata.transportId, segmentSize == 64).isEmpty());
            }
        }
    }

    void bsrWithUnmarkedFinalSegment_data()
    {
        QTest::addColumn<int>("payloadSize");
        QTest::addColumn<bool>("extended");
        QTest::newRow("A-short-final-extended") << 925 << true;
        QTest::newRow("B-short-final-compact") << 900 << false;
        QTest::newRow("C-full-final-extended") << 924 << true;
    }

    void bsrWithUnmarkedFinalSegment()
    {
        QFETCH(int, payloadSize);
        QFETCH(bool, extended);
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.transportId = 0x2451;
        metadata.filename = "repair.jpg";
        const auto original = jpegFixture(32, 24, static_cast<std::size_t>(payloadSize));
        const auto object = hamdrm::encodeHamDrmObject(metadata, original, 154);
        QVERIFY(object.ok());
        auto backend = std::make_shared<FakeTxBackend>();
        hamdrm::HamDrmController receiver({}, backends({}, backend));
        for (const auto& group : object.value->headerGroups)
            QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        const std::size_t last = object.value->bodyGroups.size() - 1U;
        for (std::size_t i = 0; i < last; ++i)
            if (i != 1U) QVERIFY(receiver.ingestMotGroup(bytes(object.value->bodyGroups[i])).ok());
        auto unmarked = hamdrm::parseHamDrmMotDataGroup(
            object.value->bodyGroups[last].data(), object.value->bodyGroups[last].size());
        QVERIFY(unmarked.ok());
        unmarked.value->lastSegment = false;
        const auto wire = hamdrm::encodeHamDrmMotDataGroup(*unmarked.value);
        QVERIFY(wire.ok());
        QVERIFY(receiver.ingestMotGroup(bytes(*wire.value)).ok());
        QVERIFY(!receiver.completedObject(metadata.transportId).ok());
        QVERIFY(receiver.canBuildBsr());
        QCOMPARE(receiver.missingSegments(), QVariantList({1, static_cast<int>(last)}));
        QVERIFY(!receiver.buildBsr(metadata.transportId, extended).isEmpty());
        QVERIFY(receiver.ingestMotGroup(bytes(object.value->bodyGroups[1])).ok());
        QVERIFY(!receiver.completedObject(metadata.transportId).ok());
        QVERIFY(receiver.ingestMotGroup(bytes(object.value->bodyGroups[last])).ok());
        const auto completed = receiver.completedObject(metadata.transportId);
        QVERIFY(completed.ok());
        QCOMPARE(completed.value->originalBytes, original);
    }

    void completedCarouselManySegments_data()
    {
        QTest::addColumn<int>("segmentSize");
        QTest::newRow("A-single-byte-segments") << 1;
        QTest::newRow("B-regular-segments") << 64;
    }

    void completedCarouselManySegments()
    {
        QFETCH(int, segmentSize);
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.transportId = 0x7412;
        metadata.filename = "many.jpg";
        const auto original = jpegFixture(16, 16, 4096);
        const auto encoded = hamdrm::encodeHamDrmObject(metadata, original,
                                                        static_cast<std::size_t>(segmentSize));
        QVERIFY(encoded.ok());
        hamdrm::HamDrmController receiver({}, backends({}, {}));
        QSignalSpy completed(&receiver, &hamdrm::HamDrmController::objectCompleted);
        for (const auto& group : encoded.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        for (const auto& group : encoded.value->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        QCOMPARE(completed.size(), 1);
        for (const auto& group : encoded.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        for (const auto& group : encoded.value->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        QCOMPARE(completed.size(), 1);
        const auto result = receiver.completedObject(metadata.transportId);
        QVERIFY(result.ok());
        QCOMPARE(result.value->originalBytes, original);
        QVERIFY(receiver.error().isEmpty());
    }

    void malformedBsrIsRejectedBeforePublication()
    {
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.transportId = 0x7131;
        metadata.filename = "bsr.bin";
        const QByteArray invalid("1\nH_OK\n82\n1\n-1\n-99\n");
        const std::vector<std::uint8_t> content(invalid.cbegin(), invalid.cend());
        const auto object = hamdrm::encodeHamDrmObject(metadata, content, 64);
        QVERIFY(object.ok());
        hamdrm::HamDrmController receiver({}, backends({}, {}));
        QSignalSpy received(&receiver, &hamdrm::HamDrmController::bsrReceived);
        QSignalSpy image(&receiver, &hamdrm::HamDrmController::objectCompleted);
        for (const auto& group : object.value->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        QVERIFY(!receiver.ingestMotGroup(bytes(object.value->bodyGroups.front())).ok());
        QVERIFY(receiver.receivedBsrText().isEmpty());
        QCOMPARE(received.size(), 0);
        QCOMPARE(image.size(), 0);
    }

    void bsrRepairCompletesPartialImage_data()
    {
        QTest::addColumn<bool>("extended");
        QTest::addColumn<bool>("missingFinal");
        QTest::addColumn<bool>("unknownSize");
        QTest::newRow("A-extended") << true << false << false;
        QTest::newRow("B-compact") << false << false << false;
        QTest::newRow("A-extended-final") << true << true << false;
        QTest::newRow("B-compact-final") << false << true << false;
        QTest::newRow("A-extended-unknown-size") << true << false << true;
        QTest::newRow("B-compact-unknown-size") << false << false << true;
    }

    void bsrRepairCompletesPartialImage()
    {
        QFETCH(bool, extended);
        QFETCH(bool, missingFinal);
        QFETCH(bool, unknownSize);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QImage original(96, 64, QImage::Format_RGB32);
        for (int y = 0; y < original.height(); ++y)
            for (int x = 0; x < original.width(); ++x)
                original.setPixelColor(x, y, QColor((x * 11 + y) % 256, (y * 7 + x) % 256,
                                                    (x * y + (extended ? 31 : 79)) % 256));
        const QString path = dir.filePath(extended ? QStringLiteral("repair-a.jpg") : QStringLiteral("repair-b.jpg"));
        QVERIFY(original.save(path, "JPG"));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const QByteArray expected = file.readAll();
        hamdrm::HamDrmControllerConfig config;
        config.txBodySegmentBytes = 128;
        auto tx = std::make_shared<FakeTxBackend>();
        hamdrm::HamDrmController sender(config, backends({}, tx));
        QVERIFY(sender.startTx(QUrl::fromLocalFile(path)));
        QVERIFY(tx->object.has_value());
        const auto sent = *tx->object;
        QVERIFY(sent.bodyGroups.size() > 5);
        const int imageId = sent.metadata.transportId;
        tx->finish();
        QCOMPARE(sender.txState(), hamdrm::HamDrmController::OperationState::Completed);
        QVERIFY(!sender.canRepairReceivedBsr());
        QVERIFY(!sender.startBsrRepair());

        // A received request must identify this image and valid body segments.
        // Exercise both wire dialects before accepting the genuine request.
        for (int mismatch = 0; mismatch < (extended ? 5 : 4); ++mismatch) {
            hamdrm::HamDrmBsrRequest request;
            request.transportId = static_cast<std::uint16_t>(imageId + (mismatch == 0));
            request.segmentSize = mismatch == 1 ? 127 : 128;
            request.missingSegments = {static_cast<std::uint16_t>(
                mismatch == 2 ? sent.bodyGroups.size() : 1)};
            // A request containing a valid segment first must still be
            // rejected if a later segment is outside the transmitted body.
            if (mismatch == (extended ? 4 : 3))
                request.missingSegments.push_back(static_cast<std::uint16_t>(sent.bodyGroups.size()));
            if (extended) {
                request.filename = mismatch == 3 ? "different.jpg" : sent.metadata.filename;
                request.qsstvCompatibilityCode = 0;
            }
            const auto payload = hamdrm::encodeHamDrmBsr(request, extended
                ? hamdrm::HamDrmBsrDialect::QsstvExtended
                : hamdrm::HamDrmBsrDialect::EasyPalCompatible);
            QVERIFY2(payload.ok(), payload.status.detail.c_str());
            hamdrm::HamDrmMotObjectMetadata metadata;
            metadata.transportId = 0x7131;
            metadata.filename = "bsr.bin";
            auto wire = bytes(*payload.value);
            if (unknownSize && mismatch != 1) {
                auto lines = wire.split('\n');
                lines[2] = "0";
                wire = lines.join('\n');
            }
            const std::vector<std::uint8_t> body(wire.begin(), wire.end());
            const auto object = hamdrm::encodeHamDrmObject(metadata, body, 128);
            QVERIFY(object.ok());
            for (const auto& group : object.value->headerGroups) QVERIFY(sender.ingestMotGroup(bytes(group)).ok());
            for (const auto& group : object.value->bodyGroups) QVERIFY(sender.ingestMotGroup(bytes(group)).ok());
            QVERIFY(!sender.canRepairReceivedBsr());
            QVERIFY(!sender.startBsrRepair());
            QVERIFY(sender.discardObject(metadata.transportId));
        }

        auto replyTx = std::make_shared<FakeTxBackend>();
        hamdrm::HamDrmController receiver(config, backends({}, replyTx));
        for (const auto& group : sent.headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        for (std::size_t i = 0; i < sent.bodyGroups.size(); ++i)
            if (i != 1 && i != (missingFinal ? sent.bodyGroups.size() - 1U : 3U))
                QVERIFY(receiver.ingestMotGroup(bytes(sent.bodyGroups[i])).ok());
        QCOMPARE(receiver.missingSegments().size(), 2);
        QVERIFY(receiver.canBuildBsr());
        QCOMPARE(receiver.missingSegmentsText().contains(QStringLiteral("inferred from header")), missingFinal);
        const QString receivedProfile = receiver.selectedProfileId();
        const QString expectedBsr = receiver.buildBsr(imageId, extended);
        QVERIFY(!expectedBsr.isEmpty());
        QString alternateReceiveProfile;
        for (const QVariant& item : receiver.profiles()) {
            const QString id = item.toMap().value(QStringLiteral("id")).toString();
            if (id != receivedProfile) { alternateReceiveProfile = id; break; }
        }
        QVERIFY(!alternateReceiveProfile.isEmpty());
        receiver.setSelectedProfileId(alternateReceiveProfile);
        QCOMPARE(receiver.selectedProfileId(), alternateReceiveProfile);
        QCOMPARE(receiver.buildBsr(imageId, extended), expectedBsr);
        receiver.setSelectedProfileId(receivedProfile);
        QVERIFY(receiver.startBsrTx(imageId, extended));
        QVERIFY(replyTx->object.has_value());
        auto replyObject = *replyTx->object;
        if (unknownSize) {
            auto lines = receiver.bsrText().toLatin1().split('\n');
            lines[2] = "0";
            const auto wire = lines.join('\n');
            const std::vector<std::uint8_t> body(wire.begin(), wire.end());
            auto metadata = replyObject.metadata;
            metadata.bodySize = 0U;
            const auto encoded = hamdrm::encodeHamDrmObject(metadata, body, 128);
            QVERIFY(encoded.ok());
            replyObject = *encoded.value;
        }
        for (const auto& group : replyObject.headerGroups) QVERIFY(sender.ingestMotGroup(bytes(group)).ok());
        for (const auto& group : replyObject.bodyGroups) QVERIFY(sender.ingestMotGroup(bytes(group)).ok());
        QVERIFY(sender.canRepairReceivedBsr());
        // A repair must use the profile of the retained transmission. Check
        // that temporarily selecting another valid profile blocks it without
        // losing the request, then restore the original profile and repair.
        const QString originalProfile = sender.selectedProfileId();
        QString alternativeProfile;
        for (const QVariant& item : sender.profiles()) {
            const QString id = item.toMap().value(QStringLiteral("id")).toString();
            if (id != originalProfile) { alternativeProfile = id; break; }
        }
        QVERIFY(!alternativeProfile.isEmpty());
        sender.setSelectedProfileId(alternativeProfile);
        QCOMPARE(sender.selectedProfileId(), alternativeProfile);
        QVERIFY(!sender.canRepairReceivedBsr());
        QVERIFY(!sender.startBsrRepair());
        sender.setSelectedProfileId(originalProfile);
        QCOMPARE(sender.selectedProfileId(), originalProfile);
        QVERIFY(sender.canRepairReceivedBsr());
        QVERIFY(sender.startBsrRepair());
        QVERIFY(tx->object.has_value());
        QCOMPARE(tx->object->bodyGroups.size(), std::size_t {2});
        QSignalSpy completed(&receiver, &hamdrm::HamDrmController::objectCompleted);
        for (const auto& group : tx->object->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        for (const auto& group : tx->object->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        QCOMPARE(completed.size(), 1);
        const auto snapshot = receiver.completedObject(static_cast<std::uint16_t>(imageId));
        QVERIFY(snapshot.ok());
        QCOMPARE(bytes(snapshot.value->originalBytes), expected);
        QVERIFY(receiver.completedObject(static_cast<std::uint16_t>(imageId)).ok());
        for (const auto& group : sent.headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        for (const auto& group : sent.bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        QCOMPARE(completed.size(), 1);
        auto collision = hamdrm::parseHamDrmMotDataGroup(sent.bodyGroups.front().data(),
                                                        sent.bodyGroups.front().size(), config.limits);
        QVERIFY(collision.ok());
        auto repeated = *collision.value;
        repeated.continuityIndex = (repeated.continuityIndex + 1U) & 0x0fU;
        repeated.repetitionIndex = (repeated.repetitionIndex + 1U) & 0x0fU;
        const auto repeatedWire = hamdrm::encodeHamDrmMotDataGroup(repeated, config.limits);
        QVERIFY(repeatedWire.ok());
        QVERIFY(receiver.ingestMotGroup(bytes(*repeatedWire.value)).ok());
        auto wrongLast = *collision.value;
        wrongLast.lastSegment = !wrongLast.lastSegment;
        const auto wrongLastWire = hamdrm::encodeHamDrmMotDataGroup(wrongLast, config.limits);
        QVERIFY(wrongLastWire.ok());
        QVERIFY(!receiver.ingestMotGroup(bytes(*wrongLastWire.value)).ok());
        receiver.clearError();
        auto unknown = *collision.value;
        unknown.segmentNumber = static_cast<std::uint16_t>(sent.bodyGroups.size());
        const auto unknownWire = hamdrm::encodeHamDrmMotDataGroup(unknown, config.limits);
        QVERIFY(unknownWire.ok());
        QVERIFY(!receiver.ingestMotGroup(bytes(*unknownWire.value)).ok());
        receiver.clearError();
        collision.value->payload.front() ^= 1U;
        const auto changed = hamdrm::encodeHamDrmMotDataGroup(*collision.value, config.limits);
        QVERIFY(changed.ok());
        QVERIFY(!receiver.ingestMotGroup(bytes(*changed.value)).ok());
        QVERIFY(receiver.completedObject(static_cast<std::uint16_t>(imageId)).ok());
        const auto result = receiver.takeCompletedObject(static_cast<std::uint16_t>(imageId));
        QVERIFY(result.ok());
        QCOMPARE(bytes(result.value->originalBytes), expected);
        QVERIFY(!receiver.completedObject(static_cast<std::uint16_t>(imageId)).ok());
        tx->finish();
        QCOMPARE(sender.txState(), hamdrm::HamDrmController::OperationState::Completed);
        QVERIFY(receiver.cancelTx());
    }

    void txUsesNamedProfileNativeJpeg2000AndCancellation()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        const auto jpeg = hamdrm::makeNativeHamDrmJpeg2000Backend();
        QVERIFY(jpeg);
        hamdrm::HamDrmRgbaImage source;
        source.width = 12U;
        source.height = 9U;
        source.rgba.resize(
            static_cast<std::size_t>(source.width) * source.height * 4U);
        for (std::size_t index = 0U; index < source.rgba.size(); index += 4U) {
            source.rgba[index] = static_cast<std::uint8_t>(index & 0xffU);
            source.rgba[index + 1U] = static_cast<std::uint8_t>(
                (index * 3U) & 0xffU);
            source.rgba[index + 2U] = static_cast<std::uint8_t>(
                (index * 7U) & 0xffU);
            source.rgba[index + 3U] = 255U;
        }
        const auto encodedJp2 = jpeg->encodeLossless(source, {});
        QVERIFY2(encodedJp2.ok(), encodedJp2.status.detail.c_str());
        const QUrl imageUrl = writeFixture(
            temporary.filePath(QStringLiteral("native.jp2")),
            *encodedJp2.value);
        QVERIFY(imageUrl.isValid());

        auto tx = std::make_shared<FakeTxBackend>();
        hamdrm::HamDrmController controller(
            {}, backends({}, tx, jpeg));
        QSignalSpy imageAccepted(
            &controller, &hamdrm::HamDrmController::txImageAccepted);
        QVERIFY(imageAccepted.isValid());
        const QVariantList profiles = controller.profiles();
        const QString chosenId = profiles.at(37).toMap()
                                     .value(QStringLiteral("id")).toString();
        controller.setSelectedProfileId(chosenId);
        QCOMPARE(controller.selectedProfileId(), chosenId);
        QVERIFY(controller.startTx(imageUrl));
        QCOMPARE(tx->starts, 1);
        QCOMPARE(tx->profileId, chosenId);
        QVERIFY(tx->object.has_value());
        QCOMPARE(tx->object->metadata.filename, std::string("native.jp2"));
        QVERIFY(!tx->object->headerGroups.empty());
        QVERIFY(!tx->object->bodyGroups.empty());
        QVERIFY(controller.lastImageValidation()
                    .value(QStringLiteral("jpeg2000Decoded")).toBool());
        QCOMPARE(imageAccepted.size(), 1);
        const QList<QVariant> accepted = imageAccepted.takeFirst();
        const QImage galleryImage = accepted.at(0).value<QImage>();
        QVERIFY(!galleryImage.isNull());
        QCOMPARE(galleryImage.size(), QSize(12, 9));
        QCOMPARE(galleryImage.format(), QImage::Format_RGBA8888);
        QCOMPARE(accepted.at(1).toString(), chosenId);
        QCOMPARE(accepted.at(2).toInt(),
                 profiles.at(37).toMap()
                     .value(QStringLiteral("bandwidthHz")).toInt());

        tx->progress(0.45);
        QCOMPARE(controller.txProgress(), 0.45);
        QVERIFY(controller.cancelTx());
        QCOMPARE(tx->cancels, 1);
        QCOMPARE(controller.txState(),
                 hamdrm::HamDrmController::OperationState::Cancelled);
        QVERIFY(!controller.busy());

        auto noCodecTx = std::make_shared<FakeTxBackend>();
        hamdrm::HamDrmController noCodec(
            {}, backends({}, noCodecTx));
        QSignalSpy noCodecAccepted(
            &noCodec, &hamdrm::HamDrmController::txImageAccepted);
        QVERIFY(noCodec.validateTxImage(imageUrl));
        QVERIFY(!noCodec.lastImageValidation()
                     .value(QStringLiteral("jpeg2000Decoded")).toBool());
        QVERIFY(!noCodec.startTx(imageUrl));
        QCOMPARE(noCodecTx->starts, 0);
        QCOMPARE(noCodecAccepted.size(), 0);
        QVERIFY(noCodec.error().contains(
            QStringLiteral("decode adapter"), Qt::CaseInsensitive));

        const QUrl remote(QStringLiteral("https://example.invalid/image.jp2"));
        QVERIFY(!controller.validateTxImage(remote));
        QVERIFY(controller.error().contains(
            QStringLiteral("local file"), Qt::CaseInsensitive));
    }

    void txRejectsOutOfBoundsImageBeforeGallerySnapshot()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        const auto jpeg = hamdrm::makeNativeHamDrmJpeg2000Backend();
        QVERIFY(jpeg);
        hamdrm::HamDrmRgbaImage source;
        source.width = 12U;
        source.height = 9U;
        source.rgba.resize(
            static_cast<std::size_t>(source.width) * source.height * 4U,
            0x7fU);
        for (std::size_t index = 3U; index < source.rgba.size(); index += 4U) {
            source.rgba[index] = 0xffU;
        }
        const auto encodedJp2 = jpeg->encodeLossless(source, {});
        QVERIFY2(encodedJp2.ok(), encodedJp2.status.detail.c_str());
        const QUrl imageUrl = writeFixture(
            temporary.filePath(QStringLiteral("oversized-for-policy.jp2")),
            *encodedJp2.value);
        QVERIFY(imageUrl.isValid());

        hamdrm::HamDrmControllerConfig config;
        config.limits.maximumImageDimension = 8U;
        config.limits.maximumImagePixels = 64U;
        auto tx = std::make_shared<FakeTxBackend>();
        hamdrm::HamDrmController controller(
            config, backends({}, tx, jpeg));
        QSignalSpy imageAccepted(
            &controller, &hamdrm::HamDrmController::txImageAccepted);
        QVERIFY(!controller.startTx(imageUrl));
        QCOMPARE(tx->starts, 0);
        QCOMPARE(imageAccepted.size(), 0);
        QVERIFY(controller.error().contains(
            QStringLiteral("dimensions exceed"), Qt::CaseInsensitive));
    }

    void txRejectsStructurallyValidImageWithoutGallerySnapshot()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        // The MOT image validator deliberately only needs a bounded JPEG
        // structure.  This fixture has such a header but no decodable scan;
        // the TX path must reject it before the waveform backend can accept
        // an image that Gallery cannot snapshot.
        const QUrl imageUrl = writeFixture(
            temporary.filePath(QStringLiteral("header-only.jpg")),
            jpegFixture(12U, 9U));
        QVERIFY(imageUrl.isValid());

        auto tx = std::make_shared<FakeTxBackend>();
        hamdrm::HamDrmController controller({}, backends({}, tx));
        QSignalSpy imageAccepted(
            &controller, &hamdrm::HamDrmController::txImageAccepted);
        QVERIFY(!controller.startTx(imageUrl));
        QCOMPARE(tx->starts, 0);
        QCOMPARE(imageAccepted.size(), 0);
        QVERIFY(controller.error().contains(
            QStringLiteral("Gallery snapshot"), Qt::CaseInsensitive));
    }

    void txRejectsGalleryIncompatibleImageBeforeJpeg2000Decode()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto native = hamdrm::makeNativeHamDrmJpeg2000Backend();
        QVERIFY(native);

        hamdrm::HamDrmLimits expandedLimits;
        expandedLimits.maximumImageDimension = 8'193U;
        expandedLimits.maximumImagePixels = 16'386U;
        hamdrm::HamDrmRgbaImage source;
        source.width = 8'193U;
        source.height = 2U;
        source.rgba.resize(
            static_cast<std::size_t>(source.width) * source.height * 4U,
            0x5aU);
        for (std::size_t index = 3U; index < source.rgba.size(); index += 4U) {
            source.rgba[index] = 0xffU;
        }
        const auto encodedJp2 = native->encodeLossless(source, expandedLimits);
        QVERIFY2(encodedJp2.ok(), encodedJp2.status.detail.c_str());
        const QUrl imageUrl = writeFixture(
            temporary.filePath(QStringLiteral("gallery-too-wide.jp2")),
            *encodedJp2.value);
        QVERIFY(imageUrl.isValid());

        auto counting = std::make_shared<CountingJpeg2000Backend>(native);
        auto tx = std::make_shared<FakeTxBackend>();
        hamdrm::HamDrmControllerConfig config;
        config.limits = expandedLimits;
        hamdrm::HamDrmController controller(
            config, backends({}, tx, counting));
        QSignalSpy imageAccepted(
            &controller, &hamdrm::HamDrmController::txImageAccepted);
        QVERIFY(!controller.startTx(imageUrl));
        QCOMPARE(counting->decodeCalls, 0);
        QCOMPARE(tx->starts, 0);
        QCOMPARE(imageAccepted.size(), 0);
        QVERIFY(controller.error().contains(
            QStringLiteral("Gallery snapshot"), Qt::CaseInsensitive));
    }
};

QTEST_GUILESS_MAIN(TestHamDrmController)
#include "test_hamdrm_controller.moc"
