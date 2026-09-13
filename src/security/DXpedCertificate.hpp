// -*- Mode: C++ -*-
#ifndef DXPEDCERTIFICATE_HPP
#define DXPEDCERTIFICATE_HPP

#include <QByteArray>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLocale>
#include <QMessageAuthenticationCode>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <algorithm>

class DXpedCertificate
{
public:
  bool loadFromFile(QString const& path)
  {
    reset();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QByteArray const raw = f.readAll();
    f.close();

    QJsonParseError jerr;
    QJsonDocument const doc = QJsonDocument::fromJson(raw, &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isObject()) return false;

    QJsonObject obj = doc.object();
    m_signature = obj.value(QStringLiteral("signature")).toString().trimmed();
    if (m_signature.isEmpty()) return false;

    obj.remove(QStringLiteral("signature"));
    m_payload = canonicalizeObject(obj).toUtf8();
    m_certHash = QString::fromLatin1(QCryptographicHash::hash(
      m_payload, QCryptographicHash::Sha256).toHex().left(8));

    QByteArray const expectedSig = QMessageAuthenticationCode::hash(
      m_payload, kHmacKey(), QCryptographicHash::Sha256).toHex();
    m_signatureValid = (QString::fromLatin1(expectedSig).compare(m_signature, Qt::CaseInsensitive) == 0);
    if (!m_signatureValid) return false;

    m_callsign = normalizedCall(obj.value(QStringLiteral("callsign")).toString());
    m_dxccEntity = obj.value(QStringLiteral("dxcc_entity")).toString().trimmed();
    m_dxccName = obj.value(QStringLiteral("dxcc_name")).toString().trimmed();
    m_maxSlots = obj.value(QStringLiteral("max_slots")).toInt(1);
    if (m_maxSlots < 1) m_maxSlots = 1;
    if (m_maxSlots > 4) m_maxSlots = 4;

    QJsonArray const opArray = obj.value(QStringLiteral("operators")).toArray();
    for (auto const& v : opArray) {
      QString const op = normalizedCall(v.toString());
      if (!op.isEmpty() && !m_operators.contains(op)) m_operators.append(op);
    }
    if (!m_callsign.isEmpty() && !m_operators.contains(m_callsign)) {
      m_operators.append(m_callsign);
    }

    m_activationStart = parseActivationDate(
      obj.value(QStringLiteral("activation_start")).toString().trimmed(), true);
    m_activationEnd = parseActivationDate(
      obj.value(QStringLiteral("activation_end")).toString().trimmed(), false);

    m_loaded = !m_callsign.isEmpty() && m_activationStart.isValid() && m_activationEnd.isValid()
               && m_signatureValid;
    return m_loaded;
  }

  bool isValid() const
  {
    if (!m_loaded || !m_signatureValid || !m_activationStart.isValid() || !m_activationEnd.isValid()) {
      return false;
    }
    QDateTime const now = QDateTime::currentDateTimeUtc();
    return now >= m_activationStart && now <= m_activationEnd;
  }

  bool signatureValid() const { return m_signatureValid; }
  QString callsign() const { return m_callsign; }
  QString dxccEntity() const { return m_dxccEntity; }
  QString dxccName() const { return m_dxccName; }
  QStringList operators() const { return m_operators; }
  int maxSlots() const { return m_maxSlots; }
  QDateTime activationStart() const { return m_activationStart; }
  QDateTime activationEnd() const { return m_activationEnd; }
  QString certHash() const { return m_certHash; }

  bool isOperator(QString const& call) const
  {
    QString const in = normalizedCall(call);
    if (in.isEmpty()) return false;
    QString const inBase = baseCall(in);
    for (auto const& op : m_operators) {
      QString const opBase = baseCall(op);
      if (in == op || inBase == op || in == opBase || inBase == opBase) return true;
    }
    return false;
  }

private:
  static QByteArray const& kHmacKey()
  {
    static QByteArray const key {"D3c0d1uM-ASYMX-DXp3d-2026-IU8LMC"};
    return key;
  }

  void reset()
  {
    m_loaded = false;
    m_signatureValid = false;
    m_callsign.clear();
    m_dxccEntity.clear();
    m_dxccName.clear();
    m_operators.clear();
    m_maxSlots = 1;
    m_activationStart = QDateTime{};
    m_activationEnd = QDateTime{};
    m_signature.clear();
    m_payload.clear();
    m_certHash.clear();
  }

  static QString normalizedCall(QString const& in)
  {
    return in.trimmed().toUpper();
  }

  static QString baseCall(QString const& in)
  {
    int const slash = in.indexOf('/');
    if (slash <= 0) return in;
    return in.left(slash);
  }

  static QString escapedJsonString(QString const& s)
  {
    QString out;
    out.reserve(s.size() + 8);
    for (QChar const c : s) {
      ushort const u = c.unicode();
      switch (u) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (u < 0x20) {
          out += QString("\\u%1").arg(u, 4, 16, QChar('0'));
        } else {
          out += c;
        }
        break;
      }
    }
    return out;
  }

  static QString canonicalizeValue(QJsonValue const& v)
  {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("null");
    if (v.isBool()) return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isString()) return QStringLiteral("\"") + escapedJsonString(v.toString()) + QStringLiteral("\"");
    if (v.isDouble()) {
      double const d = v.toDouble();
      double const r = qRound64(d);
      if (qAbs(d - r) < 1e-12) return QString::number(static_cast<qint64>(r));
      return QLocale::c().toString(d, 'g', 15);
    }
    if (v.isArray()) {
      QJsonArray const arr = v.toArray();
      QString out = QStringLiteral("[");
      for (int i = 0; i < arr.size(); ++i) {
        if (i) out += ',';
        out += canonicalizeValue(arr.at(i));
      }
      out += ']';
      return out;
    }
    if (v.isObject()) {
      return canonicalizeObject(v.toObject());
    }
    return QStringLiteral("null");
  }

  static QString canonicalizeObject(QJsonObject const& obj)
  {
    QStringList keys = obj.keys();
    std::sort(keys.begin(), keys.end());
    QString out = QStringLiteral("{");
    for (int i = 0; i < keys.size(); ++i) {
      if (i) out += ',';
      QString const& key = keys.at(i);
      out += QStringLiteral("\"") + escapedJsonString(key) + QStringLiteral("\":")
          + canonicalizeValue(obj.value(key));
    }
    out += '}';
    return out;
  }

  static QDateTime parseActivationDate(QString const& raw, bool isStart)
  {
    if (raw.isEmpty()) return {};

    static QRegularExpression const dateOnly {R"(^\d{4}-\d{2}-\d{2}$)"};
    if (dateOnly.match(raw).hasMatch()) {
      QDate const d = QDate::fromString(raw, Qt::ISODate);
      if (!d.isValid()) return {};
      return isStart
        ? QDateTime(d, QTime(0, 0, 0), Qt::UTC)
        : QDateTime(d, QTime(23, 59, 59), Qt::UTC);
    }

    QDateTime dt = QDateTime::fromString(raw, Qt::ISODateWithMs);
    if (!dt.isValid()) dt = QDateTime::fromString(raw, Qt::ISODate);
    if (!dt.isValid()) return {};
    if (dt.timeSpec() == Qt::LocalTime) dt = dt.toUTC();
    if (dt.timeSpec() == Qt::OffsetFromUTC) dt = dt.toUTC();
    if (dt.timeSpec() == Qt::TimeZone) dt = dt.toUTC();
    if (dt.timeSpec() == Qt::UTC) return dt;
    return QDateTime(dt.date(), dt.time(), Qt::UTC);
  }

  bool m_loaded {false};
  bool m_signatureValid {false};
  QString m_callsign;
  QString m_dxccEntity;
  QString m_dxccName;
  QStringList m_operators;
  int m_maxSlots {1};
  QDateTime m_activationStart;
  QDateTime m_activationEnd;
  QString m_signature;
  QByteArray m_payload;
  QString m_certHash;
};

#endif // DXPEDCERTIFICATE_HPP
