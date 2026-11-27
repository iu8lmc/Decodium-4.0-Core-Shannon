#pragma once
#include <QObject>
#include <QNetworkRequest>
#include <QTimer>
#include <QByteArray>
inline QByteArray lotwMockData;
inline bool lotwMockFail=false;
class LotwMockReply: public QObject {
 Q_OBJECT
public:
 enum NetworkError {NoError=0,TestError=1};
 explicit LotwMockReply(QObject* parent=nullptr):QObject(parent),m_fail(lotwMockFail),m_data(lotwMockData){}
 NetworkError error()const{return m_fail?TestError:NoError;}
 bool isReadable()const{return !m_fail;}
 QByteArray readAll(){return m_data;}
 QString errorString()const{return QStringLiteral("synthetic error");}
 void finish(){emit finished();}
 signals: void finished();
private: bool m_fail;QByteArray m_data;
};
class LotwMockNetwork: public QObject {
 Q_OBJECT
public:
 explicit LotwMockNetwork(QObject* parent=nullptr):QObject(parent){}
 LotwMockReply* get(const QNetworkRequest&){auto* reply=new LotwMockReply(this);QTimer::singleShot(0,reply,[reply](){reply->finish();});return reply;}
};
