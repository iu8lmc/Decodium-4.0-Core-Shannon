#pragma once

#include <QByteArray>
#include <QRegularExpression>
#include <QString>
#include <QUrl>

// eQSL's DownloadInbox endpoint builds an ADIF file and returns an HTML page
// containing its (short-lived) link.  It does not normally return the ADIF
// document in the first response.
namespace decodium::eqsl {

enum class InboxPageKind {
    DirectAdif,
    DownloadReady,
    NoRecords,
    AuthenticationError,
    InvalidResponse
};

struct InboxPageResult {
    InboxPageKind kind {InboxPageKind::InvalidResponse};
    QUrl adifUrl;
    QString error;
};

inline InboxPageResult parseInboxPage(const QByteArray& payload, const QUrl& responseUrl)
{
    const QByteArray lower = payload.toLower();
    if (lower.contains("<eoh>")) {
        return {InboxPageKind::DirectAdif, {}, {}};
    }

    const QString html = QString::fromUtf8(payload);
    const QRegularExpression hrefExpression(
        QStringLiteral("href\\s*=\\s*[\"']([^\"']+\\.adi(?:\\?[^\"']*)?)[\"']"),
        QRegularExpression::CaseInsensitiveOption);
    auto links = hrefExpression.globalMatch(html);
    while (links.hasNext()) {
        QString href = links.next().captured(1);
        href.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        const QUrl adifUrl = responseUrl.resolved(QUrl(href));
        const QString host = adifUrl.host().toLower();
        if (adifUrl.scheme() == QStringLiteral("https")
            && (host == QStringLiteral("eqsl.cc") || host.endsWith(QStringLiteral(".eqsl.cc")))) {
            return {InboxPageKind::DownloadReady, adifUrl, {}};
        }
    }

    if (lower.contains("no records") || lower.contains("there were 0 records")) {
        return {InboxPageKind::NoRecords, {}, {}};
    }
    if (lower.contains("no such callsign")
        || lower.contains("not yet logged in")
        || lower.contains("invalid password")
        || lower.contains("incorrect password")
        || lower.contains("login failed")) {
        return {InboxPageKind::AuthenticationError, {},
                QStringLiteral("eQSL non ha accettato username o password")};
    }

    return {InboxPageKind::InvalidResponse, {},
            QStringLiteral("eQSL non ha restituito il collegamento al file ADI")};
}

} // namespace decodium::eqsl
