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

// GAP 4 — C13: Maidenhead grid ↔ lat/lon + Haversine distance/bearing
bool   grid2deg(const QString& grid, double& lon, double& lat);
QString deg2grid(double lon, double lat);
// Returns distance in km, sets bearing (degrees from N) in azDeg
double azdist(double lat1, double lon1, double lat2, double lon2, double& azDeg);

#endif // HELPER_FUNCTIONS_H
