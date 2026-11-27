#include "DecodiumBridge.h"
#include "DecoPortRigDriver.h"
#include "DecodiumDecoPortGateway.h"

void decodiumBridgeDecoPortGatewayLog(const QString& message);

QVariantMap DecodiumBridge::decoPortRigDriverState() const
{
    QVariantMap out;
    bool const open = m_decoPortRig && m_decoPortRig->isOpen();
    out.insert(QStringLiteral("open"), open);
    out.insert(QStringLiteral("rigName"), open ? m_decoPortRig->rigName() : QString());
    out.insert(QStringLiteral("frequencyHz"), open ? m_decoPortRig->frequencyHz() : 0.0);
    out.insert(QStringLiteral("modeName"), open ? m_decoPortRig->modeName() : QString());
    out.insert(QStringLiteral("error"), m_decoPortRig ? m_decoPortRig->lastError() : QString());
    return out;
}

bool DecodiumBridge::startDecoPortGateway(int port)
{
    auto* gw = qobject_cast<DecodiumDecoPortGateway*>(decoPortGatewayObject());
    if (!gw)
        return false;
    decoPortEnsureRigDriver();
    bool const ok = gw->start(port);
    decodiumBridgeDecoPortGatewayLog(QStringLiteral("DecoPort gateway start on %1: %2")
                  .arg(port).arg(ok ? QStringLiteral("ok") : gw->status()));
    return ok;
}

void DecodiumBridge::stopDecoPortGateway()
{
    if (m_decoPortGateway) {
        m_decoPortGateway->stop();
        decodiumBridgeDecoPortGatewayLog(QStringLiteral("DecoPort gateway stopped"));
    }
}

