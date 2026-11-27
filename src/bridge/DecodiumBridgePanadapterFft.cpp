#include "DecodiumBridge.h"
#include "PanadapterItem.hpp"
#include <QDebug>
#include <utility>

void DecodiumBridge::setGpuPanadapterFftAvailable(bool available, const QString& reason)
{
    bool const previous = m_gpuPanadapterFftAvailable.exchange(available);
    if (!available) {
        if (reason.startsWith(QStringLiteral("OpenGL GPU FFT disabled")))
            m_forceGpuPanadapterFft.store(false);
#if defined(DECODIUM_QML_PANADAPTER_DIRECT)
        m_panadapterItems.removeAll(QPointer<PanadapterItem>(nullptr));
        for (const QPointer<PanadapterItem>& ref : std::as_const(m_panadapterItems)) {
            if (PanadapterItem* item = ref.data())
                item->activateCpuSpectrumFallback();
        }
#endif
        setGpuPanadapterFftActive(false, QStringLiteral("CPU FFTW"));
    }
    if (previous == available)
        return;

    if (available) {
        qInfo().noquote()
            << "[PANDBG] Panadapter visual FFT GPU path available"
            << "reason=" << (reason.isEmpty() ? QStringLiteral("manual/auto recovery") : reason);
    } else {
        qWarning().noquote()
            << "[PANDBG] Panadapter visual FFT GPU path disabled"
            << "reason=" << (reason.isEmpty() ? QStringLiteral("PanadapterItem rejected GPU compute") : reason)
            << "fallback=FFTW_CPU";
    }
}

void DecodiumBridge::setGpuPanadapterFftActive(bool active, const QString& backend)
{
    if (active
        && (!m_gpuPanadapterFftAvailable.load()
            || m_gpuPanadapterFftStallGuard.load())) {
        return;
    }
    QString const resolvedBackend = active
        ? (backend.trimmed().isEmpty() ? QStringLiteral("GPU RHI") : backend.trimmed())
        : QStringLiteral("CPU FFTW");
    if (m_panadapterGpuFftActive == active
        && m_panadapterGpuFftBackend == resolvedBackend) {
        return;
    }

    m_panadapterGpuFftActive = active;
    m_panadapterGpuFftBackend = resolvedBackend;
    emit panadapterGpuFftActiveChanged();
    qInfo().noquote()
        << "[PANDBG] Panadapter FFT runtime backend"
        << "active=" << (active ? 1 : 0)
        << "backend=" << resolvedBackend;
}

