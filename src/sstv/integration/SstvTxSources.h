// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../analog/SstvAvt.h"
#include "../analog/SstvMartinM1.h"
#include "../analog/SstvMmsstvExtended.h"
#include "../analog/SstvPd.h"
#include "../analog/SstvRobot.h"
#include "../analog/SstvScottie.h"
#include "../analog/SstvSequentialRgb.h"
#include "../tx/SstvFskIdTxStream.h"
#include "SstvTxAudioDevice.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace decodium::sstv {

enum class SstvCalibrationToneKind : std::uint8_t
{
    SyncReference,
    BlackReference,
    LeaderReference,
    WhiteReference,
};

struct SstvCalibrationToneSpec final
{
    SstvCalibrationToneKind kind {SstvCalibrationToneKind::SyncReference};
    const char* id {"sync-1200"};
    const char* displayName {"1200 Hz sync"};
    double frequencyHz {1'200.0};
};

// Operator calibration references use the same bounded pull contract as every
// native SSTV encoder.  The bridge can therefore submit them to
// SstvTxCoordinator::startPrepared(), preserving the normal audio lease,
// CAT/PTT confirmation, watchdog, cancellation and shutdown path.
const SstvCalibrationToneSpec& calibrationToneSpec(
    SstvCalibrationToneKind kind);
std::optional<SstvCalibrationToneKind> calibrationToneKindFromId(
    std::string_view id) noexcept;
std::unique_ptr<SstvPcm16Source> makeCalibrationTonePcm16Source(
    SstvCalibrationToneKind kind,
    std::uint32_t sampleRate = 48'000U,
    std::uint32_t durationMilliseconds = 2'000U,
    double level = 1.0,
    double headroom = kDefaultSstvTxHeadroom);

// Type-erased adapters used by the Qt audio device.  They keep the native
// pull encoders independent from QIODevice and preserve the one-frame plus
// bounded-scratch memory model through the Decodium SoundOutput boundary.
std::unique_ptr<SstvPcm16Source> makeAvtPcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvAvtEncoderConfig config = {});

std::unique_ptr<SstvPcm16Source> makeMartinM1Pcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvMartinM1EncoderConfig config = {});

std::unique_ptr<SstvPcm16Source> makeMmsstvPcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvMmsstvEncoderConfig config = {});

std::unique_ptr<SstvPcm16Source> makeScottiePcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvScottieEncoderConfig config = {});

std::unique_ptr<SstvPcm16Source> makePdPcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvPdEncoderConfig config = {});

std::unique_ptr<SstvPcm16Source> makeRobotPcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvRobotEncoderConfig config = {});

std::unique_ptr<SstvPcm16Source> makeSequentialRgbPcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvSequentialRgbEncoderConfig config = {});

std::unique_ptr<SstvPcm16Source> makeFskIdPcm16Source(
    std::string_view text,
    SstvFskIdTxConfig config = {});

} // namespace decodium::sstv
