#include "DecodiumCommunityReport.hpp"
#include "DecodiumLogging.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

namespace {

// Host CANONICO per le chiamate API: groups.ft2.it fa 301 verso qui e un 301
// non preserva metodo/body/percorso, quindi GET e POST DEVONO puntare a community.
constexpr auto kBase    = "https://community.ft2.it";
constexpr auto kNewPage = "https://community.ft2.it/groups/new";
constexpr auto kPostUrl = "https://community.ft2.it/groups";
// Indirizzo BREVE mostrato all'utente (quello che usa il radioamatore). Vale per
// la lista dei gruppi; i link diretti al topic restano su community (deep-link).
constexpr auto kDisplayBase = "https://groups.ft2.it";

// Non ripetere la stessa segnalazione entro questa finestra (giorni).
constexpr int  kDedupDays = 7;

QSettings settingsStore()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
}

}  // namespace

DecodiumCommunityReport::DecodiumCommunityReport(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void DecodiumCommunityReport::setBusy(bool b)
{
    if (m_busy == b)
        return;
    m_busy = b;
    emit busyChanged();
}

QString DecodiumCommunityReport::signatureOf(const QString& category, const QString& title) const
{
    // Firma stabile: categoria + titolo normalizzato (minuscolo, spazi compressi).
    QString norm = title.toLower().simplified();
    const QByteArray h =
        QCryptographicHash::hash((category + QLatin1Char('|') + norm).toUtf8(),
                                 QCryptographicHash::Sha1);
    return QString::fromLatin1(h.toHex());
}

bool DecodiumCommunityReport::alreadyReported(const QString& category, const QString& title) const
{
    const QString sig = signatureOf(category, title);
    const QDateTime when =
        settingsStore().value(QStringLiteral("Community/Sig_") + sig).toDateTime();
    if (!when.isValid())
        return false;
    return when.daysTo(QDateTime::currentDateTimeUtc()) < kDedupDays;
}

void DecodiumCommunityReport::rememberSignature(const QString& category, const QString& title)
{
    QSettings s = settingsStore();
    s.setValue(QStringLiteral("Community/Sig_") + signatureOf(category, title),
               QDateTime::currentDateTimeUtc());
    s.sync();
}

QString DecodiumCommunityReport::diagnosticLogTail(int maxBytes) const
{
    QFile f(DecodiumLogging::diagnosticLogPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const qint64 size = f.size();
    const qint64 start = qMax<qint64>(0, size - qMax(1024, maxBytes));
    if (start > 0) {
        f.seek(start);
        f.readLine();  // scarta la prima riga tronca
    }
    return QString::fromUtf8(f.readAll());
}

void DecodiumCommunityReport::postProblem(const QString& category, const QString& title,
                                          const QString& author, const QString& body)
{
    if (m_busy)
        return;
    if (title.trimmed().isEmpty() || body.trimmed().isEmpty()) {
        m_lastError = tr("Please describe the problem before sending.");
        emit failed(m_lastError);
        emit finished();
        return;
    }

    setBusy(true);
    m_lastUrl.clear();
    m_lastError.clear();

    // 1) GET della pagina: prende il cookie di sessione (ft2sid) e il token _csrf.
    //    Il cookie resta nel cookie-jar del QNetworkAccessManager e viaggia da
    //    solo con la POST successiva.
    QNetworkRequest req{QUrl(QString::fromLatin1(kNewPage))};
    req.setRawHeader("User-Agent", "Decodium/4.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, category, title, author, body]() {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    setBusy(false);
                    m_lastError = tr("Cannot reach the community: %1").arg(reply->errorString());
                    emit failed(m_lastError);
                    emit finished();
                    return;
                }
                const QString html = QString::fromUtf8(reply->readAll());
                // Il token puo' comparire come name="_csrf" ... value="..." oppure
                // con gli attributi invertiti: proviamo entrambe le forme.
                QString csrf;
                QRegularExpression re1(
                    QStringLiteral("name=\"_csrf\"[^>]*value=\"([^\"]+)\""));
                QRegularExpression re2(
                    QStringLiteral("value=\"([^\"]+)\"[^>]*name=\"_csrf\""));
                auto m1 = re1.match(html);
                auto m2 = re2.match(html);
                if (m1.hasMatch())
                    csrf = m1.captured(1);
                else if (m2.hasMatch())
                    csrf = m2.captured(1);

                if (csrf.isEmpty()) {
                    setBusy(false);
                    m_lastError = tr("The community form could not be prepared. Please try "
                                     "again later or post manually at %1")
                                      .arg(QString::fromLatin1(kDisplayBase));
                    emit failed(m_lastError);
                    emit finished();
                    return;
                }
                doPost(csrf, category, title, author, body);
            });
}

void DecodiumCommunityReport::doPost(const QString& csrf, const QString& category,
                                     const QString& title, const QString& author,
                                     const QString& body)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("_csrf"), csrf);
    form.addQueryItem(QStringLiteral("website"), QString());  // honeypot: DEVE restare vuoto
    form.addQueryItem(QStringLiteral("category"), category.isEmpty()
                                                      ? QStringLiteral("other") : category);
    form.addQueryItem(QStringLiteral("author"), author.left(20));
    form.addQueryItem(QStringLiteral("title"), title.left(140));
    // Il form limita il body a 8000 caratteri: se sforiamo, il server rifiuta
    // silenziosamente (rimostra la pagina). Tronchiamo tenendo la CODA, che per
    // un log e' la parte piu' recente e utile; avvisiamo che e' stato tagliato.
    QString safeBody = body;
    if (safeBody.size() > 8000) {
        const QString mark = tr("\n[...truncated, %1 characters total...]").arg(body.size());
        safeBody = body.left(8000 - mark.size() - 1) + mark;
    }
    form.addQueryItem(QStringLiteral("body"), safeBody);

    QNetworkRequest req{QUrl(QString::fromLatin1(kPostUrl))};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    req.setRawHeader("User-Agent", "Decodium/4.0");
    req.setRawHeader("Referer", kNewPage);  // il server puo' controllarlo (SameSite/CSRF)
    // NON seguiamo il redirect: cosi' distinguiamo il successo (302 verso il topic
    // creato) dal rifiuto (200 = form rimostrato con l'errore di validazione).
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);

    const QByteArray payload = form.query(QUrl::FullyEncoded).toUtf8();
    QNetworkReply* reply = m_nam->post(req, payload);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, category, title]() {
                reply->deleteLater();
                setBusy(false);

                const int status =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

                // Successo = redirect (302/303) verso il topic appena creato.
                if (status == 302 || status == 303) {
                    const QByteArray loc = reply->rawHeader("Location");
                    QString url = QString::fromUtf8(loc);
                    if (url.startsWith(QLatin1Char('/')))
                        url = QString::fromLatin1(kBase) + url;
                    m_lastUrl = url.isEmpty() ? QString::fromLatin1(kDisplayBase)
                                              : url;
                    rememberSignature(category, title);
                    emit posted(m_lastUrl);
                    emit finished();
                    return;
                }

                // 200 = il server ha rimostrato il form: la validazione e' fallita
                // (di solito il body troppo lungo, o un campo mancante).
                if (status == 200) {
                    m_lastError = tr("The community rejected the report (likely too long). "
                                     "Please shorten it and try again.");
                    emit failed(m_lastError);
                    emit finished();
                    return;
                }

                m_lastError = tr("Could not send the report: %1")
                                  .arg(reply->error() != QNetworkReply::NoError
                                           ? reply->errorString()
                                           : tr("unexpected server response (%1)").arg(status));
                emit failed(m_lastError);
                emit finished();
            });
}
