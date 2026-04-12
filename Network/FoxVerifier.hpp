
#ifndef WSJTX2_FOXVERIFIER_HPP
#define WSJTX2_FOXVERIFIER_HPP

#include <QObject>
#include <QString>
#include <QPointer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QMutex>

#define FOXVERIFIER_DEFAULT_TIMEOUT_MSEC 5000
#define FOXVERIFIER_DEFAULT_BASE_URL "https://www.9dx.cc"

class FoxVerifier : public QObject {
    Q_OBJECT
    QMutex mutex_;

public:
    explicit FoxVerifier(QString user_agent, QNetworkAccessManager *manager, QString base_url, QString callsign, QDateTime timestamp, QString code, unsigned int);
    ~FoxVerifier();

    QString return_value;
    bool finished();
    static QString formatDecodeMessage(QDateTime ts, QString callsign, unsigned int hz, QString const& verify_message);
    static QString default_url();

private:
    QPointer<QNetworkAccessManager> manager_;
    QPointer<QNetworkReply> reply_;
    QNetworkRequest request_;
    QUrl q_url_;
    bool finished_;
    bool errored_;
    unsigned int hz_;
    QString error_reason_;
    QDateTime ts_;
    QString callsign_;
    QString code_;

private slots:
    void httpFinished();
    void httpRedirected(const QUrl &url);
    void httpEncrypted();
#ifndef QT_NO_SSL
    void sslErrors(const QList<QSslError> &);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    void errorOccurred(QNetworkReply::NetworkError code);
#endif
//signals:
     //void results(QString verify_response);
     //void error(QString const& reason) const;

public slots:
signals:
     void verifyComplete(int status, QDateTime ts, QString callsign, QString code, unsigned int hz, QString const& response);
     void verifyError(int status, QDateTime ts, QString callsign, QString code, unsigned int hz, QString const& response);

};


#endif //WSJTX2_FOXVERIFIER_HPP
