#pragma once

#include <QtCore/QtGlobal>

#include <algorithm>

namespace decodium::rtl_sdr {

enum class IfSideband {
    Usb,
    Lsb
};

struct TuningRequest {
    qint64 dialFrequencyHz {14074000};
    int sampleRate {240000};
    bool ifEnabled {false};
    qint64 ifFrequencyHz {8830000};
    qint64 usbShiftHz {1500};
    qint64 lsbShiftHz {-1500};
    IfSideband sideband {IfSideband::Usb};
    bool spectrumInverted {false};
};

struct TuningPlan {
    qint64 dialFrequencyHz {0};
    qint64 selectedInputFrequencyHz {0};
    quint32 hardwareCenterFrequencyHz {0};
    int channelOffsetHz {0};
    quint32 displayCenterFrequencyHz {0};
    quint32 displaySelectedFrequencyHz {0};
    bool ifEnabled {false};
    bool spectrumInverted {false};
};

inline TuningPlan makeTuningPlan(const TuningRequest& request,
                                 qint64 minimumHardwareFrequencyHz = 100000,
                                 qint64 maximumHardwareFrequencyHz = 1766000000)
{
    TuningPlan plan;
    plan.ifEnabled = request.ifEnabled;
    plan.spectrumInverted = request.ifEnabled && request.spectrumInverted;
    plan.dialFrequencyHz = std::clamp(request.dialFrequencyHz,
                                     minimumHardwareFrequencyHz,
                                     maximumHardwareFrequencyHz);

    const qint64 sidebandShift = request.sideband == IfSideband::Lsb
        ? request.lsbShiftHz : request.usbShiftHz;
    const qint64 requestedInputFrequency = request.ifEnabled
        ? request.ifFrequencyHz + sidebandShift
        : plan.dialFrequencyHz;
    plan.selectedInputFrequencyHz = std::clamp(requestedInputFrequency,
                                               minimumHardwareFrequencyHz,
                                               maximumHardwareFrequencyHz);

    // Offset tuning keeps the selected channel away from the zero-IF DC spur.
    const qint64 preferredOffset = std::max<qint64>(12000, request.sampleRate / 4);
    qint64 hardwareCenter = plan.selectedInputFrequencyHz + preferredOffset;
    if (hardwareCenter > maximumHardwareFrequencyHz) {
        hardwareCenter = plan.selectedInputFrequencyHz - preferredOffset;
    }
    hardwareCenter = std::clamp(hardwareCenter,
                                minimumHardwareFrequencyHz,
                                maximumHardwareFrequencyHz);
    plan.hardwareCenterFrequencyHz = static_cast<quint32>(hardwareCenter);
    plan.channelOffsetHz = static_cast<int>(plan.selectedInputFrequencyHz
                                            - hardwareCenter);

    // The panadapter remains on the radio dial scale even when the dongle is
    // connected to a fixed IF output.  An inverted IF reverses the relation
    // between physical IF bins and radio-frequency bins.
    const qint64 displayCenter = plan.dialFrequencyHz
        + (plan.spectrumInverted ? plan.channelOffsetHz : -plan.channelOffsetHz);
    plan.displayCenterFrequencyHz = static_cast<quint32>(std::clamp(
        displayCenter, minimumHardwareFrequencyHz, maximumHardwareFrequencyHz));
    plan.displaySelectedFrequencyHz = static_cast<quint32>(plan.dialFrequencyHz);
    return plan;
}

} // namespace decodium::rtl_sdr
