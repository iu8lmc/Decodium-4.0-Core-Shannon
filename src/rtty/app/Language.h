// DecoRTTY — la lingua dell'interfaccia.
//
// Qt sa già scegliere da sé in base alla lingua del sistema, e nella maggior
// parte dei casi indovina. Ma non sempre: chi tiene Windows in inglese perché è
// così che l'ha trovato, e legge molto più volentieri nella propria lingua,
// deve poterlo dire — e viceversa, chi preferisce i termini tecnici in inglese
// deve poter tornare indietro senza cambiare le impostazioni del sistema.
//
// Il cambio avviene subito, senza riavviare: QQmlEngine::retranslate() rivaluta
// tutte le espressioni che contengono qsTr(), e la finestra si ridisegna nella
// lingua nuova mentre la radio continua a ricevere.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <QVariantList>

class QQmlEngine;

namespace decortty::app {

class Language : public QObject {
    Q_OBJECT

    // Il codice della lingua in uso: "it", "de", "zh_TW"… Vuoto significa che la
    // sceglie il sistema.
    Q_PROPERTY(QString current READ current WRITE setCurrent NOTIFY currentChanged)
    // L'elenco per il menu: ogni voce ha `code` e `name`, e il nome è scritto
    // nella lingua stessa. Un elenco che dice "German" a chi cerca "Deutsch"
    // costringe a tradurre mentalmente proprio chi ha più bisogno di aiuto.
    Q_PROPERTY(QVariantList available READ available CONSTANT)

public:
    explicit Language(QObject* parent = nullptr);

    QString current() const { return m_current; }
    QVariantList available() const;

    // Da chiamare una volta all'avvio, prima di caricare il QML.
    void attachEngine(QQmlEngine* engine) { m_engine = engine; }
    void setCurrent(const QString& code);

    // Il codice che si userebbe seguendo il sistema.
    static QString systemCode();

signals:
    void currentChanged();

private:
    bool install(const QString& code);

    QTranslator* m_appTranslator{nullptr};
    QTranslator* m_qtTranslator{nullptr};
    QQmlEngine*  m_engine{nullptr};
    QString      m_current;
};

} // namespace decortty::app
