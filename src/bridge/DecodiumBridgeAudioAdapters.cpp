// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#include "DecodiumBridgeAudioDeviceIdentity.h"

void decodiumBridgeUiActionLog(const QString&);
#define bridgeLog decodiumBridgeUiActionLog

void decodiumBridgeTxOutputResolveLog(const QString& message) { bridgeLog(message); }

QString decodiumCacheDeviceId(const QAudioDevice& d) { return decodiumBridgeAudioDeviceIdForLog(d); }

QString decodiumCacheAudioFormat(const QAudioFormat& f) { return decodiumBridgeAudioFormatForLog(f); }

void DecodiumBridge::initTxDevices()
{
    // Il dispositivo TX viene selezionato inline in startTx() tramite QMediaDevices (Qt6).
    // Questa funzione è mantenuta per compatibilità con le chiamate esistenti.
    bridgeLog("initTxDevices: no-op (setup inline in startTx)");
}

