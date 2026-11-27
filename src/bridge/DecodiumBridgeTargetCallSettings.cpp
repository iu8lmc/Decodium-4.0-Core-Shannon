#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include <QSettings>
#include <QDateTime>
#include <QTimer>
#include <QRegularExpression>
#include "Sequencer/MessageTokenRules.hpp"

using decodium::seq::normalizedBaseCall;
using decodium::seq::normalizedUsableCallToken;

extern void decodiumBridgeLogbookLog(const QString&);

void DecodiumBridge::setTargetCallSign(const QString& v)
{
    QString cleaned = v.trimmed().toUpper();
    if (m_targetCallSign != cleaned) {
        m_targetCallSign = cleaned;
        emit targetCallSignChanged();
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(settings);
        settings.setValue(QStringLiteral("CallFeature/TargetCallSign"), m_targetCallSign);
    }
}

void DecodiumBridge::setTargetCallMaxRetries(int v)
{
    v = qBound(0, v, 999);
    if (m_targetCallMaxRetries != v) {
        m_targetCallMaxRetries = v;
        emit targetCallMaxRetriesChanged();
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(settings);
        settings.setValue(QStringLiteral("CallFeature/MaxRetries"), v);
    }
}

void DecodiumBridge::setTargetCallTimeoutS(int v)
{
    v = qBound(10, v, 600);
    if (m_targetCallTimeoutS != v) {
        m_targetCallTimeoutS = v;
        emit targetCallTimeoutSChanged();
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(settings);
        settings.setValue(QStringLiteral("CallFeature/TimeoutS"), v);
    }
}

void DecodiumBridge::setTargetCallPeriod(int v)
{
    v = qBound(0, v, 2);
    if (m_targetCallPeriod != v) {
        m_targetCallPeriod = v;
        emit targetCallPeriodChanged();
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(settings);
        settings.setValue(QStringLiteral("CallFeature/Period"), v);
    }
}

void DecodiumBridge::setTargetCallPauseS(int v)
{
    v = qBound(0, v, 300);
    if (m_targetCallPauseS != v) {
        m_targetCallPauseS = v;
        emit targetCallPauseSChanged();
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(settings);
        settings.setValue(QStringLiteral("CallFeature/PauseS"), v);
    }
}

void DecodiumBridge::setArmedWatchEnabled(bool v)
{
    if (m_armedWatchEnabled != v) {
        m_armedWatchEnabled = v;
        emit armedWatchEnabledChanged();
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(settings);
        settings.setValue(QStringLiteral("CallFeature/Armed"), v);
        decodiumBridgeLogbookLog(QStringLiteral("[CALL] DX-watch armato %1").arg(v ? "ON" : "OFF"));
    }
}

void DecodiumBridge::setArmedReArm(bool v)
{
    if (m_armedReArm != v) {
        m_armedReArm = v;
        emit armedReArmChanged();
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(settings);
        settings.setValue(QStringLiteral("CallFeature/ReArm"), v);
        decodiumBridgeLogbookLog(QStringLiteral("[CALL] re-arm %1").arg(v ? "ON" : "OFF"));
    }
}


void DecodiumBridge::startTargetCall()
{
    if (m_targetCallSign.isEmpty()) {
        decodiumBridgeLogbookLog(QStringLiteral("startTargetCall: empty callsign, ignored"));
        return;
    }
    if (m_autoCallEnabled) {
        decodiumBridgeLogbookLog(QStringLiteral("startTargetCall: rejected while Auto Call is active"));
        emit statusMessage(QStringLiteral("Stop Auto Call before starting Direct Call"));
        return;
    }
    if (m_targetCallActive) {
        decodiumBridgeLogbookLog(QStringLiteral("startTargetCall: already active for %1").arg(m_targetCallSign));
        return;
    }
    // Salva stato pre-attivazione per ripristino on stop
    m_targetCallSavedDxCall    = m_dxCall;
    m_targetCallSavedTxPeriod  = m_txPeriod;
    m_targetCallSavedAlt12     = m_alt12Enabled;

    ++m_targetCallSessionId;
    m_targetCallRetryCount      = 0;
    m_targetCallStartedUtc      = QDateTime::currentDateTimeUtc();
    m_targetCallLastTxUtc       = QDateTime();
    m_targetCallLastSeenUtc     = QDateTime();
    m_targetCallSeenSinceLastTx = false;
    m_targetCallWasTransmitting = m_transmitting;
    m_targetCallActive          = true;
    emit targetCallActiveChanged();
    emit targetCallRetryCountChanged();

    // CALL UI convention: 0=1st (:00/:30), 1=2nd (:15/:45), 2=alterna.
    // Bridge txPeriod convention is reversed: 1=first/even, 0=second/odd.
    if (m_targetCallPeriod == 0) {
        setAlt12Enabled(false);
        setTxPeriod(1);
    } else if (m_targetCallPeriod == 1) {
        setAlt12Enabled(false);
        setTxPeriod(0);
    } else {
        setAlt12Enabled(true);
    }

    // Imposta dxCall + autoSeq. Il TX (e il timeout) parte ORA se non armato,
    // oppure da beginTargetCallCalling() quando il target compare se DX-watch armato.
    setDxCall(m_targetCallSign);
    if (!m_autoSeq) setAutoSeq(true);

    // Connect edge detector per contare i TX senza rivedere il target.
    connect(this, &DecodiumBridge::transmittingChanged,
            this, &DecodiumBridge::onTargetCallTransmittingChanged,
            Qt::UniqueConnection);

    m_armedReArmCount = 0;
    if (m_armedWatchEnabled) {
        // DX-watch ARMED: NON trasmette. Monitora i decode; beginTargetCallCalling()
        // viene chiamata da tryArmedWatchSeesTarget() quando il target compare on-air.
        // Nessun timeout in ARMED: non occupa il canale -> attesa sicura, l'operatore
        // disarma con Stop. Il TX watchdog globale resta comunque sopra in CALLING.
        m_targetCallArmedWaiting = true;
        emit targetCallArmedWaitingChanged();
        decodiumBridgeLogbookLog(QStringLiteral("[CALL] ARMED su %1: in ascolto, chiamo quando decodificato (reArm=%2)")
                      .arg(m_targetCallSign).arg(m_armedReArm ? "ON" : "OFF"));
    } else {
        m_targetCallArmedWaiting = false;
        beginTargetCallCalling();
    }

    QString const periodLabel = (m_targetCallPeriod == 0)
        ? QStringLiteral("1st")
        : (m_targetCallPeriod == 1)
            ? QStringLiteral("2nd")
            : QStringLiteral("alternate");
    decodiumBridgeLogbookLog(QStringLiteral("startTargetCall: target=%1 maxMissedTargetTx=%2 timeoutS=%3 period=%4 txPeriod=%5 pauseS=%6")
                  .arg(m_targetCallSign)
                  .arg(m_targetCallMaxRetries == 0 ? QStringLiteral("inf") : QString::number(m_targetCallMaxRetries))
                  .arg(m_targetCallTimeoutS)
                  .arg(periodLabel)
                  .arg(m_alt12Enabled ? QStringLiteral("alt") : QString::number(m_txPeriod))
                  .arg(m_targetCallPauseS));
}

void DecodiumBridge::stopTargetCall()
{
    if (!m_targetCallActive) return;

    disconnect(this, &DecodiumBridge::transmittingChanged,
               this, &DecodiumBridge::onTargetCallTransmittingChanged);

    ++m_targetCallSessionId;
    m_targetCallActive = false;
    m_targetCallSeenSinceLastTx = false;
    m_targetCallLastSeenUtc = QDateTime();
    emit targetCallActiveChanged();

    // Ripristina stato pre-attivazione SOLO se non siamo entrati in un QSO completato
    // (se qsoLogged=true il QSO si e' concluso bene, lasciamo Decodium come e' adesso)
    if (!m_qsoLogged) {
        setTxEnabled(false);
        setDxCall(m_targetCallSavedDxCall);
    }
    setAlt12Enabled(m_targetCallSavedAlt12);
    if (!m_alt12Enabled) {
        setTxPeriod(m_targetCallSavedTxPeriod);
    }

    decodiumBridgeLogbookLog(QStringLiteral("stopTargetCall: missedTargetTx=%1 qsoLogged=%2")
                  .arg(m_targetCallRetryCount)
                  .arg(m_qsoLogged ? "true" : "false"));
}

// ARMED -> CALLING: abilita il TX e arma il timeout di chiamata. Chiamata sia dal
// path non-armato (startTargetCall) sia quando il target compare on-air in ARMED
// (tryArmedWatchSeesTarget). Bumpa il sessionId per invalidare i timer della
// sessione di chiamata precedente (re-arm).
void DecodiumBridge::beginTargetCallCalling()
{
    if (!m_targetCallActive) return;
    ++m_targetCallSessionId;
    m_targetCallArmedWaiting = false;
    emit targetCallArmedWaitingChanged();
    m_targetCallStartedUtc = QDateTime::currentDateTimeUtc();  // anchor timeout CALLING
    m_targetCallRetryCount = 0;
    m_targetCallSeenSinceLastTx = true; // il primo TX dopo l'aggancio non e' una chiamata a vuoto
    m_targetCallLastSeenUtc = QDateTime::currentDateTimeUtc();
    emit targetCallRetryCountChanged();
    armExplicitManualTxRequest(m_currentTx, QStringLiteral("target call"));
    if (!m_txEnabled) setTxEnabled(true);

    quint64 const sessionId = m_targetCallSessionId;
    if (m_targetCallTimeoutS > 0) {
        QTimer::singleShot(m_targetCallTimeoutS * 1000, this, [this, sessionId]() {
            if (!m_targetCallActive || m_targetCallSessionId != sessionId || m_targetCallArmedWaiting) {
                return;
            }
            bool const qsoAlreadyProgressed = m_qsoProgress > 2 && m_currentTx >= 2 && m_currentTx <= 5;
            if (qsoAlreadyProgressed) {
                decodiumBridgeLogbookLog(QStringLiteral("targetCall timeout ignored: QSO already progressed tx=%1 progress=%2")
                              .arg(m_currentTx)
                              .arg(m_qsoProgress));
                return;
            }
            if (m_transmitting) {
                decodiumBridgeLogbookLog(QStringLiteral("targetCall timeout reached during TX; stop deferred to TX end"));
                return;
            }
            if (txWatchdogLimitConfigured()) {
                decodiumBridgeLogbookLog(QStringLiteral("targetCall: timeout reached (%1s), TX watchdog priority active -> continue")
                              .arg(m_targetCallTimeoutS));
                return;
            }
            decodiumBridgeLogbookLog(QStringLiteral("targetCall: timeout reached (%1s)")
                          .arg(m_targetCallTimeoutS));
            endTargetCallNoQso(QStringLiteral("timeout"));
        });
    }
}

// Fine di un tentativo di chiamata SENZA QSO (max retry / timeout). Con DX-watch
// armato + re-arm: torna in ascolto (ARMED) fino a un cap; oltre il cap fa Halt
// manuale (intervento operatore obbligatorio -> niente loop infinito stile WSJT-Z).
// Senza re-arm (default one-shot) o QSO gia' loggato: stop normale.
void DecodiumBridge::endTargetCallNoQso(const QString& reason)
{
    if (!m_targetCallActive) return;
    constexpr int kArmedReArmCap = 3;
    if (m_armedWatchEnabled && m_armedReArm && !m_qsoLogged) {
        if (m_armedReArmCount < kArmedReArmCap) {
            ++m_armedReArmCount;
            setTxEnabled(false);
            m_targetCallRetryCount = 0;
            m_targetCallSeenSinceLastTx = false;
            m_targetCallLastSeenUtc = QDateTime();
            emit targetCallRetryCountChanged();
            m_targetCallArmedWaiting = true;
            emit targetCallArmedWaitingChanged();
            decodiumBridgeLogbookLog(QStringLiteral("[CALL] %1 -> re-arm %2/%3: torno in ascolto su %4")
                          .arg(reason)
                          .arg(m_armedReArmCount)
                          .arg(kArmedReArmCap)
                          .arg(m_targetCallSign));
            return;
        }
        decodiumBridgeLogbookLog(QStringLiteral("[CALL] %1 -> re-arm cap %2 raggiunto: Halt manuale (decisione operatore)")
                      .arg(reason)
                      .arg(kArmedReArmCap));
        engageManualTxHold(QStringLiteral("armed reArm cap"), true);
        stopTargetCall();
        return;
    }
    decodiumBridgeLogbookLog(QStringLiteral("[CALL] %1 -> stop").arg(reason));
    stopTargetCall();
}

bool DecodiumBridge::targetCallRowsContainTarget(const QStringList& rows, QString* matchedMessage) const
{
    if (!m_targetCallActive || m_targetCallSign.isEmpty())
        return false;

    QString const targetBase = normalizedBaseCall(m_targetCallSign.trimmed().toUpper());
    if (targetBase.isEmpty()) return false;

    for (QString const& row : rows) {
        QStringList f = parseFt8Row(row);
        if (f.size() < 5) continue;
        QString const msg = f[4];
        if (!shouldAcceptDecodedMessage(msg)) continue;

        QStringList const parts = msg.trimmed().split(QRegularExpression(QStringLiteral("\\s+")),
                                                      Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;

        // Mittente: in "CQ <call> ..." e' il primo call dopo CQ (o parts[2]
        // con modificatore tipo "CQ DX <call>"); in "<to> <from> ..." e' parts[1].
        bool senderIsTarget = false;
        if (parts.at(0).compare(QStringLiteral("CQ"), Qt::CaseInsensitive) == 0) {
            QString const c1 = normalizedBaseCall(normalizedUsableCallToken(parts.at(1)));
            QString const c2 = parts.size() >= 3
                ? normalizedBaseCall(normalizedUsableCallToken(parts.at(2)))
                : QString();
            senderIsTarget = (c1 == targetBase) || (c2 == targetBase);
        } else {
            QString const fromBase = normalizedBaseCall(normalizedUsableCallToken(parts.at(1)));
            senderIsTarget = (fromBase == targetBase);
        }

        if (!senderIsTarget) continue;
        if (shouldSuppressDirectedGhostDecode(f, QStringLiteral("target-call-watch")))
            continue;

        if (matchedMessage) *matchedMessage = msg;
        return true;
    }

    return false;
}

void DecodiumBridge::observeTargetCallDecodeRows(const QStringList& rows, const QString& context)
{
    if (!m_targetCallActive || m_targetCallArmedWaiting || rows.isEmpty())
        return;

    QString matchedMessage;
    if (!targetCallRowsContainTarget(rows, &matchedMessage))
        return;

    m_targetCallSeenSinceLastTx = true;
    m_targetCallLastSeenUtc = QDateTime::currentDateTimeUtc();
    if (m_targetCallRetryCount != 0) {
        m_targetCallRetryCount = 0;
        emit targetCallRetryCountChanged();
        decodiumBridgeLogbookLog(QStringLiteral("targetCall: target seen again (%1), missedTargetTx reset context=%2 msg='%3'")
                      .arg(m_targetCallSign, context, matchedMessage));
    }
}

// DX-watch armato: in ARMED scatta quando il target compare come MITTENTE in un
// decode (CQ dal target, o target che lavora qualcuno). Clona il match di
// tryResumeQsoOnReply ma sul mittente (non sulla risposta a noi). Inerte se non
// armato/non in ascolto. Ritorna true se ha avviato la chiamata.
bool DecodiumBridge::tryArmedWatchSeesTarget(const QStringList& rows)
{
    if (!m_targetCallActive || !m_targetCallArmedWaiting || m_targetCallSign.isEmpty())
        return false;
    QString matchedMessage;
    if (!targetCallRowsContainTarget(rows, &matchedMessage))
        return false;

    QString const targetBase = normalizedBaseCall(m_targetCallSign.trimmed().toUpper());
    for (QString const& row : rows) {
        QStringList f = parseFt8Row(row);
        if (f.size() < 5) continue;
        QString const msg = f[4];
        if (msg != matchedMessage) continue;
        QStringList const parts = msg.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;
        decodiumBridgeLogbookLog(QStringLiteral("[CALL] ARMED: target %1 decodificato (msg='%2') -> inizio chiamata")
                      .arg(targetBase, msg));
        m_targetCallSeenSinceLastTx = true;
        m_targetCallLastSeenUtc = QDateTime::currentDateTimeUtc();
        // 1.0.412: ingaggia DAVVERO la chiamata, come il gemello tryResumeQsoOnReply:
        // processDecodeDoubleClick genera TX1-TX6 verso il target, porta m_currentTx a TX1
        // (advanceQsoState) e regola il periodo. Senza questo, beginTargetCallCalling
        // abilitava solo il TX ma buildCurrentTxMessage restava vuoto (m_currentTx mai
        // a TX1) -> il DX-watch si armava ma non chiamava mai.
        processDecodeDoubleClick(msg, f[0], f[1], f[3].toInt());
        beginTargetCallCalling();
        return true;
    }
    return false;
}

void DecodiumBridge::onTargetCallTransmittingChanged()
{
    if (!m_targetCallActive) return;
    bool const nowTx = m_transmitting;
    if (m_targetCallWasTransmitting && !nowTx) {
        // Edge: TX appena terminato -> tick
        tickTargetCallOnTx();
    }
    m_targetCallWasTransmitting = nowTx;
}

void DecodiumBridge::tickTargetCallOnTx()
{
    if (!m_targetCallActive) return;
    // Conta solo retry verso il target. Se siamo entrati in un QSO con risposta
    // del partner, m_qsoLogged si attivera' a fine RR73 / 73 e stopTargetCall()
    // verra' chiamata. Qui contiamo i tentativi TX1 ripetuti senza risposta.
    if (m_dxCall.compare(m_targetCallSign, Qt::CaseInsensitive) != 0) {
        // dxCall e' cambiato (es. partner ha risposto e siamo in mid-QSO) -> non contare retry
        return;
    }
    bool const qsoAlreadyProgressed = m_qsoProgress > 2 && m_currentTx >= 2 && m_currentTx <= 5;
    if (qsoAlreadyProgressed) {
        decodiumBridgeLogbookLog(QStringLiteral("targetCall tx-end: QSO progressed to TX%1, retry timing not applied")
                      .arg(m_currentTx));
        return;
    }

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    m_targetCallLastTxUtc = nowUtc;
    qint64 const elapsedS = m_targetCallStartedUtc.isValid()
        ? qMax<qint64>(0, m_targetCallStartedUtc.secsTo(nowUtc))
        : 0;

    if (m_targetCallSeenSinceLastTx) {
        m_targetCallSeenSinceLastTx = false;
        if (m_targetCallRetryCount != 0) {
            m_targetCallRetryCount = 0;
            emit targetCallRetryCountChanged();
        }
        decodiumBridgeLogbookLog(QStringLiteral("targetCall tx-end: target heard, missedTargetTx=0/%1 dxCall=%2 elapsed=%3/%4s pause=%5s")
                      .arg(m_targetCallMaxRetries == 0 ? QStringLiteral("inf") : QString::number(m_targetCallMaxRetries))
                      .arg(m_dxCall)
                      .arg(elapsedS)
                      .arg(m_targetCallTimeoutS)
                      .arg(m_targetCallPauseS));
    } else {
        ++m_targetCallRetryCount;
        emit targetCallRetryCountChanged();
        decodiumBridgeLogbookLog(QStringLiteral("targetCall tx-end: missedTargetTx=%1/%2 dxCall=%3 elapsed=%4/%5s pause=%6s")
                      .arg(m_targetCallRetryCount)
                      .arg(m_targetCallMaxRetries == 0 ? QStringLiteral("inf") : QString::number(m_targetCallMaxRetries))
                      .arg(m_dxCall)
                      .arg(elapsedS)
                      .arg(m_targetCallTimeoutS)
                      .arg(m_targetCallPauseS));
    }

    if (m_targetCallMaxRetries > 0 && m_targetCallRetryCount >= m_targetCallMaxRetries) {
        decodiumBridgeLogbookLog(QStringLiteral("targetCall: target not heard for %1/%2 TX, stopping CALL")
                  .arg(m_targetCallRetryCount)
                  .arg(m_targetCallMaxRetries));
        endTargetCallNoQso(QStringLiteral("target non ricevuto"));
        return;
    }

    if (m_targetCallTimeoutS > 0 && elapsedS >= m_targetCallTimeoutS) {
        if (txWatchdogLimitConfigured()) {
            if (m_targetCallRetryCount == 1 || m_targetCallRetryCount % 5 == 0) {
                decodiumBridgeLogbookLog(QStringLiteral("targetCall: timeout reached (%1s), TX watchdog priority active -> continue")
                              .arg(m_targetCallTimeoutS));
            }
        } else {
            decodiumBridgeLogbookLog(QStringLiteral("targetCall: timeout reached (%1s)")
                          .arg(m_targetCallTimeoutS));
            endTargetCallNoQso(QStringLiteral("timeout"));
            return;
        }
    }

    if (m_targetCallPauseS > 0) {
        quint64 const sessionId = m_targetCallSessionId;
        setTxEnabled(false);
        decodiumBridgeLogbookLog(QStringLiteral("targetCall: pause %1s before next retry")
                      .arg(m_targetCallPauseS));
        QTimer::singleShot(m_targetCallPauseS * 1000, this, [this, sessionId]() {
            if (!m_targetCallActive
                || m_targetCallSessionId != sessionId
                || m_transmitting
                || m_tuning
                || m_dxCall.compare(m_targetCallSign, Qt::CaseInsensitive) != 0) {
                return;
            }
            QDateTime const resumeNowUtc = QDateTime::currentDateTimeUtc();
            qint64 const resumeElapsedS = m_targetCallStartedUtc.isValid()
                ? qMax<qint64>(0, m_targetCallStartedUtc.secsTo(resumeNowUtc))
                : 0;
            if (m_targetCallTimeoutS > 0 && resumeElapsedS >= m_targetCallTimeoutS) {
                if (txWatchdogLimitConfigured()) {
                    decodiumBridgeLogbookLog(QStringLiteral("targetCall: timeout reached during pause (%1s), TX watchdog priority active -> resume")
                                  .arg(m_targetCallTimeoutS));
                } else {
                    decodiumBridgeLogbookLog(QStringLiteral("targetCall: timeout reached during pause (%1s)")
                                  .arg(m_targetCallTimeoutS));
                    endTargetCallNoQso(QStringLiteral("timeout pausa"));
                    return;
                }
            }
            bool const resumeQsoProgressed = m_qsoProgress > 2 && m_currentTx >= 2 && m_currentTx <= 5;
            if (resumeQsoProgressed) {
                decodiumBridgeLogbookLog(QStringLiteral("targetCall pause resume skipped: QSO already progressed tx=%1 progress=%2")
                              .arg(m_currentTx)
                              .arg(m_qsoProgress));
                return;
            }
            decodiumBridgeLogbookLog(QStringLiteral("targetCall: pause complete, re-enabling TX"));
            armExplicitManualTxRequest(m_currentTx, QStringLiteral("target call resume"));
            setTxEnabled(true);
        });
    }
}

