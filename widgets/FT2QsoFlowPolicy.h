#ifndef FT2QSOFLOWPOLICY_H
#define FT2QSOFLOWPOLICY_H

#include <QMap>
#include <QString>
#include <QtGlobal>

enum class Ft2QsoMessageProfile
{
  Ultra2 = 2,
  Fast3 = 3,
  Full5 = 5,
};

struct Ft2QsoFlowDecision
{
  int ntx {0};
  int qsoProgress {0};
  bool changed {false};
  bool markSentFirst73 {false};
  bool requestImmediateLog {false};
  QString cooldownCall;
};

Ft2QsoMessageProfile ft2QsoMessageProfileFromCount (int count);
int ft2QsoMessageCount (Ft2QsoMessageProfile profile);
Ft2QsoFlowDecision applyFt2QsoMessageProfile (Ft2QsoMessageProfile profile,
                                             int ntx,
                                             int qsoProgress,
                                             QString const& hisCall,
                                             bool sentFirst73);
void pruneFt2QsoCooldown (QMap<QString, qint64>& cooldown, qint64 now, qint64 ttlMs = 30000);
bool isFt2QsoInCooldown (QMap<QString, qint64> const& cooldown,
                         QString const& call,
                         qint64 now,
                         qint64 ttlMs = 30000);
void rememberFt2QsoCooldown (QMap<QString, qint64>& cooldown, QString const& call, qint64 now);

#endif
