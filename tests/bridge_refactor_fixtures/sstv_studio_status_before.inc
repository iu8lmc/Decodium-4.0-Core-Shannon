#if DECODIUM_HAS_SSTV
void DecodiumBridge::setSstvThumbnailProvider(
    decodium::sstv::SstvThumbnailProvider* provider)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_sstvThumbnailProvider == provider) {
        return;
    }
    if (m_sstvGalleryModel) {
        m_sstvGalleryModel->setThumbnailProvider(nullptr);
    }
    m_sstvThumbnailProvider = provider;
    if (m_sstvGalleryModel) {
        m_sstvGalleryModel->setThumbnailProvider(provider);
    }
}
#endif

std::shared_ptr<const QImage>
DecodiumBridge::sstvTxSourceImageSnapshot() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvStudioController
        ? m_sstvStudioController->sourceSnapshot()
        : std::shared_ptr<const QImage> {};
#else
    return {};
#endif
}

std::shared_ptr<const QImage>
DecodiumBridge::sstvTxPreparedImageSnapshot() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvStudioController
        ? m_sstvStudioController->preparedSnapshot()
        : std::shared_ptr<const QImage> {};
#else
    return {};
#endif
}

std::uint64_t DecodiumBridge::sstvTxNowMs() const noexcept
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvTxClock.isValid()) {
        return 0U;
    }
    const qint64 elapsed = m_sstvTxClock.elapsed();
    return elapsed <= 0 ? 0U : static_cast<std::uint64_t>(elapsed);
#else
    return 0U;
#endif
}

