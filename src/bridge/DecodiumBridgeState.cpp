// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "Q65DopplerTracker.h"
#include "controllers/ActiveStationsModel.hpp"
#include <QDateTime>

void decodiumBridgeUiActionLog(const QString&);
QString decodiumBridgeLegacyPaletteNameForUiIndex(int);
#define bridgeLog decodiumBridgeUiActionLog

void DecodiumBridge::qmlDebugLog(QString const& msg) const
{
    bridgeLog(QStringLiteral("[QML] %1").arg(msg));
}

QString DecodiumBridge::callsign() const { return m_callsign; }

QString DecodiumBridge::mode() const { return m_mode; }

bool DecodiumBridge::transmitting() const { return m_transmitting; }

bool DecodiumBridge::decoding() const { return m_decoding; }

QVariantList DecodiumBridge::decodeList() const { return m_decodeList; }

QVariantList DecodiumBridge::rxDecodeList() const { return m_rxDecodeList; }

int DecodiumBridge::periodProgress() const { return m_periodProgress; }

QString DecodiumBridge::utcTime() const { return m_utcTime; }

void DecodiumBridge::openCatSettings()
{
    emit catSettingsRequested();
}

void DecodiumBridge::onUtcTimer()
{
    m_utcTime = QDateTime::currentDateTimeUtc().toString("hh:mm:ss");
    emit utcTimeChanged();
}


QObject* DecodiumBridge::q65Doppler() const { return m_q65Doppler; }

QStringList DecodiumBridge::audioInputDevices() const { return m_audioInputDevices; }

QStringList DecodiumBridge::audioOutputDevices() const { return m_audioOutputDevices; }

QString DecodiumBridge::audioInputDevice() const { return m_audioInputDevice; }

void DecodiumBridge::setUiPaletteIndex(int v)
{
    v = qBound(0, v, 10);
    if (m_uiPaletteIndex == v) {
        return;
    }

    m_uiPaletteIndex = v;
    if (usingLegacyBackendForTx() && v <= 5) {
        m_legacyBackend->setWaterfallPalette(decodiumBridgeLegacyPaletteNameForUiIndex(v));
    }
    emit uiPaletteIndexChanged();
}

QObject* DecodiumBridge::activeStations() const
{
    return m_activeStations;
}

