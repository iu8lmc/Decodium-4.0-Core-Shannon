#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include <QSettings>

extern void decodiumBridgeLogbookLog(const QString&);

bool DecodiumBridge::autoCallModeSupported() const
{
    const QString mode = m_mode.trimmed().toUpper();
    return mode == QStringLiteral("FT8")
        || mode == QStringLiteral("FT4")
        || mode == QStringLiteral("FT2");
}

void DecodiumBridge::setAutoCallEnabled(bool v)
{
    if (m_autoCallEnabled == v) {
        return;
    }

    if (v) {
        if (!autoCallModeSupported()) {
            const QString message = QStringLiteral("Auto Call is available only in FT8, FT4 and FT2");
            decodiumBridgeLogbookLog(QStringLiteral("[AUTOCALL] rejected: unsupported mode %1").arg(m_mode));
            emit statusMessage(message);
            return;
        }
        if (!checkSwrAllowsTransmission(QStringLiteral("autocall-arm"))) {
            decodiumBridgeLogbookLog(QStringLiteral("[AUTOCALL] rejected: SWR protection is still active"));
            return;
        }
        if (m_targetCallActive) {
            const QString message = QStringLiteral("Stop Direct Call before enabling Auto Call");
            decodiumBridgeLogbookLog(QStringLiteral("[AUTOCALL] rejected: target CALL is active"));
            emit statusMessage(message);
            return;
        }
        if (m_transmitting || m_tuning
            || !m_dxCall.trimmed().isEmpty()
            || (m_qsoProgress > 1 && m_qsoProgress < 6)) {
            const QString message = QStringLiteral("Finish or stop the current QSO before enabling Auto Call");
            decodiumBridgeLogbookLog(QStringLiteral("[AUTOCALL] rejected: active QSO or transmission"));
            emit statusMessage(message);
            return;
        }

        // Auto Call owns the automatic reply path. ACQ and multi-answer are
        // stopped so that two independent schedulers cannot transmit together.
        if (m_autoCqRepeat) {
            setAutoCqRepeat(false);
        }
        if (m_multiAnswerMode) {
            setMultiAnswerMode(false);
        }
        if (!m_autoSeq) {
            m_autoSeq = true;
            emit autoSeqChanged();
        }
        setTxEnabled(false);
        m_autoCallQsoCount = 0;
        emit autoCallQsoCountChanged();
        m_autoCallCurrentCandidate.clear();
        decodiumBridgeLogbookLog(QStringLiteral("[AUTOCALL] enabled: waiting for an eligible CQ"));
    } else {
        m_autoCallCurrentCandidate.clear();
        if (!m_transmitting && !m_tuning) {
            setTxEnabled(false);
        }
        decodiumBridgeLogbookLog(QStringLiteral("[AUTOCALL] disabled"));
    }

    m_autoCallEnabled = v;
    emit autoCallEnabledChanged();
}

void DecodiumBridge::abortAutoCallAfterSwrStop(const QString& reason)
{
    if (!m_autoCallEnabled && m_autoCallCurrentCandidate.isEmpty()) {
        return;
    }

    const QString interruptedCall = m_autoCallCurrentCandidate.trimmed().isEmpty()
        ? m_dxCall.trimmed()
        : m_autoCallCurrentCandidate.trimmed();
    decodiumBridgeLogbookLog(QStringLiteral("[AUTOCALL] SWR safety stop: clearing interrupted QSO call=%1 reason=%2")
                  .arg(interruptedCall.isEmpty() ? QStringLiteral("<none>") : interruptedCall,
                       reason));

    // Do not count an interrupted transmission as a completed QSO. Disarm
    // first because resetQsoStateForNextSequence() intentionally increments
    // the Auto Call session counter when Auto Call is still armed.
    if (m_autoCallEnabled) {
        setAutoCallEnabled(false);
    }
    m_autoCallStartingCandidate = false;
    resetQsoStateForNextSequence(false);

    emit statusMessage(QStringLiteral("Auto Call fermato dalla protezione SWR: correggi l'antenna e premi Start Auto Call"));
}

void DecodiumBridge::setAutoCallMaxQsos(int v)
{
    v = qBound(0, v, 999);
    if (m_autoCallMaxQsos == v) {
        return;
    }
    m_autoCallMaxQsos = v;
    emit autoCallMaxQsosChanged();
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
              QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("CallFeature/AutoCallMaxQsos"), v);
}

void DecodiumBridge::setAutoCallPriority(int v)
{
    v = qBound(0, v, 2);
    if (m_autoCallPriority == v) {
        return;
    }
    m_autoCallPriority = v;
    emit autoCallPriorityChanged();
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
              QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("CallFeature/AutoCallPriority"), v);
}

void DecodiumBridge::resetAutoCallQsoCount()
{
    if (m_autoCallQsoCount == 0) {
        return;
    }
    m_autoCallQsoCount = 0;
    emit autoCallQsoCountChanged();
    decodiumBridgeLogbookLog(QStringLiteral("[AUTOCALL] QSO counter reset"));
    emit statusMessage(QStringLiteral("Auto Call QSO counter reset"));
}


void DecodiumBridge::noteAutoCallQsoCompleted()
{
    if (!m_autoCallEnabled || m_autoCallCurrentCandidate.isEmpty()) {
        return;
    }
    const QString completedCall = m_autoCallCurrentCandidate;
    m_autoCallCurrentCandidate.clear();
    ++m_autoCallQsoCount;
    emit autoCallQsoCountChanged();
    decodiumBridgeLogbookLog(QStringLiteral("[AUTOCALL] completed %1 (%2/%3)")
                  .arg(completedCall)
                  .arg(m_autoCallQsoCount)
                  .arg(m_autoCallMaxQsos == 0 ? QStringLiteral("inf")
                                               : QString::number(m_autoCallMaxQsos)));
    if (m_autoCallMaxQsos > 0 && m_autoCallQsoCount >= m_autoCallMaxQsos) {
        emit statusMessage(QStringLiteral("Auto Call limit reached; reset the counter to continue"));
    }
}

