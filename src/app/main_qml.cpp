// main_qml.cpp - QML entry point for Decodium3

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDateTime>
#include <QEvent>
#include <QMetaType>
#include <QStyleFactory>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QIODevice>
#include <QJsonDocument>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#if DECODIUM_HAS_RTTY
#include "DecoRttyHost.h"
#endif
#if DECODIUM_HAS_SSTV
#include <QImage>
#include <QQuickImageProvider>
#endif
#include <QQuickGraphicsConfiguration>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QThread>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
#include <QLockFile>
#include <QList>
#include <QLocale>
#include <QWindow>
#if defined(Q_OS_LINUX) && QT_CONFIG(vulkan)
#include <QVulkanFunctions>
#include <QVulkanInstance>
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <cstdio>
#include <clocale>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Hybrid-GPU laptops inspect these exports before Qt creates its Direct3D
// device. They request the discrete adapter while preserving an explicit
// per-app choice made in Windows Graphics settings.
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#endif

#include "DecodiumBridge.h"
#include "DecodiumDiagnostics.h"
#include "DecodiumDxCluster.h"
#include "DecodiumLogging.hpp"
#include "DecodiumStorageMigration.hpp"
#include "DecodiumUpdater.hpp"
#include "DecodiumCommunityReport.hpp"
#include "MapIntelligenceService.h"
#include "controllers/FT2LinkQmlAdapter.hpp"
#include "L10nLoader.hpp"
#include "MetaDataRegistry.hpp"
#include "Radio.hpp"
#include "Configuration.hpp"
#include "WFPalette.hpp"
#include "models/FrequencyList.hpp"
#include "models/StationList.hpp"
#include "models/IARURegions.hpp"
#include "models/DecodeHighlightingModel.hpp"
#include "Transceiver/TransceiverFactory.hpp"
#include "WaterfallItem.hpp"
#include "PanadapterItem.hpp"
#include "WorldMapItem.hpp"
#include "WorldMapGpuItem.hpp"
#include "Detector/FftCompat.hpp"
#include "Detector/FT4DecodeWorker.hpp"
#include "Detector/FT8DecodeWorker.hpp"
#include "lib/init_random_seed.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/image/SstvImageFrame.h"
#include "src/sstv/integration/SstvStudioController.h"
#include "src/sstv/models/SstvThumbnailProvider.h"
#endif

static void L(const char* msg) {
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/decodium-start.log";
    FILE* f = fopen(logPath.toLocal8Bit().constData(), "a");
    if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
    std::fprintf(stderr, "%s\n", msg);
    DIAG_INFO(QString::fromLocal8Bit(msg));
}

static QString firstInstalledFontFamily(QStringList const& candidates)
{
    QStringList families = QFontDatabase::families();
    families.removeDuplicates();
    for (QString const& candidate : candidates) {
        QString const clean = candidate.trimmed();
        if (clean.isEmpty()) {
            continue;
        }
        for (QString const& family : families) {
            if (family.compare(clean, Qt::CaseInsensitive) == 0) {
                return family;
            }
        }
    }

    QString const systemFamily = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family().trimmed();
    if (!systemFamily.isEmpty()
        && systemFamily.compare(QStringLiteral("MS Shell Dlg 2"), Qt::CaseInsensitive) != 0
        && systemFamily.compare(QStringLiteral("MS Shell Dlg"), Qt::CaseInsensitive) != 0) {
        return systemFamily;
    }
    return families.isEmpty() ? QString{} : families.first();
}

#if DECODIUM_HAS_SSTV
// Qt Quick owns this provider through QQmlEngine.  ForceAsynchronousImageLoading
// guarantees that the coherent native RGB snapshot is copied into QImage away
// from the GUI thread.  DecodiumBridge is intentionally constructed before the
// engine and destroyed after it, so this non-owning pointer remains valid while
// an asynchronous image request can exist.
class DecodiumSstvImageProvider final : public QQuickImageProvider
{
public:
    explicit DecodiumSstvImageProvider(const DecodiumBridge* bridge)
        : QQuickImageProvider(QQuickImageProvider::Image,
                              QQuickImageProvider::ForceAsynchronousImageLoading)
        , m_bridge(bridge)
    {
    }

    QImage requestImage(const QString& id,
                        QSize* size,
                        const QSize& requestedSize) override
    {
        Q_UNUSED(requestedSize)

        if (size) {
            *size = {};
        }
        if (id.startsWith(QStringLiteral("tx-source/"))
            || id.startsWith(QStringLiteral("tx-prepared/"))
            || id.startsWith(QStringLiteral("tx-loopback/"))) {
            const bool prepared
                = id.startsWith(QStringLiteral("tx-prepared/"));
            const bool loopback
                = id.startsWith(QStringLiteral("tx-loopback/"));
            std::shared_ptr<const QImage> studioImage;
            if (m_bridge) {
                if (loopback) {
                    const auto* const studio = qobject_cast<
                        const decodium::sstv::SstvStudioController*>(
                            m_bridge->sstvStudio());
                    studioImage = studio ? studio->loopbackSnapshot()
                                         : std::shared_ptr<const QImage> {};
                } else {
                    studioImage = prepared
                        ? m_bridge->sstvTxPreparedImageSnapshot()
                        : m_bridge->sstvTxSourceImageSnapshot();
                }
            }
            if (!studioImage || studioImage->isNull()) {
                return {};
            }
            if (size) {
                *size = studioImage->size();
            }
            // QImage is implicitly shared.  The provider returns an immutable
            // snapshot without a deep copy; detach can only occur in a later
            // consumer that explicitly writes its local value.
            return *studioImage;
        }

        const auto snapshot = m_bridge
            ? m_bridge->sstvRxImageSnapshot()
            : std::shared_ptr<const decodium::sstv::SstvImageSnapshot> {};
        if (!snapshot || snapshot->width == 0U || snapshot->height == 0U
            || snapshot->width > decodium::sstv::SstvImageFrame::kMaximumDimension
            || snapshot->height > decodium::sstv::SstvImageFrame::kMaximumDimension) {
            return {};
        }

        const std::size_t width = snapshot->width;
        const std::size_t height = snapshot->height;
        if (width > decodium::sstv::SstvImageFrame::kMaximumPixels / height
            || snapshot->pixels.size() != width * height) {
            return {};
        }

        QImage image(static_cast<int>(snapshot->width),
                     static_cast<int>(snapshot->height),
                     QImage::Format_RGB888);
        if (image.isNull()) {
            return {};
        }
        // QImage rows may include alignment padding.  Initialise the complete
        // allocation before filling RGB pixels so the render upload never
        // observes indeterminate padding bytes.
        image.fill(Qt::black);

        for (std::size_t y = 0U; y < height; ++y) {
            uchar* destination = image.scanLine(static_cast<int>(y));
            const decodium::sstv::SstvRgbPixel* source =
                snapshot->pixels.data() + (y * width);
            for (std::size_t x = 0U; x < width; ++x) {
                destination[(x * 3U) + 0U] = source[x].red;
                destination[(x * 3U) + 1U] = source[x].green;
                destination[(x * 3U) + 2U] = source[x].blue;
            }
        }
        if (size) {
            *size = image.size();
        }
        return image;
    }

private:
    const DecodiumBridge* const m_bridge;
};
#endif

static std::atomic_bool g_shuttingDown {false};

// The render-thread QSG timings can tell us that the scene graph is waiting
// for the GUI thread, but not which GUI event occupied that thread.  Time the
// actual QApplication dispatch so a field log can name the receiver of a
// blocking timer, queued invocation, or update request.  This is deliberately
// limited to the application thread: worker-thread events are unrelated to
// event-loop responsiveness and would only add noise to the diagnostic log.
static const char* mainThreadEventTypeName(QEvent::Type type)
{
    // QQmlTimer posts a private/custom event instead of QEvent::Timer.
    if (type == QEvent::Type(QEvent::User + 1)) {
        return "QQmlTimer";
    }
    switch (type) {
    case QEvent::Timer: return "Timer";
    case QEvent::MetaCall: return "MetaCall";
    case QEvent::UpdateRequest: return "UpdateRequest";
    case QEvent::UpdateLater: return "UpdateLater";
    case QEvent::Polish: return "Polish";
    case QEvent::PolishRequest: return "PolishRequest";
    case QEvent::LayoutRequest: return "LayoutRequest";
    case QEvent::DeferredDelete: return "DeferredDelete";
    case QEvent::ChildAdded: return "ChildAdded";
    case QEvent::ChildRemoved: return "ChildRemoved";
    default: return "Other";
    }
}

class DecodiumApplication final : public QApplication
{
public:
    using QApplication::QApplication;

    bool notify(QObject* receiver, QEvent* event) override
    {
        if (!receiver || !event || QThread::currentThread() != thread()) {
            return QApplication::notify(receiver, event);
        }

        constexpr qint64 kSlowMainDispatchMs = 90;
        QString const receiverClass = QString::fromLatin1(receiver->metaObject()->className());
        QString const receiverName = receiver->objectName();
        QEvent::Type const eventType = event->type();
        QElapsedTimer timer;
        timer.start();
        bool const handled = QApplication::notify(receiver, event);
        qint64 const elapsedMs = timer.elapsed();
        if (elapsedMs >= kSlowMainDispatchMs) {
            QString qmlSource;
            if (receiverClass == QStringLiteral("QQmlTimer")) {
                if (QQmlContext* context = qmlContext(receiver)) {
                    qmlSource = context->baseUrl().toString();
                }
            }
            qWarning().noquote()
                << "[MAINDISPATCH] slow_event"
                << "elapsed_ms=" << elapsedMs
                << "event=" << mainThreadEventTypeName(eventType)
                << "event_type=" << static_cast<int>(eventType)
                << "receiver=" << receiverClass
                << "object=" << (receiverName.isEmpty() ? QStringLiteral("<unnamed>") : receiverName)
                << "qml_source=" << (qmlSource.isEmpty() ? QStringLiteral("<unknown>") : qmlSource);
        }
        return handled;
    }
};

#ifdef Q_OS_WIN
static std::atomic_bool g_windowsD3d12DeviceFailed {false};
#endif

static void writeStartupLogLine(const QByteArray& logPath, const QByteArray& msg)
{
    FILE* f = fopen(logPath.constData(), "a");
    if (f) {
        fputs(msg.constData(), f);
        fputc('\n', f);
        fclose(f);
    }
    std::fprintf(stderr, "%s\n", msg.constData());
}

static void logEnvVar(const char* name)
{
    QByteArray line(name);
    line += "=";
    if (qEnvironmentVariableIsSet(name)) {
        line += qgetenv(name);
    } else {
        line += "<unset>";
    }
    L(line.constData());
}

static const char* qsgGraphicsApiName(QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Unknown: return "Unknown";
    case QSGRendererInterface::Software: return "Software";
    case QSGRendererInterface::OpenVG: return "OpenVG";
    case QSGRendererInterface::OpenGL: return "OpenGL";
    case QSGRendererInterface::Direct3D11: return "Direct3D11";
    case QSGRendererInterface::Vulkan: return "Vulkan";
    case QSGRendererInterface::Metal: return "Metal";
    case QSGRendererInterface::Null: return "Null";
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
    case QSGRendererInterface::Direct3D12: return "Direct3D12";
#endif
    default: return "Unrecognized";
    }
}

static QQuickWindow* firstQuickWindow(QQmlApplicationEngine& engine)
{
    for (QObject* root : engine.rootObjects()) {
        if (auto* quickWindow = qobject_cast<QQuickWindow*>(root)) {
            return quickWindow;
        }
        for (QObject* child : root->findChildren<QObject*>()) {
            if (auto* quickWindow = qobject_cast<QQuickWindow*>(child)) {
                return quickWindow;
            }
        }
    }
    return nullptr;
}

static void logQtQuickGraphicsApi(QQuickWindow* window, const char* context)
{
    QByteArray line("Qt Quick graphics API");
    if (context && *context) {
        line += " (";
        line += context;
        line += ")";
    }
    line += ": ";
    if (!window || !window->rendererInterface()) {
        line += "<no QQuickWindow>";
        L(line.constData());
        return;
    }

    QSGRendererInterface::GraphicsApi const api = window->rendererInterface()->graphicsApi();
    line += qsgGraphicsApiName(api);
    if (QSGRendererInterface::isApiRhiBased(api)) {
        line += " / RHI";
    }
    L(line.constData());
}

static void logFirstQuickWindowGraphicsApi(QQmlApplicationEngine& engine, const char* context)
{
    logQtQuickGraphicsApi(firstQuickWindow(engine), context);
}

#if defined(Q_OS_LINUX) && QT_CONFIG(vulkan)
struct LinuxVulkanGpuCandidate
{
    uint32_t index = 0;
    QByteArray name;
    VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    quint64 localMemoryBytes = 0;
    uint32_t apiVersion = 0;
    bool hasGraphicsComputePresentQueue = false;
    bool hasSwapchain = false;

    bool eligible() const
    {
        return type != VK_PHYSICAL_DEVICE_TYPE_CPU
            && hasGraphicsComputePresentQueue
            && hasSwapchain;
    }
};

static const char* linuxVulkanDeviceTypeName(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "dedicated";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "software";
    case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "other";
    default: return "unknown";
    }
}

static int linuxVulkanDevicePriority(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 400;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 300;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 200;
    case VK_PHYSICAL_DEVICE_TYPE_OTHER: return 100;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return 0;
    default: return 0;
    }
}

static quint64 linuxVulkanLocalMemoryBytes(QVulkanFunctions* functions,
                                           VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceMemoryProperties memoryProperties {};
    functions->vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    quint64 bytes = 0;
    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i) {
        if (memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            bytes += memoryProperties.memoryHeaps[i].size;
    }
    return bytes;
}

static bool linuxVulkanHasSwapchain(QVulkanFunctions* functions,
                                    VkPhysicalDevice physicalDevice)
{
    uint32_t extensionCount = 0;
    if (functions->vkEnumerateDeviceExtensionProperties(
            physicalDevice, nullptr, &extensionCount, nullptr) != VK_SUCCESS
        || extensionCount == 0) {
        return false;
    }

    std::vector<VkExtensionProperties> extensions(extensionCount);
    if (functions->vkEnumerateDeviceExtensionProperties(
            physicalDevice, nullptr, &extensionCount, extensions.data()) != VK_SUCCESS) {
        return false;
    }
    return std::any_of(extensions.cbegin(), extensions.cend(), [](VkExtensionProperties const& extension) {
        return std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
    });
}

static bool linuxVulkanHasGraphicsComputePresentQueue(QVulkanInstance& instance,
                                                       QVulkanFunctions* functions,
                                                       VkPhysicalDevice physicalDevice,
                                                       QWindow* probeWindow)
{
    uint32_t queueCount = 0;
    functions->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);
    if (queueCount == 0)
        return false;

    std::vector<VkQueueFamilyProperties> queues(queueCount);
    functions->vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice, &queueCount, queues.data());
    for (uint32_t i = 0; i < queueCount; ++i) {
        VkQueueFlags const required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        if ((queues[i].queueFlags & required) == required
            && instance.supportsPresent(physicalDevice, i, probeWindow)) {
            return true;
        }
    }
    return false;
}

static void configureLinuxVulkanGpuSelection()
{
    QByteArray const backend = qgetenv("QSG_RHI_BACKEND").trimmed().toLower();
    QByteArray const quickBackend = qgetenv("QT_QUICK_BACKEND").trimmed().toLower();
    bool const explicitVulkan = backend == QByteArrayLiteral("vulkan");

    if (qEnvironmentVariableIntValue("DECODIUM_DISABLE_LINUX_GPU_AUTOSELECT") != 0) {
        L("[GPUSEL] Linux GPU auto-selection disabled by environment");
        return;
    }
    if (qEnvironmentVariableIsSet("QT_VK_PHYSICAL_DEVICE_INDEX")) {
        L((QByteArray("[GPUSEL] Vulkan physical device index supplied by environment: ")
           + qgetenv("QT_VK_PHYSICAL_DEVICE_INDEX")).constData());
        return;
    }
    if (!quickBackend.isEmpty() || (!backend.isEmpty() && !explicitVulkan)) {
        L("[GPUSEL] Linux Vulkan GPU auto-selection skipped: another graphics backend was requested");
        return;
    }

    QVulkanInstance instance;
    if (!instance.create()) {
        L((QByteArray("[GPUSEL] Vulkan probe unavailable; preserving Qt Linux fallback, error=")
           + QByteArray::number(instance.errorCode())).constData());
        return;
    }
    QVulkanFunctions* functions = instance.functions();
    if (!functions) {
        L("[GPUSEL] Vulkan probe has no instance functions; preserving Qt Linux fallback");
        return;
    }

    uint32_t physicalDeviceCount = 0;
    if (functions->vkEnumeratePhysicalDevices(
            instance.vkInstance(), &physicalDeviceCount, nullptr) != VK_SUCCESS
        || physicalDeviceCount == 0) {
        L("[GPUSEL] No Vulkan physical device found; preserving Qt Linux fallback");
        return;
    }

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    if (functions->vkEnumeratePhysicalDevices(
            instance.vkInstance(), &physicalDeviceCount, physicalDevices.data()) != VK_SUCCESS) {
        L("[GPUSEL] Vulkan device enumeration failed; preserving Qt Linux fallback");
        return;
    }

    QWindow probeWindow;
    probeWindow.setSurfaceType(QSurface::VulkanSurface);
    probeWindow.setVulkanInstance(&instance);
    probeWindow.create();

    std::vector<LinuxVulkanGpuCandidate> candidates;
    candidates.reserve(physicalDeviceCount);
    bool sawDedicatedDevice = false;
    for (uint32_t i = 0; i < physicalDeviceCount; ++i) {
        VkPhysicalDeviceProperties properties {};
        functions->vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);

        LinuxVulkanGpuCandidate candidate;
        candidate.index = i;
        candidate.name = QByteArray(properties.deviceName);
        candidate.type = properties.deviceType;
        candidate.apiVersion = properties.apiVersion;
        candidate.localMemoryBytes = linuxVulkanLocalMemoryBytes(functions, physicalDevices[i]);
        candidate.hasSwapchain = linuxVulkanHasSwapchain(functions, physicalDevices[i]);
        candidate.hasGraphicsComputePresentQueue =
            linuxVulkanHasGraphicsComputePresentQueue(
                instance, functions, physicalDevices[i], &probeWindow);
        sawDedicatedDevice = sawDedicatedDevice
            || candidate.type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

        QByteArray message("[GPUSEL] Vulkan device");
        message += " index=" + QByteArray::number(candidate.index);
        message += " name=" + candidate.name;
        message += " type=" + QByteArray(linuxVulkanDeviceTypeName(candidate.type));
        message += " api=" + QByteArray::number(VK_VERSION_MAJOR(candidate.apiVersion));
        message += "." + QByteArray::number(VK_VERSION_MINOR(candidate.apiVersion));
        message += "." + QByteArray::number(VK_VERSION_PATCH(candidate.apiVersion));
        message += " local_mem_mib=" + QByteArray::number(candidate.localMemoryBytes / (1024 * 1024));
        message += " graphics_compute_present="
            + QByteArray::number(candidate.hasGraphicsComputePresentQueue ? 1 : 0);
        message += " swapchain=" + QByteArray::number(candidate.hasSwapchain ? 1 : 0);
        message += " eligible=" + QByteArray::number(candidate.eligible() ? 1 : 0);
        L(message.constData());
        candidates.push_back(std::move(candidate));
    }

    auto betterCandidate = [](LinuxVulkanGpuCandidate const& left,
                              LinuxVulkanGpuCandidate const& right) {
        if (left.eligible() != right.eligible())
            return !left.eligible();
        int const leftPriority = linuxVulkanDevicePriority(left.type);
        int const rightPriority = linuxVulkanDevicePriority(right.type);
        if (leftPriority != rightPriority)
            return leftPriority < rightPriority;
        return left.localMemoryBytes < right.localMemoryBytes;
    };
    auto best = std::max_element(candidates.cbegin(), candidates.cend(), betterCandidate);
    if (best == candidates.cend() || !best->eligible()) {
        L("[GPUSEL] No eligible hardware Vulkan device; preserving Qt Linux fallback");
        return;
    }

    // Do not force Vulkan on ordinary single/iGPU Linux systems. Hybrid systems
    // switch to Vulkan so Qt can address the dedicated adapter explicitly. If a
    // dedicated adapter was enumerated but did not qualify, the sorted choice is
    // the integrated hardware fallback. An explicit Vulkan request always gets
    // the best eligible hardware device.
    bool const bestIsDedicated = best->type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    if (!explicitVulkan && !bestIsDedicated && !sawDedicatedDevice) {
        L("[GPUSEL] No dedicated Vulkan GPU found; preserving Qt automatic integrated-GPU path");
        return;
    }

    qputenv("QT_VK_PHYSICAL_DEVICE_INDEX", QByteArray::number(best->index));
    if (backend.isEmpty())
        qputenv("QSG_RHI_BACKEND", "vulkan");

    QByteArray selected("[GPUSEL] Selected Linux GPU");
    selected += " index=" + QByteArray::number(best->index);
    selected += " name=" + best->name;
    selected += " type=" + QByteArray(linuxVulkanDeviceTypeName(best->type));
    selected += bestIsDedicated ? " policy=prefer_dedicated" : " policy=integrated_fallback";
    selected += " backend=vulkan";
    L(selected.constData());
}
#elif defined(Q_OS_LINUX)
static void configureLinuxVulkanGpuSelection()
{
    L("[GPUSEL] This Qt build has no Vulkan support; preserving Qt Linux fallback");
}
#endif

static void installMainThreadWatchdog(QObject* parent, DecodiumBridge* bridge)
{
    constexpr int kIntervalMs = 25;
    constexpr qint64 kStallThresholdMs = 90;
    constexpr qint64 kAdaptiveStartupGraceMs = 15000;

    auto* timer = new QTimer(parent);
    timer->setObjectName(QStringLiteral("decodiumMainThreadWatchdog"));
    timer->setTimerType(Qt::PreciseTimer);
    timer->setInterval(kIntervalMs);

    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();
    auto lastNs = std::make_shared<qint64>(clock->nsecsElapsed());
    auto stallCount = std::make_shared<int>(0);
    auto metricLastLogMs = std::make_shared<qint64>(clock->elapsed());
    auto metricSamples = std::make_shared<int>(0);
    auto metricAccumMs = std::make_shared<qint64>(0);
    auto metricMaxMs = std::make_shared<qint64>(0);

    QObject::connect(timer, &QTimer::timeout, parent,
                     [clock,
                      lastNs,
                      stallCount,
                      metricLastLogMs,
                      metricSamples,
                      metricAccumMs,
                      metricMaxMs,
                      bridge]() {
        qint64 const nowNs = clock->nsecsElapsed();
        qint64 const nowMs = clock->elapsed();
        qint64 const deltaMs = (nowNs - *lastNs) / 1000000;
        *lastNs = nowNs;
        if (deltaMs < kStallThresholdMs)
            return;

        if (bridge && nowMs >= kAdaptiveStartupGraceMs) {
            bridge->noteMainThreadMicroStall(deltaMs);
        }

        ++(*stallCount);
        ++(*metricSamples);
        *metricAccumMs += deltaMs;
        *metricMaxMs = qMax(*metricMaxMs, deltaMs);

        if (nowMs - *metricLastLogMs < 10000)
            return;

        qInfo().noquote()
            << "[MAINWATCH] event_loop_stalls"
            << "samples=" << *metricSamples
            << "avg_ms=" << (*metricSamples > 0 ? *metricAccumMs / *metricSamples : 0)
            << "max_ms=" << *metricMaxMs
            << "threshold_ms=" << kStallThresholdMs
            << "stalls_total=" << *stallCount
            << "monitoring=" << (bridge && bridge->monitoring() ? 1 : 0)
            << "tx=" << (bridge && bridge->transmitting() ? 1 : 0)
            << "tune=" << (bridge && bridge->tuning() ? 1 : 0)
            << "spectrum_visible=" << (bridge && bridge->spectrumVisible() ? 1 : 0)
            << "fps_cap=" << (bridge ? bridge->spectrumFpsCap() : -1);
        *metricLastLogMs = nowMs;
        *metricSamples = 0;
        *metricAccumMs = 0;
        *metricMaxMs = 0;
    });

    timer->start();
    qInfo().noquote()
        << "[MAINWATCH] event loop watchdog active"
        << "interval_ms=" << kIntervalMs
        << "threshold_ms=" << kStallThresholdMs
        << "adaptive_grace_ms=" << kAdaptiveStartupGraceMs;
}

#ifdef Q_OS_WIN
static bool hasCommandLineSwitch(int argc, char* argv[], const char* name)
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QLatin1String(name))
            return true;
    }
    return false;
}

static QString slowQmlStartupFlagPath()
{
    return QDir {QStandardPaths::writableLocation(QStandardPaths::TempLocation)}
        .absoluteFilePath(QStringLiteral("decodium-slow-qml-startup.flag"));
}

static QString graphicsStartupPendingFlagPath()
{
    return QDir {QStandardPaths::writableLocation(QStandardPaths::TempLocation)}
        .absoluteFilePath(QStringLiteral("decodium-gpu-startup-pending.flag"));
}

static QString persistentD3d11FallbackKey()
{
    return QStringLiteral("Graphics/PersistentD3D11FallbackAfterD3D12Failure");
}

static QString persistentD3d11FallbackReasonKey()
{
    return QStringLiteral("Graphics/PersistentD3D11FallbackReason");
}

static QString persistentD3d11FallbackUtcKey()
{
    return QStringLiteral("Graphics/PersistentD3D11FallbackUtc");
}

static QString persistentGraphicsFallbackModeKey()
{
    return QStringLiteral("Graphics/PersistentFallbackMode");
}

static void writePersistentD3d11Fallback(const QString& reason)
{
    QSettings settings {QSettings::IniFormat, QSettings::UserScope,
                        QString::fromLatin1(DecodiumStorageMigration::organizationName()),
                        QStringLiteral("Decodium")};
    settings.setValue(persistentD3d11FallbackKey(), true);
    settings.setValue(persistentD3d11FallbackReasonKey(), reason);
    settings.setValue(persistentD3d11FallbackUtcKey(),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    settings.setValue(persistentGraphicsFallbackModeKey(), QStringLiteral("d3d11"));
    settings.sync();
}

static QByteArray persistentGraphicsFallbackMode()
{
    QSettings settings {QSettings::IniFormat, QSettings::UserScope,
                        QString::fromLatin1(DecodiumStorageMigration::organizationName()),
                        QStringLiteral("Decodium")};
    QByteArray mode = settings.value(persistentGraphicsFallbackModeKey()).toByteArray().trimmed().toLower();
    if (mode.isEmpty() && settings.value(persistentD3d11FallbackKey(), false).toBool())
        mode = QByteArrayLiteral("d3d11");
    if (mode != QByteArrayLiteral("d3d11")
        && mode != QByteArrayLiteral("warp")
        && mode != QByteArrayLiteral("software")) {
        mode.clear();
    }
    return mode;
}

static void writePersistentGraphicsFallback(const QByteArray& mode, const QString& reason)
{
    QSettings settings {QSettings::IniFormat, QSettings::UserScope,
                        QString::fromLatin1(DecodiumStorageMigration::organizationName()),
                        QStringLiteral("Decodium")};
    settings.setValue(persistentGraphicsFallbackModeKey(), QString::fromLatin1(mode));
    settings.setValue(persistentD3d11FallbackKey(), mode == QByteArrayLiteral("d3d11"));
    settings.setValue(persistentD3d11FallbackReasonKey(), reason);
    settings.setValue(persistentD3d11FallbackUtcKey(),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    settings.sync();
}

static void clearPersistentD3d11Fallback()
{
    QSettings settings {QSettings::IniFormat, QSettings::UserScope,
                        QString::fromLatin1(DecodiumStorageMigration::organizationName()),
                        QStringLiteral("Decodium")};
    settings.remove(persistentD3d11FallbackKey());
    settings.remove(persistentD3d11FallbackReasonKey());
    settings.remove(persistentD3d11FallbackUtcKey());
    settings.remove(persistentGraphicsFallbackModeKey());
    settings.sync();
}

static QString startupLogPath()
{
    return QDir {QStandardPaths::writableLocation(QStandardPaths::TempLocation)}
        .absoluteFilePath(QStringLiteral("decodium-start.log"));
}

static QDateTime startupLogTimestamp(const QString& line)
{
    static QRegularExpression const timestampPattern {
        QStringLiteral(R"(^\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})\])")
    };
    auto const match = timestampPattern.match(line);
    if (!match.hasMatch()) {
        return {};
    }
    QDateTime ts = QDateTime::fromString(match.captured(1),
                                         QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    if (ts.isValid()) {
        ts.setTimeSpec(Qt::LocalTime);
    }
    return ts;
}

static bool previousStartupLogShowsSlowQml()
{
    QFile file {startupLogPath()};
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    qint64 constexpr maxBytesToInspect = 2LL * 1024LL * 1024LL;
    if (file.size() > maxBytesToInspect) {
        file.seek(file.size() - maxBytesToInspect);
    }
    QString recent = QString::fromLocal8Bit(file.readAll());
    // Inspect only the previous launch. main() has already logged a fresh
    // "main() START" for this process, so older watchdog lines must not keep
    // Windows trapped in safe graphics forever.
    QString const startMarker = QStringLiteral("main() START");
    int const currentStart = recent.lastIndexOf(startMarker);
    if (currentStart > 0) {
        int const previousStart = recent.lastIndexOf(startMarker, currentStart - 1);
        recent = previousStart >= 0
            ? recent.mid(previousStart, currentStart - previousStart)
            : recent.left(currentStart);
    }
    if (recent.contains(QRegularExpression(QStringLiteral(
            R"(QML LOAD WATCHDOG: .* still running after ([6-9]\d|[1-9]\d{2,}) s)")))) {
        return true;
    }
    if (recent.contains(QRegularExpression(QStringLiteral(
            R"(BootLoader watchdog: Main\.qml still loading after ([1-9]\d+) s)")))) {
        return true;
    }

    QRegularExpression const qmlLoadedMs {
        QStringLiteral(R"(Main\.qml (?:loaded OK|created as top-level window).* in ([1-9]\d{4,}) ms)")
    };
    auto it = qmlLoadedMs.globalMatch(recent);
    while (it.hasNext()) {
        auto const match = it.next();
        if (match.captured(1).toLongLong() >= 30000) {
            return true;
        }
    }

    QDateTime mainLoadStarted;
    for (QString const& line : recent.split(QLatin1Char('\n'))) {
        QDateTime const ts = startupLogTimestamp(line);
        if (!ts.isValid()) {
            continue;
        }
        if (line.contains(QStringLiteral("BootLoader: starting Main.qml load"))) {
            mainLoadStarted = ts;
            continue;
        }
        if (!mainLoadStarted.isValid()) {
            continue;
        }
        bool const mainReady =
            line.contains(QStringLiteral("Main.qml startup +0 ms"))
            || line.contains(QStringLiteral("Main.qml Component.onCompleted"))
            || line.contains(QStringLiteral("BootLoader: Loader status = 1 ready"))
            || line.contains(QStringLiteral("BootLoader: Main.qml loaded OK"))
            || line.contains(QStringLiteral("BootLoader: Main.qml created as top-level window"));
        if (mainReady && mainLoadStarted.msecsTo(ts) >= 30000) {
            return true;
        }
    }

    return recent.contains(QStringLiteral("Main.qml async load watchdog wrote safe graphics marker"));
}

static void writeSlowQmlStartupFlag(const QByteArray& flagPath, const QByteArray& reason)
{
    QFile flagFile {QString::fromLocal8Bit(flagPath)};
    if (!flagFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    flagFile.write(reason);
    flagFile.write("\n");
}

static QByteArray readSmallTextFile(const QString& path, qint64 maxBytes = 4096)
{
    QFile file {path};
    if (!file.open(QIODevice::ReadOnly))
        return {};

    return file.read(maxBytes);
}

static QByteArray graphicsStartupFlagBackend(const QString& path)
{
    QByteArray const content = readSmallTextFile(path).trimmed().toLower();
    for (QByteArray const& line : content.split('\n')) {
        QByteArray trimmed = line.trimmed();
        if (trimmed.startsWith("backend="))
            return trimmed.mid(int(sizeof("backend=") - 1)).trimmed();
    }
    return {};
}

static void writeWindowsGraphicsStartupFlag(const QString& flagPath,
                                            const QByteArray& backend,
                                            const QByteArray& reason)
{
    QFile flagFile {flagPath};
    if (!flagFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    if (!backend.isEmpty()) {
        flagFile.write("backend=");
        flagFile.write(backend);
        flagFile.write("\n");
    }
    flagFile.write("reason=");
    flagFile.write(reason);
    flagFile.write("\n");
}

static bool runWindowsGraphicsStartupSupervisor(int argc, char* argv[], int *exitCode)
{
    if (hasCommandLineSwitch(argc, argv, "--decodium-graphics-worker")
        || hasCommandLineSwitch(argc, argv, "--help")
        || hasCommandLineSwitch(argc, argv, "-h")
        || hasCommandLineSwitch(argc, argv, "--version")) {
        return false;
    }

    QString const markerPath = graphicsStartupPendingFlagPath();
    if (hasCommandLineSwitch(argc, argv, "--reset-safe-graphics"))
        QFile::remove(markerPath);
    std::wstring const baseCommandLine = GetCommandLineW();
    constexpr int kMaxAttempts = 4;

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        std::wstring commandLine = baseCommandLine;
        commandLine += L" --decodium-graphics-worker";
        if (attempt > 0)
            commandLine += L" --decodium-graphics-recovery";
        std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
        mutableCommandLine.push_back(L'\0');

        STARTUPINFOW startupInfo {};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo {};
        BOOL const created = CreateProcessW(nullptr,
                                            mutableCommandLine.data(),
                                            nullptr,
                                            nullptr,
                                            TRUE,
                                            0,
                                            nullptr,
                                            nullptr,
                                            &startupInfo,
                                            &processInfo);
        if (!created) {
            L((QByteArray("Windows graphics supervisor could not launch worker, error=")
               + QByteArray::number(GetLastError())).constData());
            return false;
        }

        CloseHandle(processInfo.hThread);
        bool markerObserved = QFile::exists(markerPath);
        for (;;) {
            DWORD const waitResult = WaitForSingleObject(processInfo.hProcess, 250);
            bool const markerExists = QFile::exists(markerPath);
            markerObserved = markerObserved || markerExists;

            if (waitResult == WAIT_OBJECT_0) {
                DWORD workerExitCode = 1;
                GetExitCodeProcess(processInfo.hProcess, &workerExitCode);
                CloseHandle(processInfo.hProcess);

                QByteArray const failedBackend = markerExists
                    ? graphicsStartupFlagBackend(markerPath)
                    : QByteArray {};
                bool const canRetry = workerExitCode != 0
                    && markerExists
                    && failedBackend != QByteArrayLiteral("software")
                    && attempt + 1 < kMaxAttempts;
                if (canRetry) {
                    L((QByteArray("Windows graphics startup failed with backend=")
                       + failedBackend
                       + "; supervisor is retrying the next fallback").constData());
                    break;
                }

                if (workerExitCode != 0
                    && markerExists
                    && failedBackend == QByteArrayLiteral("software")) {
                    MessageBoxW(nullptr,
                                L"Decodium could not initialize even the Qt software renderer. "
                                L"Please reinstall Decodium and send decodium-start.log to support.",
                                L"Decodium - Graphics startup error",
                                MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                }
                *exitCode = static_cast<int>(workerExitCode);
                return true;
            }

            if (waitResult == WAIT_FAILED) {
                CloseHandle(processInfo.hProcess);
                L((QByteArray("Windows graphics supervisor wait failed, error=")
                   + QByteArray::number(GetLastError())).constData());
                return false;
            }

            if (markerObserved && !markerExists) {
                CloseHandle(processInfo.hProcess);
                *exitCode = 0;
                return true;
            }
        }
    }

    *exitCode = -1;
    return true;
}

static void removeFileIfExists(const QString& path)
{
    if (QFile::exists(path))
        QFile::remove(path);
}

static QString sanitizedCacheComponent(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("_"));
    value = value.trimmed();
    return value.isEmpty() ? QStringLiteral("default") : value;
}

static QByteArray windowsQmlContentFingerprint(QString *qmlRootPath = nullptr)
{
    QString const qmlRoot = QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("qml"));
    if (qmlRootPath)
        *qmlRootPath = qmlRoot;

    QDir const rootDir {qmlRoot};
    if (!rootDir.exists())
        return QCryptographicHash::hash(
            QByteArrayLiteral("missing-qml-root\n") + qmlRoot.toUtf8(),
            QCryptographicHash::Sha256).toHex().left(16);

    QStringList qmlFiles;
    QDirIterator iterator(
        qmlRoot,
        QStringList {
            QStringLiteral("*.qml"),
            QStringLiteral("*.js"),
            QStringLiteral("*.mjs"),
            QStringLiteral("qmldir"),
            QStringLiteral("*.qmltypes")
        },
        QDir::Files | QDir::Readable,
        QDirIterator::Subdirectories);
    while (iterator.hasNext())
        qmlFiles.push_back(iterator.next());

    std::sort(qmlFiles.begin(), qmlFiles.end(),
              [&rootDir](const QString& lhs, const QString& rhs) {
                  return rootDir.relativeFilePath(lhs)
                      .compare(rootDir.relativeFilePath(rhs), Qt::CaseSensitive) < 0;
              });

    QCryptographicHash hash {QCryptographicHash::Sha256};
    for (const QString& filePath : qmlFiles) {
        QString relativePath = rootDir.relativeFilePath(filePath);
        relativePath.replace('\\', '/');
        hash.addData(relativePath.toUtf8());
        hash.addData(QByteArrayLiteral("\n"));

        QFile file {filePath};
        if (!file.open(QIODevice::ReadOnly)) {
            hash.addData(QByteArrayLiteral("<unreadable>\n"));
            continue;
        }
        while (!file.atEnd())
            hash.addData(file.read(64 * 1024));
        hash.addData(QByteArrayLiteral("\n"));
    }
    return hash.result().toHex().left(16);
}

static QString windowsQmlDiskCachePath(const QString& configName,
                                       QByteArray *qmlFingerprint = nullptr,
                                       QString *qmlRootPath = nullptr)
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (basePath.isEmpty()) {
        basePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }

    QByteArray const contentFingerprint = windowsQmlContentFingerprint(qmlRootPath);
    if (qmlFingerprint)
        *qmlFingerprint = contentFingerprint;

    QByteArray cacheSeed;
    cacheSeed += QDir::cleanPath(QCoreApplication::applicationDirPath()).toUtf8();
    cacheSeed += '\n';
    cacheSeed += QByteArray(FORK_RELEASE_VERSION);
    cacheSeed += '\n';
    cacheSeed += qVersion();
    cacheSeed += '\n';
    cacheSeed += QSysInfo::buildAbi().toUtf8();
    cacheSeed += '\n';
    cacheSeed += contentFingerprint;
    if (!configName.isEmpty()) {
        cacheSeed += '\n';
        cacheSeed += configName.toUtf8();
    }

    QString const installHash = QString::fromLatin1(
        QCryptographicHash::hash(cacheSeed, QCryptographicHash::Sha256).toHex().left(16));
    QString const versionComponent = sanitizedCacheComponent(QStringLiteral(FORK_RELEASE_VERSION));
    return QDir {basePath}.absoluteFilePath(
        QStringLiteral("qmlcache/%1/%2").arg(versionComponent, installHash));
}

static void configureWindowsQmlDiskCache(const QString& configName)
{
    if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_QML_CACHE")
        || qEnvironmentVariableIsSet("QML_DISABLE_DISK_CACHE")) {
        return;
    }

    if (qEnvironmentVariableIsSet("QML_DISK_CACHE_PATH")) {
        QByteArray msg("QML disk cache path from environment: ");
        msg += qgetenv("QML_DISK_CACHE_PATH");
        L(msg.constData());
        return;
    }

    QByteArray qmlFingerprint;
    QString qmlRootPath;
    QString const cachePath = windowsQmlDiskCachePath(
        configName, &qmlFingerprint, &qmlRootPath);
    if (!QDir().mkpath(cachePath)) {
        L(("QML disk cache path could not be created: " + cachePath.toLocal8Bit()).constData());
        return;
    }

    qputenv("QML_DISK_CACHE_PATH", QDir::toNativeSeparators(cachePath).toLocal8Bit());
    L(("QML content fingerprint: " + qmlFingerprint
       + " root=" + QDir::toNativeSeparators(qmlRootPath).toLocal8Bit()).constData());
    L(("QML disk cache path isolated: " + cachePath.toLocal8Bit()).constData());
}
#endif

#ifdef Q_OS_WIN
static QString g_windowsIconFilePath;
static HICON g_windowsIconSmall {nullptr};
static HICON g_windowsIconBig {nullptr};
#endif

static QIcon loadDecodiumApplicationIcon()
{
#ifdef Q_OS_WIN
    QStringList const candidates {
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("decodium.ico")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("icons/windows-icons/decodium.ico"))
    };
    for (QString const& candidate : candidates) {
        if (!QFileInfo::exists(candidate))
            continue;
        QIcon icon(candidate);
        if (!icon.isNull()) {
            g_windowsIconFilePath = candidate;
            return icon;
        }
    }
#endif

    return QIcon(QStringLiteral(":/icon_128x128.png"));
}

#ifdef Q_OS_WIN
static HICON loadWindowsIconFromFile(int width, int height)
{
    if (g_windowsIconFilePath.isEmpty())
        return nullptr;

    return static_cast<HICON>(LoadImageW(nullptr,
                                         reinterpret_cast<LPCWSTR>(g_windowsIconFilePath.utf16()),
                                         IMAGE_ICON,
                                         width,
                                         height,
                                         LR_LOADFROMFILE));
}

static void applyApplicationIconToTopLevelWindows(QIcon const& icon)
{
    if (icon.isNull())
        return;

    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        if (!window)
            continue;

        window->setIcon(icon);

        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        if (!hwnd)
            continue;

        if (!g_windowsIconSmall) {
            g_windowsIconSmall = loadWindowsIconFromFile(GetSystemMetrics(SM_CXSMICON),
                                                         GetSystemMetrics(SM_CYSMICON));
        }
        if (!g_windowsIconBig) {
            g_windowsIconBig = loadWindowsIconFromFile(GetSystemMetrics(SM_CXICON),
                                                       GetSystemMetrics(SM_CYICON));
        }
        if (g_windowsIconSmall) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_windowsIconSmall));
        }
        if (g_windowsIconBig) {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_windowsIconBig));
        }
    }
}

static void setWindowsAppUserModelId()
{
    using SetAppIdFn = HRESULT (WINAPI *)(PCWSTR);
    HMODULE shell32 = LoadLibraryW(L"shell32.dll");
    if (!shell32)
        return;

    FARPROC proc = GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID");
    if (proc) {
        SetAppIdFn setAppId = nullptr;
        static_assert(sizeof(setAppId) == sizeof(proc),
                      "Unexpected function pointer size");
        std::memcpy(&setAppId, &proc, sizeof(setAppId));
        setAppId(L"IU8LMC.Decodium4");
    }

    FreeLibrary(shell32);
}

static void scheduleWindowsTaskbarIconRefresh(QObject *context, QIcon const& icon)
{
    if (!context || icon.isNull())
        return;

    QObject::connect(qGuiApp, &QGuiApplication::focusWindowChanged, context,
                     [icon] (QWindow *) {
                         applyApplicationIconToTopLevelWindows(icon);
                     });

    for (int delayMs : {0, 250, 750, 1500, 3000, 5000, 10000, 30000, 60000}) {
        QTimer::singleShot(delayMs, context, [icon] {
            applyApplicationIconToTopLevelWindows(icon);
        });
    }
}
#endif

static bool isIgnorableShutdownQmlMessage(const QString& msg)
{
    return msg.contains(".qml:");
}

static void logQmlWarnings(const QList<QQmlError>& warnings)
{
    for (const QQmlError& warning : warnings) {
        const QString warningText = warning.toString();
        if (g_shuttingDown.load(std::memory_order_relaxed)
            && isIgnorableShutdownQmlMessage(warningText)) {
            continue;
        }
        L(warningText.toLocal8Bit().constData());
    }
}

static void qtMsgHandler(QtMsgType, const QMessageLogContext&, const QString& msg) {
    if (msg.contains("Main window closing - shutting down application")) {
        g_shuttingDown.store(true, std::memory_order_relaxed);
    }
#ifdef Q_OS_WIN
    if (msg.startsWith(QStringLiteral("DirectWrite: CreateFontFaceFromHDC() failed"))
        && msg.contains(QStringLiteral("MS Sans Serif"), Qt::CaseInsensitive)) {
        return;
    }
    if (msg.contains(QStringLiteral("Failed to create D3D12 device"), Qt::CaseInsensitive)
        || (msg.contains(QStringLiteral("D3D12"), Qt::CaseInsensitive)
            && msg.contains(QStringLiteral("0x887a0004"), Qt::CaseInsensitive))) {
        bool const firstFailure = !g_windowsD3d12DeviceFailed.exchange(true, std::memory_order_relaxed);
        if (firstFailure) {
            QString const reason =
                QStringLiteral("D3D12 device creation failed; use persistent D3D11 hardware fallback");
            writeWindowsGraphicsStartupFlag(graphicsStartupPendingFlagPath(),
                                            QByteArrayLiteral("d3d12"),
                                            reason.toLocal8Bit());
            writePersistentD3d11Fallback(reason);
            L("Windows D3D12 device failure persisted; automatic launches will use D3D11 hardware renderer");
        }
    }
#endif
    // Performance telemetry can originate from the GUI or scene-graph render
    // thread. Keep it off the synchronous startup-file/stderr path: L() opens
    // and closes a file for every line, which can itself create the stall the
    // telemetry is measuring on Windows. The diagnostic writer is ordered and
    // asynchronous, so these records remain available for support analysis.
    if (msg.startsWith(QStringLiteral("[PANMETRIC]"))
        || msg.startsWith(QStringLiteral("[MAINWATCH]"))
        || msg.startsWith(QStringLiteral("[MAINDISPATCH]"))
        || msg.startsWith(QStringLiteral("[GPUDBG] Panadapter waterfall GPU persistent upload"))) {
        DIAG_INFO(msg);
        return;
    }
    if (g_shuttingDown.load(std::memory_order_relaxed) && isIgnorableShutdownQmlMessage(msg))
        return;
    if (msg.startsWith(QStringLiteral("[TX-TL]"))
        || msg.startsWith(QStringLiteral("TX legacy bridge payload"))
        || msg.startsWith(QStringLiteral("TX mac PCM payload"))) {
        DIAG_INFO(msg);
        return;
    }
    L(msg.toLocal8Bit().constData());
}

static void registerLegacySettingsStreamTypes()
{
    qRegisterMetaType<FrequencyList_v2_101::Item>("Item_v2_101");
    qRegisterMetaType<FrequencyList_v2_101::FrequencyItems>("FrequencyItems_v2_101");
    qRegisterMetaType<FrequencyList_v2::Item>("Item_v2");
    qRegisterMetaType<FrequencyList_v2::FrequencyItems>("FrequencyItems_v2");
    qRegisterMetaType<FrequencyList::Item>("Item");
    qRegisterMetaType<FrequencyList::FrequencyItems>("FrequencyItems");
    qRegisterMetaType<Configuration::DataMode>("Configuration::DataMode");
    qRegisterMetaType<Configuration::Type2MsgGen>("Configuration::Type2MsgGen");
    qRegisterMetaType<StationList::Station>("Station");
    qRegisterMetaType<StationList::Stations>("Stations");
    qRegisterMetaType<TransceiverFactory::DataBits>("TransceiverFactory::DataBits");
    qRegisterMetaType<TransceiverFactory::StopBits>("TransceiverFactory::StopBits");
    qRegisterMetaType<TransceiverFactory::Handshake>("TransceiverFactory::Handshake");
    qRegisterMetaType<TransceiverFactory::PTTMethod>("TransceiverFactory::PTTMethod");
    qRegisterMetaType<TransceiverFactory::TXAudioSource>("TransceiverFactory::TXAudioSource");
    qRegisterMetaType<TransceiverFactory::SplitMode>("TransceiverFactory::SplitMode");
    qRegisterMetaType<WFPalette::Colours>("Colours");
    qRegisterMetaType<IARURegions::Region>("IARURegions::Region");
    qRegisterMetaType<DecodeHighlightingModel::HighlightInfo>("HighlightInfo");
    qRegisterMetaType<DecodeHighlightingModel::HighlightItems>("HighlightItems");
    qRegisterMetaType<QVector<short>>("QVector<short>");
    qRegisterMetaType<QVector<float>>("QVector<float>");
}

static bool ensureLegacySqliteDatabase()
{
    QDir writeableDataDir {QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)};
    if (!writeableDataDir.mkpath(QStringLiteral("."))) {
        L(("SQLite setup failed: cannot create data directory "
           + writeableDataDir.absolutePath().toLocal8Bit()).constData());
        return false;
    }

    if (!QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE"))) {
        L("SQLite setup failed: QSQLITE driver missing");
        return false;
    }

    QSqlDatabase db;
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
        db = QSqlDatabase::database(QSqlDatabase::defaultConnection);
    } else {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    }

    QString const dbPath = writeableDataDir.absoluteFilePath(QStringLiteral("db.sqlite"));
    if (db.databaseName().isEmpty() || db.databaseName() != dbPath) {
        if (db.isOpen()) {
            db.close();
        }
        db.setDatabaseName(dbPath);
    }

    if (!db.isOpen() && !db.open()) {
        L(("SQLite setup failed: " + db.lastError().text().toLocal8Bit()).constData());
        return false;
    }

    auto applyPragma = [&db](char const* pragma) {
        QSqlQuery query = db.exec(QString::fromLatin1(pragma));
        if (query.lastError().isValid()) {
            L(("SQLite pragma failed: " + query.lastError().text().toLocal8Bit()).constData());
        }
    };
    applyPragma("PRAGMA journal_mode=WAL");
    applyPragma("PRAGMA synchronous=NORMAL");
    applyPragma("PRAGMA busy_timeout=5000");
    // 1.0.237 (Phase 5.1 perf roadmap): PRAGMA aggiuntivi per persistence
    // di decode history. mmap 256MB + cache 64MB + temp in RAM riducono
    // I/O su SSD durante batch insert.
    applyPragma("PRAGMA temp_store=MEMORY");
    applyPragma("PRAGMA mmap_size=268435456");
    applyPragma("PRAGMA cache_size=-65536");

    // 1.0.237 (Phase 5.1): schema decode history. Tabelle vuote in 5.1,
    // populated dal write-behind worker thread in Phase 5.2.
    // Schema dalla roadmap performance: sessions + decodes con indici
    // per query (ts_utc, callsign_dx, band+mode).
    auto execDdl = [&db](char const* ddl) {
        QSqlQuery query = db.exec(QString::fromLatin1(ddl));
        if (query.lastError().isValid()) {
            L(("SQLite DDL failed: " + query.lastError().text().toLocal8Bit()
               + " | " + QByteArray(ddl)).constData());
        }
    };
    execDdl(
        "CREATE TABLE IF NOT EXISTS sessions ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  started_utc INTEGER NOT NULL,"
        "  ended_utc   INTEGER,"
        "  operator    TEXT,"
        "  station     TEXT,"
        "  decodium_ver TEXT NOT NULL"
        ")");
    execDdl(
        "CREATE TABLE IF NOT EXISTS decodes ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  ts_utc      INTEGER NOT NULL,"
        "  band        TEXT NOT NULL,"
        "  freq_hz     INTEGER NOT NULL,"
        "  mode        TEXT NOT NULL,"
        "  submode     TEXT,"
        "  callsign_dx TEXT,"
        "  callsign_de TEXT,"
        "  grid        TEXT,"
        "  snr_db      INTEGER,"
        "  dt_s        REAL,"
        "  df_hz       INTEGER,"
        "  message     TEXT NOT NULL,"
        "  confidence  INTEGER,"
        "  session_id  INTEGER NOT NULL,"
        "  FOREIGN KEY(session_id) REFERENCES sessions(id)"
        ")");
    execDdl("CREATE INDEX IF NOT EXISTS idx_decodes_ts ON decodes(ts_utc)");
    execDdl("CREATE INDEX IF NOT EXISTS idx_decodes_callsign ON decodes(callsign_dx)");
    execDdl("CREATE INDEX IF NOT EXISTS idx_decodes_band_mode ON decodes(band, mode)");

    L(("SQLite database OK: " + dbPath.toLocal8Bit()).constData());
    return true;
}

int main(int argc, char* argv[])
{
    // IU8LMC — persistenza fuori dal registro di Windows + migrazione one-shot.
    // DEVE restare la prima istruzione: da qui in poi ogni QSettings, il log e
    // il database devono gia' vedere il nuovo layout su file.
    DecodiumStorageMigration::configure();

#ifdef Q_OS_WIN
    int graphicsSupervisorExitCode = 0;
    if (runWindowsGraphicsStartupSupervisor(argc, argv, &graphicsSupervisorExitCode))
        return graphicsSupervisorExitCode;
#endif

    decodium::fft_compat::initialize_planner_thread_safety();

    qInstallMessageHandler(qtMsgHandler);
    L("main() START");
    L((QByteArray("Decodium version: ") + FORK_RELEASE_VERSION).constData());

    init_random_seed();
    Radio::register_types();
    register_types();
    registerLegacySettingsStreamTypes();
    L("legacy metatypes OK");

    // 1.0.180 — UI Revolution: stile QML Quick Controls selezionabile via QSettings.
    // 1.0.183 — FIX bug: usare lo stesso scope QSettings di DecodiumBridge
    // (IU8LMC/Decodium) altrimenti main_qml.cpp legge da un registry path
    // diverso (Decodium/Decodium3) e ignora le scelte dell'utente.
    // QQuickStyle::setStyle deve essere chiamato PRIMA di engine.load() ma
    // PUO' essere chiamato DOPO QApplication: lo spostiamo dopo cosi'
    // QSettings() default usa OrganizationName+ApplicationName corretti.
    // FluentWinUI3 = aspetto Windows 11 nativo (Qt 6.7+).

    // 1.0.180 — Per-monitor DPI v2 esplicito (gia' default Qt 6 ma chiaro)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // 1.0.307 (#2) — Scala UI globale opzionale (feedback tester "icone troppo piccole").
    // Va impostata PRIMA della QApplication: QT_SCALE_FACTOR e' il moltiplicatore nativo Qt
    // che ingrandisce icone+font+layout in modo coerente, sopra il DPI dello schermo.
    // Persistita come "uiScaleFactor" nello store canonico (Decodium3). Default 1.0 = nessun
    // cambio. Si applica al riavvio. Rispetta un QT_SCALE_FACTOR gia' impostato dall'utente.
    if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
        double const uiScale = QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"))
                                   .value(QStringLiteral("uiScaleFactor"), 1.0).toDouble();
        if (uiScale >= 0.8 && uiScale <= 2.5 && !qFuzzyCompare(uiScale, 1.0)) {
            qputenv("QT_SCALE_FACTOR", QByteArray::number(uiScale, 'g', 4));
            L((QByteArray("UI scale factor applied (QT_SCALE_FACTOR): ")
               + QByteArray::number(uiScale, 'g', 4)).constData());
        }
    }

#if !defined(Q_OS_WIN)
    if (qEnvironmentVariableIsSet("DECODIUM_GRAPHICS_BACKEND")
        && !qEnvironmentVariableIsSet("QSG_RHI_BACKEND")
        && !qEnvironmentVariableIsSet("QT_QUICK_BACKEND")) {
        qputenv("QSG_RHI_BACKEND", qgetenv("DECODIUM_GRAPHICS_BACKEND"));
        QByteArray backendMessage("Qt Quick graphics backend from DECODIUM_GRAPHICS_BACKEND: ");
        backendMessage += qgetenv("DECODIUM_GRAPHICS_BACKEND");
        L(backendMessage.constData());
    }
#endif

#if defined(Q_OS_MACOS)
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND")
        && !qEnvironmentVariableIsSet("QT_QUICK_BACKEND")) {
        qputenv("QSG_RHI_BACKEND", "metal");
        L("Qt Quick graphics backend defaulted to Metal");
    } else if (qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        QByteArray backendMessage("Qt Quick graphics backend from environment: ");
        backendMessage += qgetenv("QSG_RHI_BACKEND");
        L(backendMessage.constData());
    }
#elif defined(Q_OS_LINUX)
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND")
        && !qEnvironmentVariableIsSet("QT_QUICK_BACKEND")) {
        L("Qt Quick graphics backend left to Qt auto-selection on Linux");
    } else if (qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        QByteArray backendMessage("Qt Quick graphics backend from environment: ");
        backendMessage += qgetenv("QSG_RHI_BACKEND");
        L(backendMessage.constData());
    }
#endif

#ifdef Q_OS_WIN
    QString const slowQmlStartupFlag = slowQmlStartupFlagPath();
    QString const graphicsStartupPendingFlag = graphicsStartupPendingFlagPath();
    bool const graphicsRecoveryWorker =
        hasCommandLineSwitch(argc, argv, "--decodium-graphics-recovery");
    bool const commandLineResetSafeGraphics =
        hasCommandLineSwitch(argc, argv, "--reset-safe-graphics") && !graphicsRecoveryWorker;
    if (commandLineResetSafeGraphics) {
        removeFileIfExists(slowQmlStartupFlag);
        removeFileIfExists(graphicsStartupPendingFlag);
        clearPersistentD3d11Fallback();
    }
    auto normalizedBackend = [] (const char* name) -> QByteArray {
        return qEnvironmentVariableIsSet(name) ? qgetenv(name).trimmed().toLower() : QByteArray {};
    };
    auto requestsWarpGraphics = [] (QByteArray const& backend) {
        return backend == "safe" || backend == "warp";
    };
    auto requestsQtSoftwareGraphics = [] (QByteArray const& backend) {
        return backend == "software";
    };
    auto isSupportedWindowsRhiBackend = [] (QByteArray const& backend) {
        return backend.isEmpty()
            || backend == "d3d11"
            || backend == "d3d12"
            || backend == "opengl"
            || backend == "vulkan"
            || backend == "null";
    };

    // Low-end mode uses D3D11 hardware on Windows. OpenGL is kept only as an
    // explicit expert override because Qt 6 no longer ships ANGLE.
    bool lowEndForcedD3d11 = false;
    {
        QSettings lowEndProbe(QSettings::IniFormat, QSettings::UserScope,
                              QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        bool const lowEndMode = lowEndProbe.value(QStringLiteral("LowEndMode"), false).toBool();
        bool const previousGraphicsStartupFailed = QFile::exists(graphicsStartupPendingFlag);
        if (lowEndMode
            && !previousGraphicsStartupFailed
            && !qEnvironmentVariableIsSet("DECODIUM_GRAPHICS_BACKEND")
            && !qEnvironmentVariableIsSet("QSG_RHI_BACKEND")
            && !qEnvironmentVariableIsSet("QT_QUICK_BACKEND")) {
            qputenv("DECODIUM_GRAPHICS_BACKEND", "d3d11");
            lowEndForcedD3d11 = true;
            L("Windows low-end mode: using conservative D3D11 hardware backend");
        } else if (lowEndMode && previousGraphicsStartupFailed) {
            L("Windows low-end mode: previous graphics startup failed; using the automatic fallback chain");
        }
    }

    QByteArray const decodiumGraphicsBackend = normalizedBackend("DECODIUM_GRAPHICS_BACKEND");
    if (!decodiumGraphicsBackend.isEmpty()
        && !requestsWarpGraphics(decodiumGraphicsBackend)
        && !requestsQtSoftwareGraphics(decodiumGraphicsBackend)
        && !qEnvironmentVariableIsSet("QSG_RHI_BACKEND")
        && !qEnvironmentVariableIsSet("QT_QUICK_BACKEND")) {
        if (isSupportedWindowsRhiBackend(decodiumGraphicsBackend)) {
            qputenv("QSG_RHI_BACKEND", decodiumGraphicsBackend);
            QByteArray backendMessage("Qt Quick graphics backend from DECODIUM_GRAPHICS_BACKEND: ");
            backendMessage += decodiumGraphicsBackend;
            L(backendMessage.constData());
        } else {
            QByteArray backendMessage("Ignoring unsupported DECODIUM_GRAPHICS_BACKEND on Windows: ");
            backendMessage += decodiumGraphicsBackend;
            backendMessage += " (falling back to d3d11)";
            L(backendMessage.constData());
        }
    }

    QByteArray requestedRhiBackend = normalizedBackend("QSG_RHI_BACKEND");
    QByteArray requestedQuickBackend = normalizedBackend("QT_QUICK_BACKEND");
    bool const backendRequestsWarp =
        requestsWarpGraphics(decodiumGraphicsBackend)
        || requestsWarpGraphics(requestedRhiBackend)
        || requestsWarpGraphics(requestedQuickBackend);
    bool const backendRequestsQtSoftware =
        requestsQtSoftwareGraphics(decodiumGraphicsBackend)
        || requestsQtSoftwareGraphics(requestedRhiBackend)
        || requestsQtSoftwareGraphics(requestedQuickBackend);
    if (!requestedRhiBackend.isEmpty()
        && !requestsWarpGraphics(requestedRhiBackend)
        && !requestsQtSoftwareGraphics(requestedRhiBackend)
        && !isSupportedWindowsRhiBackend(requestedRhiBackend)) {
        QByteArray backendMessage("Ignoring unsupported QSG_RHI_BACKEND on Windows: ");
        backendMessage += requestedRhiBackend;
        backendMessage += " (falling back to d3d11)";
        L(backendMessage.constData());
        qunsetenv("QSG_RHI_BACKEND");
        requestedRhiBackend.clear();
    }

    bool const envSafeGraphics = qEnvironmentVariableIsSet("DECODIUM_SAFE_GRAPHICS");
    bool const commandLineWarpGraphics = hasCommandLineSwitch(argc, argv, "--safe-graphics");
    bool const commandLineQtSoftwareGraphics =
        hasCommandLineSwitch(argc, argv, "--disable-gpu")
        || hasCommandLineSwitch(argc, argv, "--software-renderer");
    bool const pendingGraphicsStartupMarker = QFile::exists(graphicsStartupPendingFlag);
    QByteArray const pendingGraphicsBackend =
        pendingGraphicsStartupMarker ? graphicsStartupFlagBackend(graphicsStartupPendingFlag) : QByteArray {};
    bool const explicitGraphicsBackend =
        (!decodiumGraphicsBackend.isEmpty() && !lowEndForcedD3d11)
        || !requestedRhiBackend.isEmpty()
        || !requestedQuickBackend.isEmpty();
    bool const slowQmlStartupMarker = QFile::exists(slowQmlStartupFlag);
    bool const previousSlowQmlStartup = previousStartupLogShowsSlowQml();
    QByteArray const persistentFallbackMode = commandLineResetSafeGraphics
        ? QByteArray {}
        : persistentGraphicsFallbackMode();
    auto fallbackAfter = [] (QByteArray const& failedBackend) -> QByteArray {
        if (failedBackend == QByteArrayLiteral("d3d11"))
            return QByteArrayLiteral("warp");
        if (failedBackend == QByteArrayLiteral("warp")
            || failedBackend == QByteArrayLiteral("software")) {
            return QByteArrayLiteral("software");
        }
        return QByteArrayLiteral("d3d11");
    };

    QByteArray activeWindowsGraphicsMode;
    bool graphicsRecoveryFromPendingMarker = false;
    bool automaticGraphicsFallback = false;
    bool usingPersistedGraphicsFallback = false;
    if (pendingGraphicsStartupMarker && !commandLineResetSafeGraphics) {
        activeWindowsGraphicsMode = fallbackAfter(pendingGraphicsBackend);
        graphicsRecoveryFromPendingMarker = true;
        automaticGraphicsFallback = true;
        L((QByteArray("Windows graphics recovery: previous backend=")
           + (pendingGraphicsBackend.isEmpty() ? QByteArrayLiteral("unknown") : pendingGraphicsBackend)
           + " next=" + activeWindowsGraphicsMode).constData());
    } else if (commandLineQtSoftwareGraphics || backendRequestsQtSoftware) {
        activeWindowsGraphicsMode = QByteArrayLiteral("software");
    } else if (commandLineWarpGraphics || envSafeGraphics || backendRequestsWarp) {
        activeWindowsGraphicsMode = QByteArrayLiteral("warp");
    } else if (!persistentFallbackMode.isEmpty() && !explicitGraphicsBackend) {
        activeWindowsGraphicsMode = persistentFallbackMode;
        usingPersistedGraphicsFallback = true;
    } else if ((slowQmlStartupMarker || previousSlowQmlStartup) && !explicitGraphicsBackend) {
        activeWindowsGraphicsMode = QByteArrayLiteral("d3d11");
        automaticGraphicsFallback = true;
    } else if (!decodiumGraphicsBackend.isEmpty()
               && isSupportedWindowsRhiBackend(decodiumGraphicsBackend)) {
        activeWindowsGraphicsMode = decodiumGraphicsBackend;
    } else if (!requestedQuickBackend.isEmpty()) {
        activeWindowsGraphicsMode = requestedQuickBackend;
    } else if (!requestedRhiBackend.isEmpty()) {
        activeWindowsGraphicsMode = requestedRhiBackend;
    } else {
        activeWindowsGraphicsMode = QByteArrayLiteral("d3d12");
    }

    bool const safeGraphicsRequested =
        activeWindowsGraphicsMode == QByteArrayLiteral("warp")
        || activeWindowsGraphicsMode == QByteArrayLiteral("software");
    bool const automaticD3d11Fallback =
        automaticGraphicsFallback && activeWindowsGraphicsMode == QByteArrayLiteral("d3d11");
    bool const automaticSafeGraphics = automaticGraphicsFallback && safeGraphicsRequested;
    bool const persistentD3d11FallbackUsable =
        usingPersistedGraphicsFallback && activeWindowsGraphicsMode == QByteArrayLiteral("d3d11");

    if (activeWindowsGraphicsMode == QByteArrayLiteral("software")) {
        qputenv("QT_QUICK_BACKEND", "software");
        qunsetenv("QSG_RHI_BACKEND");
        qunsetenv("QSG_RHI_PREFER_SOFTWARE_RENDERER");
        qputenv("QT_OPENGL", "software");
        L("Qt Quick graphics: using the GPU-independent Qt software renderer");
    } else if (activeWindowsGraphicsMode == QByteArrayLiteral("warp")) {
        qunsetenv("QT_QUICK_BACKEND");
        qputenv("QSG_RHI_BACKEND", "d3d11");
        qputenv("QSG_RHI_PREFER_SOFTWARE_RENDERER", "1");
        qputenv("QT_OPENGL", "software");
        L("Qt Quick graphics: using D3D11 WARP software rasterizer");
    } else if (!requestedQuickBackend.isEmpty()
               && activeWindowsGraphicsMode == requestedQuickBackend
               && !automaticGraphicsFallback) {
        L((QByteArray("Qt Quick scene graph backend from environment: ")
           + requestedQuickBackend).constData());
    } else {
        qunsetenv("QT_QUICK_BACKEND");
        qputenv("QSG_RHI_BACKEND", activeWindowsGraphicsMode);
        qunsetenv("QSG_RHI_PREFER_SOFTWARE_RENDERER");
        if (qgetenv("QT_OPENGL").trimmed().toLower() == "software")
            qunsetenv("QT_OPENGL");
        L((QByteArray("Qt Quick graphics backend selected: ")
           + activeWindowsGraphicsMode).constData());
    }
    setWindowsAppUserModelId();
#endif

    DecodiumApplication app(argc, argv);
    DecodiumLogging::installCrashHandler();
    L("QApplication OK");
#ifdef Q_OS_LINUX
    configureLinuxVulkanGpuSelection();
#endif
#ifdef Q_OS_WIN
    L("Windows dGPU preference requested via NVIDIA Optimus / AMD PowerXpress exports");
#endif

    // Set the real app identity before any QStandardPaths lookup. In AppImage
    // builds argv[0] is the launcher wrapper, and using it for CacheLocation
    // creates paths such as ~/.cache/AppRun.decodium-real.
    app.setApplicationName("Decodium");
    app.setApplicationVersion(QStringLiteral(FORK_RELEASE_VERSION));
    app.setOrganizationName(DecodiumStorageMigration::organizationName());  // IU8LMC: "Decodium" su Windows
    app.setOrganizationDomain("decodium.iu8lmc.it");

    // 1.0.180 — Pipeline cache shader: build path here, apply via objectCreated
    // (setGraphicsConfiguration is an instance method on QQuickWindow, must be
    // called before the scene graph initialises — objectCreated is the right hook).
    QString const pipelineCacheFile = []() -> QString {
        QString const dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(dir);
        return dir + QStringLiteral("/qsg_pipeline_cache.bin");
    }();
    L(("Pipeline cache path: " + pipelineCacheFile.toLocal8Bit()).constData());
    L((QByteArray("Qt version: ") + qVersion()).constData());
    L((QByteArray("OS: ") + QSysInfo::prettyProductName().toLocal8Bit()
       + " ABI=" + QSysInfo::buildAbi().toLocal8Bit()
       + " CPU=" + QSysInfo::currentCpuArchitecture().toLocal8Bit()).constData());
    logEnvVar("QSG_RHI_BACKEND");
    logEnvVar("QT_VK_PHYSICAL_DEVICE_INDEX");
    logEnvVar("QSG_RHI_PREFER_SOFTWARE_RENDERER");
    logEnvVar("QT_OPENGL");
    logEnvVar("QT_QUICK_BACKEND");
    logEnvVar("QML_DISABLE_DISK_CACHE");
    logEnvVar("QML_DISK_CACHE_PATH");
    logEnvVar("DECODIUM_SAFE_GRAPHICS");
    logEnvVar("DECODIUM_GRAPHICS_BACKEND");
    logEnvVar("DECODIUM_DISABLE_QML_CACHE");
    QIcon const appIcon = loadDecodiumApplicationIcon();
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
        L("application icon OK");
    } else {
        L("WARNING: application icon is null");
    }

    QString const fixedFontFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    if (!fixedFontFamily.isEmpty()) {
        QFont::insertSubstitution(QStringLiteral("Consolas"), fixedFontFamily);
        QFont::insertSubstitution(QStringLiteral("Monospace"), fixedFontFamily);
        QFont::insertSubstitution(QStringLiteral("monospace"), fixedFontFamily);
    }
    QString const uiFontFamily = firstInstalledFontFamily({
#if defined(Q_OS_MAC)
        QStringLiteral("Helvetica Neue"),
        QStringLiteral("Arial"),
#elif defined(Q_OS_WIN)
        QStringLiteral("Segoe UI"),
        QStringLiteral("Arial"),
#else
        QStringLiteral("Noto Sans"),
        QStringLiteral("DejaVu Sans"),
        QStringLiteral("Liberation Sans"),
        QStringLiteral("Sans Serif"),
#endif
    });
    if (!uiFontFamily.isEmpty()) {
        QFont::insertSubstitution(QStringLiteral("MS Shell Dlg 2"), uiFontFamily);
        QFont::insertSubstitution(QStringLiteral("MS Shell Dlg"), uiFontFamily);
#if defined(Q_OS_WIN)
        QFont::insertSubstitution(QStringLiteral("MS Sans Serif"), uiFontFamily);
        QFont::insertSubstitution(QStringLiteral("MS Serif"), uiFontFamily);
        QFont::insertSubstitution(QStringLiteral("System"), uiFontFamily);
        QFont::insertSubstitution(QStringLiteral("Small Fonts"), uiFontFamily);
#endif
#if !defined(Q_OS_WIN)
        QFont::insertSubstitution(QStringLiteral("Segoe UI"), uiFontFamily);
#endif
    }

    // Forza locale C per numeri (punto decimale) — evita problemi con locale FR/DE/IT
    // che usano la virgola e bloccano il parsing di frequenze/configurazioni
    QLocale::setDefault(QLocale::c());
    setlocale(LC_NUMERIC, "C");

    QApplication::setStyle(QStyleFactory::create("Fusion"));
    if (!uiFontFamily.isEmpty()) {
        QFont uiFont {uiFontFamily};
        uiFont.setPointSize(10);
        uiFont.setStyleHint(QFont::SansSerif);
        uiFont.setStyleStrategy(QFont::PreferAntialias);
        QGuiApplication::setFont(uiFont);
        QApplication::setFont(uiFont);
    }
    QQuickWindow::setTextRenderType(QQuickWindow::QtTextRendering);
    L("Qt Quick text render type forced: QtTextRendering");
    app.setApplicationName("Decodium");
    app.setApplicationVersion(QStringLiteral(FORK_RELEASE_VERSION));
    app.setOrganizationName(DecodiumStorageMigration::organizationName());  // IU8LMC: "Decodium" su Windows
    app.setOrganizationDomain("decodium.iu8lmc.it");
#ifdef Q_OS_WIN
    QGuiApplication::setDesktopFileName(QStringLiteral("IU8LMC.Decodium4"));
    scheduleWindowsTaskbarIconRefresh(&app, appIcon);
#endif

    // 1.0.183 — Lettura UI/Style DOPO QApplication cosi' QSettings() default
    // usa OrganizationName "IU8LMC" + ApplicationName "Decodium" — lo stesso
    // scope di DecodiumBridge::setUiStyle(). PRIMA della 1.0.183 usavamo
    // QSettings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3") che leggeva un registry path diverso
    // e il setter del bridge non era mai visto, fallback a "Material" sempre.
    // QQuickStyle::setStyle DEVE essere chiamato PRIMA di engine.load(),
    // qui siamo molto prima quindi OK.
    {
        QSettings s;  // default scope: IU8LMC / Decodium
        QString const styleName = s.value(QStringLiteral("UI/Style"),
                                          QStringLiteral("Default")).toString();

        // 1.0.185 — Whitelist stili supportati. Imagine e Basic rimossi (binding
        // loop / asset mancanti). "Default" diventa alias per "Material": motivo
        // e' che su Windows con Qt 6.11 il fallback Qt nativo risolve al Windows
        // Native style che NON permette customization di background/contentItem/
        // indicator (Decodium fa molte customizations -> warning massivi + UI
        // degradata). Material e' l'unico stile customizable che Decodium aspetta
        // come baseline visiva storica (default fino al 1.0.179). Quindi ora
        // SEMPRE QQuickStyle::setStyle viene chiamato, mai lasciato a Qt fallback.
        static QStringList const supportedStyles = {
            QStringLiteral("FluentWinUI3"),
            QStringLiteral("Material"),
            QStringLiteral("Universal"),
            QStringLiteral("Fusion")
        };
        QString effectiveStyle = styleName;
        if (effectiveStyle.compare(QStringLiteral("Default"), Qt::CaseInsensitive) == 0
            || !supportedStyles.contains(effectiveStyle)) {
            if (!supportedStyles.contains(effectiveStyle)
                && effectiveStyle.compare(QStringLiteral("Default"), Qt::CaseInsensitive) != 0) {
                qWarning() << "[UI] Style not in whitelist:" << effectiveStyle
                           << "-> fallback to Material";
            }
            effectiveStyle = QStringLiteral("Material");
        }
        QQuickStyle::setStyle(effectiveStyle);
        qInfo() << "[UI] QML style:" << effectiveStyle;
        L(QStringLiteral("QQuickStyle resolved: %1").arg(effectiveStyle).toUtf8().constData());
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("\nDecodium 4.0 Core Gallager: Digital Modes for Weak Signal Communications in Amateur Radio"));
    auto const helpOption = parser.addHelpOption();
    auto const versionOption = parser.addVersionOption();

    QCommandLineOption const rigOption(QStringList {} << "r" << "rig-name",
                                       QStringLiteral("Where <rig-name> is for multi-instance support."),
                                       QStringLiteral("rig-name"));
    QCommandLineOption const configOption(QStringList {} << "c" << "config",
                                          QStringLiteral("Where <configuration> is an existing one."),
                                          QStringLiteral("configuration"));
    QCommandLineOption const languageOption(QStringList {} << "l" << "language",
                                            QStringLiteral("Where <language> is <lang-code>[-<country-code>]."),
                                            QStringLiteral("language"));
    QCommandLineOption const testOption(QStringList {} << "test-mode",
                                        QStringLiteral("Writable files in test location. Use with caution, for testing only."));
    QCommandLineOption const safeGraphicsOption(
        QStringList {} << "safe-graphics",
        QStringLiteral("Use the Windows D3D11 WARP renderer for Qt Quick startup troubleshooting."));
    QCommandLineOption const softwareGraphicsOption(
        QStringList {} << "disable-gpu" << "software-renderer",
        QStringLiteral("Use the GPU-independent Qt Quick software renderer."));
    QCommandLineOption const resetSafeGraphicsOption(
        QStringList {} << "reset-safe-graphics",
        QStringLiteral("Clear automatic Windows safe graphics and persistent D3D11 fallback markers."));
    QCommandLineOption const graphicsWorkerOption(
        QStringList {} << "decodium-graphics-worker",
        QStringLiteral("Internal Windows graphics startup worker."));
    QCommandLineOption const graphicsRecoveryOption(
        QStringList {} << "decodium-graphics-recovery",
        QStringLiteral("Internal Windows graphics recovery worker."));
    QCommandLineOption const labCallsignOption(
        QStringList {} << "lab-callsign",
        QStringLiteral("Runtime lab override for the local callsign."),
        QStringLiteral("callsign"));
    QCommandLineOption const labGridOption(
        QStringList {} << "lab-grid",
        QStringLiteral("Runtime lab override for the local grid locator."),
        QStringLiteral("grid"));
    QCommandLineOption const labAudioDeviceOption(
        QStringList {} << "lab-audio-device",
        QStringLiteral("Runtime lab override for both audio input and output devices."),
        QStringLiteral("device"));
    QCommandLineOption const labAudioInputOption(
        QStringList {} << "lab-audio-input",
        QStringLiteral("Runtime lab override for the audio input device."),
        QStringLiteral("device"));
    QCommandLineOption const labAudioOutputOption(
        QStringList {} << "lab-audio-output",
        QStringLiteral("Runtime lab override for the audio output device."),
        QStringLiteral("device"));
    QCommandLineOption const labTxOutputLevelOption(
        QStringList {} << "lab-tx-output-level" << "lab-tx-level",
        QStringLiteral("Runtime lab override for TX output attenuation level 0..450; 0=max output, 450=45 dB attenuation."),
        QStringLiteral("level"));
    QCommandLineOption const labModeOption(
        QStringList {} << "lab-mode",
        QStringLiteral("Runtime lab override for the application mode."),
        QStringLiteral("mode"));
    QCommandLineOption const labDialHzOption(
        QStringList {} << "lab-dial-hz",
        QStringLiteral("Runtime lab override for the dial frequency in Hz."),
        QStringLiteral("hz"));
    QCommandLineOption const labNoCatOption(
        QStringList {} << "lab-no-cat",
        QStringLiteral("Disable CAT auto-connect for an isolated runtime lab session."));
    QCommandLineOption const labNoMonitorOption(
        QStringList {} << "lab-no-monitor",
        QStringLiteral("Force RX monitor off after lab startup/reapply for isolated TX tests."));
    QCommandLineOption const labReapplyMsOption(
        QStringList {} << "lab-reapply-ms",
        QStringLiteral("Delay before reapplying runtime lab overrides after QML/settings startup."),
        QStringLiteral("ms"),
        QStringLiteral("6500"));
    QCommandLineOption const labMonitorMsOption(
        QStringList {} << "lab-monitor-ms",
        QStringLiteral("Delay before starting RX monitor in a runtime lab session."),
        QStringLiteral("ms"));
    QCommandLineOption const labSstvRxMsOption(
        QStringList {} << "lab-sstv-rx-ms",
        QStringLiteral("Delay before starting native SSTV RX in a runtime lab session."),
        QStringLiteral("ms"));
    QCommandLineOption const labSstvRxModeOption(
        QStringList {} << "lab-sstv-rx-mode",
        QStringLiteral("Manual native SSTV RX mode used by the runtime lab session."),
        QStringLiteral("mode"));
    QCommandLineOption const labSstvRxReadyFileOption(
        QStringList {} << "lab-sstv-rx-ready-file",
        QStringLiteral("Write this file when native SSTV RX is running and its capture worker has processed audio."),
        QStringLiteral("path"));
    QCommandLineOption const labSstvTxMsOption(
        QStringList {} << "lab-sstv-tx-ms",
        QStringLiteral("Delay before preparing and transmitting a native SSTV calibration image."),
        QStringLiteral("ms"));
    QCommandLineOption const labSstvTxWaitFileOption(
        QStringList {} << "lab-sstv-tx-wait-file",
        QStringLiteral("Do not start native SSTV TX until this lab readiness file exists."),
        QStringLiteral("path"));
    QCommandLineOption const labSstvTxWaitTimeoutOption(
        QStringList {} << "lab-sstv-tx-wait-timeout-ms",
        QStringLiteral("Maximum wait for --lab-sstv-tx-wait-file; zero means wait until the lab quits."),
        QStringLiteral("ms"),
        QStringLiteral("60000"));
    QCommandLineOption const labSstvModeOption(
        QStringList {} << "lab-sstv-mode",
        QStringLiteral("Native SSTV mode used by --lab-sstv-tx-ms."),
        QStringLiteral("mode"),
        QStringLiteral("martin-m1"));
    QCommandLineOption const labSstvImageOption(
        QStringList {} << "lab-sstv-image",
        QStringLiteral("Local image used by --lab-sstv-tx-ms; omitted means calibration pattern."),
        QStringLiteral("path"));
    QCommandLineOption const labSstvVoxOption(
        QStringList {} << "lab-sstv-vox",
        QStringLiteral("Use audio-only VOX PTT for a native SSTV lab TX without CAT."));
    QCommandLineOption const labBcastMsOption(
        QStringList {} << "lab-bcast-ms",
        QStringLiteral("Delay before sending an armed FT2-Link broadcast in a runtime lab session."),
        QStringLiteral("ms"));
    QCommandLineOption const labBcastTextOption(
        QStringList {} << "lab-bcast-text",
        QStringLiteral("Broadcast text used by --lab-bcast-ms."),
        QStringLiteral("text"),
        QStringLiteral("D4 LAB BCAST"));
    QCommandLineOption const labBeaconMsOption(
        QStringList {} << "lab-beacon-ms",
        QStringLiteral("Delay before sending an armed FT2-Link beacon/CQ in a runtime lab session."),
        QStringLiteral("ms"));
    QCommandLineOption const labBeaconCqOption(
        QStringList {} << "lab-beacon-cq",
        QStringLiteral("Send CQ instead of plain beacon with --lab-beacon-ms."));
    QCommandLineOption const labCqSlotMsOption(
        QStringList {} << "lab-cq-slot-ms",
        QStringLiteral("Delay before sending an armed FT2-Link CQ slot beacon."),
        QStringLiteral("ms"));
    QCommandLineOption const labCqSlotIdOption(
        QStringList {} << "lab-cq-slot-id",
        QStringLiteral("Slot id used by --lab-cq-slot-ms."),
        QStringLiteral("id"),
        QStringLiteral("1"));
    QCommandLineOption const labCqSlotSizeOption(
        QStringList {} << "lab-cq-slot-size",
        QStringLiteral("Slot size Hz used by --lab-cq-slot-ms."),
        QStringLiteral("hz"),
        QStringLiteral("500"));
    QCommandLineOption const labAutoCqMsOption(
        QStringList {} << "lab-auto-cq-ms",
        QStringLiteral("Delay before enabling FT2-Link auto CQ in a runtime lab session."),
        QStringLiteral("ms"));
    QCommandLineOption const labAutoCqIntervalOption(
        QStringList {} << "lab-auto-cq-interval",
        QStringLiteral("Auto CQ interval seconds used by --lab-auto-cq-ms."),
        QStringLiteral("seconds"),
        QStringLiteral("60"));
    QCommandLineOption const labSendTxMsOption(
        QStringList {} << "lab-sendtx-ms",
        QStringLiteral("Delay before sending a standard TX slot in a runtime lab session."),
        QStringLiteral("ms"));
    QCommandLineOption const labSendTxSlotOption(
        QStringList {} << "lab-sendtx-slot",
        QStringLiteral("TX slot used by --lab-sendtx-ms."),
        QStringLiteral("slot"),
        QStringLiteral("6"));
    QCommandLineOption const labSendTxPlanOption(
        QStringList {} << "lab-sendtx-plan",
        QStringLiteral("Comma-separated standard TX plan entries in the form ms:slot."),
        QStringLiteral("plan"));
    QCommandLineOption const labDxCallOption(
        QStringList {} << "lab-dx-call",
        QStringLiteral("Runtime lab override for the active DX callsign."),
        QStringLiteral("callsign"));
    QCommandLineOption const labDxGridOption(
        QStringList {} << "lab-dx-grid",
        QStringLiteral("Runtime lab override for the active DX grid locator."),
        QStringLiteral("grid"));
    QCommandLineOption const labStandardAutoCqMsOption(
        QStringList {} << "lab-standard-autocq-ms",
        QStringLiteral("Delay before enabling standard-mode AutoCQ in a runtime lab session."),
        QStringLiteral("ms"));
    QCommandLineOption const labStandardWaitPounceMsOption(
        QStringList {} << "lab-standard-wait-pounce-ms",
        QStringLiteral("Delay before enabling standard-mode Wait & Pounce in a runtime lab session."),
        QStringLiteral("ms"));
    QCommandLineOption const labTxPeriodOption(
        QStringList {} << "lab-tx-period",
        QStringLiteral("Runtime lab override for TX period: 0=even/first, 1=odd/second."),
        QStringLiteral("period"));
    QCommandLineOption const labHeardCallOption(
        QStringList {} << "lab-heard-call",
        QStringLiteral("Seed a recent heard/contact callsign for path/relay lab scenarios."),
        QStringLiteral("callsign"));
    QCommandLineOption const labHeardGridOption(
        QStringList {} << "lab-heard-grid",
        QStringLiteral("Grid locator used by --lab-heard-call."),
        QStringLiteral("grid"));
    QCommandLineOption const labHeardNameOption(
        QStringList {} << "lab-heard-name",
        QStringLiteral("Name used by --lab-heard-call."),
        QStringLiteral("name"),
        QStringLiteral("LAB"));
    QCommandLineOption const labPingMsOption(
        QStringList {} << "lab-ping-ms",
        QStringLiteral("Delay before sending an armed FT2-Link ping in a runtime lab session."),
        QStringLiteral("ms"));
    QCommandLineOption const labPingCallOption(
        QStringList {} << "lab-ping-call",
        QStringLiteral("Remote callsign used by --lab-ping-ms."),
        QStringLiteral("callsign"));
    QCommandLineOption const labPathMsOption(
        QStringList {} << "lab-path-ms",
        QStringLiteral("Delay before sending an armed FT2-Link path finder request."),
        QStringLiteral("ms"));
    QCommandLineOption const labPathTargetOption(
        QStringList {} << "lab-path-target",
        QStringLiteral("Target callsign used by --lab-path-ms/--lab-path-response-ms."),
        QStringLiteral("callsign"));
    QCommandLineOption const labPathResponseMsOption(
        QStringList {} << "lab-path-response-ms",
        QStringLiteral("Delay before sending an armed FT2-Link path finder response."),
        QStringLiteral("ms"));
    QCommandLineOption const labDigiEnableOption(
        QStringList {} << "lab-digi-enable",
        QStringLiteral("Enable FT2-Link digipeater forwarding in a runtime lab session."));
    QCommandLineOption const labDigiMsOption(
        QStringList {} << "lab-digi-ms",
        QStringLiteral("Delay before sending an armed FT2-Link digipeater frame."),
        QStringLiteral("ms"));
    QCommandLineOption const labDigiTargetOption(
        QStringList {} << "lab-digi-target",
        QStringLiteral("Target callsign for --lab-digi-ms."),
        QStringLiteral("callsign"),
        QStringLiteral("ALL"));
    QCommandLineOption const labDigiBodyOption(
        QStringList {} << "lab-digi-body",
        QStringLiteral("Payload text for --lab-digi-ms."),
        QStringLiteral("text"),
        QStringLiteral("D4 LAB DIGI"));
    QCommandLineOption const labDigiHopsOption(
        QStringList {} << "lab-digi-hops",
        QStringLiteral("Maximum hop count for --lab-digi-ms and --lab-digi-enable."),
        QStringLiteral("hops"),
        QStringLiteral("2"));
    QCommandLineOption const labProfileOption(
        QStringList {} << "lab-profile",
        QStringLiteral("Preferred FT2-Link lab profile: W500 or W2300."),
        QStringLiteral("profile"),
        QStringLiteral("W2300"));
    QCommandLineOption const labConnectMsOption(
        QStringList {} << "lab-connect-ms",
        QStringLiteral("Delay before starting an FT2-Link lab session handshake."),
        QStringLiteral("ms"));
    QCommandLineOption const labConnectCallOption(
        QStringList {} << "lab-connect-call",
        QStringLiteral("Remote callsign for --lab-connect-ms."),
        QStringLiteral("callsign"));
    QCommandLineOption const labConnectGridOption(
        QStringList {} << "lab-connect-grid",
        QStringLiteral("Remote grid locator advertised before --lab-connect-ms."),
        QStringLiteral("grid"));
    QCommandLineOption const labTextMsOption(
        QStringList {} << "lab-text-ms",
        QStringLiteral("Delay before sending a session text message in the FT2-Link lab."),
        QStringLiteral("ms"));
    QCommandLineOption const labTextOption(
        QStringList {} << "lab-text",
        QStringLiteral("Session text used by --lab-text-ms."),
        QStringLiteral("text"),
        QStringLiteral("D4 LAB TEXT"));
    QCommandLineOption const labQsyMsOption(
        QStringList {} << "lab-qsy-ms",
        QStringLiteral("Delay before sending an FT2-Link lab QSY invitation over a connected session."),
        QStringLiteral("ms"));
    QCommandLineOption const labQsyOffsetHzOption(
        QStringList {} << "lab-qsy-offset-hz",
        QStringLiteral("Frequency offset in Hz used by --lab-qsy-ms."),
        QStringLiteral("hz"),
        QStringLiteral("750"));
    QCommandLineOption const labAutoAcceptQsyOption(
        QStringList {} << "lab-auto-accept-qsy",
        QStringLiteral("Automatically accept incoming FT2-Link QSY invitations in lab mode."));
    QCommandLineOption const labMailMsOption(
        QStringList {} << "lab-mail-ms",
        QStringLiteral("Delay before sending FT2-Link lab mailbox traffic."),
        QStringLiteral("ms"));
    QCommandLineOption const labMailSubjectOption(
        QStringList {} << "lab-mail-subject",
        QStringLiteral("Mailbox subject used by --lab-mail-ms."),
        QStringLiteral("subject"),
        QStringLiteral("D4 LAB MAIL"));
    QCommandLineOption const labMailBodyOption(
        QStringList {} << "lab-mail-body",
        QStringLiteral("Mailbox body used by --lab-mail-ms."),
        QStringLiteral("body"),
        QStringLiteral("D4 LAB MAIL BODY"));
    QCommandLineOption const labFormMsOption(
        QStringList {} << "lab-form-ms",
        QStringLiteral("Delay before sending FT2-Link lab form traffic."),
        QStringLiteral("ms"));
    QCommandLineOption const labFormTypeOption(
        QStringList {} << "lab-form-type",
        QStringLiteral("Form type used by --lab-form-ms."),
        QStringLiteral("type"),
        QStringLiteral("ICS213"));
    QCommandLineOption const labFormMessageOption(
        QStringList {} << "lab-form-message",
        QStringLiteral("Form message field used by --lab-form-ms."),
        QStringLiteral("message"),
        QStringLiteral("D4 LAB FORM BODY"));
    QCommandLineOption const labFileMsOption(
        QStringList {} << "lab-file-ms",
        QStringLiteral("Delay before sending FT2-Link lab file traffic."),
        QStringLiteral("ms"));
    QCommandLineOption const labFileNameOption(
        QStringList {} << "lab-file-name",
        QStringLiteral("File name used by --lab-file-ms."),
        QStringLiteral("name"),
        QStringLiteral("d4-lab.txt"));
    QCommandLineOption const labFileTextOption(
        QStringList {} << "lab-file-text",
        QStringLiteral("File text used by --lab-file-ms."),
        QStringLiteral("text"),
        QStringLiteral("D4 LAB FILE BODY"));
    QCommandLineOption const labBbsServerEnableOption(
        QStringList {} << "lab-bbs-server-enable",
        QStringLiteral("Enable the FT2-Link BBS file server in a runtime lab session."));
    QCommandLineOption const labBbsPublishNameOption(
        QStringList {} << "lab-bbs-publish-name",
        QStringLiteral("File name published by --lab-bbs-server-enable."),
        QStringLiteral("name"),
        QStringLiteral("bbs-lab.txt"));
    QCommandLineOption const labBbsPublishTextOption(
        QStringList {} << "lab-bbs-publish-text",
        QStringLiteral("Text body published by --lab-bbs-server-enable."),
        QStringLiteral("text"),
        QStringLiteral("D4 LAB BBS FILE BODY"));
    QCommandLineOption const labBbsListMsOption(
        QStringList {} << "lab-bbs-list-ms",
        QStringLiteral("Delay before requesting a remote FT2-Link BBS file list."),
        QStringLiteral("ms"));
    QCommandLineOption const labBbsGetMsOption(
        QStringList {} << "lab-bbs-get-ms",
        QStringLiteral("Delay before requesting a remote FT2-Link BBS file."),
        QStringLiteral("ms"));
    QCommandLineOption const labBbsGetNameOption(
        QStringList {} << "lab-bbs-get-name",
        QStringLiteral("File name requested by --lab-bbs-get-ms."),
        QStringLiteral("name"),
        QStringLiteral("bbs-lab.txt"));
    QCommandLineOption const labBbsServerListMsOption(
        QStringList {} << "lab-bbs-server-list-ms",
        QStringLiteral("Delay before transmitting the local FT2-Link BBS file list."),
        QStringLiteral("ms"));
    QCommandLineOption const labBbsServerFileMsOption(
        QStringList {} << "lab-bbs-server-file-ms",
        QStringLiteral("Delay before transmitting a local FT2-Link BBS server file."),
        QStringLiteral("ms"));
    QCommandLineOption const labBulletinMsOption(
        QStringList {} << "lab-bulletin-ms",
        QStringLiteral("Delay before sending FT2-Link lab BBS bulletin traffic."),
        QStringLiteral("ms"));
    QCommandLineOption const labBulletinGroupOption(
        QStringList {} << "lab-bulletin-group",
        QStringLiteral("Bulletin group used by --lab-bulletin-ms."),
        QStringLiteral("group"),
        QStringLiteral("NET"));
    QCommandLineOption const labBulletinTitleOption(
        QStringList {} << "lab-bulletin-title",
        QStringLiteral("Bulletin title used by --lab-bulletin-ms."),
        QStringLiteral("title"),
        QStringLiteral("D4 LAB BBS"));
    QCommandLineOption const labBulletinBodyOption(
        QStringList {} << "lab-bulletin-body",
        QStringLiteral("Bulletin body used by --lab-bulletin-ms."),
        QStringLiteral("body"),
        QStringLiteral("D4 LAB BBS BODY"));
    QCommandLineOption const labParkRelayMsOption(
        QStringList {} << "lab-park-relay-ms",
        QStringLiteral("Delay before parking an FT2-Link relay mailbox item."),
        QStringLiteral("ms"));
    QCommandLineOption const labRelayTargetOption(
        QStringList {} << "lab-relay-target",
        QStringLiteral("Final destination callsign for --lab-park-relay-ms."),
        QStringLiteral("callsign"));
    QCommandLineOption const labRelaySubjectOption(
        QStringList {} << "lab-relay-subject",
        QStringLiteral("Relay mailbox subject used by --lab-park-relay-ms."),
        QStringLiteral("subject"),
        QStringLiteral("D4 LAB RELAY"));
    QCommandLineOption const labRelayBodyOption(
        QStringList {} << "lab-relay-body",
        QStringLiteral("Relay mailbox body used by --lab-park-relay-ms."),
        QStringLiteral("body"),
        QStringLiteral("D4 LAB RELAY BODY"));
    QCommandLineOption const labRelayTxMsOption(
        QStringList {} << "lab-relay-tx-ms",
        QStringLiteral("Delay before transmitting a parked relay mailbox item over a connected session."),
        QStringLiteral("ms"));
    QCommandLineOption const ft2LinkRfLabDirOption(
        QStringList {} << "ft2link-rflab-dir",
        QStringLiteral("Run the FT2-Link RF WAV self-test and write generated WAVs to this directory."),
        QStringLiteral("directory"));
    QCommandLineOption const ft2LinkRfLabReplayOption(
        QStringList {} << "ft2link-rflab-replay",
        QStringLiteral("Replay a PCM16 WAV through the FT2-Link RF lab decoder."),
        QStringLiteral("wav"));
    QCommandLineOption const ft2LinkRfLabSweepDirOption(
        QStringList {} << "ft2link-rflab-sweep-dir",
        QStringLiteral("Run the FT2-Link RF channel impairment sweep and write WAVs to this directory."),
        QStringLiteral("directory"));
    QCommandLineOption const ft2LinkRfLabSweepProfilesOption(
        QStringList {} << "ft2link-rflab-sweep-profiles",
        QStringLiteral("Comma-separated profiles for --ft2link-rflab-sweep-dir: W2300, W500, NARROW."),
        QStringLiteral("profiles"),
        QStringLiteral("W2300"));
    QCommandLineOption const ft2LinkRfLabSweepMaxCasesOption(
        QStringList {} << "ft2link-rflab-sweep-max-cases",
        QStringLiteral("Limit channel sweep case count for quick FT2-Link RF lab runs."),
        QStringLiteral("count"));
    QCommandLineOption const labQuitMsOption(
        QStringList {} << "lab-quit-ms",
        QStringLiteral("Delay before quitting the runtime lab session."),
        QStringLiteral("ms"));
    parser.addOption(rigOption);
    parser.addOption(configOption);
    parser.addOption(languageOption);
    parser.addOption(testOption);
    parser.addOption(safeGraphicsOption);
    parser.addOption(softwareGraphicsOption);
    parser.addOption(resetSafeGraphicsOption);
    parser.addOption(graphicsWorkerOption);
    parser.addOption(graphicsRecoveryOption);
    parser.addOption(labCallsignOption);
    parser.addOption(labGridOption);
    parser.addOption(labAudioDeviceOption);
    parser.addOption(labAudioInputOption);
    parser.addOption(labAudioOutputOption);
    parser.addOption(labTxOutputLevelOption);
    parser.addOption(labModeOption);
    parser.addOption(labDialHzOption);
    parser.addOption(labNoCatOption);
    parser.addOption(labNoMonitorOption);
    parser.addOption(labReapplyMsOption);
    parser.addOption(labMonitorMsOption);
    parser.addOption(labSstvRxMsOption);
    parser.addOption(labSstvRxModeOption);
    parser.addOption(labSstvRxReadyFileOption);
    parser.addOption(labSstvTxMsOption);
    parser.addOption(labSstvTxWaitFileOption);
    parser.addOption(labSstvTxWaitTimeoutOption);
    parser.addOption(labSstvModeOption);
    parser.addOption(labSstvImageOption);
    parser.addOption(labSstvVoxOption);
    parser.addOption(labBcastMsOption);
    parser.addOption(labBcastTextOption);
    parser.addOption(labBeaconMsOption);
    parser.addOption(labBeaconCqOption);
    parser.addOption(labCqSlotMsOption);
    parser.addOption(labCqSlotIdOption);
    parser.addOption(labCqSlotSizeOption);
    parser.addOption(labAutoCqMsOption);
    parser.addOption(labAutoCqIntervalOption);
    parser.addOption(labSendTxMsOption);
    parser.addOption(labSendTxSlotOption);
    parser.addOption(labSendTxPlanOption);
    parser.addOption(labDxCallOption);
    parser.addOption(labDxGridOption);
    parser.addOption(labStandardAutoCqMsOption);
    parser.addOption(labStandardWaitPounceMsOption);
    parser.addOption(labTxPeriodOption);
    parser.addOption(labHeardCallOption);
    parser.addOption(labHeardGridOption);
    parser.addOption(labHeardNameOption);
    parser.addOption(labPingMsOption);
    parser.addOption(labPingCallOption);
    parser.addOption(labPathMsOption);
    parser.addOption(labPathTargetOption);
    parser.addOption(labPathResponseMsOption);
    parser.addOption(labDigiEnableOption);
    parser.addOption(labDigiMsOption);
    parser.addOption(labDigiTargetOption);
    parser.addOption(labDigiBodyOption);
    parser.addOption(labDigiHopsOption);
    parser.addOption(labProfileOption);
    parser.addOption(labConnectMsOption);
    parser.addOption(labConnectCallOption);
    parser.addOption(labConnectGridOption);
    parser.addOption(labTextMsOption);
    parser.addOption(labTextOption);
    parser.addOption(labQsyMsOption);
    parser.addOption(labQsyOffsetHzOption);
    parser.addOption(labAutoAcceptQsyOption);
    parser.addOption(labMailMsOption);
    parser.addOption(labMailSubjectOption);
    parser.addOption(labMailBodyOption);
    parser.addOption(labFormMsOption);
    parser.addOption(labFormTypeOption);
    parser.addOption(labFormMessageOption);
    parser.addOption(labFileMsOption);
    parser.addOption(labFileNameOption);
    parser.addOption(labFileTextOption);
    parser.addOption(labBbsServerEnableOption);
    parser.addOption(labBbsPublishNameOption);
    parser.addOption(labBbsPublishTextOption);
    parser.addOption(labBbsListMsOption);
    parser.addOption(labBbsGetMsOption);
    parser.addOption(labBbsGetNameOption);
    parser.addOption(labBbsServerListMsOption);
    parser.addOption(labBbsServerFileMsOption);
    parser.addOption(labBulletinMsOption);
    parser.addOption(labBulletinGroupOption);
    parser.addOption(labBulletinTitleOption);
    parser.addOption(labBulletinBodyOption);
    parser.addOption(labParkRelayMsOption);
    parser.addOption(labRelayTargetOption);
    parser.addOption(labRelaySubjectOption);
    parser.addOption(labRelayBodyOption);
    parser.addOption(labRelayTxMsOption);
    parser.addOption(ft2LinkRfLabDirOption);
    parser.addOption(ft2LinkRfLabReplayOption);
    parser.addOption(ft2LinkRfLabSweepDirOption);
    parser.addOption(ft2LinkRfLabSweepProfilesOption);
    parser.addOption(ft2LinkRfLabSweepMaxCasesOption);
    parser.addOption(labQuitMsOption);

    if (!parser.parse(app.arguments())) {
        L(("Command line error: " + parser.errorText()).toLocal8Bit().constData());
        return -1;
    }
    if (parser.isSet(helpOption)) {
        parser.showHelp(0);
    }
    if (parser.isSet(versionOption)) {
        parser.showVersion();
    }

    QStandardPaths::setTestModeEnabled(parser.isSet(testOption));

    QString const rigName = parser.value(rigOption).trimmed();
    if (!rigName.isEmpty()) {
        if (rigName.contains(QRegularExpression(QStringLiteral(R"([\\/,])")))) {
            L("Invalid rig name - \\ & / not allowed");
            return -1;
        }
        app.setApplicationName(app.applicationName() + QStringLiteral(" - ") + rigName);
    }
    if (parser.isSet(testOption)) {
        app.setApplicationName(app.applicationName() + QStringLiteral(" - test"));
    }
    DecodiumLogging::reopenDiagnosticLog();
    L(("Application identity: " + app.applicationName().toLocal8Bit()).constData());
    ensureLegacySqliteDatabase();

    QString configName = parser.value(configOption).trimmed();
    QSettings rootSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    // --rig-name sceglie la configurazione per QUESTA istanza soltanto.
    // Scriverla nella radice faceva ripartire anche la prima istanza dentro
    // il profilo della seconda: chi apriva un secondo Decodium si ritrovava
    // le impostazioni scambiate al riavvio successivo.
    bool const configFromRigName = configName.isEmpty() && !rigName.isEmpty();
    if (configFromRigName) {
        configName = rigName;
    }
    if (configName.isEmpty()) {
        configName = rootSettings.value(QStringLiteral("CurrentMultiSettingsConfiguration")).toString().trimmed();
    }
    if (!configName.isEmpty()) {
        app.setProperty("decodiumConfigName", configName);
        if (!configFromRigName) {
            rootSettings.setValue(QStringLiteral("CurrentMultiSettingsConfiguration"), configName);
            rootSettings.sync();
        }
    }
#ifdef Q_OS_WIN
    configureWindowsQmlDiskCache(configName);
#endif
    QString languageOverride = parser.value(languageOption).trimmed();

    QString const labCallsign = parser.value(labCallsignOption).trimmed().toUpper();
    QString const labGrid = parser.value(labGridOption).trimmed().toUpper();
    QString const labAudioDevice = parser.value(labAudioDeviceOption).trimmed();
    QString const labAudioInput = parser.value(labAudioInputOption).trimmed();
    QString const labAudioOutput = parser.value(labAudioOutputOption).trimmed();
    bool const labTxOutputLevelSpecified = parser.isSet(labTxOutputLevelOption);
    bool labTxOutputLevelOk = false;
    double const labTxOutputLevel = qBound(0.0,
                                           parser.value(labTxOutputLevelOption).trimmed().toDouble(&labTxOutputLevelOk),
                                           450.0);
    QString const labMode = parser.value(labModeOption).trimmed();
    qint64 const labDialHz = parser.value(labDialHzOption).trimmed().toLongLong();
    QString const effectiveLabInput = labAudioInput.isEmpty() ? labAudioDevice : labAudioInput;
    QString const effectiveLabOutput = labAudioOutput.isEmpty() ? labAudioDevice : labAudioOutput;
    if (!labCallsign.isEmpty()) {
        app.setProperty("decodiumLabCallsign", labCallsign);
    }
    if (!labGrid.isEmpty()) {
        app.setProperty("decodiumLabGrid", labGrid);
    }
    if (!labMode.isEmpty()) {
        app.setProperty("decodiumLabMode", labMode);
    }
    if (!effectiveLabInput.isEmpty()) {
        app.setProperty("decodiumLabAudioInput", effectiveLabInput);
    }
    if (!effectiveLabOutput.isEmpty()) {
        app.setProperty("decodiumLabAudioOutput", effectiveLabOutput);
    }
    if (labTxOutputLevelSpecified && labTxOutputLevelOk) {
        app.setProperty("decodiumLabTxOutputLevel", labTxOutputLevel);
    } else if (labTxOutputLevelSpecified) {
        qWarning() << "[LAB] invalid --lab-tx-output-level ignored";
    }
    bool const labNoCat = parser.isSet(labNoCatOption);
    if (labNoCat) {
        qputenv("DECODIUM_DISABLE_CAT", QByteArrayLiteral("1"));
        qInfo() << "[LAB] CAT auto-connect disabled by --lab-no-cat";
    }
    if (parser.isSet(labStandardWaitPounceMsOption)) {
        app.setProperty("decodiumLabPassiveWaitPounce", true);
    }
    // Standard FTx lab triggers intentionally bypass slot timing so short
    // tests can start immediately. Legacy minute modes search sync inside a
    // UTC-aligned 60-second window, so an arbitrary start would create a false
    // decoder failure. Let the bridge schedule those modes on a valid slot.
    QString const normalizedLabMode = labMode.trimmed().toUpper();
    bool const labMinuteSlotTest = normalizedLabMode == QStringLiteral("JT4")
        || normalizedLabMode == QStringLiteral("JT9")
        || normalizedLabMode == QStringLiteral("JT65")
        || normalizedLabMode == QStringLiteral("FST4")
        || normalizedLabMode == QStringLiteral("FST4W")
        || normalizedLabMode.startsWith(QStringLiteral("FST4-"))
        || normalizedLabMode.startsWith(QStringLiteral("FST4W-"));
    if ((parser.isSet(labSendTxMsOption) || parser.isSet(labSendTxPlanOption)) && !labMinuteSlotTest) {
        qputenv("DECODIUM_LAB_FORCE_TX_IMMEDIATE", QByteArrayLiteral("1"));
        qInfo() << "[LAB] immediate TX timing bypass enabled by lab standard TX trigger";
    } else if (labMinuteSlotTest && (parser.isSet(labSendTxMsOption) || parser.isSet(labSendTxPlanOption))) {
        qInfo() << "[LAB]" << normalizedLabMode
                << "TX slot timing preserved for decoder validation";
    }
    bool const labNoMonitor = parser.isSet(labNoMonitorOption);
    if (labNoMonitor) {
        qInfo() << "[LAB] RX monitor forced off by --lab-no-monitor";
    }
    auto const parseLabDelayMs = [&parser](QCommandLineOption const& option, int fallbackMs) {
        bool ok = false;
        int const value = parser.value(option).trimmed().toInt(&ok);
        if (!ok) {
            return fallbackMs;
        }
        return qMax(0, value);
    };
    int const labReapplyMs = parseLabDelayMs(labReapplyMsOption, 6500);
    int const labMonitorMs = parseLabDelayMs(labMonitorMsOption, 8000);
    int const labSstvRxMs = parseLabDelayMs(labSstvRxMsOption, 0);
    int const labSstvTxMs = parseLabDelayMs(labSstvTxMsOption, 0);
    int const labSstvTxWaitTimeoutMs = parseLabDelayMs(
        labSstvTxWaitTimeoutOption, 60'000);
    QString const labSstvMode = parser.value(labSstvModeOption).trimmed();
    QString const labSstvRxMode = parser.value(labSstvRxModeOption).trimmed();
    QString const labSstvRxReadyFile = parser.value(labSstvRxReadyFileOption).trimmed();
    QString const labSstvTxWaitFile = parser.value(labSstvTxWaitFileOption).trimmed();
    QString const labSstvImage = parser.value(labSstvImageOption).trimmed();
    bool const labSstvVox = parser.isSet(labSstvVoxOption);
    int const labBcastMs = parseLabDelayMs(labBcastMsOption, 14000);
    int const labDigiMs = parseLabDelayMs(labDigiMsOption, 14000);
    int const labQuitMs = parseLabDelayMs(labQuitMsOption, 0);
    QString labBcastText = parser.value(labBcastTextOption).trimmed();
    if (labBcastText.isEmpty()) {
        labBcastText = QStringLiteral("D4 LAB BCAST");
    }
    QString const labDigiTarget = parser.value(labDigiTargetOption).trimmed().toUpper();
    QString const labDigiBody = parser.value(labDigiBodyOption).trimmed().isEmpty()
        ? QStringLiteral("D4 LAB DIGI")
        : parser.value(labDigiBodyOption).trimmed();
    bool labDigiHopsOk = false;
    int const labDigiHops = qBound(
        0, parser.value(labDigiHopsOption).trimmed().toInt(&labDigiHopsOk), 9);
    int const labBeaconMs = parseLabDelayMs(labBeaconMsOption, 0);
    bool const labBeaconCq = parser.isSet(labBeaconCqOption);
    int const labCqSlotMs = parseLabDelayMs(labCqSlotMsOption, 0);
    int const labCqSlotId = qBound(-10,
                                   parser.value(labCqSlotIdOption).trimmed().toInt(),
                                   10);
    int const labCqSlotSize = qBound(100,
                                     parser.value(labCqSlotSizeOption).trimmed().toInt(),
                                     5000);
    int const labAutoCqMs = parseLabDelayMs(labAutoCqMsOption, 0);
    int const labAutoCqInterval = qMax(
        60,
        parser.value(labAutoCqIntervalOption).trimmed().toInt());
    int const labSendTxMs = parseLabDelayMs(labSendTxMsOption, 0);
    int const labSendTxSlot = qBound(
        1,
        parser.value(labSendTxSlotOption).trimmed().toInt(),
        6);
    QString const labSendTxPlan = parser.value(labSendTxPlanOption).trimmed();
    QString const labDxCall = parser.value(labDxCallOption).trimmed().toUpper();
    QString const labDxGrid = parser.value(labDxGridOption).trimmed().toUpper();
    int const labStandardAutoCqMs = parseLabDelayMs(labStandardAutoCqMsOption, 0);
    int const labStandardWaitPounceMs = parseLabDelayMs(labStandardWaitPounceMsOption, 0);
    bool const labTxPeriodSpecified = parser.isSet(labTxPeriodOption);
    int const labTxPeriod = qBound(0,
                                   parser.value(labTxPeriodOption).trimmed().toInt(),
                                   1);
    QString const labHeardCall = parser.value(labHeardCallOption).trimmed().toUpper();
    QString const labHeardGrid = parser.value(labHeardGridOption).trimmed().toUpper();
    QString labHeardName = parser.value(labHeardNameOption).trimmed();
    if (labHeardName.isEmpty()) {
        labHeardName = QStringLiteral("LAB");
    }
    int const labPingMs = parseLabDelayMs(labPingMsOption, 0);
    QString const labPingCall = parser.value(labPingCallOption).trimmed().toUpper();
    int const labPathMs = parseLabDelayMs(labPathMsOption, 0);
    QString const labPathTarget = parser.value(labPathTargetOption).trimmed().toUpper();
    int const labPathResponseMs = parseLabDelayMs(labPathResponseMsOption, 0);
    auto const parseLabProfile = [](QString value) {
        QString const normalized = value.trimmed().toUpper();
        if (normalized == QStringLiteral("1")
            || normalized == QStringLiteral("500")
            || normalized == QStringLiteral("W500")) {
            return 1;
        }
        return 2;
    };
    int const labPreferredProfile = parseLabProfile(parser.value(labProfileOption));
    bool const labSupportsW2300 = labPreferredProfile == 2;
    QString const labProfileName = labSupportsW2300 ? QStringLiteral("W2300") : QStringLiteral("W500");
    int const labConnectMs = parseLabDelayMs(labConnectMsOption, 0);
    QString const labConnectCall = parser.value(labConnectCallOption).trimmed().toUpper();
    QString const labConnectGrid = parser.value(labConnectGridOption).trimmed().toUpper();
    int const labTextMs = parseLabDelayMs(labTextMsOption, 0);
    QString labText = parser.value(labTextOption).trimmed();
    if (labText.isEmpty()) {
        labText = QStringLiteral("D4 LAB TEXT");
    }
    int const labQsyMs = parseLabDelayMs(labQsyMsOption, 0);
    int const labQsyOffsetHz = qBound(
        -5000,
        parser.value(labQsyOffsetHzOption).trimmed().toInt(),
        5000);
    bool const labAutoAcceptQsy = parser.isSet(labAutoAcceptQsyOption);
    int const labMailMs = parseLabDelayMs(labMailMsOption, 0);
    QString labMailSubject = parser.value(labMailSubjectOption).trimmed();
    if (labMailSubject.isEmpty()) {
        labMailSubject = QStringLiteral("D4 LAB MAIL");
    }
    QString labMailBody = parser.value(labMailBodyOption).trimmed();
    if (labMailBody.isEmpty()) {
        labMailBody = QStringLiteral("D4 LAB MAIL BODY");
    }
    int const labFormMs = parseLabDelayMs(labFormMsOption, 0);
    QString labFormType = parser.value(labFormTypeOption).trimmed().toUpper();
    if (labFormType.isEmpty()) {
        labFormType = QStringLiteral("ICS213");
    }
    QString labFormMessage = parser.value(labFormMessageOption).trimmed();
    if (labFormMessage.isEmpty()) {
        labFormMessage = QStringLiteral("D4 LAB FORM BODY");
    }
    int const labFileMs = parseLabDelayMs(labFileMsOption, 0);
    QString labFileName = parser.value(labFileNameOption).trimmed();
    if (labFileName.isEmpty()) {
        labFileName = QStringLiteral("d4-lab.txt");
    }
    QString labFileText = parser.value(labFileTextOption).trimmed();
    if (labFileText.isEmpty()) {
        labFileText = QStringLiteral("D4 LAB FILE BODY");
    }
    bool const labBbsServerEnable = parser.isSet(labBbsServerEnableOption);
    QString labBbsPublishName = parser.value(labBbsPublishNameOption).trimmed();
    if (labBbsPublishName.isEmpty()) {
        labBbsPublishName = QStringLiteral("bbs-lab.txt");
    }
    QString labBbsPublishText = parser.value(labBbsPublishTextOption);
    if (labBbsPublishText.trimmed().isEmpty()) {
        labBbsPublishText = QStringLiteral("D4 LAB BBS FILE BODY");
    }
    int const labBbsListMs = parseLabDelayMs(labBbsListMsOption, 0);
    int const labBbsGetMs = parseLabDelayMs(labBbsGetMsOption, 0);
    QString labBbsGetName = parser.value(labBbsGetNameOption).trimmed();
    if (labBbsGetName.isEmpty()) {
        labBbsGetName = labBbsPublishName;
    }
    int const labBbsServerListMs =
        parseLabDelayMs(labBbsServerListMsOption, 0);
    int const labBbsServerFileMs =
        parseLabDelayMs(labBbsServerFileMsOption, 0);
    int const labBulletinMs = parseLabDelayMs(labBulletinMsOption, 0);
    QString labBulletinGroup = parser.value(labBulletinGroupOption).trimmed().toUpper();
    if (labBulletinGroup.isEmpty()) {
        labBulletinGroup = QStringLiteral("NET");
    }
    QString labBulletinTitle = parser.value(labBulletinTitleOption).trimmed();
    if (labBulletinTitle.isEmpty()) {
        labBulletinTitle = QStringLiteral("D4 LAB BBS");
    }
    QString labBulletinBody = parser.value(labBulletinBodyOption).trimmed();
    if (labBulletinBody.isEmpty()) {
        labBulletinBody = QStringLiteral("D4 LAB BBS BODY");
    }
    int const labParkRelayMs = parseLabDelayMs(labParkRelayMsOption, 0);
    QString const labRelayTarget = parser.value(labRelayTargetOption).trimmed().toUpper();
    QString labRelaySubject = parser.value(labRelaySubjectOption).trimmed();
    if (labRelaySubject.isEmpty()) {
        labRelaySubject = QStringLiteral("D4 LAB RELAY");
    }
    QString labRelayBody = parser.value(labRelayBodyOption).trimmed();
    if (labRelayBody.isEmpty()) {
        labRelayBody = QStringLiteral("D4 LAB RELAY BODY");
    }
    int const labRelayTxMs = parseLabDelayMs(labRelayTxMsOption, 0);
    QString const ft2LinkRfLabDir =
        parser.value(ft2LinkRfLabDirOption).trimmed();
    QString const ft2LinkRfLabReplay =
        parser.value(ft2LinkRfLabReplayOption).trimmed();
    QString const ft2LinkRfLabSweepDir =
        parser.value(ft2LinkRfLabSweepDirOption).trimmed();
    QString const ft2LinkRfLabSweepProfiles =
        parser.value(ft2LinkRfLabSweepProfilesOption).trimmed();
    int const ft2LinkRfLabSweepMaxCases =
        qMax(0, parser.value(ft2LinkRfLabSweepMaxCasesOption).trimmed().toInt());
    if (languageOverride.isEmpty()) {
        languageOverride = rootSettings.value(QStringLiteral("UILanguage")).toString().trimmed();
    }
    if (languageOverride.startsWith(QStringLiteral("en_"), Qt::CaseInsensitive)
        || languageOverride.startsWith(QStringLiteral("en-"), Qt::CaseInsensitive)) {
        languageOverride = QStringLiteral("en");
        rootSettings.setValue(QStringLiteral("UILanguage"), languageOverride);
        rootSettings.sync();
    }
    if (!languageOverride.isEmpty()) {
        app.setProperty("decodiumLanguageOverride", languageOverride);
    }
    QLocale const uiLocale;
    L("l10n constructing");
    L10nLoader l10n {&app, uiLocale, languageOverride};
    L("l10n OK");

    // Single-instance detection: prevent multiple QML instances from running
    QDir tempDir{QStandardPaths::writableLocation(QStandardPaths::TempLocation)};
    QLockFile instanceLock{tempDir.absoluteFilePath(app.applicationName() + QStringLiteral("_qml.lock"))};
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock(500)) {
        L("Another instance is already running - exiting");
        return -1;
    }

    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        if (auto *instance = QCoreApplication::instance()) {
            instance->setProperty("decodiumShuttingDown", true);
        }
        g_shuttingDown.store(true, std::memory_order_relaxed);
        decodium::ft8::shutdownHashSeedWorker();
        decodium::ft4::shutdownHashSeedWorker();
    });

    L("bridge constructing");
    DecodiumBridge bridge;
    FT2LinkQmlAdapter ft2Link;
#if DECODIUM_HAS_RTTY
    // DecoRTTY: RTTY su VITA-49, FlexRadio diretto e FT-991A via gateway.
    // Tiene la propria sorgente audio e non tocca il percorso di Decodium:
    // FT8, FT4 e FT2 restano indipendenti da questo sottosistema.
    // Vedi doc/PIANO_INTEGRAZIONE_DECORTTY.md.
    decortty::DecoRttyHost rttyHost;
#endif
    auto labDialOverrideActive = std::make_shared<bool>(labDialHz > 0);
    auto applyLabRuntimeOverrides =
        [&bridge,
         &ft2Link,
         labCallsign,
         labGrid,
         effectiveLabInput,
         effectiveLabOutput,
         labTxOutputLevelSpecified,
         labTxOutputLevelOk,
         labTxOutputLevel,
         labMode,
         labDialHz,
         labDialOverrideActive,
         labDxCall,
         labDxGrid,
         labTxPeriodSpecified,
         labTxPeriod,
         labNoMonitor,
         labPreferredProfile,
         labSupportsW2300,
         labProfileName](QString const& reason) {
            bool active = false;
            if (!labCallsign.isEmpty()) {
                bridge.setCallsign(labCallsign);
                active = true;
            }
            if (!labGrid.isEmpty()) {
                bridge.setGrid(labGrid);
                active = true;
            }
            if (!effectiveLabInput.isEmpty()) {
                bridge.setAudioInputDevice(effectiveLabInput);
                active = true;
            }
            if (!effectiveLabOutput.isEmpty()) {
                bridge.setAudioOutputDevice(effectiveLabOutput);
                active = true;
            }
            if (labTxOutputLevelSpecified && labTxOutputLevelOk) {
                bridge.setTxOutputLevel(labTxOutputLevel);
                active = true;
            }
            bool modeAppliedWithDial = false;
            if (!labMode.isEmpty()) {
                if (labMode.compare(QStringLiteral("FT2-Link"), Qt::CaseInsensitive) == 0
                    && !bridge.ft2LinkAccessUnlocked()) {
                    QString const labPassword = qEnvironmentVariable("DECODIUM_FT2LINK_LAB_PASSWORD");
                    if (!labPassword.isEmpty()) {
                        bool const unlocked = bridge.verifyFt2LinkAccessPassword(labPassword);
                        qInfo() << "[LAB] FT2-Link unlock via environment ok=" << unlocked;
                    } else {
                        qInfo() << "[LAB] FT2-Link lab mode requested without DECODIUM_FT2LINK_LAB_PASSWORD";
                    }
                }
                if (labDialHz > 0 && labDialOverrideActive
                    && *labDialOverrideActive) {
                    bridge.qsyTo(static_cast<double>(labDialHz), labMode);
                    modeAppliedWithDial = true;
                } else {
                    bridge.setMode(labMode);
                }
                active = true;
            }
            if (labDialHz > 0 && labDialOverrideActive
                && *labDialOverrideActive && !modeAppliedWithDial) {
                bridge.qsyTo(static_cast<double>(labDialHz), QString());
                active = true;
            }
            if (!labDxCall.isEmpty()) {
                bridge.setDxCall(labDxCall);
                active = true;
            }
            if (!labDxGrid.isEmpty()) {
                bridge.setDxGrid(labDxGrid);
                active = true;
            }
            if (labTxPeriodSpecified) {
                bridge.setTxPeriod(labTxPeriod);
                active = true;
            }
            if (labNoMonitor) {
                if (bridge.monitoring()) {
                    bridge.stopMonitor();
                }
                active = true;
            }
            if (!labCallsign.isEmpty() || !labGrid.isEmpty()) {
                QString const adapterCall = labCallsign.isEmpty() ? bridge.callsign().trimmed().toUpper() : labCallsign;
                QString const adapterGrid = labGrid.isEmpty() ? bridge.grid().trimmed().toUpper() : labGrid;
                ft2Link.setLocalStation(adapterCall, adapterGrid, QStringLiteral("LAB"));
                ft2Link.setLocalCapabilities(true,
                                             labSupportsW2300,
                                             labSupportsW2300,
                                             labSupportsW2300,
                                             labPreferredProfile,
                                             0);
                active = true;
            }
            if (active) {
                qInfo().noquote()
                    << "[LAB] runtime overrides"
                    << reason
                    << "callsign=" << (labCallsign.isEmpty() ? QStringLiteral("<settings>") : labCallsign)
                    << "grid=" << (labGrid.isEmpty() ? QStringLiteral("<settings>") : labGrid)
                    << "audioIn=" << (effectiveLabInput.isEmpty() ? QStringLiteral("<settings>") : effectiveLabInput)
                    << "audioOut=" << (effectiveLabOutput.isEmpty() ? QStringLiteral("<settings>") : effectiveLabOutput)
                    << "txOutputLevel=" << (labTxOutputLevelSpecified && labTxOutputLevelOk
                                            ? QString::number(labTxOutputLevel, 'f', 1)
                                            : QStringLiteral("<settings>"))
                    << "mode=" << (labMode.isEmpty() ? QStringLiteral("<settings>") : labMode)
                    << "dialHz=" << (labDialHz > 0
                                      ? QString::number(labDialHz)
                                      : QStringLiteral("<settings>"))
                    << "dialReapply=" << (labDialOverrideActive && *labDialOverrideActive ? 1 : 0)
                    << "dxCall=" << (labDxCall.isEmpty() ? QStringLiteral("<settings>") : labDxCall)
                    << "dxGrid=" << (labDxGrid.isEmpty() ? QStringLiteral("<settings>") : labDxGrid)
                    << "txPeriod=" << (labTxPeriodSpecified ? QString::number(labTxPeriod) : QStringLiteral("<settings>"))
                    << "effectiveHz=" << qRound64(bridge.frequency())
                    << "monitor=" << (labNoMonitor ? QStringLiteral("forced-off") : QStringLiteral("<settings>"))
                    << "profile=" << labProfileName;
            }
            return active;
        };
    bool const labOverrideActive = applyLabRuntimeOverrides(QStringLiteral("initial"));
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     &ft2Link,
                     [&ft2Link]() {
                         ft2Link.stopDecodeWorker();
                     });
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     &ft2Link,
                     [&ft2Link]() {
                         ft2Link.saveLocalStore();
                     });
    QObject::connect(&ft2Link, &FT2LinkQmlAdapter::radioTxAudioRequested,
                     &bridge,
                     [&bridge](QString const& displayMessage,
                               QVector<float> const& samples,
                               QVariantMap const& plan) {
                         qInfo().noquote()
                             << "[Ft2Link][TXSIGNAL]"
                             << "display=" << displayMessage
                             << "profile=" << plan.value(QStringLiteral("profileName")).toString()
                             << "kind=" << plan.value(QStringLiteral("kind")).toString()
                             << "centerHz=" << plan.value(QStringLiteral("audioCenterHz")).toDouble()
                             << "tones=" << plan.value(QStringLiteral("audioToneHz")).toString()
                             << "carriers=" << plan.value(QStringLiteral("audioCarrierHz")).toString()
                             << "samples=" << samples.size()
                             << "plan=" << plan.value(QStringLiteral("audioPlan")).toString();
                         bridge.transmitFt2LinkAudio(displayMessage, samples, plan);
                     });
    QObject::connect(&bridge, &DecodiumBridge::ft2LinkRxSamplesReady,
                     &ft2Link,
                     [&ft2Link](QVector<short> const& samples, quint64 nowMs) {
                         ft2Link.ingestRxSamples(samples, QString {}, nowMs);
                     });
    // P0b worker-move (1.0.458): il decode live FT2-Link gira su un QThread
    // dedicato LowPriority; il main fa solo il dispatch dei chunk.
    ft2Link.startDecodeWorker();
    // P0b TX closed-loop (1.0.459): la coda TX FT2-Link avanza sull'evento
    // REALE di fine trasmissione, non solo sulla stima di durata.
    QObject::connect(&bridge, &DecodiumBridge::ft2LinkTxFinished,
                     &ft2Link, &FT2LinkQmlAdapter::notifyRadioTxFinished);
    auto logFt2LinkRfLabReport = [](QString const& label,
                                    QVariantMap const& report) {
        qInfo().noquote()
            << label
            << QString::fromUtf8(QJsonDocument::fromVariant(report).toJson(
                   QJsonDocument::Compact));
    };
    if (parser.isSet(ft2LinkRfLabDirOption)) {
        QTimer::singleShot(250, &ft2Link, [&ft2Link,
                                           ft2LinkRfLabDir,
                                           logFt2LinkRfLabReport]() {
            QVariantMap const report = ft2Link.runRfLabSelfTest(ft2LinkRfLabDir);
            logFt2LinkRfLabReport(QStringLiteral("[LAB][RFLAB] self-test"),
                                  report);
        });
    }
    if (parser.isSet(ft2LinkRfLabReplayOption)) {
        QTimer::singleShot(350, &ft2Link, [&ft2Link,
                                           ft2LinkRfLabReplay,
                                           logFt2LinkRfLabReport]() {
            QVariantMap options;
            options.insert(QStringLiteral("wide"), true);
            options.insert(QStringLiteral("applyToModel"), false);
            QVariantMap const report = ft2Link.replayRfLabWav(
                ft2LinkRfLabReplay,
                options);
            logFt2LinkRfLabReport(QStringLiteral("[LAB][RFLAB] replay"),
                                  report);
        });
    }
    if (parser.isSet(ft2LinkRfLabSweepDirOption)) {
        QTimer::singleShot(450, &ft2Link, [&ft2Link,
                                           ft2LinkRfLabSweepDir,
                                           ft2LinkRfLabSweepProfiles,
                                           ft2LinkRfLabSweepMaxCases,
                                           logFt2LinkRfLabReport]() {
            QVariantMap options;
            if (!ft2LinkRfLabSweepProfiles.isEmpty()) {
                options.insert(QStringLiteral("profiles"),
                               ft2LinkRfLabSweepProfiles);
            }
            if (ft2LinkRfLabSweepMaxCases > 0) {
                options.insert(QStringLiteral("maxCases"),
                               ft2LinkRfLabSweepMaxCases);
            }
            QVariantMap const report = ft2Link.runRfLabChannelSweep(
                ft2LinkRfLabSweepDir,
                options);
            logFt2LinkRfLabReport(QStringLiteral("[LAB][RFLAB] channel-sweep"),
                                  report);
        });
    }
    if (labOverrideActive) {
        QTimer::singleShot(labReapplyMs, &bridge, [applyLabRuntimeOverrides]() mutable {
            applyLabRuntimeOverrides(QStringLiteral("delayed"));
        });
    }
    if (parser.isSet(labMonitorMsOption) && !labNoMonitor) {
        QTimer::singleShot(labMonitorMs, &bridge, [&bridge, applyLabRuntimeOverrides]() mutable {
            applyLabRuntimeOverrides(QStringLiteral("pre-monitor"));
            if (!bridge.monitoring()) {
                bridge.startMonitor();
            }
            qInfo() << "[LAB] monitor requested active=" << bridge.monitoring();
        });
    } else if (parser.isSet(labMonitorMsOption) && labNoMonitor) {
        qInfo() << "[LAB] --lab-monitor-ms ignored because --lab-no-monitor is set";
    }
#if DECODIUM_HAS_SSTV
    auto writeLabSstvReadyFile = [](const QString& path,
                                    const QVariantMap& diagnostics) {
        if (path.isEmpty()) {
            return false;
        }
        QSaveFile readyFile(QFileInfo(path).absoluteFilePath());
        if (!readyFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qWarning() << "[LAB][SSTV] cannot open RX readiness file"
                       << readyFile.fileName() << readyFile.errorString();
            return false;
        }
        QVariantMap payload = diagnostics;
        payload.insert(QStringLiteral("readyUtc"),
                       QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        const QByteArray json = QJsonDocument::fromVariant(payload).toJson(
            QJsonDocument::Compact);
        if (readyFile.write(json) != json.size() || !readyFile.commit()) {
            qWarning() << "[LAB][SSTV] cannot commit RX readiness file"
                       << readyFile.fileName() << readyFile.errorString();
            return false;
        }
        qInfo().noquote() << "[LAB][SSTV] RX readiness file written"
                          << readyFile.fileName();
        return true;
    };
    if (parser.isSet(labSstvRxMsOption)) {
        QTimer::singleShot(labSstvRxMs, &bridge,
                           [&bridge,
                            labSstvRxMode,
                            labSstvRxReadyFile,
                            writeLabSstvReadyFile]() {
            if (!labSstvRxMode.isEmpty()) {
                const bool controlsAccepted = bridge.updateSstvRxControls(
                    {{QStringLiteral("modeControl"), QStringLiteral("manual")},
                     {QStringLiteral("manualMode"), labSstvRxMode}});
                qInfo().noquote()
                    << "[LAB][SSTV] RX manual mode requested mode="
                    << labSstvRxMode << "accepted=" << controlsAccepted;
            }
            const bool accepted = bridge.startSstvRx();
            qInfo().noquote()
                << "[LAB][SSTV] RX requested accepted=" << accepted
                << "state=" << bridge.sstvRxState()
                << "source=" << bridge.sstvRxSource();

            if (!labSstvRxReadyFile.isEmpty()) {
                // Running only proves that the state machine was armed.  Wait
                // for at least one callback from the dedicated capture worker
                // before releasing the TX side of an A/B test.  This avoids
                // declaring readiness while a stale BlackHole AudioQueue is
                // still being replaced.
                auto* readyTimer = new QTimer(&bridge);
                readyTimer->setInterval(200);
                auto readyElapsed = std::make_shared<QElapsedTimer>();
                readyElapsed->start();
                QObject::connect(
                    readyTimer, &QTimer::timeout, &bridge,
                    [readyTimer,
                     readyElapsed,
                     &bridge,
                     labSstvRxReadyFile,
                     writeLabSstvReadyFile]() mutable {
                    const QVariantMap diagnostics = bridge.sstvRxDiagnostics();
                    const bool active = diagnostics.value(QStringLiteral("active"))
                                            .toBool();
                    const bool workerRunning = diagnostics.value(
                        QStringLiteral("workerRunning")).toBool();
                    const qulonglong chunks = diagnostics.value(
                        QStringLiteral("chunksProcessed")).toULongLong();
                    if (active && workerRunning && chunks > 0U) {
                        writeLabSstvReadyFile(labSstvRxReadyFile, diagnostics);
                        readyTimer->stop();
                        readyTimer->deleteLater();
                        return;
                    }
                    if (readyElapsed->elapsed() >= 10'000) {
                        qWarning().noquote()
                            << "[LAB][SSTV] RX readiness timed out"
                            << QJsonDocument::fromVariant(diagnostics)
                                   .toJson(QJsonDocument::Compact);
                        readyTimer->stop();
                        readyTimer->deleteLater();
                    }
                });
                readyTimer->start();
            }
        });
        auto* sstvRxTraceTimer = new QTimer(&bridge);
        sstvRxTraceTimer->setInterval(5'000);
        QObject::connect(sstvRxTraceTimer, &QTimer::timeout, &bridge,
                         [&bridge]() {
            const QVariantMap diagnostics = bridge.sstvRxDiagnostics();
            qInfo().noquote()
                << "[LAB][SSTV] RX diagnostics"
                << QJsonDocument::fromVariant(diagnostics)
                       .toJson(QJsonDocument::Compact);
        });
        sstvRxTraceTimer->start();
    }
    if (parser.isSet(labSstvTxMsOption)) {
        QTimer::singleShot(labSstvTxMs, &bridge,
                           [&bridge,
                            labSstvMode,
                            labSstvImage,
                            labSstvVox,
                            labSstvTxWaitFile,
                            labSstvTxWaitTimeoutMs]() {
            auto startTxPreparation = [&bridge,
                                       labSstvMode,
                                       labSstvImage,
                                       labSstvVox]() {
            auto* studio = qobject_cast<decodium::sstv::SstvStudioController*>(
                bridge.sstvStudio());
            if (!studio) {
                qWarning() << "[LAB][SSTV] TX unavailable: studio controller missing";
                return;
            }
            if (labSstvVox) {
                if (QObject* cat = bridge.catManagerObj()) {
                    cat->setProperty("pttMethod",
                                     QStringLiteral("VOX"));
                    qInfo() << "[LAB][SSTV] VOX PTT forced on active CAT manager";
                }
            }
            studio->setModeId(labSstvMode.isEmpty()
                              ? QStringLiteral("martin-m1") : labSstvMode);
            const auto txStarted = std::make_shared<bool>(false);
            QObject::connect(
                studio, &decodium::sstv::SstvStudioController::sourceChanged,
                &bridge, [studio]() {
                    if (studio->sourceReady() && !studio->busy()) {
                        const bool accepted = studio->prepareImage();
                        qInfo() << "[LAB][SSTV] image preparation requested accepted="
                                << accepted;
                    }
                });
            QObject::connect(
                studio, &decodium::sstv::SstvStudioController::preparedChanged,
                &bridge, [&bridge, studio, txStarted]() {
                    if (*txStarted || !studio->preparedReady() || studio->busy()) {
                        return;
                    }
                    *txStarted = true;
                    qInfo().noquote()
                        << "[LAB][SSTV] TX preflight snapshot canStart="
                        << bridge.sstvTxCanStart()
                        << "legacy=" << bridge.legacyBackendActive()
                        << "transmitting=" << bridge.transmitting()
                        << "txRequested=" << bridge.txRequested()
                        << "pttPending=" << bridge.pttPending()
                        << "pttConfirmed=" << bridge.pttConfirmed()
                        << "output=" << bridge.audioOutputDevice();
                    const bool accepted = bridge.startSstvTx();
                    qInfo().noquote()
                        << "[LAB][SSTV] TX requested accepted=" << accepted
                        << "mode=" << studio->modeId()
                        << "output=" << bridge.audioOutputDevice();
                });

            bool accepted = false;
            if (!labSstvImage.isEmpty()) {
                accepted = studio->loadSource(
                    QUrl::fromLocalFile(QFileInfo(labSstvImage).absoluteFilePath()));
            } else {
                accepted = studio->generateCalibrationPattern();
            }
            qInfo().noquote()
                << "[LAB][SSTV] TX source requested accepted=" << accepted
                << "mode=" << studio->modeId()
                << "image=" << (labSstvImage.isEmpty()
                                  ? QStringLiteral("calibration-pattern")
                                  : labSstvImage);
            };

            if (labSstvTxWaitFile.isEmpty()) {
                startTxPreparation();
                return;
            }

            auto* waitTimer = new QTimer(&bridge);
            waitTimer->setInterval(200);
            auto waitElapsed = std::make_shared<QElapsedTimer>();
            waitElapsed->start();
            QObject::connect(
                waitTimer, &QTimer::timeout, &bridge,
                [waitTimer,
                 waitElapsed,
                 labSstvTxWaitFile,
                 labSstvTxWaitTimeoutMs,
                 startTxPreparation]() mutable {
                if (QFileInfo::exists(labSstvTxWaitFile)) {
                    qInfo().noquote() << "[LAB][SSTV] RX readiness observed; releasing TX"
                                      << labSstvTxWaitFile;
                    waitTimer->stop();
                    waitTimer->deleteLater();
                    startTxPreparation();
                    return;
                }
                if (labSstvTxWaitTimeoutMs > 0
                    && waitElapsed->elapsed() >= labSstvTxWaitTimeoutMs) {
                    qWarning().noquote()
                        << "[LAB][SSTV] TX readiness wait timed out"
                        << labSstvTxWaitFile;
                    waitTimer->stop();
                    waitTimer->deleteLater();
                }
            });
            qInfo().noquote() << "[LAB][SSTV] TX waiting for RX readiness file"
                              << labSstvTxWaitFile;
            waitTimer->start();
        });
    }
#endif
    if (parser.isSet(labSendTxMsOption)) {
        QTimer::singleShot(labSendTxMs, &bridge, [&bridge,
                                                  applyLabRuntimeOverrides,
                                                  labSendTxSlot]() mutable {
            applyLabRuntimeOverrides(QStringLiteral("pre-sendtx"));
            bridge.resetStandardTxMessages();
            bridge.sendTx(labSendTxSlot);
            qInfo().noquote()
                << "[LAB] auto SENDTX requested"
                << "slot=" << labSendTxSlot
                << "mode=" << bridge.mode()
                << "message=" << bridge.currentTxMessage();
        });
    }
    if (parser.isSet(labSendTxPlanOption)) {
        QStringList const entries = labSendTxPlan.split(',', Qt::SkipEmptyParts);
        for (QString const& rawEntry : entries) {
            QStringList const parts = rawEntry.trimmed().split(':', Qt::SkipEmptyParts);
            if (parts.size() != 2) {
                qWarning() << "[LAB] ignoring invalid sendtx plan entry" << rawEntry;
                continue;
            }
            bool okMs = false;
            bool okSlot = false;
            int const delayMs = parts.at(0).trimmed().toInt(&okMs);
            int const slot = qBound(1, parts.at(1).trimmed().toInt(&okSlot), 6);
            if (!okMs || !okSlot || delayMs < 0) {
                qWarning() << "[LAB] ignoring invalid sendtx plan entry" << rawEntry;
                continue;
            }
            QTimer::singleShot(delayMs,
                               &bridge,
                               [&bridge, applyLabRuntimeOverrides, delayMs, slot]() mutable {
                applyLabRuntimeOverrides(QStringLiteral("pre-sendtx-plan"));
                bridge.sendTx(slot);
                qInfo().noquote()
                    << "[LAB] TXPLAN fired"
                    << "ms=" << delayMs
                    << "slot=" << slot
                    << "mode=" << bridge.mode()
                    << "message=" << bridge.currentTxMessage();
            });
        }
    }
    if (parser.isSet(labStandardAutoCqMsOption)) {
        QTimer::singleShot(labStandardAutoCqMs,
                           &bridge,
                           [&bridge, applyLabRuntimeOverrides]() mutable {
            applyLabRuntimeOverrides(QStringLiteral("pre-standard-autocq"));
            bridge.setAutoSeq(true);
            bridge.setCurrentTx(6);
            bridge.setAutoCqRepeat(true);
            qInfo().noquote()
                << "[LAB] standard AutoCQ enabled"
                << "mode=" << bridge.mode()
                << "message=" << bridge.currentTxMessage();
        });
    }
    if (parser.isSet(labStandardWaitPounceMsOption)) {
        QTimer::singleShot(labStandardWaitPounceMs,
                           &bridge,
                           [&bridge, applyLabRuntimeOverrides]() mutable {
            applyLabRuntimeOverrides(QStringLiteral("pre-standard-wait-pounce"));
            bridge.setAutoSeq(true);
            bridge.setCurrentTx(6);
            bridge.setWaitPounceActive(true);
            bridge.setTxEnabled(false);
            qInfo().noquote()
                << "[LAB] standard passive WaitPounce armed"
                << "mode=" << bridge.mode()
                << "txEnabled=" << bridge.txEnabled()
                << "message=" << bridge.currentTxMessage();
        });
    }
    if (!labHeardCall.isEmpty()) {
        QTimer::singleShot(1000,
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labHeardCall,
                            labHeardGrid,
                            labHeardName,
                            labPreferredProfile,
                            labSupportsW2300]() mutable {
                               applyLabRuntimeOverrides(QStringLiteral("pre-heard"));
                               quint64 const nowMs = static_cast<quint64>(
                                   QDateTime::currentMSecsSinceEpoch());
                               bool const ok = ft2Link.observeStation(labHeardCall,
                                                                      labHeardGrid,
                                                                      labHeardName,
                                                                      true,
                                                                      true,
                                                                      labSupportsW2300,
                                                                      labSupportsW2300,
                                                                      labSupportsW2300,
                                                                      labPreferredProfile,
                                                                      0,
                                                                      nowMs);
                               qInfo().noquote()
                                   << "[LAB] seeded HEARD"
                                   << "ok=" << (ok ? 1 : 0)
                                   << "call=" << labHeardCall
                                   << "grid=" << labHeardGrid
                                   << "lastError=" << ft2Link.lastError();
                           });
    }
    if (parser.isSet(labBeaconMsOption)) {
        QTimer::singleShot(labBeaconMs,
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labBeaconCq]() mutable {
                               applyLabRuntimeOverrides(QStringLiteral("pre-beacon"));
                               ft2Link.setRadioTxArmed(true);
                               bool const ok = ft2Link.transmitBeaconRadio(
                                   labBeaconCq,
                                   static_cast<quint64>(
                                       QDateTime::currentMSecsSinceEpoch()));
                               qInfo().noquote()
                                   << "[LAB] auto BEACON requested"
                                   << "ok=" << (ok ? 1 : 0)
                                   << "cq=" << (labBeaconCq ? 1 : 0)
                                   << "lastError=" << ft2Link.lastError();
                           });
    }
    if (parser.isSet(labCqSlotMsOption)) {
        QTimer::singleShot(labCqSlotMs,
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labCqSlotId,
                            labCqSlotSize]() mutable {
                               applyLabRuntimeOverrides(QStringLiteral("pre-cq-slot"));
                               ft2Link.setRadioTxArmed(true);
                               bool const ok = ft2Link.transmitCqSlotRadio(
                                   labCqSlotId,
                                   labCqSlotSize,
                                   static_cast<quint64>(
                                       QDateTime::currentMSecsSinceEpoch()));
                               qInfo().noquote()
                                   << "[LAB] auto CQ_SLOT requested"
                                   << "ok=" << (ok ? 1 : 0)
                                   << "slot=" << labCqSlotId
                                   << "sizeHz=" << labCqSlotSize
                                   << "lastError=" << ft2Link.lastError();
                           });
    }
    if (parser.isSet(labAutoCqMsOption)) {
        QTimer::singleShot(labAutoCqMs,
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labAutoCqInterval]() mutable {
                               applyLabRuntimeOverrides(QStringLiteral("pre-auto-cq"));
                               ft2Link.setRadioTxArmed(true);
                               bool const ok = ft2Link.configureAutoBeacon(
                                   true,
                                   labAutoCqInterval,
                                   true,
                                   static_cast<quint64>(
                                       QDateTime::currentMSecsSinceEpoch()));
                               qInfo().noquote()
                                   << "[LAB] auto AUTO_CQ requested"
                                   << "ok=" << (ok ? 1 : 0)
                                   << "interval=" << labAutoCqInterval
                                   << "lastError=" << ft2Link.lastError();
                           });
    }
    if (parser.isSet(labPingMsOption)) {
        QTimer::singleShot(labPingMs,
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labPingCall]() mutable {
                               applyLabRuntimeOverrides(QStringLiteral("pre-ping"));
                               ft2Link.setRadioTxArmed(true);
                               bool const ok = !labPingCall.isEmpty()
                                   && ft2Link.transmitPingRadio(
                                       labPingCall,
                                       static_cast<quint64>(
                                           QDateTime::currentMSecsSinceEpoch()));
                               qInfo().noquote()
                                   << "[LAB] auto PING requested"
                                   << "ok=" << (ok ? 1 : 0)
                                   << "call=" << labPingCall
                                   << "lastError=" << ft2Link.lastError();
                           });
    }
    if (parser.isSet(labPathMsOption)) {
        QTimer::singleShot(labPathMs,
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labPathTarget]() mutable {
                               applyLabRuntimeOverrides(QStringLiteral("pre-path"));
                               ft2Link.setRadioTxArmed(true);
                               bool const ok = !labPathTarget.isEmpty()
                                   && ft2Link.transmitPathFinderRadio(
                                       labPathTarget,
                                       static_cast<quint64>(
                                           QDateTime::currentMSecsSinceEpoch()));
                               qInfo().noquote()
                                   << "[LAB] auto PATH requested"
                                   << "ok=" << (ok ? 1 : 0)
                                   << "target=" << labPathTarget
                                   << "lastError=" << ft2Link.lastError();
                           });
    }
    if (parser.isSet(labPathResponseMsOption)) {
        QTimer::singleShot(labPathResponseMs,
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labPathTarget]() mutable {
                               applyLabRuntimeOverrides(QStringLiteral("pre-path-response"));
                               ft2Link.setRadioTxArmed(true);
                               bool const ok = !labPathTarget.isEmpty()
                                   && ft2Link.transmitPathFinderResponseRadio(
                                       labPathTarget,
                                       static_cast<quint64>(
                                           QDateTime::currentMSecsSinceEpoch()));
                               qInfo().noquote()
                                   << "[LAB] auto PATH_RESPONSE requested"
                                   << "ok=" << (ok ? 1 : 0)
                                   << "target=" << labPathTarget
                                   << "lastError=" << ft2Link.lastError();
                           });
    }
    if (parser.isSet(labDigiEnableOption)) {
        QTimer::singleShot(qMax(0, labReapplyMs + 500),
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labDigiHops]() mutable {
                               applyLabRuntimeOverrides(QStringLiteral("pre-digi-enable"));
                               QVariantMap const result = ft2Link.configureDigipeater(
                                   true, labDigiHops);
                               qInfo().noquote()
                                   << "[LAB] digipeater enabled"
                                   << "state=" << QJsonDocument::fromVariant(result).toJson(
                                          QJsonDocument::Compact);
                           });
    }
    if (parser.isSet(labDigiMsOption)) {
        QTimer::singleShot(labDigiMs,
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labDigiTarget,
                            labDigiBody,
                            labDigiHops]() mutable {
                               applyLabRuntimeOverrides(QStringLiteral("pre-digi"));
                               ft2Link.setRadioTxArmed(true);
                               bool const ok = ft2Link.transmitDigipeaterRadio(
                                   labDigiTarget,
                                   labDigiBody,
                                   labDigiHops,
                                   static_cast<quint64>(
                                       QDateTime::currentMSecsSinceEpoch()));
                               qInfo().noquote()
                                   << "[LAB] auto DIGI requested"
                                   << "ok=" << (ok ? 1 : 0)
                                   << "target=" << labDigiTarget
                                   << "hops=" << labDigiHops
                                   << "body=" << labDigiBody
                                   << "lastError=" << ft2Link.lastError();
                           });
    }
    if (parser.isSet(labBcastMsOption)) {
        QTimer::singleShot(labBcastMs, &ft2Link, [&ft2Link, applyLabRuntimeOverrides, labBcastText]() mutable {
            applyLabRuntimeOverrides(QStringLiteral("pre-bcast"));
            ft2Link.setRadioTxArmed(true);
            bool const ok = ft2Link.transmitBroadcastRadio(
                labBcastText,
                static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()));
            qInfo().noquote()
                << "[LAB] auto BCAST requested"
                << "ok=" << (ok ? 1 : 0)
                << "text=" << labBcastText
                << "lastError=" << ft2Link.lastError();
        });
    }
    if (parser.isSet(labConnectMsOption)) {
        QTimer::singleShot(labConnectMs,
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labConnectCall,
                            labConnectGrid,
                            labPreferredProfile,
                            labSupportsW2300]() mutable {
                               applyLabRuntimeOverrides(QStringLiteral("pre-connect"));
                               quint64 const nowMs = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
                               bool observed = false;
                               if (!labConnectCall.isEmpty()) {
                                   observed = ft2Link.observeStation(labConnectCall,
                                                                     labConnectGrid,
                                                                     QStringLiteral("LAB"),
                                                                     true,
                                                                     true,
                                                                     labSupportsW2300,
                                                                     labSupportsW2300,
                                                                     labSupportsW2300,
                                                                     labPreferredProfile,
                                                                     0,
                                                                     nowMs);
                               }
                               ft2Link.setRadioTxArmed(true);
                               bool const ok = !labConnectCall.isEmpty()
                                   && ft2Link.startSessionRadioHandshake(labConnectCall, nowMs);
                               qInfo().noquote()
                                   << "[LAB] auto CONNECT requested"
                                   << "ok=" << (ok ? 1 : 0)
                                   << "observed=" << (observed ? 1 : 0)
                                   << "call=" << labConnectCall
                                   << "session=" << ft2Link.activeSessionId()
                                   << "lastError=" << ft2Link.lastError();
                           });
    }
    auto scheduleLabConnectedSessionTx =
        [&ft2Link, applyLabRuntimeOverrides](
            int delayMs,
            QString label,
            std::function<bool(quint16, quint64)> transmit) mutable {
            auto attempt = std::make_shared<std::function<void(int)>>();
            *attempt =
                [&ft2Link,
                 applyLabRuntimeOverrides,
                 label,
                 transmit,
                 attempt](int attemptNumber) mutable {
                    applyLabRuntimeOverrides(QStringLiteral("pre-") + label.toLower());
                    quint16 const sessionId = ft2Link.activeSessionId();
                    QVariantMap const session = sessionId == 0u
                        ? QVariantMap {}
                        : ft2Link.sessionInfo(sessionId);
                    QString const state = session.value(QStringLiteral("stateName")).toString();
                    if (sessionId == 0u || state != QStringLiteral("Connected")) {
                        qInfo().noquote()
                            << "[LAB] auto" << label << "waiting"
                            << "attempt=" << attemptNumber
                            << "session=" << sessionId
                            << "state=" << (state.isEmpty()
                                            ? QStringLiteral("<none>")
                                            : state);
                        if (attemptNumber < 20) {
                            QTimer::singleShot(1500, &ft2Link, [attempt, attemptNumber] {
                                (*attempt)(attemptNumber + 1);
                            });
                        }
                        return;
                    }
                    ft2Link.setRadioTxArmed(true);
                    quint64 const nowMs = static_cast<quint64>(
                        QDateTime::currentMSecsSinceEpoch());
                    bool const ok = transmit(sessionId, nowMs);
                    QVariantMap const after = ft2Link.sessionInfo(sessionId);
                    qInfo().noquote()
                        << "[LAB] auto" << label << "requested"
                        << "ok=" << (ok ? 1 : 0)
                        << "session=" << sessionId
                        << "state=" << after.value(QStringLiteral("stateName")).toString()
                        << "profile=" << after.value(QStringLiteral("profileName")).toString()
                        << "lastError=" << ft2Link.lastError();
                };
            QTimer::singleShot(delayMs, &ft2Link, [attempt] {
                (*attempt)(0);
            });
        };
    if (labBbsServerEnable) {
        QTimer::singleShot(qMax(0, labReapplyMs + 900),
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labBbsPublishName,
                            labBbsPublishText]() mutable {
                               applyLabRuntimeOverrides(
                                   QStringLiteral("pre-bbs-server"));
                               QVariantMap const enabled =
                                   ft2Link.configureBbsFileServer(true);
                               QVariantMap const published =
                                   ft2Link.publishBbsSharedFileText(
                                       labBbsPublishName,
                                       labBbsPublishText,
                                       static_cast<quint64>(
                                           QDateTime::currentMSecsSinceEpoch()));
                               qInfo().noquote()
                                   << "[LAB] BBS server configured"
                                   << "enabled="
                                   << (enabled.value(QStringLiteral("enabled")).toBool()
                                       ? 1
                                       : 0)
                                   << "file=" << labBbsPublishName
                                   << "ok="
                                   << (published.value(QStringLiteral("ok")).toBool()
                                       ? 1
                                       : 0)
                                   << "lastError=" << ft2Link.lastError();
                           });
    }
    if (parser.isSet(labBbsListMsOption)) {
        scheduleLabConnectedSessionTx(
            labBbsListMs,
            QStringLiteral("BBS_LIST_REQ"),
            [&ft2Link](quint16 sessionId, quint64 nowMs) {
                return ft2Link.requestBbsFileListRadio(sessionId, nowMs);
            });
    }
    if (parser.isSet(labBbsGetMsOption)) {
        scheduleLabConnectedSessionTx(
            labBbsGetMs,
            QStringLiteral("BBS_FILE_REQ"),
            [&ft2Link, labBbsGetName](quint16 sessionId, quint64 nowMs) {
                return ft2Link.requestBbsFileRadio(
                    sessionId, labBbsGetName, nowMs);
            });
    }
    if (parser.isSet(labBbsServerListMsOption)) {
        scheduleLabConnectedSessionTx(
            labBbsServerListMs,
            QStringLiteral("BBS_LIST_TX"),
            [&ft2Link](quint16 sessionId, quint64 nowMs) {
                return ft2Link.transmitBbsSharedFileListRadio(
                    sessionId, nowMs);
            });
    }
    if (parser.isSet(labBbsServerFileMsOption)) {
        scheduleLabConnectedSessionTx(
            labBbsServerFileMs,
            QStringLiteral("BBS_FILE_TX"),
            [&ft2Link, labBbsPublishName](quint16 sessionId, quint64 nowMs) {
                return ft2Link.transmitBbsSharedFileRadio(
                    sessionId, labBbsPublishName, nowMs);
            });
    }
    struct LabQsyState
    {
        quint16 pendingSessionId {0u};
        qint64 pendingDialHz {0};
        quint64 startedAtMs {0u};
        QStringList processedMessageKeys;
    };
    auto labQsyState = std::make_shared<LabQsyState>();
    labQsyState->startedAtMs = static_cast<quint64>(
        QDateTime::currentMSecsSinceEpoch());
    auto const labApplyQsy =
        [&bridge, labDialOverrideActive](qint64 targetHz, QString const& reason) {
            qint64 const beforeHz = qRound64(bridge.frequency());
            if (labDialOverrideActive) {
                *labDialOverrideActive = false;
            }
            bridge.qsyTo(static_cast<double>(targetHz), QStringLiteral("FT2-Link"));
            qint64 const afterHz = qRound64(bridge.frequency());
            qInfo().noquote()
                << "[LAB] QSY apply"
                << "reason=" << reason
                << "targetHz=" << targetHz
                << "beforeHz=" << beforeHz
                << "afterHz=" << afterHz;
        };
    auto const labScheduleQsyApply =
        [&ft2Link, &bridge, labApplyQsy](qint64 targetHz,
                                         QString const& reason) {
            QVariantMap const plan = ft2Link.lastRadioTxPlan();
            int delayMs = qMax(
                1200,
                qRound(plan.value(QStringLiteral("audioSeconds")).toDouble()
                       * 1000.0) + 900);
            qInfo().noquote()
                << "[LAB] QSY apply queued"
                << "reason=" << reason
                << "targetHz=" << targetHz
                << "delayMs=" << delayMs;
            QTimer::singleShot(delayMs, &bridge, [labApplyQsy, targetHz, reason] {
                labApplyQsy(targetHz, reason);
            });
        };
    if (labAutoAcceptQsy || parser.isSet(labQsyMsOption)) {
        QObject::connect(&ft2Link,
                         &FT2LinkQmlAdapter::messagesChanged,
                         &bridge,
                         [&ft2Link,
                          &bridge,
                          labQsyState,
                          labAutoAcceptQsy,
                          labApplyQsy,
                          labScheduleQsyApply](quint16 sessionId) mutable {
            QVariantList const messages = ft2Link.chatLog(sessionId);
            for (QVariant const& value : messages) {
                QVariantMap const message = value.toMap();
                QString const direction =
                    message.value(QStringLiteral("directionName")).toString();
                if (direction != QStringLiteral("Incoming")) {
                    continue;
                }
                QString const text = message.value(QStringLiteral("text")).toString();
                quint64 const atMs = message.value(QStringLiteral("atMs")).toULongLong();
                if (atMs > 0u && atMs < labQsyState->startedAtMs) {
                    continue;
                }
                QString const upper = text.toUpper();
                QString const key = QStringLiteral("%1|%2|%3|%4")
                    .arg(sessionId)
                    .arg(direction, text)
                    .arg(static_cast<qulonglong>(atMs));
                if (labQsyState->processedMessageKeys.contains(key)) {
                    continue;
                }

                if ((upper.contains(QStringLiteral("<QSYJ>"))
                     || upper.contains(QStringLiteral("<QJO>")))
                    && labQsyState->pendingSessionId == sessionId) {
                    labQsyState->processedMessageKeys.push_back(key);
                    qInfo().noquote()
                        << "[LAB] QSY rejected"
                        << "session=" << sessionId
                        << "text=" << text;
                    labQsyState->pendingSessionId = 0u;
                    labQsyState->pendingDialHz = 0;
                    continue;
                }

                if (upper.contains(QStringLiteral("<QSYR>"))
                    && labQsyState->pendingSessionId == sessionId
                    && labQsyState->pendingDialHz > 0) {
                    labQsyState->processedMessageKeys.push_back(key);
                    qint64 const targetHz = labQsyState->pendingDialHz;
                    labQsyState->pendingSessionId = 0u;
                    labQsyState->pendingDialHz = 0;
                    labApplyQsy(targetHz, QStringLiteral("remote-accepted"));
                    continue;
                }

                if (!labAutoAcceptQsy) {
                    continue;
                }

                QVariantMap const plan = ft2Link.qsyPlanForText(
                    text, qRound64(bridge.frequency()));
                if (!plan.value(QStringLiteral("valid")).toBool()) {
                    continue;
                }
                labQsyState->processedMessageKeys.push_back(key);
                qint64 const targetHz =
                    plan.value(QStringLiteral("dialFrequencyHz")).toLongLong();
                bool const allowed = plan.value(QStringLiteral("allowed")).toBool();
                QString const response = allowed
                    ? QStringLiteral("<QSYR>")
                    : QStringLiteral("<QJO>");
                ft2Link.setRadioTxArmed(true);
                bool const ok = ft2Link.transmitPreparedRadioTxAudio(
                    sessionId,
                    response,
                    static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()));
                qInfo().noquote()
                    << "[LAB] QSY auto-accept"
                    << "ok=" << (ok ? 1 : 0)
                    << "session=" << sessionId
                    << "targetHz=" << targetHz
                    << "allowed=" << (allowed ? 1 : 0)
                    << "response=" << response
                    << "lastError=" << ft2Link.lastError();
                if (ok && allowed && targetHz > 0) {
                    labScheduleQsyApply(targetHz, QStringLiteral("local-accepted"));
                }
            }
            if (labQsyState->processedMessageKeys.size() > 200) {
                labQsyState->processedMessageKeys =
                    labQsyState->processedMessageKeys.mid(
                        labQsyState->processedMessageKeys.size() - 100);
            }
        });
    }
    if (parser.isSet(labParkRelayMsOption)) {
        QTimer::singleShot(labParkRelayMs,
                           &ft2Link,
                           [&ft2Link,
                            applyLabRuntimeOverrides,
                            labRelayTarget,
                            labRelaySubject,
                            labRelayBody]() mutable {
                               applyLabRuntimeOverrides(QStringLiteral("pre-park-relay"));
                               bool const ok = !labRelayTarget.isEmpty()
                                   && ft2Link.parkMailboxTyped(
                                       labRelayTarget,
                                       labRelaySubject,
                                       labRelayBody,
                                       false,
                                       false,
                                       static_cast<quint64>(
                                           QDateTime::currentMSecsSinceEpoch()));
                               qInfo().noquote()
                                   << "[LAB] auto PARK_RELAY requested"
                                   << "ok=" << (ok ? 1 : 0)
                                   << "target=" << labRelayTarget
                                   << "subject=" << labRelaySubject
                                   << "lastError=" << ft2Link.lastError();
                           });
    }
    if (parser.isSet(labRelayTxMsOption)) {
        scheduleLabConnectedSessionTx(
            labRelayTxMs,
            QStringLiteral("RELAY"),
            [&ft2Link,
             labRelayTarget,
             labRelaySubject,
             labRelayBody](quint16 sessionId, quint64 nowMs) {
                quint32 mailboxId = 0u;
                for (QVariant const& value : ft2Link.mailbox()) {
                    QVariantMap const item = value.toMap();
                    if (item.value(QStringLiteral("toCall")).toString() == labRelayTarget
                        && item.value(QStringLiteral("subject")).toString() == labRelaySubject
                        && item.value(QStringLiteral("body")).toString() == labRelayBody) {
                        mailboxId = item.value(QStringLiteral("id")).toUInt();
                        break;
                    }
                }
                return mailboxId != 0u
                    && ft2Link.transmitRelayMailboxRadio(
                        sessionId,
                        mailboxId,
                        nowMs);
            });
    }
    if (parser.isSet(labTextMsOption)) {
        auto textAttempt = std::make_shared<std::function<void(int)>>();
        *textAttempt =
            [&ft2Link, applyLabRuntimeOverrides, labText, textAttempt](int attempt) mutable {
                applyLabRuntimeOverrides(QStringLiteral("pre-text"));
                quint16 const sessionId = ft2Link.activeSessionId();
                QVariantMap const session = sessionId == 0u
                    ? QVariantMap {}
                    : ft2Link.sessionInfo(sessionId);
                QString const state = session.value(QStringLiteral("stateName")).toString();
                if (sessionId == 0u || state != QStringLiteral("Connected")) {
                    qInfo().noquote()
                        << "[LAB] auto TEXT waiting"
                        << "attempt=" << attempt
                        << "session=" << sessionId
                        << "state=" << (state.isEmpty() ? QStringLiteral("<none>") : state);
                    if (attempt < 12) {
                        QTimer::singleShot(1500, &ft2Link, [textAttempt, attempt] {
                            (*textAttempt)(attempt + 1);
                        });
                    }
                    return;
                }
                ft2Link.setRadioTxArmed(true);
                bool const ok = ft2Link.transmitPreparedRadioTxAudio(
                    sessionId,
                    labText,
                    static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()));
                QVariantMap const after = ft2Link.sessionInfo(sessionId);
                qInfo().noquote()
                    << "[LAB] auto TEXT requested"
                    << "ok=" << (ok ? 1 : 0)
                    << "session=" << sessionId
                    << "state=" << after.value(QStringLiteral("stateName")).toString()
                    << "profile=" << after.value(QStringLiteral("profileName")).toString()
                    << "text=" << labText
                    << "lastError=" << ft2Link.lastError();
            };
        QTimer::singleShot(labTextMs, &ft2Link, [textAttempt] {
            (*textAttempt)(0);
        });
    }
    if (parser.isSet(labQsyMsOption)) {
        scheduleLabConnectedSessionTx(
            labQsyMs,
            QStringLiteral("QSY"),
            [&ft2Link,
             &bridge,
             labQsyOffsetHz,
             labQsyState](quint16 sessionId, quint64 nowMs) {
                if (labQsyOffsetHz == 0) {
                    qInfo() << "[LAB] QSY invite rejected: zero offset";
                    return false;
                }
                qint64 const currentHz = qRound64(bridge.frequency());
                qint64 const proposedTargetHz = currentHz + labQsyOffsetHz;
                QString const tag = ft2Link.qsyFrequencyTag(proposedTargetHz);
                if (tag.isEmpty()) {
                    qInfo() << "[LAB] QSY invite rejected: empty frequency tag";
                    return false;
                }
                QVariantMap const plan = ft2Link.qsyPlanForText(tag, currentHz);
                qint64 const targetHz =
                    plan.value(QStringLiteral("dialFrequencyHz")).toLongLong();
                bool const allowed =
                    plan.value(QStringLiteral("allowed")).toBool();
                if (!plan.value(QStringLiteral("valid")).toBool()
                    || !allowed
                    || targetHz <= 0) {
                    qInfo().noquote()
                        << "[LAB] QSY invite rejected before TX"
                        << "session=" << sessionId
                        << "currentHz=" << currentHz
                        << "offsetHz=" << labQsyOffsetHz
                        << "targetHz=" << targetHz
                        << "allowed=" << (allowed ? 1 : 0)
                        << "plan=" << plan;
                    return false;
                }
                bool const ok = ft2Link.transmitPreparedRadioTxAudio(
                    sessionId,
                    tag,
                    nowMs);
                if (ok) {
                    labQsyState->pendingSessionId = sessionId;
                    labQsyState->pendingDialHz = targetHz;
                }
                qInfo().noquote()
                    << "[LAB] QSY invite requested"
                    << "ok=" << (ok ? 1 : 0)
                    << "session=" << sessionId
                    << "currentHz=" << currentHz
                    << "offsetHz=" << labQsyOffsetHz
                    << "targetHz=" << targetHz
                    << "tag=" << tag
                    << "lastError=" << ft2Link.lastError();
                return ok;
            });
    }
    if (parser.isSet(labMailMsOption)) {
        scheduleLabConnectedSessionTx(
            labMailMs,
            QStringLiteral("MAIL"),
            [&ft2Link, labMailSubject, labMailBody](quint16 sessionId, quint64 nowMs) {
                return ft2Link.transmitMailboxRadioTyped(
                    sessionId,
                    QString {},
                    labMailSubject,
                    labMailBody,
                    false,
                    false,
                    nowMs);
            });
    }
    if (parser.isSet(labFormMsOption)) {
        scheduleLabConnectedSessionTx(
            labFormMs,
            QStringLiteral("FORM"),
            [&ft2Link, labFormType, labFormMessage](quint16 sessionId, quint64 nowMs) {
                QVariantMap fields;
                fields.insert(QStringLiteral("message"), labFormMessage);
                return ft2Link.transmitFormRadio(
                    sessionId,
                    QString {},
                    labFormType,
                    fields,
                    nowMs);
            });
    }
    if (parser.isSet(labFileMsOption)) {
        scheduleLabConnectedSessionTx(
            labFileMs,
            QStringLiteral("FILE"),
            [&ft2Link, labFileName, labFileText](quint16 sessionId, quint64 nowMs) {
                return ft2Link.transmitFileRadio(
                    sessionId,
                    QString {},
                    labFileName,
                    labFileText,
                    nowMs);
            });
    }
    if (parser.isSet(labBulletinMsOption)) {
        scheduleLabConnectedSessionTx(
            labBulletinMs,
            QStringLiteral("BBS"),
            [&ft2Link,
             labBulletinGroup,
             labBulletinTitle,
             labBulletinBody](quint16 sessionId, quint64 nowMs) {
                return ft2Link.transmitBulletinRadio(
                    sessionId,
                    labBulletinGroup,
                    labBulletinTitle,
                    labBulletinBody,
                    nowMs);
            });
    }
    if (parser.isSet(labQuitMsOption)) {
        QTimer::singleShot(labQuitMs, &app, [&app] {
            qInfo() << "[LAB] quit requested";
            app.quit();
        });
    }
    app.setProperty("decodiumBridge", QVariant::fromValue<QObject*>(&bridge));
#ifdef Q_OS_WIN
    QObject::connect(&bridge, &DecodiumBridge::mainQmlReadyForNativeWindowing, &app,
                     [&app, appIcon] {
                         applyApplicationIconToTopLevelWindows(appIcon);
                         for (int delayMs : {100, 500, 1500, 3000, 7000}) {
                             QTimer::singleShot(delayMs, &app, [appIcon] {
                                 applyApplicationIconToTopLevelWindows(appIcon);
                             });
                         }
                     });
#endif
    if (DecodiumLogging::instance())
        DecodiumLogging::instance()->logStartupDiagnostics();
    L("bridge OK");

    // 1.0.364+ — MAM multi-stream nativo (FASE 1): hook di verifica env-gated.
    // Con DECODIUM_MAM_DUMP impostato, genera un WAV multi-stream FT8 di test e
    // lo scrive su disco. Gated da env var => zero impatto in produzione; utile
    // per l'analisi spettrale offline delle fasi 2-3.
    if (qEnvironmentVariableIsSet("DECODIUM_MAM_DUMP")) {
        bridge.mamDumpTestWav(QStringLiteral("C:/decodium-4.0/_mam_test.wav"),
                              {QStringLiteral("CQ TEST AA1AAA"),
                               QStringLiteral("BB2BBB AA1AAA -10"),
                               QStringLiteral("CC3CCC AA1AAA RR73")},
                              {600, 1200, 1800});
        L("MAM dump test WAV requested via DECODIUM_MAM_DUMP");
    }

    // Keep the bridge alive longer than the QML engine. During shutdown QML
    // bindings still reevaluate while root objects are being torn down.
    // If the context object dies first, QML sees bridge == null and floods the
    // terminal with TypeError messages on exit.
    bool const runningFromAppImage =
        qEnvironmentVariableIsSet("APPIMAGE")
        || qEnvironmentVariableIsSet("APPDIR")
        || QCoreApplication::applicationDirPath().startsWith(QStringLiteral("/tmp/.mount_"));

    // Keep QML disk cache enabled by default. Disabling it forces every launch
    // to recompile QML/JS and is expensive on Windows when antivirus scans the
    // installed Qt/QML tree. AppImages are the exception: the executable lives
    // in a transient mount path, and stale QML bytecode has caused startup
    // failures after replacing the AppImage.
    if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_QML_CACHE")) {
        qputenv("QML_DISABLE_DISK_CACHE", "1");
        L("QML disk cache disabled by DECODIUM_DISABLE_QML_CACHE");
    } else if (qEnvironmentVariableIsSet("QML_DISABLE_DISK_CACHE")) {
        L("QML disk cache disabled by environment");
    } else if (runningFromAppImage) {
        qputenv("QML_DISABLE_DISK_CACHE", "1");
        L("QML disk cache disabled for AppImage runtime");
    } else {
        L("QML disk cache enabled");
    }
    logEnvVar("QML_DISABLE_DISK_CACHE");
    logEnvVar("QML_DISK_CACHE_PATH");
    L(("Qt CacheLocation: " + QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toLocal8Bit()).constData());
    L(("Qt GenericCacheLocation: " + QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation).toLocal8Bit()).constData());
    L(("Qt AppDataLocation: " + QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toLocal8Bit()).constData());
    L(("Qt AppLocalDataLocation: " + QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toLocal8Bit()).constData());
    L(("Qt TempLocation: " + QStandardPaths::writableLocation(QStandardPaths::TempLocation).toLocal8Bit()).constData());

    QQmlApplicationEngine engine;
    L("engine OK");
#if DECODIUM_HAS_SSTV
    engine.addImageProvider(QStringLiteral("decodium-sstv"),
                            new DecodiumSstvImageProvider(&bridge));
    // QQmlEngine owns both providers. The gallery provider's destructor
    // cancels responses and joins its bounded decode worker; the bridge keeps
    // only a QPointer and is constructed before/destroyed after this engine.
    auto* sstvThumbnailProvider
        = new decodium::sstv::SstvThumbnailProvider();
    bridge.setSstvThumbnailProvider(sstvThumbnailProvider);
    engine.addImageProvider(QStringLiteral("decodium-sstv-gallery"),
                            sstvThumbnailProvider);
#endif
    engine.setOutputWarningsToStandardError(true);  // show QML errors on stderr for debugging
    QStringList qmlWarningMessages;
    QObject::connect(&engine, &QQmlEngine::warnings, &app,
                     [&bridge, &qmlWarningMessages] (const QList<QQmlError>& warnings) {
        logQmlWarnings(warnings);
        for (auto const& w : warnings) {
            QString const warning = w.toString();
            qmlWarningMessages.push_back(warning);
            qWarning("QML WARNING: %s", qPrintable(warning));
            // Feed warnings to the in-app diagnostics system
            if (auto *diag = qobject_cast<DecodiumDiagnostics*>(bridge.diagnostics()))
                diag->addQmlWarning(warning);
        }
    });
    QString const appQmlPath = QCoreApplication::applicationDirPath() + QStringLiteral("/qml");
    QString const bundledQtQmlPath = QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../Resources/qml"));
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
    engine.addImportPath(appQmlPath);
    if (QDir(bundledQtQmlPath).exists()) {
        engine.addImportPath(QDir(bundledQtQmlPath).canonicalPath());
    }
    {
        QStringList importPaths = engine.importPathList();
        auto promoteImportPath = [&importPaths](const QString& path) {
            QString normalized = path;
            if (QDir(path).exists()) {
                normalized = QDir(path).canonicalPath();
            }
            qsizetype index = importPaths.indexOf(normalized);
            if (index < 0 && normalized != path) {
                index = importPaths.indexOf(path);
            }
            if (index > 0) {
                importPaths.move(index, 0);
            }
        };
        promoteImportPath(appQmlPath);
        if (QDir(bundledQtQmlPath).exists()) {
            promoteImportPath(QDir(bundledQtQmlPath).canonicalPath());
        }
        engine.setImportPathList(importPaths);
    }
    for (const QString& importPath : engine.importPathList()) {
        L(("QML import path: " + importPath.toLocal8Bit()).constData());
    }

    engine.rootContext()->setContextProperty("bridge", &bridge);
    engine.rootContext()->setContextProperty("appEngine", &bridge);
    engine.rootContext()->setContextProperty("ft2Link", &ft2Link);
#if DECODIUM_HAS_RTTY
    {
        // Le impostazioni RTTY usano le stesse chiavi del progetto originale,
        // cosi' chi viene da DecoRTTY ritrova la propria configurazione.
        QSettings rttySettings {QSettings::IniFormat, QSettings::UserScope,
                                QStringLiteral("Decodium"), QStringLiteral("Decodium")};
        // La radio di RTTY e' quella di Decodium: frequenza, modo e PTT
        // arrivano dal CAT dell'applicazione attraverso questi ganci, come
        // gia' fa DecoPort. Il trasporto di rete del progetto originale —
        // VITA-49 verso un FlexRadio, il gateway per una FT-991A altrove —
        // e' stato tolto: cercava in rete una radio che qui sta su una porta
        // seriale, e non poteva che non trovarla.
        decortty::link::DecodiumLink::Ganci ganci;
        ganci.connesso           = [&bridge] { return bridge.catConnected (); };
        ganci.nomeRadio          = [&bridge] { return bridge.catRigName (); };
        ganci.frequenzaHz        = [&bridge] { return bridge.frequency (); };
        ganci.modo               = [&bridge] { return bridge.catMode (); };
        // Comprende il PTT che RTTY stessa ha alzato: il motore ferma la
        // trasmissione se la radio dice di non essere in aria, e transmitting()
        // da solo riguarda il sequencer dei modi digitali, non questo PTT.
        ganci.inTrasmissione     = [&bridge] {
            return bridge.transmitting () || bridge.decoPortRemoteKeyed ();
        };
        // La trasmissione passa dalla stessa uscita che Decodium apre per
        // l'audio dei client DecoPort: una sola strada verso la radio, con i
        // ritegni gia' scritti — non suona nulla mentre il sequencer dei modi
        // digitali sta trasmettendo o accordando, e il PTT viene rifiutato se
        // il CAT non c'e'.
        ganci.puoTrasmettere     = [&bridge] {
            return bridge.catConnected () && !bridge.transmitting ();
        };
        ganci.impostaPtt         = [&bridge] (bool on) { bridge.rttyAlzaPtt (on); };
        ganci.mandaAudioTx       = [&bridge] (QVector<short> const& c) {
            bridge.rttyMandaAudioTx (c);
        };
        ganci.impostaFrequenzaHz = [&bridge] (double hz) { bridge.setFrequency (hz); };
        // Questo e' un modo della RADIO, non dell'applicazione. setMode()
        // accetta RTTY/FT8/FT4 ecc. e scarta correttamente DATA-U come modo
        // applicativo; usarlo qui faceva quindi apparire "Set radio" riuscito
        // senza mandare alcun comando CAT all'apparato.
        ganci.impostaModo        = [&bridge] (QString const& m) {
            bridge.impostaModoRadioRtty (m);
        };
        rttyHost.impostaGanciRadio (std::move (ganci));

        rttyHost.avvia (rttySettings);

        // Il sottosistema originale aveva un secondo profilo stazione e una
        // seconda lingua. Dentro Decodium non devono esistere due verita': le
        // macro RTTY prendono nominativo, nome/etichetta e QTH dal profilo
        // generale e si aggiornano immediatamente quando quel profilo cambia.
        auto sincronizzaProfiloRtty = [&bridge, &rttyHost] {
            rttyHost.macro ().setStationProfile (bridge.callsign (),
                                                  bridge.stationName (),
                                                  bridge.stationQth ());
        };
        sincronizzaProfiloRtty ();
        QObject::connect (&bridge, &DecodiumBridge::callsignChanged,
                          &rttyHost, sincronizzaProfiloRtty);
        QObject::connect (&bridge, &DecodiumBridge::stationNameChanged,
                          &rttyHost, sincronizzaProfiloRtty);
        QObject::connect (&bridge, &DecodiumBridge::stationQthChanged,
                          &rttyHost, sincronizzaProfiloRtty);

        // Anche RTTY parte nella lingua scelta per Decodium. Il selettore
        // globale richiede il riavvio dell'applicazione, quindi questa lettura
        // iniziale mantiene entrambe le interfacce coerenti per tutta la
        // sessione senza conservare una preferenza RTTY concorrente.
        rttyHost.lingua ().attachEngine (&engine);
        rttyHost.lingua ().setCurrent (
            bridge.getSetting (QStringLiteral ("UILanguage"),
                               QStringLiteral ("en")).toString ());

        // L'audio: quello che la radio ascolta, gia' a 12 kHz, dallo stesso
        // rubinetto di DecoPort. L'host lo porta a 24 kHz per il motore RTTY.
        QObject::connect (&bridge, &DecodiumBridge::campioniRxRtty,
                          &rttyHost, [&rttyHost] (QVector<short> const& campioni) {
            std::vector<float> f (static_cast<size_t> (campioni.size ()));
            for (int i = 0; i < campioni.size (); ++i)
                f[static_cast<size_t> (i)] = campioni[i] / 32768.0f;
            rttyHost.consegnaAudio (f, campioni.size ());
        });
        // Le righe RTTY complete entrano nella lista dei decodificati come
        // le altre: cosi' finiscono nella cronologia e nell'archivio. Il
        // testo continua a scorrere carattere per carattere nella finestra
        // dedicata, che e' il modo naturale di leggere un flusso.
        QObject::connect (&rttyHost, &decortty::DecoRttyHost::rigaDecodificata,
                          &bridge, &DecodiumBridge::aggiungiRigaRtty);
        rttyHost.esponiAlQml (*engine.rootContext(),
                              QStringLiteral (FORK_RELEASE_VERSION));
        L("DecoRTTY: sottosistema RTTY avviato");
    }
#endif
    // IU8LMC: aggiornamento automatico con avviso e conferma. Il checker
    // storico (DecodiumBridge::checkForUpdates) e' spento dalla 1.0.62 e non ha
    // mai avvisato nessuno: e' il motivo per cui i tester restano su release
    // vecchissime e segnalano bug gia' corretti.
    static DecodiumUpdater updater;
    engine.rootContext()->setContextProperty("updater", &updater);
    if (auto* map = qobject_cast<MapIntelligenceService*>(bridge.mapIntelligenceService())) {
        DecodiumUpdater* updaterPtr = &updater;
        QObject::connect(map, &MapIntelligenceService::offlineModeChanged,
                         updaterPtr, [updaterPtr, map]() {
            updaterPtr->setOfflineMode(map->offlineMode());
        });
        updater.setOfflineMode(map->offlineMode());
    }
    // IU8LMC: segnalazione problemi al forum community.ft2.it (sostituisce GitHub).
    static DecodiumCommunityReport community;
    engine.rootContext()->setContextProperty("community", &community);
    engine.rootContext()->setContextProperty(
        "decodiumMonoFontFamily",
        fixedFontFamily.isEmpty() ? QStringLiteral("monospace") : fixedFontFamily);
    L("QSG Live Map enabled by default; CPU WorldMapItem fallback remains available via LiveMapUseGpu=false");

    // Load BootLoader first so the process shows a real window before the
    // heavy QML tree is compiled. This also lets Windows users recover from
    // driver-specific Qt Quick stalls instead of waiting on a silent startup.
    QString bundledAppQmlPath = QDir(bundledQtQmlPath).absoluteFilePath(QStringLiteral("decodium"));
    if (!QFile::exists(QDir(bundledAppQmlPath).absoluteFilePath(QStringLiteral("BootLoader.qml")))) {
        bundledAppQmlPath = QDir(appQmlPath).absoluteFilePath(QStringLiteral("decodium"));
    }
    QDir const qmlDir {bundledAppQmlPath};

    QString qmlPath = qmlDir.absoluteFilePath(QStringLiteral("BootLoader.qml"));

    // Fallback to Main.qml if BootLoader doesn't exist (portable/dev builds)
    if (!QFile::exists(qmlPath)) {
        qmlPath = qmlDir.absoluteFilePath(QStringLiteral("Main.qml"));
    }

    if (!QFile::exists(qmlPath)) {
        L("QML file NOT FOUND");
        return -1;
    }
    L(("qmlPath=" + qmlPath.toLocal8Bit()).constData());
    auto logQmlFileInfo = [] (const QString& label, const QString& path) {
        QFileInfo const info(path);
        QByteArray line("QML file ");
        line += label.toLocal8Bit();
        line += ": exists=";
        line += info.exists() ? "1" : "0";
        line += " size=";
        line += QByteArray::number(info.exists() ? info.size() : -1);
        line += " path=";
        line += QDir::toNativeSeparators(path).toLocal8Bit();
        L(line.constData());
    };
    logQmlFileInfo(QStringLiteral("BootLoader"), qmlDir.absoluteFilePath(QStringLiteral("BootLoader.qml")));
    logQmlFileInfo(QStringLiteral("Main"), qmlDir.absoluteFilePath(QStringLiteral("Main.qml")));
    logQmlFileInfo(QStringLiteral("SettingsDialog"), qmlDir.absoluteFilePath(QStringLiteral("components/SettingsDialog.qml")));

    qmlRegisterType<WaterfallItem>("Decodium", 1, 0, "WaterfallItem");
    qmlRegisterType<PanadapterItem>("Decodium", 1, 0, "PanadapterItem");
    qmlRegisterType<WorldMapItem>("Decodium", 1, 0, "WorldMapItem");
    qmlRegisterType<WorldMapGpuItem>("Decodium", 1, 0, "WorldMapGpuItem");
    // Registra DecodiumDxCluster come tipo QML non-creabile (accessibile solo come proprietà di bridge)
    qmlRegisterUncreatableType<DecodiumDxCluster>("Decodium", 1, 0, "DecodiumDxCluster",
        "DecodiumDxCluster is created by DecodiumBridge");

    // Log any QML component creation failures
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [](QObject *obj, const QUrl &url) {
        if (!obj) {
            qCritical("QML FAILED to create object from: %s", qPrintable(url.toString()));
        } else {
            qDebug("QML object created OK from: %s", qPrintable(url.toString()));
        }
    });

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // 1.0.180 — Apply pipeline cache to each QQuickWindow before scene graph init.
    // setGraphicsConfiguration is an instance method; objectCreated fires before
    // the scene graph starts, so this is the correct hook per Qt docs.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [&pipelineCacheFile](QObject *obj, const QUrl &) {
        if (auto *win = qobject_cast<QQuickWindow *>(obj)) {
            QQuickGraphicsConfiguration gc;
            gc.setPipelineCacheSaveFile(pipelineCacheFile);
            bool const canLoadPipelineCache = QFileInfo::exists(pipelineCacheFile);
            if (canLoadPipelineCache) {
                gc.setPipelineCacheLoadFile(pipelineCacheFile);
            }
            win->setGraphicsConfiguration(gc);
            qInfo() << "[UI] Pipeline cache applied to window:" << pipelineCacheFile
                    << "load=" << canLoadPipelineCache;
        }
    });
#else
    qInfo() << "[UI] Pipeline cache skipped: Qt" << QT_VERSION_STR
            << "does not expose QQuickGraphicsConfiguration pipeline cache files";
#endif

    L("loading QML...");
    // Watchdog: log if QML loading takes too long (helps diagnose hangs)
    QElapsedTimer loadTimer;
    loadTimer.start();
    std::atomic_bool qmlLoadDone {false};
    std::mutex qmlLoadWatchdogMutex;
    std::condition_variable qmlLoadWatchdogCv;
    QByteArray const startupLogPath =
        (QStandardPaths::writableLocation(QStandardPaths::TempLocation)
         + QStringLiteral("/decodium-start.log")).toLocal8Bit();
    QByteArray const watchedQmlPath = qmlPath.toLocal8Bit();
    QByteArray slowQmlStartupFlagBytes;
#ifdef Q_OS_WIN
    slowQmlStartupFlagBytes = slowQmlStartupFlag.toLocal8Bit();
    QByteArray pendingReason("Windows graphics startup did not complete; backend=");
    pendingReason += activeWindowsGraphicsMode;
    writeWindowsGraphicsStartupFlag(graphicsStartupPendingFlag,
                                    activeWindowsGraphicsMode,
                                    pendingReason);
    L((QByteArray("Windows graphics startup marker written: backend=")
       + activeWindowsGraphicsMode
       + " path=" + graphicsStartupPendingFlag.toLocal8Bit()).constData());

    QByteArray const forcedFailureModes =
        qgetenv("DECODIUM_TEST_FAIL_GRAPHICS_BACKENDS").trimmed().toLower();
    if (!forcedFailureModes.isEmpty()
        && forcedFailureModes.split(',').contains(activeWindowsGraphicsMode)) {
        L((QByteArray("[GRAPHICS-TEST] simulating startup failure for backend=")
           + activeWindowsGraphicsMode).constData());
        ExitProcess(86);
    }
#endif
    std::thread qmlLoadWatchdog([&qmlLoadDone, &qmlLoadWatchdogMutex, &qmlLoadWatchdogCv,
                                  startupLogPath, watchedQmlPath,
                                  slowQmlStartupFlagBytes]() {
        std::unique_lock<std::mutex> lock(qmlLoadWatchdogMutex);
        for (int elapsedSeconds = 10; elapsedSeconds <= 900; elapsedSeconds += 10) {
            if (qmlLoadWatchdogCv.wait_for(lock, std::chrono::seconds(10),
                                           [&qmlLoadDone]() {
                                               return qmlLoadDone.load(std::memory_order_relaxed);
                                           })) {
                return;
            }

            QByteArray msg("QML LOAD WATCHDOG: engine.load(");
            msg += watchedQmlPath;
            msg += ") still running after ";
            msg += QByteArray::number(elapsedSeconds);
            msg += " s";
            writeStartupLogLine(startupLogPath, msg);

#ifdef Q_OS_WIN
            if (elapsedSeconds == 60 && !slowQmlStartupFlagBytes.isEmpty()) {
                QByteArray reason("QML load exceeded 60 seconds; use safe graphics on next launch");
                writeSlowQmlStartupFlag(slowQmlStartupFlagBytes, reason);
                QByteArray flagMsg("QML LOAD WATCHDOG: slow startup marker written: ");
                flagMsg += slowQmlStartupFlagBytes;
                writeStartupLogLine(startupLogPath, flagMsg);
            }
#endif
        }
    });

    engine.load(QUrl::fromLocalFile(qmlPath));
#ifdef Q_OS_WIN
    applyApplicationIconToTopLevelWindows(appIcon);
#endif

    {
        std::lock_guard<std::mutex> lock(qmlLoadWatchdogMutex);
        qmlLoadDone.store(true, std::memory_order_relaxed);
    }
    qmlLoadWatchdogCv.notify_all();
    if (qmlLoadWatchdog.joinable())
        qmlLoadWatchdog.join();
    L(("engine.load() returned in " + QByteArray::number(loadTimer.elapsed()) + " ms").constData());

#ifdef Q_OS_WIN
    if (loadTimer.elapsed() > 60000) {
        QByteArray reason("Last QML load took ");
        reason += QByteArray::number(loadTimer.elapsed());
        reason += " ms; use safe graphics on next launch";
        writeSlowQmlStartupFlag(slowQmlStartupFlag.toLocal8Bit(), reason);
        L(("Slow QML startup marker kept for next launch: "
           + slowQmlStartupFlag.toLocal8Bit()).constData());
    }
#endif


    if (engine.rootObjects().isEmpty()) {
        L("ERROR: rootObjects empty — QML failed to load. Check console for QML errors.");
        QString const startupLogPath = QDir {QStandardPaths::writableLocation(QStandardPaths::TempLocation)}
            .absoluteFilePath(QStringLiteral("decodium-start.log"));
        QString const details = qmlWarningMessages.isEmpty()
            ? QStringLiteral("No QML warning was reported before the root object failed.")
            : qmlWarningMessages.join(QStringLiteral("\n"));
        // Show a native error dialog so user knows what happened
        QMessageBox::critical(nullptr, QStringLiteral("Decodium — QML Error"),
            QStringLiteral("The user interface failed to load.\n\n"
                           "This is usually caused by a missing Qt plugin or a corrupted installation.\n\n"
                           "Try reinstalling Decodium or deleting the qmlcache folder in the install directory.\n\n"
                           "QML path: %1\n\n"
                           "Startup log: %2\n\n"
                           "QML details:\n%3").arg(qmlPath, startupLogPath, details));
        return -1;
    }
    logFirstQuickWindowGraphicsApi(engine, "after engine.load");
    QTimer::singleShot(0, &app, [&engine] {
        logFirstQuickWindowGraphicsApi(engine, "event loop start");
    });

    // 1.0.233 — DevOverlay (Sprint 2 Phase 7): connect frameSwapped a Bridge
    // per ring buffer frame time, ed espone il backend RHI corrente.
    // Lo slot recordFrameTimestamp short-circuita quando overlay e' off,
    // quindi overhead a riposo = costo di una virtual call + branch.
    // 1.0.236 fix: QueuedConnection invece di DirectConnection.
    // frameSwapped e' emesso dal RENDER thread mentre recordFrameTimestamp
    // scrive in m_frameTimeRing[] / m_perfFrameElapsed (non atomic) e legge
    // m_devOverlayActive: con DirectConnection si esegue sul render thread
    // -> race con il QTimer GUI thread che legge gli stessi membri.
    // Queued route i frame swap sul GUI thread (eventloop bridge), no race.
    // 1.0.249 fix: connettiamo frameSwapped a TUTTI i QQuickWindow trovati.
    // 1.0.248 ha mostrato totalFrameSamples=205 costante in ogni sessione ->
    // il signal scattava solo durante startup poi si fermava. Probabile che
    // firstQuickWindow ritornava un Window splash/temporaneo non quello del
    // root ApplicationWindow attivo per i waterfall paint.
    {
        QList<QQuickWindow*> windows;
        for (QObject* root : engine.rootObjects()) {
            if (auto* qw = qobject_cast<QQuickWindow*>(root)) windows.append(qw);
            for (QObject* child : root->findChildren<QObject*>()) {
                if (auto* qw = qobject_cast<QQuickWindow*>(child)) windows.append(qw);
            }
        }
        QString rhiName = QStringLiteral("unknown");
        for (QQuickWindow* qw : std::as_const(windows)) {
            // 1.0.250: connetto BOTH frameSwapped E afterRendering.
            // 1.0.249 mostrava totalFrameSamples=205 fisso anche con
            // connect all-windows: frameSwapped non scatta dopo init su
            // Qt 6.11 con RHI in alcune pipeline. afterRendering invece
            // emette piu' affidabilmente per ogni render pass.
            QObject::connect(qw, &QQuickWindow::frameSwapped,
                             &bridge, &DecodiumBridge::recordFrameTimestamp,
                             Qt::QueuedConnection);
            QObject::connect(qw, &QQuickWindow::afterRendering,
                             &bridge, &DecodiumBridge::recordFrameTimestamp,
                             Qt::QueuedConnection);
            if (rhiName == QStringLiteral("unknown") && qw->rendererInterface()) {
                rhiName = QString::fromLatin1(
                    qsgGraphicsApiName(qw->rendererInterface()->graphicsApi()));
            }
        }
        if (rhiName == QStringLiteral("unknown")) {
            QByteArray const env = qgetenv("QSG_RHI_BACKEND");
            if (!env.isEmpty()) rhiName = QString::fromLatin1(env);
        }
        bridge.setActiveRhiBackend(rhiName);
        L(("DevOverlay: frameSwapped wired su " + QByteArray::number(windows.size())
           + " QQuickWindow(s), RHI backend = "
           + rhiName.toLocal8Bit()).constData());
    }
#ifdef Q_OS_WIN
    QTimer::singleShot(8000, &app, [graphicsStartupPendingFlag,
                                    slowQmlStartupFlag,
                                    activeWindowsGraphicsMode,
                                    graphicsRecoveryFromPendingMarker,
                                    persistentD3d11FallbackUsable,
                                    automaticSafeGraphics,
                                    automaticD3d11Fallback] {
        if (QFile::exists(graphicsStartupPendingFlag)) {
            if (g_windowsD3d12DeviceFailed.load(std::memory_order_relaxed)) {
                L("Windows D3D12 device failure observed; persistent fallback kept for future launches");
            } else {
                QFile::remove(graphicsStartupPendingFlag);
                L((QByteArray("Windows graphics startup completed; marker cleared backend=")
                   + activeWindowsGraphicsMode).constData());
                if (graphicsRecoveryFromPendingMarker) {
                    writePersistentGraphicsFallback(
                        activeWindowsGraphicsMode,
                        QStringLiteral("Automatic recovery after a graphics startup failure"));
                    L((QByteArray("Windows graphics fallback persisted after successful recovery: ")
                       + activeWindowsGraphicsMode).constData());
                }
            }
        }
        if (persistentD3d11FallbackUsable)
            L("Windows persistent D3D11 fallback remains active; use --reset-safe-graphics to retry D3D12");
        if ((automaticSafeGraphics || automaticD3d11Fallback) && QFile::exists(slowQmlStartupFlag)) {
            QFile::remove(slowQmlStartupFlag);
            L("Windows automatic graphics fallback stable; slow-startup marker cleared");
        }
    });
#endif
    L("QML OK - entering event loop");

    // 1.0.220 — Smoke TX test mode: DECODIUM_TX_SMOKE_TEST=I_UNDERSTAND_THIS_KEYS_TX
    // attiva una sequenza di startTune/stopTune programmatica per stressare il
    // lifecycle audio sink senza UI interaction. Questa prova puo' keyare la
    // radio: la semplice presenza della variabile non e' sufficiente. Sequenza:
    //   t=5s   tune ON  (first cycle, ctor sink + WASAPI start)
    //   t=8s   tune OFF (retire + park 8s window inizia)
    //   t=11s  tune ON  (verifica recovery: sink_create durante park overlap)
    //   t=14s  tune OFF (retire seconda volta)
    //   t=17s  quit     (esegue exit cleanup, verifica delete delayed)
    // Tutti gli eventi loggati con prefix [TX-SMOKE] + i marker [TX-TL]
    // standard dei fix 1.0.218 sono visibili nel diagnostic log.
    QString const txSmokeTest = qEnvironmentVariable("DECODIUM_TX_SMOKE_TEST").trimmed();
    if (!txSmokeTest.isEmpty() && txSmokeTest != QStringLiteral("I_UNDERSTAND_THIS_KEYS_TX")) {
        L("[TX-SMOKE] DECODIUM_TX_SMOKE_TEST ignored: set it to I_UNDERSTAND_THIS_KEYS_TX to run the unsafe Tune cycle");
    } else if (txSmokeTest == QStringLiteral("I_UNDERSTAND_THIS_KEYS_TX")) {
        L("[TX-SMOKE] explicit DECODIUM_TX_SMOKE_TEST consent detected — arming Tune cycle");
        QTimer::singleShot(5000, &bridge, [&bridge]() {
            qInfo() << "[TX-SMOKE] t=5s startTune (cycle 1)";
            bridge.startTune();
        });
        QTimer::singleShot(8000, &bridge, [&bridge]() {
            qInfo() << "[TX-SMOKE] t=8s stopTune (cycle 1)";
            bridge.stopTune();
        });
        QTimer::singleShot(11000, &bridge, [&bridge]() {
            qInfo() << "[TX-SMOKE] t=11s startTune (cycle 2, during park overlap)";
            bridge.startTune();
        });
        QTimer::singleShot(14000, &bridge, [&bridge]() {
            qInfo() << "[TX-SMOKE] t=14s stopTune (cycle 2)";
            bridge.stopTune();
        });
        QTimer::singleShot(17000, &app, [&app]() {
            qInfo() << "[TX-SMOKE] t=17s quit";
            app.quit();
        });
    }

    QString const rxRecordSecondsText = qEnvironmentVariable("DECODIUM_RX_RECORD_SECONDS").trimmed();
    if (!rxRecordSecondsText.isEmpty()) {
        bool secondsOk = false;
        int const requestedSeconds = rxRecordSecondsText.toInt(&secondsOk);
        if (!secondsOk || requestedSeconds <= 0) {
            L("[RX-RECORD] ignored: DECODIUM_RX_RECORD_SECONDS must be a positive integer");
        } else {
            bool delayOk = false;
            int const requestedDelayMs = qEnvironmentVariableIntValue("DECODIUM_RX_RECORD_START_DELAY_MS", &delayOk);
            int const startDelayMs = delayOk ? qBound(0, requestedDelayMs, 600000) : 5000;
            bool alignOk = false;
            int const requestedAlignMs = qEnvironmentVariableIntValue("DECODIUM_RX_RECORD_ALIGN_PERIOD_MS", &alignOk);
            int const alignPeriodMs = alignOk ? qBound(0, requestedAlignMs, 60000) : 0;
            bool quitDelayOk = false;
            int const requestedQuitDelayMs = qEnvironmentVariableIntValue("DECODIUM_RX_RECORD_QUIT_DELAY_MS", &quitDelayOk);
            int const quitDelayMs = quitDelayOk ? qBound(0, requestedQuitDelayMs, 120000) : 1500;
            bool readyTimeoutOk = false;
            int const requestedReadyTimeoutMs = qEnvironmentVariableIntValue("DECODIUM_RX_RECORD_READY_TIMEOUT_MS", &readyTimeoutOk);
            int const readyTimeoutMs = readyTimeoutOk ? qBound(0, requestedReadyTimeoutMs, 120000) : 30000;
            int const seconds = qBound(1, requestedSeconds, 3600);
            QString const forcedMode = qEnvironmentVariable("DECODIUM_RX_RECORD_MODE").trimmed().toUpper();
            bool forcedDialOk = false;
            int const requestedForcedDialHz =
                qEnvironmentVariableIntValue("DECODIUM_RX_RECORD_DIAL_HZ", &forcedDialOk);
            int const forcedDialHz = forcedDialOk
                ? qBound(100000, requestedForcedDialHz, 2000000000)
                : 0;
            QString const quitText = qEnvironmentVariable("DECODIUM_RX_RECORD_QUIT").trimmed().toLower();
            bool const quitAfter =
                quitText == QStringLiteral("1")
                || quitText == QStringLiteral("true")
                || quitText == QStringLiteral("yes");
            L(QStringLiteral("[RX-RECORD] armed: delay_ms=%1 align_period_ms=%2 seconds=%3 quit=%4 quit_delay_ms=%5 ready_timeout_ms=%6 forced_mode=%7 forced_dial_hz=%8")
                  .arg(startDelayMs)
                  .arg(alignPeriodMs)
                  .arg(seconds)
                  .arg(quitAfter ? 1 : 0)
                  .arg(quitDelayMs)
                  .arg(readyTimeoutMs)
                  .arg(forcedMode.isEmpty() ? QStringLiteral("<none>") : forcedMode)
                  .arg(forcedDialHz)
                  .toUtf8()
                  .constData());
              QTimer::singleShot(startDelayMs, &bridge, [&bridge, seconds, quitAfter, quitDelayMs, alignPeriodMs, readyTimeoutMs, forcedMode, forcedDialHz, &app]() {
                auto applyForcedRecordState = [&bridge, forcedMode, forcedDialHz]() {
                  if (!forcedMode.isEmpty()) {
                      qInfo().noquote() << "[RX-RECORD] force_mode=" << forcedMode;
                      bridge.setMode(forcedMode);
                  }
                  if (forcedDialHz > 0) {
                      qInfo().noquote() << "[RX-RECORD] force_dial_hz=" << forcedDialHz;
                      bridge.setFrequency(static_cast<double>(forcedDialHz));
                  }
                };
                applyForcedRecordState();
                auto startAlignedRecording = [&bridge, seconds, quitAfter, quitDelayMs, alignPeriodMs, applyForcedRecordState, &app]() {
                  applyForcedRecordState();
                  int alignDelayMs = 0;
                  if (alignPeriodMs > 0) {
                    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
                    qint64 const remainder = nowMs % alignPeriodMs;
                    alignDelayMs = remainder == 0 ? 0 : int(alignPeriodMs - remainder);
                    qInfo().noquote() << "[RX-RECORD] align_delay_ms="
                                      << alignDelayMs
                                      << "period_ms=" << alignPeriodMs;
                }
                QTimer::singleShot(alignDelayMs, &bridge, [&bridge, seconds, quitAfter, quitDelayMs, &app]() {
                qInfo().noquote() << "[RX-RECORD] start_utc="
                                  << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
                                  << "seconds=" << seconds;
                bridge.setRecordRxEnabled(true);
                QTimer::singleShot(seconds * 1000, &bridge, [&bridge, quitAfter, quitDelayMs, &app]() {
                    qInfo().noquote() << "[RX-RECORD] stop_utc="
                                      << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
                    bridge.setRecordRxEnabled(false);
                    if (quitAfter) {
                        QTimer::singleShot(quitDelayMs, &app, [&app]() {
                            qInfo() << "[RX-RECORD] quit";
                            app.quit();
                        });
                    }
                });
                });
                };

                if (bridge.monitoring()) {
                    startAlignedRecording();
                    return;
                }

                qInfo().noquote() << "[RX-RECORD] waiting_for_monitoring timeout_ms=" << readyTimeoutMs;
                bridge.setMonitoring(true);
                auto* readyTimer = new QTimer(&bridge);
                readyTimer->setInterval(250);
                auto waitTimer = std::make_shared<QElapsedTimer>();
                waitTimer->start();
                QObject::connect(readyTimer, &QTimer::timeout, &bridge,
                                 [&bridge, readyTimer, waitTimer, readyTimeoutMs, startAlignedRecording]() mutable {
                    if (bridge.monitoring()) {
                        qInfo().noquote() << "[RX-RECORD] monitoring_ready elapsed_ms="
                                          << waitTimer->elapsed();
                        readyTimer->stop();
                        readyTimer->deleteLater();
                        startAlignedRecording();
                        return;
                    }
                    if (readyTimeoutMs > 0 && waitTimer->elapsed() >= readyTimeoutMs) {
                        qWarning().noquote() << "[RX-RECORD] monitoring_wait_timeout elapsed_ms="
                                             << waitTimer->elapsed()
                                             << "starting_recording_anyway=1";
                        readyTimer->stop();
                        readyTimer->deleteLater();
                        startAlignedRecording();
                    }
                });
                readyTimer->start();
            });
        }
    }

    auto mainThreadWatchdogInstalled = std::make_shared<bool>(false);
    QObject::connect(&bridge,
                     &DecodiumBridge::mainQmlReadyForNativeWindowing,
                     &app,
                     [&app, &bridge, mainThreadWatchdogInstalled]() {
        if (*mainThreadWatchdogInstalled) {
            return;
        }
        *mainThreadWatchdogInstalled = true;
        installMainThreadWatchdog(&app, &bridge);
    });
    qInfo().noquote() << "[MAINWATCH] event loop watchdog waiting for Main.qml ready";

    int r = app.exec();
    g_shuttingDown.store(true, std::memory_order_relaxed);
    L("app.exec() exited");
    return r;
}
