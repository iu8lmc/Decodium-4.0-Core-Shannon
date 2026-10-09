// DecoRTTY — transmit macros.
//
// RTTY operating is almost entirely macro-driven: a contest exchange is the
// same forty characters every time, and typing them at 45 baud while the other
// station waits is not practical. Macros expand placeholders against the
// current QSO so one button sends a complete, correct over.
#pragma once

#include <QAbstractListModel>
#include <QSettings>
#include <QString>
#include <QVector>

namespace decortty::app {

class MacroModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString myCall READ myCall WRITE setMyCall NOTIFY qsoChanged)
    Q_PROPERTY(QString hisCall READ hisCall WRITE setHisCall NOTIFY qsoChanged)
    Q_PROPERTY(QString rstSent READ rstSent WRITE setRstSent NOTIFY qsoChanged)
    Q_PROPERTY(QString myName READ myName WRITE setMyName NOTIFY qsoChanged)
    Q_PROPERTY(QString myQth READ myQth WRITE setMyQth NOTIFY qsoChanged)
    Q_PROPERTY(int serialNumber READ serialNumber WRITE setSerialNumber NOTIFY qsoChanged)

public:
    enum Roles {
        LabelRole = Qt::UserRole + 1,
        TemplateRole,
        ExpandedRole,
    };

    explicit MacroModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString myCall() const  { return m_myCall; }
    QString hisCall() const { return m_hisCall; }
    QString rstSent() const { return m_rstSent; }
    QString myName() const  { return m_myName; }
    QString myQth() const   { return m_myQth; }
    int serialNumber() const { return m_serial; }

    // Dentro Decodium l'identita' di stazione ha una sola sorgente: il
    // profilo generale dell'applicazione. Aggiornarla in un colpo solo evita
    // tre salvataggi intermedi e, soprattutto, fa riespandere subito tutte le
    // macro quando il profilo viene modificato nelle impostazioni principali.
    void setStationProfile(const QString& call, const QString& name,
                           const QString& qth);

    void setMyCall(const QString& call);
    void setHisCall(const QString& call);
    void setRstSent(const QString& rst);
    void setMyName(const QString& name);
    void setMyQth(const QString& qth);
    void setSerialNumber(int serial);

    // Substitute the placeholders: %MYCALL% %HISCALL% %RST% %NAME% %QTH%
    // %SERIAL% %TIME% %DATE%. Unknown placeholders are left alone rather than
    // silently deleted, so a typo is visible instead of producing a wrong over.
    Q_INVOKABLE QString expand(const QString& templateText) const;
    Q_INVOKABLE QString expandAt(int row) const;

    Q_INVOKABLE void setMacro(int row, const QString& label, const QString& templateText);
    Q_INVOKABLE void resetToDefaults();
    // Advance the serial after a completed contest QSO.
    Q_INVOKABLE void incrementSerial();

    void load(QSettings& settings);
    void save(QSettings& settings) const;

signals:
    void qsoChanged();

private:
    struct Macro {
        QString label;
        QString text;
    };

    void refreshExpanded();

    QVector<Macro> m_macros;
    QString m_myCall;
    QString m_hisCall;
    QString m_rstSent{QStringLiteral("599")};
    QString m_myName;
    QString m_myQth;
    int     m_serial{1};
};

} // namespace decortty::app
