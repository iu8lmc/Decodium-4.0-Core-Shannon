#include "app/Language.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QQmlEngine>
#include <QVariantMap>

namespace decortty::app {

namespace {

// Le lingue disponibili, con il nome scritto come lo scrive chi la parla.
struct Entry {
    const char* code;
    const char* name;
};

const Entry kLanguages[] = {
    {"en",    "English"},
    {"it",    "Italiano"},
    {"de",    "Deutsch"},
    {"fr",    "Français"},
    {"es",    "Español"},
    {"pt",    "Português"},
    {"nl",    "Nederlands"},
    {"ca",    "Català"},
    {"da",    "Dansk"},
    {"hu",    "Magyar"},
    {"ro",    "Română"},
    {"lv",    "Latviešu"},
    {"ru",    "Русский"},
    {"ja",    "日本語"},
    {"zh",    "简体中文"},
    {"zh_TW", "繁體中文"},
};

// Dove stanno i cataloghi compilati. Nella cartella distribuita sono accanto
// all'eseguibile; dall'albero di compilazione, in translations/.
QStringList searchPaths()
{
    const QString base = QCoreApplication::applicationDirPath();
    return {base + QStringLiteral("/translations"),
            base + QStringLiteral("/../translations"),
            base};
}

} // namespace

Language::Language(QObject* parent)
    : QObject(parent)
{
}

QVariantList Language::available() const
{
    QVariantList list;
    for (const Entry& entry : kLanguages) {
        QVariantMap map;
        map.insert(QStringLiteral("code"), QString::fromLatin1(entry.code));
        map.insert(QStringLiteral("name"), QString::fromUtf8(entry.name));
        list.append(map);
    }
    return list;
}

QString Language::systemCode()
{
    const QLocale locale;
    // Il cinese si distingue per scrittura, non per paese: un lettore di Taiwan
    // e uno di Hong Kong leggono entrambi i caratteri tradizionali pur avendo
    // codici di paese diversi.
    if (locale.language() == QLocale::Chinese) {
        return locale.script() == QLocale::TraditionalHanScript ? QStringLiteral("zh_TW")
                                                                : QStringLiteral("zh");
    }
    const QString name = locale.name();          // per esempio "it_IT"
    const QString code = name.section(QLatin1Char('_'), 0, 0);

    for (const Entry& entry : kLanguages) {
        if (code == QLatin1String(entry.code))
            return code;
    }
    return QStringLiteral("en");
}

bool Language::install(const QString& code)
{
    // Via i traduttori vecchi prima di mettere i nuovi, o le stringhe della
    // lingua precedente resterebbero a vincere su quelle che il catalogo nuovo
    // non traduce.
    if (m_appTranslator) {
        QCoreApplication::removeTranslator(m_appTranslator);
        delete m_appTranslator;
        m_appTranslator = nullptr;
    }
    if (m_qtTranslator) {
        QCoreApplication::removeTranslator(m_qtTranslator);
        delete m_qtTranslator;
        m_qtTranslator = nullptr;
    }

    // L'inglese è la lingua in cui il programma è scritto: non ha catalogo, e
    // non averne uno è la condizione giusta perché compaiano le stringhe
    // originali.
    if (code == QLatin1String("en"))
        return true;

    auto* translator = new QTranslator(this);
    bool loaded = false;
    for (const QString& path : searchPaths()) {
        if (translator->load(QStringLiteral("decortty_") + code, path)) {
            loaded = true;
            break;
        }
    }
    if (!loaded) {
        delete translator;
        return false;
    }
    QCoreApplication::installTranslator(translator);
    m_appTranslator = translator;

    // E i testi che vengono da Qt stessa — i pulsanti dei dialoghi di sistema,
    // i menu contestuali dei campi di testo. Se manca non è grave: restano in
    // inglese mentre il resto è tradotto, il che è meno peggio di un errore.
    auto* qtTranslator = new QTranslator(this);
    const QString qtDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qtTranslator->load(QStringLiteral("qt_") + code, qtDir)
        || qtTranslator->load(QStringLiteral("qtbase_") + code, qtDir)) {
        QCoreApplication::installTranslator(qtTranslator);
        m_qtTranslator = qtTranslator;
    } else {
        delete qtTranslator;
    }
    return true;
}

void Language::setCurrent(const QString& code)
{
    const QString wanted = code.isEmpty() ? systemCode() : code;
    if (m_current == wanted)
        return;

    if (!install(wanted) && wanted != QLatin1String("en")) {
        // Catalogo mancante: si resta dove si è invece di lasciare l'interfaccia
        // a metà fra due lingue.
        return;
    }

    m_current = wanted;
    // Rivaluta tutte le espressioni QML che contengono qsTr(): la finestra passa
    // alla lingua nuova senza riavviare e senza interrompere la ricezione.
    if (m_engine)
        m_engine->retranslate();
    emit currentChanged();
}

} // namespace decortty::app
