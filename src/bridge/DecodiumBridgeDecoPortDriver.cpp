#include "DecodiumBridge.h"
#include "DecoPortRigDriver.h"
#include "DecodiumRigDetector.h"

void decodiumBridgeAudioLevelLog(const QString&);
#define bridgeLog decodiumBridgeAudioLevelLog

void DecodiumBridge::decoPortEnsureRigDriver()
{
    if (m_decoPortRig && m_decoPortRig->isOpen())
        return;

    QVariantList const candidates = DecodiumRigDetector::detect();
    if (candidates.isEmpty()) {
        bridgeLog(QStringLiteral("DecoPort rig driver: nothing detected"));
        return;
    }

    QVariantMap const best = candidates.first().toMap();
    QString const port  = best.value(QStringLiteral("catPort")).toString().trimmed();
    QString const label = best.value(QStringLiteral("rigLabel")).toString();
    QString const token = best.value(QStringLiteral("rigToken")).toString();
    int const baud      = best.value(QStringLiteral("baudRate")).toInt();
    int const civ       = best.value(QStringLiteral("civAddress")).toInt();

    if (port.isEmpty()) {
        bridgeLog(QStringLiteral("DecoPort rig driver: no CAT port for [%1]").arg(label));
        return;
    }

    // Il modello si risolve sempre, anche quando poi non si aprira' niente:
    // sapere che radio DecoPort ha riconosciuto e' meta' della diagnosi, e
    // cercarla nel catalogo non tocca il bus.
    QString matched;
    int const model = DecoPortRigDriver::resolveModel(label, token, &matched);
    if (model == 0) {
        bridgeLog(QStringLiteral("DecoPort rig driver: no Hamlib model matches [%1] token=[%2]")
                      .arg(label, token));
    } else {
        bridgeLog(QStringLiteral("DecoPort rig driver: [%1] resolved to Hamlib model %2 (%3)")
                      .arg(label).arg(model).arg(matched));
    }

    QString const wanted = port.toUpper();
    bool const clash =
        m_hamlibCat && (wanted == m_hamlibCat->serialPort().trimmed().toUpper()
                        || wanted == m_hamlibCat->pttPort().trimmed().toUpper());
    if (clash) {
        bridgeLog(QStringLiteral("DecoPort rig driver: %1 is already the application's CAT port; "
                                 "staying on the application hooks").arg(port));
        return;
    }

    if (model == 0)
        return;

    // Si apre da soli solo cio' che il rilevatore riconosce come radio, per
    // modello o firma USB. Un adattatore generico, o la porta di un altro
    // apparato, risolto per caso a un modello dal confronto largo sul
    // catalogo, riceverebbe comandi CAT che non capisce: li' si resta sui
    // ganci. Le porte virtuali (com0com) il rilevatore non le propone affatto.
    if (!best.value(QStringLiteral("isRadio")).toBool()) {
        bridgeLog(QStringLiteral("DecoPort rig driver: [%1] on %2 is not recognised as a radio; "
                                 "staying on the application hooks").arg(label, port));
        return;
    }

    if (!m_decoPortRig) {
        m_decoPortRig = new DecoPortRigDriver;
        connect(m_decoPortRig, &DecoPortRigDriver::opened, this, [](const QString& name) {
            bridgeLog(QStringLiteral("DecoPort rig driver: %1 open, talking to the radio directly")
                          .arg(name));
        });
        connect(m_decoPortRig, &DecoPortRigDriver::failed, this, [](const QString& why) {
            bridgeLog(QStringLiteral("DecoPort rig driver: %1").arg(why));
        });
    }
    bridgeLog(QStringLiteral("DecoPort rig driver: opening %1 at %2 baud as Hamlib model %3 (%4)")
                  .arg(port).arg(baud).arg(model).arg(matched));
    m_decoPortRig->open(port, baud, civ, model);
}

#undef bridgeLog
