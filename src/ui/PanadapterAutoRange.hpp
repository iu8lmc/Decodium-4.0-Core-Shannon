#ifndef DECODIUM_PANADAPTER_AUTO_RANGE_HPP
#define DECODIUM_PANADAPTER_AUTO_RANGE_HPP

// Black is applied after normalization by the waterfall shader.  The dB
// range must not compensate for it, or its effect disappears.
inline float panadapterDirectAutoRangeMinDb(float floorDb, float marginDb, float noiseBiasDb)
{
    return floorDb - marginDb + noiseBiasDb;
}

#endif
