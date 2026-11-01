// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/digital/HamDrmController.h"
#include "src/sstv/digital/HamDrmBsrCodec.h"

#include <QtTest/QtTest>

#include <QFile>
#include <QImage>
#include <QSet>
#include <QSignalSpy>
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

    void partialMotObjectPersistsResumesBuildsBsrAndCompletes()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        constexpr std::uint16_t transportId = 0x4265U;
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.transportId = transportId;
        metadata.filename = "resume.jpg";
        const std::vector<std::uint8_t> image = jpegFixture(48U, 32U, 192U);
        const auto encoded = hamdrm::encodeHamDrmObject(
            metadata, image, 24U);
        QVERIFY2(encoded.ok(), encoded.status.detail.c_str());
        QVERIFY(encoded.value->bodyGroups.size() >= 4U);

        hamdrm::HamDrmControllerConfig config;
        config.partialStoreRoot = temporary.path();
        auto rx = std::make_shared<FakeRxBackend>();
        {
            hamdrm::HamDrmController controller(
                config, backends(rx, {}));
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
        QVERIFY(resumed.resumePartial(transportId));
        QCOMPARE(resumed.missingSegments().size(),
                 static_cast<qsizetype>(
                     encoded.value->bodyGroups.size() - 2U));
        QVERIFY(resumed.canBuildBsr());
        QVERIFY(resumed.startRx());

        QSignalSpy completed(&resumed,
                             &hamdrm::HamDrmController::objectCompleted);
        for (std::size_t index = 1U;
             index + 1U < encoded.value->bodyGroups.size(); ++index) {
            resumedRx->group(encoded.value->bodyGroups[index]);
        }
        QCOMPARE(completed.size(), 1);
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
        QVERIFY(recipient.cancelRx());
        QVERIFY(!sender.startBsrTx(transportId, extended));
        QVERIFY(sender.cancelTx());
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
        QTest::newRow("A-extended") << true;
        QTest::newRow("B-compact") << false;
    }

    void bsrRepairCompletesPartialImage()
    {
        QFETCH(bool, extended);
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
        for (int mismatch = 0; mismatch < (extended ? 4 : 3); ++mismatch) {
            hamdrm::HamDrmBsrRequest request;
            request.transportId = static_cast<std::uint16_t>(imageId + (mismatch == 0));
            request.segmentSize = mismatch == 1 ? 127 : 128;
            request.missingSegments = {static_cast<std::uint16_t>(
                mismatch == 2 ? sent.bodyGroups.size() : 1)};
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
            const auto object = hamdrm::encodeHamDrmObject(metadata, *payload.value, 128);
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
            if (i != 1 && i != 3) QVERIFY(receiver.ingestMotGroup(bytes(sent.bodyGroups[i])).ok());
        QCOMPARE(receiver.missingSegments().size(), 2);
        QVERIFY(receiver.startBsrTx(imageId, extended));
        QVERIFY(replyTx->object.has_value());
        for (const auto& group : replyTx->object->headerGroups) QVERIFY(sender.ingestMotGroup(bytes(group)).ok());
        for (const auto& group : replyTx->object->bodyGroups) QVERIFY(sender.ingestMotGroup(bytes(group)).ok());
        QVERIFY(sender.canRepairReceivedBsr());
        QVERIFY(sender.startBsrRepair());
        QVERIFY(tx->object.has_value());
        QCOMPARE(tx->object->bodyGroups.size(), std::size_t {2});
        QSignalSpy completed(&receiver, &hamdrm::HamDrmController::objectCompleted);
        for (const auto& group : tx->object->headerGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        for (const auto& group : tx->object->bodyGroups) QVERIFY(receiver.ingestMotGroup(bytes(group)).ok());
        QCOMPARE(completed.size(), 1);
        const auto result = receiver.takeCompletedObject(static_cast<std::uint16_t>(imageId));
        QVERIFY(result.ok());
        QCOMPARE(bytes(result.value->originalBytes), expected);
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
