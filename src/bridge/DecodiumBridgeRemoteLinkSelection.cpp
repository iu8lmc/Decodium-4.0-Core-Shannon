#include "DecodiumBridge.h"
#include "DecoPortLink.h"
#include "DecolinkLink.h"

RemoteRadioLink* DecodiumBridge::remoteLink() const
{
    if (m_remoteIsDecolink) {
        decolinkLinkObject();
        return m_decolinkLink;
    }
    decoPortLinkObject();
    return m_decoPortLink;
}

RemoteRadioLink* DecodiumBridge::remoteLinkIfAny() const
{
    if (m_remoteIsDecolink)
        return m_decolinkLink;
    return m_decoPortLink;
}

// Decolink prende il posto della scheda audio e del CAT locali con lo stesso
// meccanismo di DecoPort: cambia solo il collegamento che sta dietro.
void DecodiumBridge::setDecolinkUseRemote(bool on)
{
    ssbStop();
    if (on) {
        if (m_decoPortUseRemote && !m_remoteIsDecolink)
            setDecoPortUseRemote(false);
        m_remoteIsDecolink = true;
        setDecoPortUseRemote(true);
        if (!m_decoPortUseRemote)
            m_remoteIsDecolink = false;
    } else if (m_remoteIsDecolink) {
        setDecoPortUseRemote(false);
        m_remoteIsDecolink = false;
    }
    emit decoPortUseRemoteChanged();
}

