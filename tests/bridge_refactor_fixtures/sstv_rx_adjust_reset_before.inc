void DecodiumBridge::resetSstvRxAfc()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->resetRxAfc();
        emit sstvRxSnapshotChanged();
    }
#endif
}

void DecodiumBridge::resetSstvRxSlant()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->resetRxSlant();
        emit sstvRxSnapshotChanged();
    }
#endif
}

