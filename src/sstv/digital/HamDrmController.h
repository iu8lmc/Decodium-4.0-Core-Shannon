// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "HamDrmImageValidator.h"
#include "HamDrmJpeg2000Codec.h"
#include "HamDrmMotCodec.h"
#include "HamDrmObjectAssembler.h"
#include "HamDrmPartialStore.h"
#include "HamDrmProfileRegistry.h"

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace decodium::sstv::hamdrm {

struct HamDrmWaveformCapability final
{
    // True only for an adapter that implements the complete application to
    // audio/RF boundary.  A profile codec, OFDM primitive or channel codec by
    // itself must leave this false.
    bool completeBackend {false};
    QString backendName;
    QString detail;
};

class HamDrmWaveformRxSink
{
public:
    virtual ~HamDrmWaveformRxSink() = default;

    virtual void hamDrmRxProgress(std::uint64_t sessionId,
                                  double progress) = 0;
    virtual void hamDrmRxMotGroup(std::uint64_t sessionId,
                                  QByteArray encodedGroup) = 0;
    virtual void hamDrmRxFinished(std::uint64_t sessionId,
                                  HamDrmStatus status) = 0;
};

class HamDrmWaveformTxSink
{
public:
    virtual ~HamDrmWaveformTxSink() = default;

    virtual void hamDrmTxProgress(std::uint64_t sessionId,
                                  double progress) = 0;
    virtual void hamDrmTxFinished(std::uint64_t sessionId,
                                  HamDrmStatus status) = 0;
};

// Injected waveform adapters own the radio/audio integration.  cancel() must
// synchronously detach the sink for the supplied session before it returns;
// callbacks may originate on any thread and are marshalled by the controller.
class HamDrmWaveformRxBackend
{
public:
    virtual ~HamDrmWaveformRxBackend() = default;
    virtual HamDrmWaveformCapability capability() const = 0;
    virtual HamDrmStatus start(const HamDrmProfile& profile,
                               std::uint64_t sessionId,
                               HamDrmWaveformRxSink& sink) = 0;
    virtual void cancel(std::uint64_t sessionId) noexcept = 0;
};

class HamDrmWaveformTxBackend
{
public:
    virtual ~HamDrmWaveformTxBackend() = default;
    virtual HamDrmWaveformCapability capability() const = 0;
    virtual HamDrmStatus start(const HamDrmProfile& profile,
                               HamDrmEncodedObject object,
                               std::uint64_t sessionId,
                               HamDrmWaveformTxSink& sink) = 0;
    virtual void cancel(std::uint64_t sessionId) noexcept = 0;
};

struct HamDrmJpeg2000Capability final
{
    bool decodeAvailable {false};
    bool encodeAvailable {false};
    QString backendName;
    QString detail;
};

// The application layer does not infer JPEG2000 support from headers or a
// build flag.  A linked, operational codec adapter must explicitly advertise
// each direction and perform the bounded operation.
class HamDrmJpeg2000Backend
{
public:
    virtual ~HamDrmJpeg2000Backend() = default;
    virtual HamDrmJpeg2000Capability capability() const = 0;
    virtual HamDrmValueResult<HamDrmRgbaImage> decode(
        const std::uint8_t* data,
        std::size_t size,
        const HamDrmLimits& limits) const = 0;
    virtual HamDrmValueResult<std::vector<std::uint8_t>> encodeLossless(
        const HamDrmRgbaImage& image,
        const HamDrmLimits& limits) const = 0;
};

// Production adapter for the native OpenJPEG-backed codec compiled in
// HamDrmJpeg2000Codec.cpp.  Bridge/CMake integration can combine this factory
// with the full waveform adapters and an AppData partialStoreRoot.
std::shared_ptr<HamDrmJpeg2000Backend>
makeNativeHamDrmJpeg2000Backend();

struct HamDrmControllerBackends final
{
    std::shared_ptr<HamDrmWaveformRxBackend> waveformRx;
    std::shared_ptr<HamDrmWaveformTxBackend> waveformTx;
    std::shared_ptr<HamDrmJpeg2000Backend> jpeg2000;
};

struct HamDrmControllerConfig final
{
    HamDrmLimits limits;
    QString partialStoreRoot;
    std::size_t maximumInboxObjects {64U};
    std::size_t maximumCompletedObjects {8U};
    std::size_t maximumCompletedBytes {32U * 1024U * 1024U};
    std::size_t txBodySegmentBytes {1'024U};
    std::size_t maximumErrorCharacters {1'024U};
};

// Owner-thread application facade.  It exposes stable profile IDs and named
// typed fields to QML, never registry indexes or QSSTV compatibility numbers.
// Complete waveform backends are injected explicitly. MOT/BSR/partial-state
// and validation remain usable without them; startRx/startTx then fail
// accurately and visibly instead of advertising a false RF capability.
class HamDrmController final : public QObject,
                               private HamDrmWaveformRxSink,
                               private HamDrmWaveformTxSink
{
    Q_OBJECT
    Q_PROPERTY(QVariantList profiles READ profiles CONSTANT)
    Q_PROPERTY(QString selectedProfileId READ selectedProfileId
               WRITE setSelectedProfileId NOTIFY selectedProfileChanged)
    Q_PROPERTY(QVariantMap selectedProfile READ selectedProfile
               NOTIFY selectedProfileChanged)
    Q_PROPERTY(QVariantMap capabilities READ capabilities CONSTANT)
    Q_PROPERTY(QString capabilityMessage READ capabilityMessage CONSTANT)
    Q_PROPERTY(bool waveformRxAvailable READ waveformRxAvailable CONSTANT)
    Q_PROPERTY(bool waveformTxAvailable READ waveformTxAvailable CONSTANT)
    Q_PROPERTY(bool jpeg2000DecodeAvailable READ jpeg2000DecodeAvailable CONSTANT)
    Q_PROPERTY(bool jpeg2000EncodeAvailable READ jpeg2000EncodeAvailable CONSTANT)
    Q_PROPERTY(bool partialResumeAvailable READ partialResumeAvailable CONSTANT)
    Q_PROPERTY(OperationState rxState READ rxState NOTIFY operationStateChanged)
    Q_PROPERTY(OperationState txState READ txState NOTIFY operationStateChanged)
    Q_PROPERTY(QString rxStateName READ rxStateName NOTIFY operationStateChanged)
    Q_PROPERTY(QString txStateName READ txStateName NOTIFY operationStateChanged)
    Q_PROPERTY(double rxProgress READ rxProgress NOTIFY operationStateChanged)
    Q_PROPERTY(double txProgress READ txProgress NOTIFY operationStateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY operationStateChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantList inbox READ inbox NOTIFY inboxChanged)
    Q_PROPERTY(int selectedTransportId READ selectedTransportId
               NOTIFY selectedObjectChanged)
    Q_PROPERTY(QVariantList missingSegments READ missingSegments
               NOTIFY selectedObjectChanged)
    Q_PROPERTY(QString missingSegmentsText READ missingSegmentsText
               NOTIFY selectedObjectChanged)
    Q_PROPERTY(bool canBuildBsr READ canBuildBsr NOTIFY selectedObjectChanged)
    Q_PROPERTY(QString bsrText READ bsrText NOTIFY bsrChanged)
    Q_PROPERTY(QVariantMap lastImageValidation READ lastImageValidation
               NOTIFY imageValidationChanged)

public:
    enum class OperationState : std::uint8_t {
        Unavailable,
        Idle,
        Starting,
        Active,
        Cancelling,
        Completed,
        Cancelled,
        Error,
    };
    Q_ENUM(OperationState)

    explicit HamDrmController(QObject* parent = nullptr);
    HamDrmController(HamDrmControllerConfig config,
                     HamDrmControllerBackends backends,
                     QObject* parent = nullptr);
    ~HamDrmController() override;

    HamDrmController(const HamDrmController&) = delete;
    HamDrmController& operator=(const HamDrmController&) = delete;

    QVariantList profiles() const;
    QString selectedProfileId() const;
    QVariantMap selectedProfile() const;
    void setSelectedProfileId(const QString& profileId);

    QVariantMap capabilities() const;
    QString capabilityMessage() const;
    bool waveformRxAvailable() const;
    bool waveformTxAvailable() const;
    bool jpeg2000DecodeAvailable() const;
    bool jpeg2000EncodeAvailable() const;
    bool partialResumeAvailable() const noexcept;

    OperationState rxState() const noexcept;
    OperationState txState() const noexcept;
    QString rxStateName() const;
    QString txStateName() const;
    double rxProgress() const noexcept;
    double txProgress() const noexcept;
    bool busy() const noexcept;
    QString error() const;

    QVariantList inbox() const;
    int selectedTransportId() const noexcept;
    QVariantList missingSegments() const;
    QString missingSegmentsText() const;
    bool canBuildBsr() const;
    QString bsrText() const;
    QVariantMap lastImageValidation() const;

    Q_INVOKABLE bool startRx();
    Q_INVOKABLE bool startTx(const QUrl& localImage);
    Q_INVOKABLE bool cancelRx();
    Q_INVOKABLE bool cancelTx();
    Q_INVOKABLE bool cancelAll();
    Q_INVOKABLE bool validateTxImage(const QUrl& localImage);
    Q_INVOKABLE bool selectObject(int transportId);
    Q_INVOKABLE QString buildBsr(int transportId,
                                 bool qsstvExtended = true);
    Q_INVOKABLE bool resumePartial(int transportId);
    Q_INVOKABLE bool discardObject(int transportId);
    Q_INVOKABLE void clearError();

    // Owner-thread handoff for an adapter that already has a complete MOT
    // data group.  RF callbacks normally enter through HamDrmWaveformRxSink.
    HamDrmStatus ingestMotGroup(const QByteArray& encodedGroup);
    HamDrmValueResult<HamDrmAssembledObject> takeCompletedObject(
        std::uint16_t transportId);

signals:
    void selectedProfileChanged();
    void operationStateChanged();
    void errorChanged();
    void inboxChanged();
    void selectedObjectChanged();
    void bsrChanged();
    void imageValidationChanged();
    // Emitted only after the waveform backend has accepted a validated TX
    // object. The image is an in-memory normalized snapshot; no caller path
    // crosses this boundary.
    void txImageAccepted(QImage image,
                         QString profileId,
                         int occupiedBandwidthHz);
    void objectCompleted(quint16 transportId, const QString& filename);
    void operationRejected(const QString& operation, const QString& detail);

private:
    struct InboxRecord;
    struct TxImageCandidate final
    {
        QByteArray bytes;
        HamDrmMotObjectMetadata metadata;
        HamDrmImageInfo imageInfo;
        QString canonicalPath;
        bool jpeg2000Decoded {false};
        QImage galleryImage;
    };

    static QVariantMap profileMap(const HamDrmProfile& profile);
    static QString operationStateName(OperationState state, bool receive);
    static QString imageFormatName(HamDrmImageFormat format);
    static bool activeState(OperationState state) noexcept;
    static bool validTransportId(int transportId) noexcept;

    void validateConfig();
    bool ownerThread() const noexcept;
    const HamDrmProfile* currentProfile() const noexcept;
    HamDrmWaveformCapability rxCapability() const;
    HamDrmWaveformCapability txCapability() const;
    HamDrmJpeg2000Capability jpegCapability() const;
    std::uint64_t nextSessionId() noexcept;

    void setRxState(OperationState state, double progress);
    void setTxState(OperationState state, double progress);
    bool reject(const QString& operation, const QString& detail);
    void setError(QString detail);
    QString bounded(QString detail) const;

    HamDrmValueResult<TxImageCandidate> readTxImage(
        const QUrl& localImage,
        bool requireJpeg2000Decode) const;
    QVariantMap imageValidationMap(const TxImageCandidate& candidate) const;
    HamDrmStatus updateHeaderMetadata(InboxRecord& record,
                                      const HamDrmMotDataGroup& group);
    HamDrmStatus rebuildRecordMetadata(InboxRecord& record);
    HamDrmStatus persistRecord(InboxRecord& record);
    HamDrmStatus finishRecord(InboxRecord& record);
    void publishInbox(std::uint16_t preferredTransportId);
    InboxRecord* recordFor(int transportId) noexcept;
    const InboxRecord* recordFor(int transportId) const noexcept;
    std::vector<std::uint16_t> selectedMissingSegments() const;

    void handleRxProgress(std::uint64_t sessionId, double progress);
    void handleRxMotGroup(std::uint64_t sessionId, QByteArray encodedGroup);
    void handleRxFinished(std::uint64_t sessionId, HamDrmStatus status);
    void handleTxProgress(std::uint64_t sessionId, double progress);
    void handleTxFinished(std::uint64_t sessionId, HamDrmStatus status);

    void hamDrmRxProgress(std::uint64_t sessionId,
                          double progress) override;
    void hamDrmRxMotGroup(std::uint64_t sessionId,
                          QByteArray encodedGroup) override;
    void hamDrmRxFinished(std::uint64_t sessionId,
                          HamDrmStatus status) override;
    void hamDrmTxProgress(std::uint64_t sessionId,
                          double progress) override;
    void hamDrmTxFinished(std::uint64_t sessionId,
                          HamDrmStatus status) override;

    HamDrmControllerConfig config_;
    HamDrmControllerBackends backends_;
    std::unique_ptr<HamDrmPartialStore> partialStore_;
    QVariantList profiles_;
    QString selectedProfileId_;
    OperationState rxState_ {OperationState::Unavailable};
    OperationState txState_ {OperationState::Unavailable};
    double rxProgress_ {0.0};
    double txProgress_ {0.0};
    QString error_;
    QString bsrText_;
    QVariantMap lastImageValidation_;
    std::map<std::uint16_t, std::unique_ptr<InboxRecord>> inbox_;
    std::size_t completedObjects_ {0U};
    std::size_t completedBytes_ {0U};
    int selectedTransportId_ {-1};
    std::uint64_t sessionSequence_ {0U};
    std::uint64_t rxSessionId_ {0U};
    std::uint64_t txSessionId_ {0U};
};

} // namespace decodium::sstv::hamdrm
