#include "DecodiumBridge.h"
#include "src/rtl/RtlSdrInput.h"

bool DecodiumBridge::rtlSdrSupported() const
{
    return RtlSdrInput::compiledIn();
}

QStringList DecodiumBridge::rtlSdrDevices() const
{
    return m_rtlSdrDevices;
}

bool DecodiumBridge::rtlSdrRunning() const
{
    return m_rtlSdrInput && m_rtlSdrInput->isRunning();
}

QString DecodiumBridge::rtlSdrStatus() const
{
    return m_rtlSdrStatus;
}

bool DecodiumBridge::rtlSdrRfView() const
{
    return rtlSdrEnabled() && m_rtlSdrInput && m_rtlSdrInput->isRunning()
        && m_rtlSdrRfSampleRate > 0 && m_rtlSdrRfCenterFrequencyHz > 0;
}

int DecodiumBridge::rtlSdrRfSampleRate() const
{
    return m_rtlSdrRfSampleRate;
}

double DecodiumBridge::rtlSdrRfCenterFrequency() const
{
    return static_cast<double>(m_rtlSdrRfCenterFrequencyHz);
}

double DecodiumBridge::rtlSdrRfSelectedFrequency() const
{
    return static_cast<double>(m_rtlSdrRfSelectedFrequencyHz);
}

