#include "DecodiumCertificate.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QTextStream>

namespace
{
QByteArray signingKey ()
{
  QByteArray k;
  k.append ("D3c0d");
  k.append ("1um_R");
  k.append ("4pt0r");
  k.append ("_2026");
  k.append ("_IU8L");
  k.append ("MC_s1");
  k.append ("gn_k3");
  k.append ("y_v01");
  return k;
}

QByteArray hmacSha256 (QByteArray const& key, QByteArray const& message)
{
  int constexpr blockSize = 64;

  QByteArray k = key;
  if (k.size () > blockSize) {
    k = QCryptographicHash::hash (k, QCryptographicHash::Sha256);
  }
  if (k.size () < blockSize) {
    k.append (QByteArray (blockSize - k.size (), '\0'));
  }

  QByteArray opad (blockSize, 0x5c);
  QByteArray ipad (blockSize, 0x36);
  for (int i = 0; i < blockSize; ++i) {
    opad[i] = opad[i] ^ k[i];
    ipad[i] = ipad[i] ^ k[i];
  }

  QByteArray inner = QCryptographicHash::hash (ipad + message, QCryptographicHash::Sha256);
  return QCryptographicHash::hash (opad + inner, QCryptographicHash::Sha256);
}
}

DecodiumCertificate::DecodiumCertificate () = default;

bool DecodiumCertificate::load (QString const& filePath)
{
  clear ();

  QFile f {filePath};
  if (!f.open (QIODevice::ReadOnly | QIODevice::Text)) {
    qDebug () << "DecodiumCertificate: cannot open" << filePath;
    return false;
  }

  QTextStream in {&f};
  QString header = in.readLine ().trimmed ();
  if (header != "DECODIUM-CERT-V1") {
    qDebug () << "DecodiumCertificate: invalid header" << header;
    return false;
  }

  QString call;
  QString tierStr;
  QString expiresStr;
  QString sig;

  while (!in.atEnd ()) {
    QString line = in.readLine ().trimmed ();
    if (line.isEmpty () || line.startsWith ('#')) {
      continue;
    }

    int eq = line.indexOf ('=');
    if (eq < 0) {
      continue;
    }

    QString key = line.left (eq).trimmed ().toUpper ();
    QString val = line.mid (eq + 1).trimmed ();
    if (key == "CALL") {
      call = val.toUpper ();
    } else if (key == "TIER") {
      tierStr = val.toUpper ();
    } else if (key == "EXPIRES") {
      expiresStr = val;
    } else if (key == "SIG") {
      sig = val;
    }
  }

  if (call.isEmpty () || tierStr.isEmpty () || expiresStr.isEmpty () || sig.isEmpty ()) {
    qDebug () << "DecodiumCertificate: missing fields";
    return false;
  }

  if (!verify (call, tierStr, expiresStr, sig)) {
    qDebug () << "DecodiumCertificate: signature verification FAILED for" << call;
    return false;
  }

  m_callsign = call;
  m_tier = (tierStr == "PRO") ? PRO : FREE;
  m_expires = QDate::fromString (expiresStr, "yyyy-MM-dd");
  m_valid = m_expires.isValid ();
  m_sourcePath = filePath;

  if (m_valid) {
    qDebug () << "DecodiumCertificate: loaded OK -" << m_callsign
              << tierName () << "expires" << m_expires.toString ("yyyy-MM-dd")
              << (isExpired () ? "(EXPIRED)" : "(ACTIVE)");
  }

  return m_valid;
}

bool DecodiumCertificate::verify (QString const& call, QString const& tierStr,
                                  QString const& expiresStr, QString const& signature) const
{
  QByteArray payload = (call + "|" + tierStr + "|" + expiresStr).toUtf8 ();
  QByteArray expected = hmacSha256 (signingKey (), payload);
  return signature.toLatin1 ().toLower () == expected.toHex ().toLower ();
}

QString DecodiumCertificate::tierName () const
{
  switch (m_tier) {
    case PRO: return "PRO";
    case FREE: return "FREE";
    default: return "NONE";
  }
}

void DecodiumCertificate::clear ()
{
  m_valid = false;
  m_callsign.clear ();
  m_tier = NONE;
  m_expires = QDate {};
  m_sourcePath.clear ();
}
