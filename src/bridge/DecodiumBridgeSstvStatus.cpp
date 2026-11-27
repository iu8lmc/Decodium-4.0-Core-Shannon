// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/integration/SstvStudioController.h"
#include "src/sstv/models/SstvGalleryModel.h"
#include "src/sstv/models/SstvThumbnailProvider.h"
#include "src/sstv/models/SstvShareController.h"
#include "src/sstv/diagnostics/SstvDiagnosticsController.h"
#if DECODIUM_HAS_HAMDRM
#include "src/sstv/digital/HamDrmController.h"
#endif
using decodium::sstv::SstvRxRuntime;
using decodium::sstv::SstvAudioSourceKind;
QString decodiumSstvSourceName(SstvAudioSourceKind);
QString decodiumSstvRuntimeStateName(SstvRxRuntime::State);
qulonglong decodiumSstvBoundedUnsigned(std::uint64_t);
#define sstvSourceName decodiumSstvSourceName
#define sstvRuntimeStateName decodiumSstvRuntimeStateName
#define boundedUnsigned decodiumSstvBoundedUnsigned
#endif

bool DecodiumBridge::sstvAvailable() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxRuntime != nullptr;
#else
    return false;
#endif
}

bool DecodiumBridge::sstvRxRequested() const
{
    return m_sstvRxRequested;
}

bool DecodiumBridge::sstvRxActive() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxRequested
        && m_sstvRxRuntime
        && m_sstvRxRuntime->state() == SstvRxRuntime::State::Running;
#else
    return false;
#endif
}

QString DecodiumBridge::sstvRxSource() const
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return QStringLiteral("Unavailable");
    }
    return sstvSourceName(m_sstvRxRuntime->routeToken().source.kind);
#else
    return QStringLiteral("Unavailable");
#endif
}

QString DecodiumBridge::sstvRxSourceDevice() const
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return {};
    }

    const auto kind = m_sstvRxRuntime->routeToken().source.kind;
    switch (kind) {
    case SstvAudioSourceKind::LocalSoundCard:
    case SstvAudioSourceKind::WebSdr: {
        // While a native session is live, report the QAudioDevice opened by
        // SoundInput. Before capture starts, report the configured selection.
        if (m_sstvRxRequested && !m_activeRxInputDeviceName.trimmed().isEmpty()) {
            return m_activeRxInputDeviceName.trimmed();
        }
        const QString configured = m_audioInputDevice.trimmed();
        return configured.isEmpty()
            ? tr("System default input") : configured;
    }
    case SstvAudioSourceKind::RtlSdr:
        return m_activeRxInputDeviceName.trimmed();
    case SstvAudioSourceKind::DecoPort:
    case SstvAudioSourceKind::Tci:
    case SstvAudioSourceKind::LegacyBackend:
    case SstvAudioSourceKind::Replay:
    case SstvAudioSourceKind::Unknown:
        break;
    }
#endif
    return {};
}

QString DecodiumBridge::sstvRxState() const
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return QStringLiteral("Unavailable");
    }
    return sstvRuntimeStateName(m_sstvRxRuntime->state());
#else
    return QStringLiteral("Unavailable");
#endif
}

QString DecodiumBridge::sstvDetectedMode() const
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return {};
    }
    const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
    return snapshot.vis.modeMapped ? snapshot.vis.mappedMode : QString();
#else
    return {};
#endif
}

QString DecodiumBridge::sstvRxImageSource() const
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return {};
    }
    const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
    if (!snapshot.image.available || snapshot.image.generation == 0U
        || snapshot.image.revision == 0U) {
        return {};
    }
    // A new URL only when the immutable pixel revision changes prevents
    // redundant provider work for telemetry-only snapshot notifications.
    return QStringLiteral("image://decodium-sstv/live/%1/%2")
        .arg(boundedUnsigned(snapshot.image.generation))
        .arg(boundedUnsigned(snapshot.image.revision));
#else
    return {};
#endif
}

std::shared_ptr<const decodium::sstv::SstvImageSnapshot>
DecodiumBridge::sstvRxImageSnapshot() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxRuntime
        ? m_sstvRxRuntime->latestImageSnapshot()
        : std::shared_ptr<const decodium::sstv::SstvImageSnapshot> {};
#else
    return {};
#endif
}

QObject* DecodiumBridge::sstvStudio() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvStudioController;
#else
    return nullptr;
#endif
}

QObject* DecodiumBridge::sstvGallery() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvGalleryModel;
#else
    return nullptr;
#endif
}

QObject* DecodiumBridge::sstvShare() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvShareController;
#else
    return nullptr;
#endif
}

QObject* DecodiumBridge::sstvDigital() const
{
#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
    return m_hamDrmController;
#else
    return nullptr;
#endif
}

QObject* DecodiumBridge::sstvDiagnostics() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvDiagnosticsController;
#else
    return nullptr;
#endif
}

#if DECODIUM_HAS_SSTV
void DecodiumBridge::setSstvThumbnailProvider(
    decodium::sstv::SstvThumbnailProvider* provider)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_sstvThumbnailProvider == provider) {
        return;
    }
    if (m_sstvGalleryModel) {
        m_sstvGalleryModel->setThumbnailProvider(nullptr);
    }
    m_sstvThumbnailProvider = provider;
    if (m_sstvGalleryModel) {
        m_sstvGalleryModel->setThumbnailProvider(provider);
    }
}
#endif

std::shared_ptr<const QImage>
DecodiumBridge::sstvTxSourceImageSnapshot() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvStudioController
        ? m_sstvStudioController->sourceSnapshot()
        : std::shared_ptr<const QImage> {};
#else
    return {};
#endif
}

std::shared_ptr<const QImage>
DecodiumBridge::sstvTxPreparedImageSnapshot() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvStudioController
        ? m_sstvStudioController->preparedSnapshot()
        : std::shared_ptr<const QImage> {};
#else
    return {};
#endif
}

std::uint64_t DecodiumBridge::sstvTxNowMs() const noexcept
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvTxClock.isValid()) {
        return 0U;
    }
    const qint64 elapsed = m_sstvTxClock.elapsed();
    return elapsed <= 0 ? 0U : static_cast<std::uint64_t>(elapsed);
#else
    return 0U;
#endif
}

