int ftAutoUiReserveForCores(int cores)
{
    int const boundedCores = qMax(1, cores);
    if (qEnvironmentVariableIsSet("DECODIUM_FT_AUTO_UI_RESERVE")) {
        int const envReserve = qEnvironmentVariableIntValue("DECODIUM_FT_AUTO_UI_RESERVE");
        return qBound(0, envReserve, qMax(0, boundedCores - 1));
    }
#ifdef Q_OS_WIN
    if (boundedCores <= 1) {
        return 0;
    }
    if (boundedCores <= 4) {
        return 1;
    }
    if (boundedCores <= 7) {
        return 2;
    }
    if (boundedCores >= 16) {
        return 4;
    }
    return 2;
#else
    if (boundedCores <= 1) {
        return 0;
    }
    if (boundedCores <= 4) {
        return 1;
    }
    if (boundedCores <= 11) {
        return 2;
    }
    return 3;
#endif
}

int autoFtThreadCountForCores(int cores)
{
    int const boundedCores = qMax(1, cores);
    int const reserve = ftAutoUiReserveForCores(boundedCores);
    return qBound(1, boundedCores - reserve, kMaxFtDecodeThreads);
}

int adaptiveInteractiveFtThreadCount(int normalLimit)
{
    // effectiveFtThreadLimit() has already applied manual settings and
    // runtime CPU-pressure reductions. This second cap reserves execution
    // capacity for Qt Quick, the render thread and the OS compositor.
    return decodium::decode::adaptiveInteractiveThreadCount(
        QThread::idealThreadCount(), normalLimit, kMaxFtDecodeThreads);
}

}

int decodiumBridgeFtAutoUiReserveForCores(int cores) { return ftAutoUiReserveForCores(cores); }
int decodiumBridgeAutoFtThreadCountForCores(int cores) { return autoFtThreadCountForCores(cores); }
int decodiumBridgeAdaptiveInteractiveFtThreadCount(int limit) { return adaptiveInteractiveFtThreadCount(limit); }
