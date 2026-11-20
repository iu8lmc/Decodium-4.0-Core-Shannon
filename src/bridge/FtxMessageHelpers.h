#pragma once
#include <QString>

bool isFtx77MessageMode(QString const& mode);
QString standardFtxQsoMessage(int txNumber, QString const& dxCall, QString const& myCall,
    QString const& grid4, QString const& reportSent, bool sendRR73, bool quickQso, QString const& mode);
QString standardFtxCqMessage(QString const& myCall, QString const& grid4, QString const& mode);
QString customCqPrefixFromMessage(QString const& message, QString const& myCall);
QString ftxCqMessageWithCustomPrefix(QString const& standardMessage, QString const& customPrefix);
