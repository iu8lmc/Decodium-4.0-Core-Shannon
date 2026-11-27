#include "DecodiumBridge.h"
#include "helper_functions.h"

double DecodiumBridge::calcDistance(const QString& myGrid, const QString& dxGrid) const
{
    double lat1, lon1, lat2, lon2;
    if (!grid2deg(myGrid, lon1, lat1)) return -1.0;
    if (!grid2deg(dxGrid, lon2, lat2)) return -1.0;
    double az;
    return azdist(lat1, lon1, lat2, lon2, az);
}

double DecodiumBridge::calcBearing(const QString& myGrid, const QString& dxGrid) const
{
    double lat1, lon1, lat2, lon2;
    if (!grid2deg(myGrid, lon1, lat1)) return -1.0;
    if (!grid2deg(dxGrid, lon2, lat2)) return -1.0;
    double az;
    azdist(lat1, lon1, lat2, lon2, az);
    return az;
}

// ============================================================
// C14 — Grid → lat/lon (per AstroPanel QML)
// ============================================================

double DecodiumBridge::latFromGrid(const QString& grid) const
{
    double lon, lat;
    if (!grid2deg(grid, lon, lat)) return 0.0;
    return lat;
}

double DecodiumBridge::lonFromGrid(const QString& grid) const
{
    double lon, lat;
    if (!grid2deg(grid, lon, lat)) return 0.0;
    return lon;
}

