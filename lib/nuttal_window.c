/* nuttal_window.c — C replacement for lib/nuttal_window.f90
 *
 * subroutine nuttal_window(win, n)
 *   Fill win(n) with a Nuttall window.
 */
#include <math.h>

void nuttal_window_(float *win, const int *n)
{
    static const float a0 =  0.3635819f;
    static const float a1 = -0.4891775f;
    static const float a2 =  0.1365995f;
    static const float a3 = -0.0106411f;
    float pi = 4.0f * atanf(1.0f);
    int N = *n;
    for (int i = 0; i < N; i++) {
        float t = (float)i / (float)N;
        win[i] = a0
               + a1 * cosf(2.0f * pi * t)
               + a2 * cosf(4.0f * pi * t)
               + a3 * cosf(6.0f * pi * t);
    }
}
