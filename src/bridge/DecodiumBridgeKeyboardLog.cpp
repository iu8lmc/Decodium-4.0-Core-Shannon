#include "DecodiumBridge.h"

void decodiumBridgeDecodeFlowLog(const QString&);
bool decodiumBridgeIsHamlibFamilyBackend(QString const&);

bool DecodiumBridge::registraQsoCw(const QString& nominativo, const QString& rstInviato,
                                   const QString& rstRicevuto, const QString& nome,
                                   const QString& qth, const QString& locatore)
{
    m_logModeOverride = QStringLiteral("CW");
    const bool ok = registraQsoRtty(nominativo, rstInviato, rstRicevuto, nome, qth, locatore);
    if (!ok)
        m_logModeOverride.clear();
    return ok;
}

bool DecodiumBridge::registraQsoRtty(const QString& nominativo,
                                    const QString& rstInviato,
                                    const QString& rstRicevuto,
                                    const QString& nome,
                                    const QString& qth,
                                    const QString& locatore)
{
    QString const call = nominativo.trimmed().toUpper();
    if (call.isEmpty()) {
        decodiumBridgeDecodeFlowLog(QStringLiteral("RTTY: niente da registrare, manca il nominativo"));
        return false;
    }

    // I campi del QSO sono quelli dell'applicazione: si compilano e si lascia
    // fare al percorso normale. Il modo lo mette logQso() leggendo m_mode, che
    // in RTTY vale "RTTY": nell'ADIF finisce MODE=RTTY senza doverlo dire qui.
    setDxCall(call);
    if (!locatore.trimmed().isEmpty())
        setDxGrid(locatore.trimmed().toUpper());
    setReportSent(rstInviato.trimmed().isEmpty() ? QStringLiteral("599") : rstInviato.trimmed());
    setReportReceived(rstRicevuto.trimmed().isEmpty() ? QStringLiteral("599") : rstRicevuto.trimmed());

    // Nome e QTH non hanno un campo proprio nel percorso di log: vanno nel
    // commento, che e' dove l'operatore li ritrova rileggendo il collegamento.
    QStringList note;
    if (!nome.trimmed().isEmpty())
        note << nome.trimmed();
    if (!qth.trimmed().isEmpty())
        note << qth.trimmed();
    if (!note.isEmpty()) {
        m_promptLogComment = note.join(QStringLiteral(", "));
        m_promptLogCommentOverrideValid = true;
    }

    // Il fermo anti-doppio vale per il QSO che il sequencer ha appena chiuso:
    // questo e' un collegamento diverso, fatto a mano in RTTY, e senza
    // azzerarlo il primo registrato sarebbe anche l'ultimo.
    m_qsoLogged = false;

    decodiumBridgeDecodeFlowLog(QStringLiteral("%1: registro il collegamento con %2 (%3/%4)")
                  .arg(m_logModeOverride.isEmpty() ? QStringLiteral("RTTY") : m_logModeOverride,
                       call, rstInviato, rstRicevuto));
    logQso();
    return true;
}


void DecodiumBridge::impostaModoRadioRtty(const QString& modo)
{
    QString const scelto = modo.trimmed().toUpper();
    if (scelto.isEmpty())
        return;

    // Si ricorda per prima cosa: applyRttyRigMode legge di qui, e vogliamo che
    // legga il nuovo valore anche quando lo richiamera' dopo il prossimo QSY.
    setSetting(QStringLiteral("RttyRigMode"), scelto);
    decodiumBridgeDecodeFlowLog(QStringLiteral("RTTY: modo radio scelto dall'operatore: %1").arg(scelto));

    // In RTTY lo applica il percorso dell'RTTY, che sa anche rimandare durante
    // l'assestamento di un QSY. Fuori da RTTY si comanda e basta: la scelta
    // resta comunque salvata per quando ci si tornera'.
    if (m_mode.compare(QStringLiteral("RTTY"), Qt::CaseInsensitive) == 0) {
        applyRttyRigMode(QStringLiteral("scelta-operatore"));
        return;
    }
    if (m_catBackend == QStringLiteral("cat4om") && m_cat4OmCat && m_cat4OmCat->connected())
        m_cat4OmCat->setRigMode(scelto);
    else if (decodiumBridgeIsHamlibFamilyBackend(m_catBackend) && m_hamlibCat && m_hamlibCat->connected())
        m_hamlibCat->setRigMode(scelto);
    else if (m_catBackend == QStringLiteral("omnirig") && m_omniRigCat && m_omniRigCat->connected())
        m_omniRigCat->setRigMode(scelto);
}

