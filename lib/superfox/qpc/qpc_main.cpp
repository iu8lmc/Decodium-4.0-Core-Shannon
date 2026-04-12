
// ------------------------------------------------------------------------------
// qpc_main.cpp
// 
// Test the WER performance of the QPC (127,50) Q=128 code
// 
// (c) 2024 - Nico Palermo, IV3NWV - Microtelecom Srl, Italy
// ------------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "dbgprintf.h"
#include "qpc_fwht.h"
#include "np_qpc.h"

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

int main()
{
    int k;

    int NT = 20000;             // number of transmissions to simultate
    float EbNodB = 3.7f;        // Eb/No to simulate


    int kc = qpccode.k;
    int np = qpccode.np;
    float Rc = 1.0f * kc / np;
    float Tm = 10.0f;                                // message duration
    float Rb = 1.0f / Tm * (QPC_K - 2) * QPC_LOG2Q;  // Bit rate assuming two symbols for CRC

    float EsNodB = EbNodB + 10.0f * log10f(Rc * QPC_LOG2Q);

    static unsigned char xin[QPC_K];              // input   information symbols
    static unsigned char xdec[QPC_K];             // decoded information symbols
    static unsigned char y[QPC_N];                // encoded codeword
    static unsigned char ydec[QPC_N];             // decoded codeword

    static float yout[QPC_N * QPC_Q * 2];  // channel complex amplitutes output
    static float py[QPC_N * QPC_Q];        // channel output probabilities

    float EsNo = powf(10.0f, EsNodB / 10.0f);
    float No = 1.0f;

    //JHT    static float we;
    static float wer;

    int kk;

    printf("QPC Word Error Rate Test\n");
    printf("Polar Code (%d,%d) Q=%d for the Rayleigh channel\n", qpccode.np, qpccode.k, qpccode.q);
    printf("Eb/No       = %.1f dB\n", EbNodB);
    printf("Es/No       = %.1f dB\n", EsNodB);
    printf("Tmsg        = %.1f s\n", Tm);
    printf("Bit Rate    = %.1f bit/s\n", Rb);
    printf("SNR(2500)   = %.1f dB\n\n", EbNodB + 10.0f * log10f(Rb/ 2500));


    for (k = 0; k < NT; k++) {

        np_unidrnd_uc(xin, QPC_K, QPC_Q);           // generate random information symbols

        qpc_encode(y, xin);                         // encode

        // compute channel outputs and probabilities

        // Note that if the code is punctured (i.e. N=127)
        // the first codeword symbol must not be transmitted over the channel
        // Here, in order to avoid useless copies,
        // the vector of likelihoods py continues to be a QPC_N*QPC_Q vector of floats.
        // The first received symbol likelihoods should start always at offset QPC_Q.
        // The first QPC_Q positions of py will be ignored (and set) by the decoder.
        qpc_channel(yout, y, EsNo);

        // The decoder can't estimate the EsNo easily
        // so we assume that in any case it is 5 dB
        float EsNoDec = 3.16; 
        qpc_likelihoods(py, yout, EsNoDec, No);
        
        qpc_decode(xdec, ydec, py);                 // decode 

        // count words in errors
        for (kk = 0; kk < QPC_K; kk++)
            if (xin[kk] ^ xdec[kk]) {
                wer += 1;
                break;
            }

        // show the decoder performance every 200 codewords transmissions
        if ((k % 200) == 0 && k>0) {
            printf("k=%6d/%6d wer = %8.2E\n", k, NT, wer / k);
	    fflush(stdout);
	}

    }
    // show the final result
    printf("k=%6d/%6d wer = %8.2E\n", k, NT, wer / k);
    printf("\nAll done.\n");
    return 0;
}
