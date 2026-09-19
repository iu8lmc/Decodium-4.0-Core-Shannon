#pragma once

#include <QtGlobal>
#include <QString>

// Small, dependency-free policy object for FT2-Link satellite half-duplex.
// The CAT implementation lives in DecodiumBridge/DecodiumTransceiverManager;
// keeping the validation here makes the safety contract easy to test without
// requiring a physical radio.
namespace decodium {
namespace ft2link_satellite {

struct HalfDuplexConfiguration
{
    bool enabled {false};
    qint64 rxDialHz {0};
    qint64 txDialHz {0};
    int settleMs {900};
};

inline int normalizedSettleMs(int value)
{
    return qBound(250, value, 5000);
}

inline QString validationError(const HalfDuplexConfiguration& configuration)
{
    if (!configuration.enabled) {
        return {};
    }

    // The intentionally broad range covers ordinary IF radios as well as
    // transverter/SDR front ends used on QO-100, without accepting obviously
    // malformed values from a settings file.
    constexpr qint64 kMinimumDialHz = 1000000;       // 1 MHz
    constexpr qint64 kMaximumDialHz = 30000000000LL; // 30 GHz
    if (configuration.rxDialHz < kMinimumDialHz
        || configuration.rxDialHz > kMaximumDialHz) {
        return QStringLiteral("A valid RX/downlink dial frequency is required");
    }
    if (configuration.txDialHz < kMinimumDialHz
        || configuration.txDialHz > kMaximumDialHz) {
        return QStringLiteral("A valid TX/uplink dial frequency is required");
    }
    if (configuration.rxDialHz == configuration.txDialHz) {
        return QStringLiteral("RX and TX dial frequencies must be different for satellite half-duplex");
    }
    return {};
}

inline bool isValid(const HalfDuplexConfiguration& configuration)
{
    return validationError(configuration).isEmpty();
}

} // namespace ft2link_satellite
} // namespace decodium
