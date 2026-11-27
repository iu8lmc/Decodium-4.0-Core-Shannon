// SPDX-License-Identifier: GPL-3.0-or-later
#include "HamDrmHybridFtp.h"
#include "HamDrmMotCodec.h"
#include <QHostAddress>
#include <QRegularExpression>
#include <QStringList>

namespace decodium::sstv::hamdrm {
namespace {
bool validDirectory(const std::string& directory)
{
    if (directory.size() > 512) return false;
    const auto parts = QString::fromStdString(directory).split('/');
    for (const QString& part : parts) if (part == "." || part == "..") return false;
    return true;
}
}

HamDrmHybridFtp::HamDrmHybridFtp(QObject* parent, int idleTimeoutMs) : QObject(parent)
{
    // Retain at most one byte beyond the reply limit so oversized control
    // responses are detected without buffering an arbitrarily large reply.
    control_.setReadBufferSize(16385);
    timer_.setSingleShot(true);
    idleTimeoutMs_ = qBound(100, idleTimeoutMs, 30000);
    timer_.setInterval(idleTimeoutMs_);
    connect(&timer_, &QTimer::timeout, this, [this] { fail(tr("Hybrid FTP operation timed out")); });
    connect(&control_, &QTcpSocket::readyRead, this, &HamDrmHybridFtp::readControl);
    connect(&control_, &QTcpSocket::errorOccurred, this, [this](auto) {
        if (state_ != State::Idle) fail(tr("Hybrid FTP control connection failed"));
    });
    connect(&control_, &QTcpSocket::bytesWritten, this, [this](qint64 bytes) {
        if (bytes > 0) noteProgress();
    });
    connect(&data_, &QTcpSocket::bytesWritten, this, [this](qint64 bytes) {
        if (bytes > 0) noteProgress();
    });
    connect(&data_, &QTcpSocket::connected, this, [this] {
        if (state_ == State::DataConnect) { noteProgress(); command((upload_ ? "STOR " : "RETR ") + filename_, State::Retrieve); }
    });
    connect(&data_, &QTcpSocket::readyRead, this, [this] {
        if (state_ == State::Idle) return;
        if (upload_) { fail(tr("Unexpected hybrid FTP data")); return; }
        if (data_.bytesAvailable() > maximumBytes_ - payload_.size()) {
            fail(tr("Hybrid FTP file exceeds the allowed size")); return;
        }
        const auto bytes = data_.readAll();
        if (!bytes.isEmpty()) noteProgress();
        payload_ += bytes;
    });
    connect(&data_, &QTcpSocket::disconnected, this, [this] {
        if (state_ == State::Idle) return;
        dataFinished_ = true;
        // The local socket can close after handing bytes to the OS while the
        // FTP server is still draining them. No further data progress is
        // observable here; allow a bounded interval for its final 226 reply.
        if (upload_ && !controlFinished_)
            timer_.start(idleTimeoutMs_ * 10);
        else
            noteProgress();
        maybeFinish();
    });
    connect(&data_, &QTcpSocket::errorOccurred, this, [this](auto error) {
        if (state_ != State::Idle && error != QAbstractSocket::RemoteHostClosedError)
            fail(tr("Hybrid FTP data connection failed"));
    });
}

bool HamDrmHybridFtp::download(const HamDrmHybridServer& server, const QString& filename,
                                quint16 port, qsizetype maximumBytes)
{
    if (state_ != State::Idle || port == 0 || maximumBytes <= 0
        || maximumBytes > 16 * 1024 * 1024 || !validDirectory(server.directory)
        || !encodeHamDrmHybridServer(server).ok()
        || filename.toLatin1() != filename.toUtf8()
        || !validateHamDrmFilename(filename.toStdString()).ok()) return false;
    server_ = server;
    upload_ = false;
    filename_ = filename.toLatin1();
    maximumBytes_ = maximumBytes;
    replyBuffer_.clear(); payload_.clear(); multilineCode_ = 0;
    controlFinished_ = dataFinished_ = false;
    state_ = State::Greeting;
    timer_.start(idleTimeoutMs_);
    control_.connectToHost(QString::fromStdString(server.host), port);
    return true;
}

bool HamDrmHybridFtp::upload(const HamDrmHybridServer& server, const QString& filename,
                              const QByteArray& data, quint16 port)
{
    if (data.isEmpty() || data.size() > 16 * 1024 * 1024) return false;
    if (!download(server, filename, port, 16 * 1024 * 1024)) return false;
    upload_ = true;
    payload_ = data;
    return true;
}

void HamDrmHybridFtp::cancel()
{
    if (state_ != State::Idle) fail(tr("Hybrid FTP operation cancelled"));
}

void HamDrmHybridFtp::noteProgress()
{
    if (state_ != State::Idle) timer_.start(idleTimeoutMs_);
}

void HamDrmHybridFtp::command(const QByteArray& text, State state)
{
    state_ = state;
    if (control_.write(text + "\r\n") < 0) fail(tr("Hybrid FTP command failed"));
}

void HamDrmHybridFtp::readControl()
{
    if (control_.bytesAvailable() > 16384 - replyBuffer_.size()) {
        fail(tr("Hybrid FTP reply is too large")); return;
    }
    const auto bytes = control_.readAll();
    if (!bytes.isEmpty()) noteProgress();
    replyBuffer_ += bytes;
    if (replyBuffer_.size() > 16384) { fail(tr("Hybrid FTP reply is too large")); return; }
    while (state_ != State::Idle) {
        const auto end = replyBuffer_.indexOf("\r\n");
        if (end < 0) return;
        const auto line = replyBuffer_.left(end);
        replyBuffer_.remove(0, end + 2);
        if (line.size() < 4 || line[0] < '1' || line[0] > '5'
            || line[1] < '0' || line[1] > '9' || line[2] < '0' || line[2] > '9') {
            if (multilineCode_) continue;
            fail(tr("Invalid hybrid FTP response")); return;
        }
        const int code = (line[0]-'0')*100 + (line[1]-'0')*10 + line[2]-'0';
        if (multilineCode_) {
            if (code != multilineCode_ || line[3] != ' ') continue;
            multilineCode_ = 0;
        } else if (line[3] == '-') { multilineCode_ = code; continue; }
        else if (line[3] != ' ') { fail(tr("Invalid hybrid FTP response")); return; }
        handleReply(code, line);
    }
}

void HamDrmHybridFtp::handleReply(int code, const QByteArray& line)
{
    switch (state_) {
    case State::Greeting:
        if (code == 220) { command("USER " + QByteArray::fromStdString(server_.user), State::User); return; }
        break;
    case State::User:
        if (code == 331) { command("PASS " + QByteArray::fromStdString(server_.password), State::Password); return; }
        if (code == 230) { command("TYPE I", State::Type); return; }
        break;
    case State::Password:
        if (code == 230) { command("TYPE I", State::Type); return; }
        break;
    case State::Type:
        if (code == 200) {
            if (!server_.directory.empty()) command("CWD " + QByteArray::fromStdString(server_.directory), State::Directory);
            else command("CWD HybridFiles1", State::HybridDirectory);
            return;
        }
        break;
    case State::Directory:
        if (code == 250) { command("CWD HybridFiles1", State::HybridDirectory); return; }
        break;
    case State::HybridDirectory:
        if (code == 250) { command("EPSV", State::Epsv); return; }
        break;
    case State::Epsv:
    case State::Pasv: {
        if (state_ == State::Epsv && code >= 500) { command("PASV", State::Pasv); return; }
        const bool epsv = state_ == State::Epsv;
        const QRegularExpression expression(epsv
            ? QStringLiteral(R"(\(([!-~])\1\1(\d{1,5})\1\))")
            : QStringLiteral(R"(\((\d{1,3}),(\d{1,3}),(\d{1,3}),(\d{1,3}),(\d{1,3}),(\d{1,3})\))"));
        const auto match = expression.match(QString::fromLatin1(line));
        if ((epsv && code != 229) || (!epsv && code != 227) || !match.hasMatch()) break;
        bool ok = false;
        unsigned port = match.captured(epsv ? 2 : 5).toUInt(&ok);
        if (!ok) break;
        if (!epsv) {
            const unsigned low = match.captured(6).toUInt(&ok);
            if (!ok || port > 255 || low > 255) break;
            port = port * 256 + low;
        }
        if (!port || port > 65535) break;
        // Ignore the PASV-provided address: use the established control peer.
        state_ = State::DataConnect;
        data_.connectToHost(control_.peerAddress(), static_cast<quint16>(port));
        return;
    }
    case State::Retrieve:
        if (code == 125 || code == 150) {
            state_ = State::Transfer;
            if (upload_) {
                if (data_.write(payload_) != payload_.size()) { fail(tr("Hybrid FTP upload failed")); return; }
                data_.disconnectFromHost();
            }
            return;
        }
        break;
    case State::Transfer:
        if (code == 226 || code == 250) { controlFinished_ = true; maybeFinish(); return; }
        break;
    default: break;
    }
    fail(tr("Hybrid FTP server rejected the request"));
}

void HamDrmHybridFtp::maybeFinish()
{
    if (!controlFinished_ || !dataFinished_) return;
    if (!upload_ && payload_.isEmpty()) { fail(tr("Hybrid FTP returned an empty file")); return; }
    QByteArray result = upload_ ? QByteArray() : std::move(payload_);
    state_ = State::Idle; timer_.stop();
    control_.abort(); data_.abort(); server_ = {}; filename_.clear(); payload_.clear();
    emit finished(true, result, {});
}

void HamDrmHybridFtp::fail(const QString& message)
{
    if (state_ == State::Idle) return;
    state_ = State::Idle; timer_.stop();
    control_.abort(); data_.abort(); server_ = {}; filename_.clear(); payload_.clear();
    emit finished(false, {}, message);
}
}
