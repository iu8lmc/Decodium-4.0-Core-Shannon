#include "DecodiumBridge.h"
#include "PanadapterItem.hpp"
#include <QDebug>
#include <QPointer>

void DecodiumBridge::registerPanadapterItem(PanadapterItem* item)
{
    if (!item)
        return;

    // 1.0.347 - lista di target PCM (non piu' puntatore singolo): in DX-Pedition
    // coesistono 2 PanadapterItem (classico + tactical). Con un solo slot, la
    // distruzione del waterfall tactical all'uscita azzerava il feed lasciando il
    // classico congelato. Con la lista ogni item vivo registrato riceve il PCM.
    m_panadapterItems.removeAll(QPointer<PanadapterItem>(nullptr));
    if (m_panadapterItems.contains(QPointer<PanadapterItem>(item)))
        return;

    m_panadapterItems.append(QPointer<PanadapterItem>(item));
    qInfo().noquote()
        << "[PANDBG] Panadapter PCM frame feed registered"
        << "route=C++_I16_ring"
        << "qml_bypass=1"
        << "bridge_vector_float=0"
        << "targets=" << m_panadapterItems.size();
}

void DecodiumBridge::unregisterPanadapterItem(PanadapterItem* item)
{
    m_panadapterItems.removeAll(QPointer<PanadapterItem>(item));
    m_panadapterItems.removeAll(QPointer<PanadapterItem>(nullptr));
    qInfo().noquote()
        << "[PANDBG] Panadapter PCM frame feed unregistered"
        << "targets=" << m_panadapterItems.size();
}

