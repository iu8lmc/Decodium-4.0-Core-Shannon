#include "DecodiumBridge.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/core/SstvModeRegistry.h"
#include "src/sstv/integration/SstvRxRuntime.h"
#include <cstdint>
#include <optional>
extern QString decodiumSstvRxModeControlName(decodium::sstv::SstvRxModeControl);
extern QString decodiumSstvRxAfcModeName(decodium::sstv::SstvRxAfcMode);
extern QString decodiumSstvRxSlantModeName(decodium::sstv::SstvRxSlantMode);
extern qulonglong decodiumSstvBoundedUnsigned(std::uint64_t);
#endif

QVariantMap DecodiumBridge::sstvRxControls() const
{
    QVariantMap result;
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return result;
    }
    const auto controls = m_sstvRxRuntime->rxControlSnapshot();
    const auto notch = m_sstvRxRuntime->interferenceNotch();
    result.insert(QStringLiteral("interferenceNotchEnabled"), notch.has_value());
    result.insert(QStringLiteral("interferenceNotchHz"), notch.value_or(1700));
    const auto fax480 = m_sstvRxRuntime->fax480Config();
    result.insert(QStringLiteral("fax480Enabled"), fax480.has_value());
    result.insert(QStringLiteral("fax480Profile"), !fax480 || *fax480 == decodium::sstv::SstvFax480Profile::Dayton480
                  ? QStringLiteral("dayton-480") : (*fax480 == decodium::sstv::SstvFax480Profile::Qsstv500
                     ? QStringLiteral("qsstv-500") : QStringLiteral("libsstv-vis-480")));
    const auto fax = m_sstvRxRuntime->radiofaxConfig();
    result.insert(QStringLiteral("radiofaxEnabled"), fax.has_value());
    result.insert(QStringLiteral("radiofaxIoc"), fax ? int(fax->ioc) : 576);
    result.insert(QStringLiteral("radiofaxWidth"), fax ? int(fax->width) : 1809);
    result.insert(QStringLiteral("radiofaxCenteredPhasing"), fax && fax->centeredPhasingWithWhiteEndLine);
    result.insert(QStringLiteral("radiofaxWmoDeadSector"), fax && fax->wmoDeadSector);
    result.insert(QStringLiteral("radiofaxWmoTimedPhasing"), fax && fax->wmoTimedPhasing);
    result.insert(QStringLiteral("radiofaxHeight"), fax ? int(fax->height) : 1000);
    result.insert(QStringLiteral("modeControl"),
                  decodiumSstvRxModeControlName(controls.settings.modeControl));
    result.insert(QStringLiteral("manualMode"),
                  QString::fromStdString(controls.settings.manualMode));
    result.insert(QStringLiteral("modeLockEnabled"),
                  controls.settings.modeLockEnabled);
    result.insert(QStringLiteral("lockedMode"),
                  QString::fromStdString(controls.settings.lockedMode));
    result.insert(QStringLiteral("receiveWithoutVis"),
                  controls.settings.receiveWithoutVis);
    result.insert(QStringLiteral("timingFallbackEnabled"),
                  controls.settings.timingFallbackEnabled);
    result.insert(QStringLiteral("afcMode"),
                  decodiumSstvRxAfcModeName(controls.settings.afcMode));
    result.insert(QStringLiteral("manualFrequencyCorrectionHz"),
                  controls.settings.manualFrequencyCorrectionHz);
    result.insert(QStringLiteral("slantMode"),
                  decodiumSstvRxSlantModeName(controls.settings.slantMode));
    result.insert(QStringLiteral("manualClockErrorPpm"),
                  controls.settings.manualClockErrorPpm);
    result.insert(QStringLiteral("replayRetentionSeconds"),
                  controls.settings.replayRetentionSeconds);
    result.insert(QStringLiteral("retainRawAudio"),
                  controls.settings.retainRawAudio);
    result.insert(QStringLiteral("diagnosticScopeEnabled"),
                  controls.settings.diagnosticScopeEnabled);
    result.insert(QStringLiteral("revision"),
                  decodiumSstvBoundedUnsigned(controls.revision));
#endif
    return result;
}

QVariantList DecodiumBridge::sstvRxModeChoices() const
{
    QVariantList result;
#if DECODIUM_HAS_SSTV
    const decodium::sstv::SstvModeRegistry registry =
        decodium::sstv::SstvModeRegistry::canonical();
    for (const decodium::sstv::SstvModeSpec& mode : registry.modes()) {
        if (mode.classification
                != decodium::sstv::ModeClassification::AnalogSstv
            || !mode.claimsRxSupport()) {
            continue;
        }
        result.push_back(QVariantMap {
            {QStringLiteral("id"), QString::fromStdString(mode.id)},
            {QStringLiteral("name"), QString::fromStdString(mode.longName)},
            {QStringLiteral("family"), QString::fromStdString(mode.family)},
        });
    }
#endif
    return result;
}

bool DecodiumBridge::configureSstvInterferenceNotch(bool enabled, int frequencyHz)
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime || m_sstvRxRequested || m_sstvRedecodeActive || sstvWavReplayActive()) return false;
    const bool accepted = m_sstvRxRuntime->setInterferenceNotch(
        enabled ? std::optional<int>(frequencyHz) : std::nullopt);
    if (accepted) emit sstvRxSnapshotChanged();
    return accepted;
#else
    Q_UNUSED(enabled) Q_UNUSED(frequencyHz)
    return false;
#endif
}

bool DecodiumBridge::configureSstvFax480Rx(bool enabled, const QString& profile)
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime || m_sstvRxRequested || m_sstvRedecodeActive) return false;
    using decodium::sstv::SstvFax480Profile;
    if (enabled && profile!=QStringLiteral("dayton-480") && profile!=QStringLiteral("qsstv-500")
        && profile!=QStringLiteral("libsstv-vis-480")) return false;
    std::optional<SstvFax480Profile> selected;
    if (enabled) selected=profile==QStringLiteral("dayton-480") ? SstvFax480Profile::Dayton480
        : (profile==QStringLiteral("qsstv-500") ? SstvFax480Profile::Qsstv500 : SstvFax480Profile::LibsstvVis480);
    const bool accepted=m_sstvRxRuntime->setFax480Config(selected);
    if (accepted) emit sstvRxSnapshotChanged();
    return accepted;
#else
    Q_UNUSED(enabled) Q_UNUSED(profile)
    return false;
#endif
}

bool DecodiumBridge::configureSstvRadiofaxRx(bool enabled, int ioc, int width, int height)
{
    return configureSstvRadiofaxRxAligned(enabled, ioc, width, height, false);
}

bool DecodiumBridge::configureSstvRadiofaxRxAligned(bool enabled, int ioc, int width, int height,
                                                    bool centeredPhasingWithWhiteEndLine)
{
    return configureSstvRadiofaxRxProfile(enabled, ioc, width, height, centeredPhasingWithWhiteEndLine, false);
}

bool DecodiumBridge::configureSstvRadiofaxRxProfile(bool enabled, int ioc, int width, int height,
                                                    bool centeredPhasingWithWhiteEndLine, bool wmoDeadSector)
{
    return configureSstvRadiofaxRxAdvanced(enabled, ioc, width, height,
                                           centeredPhasingWithWhiteEndLine, wmoDeadSector, false);
}

bool DecodiumBridge::configureSstvRadiofaxRxAdvanced(bool enabled, int ioc, int width, int height,
                                                     bool centeredPhasingWithWhiteEndLine, bool wmoDeadSector,
                                                     bool wmoTimedPhasing)
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime || m_sstvRxRequested || m_sstvRedecodeActive
        || (enabled && ((ioc != 288 && ioc != 576) || width < 1 || height < 1)))
        return false;
    std::optional<decodium::sstv::SstvFaxConfig> config;
    if (enabled) {
        config.emplace();
        config->ioc = ioc == 288 ? decodium::sstv::SstvFaxIoc::Ioc288
                                 : decodium::sstv::SstvFaxIoc::Ioc576;
        config->width = static_cast<std::uint32_t>(width);
        config->height = static_cast<std::uint32_t>(height);
        config->centeredPhasingWithWhiteEndLine = centeredPhasingWithWhiteEndLine;
        config->wmoDeadSector = wmoDeadSector;
        config->wmoTimedPhasing = wmoTimedPhasing;
    }
    const bool accepted = m_sstvRxRuntime->setRadiofaxConfig(config);
    if (accepted) emit sstvRxSnapshotChanged();
    return accepted;
#else
    Q_UNUSED(enabled) Q_UNUSED(ioc) Q_UNUSED(width) Q_UNUSED(height) Q_UNUSED(centeredPhasingWithWhiteEndLine) Q_UNUSED(wmoDeadSector) Q_UNUSED(wmoTimedPhasing)
    return false;
#endif
}

