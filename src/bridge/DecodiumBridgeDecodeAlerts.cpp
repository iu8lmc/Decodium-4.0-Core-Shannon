#include "DecodiumBridge.h"
#include "DecodiumAlertManager.h"
#include "DecodiumProfileSettings.h"
#include <QApplication>
#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>

void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::maybePlayDecodeAlert(bool isCQ, bool isMyCall, const QString &message)
{
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const alertGapMs = qMax<qint64>(2500, periodMsForMode(m_mode) * 3 / 4);
    if (isMyCall) {
        QString const reason = !m_alertSoundsEnabled ? QStringLiteral("sounds-disabled")
            : !m_alertManager ? QStringLiteral("no-manager")
            : !m_alertOnMyCall ? QStringLiteral("mycall-disabled")
            : nowMs - m_lastMyCallAlertMs < alertGapMs ? QStringLiteral("cooldown")
            : QStringLiteral("play-requested");
        decodiumBridgeAudioLevelLog(QStringLiteral("[DECODE-ALERT] type=MyCall decision=%1 mode=%2 msg=[%3]")
                      .arg(reason, m_mode, message));
    }
    if (!m_alertSoundsEnabled || !m_alertManager) {
        return;
    }

    if (isMyCall && m_alertOnMyCall && nowMs - m_lastMyCallAlertMs >= alertGapMs) {
        m_lastMyCallAlertMs = nowMs;
        playAlert(QStringLiteral("MyCall"));
        return;
    }

    if (isCQ && m_alertOnCq && nowMs - m_lastCqAlertMs >= alertGapMs) {
        m_lastCqAlertMs = nowMs;
        playAlert(QStringLiteral("CQ"));
        return;
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    if (!settings.value(QStringLiteral("alertOnWantedCallsign"), false).toBool()) return;
    const QString wantedText = settings.value(QStringLiteral("wantedCallsigns"), QString()).toString().toUpper();
    const QStringList wanted = wantedText.split(QRegularExpression(QStringLiteral("[,\\s;]+")), Qt::SkipEmptyParts);
    const QStringList words = message.toUpper().replace(QRegularExpression(QStringLiteral("[<>;,]")), QStringLiteral(" "))
                                   .split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    QString matched;
    for (const QString &pattern : wanted) {
        QString rx = QRegularExpression::escape(pattern).replace(QStringLiteral("\\*"), QStringLiteral(".*"));
        QRegularExpression re(QStringLiteral("^") + rx + QStringLiteral("$"));
        for (const QString &word : words) {
            if (re.match(word).hasMatch()) { matched = word; break; }
        }
        if (!matched.isEmpty()) break;
    }
    if (!matched.isEmpty() && (matched != m_lastWantedCall || nowMs - m_lastWantedAlertMs >= alertGapMs)) {
        m_lastWantedCall = matched;
        m_lastWantedAlertMs = nowMs;
        playAlert(QStringLiteral("WantedCallsign"));
    }
}

void DecodiumBridge::playAlert(const QString& alertType)
{
    if (!m_alertSoundsEnabled) return;
    if (m_alertManager) {
        m_alertManager->playAlert(alertType);
    } else {
        // Fallback di sistema se il manager non e' ancora inizializzato
        QApplication::beep();
    }
}

