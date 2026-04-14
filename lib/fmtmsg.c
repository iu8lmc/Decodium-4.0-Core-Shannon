/* fmtmsg.c — C replacement for lib/fmtmsg.f90
 *
 * subroutine fmtmsg(msg, iz)
 *   Convert msg to upper-case, collapse multiple blanks into one, and
 *   return the 1-based position of the last non-space character in iz.
 *
 * Fortran ABI: character*(*) msg → hidden length appended after iz.
 */
#include <string.h>

void fmtmsg_(char *msg, int *iz, size_t msg_hidden_len)
{
    int n = (int)msg_hidden_len;

    /* upper-case and find last non-space position (1-based) */
    *iz = n;
    for (int i = 0; i < n; i++) {
        if (msg[i] >= 'a' && msg[i] <= 'z')
            msg[i] = (char)(msg[i] - ('a' - 'A'));
        if (msg[i] != ' ')
            *iz = i + 1;
    }

    /* collapse double spaces, up to 37 iterations (matching Fortran) */
    for (int iter = 0; iter < 37; iter++) {
        int pos = -1;
        for (int i = 0; i < *iz - 1; i++) {
            if (msg[i] == ' ' && msg[i + 1] == ' ') { pos = i; break; }
        }
        if (pos < 0) break;
        /* remove msg[pos+1], shift left, pad end with space */
        memmove(msg + pos + 1, msg + pos + 2, (size_t)(n - pos - 2));
        msg[n - 1] = ' ';
        (*iz)--;
    }
}
