    bool const openGlGpuFftRequested =
        qEnvironmentVariableIsSet("DECODIUM_ENABLE_EXPERIMENTAL_OPENGL_GPU_PANADAPTER_FFT")
        || getSetting(QStringLiteral("OpenGlGpuPanadapterFft"), false).toBool();
    m_openGlGpuPanadapterFftEnabled.store(openGlGpuFftRequested,
                                           std::memory_order_relaxed);

    if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_GPU_PANADAPTER_FFT")) {
        m_gpuPanadapterFftAvailable.store(false);
        m_forceGpuPanadapterFft.store(false);
        qWarning().noquote()
            << "[PANDBG] Panadapter visual FFT GPU path disabled"
            << "reason=DECODIUM_DISABLE_GPU_PANADAPTER_FFT";
    } else {
        if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_GPU_PANADAPTER_FORCE"))
            m_forceGpuPanadapterFft.store(false);

        qInfo().noquote()
            << "[PANDBG] Panadapter visual FFT GPU-first path"
            << "force=" << (m_forceGpuPanadapterFft.load() ? 1 : 0)
            << "opengl_opt_in=" << (openGlGpuFftRequested ? 1 : 0)
            << "fallback=FFTW_CPU_on_failure";
    }

