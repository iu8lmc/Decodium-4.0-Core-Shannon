QObject* DecodiumBridge::catShareObject() const
{
    return m_catShare;
}

QString DecodiumBridge::catBackendForPersistence() const
{
    return (m_catBackend == QLatin1String("decoport") && !m_catBackendBeforeDecoPort.isEmpty())
               ? m_catBackendBeforeDecoPort
               : m_catBackend;
}

QObject* DecodiumBridge::spotShareObject() const
{
    return m_spotShare;
}

QObject* DecodiumBridge::amplifierObject() const
{
    return m_amplifier;
}

