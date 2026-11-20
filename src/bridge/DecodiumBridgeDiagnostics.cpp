#include <QString>

static QString bridgeDiagnosticOneLine(QString msg)
{
    msg.replace(QStringLiteral("\r"), QStringLiteral("\\r"));
    msg.replace(QStringLiteral("\n"), QStringLiteral(" | "));
    return msg.simplified();
}
QString decodiumBridgeDiagnosticOneLine(QString value) { return bridgeDiagnosticOneLine(value); }


static bool bridgeStatusLooksLikeWarning(const QString& msg)
{
    QString const lower = msg.toLower();
    return lower.contains(QStringLiteral("errore"))
        || lower.contains(QStringLiteral("error"))
        || lower.contains(QStringLiteral("fallit"))
        || lower.contains(QStringLiteral("failed"))
        || lower.contains(QStringLiteral("impossibile"))
        || lower.contains(QStringLiteral("invalid"))
        || lower.contains(QStringLiteral("non disponibile"))
        || lower.contains(QStringLiteral("non trovato"))
        || lower.contains(QStringLiteral("blocc"))
        || lower.contains(QStringLiteral("blocked"))
        || lower.contains(QStringLiteral("interrott"))
        || lower.contains(QStringLiteral("timeout"))
        || lower.contains(QStringLiteral("watchdog"))
        || lower.contains(QStringLiteral("refused"))
        || lower.contains(QStringLiteral("skipped"));
}

bool decodiumBridgeStatusLooksLikeWarning(const QString& msg) { return bridgeStatusLooksLikeWarning(msg); }

