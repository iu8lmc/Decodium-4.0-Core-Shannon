void DecodiumBridge::refreshDecoPortHub()
{
    if (m_decoPortHubBusy)
        return;
    m_decoPortHubBusy = true;
    QString const catPort = m_hamlibCat ? m_hamlibCat->serialPort().trimmed().toUpper() : QString();
    int const sharePort = m_catShare ? m_catShare->port() : 4533;
    QPointer<DecodiumBridge> self(this);
    (void)QtConcurrent::run([self, catPort, sharePort]() {
        QVariantList const rows = buildDecoPortHubRows(catPort, sharePort);
        QMetaObject::invokeMethod(qApp, [self, rows]() {
            if (!self)
                return;
            self->m_decoPortHub = rows;
            self->m_decoPortHubBusy = false;
            emit self->decoPortHubChanged();
        }, Qt::QueuedConnection);
    });
}

