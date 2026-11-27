#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <algorithm>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

int decodiumBridgeMaxFtDecodeThreads();
int decodiumBridgeFtAutoUiReserveForCores(int cores);
int decodiumBridgeAutoFtThreadCountForCores(int cores);
void decodiumBridgeCpuProfileLog(const QString& message);

void DecodiumBridge::setFtThreads(int v)
{
    const int clamped = std::clamp(v, 1, decodiumBridgeMaxFtDecodeThreads());
    if (m_ftThreads == clamped && !m_ftThreadsAuto) return;
    m_ftThreads = clamped;
    m_ftThreadsAuto = false;
    applyLowCpuRuntimeProfile(QStringLiteral("ft-threads"));
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue("ftThreads", m_ftThreads);
    settings.setValue("ftThreadsAuto", false);
    emit ftThreadsChanged();
}

void DecodiumBridge::setFtThreadsAuto(bool enabled)
{
    if (!enabled) {
        if (!m_ftThreadsAuto) return;
        m_ftThreadsAuto = false;
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(settings);
        settings.setValue("ftThreadsAuto", false);
        emit ftThreadsChanged();
        return;
    }

    int const cores = qMax(1, QThread::idealThreadCount());
    int const uiReserve = decodiumBridgeFtAutoUiReserveForCores(cores);
    int const autoThreads = decodiumBridgeAutoFtThreadCountForCores(cores);
    if (m_ftThreadsAuto && m_ftThreads == autoThreads) return;
    m_ftThreadsAuto = true;
    m_ftThreads = autoThreads;
    applyLowCpuRuntimeProfile(QStringLiteral("ft-threads-auto"));
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue("ftThreads", m_ftThreads);
    settings.setValue("ftThreadsAuto", true);
    settings.setValue("ftThreadsAutoDetected", true);
    decodiumBridgeCpuProfileLog(QStringLiteral("FT decoder threads AUTO: cores=%1 ui_reserve=%2 ft_threads=%3")
                  .arg(cores)
                  .arg(uiReserve)
                  .arg(m_ftThreads));
    emit ftThreadsChanged();
}

void DecodiumBridge::cycleFtThreads()
{
    setFtThreads(m_ftThreads >= decodiumBridgeMaxFtDecodeThreads() ? 1 : m_ftThreads + 1);
}

void DecodiumBridge::setLowCpuModeEnabled(bool enabled)
{
    if (m_lowCpuModeEnabled == enabled) {
        return;
    }

    m_lowCpuModeEnabled = enabled;
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("LowCpuMode"), m_lowCpuModeEnabled);
    s.setValue(QStringLiteral("lowCpuModeEnabled"), m_lowCpuModeEnabled);
    s.sync();

    applyLowCpuRuntimeProfile(QStringLiteral("setting"));
    emit lowCpuModeEnabledChanged();
    emit settingValueChanged(QStringLiteral("LowCpuMode"), m_lowCpuModeEnabled);
    emit settingValueChanged(QStringLiteral("lowCpuModeEnabled"), m_lowCpuModeEnabled);
    emit statusMessage(m_lowCpuModeEnabled
                       ? QStringLiteral("Low CPU mode attiva: FT max 2 thread, waterfall ridotta, deep/early limitati")
                       : QStringLiteral("Low CPU mode disattiva"));
}

// First-run Slow-PC offer: only on computers that plausibly need it, so a fast machine
// is not asked a question it has no use for (the switch stays in Settings for everyone).
// Weak = 4 logical cores or fewer, under 8 GB of RAM, or a previous start that was slow
// or whose graphics start-up failed (the flags main_qml leaves in the temp folder).
bool DecodiumBridge::looksLikeWeakComputer() const
{
    int const cores = qMax(1, QThread::idealThreadCount());
    qint64 ramMb = -1;
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status {};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        ramMb = static_cast<qint64>(status.ullTotalPhys / (1024ull * 1024ull));
    }
#endif
    QDir const temp {QStandardPaths::writableLocation(QStandardPaths::TempLocation)};
    bool const slowStartBefore = temp.exists(QStringLiteral("decodium-slow-qml-startup.flag"))
        || temp.exists(QStringLiteral("decodium-gpu-startup-pending.flag"));
    bool const weak = cores <= 4 || (ramMb >= 0 && ramMb < 8192) || slowStartBefore;
    decodiumBridgeCpuProfileLog(QStringLiteral("SlowPcOffer: cores=%1 ram_mb=%2 slow_start_before=%3 weak=%4")
                  .arg(cores)
                  .arg(ramMb)
                  .arg(int(slowStartBefore))
                  .arg(int(weak)));
    return weak;
}

// 1.0.497 — Modalità PC lento (master). Orchestra in blocco le leve runtime che
// contano su hardware datato: grafica OpenGL (persistita per l'avvio, letta in
// main_qml), low-CPU, thread FT bassi, priorità Normale, profilo decode 'cpu'.
// Le Live Map/Full Spectrum le nasconde il QML leggendo lowEndMode. La grafica
// richiede riavvio: lo comunichiamo con un toast.
void DecodiumBridge::setLowEndMode(bool enabled)
{
    if (m_lowEndMode == enabled) {
        return;
    }
    m_lowEndMode = enabled;
    // LowEndMode è una scelta per-MACCHINA (la grafica va forzata in main_qml
    // prima che il profilo -config sia noto): scrivi sempre nella ROOT, così lo
    // startup probe la trova a prescindere dal profilo attivo.
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    s.setValue(QStringLiteral("LowEndMode"), m_lowEndMode);
    s.sync();

    if (m_lowEndMode) {
        setLowCpuModeEnabled(true);
        setFtThreadsAuto(false);
        if (m_ftThreads > 4) setFtThreads(4);
        setProcessPriority(0);            // Normale: niente preempt su hw debole
        applyReadyProfile(QStringLiteral("cpu"));
        // Le animazioni infinite dei LED/indicatori sono gia' condizionate a
        // uiQuality != "Low": la Modalita' PC lento le spegne, e ricorda la scelta
        // precedente per ripristinarla alla disattivazione.
        if (m_uiQuality != QStringLiteral("Low")) {
            QSettings().setValue(QStringLiteral("UI/QualityBeforeLowEnd"), m_uiQuality);
        }
        setUiQuality(QStringLiteral("Low"));
        emit statusMessage(QStringLiteral("Modalità PC lento attiva. Riavvia Decodium per applicare la grafica OpenGL (più stabile su GPU vecchie)."));
    } else {
        setLowCpuModeEnabled(false);
        if (m_uiQuality == QStringLiteral("Low")) {
            QSettings settings;
            QString const previous = settings.value(QStringLiteral("UI/QualityBeforeLowEnd")).toString();
            if (!previous.isEmpty()) {
                setUiQuality(previous);
                settings.remove(QStringLiteral("UI/QualityBeforeLowEnd"));
            }
        }
        emit statusMessage(QStringLiteral("Modalità PC lento disattivata. Riavvia per tornare alla grafica automatica."));
    }

    emit lowEndModeChanged();
    emit settingValueChanged(QStringLiteral("LowEndMode"), m_lowEndMode);
}

void DecodiumBridge::setResetSafeGraphicsNextLaunch(bool enabled)
{
    if (m_resetSafeGraphicsNextLaunch == enabled)
        return;

    m_resetSafeGraphicsNextLaunch = enabled;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    constexpr auto key = "Graphics/ResetSafeGraphicsNextLaunch";
    if (enabled)
        settings.setValue(QString::fromLatin1(key), true);
    else
        settings.remove(QString::fromLatin1(key));
    settings.sync();

    decodiumBridgeCpuProfileLog(QStringLiteral("Reset graphics next launch: %1")
                  .arg(enabled ? QStringLiteral("armed") : QStringLiteral("cancelled")));
    emit resetSafeGraphicsNextLaunchChanged();
    emit settingValueChanged(QStringLiteral("ResetSafeGraphicsNextLaunch"), enabled);
    emit statusMessage(enabled
                       ? QStringLiteral("Al prossimo avvio verranno ripristinate le impostazioni grafiche automatiche")
                       : QStringLiteral("Reset grafico al prossimo avvio annullato"));
}


// 1.0.388 — priorità processo scelta dall'utente. Sovrascrive il profilo interattivo
// di default (ABOVE_NORMAL). "Tempo reale" è rischioso (può rendere il sistema non
// responsivo) e richiede privilegi admin: senza elevazione Windows lo declassa a HIGH.
void DecodiumBridge::applyProcessPriority()
{
#ifdef Q_OS_WIN
    DWORD targetClass;
    switch (m_processPriority) {
        case 0:  targetClass = NORMAL_PRIORITY_CLASS;       break;
        case 2:  targetClass = HIGH_PRIORITY_CLASS;         break;
        case 3:  targetClass = REALTIME_PRIORITY_CLASS;     break;
        default: targetClass = ABOVE_NORMAL_PRIORITY_CLASS; break;  // 1 = default
    }
    HANDLE const process = GetCurrentProcess();
    if (SetPriorityClass(process, targetClass)) {
        decodiumBridgeCpuProfileLog(QStringLiteral("[Priority] process priority -> livello %1 (class 0x%2)")
                      .arg(m_processPriority)
                      .arg(static_cast<qulonglong>(targetClass), 0, 16));
    } else {
        decodiumBridgeCpuProfileLog(QStringLiteral("[Priority] SetPriorityClass FALLITO livello %1 err=%2 (Tempo reale richiede admin)")
                      .arg(m_processPriority)
                      .arg(static_cast<qulonglong>(GetLastError())));
    }
#endif
}

void DecodiumBridge::setProcessPriority(int v)
{
    int const clamped = qBound(0, v, 3);
    if (m_processPriority != clamped) {
        m_processPriority = clamped;
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        decodium::beginActiveSettingsProfile(settings);
        settings.setValue(QStringLiteral("ProcessPriority"), m_processPriority);
        emit processPriorityChanged();
    }
    applyProcessPriority();
}



