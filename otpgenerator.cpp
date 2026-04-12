
#include "otpgenerator.h"

#include <QMessageAuthenticationCode>
#include <QtEndian>
#include <QDateTime>
#include <QtMath>

#include "PrecisionTime.hpp"

// FROM https://github.com/RikudouSage/QtOneTimePassword/
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

OTPGenerator::OTPGenerator(QObject *parent)
        : QObject{parent}
{

}

QByteArray OTPGenerator::generateHOTP(const QByteArray &rawSecret, quint64 counter, int length)
{
  return generateHOTP(rawSecret, counter, length, QCryptographicHash::Sha1);
}

QByteArray OTPGenerator::generateHOTP(const QByteArray &rawSecret, quint64 counter, int length, QCryptographicHash::Algorithm algorithm)
{
  quint64 const beCounter = qToBigEndian(counter);
  QByteArray data(reinterpret_cast<char const *>(&beCounter), sizeof(beCounter));
  QMessageAuthenticationCode mac(algorithm);
  mac.setKey(rawSecret);
  mac.addData(data);
  QByteArray hmac = mac.result();
  int offset = hmac.at(hmac.length() - 1) & 0xf;
  quint32 truncatedHash = ((hmac.at(offset) & 0x7f) << 24)
                          | ((hmac.at(offset + 1) & 0xff) << 16)
                          | ((hmac.at(offset + 2) & 0xff) << 8)
                          | (hmac.at(offset + 3) & 0xff);
  int modulus = int(qPow(10, length));
  return QByteArray::number(truncatedHash % modulus, 10).rightJustified(length, '0');
}

QString OTPGenerator::generateHOTP(const QString &secret, quint64 counter, int length)
{
  return generateHOTP(fromBase32(secret), counter, length);
}

QByteArray OTPGenerator::generateTOTP(const QByteArray &rawSecret, int length)
{
  const qint64 counter = ntpCorrectedCurrentMSecsSinceEpoch() / 30000;
  return generateHOTP(rawSecret, counter, length);
}

QByteArray OTPGenerator::generateTOTP(const QByteArray &rawSecret, QDateTime dt, int length, QCryptographicHash::Algorithm algorithm)
{
  const qint64 counter = dt.toMSecsSinceEpoch() / 30000;
  return generateHOTP(rawSecret, counter, length, algorithm);
}

QString OTPGenerator::generateTOTP(const QString &secret, int length)
{
  return generateTOTP(fromBase32(secret), length);
}

QString OTPGenerator::generateTOTP(const QString &secret, QDateTime dt, int length)
{
  return generateTOTP(fromBase32(secret), dt, length, QCryptographicHash::Sha1);
}

QByteArray OTPGenerator::fromBase32(const QString &input)
{
  QByteArray result;
  result.reserve((input.length() * 5 + 7) / 8);
  int buffer = 0;
  int bitsLeft = 0;
  for (int i = 0; i < input.length(); i++) {
    int ch = input[i].toLatin1();
    int value;
    if (ch >= 'A' && ch <= 'Z')
      value = ch - 'A';
    else if (ch >= '2' && ch <= '7')
      value = 26 + ch - '2';
    else
      continue;
    buffer = (buffer << 5) | value;
    bitsLeft += 5;
    if (bitsLeft >= 8) {
      result.append(buffer >> (bitsLeft - 8));
      bitsLeft -= 8;
    }
  }
  return result;
}
