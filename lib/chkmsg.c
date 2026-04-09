/* chkmsg.c — C replacement for lib/chkmsg.f90
 *
 * subroutine chkmsg(message, cok, nspecial, flip)
 *   Detect OOO/OO suffix and RO/RRR/73 special messages in a JT65 22-char
 *   message.  Strips the suffix from message and sets cok, flip, nspecial.
 *
 * Fortran ABI: character*22 message, character*3 cok → hidden lengths
 *   appended after all explicit args: (..., size_t message_len, size_t cok_len).
 */
#include <string.h>

void chkmsg_(char *message, char *cok, int *nspecial, float *flip,
             size_t message_len, size_t cok_len)
{
    (void)message_len; (void)cok_len;

    *nspecial = 0;
    *flip     = 1.0f;
    memset(cok, ' ', 3);

    /* find last non-space character (1-based, matching Fortran) */
    int i = 22;
    for (int k = 21; k >= 0; k--) {
        if (message[k] != ' ') { i = k + 1; break; }
    }

    /* check for OOO/OO suffix */
    if (i >= 11) {
        /* message(i-3:i) in Fortran 1-based = message[i-4..i-1] in C 0-based */
        int ooo = (message[i-4] == ' ' &&
                   message[i-3] == 'O' &&
                   message[i-2] == 'O' &&
                   message[i-1] == 'O');
        /* message(20:22) = message[19..21] */
        int oo  = (message[19] == ' ' &&
                   message[20] == 'O' &&
                   message[21] == 'O');

        if (ooo || oo) {
            memcpy(cok, "OOO", 3);
            *flip = -1.0f;
            if (oo) {
                /* message = message(1:19): keep first 19 chars, pad rest */
                memset(message + 19, ' ', 3);
            } else {
                /* message = message(1:i-4): keep first i-4 chars, pad rest */
                memset(message + (i - 4), ' ', 22 - (i - 4));
            }
        }
    }

    /* check for RO / RRR / 73 special messages (full 22-char comparison) */
    if (memcmp(message, "RO                    ", 22) == 0) *nspecial = 2;
    if (memcmp(message, "RRR                   ", 22) == 0) *nspecial = 3;
    if (memcmp(message, "73                    ", 22) == 0) *nspecial = 4;
}
