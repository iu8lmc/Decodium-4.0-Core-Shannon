#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H
#include <QString>

struct GeoDistanceInfo
{
  int az {0};
  int el {0};
  int miles {0};
  int km {0};
  int hotAz {0};
  bool hotABetter {true};
};

double tx_duration(QString mode, double trPeriod, int nsps, bool bFast9);
GeoDistanceInfo geo_distance(QString const& myGrid, QString const& hisGrid, double utch);

#endif // HELPER_FUNCTIONS_H
