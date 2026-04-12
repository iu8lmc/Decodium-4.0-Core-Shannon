#ifndef DECODIUM_CERTIFICATE_HPP
#define DECODIUM_CERTIFICATE_HPP

#include <QDate>
#include <QString>

class DecodiumCertificate
{
public:
  enum Tier { NONE = 0, FREE = 1, PRO = 2 };

  DecodiumCertificate ();

  bool load (QString const& filePath);

  bool isValid () const { return m_valid; }
  bool isExpired () const { return m_valid && m_expires < QDate::currentDate (); }
  bool isActive () const { return m_valid && !isExpired (); }

  QString callsign () const { return m_callsign; }
  Tier tier () const { return m_tier; }
  QDate expires () const { return m_expires; }
  QString tierName () const;
  QString sourcePath () const { return m_sourcePath; }

  // Reserved for future feature gating. The current integration does not enforce them.
  bool canSendDecodiumId () const { return isActive (); }
  bool canQuickQSO () const { return isActive (); }
  bool canShowVerifiedBadge () const { return isActive (); }
  bool canUsePremiumFeatures () const { return isActive () && m_tier >= PRO; }

  void clear ();

private:
  bool verify (QString const& call, QString const& tierStr,
               QString const& expiresStr, QString const& signature) const;

  bool m_valid {false};
  QString m_callsign;
  Tier m_tier {NONE};
  QDate m_expires;
  QString m_sourcePath;
};

#endif
