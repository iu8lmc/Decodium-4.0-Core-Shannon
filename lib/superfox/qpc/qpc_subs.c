// ------------------------------------------------------------------------------
// qpc_test.cpp
// 
// Test the WER performance of the QPC (127,50) Q=128 code
// 
// (c) 2024 - Nico Palermo, IV3NWV - Microtelecom Srl, Italy
// ------------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>

#include "dbgprintf.h"
#include "qpc_fwht.h"
#include "np_qpc.h"

#include "nhash2.h"

// for random numbers generation and NCFSK Rayleigh channel simulation
#include "np_rnd.h"  
void qpc_channel(float* yout, unsigned char* y, float EsNo)
{
    // compute channel outputs amplitudes

    int k;

    // generate noise samples  -----------------------------------------
    float sigman = 1.0f / sqrtf(2.0f);
    np_normrnd_cpx(yout, QPC_N * QPC_Q, 0, sigman);

    // dbgprintf_rows_float("yout", yout, QPC_Q*2, QPC_N);

    // generate rayleigh distributed signal amplitudes -----------------
    float symamps[QPC_N * 2];
    np_normrnd_cpx(symamps, QPC_N, 0, sigman);

    // dbgprintf_vector_float("symamps", symamps, QPC_N*2);

    // normalize signal amps to unity ----------------------------------
    float pwr = 0.0f;
    float* psymamps = symamps;
    // compute sig power
    for (k = 0; k < QPC_N; k++) {
        pwr += psymamps[0] * psymamps[0] + psymamps[1] * psymamps[1];
        psymamps += 2;
    }
    pwr = pwr / QPC_N;

    // normalize to avg EsNo
    float norm = sqrtf(EsNo/pwr);
    psymamps = symamps;
    for (k = 0; k < QPC_N; k++) {
        psymamps[0] *= norm;
        psymamps[1] *= norm;
        psymamps += 2;
    }
    // dbgprintf_vector_float("symamps norm", symamps, QPC_N * 2);

    // add signal amplitudes to noise -----------------------------------
    float *pyout   = yout;
    psymamps= symamps;
    for (k = 0; k < QPC_N; k++) {
        pyout[y[k]<<1]   += psymamps[0];
        pyout[(y[k]<<1)+1] += psymamps[1];
        pyout += (QPC_Q*2);
        psymamps += 2;
    }

    // dbgprintf_rows_float("s+n out", yout, QPC_Q * 2, QPC_N);

    return;
}
void qpc_likelihoods(float* py, float* yout, float EsNo, float No)
{
    // compute symbols likelihoods
    // (rayleigh channel)

    int k, j;

    float norm = EsNo / (EsNo + 1) / No;

    // compute likelihoods from energies ----------------------------
    float* pybase;
    float* ppyout = yout;
    float normpwr;
    float normpwrmax;
    float pynorm;

    for (k = 0; k < QPC_N; k++) {

        // compute loglikelihoods and largest one from energies
        pybase = py + k * QPC_Q;
        normpwrmax = 0.0f;
        for (j = 0; j < QPC_Q; j++) {
            normpwr = norm * (ppyout[0] * ppyout[0] + ppyout[1] * ppyout[1]);
            pybase[j] = normpwr;
            if (normpwr > normpwrmax)
                normpwrmax = normpwr;
            ppyout += 2;
        }
        // subtract largest exponent
        pynorm = 0.0f;
        for (j = 0; j < QPC_Q; j++) {
            pybase[j] = expf(pybase[j] - normpwrmax);
            pynorm += pybase[j];
        }
        // normalize to probabilities
        pynorm = 1.0f / pynorm;
        for (j = 0; j < QPC_Q; j++)
            pybase[j] = pybase[j] * pynorm;
    }

    return;
}
