#pragma once

#include <QByteArray>
#include <QString>

namespace decodium::lotw {

enum class ReportResponseKind {
    Adif,
    AuthenticationError,
    ServiceError,
    InvalidResponse
};

struct ReportResponse {
    ReportResponseKind kind {ReportResponseKind::InvalidResponse};
    QString error;
};

// The LoTW report endpoint returns an HTML login/error page on failure and an
// ADIF document on success.  Keep the diagnosis explicit so an unavailable
// service is never mistaken for bad credentials.
inline ReportResponse parseReportResponse(const QByteArray& payload)
{
    const QByteArray lower = payload.toLower();
    if (lower.contains("<eoh>")) {
        return {ReportResponseKind::Adif, {}};
    }

    if (lower.contains("username/password incorrect")
        || lower.contains("invalid username")
        || lower.contains("invalid password")
        || lower.contains("login failed")
        || lower.contains("not logged in")) {
        return {ReportResponseKind::AuthenticationError,
                QStringLiteral("LoTW ha rifiutato username o password")};
    }
    if (lower.contains("maintenance")
        || lower.contains("temporarily unavailable")
        || lower.contains("service unavailable")
        || lower.contains("try again later")) {
        return {ReportResponseKind::ServiceError,
                QStringLiteral("servizio LoTW temporaneamente non disponibile; riprovare più tardi")};
    }
    if (lower.contains("<html")) {
        return {ReportResponseKind::InvalidResponse,
                QStringLiteral("LoTW ha restituito una pagina HTML senza un errore di autenticazione riconoscibile")};
    }
    return {ReportResponseKind::InvalidResponse,
            QStringLiteral("la risposta LoTW non contiene un ADIF valido")};
}

} // namespace decodium::lotw
