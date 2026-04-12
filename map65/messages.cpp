#include "messages.h"
#include <QSettings>
#include "SettingsGroup.hpp"
#include "ui_messages.h"
#include "mainwindow.h"
#include "qt_helpers.hpp"

#include <QCoreApplication> //liveCQ
#include <QNetworkAccessManager> //liveCQ
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QEventLoop>

#include <iostream>
#include <string>

Messages::Messages (QString const& settings_filename, QWidget * parent) :
  QDialog {parent},
  ui {new Ui::Messages},
  m_settings_filename {settings_filename},
  m_liveCqManager {new QNetworkAccessManager {this}}
{
  ui->setupUi(this);
  setWindowTitle("Messages");
  setWindowFlags (Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
  QSettings settings {m_settings_filename, QSettings::IniFormat};
  SettingsGroup g {&settings, "MainWindow"}; // MainWindow group for
                                             // historical reasons
  setGeometry (settings.value ("MessagesGeom", QRect {800, 400, 381, 400}).toRect ());
  ui->messagesTextBrowser->setStyleSheet( \
          "QTextBrowser { background-color : #000066; color : red; }");
  ui->messagesTextBrowser->clear();
  m_cqOnly=false;
  m_cqStarOnly=false;
  connect (ui->messagesTextBrowser, &DisplayText::selectCallsign, this, &Messages::selectCallsign2);
}

Messages::~Messages()
{
  QSettings settings {m_settings_filename, QSettings::IniFormat};
  SettingsGroup g {&settings, "MainWindow"};
  settings.setValue ("MessagesGeom", geometry ());
  delete ui;
}

void Messages::sendLiveCQData(QStringList decodeList) {

  QSettings settings(m_settings_filename, QSettings::IniFormat);
  {
    SettingsGroup g {&settings, "MainWindow"};
  }

  SettingsGroup g {&settings, "Common"};
  bool m_w3szUrl = settings.value("w3szUrl",true).toBool();
  QString m_otherUrl = settings.value("otherUrl","").toString();
  QString m_myCall=settings.value("MyCall","").toString();
  QString m_myGrid=settings.value("MyGrid","").toString();
  bool m_xpol = settings.value("Xpol",false).toBool();
  QString theDate = guiDate;
  QString rpol = "--";
  QString theUrl;
  if (m_myCall.length() < 3 || m_myGrid.length() < 4) return;

  if(m_w3szUrl) {
    theUrl = w3szUrlAddr;
  } else {
    theUrl = m_otherUrl;
  }

  QUrl url(theUrl);
  QNetworkRequest request(url);
  request.setRawHeader("User-Agent", "QMAP v0.5");
  request.setRawHeader("X-Custom-User-Agent", "QMAP v0.5");
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  for (const QString &theLine : decodeList) {
    QStringList thePostLine = theLine.split(" ",SkipEmptyParts);
    if (thePostLine.size() < 8) continue;
    if((thePostLine.at(5) == "CQ" || thePostLine.at(5) == "QRZ" || thePostLine.at(5) == "CQV" ||  thePostLine.at(5) == "CQH" || thePostLine.at(5) == "QRT")) {
      if(allDecodes.filter(theLine.mid(0,53)).length() == 0) {
        allDecodes.append(theLine);
        if (thePostLine.size() < 8) continue;
        QString freq = thePostLine.at(0).trimmed();
        QString dF = thePostLine.at(1).trimmed();
        QString utcdatetimestringOriginal = guiDate + " " + thePostLine.at(3).trimmed() + "00"; //needs 2 spaces between date and time
        QDateTime utcdatetimeUTC = QDateTime::fromString(utcdatetimestringOriginal, "yyyy MMM dd  HHmmss");
        utcdatetimeUTC.setTimeSpec((Qt::UTC));
        QString utcdatetimeUTCString = utcdatetimeUTC.toString("yyyy-MM-ddTHH:mm:ss");
        utcdatetimeUTCString = utcdatetimeUTCString + "Z";
        QString dB = thePostLine.at(4).trimmed();
        QString msgType = thePostLine.at(5).trimmed().toUpper();
        QString callsign = thePostLine.at(6).trimmed().toUpper();
        QString grid = "--";
        QString mode="";
        QString txpol = " ";
        QString dT = "";
        QString modeChar = "";
        if(thePostLine.at(7).contains(".")) {
          if (thePostLine.size() < 9) continue;
          dT =thePostLine.at(7).trimmed();
          modeChar = thePostLine.at(8).trimmed(); 
          if(modeChar.contains("#")) mode = "JT65" + modeChar.back();
          else if(modeChar.contains(":")) mode = "Q65-60" + modeChar.back();          
          if(m_xpol) {
            rpol = thePostLine.at(2).trimmed();
          } else {
            rpol = "--";
          }
          txpol = "--";    
        } else {
          if (thePostLine.size() < 10) continue;
          grid = thePostLine.at(7).trimmed();
          dT =thePostLine.at(8).trimmed();
          modeChar = thePostLine.at(9).trimmed();
          if(modeChar.contains("#")) 
          {  
            mode = "JT65" + modeChar.back();            
            if (m_xpol) {
              if (thePostLine.size() < 11) continue;
              rpol = thePostLine.at(2).trimmed();
              if(thePostLine.at(10).contains("H")) txpol = "H";
              else if(thePostLine.at(10).contains("V")) txpol = "V";
            } else txpol="--";
          } else if(modeChar.contains(":")) {
            mode = "Q65-60" + modeChar.back();            
            if (m_xpol) {
              if (thePostLine.size() < 11) continue;
              rpol = thePostLine.at(2).trimmed();
              if(thePostLine.at(10).contains("H")) txpol = "H";
              else if(thePostLine.at(10).contains("V")) txpol = "V";
            } else txpol="--";
          }
        }
        if(mode.contains("JT65") || mode.contains("Q65")) {
          QUrlQuery postData;
          postData.addQueryItem("skedfreq", freq);
          postData.addQueryItem("rxfreq", dF);
          postData.addQueryItem("rpol", rpol);
          postData.addQueryItem("dt", dT);
          postData.addQueryItem("dB", dB);
          postData.addQueryItem("msgtype", msgType.toUpper());
          postData.addQueryItem("callsign", callsign.toUpper());
          postData.addQueryItem("grid", grid.toUpper());
          postData.addQueryItem("mode", mode);
          postData.addQueryItem("utcdatetime", utcdatetimeUTCString);
          postData.addQueryItem("spotter", m_myCall.toUpper());
          postData.addQueryItem("spottergrid", m_myGrid.toUpper());
          postData.addQueryItem("txpol", txpol);
          postData.addQueryItem("apptype", "MAP65");
          QByteArray postByteArray = postData.query(QUrl::FullyEncoded).toUtf8();
          request.setRawHeader("Content-Length",QByteArray::number(postByteArray.size()));

          if (!m_liveCqManager) continue;
          QNetworkReply *reply = m_liveCqManager->post(request,postByteArray);
          QObject::connect(reply, &QNetworkReply::finished, this, &Messages::handleReply);
        }
      }
    }
  }
  // qDebug() << "Size of allDecodes = " << allDecodes.size() ;
}

void Messages::handleReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    if (reply->error() == QNetworkReply::NoError) {
		qDebug() << reply->readAll();
    } else {
		qDebug() << reply->errorString();
    }
    reply->deleteLater();
}

void Messages::setText(QString t, QString t2)
{
  QString cfreq,cfreq0;
  m_t=t;
  m_t2=t2;
  //bool firstTime = true;

  QStringList cqliveText;  //liveCQ
  doLiveCQ = true;         //liveCQ

  QString s="QTextBrowser{background-color: "+m_colorBackground+"}";
  ui->messagesTextBrowser->setStyleSheet(s);

  ui->messagesTextBrowser->clear();
  QStringList lines = t.split( "\n", SkipEmptyParts );
  foreach( QString line, lines ) {
    QString t1=line.mid(0,81); //was 0,75
    int ncq=t1.indexOf(" CQ ");
    if((m_cqOnly or m_cqStarOnly) and  ncq< 0) continue;
    if(m_cqStarOnly) {
      QString caller=t1.mid(ncq+4,-1);
      int nz=caller.indexOf(" ");
      caller=caller.mid(0,nz);
      int i=t2.indexOf(caller);
      if(t2.mid(i-1,1)==" ") continue;
    }
    int n=line.mid(61,2).toInt();  //was 55,2
//    if(line.indexOf(":")>0) n=-1;
//    if(n==-1) ui->messagesTextBrowser->setTextColor("#ffffff");  // white
    if(n==0) ui->messagesTextBrowser->setTextColor(m_color0);
    if(n==1) ui->messagesTextBrowser->setTextColor(m_color1);
    if(n==2) ui->messagesTextBrowser->setTextColor(m_color2);
    if(n>=3) ui->messagesTextBrowser->setTextColor(m_color3);
    QString livecqStr = t1.mid(0,59) + t1.mid(62,t1.length()-62) + " " + t1.mid(60,2); // was 53,56,56,54
  //   QMessageBox msgBox;
  //   msgBox.setText(livecqStr);
  //   if(firstTime) msgBox.exec();
  //   firstTime = false;
    if(cqliveText.filter(livecqStr.mid(0,59)).length()==0) cqliveText.append(livecqStr); // was 0,53
    cfreq=t1.mid(5,3);
    if(cfreq == cfreq0) {
      t1="        " + t1.mid(8,-1);
    }
    cfreq0=cfreq;
    ui->messagesTextBrowser->append(t1.mid(5,67)); //was 5,61
  }

  if(doLiveCQ) {                      //liveCQ
    if(cqliveText.size() > 0) {       //liveCQ
      sendLiveCQData(cqliveText);     //liveCQ
      doLiveCQ = false;               //liveCQ
    }                                 //liveCQ
  }                                   //liveCQ
}

void Messages::selectCallsign2(bool ctrl)
{
  QString t = ui->messagesTextBrowser->toPlainText();   //Full contents
  int i=ui->messagesTextBrowser->textCursor().position();
  int i0=t.lastIndexOf(" ",i);
  int i1=t.indexOf(" ",i);
  QString hiscall=t.mid(i0+1,i1-i0-1);
  if(hiscall!="") {
    if(hiscall.length() < 13) {
      QString t1 = t.mid(0,i);              //contents up to text cursor
      int i1=t1.lastIndexOf("\n") + 1;
      QString t2 = t.mid(i1,-1);            //selected line to end
      int i2=t2.indexOf("\n");
      t2=t2.left(i2);                       //selected line
      emit click2OnCallsign(hiscall,t2,ctrl);
    }
  }
}

void Messages::setColors(QString t)
{
  m_colorBackground = "#"+t.mid(0,6);
  m_color0 = "#"+t.mid(6,6);
  m_color1 = "#"+t.mid(12,6);
  m_color2 = "#"+t.mid(18,6);
  m_color3 = "#"+t.mid(24,6);
  setText(m_t,m_t2);
}

void Messages::on_cbCQ_toggled(bool checked)
{
  m_cqOnly = checked;
  setText(m_t,m_t2);
}

void Messages::on_cbCQstar_toggled(bool checked)
{
  m_cqStarOnly = checked;
  setText(m_t,m_t2);
}
