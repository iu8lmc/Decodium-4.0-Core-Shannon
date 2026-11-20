#pragma once

#include <QString>

namespace decodium::cat {

// The immutable set of values used to create one Hamlib/TCI CAT session.
// Keeping it explicit prevents profile loads from reconnecting half way
// through an update and makes every endpoint-changing field auditable.
struct ConfigurationSnapshot
{
    QString rigName;
    QString serialPort;
    QString networkPort;
    QString tciPort;
    QString portType;
    int baudRate {0};
    QString dataBits;
    QString stopBits;
    QString handshake;
    bool forceDtr {false};
    bool dtrHigh {false};
    bool forceRts {false};
    bool rtsHigh {false};
    QString pttMethod;
    QString pttPort;
    QString splitMode;
    int civAddress {0};
    bool catKeepAlive {false};
    int pollInterval {0};
    bool tciAudioEnabled {false};

    bool operator==(ConfigurationSnapshot const& other) const noexcept
    {
        return rigName == other.rigName
            && serialPort == other.serialPort
            && networkPort == other.networkPort
            && tciPort == other.tciPort
            && portType == other.portType
            && baudRate == other.baudRate
            && dataBits == other.dataBits
            && stopBits == other.stopBits
            && handshake == other.handshake
            && forceDtr == other.forceDtr
            && dtrHigh == other.dtrHigh
            && forceRts == other.forceRts
            && rtsHigh == other.rtsHigh
            && pttMethod == other.pttMethod
            && pttPort == other.pttPort
            && splitMode == other.splitMode
            && civAddress == other.civAddress
            && catKeepAlive == other.catKeepAlive
            && pollInterval == other.pollInterval
            && tciAudioEnabled == other.tciAudioEnabled;
    }

    bool operator!=(ConfigurationSnapshot const& other) const noexcept
    {
        return !(*this == other);
    }

    // When these values change a remembered physical-device identity cannot
    // safely be carried across to the replacement session.
    bool endpointDiffers(ConfigurationSnapshot const& other) const noexcept
    {
        return rigName != other.rigName
            || serialPort != other.serialPort
            || networkPort != other.networkPort
            || tciPort != other.tciPort
            || portType != other.portType;
    }
};

} // namespace decodium::cat
