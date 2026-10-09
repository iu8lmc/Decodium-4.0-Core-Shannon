// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/digital/HamDrmController.h"

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
