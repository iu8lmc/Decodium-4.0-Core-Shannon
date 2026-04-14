#ifndef FTX_MESSAGE_ENCODER_HPP
#define FTX_MESSAGE_ENCODER_HPP

#include <QByteArray>
#include <QVector>
#include <QString>
#include <QStringList>

namespace decodium
{
namespace txmsg
{

struct EncodedMessage
{
  bool ok {false};
  QByteArray msgsent;
  QByteArray msgbits;
  QVector<int> tones;
  int i3 {-1};
  int n3 {-1};
  int messageType {-1};
};

struct DecodedMessage
{
  bool ok {false};
  QByteArray msgsent;
};

class Decode77Context
{
public:
  Decode77Context ();

  void clear ();
  void setMyCall (QString const& call);
  void setDxCall (QString const& call);
  void saveHashCall (QString const& call);
  void addRecentCall (QString const& callsign);

  QString lookupHash10 (int hash) const;
  QString lookupHash12 (int hash) const;
  QString lookupHash22 (int hash) const;
  QStringList recentCalls () const;

  bool hasMyCall () const;
  bool hasDxCall () const;
  QString myCall () const;
  QString dxCall () const;
  int hashMy10 () const;
  int hashMy12 () const;
  int hashMy22 () const;
  int hashDx10 () const;
  int hashDx12 () const;
  int hashDx22 () const;

private:
  QVector<QString> m_calls10;
  QVector<QString> m_calls12;
  QVector<int> m_hash22;
  QVector<QString> m_calls22;
  QStringList m_recentCalls;
  QString m_mycall13;
  QString m_dxcall13;
  QString m_mycall13_0;
  QString m_dxcall13_0;
  int m_hashmy10 {-1};
  int m_hashmy12 {-1};
  int m_hashmy22 {-1};
  int m_hashdx10 {-1};
  int m_hashdx12 {-1};
  int m_hashdx22 {-1};
  bool m_mycallSet {false};
  bool m_dxcallSet {false};
};

Decode77Context& sharedDecode77Context ();

EncodedMessage encodeFt2 (QString const& message, bool check_only = false);
EncodedMessage encodeFt4 (QString const& message, bool check_only = false);
EncodedMessage encodeFst4 (QString const& message, bool check_only = false);
EncodedMessage encodeFst4WithHint (QString const& message, bool wspr_hint, bool check_only = false);
EncodedMessage encodeFt8 (QString const& message);
EncodedMessage encodeJt4 (QString const& message, bool check_only = false);
EncodedMessage encodeJt65 (QString const& message, bool check_only = false);
EncodedMessage encodeJt9 (QString const& message, bool check_only = false);
EncodedMessage encodeQ65 (QString const& message, bool check_only = false);
EncodedMessage encodeWspr (QString const& message, bool check_only = false);
EncodedMessage encodeMsk144 (QString const& message, bool check_only = false);
DecodedMessage decode77 (QByteArray const& msgbits, Decode77Context* context, bool received = false);
DecodedMessage decode77 (QByteArray const& msgbits, int i3, int n3, Decode77Context* context, bool received = false);
DecodedMessage decode77 (QByteArray const& msgbits, int i3, int n3);

}
}

#endif // FTX_MESSAGE_ENCODER_HPP
