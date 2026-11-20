// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/integration/SstvRxAudioJobController.h"
using decodium::sstv::SstvRxRuntime;
using decodium::sstv::SstvRxControlSettings;
using decodium::sstv::SstvRxRedecodeParameters;
bool decodiumSstvParseAfc(const QVariant&, decodium::sstv::SstvRxAfcMode*);
bool decodiumSstvParseSlant(const QVariant&, decodium::sstv::SstvRxSlantMode*);
bool decodiumSstvSelectableMode(const std::string&);
#define parseSstvRxAfcMode decodiumSstvParseAfc
#define parseSstvRxSlantMode decodiumSstvParseSlant
#define selectableSstvRxMode decodiumSstvSelectableMode
#endif

bool DecodiumBridge::redecodeRecentSstv(
    const QVariantMap& parameters)
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime || !m_sstvRxAudioJobController
        || m_sstvRxAudioJobController->busy()
        || sstvWavReplayActive() || m_sstvRedecodeActive) {
        return false;
    }
    const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
    if (snapshot.replay.retainedSamples == 0U) {
        emit errorMessage(tr("No retained SSTV audio is available to re-decode"));
        return false;
    }

    const SstvRxControlSettings controls =
        m_sstvRxRuntime->rxControlSnapshot().settings;
    SstvRxRedecodeParameters parsed;
    parsed.afcMode = controls.afcMode;
    parsed.frequencyCorrectionHz = controls.manualFrequencyCorrectionHz;
    parsed.slantMode = controls.slantMode;
    parsed.clockErrorPpm = controls.manualClockErrorPpm;
    if (parameters.contains(QStringLiteral("mode"))) {
        const QString mode = parameters.value(QStringLiteral("mode"))
                                 .toString().trimmed();
        if (mode.compare(QStringLiteral("auto"),
                         Qt::CaseInsensitive) != 0) {
            parsed.mode = mode.toStdString();
        }
    }
    if (parameters.contains(QStringLiteral("afcMode"))
        && !parseSstvRxAfcMode(parameters.value(QStringLiteral("afcMode")),
                               &parsed.afcMode)) {
        return false;
    }
    if (parameters.contains(QStringLiteral("frequencyCorrectionHz"))) {
        bool ok = false;
        parsed.frequencyCorrectionHz = parameters.value(
            QStringLiteral("frequencyCorrectionHz")).toDouble(&ok);
        if (!ok) {
            return false;
        }
    }
    if (parameters.contains(QStringLiteral("slantMode"))
        && !parseSstvRxSlantMode(
            parameters.value(QStringLiteral("slantMode")),
            &parsed.slantMode)) {
        return false;
    }
    if (parameters.contains(QStringLiteral("clockErrorPpm"))) {
        bool ok = false;
        parsed.clockErrorPpm = parameters.value(
            QStringLiteral("clockErrorPpm")).toDouble(&ok);
        if (!ok) {
            return false;
        }
    }
    if ((!parsed.mode.empty() && !selectableSstvRxMode(parsed.mode))
        || !decodium::sstv::SstvRxControlPolicy::
               redecodeParametersAreValid(parsed)) {
        emit errorMessage(tr("Invalid SSTV re-decode parameters"));
        return false;
    }
    const bool started = m_sstvRxAudioJobController
        ->prepareRecentRedecode(std::move(parsed));
    if (started) {
        emit sstvRxAudioJobChanged();
    }
    return started;
#else
    Q_UNUSED(parameters)
    return false;
#endif
}
