#include "DecodiumBridge.h"
#include "UdpTargetHelpers.h"
#include "Network/DecodiumCloudlogLite.h"
#include "Network/DecodiumQrzLogbookLite.h"
#include <QTimer>

void decodiumBridgeExternalAdifLog(const QString&);

QVariantMap DecodiumBridge::uploadExternalAdif(const QString& dxCall,
                                               const QString& adifRecord,
                                               const QString& target)
{
    return uploadExternalAdifInternal(0, dxCall, adifRecord, target);
}

QVariantMap DecodiumBridge::uploadExternalAdifForOutbox(quint32 uploadId,
                                                        const QString& dxCall,
                                                        const QString& adifRecord,
                                                        const QString& target)
{
    return uploadExternalAdifInternal(uploadId, dxCall, adifRecord, target);
}

QVariantMap DecodiumBridge::uploadExternalAdifInternal(quint32 uploadId,
                                                       const QString& dxCall,
                                                       const QString& adifRecord,
                                                       const QString& target)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    if (uploadId != 0) {
        result.insert(QStringLiteral("uploadId"), uploadId);
    }

    QString const cleanCall = dxCall.trimmed().toUpper();
    QByteArray const adifBytes = adifRecord.trimmed().toUtf8();
    QString cleanTarget = target.trimmed().toUpper();
    if (cleanTarget != QStringLiteral("UDP")
        && cleanTarget != QStringLiteral("TCP")
        && cleanTarget != QStringLiteral("N1MM")
        && cleanTarget != QStringLiteral("QRZ")
        && cleanTarget != QStringLiteral("CLOUDLOG")
        && cleanTarget != QStringLiteral("ALL")) {
        cleanTarget = QStringLiteral("ALL");
    }

    result.insert(QStringLiteral("target"), cleanTarget);
    result.insert(QStringLiteral("dxCall"), cleanCall);
    result.insert(QStringLiteral("bytes"), adifBytes.size());

    if (cleanCall.isEmpty() || adifBytes.trimmed().isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Missing call or ADIF payload"));
        return result;
    }

    QStringList attempted;
    QStringList skipped;
    QStringList pendingAsync;
    auto wants = [&cleanTarget](QString const& name) {
        return cleanTarget == QStringLiteral("ALL") || cleanTarget == name;
    };

    if (wants(QStringLiteral("UDP"))) {
        int udpAttempts = 0;
        int udpDelivered = 0;
        QString const primaryServer =
            getSetting(QStringLiteral("UDPServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
        quint16 const primaryPort = udpPortFromSettingValue(
            getSetting(QStringLiteral("UDPServerPort"), 2237), 2237);
        if (udpTrafficEnabled(QStringLiteral("UDPPrimary"), QStringLiteral("Qso"))) {
            ++udpAttempts;
            if (udpSendRawAdifDatagram(QStringLiteral("FT2-Link UDP primary raw ADIF"),
                                       primaryServer, primaryPort, cleanCall, adifBytes)) {
                ++udpDelivered;
                attempted << QStringLiteral("UDP primary %1:%2").arg(primaryServer).arg(primaryPort);
            }
        } else {
            skipped << QStringLiteral("UDP primary disabled");
        }

        bool const secondaryEnabled =
            getSetting(QStringLiteral("UDPSecondaryEnabled"), true).toBool();
        bool const secondaryQsoEnabled =
            udpTrafficEnabled(QStringLiteral("UDPSecondary"), QStringLiteral("Qso"));
        if (secondaryEnabled && secondaryQsoEnabled) {
            QString const secondaryServer =
                getSetting(QStringLiteral("UDPSecondaryServer"), primaryServer).toString().trimmed();
            quint16 const secondaryPort = udpPortFromSettingValue(
                getSetting(QStringLiteral("UDPSecondaryServerPort"), 2239), 2239);
            ++udpAttempts;
            if (udpSendRawAdifDatagram(QStringLiteral("FT2-Link UDP secondary raw ADIF"),
                                       secondaryServer, secondaryPort, cleanCall, adifBytes)) {
                ++udpDelivered;
                attempted << QStringLiteral("UDP secondary %1:%2").arg(secondaryServer).arg(secondaryPort);
            }
        } else {
            skipped << QStringLiteral("UDP secondary disabled");
        }

        bool const tertiaryEnabled =
            getSetting(QStringLiteral("UDPTertiaryEnabled"), false).toBool();
        bool const tertiaryQsoEnabled =
            udpTrafficEnabled(QStringLiteral("UDPTertiary"), QStringLiteral("Qso"));
        if (tertiaryEnabled && tertiaryQsoEnabled) {
            QString const tertiaryServer =
                getSetting(QStringLiteral("UDPTertiaryServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
            quint16 const tertiaryPort = udpPortFromSettingValue(
                getSetting(QStringLiteral("UDPTertiaryServerPort"), 2237), 2237);
            ++udpAttempts;
            if (udpSendRawAdifDatagram(QStringLiteral("FT2-Link UDP tertiary raw ADIF"),
                                       tertiaryServer, tertiaryPort, cleanCall, adifBytes)) {
                ++udpDelivered;
                attempted << QStringLiteral("UDP tertiary %1:%2").arg(tertiaryServer).arg(tertiaryPort);
            }
        } else {
            skipped << QStringLiteral("UDP tertiary disabled");
        }

        if (udpAttempts > 0 && udpDelivered == 0) {
            skipped << QStringLiteral("UDP raw ADIF targets failed");
        }
    }

    if (wants(QStringLiteral("N1MM"))) {
        if (getSetting(QStringLiteral("BroadcastToN1MM"), false).toBool()) {
            udpSendN1mmLoggedQso(cleanCall, adifBytes);
            attempted << QStringLiteral("N1MM");
        } else {
            skipped << QStringLiteral("N1MM disabled");
        }
    }

    if (wants(QStringLiteral("TCP"))) {
        if (getSetting(QStringLiteral("ADIFTcpEnabled"), false).toBool()) {
            tcpSendLoggedAdifQso(cleanCall, adifBytes);
            attempted << QStringLiteral("ADIF TCP");
        } else {
            skipped << QStringLiteral("ADIF TCP disabled");
        }
    }

    if (wants(QStringLiteral("QRZ"))) {
        if (m_qrzLogbookEnabled && m_qrzLogbook) {
            m_qrzLogbook->setEnabled(m_qrzLogbookEnabled);
            m_qrzLogbook->setApiKey(m_qrzLogbookApiKey);
            m_qrzLogbook->setReplaceDuplicates(m_qrzLogbookReplaceDuplicates);
            m_qrzLogbook->uploadAdif(cleanCall, adifBytes, uploadId);
            attempted << QStringLiteral("QRZ Logbook");
            if (uploadId != 0) {
                pendingAsync << QStringLiteral("QRZ Logbook");
            }
        } else {
            skipped << QStringLiteral("QRZ Logbook disabled");
        }
    }

    if (wants(QStringLiteral("CLOUDLOG"))) {
        if (m_cloudlogEnabled && m_cloudlog) {
            m_cloudlog->setEnabled(m_cloudlogEnabled);
            m_cloudlog->setApiUrl(m_cloudlogUrl);
            m_cloudlog->setApiKey(m_cloudlogApiKey);
            m_cloudlog->setStationId(qBound(0, getSetting(QStringLiteral("CloudLogStationID"), 1).toInt(), 999));
            m_cloudlog->uploadAdif(cleanCall, adifBytes, uploadId);
            attempted << QStringLiteral("Cloudlog");
            if (uploadId != 0) {
                pendingAsync << QStringLiteral("Cloudlog");
            }
        } else {
            skipped << QStringLiteral("Cloudlog disabled");
        }
    }

    QStringList immediateTargets = attempted;
    for (QString const& backend : pendingAsync) {
        immediateTargets.removeAll(backend);
    }

    bool const ok = !attempted.isEmpty();
    QString const initialDetail =
        ok ? attempted.join(QStringLiteral(", "))
           : (skipped.isEmpty()
              ? QStringLiteral("No configured external logbook target")
              : skipped.join(QStringLiteral(", ")));

    if (uploadId != 0 && ok) {
        ExternalAdifUploadPending pending;
        pending.dxCall = cleanCall;
        pending.target = cleanTarget;
        pending.pendingBackends = pendingAsync;
        pending.skippedTargets = skipped;
        pending.immediateTargets = immediateTargets;
        if (!pending.pendingBackends.isEmpty()) {
            m_externalAdifUploads.insert(uploadId, pending);
        }
    }

    result.insert(QStringLiteral("ok"), ok);
    result.insert(QStringLiteral("attempted"), attempted);
    result.insert(QStringLiteral("skipped"), skipped);
    result.insert(QStringLiteral("pending"), pendingAsync);
    result.insert(QStringLiteral("immediate"), immediateTargets);
    result.insert(QStringLiteral("state"),
                  ok ? QStringLiteral("Submitted") : QStringLiteral("Failed"));
    result.insert(QStringLiteral("detail"), initialDetail);
    if (!ok) {
        result.insert(QStringLiteral("error"), result.value(QStringLiteral("detail")).toString());
    } else if (uploadId != 0 && pendingAsync.isEmpty()) {
        QTimer::singleShot(0, this, [this, uploadId, initialDetail]() {
            emit externalAdifUploadStatus(uploadId, QStringLiteral("Sent"), initialDetail);
        });
    }

    decodiumBridgeExternalAdifLog(QStringLiteral("FT2-Link external ADIF upload: call=%1 target=%2 ok=%3 attempted=%4 skipped=%5 bytes=%6")
                  .arg(cleanCall, cleanTarget, ok ? QStringLiteral("true") : QStringLiteral("false"),
                       attempted.join(QStringLiteral("|")),
                       skipped.join(QStringLiteral("|")))
                  .arg(adifBytes.size()));
    if (ok) {
        emit statusMessage(QStringLiteral("FT2-Link QSO %1 -> %2")
                           .arg(cleanCall, attempted.join(QStringLiteral(", "))));
    }
    return result;
}

void DecodiumBridge::noteExternalAdifUploadBackend(quint32 uploadId,
                                                   const QString& backend,
                                                   const QString& dxCall,
                                                   bool ok,
                                                   const QString& detail)
{
    if (uploadId == 0) {
        return;
    }

    auto it = m_externalAdifUploads.find(uploadId);
    if (it == m_externalAdifUploads.end()) {
        decodiumBridgeExternalAdifLog(QStringLiteral("FT2-Link external ADIF late status ignored: id=%1 backend=%2 call=%3 ok=%4 detail=%5")
                      .arg(uploadId)
                      .arg(backend)
                      .arg(dxCall)
                      .arg(ok ? QStringLiteral("true") : QStringLiteral("false"))
                      .arg(detail.simplified()));
        return;
    }

    ExternalAdifUploadPending& pending = it.value();
    pending.pendingBackends.removeAll(backend);
    QString const backendDetail = detail.simplified();
    QString const item = backendDetail.isEmpty()
        ? backend
        : QStringLiteral("%1: %2").arg(backend, backendDetail);
    if (ok) {
        pending.sentBackends << item;
    } else {
        pending.failedBackends << item;
    }

    decodiumBridgeExternalAdifLog(QStringLiteral("FT2-Link external ADIF backend status: id=%1 backend=%2 call=%3 ok=%4 pending=%5")
                  .arg(uploadId)
                  .arg(backend)
                  .arg(dxCall)
                  .arg(ok ? QStringLiteral("true") : QStringLiteral("false"))
                  .arg(pending.pendingBackends.join(QStringLiteral("|"))));
    finishExternalAdifUploadIfReady(uploadId);
}

void DecodiumBridge::finishExternalAdifUploadIfReady(quint32 uploadId)
{
    auto it = m_externalAdifUploads.find(uploadId);
    if (it == m_externalAdifUploads.end()) {
        return;
    }
    if (!it.value().pendingBackends.isEmpty()) {
        return;
    }

    ExternalAdifUploadPending const pending = it.value();
    m_externalAdifUploads.erase(it);

    QStringList detailParts;
    if (!pending.immediateTargets.isEmpty()) {
        detailParts << QStringLiteral("submitted: %1")
                           .arg(pending.immediateTargets.join(QStringLiteral(", ")));
    }
    if (!pending.sentBackends.isEmpty()) {
        detailParts << QStringLiteral("confirmed: %1")
                           .arg(pending.sentBackends.join(QStringLiteral("; ")));
    }
    if (!pending.failedBackends.isEmpty()) {
        detailParts << QStringLiteral("failed: %1")
                           .arg(pending.failedBackends.join(QStringLiteral("; ")));
    }
    if (!pending.skippedTargets.isEmpty()) {
        detailParts << QStringLiteral("skipped: %1")
                           .arg(pending.skippedTargets.join(QStringLiteral(", ")));
    }

    QString const state = pending.failedBackends.isEmpty()
        ? QStringLiteral("Sent")
        : QStringLiteral("Failed");
    QString detail = detailParts.join(QStringLiteral(" | ")).simplified();
    if (detail.isEmpty()) {
        detail = state == QStringLiteral("Sent")
            ? QStringLiteral("External logbook upload confirmed")
            : QStringLiteral("External logbook upload failed");
    }

    decodiumBridgeExternalAdifLog(QStringLiteral("FT2-Link external ADIF final status: id=%1 call=%2 target=%3 state=%4 detail=%5")
                  .arg(uploadId)
                  .arg(pending.dxCall)
                  .arg(pending.target)
                  .arg(state)
                  .arg(detail));
    emit externalAdifUploadStatus(uploadId, state, detail);
    if (state == QStringLiteral("Sent")) {
        emit statusMessage(QStringLiteral("FT2-Link QSO %1 upload confermato").arg(pending.dxCall));
    } else {
        emit errorMessage(QStringLiteral("FT2-Link QSO %1 upload fallito: %2")
                          .arg(pending.dxCall, detail));
    }
}

