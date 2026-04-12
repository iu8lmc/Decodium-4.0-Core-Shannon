#ifndef OTPGENERATOR_H
#define OTPGENERATOR_H
/*
MIT License

Copyright (c) 2023 Dominik Chrástecký

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <QObject>
#include <QCryptographicHash>

#define BASE32_CHARSET "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"

class OTPGenerator : public QObject
{
    Q_OBJECT
public:
    explicit OTPGenerator(QObject *parent = nullptr);

    QByteArray generateHOTP(const QByteArray &rawSecret, quint64 counter, int length);
    QByteArray generateHOTP(const QByteArray &rawSecret, quint64 counter, int length, QCryptographicHash::Algorithm algorithm);
    Q_INVOKABLE QString generateHOTP(const QString &secret, quint64 counter, int length);

    QByteArray generateTOTP(const QByteArray &rawSecret, int length);
    QByteArray generateTOTP(const QByteArray &rawSecret, QDateTime dt, int length, QCryptographicHash::Algorithm algorithm);
    Q_INVOKABLE QString generateTOTP(const QString &secret, QDateTime dt, int length);
    Q_INVOKABLE QString generateTOTP(const QString &secret, int length);
private:
    QByteArray fromBase32(const QString &input);

    signals:

};

#endif // OTPGENERATOR_H
