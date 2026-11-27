#include "DecodiumBridge.h"
#include "DecodiumBridgeTxFormat.h"
#include <QCoreApplication>
#include <QTimer>

QAudioDevice decodiumBridgeFindTxOutputDevice(const QString&, const QString&, const QList<QAudioDevice>&, const QAudioDevice&, bool*, QString*);
QString decodiumBridgeAudioDeviceIdSettingForLog(QString);
QString decodiumBridgeAudioDeviceIdForSettings(const QAudioDevice&);
void decodiumBridgeTxPrecomputeLog(const QString&);

QAudioDevice DecodiumBridge::resolveTxOutputDevice(bool* requestedDeviceFound)
{
    if (!m_audioDeviceCacheValid || m_audioDeviceCacheDirty) {
        m_cachedTxOutputDeviceValid = false;
        refreshAudioDeviceCache(QStringLiteral("resolveTxOutputDevice route refresh"), false, true);
    }

    if (m_cachedTxOutputDeviceValid
        && m_cachedTxOutputDeviceName == m_audioOutputDevice
        && m_cachedTxOutputDeviceId == m_audioOutputDeviceId) {
        if (requestedDeviceFound) {
            *requestedDeviceFound = m_cachedTxOutputDeviceFound;
        }
        return m_cachedTxOutputDevice;
    }

    bool found = false;
    QString matchReason;
    QAudioDevice device = decodiumBridgeFindTxOutputDevice(m_audioOutputDevice,
                                           m_audioOutputDeviceId,
                                           cachedAudioOutputs(QStringLiteral("resolveTxOutputDevice"), false),
                                           cachedDefaultAudioOutput(QStringLiteral("resolveTxOutputDevice"), false),
                                           &found,
                                           &matchReason);
    decodiumBridgeTxPrecomputeLog(QStringLiteral("resolveTxOutputDevice: saved=[%1] saved_id=%2 match=%3 found=%4 selected=[%5] selected_id=%6")
                  .arg(m_audioOutputDevice,
                       decodiumBridgeAudioDeviceIdSettingForLog(m_audioOutputDeviceId),
                       matchReason,
                       found ? QStringLiteral("1") : QStringLiteral("0"),
                       device.description(),
                       decodiumBridgeAudioDeviceIdSettingForLog(decodiumBridgeAudioDeviceIdForSettings(device))));
    if (found) {
        rememberAudioOutputDeviceIdentity(device,
                                          QStringLiteral("resolveTxOutputDevice %1").arg(matchReason),
                                          !m_audioOutputDevice.trimmed().isEmpty());
    }
    m_cachedTxOutputDeviceName = m_audioOutputDevice;
    m_cachedTxOutputDeviceId = m_audioOutputDeviceId;
    m_cachedTxOutputDeviceFound = found;
    m_cachedTxOutputDevice = device;
    m_cachedTxOutputDeviceValid = true;
    if (requestedDeviceFound) {
        *requestedDeviceFound = found;
    }
    return device;
}

void DecodiumBridge::scheduleTxAudioPrecompute(int delayMs)
{
    if (m_txAudioPrecomputeScheduled || m_shuttingDown || QCoreApplication::closingDown()) {
        return;
    }

    m_txAudioPrecomputeScheduled = true;
    QTimer::singleShot(qMax(0, delayMs), this, [this]() {
        m_txAudioPrecomputeScheduled = false;
        precomputeTxAudioForCurrentMessage(QStringLiteral("scheduled"));
    });
}

void DecodiumBridge::precomputeTxAudioForCurrentMessage(const QString& reason)
{
    if (m_shuttingDown || QCoreApplication::closingDown() || m_transmitting || m_tuning) {
        return;
    }
    if (m_decoding) {
        // Do not compete with the current decode pass; the startTx() path still
        // has a synchronous fallback if an immediate FT2 reply is required.
        scheduleTxAudioPrecompute(250);
        return;
    }
    if (!m_decodeStartMsBySerial.isEmpty() || m_asyncDecodePending) {
        scheduleTxAudioPrecompute(250);
        return;
    }
    int const slotDelayMs = txAudioPrecomputeRetryDelayMs(m_mode, m_monitoring);
    if (slotDelayMs > 0) {
        scheduleTxAudioPrecompute(slotDelayMs);
        return;
    }
    if (!m_txEnabled && !m_autoCqRepeat && !m_deferredManualSyncTx) {
        return;
    }

    QString const msg = buildCurrentTxMessage();
    if (msg.trimmed().isEmpty()) {
        return;
    }

    // Con la radio remota in uso il PCM per la scheda locale non serve: l'audio
    // esce dalla forma d'onda verso la rete. Costruirlo comunque vuol dire
    // pretendere un dispositivo di uscita che su quel computer puo' benissimo
    // non esserci — e fallire li', prima ancora di provare a trasmettere.
    bool needPcm = !usingTciAudioInput() && !m_decoPortUseRemote;
#if defined(Q_OS_MAC)
    needPcm = shouldUseBridgeAudioForLegacyDigitalTx() && !m_decoPortUseRemote;
#endif

    QVector<float> wave;
    QByteArray pcm;
    QAudioFormat format;
    QAudioDevice device;
    QString error;
    if (ensureTxAudioPrepared(msg, effectiveTxAudioFrequencyHz(), needPcm,
                              &wave, &pcm, &format, &device, &error)) {
        QString const pcmInfo = needPcm
            ? QString::number(pcm.size())
            : QStringLiteral("not-required");
        decodiumBridgeTxPrecomputeLog(QStringLiteral("TX audio precomputed (%1): mode=%2 msg=[%3] samples=%4 pcm=%5")
                      .arg(reason, m_mode, msg.trimmed())
                      .arg(wave.size())
                      .arg(pcmInfo));
    } else if (!error.isEmpty()) {
        decodiumBridgeTxPrecomputeLog(QStringLiteral("TX audio precompute skipped (%1): %2").arg(reason, error));
    }
}

