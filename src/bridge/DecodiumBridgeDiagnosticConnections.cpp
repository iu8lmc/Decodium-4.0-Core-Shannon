#include "DecodiumBridge.h"
#include "DecodiumLogging.hpp"

QString decodiumBridgeDiagnosticOneLine(QString);
void decodiumBridgeQsoJournal(const QString&);
bool decodiumBridgeStatusLooksLikeWarning(const QString&);
#define bridgeDiagnosticOneLine decodiumBridgeDiagnosticOneLine
#define qsoJournal decodiumBridgeQsoJournal
#define bridgeStatusLooksLikeWarning decodiumBridgeStatusLooksLikeWarning

void DecodiumBridge::initialiseDiagnosticConnections()
{
    connect(this, &DecodiumBridge::errorMessage, this, [](const QString& msg) {
        DIAG_ERROR(QStringLiteral("[UIERR] non-blocking error: %1")
                       .arg(bridgeDiagnosticOneLine(msg)));
    }, Qt::DirectConnection);
    connect(this, &DecodiumBridge::warningRaised, this,
            [](const QString& title, const QString& summary, const QString& details) {
        QString line = QStringLiteral("[UIWARN] non-blocking warning: title=\"%1\" summary=\"%2\"")
                           .arg(bridgeDiagnosticOneLine(title),
                                bridgeDiagnosticOneLine(summary));
        if (!details.trimmed().isEmpty()) {
            line += QStringLiteral(" details=\"%1\"").arg(bridgeDiagnosticOneLine(details));
        }
        DIAG_WARN(line);
    }, Qt::DirectConnection);
    connect(this, &DecodiumBridge::rigErrorRaised, this,
            [](const QString& title, const QString& summary, const QString& details) {
        QString line = QStringLiteral("[RIGERR] non-blocking rig error: title=\"%1\" summary=\"%2\"")
                           .arg(bridgeDiagnosticOneLine(title),
                                bridgeDiagnosticOneLine(summary));
        if (!details.trimmed().isEmpty()) {
            line += QStringLiteral(" details=\"%1\"").arg(bridgeDiagnosticOneLine(details));
        }
        DIAG_WARN(line);
    }, Qt::DirectConnection);
    connect(this, &DecodiumBridge::statusMessage, this, [](const QString& msg) {
        // Registro dei QSO: cosa abbiamo trasmesso e come e' finito.
        if (msg.startsWith(QStringLiteral("TX: ")) || msg.contains(QStringLiteral("QSO"))) {
            qsoJournal(QStringLiteral("[STATUS] ") + msg);
        }
        if (bridgeStatusLooksLikeWarning(msg)) {
            DIAG_WARN(QStringLiteral("[UISTATUS] non-blocking status warning: %1")
                          .arg(bridgeDiagnosticOneLine(msg)));
        }
    }, Qt::DirectConnection);

}
