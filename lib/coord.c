/* coord.c — C replacement for lib/coord.f90
 *
 * subroutine COORD(A0,B0,AP,BP,A1,B1,A2,B2)
 *   General spherical-coordinate rotation (Euler angles).
 *   Used for ha/dec ↔ az/el, ra/dec ↔ galactic l/b, ecliptic, etc.
 *   All arguments are single-precision reals (float).
 */
#include <math.h>

void coord_(const float *A0, const float *B0,
            const float *AP, const float *BP,
            const float *A1, const float *B1,
            float *A2, float *B2)
{
    float SB0 = sinf(*B0), CB0 = cosf(*B0);
    float SBP = sinf(*BP), CBP = cosf(*BP);
    float SB1 = sinf(*B1), CB1 = cosf(*B1);

    float SB2 = SBP*SB1 + CBP*CB1*cosf(*AP - *A1);
    float CB2 = sqrtf(1.0f - SB2*SB2);
    *B2 = atanf(SB2 / CB2);

    float SAA = sinf(*AP - *A1) * CB1 / CB2;
    float CAA = (SB1 - SB2*SBP) / (CB2*CBP);
    float CBB = SB0 / CBP;
    float SBB = sinf(*AP - *A0) * CB0;

    float SA2 = SAA*CBB - CAA*SBB;
    float CA2 = CAA*CBB + SAA*SBB;

    float TA2O2 = 0.0f;   /* silences compiler warning, matches Fortran */
    if (CA2 <= 0.0f) TA2O2 = (1.0f - CA2) / SA2;
    if (CA2 >  0.0f) TA2O2 = SA2 / (1.0f + CA2);

    *A2 = 2.0f * atanf(TA2O2);
    if (*A2 < 0.0f) *A2 += 6.2831853f;
}
