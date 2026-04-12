#include "FT2QsoFlowPolicy.h"

namespace
{
constexpr int kCalling = 0;
constexpr int kReport = 2;
constexpr int kRogers = 4;
}

Ft2QsoMessageProfile ft2QsoMessageProfileFromCount (int count)
{
  switch (count) {
    case 2: return Ft2QsoMessageProfile::Ultra2;
    case 3: return Ft2QsoMessageProfile::Fast3;
    default: return Ft2QsoMessageProfile::Full5;
  }
}

int ft2QsoMessageCount (Ft2QsoMessageProfile profile)
{
  return static_cast<int> (profile);
}

Ft2QsoFlowDecision applyFt2QsoMessageProfile (Ft2QsoMessageProfile profile,
                                             int ntx,
                                             int qsoProgress,
                                             QString const& hisCall,
                                             bool sentFirst73)
{
  Ft2QsoFlowDecision decision;
  decision.ntx = ntx;
  decision.qsoProgress = qsoProgress;

  if (Ft2QsoMessageProfile::Full5 == profile) {
    return decision;
  }

  if (Ft2QsoMessageProfile::Ultra2 == profile && 1 == ntx) {
    decision.ntx = 2;
    decision.qsoProgress = kReport;
    decision.changed = true;
    return decision;
  }

  if (Ft2QsoMessageProfile::Fast3 == profile && 3 == ntx) {
    decision.ntx = 4;
    decision.qsoProgress = kRogers;
    decision.changed = true;
    return decision;
  }

  if (Ft2QsoMessageProfile::Fast3 == profile && 5 == ntx) {
    decision.ntx = 6;
    decision.qsoProgress = kCalling;
    decision.changed = true;
    decision.markSentFirst73 = !sentFirst73;
    decision.requestImmediateLog = true;
    decision.cooldownCall = hisCall.trimmed ();
  }

  return decision;
}

void pruneFt2QsoCooldown (QMap<QString, qint64>& cooldown, qint64 now, qint64 ttlMs)
{
  for (auto it = cooldown.begin (); it != cooldown.end ();) {
    if (now - it.value () > ttlMs) {
      it = cooldown.erase (it);
    } else {
      ++it;
    }
  }
}

bool isFt2QsoInCooldown (QMap<QString, qint64> const& cooldown,
                         QString const& call,
                         qint64 now,
                         qint64 ttlMs)
{
  auto const normalized = call.trimmed ().toUpper ();
  if (normalized.isEmpty ()) {
    return false;
  }
  auto const it = cooldown.constFind (normalized);
  return it != cooldown.constEnd () && now - it.value () <= ttlMs;
}

void rememberFt2QsoCooldown (QMap<QString, qint64>& cooldown, QString const& call, qint64 now)
{
  auto const normalized = call.trimmed ().toUpper ();
  if (!normalized.isEmpty ()) {
    cooldown[normalized] = now;
  }
}
