#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>

#include <atomic>
#include <utility>
#include <vector>

namespace decodium::linux_graphics {

inline bool isNvk(quint32 vendorId, const QByteArray& name)
{
    return vendorId == 0x10de && name.toUpper().contains("NVK");
}

// Keep the existing ranking for all other drivers and explicit selections.
// Candidate is the Vulkan probe record; this policy itself needs no Vulkan
// device and can be regression-tested on every build platform.
template<class Candidate>
const Candidate* automaticAlternativeForNvk(const std::vector<Candidate>& devices,
                                           const Candidate* preferred,
                                           bool explicitVulkan)
{
    if (!preferred || explicitVulkan || !isNvk(preferred->vendorId, preferred->name))
        return preferred;
    const Candidate* intel = nullptr;
    for (const auto& device : devices) {
        if (device.vendorId == 0x8086 && device.integrated() && device.eligible()
            && (!intel || device.localMemoryBytes > intel->localMemoryBytes))
            intel = &device;
    }
    // Do not fall through to OpenGL on the same NVIDIA GPU: Mesa may route
    // OpenGL through Zink/NVK. No eligible Intel alternative means software.
    return intel;
}

inline void useSoftwareRendering()
{
    qunsetenv("QSG_RHI_BACKEND");
    qunsetenv("QT_VK_PHYSICAL_DEVICE_INDEX");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_OPENGL", "software");
    qputenv("DECODIUM_DISABLE_GPU_PANADAPTER_FFT", "1");
}

inline bool hasExplicitGraphicsChoice()
{
    for (const char* name : {"QSG_RHI_BACKEND", "QT_QUICK_BACKEND", "QMLSCENE_DEVICE",
                             "DECODIUM_GRAPHICS_BACKEND", "QT_VK_PHYSICAL_DEVICE_INDEX"}) {
        if (!qgetenv(name).trimmed().isEmpty())
            return true;
    }
    return qEnvironmentVariableIntValue("DECODIUM_DISABLE_LINUX_GPU_AUTOSELECT") != 0;
}

inline QString recoveryMarkerPath()
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return config.isEmpty() ? QString {} : QDir(config).filePath(
        QStringLiteral("Decodium/linux-vulkan-device-loss.json"));
}

inline bool isDeviceLossMessage(const QString& message)
{
    // Driver debug output is optional. Qt's QRhi Vulkan backend reports the
    // same failure without spelling VK_ERROR_DEVICE_LOST; both Qt Quick
    // render loops also report it before tearing down/recreating the RHI.
    // See qtbase/src/gui/rhi/qrhivulkan.cpp and qtdeclarative's
    // qsgthreadedrenderloop.cpp / qsgrenderloop.cpp handleDeviceLoss paths (#101).
    if (message.contains(QLatin1String("VK_ERROR_DEVICE_LOST"))) return true;
    if (message.startsWith(QLatin1String("Device loss detected in vk"))
        && message.endsWith(QLatin1String("()"))) return true;
    return message == QLatin1String("Graphics device lost, cleaning up scenegraph and releasing RHI")
        || message == QLatin1String("Graphics device lost, cleaning up scenegraph and releasing RHIs");
}

class DeviceLossRecovery final
{
public:
    explicit DeviceLossRecovery(QString path) : m_path(std::move(path)) {}

    bool active() const { return QFileInfo(m_path).isFile(); }
    bool reset() { return !active() || QFile::remove(m_path); }
    bool observed() const { return m_observed.load(std::memory_order_relaxed); }
    bool persisted() const { return m_persisted.load(std::memory_order_relaxed); }

    // May run on Qt's render thread. Persist before queuing any GUI work so a
    // stalled event loop or forced shutdown cannot erase the recovery choice.
    // No qDebug/qWarning here: this is called from the Qt message handler.
    bool record(const QString& message)
    {
        if (!isDeviceLossMessage(message)
            || m_observed.exchange(true, std::memory_order_relaxed))
            return false;
        if (!m_path.isEmpty() && QDir().mkpath(QFileInfo(m_path).absolutePath())) {
            QSaveFile file(m_path);
            const QByteArray data = QJsonDocument(QJsonObject {
                {QStringLiteral("schema"), 1},
                {QStringLiteral("fallback"), QStringLiteral("software")},
                {QStringLiteral("reason"), message.left(1024)},
                {QStringLiteral("utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
            }).toJson(QJsonDocument::Compact);
            if (file.open(QIODevice::WriteOnly)
                && file.write(data) == data.size() && file.commit())
                m_persisted.store(true, std::memory_order_relaxed);
        }
        return true;
    }

private:
    const QString m_path;
    std::atomic_bool m_observed {false};
    std::atomic_bool m_persisted {false};
};

} // namespace decodium::linux_graphics
