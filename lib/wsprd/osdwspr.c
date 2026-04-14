/*
 * C translation of osdwspr.f90 + helper subroutines.
 * Original Fortran by Joe Taylor K1JT and Steven Franke K9AN.
 * Translated as part of the decodium3-build Fortran→C migration.
 *
 * Calling convention matches the Fortran-mangled name used in wsprd.c:
 *   void wsprd_osd_decode(float ss[], unsigned char apmask[], int *ndeep,
 *                 unsigned char cw[], int *nhardmin, float *dmin);
 */

#include <string.h>
#include <stdint.h>
#include <math.h>

#define N_WSPR   162
#define K_WSPR    50
#define NK_WSPR  112   /* N-K */

/* ------------------------------------------------------------------ */
/* indexx_c: argsort ascending (Numerical Recipes quicksort)           */
/* arr[0..n-1] → indx[0..n-1] with 0-based indices                   */
/* ------------------------------------------------------------------ */
static void indexx_c(const float *arr, int n, int *indx)
{
    enum { M = 7, NSTACK = 64 };
    int i, j, k, l, ir, jstack, indxt, itemp;
    float a;
    int istack[NSTACK];

    for (j = 0; j < n; j++) indx[j] = j;

    jstack = 0;
    l  = 0;
    ir = n - 1;

    for (;;) {
        if (ir - l < M) {
            for (j = l + 1; j <= ir; j++) {
                indxt = indx[j];
                a     = arr[indxt];
                for (i = j - 1; i >= l; i--) {
                    if (arr[indx[i]] <= a) break;
                    indx[i + 1] = indx[i];
                }
                indx[i + 1] = indxt;
            }
            if (jstack == 0) return;
            ir = istack[jstack--];
            l  = istack[jstack--];
        } else {
            k = (l + ir) / 2;
            itemp = indx[k];    indx[k]    = indx[l+1]; indx[l+1] = itemp;
            if (arr[indx[l+1]] > arr[indx[ir]]) { itemp=indx[l+1]; indx[l+1]=indx[ir];  indx[ir]=itemp;  }
            if (arr[indx[l]]   > arr[indx[ir]]) { itemp=indx[l];   indx[l]=indx[ir];    indx[ir]=itemp;  }
            if (arr[indx[l+1]] > arr[indx[l]])  { itemp=indx[l+1]; indx[l+1]=indx[l];   indx[l]=itemp;   }
            i     = l + 1;
            j     = ir;
            indxt = indx[l];
            a     = arr[indxt];
            for (;;) {
                do { i++; } while (arr[indx[i]] < a);
                do { j--; } while (arr[indx[j]] > a);
                if (j < i) break;
                itemp = indx[i]; indx[i] = indx[j]; indx[j] = itemp;
            }
            indx[l] = indx[j];
            indx[j] = indxt;
            jstack += 2;
            if (jstack >= NSTACK) break; /* should never happen */
            if (ir - i + 1 >= j - l) {
                istack[jstack]     = ir;
                istack[jstack - 1] = i;
                ir = j - 1;
            } else {
                istack[jstack]     = j - 1;
                istack[jstack - 1] = l;
                l = i;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* mrbencode: encode me[K] → codeword[N] using g2[N][K]               */
/* g2[row][col] corresponds to Fortran g2(row+1, col+1)               */
/* ------------------------------------------------------------------ */
static void mrbencode(const int8_t *me,
                      int8_t *codeword,
                      const int8_t g2[N_WSPR][K_WSPR])
{
    memset(codeword, 0, N_WSPR);
    for (int i = 0; i < K_WSPR; i++) {
        if (me[i] == 1) {
            for (int j = 0; j < N_WSPR; j++)
                codeword[j] ^= g2[j][i];
        }
    }
}

/* ------------------------------------------------------------------ */
/* nextpat: generate next OSD test-error pattern                       */
/* mi[0..k-1]: current pattern (updated in place)                      */
/* *iflag: 1-based index of lowest set bit; set to -1 when exhausted   */
/* ------------------------------------------------------------------ */
static void nextpat(int8_t *mi, int k, int iorder, int *iflag)
{
    /* find last 1-indexed i in 1..k-1 where mi[i-1]==0 and mi[i]==1 */
    int ind = -1;
    for (int i = 1; i <= k - 1; i++) {
        if (mi[i - 1] == 0 && mi[i] == 1) ind = i;
    }
    if (ind < 0) { *iflag = -1; return; }

    int8_t ms[K_WSPR];
    memset(ms, 0, (size_t)k);

    /* ms(1:ind-1) = mi(1:ind-1) → C: ms[0..ind-2] = mi[0..ind-2] */
    for (int i = 1; i <= ind - 1; i++) ms[i - 1] = mi[i - 1];
    ms[ind - 1] = 1; /* ms(ind) = 1 */

    /* if(ind+1 < k): fill tail with remaining ones */
    if (ind + 1 < k) {
        int nz = iorder;
        for (int i = 0; i < k; i++) nz -= ms[i];
        for (int i = k - nz + 1; i <= k; i++) ms[i - 1] = 1;
    }

    for (int i = 0; i < k; i++) mi[i] = ms[i];

    /* iflag = first 1-indexed position where mi==1 */
    for (int i = 1; i <= k; i++) {
        if (mi[i - 1] == 1) { *iflag = i; return; }
    }
    *iflag = -1;
}

/* ------------------------------------------------------------------ */
/* COMMON /boxes/ state shared between boxit and fetchit               */
/* Fortran: indexes(4000,2), fp(0:525000), np(4000)                    */
/* npindex values are 1-based (as in Fortran)                          */
/* ------------------------------------------------------------------ */
static int boxes_indexes[4000][2];
static int boxes_fp[525001];
static int boxes_np[4000];

static void boxit(int *reset, const int8_t *e2, int ntau,
                  int npindex, int i1, int i2)
{
    if (*reset) {
        memset(boxes_indexes, -1, sizeof(boxes_indexes));
        memset(boxes_fp,      -1, sizeof(boxes_fp));
        memset(boxes_np,      -1, sizeof(boxes_np));
        *reset = 0;
    }

    /* indexes(npindex,1) = i1; indexes(npindex,2) = i2  (1-indexed npindex) */
    boxes_indexes[npindex - 1][0] = i1;
    boxes_indexes[npindex - 1][1] = i2;

    /* ipat = bit-string representation of e2(1:ntau) */
    int ipat = 0;
    for (int i = 1; i <= ntau; i++) {
        if (e2[i - 1] == 1) ipat |= (1 << (ntau - i));
    }

    int ip = boxes_fp[ipat];
    if (ip == -1) {
        boxes_fp[ipat] = npindex;
    } else {
        while (boxes_np[ip - 1] != -1)
            ip = boxes_np[ip - 1];
        boxes_np[ip - 1] = npindex;
    }
}

static void fetchit(int *reset, const int8_t *e2, int ntau,
                    int *i1_out, int *i2_out)
{
    static int lastpat = -1;
    static int inext   = -1;

    if (*reset) {
        lastpat = -1;
        *reset  = 0;
    }

    int ipat = 0;
    for (int i = 1; i <= ntau; i++) {
        if (e2[i - 1] == 1) ipat |= (1 << (ntau - i));
    }
    int idx = boxes_fp[ipat];

    if (lastpat != ipat && idx > 0) {
        *i1_out = boxes_indexes[idx - 1][0];
        *i2_out = boxes_indexes[idx - 1][1];
        inext   = boxes_np[idx - 1];
    } else if (lastpat == ipat && inext > 0) {
        *i1_out = boxes_indexes[inext - 1][0];
        *i2_out = boxes_indexes[inext - 1][1];
        inext   = boxes_np[inext - 1];
    } else {
        *i1_out = -1;
        *i2_out = -1;
        inext   = -1;
    }
    lastpat = ipat;
}

/* ------------------------------------------------------------------ */
/* wsprd_osd_decode: ordered-statistics decoder for WSPR               */
/* C translation of subroutine osdwspr in osdwspr.f90                 */
/* ------------------------------------------------------------------ */
void wsprd_osd_decode(float ss[], unsigned char apmask[],
              int *ndeep_ptr,
              unsigned char cw[],
              int *nhardmin_ptr,
              float *dmin_ptr)
{
    /* Generator matrix – initialised once from the gg seed vector.    *
     * gen[k][n] ↔ Fortran gen(k+1, n+1), dimensions K×N.             */
    static int first = 1;
    static int8_t gen[K_WSPR][N_WSPR];

    static const int8_t gg[64] = {
        1,1,0,1,0,1,0,0,1,0,0,0,1,1,0,0,1,0,1,0,0,1,0,1,1,1,0,1,1,0,0,0,
        0,1,0,0,0,0,0,0,1,0,0,1,1,1,1,0,0,0,1,0,0,1,0,0,1,0,1,1,1,1,1,1
    };

    if (first) {
        memset(gen, 0, sizeof(gen));
        /* gen(1, 1:2*L) = gg  →  gen[0][0..63] = gg[0..63] */
        memcpy(gen[0], gg, 64);
        /* gen(i,:) = cshift(gen(i-1,:), -2)                          *
         * cshift with shift=-2: result[j] = source[(j-2+N) % N]      */
        for (int i = 1; i < K_WSPR; i++)
            for (int j = 0; j < N_WSPR; j++)
                gen[i][j] = gen[i - 1][(j - 2 + N_WSPR) % N_WSPR];
        first = 0;
    }

    int ndeep = *ndeep_ptr;

    /* Working arrays ------------------------------------------------- */
    float   rx[N_WSPR], absrx[N_WSPR];
    int8_t  apmaskr[N_WSPR], hdec[N_WSPR];
    int8_t  genmrb[K_WSPR][N_WSPR];
    int8_t  g2[N_WSPR][K_WSPR];       /* g2[n][k] = genmrb[k][n] */
    int     indices[N_WSPR], indx[N_WSPR];
    int8_t  m0[K_WSPR], me[K_WSPR], mi[K_WSPR], misub[K_WSPR];
    int8_t  e2sub[NK_WSPR], e2[NK_WSPR], ui[NK_WSPR], r2pat[NK_WSPR];
    int8_t  c0[N_WSPR], ce[N_WSPR], cw_work[N_WSPR];
    int     nxor[N_WSPR];

    /* Initialise ----------------------------------------------------- */
    for (int i = 0; i < N_WSPR; i++) {
        rx[i]      = ss[i] / 127.0f;
        apmaskr[i] = (int8_t)apmask[i];
        hdec[i]    = (rx[i] >= 0.0f) ? 1 : 0;
        absrx[i]   = fabsf(rx[i]);
    }

    /* Sort absrx ascending → indx[0..N-1] (0-based) */
    indexx_c(absrx, N_WSPR, indx);

    /* Build genmrb: column i ← column indx[N-1-i] of gen             *
     * (most-reliable basis vectors first)                             */
    for (int i = 0; i < N_WSPR; i++) {
        int src = indx[N_WSPR - 1 - i];
        for (int k = 0; k < K_WSPR; k++)
            genmrb[k][i] = gen[k][src];
        indices[i] = src;
    }

    /* Gaussian elimination – reduce to MRB systematic form ---------- */
    for (int id = 0; id < K_WSPR; id++) {
        int lim = K_WSPR + 20;
        if (lim > N_WSPR) lim = N_WSPR;
        for (int icol = id; icol < lim; icol++) {
            if (genmrb[id][icol] != 1) continue;
            if (icol != id) {
                /* swap columns id ↔ icol */
                for (int ii = 0; ii < K_WSPR; ii++) {
                    int8_t t        = genmrb[ii][id];
                    genmrb[ii][id]   = genmrb[ii][icol];
                    genmrb[ii][icol] = t;
                }
                int itmp      = indices[id];
                indices[id]   = indices[icol];
                indices[icol] = itmp;
            }
            /* zero out column id in all other rows */
            for (int ii = 0; ii < K_WSPR; ii++) {
                if (ii != id && genmrb[ii][id] == 1)
                    for (int j = 0; j < N_WSPR; j++)
                        genmrb[ii][j] ^= genmrb[id][j];
            }
            break;
        }
    }

    /* g2 = transpose(genmrb): g2[n][k] = genmrb[k][n] */
    for (int k = 0; k < K_WSPR; k++)
        for (int n = 0; n < N_WSPR; n++)
            g2[n][k] = genmrb[k][n];

    /* Reorder hdec, absrx, rx, apmaskr by indices ------------------- */
    {
        int8_t tmp8[N_WSPR];
        float  tmpf[N_WSPR];

        for (int i = 0; i < N_WSPR; i++) tmp8[i] = hdec[indices[i]];
        memcpy(hdec, tmp8, N_WSPR);

        for (int i = 0; i < N_WSPR; i++) tmpf[i] = absrx[indices[i]];
        memcpy(absrx, tmpf, N_WSPR * sizeof(float));

        for (int i = 0; i < N_WSPR; i++) tmpf[i] = rx[indices[i]];
        memcpy(rx, tmpf, N_WSPR * sizeof(float));

        for (int i = 0; i < N_WSPR; i++) tmp8[i] = apmaskr[indices[i]];
        memcpy(apmaskr, tmp8, N_WSPR);
    }

    /* m0 = first K hard decisions in MRB order */
    memcpy(m0, hdec, K_WSPR);

    /* Order-0 codeword */
    mrbencode(m0, c0, g2);

    int   nhardmin_l = 0;
    float dmin_l     = 0.0f;
    for (int i = 0; i < N_WSPR; i++) {
        int x        = (c0[i] ^ hdec[i]) & 1;
        nxor[i]      = x;
        nhardmin_l  += x;
        dmin_l      += (float)x * absrx[i];
    }
    memcpy(cw_work, c0, N_WSPR);

    if (ndeep <= 0) goto done;
    if (ndeep >  5) ndeep = 5;

    /* Set search depth parameters ------------------------------------ */
    int nord, npre1, npre2, nt, ntheta, ntau;
    ntau = 0;
    switch (ndeep) {
    case 1:  nord=1; npre1=0; npre2=0; nt=66; ntheta=16; ntau= 0; break;
    case 2:  nord=1; npre1=1; npre2=0; nt=66; ntheta=22; ntau=16; break;
    case 3:  nord=1; npre1=1; npre2=1; nt=66; ntheta=22; ntau=16; break;
    case 4:  nord=2; npre1=1; npre2=1; nt=66; ntheta=22; ntau=16; break;
    default: nord=3; npre1=1; npre2=1; nt=66; ntheta=22; ntau=16; break;
    }

    int ntotal   = 0;
    int nrejected = 0;

    /* Main OSD search ----------------------------------------------- */
    for (int iorder = 1; iorder <= nord; iorder++) {

        /* Initial pattern: K-iorder zeros followed by iorder ones */
        memset(misub, 0, K_WSPR);
        for (int i = K_WSPR - iorder; i < K_WSPR; i++) misub[i] = 1;
        int iflag = K_WSPR - iorder + 1; /* 1-indexed lowest set-bit */

        while (iflag >= 0) {

            int iend = (iorder == nord && npre1 == 0) ? iflag : 1;
            float d1 = 0.0f;

            /* n1 is 1-indexed throughout this loop */
            for (int n1 = iflag; n1 >= iend; n1--) {

                memcpy(mi, misub, K_WSPR);
                mi[n1 - 1] = 1;

                /* Skip if AP mask blocks any flipped bit */
                int skip = 0;
                for (int k = 0; k < K_WSPR; k++)
                    if ((apmaskr[k] & mi[k]) == 1) { skip = 1; break; }
                if (skip) continue;

                ntotal++;
                for (int j = 0; j < K_WSPR; j++) me[j] = m0[j] ^ mi[j];

                int nd1Kpt;
                if (n1 == iflag) {
                    mrbencode(me, ce, g2);
                    for (int j = 0; j < NK_WSPR; j++)
                        e2sub[j] = ce[K_WSPR + j] ^ hdec[K_WSPR + j];
                    memcpy(e2, e2sub, NK_WSPR);

                    nd1Kpt = 1;
                    for (int j = 0; j < nt; j++) nd1Kpt += e2sub[j];

                    d1 = 0.0f;
                    for (int k = 0; k < K_WSPR; k++)
                        d1 += (float)((me[k] ^ hdec[k]) & 1) * absrx[k];
                } else {
                    /* Fast approximate update: skip re-encoding for threshold */
                    for (int j = 0; j < NK_WSPR; j++)
                        e2[j] = e2sub[j] ^ g2[K_WSPR + j][n1 - 1];

                    nd1Kpt = 2;
                    for (int j = 0; j < nt; j++) nd1Kpt += e2[j];
                }

                if (nd1Kpt <= ntheta) {
                    mrbencode(me, ce, g2);
                    for (int i = 0; i < N_WSPR; i++) nxor[i] = (ce[i] ^ hdec[i]) & 1;

                    float dd;
                    if (n1 == iflag) {
                        dd = d1;
                        for (int j = 0; j < NK_WSPR; j++)
                            dd += (float)e2sub[j] * absrx[K_WSPR + j];
                    } else {
                        dd = d1 + (float)((ce[n1 - 1] ^ hdec[n1 - 1]) & 1) * absrx[n1 - 1];
                        for (int j = 0; j < NK_WSPR; j++)
                            dd += (float)e2[j] * absrx[K_WSPR + j];
                    }

                    if (dd < dmin_l) {
                        dmin_l     = dd;
                        memcpy(cw_work, ce, N_WSPR);
                        nhardmin_l = 0;
                        for (int i = 0; i < N_WSPR; i++) nhardmin_l += nxor[i];
                    }
                } else {
                    nrejected++;
                }
            } /* n1 loop */

            nextpat(misub, K_WSPR, iorder, &iflag);
        } /* while iflag >= 0 */
    } /* iorder loop */

    /* Second pre-processing rule (npre2) ----------------------------- */
    if (npre2 == 1) {

        /* Phase 1: build lookup table of (i1,i2) pairs indexed by     *
         * their parity pattern in the redundancy-check positions.      */
        int reset_boxit = 1;
        ntotal = 0;
        for (int i1 = K_WSPR; i1 >= 1; i1--) {
            for (int i2 = i1 - 1; i2 >= 1; i2--) {
                ntotal++;
                /* mi(1:ntau) = xor(g2(K+1:K+ntau, i1), g2(K+1:K+ntau, i2)) */
                for (int j = 0; j < ntau; j++)
                    mi[j] = g2[K_WSPR + j][i1 - 1] ^ g2[K_WSPR + j][i2 - 1];
                boxit(&reset_boxit, mi, ntau, ntotal, i1, i2);
            }
        }

        /* Phase 2: search codewords generated from each base pattern  */
        int reset_fetchit = 1;

        memset(misub, 0, K_WSPR);
        for (int i = K_WSPR - nord; i < K_WSPR; i++) misub[i] = 1;
        int iflag = K_WSPR - nord + 1; /* 1-indexed */

        while (iflag >= 0) {

            for (int j = 0; j < K_WSPR; j++) me[j] = m0[j] ^ misub[j];
            mrbencode(me, ce, g2);
            for (int j = 0; j < NK_WSPR; j++)
                e2sub[j] = ce[K_WSPR + j] ^ hdec[K_WSPR + j];

            for (int i2 = 0; i2 <= ntau; i2++) {
                memset(ui, 0, NK_WSPR);
                if (i2 > 0) ui[i2 - 1] = 1; /* ui(i2)=1, 1-indexed */
                for (int j = 0; j < NK_WSPR; j++)
                    r2pat[j] = e2sub[j] ^ ui[j];

                /* Retrieve all stored (in1,in2) pairs matching r2pat */
                while (1) {
                    int in1, in2;
                    fetchit(&reset_fetchit, r2pat, ntau, &in1, &in2);
                    if (in1 <= 0 || in2 <= 0) break;

                    memcpy(mi, misub, K_WSPR);
                    mi[in1 - 1] = 1;
                    mi[in2 - 1] = 1;

                    int summi = 0;
                    for (int k = 0; k < K_WSPR; k++) summi += mi[k];
                    if (summi < nord + npre1 + npre2) continue;

                    int skip = 0;
                    for (int k = 0; k < K_WSPR; k++)
                        if ((apmaskr[k] & mi[k]) == 1) { skip = 1; break; }
                    if (skip) continue;

                    for (int j = 0; j < K_WSPR; j++) me[j] = m0[j] ^ mi[j];
                    mrbencode(me, ce, g2);

                    float dd = 0.0f;
                    for (int i = 0; i < N_WSPR; i++) {
                        int x  = (ce[i] ^ hdec[i]) & 1;
                        nxor[i] = x;
                        dd     += (float)x * absrx[i];
                    }
                    if (dd < dmin_l) {
                        dmin_l     = dd;
                        memcpy(cw_work, ce, N_WSPR);
                        nhardmin_l = 0;
                        for (int i = 0; i < N_WSPR; i++) nhardmin_l += nxor[i];
                    }
                } /* while fetchit pairs */
            } /* i2 loop */

            nextpat(misub, K_WSPR, nord, &iflag);
        } /* while iflag >= 0 */
    } /* npre2 */

    (void)ntotal;
    (void)nrejected;

done:
    /* Scatter cw_work back to as-received order: cw[indices[i]] = cw_work[i] */
    {
        int8_t tmp[N_WSPR];
        memcpy(tmp, cw_work, N_WSPR);
        for (int i = 0; i < N_WSPR; i++)
            cw[indices[i]] = (unsigned char)tmp[i];
    }

    *ndeep_ptr    = ndeep;
    *nhardmin_ptr = nhardmin_l;
    *dmin_ptr     = dmin_l;
}

/* Legacy wsprd.c still expects the historical Fortran entry point name. */
void osdwspr_(float ss[], unsigned char apmask[],
              int *ndeep_ptr,
              unsigned char cw[],
              int *nhardmin_ptr,
              float *dmin_ptr)
{
    wsprd_osd_decode(ss, apmask, ndeep_ptr, cw, nhardmin_ptr, dmin_ptr);
}
