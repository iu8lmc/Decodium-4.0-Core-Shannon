#pragma once

namespace decodium {
namespace cat {

// Opening a serial device is not electrically neutral on every USB/serial
// driver.  Some adapters briefly assert DTR or RTS while the port is being
// brought up.  For an ordinary CAT connection those lines are unused, but a
// radio interface may still wire either one to PTT.  Make the safe state
// explicit before Hamlib opens the port, while preserving an intentional
// DTR/RTS PTT line and hardware flow control.
struct SerialOpenLineSafetyInput
{
    bool serialCat {false};
    bool canForceDtr {false};
    bool canForceRts {false};
    bool userForceDtr {false};
    bool userDtrHigh {false};
    bool userForceRts {false};
    bool userRtsHigh {false};
    bool hardwareHandshake {false};
    bool dtrIsPttOnCatPort {false};
    bool rtsIsPttOnCatPort {false};
};

struct SerialOpenLineSafety
{
    bool forceDtr {false};
    bool dtrHigh {false};
    bool forceRts {false};
    bool rtsHigh {false};
    bool automaticDtrLow {false};
    bool automaticRtsLow {false};
};

inline SerialOpenLineSafety serialOpenLineSafety(SerialOpenLineSafetyInput const& input)
{
    SerialOpenLineSafety result;

    result.automaticDtrLow = input.serialCat
        && input.canForceDtr
        && !input.userForceDtr
        && !input.dtrIsPttOnCatPort;
    result.automaticRtsLow = input.serialCat
        && input.canForceRts
        && !input.userForceRts
        && !input.hardwareHandshake
        && !input.rtsIsPttOnCatPort;

    result.forceDtr = input.canForceDtr
        && (input.userForceDtr || result.automaticDtrLow);
    result.dtrHigh = input.canForceDtr
        && input.userForceDtr
        && input.userDtrHigh;
    result.forceRts = input.canForceRts
        && (input.userForceRts || result.automaticRtsLow);
    result.rtsHigh = input.canForceRts
        && input.userForceRts
        && input.userRtsHigh;
    return result;
}

} // namespace cat
} // namespace decodium
