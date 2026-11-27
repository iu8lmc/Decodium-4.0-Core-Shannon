bool DecodiumBridge::sstvTxUsesVoxPtt() const
{
#if DECODIUM_HAS_SSTV
    return activeCatUsesVoxPtt(this, m_hamlibCat, m_catBackend,
                               m_omniRigCat, m_cat4OmCat);
#else
    return false;
#endif
}

bool DecodiumBridge::sstvTxCanControlPtt() const
{
#if DECODIUM_HAS_SSTV
    return activeCatCanPtt(this, m_hamlibCat, m_catBackend,
                           m_omniRigCat, m_legacyBackend, m_cat4OmCat);
#else
    return false;
#endif
}

bool DecodiumBridge::sstvTxPttActive() const
{
#if DECODIUM_HAS_SSTV
    if (decoPortIsCat(m_catBackend)) {
        return remoteLinkIfAny() && remoteLinkIfAny()->isLinked()
            && remoteLinkIfAny()->ptt();
    }
    return activeCatReportsPttActive();
#else
    return false;
#endif
}

void DecodiumBridge::setSstvTxPtt(bool on)
{
#if DECODIUM_HAS_SSTV
    activeCatSetPtt(this, m_hamlibCat, m_catBackend, on,
                    m_omniRigCat, m_legacyBackend, m_cat4OmCat);
#else
    Q_UNUSED(on)
#endif
}

