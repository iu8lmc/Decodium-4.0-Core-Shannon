#ifndef DECODIUMQRZLOGBOOKLITE_H
#define DECODIUMQRZLOGBOOKLITE_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

class QNetworkAccessManager;

class DecodiumQrzLogbookLite : public QObject
{
    Q_OBJECT

public:
    explicit DecodiumQrzLogbookLite(QObject* parent = nullptr);
    ~DecodiumQrzLogbookLite() override;

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    QString apiKey() const { return m_apiKey; }
    void setApiKey(const QString& apiKey);

    bool replaceDuplicates() const { return m_replaceDuplicates; }
    void setReplaceDuplicates(bool replace) { m_replaceDuplicates = replace; }

    void uploadAdif(const QString& dxCall, const QByteArray& adifRecord);
    Q_INVOKABLE void testApi();

signals:
    void apiKeyOk();
    void qsoLogged(const QString& dxCall);
    void errorOccurred(const QString& msg);

private:
    QByteArray formBody(const QList<QPair<QString, QString>>& fields) const;
    QNetworkAccessManager* m_nam {nullptr};
    QString m_apiKey;
    bool m_enabled {false};
    bool m_replaceDuplicates {false};
};

#endif // DECODIUMQRZLOGBOOKLITE_H
