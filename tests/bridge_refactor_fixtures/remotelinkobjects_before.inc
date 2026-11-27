QObject* DecodiumBridge::decoPortDiscoveryObject() const
{
    if (!m_decoPortDiscovery) {
        m_decoPortDiscovery = new DecoPortDiscovery(const_cast<DecodiumBridge*>(this));
        m_decoPortDiscovery->setAuthKey(decoPortAuthKey());
    }
    return m_decoPortDiscovery;
}

QObject* DecodiumBridge::decoPortLinkObject() const
{
    if (!m_decoPortLink) {
        m_decoPortLink = new DecoPortLink(const_cast<DecodiumBridge*>(this));
        m_decoPortLink->setAuthKey(decoPortAuthKey());
        // Se il collegamento cade, la scheda audio locale deve tornare da sola:
        // restare a decodificare il silenzio sarebbe il modo peggiore di
        // accorgersi che la radio remota non c'e' piu'.
        auto* self = const_cast<DecodiumBridge*>(this);
        connect(m_decoPortLink, &DecoPortLink::linkedChanged, self, [self]() {
            if (self->m_decoPortUseRemote && !self->m_remoteIsDecolink && self->m_decoPortLink
                && !self->m_decoPortLink->isLinked())
                self->setDecoPortUseRemote(false);
        });
#if DECODIUM_HAS_SSTV
        connect(m_decoPortLink, &DecoPortLink::remoteStreamChanged,
                self, [self](quint32 streamId) {
            if (!self->m_decoPortUseRemote || !self->m_sstvRxRequested
                || streamId == 0U) {
                return;
            }
            self->selectSstvRxSource(
                decodium::sstv::SstvAudioSourceKind::DecoPort,
                streamId);
        }, Qt::DirectConnection);
#endif
    }
    return m_decoPortLink;
}

QObject* DecodiumBridge::decolinkLinkObject() const
{
    if (!m_decolinkLink) {
        m_decolinkLink = new DecolinkLink(const_cast<DecodiumBridge*>(this));
        auto* self = const_cast<DecodiumBridge*>(this);
        // Come per DecoPort: se il relay o il gateway spariscono, torna la
        // scheda audio locale invece di decodificare silenzio.
        connect(m_decolinkLink, &DecolinkLink::linkedChanged, self, [self]() {
            if (self->m_decoPortUseRemote && self->m_remoteIsDecolink
                && self->m_decolinkLink && !self->m_decolinkLink->isLinked())
                self->setDecolinkUseRemote(false);
        });
#if DECODIUM_HAS_SSTV
        connect(m_decolinkLink, &RemoteRadioLink::remoteStreamChanged,
                self, [self](quint32 streamId) {
            if (!self->m_decoPortUseRemote || !self->m_remoteIsDecolink
                || !self->m_sstvRxRequested || streamId == 0U) {
                return;
            }
            self->selectSstvRxSource(
                decodium::sstv::SstvAudioSourceKind::DecoPort,
                streamId);
        }, Qt::DirectConnection);
#endif
    }
    return m_decolinkLink;
}

