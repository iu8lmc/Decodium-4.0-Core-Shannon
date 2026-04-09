/* chkcall.c — C replacement for lib/chkcall.f90
 *
 * subroutine chkcall(w, bc, cok)
 *   Validate a standard or compound callsign.
 *   w(13):  input putative callsign (Fortran character*13, space-padded).
 *   bc(6):  output base call (Fortran character*6, space-padded).
 *   cok:    output Fortran LOGICAL (int32, 0=false, 1=true).
 *
 * Fortran ABI: two hidden size_t lengths appended after all explicit args.
 */
#include <stddef.h>
#include <string.h>

void chkcall_(const char *w, char *bc, int *cok,
              size_t w_hidden_len, size_t bc_hidden_len)
{
    /* Copy w into a null-terminated local buffer (Fortran pads with spaces) */
    char w13[14];
    size_t wl = w_hidden_len < 13 ? w_hidden_len : 13;
    memcpy(w13, w, wl);
    w13[wl] = '\0';

    /* n1 = len_trim(w): length ignoring trailing spaces */
    int n1 = (int)wl;
    while (n1 > 0 && w13[n1 - 1] == ' ') n1--;

    /* Initialise bc to first 6 chars (space-padded) */
    memset(bc, ' ', bc_hidden_len < 6 ? bc_hidden_len : 6);
    size_t bcl = bc_hidden_len < 6 ? bc_hidden_len : 6;
    for (size_t i = 0; i < bcl && i < 6 && i < wl; i++)
        bc[i] = w[i];

    *cok = 1;  /* assume valid */

    /* Basic rejections */
    if (n1 > 11)  goto reject;
    for (int i = 0; i < n1; i++) {
        char c = w13[i];
        if (c == '.' || c == '+' || c == '-' || c == '?') goto reject;
    }
    if (n1 > 6 && memchr(w13, '/', n1) == NULL) goto reject;

    {
        /* Find slash position (0-based, 0 if absent) */
        int i0 = 0;
        const char *sl = memchr(w13, '/', n1);
        if (sl) i0 = (int)(sl - w13) + 1;  /* 1-based as in Fortran */

        /* Base call must be < 7 characters */
        int left  = (i0 >= 1) ? i0 - 1 : n1;
        int right = (i0 >= 1) ? n1 - i0 : 0;
        if ((left > right ? left : right) > 6) goto reject;

        /* Extract base call from compound call */
        memset(bc, ' ', bcl);
        if (i0 >= 2 && i0 <= n1 - 1) {
            /* compound call: pick longer side */
            const char *src;
            int slen;
            if (i0 - 1 <= n1 - i0) { src = w13 + i0; slen = n1 - i0; }
            else                    { src = w13;       slen = i0 - 1;  }
            for (int i = 0; i < slen && i < (int)bcl; i++)
                bc[i] = src[i];
        }

        /* nbc = len_trim(bc) */
        int nbc = (int)bcl;
        while (nbc > 0 && bc[nbc - 1] == ' ') nbc--;
        if (nbc > 6) goto reject;

        /* One of first two chars must be a letter */
        int c1ok = (bc[0] >= 'A' && bc[0] <= 'Z') || (bc[0] >= 'a' && bc[0] <= 'z');
        int c2ok = (nbc >= 2) &&
                   ((bc[1] >= 'A' && bc[1] <= 'Z') || (bc[1] >= 'a' && bc[1] <= 'z'));
        if (!c1ok && !c2ok) goto reject;

        /* No Q-calls except QU1RK placeholder */
        if ((bc[0] == 'Q' || bc[0] == 'q') &&
            strncmp(bc, "QU1RK", nbc < 5 ? nbc : 5) != 0) goto reject;

        /* Digit in 2nd or 3rd position */
        int i1 = 0;
        if (nbc >= 2 && bc[1] >= '0' && bc[1] <= '9') i1 = 2;
        if (nbc >= 3 && bc[2] >= '0' && bc[2] <= '9') i1 = 3;
        if (i1 == 0) goto reject;

        /* Must have suffix of 1-3 letters after the digit */
        if (i1 == nbc) goto reject;
        int nsuf = 0;
        for (int i = i1; i < nbc; i++) {
            char c = bc[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) goto reject;
            nsuf++;
        }
        if (nsuf < 1 || nsuf > 3) goto reject;
    }
    return;

reject:
    *cok = 0;
}
