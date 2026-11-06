// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "HamDrmHybridCodec.h"
#include <QByteArray>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

namespace decodium::sstv::hamdrm {

// FTP is the legacy hybrid wire protocol, not a confidential transport.
// Call only after an explicit user action; never log server credentials.
class HamDrmHybridFtp final : public QObject {
    Q_OBJECT
public:
    explicit HamDrmHybridFtp(QObject* parent = nullptr, int idleTimeoutMs = 30000);
    bool download(const HamDrmHybridServer& server, const QString& filename,
                  quint16 port = 21, qsizetype maximumBytes = 16 * 1024 * 1024);
    bool upload(const HamDrmHybridServer& server, const QString& filename,
                const QByteArray& data, quint16 port = 21);
    void cancel();
signals:
    void finished(bool success, QByteArray data, QString error);
private:
    enum class State { Idle, Greeting, User, Password, Type, Directory,
                       HybridDirectory, Epsv, Pasv, DataConnect, Retrieve, Transfer };
    void readControl();
    void handleReply(int code, const QByteArray& line);
    void command(const QByteArray& text, State state);
    void fail(const QString& message);
    void maybeFinish();
    void noteProgress();
    QTcpSocket control_, data_;
    QTimer timer_;
    HamDrmHybridServer server_;
    QByteArray filename_, replyBuffer_, payload_;
    State state_ {State::Idle};
    int multilineCode_ {0};
    int idleTimeoutMs_ {30000};
    qsizetype maximumBytes_ {0};
    bool controlFinished_ {false};
    bool dataFinished_ {false};
    bool upload_ {false};
};
}
