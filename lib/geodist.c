/* geodist.c — C replacement for lib/geodist.f90
 *
 * subroutine geodist(Eplat, Eplon, Stlat, Stlon, Az, Baz, Dist)
 *   Spheroidal geodesic distance and azimuths (Thomas 1970).
 *   Clarke 1866 ellipsoid.  West longitude is positive.
 *   All arguments are single-precision reals (float).
 */
#include <math.h>

void geodist_(const float *Eplat, const float *Eplon,
              const float *Stlat, const float *Stlon,
              float *Az, float *Baz, float *Dist)
{
    static const float AL  = 6378206.4f;   /* Clarke 1866 semi-major axis */
    static const float BL  = 6356583.8f;
    static const float D2R = 0.01745329251994f;
    static const float Pi2 = 6.28318530718f;

    if (fabsf(*Eplat - *Stlat) < 0.02f && fabsf(*Eplon - *Stlon) < 0.02f) {
        *Az = 0.0f; *Baz = 180.0f; *Dist = 0.0f;
        return;
    }

    float BOA = BL / AL;
    float F   = 1.0f - BOA;

    float P1R = *Eplat * D2R,  P2R = *Stlat * D2R;
    float L1R = *Eplon * D2R,  L2R = *Stlon * D2R;
    float DLR = L2R - L1R;

    float T1R  = atanf(BOA * tanf(P1R));
    float T2R  = atanf(BOA * tanf(P2R));
    float TM   = (T1R + T2R) / 2.0f;
    float DTM  = (T2R - T1R) / 2.0f;
    float STM  = sinf(TM),  CTM  = cosf(TM);
    float SDTM = sinf(DTM), CDTM = cosf(DTM);

    float KL    = STM * CDTM;
    float KK    = SDTM * CTM;
    float SDLMR = sinf(DLR / 2.0f);
    float L     = SDTM*SDTM + SDLMR*SDLMR * (CDTM*CDTM - STM*STM);
    float CD    = 1.0f - 2.0f * L;
    float DL    = acosf(CD);
    float SD    = sinf(DL);
    float T     = DL / SD;
    float U     = 2.0f * KL * KL / (1.0f - L);
    float V     = 2.0f * KK * KK / L;
    float D     = 4.0f * T * T;
    float X     = U + V;
    float E     = -2.0f * CD;
    float Y     = U - V;
    float A     = -D * E;
    float FF64  = F * F / 64.0f;

    *Dist = AL * SD * (T - (F / 4.0f) * (T*X - Y)
            + FF64 * (X*(A + (T - (A+E)/2.0f)*X) + Y*(-2.0f*D + E*Y) + D*X*Y))
            / 1000.0f;

    float TDLPM = tanf((DLR + (-((E*(4.0f-X) + 2.0f*Y)
                  * ((F/2.0f)*T + FF64*(32.0f*T + (A-20.0f*T)*X
                  - 2.0f*(D+2.0f)*Y)) / 4.0f) * tanf(DLR))) / 2.0f);

    float HAPBR = atan2f(SDTM, CTM * TDLPM);
    float HAMBR = atan2f(CDTM, STM * TDLPM);

    float A1M2 = Pi2 + HAMBR - HAPBR;
    float A2M1 = Pi2 - HAMBR - HAPBR;

    while (A1M2 < 0.0f)   A1M2 += Pi2;
    while (A1M2 >= Pi2)   A1M2 -= Pi2;
    while (A2M1 < 0.0f)   A2M1 += Pi2;
    while (A2M1 >= Pi2)   A2M1 -= Pi2;

    *Az  = 360.0f - A1M2 / D2R;
    *Baz = 360.0f - A2M1 / D2R;
}
