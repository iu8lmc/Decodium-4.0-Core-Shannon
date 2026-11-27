// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvTxCoordinator.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstdint>

int DecodiumBridge::sstvPttLeadMs() const
{
    const double legacySeconds = getSetting(
        QStringLiteral("TxDelay"), 0.2).toDouble();
    const double safeLegacy = std::isfinite(legacySeconds)
        ? std::clamp(legacySeconds, 0.0, 5.0) : 0.2;
    const int fallback = static_cast<int>(std::llround(safeLegacy * 1'000.0));
    return std::clamp(getSetting(
        QStringLiteral("SSTV/PttLeadMs"), fallback).toInt(), 0, 5'000);
}

void DecodiumBridge::setSstvPttLeadMs(int value)
{
    const int bounded = std::clamp(value, 0, 5'000);
    if (bounded == sstvPttLeadMs()) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const int previous = sstvPttLeadMs();
    setSetting(QStringLiteral("SSTV/PttLeadMs"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/PttLeadMs"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

int DecodiumBridge::sstvPttTailMs() const
{
    return std::clamp(getSetting(
        QStringLiteral("SSTV/PttTailMs"), 500).toInt(), 0, 5'000);
}

void DecodiumBridge::setSstvPttTailMs(int value)
{
    const int bounded = std::clamp(value, 0, 5'000);
    if (bounded == sstvPttTailMs()) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const int previous = sstvPttTailMs();
    setSetting(QStringLiteral("SSTV/PttTailMs"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/PttTailMs"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

int DecodiumBridge::sstvPttReleaseRetryMs() const
{
    return std::clamp(getSetting(
        QStringLiteral("SSTV/PttReleaseRetryMs"), 500).toInt(),
        100, 2'000);
}

void DecodiumBridge::setSstvPttReleaseRetryMs(int value)
{
    const int bounded = std::clamp(value, 100, 2'000);
    if (bounded == sstvPttReleaseRetryMs()) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const int previous = sstvPttReleaseRetryMs();
    setSetting(QStringLiteral("SSTV/PttReleaseRetryMs"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/PttReleaseRetryMs"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

int DecodiumBridge::sstvVoxPreKeyMs() const
{
    return std::clamp(getSetting(
        QStringLiteral("SSTV/VoxPreKeyMs"), 750).toInt(), 100, 4'000);
}

void DecodiumBridge::setSstvVoxPreKeyMs(int value)
{
    const int bounded = std::clamp(value, 100, 4'000);
    if (bounded == sstvVoxPreKeyMs()) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const int previous = sstvVoxPreKeyMs();
    setSetting(QStringLiteral("SSTV/VoxPreKeyMs"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/VoxPreKeyMs"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

int DecodiumBridge::sstvVoxHangMs() const
{
    return std::clamp(getSetting(
        QStringLiteral("SSTV/VoxHangMs"), 500).toInt(), 100, 5'000);
}

void DecodiumBridge::setSstvVoxHangMs(int value)
{
    const int bounded = std::clamp(value, 100, 5'000);
    if (bounded == sstvVoxHangMs()) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const int previous = sstvVoxHangMs();
    setSetting(QStringLiteral("SSTV/VoxHangMs"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/VoxHangMs"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

double DecodiumBridge::sstvVoxToneFrequencyHz() const
{
    const double value = getSetting(
        QStringLiteral("SSTV/VoxToneFrequencyHz"), 1'900.0).toDouble();
    return std::isfinite(value)
        ? std::clamp(value, 300.0, 3'000.0) : 1'900.0;
}

void DecodiumBridge::setSstvVoxToneFrequencyHz(double value)
{
    const double bounded = std::isfinite(value)
        ? std::clamp(value, 300.0, 3'000.0) : 1'900.0;
    if (qFuzzyCompare(bounded + 1.0, sstvVoxToneFrequencyHz() + 1.0)) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const double previous = sstvVoxToneFrequencyHz();
    setSetting(QStringLiteral("SSTV/VoxToneFrequencyHz"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/VoxToneFrequencyHz"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

double DecodiumBridge::sstvVoxToneLevel() const
{
    const double value = getSetting(
        QStringLiteral("SSTV/VoxToneLevel"), 0.5).toDouble();
    return std::isfinite(value)
        ? std::clamp(value, 0.05, 1.0) : 0.5;
}

void DecodiumBridge::setSstvVoxToneLevel(double value)
{
    const double bounded = std::isfinite(value)
        ? std::clamp(value, 0.05, 1.0) : 0.5;
    if (qFuzzyCompare(bounded + 1.0, sstvVoxToneLevel() + 1.0)) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const double previous = sstvVoxToneLevel();
    setSetting(QStringLiteral("SSTV/VoxToneLevel"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/VoxToneLevel"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

bool DecodiumBridge::applySstvTxTimingSettings()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvTxCoordinator) {
        return true;
    }
    decodium::sstv::SstvTxTimingConfig timing;
    timing.pttLeadDelayMs = static_cast<std::uint64_t>(sstvPttLeadMs());
    timing.pttTailDelayMs = static_cast<std::uint64_t>(sstvPttTailMs());
    timing.pttReleaseRetryMs = static_cast<std::uint64_t>(
        sstvPttReleaseRetryMs());
    timing.voxPreKeyMs = static_cast<std::uint64_t>(sstvVoxPreKeyMs());
    timing.voxHangMs = static_cast<std::uint64_t>(sstvVoxHangMs());
    timing.voxToneFrequencyHz = sstvVoxToneFrequencyHz();
    timing.voxToneLevel = sstvVoxToneLevel();
    return m_sstvTxCoordinator->updateTimingConfig(timing);
#else
    return true;
#endif
}

