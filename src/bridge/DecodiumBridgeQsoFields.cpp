#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumLogging.hpp"
#include "DecodeSignalReport.h"
#include "Sequencer/MessageTokenRules.hpp"

using decodium::seq::isGridTokenStrict;

QString DecodiumBridge::dxGrid() const { return m_dxGrid; }
void DecodiumBridge::setDxGrid(const QString& v) {
    QString clean = v.trimmed().toUpper();
    if (!clean.isEmpty() && !isGridTokenStrict(clean)) {
        DIAG_WARN(QStringLiteral("setDxGrid rejected non-locator value=\"%1\"").arg(clean));
        clean.clear();
    }
    if (m_dxGrid!=clean) {
        m_dxGrid=clean;
        emit dxGridChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setDxGrid(clean);
        }
        regenerateTxMessages();
        emitCurrentWorldMapQsoPath();
    }
}

void DecodiumBridge::setReportSent(const QString& v)
{
    QString const normalized = normalizeSignalReportText(v);
    if (m_reportSent != normalized) {
        m_reportSent = normalized;
        emit reportSentChanged();
        regenerateTxMessages();
    }
}

void DecodiumBridge::setReportReceivedPlaceholder(const QString& v)
{
    QString const normalized = normalizeSignalReportText(v);
    m_reportReceivedReal = false;
    if (m_reportReceived != normalized) {
        m_reportReceived = normalized;
        emit reportReceivedChanged();
        regenerateTxMessages();
    }
}

void DecodiumBridge::setReportReceived(const QString& v)
{
    QString const normalized = normalizeSignalReportText(v);
    m_reportReceivedReal = true;
    if (m_reportReceived != normalized) {
        m_reportReceived = normalized;
        emit reportReceivedChanged();
        regenerateTxMessages();
    }
}

void DecodiumBridge::setSendRR73(bool v)
{
    if (m_sendRR73 != v) {
        m_sendRR73 = v;
        emit sendRR73Changed();
        regenerateTxMessages();
    }
}

